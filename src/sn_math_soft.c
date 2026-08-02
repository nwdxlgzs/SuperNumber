/*
 * Pure soft transcendentals for multiprec floats (m_bits > 52).
 * Built on top of public sn_add/sub/mul/div/sqrt so both mp and narrow work.
 * Narrow formats (m_bits <= 52) keep host math.h path in sn_math.c.
 */
#include "internal/sn_impl.h"
#include <math.h>
#include <string.h>
#include <limits.h>

/* Match sn_float.c / sn_float_mp.c: E/M memory-bound, no small hard cap. */
#ifndef SN_FLOAT_E_MAX
#define SN_FLOAT_E_MAX (INT_MAX / 4)
#endif
#ifndef SN_FLOAT_M_MAX
#define SN_FLOAT_M_MAX (INT_MAX / 4)
#endif

double j0(double);
double j1(double);
double jn(int, double);
double y0(double);
double y1(double);
double yn(int, double);

int sn_math_need_soft(const sn_value *a)
{
    return a && a->kind == SN_KIND_FLOAT && sn_float_mp_supported(a->e_bits, a->m_bits);
}

sn_status sn_soft_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a))
        return sn_float_mp_sqrt(ctx, out, a, opt);
    return sn_float_sqrt(ctx, out, a, opt);
}

static sn_status soft_from_d(sn_ctx *ctx, sn_value *out, double x, const sn_value *fmt,
                             const sn_op_opt *opt)
{
    return sn_float_from_double(ctx, out, x, fmt->e_bits, fmt->m_bits, fmt->nan_enabled, opt);
}


/* Exact integer as float in `fmt` layout (no double seed). */
static sn_status soft_from_i(sn_ctx *ctx, sn_value *out, int64_t n, const sn_value *fmt,
                             const sn_op_opt *opt)
{
    return sn_float_from_i64(ctx, out, n, fmt->e_bits, fmt->m_bits, fmt->nan_enabled, opt);
}



/* -------------------------------------------------------------------------- */
/* Per-ctx multiprec constant cache (ln2 / pi). No process globals.           */
/* -------------------------------------------------------------------------- */

typedef struct sn_soft_cache {
    int      have_ln2;
    int      have_pi;
    int      have_ln10;
    int      e_bits;
    int      m_bits;
    int      nan_enabled;
    sn_value ln2;
    sn_value pi;
    sn_value ln10;
} sn_soft_cache;

void sn_soft_cache_free(sn_ctx *ctx)
{
    sn_soft_cache *c;
    if (!ctx || !ctx->soft_cache) return;
    c = (sn_soft_cache *)ctx->soft_cache;
    sn_value_clear(ctx, &c->ln2);
    sn_value_clear(ctx, &c->pi);
    sn_value_clear(ctx, &c->ln10);
    sn_free(ctx, c, sizeof(*c));
    ctx->soft_cache = NULL;
}

static sn_soft_cache *soft_cache_get(sn_ctx *ctx, const sn_value *fmt)
{
    sn_soft_cache *c;
    if (!ctx || !fmt) return NULL;
    c = (sn_soft_cache *)ctx->soft_cache;
    if (c) {
        if (c->e_bits == fmt->e_bits && c->m_bits == fmt->m_bits &&
            c->nan_enabled == fmt->nan_enabled)
            return c;
        /* format changed: drop cache */
        sn_soft_cache_free(ctx);
    }
    c = (sn_soft_cache *)sn_malloc(ctx, sizeof(*c));
    if (!c) return NULL;
    memset(c, 0, sizeof(*c));
    c->e_bits = fmt->e_bits;
    c->m_bits = fmt->m_bits;
    c->nan_enabled = fmt->nan_enabled;
    sn_value_init(&c->ln2);
    sn_value_init(&c->pi);
    sn_value_init(&c->ln10);
    ctx->soft_cache = c;
    return c;
}


/*
 * ln(2) by series: ln2 = 2*artanh(1/3) = 2 * sum_{n=0}^N z^{2n+1}/(2n+1), z=1/3.
 * Pure soft; no static digit table. m_bits<=52 uses host double seed for speed.
 */
static sn_status soft_const_ln2_compute(sn_ctx *ctx, sn_value *out, const sn_value *fmt, const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, three, z, z2, term, sum, t, den;
    int n, max_n;

    if (fmt->m_bits <= 52)
        return soft_from_d(ctx, out, 0.69314718055994530941723212145818, fmt, opt);

    sn_value_init(&one);
    sn_value_init(&three);
    sn_value_init(&z);
    sn_value_init(&z2);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&t);
    sn_value_init(&den);

    st = sn_float_from_i64(ctx, &one, 1, fmt->e_bits, fmt->m_bits, fmt->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_from_i64(ctx, &three, 3, fmt->e_bits, fmt->m_bits, fmt->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_div(ctx, &z, &one, &three, opt); if (st != SN_OK) goto done; /* 1/3 */
    st = sn_mul(ctx, &z2, &z, &z, opt); if (st != SN_OK) goto done;       /* 1/9 */
    st = sn_value_copy(ctx, &term, &z); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &z); if (st != SN_OK) goto done;

    /* log9(2^m) ~ m / log2(9) terms; pad guards */
    max_n = fmt->m_bits / 3 + 16;
    if (max_n < 24) max_n = 24;
    if (max_n > 600) max_n = 600;

    for (n = 1; n <= max_n; n++) {
        st = sn_mul(ctx, &t, &term, &z2, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &term); sn_value_move(&term, &t); sn_value_init(&t);
        st = sn_float_from_i64(ctx, &den, (int64_t)(2 * n + 1), fmt->e_bits, fmt->m_bits, fmt->nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &term, &den, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &sum, &t, opt); if (st != SN_OK) goto done;
        if (n > 8 && sn_fp_classify(&t) == SN_FP_ZERO) break;
    }
    st = soft_from_d(ctx, &t, 2.0, fmt, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, out, &sum, &t, opt);
done:
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &three);
    sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &z2);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &den);
    return st;
}

static sn_status soft_const_ln2(sn_ctx *ctx, sn_value *out, const sn_value *fmt, const sn_op_opt *opt)
{
    sn_soft_cache *c;
    sn_status st;
    if (!fmt) return SN_ERR_ARG;
    if (fmt->m_bits <= 52)
        return soft_from_d(ctx, out, 0.69314718055994530941723212145818, fmt, opt);
    c = soft_cache_get(ctx, fmt);
    if (c && c->have_ln2)
        return sn_value_copy(ctx, out, &c->ln2);
    st = soft_const_ln2_compute(ctx, out, fmt, opt);
    if (st == SN_OK && c) {
        sn_value_clear(ctx, &c->ln2);
        if (sn_value_copy(ctx, &c->ln2, out) == SN_OK)
            c->have_ln2 = 1;
    }
    return st;
}

/*
 * pi by Brent-Salamin AGM (quadratic convergence).
 * a0=1, b0=1/sqrt(2), t0=1/4, p0=1;
 * a'=(a+b)/2, b'=sqrt(a*b), t'=t-p*(a-a')^2, p'=2p;
 * pi ~ (a'+b')^2 / (4 t').
 */
static sn_status soft_const_ln10(sn_ctx *ctx, sn_value *out, const sn_value *fmt, const sn_op_opt *opt)
{
    sn_soft_cache *c;
    sn_status st;
    sn_value ten;
    if (!fmt) return SN_ERR_ARG;
    if (fmt->m_bits <= 52)
        return soft_from_d(ctx, out, 2.3025850929940456840179914546844, fmt, opt);
    c = soft_cache_get(ctx, fmt);
    if (c && c->have_ln10)
        return sn_value_copy(ctx, out, &c->ln10);
    sn_value_init(&ten);
    st = soft_from_d(ctx, &ten, 10.0, fmt, opt);
    if (st != SN_OK) { sn_value_clear(ctx, &ten); return st; }
    st = sn_soft_log(ctx, out, &ten, opt);
    sn_value_clear(ctx, &ten);
    if (st != SN_OK) return st;
    if (c) {
        st = sn_value_copy(ctx, &c->ln10, out);
        if (st == SN_OK) c->have_ln10 = 1;
    }
    return st;
}

static sn_status soft_const_pi_compute(sn_ctx *ctx, sn_value *out, const sn_value *fmt, const sn_op_opt *opt)
{
    sn_status st;
    sn_value a, b, t, p, an, bn, tmp, tmp2, two, four, half;
    int i, iters;

    if (fmt->m_bits <= 52)
        return soft_from_d(ctx, out, 3.1415926535897932384626433832795, fmt, opt);

    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&t);
    sn_value_init(&p);
    sn_value_init(&an);
    sn_value_init(&bn);
    sn_value_init(&tmp);
    sn_value_init(&tmp2);
    sn_value_init(&two);
    sn_value_init(&four);
    sn_value_init(&half);

    st = soft_from_d(ctx, &a, 1.0, fmt, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, fmt, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, fmt, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, fmt, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.25, fmt, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &p, 1.0, fmt, opt); if (st != SN_OK) goto done;
    /* b0 = 1/sqrt(2) */
    st = sn_sqrt(ctx, &tmp, &two, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &b, &a, &tmp, opt); if (st != SN_OK) goto done;

    /* quadratic: ~log2(m) + a few iters */
    iters = 0;
    {
        int m = fmt->m_bits;
        while (m > 1) { m = (m + 1) / 2; iters++; }
        iters += 5;
        if (iters < 8) iters = 8;
        if (iters > 64) iters = 64;
    }

    for (i = 0; i < iters; i++) {
        /* an = (a+b)/2 */
        st = sn_add(ctx, &tmp, &a, &b, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &an, &tmp, &half, opt); if (st != SN_OK) goto done;
        /* bn = sqrt(a*b) */
        st = sn_mul(ctx, &tmp, &a, &b, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &bn, &tmp, opt); if (st != SN_OK) goto done;
        /* t = t - p*(a-an)^2 */
        st = sn_sub(ctx, &tmp, &a, &an, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &tmp2, &tmp, &tmp, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &tmp, &p, &tmp2, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &t, &t, &tmp, opt); if (st != SN_OK) goto done;
        /* p = 2*p */
        st = sn_mul(ctx, &p, &p, &two, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &a); sn_value_move(&a, &an); sn_value_init(&an);
        sn_value_clear(ctx, &b); sn_value_move(&b, &bn); sn_value_init(&bn);
    }
    /* pi = (a+b)^2 / (4*t) */
    st = sn_add(ctx, &tmp, &a, &b, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &tmp2, &tmp, &tmp, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &tmp, &four, &t, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &tmp2, &tmp, opt);
done:
    sn_value_clear(ctx, &a);
    sn_value_clear(ctx, &b);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &p);
    sn_value_clear(ctx, &an);
    sn_value_clear(ctx, &bn);
    sn_value_clear(ctx, &tmp);
    sn_value_clear(ctx, &tmp2);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &four);
    sn_value_clear(ctx, &half);
    return st;
}

static sn_status soft_const_pi(sn_ctx *ctx, sn_value *out, const sn_value *fmt, const sn_op_opt *opt)
{
    sn_soft_cache *c;
    sn_status st;
    if (!fmt) return SN_ERR_ARG;
    if (fmt->m_bits <= 52)
        return soft_from_d(ctx, out, 3.1415926535897932384626433832795, fmt, opt);
    c = soft_cache_get(ctx, fmt);
    if (c && c->have_pi)
        return sn_value_copy(ctx, out, &c->pi);
    st = soft_const_pi_compute(ctx, out, fmt, opt);
    if (st == SN_OK && c) {
        sn_value_clear(ctx, &c->pi);
        if (sn_value_copy(ctx, &c->pi, out) == SN_OK)
            c->have_pi = 1;
    }
    return st;
}


static sn_status soft_pow2i(sn_ctx *ctx, sn_value *out, int n, const sn_value *fmt, const sn_op_opt *opt)
{
    /* 2^n via exact integer 1 then sn_ldexp (multiprec adjusts exp; no O(|n|) mul chain). */
    sn_status st;
    sn_value one;
    sn_value_init(&one);
    st = sn_float_from_i64(ctx, &one, 1, fmt->e_bits, fmt->m_bits, fmt->nan_enabled, opt);
    if (st != SN_OK) { sn_value_clear(ctx, &one); return st; }
    if (n == 0) {
        sn_value_move(out, &one);
        return SN_OK;
    }
    st = sn_ldexp(ctx, out, &one, n, opt);
    sn_value_clear(ctx, &one);
    return st;
}

/* frexp-like on soft float: a = m * 2^e, m in [0.5, 1).
 * Multiprec must NOT go through host double (loses bits for m_bits>52). */
static sn_status soft_frexp(sn_ctx *ctx, sn_value *mant, int *exp, const sn_value *a,
                            const sn_op_opt *opt)
{
    if (!exp) return SN_ERR_ARG;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    /* sn_frexp dispatches to sn_float_mp_frexp when need_soft. */
    return sn_frexp(ctx, mant, exp, a, opt);
}

/* exp series: exp(x) for reduced residual, x real float.
 * Identity exp(x)=exp(x/2)^2 applied until |x| < thr so Taylor is O(m) short. */
static sn_status soft_exp_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    /* sum_{k=0}^N w^k / k!  then square halves times */
    sn_status st;
    sn_value term, sum, t, one, k, prev, w, thr, two, aw;
    int i, max_terms, rel, halves, pass;

    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&t);
    sn_value_init(&one);
    sn_value_init(&k);
    sn_value_init(&prev);
    sn_value_init(&w);
    sn_value_init(&thr);
    sn_value_init(&two);
    sn_value_init(&aw);

    st = sn_value_copy(ctx, &w, x); if (st != SN_OK) goto done;
    st = sn_float_from_i64(ctx, &one, 1, x->e_bits, x->m_bits, x->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_from_i64(ctx, &two, 2, x->e_bits, x->m_bits, x->nan_enabled, opt);
    if (st != SN_OK) goto done;
    /* ~0.125: after ln2 reduction |x| is O(1); a few halvings make series tiny */
    st = soft_from_d(ctx, &thr, 0.125, x, opt); if (st != SN_OK) goto done;
    halves = 0;
    for (pass = 0; pass < 20; pass++) {
        if (sn_fp_classify(&w) == SN_FP_ZERO) break;
        st = sn_abs(ctx, &aw, &w, opt); if (st != SN_OK) goto done;
        st = sn_cmp(ctx, &rel, &aw, &thr); if (st != SN_OK) goto done;
        if (rel < 0) break;
        st = sn_div(ctx, &t, &w, &two, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &w);
        sn_value_move(&w, &t);
        sn_value_init(&t);
        halves++;
    }

    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &one); if (st != SN_OK) goto done;

    /* |w| < 0.125: terms shrink fast; m/4 + pad suffices */
    max_terms = x->m_bits / 4 + 32;
    if (max_terms < 24) max_terms = 24;
    if (max_terms > 2000) max_terms = 2000;

    for (i = 1; i <= max_terms; i++) {
        st = sn_float_from_i64(ctx, &k, (int64_t)i, x->e_bits, x->m_bits, x->nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &term, &w, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &term, &t, &k, opt); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &prev, &sum); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &term, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum);
        sn_value_move(&sum, &t);
        sn_value_init(&t);
        /* stop when term no longer changes sum (full working precision) */
        if (i > 6 && sn_cmp(ctx, &rel, &sum, &prev) == SN_OK && rel == 0) break;
        if (sn_fp_classify(&term) == SN_FP_ZERO) break;
    }

    while (halves-- > 0) {
        st = sn_mul(ctx, &t, &sum, &sum, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum);
        sn_value_move(&sum, &t);
        sn_value_init(&t);
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &prev);
    sn_value_clear(ctx, &w);
    sn_value_clear(ctx, &thr);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &aw);
    return st;
}

sn_status sn_soft_exp(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value ln2, r, k_f, t, two_k, aw, resw;
    const sn_value *x;
    double kd;
    int ki, cls, e_orig, m_orig, nan_orig, e_work, m_work, elev = 0;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        if (sn_fp_signbit(a))
            return sn_float_set_zero(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }
    if (cls == SN_FP_ZERO)
        return soft_from_d(ctx, out, 1.0, a, opt);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&ln2);
    sn_value_init(&r);
    sn_value_init(&k_f);
    sn_value_init(&t);
    sn_value_init(&two_k);
    sn_value_init(&aw);
    sn_value_init(&resw);

    x = a;
    /* Elevate mantissa for multiprec so k*ln2 cancellation keeps full target digits. */
    if (m_orig > 52) {
        e_work = e_orig < 16 ? 16 : e_orig;
        /* m+48: extra guard for k*ln2 cancellation on large |x| */
        m_work = m_orig + 48;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    /* Reduce: exp(x) = 2^k * exp(r), r = x - k*ln2, k = round(x/ln2). */
    st = soft_const_ln2(ctx, &ln2, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, x, &ln2, opt); if (st != SN_OK) goto done;
    st = sn_fround(ctx, &k_f, &t, opt); if (st != SN_OK) goto done;
    {
        int64_t ki64 = 0;
        st = sn_to_i64(ctx, &k_f, &ki64);
        if (st != SN_OK) {
            st = sn_to_double(ctx, &k_f, &kd);
            if (st != SN_OK) goto done;
            if (kd > 1e9) kd = 1e9;
            if (kd < -1e9) kd = -1e9;
            ki64 = (int64_t)(kd >= 0.0 ? floor(kd + 0.5) : ceil(kd - 0.5));
        }
        if (ki64 > 1000000000LL) ki64 = 1000000000LL;
        if (ki64 < -1000000000LL) ki64 = -1000000000LL;
        ki = (int)ki64;
    }
    st = sn_float_from_i64(ctx, &k_f, (int64_t)ki, x->e_bits, x->m_bits, x->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &k_f, &ln2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &r, x, &t, opt); if (st != SN_OK) goto done;

    /* Residual re-reduce if |r| > 1. */
    {
        sn_value one, ar;
        int rel = 0, extra, pass;
        sn_value_init(&one);
        sn_value_init(&ar);
        st = sn_float_from_i64(ctx, &one, 1, x->e_bits, x->m_bits, x->nan_enabled, opt);
        for (pass = 0; st == SN_OK && pass < 4; pass++) {
            st = sn_abs(ctx, &ar, &r, opt); if (st != SN_OK) break;
            st = sn_cmp(ctx, &rel, &ar, &one); if (st != SN_OK) break;
            if (rel <= 0) break;
            st = sn_div(ctx, &t, &r, &ln2, opt); if (st != SN_OK) break;
            st = sn_fround(ctx, &k_f, &t, opt); if (st != SN_OK) break;
            {
                int64_t e64 = 0;
                if (sn_to_i64(ctx, &k_f, &e64) != SN_OK) {
                    st = sn_to_double(ctx, &k_f, &kd);
                    if (st != SN_OK) break;
                    e64 = (int64_t)(kd >= 0.0 ? floor(kd + 0.5) : ceil(kd - 0.5));
                }
                if (e64 > 1000000LL) e64 = 1000000LL;
                if (e64 < -1000000LL) e64 = -1000000LL;
                extra = (int)e64;
            }
            if (extra == 0) break;
            ki += extra;
            st = sn_float_from_i64(ctx, &k_f, (int64_t)extra, x->e_bits, x->m_bits, x->nan_enabled, opt);
            if (st != SN_OK) break;
            st = sn_mul(ctx, &t, &k_f, &ln2, opt); if (st != SN_OK) break;
            st = sn_sub(ctx, &r, &r, &t, opt);
        }
        sn_value_clear(ctx, &one);
        sn_value_clear(ctx, &ar);
        if (st != SN_OK) goto done;
    }

    st = soft_exp_series(ctx, &resw, &r, opt); if (st != SN_OK) goto done;
    st = soft_pow2i(ctx, &two_k, ki, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &resw, &two_k, opt); if (st != SN_OK) goto done;
    sn_value_clear(ctx, &resw);
    sn_value_move(&resw, &t);
    sn_value_init(&t);

    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &ln2);
    sn_value_clear(ctx, &r);
    sn_value_clear(ctx, &k_f);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &two_k);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

/* artanh series: artanh(z) = sum z^{2k+1}/(2k+1), |z|<1.
 * Double-angle reduction: artanh(z) = 2*artanh(z/(1+sqrt(1-z^2)))
 * applied while |z| is not tiny so the Taylor loop is O(m) with small constant. */
static sn_status soft_artanh_series(sn_ctx *ctx, sn_value *out, const sn_value *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value z2, term, sum, t, odd, w, one, thr, s, den, az;
    int i, max_terms, dbl = 0, rel, pass;

    sn_value_init(&z2);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&t);
    sn_value_init(&odd);
    sn_value_init(&w);
    sn_value_init(&one);
    sn_value_init(&thr);
    sn_value_init(&s);
    sn_value_init(&den);
    sn_value_init(&az);

    st = sn_value_copy(ctx, &w, z); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, z, opt); if (st != SN_OK) goto done;
    /* reduce until |w| < ~0.2 so series converges in ~O(m/4) terms */
    st = soft_from_d(ctx, &thr, 0.2, z, opt); if (st != SN_OK) goto done;
    for (pass = 0; pass < 16; pass++) {
        st = sn_abs(ctx, &az, &w, opt); if (st != SN_OK) goto done;
        st = sn_cmp(ctx, &rel, &az, &thr); if (st != SN_OK) goto done;
        if (rel < 0) break;
        /* w <- w / (1 + sqrt(1 - w^2)) */
        st = sn_mul(ctx, &z2, &w, &w, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &s, &one, &z2, opt); if (st != SN_OK) goto done;
        st = sn_soft_sqrt(ctx, &s, &s, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &den, &one, &s, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &w, &den, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &w);
        sn_value_move(&w, &t);
        sn_value_init(&t);
        dbl++;
    }

    if (sn_fp_classify(&w) == SN_FP_ZERO) {
        st = sn_float_set_zero(ctx, &sum, 0, z->e_bits, z->m_bits, z->nan_enabled);
        if (st != SN_OK) goto done;
    } else {
        st = sn_mul(ctx, &z2, &w, &w, opt); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &term, &w); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &sum, &w); if (st != SN_OK) goto done;

        /* After reduction |w|<0.2: terms shrink ~w^2 each step; m/3 + pad is enough. */
        max_terms = z->m_bits / 3 + 24;
        if (max_terms < 24) max_terms = 24;
        if (max_terms > 2000) max_terms = 2000;

        for (i = 1; i <= max_terms; i++) {
            st = sn_mul(ctx, &t, &term, &z2, opt); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &term);
            sn_value_move(&term, &t);
            sn_value_init(&t);
            st = sn_float_from_i64(ctx, &odd, (int64_t)(2 * i + 1), z->e_bits, z->m_bits, z->nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_div(ctx, &t, &term, &odd, opt); if (st != SN_OK) goto done;
            st = sn_add(ctx, &sum, &sum, &t, opt); if (st != SN_OK) goto done;
            /* relative early exit: |t| << |sum| by ~m bits (compare vs sum scaled) */
            if (i > 6) {
                /* stop when |term| < |sum| * 2^-(m+4) (working precision) */
                sn_value sc, as, at, thr;
                int r2 = 0;
                sn_value_init(&sc); sn_value_init(&as); sn_value_init(&at); sn_value_init(&thr);
                if (soft_pow2i(ctx, &sc, -(z->m_bits + 4), z, opt) == SN_OK &&
                    sn_abs(ctx, &as, &sum, opt) == SN_OK &&
                    sn_mul(ctx, &thr, &as, &sc, opt) == SN_OK &&
                    sn_abs(ctx, &at, &t, opt) == SN_OK &&
                    sn_cmp(ctx, &r2, &at, &thr) == SN_OK && r2 < 0) {
                    sn_value_clear(ctx, &sc); sn_value_clear(ctx, &as); sn_value_clear(ctx, &at); sn_value_clear(ctx, &thr);
                    break;
                }
                sn_value_clear(ctx, &sc); sn_value_clear(ctx, &as); sn_value_clear(ctx, &at); sn_value_clear(ctx, &thr);
                if (sn_fp_classify(&t) == SN_FP_ZERO) break;
            }
        }
    }
    /* undo double-angle: multiply by 2^dbl */
    while (dbl-- > 0) {
        st = sn_add(ctx, &sum, &sum, &sum, opt); if (st != SN_OK) goto done;
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &z2);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &odd);
    sn_value_clear(ctx, &w);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &thr);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &az);
    return st;
}


/* AGM log threshold: artanh+half-angle is fine below this; AGM is O(log m) and wins
 * for large multiprec (m_bits >> 100). */
#ifndef SN_SOFT_LOG_AGM_MIN_M
/* Calibrated 2026-08-02 vs thr={200,220,240} (same residual, max_rel~2e-49):
 * thr=200 forces AGM at m=200 (~6.9s) while artanh is ~2.1s;
 * thr=220 still pays AGM at m=220 (~3.4s) vs artanh ~0.9s under thr=240;
 * thr=240 keeps artanh through m=220 and switches AGM from ~240 up — best mid-range. */
#define SN_SOFT_LOG_AGM_MIN_M 240
#endif

/* AGM(1, b0) with quadratic convergence; returns final a (~b). */
static sn_status soft_agm12(sn_ctx *ctx, sn_value *out, const sn_value *b0,
                            const sn_value *fmt, const sn_op_opt *opt,
                            int extra_gap_bits)
{
    sn_status st;
    sn_value a, b, an, bn, tmp, half, diff, thr, sc;
    int i, iters, rel, m, g;

    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&an);
    sn_value_init(&bn);
    sn_value_init(&tmp);
    sn_value_init(&half);
    sn_value_init(&diff);
    sn_value_init(&thr);
    sn_value_init(&sc);

    st = soft_from_d(ctx, &a, 1.0, fmt, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &b, b0); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, fmt, opt); if (st != SN_OK) goto done;

    m = fmt->m_bits;
    iters = 0;
    while (m > 1) { m = (m + 1) / 2; iters++; }
    /* When b0 ~ 2^{-G}, early AGM steps only shrink log-gap ~halving G each iter. */
    g = extra_gap_bits;
    if (g < 0) g = 0;
    while (g > 1) { g = (g + 1) / 2; iters++; }
    iters += 6;
    if (iters < 10) iters = 10;
    if (iters > 128) iters = 128;

    /* stop when |a-b| < 2^{-(m_bits+16)} * max(a,1) roughly */
    st = soft_pow2i(ctx, &sc, -(fmt->m_bits + 16), fmt, opt); if (st != SN_OK) goto done;

    for (i = 0; i < iters; i++) {
        st = sn_add(ctx, &tmp, &a, &b, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &an, &tmp, &half, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &tmp, &a, &b, opt); if (st != SN_OK) goto done;
        st = sn_soft_sqrt(ctx, &bn, &tmp, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &a); sn_value_move(&a, &an); sn_value_init(&an);
        sn_value_clear(ctx, &b); sn_value_move(&b, &bn); sn_value_init(&bn);
        if (i >= 3) {
            st = sn_sub(ctx, &diff, &a, &b, opt); if (st != SN_OK) goto done;
            st = sn_abs(ctx, &diff, &diff, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, &thr, &a, &sc, opt); if (st != SN_OK) goto done;
            st = sn_cmp(ctx, &rel, &diff, &thr); if (st != SN_OK) goto done;
            if (rel < 0) break;
            if (sn_fp_classify(&diff) == SN_FP_ZERO) break;
        }
    }
    st = sn_value_copy(ctx, out, &a);
done:
    sn_value_clear(ctx, &a);
    sn_value_clear(ctx, &b);
    sn_value_clear(ctx, &an);
    sn_value_clear(ctx, &bn);
    sn_value_clear(ctx, &tmp);
    sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &diff);
    sn_value_clear(ctx, &thr);
    sn_value_clear(ctx, &sc);
    return st;
}

/* ln(x) via AGM for x>0 finite: ln(x) = pi/(2*AGM(1, 4/M)) - N*ln2, M = x*2^N.
 * Work at elevated mantissa so cancellation of pi/(2*AGM) vs N*ln2 keeps full target digits. */
static sn_status soft_log_agm(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value four, M, scale, b0, agm, pi, ln2, Nf, t, xw, resw;
    int N, e_work, m_work, nan_en;

    sn_value_init(&four);
    sn_value_init(&M);
    sn_value_init(&scale);
    sn_value_init(&b0);
    sn_value_init(&agm);
    sn_value_init(&pi);
    sn_value_init(&ln2);
    sn_value_init(&Nf);
    sn_value_init(&t);
    sn_value_init(&xw);
    sn_value_init(&resw);

    e_work = x->e_bits;
    if (e_work < 16) e_work = 16;
    /* Extra guard digits: AGM cancels pi/(2*AGM) vs N*ln2; need headroom for
     * target residual ~1 ulp even when outer path already elevated. */
    m_work = x->m_bits + 80;
    if (m_work < x->m_bits + 48) m_work = x->m_bits + 48;
    if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
    nan_en = x->nan_enabled;

    st = sn_cast_float(ctx, &xw, x, e_work, m_work, nan_en, opt);
    if (st != SN_OK) goto done;

    /* N must exceed working m so asymptotic AGM error << 1 ulp at m_work. */
    N = m_work + 48;
    if (N < 80) N = 80;
    if (N > 100000) N = 100000;

    st = soft_pow2i(ctx, &scale, N, &xw, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &M, &xw, &scale, opt); if (st != SN_OK) goto done; /* M = x * 2^N */
    st = soft_from_d(ctx, &four, 4.0, &xw, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &b0, &four, &M, opt); if (st != SN_OK) goto done; /* 4/M */
    /* b0 ~ 2^{-N}; pass N as gap so soft_agm12 runs enough pre-quadratic steps. */
    st = soft_agm12(ctx, &agm, &b0, &xw, opt, N); if (st != SN_OK) goto done;
    st = soft_const_pi(ctx, &pi, &xw, opt); if (st != SN_OK) goto done;
    /* ln(M) ~ pi / (2*AGM(1, 4/M)) */
    st = sn_add(ctx, &agm, &agm, &agm, opt); if (st != SN_OK) goto done; /* 2*AGM */
    st = sn_div(ctx, &t, &pi, &agm, opt); if (st != SN_OK) goto done; /* pi/(2*AGM) */
    st = soft_const_ln2(ctx, &ln2, &xw, opt); if (st != SN_OK) goto done;
    st = sn_float_from_i64(ctx, &Nf, (int64_t)N, e_work, m_work, nan_en, opt);
    if (st != SN_OK) goto done;
    st = sn_mul(ctx, &Nf, &Nf, &ln2, opt); if (st != SN_OK) goto done; /* N*ln2 */
    st = sn_sub(ctx, &resw, &t, &Nf, opt); if (st != SN_OK) goto done;
    /* Round back to caller format. */
    st = sn_cast_float(ctx, out, &resw, x->e_bits, x->m_bits, x->nan_enabled, opt);
done:
    sn_value_clear(ctx, &four);
    sn_value_clear(ctx, &M);
    sn_value_clear(ctx, &scale);
    sn_value_clear(ctx, &b0);
    sn_value_clear(ctx, &agm);
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &ln2);
    sn_value_clear(ctx, &Nf);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &xw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_log(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value m, z, t, one, two, ln2, e_f, aw, resw;
    const sn_value *x;
    int e, cls, sign, e_orig, m_orig, nan_orig, elev = 0;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    sign = sn_fp_signbit(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
    }
    if (sign) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (cls == SN_FP_INFINITE)
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&m);
    sn_value_init(&z);
    sn_value_init(&t);
    sn_value_init(&one);
    sn_value_init(&two);
    sn_value_init(&ln2);
    sn_value_init(&e_f);
    sn_value_init(&aw);
    sn_value_init(&resw);

    x = a;
    /* Elevate all multiprec logs so final e*ln2 + cast keep full target digits.
     * AGM still has internal headroom; outer elev closes ~1-2 ulp residual at m>=200. */
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 64;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    /* log(x) = log(m) + e*ln2; frexp m in [0.5,1), shift to ~[sqrt(1/2), sqrt(2)]. */
    st = soft_frexp(ctx, &m, &e, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    {
        sn_value half_sqrt2, half;
        int rel = 0;
        sn_value_init(&half_sqrt2);
        sn_value_init(&half);
        if (x->m_bits <= 52) {
            st = soft_from_d(ctx, &half_sqrt2, 0.7071067811865476, x, opt);
        } else {
            st = soft_from_d(ctx, &half, 0.5, x, opt);
            if (st == SN_OK) st = sn_soft_sqrt(ctx, &half_sqrt2, &half, opt);
        }
        if (st == SN_OK) st = sn_cmp(ctx, &rel, &m, &half_sqrt2);
        if (st == SN_OK && rel < 0) {
            st = sn_mul(ctx, &m, &m, &two, opt);
            e -= 1;
        }
        sn_value_clear(ctx, &half_sqrt2);
        sn_value_clear(ctx, &half);
        if (st != SN_OK) goto done;
    }
    /* Exact power-of-two: m == 1 => log = e*ln2. */
    {
        int rel = 0;
        st = sn_cmp(ctx, &rel, &m, &one);
        if (st != SN_OK) goto done;
        if (rel == 0) {
            if (e == 0) {
                st = sn_float_set_zero(ctx, out, 0, e_orig, m_orig, nan_orig);
                goto done;
            }
            st = soft_const_ln2(ctx, &ln2, x, opt); if (st != SN_OK) goto done;
            st = sn_float_from_i64(ctx, &e_f, (int64_t)e, x->e_bits, x->m_bits, x->nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_mul(ctx, &resw, &e_f, &ln2, opt); if (st != SN_OK) goto done;
            if (elev)
                st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
            else
                st = sn_value_copy(ctx, out, &resw);
            goto done;
        }
    }
    /* Large multiprec: AGM log on reduced mantissa (elevates internally).
     * Use m_orig so outer elev does not force AGM for mid-precision artanh path. */
    if (m_orig >= SN_SOFT_LOG_AGM_MIN_M) {
        st = soft_log_agm(ctx, &resw, &m, opt); if (st != SN_OK) goto done;
        if (e != 0) {
            st = soft_const_ln2(ctx, &ln2, x, opt); if (st != SN_OK) goto done;
            st = sn_float_from_i64(ctx, &e_f, (int64_t)e, x->e_bits, x->m_bits, x->nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t, &e_f, &ln2, opt); if (st != SN_OK) goto done;
            st = sn_add(ctx, &resw, &resw, &t, opt); if (st != SN_OK) goto done;
        }
        if (elev)
            st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
        else
            st = sn_value_copy(ctx, out, &resw);
        goto done;
    }

    /* z = (m-1)/(m+1); log(m) = 2 artanh(z) */
    st = sn_sub(ctx, &t, &m, &one, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &z, &m, &one, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &z, &t, &z, opt); if (st != SN_OK) goto done;
    st = soft_artanh_series(ctx, &resw, &z, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &resw, &resw, &two, opt); if (st != SN_OK) goto done;

    if (e != 0) {
        st = soft_const_ln2(ctx, &ln2, x, opt); if (st != SN_OK) goto done;
        st = sn_float_from_i64(ctx, &e_f, (int64_t)e, x->e_bits, x->m_bits, x->nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &e_f, &ln2, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &resw, &resw, &t, opt); if (st != SN_OK) goto done;
    }
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &m);
    sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &ln2);
    sn_value_clear(ctx, &e_f);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

