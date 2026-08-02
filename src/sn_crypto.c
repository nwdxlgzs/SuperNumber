#include "internal/sn_impl.h"
#include <string.h>

/* Phase 3: gcd, modular inverse, mulmod, powmod for INT/BIGINT.
 * Results are BIGINT (or INT if both inputs INT and result fits path via pack when possible).
 * Convention: gcd result is non-negative. modinv/mulmod/powmod results in [0, m).
 *
 * Constant-time notes (sn_powmod_ct + mont final reduce):
 *  - Final Montgomery conditional subtraction is branch-free on limb data.
 *  - sn_powmod_ct always squares and always multiplies; selects with masks.
 *  - Intermediate Mont values stay padded (no value-dependent normalize in CT loop).
 *  - Exponent limb width is public (padded to >= modulus nlimbs); pad more if secret.
 *  - Heap, cache, and non-CT helpers (setup/from use div) remain out of scope.
 */

static sn_status as_bigint_mag(sn_ctx *ctx, sn_value *dst, const sn_value *src)
{
    sn_status st;
    if (!src || !sn_value_is_num(src)) return SN_ERR_TYPE;
    st = sn_value_copy(ctx, dst, src);
    if (st != SN_OK) return st;
    if (dst->kind == SN_KIND_INT) {
        /* abs + convert to bigint magnitude */
        if (dst->is_signed) {
            /* two's complement neg if negative */
            const sn_limb *L = SN_CLIMBS(dst);
            int top = (dst->width - 1) / SN_LIMB_BITS;
            int rem = (dst->width - 1) % SN_LIMB_BITS;
            int neg = (top < dst->nlimbs && (L[top] & ((sn_limb)1 << rem))) != 0;
            if (neg) {
                sn_limb *M = SN_LIMBS(dst);
                uint64_t carry = 1;
                int i;
                for (i = 0; i < dst->nlimbs; i++) {
                    uint64_t x = ((uint64_t)(~M[i]) & SN_LIMB_MASK) + carry;
                    M[i] = (sn_limb)(x & SN_LIMB_MASK);
                    carry = x >> SN_LIMB_BITS;
                }
            }
        }
        dst->kind = SN_KIND_BIGINT;
        dst->width = 0;
        dst->is_signed = 0;
        dst->negative = 0;
        sn_bigint_normalize(dst);
    } else {
        dst->negative = 0;
        sn_bigint_normalize(dst);
    }
    return SN_OK;
}

static int mag_is_zero(const sn_value *v)
{
    return v->nlimbs == 1 && SN_CLIMBS(v)[0] == 0;
}

static int mag_is_one(const sn_value *v)
{
    return v->nlimbs == 1 && SN_CLIMBS(v)[0] == 1;
}

static sn_status mag_mod(sn_ctx *ctx, sn_value *r, const sn_value *a, const sn_value *m)
{
    sn_value q, rr;
    sn_status st;
    sn_value_init(&q);
    sn_value_init(&rr);
    if (mag_is_zero(m)) return SN_ERR_DIVZERO;
    st = sn_limb_divmod(ctx, &q, &rr, SN_CLIMBS(a), a->nlimbs, SN_CLIMBS(m), m->nlimbs);
    if (st != SN_OK) {
        sn_value_clear(ctx, &q);
        sn_value_clear(ctx, &rr);
        return st;
    }
    sn_value_clear(ctx, &q);
    sn_value_clear(ctx, r);
    sn_value_move(r, &rr);
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    return SN_OK;
}

static sn_status mag_add(sn_ctx *ctx, sn_value *r, const sn_value *a, const sn_value *b)
{
    sn_status st = sn_limb_add(ctx, r, SN_CLIMBS(a), a->nlimbs, SN_CLIMBS(b), b->nlimbs);
    if (st != SN_OK) return st;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    return SN_OK;
}

static sn_status mag_sub(sn_ctx *ctx, sn_value *r, const sn_value *a, const sn_value *b)
{
    /* assume a >= b */
    sn_status st = sn_limb_sub(ctx, r, SN_CLIMBS(a), a->nlimbs, SN_CLIMBS(b), b->nlimbs);
    if (st != SN_OK) return st;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    return SN_OK;
}

static sn_status mag_mul(sn_ctx *ctx, sn_value *r, const sn_value *a, const sn_value *b)
{
    sn_status st = sn_limb_mul(ctx, r, SN_CLIMBS(a), a->nlimbs, SN_CLIMBS(b), b->nlimbs);
    if (st != SN_OK) return st;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    return SN_OK;
}

static int mag_cmp(const sn_value *a, const sn_value *b)
{
    return sn_limb_cmp(SN_CLIMBS(a), a->nlimbs, SN_CLIMBS(b), b->nlimbs);
}

sn_status sn_gcd(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b)
{
    sn_value A, B, T;
    sn_status st;
    if (!ctx || !out || !a || !b) return SN_ERR_ARG;
    if (!sn_value_is_num(a) || !sn_value_is_num(b)) return SN_ERR_TYPE;

    sn_value_init(&A);
    sn_value_init(&B);
    sn_value_init(&T);
    st = as_bigint_mag(ctx, &A, a); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &B, b); if (st != SN_OK) goto done;

    while (!mag_is_zero(&B)) {
        st = mag_mod(ctx, &T, &A, &B);
        if (st != SN_OK) goto done;
        sn_value_clear(ctx, &A);
        sn_value_move(&A, &B);
        sn_value_init(&B);
        sn_value_move(&B, &T);
        sn_value_init(&T);
    }
    sn_value_clear(ctx, out);
    sn_value_move(out, &A);
    sn_value_init(&A);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &A);
    sn_value_clear(ctx, &B);
    sn_value_clear(ctx, &T);
    return st;
}

/* Extended Euclidean: find x such that a*x ≡ 1 (mod m), m > 1, gcd(a,m)=1.
 * Returns SN_ERR_DOMAIN if not invertible.
 */
sn_status sn_modinv(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *m)
{
    sn_value A, M, r0, r1, s0, s1, q, t, prod, tmp;
    sn_status st;
    int s0_neg = 0, s1_neg = 0, t_neg;

    if (!ctx || !out || !a || !m) return SN_ERR_ARG;
    if (!sn_value_is_num(a) || !sn_value_is_num(m)) return SN_ERR_TYPE;

    sn_value_init(&A); sn_value_init(&M);
    sn_value_init(&r0); sn_value_init(&r1);
    sn_value_init(&s0); sn_value_init(&s1);
    sn_value_init(&q); sn_value_init(&t);
    sn_value_init(&prod); sn_value_init(&tmp);

    st = as_bigint_mag(ctx, &A, a); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &M, m); if (st != SN_OK) goto done;
    if (mag_is_zero(&M) || mag_is_one(&M)) { st = SN_ERR_DOMAIN; goto done; }

    /* r0 = m, r1 = a mod m; s0 = 0, s1 = 1 */
    st = sn_value_copy(ctx, &r0, &M); if (st != SN_OK) goto done;
    st = mag_mod(ctx, &r1, &A, &M); if (st != SN_OK) goto done;
    st = sn_bigint_set_u64(ctx, &s0, 0); if (st != SN_OK) goto done;
    st = sn_bigint_set_u64(ctx, &s1, 1); if (st != SN_OK) goto done;
    s0_neg = 0; s1_neg = 0;

    while (!mag_is_zero(&r1)) {
        /* q = r0 / r1 */
        sn_value rem;
        sn_value_init(&rem);
        st = sn_limb_divmod(ctx, &q, &rem, SN_CLIMBS(&r0), r0.nlimbs, SN_CLIMBS(&r1), r1.nlimbs);
        if (st != SN_OK) { sn_value_clear(ctx, &rem); goto done; }
        sn_value_clear(ctx, &rem);
        q.kind = SN_KIND_BIGINT; q.negative = 0; sn_bigint_normalize(&q);

        /* r = r0 - q*r1 */
        st = mag_mul(ctx, &prod, &q, &r1); if (st != SN_OK) goto done;
        st = mag_sub(ctx, &t, &r0, &prod); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &r0);
        sn_value_move(&r0, &r1); sn_value_init(&r1);
        sn_value_move(&r1, &t); sn_value_init(&t);

        /* s = s0 - q*s1  (signed) */
        st = mag_mul(ctx, &prod, &q, &s1); if (st != SN_OK) goto done;
        /* s0_signed - (q*s1) with s1_neg */
        /* result sign/magnitude */
        {
            int p_neg = s1_neg; /* prod = q * s1 magnitude, sign = s1_neg */
            int cmp;
            if (s0_neg == p_neg) {
                /* same sign: |s0| - |prod| or reverse */
                cmp = mag_cmp(&s0, &prod);
                if (cmp >= 0) {
                    st = mag_sub(ctx, &t, &s0, &prod); if (st != SN_OK) goto done;
                    t_neg = s0_neg;
                } else {
                    st = mag_sub(ctx, &t, &prod, &s0); if (st != SN_OK) goto done;
                    t_neg = !s0_neg;
                }
            } else {
                /* different signs: magnitudes add */
                st = mag_add(ctx, &t, &s0, &prod); if (st != SN_OK) goto done;
                t_neg = s0_neg;
            }
            if (mag_is_zero(&t)) t_neg = 0;
        }
        sn_value_clear(ctx, &s0);
        sn_value_move(&s0, &s1); sn_value_init(&s1);
        s0_neg = s1_neg;
        sn_value_move(&s1, &t); sn_value_init(&t);
        s1_neg = t_neg;

        sn_value_clear(ctx, &q);
        sn_value_clear(ctx, &prod);
    }

    if (!mag_is_one(&r0)) { st = SN_ERR_DOMAIN; goto done; }

    /* result is s0 mod m */
    if (s0_neg) {
        /* m - (s0 mod m) */
        st = mag_mod(ctx, &t, &s0, &M); if (st != SN_OK) goto done;
        if (!mag_is_zero(&t)) {
            st = mag_sub(ctx, &tmp, &M, &t); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &t);
            sn_value_move(&t, &tmp); sn_value_init(&tmp);
        }
    } else {
        st = mag_mod(ctx, &t, &s0, &M); if (st != SN_OK) goto done;
    }
    sn_value_clear(ctx, out);
    sn_value_move(out, &t);
    sn_value_init(&t);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &A); sn_value_clear(ctx, &M);
    sn_value_clear(ctx, &r0); sn_value_clear(ctx, &r1);
    sn_value_clear(ctx, &s0); sn_value_clear(ctx, &s1);
    sn_value_clear(ctx, &q); sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &prod); sn_value_clear(ctx, &tmp);
    return st;
}


/* -------------------------------------------------------------------------- */
/* Montgomery multiplication (odd modulus, base 2^SN_LIMB_BITS)                 */
/* Domain: values are x' = x * R mod m with R = 2^(n*SN_LIMB_BITS).             */
/* -------------------------------------------------------------------------- */

static sn_limb mont_inv_limb(sn_limb a)
{
    /* a odd: compute a^{-1} mod 2^32 via Newton */
    sn_limb x;
    int i;
    if ((a & 1u) == 0) return 0;
    x = 1;
    for (i = 0; i < 5; i++)
        x *= (sn_limb)(2u - (sn_limb)(a * x));
    return x;
}

/* ---- Constant-time limb helpers ---- */

static sn_limb sn_ct_eq_mask(sn_limb a, sn_limb b)
{
    uint32_t u = (uint32_t)(a ^ b);
    uint32_t nz = (u | (0u - u)) >> 31; /* 0 if equal else 1 */
    return (sn_limb)(nz - 1u);           /* all-ones if equal */
}

static sn_limb sn_ct_bit_mask(unsigned bit)
{
    return (sn_limb)(0u - (sn_limb)(bit & 1u));
}

static sn_limb sn_ct_select_limb(sn_limb mask, sn_limb a, sn_limb b)
{
    return (sn_limb)((mask & a) | ((~mask) & b));
}