/* sin/cos series on reduced argument */
static sn_status soft_sin_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    /* sin x = x - x^3/3! + x^5/5! - ... */
    sn_status st;
    sn_value x2, term, sum, t, den;
    int n, max_terms, sign;

    sn_value_init(&x2);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&t);
    sn_value_init(&den);

    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, x); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, x); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 32) max_terms = 32;
    if (max_terms > 2000) max_terms = 2000;
    sign = -1;
    for (n = 1; n <= max_terms; n++) {
        /* term *= x2 / ((2n)*(2n+1)); exact integer denoms via from_i64 */
        st = sn_float_from_i64(ctx, &den, (int64_t)(2 * n) * (int64_t)(2 * n + 1),
                               x->e_bits, x->m_bits, x->nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &term, &x2, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &term, &t, &den, opt); if (st != SN_OK) goto done;
        if (sign < 0)
            st = sn_sub(ctx, &sum, &sum, &term, opt);
        else
            st = sn_add(ctx, &sum, &sum, &term, opt);
        if (st != SN_OK) goto done;
        sign = -sign;
        if (n > 6 && sn_fp_classify(&term) == SN_FP_ZERO) break;
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &den);
    return st;
}

static sn_status soft_cos_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    /* cos x = 1 - x^2/2! + x^4/4! - ... */
    sn_status st;
    sn_value x2, term, sum, t, den, one;
    int n, max_terms, sign;

    sn_value_init(&x2);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&t);
    sn_value_init(&den);
    sn_value_init(&one);

    st = sn_float_from_i64(ctx, &one, 1, x->e_bits, x->m_bits, x->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &one); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 32) max_terms = 32;
    if (max_terms > 2000) max_terms = 2000;
    sign = -1;
    for (n = 1; n <= max_terms; n++) {
        st = sn_float_from_i64(ctx, &den, (int64_t)(2 * n - 1) * (int64_t)(2 * n),
                               x->e_bits, x->m_bits, x->nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &term, &x2, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &term, &t, &den, opt); if (st != SN_OK) goto done;
        if (sign < 0)
            st = sn_sub(ctx, &sum, &sum, &term, opt);
        else
            st = sn_add(ctx, &sum, &sum, &term, opt);
        if (st != SN_OK) goto done;
        sign = -sign;
        if (n > 6 && sn_fp_classify(&term) == SN_FP_ZERO) break;
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &one);
    return st;
}

/*
 * Reduce x for sin/cos series (Cody-Waite style with extra guard digits):
 *   Work at m_work = m + guard so that 2*pi and the quotient keep enough bits
 *   when |x| is large; then fold into [-pi/2, pi/2] for Taylor.
 *   1) cast a, pi to working format
 *   2) r = frem(a_w, 2*pi_w)  pure soft IEEE-style rem
 *   3) if |r| > pi/2, fold and set *neg_cos for cos sign
 *   4) cast residual back to original format
 * Result |r| <= pi/2 so series converges well (incl. sin(pi)~0).
 */
static sn_status soft_reduce_trig(sn_ctx *ctx, sn_value *out, int *neg_cos,
                                  const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value pi, two_pi, half_pi, r, ar, t, aw, piw, fmt;
    int rel;
    int e_work, m_work, nan_en;
    int guard;

    if (neg_cos) *neg_cos = 0;

    sn_value_init(&pi);
    sn_value_init(&two_pi);
    sn_value_init(&half_pi);
    sn_value_init(&r);
    sn_value_init(&ar);
    sn_value_init(&t);
    sn_value_init(&aw);
    sn_value_init(&piw);
    sn_value_init(&fmt);

    /* Extra mantissa for argument reduction: enough that for |x| <~ 2^(m/2)
     * the residual keeps full target precision. Cap to avoid huge cost. */
    guard = a->m_bits / 2 + 24;
    if (guard < 32) guard = 32;
    if (guard > 128) guard = 128;
    m_work = a->m_bits + guard;
    if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
    e_work = a->e_bits;
    if (e_work < 15) e_work = 15;
    if (e_work > SN_FLOAT_E_MAX) e_work = SN_FLOAT_E_MAX;
    nan_en = a->nan_enabled;

    /* Dummy format carrier for soft_const_pi / soft_from_d */
    st = sn_float_set_zero(ctx, &fmt, 0, e_work, m_work, nan_en);
    if (st != SN_OK) goto done;

    st = soft_const_pi(ctx, &piw, &fmt, opt); if (st != SN_OK) goto done;
    st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_en, opt);
    if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &two_pi, 2.0, &fmt, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &two_pi, &two_pi, &piw, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half_pi, 0.5, &fmt, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &half_pi, &half_pi, &piw, opt); if (st != SN_OK) goto done;

    /* pure soft multiprec remainder at working precision */
    st = sn_frem(ctx, &r, &aw, &two_pi, opt); if (st != SN_OK) goto done;

    st = sn_abs(ctx, &ar, &r, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &ar, &half_pi); if (st != SN_OK) goto done;
    if (rel > 0) {
        /* |r| > pi/2: fold into [-pi/2, pi/2]; cos needs a sign flip */
        st = sn_sub(ctx, &t, &piw, &ar, opt); if (st != SN_OK) goto done;
        if (sn_fp_signbit(&r)) {
            st = sn_neg(ctx, &r, &t, opt); if (st != SN_OK) goto done;
        } else {
            sn_value_clear(ctx, &r);
            sn_value_move(&r, &t);
            sn_value_init(&t);
        }
        if (neg_cos) *neg_cos = 1;
    }

    /* Keep residual at working precision for series; caller casts result back. */
    st = sn_value_copy(ctx, out, &r);
done:
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &two_pi);
    sn_value_clear(ctx, &half_pi);
    sn_value_clear(ctx, &r);
    sn_value_clear(ctx, &ar);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &piw);
    sn_value_clear(ctx, &fmt);
    return st;
}

sn_status sn_soft_sin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value r, sw;
    int cls, neg_cos;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);

    sn_value_init(&r);
    sn_value_init(&sw);
    /* reduce at elevated precision; series on residual; cast result to target */
    st = soft_reduce_trig(ctx, &r, &neg_cos, a, opt); if (st != SN_OK) goto done;
    st = soft_sin_series(ctx, &sw, &r, opt); if (st != SN_OK) goto done;
    if (sw.m_bits != a->m_bits || sw.e_bits != a->e_bits)
        st = sn_cast_float(ctx, out, &sw, a->e_bits, a->m_bits, a->nan_enabled, opt);
    else
        st = sn_value_copy(ctx, out, &sw);
done:
    sn_value_clear(ctx, &r);
    sn_value_clear(ctx, &sw);
    return st;
}

sn_status sn_soft_cos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value r, cw;
    int cls, neg_cos;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (cls == SN_FP_ZERO)
        return soft_from_d(ctx, out, 1.0, a, opt);

    sn_value_init(&r);
    sn_value_init(&cw);
    st = soft_reduce_trig(ctx, &r, &neg_cos, a, opt); if (st != SN_OK) goto done;
    st = soft_cos_series(ctx, &cw, &r, opt); if (st != SN_OK) goto done;
    if (neg_cos) {
        st = sn_neg(ctx, &cw, &cw, opt); if (st != SN_OK) goto done;
    }
    if (cw.m_bits != a->m_bits || cw.e_bits != a->e_bits)
        st = sn_cast_float(ctx, out, &cw, a->e_bits, a->m_bits, a->nan_enabled, opt);
    else
        st = sn_value_copy(ctx, out, &cw);
done:
    sn_value_clear(ctx, &r);
    sn_value_clear(ctx, &cw);
    return st;
}

/* ---------- tan / asin / pow / hypot ---------- */

sn_status sn_soft_tan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value s, c, aw, resw;
    int cls;
    int e_orig, m_orig, nan_orig, elev = 0;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&s);
    sn_value_init(&c);
    sn_value_init(&aw);
    sn_value_init(&resw);

    x = a;
    /* Elevate so sin/cos residual + division keep target digits near poles. */
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = sn_soft_sin(ctx, &s, x, opt); if (st != SN_OK) goto done;
    st = sn_soft_cos(ctx, &c, x, opt); if (st != SN_OK) goto done;
    if (sn_fp_classify(&c) == SN_FP_ZERO) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        st = sn_float_set_inf(ctx, out, sn_fp_signbit(&s), e_orig, m_orig, nan_orig);
        goto done;
    }
    st = sn_div(ctx, &resw, &s, &c, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

/* asin series: asin(x) = x + sum (binom) 闂?use atan form:
 * asin(x) = atan(x / sqrt(1-x^2)) for |x|<1; 闂傚倸鍊搁崐鎼佸磹閹间礁纾归柟闂寸绾惧綊鏌熼梻瀵割槮缁炬儳缍婇弻鐔兼⒒鐎靛壊妲紒鎯у⒔閹虫捇鈥旈崘顏佸亾閿濆簼绨奸柟鐧哥秮閺岋綁顢橀悙鎼闂侀潧妫欑敮鎺楋綖濠靛鏅查柛娑卞墮椤ユ艾鈹戞幊閸婃鎱ㄩ悜钘夌；婵炴垟鎳為崶顒佸仺缂佸瀵ч悗顒勬⒑閻熸澘鈷旂紒顕呭灦瀹曟垿骞囬婊€绨婚梺鍝勫暙閸婂綊宕甸埀顒佺箾鐎涙鐭掔紒鐘崇墵瀵鈽夐姀鐘电杸闂傚倸鐗婄粙鎴犵不婵犳碍鈷戞慨鐟版搐閳ь剚鍔欏畷鎴﹀箻缂佹ǚ鎷绘繛杈剧到閹诧紕鎷归敓鐘崇厱閻庯綆鍋呭畷宀€鈧娲樼换鍡浰囩€电硶鍋撻崹顐ｇ凡闁挎洏鍎崇划瀣箳閺傚搫浜鹃柨婵嗘噹椤ｅ磭绱?2 at 闂?.
 * Implement atan via series for |u|<=1. */
static sn_status soft_atan_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    /* atan x via Taylor after range reduction:
     *  |x|>1  -> pi/2 - atan(1/|x|)
     *  |x|>=1/2 -> pi/4 + atan((|x|-1)/(|x|+1))
     *  then half-angle: atan(u)=2*atan(u/(1+sqrt(1+u^2))) while |u|>=0.2
     *  then sum (-1)^n z^{2n+1}/(2n+1).
     */
    sn_status st;
    sn_value x2, term, sum, t, den, ax, one, half_pi, half, z, num, denv, thr;
    int i, max_terms, neg, flip, add_pi4, rel, halves, pass;

    sn_value_init(&x2);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&t);
    sn_value_init(&den);
    sn_value_init(&ax);
    sn_value_init(&one);
    sn_value_init(&half_pi);
    sn_value_init(&half);
    sn_value_init(&z);
    sn_value_init(&num);
    sn_value_init(&denv);
    sn_value_init(&thr);

    st = sn_abs(ctx, &ax, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, x, opt); if (st != SN_OK) goto done;
    neg = sn_fp_signbit(x);
    flip = 0;
    add_pi4 = 0;
    halves = 0;

    st = sn_cmp(ctx, &rel, &ax, &one); if (st != SN_OK) goto done;
    if (rel == 0) {
        /* atan(+/-1) = +/- pi/4 */
        st = soft_const_pi(ctx, &half_pi, x, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &t, 0.25, x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &sum, &half_pi, &t, opt); if (st != SN_OK) goto done;
        if (neg) st = sn_neg(ctx, out, &sum, opt);
        else st = sn_value_copy(ctx, out, &sum);
        goto done;
    }
    if (rel > 0) {
        /* atan x = pi/2 - atan(1/x) for x>0 */
        flip = 1;
        st = sn_div(ctx, &t, &one, &ax, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &ax); sn_value_move(&ax, &t); sn_value_init(&t);
    }

    /* Reduce |ax| >= 1/2 via pi/4 identity for fast series. */
    st = sn_cmp(ctx, &rel, &ax, &half); if (st != SN_OK) goto done;
    if (rel >= 0) {
        /* z = (ax-1)/(ax+1); atan(ax)=pi/4+atan(z) */
        add_pi4 = 1;
        st = sn_sub(ctx, &num, &ax, &one, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &denv, &ax, &one, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &z, &num, &denv, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &ax); sn_value_move(&ax, &z); sn_value_init(&z);
    }

    /* Half-angle reductions for remaining |ax|. */
    st = soft_from_d(ctx, &thr, 0.2, x, opt); if (st != SN_OK) goto done;
    for (pass = 0; pass < 12; pass++) {
        if (sn_fp_classify(&ax) == SN_FP_ZERO) break;
        st = sn_cmp(ctx, &rel, &ax, &thr); if (st != SN_OK) goto done;
        if (rel < 0) break;
        st = sn_mul(ctx, &x2, &ax, &ax, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &num, &one, &x2, opt); if (st != SN_OK) goto done;
        st = sn_soft_sqrt(ctx, &num, &num, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &denv, &one, &num, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &ax, &denv, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &ax); sn_value_move(&ax, &t); sn_value_init(&t);
        halves++;
    }

    if (sn_fp_classify(&ax) == SN_FP_ZERO) {
        st = sn_float_set_zero(ctx, &sum, 0, x->e_bits, x->m_bits, x->nan_enabled);
        if (st != SN_OK) goto done;
    } else {
        st = sn_mul(ctx, &x2, &ax, &ax, opt); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &term, &ax); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &sum, &ax); if (st != SN_OK) goto done;
        max_terms = x->m_bits / 3 + 24;
        if (max_terms < 24) max_terms = 24;
        if (max_terms > 2000) max_terms = 2000;
        for (i = 1; i <= max_terms; i++) {
            st = sn_mul(ctx, &t, &term, &x2, opt); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &term); sn_value_move(&term, &t); sn_value_init(&t);
            st = sn_float_from_i64(ctx, &den, (int64_t)(2 * i + 1),
                                   x->e_bits, x->m_bits, x->nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_div(ctx, &t, &term, &den, opt); if (st != SN_OK) goto done;
            if (i & 1)
                st = sn_sub(ctx, &sum, &sum, &t, opt);
            else
                st = sn_add(ctx, &sum, &sum, &t, opt);
            if (st != SN_OK) goto done;
            if (i > 6) {
                sn_value sc, as, at, thr;
                int r2 = 0;
                sn_value_init(&sc); sn_value_init(&as); sn_value_init(&at); sn_value_init(&thr);
                if (soft_pow2i(ctx, &sc, -(x->m_bits + 4), x, opt) == SN_OK &&
                    sn_abs(ctx, &as, &sum, opt) == SN_OK &&
                    sn_mul(ctx, &thr, &as, &sc, opt) == SN_OK &&
                    sn_abs(ctx, &at, &t, opt) == SN_OK &&
                    sn_cmp(ctx, &r2, &at, &thr) == SN_OK && r2 < 0) {
                    sn_value_clear(ctx, &sc); sn_value_clear(ctx, &as); sn_value_clear(ctx, &at); sn_value_clear(ctx, &thr);
                    break;
                }
                sn_value_clear(ctx, &sc); sn_value_clear(ctx, &as); sn_value_clear(ctx, &at); sn_value_clear(ctx, &thr);
                if (sn_fp_classify(&t) == SN_FP_ZERO) break;
            }
        }
    }

    /* Undo half-angles: sum *= 2^halves */
    while (halves-- > 0) {
        st = sn_add(ctx, &sum, &sum, &sum, opt); if (st != SN_OK) goto done;
    }
    if (add_pi4) {
        st = soft_const_pi(ctx, &half_pi, x, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &t, 0.25, x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &half_pi, &half_pi, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &half_pi, &sum, opt); if (st != SN_OK) goto done;
    }
    if (flip) {
        st = soft_const_pi(ctx, &half_pi, x, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &t, 0.5, x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &half_pi, &half_pi, &t, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &sum, &half_pi, &sum, opt); if (st != SN_OK) goto done;
    }
    if (neg)
        st = sn_neg(ctx, out, &sum, opt);
    else
        st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &half_pi);
    sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &num);
    sn_value_clear(ctx, &denv);
    sn_value_clear(ctx, &thr);
    return st;
}

sn_status sn_soft_asin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, x2, t, s, half_pi, aw, resw;
    int cls, rel;
    int e_orig, m_orig, nan_orig, elev = 0;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&one);
    sn_value_init(&x2);
    sn_value_init(&t);
    sn_value_init(&s);
    sn_value_init(&half_pi);
    sn_value_init(&aw);
    sn_value_init(&resw);

    /* Domain check on original format first. */
    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &t, a, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &t, &one); if (st != SN_OK) goto done;
    if (rel > 0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            st = sn_float_set_inf(ctx, out, 0, e_orig, m_orig, nan_orig);
        else
            st = sn_float_set_nan(ctx, out, e_orig, m_orig);
        goto done;
    }
    if (rel == 0) {
        /* +/- pi/2 */
        st = soft_const_pi(ctx, &half_pi, a, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &t, 0.5, a, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, out, &half_pi, &t, opt); if (st != SN_OK) goto done;
        if (sn_fp_signbit(a))
            st = sn_neg(ctx, out, out, opt);
        goto done;
    }

    /* Elevate multiprec so sqrt(1-x^2) + atan keep full target digits. */
    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
            sn_value_clear(ctx, &one);
            sn_value_init(&one);
            st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
        }
    }

    /* Near |x|~1: atan(x/sqrt(1-x^2)) loses digits; use
     * asin(x) = sign(x) * (pi/2 - 2*atan(sqrt((1-|x|)/(1+|x|)))). */
    {
        sn_value thr_near, ax;
        int rnear = 0;
        sn_value_init(&thr_near);
        sn_value_init(&ax);
        st = soft_from_d(ctx, &thr_near, 0.875, x, opt);
        if (st == SN_OK) st = sn_abs(ctx, &ax, x, opt);
        if (st == SN_OK) st = sn_cmp(ctx, &rnear, &ax, &thr_near);
        if (st == SN_OK && rnear >= 0) {
            sn_value num, den, q, two, local;
            sn_value_init(&num); sn_value_init(&den); sn_value_init(&q);
            sn_value_init(&two); sn_value_init(&local);
            st = soft_const_pi(ctx, &half_pi, x, opt);
            if (st == SN_OK) st = soft_from_d(ctx, &t, 0.5, x, opt);
            if (st == SN_OK) st = sn_mul(ctx, &half_pi, &half_pi, &t, opt);
            if (st == SN_OK) st = soft_from_d(ctx, &two, 2.0, x, opt);
            if (st == SN_OK) st = sn_sub(ctx, &num, &one, &ax, opt);
            if (st == SN_OK) st = sn_add(ctx, &den, &one, &ax, opt);
            if (st == SN_OK) st = sn_div(ctx, &q, &num, &den, opt);
            if (st == SN_OK) st = sn_soft_sqrt(ctx, &s, &q, opt);
            if (st == SN_OK) st = soft_atan_series(ctx, &t, &s, opt);
            if (st == SN_OK) st = sn_mul(ctx, &t, &t, &two, opt);
            if (st == SN_OK) st = sn_sub(ctx, &local, &half_pi, &t, opt);
            if (st == SN_OK && sn_fp_signbit(x))
                st = sn_neg(ctx, &local, &local, opt);
            if (st == SN_OK) {
                if (elev)
                    st = sn_cast_float(ctx, out, &local, e_orig, m_orig, nan_orig, opt);
                else
                    st = sn_value_copy(ctx, out, &local);
            }
            sn_value_clear(ctx, &num); sn_value_clear(ctx, &den); sn_value_clear(ctx, &q);
            sn_value_clear(ctx, &two); sn_value_clear(ctx, &local);
            sn_value_clear(ctx, &thr_near); sn_value_clear(ctx, &ax);
            goto done;
        }
        sn_value_clear(ctx, &thr_near);
        sn_value_clear(ctx, &ax);
        if (st != SN_OK) goto done;
    }

    /* asin x = atan(x/sqrt(1-x^2)) for moderate |x| */
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &one, &x2, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done;
    if (sn_fp_classify(&s) == SN_FP_ZERO) {
        st = soft_const_pi(ctx, &half_pi, x, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &t, 0.5, x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &resw, &half_pi, &t, opt); if (st != SN_OK) goto done;
        if (sn_fp_signbit(x)) st = sn_neg(ctx, &resw, &resw, opt);
        if (st == SN_OK) {
            if (elev)
                st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
            else
                st = sn_value_copy(ctx, out, &resw);
        }
        goto done;
    }
    st = sn_div(ctx, &t, x, &s, opt); if (st != SN_OK) goto done;
    st = soft_atan_series(ctx, &resw, &t, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &half_pi);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

/* Integer power via binary exponentiation (exact multiplies; no exp/log). */
static sn_status soft_pow_int(sn_ctx *ctx, sn_value *out, const sn_value *a, int64_t n,
                              const sn_op_opt *opt)
{
    sn_status st;
    sn_value base, res, t;
    int64_t e;
    int neg_exp = 0;

    sn_value_init(&base);
    sn_value_init(&res);
    sn_value_init(&t);

    if (n == 0)
        return soft_from_d(ctx, out, 1.0, a, opt);
    if (n < 0) {
        neg_exp = 1;
        /* careful: n = INT64_MIN not handled; exponents from float won't hit that */
        if (n == INT64_MIN) {
            st = SN_ERR_RANGE;
            goto done;
        }
        e = -n;
    } else {
        e = n;
    }

    st = sn_value_copy(ctx, &base, a); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &res, 1.0, a, opt); if (st != SN_OK) goto done;
    while (e > 0) {
        if (e & 1) {
            st = sn_mul(ctx, &t, &res, &base, opt); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &res); sn_value_move(&res, &t); sn_value_init(&t);
        }
        e >>= 1;
        if (e) {
            st = sn_mul(ctx, &t, &base, &base, opt); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &base); sn_value_move(&base, &t); sn_value_init(&t);
        }
    }
    if (neg_exp) {
        sn_value one;
        sn_value_init(&one);
        st = soft_from_d(ctx, &one, 1.0, a, opt);
        if (st == SN_OK) st = sn_div(ctx, out, &one, &res, opt);
        sn_value_clear(ctx, &one);
    } else {
        st = sn_value_copy(ctx, out, &res);
    }
done:
    sn_value_clear(ctx, &base);
    sn_value_clear(ctx, &res);
    sn_value_clear(ctx, &t);
    return st;
}

sn_status sn_soft_pow(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    /* a^b = exp(b * log(a)) for a>0; integer b uses binary pow for accuracy/speed. */
    sn_status st;
    sn_value t, la, bi;
    int ca, cb, sign_a, rel;
    sn_value one;
    double bd;
    int64_t ni;

    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;

    ca = sn_fp_classify(a);
    cb = sn_fp_classify(b);
    sign_a = sn_fp_signbit(a);

    sn_value_init(&t);
    sn_value_init(&la);
    sn_value_init(&one);
    sn_value_init(&bi);

    /* b==0 -> 1 */
    if (cb == SN_FP_ZERO) {
        st = soft_from_d(ctx, out, 1.0, a, opt);
        goto done;
    }
    /* a==1 -> 1 */
    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, a, &one);
    if (st == SN_OK && rel == 0 && !sign_a) {
        st = sn_value_copy(ctx, out, &one);
        goto done;
    }
    /* a==0 */
    if (ca == SN_FP_ZERO) {
        if (sn_fp_signbit(b)) {
            sn_raise(ctx, SN_FLAG_DIVZERO);
            st = sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        } else {
            st = sn_float_set_zero(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        }
        goto done;
    }
    if (ca == SN_FP_NAN || cb == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        st = sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        if (!a->nan_enabled)
            st = sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        goto done;
    }

    /* Integer exponent path (incl. negative base with integer exp).
     * Detect integer via soft round + cmp so multiprec does not depend on host double. */
    if (cb == SN_FP_NORMAL || cb == SN_FP_SUBNORMAL) {
        int rel_int = 0;
        sn_value br;
        sn_value_init(&br);
        st = sn_fround(ctx, &br, b, opt);
        if (st == SN_OK) st = sn_cmp(ctx, &rel_int, b, &br);
        if (st == SN_OK && rel_int == 0) {
            st = sn_to_i64(ctx, &br, &ni);
            if (st == SN_OK) {
                if (sign_a) {
                    st = soft_pow_int(ctx, out, a, ni, opt);
                    sn_value_clear(ctx, &br);
                    goto done;
                }
                if (ni >= -1000000 && ni <= 1000000) {
                    st = soft_pow_int(ctx, out, a, ni, opt);
                    sn_value_clear(ctx, &br);
                    goto done;
                }
            } else {
                /* |b| huge integer: fall through to exp/log if a>0 */
                st = SN_OK;
            }
        }
        sn_value_clear(ctx, &br);
        if (st != SN_OK) goto done;
        /* narrow host fallback for near-integer when soft cmp missed due to format */
        if (!sn_math_need_soft(b)) {
            st = sn_to_double(ctx, b, &bd);
            if (st == SN_OK && isfinite(bd) && fabs(bd) <= 1e15) {
                ni = (int64_t)(bd >= 0.0 ? floor(bd + 0.5) : ceil(bd - 0.5));
                if (fabs(bd - (double)ni) < 1e-12) {
                    if (sign_a) {
                        st = soft_pow_int(ctx, out, a, ni, opt);
                        goto done;
                    }
                    if (ni >= -1000000 && ni <= 1000000) {
                        st = soft_pow_int(ctx, out, a, ni, opt);
                        goto done;
                    }
                }
            }
        }
    }

    if (sign_a) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            st = sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        else
            st = sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        goto done;
    }

    /* Multiprec compose elev: exp(b*log(a)) needs headroom beyond inner log/exp elev. */
    {
        sn_value aw, bw, resw;
        const sn_value *ap = a, *bp = b;
        int e_orig = a->e_bits, m_orig = a->m_bits, nan_orig = a->nan_enabled;
        int elev = 0;

        sn_value_init(&aw);
        sn_value_init(&bw);
        sn_value_init(&resw);

        if (m_orig > 52) {
            int e_work = e_orig < 16 ? 16 : e_orig;
            /* Compose exp(b*log(a)): need extra guard for large |b| / a~1 cancellation. */
            int m_work = m_orig + 128;
            if (m_work < m_orig + 96) m_work = m_orig + 96;
            if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
            if (m_work > m_orig) {
                st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
                if (st == SN_OK)
                    st = sn_cast_float(ctx, &bw, b, e_work, m_work, nan_orig, opt);
                if (st == SN_OK) {
                    ap = &aw;
                    bp = &bw;
                    elev = 1;
                }
            }
        }
        if (st == SN_OK) {
            /* Near-1 base: log1p(a-1) avoids catastrophic cancellation in log(a).
             * Use work-format 1.0 (ap), not outer `one` which may have lower m_bits. */
            {
                sn_value diff, thr_near, one_w, absd;
                int reln = 0;
                sn_value_init(&diff);
                sn_value_init(&thr_near);
                sn_value_init(&one_w);
                sn_value_init(&absd);
                st = soft_from_d(ctx, &one_w, 1.0, ap, opt);
                if (st == SN_OK) st = sn_sub(ctx, &diff, ap, &one_w, opt);
                if (st == SN_OK) st = sn_abs(ctx, &absd, &diff, opt);
                if (st == SN_OK) st = soft_from_d(ctx, &thr_near, 0.25, ap, opt);
                if (st == SN_OK) st = sn_cmp(ctx, &reln, &absd, &thr_near);
                if (st == SN_OK && reln < 0) {
                    /* |a-1| < 1/4 : keep signed (a-1) in diff */
                    st = sn_soft_log1p(ctx, &la, &diff, opt);
                } else if (st == SN_OK) {
                    st = sn_soft_log(ctx, &la, ap, opt);
                }
                sn_value_clear(ctx, &diff);
                sn_value_clear(ctx, &thr_near);
                sn_value_clear(ctx, &one_w);
                sn_value_clear(ctx, &absd);
            }
            if (st == SN_OK) st = sn_mul(ctx, &t, bp, &la, opt);
            if (st == SN_OK) st = sn_soft_exp(ctx, elev ? &resw : out, &t, opt);
            if (st == SN_OK && elev)
                st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
        }
        sn_value_clear(ctx, &aw);
        sn_value_clear(ctx, &bw);
        sn_value_clear(ctx, &resw);
    }
done:
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &la);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &bi);
    return st;
}