/* all-ones if (a || extra_hi) >= m, else 0. Fixed limb iterations. */
static sn_limb sn_ct_ge_mask(const sn_limb *a, const sn_limb *m, int n, sn_limb extra_hi)
{
    sn_limb gt = 0;
    sn_limb eq = (sn_limb)0xFFFFFFFFu;
    int i;
    /* virtual high limb */
    {
        sn_limb ai = extra_hi;
        sn_limb mi = 0;
        uint32_t ua = (uint32_t)ai, um = (uint32_t)mi;
        uint32_t br = ((~ua & um) | ((~(ua ^ um)) & (ua - um))) >> 31;
        sn_limb lt = (sn_limb)(0u - br);
        sn_limb neq = (sn_limb)~sn_ct_eq_mask(ai, mi);
        sn_limb this_gt = (sn_limb)(neq & ~lt);
        gt = (sn_limb)(gt | (eq & this_gt));
        eq = (sn_limb)(eq & sn_ct_eq_mask(ai, mi));
    }
    for (i = n - 1; i >= 0; i--) {
        sn_limb ai = a[i];
        sn_limb mi = m[i];
        uint32_t ua = (uint32_t)ai, um = (uint32_t)mi;
        uint32_t br = ((~ua & um) | ((~(ua ^ um)) & (ua - um))) >> 31;
        sn_limb lt = (sn_limb)(0u - br);
        sn_limb neq = (sn_limb)~sn_ct_eq_mask(ai, mi);
        sn_limb this_gt = (sn_limb)(neq & ~lt);
        gt = (sn_limb)(gt | (eq & this_gt));
        eq = (sn_limb)(eq & sn_ct_eq_mask(ai, mi));
    }
    return (sn_limb)(gt | eq);
}

/* a -= (m & mask) with portable borrow; mask is 0 or all-ones. */
static void sn_ct_sub_cond(sn_limb *a, const sn_limb *m, int n, sn_limb mask)
{
    uint64_t borrow = 0;
    int j;
    for (j = 0; j < n; j++) {
        uint64_t sub = (uint64_t)(m[j] & mask) + borrow;
        uint64_t aj = (uint64_t)a[j];
        uint64_t diff = aj - sub;
        a[j] = (sn_limb)(diff & SN_LIMB_MASK);
        borrow = (aj < sub) ? 1u : 0u;
    }
    (void)borrow;
}

static void sn_ct_select_n(sn_limb *dst, const sn_limb *src_a, const sn_limb *src_b, int n, sn_limb mask)
{
    int i;
    for (i = 0; i < n; i++)
        dst[i] = sn_ct_select_limb(mask, src_a[i], src_b[i]);
}

static unsigned sn_ct_getbit(const sn_value *v, int i)
{
    int li, bi;
    const sn_limb *L;
    if (!v || i < 0) return 0;
    li = i / SN_LIMB_BITS;
    bi = i % SN_LIMB_BITS;
    if (li >= v->nlimbs) return 0;
    L = SN_CLIMBS(v);
    return (unsigned)((L[li] >> bi) & 1u);
}


static sn_status mag_pad(sn_ctx *ctx, sn_value *v, int n)
{
    sn_status st;
    sn_limb *L;
    int i;
    if (n < 1) n = 1;
    st = sn_value_reserve(ctx, v, n);
    if (st != SN_OK) return st;
    L = SN_LIMBS(v);
    for (i = v->nlimbs; i < n; i++) L[i] = 0;
    v->nlimbs = n;
    v->kind = SN_KIND_BIGINT;
    v->negative = 0;
    return SN_OK;
}

static sn_status mag_shl_bits(sn_ctx *ctx, sn_value *r, const sn_value *a, int bits)
{
    sn_value tmp;
    sn_status st;
    int limb_shift, bit_shift, n, i;
    const sn_limb *src;
    sn_limb *dst;
    if (bits < 0) return SN_ERR_ARG;
    if (bits == 0) return sn_value_copy(ctx, r, a);
    if (mag_is_zero(a)) return sn_bigint_set_u64(ctx, r, 0);
    limb_shift = bits / SN_LIMB_BITS;
    bit_shift = bits % SN_LIMB_BITS;
    n = a->nlimbs + limb_shift + (bit_shift ? 1 : 0);
    sn_value_init(&tmp);
    st = sn_value_reserve(ctx, &tmp, n);
    if (st != SN_OK) return st;
    dst = SN_LIMBS(&tmp);
    for (i = 0; i < n; i++) dst[i] = 0;
    src = SN_CLIMBS(a);
    if (bit_shift == 0) {
        for (i = 0; i < a->nlimbs; i++) dst[i + limb_shift] = src[i];
    } else {
        sn_limb carry = 0;
        for (i = 0; i < a->nlimbs; i++) {
            sn_limb x = src[i];
            dst[i + limb_shift] = (sn_limb)((x << bit_shift) | carry);
            carry = (sn_limb)(x >> (SN_LIMB_BITS - bit_shift));
        }
        dst[a->nlimbs + limb_shift] = carry;
    }
    tmp.nlimbs = n;
    tmp.kind = SN_KIND_BIGINT;
    tmp.negative = 0;
    sn_bigint_normalize(&tmp);
    sn_value_clear(ctx, r);
    sn_value_move(r, &tmp);
    return SN_OK;
}

static sn_status mag_mod_pow2(sn_ctx *ctx, sn_value *r, const sn_value *m, int ebits)
{
    /* r = 2^ebits mod m, m > 1 */
    sn_value one, pow;
    sn_status st;
    sn_value_init(&one);
    sn_value_init(&pow);
    st = sn_bigint_set_u64(ctx, &one, 1);
    if (st != SN_OK) goto done;
    st = mag_shl_bits(ctx, &pow, &one, ebits);
    if (st != SN_OK) goto done;
    st = mag_mod(ctx, r, &pow, m);
done:
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &pow);
    return st;
}

void sn_mont_init(sn_mont *mont)
{
    if (!mont) return;
    memset(mont, 0, sizeof(*mont));
    sn_value_init(&mont->m);
    sn_value_init(&mont->rr);
}

void sn_mont_clear(sn_ctx *ctx, sn_mont *mont)
{
    if (!mont) return;
    sn_value_clear(ctx, &mont->m);
    sn_value_clear(ctx, &mont->rr);
    memset(mont, 0, sizeof(*mont));
    sn_value_init(&mont->m);
    sn_value_init(&mont->rr);
}