sn_status sn_soft_hypot(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_status st;
    sn_value aa, bb, s, t;
    int ca, cb;

    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;

    ca = sn_fp_classify(a);
    cb = sn_fp_classify(b);
    if (ca == SN_FP_INFINITE || cb == SN_FP_INFINITE)
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    if (ca == SN_FP_NAN || cb == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }

    sn_value_init(&aa);
    sn_value_init(&bb);
    sn_value_init(&s);
    sn_value_init(&t);
    st = sn_abs(ctx, &aa, a, opt); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &bb, b, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &s, &aa, &aa, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &bb, &bb, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, out, &s, opt);
done:
    sn_value_clear(ctx, &aa);
    sn_value_clear(ctx, &bb);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &t);
    return st;
}

/* ---------- inverse trig (continued) & hyperbolics ---------- */

sn_status sn_soft_atan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value aw, resw;
    int cls, elev = 0;
    int e_orig, m_orig, nan_orig;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);
    if (cls == SN_FP_INFINITE) {
        sn_value pi, half;
        sn_value_init(&pi);
        sn_value_init(&half);
        st = soft_const_pi(ctx, &pi, a, opt);
        if (st == SN_OK) st = soft_from_d(ctx, &half, 0.5, a, opt);
        if (st == SN_OK) st = sn_mul(ctx, out, &pi, &half, opt);
        if (st == SN_OK && sn_fp_signbit(a))
            st = sn_neg(ctx, out, out, opt);
        sn_value_clear(ctx, &pi);
        sn_value_clear(ctx, &half);
        return st;
    }

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&aw);
    sn_value_init(&resw);
    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 32;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) { sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw); return st; }
            x = &aw;
            elev = 1;
        }
    }
    st = soft_atan_series(ctx, elev ? &resw : out, x, opt);
    if (st == SN_OK && elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_acos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* acos(x) = pi/2 - asin(x); elevate multiprec so subtraction keeps digits. */
    sn_status st;
    sn_value as, pi, half, one, ax, aw, resw;
    int cls, rel;
    int e_orig, m_orig, nan_orig, elev = 0;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&as);
    sn_value_init(&pi);
    sn_value_init(&half);
    sn_value_init(&one);
    sn_value_init(&ax);
    sn_value_init(&aw);
    sn_value_init(&resw);

    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &ax, a, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &ax, &one); if (st != SN_OK) goto done;
    if (rel > 0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            st = sn_float_set_inf(ctx, out, 0, e_orig, m_orig, nan_orig);
        else
            st = sn_float_set_nan(ctx, out, e_orig, m_orig);
        goto done;
    }

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = sn_soft_asin(ctx, &as, x, opt); if (st != SN_OK) goto done;
    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &pi, &pi, &half, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &resw, &pi, &as, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &as);
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_atan2(sn_ctx *ctx, sn_value *out, const sn_value *y, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value q, pi, t;
    int cy, cx, sy, sx;

    if (!y || !x || y->kind != SN_KIND_FLOAT || x->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (y->e_bits != x->e_bits || y->m_bits != x->m_bits) return SN_ERR_TYPE;

    cy = sn_fp_classify(y);
    cx = sn_fp_classify(x);
    sy = sn_fp_signbit(y);
    sx = sn_fp_signbit(x);

    if (cy == SN_FP_NAN || cx == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!y->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, y->e_bits, y->m_bits, y->nan_enabled);
        return sn_float_set_nan(ctx, out, y->e_bits, y->m_bits);
    }

    sn_value_init(&q);
    sn_value_init(&pi);
    sn_value_init(&t);

    /* both zero */
    if (cy == SN_FP_ZERO && cx == SN_FP_ZERO) {
        if (!sx) {
            st = sn_float_set_zero(ctx, out, sy, y->e_bits, y->m_bits, y->nan_enabled);
        } else {
            st = soft_const_pi(ctx, out, y, opt);
            if (st == SN_OK && sy) st = sn_neg(ctx, out, out, opt);
        }
        goto done;
    }

    /* x == 0, y != 0 -> +/- pi/2 */
    if (cx == SN_FP_ZERO) {
        st = soft_const_pi(ctx, &pi, y, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &t, 0.5, y, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, out, &pi, &t, opt); if (st != SN_OK) goto done;
        if (sy) st = sn_neg(ctx, out, out, opt);
        goto done;
    }

    /* y == 0, x != 0 -> 0 or +/- pi */
    if (cy == SN_FP_ZERO) {
        if (!sx) {
            st = sn_float_set_zero(ctx, out, sy, y->e_bits, y->m_bits, y->nan_enabled);
        } else {
            st = soft_const_pi(ctx, out, y, opt);
            if (st == SN_OK && sy) st = sn_neg(ctx, out, out, opt);
        }
        goto done;
    }

    /* infinities */
    if (cy == SN_FP_INFINITE || cx == SN_FP_INFINITE) {
        st = soft_const_pi(ctx, &pi, y, opt); if (st != SN_OK) goto done;
        if (cy == SN_FP_INFINITE && cx == SN_FP_INFINITE) {
            /* +/- 3pi/4 or +/- pi/4 */
            st = soft_from_d(ctx, &t, sx ? 0.75 : 0.25, y, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, out, &pi, &t, opt); if (st != SN_OK) goto done;
            if (sy) st = sn_neg(ctx, out, out, opt);
            goto done;
        }
        if (cy == SN_FP_INFINITE) {
            st = soft_from_d(ctx, &t, 0.5, y, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, out, &pi, &t, opt); if (st != SN_OK) goto done;
            if (sy) st = sn_neg(ctx, out, out, opt);
            goto done;
        }
        /* |x| inf, y finite */
        if (!sx) {
            st = sn_float_set_zero(ctx, out, sy, y->e_bits, y->m_bits, y->nan_enabled);
        } else {
            st = sn_value_copy(ctx, out, &pi);
            if (st == SN_OK && sy) st = sn_neg(ctx, out, out, opt);
        }
        goto done;
    }

    /* general: atan(|y|/|x|) then adjust quadrant */
    {
        sn_value ay, ax;
        sn_value_init(&ay);
        sn_value_init(&ax);
        st = sn_abs(ctx, &ay, y, opt);
        if (st == SN_OK) st = sn_abs(ctx, &ax, x, opt);
        if (st == SN_OK) st = sn_div(ctx, &q, &ay, &ax, opt);
        if (st == SN_OK) st = soft_atan_series(ctx, out, &q, opt);
        sn_value_clear(ctx, &ay);
        sn_value_clear(ctx, &ax);
        if (st != SN_OK) goto done;
    }

    if (!sx) {
        /* Q1 or Q4 */
        if (sy) st = sn_neg(ctx, out, out, opt);
    } else {
        /* Q2 or Q3: +/- (pi - atan) */
        st = soft_const_pi(ctx, &pi, y, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &t, &pi, out, opt); if (st != SN_OK) goto done;
        if (sy)
            st = sn_neg(ctx, out, &t, opt);
        else
            st = sn_value_copy(ctx, out, &t);
    }
done:
    sn_value_clear(ctx, &q);
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &t);
    return st;
}

sn_status sn_soft_sinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* sinh x = (e^x - e^{-x}) / 2; small |x| series; multiprec elevates. */
    sn_status st;
    sn_value ex, en, t, two, aw, resw, term, sum, k, x2, ax, thr;
    int cls, elev = 0, e_orig, m_orig, nan_orig, i, max_terms, rel;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO || cls == SN_FP_INFINITE)
        return sn_value_copy(ctx, out, a);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&ex);
    sn_value_init(&en);
    sn_value_init(&t);
    sn_value_init(&two);
    sn_value_init(&aw);
    sn_value_init(&resw);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&k);
    sn_value_init(&x2);
    sn_value_init(&ax);
    sn_value_init(&thr);
    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    /* series for |x| < 0.5: sinh x = x + x^3/3! + x^5/5! + ... */
    st = sn_abs(ctx, &ax, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &thr, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &ax, &thr); if (st != SN_OK) goto done;
    if (rel < 0) {
        st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &term, x); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &sum, x); if (st != SN_OK) goto done;
        max_terms = x->m_bits / 2 + 40;
        if (max_terms < 40) max_terms = 40;
        if (max_terms > 2000) max_terms = 2000;
        for (i = 1; i <= max_terms; i++) {
            /* term *= x^2 / ((2i)(2i+1)) */
            st = sn_float_from_i64(ctx, &k, (int64_t)(2 * i), x->e_bits, x->m_bits, x->nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t, &term, &x2, opt); if (st != SN_OK) goto done;
            st = sn_div(ctx, &term, &t, &k, opt); if (st != SN_OK) goto done;
            st = sn_float_from_i64(ctx, &k, (int64_t)(2 * i + 1), x->e_bits, x->m_bits, x->nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_div(ctx, &term, &term, &k, opt); if (st != SN_OK) goto done;
            st = sn_add(ctx, &sum, &sum, &term, opt); if (st != SN_OK) goto done;
            if (i > 4 && sn_fp_classify(&term) == SN_FP_ZERO) break;
        }
        if (elev)
            st = sn_cast_float(ctx, out, &sum, e_orig, m_orig, nan_orig, opt);
        else
            st = sn_value_copy(ctx, out, &sum);
        goto done;
    }

    st = sn_soft_exp(ctx, &ex, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &en, &two, &ex, opt); if (st != SN_OK) goto done; /* e^{-x} */
    st = sn_sub(ctx, &t, &ex, &en, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &resw, &t, &two, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &ex);
    sn_value_clear(ctx, &en);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &thr);
    return st;
}

sn_status sn_soft_cosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* cosh x = (e^x + e^{-x}) / 2; multiprec elevates. */
    sn_status st;
    sn_value ex, en, t, two, ax, aw, resw;
    int cls, elev = 0, e_orig, m_orig, nan_orig;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE)
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    if (cls == SN_FP_ZERO)
        return soft_from_d(ctx, out, 1.0, a, opt);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&ex);
    sn_value_init(&en);
    sn_value_init(&t);
    sn_value_init(&two);
    sn_value_init(&ax);
    sn_value_init(&aw);
    sn_value_init(&resw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    /* even function: use |x| for exp */
    st = sn_abs(ctx, &ax, x, opt); if (st != SN_OK) goto done;
    st = sn_soft_exp(ctx, &ex, &ax, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &en, &two, &ex, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &ex, &en, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &resw, &t, &two, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &ex);
    sn_value_clear(ctx, &en);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_tanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* tanh x = sinh/cosh; multiprec elevates; large |x| ~ sign(x) */
    sn_status st;
    sn_value s, c, aw, resw;
    int cls, elev = 0, e_orig, m_orig, nan_orig;
    const sn_value *x;
    double da;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);
    if (cls == SN_FP_INFINITE)
        return soft_from_d(ctx, out, sn_fp_signbit(a) ? -1.0 : 1.0, a, opt);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    /* |x| large: exp overflows -> clamp to +/-1 (use original width for host seed) */
    st = sn_to_double(ctx, a, &da);
    if (st == SN_OK && fabs(da) > 40.0)
        return soft_from_d(ctx, out, da < 0 ? -1.0 : 1.0, a, opt);

    sn_value_init(&s);
    sn_value_init(&c);
    sn_value_init(&aw);
    sn_value_init(&resw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = sn_soft_sinh(ctx, &s, x, opt); if (st != SN_OK) goto done;
    st = sn_soft_cosh(ctx, &c, x, opt); if (st != SN_OK) goto done;
    if (sn_fp_classify(&c) == SN_FP_ZERO) {
        st = soft_from_d(ctx, out, sn_fp_signbit(a) ? -1.0 : 1.0, a, opt);
        goto done;
    }
    st = sn_div(ctx, &resw, &s, &c, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_asinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* asinh x = log(x + sqrt(x^2 + 1)); multiprec elevates for cancel/sqrt. */
    sn_status st;
    sn_value one, x2, t, s, aw, resw;
    int cls, elev = 0, e_orig, m_orig, nan_orig;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO || cls == SN_FP_INFINITE)
        return sn_value_copy(ctx, out, a);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&one);
    sn_value_init(&x2);
    sn_value_init(&t);
    sn_value_init(&s);
    sn_value_init(&aw);
    sn_value_init(&resw);
    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &x2, &one, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, x, &s, opt); if (st != SN_OK) goto done;
    /* if t <= 0 due to cancellation for large negative, use asinh x = -asinh(-x) */
    if (sn_fp_signbit(&t) || sn_fp_classify(&t) == SN_FP_ZERO) {
        sn_value na, tmp;
        sn_value_init(&na);
        sn_value_init(&tmp);
        st = sn_neg(ctx, &na, x, opt);
        if (st == SN_OK) st = sn_soft_asinh(ctx, &tmp, &na, opt);
        if (st == SN_OK) st = sn_neg(ctx, &resw, &tmp, opt);
        sn_value_clear(ctx, &na);
        sn_value_clear(ctx, &tmp);
        if (st != SN_OK) goto done;
        if (elev)
            st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
        else
            st = sn_value_copy(ctx, out, &resw);
        goto done;
    }
    st = sn_soft_log(ctx, &resw, &t, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_acosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* acosh x = log(x + sqrt(x^2 - 1)), x >= 1; multiprec elevates. */
    sn_status st;
    sn_value one, x2, t, s, aw, resw;
    int cls, rel, elev = 0, e_orig, m_orig, nan_orig;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        if (sn_fp_signbit(a)) {
            sn_raise(ctx, SN_FLAG_INVALID);
            if (!a->nan_enabled)
                return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
            return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        }
        return sn_value_copy(ctx, out, a);
    }

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&one);
    sn_value_init(&x2);
    sn_value_init(&t);
    sn_value_init(&s);
    sn_value_init(&aw);
    sn_value_init(&resw);

    /* domain checks at original precision first */
    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, a, &one); if (st != SN_OK) goto done;
    if (rel < 0 || sn_fp_signbit(a)) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            st = sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        else
            st = sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        goto done;
    }
    if (rel == 0) {
        st = sn_float_set_zero(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        goto done;
    }

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
            /* refresh one at elevated width */
            st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
        }
    }

    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &x2, &one, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, x, &s, opt); if (st != SN_OK) goto done;
    st = sn_soft_log(ctx, &resw, &t, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_atanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* atanh x = 0.5 * log((1+x)/(1-x)), |x| < 1; multiprec elevates. */
    sn_status st;
    sn_value one, num, den, t, half, aw, resw, ax;
    int cls, rel, elev = 0, e_orig, m_orig, nan_orig;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);
    if (cls == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&one);
    sn_value_init(&num);
    sn_value_init(&den);
    sn_value_init(&t);
    sn_value_init(&half);
    sn_value_init(&aw);
    sn_value_init(&resw);
    sn_value_init(&ax);

    /* domain at original precision */
    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &ax, a, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &ax, &one); if (st != SN_OK) goto done;
    if (rel > 0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            st = sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        else
            st = sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        goto done;
    }
    if (rel == 0) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        st = sn_float_set_inf(ctx, out, sn_fp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
        goto done;
    }

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
            st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
        }
    }

    st = sn_add(ctx, &num, &one, x, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &den, &one, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &num, &den, opt); if (st != SN_OK) goto done;
    st = sn_soft_log(ctx, &t, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &resw, &t, &half, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &num);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    sn_value_clear(ctx, &ax);
    return st;
}


/* -------------------------------------------------------------------------- */
/* erf / erfc  (series + asymptotic complementary expansion)                   */
/* -------------------------------------------------------------------------- */

/* erf(x) = (2/sqrt(pi)) * sum_{n=0} (-1)^n x^{2n+1} / (n! (2n+1)), |x| modest */
static sn_status soft_erf_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value x2, term, sum, t, den, n_f, one, two, pi, s, fac;
    int n, max_n, rel;
    double dt;

    sn_value_init(&x2);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&t);
    sn_value_init(&den);
    sn_value_init(&n_f);
    sn_value_init(&one);
    sn_value_init(&two);
    sn_value_init(&pi);
    sn_value_init(&s);
    sn_value_init(&fac);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    /* term_0 = x; sum = x; fac tracks n! */
    st = sn_value_copy(ctx, &term, x); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, x); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &fac, &one); if (st != SN_OK) goto done;

    max_n = x->m_bits * 2 + 128;
    if (max_n < 160) max_n = 160;
    if (max_n > 4000) max_n = 4000;

    for (n = 1; n <= max_n; n++) {
        /* term *= -x2 / n ;  contrib = term / (2n+1) */
        st = soft_from_i(ctx, &n_f, (int64_t)(n), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &term, &x2, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &term, &t, &n_f, opt); if (st != SN_OK) goto done;
        st = sn_neg(ctx, &term, &term, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &den, (int64_t)((2 * n + 1)), x, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &term, &den, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &s, &sum, &t, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum); sn_value_move(&sum, &s); sn_value_init(&s);

        {
            /* Keep iterating until term is far below target ulp; double early-exit
             * underestimates residual at multiprec (m>=160) near x~1.5. */
            double ds;
            int need = x->m_bits / 2 + 16;
            if (need < 24) need = 24;
            st = sn_to_double(ctx, &t, &dt); if (st != SN_OK) goto done;
            st = sn_to_double(ctx, &sum, &ds); if (st != SN_OK) goto done;
            if (n > need && (fabs(dt) == 0.0 ||
                (ds != 0.0 && fabs(dt) < fabs(ds) * ldexp(1.0, -(x->m_bits + 16)))))
                break;
        }
    }

    /* scale by 2/sqrt(pi) */
    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &pi, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &two, &s, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, out, &sum, &t, opt);
    (void)rel;
done:
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &n_f);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &fac);
    return st;
}

/*
 * For x > 0: erfc via continued fraction (Lentz-style product form).
 * erfc(x) = exp(-x^2) / (x * sqrt(pi)) * cf
 *   cf = 1 / (1 + a1/(1 + a2/(1 + ...))) with a_n = n / (2 x^2)
 * More stable than the alternating asymptotic series near x~2..4.
 */
static sn_status soft_erfc_asymp(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value x2, two_x2, one, two, an, t, s, pi, pref, expv, cf, den, num, c, d, delta;
    int n, max_n, rel;
    double dt;

    sn_value_init(&x2);
    sn_value_init(&two_x2);
    sn_value_init(&one);
    sn_value_init(&two);
    sn_value_init(&an);
    sn_value_init(&t);
    sn_value_init(&s);
    sn_value_init(&pi);
    sn_value_init(&pref);
    sn_value_init(&expv);
    sn_value_init(&cf);
    sn_value_init(&den);
    sn_value_init(&num);
    sn_value_init(&c);
    sn_value_init(&d);
    sn_value_init(&delta);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &two_x2, &two, &x2, opt); if (st != SN_OK) goto done;

    /* Modified Lentz: f = b0; here b0 = 0 effectively starts as 1/(b1) form.
     * Use recursive product for continued fraction:
     *   h = 1; for n=max..1: h = 1 + (n/(2x^2)) / h; then cf = 1/h
     * backward recurrence is simple and multiprec-friendly. */
    max_n = x->m_bits * 2 + 160;
    if (max_n < 160) max_n = 160;
    if (max_n > 5000) max_n = 5000;

    st = sn_value_copy(ctx, &cf, &one); if (st != SN_OK) goto done; /* h */
    for (n = max_n; n >= 1; n--) {
        /* h = 1 + (n / (2 x^2)) / h  = 1 + n/(2x^2 * h) */
        st = soft_from_i(ctx, &an, (int64_t)(n), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &two_x2, &cf, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &s, &an, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &cf, &one, &s, opt); if (st != SN_OK) goto done;
    }
    /* cf currently holds h_1; actual cf factor is 1/h_1 */
    st = sn_div(ctx, &t, &one, &cf, opt); if (st != SN_OK) goto done;
    sn_value_clear(ctx, &cf); sn_value_move(&cf, &t); sn_value_init(&t);

    /* pref = exp(-x^2) / (x * sqrt(pi)) */
    st = sn_neg(ctx, &t, &x2, opt); if (st != SN_OK) goto done;
    st = sn_soft_exp(ctx, &expv, &t, opt); if (st != SN_OK) goto done;
    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &pi, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, x, &s, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &pref, &expv, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, out, &pref, &cf, opt);
    (void)rel; (void)dt; (void)num; (void)den; (void)c; (void)d; (void)delta;
done:
    sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &two_x2);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &an);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &pref);
    sn_value_clear(ctx, &expv);
    sn_value_clear(ctx, &cf);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &num);
    sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &d);
    sn_value_clear(ctx, &delta);
    return st;
}

sn_status sn_soft_erf(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value ax, one, erfcv, t, aw, resw;
    int cls, rel, neg, elev = 0, e_orig, m_orig, nan_orig;
    double da;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);
    if (cls == SN_FP_INFINITE)
        return soft_from_d(ctx, out, sn_fp_signbit(a) ? -1.0 : 1.0, a, opt);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&ax);
    sn_value_init(&one);
    sn_value_init(&erfcv);
    sn_value_init(&t);
    sn_value_init(&aw);
    sn_value_init(&resw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        /* Extra headroom: series/CF near |x|~1.5..2.5 needs more than +128 at m>=160. */
        int m_work = m_orig + (m_orig >= 160 ? 256 : 160);
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    neg = sn_fp_signbit(x) ? 1 : 0;
    st = sn_abs(ctx, &ax, x, opt); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &ax, &da); if (st != SN_OK) goto done;

    /* series for modest |x|; continued-fraction erfc for larger.
     * Prefer series a bit farther for multiprec to avoid CF truncation near 1.5. */
    if (da < (m_orig > 120 ? 2.0 : 1.5)) {
        st = soft_erf_series(ctx, &resw, &ax, opt);
    } else if (da > 40.0) {
        st = soft_from_d(ctx, &resw, 1.0, x, opt);
    } else {
        st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
        st = soft_erfc_asymp(ctx, &erfcv, &ax, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &resw, &one, &erfcv, opt);
    }
    if (st != SN_OK) goto done;
    if (neg) {
        st = sn_neg(ctx, &t, &resw, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &resw); sn_value_move(&resw, &t); sn_value_init(&t);
    }
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
    (void)rel;
done:
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &erfcv);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_erfc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value ax, one, erf_v, t, two, aw, resw;
    int cls, neg, elev = 0, e_orig, m_orig, nan_orig;
    double da;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return soft_from_d(ctx, out, 1.0, a, opt);
    if (cls == SN_FP_INFINITE) {
        if (sn_fp_signbit(a))
            return soft_from_d(ctx, out, 2.0, a, opt);
        return sn_float_set_zero(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&ax);
    sn_value_init(&one);
    sn_value_init(&erf_v);
    sn_value_init(&t);
    sn_value_init(&two);
    sn_value_init(&aw);
    sn_value_init(&resw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + (m_orig >= 160 ? 256 : 160);
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    neg = sn_fp_signbit(x) ? 1 : 0;
    st = sn_abs(ctx, &ax, x, opt); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &ax, &da); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;

    if (da < (m_orig > 120 ? 2.0 : 1.5)) {
        st = soft_erf_series(ctx, &erf_v, &ax, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &resw, &one, &erf_v, opt); if (st != SN_OK) goto done;
    } else if (da > 40.0) {
        st = sn_float_set_zero(ctx, &resw, 0, x->e_bits, x->m_bits, x->nan_enabled);
        if (st != SN_OK) goto done;
    } else {
        st = soft_erfc_asymp(ctx, &resw, &ax, opt); if (st != SN_OK) goto done;
    }

    /* erfc(-x) = 2 - erfc(x) */
    if (neg) {
        st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &t, &two, &resw, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &resw); sn_value_move(&resw, &t); sn_value_init(&t);
    }
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &erf_v);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    return st;
}

/* -------------------------------------------------------------------------- */
/* lgamma / tgamma  (Stirling + recurrence + reflection)                       */
/* -------------------------------------------------------------------------- */

/* Bernoulli B2..B32 as exact integer pairs num/den (fits int64; avoid double). */
typedef struct { int64_t num; int64_t den; } soft_q64;
static const soft_q64 soft_bern_even_q[] = {
    { 1, 6 },                          /* B2 */
    { -1, 30 },                        /* B4 */
    { 1, 42 },                         /* B6 */
    { -1, 30 },                        /* B8 */
    { 5, 66 },                         /* B10 */
    { -691, 2730 },                    /* B12 */
    { 7, 6 },                          /* B14 */
    { -3617, 510 },                    /* B16 */
    { 43867, 798 },                    /* B18 */
    { -174611, 330 },                  /* B20 */
    { 854513, 138 },                   /* B22 */
    { -236364091, 2730 },              /* B24 */
    { 8553103, 6 },                    /* B26 */
    { -23749461029LL, 870 },           /* B28 */
    { 8615841276005LL, 14322 },        /* B30 */
    { -7709321041217LL, 510 }          /* B32 */
};

static sn_status soft_load_bern(sn_ctx *ctx, sn_value *out, int k1based,
                                const sn_value *fmt, const sn_op_opt *opt)
{
    sn_status st;
    sn_value n, d;
    soft_q64 q;
    if (k1based < 1 || k1based > (int)(sizeof(soft_bern_even_q) / sizeof(soft_bern_even_q[0])))
        return SN_ERR_ARG;
    q = soft_bern_even_q[k1based - 1];
    sn_value_init(&n); sn_value_init(&d);
    st = soft_from_i(ctx, &n, q.num, fmt, opt); if (st != SN_OK) goto done;
    st = soft_from_i(ctx, &d, q.den, fmt, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &n, &d, opt);
done:
    sn_value_clear(ctx, &n); sn_value_clear(ctx, &d);
    return st;
}

/* Stirling for z >= ~8: ln ?(z) ? (z-1/2)ln z - z + ?ln(2?) + ? B_{2k}/(2k(2k-1) z^{2k-1}) */
static sn_status soft_lgamma_stirling(sn_ctx *ctx, sn_value *out, const sn_value *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value half, one, two, pi, lz, t, s, inv, inv2, term, sum, bk, den, c;
    int k, max_k;

    sn_value_init(&half);
    sn_value_init(&one);
    sn_value_init(&two);
    sn_value_init(&pi);
    sn_value_init(&lz);
    sn_value_init(&t);
    sn_value_init(&s);
    sn_value_init(&inv);
    sn_value_init(&inv2);
    sn_value_init(&term);
    sn_value_init(&sum);
    sn_value_init(&bk);
    sn_value_init(&den);
    sn_value_init(&c);

    st = soft_from_d(ctx, &half, 0.5, z, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, z, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, z, opt); if (st != SN_OK) goto done;
    st = soft_const_pi(ctx, &pi, z, opt); if (st != SN_OK) goto done;

    st = sn_soft_log(ctx, &lz, z, opt); if (st != SN_OK) goto done;
    /* (z - 1/2) * ln z - z */
    st = sn_sub(ctx, &t, z, &half, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &s, &t, &lz, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &sum, &s, z, opt); if (st != SN_OK) goto done;
    /* + 1/2 * ln(2?) */
    st = sn_mul(ctx, &t, &two, &pi, opt); if (st != SN_OK) goto done;
    st = sn_soft_log(ctx, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &half, &s, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &sum, &t, opt); if (st != SN_OK) goto done;
    sn_value_clear(ctx, &sum); sn_value_move(&sum, &s); sn_value_init(&s);

    st = sn_div(ctx, &inv, &one, z, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &inv2, &inv, &inv, opt); if (st != SN_OK) goto done;
    /* term starts as inv (z^{-1}); series uses z^{-(2k-1)} */
    st = sn_value_copy(ctx, &term, &inv); if (st != SN_OK) goto done;

    max_k = (int)(sizeof(soft_bern_even_q) / sizeof(soft_bern_even_q[0]));
    /* Use more Bernoulli terms at higher precision; thr scales with m. */
    if (z->m_bits <= 64) { if (max_k > 12) max_k = 12; }
    else if (z->m_bits <= 112) { if (max_k > 14) max_k = 14; }
    /* else full table (16 terms through B32) */

    for (k = 1; k <= max_k; k++) {
        /* contrib = B_{2k} / (2k (2k-1)) * term, term = z^{-(2k-1)} */
        st = soft_load_bern(ctx, &bk, k, z, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &den, (int64_t)((2 * k * (2 * k - 1))), z, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &c, &bk, &den, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &c, &term, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &s, &sum, &t, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum); sn_value_move(&sum, &s); sn_value_init(&s);
        /* term *= inv2 for next power +2.
         * Do NOT early-exit via sn_to_double: multiprec terms underflow double
         * long before target ulp, which falsely stopped at ~53 bits. */
        st = sn_mul(ctx, &t, &term, &inv2, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &term); sn_value_move(&term, &t); sn_value_init(&t);
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &lz);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &inv);
    sn_value_clear(ctx, &inv2);
    sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &bk);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &c);
    return st;
}

/* Raise z by integer steps until >= thr.
 * ln Gamma(z) = ln Gamma(z+n) - ln(prod (z+i)).
 * Form the product in chunks, folding each chunk with one soft_log so the
 * intermediate never needs exponent range ~ thr*log2(thr) (e_bits=16 overflows
 * near thr>~2k). Final corr = sum of chunk logs.
 */
static sn_status soft_lgamma_raise(sn_ctx *ctx, sn_value *z_out, sn_value *corr,
                                   const sn_value *z_in, double thr, const sn_op_opt *opt)
{
    sn_status st;
    sn_value z, one, t, prod, sum, lchunk;
    double dz;
    int guard = 0;
    /* Fold before prod grows past ~2^(2^(e-2)); keep a safe margin for e>=11. */
    int fold_every = 48;
    int since_fold = 0;

    sn_value_init(&z);
    sn_value_init(&one);
    sn_value_init(&t);
    sn_value_init(&prod);
    sn_value_init(&sum);
    sn_value_init(&lchunk);

    st = sn_value_copy(ctx, &z, z_in); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, z_in, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &prod, 1.0, z_in, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &sum, 0.0, z_in, opt); if (st != SN_OK) goto done;

    /* Larger thr => slightly larger chunks still OK; smaller e => fold more often. */
    if (z_in->e_bits > 0 && z_in->e_bits < 20) {
        fold_every = 24 + 2 * z_in->e_bits; /* e=16 -> 56 */
        if (fold_every > 64) fold_every = 64;
    } else {
        fold_every = 96;
    }

    for (;;) {
        st = sn_to_double(ctx, &z, &dz); if (st != SN_OK) goto done;
        if (dz >= thr) break;
        st = sn_mul(ctx, &t, &prod, &z, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &prod); sn_value_move(&prod, &t); sn_value_init(&t);
        st = sn_add(ctx, &t, &z, &one, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &z); sn_value_move(&z, &t); sn_value_init(&t);
        since_fold++;
        guard++;
        if (guard > 100000) { st = SN_ERR_DOMAIN; goto done; }
        if (since_fold >= fold_every) {
            st = sn_soft_log(ctx, &lchunk, &prod, opt); if (st != SN_OK) goto done;
            st = sn_add(ctx, &t, &sum, &lchunk, opt); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &sum); sn_value_move(&sum, &t); sn_value_init(&t);
            st = soft_from_d(ctx, &prod, 1.0, z_in, opt); if (st != SN_OK) goto done;
            since_fold = 0;
        }
    }
    if (since_fold > 0) {
        st = sn_soft_log(ctx, &lchunk, &prod, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &lchunk, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum); sn_value_move(&sum, &t); sn_value_init(&t);
    }
    st = sn_value_copy(ctx, z_out, &z); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, corr, &sum);
done:
    sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &prod);
    sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &lchunk);
    return st;
}

/* thr_m: target m_bits for raise thr (original precision, not elevated work width). */
static sn_status soft_lgamma_pos(sn_ctx *ctx, sn_value *out, const sn_value *x, int thr_m, const sn_op_opt *opt)
{
    sn_status st;
    sn_value z, corr, lg, t;
    double thr;

    sn_value_init(&z);
    sn_value_init(&corr);
    sn_value_init(&lg);
    sn_value_init(&t);

    if (thr_m <= 0) thr_m = x->m_bits;
    /*
     * Stirling table is B2..B32 (final power z^{-31}). Absolute remainder ~ C/z^{31}
     * with C~|B32|/(32*31) ≈ 2^24. Need C/z^{31} << 2^{-thr_m} so
     * log2(z) ≳ (thr_m + 24 + guard)/31. Old linear thr (cap 320) under-raised
     * at m>=256 (~2^{-195} residual vs target ~2^{-242}).
     */
    {
        double lg2z = ((double)thr_m + 24.0 + 48.0) / 31.0;
        thr = pow(2.0, lg2z);
    }
    if (thr < 24.0) thr = 24.0;
    if (thr > 8192.0) thr = 8192.0;

    st = soft_lgamma_raise(ctx, &z, &corr, x, thr, opt); if (st != SN_OK) goto done;
    st = soft_lgamma_stirling(ctx, &lg, &z, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, out, &lg, &corr, opt);
done:
    sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &corr);
    sn_value_clear(ctx, &lg);
    sn_value_clear(ctx, &t);
    return st;
}

/* Detect non-positive integer poles roughly via double */
static int soft_is_nonpos_int(sn_ctx *ctx, const sn_value *a)
{
    double d;
    if (sn_to_double(ctx, a, &d) != SN_OK) return 0;
    if (d > 0.0) return 0;
    if (fabs(d - floor(d + 0.5)) > 1e-12) return 0;
    return 1;
}

sn_status sn_soft_lgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, pi, sinv, t, u, v, ax, lg, half, aw, resw;
    int cls, rel, elev = 0, e_orig, m_orig, nan_orig;
    double da;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        /* lgamma(+inf)=+inf; lgamma(-inf)=+inf in many libm */
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }
    if (cls == SN_FP_ZERO) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }

    sn_value_init(&one);
    sn_value_init(&pi);
    sn_value_init(&sinv);
    sn_value_init(&t);
    sn_value_init(&u);
    sn_value_init(&v);
    sn_value_init(&ax);
    sn_value_init(&lg);
    sn_value_init(&half);

    st = sn_to_double(ctx, a, &da); if (st != SN_OK) goto done;
    if (soft_is_nonpos_int(ctx, a)) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        st = sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        goto done;
    }

    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, a, opt); if (st != SN_OK) goto done;

    e_orig = a->e_bits; m_orig = a->m_bits; nan_orig = a->nan_enabled;
    sn_value_init(&aw); sn_value_init(&resw);
    {
        const sn_value *x = a;
        /* Exact positive integers: lgamma(n)=log((n-1)!) via product+log (no Stirling residual). */
        if (da >= 1.0 && da < 100000.0 && fabs(da - floor(da + 0.5)) < 1e-12) {
            int n = (int)floor(da + 0.5);
            int i;
            sn_value prod, zi, tlog;
            sn_value_init(&prod); sn_value_init(&zi); sn_value_init(&tlog);
            if (n <= 2) {
                st = soft_from_d(ctx, out, 0.0, a, opt);
            } else {
                st = soft_from_i(ctx, &prod, 1, a, opt);
                for (i = 2; i < n && st == SN_OK; i++) {
                    st = soft_from_i(ctx, &zi, (int64_t)i, a, opt);
                    if (st == SN_OK) st = sn_mul(ctx, &tlog, &prod, &zi, opt);
                    if (st == SN_OK) {
                        sn_value_clear(ctx, &prod);
                        sn_value_move(&prod, &tlog);
                        sn_value_init(&tlog);
                    }
                }
                if (st == SN_OK) st = sn_soft_log(ctx, out, &prod, opt);
            }
            sn_value_clear(ctx, &prod); sn_value_clear(ctx, &zi); sn_value_clear(ctx, &tlog);
            sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw);
            goto done;
        }
        if (m_orig > 52) {
            int e_work = e_orig < 16 ? 16 : e_orig;
            /* Match tgamma elev; long raise (chunked ln) needs mant headroom. */
            int m_work = m_orig + 192;
            if (m_work < m_orig + m_orig / 2) m_work = m_orig + m_orig / 2;
            if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
            if (e_work < 16) e_work = 16;
            if (m_work > m_orig || e_work > e_orig) {
                st = sn_cast_float(ctx, &aw, a, e_work, m_work > m_orig ? m_work : m_orig, nan_orig, opt);
                if (st != SN_OK) { sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw); goto done; }
                x = &aw; elev = 1;
                st = sn_to_double(ctx, x, &da); if (st != SN_OK) { sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw); goto done; }
            }
        }
        if (da >= 0.5) {
            st = soft_lgamma_pos(ctx, elev ? &resw : out, x, m_orig, opt);
        } else {
            /* Reflection: ln|Gamma(z)| = ln pi - ln|sin(pi z)| - ln|Gamma(1-z)| */
            st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_mul(ctx, &t, &pi, x, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_soft_sin(ctx, &sinv, &t, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_abs(ctx, &sinv, &sinv, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_soft_log(ctx, &u, &sinv, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_soft_log(ctx, &v, &pi, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_sub(ctx, &t, &v, &u, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_sub(ctx, &ax, &one, x, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = soft_lgamma_pos(ctx, &lg, &ax, m_orig, opt); if (st != SN_OK) goto lgamma_elev_done;
            st = sn_sub(ctx, elev ? &resw : out, &t, &lg, opt);
        }
        if (st == SN_OK && elev)
            st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    lgamma_elev_done:;
        sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw);
        (void)rel; (void)half;
        goto done;
    }
done:
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &sinv);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &u);
    sn_value_clear(ctx, &v);
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &lg);
    sn_value_clear(ctx, &half);
    return st;
}

/* sign of ?(x) for real x: positive on (0,inf); on negatives alternates between poles */
static int soft_tgamma_sign(double x)
{
    double n;
    if (x > 0.0) return 1;
    n = floor(-x);
    /* in (-1,0) negative; (-2,-1) positive; ... sign = (-1)^{floor(-x)} */
    return ((int)n % 2 == 0) ? -1 : 1;
}

sn_status sn_soft_tgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value lg, t, one, prod, z, ax, aw, resw, g, g2, prod_work;
    const sn_value *x;
    int cls, sign, i, nsteps, elev = 0;
    int e_orig, m_orig, nan_orig, e_work, m_work;
    double da, thr;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        if (sn_fp_signbit(a)) {
            sn_raise(ctx, SN_FLAG_INVALID);
            if (!a->nan_enabled)
                return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
            return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        }
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }
    if (cls == SN_FP_ZERO) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, sn_fp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
    }
    if (soft_is_nonpos_int(ctx, a)) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }

    sn_value_init(&lg);
    sn_value_init(&t);
    sn_value_init(&one);
    sn_value_init(&prod);
    sn_value_init(&z);
    sn_value_init(&ax);
    sn_value_init(&aw);
    sn_value_init(&resw);
    sn_value_init(&g);
    sn_value_init(&g2);
    sn_value_init(&prod_work);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    x = a;
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) goto done;
    sign = soft_tgamma_sign(da);

    /* Modest positive integers: exact factorial product (no exp(lgamma)). */
    if (da > 0.5 && da < 200.0 && fabs(da - floor(da + 0.5)) < 1e-12) {
        nsteps = (int)floor(da + 0.5);
        if (nsteps >= 1) {
            st = soft_from_i(ctx, &one, 1, a, opt); if (st != SN_OK) goto done;
            st = sn_value_copy(ctx, &prod, &one); if (st != SN_OK) goto done;
            for (i = 1; i < nsteps; i++) {
                st = soft_from_i(ctx, &z, (int64_t)i, a, opt); if (st != SN_OK) goto done;
                st = sn_mul(ctx, &t, &prod, &z, opt); if (st != SN_OK) goto done;
                sn_value_clear(ctx, &prod); sn_value_move(&prod, &t); sn_value_init(&t);
            }
            sn_value_clear(ctx, out); sn_value_move(out, &prod); sn_value_init(&prod);
            goto done;
        }
    }

    /*
     * Multiprec: compute Γ via elevated exp(lgamma) then enforce recurrence by
     * raising z until Stirling region, Γ(z) = Γ(z+n)/(z...(z+n-1)).
     * Bare exp(lgamma) at target width only keeps ~53 reliable bits of the
     * recurrence identity because exp and lgamma round independently.
     */
    if (m_orig > 52) {
        e_work = e_orig < 16 ? 16 : e_orig;
        m_work = m_orig + 192;
        if (m_work < m_orig + m_orig / 2) m_work = m_orig + m_orig / 2;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        /* prod_{i} (z+i) up to thr needs ~thr*log2(thr) exponent; e=16 tops ~2^15. */
        {
            double lg2z = ((double)m_orig + 24.0 + 48.0) / 31.0;
            double thr_est = pow(2.0, lg2z);
            double need_exp;
            int e_need;
            if (thr_est < 24.0) thr_est = 24.0;
            if (thr_est > 8192.0) thr_est = 8192.0;
            need_exp = thr_est * (log(thr_est + 1.0) / log(2.0)) + 64.0;
            e_need = 16;
            while (e_need < 48 && ldexp(1.0, e_need - 1) < need_exp)
                e_need++;
            if (e_work < e_need) e_work = e_need;
        }
        if (m_work > m_orig || e_work > e_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work > m_orig ? m_work : m_orig, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
            st = sn_to_double(ctx, x, &da); if (st != SN_OK) goto done;
            m_work = x->m_bits;
            e_work = x->e_bits;
        }

        /* Same B2..B32 raise threshold as soft_lgamma_pos (target m_orig). */
        {
            double lg2z = ((double)m_orig + 24.0 + 48.0) / 31.0;
            thr = pow(2.0, lg2z);
        }
        if (thr < 24.0) thr = 24.0;
        if (thr > 8192.0) thr = 8192.0;

        st = soft_from_i(ctx, &one, 1, x, opt); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &z, x); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &prod_work, 1, x, opt); if (st != SN_OK) goto done;

        /* Raise positive/near-zero args into the asymptotic region. */
        nsteps = 0;
        if (da > 0.0) {
            while (da < thr && nsteps < 100000) {
                st = sn_mul(ctx, &t, &prod_work, &z, opt); if (st != SN_OK) goto done;
                sn_value_clear(ctx, &prod_work); sn_value_move(&prod_work, &t); sn_value_init(&t);
                st = sn_add(ctx, &t, &z, &one, opt); if (st != SN_OK) goto done;
                sn_value_clear(ctx, &z); sn_value_move(&z, &t); sn_value_init(&t);
                st = sn_to_double(ctx, &z, &da); if (st != SN_OK) goto done;
                nsteps++;
            }
            /* thr_m must be target m_orig: z carries elevated m_work, and
             * sn_soft_lgamma would re-raise with thr(m_work) and overflow e. */
            st = soft_lgamma_pos(ctx, &lg, &z, m_orig, opt); if (st != SN_OK) goto done;
            st = sn_soft_exp(ctx, &g, &lg, opt); if (st != SN_OK) goto done;
            if (nsteps > 0) {
                st = sn_div(ctx, elev ? &resw : out, &g, &prod_work, opt); if (st != SN_OK) goto done;
            } else {
                st = sn_value_copy(ctx, elev ? &resw : out, &g); if (st != SN_OK) goto done;
            }
        } else {
            /* Negative non-integer: reflection Γ(z)=π/(sin(πz)Γ(1-z)). */
            st = soft_const_pi(ctx, &ax, x, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t, &ax, x, opt); if (st != SN_OK) goto done; /* πz */
            st = sn_soft_sin(ctx, &g, &t, opt); if (st != SN_OK) goto done;
            st = sn_sub(ctx, &z, &one, x, opt); if (st != SN_OK) goto done; /* 1-z > 1 */
            /* Recursive positive path via elevated exp/raise on (1-z). */
            {
                sn_value zg, prodn, one2;
                double dz2;
                int ns2 = 0;
                sn_value_init(&zg); sn_value_init(&prodn); sn_value_init(&one2);
                st = soft_from_i(ctx, &one2, 1, x, opt);
                if (st == SN_OK) st = sn_value_copy(ctx, &zg, &z);
                if (st == SN_OK) st = soft_from_i(ctx, &prodn, 1, x, opt);
                if (st == SN_OK) st = sn_to_double(ctx, &zg, &dz2);
                while (st == SN_OK && dz2 < thr && ns2 < 100000) {
                    st = sn_mul(ctx, &t, &prodn, &zg, opt);
                    if (st != SN_OK) break;
                    sn_value_clear(ctx, &prodn); sn_value_move(&prodn, &t); sn_value_init(&t);
                    st = sn_add(ctx, &t, &zg, &one2, opt);
                    if (st != SN_OK) break;
                    sn_value_clear(ctx, &zg); sn_value_move(&zg, &t); sn_value_init(&t);
                    st = sn_to_double(ctx, &zg, &dz2);
                    ns2++;
                }
                if (st == SN_OK) st = soft_lgamma_pos(ctx, &lg, &zg, m_orig, opt);
                if (st == SN_OK) st = sn_soft_exp(ctx, &g2, &lg, opt);
                if (st == SN_OK && ns2 > 0) st = sn_div(ctx, &g2, &g2, &prodn, opt);
                sn_value_clear(ctx, &zg); sn_value_clear(ctx, &prodn); sn_value_clear(ctx, &one2);
            }
            if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t, &g, &g2, opt); if (st != SN_OK) goto done; /* sin * Γ(1-z) */
            st = sn_div(ctx, elev ? &resw : out, &ax, &t, opt); if (st != SN_OK) goto done;
            (void)sign;
        }
        if (st == SN_OK && elev)
            st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
        goto done;
    }

    /* Host-width path: exp(lgamma) + sign for negatives. */
    st = sn_soft_lgamma(ctx, &lg, a, opt); if (st != SN_OK) goto done;
    st = sn_soft_exp(ctx, out, &lg, opt); if (st != SN_OK) goto done;
    if (sign < 0) {
        st = sn_neg(ctx, &t, out, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, out); sn_value_move(out, &t); sn_value_init(&t);
    }
done:
    sn_value_clear(ctx, &lg);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &prod);
    sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &aw);
    sn_value_clear(ctx, &resw);
    sn_value_clear(ctx, &g);
    sn_value_clear(ctx, &g2);
    sn_value_clear(ctx, &prod_work);
    return st;
}


/* ---------- derived soft: exp2/expm1/log2/log10/log1p/cbrt ---------- */

sn_status sn_soft_exp2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value ln2, t, aw, resw;
    const sn_value *x;
    int e_orig, m_orig, nan_orig, elev = 0;

    if (!sn_math_need_soft(a)) return SN_ERR_TYPE;
    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&ln2); sn_value_init(&t);
    sn_value_init(&aw); sn_value_init(&resw);

    x = a;
    /* Elevate multiprec so x*ln2 + exp retain full target digits (exp2(n) near powers of two). */
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 64;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = soft_const_ln2(ctx, &ln2, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, x, &ln2, opt); if (st != SN_OK) goto done;
    st = sn_soft_exp(ctx, &resw, &t, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &ln2); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_expm1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* expm1(x) = exp(x)-1. For small |x| use series sum_{k>=1} x^k/k! to avoid cancel. */
    sn_status st;
    sn_value e, one, term, sum, t, k, ax, half;
    int cls, i, max_terms, rel;
    double da;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (!sn_math_need_soft(a)) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);
    if (cls == SN_FP_INFINITE) {
        if (sn_fp_signbit(a))
            return soft_from_d(ctx, out, -1.0, a, opt);
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }

    sn_value_init(&e); sn_value_init(&one); sn_value_init(&term);
    sn_value_init(&sum); sn_value_init(&t); sn_value_init(&k);
    sn_value_init(&ax); sn_value_init(&half);

    st = sn_abs(ctx, &ax, a, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, a, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &ax, &half); if (st != SN_OK) goto done;
    if (rel <= 0) {
        /* series: term_1=x; term_{k}=term_{k-1}*x/k; sum terms */
        st = sn_value_copy(ctx, &term, a); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, &sum, a); if (st != SN_OK) goto done;
        max_terms = a->m_bits / 2 + 40;
        if (max_terms < 40) max_terms = 40;
        if (max_terms > 2000) max_terms = 2000;
        for (i = 2; i <= max_terms; i++) {
            st = sn_float_from_i64(ctx, &k, (int64_t)i, a->e_bits, a->m_bits, a->nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t, &term, a, opt); if (st != SN_OK) goto done;
            st = sn_div(ctx, &term, &t, &k, opt); if (st != SN_OK) goto done;
            st = sn_add(ctx, &sum, &sum, &term, opt); if (st != SN_OK) goto done;
            if (i > 8 && sn_fp_classify(&term) == SN_FP_ZERO) break;
        }
        st = sn_value_copy(ctx, out, &sum);
        goto done;
    }
    /* multiprec: elevate then exp-1 so cancellation near large positive retains digits */
    if (a->m_bits > 52) {
        sn_value aw, e1;
        int e_work = a->e_bits < 16 ? 16 : a->e_bits;
        int m_work = a->m_bits + 48;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        sn_value_init(&aw); sn_value_init(&e1);
        st = sn_cast_float(ctx, &aw, a, e_work, m_work, a->nan_enabled, opt);
        if (st == SN_OK) st = sn_soft_exp(ctx, &e, &aw, opt);
        if (st == SN_OK) st = soft_from_d(ctx, &one, 1.0, &aw, opt);
        if (st == SN_OK) st = sn_sub(ctx, &e1, &e, &one, opt);
        if (st == SN_OK) st = sn_cast_float(ctx, out, &e1, a->e_bits, a->m_bits, a->nan_enabled, opt);
        sn_value_clear(ctx, &aw); sn_value_clear(ctx, &e1);
        goto done;
    }
    st = sn_soft_exp(ctx, &e, a, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, out, &e, &one, opt);
    (void)da;
done:
    sn_value_clear(ctx, &e); sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &term); sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &ax); sn_value_clear(ctx, &half);
    return st;
}

sn_status sn_soft_log2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value ln, ln2, aw, resw;
    const sn_value *x;
    int e_orig, m_orig, nan_orig, elev = 0;

    if (!sn_math_need_soft(a)) return SN_ERR_TYPE;
    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&ln); sn_value_init(&ln2);
    sn_value_init(&aw); sn_value_init(&resw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 64;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = sn_soft_log(ctx, &ln, x, opt); if (st != SN_OK) goto done;
    st = soft_const_ln2(ctx, &ln2, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &resw, &ln, &ln2, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &ln); sn_value_clear(ctx, &ln2);
    sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_log10(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value ln, ln10, aw, resw;
    const sn_value *x;
    int e_orig, m_orig, nan_orig, elev = 0;

    if (!sn_math_need_soft(a)) return SN_ERR_TYPE;
    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&ln); sn_value_init(&ln10);
    sn_value_init(&aw); sn_value_init(&resw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 64;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = sn_soft_log(ctx, &ln, x, opt); if (st != SN_OK) goto done;
    st = soft_const_ln10(ctx, &ln10, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &resw, &ln, &ln10, opt); if (st != SN_OK) goto done;
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &ln); sn_value_clear(ctx, &ln10);
    sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_log1p(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    /* log1p(x)=log(1+x). For |x| small use artanh form: log1p(x)=2*artanh(x/(2+x)).
     * Multiprec elevates so 1+x and log keep full target digits. */
    sn_status st;
    sn_value one, t, two, z, ax, half, aw, resw;
    int cls, rel, elev = 0, e_orig, m_orig, nan_orig;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (!sn_math_need_soft(a)) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);

    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;
    sn_value_init(&one); sn_value_init(&t); sn_value_init(&two);
    sn_value_init(&z); sn_value_init(&ax); sn_value_init(&half);
    sn_value_init(&aw); sn_value_init(&resw);

    /* domain at original precision */
    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = sn_neg(ctx, &t, &one, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, a, &t); if (st != SN_OK) goto done;
    if (rel < 0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            st = sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        else
            st = sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        goto done;
    }
    if (rel == 0) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        st = sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
        goto done;
    }

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
            st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
        }
    }

    st = sn_abs(ctx, &ax, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &ax, &half); if (st != SN_OK) goto done;
    if (rel <= 0) {
        /* z = x/(2+x); log1p = 2 artanh(z) */
        st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &two, x, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &z, x, &t, opt); if (st != SN_OK) goto done;
        st = soft_artanh_series(ctx, &resw, &z, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &resw, &resw, &two, opt); if (st != SN_OK) goto done;
    } else {
        st = sn_add(ctx, &t, &one, x, opt); if (st != SN_OK) goto done;
        st = sn_soft_log(ctx, &resw, &t, opt); if (st != SN_OK) goto done;
    }
    if (elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &resw);
done:
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &two); sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &ax); sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw);
    return st;
}

/* cbrt via Newton: y_{n+1} = (2*y_n + x/y_n^2)/3
 * Multiprec elevates work width so final cast retains full target digits. */
sn_status sn_soft_cbrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value y, y2, t, two, three, abs_a, seed, aw, yw;
    const sn_value *x;
    sn_fpclass c;
    int i, niter, sign, e_orig, m_orig, nan_orig, elev = 0;
    double da, dy;

    if (!sn_math_need_soft(a)) return SN_ERR_TYPE;
    c = sn_fp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE || c == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);

    sign = sn_fp_signbit(a);
    e_orig = a->e_bits;
    m_orig = a->m_bits;
    nan_orig = a->nan_enabled;

    sn_value_init(&y); sn_value_init(&y2); sn_value_init(&t);
    sn_value_init(&two); sn_value_init(&three); sn_value_init(&abs_a); sn_value_init(&seed);
    sn_value_init(&aw); sn_value_init(&yw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 48;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = sn_fabs(ctx, &abs_a, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &three, 3.0, x, opt); if (st != SN_OK) goto done;

    /* seed from host cbrt of double approx (magnitude only) */
    st = sn_to_double(ctx, &abs_a, &da); if (st != SN_OK) goto done;
    if (!(da > 0.0) || da != da) {
        dy = 1.0;
    } else {
        dy = cbrt(da);
        if (!(dy > 0.0) || dy != dy) dy = 1.0;
    }
    st = soft_from_d(ctx, &y, dy, x, opt); if (st != SN_OK) goto done;

    niter = x->m_bits / 2 + 12;
    if (niter < 16) niter = 16;
    if (niter > 120) niter = 120;
    for (i = 0; i < niter; i++) {
        st = sn_mul(ctx, &y2, &y, &y, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &abs_a, &y2, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &y2, &two, &y, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &seed, &y2, &t, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &y, &seed, &three, opt); if (st != SN_OK) goto done;
    }
    if (sign) {
        st = sn_neg(ctx, &yw, &y, opt); if (st != SN_OK) goto done;
    } else {
        st = sn_value_copy(ctx, &yw, &y); if (st != SN_OK) goto done;
    }
    if (elev)
        st = sn_cast_float(ctx, out, &yw, e_orig, m_orig, nan_orig, opt);
    else
        st = sn_value_copy(ctx, out, &yw);
done:
    sn_value_clear(ctx, &y); sn_value_clear(ctx, &y2); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &two); sn_value_clear(ctx, &three); sn_value_clear(ctx, &abs_a);
    sn_value_clear(ctx, &seed); sn_value_clear(ctx, &aw); sn_value_clear(ctx, &yw);
    return st;
}


/* -------------------------------------------------------------------------- */
/* Bessel J_n / Y_n soft multiprec (series + recurrence; modest |x|,|n|)      */
/* -------------------------------------------------------------------------- */

/* J0: sum_{k=0} (-1)^k (x/2)^{2k} / (k!)^2 */
static sn_status soft_j0_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value term, sum, t, x2, four, k, ksq, one;
    int i, max_terms;
    double dx;

    sn_value_init(&term); sn_value_init(&sum); sn_value_init(&t);
    sn_value_init(&x2); sn_value_init(&four); sn_value_init(&k);
    sn_value_init(&ksq); sn_value_init(&one);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &one); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 40) max_terms = 40;
    if (max_terms > 240) max_terms = 240;

    for (i = 1; i <= max_terms; i++) {
        st = soft_from_i(ctx, &k, (int64_t)(i), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &ksq, &k, &k, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &four, &ksq, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &x2, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &t, opt); if (st != SN_OK) goto done;
        st = sn_neg(ctx, &term, &term, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &term, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum);
        sn_value_move(&sum, &t);
        sn_value_init(&t);
        st = sn_to_double(ctx, &term, &dx); if (st != SN_OK) goto done;
        if (i > 8 && fabs(dx) == 0.0) break;
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &term); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &x2); sn_value_clear(ctx, &four); sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &ksq); sn_value_clear(ctx, &one);
    return st;
}

/* J1: (x/2) * sum (-1)^k (x/2)^{2k} / (k! (k+1)!) */
static sn_status soft_j1_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value term, sum, t, x2, four, k, kp1, den, halfx, one;
    int i, max_terms;
    double dx;

    sn_value_init(&term); sn_value_init(&sum); sn_value_init(&t);
    sn_value_init(&x2); sn_value_init(&four); sn_value_init(&k);
    sn_value_init(&kp1); sn_value_init(&den); sn_value_init(&halfx); sn_value_init(&one);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &halfx, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &halfx, &halfx, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &one); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 40) max_terms = 40;
    if (max_terms > 240) max_terms = 240;

    for (i = 1; i <= max_terms; i++) {
        st = soft_from_i(ctx, &k, (int64_t)(i), x, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &kp1, (int64_t)((i + 1)), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &den, &k, &kp1, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &den, &den, &four, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &x2, &den, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &t, opt); if (st != SN_OK) goto done;
        st = sn_neg(ctx, &term, &term, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &term, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum);
        sn_value_move(&sum, &t);
        sn_value_init(&t);
        st = sn_to_double(ctx, &term, &dx); if (st != SN_OK) goto done;
        if (i > 8 && fabs(dx) == 0.0) break;
    }
    st = sn_mul(ctx, out, &halfx, &sum, opt);
done:
    sn_value_clear(ctx, &term); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &x2); sn_value_clear(ctx, &four); sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &kp1); sn_value_clear(ctx, &den); sn_value_clear(ctx, &halfx);
    sn_value_clear(ctx, &one);
    return st;
}

static sn_status soft_jn_forward(sn_ctx *ctx, sn_value *out, int n, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value jnm1, jn, jnp1, t, nf;
    int k, an;
    double dx;

    if (n == 0) return soft_j0_series(ctx, out, x, opt);
    if (n == 1) return soft_j1_series(ctx, out, x, opt);
    if (n == -1) {
        st = soft_j1_series(ctx, out, x, opt);
        if (st != SN_OK) return st;
        return sn_neg(ctx, out, out, opt);
    }
    an = n < 0 ? -n : n;

    sn_value_init(&jnm1); sn_value_init(&jn); sn_value_init(&jnp1);
    sn_value_init(&t); sn_value_init(&nf);

    st = sn_to_double(ctx, x, &dx); if (st != SN_OK) goto done;
    if (fabs(dx) < 1e-300) {
        st = soft_from_d(ctx, out, 0.0, x, opt);
        goto done;
    }

    st = soft_j0_series(ctx, &jnm1, x, opt); if (st != SN_OK) goto done;
    st = soft_j1_series(ctx, &jn, x, opt); if (st != SN_OK) goto done;

    for (k = 1; k < an; k++) {
        st = soft_from_i(ctx, &nf, (int64_t)((2 * k)), x, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &nf, x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &t, &jn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &jnp1, &t, &jnm1, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &jnm1);
        sn_value_move(&jnm1, &jn);
        sn_value_init(&jn);
        sn_value_move(&jn, &jnp1);
        sn_value_init(&jnp1);
    }
    st = sn_value_copy(ctx, out, &jn); if (st != SN_OK) goto done;
    if (n < 0 && (an & 1))
        st = sn_neg(ctx, out, out, opt);
done:
    sn_value_clear(ctx, &jnm1); sn_value_clear(ctx, &jn); sn_value_clear(ctx, &jnp1);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &nf);
    return st;
}

sn_status sn_soft_j0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    int cls;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_INFINITE) return soft_from_d(ctx, out, 0.0, a, opt);
    if (cls == SN_FP_ZERO) return soft_from_d(ctx, out, 1.0, a, opt);
    return soft_j0_series(ctx, out, a, opt);
}

sn_status sn_soft_j1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    int cls;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_INFINITE || cls == SN_FP_ZERO) return soft_from_d(ctx, out, 0.0, a, opt);
    return soft_j1_series(ctx, out, a, opt);
}

sn_status sn_soft_jn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    int cls;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (n == 0) return sn_soft_j0(ctx, out, a, opt);
    if (n == 1) return sn_soft_j1(ctx, out, a, opt);
    if (n == -1) {
        sn_status st = sn_soft_j1(ctx, out, a, opt);
        if (st != SN_OK) return st;
        return sn_neg(ctx, out, out, opt);
    }
    return soft_jn_forward(ctx, out, n, a, opt);
}

/* Y0 series (DLMF / A&S equivalent):
 * Y0(x) = (2/pi)*( J0(x)*(ln(x/2)+gamma) + sum_{k=1} (-1)^{k+1} H_k (x/2)^{2k}/(k!)^2 )
 */
static sn_status soft_y0_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value j0v, ln, half, two_over_pi, pi, t, sum, term, x2, four, k, ksq, hk, invk, one, gamma;
    int i, max_terms;
    double dx;

    sn_value_init(&j0v); sn_value_init(&ln); sn_value_init(&half); sn_value_init(&two_over_pi);
    sn_value_init(&pi); sn_value_init(&t); sn_value_init(&sum); sn_value_init(&term);
    sn_value_init(&x2); sn_value_init(&four); sn_value_init(&k); sn_value_init(&ksq);
    sn_value_init(&hk); sn_value_init(&invk); sn_value_init(&one); sn_value_init(&gamma);

    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two_over_pi, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &two_over_pi, &two_over_pi, &pi, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &half, &half, x, opt); if (st != SN_OK) goto done;
    st = sn_log(ctx, &ln, &half, opt); if (st != SN_OK) goto done;
    st = soft_j0_series(ctx, &j0v, x, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &gamma, 0.5772156649015328606, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;

    /* sum_{k>=1} (-1)^{k+1} H_k (x/2)^{2k}/(k!)^2 ; term tracks (-1)^k base */
    st = soft_from_d(ctx, &sum, 0.0, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &hk, 0.0, x, opt); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 40) max_terms = 40;
    if (max_terms > 240) max_terms = 240;

    for (i = 1; i <= max_terms; i++) {
        st = soft_from_d(ctx, &invk, 1.0 / (double)i, x, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &hk, &hk, &invk, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &k, (int64_t)(i), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &ksq, &k, &k, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &four, &ksq, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &x2, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &t, opt); if (st != SN_OK) goto done;
        st = sn_neg(ctx, &term, &term, opt); if (st != SN_OK) goto done;
        /* series wants (-1)^{k+1} = -term (since term==(-1)^k base) */
        st = sn_mul(ctx, &t, &term, &hk, opt); if (st != SN_OK) goto done;
        st = sn_neg(ctx, &t, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &sum, &t, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &term, &dx); if (st != SN_OK) goto done;
        if (i > 8 && fabs(dx) == 0.0) break;
    }

    /* J0*(ln(x/2)+gamma) + sum */
    st = sn_add(ctx, &ln, &ln, &gamma, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &j0v, &ln, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &t, &sum, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, out, &two_over_pi, &t, opt);
done:
    sn_value_clear(ctx, &j0v); sn_value_clear(ctx, &ln); sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &two_over_pi); sn_value_clear(ctx, &pi); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &sum); sn_value_clear(ctx, &term); sn_value_clear(ctx, &x2);
    sn_value_clear(ctx, &four); sn_value_clear(ctx, &k); sn_value_clear(ctx, &ksq);
    sn_value_clear(ctx, &hk); sn_value_clear(ctx, &invk); sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &gamma);
    return st;
}