sn_status sn_mont_setup(sn_ctx *ctx, sn_mont *mont, const sn_value *modulus)
{
    sn_value M, Rmod, tmp;
    sn_status st;
    int n, ebits;

    if (!ctx || !mont || !modulus) return SN_ERR_ARG;
    if (!sn_value_is_num(modulus)) return SN_ERR_TYPE;

    sn_mont_clear(ctx, mont);
    sn_value_init(&M);
    sn_value_init(&Rmod);
    sn_value_init(&tmp);

    st = as_bigint_mag(ctx, &M, modulus);
    if (st != SN_OK) goto done;
    if (mag_is_zero(&M) || mag_is_one(&M)) {
        st = SN_ERR_RANGE;
        goto done;
    }
    if ((SN_CLIMBS(&M)[0] & 1u) == 0) {
        /* even modulus: Montgomery not applicable */
        st = SN_ERR_DOMAIN;
        goto done;
    }

    sn_bigint_normalize(&M);
    n = M.nlimbs;
    if (n < 1) n = 1;
    st = mag_pad(ctx, &M, n);
    if (st != SN_OK) goto done;

    mont->nlimbs = n;
    mont->n0 = (sn_limb)(0u - mont_inv_limb(SN_CLIMBS(&M)[0]));

    ebits = n * SN_LIMB_BITS;
    /* R mod m = 2^{n*w} mod m */
    st = mag_mod_pow2(ctx, &Rmod, &M, ebits);
    if (st != SN_OK) goto done;
    /* rr = R^2 mod m */
    st = mag_mul(ctx, &tmp, &Rmod, &Rmod);
    if (st != SN_OK) goto done;
    st = mag_mod(ctx, &mont->rr, &tmp, &M);
    if (st != SN_OK) goto done;
    st = mag_pad(ctx, &mont->rr, n);
    if (st != SN_OK) goto done;

    sn_value_clear(ctx, &mont->m);
    sn_value_move(&mont->m, &M);
    sn_value_init(&M);
    mont->ready = 1;
    st = SN_OK;
done:
    sn_value_clear(ctx, &M);
    sn_value_clear(ctx, &Rmod);
    sn_value_clear(ctx, &tmp);
    if (st != SN_OK) sn_mont_clear(ctx, mont);
    return st;
}

/* CIOS Montgomery product: out = a*b*R^{-1} mod m; a,b n limbs. */
static sn_status mont_mul_raw(sn_ctx *ctx, sn_value *out, const sn_mont *mont,
                              const sn_limb *a, const sn_limb *b)
{
    int n = mont->nlimbs;
    const sn_limb *m = SN_CLIMBS(&mont->m);
    sn_limb n0 = mont->n0;
    sn_status st;
    sn_value t;
    sn_limb *T;
    int i, j;
    uint64_t c, uv;

    sn_value_init(&t);
    /* need n+2 limbs: result + carries during CIOS */
    st = sn_value_reserve(ctx, &t, n + 2);
    if (st != SN_OK) return st;
    T = SN_LIMBS(&t);
    for (i = 0; i < n + 2; i++) T[i] = 0;
    t.nlimbs = n + 2;
    t.kind = SN_KIND_BIGINT;
    t.negative = 0;

    for (i = 0; i < n; i++) {
        c = 0;
        for (j = 0; j < n; j++) {
            uv = (uint64_t)T[j] + (uint64_t)a[i] * (uint64_t)b[j] + c;
            T[j] = (sn_limb)(uv & SN_LIMB_MASK);
            c = uv >> SN_LIMB_BITS;
        }
        uv = (uint64_t)T[n] + c;
        T[n] = (sn_limb)(uv & SN_LIMB_MASK);
        T[n + 1] = (sn_limb)(uv >> SN_LIMB_BITS);

        {
            sn_limb u = (sn_limb)((uint64_t)T[0] * (uint64_t)n0);
            c = 0;
            for (j = 0; j < n; j++) {
                uv = (uint64_t)T[j] + (uint64_t)u * (uint64_t)m[j] + c;
                T[j] = (sn_limb)(uv & SN_LIMB_MASK);
                c = uv >> SN_LIMB_BITS;
            }
            uv = (uint64_t)T[n] + c;
            T[n] = (sn_limb)(uv & SN_LIMB_MASK);
            uv = (uint64_t)T[n + 1] + (uv >> SN_LIMB_BITS);
            T[n + 1] = (sn_limb)(uv & SN_LIMB_MASK);
        }

        /* t >>= w (T[0] is 0) */
        for (j = 0; j <= n; j++) T[j] = T[j + 1];
        T[n + 1] = 0;
    }

    /* T[0..n-1] candidate; T[n] is 0 or 1. Constant-time conditional subtract. */
    {
        sn_limb need = sn_ct_ge_mask(T, m, n, T[n]);
        sn_ct_sub_cond(T, m, n, need);
        T[n] = 0;
    }

    t.nlimbs = n;
    sn_bigint_normalize(&t);
    sn_value_clear(ctx, out);
    sn_value_move(out, &t);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    return SN_OK;
}



sn_status sn_mont_mul(sn_ctx *ctx, sn_value *out, const sn_mont *mont,
                      const sn_value *a, const sn_value *b)
{
    sn_value A, B, Ap, Bp, R;
    sn_status st;
    int n;

    if (!ctx || !out || !mont || !a || !b) return SN_ERR_ARG;
    if (!mont->ready) return SN_ERR_ARG;
    n = mont->nlimbs;

    sn_value_init(&A);
    sn_value_init(&B);
    sn_value_init(&Ap);
    sn_value_init(&Bp);
    sn_value_init(&R);

    st = as_bigint_mag(ctx, &A, a); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &B, b); if (st != SN_OK) goto done;
    st = mag_mod(ctx, &Ap, &A, &mont->m); if (st != SN_OK) goto done;
    st = mag_mod(ctx, &Bp, &B, &mont->m); if (st != SN_OK) goto done;
    st = mag_pad(ctx, &Ap, n); if (st != SN_OK) goto done;
    st = mag_pad(ctx, &Bp, n); if (st != SN_OK) goto done;

    st = mont_mul_raw(ctx, &R, mont, SN_CLIMBS(&Ap), SN_CLIMBS(&Bp));
    if (st != SN_OK) goto done;
    sn_value_clear(ctx, out);
    sn_value_move(out, &R);
    sn_value_init(&R);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &A);
    sn_value_clear(ctx, &B);
    sn_value_clear(ctx, &Ap);
    sn_value_clear(ctx, &Bp);
    sn_value_clear(ctx, &R);
    return st;
}