/* Wronskian: J0 Y1 - J1 Y0 = 2/(pi x)  =>  Y1 = (J1 Y0 - 2/(pi x)) / J0 */
static sn_status soft_y1_from_wronskian(sn_ctx *ctx, sn_value *out, const sn_value *x,
                                        const sn_value *y0v, const sn_op_opt *opt)
{
    sn_status st;
    sn_value j0v, j1v, pi, two, t, den;
    sn_value_init(&j0v); sn_value_init(&j1v); sn_value_init(&pi);
    sn_value_init(&two); sn_value_init(&t); sn_value_init(&den);

    st = soft_j0_series(ctx, &j0v, x, opt); if (st != SN_OK) goto done;
    st = soft_j1_series(ctx, &j1v, x, opt); if (st != SN_OK) goto done;
    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &den, &pi, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &two, &den, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &den, &j1v, y0v, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &den, &den, &t, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &den, &j0v, opt);
done:
    sn_value_clear(ctx, &j0v); sn_value_clear(ctx, &j1v); sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &two); sn_value_clear(ctx, &t); sn_value_clear(ctx, &den);
    return st;
}

static sn_status soft_y_domain_err(sn_ctx *ctx, sn_value *out, const sn_value *a)
{
    sn_raise(ctx, SN_FLAG_INVALID);
    if (a->nan_enabled)
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
}

sn_status sn_soft_y0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int cls;
    double dx;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO || (cls != SN_FP_INFINITE && sn_fp_signbit(a)))
        return soft_y_domain_err(ctx, out, a);
    if (cls == SN_FP_INFINITE) return soft_from_d(ctx, out, 0.0, a, opt);
    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    if (fabs(dx) > 40.0)
        return soft_from_d(ctx, out, y0(dx), a, opt);
    return soft_y0_series(ctx, out, a, opt);
}

sn_status sn_soft_y1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value y0v;
    int cls;
    double dx;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO || (cls != SN_FP_INFINITE && sn_fp_signbit(a)))
        return soft_y_domain_err(ctx, out, a);
    if (cls == SN_FP_INFINITE) return soft_from_d(ctx, out, 0.0, a, opt);
    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    if (fabs(dx) > 40.0)
        return soft_from_d(ctx, out, y1(dx), a, opt);
    sn_value_init(&y0v);
    st = soft_y0_series(ctx, &y0v, a, opt);
    if (st == SN_OK) st = soft_y1_from_wronskian(ctx, out, a, &y0v, opt);
    sn_value_clear(ctx, &y0v);
    return st;
}

sn_status sn_soft_yn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value y_nm1, y_n, y_np1, t, nf;
    int k, an, cls;
    double dx;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (n == 0) return sn_soft_y0(ctx, out, a, opt);
    if (n == 1) return sn_soft_y1(ctx, out, a, opt);
    if (n < 0) {
        an = -n;
        st = sn_soft_yn(ctx, out, an, a, opt);
        if (st != SN_OK) return st;
        if (an & 1) return sn_neg(ctx, out, out, opt);
        return SN_OK;
    }

    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO || sn_fp_signbit(a))
        return soft_y_domain_err(ctx, out, a);

    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    if (fabs(dx) > 40.0)
        return soft_from_d(ctx, out, yn(n, dx), a, opt);

    sn_value_init(&y_nm1); sn_value_init(&y_n); sn_value_init(&y_np1);
    sn_value_init(&t); sn_value_init(&nf);

    st = sn_soft_y0(ctx, &y_nm1, a, opt); if (st != SN_OK) goto done;
    st = sn_soft_y1(ctx, &y_n, a, opt); if (st != SN_OK) goto done;
    for (k = 1; k < n; k++) {
        st = soft_from_i(ctx, &nf, (int64_t)((2 * k)), a, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &nf, a, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &t, &y_n, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &y_np1, &t, &y_nm1, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &y_nm1);
        sn_value_move(&y_nm1, &y_n);
        sn_value_init(&y_n);
        sn_value_move(&y_n, &y_np1);
        sn_value_init(&y_np1);
    }
    st = sn_value_copy(ctx, out, &y_n);
done:
    sn_value_clear(ctx, &y_nm1); sn_value_clear(ctx, &y_n); sn_value_clear(ctx, &y_np1);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &nf);
    return st;
}



/* -------------------------------------------------------------------------- */
/* Modified Bessel I_n / K_n soft (series + recurrence; modest |x|,|n|)       */
/* Always used for all formats: many libm lack i0/k0 (e.g. MinGW).           */
/* -------------------------------------------------------------------------- */

/* I0: sum_{k=0} (x/2)^{2k} / (k!)^2  (J0 without alternating sign) */
static sn_status soft_i0_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value term, sum, t, x2, four, k, ksq, one;
    int i, max_terms;
    double dx;

    sn_value_init(&term); sn_value_init(&sum); sn_value_init(&t);
    sn_value_init(&x2); sn_value_init(&four); sn_value_init(&k);
    sn_value_init(&ksq); sn_value_init(&one);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &one); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 40) max_terms = 40;
    if (max_terms > 240) max_terms = 240;

    for (i = 1; i <= max_terms; i++) {
        st = soft_from_i(ctx, &k, (int64_t)(i), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &ksq, &k, &k, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &four, &ksq, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &x2, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &term, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum);
        sn_value_move(&sum, &t);
        sn_value_init(&t);
        st = sn_to_double(ctx, &term, &dx); if (st != SN_OK) goto done;
        if (i > 8 && fabs(dx) == 0.0) break;
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &term); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &x2); sn_value_clear(ctx, &four); sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &ksq); sn_value_clear(ctx, &one);
    return st;
}

/* I1: (x/2) * sum (x/2)^{2k} / (k! (k+1)!) */
static sn_status soft_i1_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value term, sum, t, x2, four, k, kp1, den, halfx, one;
    int i, max_terms;
    double dx;

    sn_value_init(&term); sn_value_init(&sum); sn_value_init(&t);
    sn_value_init(&x2); sn_value_init(&four); sn_value_init(&k);
    sn_value_init(&kp1); sn_value_init(&den); sn_value_init(&halfx); sn_value_init(&one);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &halfx, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &halfx, &halfx, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &one); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 40) max_terms = 40;
    if (max_terms > 240) max_terms = 240;

    for (i = 1; i <= max_terms; i++) {
        st = soft_from_i(ctx, &k, (int64_t)(i), x, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &kp1, (int64_t)((i + 1)), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &den, &k, &kp1, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &den, &den, &four, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &x2, &den, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &term, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum);
        sn_value_move(&sum, &t);
        sn_value_init(&t);
        st = sn_to_double(ctx, &term, &dx); if (st != SN_OK) goto done;
        if (i > 8 && fabs(dx) == 0.0) break;
    }
    st = sn_mul(ctx, out, &halfx, &sum, opt);
done:
    sn_value_clear(ctx, &term); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &x2); sn_value_clear(ctx, &four); sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &kp1); sn_value_clear(ctx, &den); sn_value_clear(ctx, &halfx);
    sn_value_clear(ctx, &one);
    return st;
}

/* Recurrence: I_{n+1}(x) = I_{n-1}(x) - (2n/x) I_n(x)
 * Forward is OK when n is modest relative to |x|. */
static sn_status soft_in_forward(sn_ctx *ctx, sn_value *out, int n, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value inm1, inv, inp1, t, nf;
    int k;

    if (n == 0) return soft_i0_series(ctx, out, x, opt);
    if (n == 1) return soft_i1_series(ctx, out, x, opt);

    sn_value_init(&inm1); sn_value_init(&inv); sn_value_init(&inp1);
    sn_value_init(&t); sn_value_init(&nf);

    st = soft_i0_series(ctx, &inm1, x, opt); if (st != SN_OK) goto done;
    st = soft_i1_series(ctx, &inv, x, opt); if (st != SN_OK) goto done;
    for (k = 1; k < n; k++) {
        st = soft_from_i(ctx, &nf, (int64_t)((2 * k)), x, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &nf, x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &t, &inv, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &inp1, &inm1, &t, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &inm1);
        sn_value_move(&inm1, &inv);
        sn_value_init(&inv);
        sn_value_move(&inv, &inp1);
        sn_value_init(&inp1);
    }
    st = sn_value_copy(ctx, out, &inv);
done:
    sn_value_clear(ctx, &inm1); sn_value_clear(ctx, &inv); sn_value_clear(ctx, &inp1);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &nf);
    return st;
}

/* Miller downward recurrence for I_n (x>0, n>=2).
 * Start at N >> n with f_N=0, f_{N-1}=1, recur:
 *   f_{k-1} = (2k/x) f_k + f_{k+1}
 * Normalize via I0(x) / f0. Stable when n is large vs x. */
static sn_status soft_in_miller(sn_ctx *ctx, sn_value *out, int n, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value fk, fkp1, fkm1, t, nf, scale, i0v, two;
    int k, N;
    double dx;

    if (n <= 1) return soft_in_forward(ctx, out, n, x, opt);
    st = sn_to_double(ctx, x, &dx); if (st != SN_OK) return st;
    if (dx <= 0.0) return soft_in_forward(ctx, out, n, x, opt);

    /* Start order: enough headroom above n and x */
    N = n + (int)(dx) + 20;
    if (N < n + 10) N = n + 10;
    if (N > 200) N = 200;
    if (N <= n) N = n + 10;

    sn_value_init(&fk); sn_value_init(&fkp1); sn_value_init(&fkm1);
    sn_value_init(&t); sn_value_init(&nf); sn_value_init(&scale);
    sn_value_init(&i0v); sn_value_init(&two);

    /* f_N = 0, f_{N-1} = 1 */
    st = soft_from_d(ctx, &fkp1, 0.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &fk, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;

    /* also accumulate even terms for optional alt-normalization; use f0 at end */
    for (k = N - 1; k >= 1; k--) {
        /* f_{k-1} = (2k/x)*f_k + f_{k+1} */
        st = soft_from_i(ctx, &nf, (int64_t)((2 * k)), x, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &nf, x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &t, &fk, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &fkm1, &t, &fkp1, opt); if (st != SN_OK) goto done;
        /* shift: fkp1=fk, fk=fkm1; if k-1==n save out unnormalized */
        sn_value_clear(ctx, &fkp1);
        sn_value_move(&fkp1, &fk);
        sn_value_init(&fk);
        sn_value_move(&fk, &fkm1);
        sn_value_init(&fkm1);
        if (k - 1 == n) {
            st = sn_value_copy(ctx, out, &fk); if (st != SN_OK) goto done;
        }
    }
    /* now fk holds f0 (k loop ended at k=1 -> computed f0 into fk) */
    st = soft_i0_series(ctx, &i0v, x, opt); if (st != SN_OK) goto done;
    /* scale = I0 / f0 */
    st = sn_div(ctx, &scale, &i0v, &fk, opt); if (st != SN_OK) goto done;
    if (n == 0) {
        st = sn_value_copy(ctx, out, &i0v);
    } else {
        st = sn_mul(ctx, out, out, &scale, opt);
    }
done:
    sn_value_clear(ctx, &fk); sn_value_clear(ctx, &fkp1); sn_value_clear(ctx, &fkm1);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &nf); sn_value_clear(ctx, &scale);
    sn_value_clear(ctx, &i0v); sn_value_clear(ctx, &two);
    return st;
}

/* Choose forward vs Miller: Miller when n > x + a few (forward loses digits). */
static sn_status soft_in_stable(sn_ctx *ctx, sn_value *out, int n, const sn_value *x, const sn_op_opt *opt)
{
    double dx;
    sn_status st;
    if (n <= 1) return soft_in_forward(ctx, out, n, x, opt);
    st = sn_to_double(ctx, x, &dx); if (st != SN_OK) return st;
    if (dx < 0) dx = -dx;
    if ((double)n > dx + 4.0) return soft_in_miller(ctx, out, n, x, opt);
    return soft_in_forward(ctx, out, n, x, opt);
}

/* K0 (A&S 9.6.13 / digamma form):
 * K0(x) = -I0(x)*(ln(x/2)+gamma) + sum_{k=1} H_k (x/2)^{2k}/(k!)^2
 * Equiv: -ln(x/2)*I0 + sum_{k=0} psi(k+1) term_k, psi(1)=-gamma, psi(k+1)=-gamma+H_k.
 */
static sn_status soft_k0_series(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value i0v, ln, half, t, sum, term, x2, four, k, ksq, hk, invk, one, gamma;
    int i, max_terms;
    double dx;

    sn_value_init(&i0v); sn_value_init(&ln); sn_value_init(&half); sn_value_init(&t);
    sn_value_init(&sum); sn_value_init(&term); sn_value_init(&x2); sn_value_init(&four);
    sn_value_init(&k); sn_value_init(&ksq); sn_value_init(&hk); sn_value_init(&invk);
    sn_value_init(&one); sn_value_init(&gamma);

    st = soft_from_d(ctx, &half, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &half, &half, x, opt); if (st != SN_OK) goto done; /* x/2 */
    st = sn_log(ctx, &ln, &half, opt); if (st != SN_OK) goto done;
    st = soft_i0_series(ctx, &i0v, x, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &gamma, 0.5772156649015328606, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &x2, x, x, opt); if (st != SN_OK) goto done;

    /* sum starts at 0; term tracks (x/2)^{2k}/(k!)^2, base k=0 is 1 */
    st = soft_from_d(ctx, &sum, 0.0, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &term, &one); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &hk, 0.0, x, opt); if (st != SN_OK) goto done;

    max_terms = x->m_bits / 2 + 24;
    if (max_terms < 40) max_terms = 40;
    if (max_terms > 240) max_terms = 240;

    for (i = 1; i <= max_terms; i++) {
        st = soft_from_d(ctx, &invk, 1.0 / (double)i, x, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &hk, &hk, &invk, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &k, (int64_t)(i), x, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &ksq, &k, &k, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &four, &ksq, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &x2, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &term, &hk, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &sum, &t, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &term, &dx); if (st != SN_OK) goto done;
        if (i > 8 && fabs(dx) == 0.0) break;
    }

    /* K0 = -I0*(ln(x/2)+gamma) + sum */
    st = sn_add(ctx, &ln, &ln, &gamma, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &i0v, &ln, opt); if (st != SN_OK) goto done;
    st = sn_neg(ctx, &t, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, out, &t, &sum, opt);
done:
    sn_value_clear(ctx, &i0v); sn_value_clear(ctx, &ln); sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &term);
    sn_value_clear(ctx, &x2); sn_value_clear(ctx, &four); sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &ksq); sn_value_clear(ctx, &hk); sn_value_clear(ctx, &invk);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &gamma);
    return st;
}

/* Wronskian (modified Bessel): I0 K1 + I1 K0 = 1/x
 * => K1 = (1/x - I1*K0) / I0
 */
static sn_status soft_k1_from_wronskian(sn_ctx *ctx, sn_value *out, const sn_value *x,
                                        const sn_value *k0v, const sn_op_opt *opt)
{
    sn_status st;
    sn_value i0v, i1v, invx, t;
    sn_value_init(&i0v); sn_value_init(&i1v); sn_value_init(&invx); sn_value_init(&t);

    st = soft_i0_series(ctx, &i0v, x, opt); if (st != SN_OK) goto done;
    st = soft_i1_series(ctx, &i1v, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &invx, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &invx, &invx, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &i1v, k0v, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &invx, &t, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &t, &i0v, opt);
done:
    sn_value_clear(ctx, &i0v); sn_value_clear(ctx, &i1v);
    sn_value_clear(ctx, &invx); sn_value_clear(ctx, &t);
    return st;
}


/* Asymptotic helpers for large |x| (Abramowitz & Stegun 9.7.1 / 9.7.2).
 * I0(x) ~ exp(x)/sqrt(2 pi x) * (1 + 1/(8x) + 9/(128 x^2) + 75/(1024 x^3) + ...)
 * I1(x) ~ exp(x)/sqrt(2 pi x) * (1 - 3/(8x) - 15/(128 x^2) - 105/(1024 x^3) + ...)
 * K0(x) ~ exp(-x)*sqrt(pi/(2x)) * (1 - 1/(8x) + 9/(128 x^2) - 75/(1024 x^3) + ...)
 * K1(x) ~ exp(-x)*sqrt(pi/(2x)) * (1 + 3/(8x) - 15/(128 x^2) + 105/(1024 x^3) + ...)
 * Domain: x > 0; caller handles signs for I.
 */

static sn_status soft_i0_asymp(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value pi, two, t, s, invx, invx2, invx3, series, pref;
    sn_value_init(&pi); sn_value_init(&two); sn_value_init(&t); sn_value_init(&s);
    sn_value_init(&invx); sn_value_init(&invx2); sn_value_init(&invx3);
    sn_value_init(&series); sn_value_init(&pref);

    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &two, &pi, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, x, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done; /* sqrt(2 pi x) */
    st = sn_exp(ctx, &pref, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &pref, &pref, &s, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &invx, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &invx, &invx, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx2, &invx, &invx, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx3, &invx2, &invx, opt); if (st != SN_OK) goto done;

    /* 1 + 1/(8x) + 9/(128 x^2) + 75/(1024 x^3) */
    st = soft_from_d(ctx, &series, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.125, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 9.0/128.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx2, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 75.0/1024.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx3, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, out, &pref, &series, opt);
done:
    sn_value_clear(ctx, &pi); sn_value_clear(ctx, &two); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &invx); sn_value_clear(ctx, &invx2);
    sn_value_clear(ctx, &invx3); sn_value_clear(ctx, &series); sn_value_clear(ctx, &pref);
    return st;
}

static sn_status soft_i1_asymp(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value pi, two, t, s, invx, invx2, invx3, series, pref;
    sn_value_init(&pi); sn_value_init(&two); sn_value_init(&t); sn_value_init(&s);
    sn_value_init(&invx); sn_value_init(&invx2); sn_value_init(&invx3);
    sn_value_init(&series); sn_value_init(&pref);

    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &two, &pi, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, x, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_exp(ctx, &pref, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &pref, &pref, &s, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &invx, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &invx, &invx, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx2, &invx, &invx, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx3, &invx2, &invx, opt); if (st != SN_OK) goto done;

    /* 1 - 3/(8x) - 15/(128 x^2) - 105/(1024 x^3) */
    st = soft_from_d(ctx, &series, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.375, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 15.0/128.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 105.0/1024.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx3, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, out, &pref, &series, opt);
done:
    sn_value_clear(ctx, &pi); sn_value_clear(ctx, &two); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &invx); sn_value_clear(ctx, &invx2);
    sn_value_clear(ctx, &invx3); sn_value_clear(ctx, &series); sn_value_clear(ctx, &pref);
    return st;
}

static sn_status soft_k0_asymp(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value pi, two, t, s, invx, invx2, invx3, series, pref, nx;
    sn_value_init(&pi); sn_value_init(&two); sn_value_init(&t); sn_value_init(&s);
    sn_value_init(&invx); sn_value_init(&invx2); sn_value_init(&invx3);
    sn_value_init(&series); sn_value_init(&pref); sn_value_init(&nx);

    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &pi, &two, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &t, x, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done; /* sqrt(pi/(2x)) */
    st = sn_neg(ctx, &nx, x, opt); if (st != SN_OK) goto done;
    st = sn_exp(ctx, &pref, &nx, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &pref, &pref, &s, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &invx, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &invx, &invx, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx2, &invx, &invx, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx3, &invx2, &invx, opt); if (st != SN_OK) goto done;

    /* 1 - 1/(8x) + 9/(128 x^2) - 75/(1024 x^3) */
    st = soft_from_d(ctx, &series, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.125, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 9.0/128.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx2, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 75.0/1024.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx3, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, out, &pref, &series, opt);
done:
    sn_value_clear(ctx, &pi); sn_value_clear(ctx, &two); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &invx); sn_value_clear(ctx, &invx2);
    sn_value_clear(ctx, &invx3); sn_value_clear(ctx, &series); sn_value_clear(ctx, &pref);
    sn_value_clear(ctx, &nx);
    return st;
}

static sn_status soft_k1_asymp(sn_ctx *ctx, sn_value *out, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value pi, two, t, s, invx, invx2, invx3, series, pref, nx;
    sn_value_init(&pi); sn_value_init(&two); sn_value_init(&t); sn_value_init(&s);
    sn_value_init(&invx); sn_value_init(&invx2); sn_value_init(&invx3);
    sn_value_init(&series); sn_value_init(&pref); sn_value_init(&nx);

    st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &pi, &two, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &t, x, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_neg(ctx, &nx, x, opt); if (st != SN_OK) goto done;
    st = sn_exp(ctx, &pref, &nx, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &pref, &pref, &s, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &invx, 1.0, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &invx, &invx, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx2, &invx, &invx, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &invx3, &invx2, &invx, opt); if (st != SN_OK) goto done;

    /* 1 + 3/(8x) - 15/(128 x^2) + 105/(1024 x^3) */
    st = soft_from_d(ctx, &series, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.375, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 15.0/128.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 105.0/1024.0, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &invx3, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &series, &series, &t, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, out, &pref, &series, opt);
done:
    sn_value_clear(ctx, &pi); sn_value_clear(ctx, &two); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &invx); sn_value_clear(ctx, &invx2);
    sn_value_clear(ctx, &invx3); sn_value_clear(ctx, &series); sn_value_clear(ctx, &pref);
    sn_value_clear(ctx, &nx);
    return st;
}

static sn_status soft_k_domain_err(sn_ctx *ctx, sn_value *out, const sn_value *a)
{
    sn_raise(ctx, SN_FLAG_INVALID);
    if (a->nan_enabled)
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
}

sn_status sn_soft_i0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int cls;
    double dx;
    sn_value ax;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO) return soft_from_d(ctx, out, 1.0, a, opt);
    if (cls == SN_FP_INFINITE) return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);

    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    sn_value_init(&ax);
    st = sn_fabs(ctx, &ax, a, opt);
    if (st != SN_OK) { sn_value_clear(ctx, &ax); return st; }
    if (fabs(dx) > 20.0) {
        /* asymptotic; may still overflow for huge |x| via sn_exp */
        st = soft_i0_asymp(ctx, out, &ax, opt);
    } else {
        st = soft_i0_series(ctx, out, &ax, opt);
    }
    sn_value_clear(ctx, &ax);
    return st;
}

sn_status sn_soft_i1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int cls, neg;
    double dx;
    sn_value ax;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO) return soft_from_d(ctx, out, 0.0, a, opt);
    if (cls == SN_FP_INFINITE) {
        return sn_float_set_inf(ctx, out, sn_fp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
    }

    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    neg = sn_fp_signbit(a);
    sn_value_init(&ax);
    st = sn_fabs(ctx, &ax, a, opt);
    if (st != SN_OK) { sn_value_clear(ctx, &ax); return st; }
    if (fabs(dx) > 20.0)
        st = soft_i1_asymp(ctx, out, &ax, opt);
    else
        st = soft_i1_series(ctx, out, &ax, opt);
    if (st == SN_OK && neg) st = sn_neg(ctx, out, out, opt);
    sn_value_clear(ctx, &ax);
    return st;
}

sn_status sn_soft_in(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int cls, an, neg_x, flip;
    double dx;
    sn_value ax;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (n == 0) return sn_soft_i0(ctx, out, a, opt);
    if (n == 1) return sn_soft_i1(ctx, out, a, opt);
    if (n == -1) {
        /* Integer order: I_{-n} = I_n, so I_{-1} = I_1. */
        return sn_soft_i1(ctx, out, a, opt);
    }
    an = n < 0 ? -n : n;
    /* I_{-n} = I_n for integer n; Miller path allows larger n */
    if (an > 80) return SN_ERR_RANGE;

    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO) return soft_from_d(ctx, out, 0.0, a, opt);
    if (cls == SN_FP_INFINITE) {
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }

    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    if (fabs(dx) < 1e-300 && an >= 1)
        return soft_from_d(ctx, out, 0.0, a, opt);

    neg_x = sn_fp_signbit(a);
    /* I_n(-x) = (-1)^n I_n(x) */
    flip = neg_x && (an & 1);
    sn_value_init(&ax);
    st = sn_fabs(ctx, &ax, a, opt);
    if (st != SN_OK) { sn_value_clear(ctx, &ax); return st; }
    if (fabs(dx) > 20.0) {
        /* Large |x|: seed forward recurrence from asymptotic I0/I1.
         * Note: n==0/1 already returned above; an >= 2 here. */
        sn_value inm1, inv, inp1, t, nf;
        int k;
        sn_value_init(&inm1); sn_value_init(&inv); sn_value_init(&inp1);
        sn_value_init(&t); sn_value_init(&nf);
        st = soft_i0_asymp(ctx, &inm1, &ax, opt);
        if (st == SN_OK) st = soft_i1_asymp(ctx, &inv, &ax, opt);
        for (k = 1; st == SN_OK && k < an; k++) {
            st = soft_from_i(ctx, &nf, (int64_t)((2 * k)), &ax, opt); if (st != SN_OK) break;
            st = sn_div(ctx, &t, &nf, &ax, opt); if (st != SN_OK) break;
            st = sn_mul(ctx, &t, &t, &inv, opt); if (st != SN_OK) break;
            st = sn_sub(ctx, &inp1, &inm1, &t, opt); if (st != SN_OK) break;
            sn_value_clear(ctx, &inm1);
            sn_value_move(&inm1, &inv);
            sn_value_init(&inv);
            sn_value_move(&inv, &inp1);
            sn_value_init(&inp1);
        }
        if (st == SN_OK) st = sn_value_copy(ctx, out, &inv);
        sn_value_clear(ctx, &inm1); sn_value_clear(ctx, &inv); sn_value_clear(ctx, &inp1);
        sn_value_clear(ctx, &t); sn_value_clear(ctx, &nf);
    } else {
        st = soft_in_stable(ctx, out, an, &ax, opt);
    }
    if (st == SN_OK && flip) st = sn_neg(ctx, out, out, opt);
    sn_value_clear(ctx, &ax);
    return st;
}

sn_status sn_soft_k0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int cls;
    double dx;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO || sn_fp_signbit(a))
        return soft_k_domain_err(ctx, out, a);
    if (cls == SN_FP_INFINITE) return soft_from_d(ctx, out, 0.0, a, opt);

    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    if (dx > 20.0)
        return soft_k0_asymp(ctx, out, a, opt);
    return soft_k0_series(ctx, out, a, opt);
}

sn_status sn_soft_k1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value k0v;
    int cls;
    double dx;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO || sn_fp_signbit(a))
        return soft_k_domain_err(ctx, out, a);
    if (cls == SN_FP_INFINITE) return soft_from_d(ctx, out, 0.0, a, opt);

    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    if (dx > 20.0)
        return soft_k1_asymp(ctx, out, a, opt);

    sn_value_init(&k0v);
    st = soft_k0_series(ctx, &k0v, a, opt);
    if (st == SN_OK) st = soft_k1_from_wronskian(ctx, out, a, &k0v, opt);
    sn_value_clear(ctx, &k0v);
    return st;
}

/* Recurrence: K_{n+1}(x) = K_{n-1}(x) + (2n/x) K_n(x)
 * K_{-n} = K_n for integer n. Domain x > 0.
 */
sn_status sn_soft_kn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value knm1, kn, knp1, t, nf;
    int k, an, cls;
    double dx;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (n == 0) return sn_soft_k0(ctx, out, a, opt);
    if (n == 1 || n == -1) return sn_soft_k1(ctx, out, a, opt);
    an = n < 0 ? -n : n;
    if (an > 20) return SN_ERR_RANGE;

    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, a); }
    if (cls == SN_FP_ZERO || sn_fp_signbit(a))
        return soft_k_domain_err(ctx, out, a);
    if (cls == SN_FP_INFINITE) return soft_from_d(ctx, out, 0.0, a, opt);

    st = sn_to_double(ctx, a, &dx); if (st != SN_OK) return st;
    /* For large x, sn_soft_k0/k1 already use asymptotics; recurrence still works. */

    sn_value_init(&knm1); sn_value_init(&kn); sn_value_init(&knp1);
    sn_value_init(&t); sn_value_init(&nf);

    st = sn_soft_k0(ctx, &knm1, a, opt); if (st != SN_OK) goto done;
    st = sn_soft_k1(ctx, &kn, a, opt); if (st != SN_OK) goto done;
    for (k = 1; k < an; k++) {
        st = soft_from_i(ctx, &nf, (int64_t)((2 * k)), a, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &nf, a, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &t, &kn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &knp1, &knm1, &t, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &knm1);
        sn_value_move(&knm1, &kn);
        sn_value_init(&kn);
        sn_value_move(&kn, &knp1);
        sn_value_init(&knp1);
    }
    st = sn_value_copy(ctx, out, &kn);
done:
    sn_value_clear(ctx, &knm1); sn_value_clear(ctx, &kn); sn_value_clear(ctx, &knp1);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &nf);
    return st;
}


/* -------------------------------------------------------------------------- */
/* Complete elliptic integrals K(m), E(m) via AGM (parameter m = k^2).        */
/* Domain: m in [0,1); m=1 -> K=+inf, E=1; m<0 or m>1 -> DOMAIN/NaN.         */
/* -------------------------------------------------------------------------- */

static sn_status soft_ellip_domain(sn_ctx *ctx, sn_value *out, const sn_value *a)
{
    sn_raise(ctx, SN_FLAG_INVALID);
    if (a->nan_enabled)
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
}

/* Shared AGM: returns K and optionally E.
 * a0=1, b0=sqrt(1-m), c0=sqrt(m)
 * a_{n+1}=(a_n+b_n)/2, b_{n+1}=sqrt(a_n b_n), c_{n+1}=(a_n-b_n)/2
 * K = pi/(2 a_N)
 * E = K * (1 - sum 2^{n-1} c_n^2)  (with n starting 0: sum 2^{n-1} for n>=1, and c0^2/2 term)
 * Standard: E = K * (1 - sum_{n=0}^N 2^{n-1} c_n^2) where 2^{-1} for n=0 is 1/2.
 */
static sn_status soft_ellip_agm(sn_ctx *ctx, sn_value *k_out, sn_value *e_out,
                                const sn_value *m, const sn_op_opt *opt)
{
    sn_status st;
    sn_value an, bn, cn, an1, bn1, t, one, half, two, pi, sum, csq, pow2, s;
    int i, max_iter;
    double da, db;

    sn_value_init(&an); sn_value_init(&bn); sn_value_init(&cn); sn_value_init(&an1);
    sn_value_init(&bn1); sn_value_init(&t); sn_value_init(&one); sn_value_init(&half);
    sn_value_init(&two); sn_value_init(&pi); sn_value_init(&sum); sn_value_init(&csq);
    sn_value_init(&pow2); sn_value_init(&s);

    st = soft_from_d(ctx, &one, 1.0, m, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, m, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, m, opt); if (st != SN_OK) goto done;
    st = soft_const_pi(ctx, &pi, m, opt); if (st != SN_OK) goto done;

    /* an=1, bn=sqrt(1-m), cn=sqrt(m) */
    st = sn_value_copy(ctx, &an, &one); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &one, m, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &bn, &t, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &cn, m, opt); if (st != SN_OK) goto done;

    /* sum = (1/2) * c0^2 */
    st = sn_mul(ctx, &csq, &cn, &cn, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &sum, &half, &csq, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &pow2, 0.5, m, opt); if (st != SN_OK) goto done; /* 2^{n-1} at n=0 */

    max_iter = m->m_bits + 16;
    if (max_iter < 24) max_iter = 24;
    if (max_iter > 200) max_iter = 200;

    for (i = 0; i < max_iter; i++) {
        /* an1 = (an+bn)/2 */
        st = sn_add(ctx, &t, &an, &bn, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &an1, &t, &half, opt); if (st != SN_OK) goto done;
        /* bn1 = sqrt(an*bn) */
        st = sn_mul(ctx, &t, &an, &bn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &bn1, &t, opt); if (st != SN_OK) goto done;
        /* cn1 = (an-bn)/2 */
        st = sn_sub(ctx, &t, &an, &bn, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &cn, &t, &half, opt); if (st != SN_OK) goto done;

        sn_value_clear(ctx, &an); sn_value_move(&an, &an1); sn_value_init(&an1);
        sn_value_clear(ctx, &bn); sn_value_move(&bn, &bn1); sn_value_init(&bn1);

        /* pow2 *= 2 -> 2^{n-1} for next n (after first iter n=1: was 0.5 -> 1) */
        st = sn_mul(ctx, &pow2, &pow2, &two, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &csq, &cn, &cn, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &pow2, &csq, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &sum, &t, opt); if (st != SN_OK) goto done;

        st = sn_to_double(ctx, &an, &da); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &bn, &db); if (st != SN_OK) goto done;
        if (i > 2 && fabs(da - db) <= 1e-16 * (fabs(da) + 1.0)) break;
        /* multiprec: also stop when cn ~ 0 relative */
        st = sn_to_double(ctx, &cn, &da); if (st != SN_OK) goto done;
        if (i > 4 && fabs(da) == 0.0) break;
    }

    /* K = pi / (2 an) */
    st = sn_mul(ctx, &t, &two, &an, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &s, &pi, &t, opt); if (st != SN_OK) goto done;
    if (k_out) {
        st = sn_value_copy(ctx, k_out, &s); if (st != SN_OK) goto done;
    }
    if (e_out) {
        /* E = K * (1 - sum) */
        st = sn_sub(ctx, &t, &one, &sum, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, e_out, &s, &t, opt); if (st != SN_OK) goto done;
    }
done:
    sn_value_clear(ctx, &an); sn_value_clear(ctx, &bn); sn_value_clear(ctx, &cn);
    sn_value_clear(ctx, &an1); sn_value_clear(ctx, &bn1); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &half); sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &pi); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &csq);
    sn_value_clear(ctx, &pow2); sn_value_clear(ctx, &s);
    return st;
}

sn_status sn_soft_ellipk(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt)
{
    sn_status st;
    int cls;
    double dm;
    sn_value one, kv;
    if (!m || m->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(m);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, m); }
    if (sn_fp_signbit(m) && cls != SN_FP_ZERO) return soft_ellip_domain(ctx, out, m);
    if (cls == SN_FP_INFINITE) return soft_ellip_domain(ctx, out, m);
    if (cls == SN_FP_ZERO) {
        /* K(0) = pi/2 */
        sn_value_init(&kv);
        st = soft_const_pi(ctx, &kv, m, opt);
        if (st == SN_OK) {
            sn_value_init(&one);
            st = soft_from_d(ctx, &one, 2.0, m, opt);
            if (st == SN_OK) st = sn_div(ctx, out, &kv, &one, opt);
            sn_value_clear(ctx, &one);
        }
        sn_value_clear(ctx, &kv);
        return st;
    }
    st = sn_to_double(ctx, m, &dm); if (st != SN_OK) return st;
    if (dm > 1.0) return soft_ellip_domain(ctx, out, m);
    if (dm >= 1.0) {
        /* K(1)=+inf */
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, 0, m->e_bits, m->m_bits, m->nan_enabled);
    }
    return soft_ellip_agm(ctx, out, NULL, m, opt);
}

sn_status sn_soft_ellipe(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt)
{
    sn_status st;
    int cls;
    double dm;
    sn_value kv, one;
    if (!m || m->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(m);
    if (cls == SN_FP_NAN) { sn_raise(ctx, SN_FLAG_INVALID); return sn_value_copy(ctx, out, m); }
    if (sn_fp_signbit(m) && cls != SN_FP_ZERO) return soft_ellip_domain(ctx, out, m);
    if (cls == SN_FP_INFINITE) return soft_ellip_domain(ctx, out, m);
    if (cls == SN_FP_ZERO) {
        /* E(0)=pi/2 */
        sn_value_init(&kv);
        st = soft_const_pi(ctx, &kv, m, opt);
        if (st == SN_OK) {
            sn_value_init(&one);
            st = soft_from_d(ctx, &one, 2.0, m, opt);
            if (st == SN_OK) st = sn_div(ctx, out, &kv, &one, opt);
            sn_value_clear(ctx, &one);
        }
        sn_value_clear(ctx, &kv);
        return st;
    }
    st = sn_to_double(ctx, m, &dm); if (st != SN_OK) return st;
    if (dm > 1.0) return soft_ellip_domain(ctx, out, m);
    if (dm >= 1.0) {
        /* E(1)=1 */
        return soft_from_d(ctx, out, 1.0, m, opt);
    }
    sn_value_init(&kv);
    st = soft_ellip_agm(ctx, &kv, out, m, opt);
    sn_value_clear(ctx, &kv);
    return st;
}


/* -------------------------------------------------------------------------- */
/* Incomplete gamma: lower P and upper Q (regularized), plus raw gamma forms. */
/* sn_soft_igamma  -> lower gamma(a,x) / Gamma(a) = P(a,x)                    */
/* sn_soft_igammac -> upper gamma(a,x) / Gamma(a) = Q(a,x) = 1-P              */
/* Domain: a>0, x>=0. Uses series for x < a+1, continued fraction otherwise.  */
/* -------------------------------------------------------------------------- */

static sn_status soft_igamma_domain(sn_ctx *ctx, sn_value *out, const sn_value *a)
{
    sn_raise(ctx, SN_FLAG_INVALID);
    if (a->nan_enabled)
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
}

/* lower incomplete series: P = e^{-x} x^a / Gamma(a) * sum x^k / (a(a+1)...(a+k))
 * or gamma(a,x) series: x^a e^{-x} sum_{n=0} x^n / Gamma(a+n+1)
 * We compute unnormalized then divide by tgamma(a).
 */
static sn_status soft_igamma_series(sn_ctx *ctx, sn_value *out, const sn_value *a,
                                    const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value term, sum, ap, t, one, ax, pref, ga;
    int i, max_iter;
    double dterm, dsum;

    sn_value_init(&term); sn_value_init(&sum); sn_value_init(&ap);
    sn_value_init(&t); sn_value_init(&one); sn_value_init(&ax);
    sn_value_init(&pref); sn_value_init(&ga);

    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    /* term0 = 1/a ; sum */
    st = sn_div(ctx, &term, &one, a, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &sum, &term); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &ap, a); if (st != SN_OK) goto done;

    max_iter = a->m_bits + 40;
    if (max_iter < 60) max_iter = 60;
    if (max_iter > 400) max_iter = 400;

    for (i = 0; i < max_iter; i++) {
        /* term *= x / (a+1+i) ; ap := ap+1 */
        st = sn_add(ctx, &ap, &ap, &one, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, x, &ap, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &sum, &term, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &term, &dterm); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &sum, &dsum); if (st != SN_OK) goto done;
        if (i > 4 && fabs(dterm) <= 1e-16 * (fabs(dsum) + 1.0)) break;
    }

    /* pref = exp(-x) * pow(x,a) */
    st = sn_neg(ctx, &t, x, opt); if (st != SN_OK) goto done;
    st = sn_exp(ctx, &pref, &t, opt); if (st != SN_OK) goto done;
    st = sn_pow(ctx, &ax, x, a, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &pref, &pref, &ax, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &pref, &pref, &sum, opt); if (st != SN_OK) goto done;
    /* P = pref / Gamma(a) */
    st = sn_tgamma(ctx, &ga, a, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &pref, &ga, opt);
done:
    sn_value_clear(ctx, &term); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &ap);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &one); sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &pref); sn_value_clear(ctx, &ga);
    return st;
}

/* Upper continued fraction Lentz for Q(a,x) (NR-style):
 * Q = e^{-x} x^a / Gamma(a) * 1/(x+1-a - 1*(1-a)/(x+3-a - ...))
 * freer: b0=x+1-a, c=1/tiny, d=1/b0, ...
 */
static sn_status soft_igammac_cf(sn_ctx *ctx, sn_value *out, const sn_value *a,
                                 const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value b, c, d, h, an, del, t, one, two, ax, pref, ga, u, v;
    int i, max_iter;
    double dh, ddel;

    sn_value_init(&b); sn_value_init(&c); sn_value_init(&d); sn_value_init(&h);
    sn_value_init(&an); sn_value_init(&del); sn_value_init(&t); sn_value_init(&one);
    sn_value_init(&two); sn_value_init(&ax); sn_value_init(&pref); sn_value_init(&ga);
    sn_value_init(&u); sn_value_init(&v);

    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, a, opt); if (st != SN_OK) goto done;

    /* b = x+1-a ; h = d = 1/b ; c = 1/tiny ~ huge */
    st = sn_add(ctx, &b, x, &one, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &b, &b, a, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c, 1e300, a, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &d, &one, &b, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &h, &d); if (st != SN_OK) goto done;

    max_iter = a->m_bits + 60;
    if (max_iter < 80) max_iter = 80;
    if (max_iter > 500) max_iter = 500;

    for (i = 1; i <= max_iter; i++) {
        /* an = -i*(i-a) */
        st = soft_from_i(ctx, &u, (int64_t)(i), a, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &v, &u, a, opt); if (st != SN_OK) goto done; /* i-a */
        st = sn_mul(ctx, &an, &u, &v, opt); if (st != SN_OK) goto done;
        st = sn_neg(ctx, &an, &an, opt); if (st != SN_OK) goto done;
        /* b += 2 */
        st = sn_add(ctx, &b, &b, &two, opt); if (st != SN_OK) goto done;
        /* d = 1/(an*d + b); c = b + an/c; del=c*d; h*=del */
        st = sn_mul(ctx, &t, &an, &d, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &t, &b, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &d, &one, &t, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &an, &c, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &c, &b, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &del, &c, &d, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &h, &h, &del, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &del, &ddel); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &h, &dh); if (st != SN_OK) goto done;
        (void)dh;
        if (i > 2 && fabs(ddel - 1.0) < 1e-14) break;
    }

    st = sn_neg(ctx, &t, x, opt); if (st != SN_OK) goto done;
    st = sn_exp(ctx, &pref, &t, opt); if (st != SN_OK) goto done;
    st = sn_pow(ctx, &ax, x, a, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &pref, &pref, &ax, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &pref, &pref, &h, opt); if (st != SN_OK) goto done;
    st = sn_tgamma(ctx, &ga, a, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &pref, &ga, opt);
done:
    sn_value_clear(ctx, &b); sn_value_clear(ctx, &c); sn_value_clear(ctx, &d);
    sn_value_clear(ctx, &h); sn_value_clear(ctx, &an); sn_value_clear(ctx, &del);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &one); sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &ax); sn_value_clear(ctx, &pref); sn_value_clear(ctx, &ga);
    sn_value_clear(ctx, &u); sn_value_clear(ctx, &v);
    return st;
}

sn_status sn_soft_igamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    int clsa, clsx;
    double da, dx;
    sn_value one, q;
    if (!a || !x || a->kind != SN_KIND_FLOAT || x->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != x->e_bits || a->m_bits != x->m_bits || a->nan_enabled != x->nan_enabled)
        return SN_ERR_TYPE;
    clsa = sn_fp_classify(a);
    clsx = sn_fp_classify(x);
    if (clsa == SN_FP_NAN || clsx == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, clsa == SN_FP_NAN ? a : x);
    }
    if (sn_fp_signbit(a) || clsa == SN_FP_ZERO || clsa == SN_FP_INFINITE)
        return soft_igamma_domain(ctx, out, a);
    if (sn_fp_signbit(x) && clsx != SN_FP_ZERO) return soft_igamma_domain(ctx, out, a);
    if (clsx == SN_FP_ZERO) return soft_from_d(ctx, out, 0.0, a, opt);
    if (clsx == SN_FP_INFINITE) return soft_from_d(ctx, out, 1.0, a, opt);

    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, x, &dx); if (st != SN_OK) return st;
    if (da <= 0.0 || dx < 0.0) return soft_igamma_domain(ctx, out, a);

    if (dx < da + 1.0) {
        return soft_igamma_series(ctx, out, a, x, opt);
    }
    /* large x: Q via CF, P = 1-Q */
    sn_value_init(&q); sn_value_init(&one);
    st = soft_igammac_cf(ctx, &q, a, x, opt);
    if (st == SN_OK) {
        st = soft_from_d(ctx, &one, 1.0, a, opt);
        if (st == SN_OK) st = sn_sub(ctx, out, &one, &q, opt);
    }
    sn_value_clear(ctx, &q); sn_value_clear(ctx, &one);
    return st;
}

sn_status sn_soft_igammac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    int clsa, clsx;
    double da, dx;
    sn_value one, p;
    if (!a || !x || a->kind != SN_KIND_FLOAT || x->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != x->e_bits || a->m_bits != x->m_bits || a->nan_enabled != x->nan_enabled)
        return SN_ERR_TYPE;
    clsa = sn_fp_classify(a);
    clsx = sn_fp_classify(x);
    if (clsa == SN_FP_NAN || clsx == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, clsa == SN_FP_NAN ? a : x);
    }
    if (sn_fp_signbit(a) || clsa == SN_FP_ZERO || clsa == SN_FP_INFINITE)
        return soft_igamma_domain(ctx, out, a);
    if (sn_fp_signbit(x) && clsx != SN_FP_ZERO) return soft_igamma_domain(ctx, out, a);
    if (clsx == SN_FP_ZERO) return soft_from_d(ctx, out, 1.0, a, opt);
    if (clsx == SN_FP_INFINITE) return soft_from_d(ctx, out, 0.0, a, opt);

    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, x, &dx); if (st != SN_OK) return st;
    if (da <= 0.0 || dx < 0.0) return soft_igamma_domain(ctx, out, a);

    if (dx < da + 1.0) {
        sn_value_init(&p); sn_value_init(&one);
        st = soft_igamma_series(ctx, &p, a, x, opt);
        if (st == SN_OK) {
            st = soft_from_d(ctx, &one, 1.0, a, opt);
            if (st == SN_OK) st = sn_sub(ctx, out, &one, &p, opt);
        }
        sn_value_clear(ctx, &p); sn_value_clear(ctx, &one);
        return st;
    }
    return soft_igammac_cf(ctx, out, a, x, opt);
}


/* -------------------------------------------------------------------------- */
/* Regularized incomplete beta I_x(a,b) and complement.                       */
/* I_x = B_x(a,b)/B(a,b); B(a,b)=Gamma(a)Gamma(b)/Gamma(a+b).                 */
/* Series for small x (after symmetry); continued fraction (Lentz) otherwise. */
/* Domain: a>0, b>0, x in [0,1]. Always soft.                                 */
/* -------------------------------------------------------------------------- */

static sn_status soft_beta_domain(sn_ctx *ctx, sn_value *out, const sn_value *fmt)
{
    sn_raise(ctx, SN_FLAG_INVALID);
    if (fmt->nan_enabled)
        return sn_float_set_nan(ctx, out, fmt->e_bits, fmt->m_bits);
    return sn_float_set_inf(ctx, out, 0, fmt->e_bits, fmt->m_bits, fmt->nan_enabled);
}

/* log B(a,b) = lgamma(a)+lgamma(b)-lgamma(a+b) */
static sn_status soft_log_beta(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                               const sn_op_opt *opt)
{
    sn_status st;
    sn_value la, lb, lab, ab, t;
    sn_value_init(&la); sn_value_init(&lb); sn_value_init(&lab);
    sn_value_init(&ab); sn_value_init(&t);
    /* Always soft lgamma so multiprec front factor matches elevated CF width. */
    st = sn_soft_lgamma(ctx, &la, a, opt); if (st != SN_OK) goto done;
    st = sn_soft_lgamma(ctx, &lb, b, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &ab, a, b, opt); if (st != SN_OK) goto done;
    st = sn_soft_lgamma(ctx, &lab, &ab, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &la, &lb, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, out, &t, &lab, opt);
done:
    sn_value_clear(ctx, &la); sn_value_clear(ctx, &lb); sn_value_clear(ctx, &lab);
    sn_value_clear(ctx, &ab); sn_value_clear(ctx, &t);
    return st;
}

/* Multiprec-friendly Lentz "tiny": 2^-(m+8), not host 1e-300. */
static sn_status soft_tiny_mag(sn_ctx *ctx, sn_value *out, const sn_value *fmt,
                               const sn_op_opt *opt)
{
    int e = fmt->m_bits + 8;
    if (e < 40) e = 40;
    if (e > 4090) e = 4090;
    return soft_pow2i(ctx, out, -e, fmt, opt);
}

/* Continued fraction for incomplete beta (Numerical Recipes betacf / Lentz).
 * Each m applies two aa terms; returns CF factor h such that I_x = front * h.
 * Domain assumed already checked: a>0,b>0,x in (0,1). */
/* Lentz step with caller-cached |tiny| = 2^-(m+8). Avoids rebuild every iteration. */
static sn_status soft_ibeta_cf_step(sn_ctx *ctx, sn_value *c, sn_value *d, sn_value *h,
                                    sn_value *del, const sn_value *aa, const sn_value *one,
                                    const sn_value *tiny, const sn_op_opt *opt, double *ddel)
{
    sn_status st;
    sn_value u, v, absu;
    int rel = 0;
    sn_value_init(&u); sn_value_init(&v); sn_value_init(&absu);
    st = sn_mul(ctx, &u, aa, d, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &u, one, &u, opt); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &absu, &u, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &absu, tiny); if (st != SN_OK) goto done;
    if (rel < 0) {
        st = sn_value_copy(ctx, &u, tiny); if (st != SN_OK) goto done;
    }
    st = sn_div(ctx, d, one, &u, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &v, aa, c, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, c, one, &v, opt); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &absu, c, opt); if (st != SN_OK) goto done;
    st = sn_cmp(ctx, &rel, &absu, tiny); if (st != SN_OK) goto done;
    if (rel < 0) {
        st = sn_value_copy(ctx, c, tiny); if (st != SN_OK) goto done;
    }
    st = sn_mul(ctx, del, c, d, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, h, h, del, opt); if (st != SN_OK) goto done;
    if (ddel) {
        st = sn_to_double(ctx, del, ddel);
        if (st != SN_OK) goto done;
    }
done:
    sn_value_clear(ctx, &u); sn_value_clear(ctx, &v);
    sn_value_clear(ctx, &absu);
    return st;
}

static sn_status soft_ibeta_cf(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                               const sn_value *x, int thr_bits, const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, front, c, d, h, del, m, t, u, v, lbeta, ax, bx, qab, qap, qam, aa, m2;
    sn_value tiny, thrv, one_del, ad;
    int mi, max_iter, rel;
    double ddel = 0.0;

    sn_value_init(&one); sn_value_init(&front);
    sn_value_init(&c); sn_value_init(&d); sn_value_init(&h); sn_value_init(&del);
    sn_value_init(&m); sn_value_init(&t); sn_value_init(&u); sn_value_init(&v);
    sn_value_init(&lbeta); sn_value_init(&ax); sn_value_init(&bx);
    sn_value_init(&qab); sn_value_init(&qap); sn_value_init(&qam);
    sn_value_init(&aa); sn_value_init(&m2);
    sn_value_init(&tiny); sn_value_init(&thrv);
    sn_value_init(&one_del); sn_value_init(&ad);

    st = soft_from_d(ctx, &one, 1.0, a, opt); if (st != SN_OK) goto done;
    /* Cached Lentz tiny (work prec) + stop thr from *target* thr_bits (not elev m). */
    st = soft_tiny_mag(ctx, &tiny, a, opt); if (st != SN_OK) goto done;
    {
        int e = thr_bits + 8;
        if (e < 24) e = 24;
        /* Do not demand tighter than work precision can express. */
        if (e > a->m_bits + 4) e = a->m_bits + 4;
        st = soft_pow2i(ctx, &thrv, -e, a, opt); if (st != SN_OK) goto done;
    }

    /* front = exp(a*ln(x)+b*ln(1-x)-logB)/a */
    st = soft_log_beta(ctx, &lbeta, a, b, opt); if (st != SN_OK) goto done;
    st = sn_soft_log(ctx, &ax, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &ax, &ax, a, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &bx, &one, x, opt); if (st != SN_OK) goto done;
    st = sn_soft_log(ctx, &bx, &bx, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &bx, &bx, b, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &ax, &bx, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &t, &lbeta, opt); if (st != SN_OK) goto done;
    st = sn_soft_exp(ctx, &front, &t, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &front, &front, a, opt); if (st != SN_OK) goto done;

    /* qab=a+b, qap=a+1, qam=a-1 */
    st = sn_add(ctx, &qab, a, b, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &qap, a, &one, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &qam, a, &one, opt); if (st != SN_OK) goto done;

    /* Lentz init: c=1, d=1/(1-qab*x/qap), h=d */
    st = sn_value_copy(ctx, &c, &one); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &qab, x, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &t, &qap, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &one, &t, opt); if (st != SN_OK) goto done;
    {
        sn_value abst;
        sn_value_init(&abst);
        st = sn_abs(ctx, &abst, &t, opt);
        if (st == SN_OK) st = sn_cmp(ctx, &rel, &abst, &tiny);
        if (st == SN_OK && rel < 0) st = sn_value_copy(ctx, &t, &tiny);
        sn_value_clear(ctx, &abst);
        if (st != SN_OK) goto done;
    }
    st = sn_div(ctx, &d, &one, &t, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &h, &d); if (st != SN_OK) goto done;

    /* Iterations scale with target digits, not elevated work width. */
    max_iter = thr_bits * 2 + 80;
    if (max_iter < 120) max_iter = 120;
    if (max_iter > 2000) max_iter = 2000;

    /* Incremental m, m2 (avoid soft_from_i each iteration). */
    st = soft_from_i(ctx, &m, 0, a, opt); if (st != SN_OK) goto done;
    st = soft_from_i(ctx, &m2, 0, a, opt); if (st != SN_OK) goto done;

    for (mi = 1; mi <= max_iter; mi++) {
        st = sn_add(ctx, &m, &m, &one, opt); if (st != SN_OK) goto done;           /* m = mi */
        st = sn_add(ctx, &m2, &m2, &one, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &m2, &m2, &one, opt); if (st != SN_OK) goto done;         /* m2 = 2*mi */

        /* aa1 = m*(b-m)*x / ((qam+m2)*(a+m2)) */
        st = sn_sub(ctx, &u, b, &m, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &aa, &m, &u, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &aa, &aa, x, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &u, &qam, &m2, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &v, a, &m2, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &u, &u, &v, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &aa, &aa, &u, opt); if (st != SN_OK) goto done;
        st = soft_ibeta_cf_step(ctx, &c, &d, &h, &del, &aa, &one, &tiny, opt, &ddel);
        if (st != SN_OK) goto done;

        /* aa2 = -(a+m)*(qab+m)*x / ((a+m2)*(qap+m2)) */
        st = sn_add(ctx, &u, a, &m, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &v, &qab, &m, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &aa, &u, &v, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &aa, &aa, x, opt); if (st != SN_OK) goto done;
        st = sn_neg(ctx, &aa, &aa, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &u, a, &m2, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &v, &qap, &m2, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &u, &u, &v, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &aa, &aa, &u, opt); if (st != SN_OK) goto done;
        st = soft_ibeta_cf_step(ctx, &c, &d, &h, &del, &aa, &one, &tiny, opt, &ddel);
        if (st != SN_OK) goto done;

        /* Fast double early-out when |del-1| is still large (host double). */
        if (a->m_bits <= 53) {
            if (ddel == ddel && fabs(ddel - 1.0) < 1e-14)
                break;
        } else if (ddel == ddel) {
            double adbl = fabs(ddel - 1.0);
            /* Only skip multiprec compare while clearly not converged. */
            if (adbl > 1e-8)
                continue;
        }

        /* Multiprec stop: |del-1| < 2^-(m+4). */
        st = sn_sub(ctx, &one_del, &del, &one, opt); if (st != SN_OK) goto done;
        st = sn_abs(ctx, &ad, &one_del, opt); if (st != SN_OK) goto done;
        st = sn_cmp(ctx, &rel, &ad, &thrv); if (st != SN_OK) goto done;
        if (rel < 0) break;
    }
    st = sn_mul(ctx, out, &front, &h, opt);
done:
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &front);
    sn_value_clear(ctx, &c); sn_value_clear(ctx, &d); sn_value_clear(ctx, &h);
    sn_value_clear(ctx, &del); sn_value_clear(ctx, &m); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &u); sn_value_clear(ctx, &v);
    sn_value_clear(ctx, &lbeta); sn_value_clear(ctx, &ax); sn_value_clear(ctx, &bx);
    sn_value_clear(ctx, &qab); sn_value_clear(ctx, &qap); sn_value_clear(ctx, &qam);
    sn_value_clear(ctx, &aa); sn_value_clear(ctx, &m2);
    sn_value_clear(ctx, &tiny); sn_value_clear(ctx, &thrv);
    sn_value_clear(ctx, &one_del); sn_value_clear(ctx, &ad);
    return st;
}

sn_status sn_soft_ibeta(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                        const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    int clsa, clsb, clsx;
    int e_orig, m_orig, nan_orig, elev = 0;
    double da, db, dx, thr;
    sn_value one, y, r, aw, bw, xw, resw;
    const sn_value *aa = a, *bb = b, *xx = x;
    if (!a || !b || !x) return SN_ERR_TYPE;
    if (a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT || x->kind != SN_KIND_FLOAT)
        return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits || a->nan_enabled != b->nan_enabled ||
        a->e_bits != x->e_bits || a->m_bits != x->m_bits || a->nan_enabled != x->nan_enabled)
        return SN_ERR_TYPE;
    clsa = sn_fp_classify(a); clsb = sn_fp_classify(b); clsx = sn_fp_classify(x);
    if (clsa == SN_FP_NAN || clsb == SN_FP_NAN || clsx == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, clsa == SN_FP_NAN ? a : (clsb == SN_FP_NAN ? b : x));
    }
    if (sn_fp_signbit(a) || clsa == SN_FP_ZERO || clsa == SN_FP_INFINITE ||
        sn_fp_signbit(b) || clsb == SN_FP_ZERO || clsb == SN_FP_INFINITE)
        return soft_beta_domain(ctx, out, a);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, b, &db); if (st != SN_OK) return st;
    st = sn_to_double(ctx, x, &dx); if (st != SN_OK) return st;
    if (da <= 0.0 || db <= 0.0) return soft_beta_domain(ctx, out, a);
    if (dx < 0.0 || dx > 1.0) return soft_beta_domain(ctx, out, a);
    if (clsx == SN_FP_ZERO || dx == 0.0) return soft_from_d(ctx, out, 0.0, a, opt);
    if (dx == 1.0) return soft_from_d(ctx, out, 1.0, a, opt);

    e_orig = a->e_bits; m_orig = a->m_bits; nan_orig = a->nan_enabled;
    sn_value_init(&one); sn_value_init(&y); sn_value_init(&r);
    sn_value_init(&aw); sn_value_init(&bw); sn_value_init(&xw); sn_value_init(&resw);

    /* Elev: front lgamma/log/exp + CF need headroom; thr/stop use thr_bits=m_orig. */
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 96;
        if (m_work < m_orig + 48) m_work = m_orig + 48;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt); if (st != SN_OK) goto done;
            st = sn_cast_float(ctx, &bw, b, e_work, m_work, nan_orig, opt); if (st != SN_OK) goto done;
            st = sn_cast_float(ctx, &xw, x, e_work, m_work, nan_orig, opt); if (st != SN_OK) goto done;
            aa = &aw; bb = &bw; xx = &xw; elev = 1;
            st = sn_to_double(ctx, aa, &da); if (st != SN_OK) goto done;
            st = sn_to_double(ctx, bb, &db); if (st != SN_OK) goto done;
            st = sn_to_double(ctx, xx, &dx); if (st != SN_OK) goto done;
        }
    }

    thr = (da + 1.0) / (da + db + 2.0);
    st = soft_from_d(ctx, &one, 1.0, aa, opt);
    if (st != SN_OK) goto done;
    if (dx < thr) {
        st = soft_ibeta_cf(ctx, elev ? &resw : out, aa, bb, xx, m_orig, opt);
    } else {
        /* I_x(a,b) = 1 - I_{1-x}(b,a) */
        st = sn_sub(ctx, &y, &one, xx, opt);
        if (st == SN_OK) st = soft_ibeta_cf(ctx, &r, bb, aa, &y, m_orig, opt);
        if (st == SN_OK) st = sn_sub(ctx, elev ? &resw : out, &one, &r, opt);
    }
    if (st == SN_OK && elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
done:
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &y); sn_value_clear(ctx, &r);
    sn_value_clear(ctx, &aw); sn_value_clear(ctx, &bw); sn_value_clear(ctx, &xw);
    sn_value_clear(ctx, &resw);
    return st;
}

sn_status sn_soft_ibetac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                         const sn_value *x, const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, p;
    if (!a || !b || !x) return SN_ERR_TYPE;
    sn_value_init(&one); sn_value_init(&p);
    st = sn_soft_ibeta(ctx, &p, a, b, x, opt);
    if (st == SN_OK) {
        st = soft_from_d(ctx, &one, 1.0, a, opt);
        if (st == SN_OK) st = sn_sub(ctx, out, &one, &p, opt);
    }
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &p);
    return st;
}


/* -------------------------------------------------------------------------- */
/* Jacobi elliptic: sn(u|m), cn(u|m), dn(u|m). Parameter m=k^2 in [0,1].       */
/* AGM / descending Landen (cephes-style). Always soft.                       */
/* -------------------------------------------------------------------------- */

static sn_status soft_jacobi_all(sn_ctx *ctx, sn_value *sn, sn_value *cn, sn_value *dn,
                                 const sn_value *u, const sn_value *m, const sn_op_opt *opt)
{
    sn_status st;
    int clsu, clsm, i, n, maxn;
    double dm, du;
    sn_value a[32], c[32], b, t, one, half, two, phi, sphi, cphi, tmp, em, emc;
    sn_value_init(&b); sn_value_init(&t); sn_value_init(&one); sn_value_init(&half);
    sn_value_init(&two); sn_value_init(&phi); sn_value_init(&sphi); sn_value_init(&cphi);
    sn_value_init(&tmp); sn_value_init(&em); sn_value_init(&emc);
    for (i = 0; i < 32; i++) { sn_value_init(&a[i]); sn_value_init(&c[i]); }

    if (!u || !m || u->kind != SN_KIND_FLOAT || m->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (u->e_bits != m->e_bits || u->m_bits != m->m_bits || u->nan_enabled != m->nan_enabled)
        return SN_ERR_TYPE;
    clsu = sn_fp_classify(u); clsm = sn_fp_classify(m);
    if (clsu == SN_FP_NAN || clsm == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        st = sn_value_copy(ctx, sn, clsu == SN_FP_NAN ? u : m);
        if (st == SN_OK) st = sn_value_copy(ctx, cn, sn);
        if (st == SN_OK) st = sn_value_copy(ctx, dn, sn);
        goto done;
    }
    if (sn_fp_signbit(m) && clsm != SN_FP_ZERO) {
        st = soft_beta_domain(ctx, sn, m);
        if (st == SN_OK) st = sn_value_copy(ctx, cn, sn);
        if (st == SN_OK) st = sn_value_copy(ctx, dn, sn);
        goto done;
    }
    st = sn_to_double(ctx, m, &dm); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, u, &du); if (st != SN_OK) goto done;
    if (dm > 1.0) {
        st = soft_beta_domain(ctx, sn, m);
        if (st == SN_OK) st = sn_value_copy(ctx, cn, sn);
        if (st == SN_OK) st = sn_value_copy(ctx, dn, sn);
        goto done;
    }

    st = soft_from_d(ctx, &one, 1.0, u, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, u, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, u, opt); if (st != SN_OK) goto done;

    /* m=0: sn=sin, cn=cos, dn=1 */
    if (dm == 0.0 || clsm == SN_FP_ZERO) {
        st = sn_sin(ctx, sn, u, opt); if (st != SN_OK) goto done;
        st = sn_cos(ctx, cn, u, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, dn, 1.0, u, opt);
        goto done;
    }
    /* m=1: sn=tanh, cn=sech=1/cosh, dn=sech */
    if (dm >= 1.0) {
        st = sn_tanh(ctx, sn, u, opt); if (st != SN_OK) goto done;
        st = sn_cosh(ctx, &t, u, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, cn, &one, &t, opt); if (st != SN_OK) goto done;
        st = sn_value_copy(ctx, dn, cn);
        goto done;
    }

    /* AGM descending: a0=1, b0=sqrt(1-m), c0=sqrt(m) */
    st = sn_value_copy(ctx, &a[0], &one); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &emc, &one, m, opt); if (st != SN_OK) goto done; /* 1-m */
    st = sn_sqrt(ctx, &b, &emc, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &c[0], m, opt); if (st != SN_OK) goto done;

    maxn = 31;
    n = 0;
    for (i = 0; i < maxn; i++) {
        double dc, da;
        /* a_{i+1} = (a_i + b)/2 */
        st = sn_add(ctx, &t, &a[i], &b, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &a[i + 1], &t, &half, opt); if (st != SN_OK) goto done;
        /* c_{i+1} = (a_i - b)/2 */
        st = sn_sub(ctx, &t, &a[i], &b, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &c[i + 1], &t, &half, opt); if (st != SN_OK) goto done;
        /* b = sqrt(a_i * b) */
        st = sn_mul(ctx, &t, &a[i], &b, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &b, &t, opt); if (st != SN_OK) goto done;
        n = i + 1;
        st = sn_to_double(ctx, &c[n], &dc); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &a[n], &da); if (st != SN_OK) goto done;
        if (fabs(dc) <= 1e-16 * (fabs(da) + 1.0)) break;
    }

    /* phi = 2^n * a_n * u */
    st = soft_from_d(ctx, &t, ldexp(1.0, n), u, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &phi, &t, &a[n], opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &phi, &phi, u, opt); if (st != SN_OK) goto done;

    /* ascend: for k=n..1: phi = 0.5*(phi + asin(c_k/a_k * sin(phi))) */
    for (i = n; i >= 1; i--) {
        st = sn_sin(ctx, &sphi, &phi, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &t, &c[i], &a[i], opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &t, &sphi, opt); if (st != SN_OK) goto done;
        st = sn_asin(ctx, &tmp, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &phi, &phi, &tmp, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &phi, &phi, &half, opt); if (st != SN_OK) goto done;
    }

    st = sn_sin(ctx, sn, &phi, opt); if (st != SN_OK) goto done;
    st = sn_cos(ctx, cn, &phi, opt); if (st != SN_OK) goto done;
    /* dn = sqrt(1 - m * sn^2) */
    st = sn_mul(ctx, &t, sn, sn, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, m, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &one, &t, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, dn, &t, opt);