/* Convert normal integer x into Montgomery domain: xR mod m */
sn_status sn_mont_from(sn_ctx *ctx, sn_value *out, const sn_mont *mont, const sn_value *x)
{
    sn_value X, Xp, R;
    sn_status st;
    int n;
    if (!ctx || !out || !mont || !x) return SN_ERR_ARG;
    if (!mont->ready) return SN_ERR_ARG;
    n = mont->nlimbs;
    sn_value_init(&X);
    sn_value_init(&Xp);
    sn_value_init(&R);
    st = as_bigint_mag(ctx, &X, x); if (st != SN_OK) goto done;
    st = mag_mod(ctx, &Xp, &X, &mont->m); if (st != SN_OK) goto done;
    st = mag_pad(ctx, &Xp, n); if (st != SN_OK) goto done;
    st = mont_mul_raw(ctx, &R, mont, SN_CLIMBS(&Xp), SN_CLIMBS(&mont->rr));
    if (st != SN_OK) goto done;
    sn_value_clear(ctx, out);
    sn_value_move(out, &R);
    sn_value_init(&R);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &X);
    sn_value_clear(ctx, &Xp);
    sn_value_clear(ctx, &R);
    return st;
}

/* Convert Montgomery residue xR back to normal: x = REDC(xR) */
sn_status sn_mont_to(sn_ctx *ctx, sn_value *out, const sn_mont *mont, const sn_value *x)
{
    sn_value X, Xp, one, R;
    sn_status st;
    int n;
    if (!ctx || !out || !mont || !x) return SN_ERR_ARG;
    if (!mont->ready) return SN_ERR_ARG;
    n = mont->nlimbs;
    sn_value_init(&X);
    sn_value_init(&Xp);
    sn_value_init(&one);
    sn_value_init(&R);
    st = as_bigint_mag(ctx, &X, x); if (st != SN_OK) goto done;
    st = mag_mod(ctx, &Xp, &X, &mont->m); if (st != SN_OK) goto done;
    st = mag_pad(ctx, &Xp, n); if (st != SN_OK) goto done;
    st = sn_bigint_set_u64(ctx, &one, 1); if (st != SN_OK) goto done;
    st = mag_pad(ctx, &one, n); if (st != SN_OK) goto done;
    st = mont_mul_raw(ctx, &R, mont, SN_CLIMBS(&Xp), SN_CLIMBS(&one));
    if (st != SN_OK) goto done;
    sn_value_clear(ctx, out);
    sn_value_move(out, &R);
    sn_value_init(&R);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &X);
    sn_value_clear(ctx, &Xp);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &R);
    return st;
}

static sn_status powmod_mont(sn_ctx *ctx, sn_value *out, const sn_value *B, const sn_value *E, const sn_mont *mont)
{
    sn_value base_m, acc_m, t, one;
    sn_status st;
    int i, nbits, bit, n;

    n = mont->nlimbs;
    sn_value_init(&base_m);
    sn_value_init(&acc_m);
    sn_value_init(&t);
    sn_value_init(&one);

    st = sn_mont_from(ctx, &base_m, mont, B); if (st != SN_OK) goto done;
    st = sn_bigint_set_u64(ctx, &one, 1); if (st != SN_OK) goto done;
    st = sn_mont_from(ctx, &acc_m, mont, &one); if (st != SN_OK) goto done;
    st = mag_pad(ctx, &base_m, n); if (st != SN_OK) goto done;
    st = mag_pad(ctx, &acc_m, n); if (st != SN_OK) goto done;

    nbits = sn_bitlen(E);
    for (i = nbits - 1; i >= 0; i--) {
        st = mont_mul_raw(ctx, &t, mont, SN_CLIMBS(&acc_m), SN_CLIMBS(&acc_m));
        if (st != SN_OK) goto done;
        sn_value_clear(ctx, &acc_m);
        sn_value_move(&acc_m, &t);
        sn_value_init(&t);
        st = mag_pad(ctx, &acc_m, n); if (st != SN_OK) goto done;

        st = sn_getbit(E, i, &bit); if (st != SN_OK) goto done;
        if (bit) {
            st = mont_mul_raw(ctx, &t, mont, SN_CLIMBS(&acc_m), SN_CLIMBS(&base_m));
            if (st != SN_OK) goto done;
            sn_value_clear(ctx, &acc_m);
            sn_value_move(&acc_m, &t);
            sn_value_init(&t);
            st = mag_pad(ctx, &acc_m, n); if (st != SN_OK) goto done;
        }
    }
    st = sn_mont_to(ctx, out, mont, &acc_m);
done:
    sn_value_clear(ctx, &base_m);
    sn_value_clear(ctx, &acc_m);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one);
    return st;
}

static sn_status powmod_naive(sn_ctx *ctx, sn_value *out, sn_value *B, sn_value *E, sn_value *M)
{
    sn_value R, T;
    sn_status st;
    int i, nbits, bit;

    sn_value_init(&R);
    sn_value_init(&T);
    st = sn_bigint_set_u64(ctx, &R, 1); if (st != SN_OK) goto done;
    nbits = sn_bitlen(E);
    for (i = nbits - 1; i >= 0; i--) {
        st = mag_mul(ctx, &T, &R, &R); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &R);
        st = mag_mod(ctx, &R, &T, M); if (st != SN_OK) goto done;
        sn_value_clear(ctx, &T);

        st = sn_getbit(E, i, &bit); if (st != SN_OK) goto done;
        if (bit) {
            st = mag_mul(ctx, &T, &R, B); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &R);
            st = mag_mod(ctx, &R, &T, M); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &T);
        }
    }
    sn_value_clear(ctx, out);
    sn_value_move(out, &R);
    sn_value_init(&R);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &R);
    sn_value_clear(ctx, &T);
    return st;
}



/* Like mont_mul_raw but keeps exactly n limbs (no normalize) for CT exponentiation. */
static sn_status mont_mul_raw_pad(sn_ctx *ctx, sn_value *out, const sn_mont *mont,
                                  const sn_limb *a, const sn_limb *b)
{
    sn_status st = mont_mul_raw(ctx, out, mont, a, b);
    if (st != SN_OK) return st;
    return mag_pad(ctx, out, mont->nlimbs);
}