done:
    sn_value_clear(ctx, &b); sn_value_clear(ctx, &t); sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &half); sn_value_clear(ctx, &two); sn_value_clear(ctx, &phi);
    sn_value_clear(ctx, &sphi); sn_value_clear(ctx, &cphi); sn_value_clear(ctx, &tmp);
    sn_value_clear(ctx, &em); sn_value_clear(ctx, &emc);
    for (i = 0; i < 32; i++) { sn_value_clear(ctx, &a[i]); sn_value_clear(ctx, &c[i]); }
    return st;
}

sn_status sn_soft_jacobi_sn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt)
{
    sn_status st;
    sn_value cn, dn;
    sn_value_init(&cn); sn_value_init(&dn);
    st = soft_jacobi_all(ctx, out, &cn, &dn, u, m, opt);
    sn_value_clear(ctx, &cn); sn_value_clear(ctx, &dn);
    return st;
}

sn_status sn_soft_jacobi_cn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt)
{
    sn_status st;
    sn_value sn, dn;
    sn_value_init(&sn); sn_value_init(&dn);
    st = soft_jacobi_all(ctx, &sn, out, &dn, u, m, opt);
    sn_value_clear(ctx, &sn); sn_value_clear(ctx, &dn);
    return st;
}

sn_status sn_soft_jacobi_dn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt)
{
    sn_status st;
    sn_value sn, cn;
    sn_value_init(&sn); sn_value_init(&cn);
    st = soft_jacobi_all(ctx, &sn, &cn, out, u, m, opt);
    sn_value_clear(ctx, &sn); sn_value_clear(ctx, &cn);
    return st;
}


/* -------------------------------------------------------------------------- */
/* digamma ?(x) = d/dx ln ?(x)                                                 */
/* Positive: raise to thr then Stirling: ?(z)=ln z - 1/(2z) - ? B_{2k}/(2k z^{2k}) */
/* Recurrence: ?(z) = ?(z+1) - 1/z                                              */
/* Reflection: ?(1-z) - ?(z) = ? cot(?z)  =>  ?(z) = ?(1-z) - ? cot(?z)        */
/* -------------------------------------------------------------------------- */

static sn_status soft_digamma_stirling(sn_ctx *ctx, sn_value *out, const sn_value *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, two, half, lz, inv, inv2, term, sum, t, bk, den, c;
    int k, max_k;

    sn_value_init(&one); sn_value_init(&two); sn_value_init(&half);
    sn_value_init(&lz); sn_value_init(&inv); sn_value_init(&inv2);
    sn_value_init(&term); sn_value_init(&sum); sn_value_init(&t);
    sn_value_init(&bk); sn_value_init(&den); sn_value_init(&c);

    st = soft_from_d(ctx, &one, 1.0, z, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, z, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, z, opt); if (st != SN_OK) goto done;
    st = sn_soft_log(ctx, &lz, z, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &inv, &one, z, opt); if (st != SN_OK) goto done;
    /* ln z - 1/(2z) */
    st = sn_mul(ctx, &t, &half, &inv, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &sum, &lz, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &inv2, &inv, &inv, opt); if (st != SN_OK) goto done;
    /* term = 1/z^2 = z^{-2}; series ? B_{2k}/(2k) * z^{-2k} */
    st = sn_value_copy(ctx, &term, &inv2); if (st != SN_OK) goto done;

    max_k = (int)(sizeof(soft_bern_even_q) / sizeof(soft_bern_even_q[0]));
    if (z->m_bits <= 64) { if (max_k > 10) max_k = 10; }
    else if (z->m_bits <= 128) { if (max_k > 12) max_k = 12; }
    else if (z->m_bits <= 256) { if (max_k > 14) max_k = 14; }

    for (k = 1; k <= max_k; k++) {
        st = soft_load_bern(ctx, &bk, k, z, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &den, (int64_t)((2 * k)), z, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &c, &bk, &den, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &c, &term, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &sum, &sum, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &term, &term, &inv2, opt); if (st != SN_OK) goto done;
        /* no sn_to_double early-exit (see soft_lgamma_stirling) */
    }
    st = sn_value_copy(ctx, out, &sum);
done:
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &two); sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &lz); sn_value_clear(ctx, &inv); sn_value_clear(ctx, &inv2);
    sn_value_clear(ctx, &term); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &bk); sn_value_clear(ctx, &den); sn_value_clear(ctx, &c);
    return st;
}

/* Raise z until >= thr; accumulate sum 1/(z+i) so ?(z)=?(z_raised)-sum */
static sn_status soft_digamma_raise(sn_ctx *ctx, sn_value *z_out, sn_value *corr,
                                    const sn_value *z_in, double thr, const sn_op_opt *opt)
{
    sn_status st;
    sn_value z, one, inv, t, sum;
    double dz;
    int guard = 0;

    sn_value_init(&z); sn_value_init(&one); sn_value_init(&inv);
    sn_value_init(&t); sn_value_init(&sum);

    st = sn_value_copy(ctx, &z, z_in); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, z_in, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &sum, 0.0, z_in, opt); if (st != SN_OK) goto done;

    for (;;) {
        st = sn_to_double(ctx, &z, &dz); if (st != SN_OK) goto done;
        if (dz >= thr) break;
        st = sn_div(ctx, &inv, &one, &z, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &inv, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum); sn_value_move(&sum, &t); sn_value_init(&t);
        st = sn_add(ctx, &t, &z, &one, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &z); sn_value_move(&z, &t); sn_value_init(&t);
        if (++guard > 100000) { st = SN_ERR_DOMAIN; goto done; }
    }
    st = sn_value_copy(ctx, z_out, &z); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, corr, &sum);
done:
    sn_value_clear(ctx, &z); sn_value_clear(ctx, &one); sn_value_clear(ctx, &inv);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &sum);
    return st;
}

/* thr_m: target m_bits for raise thr (original precision, not elevated). */
static sn_status soft_digamma_pos(sn_ctx *ctx, sn_value *out, const sn_value *x, int thr_m, const sn_op_opt *opt)
{
    sn_status st;
    sn_value z, corr, psi;
    double thr;

    sn_value_init(&z); sn_value_init(&corr); sn_value_init(&psi);
    if (thr_m <= 0) thr_m = x->m_bits;
    thr = 20.0 + 0.45 * (double)thr_m;
    if (thr < 16.0) thr = 16.0;
    if (thr > 320.0) thr = 320.0;
    st = soft_digamma_raise(ctx, &z, &corr, x, thr, opt); if (st != SN_OK) goto done;
    st = soft_digamma_stirling(ctx, &psi, &z, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, out, &psi, &corr, opt);
done:
    sn_value_clear(ctx, &z); sn_value_clear(ctx, &corr); sn_value_clear(ctx, &psi);
    return st;
}

sn_status sn_soft_digamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, pi, t, u, v, ax, cotv, s, aw, resw;
    int cls, elev = 0, e_orig, m_orig, nan_orig;
    double da;
    const sn_value *x;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        if (sn_fp_signbit(a)) {
            sn_raise(ctx, SN_FLAG_INVALID);
            if (a->nan_enabled)
                return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        }
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }
    if (cls == SN_FP_ZERO || soft_is_nonpos_int(ctx, a)) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        if (a->nan_enabled)
            return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }

    e_orig = a->e_bits; m_orig = a->m_bits; nan_orig = a->nan_enabled;
    sn_value_init(&one); sn_value_init(&pi); sn_value_init(&t);
    sn_value_init(&u); sn_value_init(&v); sn_value_init(&ax);
    sn_value_init(&cotv); sn_value_init(&s);
    sn_value_init(&aw); sn_value_init(&resw);

    x = a;
    if (m_orig > 52) {
        int e_work = e_orig < 16 ? 16 : e_orig;
        int m_work = m_orig + 128;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        if (m_work > m_orig) {
            st = sn_cast_float(ctx, &aw, a, e_work, m_work, nan_orig, opt);
            if (st != SN_OK) goto done;
            x = &aw;
            elev = 1;
        }
    }

    st = sn_to_double(ctx, x, &da); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;

    if (da >= 0.5) {
        st = soft_digamma_pos(ctx, elev ? &resw : out, x, m_orig, opt);
    } else {
        st = soft_const_pi(ctx, &pi, x, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &ax, &one, x, opt); if (st != SN_OK) goto done;
        st = soft_digamma_pos(ctx, &u, &ax, m_orig, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &pi, x, opt); if (st != SN_OK) goto done;
        st = sn_soft_sin(ctx, &s, &t, opt); if (st != SN_OK) goto done;
        st = sn_soft_cos(ctx, &v, &t, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &cotv, &v, &s, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &pi, &cotv, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, elev ? &resw : out, &u, &t, opt);
    }
    if (st == SN_OK && elev)
        st = sn_cast_float(ctx, out, &resw, e_orig, m_orig, nan_orig, opt);
done:
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &pi); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &u); sn_value_clear(ctx, &v); sn_value_clear(ctx, &ax);
    sn_value_clear(ctx, &cotv); sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &aw); sn_value_clear(ctx, &resw);
    return st;
}



/* -------------------------------------------------------------------------- */
/* trigamma psi_1(z)=psi^{(1)}(z) and polygamma psi^{(n)}(z)                     */
/* Recurrence: psi^{(n)}(z)=psi^{(n)}(z+1)+(-1)^{n+1} n!/z^{n+1}  (n>=1)       */
/* Large z: psi^{(n)}(z) ~ (-1)^{n+1}[ (n-1)!/z^n + n!/(2 z^{n+1})              */
/*   + sum_{k>=1} B_{2k} * C(n+2k-1,2k) / z^{n+2k} ]                           */
/* Raise until thr then asymptotic. n=0 delegates to digamma.                  */
/* Poles at non-positive integers.                                             */
/* -------------------------------------------------------------------------- */

/* n! as multiprec float (n small; used by polygamma, avoid double factorial). */
static sn_status soft_fact_mp(sn_ctx *ctx, sn_value *out, int n, const sn_value *fmt,
                              const sn_op_opt *opt)
{
    sn_status st;
    sn_value f, t, k;
    int i;

    if (n < 0) return SN_ERR_DOMAIN;
    sn_value_init(&f); sn_value_init(&t); sn_value_init(&k);
    st = soft_from_i(ctx, &f, 1, fmt, opt); if (st != SN_OK) goto done;
    for (i = 2; i <= n; i++) {
        st = soft_from_i(ctx, &k, (int64_t)i, fmt, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &f, &k, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &f); sn_value_move(&f, &t); sn_value_init(&t);
    }
    st = sn_value_copy(ctx, out, &f);
done:
    sn_value_clear(ctx, &f); sn_value_clear(ctx, &t); sn_value_clear(ctx, &k);
    return st;
}

/* binom(n+k2-1, k2) multiprec: product_{j=1}^{k2} (n-1+j)/j  (for polygamma C(n+2k-1,2k)). */
static sn_status soft_binom_n2k_mp(sn_ctx *ctx, sn_value *out, int n, int k2,
                                   const sn_value *fmt, const sn_op_opt *opt)
{
    sn_status st;
    sn_value r, t, num, den;
    int j;

    if (k2 < 0 || n < 1) return SN_ERR_DOMAIN;
    sn_value_init(&r); sn_value_init(&t); sn_value_init(&num); sn_value_init(&den);
    st = soft_from_i(ctx, &r, 1, fmt, opt); if (st != SN_OK) goto done;
    for (j = 1; j <= k2; j++) {
        st = soft_from_i(ctx, &num, (int64_t)(n - 1 + j), fmt, opt); if (st != SN_OK) goto done;
        st = soft_from_i(ctx, &den, (int64_t)j, fmt, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &r, &num, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &r); sn_value_move(&r, &t); sn_value_init(&t);
        st = sn_div(ctx, &t, &r, &den, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &r); sn_value_move(&r, &t); sn_value_init(&t);
    }
    st = sn_value_copy(ctx, out, &r);
done:
    sn_value_clear(ctx, &r); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &num); sn_value_clear(ctx, &den);
    return st;
}

/* z^{-p} for integer p>=1 via repeated division. */
static sn_status soft_pow_neg_int(sn_ctx *ctx, sn_value *out, const sn_value *z, int p,
                                  const sn_op_opt *opt)
{
    sn_status st;
    sn_value one, inv, t;
    int i;

    if (p < 1) return SN_ERR_DOMAIN;
    sn_value_init(&one); sn_value_init(&inv); sn_value_init(&t);
    st = soft_from_d(ctx, &one, 1.0, z, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &inv, &one, z, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, out, &inv); if (st != SN_OK) goto done;
    for (i = 1; i < p; i++) {
        st = sn_mul(ctx, &t, out, &inv, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, out); sn_value_move(out, &t); sn_value_init(&t);
    }
done:
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &inv); sn_value_clear(ctx, &t);
    return st;
}

/* Asymptotic for polygamma n>=1 at large positive z (full multiprec coeffs). */
static sn_status soft_polygamma_asymp(sn_ctx *ctx, sn_value *out, int n, const sn_value *z,
                                     const sn_op_opt *opt)
{
    sn_status st;
    sn_value sum, t, u, half, c, bk, bin, factnm1, factn, zero;
    int k, max_k, sign_pos;

    if (n < 1) return SN_ERR_DOMAIN;

    sn_value_init(&sum); sn_value_init(&t); sn_value_init(&u);
    sn_value_init(&half); sn_value_init(&c); sn_value_init(&bk);
    sn_value_init(&bin); sn_value_init(&factnm1); sn_value_init(&factn);
    sn_value_init(&zero);

    /* sign = (-1)^{n+1}: odd n -> +1, even n -> -1 */
    sign_pos = (n % 2) ? 1 : 0;

    st = soft_fact_mp(ctx, &factnm1, n - 1, z, opt); if (st != SN_OK) goto done;
    st = soft_fact_mp(ctx, &factn, n, z, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &half, 0.5, z, opt); if (st != SN_OK) goto done;

    /* (n-1)! / z^n */
    st = soft_pow_neg_int(ctx, &t, z, n, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &sum, &factnm1, &t, opt); if (st != SN_OK) goto done;

    /* + n! / (2 z^{n+1}) */
    st = soft_pow_neg_int(ctx, &t, z, n + 1, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &factn, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &u, &half, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &sum, &u, opt); if (st != SN_OK) goto done;
    sn_value_clear(ctx, &sum); sn_value_move(&sum, &t); sn_value_init(&t);

    max_k = (int)(sizeof(soft_bern_even_q) / sizeof(soft_bern_even_q[0]));
    if (z->m_bits <= 64) { if (max_k > 10) max_k = 10; }
    else if (z->m_bits <= 128) { if (max_k > 12) max_k = 12; }
    else if (z->m_bits <= 256) { if (max_k > 14) max_k = 14; }

    for (k = 1; k <= max_k; k++) {
        /* B_{2k} * C(n+2k-1, 2k) * (n-1)! / z^{n+2k} */
        st = soft_load_bern(ctx, &bk, k, z, opt); if (st != SN_OK) goto done;
        st = soft_binom_n2k_mp(ctx, &bin, n, 2 * k, z, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &c, &bk, &bin, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &c, &c, &factnm1, opt); if (st != SN_OK) goto done;
        st = soft_pow_neg_int(ctx, &t, z, n + 2 * k, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &u, &c, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &sum, &u, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum); sn_value_move(&sum, &t); sn_value_init(&t);
    }

    if (sign_pos) {
        st = sn_value_copy(ctx, out, &sum);
    } else {
        st = soft_from_d(ctx, &zero, 0.0, z, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, out, &zero, &sum, opt);
    }
done:
    sn_value_clear(ctx, &sum); sn_value_clear(ctx, &t); sn_value_clear(ctx, &u);
    sn_value_clear(ctx, &half); sn_value_clear(ctx, &c); sn_value_clear(ctx, &bk);
    sn_value_clear(ctx, &bin); sn_value_clear(ctx, &factnm1); sn_value_clear(ctx, &factn);
    sn_value_clear(ctx, &zero);
    return st;
}

/* Raise z until >= thr; corr = (-1)^{n+1} n! * sum_j 1/(z+j)^{n+1} */
static sn_status soft_polygamma_raise(sn_ctx *ctx, sn_value *z_out, sn_value *corr,
                                      int n, const sn_value *z_in, double thr,
                                      const sn_op_opt *opt)
{
    sn_status st;
    sn_value z, one, invp, t, sum, nf, c;
    double dz;
    int guard = 0;

    sn_value_init(&z); sn_value_init(&one); sn_value_init(&invp);
    sn_value_init(&t); sn_value_init(&sum); sn_value_init(&nf); sn_value_init(&c);

    st = sn_value_copy(ctx, &z, z_in); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &one, 1.0, z_in, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &sum, 0.0, z_in, opt); if (st != SN_OK) goto done;
    /* (-1)^{n+1} * n!  (multiprec factorial; no double seed) */
    st = soft_fact_mp(ctx, &nf, n, z_in, opt); if (st != SN_OK) goto done;
    if ((n % 2) == 0) {
        sn_value zero;
        sn_value_init(&zero);
        st = soft_from_d(ctx, &zero, 0.0, z_in, opt);
        if (st == SN_OK) st = sn_sub(ctx, &t, &zero, &nf, opt);
        sn_value_clear(ctx, &zero);
        if (st != SN_OK) goto done;
        sn_value_clear(ctx, &nf); sn_value_move(&nf, &t); sn_value_init(&t);
    }

    for (;;) {
        st = sn_to_double(ctx, &z, &dz); if (st != SN_OK) goto done;
        if (dz >= thr) break;
        /* avoid exact non-positive integer poles during raise */
        if (dz <= 0.0 && fabs(dz - floor(dz + 0.5)) < 1e-12) {
            st = SN_ERR_DOMAIN;
            goto done;
        }
        st = soft_pow_neg_int(ctx, &invp, &z, n + 1, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &nf, &invp, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &c, &sum, &t, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &sum); sn_value_move(&sum, &c); sn_value_init(&c);
        st = sn_add(ctx, &t, &z, &one, opt); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &z); sn_value_move(&z, &t); sn_value_init(&t);
        if (++guard > 100000) { st = SN_ERR_DOMAIN; goto done; }
    }
    st = sn_value_copy(ctx, z_out, &z); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, corr, &sum);
done:
    sn_value_clear(ctx, &z); sn_value_clear(ctx, &one); sn_value_clear(ctx, &invp);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &nf);
    sn_value_clear(ctx, &c);
    return st;
}

static sn_status soft_polygamma_core(sn_ctx *ctx, sn_value *out, int n, const sn_value *x,
                                     const sn_op_opt *opt)
{
    sn_status st;
    sn_value z, corr, psi;
    double thr;

    sn_value_init(&z); sn_value_init(&corr); sn_value_init(&psi);
    thr = 8.0 + (double)(x->m_bits / 16) + (double)n;
    if (thr < 10.0) thr = 10.0;
    if (thr > 60.0) thr = 60.0;
    st = soft_polygamma_raise(ctx, &z, &corr, n, x, thr, opt); if (st != SN_OK) goto done;
    st = soft_polygamma_asymp(ctx, &psi, n, &z, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, out, &psi, &corr, opt);
done:
    sn_value_clear(ctx, &z); sn_value_clear(ctx, &corr); sn_value_clear(ctx, &psi);
    return st;
}

sn_status sn_soft_polygamma(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int cls;
    double da;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (n < 0) return SN_ERR_DOMAIN;
    if (n == 0) return sn_soft_digamma(ctx, out, a, opt);
    if (n > 32) return SN_ERR_DOMAIN; /* factorial / series practical limit */

    cls = sn_fp_classify(a);
    if (cls == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, a);
    }
    if (cls == SN_FP_INFINITE) {
        if (sn_fp_signbit(a)) {
            sn_raise(ctx, SN_FLAG_INVALID);
            if (a->nan_enabled)
                return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        }
        /* psi^{(n)}(+inf) = 0 for n>=1 */
        return soft_from_d(ctx, out, 0.0, a, opt);
    }
    if (cls == SN_FP_ZERO || soft_is_nonpos_int(ctx, a)) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        if (a->nan_enabled)
            return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
    }

    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    /* Core raise handles negative non-integers by walking past poles. */
    return soft_polygamma_core(ctx, out, n, a, opt);
}

sn_status sn_soft_trigamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_polygamma(ctx, out, 1, a, opt);
}


/* -------------------------------------------------------------------------- */
/* Incomplete elliptic integral of the first kind F(?|m) via Carlson RF.      */
/* F(?|m) = sin? * RF(cos??, 1-m sin??, 1),  m=k?.                            */
/* ? reduced by oddness; |?|<=?/2 for this implementation.                      */
/* Special: m=0 -> ?; ?=?/2 -> K(m).                                            */
/* -------------------------------------------------------------------------- */

/* Carlson RF(x,y,z) = (1/2)?_0^? [(t+x)(t+y)(t+z)]^{-1/2} dt
 * Duplication + series (Carlson 1979 / Numerical Recipes). */
static sn_status soft_carlson_rf(sn_ctx *ctx, sn_value *out,
                                 const sn_value *x, const sn_value *y, const sn_value *z,
                                 const sn_op_opt *opt)
{
    sn_status st;
    sn_value xn, yn, zn, an, an1, sx, sy, sz, lam, t, u, v, one, two, three, four;
    sn_value dx, dy, dz, e2, e3, s, c1, c2, c3, c4;
    int i, max_iter;
    double ddx, ddy, ddz, thr;

    sn_value_init(&xn); sn_value_init(&yn); sn_value_init(&zn); sn_value_init(&an);
    sn_value_init(&an1); sn_value_init(&sx); sn_value_init(&sy); sn_value_init(&sz);
    sn_value_init(&lam); sn_value_init(&t); sn_value_init(&u); sn_value_init(&v);
    sn_value_init(&one); sn_value_init(&two); sn_value_init(&three); sn_value_init(&four);
    sn_value_init(&dx); sn_value_init(&dy); sn_value_init(&dz);
    sn_value_init(&e2); sn_value_init(&e3); sn_value_init(&s);
    sn_value_init(&c1); sn_value_init(&c2); sn_value_init(&c3); sn_value_init(&c4);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &three, 3.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &xn, x); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &yn, y); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &zn, z); if (st != SN_OK) goto done;

    thr = 0.0025; /* relative convergence of deviations */
    if (x->m_bits > 64) thr = 1e-6;
    max_iter = x->m_bits + 20;
    if (max_iter < 40) max_iter = 40;
    if (max_iter > 200) max_iter = 200;

    for (i = 0; i < max_iter; i++) {
        /* A = (x+y+z)/3 */
        st = sn_add(ctx, &t, &xn, &yn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &t, &zn, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &an, &t, &three, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dx, &an, &xn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dy, &an, &yn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dz, &an, &zn, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dx, &dx, &an, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dy, &dy, &an, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dz, &dz, &an, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dx, &ddx); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dy, &ddy); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dz, &ddz); if (st != SN_OK) goto done;
        if (fabs(ddx) < thr && fabs(ddy) < thr && fabs(ddz) < thr) break;

        st = sn_sqrt(ctx, &sx, &xn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &sy, &yn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &sz, &zn, opt); if (st != SN_OK) goto done;
        /* ? = ?x?y + ?y?z + ?z?x */
        st = sn_mul(ctx, &lam, &sx, &sy, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &sy, &sz, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &lam, &lam, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &sz, &sx, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &lam, &lam, &t, opt); if (st != SN_OK) goto done;
        /* x=(x+?)/4 etc */
        st = sn_add(ctx, &t, &xn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &xn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &yn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &yn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &zn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &zn, &t, &four, opt); if (st != SN_OK) goto done;
    }

    /* series: RF ? 1/?A * (1 - e2/10 + e3/14 + e2?/24 - 3 e2 e3/44) */
    /* e2 = dx dy + dy dz + dz dx; e3 = dx dy dz  (dx already relative) */
    st = sn_mul(ctx, &e2, &dx, &dy, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &dy, &dz, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &e2, &e2, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &dz, &dx, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &e2, &e2, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &e3, &dx, &dy, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &e3, &e3, &dz, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &c1, 0.1, x, opt); if (st != SN_OK) goto done;   /* 1/10 */
    st = soft_from_d(ctx, &c2, 1.0/14.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c3, 1.0/24.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c4, 3.0/44.0, x, opt); if (st != SN_OK) goto done;

    st = sn_value_copy(ctx, &s, &one); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &c1, &e2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &c2, &e3, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &e2, &e2, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &c3, &u, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &e2, &e3, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &c4, &u, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;

    st = sn_sqrt(ctx, &t, &an, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &s, &t, opt);
done:
    sn_value_clear(ctx, &xn); sn_value_clear(ctx, &yn); sn_value_clear(ctx, &zn);
    sn_value_clear(ctx, &an); sn_value_clear(ctx, &an1); sn_value_clear(ctx, &sx);
    sn_value_clear(ctx, &sy); sn_value_clear(ctx, &sz); sn_value_clear(ctx, &lam);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &u); sn_value_clear(ctx, &v);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &two); sn_value_clear(ctx, &three);
    sn_value_clear(ctx, &four); sn_value_clear(ctx, &dx); sn_value_clear(ctx, &dy);
    sn_value_clear(ctx, &dz); sn_value_clear(ctx, &e2); sn_value_clear(ctx, &e3);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c1); sn_value_clear(ctx, &c2);
    sn_value_clear(ctx, &c3); sn_value_clear(ctx, &c4);
    return st;
}

sn_status sn_soft_ellipf(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m,
                         const sn_op_opt *opt)
{
    sn_status st;
    int clsp, clsm, neg;
    double dm, dphi, halfpi;
    sn_value s, c, s2, c2, t, one, x, y, z, rf, aphi, pi, two;

    if (!phi || !m || phi->kind != SN_KIND_FLOAT || m->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (phi->e_bits != m->e_bits || phi->m_bits != m->m_bits || phi->nan_enabled != m->nan_enabled)
        return SN_ERR_TYPE;

    clsp = sn_fp_classify(phi);
    clsm = sn_fp_classify(m);
    if (clsp == SN_FP_NAN || clsm == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, clsp == SN_FP_NAN ? phi : m);
    }
    if (sn_fp_signbit(m) && clsm != SN_FP_ZERO) return soft_ellip_domain(ctx, out, m);
    if (clsm == SN_FP_INFINITE) return soft_ellip_domain(ctx, out, m);

    sn_value_init(&s); sn_value_init(&c); sn_value_init(&s2); sn_value_init(&c2);
    sn_value_init(&t); sn_value_init(&one); sn_value_init(&x); sn_value_init(&y);
    sn_value_init(&z); sn_value_init(&rf); sn_value_init(&aphi); sn_value_init(&pi);
    sn_value_init(&two);

    st = sn_to_double(ctx, m, &dm); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, phi, &dphi); if (st != SN_OK) goto done;
    if (dm > 1.0) { st = soft_ellip_domain(ctx, out, m); goto done; }
    if (dm >= 1.0) {
        /* m=1: F(?|1)=artanh(sin ?) for |?|<?/2; ?=?/2 -> +inf */
        halfpi = 1.5707963267948966;
        if (fabs(dphi) >= halfpi - 1e-15) {
            sn_raise(ctx, SN_FLAG_DIVZERO);
            st = sn_float_set_inf(ctx, out, sn_fp_signbit(phi), m->e_bits, m->m_bits, m->nan_enabled);
            goto done;
        }
        st = sn_soft_sin(ctx, &s, phi, opt); if (st != SN_OK) goto done;
        st = sn_soft_atanh(ctx, out, &s, opt);
        goto done;
    }

    /* odd in ? */
    neg = sn_fp_signbit(phi) && clsp != SN_FP_ZERO;
    st = sn_abs(ctx, &aphi, phi, opt); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &aphi, &dphi); if (st != SN_OK) goto done;

    /* |?| ~ ?/2 -> K(m) */
    st = soft_const_pi(ctx, &pi, m, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, m, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &pi, &two, opt); if (st != SN_OK) goto done; /* ?/2 */
    st = sn_to_double(ctx, &t, &halfpi); if (st != SN_OK) goto done;
    if (fabs(dphi - halfpi) < 1e-14 || dphi > halfpi) {
        if (dphi > halfpi + 1e-12) {
            /* outside principal range */
            st = soft_ellip_domain(ctx, out, phi);
            goto done;
        }
        st = sn_soft_ellipk(ctx, out, m, opt);
        if (st == SN_OK && neg) st = sn_neg(ctx, out, out, opt);
        goto done;
    }
    if (clsp == SN_FP_ZERO || dphi == 0.0) {
        st = soft_from_d(ctx, out, 0.0, m, opt);
        goto done;
    }

    /* m=0: F(?|0)=? */
    if (dm == 0.0 || clsm == SN_FP_ZERO) {
        st = sn_value_copy(ctx, out, phi);
        goto done;
    }

    st = soft_from_d(ctx, &one, 1.0, m, opt); if (st != SN_OK) goto done;
    st = sn_soft_sin(ctx, &s, &aphi, opt); if (st != SN_OK) goto done;
    st = sn_soft_cos(ctx, &c, &aphi, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &s2, &s, &s, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &c2, &c, &c, opt); if (st != SN_OK) goto done;
    /* x=c?, y=1-m s?, z=1 */
    st = sn_value_copy(ctx, &x, &c2); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, m, &s2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &y, &one, &t, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &z, &one); if (st != SN_OK) goto done;
    st = soft_carlson_rf(ctx, &rf, &x, &y, &z, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, out, &s, &rf, opt); if (st != SN_OK) goto done;
    if (neg) st = sn_neg(ctx, out, out, opt);