static sn_status mont_from_pad(sn_ctx *ctx, sn_value *out, const sn_mont *mont, const sn_value *x)
{
    sn_status st = sn_mont_from(ctx, out, mont, x);
    if (st != SN_OK) return st;
    return mag_pad(ctx, out, mont->nlimbs);
}

static sn_status powmod_mont_ct(sn_ctx *ctx, sn_value *out, const sn_value *B, const sn_value *E, const sn_mont *mont)
{
    sn_value base_m, acc_m, t, one, tmp;
    sn_status st;
    int nbits, i, n;
    sn_limb *accL, *tL;

    n = mont->nlimbs;
    sn_value_init(&base_m);
    sn_value_init(&acc_m);
    sn_value_init(&t);
    sn_value_init(&one);
    sn_value_init(&tmp);

    st = sn_bigint_set_u64(ctx, &one, 1); if (st != SN_OK) goto done;
    st = mont_from_pad(ctx, &base_m, mont, B); if (st != SN_OK) goto done;
    st = mont_from_pad(ctx, &acc_m, mont, &one); if (st != SN_OK) goto done;

    /* Full limb buffer width of E (public length). */
    nbits = E->nlimbs * SN_LIMB_BITS;
    if (nbits < 1) nbits = 1;

    for (i = nbits - 1; i >= 0; i--) {
        unsigned bit = sn_ct_getbit(E, i);
        sn_limb mask = sn_ct_bit_mask(bit);
        int j;

        /* always square */
        st = mont_mul_raw_pad(ctx, &t, mont, SN_CLIMBS(&acc_m), SN_CLIMBS(&acc_m));
        if (st != SN_OK) goto done;
        sn_value_clear(ctx, &acc_m);
        sn_value_move(&acc_m, &t);
        sn_value_init(&t);
        st = mag_pad(ctx, &acc_m, n); if (st != SN_OK) goto done;

        /* always multiply by base, then CT-select into acc */
        st = mont_mul_raw_pad(ctx, &t, mont, SN_CLIMBS(&acc_m), SN_CLIMBS(&base_m));
        if (st != SN_OK) goto done;
        st = mag_pad(ctx, &t, n); if (st != SN_OK) goto done;

        st = sn_value_reserve(ctx, &tmp, n); if (st != SN_OK) goto done;
        accL = SN_LIMBS(&acc_m);
        tL = SN_LIMBS(&t);
        {
            sn_limb *d = SN_LIMBS(&tmp);
            for (j = 0; j < n; j++) d[j] = 0;
            sn_ct_select_n(d, tL, accL, n, mask); /* bit=1 -> mul result, else keep square */
            tmp.nlimbs = n;
            tmp.kind = SN_KIND_BIGINT;
            tmp.negative = 0;
        }
        sn_value_clear(ctx, &acc_m);
        sn_value_move(&acc_m, &tmp);
        sn_value_init(&tmp);
        sn_value_clear(ctx, &t);
        sn_value_init(&t);
    }

    st = sn_mont_to(ctx, out, mont, &acc_m);
done:
    sn_value_clear(ctx, &base_m);
    sn_value_clear(ctx, &acc_m);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &tmp);
    return st;
}

sn_status sn_powmod_ct(sn_ctx *ctx, sn_value *out, const sn_value *base, const sn_value *exp, const sn_value *m)
{
    sn_value B, E, M, T;
    sn_mont mont;
    sn_status st;

    if (!ctx || !out || !base || !exp || !m) return SN_ERR_ARG;
    if (!sn_value_is_num(base) || !sn_value_is_num(exp) || !sn_value_is_num(m)) return SN_ERR_TYPE;

    sn_value_init(&B); sn_value_init(&E); sn_value_init(&M);
    sn_value_init(&T);
    sn_mont_init(&mont);

    st = as_bigint_mag(ctx, &B, base); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &E, exp); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &M, m); if (st != SN_OK) goto done;
    if (mag_is_zero(&M)) { sn_raise(ctx, SN_FLAG_DIVZERO); st = SN_ERR_DIVZERO; goto done; }
    if (mag_is_one(&M)) {
        st = sn_bigint_set_u64(ctx, out, 0);
        goto done;
    }
    if ((SN_CLIMBS(&M)[0] & 1u) == 0) {
        st = SN_ERR_DOMAIN; /* even modulus: no CT Mont path */
        goto done;
    }

    /* Pad exponent to at least modulus limb count (public width floor). */
    {
        int want = M.nlimbs;
        if (want < 1) want = 1;
        if (E.nlimbs < want) {
            st = mag_pad(ctx, &E, want);
            if (st != SN_OK) goto done;
        }
    }

    st = mag_mod(ctx, &T, &B, &M); if (st != SN_OK) goto done;
    sn_value_clear(ctx, &B); sn_value_move(&B, &T); sn_value_init(&T);

    st = sn_mont_setup(ctx, &mont, &M);
    if (st != SN_OK) goto done;
    st = powmod_mont_ct(ctx, out, &B, &E, &mont);
done:
    sn_value_clear(ctx, &B); sn_value_clear(ctx, &E); sn_value_clear(ctx, &M);
    sn_value_clear(ctx, &T);
    sn_mont_clear(ctx, &mont);
    return st;
}


sn_status sn_mulmod(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *m)
{
    sn_value A, B, M, P, R;
    sn_status st;
    int neg = 0;
    if (!ctx || !out || !a || !b || !m) return SN_ERR_ARG;
    if (!sn_value_is_num(a) || !sn_value_is_num(b) || !sn_value_is_num(m)) return SN_ERR_TYPE;

    sn_value_init(&A); sn_value_init(&B); sn_value_init(&M);
    sn_value_init(&P); sn_value_init(&R);
    /* Track product sign before magnitude collapse so result matches (a*b) mod m in [0,m). */
    if (a->kind == SN_KIND_BIGINT && a->negative) neg ^= 1;
    if (b->kind == SN_KIND_BIGINT && b->negative) neg ^= 1;
    if (a->kind == SN_KIND_INT && a->is_signed) {
        const sn_limb *L = SN_CLIMBS(a);
        int top = (a->width - 1) / SN_LIMB_BITS;
        int rem = (a->width - 1) % SN_LIMB_BITS;
        if (top < a->nlimbs && (L[top] & ((sn_limb)1 << rem)) != 0) neg ^= 1;
    }
    if (b->kind == SN_KIND_INT && b->is_signed) {
        const sn_limb *L = SN_CLIMBS(b);
        int top = (b->width - 1) / SN_LIMB_BITS;
        int rem = (b->width - 1) % SN_LIMB_BITS;
        if (top < b->nlimbs && (L[top] & ((sn_limb)1 << rem)) != 0) neg ^= 1;
    }
    st = as_bigint_mag(ctx, &A, a); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &B, b); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &M, m); if (st != SN_OK) goto done;
    if (mag_is_zero(&M)) { sn_raise(ctx, SN_FLAG_DIVZERO); st = SN_ERR_DIVZERO; goto done; }
    st = mag_mul(ctx, &P, &A, &B); if (st != SN_OK) goto done;
    st = mag_mod(ctx, &R, &P, &M); if (st != SN_OK) goto done;
    if (neg && !mag_is_zero(&R)) {
        sn_value T;
        sn_value_init(&T);
        st = mag_sub(ctx, &T, &M, &R);
        if (st != SN_OK) { sn_value_clear(ctx, &T); goto done; }
        sn_value_clear(ctx, &R);
        sn_value_move(&R, &T);
    }
    sn_value_clear(ctx, out);
    sn_value_move(out, &R);
    sn_value_init(&R);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &A); sn_value_clear(ctx, &B); sn_value_clear(ctx, &M);
    sn_value_clear(ctx, &P); sn_value_clear(ctx, &R);
    return st;
}

sn_status sn_powmod(sn_ctx *ctx, sn_value *out, const sn_value *base, const sn_value *exp, const sn_value *m)
{
    sn_value B, E, M, T;
    sn_mont mont;
    sn_status st;

    if (!ctx || !out || !base || !exp || !m) return SN_ERR_ARG;
    if (!sn_value_is_num(base) || !sn_value_is_num(exp) || !sn_value_is_num(m)) return SN_ERR_TYPE;

    sn_value_init(&B); sn_value_init(&E); sn_value_init(&M);
    sn_value_init(&T);
    sn_mont_init(&mont);

    st = as_bigint_mag(ctx, &B, base); if (st != SN_OK) goto done;
    /* exp treated as non-negative magnitude */
    st = as_bigint_mag(ctx, &E, exp); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &M, m); if (st != SN_OK) goto done;
    if (mag_is_zero(&M)) { sn_raise(ctx, SN_FLAG_DIVZERO); st = SN_ERR_DIVZERO; goto done; }
    if (mag_is_one(&M)) {
        st = sn_bigint_set_u64(ctx, out, 0);
        goto done;
    }

    st = mag_mod(ctx, &T, &B, &M); if (st != SN_OK) goto done;
    sn_value_clear(ctx, &B); sn_value_move(&B, &T); sn_value_init(&T);

    /* Odd modulus: Montgomery acceleration; even: naive square-and-multiply */
    if ((SN_CLIMBS(&M)[0] & 1u) != 0) {
        st = sn_mont_setup(ctx, &mont, &M);
        if (st == SN_OK) {
            st = powmod_mont(ctx, out, &B, &E, &mont);
            goto done;
        }
        /* setup failed unexpectedly -> fall through to naive */
        sn_mont_clear(ctx, &mont);
    }
    st = powmod_naive(ctx, out, &B, &E, &M);
done:
    sn_value_clear(ctx, &B); sn_value_clear(ctx, &E); sn_value_clear(ctx, &M);
    sn_value_clear(ctx, &T);
    sn_mont_clear(ctx, &mont);
    return st;
}

/* lcm(|a|,|b|) = |a|/gcd(|a|,|b|) * |b|; non-negative BIGINT. lcm(0,x)=0. */
sn_status sn_lcm(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b)
{
    sn_value A, B, G, Q, T;
    sn_status st;
    if (!ctx || !out || !a || !b) return SN_ERR_ARG;
    if (!sn_value_is_num(a) || !sn_value_is_num(b)) return SN_ERR_TYPE;

    sn_value_init(&A);
    sn_value_init(&B);
    sn_value_init(&G);
    sn_value_init(&Q);
    sn_value_init(&T);

    st = as_bigint_mag(ctx, &A, a); if (st != SN_OK) goto done;
    st = as_bigint_mag(ctx, &B, b); if (st != SN_OK) goto done;
    if (mag_is_zero(&A) || mag_is_zero(&B)) {
        st = sn_bigint_set_u64(ctx, out, 0);
        goto done;
    }
    st = sn_gcd(ctx, &G, &A, &B); if (st != SN_OK) goto done;
    /* Q = A / G */
    {
        sn_value rem;
        sn_value_init(&rem);
        st = sn_limb_divmod(ctx, &Q, &rem, SN_CLIMBS(&A), A.nlimbs, SN_CLIMBS(&G), G.nlimbs);
        sn_value_clear(ctx, &rem);
        if (st != SN_OK) goto done;
        Q.kind = SN_KIND_BIGINT;
        Q.negative = 0;
        sn_bigint_normalize(&Q);
    }
    st = mag_mul(ctx, &T, &Q, &B); if (st != SN_OK) goto done;
    sn_value_clear(ctx, out);
    sn_value_move(out, &T);
    sn_value_init(&T);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    sn_bigint_normalize(out);
    st = SN_OK;
done:
    sn_value_clear(ctx, &A);
    sn_value_clear(ctx, &B);
    sn_value_clear(ctx, &G);
    sn_value_clear(ctx, &Q);
    sn_value_clear(ctx, &T);
    return st;
}