done:
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c); sn_value_clear(ctx, &s2);
    sn_value_clear(ctx, &c2); sn_value_clear(ctx, &t); sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &x); sn_value_clear(ctx, &y); sn_value_clear(ctx, &z);
    sn_value_clear(ctx, &rf); sn_value_clear(ctx, &aphi); sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &two);
    return st;
}


/* -------------------------------------------------------------------------- */
/* Incomplete elliptic integral of the second kind E(?|m) via Carlson RD.     */
/* E(?|m) = sin??RF(c?,1-m s?,1) - (m/3) sin?? ? RD(c?,1-m s?,1)             */
/*          with s=sin?, c=cos?. Special: m=0??; ?=?/2?E(m) complete.         */
/* -------------------------------------------------------------------------- */

/* Carlson RD(x,y,z) = (3/2)?_0^? [(t+x)(t+y)]^{-1/2} (t+z)^{-3/2} dt
 * Duplication + series (Carlson / Numerical Recipes). */
static sn_status soft_carlson_rd(sn_ctx *ctx, sn_value *out,
                                 const sn_value *x, const sn_value *y, const sn_value *z,
                                 const sn_op_opt *opt)
{
    sn_status st;
    sn_value xn, yn, zn, an, sx, sy, sz, lam, t, u, v, one, two, three, four;
    sn_value dx, dy, dz, ea, eb, ec, ed, ee, s, sum, fac, c;
    int i, max_iter;
    double ddx, ddy, ddz, thr;

    sn_value_init(&xn); sn_value_init(&yn); sn_value_init(&zn); sn_value_init(&an);
    sn_value_init(&sx); sn_value_init(&sy); sn_value_init(&sz); sn_value_init(&lam);
    sn_value_init(&t); sn_value_init(&u); sn_value_init(&v);
    sn_value_init(&one); sn_value_init(&two); sn_value_init(&three); sn_value_init(&four);
    sn_value_init(&dx); sn_value_init(&dy); sn_value_init(&dz);
    sn_value_init(&ea); sn_value_init(&eb); sn_value_init(&ec); sn_value_init(&ed);
    sn_value_init(&ee); sn_value_init(&s); sn_value_init(&sum); sn_value_init(&fac);
    sn_value_init(&c);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &three, 3.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &xn, x); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &yn, y); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &zn, z); if (st != SN_OK) goto done;
    /* sum accumulates (? path) contribution: ? 1/(?z (z+?)) * 4^{-n} / something
     * NR: sum += fac / (sqrt(zn)*(zn+lam)); fac *= 0.25 each step; then RD=3*sum + series */
    st = soft_from_d(ctx, &sum, 0.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &fac, 1.0, x, opt); if (st != SN_OK) goto done;

    thr = 0.0025;
    if (x->m_bits > 64) thr = 1e-6;
    max_iter = x->m_bits + 20;
    if (max_iter < 40) max_iter = 40;
    if (max_iter > 200) max_iter = 200;

    for (i = 0; i < max_iter; i++) {
        /* A = (x+y+3z)/5 */
        st = sn_add(ctx, &t, &xn, &yn, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &u, &three, &zn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &t, &u, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &c, 5.0, x, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &an, &t, &c, opt); if (st != SN_OK) goto done;

        st = sn_sub(ctx, &dx, &an, &xn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dy, &an, &yn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dz, &an, &zn, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dx, &dx, &an, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dy, &dy, &an, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dz, &dz, &an, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dx, &ddx); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dy, &ddy); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dz, &ddz); if (st != SN_OK) goto done;
        if (fabs(ddx) < thr && fabs(ddy) < thr && fabs(ddz) < thr) break;

        st = sn_sqrt(ctx, &sx, &xn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &sy, &yn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &sz, &zn, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &lam, &sx, &sy, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &sy, &sz, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &lam, &lam, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &sz, &sx, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &lam, &lam, &t, opt); if (st != SN_OK) goto done;

        /* sum += fac / (?zn * (zn+?)) */
        st = sn_add(ctx, &t, &zn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &u, &sz, &t, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &v, &fac, &u, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &sum, &v, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &fac, &fac, &four, opt); if (st != SN_OK) goto done;

        st = sn_add(ctx, &t, &xn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &xn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &yn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &yn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &zn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &zn, &t, &four, opt); if (st != SN_OK) goto done;
    }

    /* Series after convergence (NR rd):
     * ea = dx*dy; eb = dz*dz; ec = ea - eb; ed = ea - 6*eb; ee = ed + ec + ec;
     * RD ? 3*sum + fac/A^{3/2} * (1 + ed*(-3/14 + 9/88*ed - 9/52*dz*ee + ...) ...)
     * Use truncated NR form sufficient for double:
     *   1 - (3/14)*ed + (1/6)*ea*(ea-100/9*eb? wait use standard:
     *   c1=-3/14, c2=1/6, c3=9/88, c4=-3/22, c5=-9/52, c6=3/26
     *   s = 1 + ed*(c1 + c3*ed - c5*dz*ee) + dz*(c2*ea + c4*dz*ee - c6*dz*eb)
     */
    {
        sn_value c1, c2, c3, c4, c5, c6, six, ee2;
        sn_value_init(&c1); sn_value_init(&c2); sn_value_init(&c3);
        sn_value_init(&c4); sn_value_init(&c5); sn_value_init(&c6);
        sn_value_init(&six); sn_value_init(&ee2);

        st = sn_mul(ctx, &ea, &dx, &dy, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &eb, &dz, &dz, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_sub(ctx, &ec, &ea, &eb, opt); if (st != SN_OK) goto rd_series_done;
        st = soft_from_d(ctx, &six, 6.0, x, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &t, &six, &eb, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_sub(ctx, &ed, &ea, &t, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &ee, &ed, &ec, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &ee, &ee, &ec, opt); if (st != SN_OK) goto rd_series_done;

        st = soft_from_d(ctx, &c1, -3.0/14.0, x, opt); if (st != SN_OK) goto rd_series_done;
        st = soft_from_d(ctx, &c2, 1.0/6.0, x, opt); if (st != SN_OK) goto rd_series_done;
        st = soft_from_d(ctx, &c3, 9.0/88.0, x, opt); if (st != SN_OK) goto rd_series_done;
        st = soft_from_d(ctx, &c4, -3.0/22.0, x, opt); if (st != SN_OK) goto rd_series_done;
        st = soft_from_d(ctx, &c5, -9.0/52.0, x, opt); if (st != SN_OK) goto rd_series_done;
        st = soft_from_d(ctx, &c6, 3.0/26.0, x, opt); if (st != SN_OK) goto rd_series_done;

        /* s = 1 + ed*(c1 + c3*ed + c5*dz*ee) + dz*(c2*ea + dz*(c4*ee + c6*dz*eb? NR: c4*dz*ee - c6*dz*eb))
         * NR formula:
         * s = 1.0 + ed*(-C1+C3*ed-C5*dz*ee) + dz*(C2*ea+dz*(-C4*ee+C6*dz*eb))
         * with C1=3/14 etc positive constants; we baked signs into c1..c6.
         * Using:
         *   term1 = ed * (c1 + c3*ed + c5*dz*ee)  where c1=-3/14, c5=-9/52 matches -C1 + ... -C5*...
         *   term2 = dz * (c2*ea + dz*(c4*ee + c6*dz*eb)) with c4=-3/22, c6=+3/26
         *   actually NR: dz*(C2*ea + dz*(-C4*ee + C6*dz*eb))
         *   = dz*(c2*ea + dz*(c4*ee + c6*eb*dz)) with c4=-C4, c6=C6
         */
        st = sn_mul(ctx, &t, &c3, &ed, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &t, &c1, &t, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &u, &dz, &ee, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &u, &c5, &u, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &t, &t, &u, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &s, &ed, &t, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &s, &one, &s, opt); if (st != SN_OK) goto rd_series_done;

        st = sn_mul(ctx, &t, &c2, &ea, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &u, &c4, &ee, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &v, &c6, &eb, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &v, &v, &dz, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &u, &u, &v, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &u, &u, &dz, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &t, &t, &u, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &t, &t, &dz, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, &s, &s, &t, opt); if (st != SN_OK) goto rd_series_done;

        /* fac / A^{3/2} * s + 3*sum */
        st = sn_sqrt(ctx, &t, &an, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &u, &an, &t, opt); if (st != SN_OK) goto rd_series_done; /* A^{3/2} */
        st = sn_div(ctx, &t, &fac, &u, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &t, &t, &s, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_mul(ctx, &u, &three, &sum, opt); if (st != SN_OK) goto rd_series_done;
        st = sn_add(ctx, out, &u, &t, opt);
rd_series_done:
        sn_value_clear(ctx, &c1); sn_value_clear(ctx, &c2); sn_value_clear(ctx, &c3);
        sn_value_clear(ctx, &c4); sn_value_clear(ctx, &c5); sn_value_clear(ctx, &c6);
        sn_value_clear(ctx, &six); sn_value_clear(ctx, &ee2);
        if (st != SN_OK) goto done;
    }

done:
    sn_value_clear(ctx, &xn); sn_value_clear(ctx, &yn); sn_value_clear(ctx, &zn);
    sn_value_clear(ctx, &an); sn_value_clear(ctx, &sx); sn_value_clear(ctx, &sy);
    sn_value_clear(ctx, &sz); sn_value_clear(ctx, &lam); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &u); sn_value_clear(ctx, &v); sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &two); sn_value_clear(ctx, &three); sn_value_clear(ctx, &four);
    sn_value_clear(ctx, &dx); sn_value_clear(ctx, &dy); sn_value_clear(ctx, &dz);
    sn_value_clear(ctx, &ea); sn_value_clear(ctx, &eb); sn_value_clear(ctx, &ec);
    sn_value_clear(ctx, &ed); sn_value_clear(ctx, &ee); sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &sum); sn_value_clear(ctx, &fac); sn_value_clear(ctx, &c);
    return st;
}

sn_status sn_soft_ellipeinc(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m,
                            const sn_op_opt *opt)
{
    sn_status st;
    int clsp, clsm, neg;
    double dm, dphi, halfpi;
    sn_value s, c, s2, c2, s3, t, one, three, x, y, z, rf, rd, aphi, pi, two;

    if (!phi || !m || phi->kind != SN_KIND_FLOAT || m->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (phi->e_bits != m->e_bits || phi->m_bits != m->m_bits || phi->nan_enabled != m->nan_enabled)
        return SN_ERR_TYPE;

    clsp = sn_fp_classify(phi);
    clsm = sn_fp_classify(m);
    if (clsp == SN_FP_NAN || clsm == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return sn_value_copy(ctx, out, clsp == SN_FP_NAN ? phi : m);
    }
    if (sn_fp_signbit(m) && clsm != SN_FP_ZERO) return soft_ellip_domain(ctx, out, m);
    if (clsm == SN_FP_INFINITE) return soft_ellip_domain(ctx, out, m);

    sn_value_init(&s); sn_value_init(&c); sn_value_init(&s2); sn_value_init(&c2);
    sn_value_init(&s3); sn_value_init(&t); sn_value_init(&one); sn_value_init(&three);
    sn_value_init(&x); sn_value_init(&y); sn_value_init(&z); sn_value_init(&rf);
    sn_value_init(&rd); sn_value_init(&aphi); sn_value_init(&pi); sn_value_init(&two);

    st = sn_to_double(ctx, m, &dm); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, phi, &dphi); if (st != SN_OK) goto done;
    if (dm > 1.0) { st = soft_ellip_domain(ctx, out, m); goto done; }

    neg = sn_fp_signbit(phi) && clsp != SN_FP_ZERO;
    st = sn_abs(ctx, &aphi, phi, opt); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &aphi, &dphi); if (st != SN_OK) goto done;

    st = soft_const_pi(ctx, &pi, m, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, m, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &pi, &two, opt); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &t, &halfpi); if (st != SN_OK) goto done;

    if (fabs(dphi - halfpi) < 1e-14 || dphi > halfpi) {
        if (dphi > halfpi + 1e-12) {
            st = soft_ellip_domain(ctx, out, phi);
            goto done;
        }
        /* E(?/2|m) = complete E(m) */
        st = sn_soft_ellipe(ctx, out, m, opt);
        if (st == SN_OK && neg) st = sn_neg(ctx, out, out, opt);
        goto done;
    }
    if (clsp == SN_FP_ZERO || dphi == 0.0) {
        st = soft_from_d(ctx, out, 0.0, m, opt);
        goto done;
    }
    /* m=0: E(?|0)=? */
    if (dm == 0.0 || clsm == SN_FP_ZERO) {
        st = sn_value_copy(ctx, out, phi);
        goto done;
    }
    /* m=1: E(?|1)=sin ? */
    if (dm >= 1.0) {
        st = sn_soft_sin(ctx, out, phi, opt);
        goto done;
    }

    st = soft_from_d(ctx, &one, 1.0, m, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &three, 3.0, m, opt); if (st != SN_OK) goto done;
    st = sn_soft_sin(ctx, &s, &aphi, opt); if (st != SN_OK) goto done;
    st = sn_soft_cos(ctx, &c, &aphi, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &s2, &s, &s, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &c2, &c, &c, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &s3, &s2, &s, opt); if (st != SN_OK) goto done;

    /* x=c?, y=1-m s?, z=1 */
    st = sn_value_copy(ctx, &x, &c2); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, m, &s2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &y, &one, &t, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &z, &one); if (st != SN_OK) goto done;

    st = soft_carlson_rf(ctx, &rf, &x, &y, &z, opt); if (st != SN_OK) goto done;
    st = soft_carlson_rd(ctx, &rd, &x, &y, &z, opt); if (st != SN_OK) goto done;

    /* E = s*RF - (m/3)*s?*RD */
    st = sn_mul(ctx, &t, &s, &rf, opt); if (st != SN_OK) goto done;
    {
        sn_value tmp, scale;
        sn_value_init(&tmp); sn_value_init(&scale);
        st = sn_div(ctx, &scale, m, &three, opt);
        if (st == SN_OK) st = sn_mul(ctx, &tmp, &scale, &s3, opt);
        if (st == SN_OK) st = sn_mul(ctx, &tmp, &tmp, &rd, opt);
        if (st == SN_OK) st = sn_sub(ctx, out, &t, &tmp, opt);
        sn_value_clear(ctx, &tmp); sn_value_clear(ctx, &scale);
        if (st != SN_OK) goto done;
    }
    if (neg) st = sn_neg(ctx, out, out, opt);
done:
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c); sn_value_clear(ctx, &s2);
    sn_value_clear(ctx, &c2); sn_value_clear(ctx, &s3); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &three); sn_value_clear(ctx, &x);
    sn_value_clear(ctx, &y); sn_value_clear(ctx, &z); sn_value_clear(ctx, &rf);
    sn_value_clear(ctx, &rd); sn_value_clear(ctx, &aphi); sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &two);
    return st;
}

/* -------------------------------------------------------------------------- */
/* Incomplete elliptic integral of the third kind Pi(n;phi|m) via Carlson RJ. */
/* Pi = s*RF(c^2,1-m s^2,1) + (n/3)*s^3*RJ(c^2,1-m s^2,1,1-n s^2), s=sin phi */
/* Domain: m in [0,1], |phi|<=pi/2, 1-n sin^2(phi) > 0 (no real pole). Odd in phi. */
/* Special: n=0 -> F(phi|m); phi=0 -> 0; |phi|=pi/2 -> complete Pi(n|m). Soft. */
/* -------------------------------------------------------------------------- */

/* Carlson RC(x,y) = (1/2) int (t+x)^{-1/2}(t+y)^{-1} dt
 * Duplication + series (Carlson / Numerical Recipes). y > 0 for real path. */
static sn_status soft_carlson_rc(sn_ctx *ctx, sn_value *out,
                                 const sn_value *x, const sn_value *y,
                                 const sn_op_opt *opt)
{
    sn_status st;
    sn_value xn, yn, sn, t, u, one, three, four, ave, s, c1, c2, c3, c4;
    sn_value sx, sy, alamb;
    int i, max_iter;
    double dsn, thr;

    sn_value_init(&xn); sn_value_init(&yn); sn_value_init(&sn);
    sn_value_init(&t); sn_value_init(&u); sn_value_init(&one);
    sn_value_init(&three); sn_value_init(&four); sn_value_init(&ave);
    sn_value_init(&s); sn_value_init(&c1); sn_value_init(&c2);
    sn_value_init(&c3); sn_value_init(&c4);
    sn_value_init(&sx); sn_value_init(&sy); sn_value_init(&alamb);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &three, 3.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &xn, x); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &yn, y); if (st != SN_OK) goto done;

    thr = 0.0012;
    if (x->m_bits > 64) thr = 1e-6;
    max_iter = x->m_bits + 20;
    if (max_iter < 40) max_iter = 40;
    if (max_iter > 200) max_iter = 200;

    for (i = 0; i < max_iter; i++) {
        st = sn_sqrt(ctx, &sx, &xn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &sy, &yn, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &sx, &sy, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &t, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &alamb, &t, &yn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &xn, &alamb, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &xn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &yn, &alamb, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &yn, &t, &four, opt); if (st != SN_OK) goto done;

        st = sn_add(ctx, &t, &yn, &yn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &xn, &t, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &ave, &t, &three, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &sn, &yn, &ave, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &sn, &sn, &ave, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &sn, &dsn); if (st != SN_OK) goto done;
        if (fabs(dsn) < thr) break;
    }

    st = soft_from_d(ctx, &c1, 0.3, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c2, 1.0/7.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c3, 0.375, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c4, 9.0/22.0, x, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, &t, &c4, &sn, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &c3, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &sn, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &c2, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &sn, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &c1, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &sn, &sn, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &u, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &one, &t, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &t, &ave, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, out, &s, &t, opt);

done:
    sn_value_clear(ctx, &xn); sn_value_clear(ctx, &yn); sn_value_clear(ctx, &sn);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &u); sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &three); sn_value_clear(ctx, &four); sn_value_clear(ctx, &ave);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c1); sn_value_clear(ctx, &c2);
    sn_value_clear(ctx, &c3); sn_value_clear(ctx, &c4);
    sn_value_clear(ctx, &sx); sn_value_clear(ctx, &sy); sn_value_clear(ctx, &alamb);
    return st;
}

/* Carlson RJ(x,y,z,p) duplication + series (Carlson / Numerical Recipes). No __int128. */
static sn_status soft_carlson_rj(sn_ctx *ctx, sn_value *out,
                                 const sn_value *x, const sn_value *y,
                                 const sn_value *z, const sn_value *p,
                                 const sn_op_opt *opt)
{
    sn_status st;
    sn_value xn, yn, zn, pn, an, sx, sy, sz, lam, t, u, v, one, two, three, four, five;
    sn_value dx, dy, dz, dp, ea, eb, ec, ed, ee, s, sum, fac, alpha, beta, rc;
    sn_value c1, c2, c3, c4, c5, c6, c7, c8;
    int i, max_iter;
    double ddx, ddy, ddz, ddp, thr;

    sn_value_init(&xn); sn_value_init(&yn); sn_value_init(&zn); sn_value_init(&pn);
    sn_value_init(&an); sn_value_init(&sx); sn_value_init(&sy); sn_value_init(&sz);
    sn_value_init(&lam); sn_value_init(&t); sn_value_init(&u); sn_value_init(&v);
    sn_value_init(&one); sn_value_init(&two); sn_value_init(&three);
    sn_value_init(&four); sn_value_init(&five); sn_value_init(&dx); sn_value_init(&dy);
    sn_value_init(&dz); sn_value_init(&dp); sn_value_init(&ea); sn_value_init(&eb);
    sn_value_init(&ec); sn_value_init(&ed); sn_value_init(&ee); sn_value_init(&s);
    sn_value_init(&sum); sn_value_init(&fac); sn_value_init(&alpha); sn_value_init(&beta);
    sn_value_init(&rc); sn_value_init(&c1); sn_value_init(&c2); sn_value_init(&c3);
    sn_value_init(&c4); sn_value_init(&c5); sn_value_init(&c6); sn_value_init(&c7);
    sn_value_init(&c8);

    st = soft_from_d(ctx, &one, 1.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &three, 3.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &four, 4.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &five, 5.0, x, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &xn, x); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &yn, y); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &zn, z); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &pn, p); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &sum, 0.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &fac, 1.0, x, opt); if (st != SN_OK) goto done;

    thr = 0.0015;
    if (x->m_bits > 64) thr = 1e-6;
    max_iter = x->m_bits + 20;
    if (max_iter < 40) max_iter = 40;
    if (max_iter > 200) max_iter = 200;

    for (i = 0; i < max_iter; i++) {
        st = sn_sqrt(ctx, &sx, &xn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &sy, &yn, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &sz, &zn, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &lam, &sx, &sy, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &sy, &sz, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &lam, &lam, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &sz, &sx, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &lam, &lam, &t, opt); if (st != SN_OK) goto done;

        st = sn_add(ctx, &t, &sx, &sy, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &t, &sz, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &u, &pn, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &v, &sx, &sy, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &v, &v, &sz, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &u, &v, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &alpha, &t, &t, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &pn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &u, &t, &t, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &beta, &pn, &u, opt); if (st != SN_OK) goto done;
        st = soft_carlson_rc(ctx, &rc, &alpha, &beta, opt); if (st != SN_OK) goto done;
        st = sn_mul(ctx, &t, &fac, &rc, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &sum, &sum, &t, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &fac, &fac, &four, opt); if (st != SN_OK) goto done;

        st = sn_add(ctx, &t, &xn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &xn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &yn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &yn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &zn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &zn, &t, &four, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &pn, &lam, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &pn, &t, &four, opt); if (st != SN_OK) goto done;

        st = sn_add(ctx, &t, &xn, &yn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &t, &zn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &u, &pn, &pn, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &t, &t, &u, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &an, &t, &five, opt); if (st != SN_OK) goto done;

        st = sn_sub(ctx, &dx, &an, &xn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dy, &an, &yn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dz, &an, &zn, opt); if (st != SN_OK) goto done;
        st = sn_sub(ctx, &dp, &an, &pn, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dx, &dx, &an, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dy, &dy, &an, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dz, &dz, &an, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &dp, &dp, &an, opt); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dx, &ddx); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dy, &ddy); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dz, &ddz); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &dp, &ddp); if (st != SN_OK) goto done;
        if (fabs(ddx) < thr && fabs(ddy) < thr && fabs(ddz) < thr && fabs(ddp) < thr) break;
    }

    st = sn_add(ctx, &t, &dy, &dz, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &ea, &dx, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &dy, &dz, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &ea, &ea, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &eb, &dx, &dy, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &eb, &eb, &dz, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &ec, &dp, &dp, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &three, &ec, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &ed, &ea, &t, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &ea, &ec, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &two, &dp, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &u, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &ee, &eb, &u, opt); if (st != SN_OK) goto done;

    st = soft_from_d(ctx, &c1, 3.0/14.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c2, 1.0/3.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c3, 3.0/22.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &c4, 3.0/26.0, x, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.75, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &c5, &t, &c3, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &t, &c1, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c5, &c5, &u, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.25, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &t, &c2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c5, &c5, &u, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.375, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &c6, &t, &c2, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.25, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &t, &c4, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c6, &c6, &u, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.1875, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &t, &c3, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c6, &c6, &u, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &c7, &t, &c2, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &t, &c3, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c7, &c7, &u, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.25, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &t, &c4, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c7, &c7, &u, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c8, &c3, &c1, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &t, 0.5, x, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &t, &c2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &c8, &c8, &u, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, &t, &c5, &ed, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &t, &c1, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &c6, &ee, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &t, &u, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &s, &ed, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &one, &s, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, &t, &c4, &dp, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &t, &c8, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &dp, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &t, &c7, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &eb, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, &t, &c3, &dp, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &t, &c2, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &ea, &t, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &dp, &t, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, &t, &c2, &dp, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &ec, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &s, &s, &t, opt); if (st != SN_OK) goto done;

    st = sn_sqrt(ctx, &t, &an, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &an, &t, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &fac, &u, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, &t, &s, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &u, &three, &sum, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, out, &u, &t, opt);

done:
    sn_value_clear(ctx, &xn); sn_value_clear(ctx, &yn); sn_value_clear(ctx, &zn);
    sn_value_clear(ctx, &pn); sn_value_clear(ctx, &an); sn_value_clear(ctx, &sx);
    sn_value_clear(ctx, &sy); sn_value_clear(ctx, &sz); sn_value_clear(ctx, &lam);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &u); sn_value_clear(ctx, &v);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &three); sn_value_clear(ctx, &four); sn_value_clear(ctx, &five);
    sn_value_clear(ctx, &dx); sn_value_clear(ctx, &dy); sn_value_clear(ctx, &dz);
    sn_value_clear(ctx, &dp); sn_value_clear(ctx, &ea); sn_value_clear(ctx, &eb);
    sn_value_clear(ctx, &ec); sn_value_clear(ctx, &ed); sn_value_clear(ctx, &ee);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &fac);
    sn_value_clear(ctx, &alpha); sn_value_clear(ctx, &beta); sn_value_clear(ctx, &rc);
    sn_value_clear(ctx, &c1); sn_value_clear(ctx, &c2); sn_value_clear(ctx, &c3);
    sn_value_clear(ctx, &c4); sn_value_clear(ctx, &c5); sn_value_clear(ctx, &c6);
    sn_value_clear(ctx, &c7); sn_value_clear(ctx, &c8);
    return st;
}

sn_status sn_soft_ellipiinc(sn_ctx *ctx, sn_value *out,
                            const sn_value *phi, const sn_value *n, const sn_value *m,
                            const sn_op_opt *opt)
{
    sn_status st;
    int clsp, clsn, clsm, neg;
    double dm, dn, dphi, halfpi, ds2, dpole;
    sn_value s, c, s2, c2, s3, t, one, three, x, y, z, p, rf, rj, aphi, pi, two;

    if (!phi || !n || !m) return SN_ERR_TYPE;
    if (phi->kind != SN_KIND_FLOAT || n->kind != SN_KIND_FLOAT || m->kind != SN_KIND_FLOAT)
        return SN_ERR_TYPE;
    if (phi->e_bits != m->e_bits || phi->m_bits != m->m_bits || phi->nan_enabled != m->nan_enabled)
        return SN_ERR_TYPE;
    if (n->e_bits != m->e_bits || n->m_bits != m->m_bits || n->nan_enabled != m->nan_enabled)
        return SN_ERR_TYPE;

    clsp = sn_fp_classify(phi);
    clsn = sn_fp_classify(n);
    clsm = sn_fp_classify(m);
    if (clsp == SN_FP_NAN || clsn == SN_FP_NAN || clsm == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (clsp == SN_FP_NAN) return sn_value_copy(ctx, out, phi);
        if (clsn == SN_FP_NAN) return sn_value_copy(ctx, out, n);
        return sn_value_copy(ctx, out, m);
    }
    if (sn_fp_signbit(m) && clsm != SN_FP_ZERO) return soft_ellip_domain(ctx, out, m);
    if (clsm == SN_FP_INFINITE || clsn == SN_FP_INFINITE) return soft_ellip_domain(ctx, out, m);

    sn_value_init(&s); sn_value_init(&c); sn_value_init(&s2); sn_value_init(&c2);
    sn_value_init(&s3); sn_value_init(&t); sn_value_init(&one); sn_value_init(&three);
    sn_value_init(&x); sn_value_init(&y); sn_value_init(&z); sn_value_init(&p);
    sn_value_init(&rf); sn_value_init(&rj); sn_value_init(&aphi); sn_value_init(&pi);
    sn_value_init(&two);

    st = sn_to_double(ctx, m, &dm); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, n, &dn); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, phi, &dphi); if (st != SN_OK) goto done;
    if (dm > 1.0) { st = soft_ellip_domain(ctx, out, m); goto done; }

    if (dn == 0.0 || clsn == SN_FP_ZERO) {
        st = sn_soft_ellipf(ctx, out, phi, m, opt);
        goto done;
    }

    neg = sn_fp_signbit(phi) && clsp != SN_FP_ZERO;
    st = sn_abs(ctx, &aphi, phi, opt); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &aphi, &dphi); if (st != SN_OK) goto done;

    st = soft_const_pi(ctx, &pi, m, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &two, 2.0, m, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &pi, &two, opt); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &t, &halfpi); if (st != SN_OK) goto done;

    if (dphi > halfpi + 1e-12) {
        st = soft_ellip_domain(ctx, out, phi);
        goto done;
    }
    if (clsp == SN_FP_ZERO || dphi == 0.0) {
        st = soft_from_d(ctx, out, 0.0, m, opt);
        goto done;
    }

    {
        double sphi = sin(dphi);
        ds2 = sphi * sphi;
        if (fabs(dphi - halfpi) < 1e-14 || dphi >= halfpi) ds2 = 1.0;
        dpole = 1.0 - dn * ds2;
        if (dpole <= 1e-15) {
            st = soft_ellip_domain(ctx, out, n);
            goto done;
        }
    }

    st = soft_from_d(ctx, &one, 1.0, m, opt); if (st != SN_OK) goto done;
    st = soft_from_d(ctx, &three, 3.0, m, opt); if (st != SN_OK) goto done;
    st = sn_soft_sin(ctx, &s, &aphi, opt); if (st != SN_OK) goto done;
    st = sn_soft_cos(ctx, &c, &aphi, opt); if (st != SN_OK) goto done;
    if (fabs(dphi - halfpi) < 1e-14 || dphi >= halfpi) {
        st = soft_from_d(ctx, &s, 1.0, m, opt); if (st != SN_OK) goto done;
        st = soft_from_d(ctx, &c, 0.0, m, opt); if (st != SN_OK) goto done;
    }
    st = sn_mul(ctx, &s2, &s, &s, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &c2, &c, &c, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &s3, &s2, &s, opt); if (st != SN_OK) goto done;

    st = sn_value_copy(ctx, &x, &c2); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, m, &s2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &y, &one, &t, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &z, &one); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t, n, &s2, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &p, &one, &t, opt); if (st != SN_OK) goto done;

    {
        double dy, dpp;
        st = sn_to_double(ctx, &y, &dy); if (st != SN_OK) goto done;
        st = sn_to_double(ctx, &p, &dpp); if (st != SN_OK) goto done;
        if (dy < 0.0 || dpp <= 0.0) {
            st = soft_ellip_domain(ctx, out, m);
            goto done;
        }
    }

    st = soft_carlson_rf(ctx, &rf, &x, &y, &z, opt); if (st != SN_OK) goto done;
    st = soft_carlson_rj(ctx, &rj, &x, &y, &z, &p, opt); if (st != SN_OK) goto done;

    st = sn_mul(ctx, &t, &s, &rf, opt); if (st != SN_OK) goto done;
    {
        sn_value tmp, scale;
        sn_value_init(&tmp); sn_value_init(&scale);
        st = sn_div(ctx, &scale, n, &three, opt);
        if (st == SN_OK) st = sn_mul(ctx, &tmp, &scale, &s3, opt);
        if (st == SN_OK) st = sn_mul(ctx, &tmp, &tmp, &rj, opt);
        if (st == SN_OK) st = sn_add(ctx, out, &t, &tmp, opt);
        sn_value_clear(ctx, &tmp); sn_value_clear(ctx, &scale);
        if (st != SN_OK) goto done;
    }
    if (neg) st = sn_neg(ctx, out, out, opt);
done:
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c); sn_value_clear(ctx, &s2);
    sn_value_clear(ctx, &c2); sn_value_clear(ctx, &s3); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &three); sn_value_clear(ctx, &x);
    sn_value_clear(ctx, &y); sn_value_clear(ctx, &z); sn_value_clear(ctx, &p);
    sn_value_clear(ctx, &rf); sn_value_clear(ctx, &rj); sn_value_clear(ctx, &aphi);
    sn_value_clear(ctx, &pi); sn_value_clear(ctx, &two);
    return st;
}