/* floor(sqrt(mag(a))) as BIGINT. */
sn_status sn_isqrt(sn_ctx *ctx, sn_value *out, const sn_value *a)
{
    sn_value X, Lo, Hi, Mid, Sq, One, Two, Tmp, Best;
    sn_status st;
    int cmp;
    if (!ctx || !out || !a) return SN_ERR_ARG;
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;

    sn_value_init(&X);
    sn_value_init(&Lo);
    sn_value_init(&Hi);
    sn_value_init(&Mid);
    sn_value_init(&Sq);
    sn_value_init(&One);
    sn_value_init(&Two);
    sn_value_init(&Tmp);
    sn_value_init(&Best);

    st = as_bigint_mag(ctx, &X, a); if (st != SN_OK) goto done;
    if (mag_is_zero(&X)) {
        st = sn_bigint_set_u64(ctx, out, 0);
        goto done;
    }
    if (mag_is_one(&X)) {
        st = sn_bigint_set_u64(ctx, out, 1);
        goto done;
    }

    st = sn_bigint_set_u64(ctx, &Lo, 0); if (st != SN_OK) goto done;
    st = sn_bigint_set_u64(ctx, &One, 1); if (st != SN_OK) goto done;
    st = sn_bigint_set_u64(ctx, &Two, 2); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &Hi, &X); if (st != SN_OK) goto done;
    st = sn_bigint_set_u64(ctx, &Best, 0); if (st != SN_OK) goto done;

    while (mag_cmp(&Lo, &Hi) <= 0) {
        /* Mid = (Lo + Hi) / 2 */
        st = mag_add(ctx, &Tmp, &Lo, &Hi); if (st != SN_OK) goto done;
        {
            sn_value rem;
            sn_value_init(&rem);
            sn_value_clear(ctx, &Mid);
            sn_value_init(&Mid);
            st = sn_limb_divmod(ctx, &Mid, &rem, SN_CLIMBS(&Tmp), Tmp.nlimbs,
                                SN_CLIMBS(&Two), Two.nlimbs);
            sn_value_clear(ctx, &rem);
            if (st != SN_OK) goto done;
            Mid.kind = SN_KIND_BIGINT;
            Mid.negative = 0;
            sn_bigint_normalize(&Mid);
        }
        sn_value_clear(ctx, &Sq);
        sn_value_init(&Sq);
        st = mag_mul(ctx, &Sq, &Mid, &Mid); if (st != SN_OK) goto done;
        cmp = mag_cmp(&Sq, &X);
        if (cmp == 0) {
            sn_value_clear(ctx, out);
            sn_value_move(out, &Mid);
            sn_value_init(&Mid);
            out->kind = SN_KIND_BIGINT;
            out->negative = 0;
            st = SN_OK;
            goto done;
        }
        if (cmp < 0) {
            /* Best = Mid; Lo = Mid + 1 */
            sn_value_clear(ctx, &Best);
            st = sn_value_copy(ctx, &Best, &Mid); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &Tmp);
            st = mag_add(ctx, &Tmp, &Mid, &One); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &Lo);
            sn_value_move(&Lo, &Tmp);
            sn_value_init(&Tmp);
        } else {
            /* Hi = Mid - 1 */
            sn_value_clear(ctx, &Tmp);
            st = mag_sub(ctx, &Tmp, &Mid, &One); if (st != SN_OK) goto done;
            sn_value_clear(ctx, &Hi);
            sn_value_move(&Hi, &Tmp);
            sn_value_init(&Tmp);
        }
    }
    sn_value_clear(ctx, out);
    sn_value_move(out, &Best);
    sn_value_init(&Best);
    out->kind = SN_KIND_BIGINT;
    out->negative = 0;
    st = SN_OK;
done:
    sn_value_clear(ctx, &X);
    sn_value_clear(ctx, &Lo);
    sn_value_clear(ctx, &Hi);
    sn_value_clear(ctx, &Mid);
    sn_value_clear(ctx, &Sq);
    sn_value_clear(ctx, &One);
    sn_value_clear(ctx, &Two);
    sn_value_clear(ctx, &Tmp);
    sn_value_clear(ctx, &Best);
    return st;
}

/* Portable SWAR helpers for 32-bit limbs (no asm, no compiler builtins required). */
static unsigned sn_limb_popcount32(sn_limb x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (unsigned)((x * 0x01010101u) >> 24);
}

/* x != 0 */
static unsigned sn_limb_ctz32(sn_limb x)
{
    unsigned n = 0;
    if ((x & 0xFFFFu) == 0u) { n += 16u; x >>= 16; }
    if ((x & 0xFFu) == 0u) { n += 8u; x >>= 8; }
    if ((x & 0xFu) == 0u) { n += 4u; x >>= 4; }
    if ((x & 0x3u) == 0u) { n += 2u; x >>= 2; }
    if ((x & 0x1u) == 0u) n += 1u;
    return n;
}

sn_status sn_popcount(sn_ctx *ctx, sn_value *out, const sn_value *a)
{
    sn_value M;
    sn_status st;
    const sn_limb *L;
    int i;
    uint64_t cnt = 0;
    if (!ctx || !out || !a) return SN_ERR_ARG;
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;
    sn_value_init(&M);
    st = as_bigint_mag(ctx, &M, a); if (st != SN_OK) { sn_value_clear(ctx, &M); return st; }
    L = SN_CLIMBS(&M);
    for (i = 0; i < M.nlimbs; i++)
        cnt += (uint64_t)sn_limb_popcount32(L[i]);
    sn_value_clear(ctx, &M);
    return sn_bigint_set_u64(ctx, out, cnt);
}

sn_status sn_ctz(sn_ctx *ctx, sn_value *out, const sn_value *a)
{
    sn_value M;
    sn_status st;
    const sn_limb *L;
    int i;
    uint64_t cnt = 0;
    if (!ctx || !out || !a) return SN_ERR_ARG;
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;
    sn_value_init(&M);
    st = as_bigint_mag(ctx, &M, a); if (st != SN_OK) { sn_value_clear(ctx, &M); return st; }
    if (mag_is_zero(&M)) {
        sn_value_clear(ctx, &M);
        return sn_bigint_set_u64(ctx, out, 0);
    }
    L = SN_CLIMBS(&M);
    for (i = 0; i < M.nlimbs; i++) {
        if (L[i] == 0) {
            cnt += (uint64_t)SN_LIMB_BITS;
            continue;
        }
        cnt += (uint64_t)sn_limb_ctz32(L[i]);
        break;
    }
    sn_value_clear(ctx, &M);
    return sn_bigint_set_u64(ctx, out, cnt);
}

