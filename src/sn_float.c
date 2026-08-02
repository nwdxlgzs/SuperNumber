#include "internal/sn_impl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* IEEE-like binary float: bits = sign(1) | exp(e_bits) | mant(m_bits)
 * Storage is multi-limb.
 *  - m_bits <= 52 and e_bits <= 30: fast uint64 significand path (this file)
 *  - else: multiprec soft path (sn_float_mp.c)
 * E/M are memory-bound (SN_FLOAT_E_MAX / SN_FLOAT_M_MAX); no small hard cap.
 */

#ifndef SN_FLOAT_E_MAX
#define SN_FLOAT_E_MAX (INT_MAX / 4)
#endif
#ifndef SN_FLOAT_M_MAX
#define SN_FLOAT_M_MAX (INT_MAX / 4)
#endif

static int float_total_bits(int e_bits, int m_bits)
{
    return 1 + e_bits + m_bits;
}

/* Bias = 2^(e-1)-1. Safe for e_bits in [2, 63]. */
static int64_t float_bias64(int e_bits)
{
    if (e_bits < 2 || e_bits > 63) return 0;
    return ((int64_t)1 << (e_bits - 1)) - 1;
}

static int64_t float_exp_max64(int e_bits)
{
    if (e_bits < 1) return 0;
    if (e_bits >= 63) return INT64_MAX;
    return ((int64_t)1 << e_bits) - 1;
}

/* legacy int bias only for narrow path e<=30 */
static int float_bias(int e_bits)
{
    return (int)float_bias64(e_bits);
}

static sn_round sn_eff_round(const sn_ctx *ctx, const sn_op_opt *opt)
{
    if (opt && opt->has_round) return opt->round;
    return ctx ? ctx->round : SN_ROUND_NTE;
}

/* Narrow soft path uses uint64 sig => m_bits <= 52. e_bits up to 30. */
static int float_soft_supported(int e_bits, int m_bits)
{
    return e_bits >= 2 && e_bits <= 30 && m_bits >= 1 && m_bits <= 52;
}

static int float_format_ok(int e_bits, int m_bits)
{
    int64_t tb;
    if (e_bits < 2 || e_bits > SN_FLOAT_E_MAX) return 0;
    if (m_bits < 1 || m_bits > SN_FLOAT_M_MAX) return 0;
    tb = (int64_t)1 + (int64_t)e_bits + (int64_t)m_bits;
    if (tb < 3 || tb > (int64_t)INT_MAX) return 0;
    return 1;
}

static sn_status float_ensure(sn_ctx *ctx, sn_value *v, int e_bits, int m_bits, int nan_enabled)
{
    sn_status st;
    int tb, n, i;
    sn_limb *L;
    if (!ctx || !v) return SN_ERR_ARG;
    if (!float_format_ok(e_bits, m_bits)) return SN_ERR_ARG;
    tb = float_total_bits(e_bits, m_bits);
    if (tb < 3) return SN_ERR_ARG;
    n = sn_limbs_for_bits(tb);
    if (n < 1) return SN_ERR_NOMEM;
    sn_value_clear(ctx, v);
    v->kind = SN_KIND_FLOAT;
    v->e_bits = e_bits;
    v->m_bits = m_bits;
    v->nan_enabled = nan_enabled ? 1 : 0;
    v->width = tb;
    v->is_signed = 1;
    v->negative = 0;
    st = sn_value_reserve(ctx, v, n);
    if (st != SN_OK) return st;
    L = SN_LIMBS(v);
    for (i = 0; i < n; i++) L[i] = 0;
    v->nlimbs = n;
    return SN_OK;
}

/* Pack arbitrary-width bit field into little-endian limbs (bit 0 = LSB of mant). */
static void float_pack_bits(sn_value *v, int sign, uint32_t expf, uint64_t mant, int e_bits, int m_bits)
{
    int tb = float_total_bits(e_bits, m_bits);
    int i, n = sn_limbs_for_bits(tb);
    sn_limb *L = SN_LIMBS(v);
    for (i = 0; i < n; i++) L[i] = 0;
    /* write mant bits [0, m_bits) */
    for (i = 0; i < m_bits && i < 64; i++) {
        if ((mant >> i) & 1ull) {
            int li = i / SN_LIMB_BITS, off = i % SN_LIMB_BITS;
            L[li] |= (sn_limb)1u << off;
        }
    }
    /* exp at [m_bits, m_bits+e_bits) */
    for (i = 0; i < e_bits; i++) {
        if ((expf >> i) & 1u) {
            int b = m_bits + i;
            int li = b / SN_LIMB_BITS, off = b % SN_LIMB_BITS;
            L[li] |= (sn_limb)1u << off;
        }
    }
    if (sign) {
        int b = tb - 1;
        int li = b / SN_LIMB_BITS, off = b % SN_LIMB_BITS;
        L[li] |= (sn_limb)1u << off;
    }
    v->nlimbs = n;
}

static void float_unpack_bits(const sn_value *v, int *sign, uint32_t *expf, uint64_t *mant)
{
    int e_bits = v->e_bits, m_bits = v->m_bits;
    int tb = float_total_bits(e_bits, m_bits);
    int i;
    const sn_limb *L = SN_CLIMBS(v);
    uint64_t m = 0;
    uint32_t e = 0;
    *sign = 0;
    for (i = 0; i < m_bits && i < 64; i++) {
        int li = i / SN_LIMB_BITS, off = i % SN_LIMB_BITS;
        if (li < v->nlimbs && ((L[li] >> off) & 1u)) m |= (1ull << i);
    }
    for (i = 0; i < e_bits; i++) {
        int b = m_bits + i;
        int li = b / SN_LIMB_BITS, off = b % SN_LIMB_BITS;
        if (li < v->nlimbs && ((L[li] >> off) & 1u)) e |= (1u << i);
    }
    {
        int b = tb - 1;
        int li = b / SN_LIMB_BITS, off = b % SN_LIMB_BITS;
        if (li < v->nlimbs && ((L[li] >> off) & 1u)) *sign = 1;
    }
    *expf = e;
    *mant = m;
}

static void float_set_bit(sn_value *v, int bit, int on)
{
    sn_limb *L = SN_LIMBS(v);
    int li = bit / SN_LIMB_BITS;
    int off = bit % SN_LIMB_BITS;
    if (li < 0 || li >= v->nlimbs) return;
    if (on) L[li] |= ((sn_limb)1u << off);
    else L[li] &= ~((sn_limb)1u << off);
}

static int float_get_bit(const sn_value *v, int bit)
{
    const sn_limb *L = SN_CLIMBS(v);
    int li = bit / SN_LIMB_BITS;
    int off = bit % SN_LIMB_BITS;
    if (li < 0 || li >= v->nlimbs) return 0;
    return (int)((L[li] >> off) & 1u);
}

static void float_pack_u64(sn_value *v, uint64_t bits)
{
    int tb = v->width > 0 ? v->width : (v->nlimbs * SN_LIMB_BITS);
    if (tb <= 64) {
        sn_limbs_from_u64(SN_LIMBS(v), v->nlimbs, bits);
        return;
    }
    /* Wide: interpret bits as low tb bits of IEEE layout when tb<=64 only;
     * for wide formats callers should use float_pack_bits. Fallback: write low 64. */
    sn_limbs_from_u64(SN_LIMBS(v), v->nlimbs < 2 ? v->nlimbs : 2, bits);
}

static uint64_t float_unpack_u64(const sn_value *v)
{
    return sn_limbs_to_u64(SN_CLIMBS(v), v->nlimbs);
}


typedef struct {
    int sign;
    int exp; /* power of two for integer significand */
    int is_zero;
    int is_inf;
    int is_nan;
    int sticky; /* nonzero if any bit was discarded below sig */
    uint64_t sig;
} sn_funp;

static sn_status float_unpack(const sn_value *v, sn_funp *u)
{
    u->sticky = 0;
    int e_bits, m_bits, bias, tb, sign;
    uint64_t mant, mant_mask;
    uint32_t expf, exp_mask;
    if (!v || v->kind != SN_KIND_FLOAT || !u) return SN_ERR_TYPE;
    e_bits = v->e_bits;
    m_bits = v->m_bits;
    if (!float_soft_supported(e_bits, m_bits)) return SN_ERR_RANGE;
    bias = float_bias(e_bits);
    tb = float_total_bits(e_bits, m_bits);
    if (tb <= 64) {
        uint64_t bits = float_unpack_u64(v) & (tb == 64 ? ~0ull : ((1ull << tb) - 1ull));
        sign = (int)((bits >> (tb - 1)) & 1u);
        mant_mask = (m_bits >= 64) ? ~0ull : ((1ull << m_bits) - 1ull);
        exp_mask = (e_bits >= 64) ? ~0u : ((1u << e_bits) - 1u);
        mant = bits & mant_mask;
        expf = (uint32_t)((bits >> m_bits) & exp_mask);
    } else {
        float_unpack_bits(v, &sign, &expf, &mant);
        mant_mask = (m_bits >= 64) ? ~0ull : ((1ull << m_bits) - 1ull);
        exp_mask = (e_bits >= 64) ? ~0u : ((1u << e_bits) - 1u);
        mant &= mant_mask;
        expf &= exp_mask;
    }

    u->sign = sign;
    u->sig = 0;
    u->exp = 0;
    u->is_zero = u->is_inf = u->is_nan = 0;

    if (expf == exp_mask) {
        if (mant != 0 && v->nan_enabled) {
            u->is_nan = 1;
            u->sig = mant;
            return SN_OK;
        }
        u->is_inf = 1;
        return SN_OK;
    }
    if (expf == 0) {
        if (mant == 0) {
            u->is_zero = 1;
            return SN_OK;
        }
        u->sig = mant;
        u->exp = 1 - bias - m_bits;
        return SN_OK;
    }
    u->sig = mant | (1ull << m_bits);
    u->exp = (int)expf - bias - m_bits;
    return SN_OK;
}

static int round_up(sn_round rnd, int sign, int lsb, int guard, int sticky)
{
    if (!guard && !sticky) return 0;
    switch (rnd) {
    case SN_ROUND_TZ: return 0;
    case SN_ROUND_UP: return sign == 0;
    case SN_ROUND_DN: return sign != 0;
    case SN_ROUND_NA: return guard != 0;
    case SN_ROUND_NTE:
    default:
        if (!guard) return 0;
        if (sticky) return 1;
        return lsb != 0;
    }
}

static void float_emit(sn_value *out, int sign, uint32_t expf, uint64_t mant, int e_bits, int m_bits)
{
    int tb = float_total_bits(e_bits, m_bits);
    if (tb <= 64) {
        uint64_t bits = ((uint64_t)sign << (tb - 1)) | ((uint64_t)expf << m_bits) | mant;
        float_pack_u64(out, bits);
    } else {
        float_pack_bits(out, sign, expf, mant, e_bits, m_bits);
    }
}

static sn_status float_pack(sn_ctx *ctx, sn_value *out, sn_funp u,
                            int e_bits, int m_bits, int nan_enabled, sn_round rnd)
{
    sn_status st;
    int bias, exp_max, e, shift, uexp;
    uint64_t mant, sig, mant_mask;
    uint32_t expf;
    int guard, sticky, lsb;

    if (!float_soft_supported(e_bits, m_bits)) return SN_ERR_RANGE;
    st = float_ensure(ctx, out, e_bits, m_bits, nan_enabled);
    if (st != SN_OK) return st;
    bias = float_bias(e_bits);
    exp_max = (int)float_exp_max64(e_bits);
    mant_mask = (m_bits >= 64) ? ~0ull : ((1ull << m_bits) - 1ull);

    if (u.is_nan) {
        if (!nan_enabled) {
            sn_raise(ctx, SN_FLAG_INVALID);
            float_emit(out, u.sign, (uint32_t)exp_max, 0, e_bits, m_bits);
            return SN_OK;
        }
        mant = u.sig ? u.sig : (1ull << (m_bits ? m_bits - 1 : 0));
        mant &= mant_mask;
        if (!mant) mant = 1;
        float_emit(out, u.sign, (uint32_t)exp_max, mant, e_bits, m_bits);
        return SN_OK;
    }
    if (u.is_inf) {
        float_emit(out, u.sign, (uint32_t)exp_max, 0, e_bits, m_bits);
        return SN_OK;
    }
    if (u.is_zero || u.sig == 0) {
        float_emit(out, u.sign, 0, 0, e_bits, m_bits);
        return SN_OK;
    }

    sig = u.sig;
    e = u.exp;
    sticky = u.sticky ? 1 : 0;
    guard = 0;
    /* Left-normalize to [2^m, 2^{m+1}) */
    while (sig && sig < (1ull << m_bits)) {
        sig <<= 1;
        e--;
    }
    /* Right-normalize: last out bit is guard; older out bits OR into sticky */
    while (sig >= (2ull << m_bits)) {
        sticky |= guard;
        guard = (int)(sig & 1ull);
        sig >>= 1;
        e++;
    }

    /* value = sig * 2^e with sig in [2^m, 2^{m+1}); unbiased exp of 1.frac form is e+m */
    uexp = e + m_bits;

    /*
     * Softfp-style: decide subnormal BEFORE rounding (no double-round).
     * See playground/libbf/softfp_template.h round_pack_sf / rshift_rnd.
     */
    if (uexp + bias <= 0) {
        shift = 1 - bias - uexp; /* >= 1 */
        sticky |= guard;
        guard = 0;
        if (shift >= 64) {
            sticky |= (sig != 0);
            sig = 0;
        } else {
            while (shift-- > 0) {
                sticky |= guard;
                guard = (int)(sig & 1ull);
                sig >>= 1;
            }
        }
        mant = sig & mant_mask;
        if (round_up(rnd, u.sign, (int)(mant & 1ull), guard, sticky)) {
            mant += 1;
            if (mant == (1ull << m_bits) || (m_bits >= 64 && mant == 0)) {
                /* rounded up to minimum normal */
                float_emit(out, u.sign, 1u, 0, e_bits, m_bits);
                sn_raise(ctx, SN_FLAG_UNDERFLOW | SN_FLAG_INEXACT);
                return SN_OK;
            }
        }
        if (guard || sticky) sn_raise(ctx, SN_FLAG_UNDERFLOW | SN_FLAG_INEXACT);
        else if (mant != 0) sn_raise(ctx, SN_FLAG_UNDERFLOW);
        float_emit(out, u.sign, 0, mant, e_bits, m_bits);
        return SN_OK;
    }

    /* Normal: single RNE (etc.) round on m_bits+1 integer sig + guard/sticky */
    lsb = (int)(sig & 1ull);
    if (round_up(rnd, u.sign, lsb, guard, sticky)) {
        sig += 1;
        if (sig == (2ull << m_bits)) {
            sig >>= 1;
            e++;
            uexp = e + m_bits;
        }
    }

    if (uexp + bias >= exp_max) {
        sn_raise(ctx, SN_FLAG_OVERFLOW | SN_FLAG_INEXACT);
        if (rnd == SN_ROUND_TZ ||
            (rnd == SN_ROUND_DN && u.sign == 0) ||
            (rnd == SN_ROUND_UP && u.sign != 0)) {
            mant = mant_mask;
            float_emit(out, u.sign, (uint32_t)(exp_max - 1), mant, e_bits, m_bits);
        } else {
            float_emit(out, u.sign, (uint32_t)exp_max, 0, e_bits, m_bits);
        }
        return SN_OK;
    }
    if (guard || sticky) sn_raise(ctx, SN_FLAG_INEXACT);
    expf = (uint32_t)(uexp + bias);
    mant = sig & mant_mask;
    float_emit(out, u.sign, expf, mant, e_bits, m_bits);
    return SN_OK;
}


sn_status sn_float_new(sn_ctx *ctx, sn_value *out, int e_bits, int m_bits, int nan_enabled)
{
    return float_ensure(ctx, out, e_bits, m_bits, nan_enabled);
}

sn_status sn_float_set_zero(sn_ctx *ctx, sn_value *out, int sign,
                            int e_bits, int m_bits, int nan_enabled)
{
    sn_status st = float_ensure(ctx, out, e_bits, m_bits, nan_enabled);
    if (st != SN_OK) return st;
    if (sign) {
        int tb = float_total_bits(e_bits, m_bits);
        float_set_bit(out, tb - 1, 1);
    }
    return SN_OK;
}

sn_status sn_float_set_inf(sn_ctx *ctx, sn_value *out, int sign,
                           int e_bits, int m_bits, int nan_enabled)
{
    sn_status st;
    int exp_max, tb, n, i;
    sn_limb *L;
    st = float_ensure(ctx, out, e_bits, m_bits, nan_enabled);
    if (st != SN_OK) return st;
    exp_max = (e_bits <= 30) ? (int)float_exp_max64(e_bits) : -1;
    if (float_soft_supported(e_bits, m_bits)) {
        float_emit(out, sign ? 1 : 0, (uint32_t)exp_max, 0, e_bits, m_bits);
        return SN_OK;
    }
    tb = 1 + e_bits + m_bits;
    n = sn_limbs_for_bits(tb);
    L = SN_LIMBS(out);
    for (i = 0; i < n; i++) L[i] = 0;
    /* all-ones exp field (works for any e_bits) */
    for (i = 0; i < e_bits; i++) {
        int b = m_bits + i;
        L[b / SN_LIMB_BITS] |= (sn_limb)1u << (b % SN_LIMB_BITS);
    }
    if (sign) {
        int b = tb - 1;
        L[b / SN_LIMB_BITS] |= (sn_limb)1u << (b % SN_LIMB_BITS);
    }
    out->nlimbs = n;
    return SN_OK;
}

sn_status sn_float_set_nan(sn_ctx *ctx, sn_value *out, int e_bits, int m_bits)
{
    sn_funp u;
    if (sn_float_mp_supported(e_bits, m_bits) || !float_soft_supported(e_bits, m_bits)) {
        /* quiet NaN: all-ones exp + top mant bit (any e_bits) */
        sn_value tmp;
        sn_status st;
        int tb = 1 + e_bits + m_bits;
        int i, n;
        sn_limb *L;
        st = sn_float_new(ctx, out, e_bits, m_bits, 1);
        if (st != SN_OK) return st;
        n = sn_limbs_for_bits(tb);
        L = SN_LIMBS(out);
        for (i = 0; i < n; i++) L[i] = 0;
        /* set top mant bit */
        if (m_bits > 0) {
            int b = m_bits - 1;
            L[b / SN_LIMB_BITS] |= (sn_limb)1u << (b % SN_LIMB_BITS);
        }
        for (i = 0; i < e_bits; i++) {
            int b = m_bits + i;
            L[b / SN_LIMB_BITS] |= (sn_limb)1u << (b % SN_LIMB_BITS);
        }
        out->nlimbs = n;
        (void)tmp;
        return SN_OK;
    }
    u.sign = 0;
    u.exp = 0;
    u.is_zero = u.is_inf = 0;
    u.is_nan = 1;
    u.sig = (m_bits > 0 && m_bits < 64) ? (1ull << (m_bits - 1)) : 1ull;
    return float_pack(ctx, out, u, e_bits, m_bits, 1, SN_ROUND_NTE);
}

static sn_status encode_from_double(sn_ctx *ctx, sn_value *out, double x,
                                    int e_bits, int m_bits, int nan_enabled, sn_round rnd)
{
    sn_funp u;
    int exp;
    double frac, scaled;
    uint64_t sig;
    double rem;

    u.sign = u.exp = u.is_zero = u.is_inf = u.is_nan = 0;
    u.sig = 0;

    if (x != x) {
        u.is_nan = 1;
        u.sig = 1;
        return float_pack(ctx, out, u, e_bits, m_bits, nan_enabled, rnd);
    }
    u.sign = signbit(x) ? 1 : 0;
    if (x == 0.0) {
        u.is_zero = 1;
        return float_pack(ctx, out, u, e_bits, m_bits, nan_enabled, rnd);
    }
    if (isinf(x)) {
        u.is_inf = 1;
        return float_pack(ctx, out, u, e_bits, m_bits, nan_enabled, rnd);
    }
    frac = frexp(fabs(x), &exp); /* [0.5,1) * 2^exp */
    scaled = ldexp(2.0 * frac, m_bits); /* [2^m, 2^{m+1}) */
    sig = (uint64_t)scaled;
    rem = scaled - (double)sig;
    u.sig = sig ? sig : 1;
    u.exp = exp - 1 - m_bits;
    if (rem != 0.0) sn_raise(ctx, SN_FLAG_INEXACT);
    return float_pack(ctx, out, u, e_bits, m_bits, nan_enabled, rnd);
}

sn_status sn_f16(sn_ctx *ctx, sn_value *out, double x)
{
    return encode_from_double(ctx, out, x, 5, 10, 1, sn_eff_round(ctx, NULL));
}

sn_status sn_f32(sn_ctx *ctx, sn_value *out, double x)
{
    return encode_from_double(ctx, out, x, 8, 23, 1, sn_eff_round(ctx, NULL));
}

sn_status sn_f64(sn_ctx *ctx, sn_value *out, double x)
{
    return encode_from_double(ctx, out, x, 11, 52, 1, sn_eff_round(ctx, NULL));
}

sn_status sn_float_from_i64(sn_ctx *ctx, sn_value *out, int64_t x,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt)
{
    if (sn_float_mp_supported(e_bits, m_bits))
        return sn_float_mp_from_i64(ctx, out, x, e_bits, m_bits, nan_enabled, opt);
    return encode_from_double(ctx, out, (double)x, e_bits, m_bits, nan_enabled,
                              sn_eff_round(ctx, opt));
}

sn_fpclass sn_fp_classify(const sn_value *v)
{
    sn_funp u;
    uint64_t bits, exp_mask, expf;
    if (!v || v->kind != SN_KIND_FLOAT) return SN_FP_NAN;
    if (sn_float_mp_supported(v->e_bits, v->m_bits))
        return sn_float_mp_classify(v);
    if (float_unpack(v, &u) != SN_OK) return SN_FP_NAN;
    if (u.is_nan) return SN_FP_NAN;
    if (u.is_inf) return SN_FP_INFINITE;
    if (u.is_zero) return SN_FP_ZERO;
    bits = float_unpack_u64(v);
    exp_mask = (v->e_bits >= 64) ? ~0ull : ((1ull << v->e_bits) - 1ull);
    expf = (bits >> v->m_bits) & exp_mask;
    if (expf == 0) return SN_FP_SUBNORMAL;
    return SN_FP_NORMAL;
}

int sn_fp_signbit(const sn_value *v)
{
    sn_funp u;
    if (!v || v->kind != SN_KIND_FLOAT) return 0;
    if (sn_float_mp_supported(v->e_bits, v->m_bits))
        return sn_float_mp_signbit(v);
    if (float_unpack(v, &u) != SN_OK) return 0;
    return u.sign;
}

sn_status sn_to_double(sn_ctx *ctx, const sn_value *v, double *out)
{
    sn_funp u;
    sn_status st;
    if (!out) return SN_ERR_ARG;
    if (v && v->kind == SN_KIND_FLOAT && sn_float_mp_supported(v->e_bits, v->m_bits))
        return sn_float_mp_to_double(ctx, v, out);
    st = float_unpack(v, &u);
    if (st != SN_OK) return st;
    if (u.is_nan) { *out = nan(""); return SN_OK; }
    if (u.is_inf) { *out = u.sign ? -INFINITY : INFINITY; return SN_OK; }
    if (u.is_zero) { *out = u.sign ? -0.0 : 0.0; return SN_OK; }
    {
        double d = ldexp((double)u.sig, u.exp);
        *out = u.sign ? -d : d;
        return SN_OK;
    }
}

static int same_float_fmt(const sn_value *a, const sn_value *b)
{
    return a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT &&
           a->e_bits == b->e_bits && a->m_bits == b->m_bits &&
           a->nan_enabled == b->nan_enabled;
}

static sn_funp funp_add(sn_funp a, sn_funp b, int sub)
{
    sn_funp r;
    int ys = b.sign ^ (sub ? 1 : 0);
    int sticky = 0;
    r.sign = r.exp = r.is_zero = r.is_inf = r.is_nan = r.sticky = 0;
    r.sig = 0;

    if (a.is_nan) return a;
    if (b.is_nan) { b.sign = ys; return b; }
    if (a.is_inf && b.is_inf && a.sign != ys) {
        r.is_nan = 1; r.sig = 1; return r;
    }
    if (a.is_inf) return a;
    if (b.is_inf) { b.sign = ys; return b; }

    /* Use only is_zero flags — Inf/NaN keep sig==0 and must not look like zero. */
    if (a.is_zero && b.is_zero) {
        r.is_zero = 1;
        r.sign = (a.sign && ys) ? 1 : 0; /* (-0)+(-0) => -0; else +0 (NTE add) */
        return r;
    }
    if (a.is_zero) { b.sign = ys; return b; }
    if (b.is_zero) return a;
    b.sign = ys;

    /*
     * IEEE-754 style add: keep 3 guard bits of headroom so alignment + pack
     * recover correct RNE (libbf/softfloat-like). Unpacked FP64 significands
     * are m+1<=53 bits so <<3 fits in uint64. Intermediate FMA products may
     * already occupy ~64 bits — skip inject then (softfp uses wider mant).
     */
    if (a.sig < (1ull << 61) && b.sig < (1ull << 61)) {
        a.sig <<= 3;
        a.exp -= 3;
        b.sig <<= 3;
        b.exp -= 3;
    }

    if (a.exp < b.exp) { sn_funp t = a; a = b; b = t; }
    while (b.exp < a.exp) {
        sticky |= (int)(b.sig & 1ull);
        b.sig >>= 1;
        b.exp++;
    }
    /* softfloat/libbf: fold lost bits into LSB so sub sees "slightly larger" b */
    if (sticky && b.sig != 0) b.sig |= 1ull;
    if (b.sig == 0 && sticky) {
        /* |b| fully shifted out: sum/diff is a ± epsilon (epsilon < 1 ulp of a/8 after GRS).
         * Encode epsilon via sticky for pack; for opposite signs result is slightly below |a|. */
        r = a;
        if (a.sign != b.sign) {
            /* |a| - eps: force sticky; if GRS all zero, subtract 1 ulp of extended sig */
            if ((r.sig & 7ull) == 0) {
                r.sig -= 1ull; /* borrow into extended field */
            } else {
                /* already have low bits; sticky-in will be applied below */
            }
            r.sticky = 1;
        } else {
            r.sticky = 1;
        }
        r.sticky |= a.sticky | b.sticky;
        return r;
    }
    r.exp = a.exp;
    if (a.sign == b.sign) {
        r.sig = a.sig + b.sig;
        r.sign = a.sign;
        if (r.sig < a.sig) { /* 64-bit carry — shift right with sticky */
            sticky |= (int)(r.sig & 1ull);
            r.sig = (a.sig >> 1) + (b.sig >> 1) + ((a.sig | b.sig) & 1ull);
            r.exp++;
        }
    } else {
        if (a.sig > b.sig) {
            r.sig = a.sig - b.sig;
            r.sign = a.sign;
        } else if (a.sig < b.sig) {
            r.sig = b.sig - a.sig;
            r.sign = b.sign;
        } else {
            /* exact cancel of visible bits; sticky means nonzero residual of opposite sign to a */
            if (sticky || a.sticky || b.sticky) {
                r.sig = 1ull;
                r.sign = b.sign; /* residual toward the truncated smaller term */
                r.exp = a.exp;
                r.sticky = 1;
                sticky |= a.sticky | b.sticky;
                r.sticky = sticky ? 1 : 0;
                return r;
            }
            r.is_zero = 1;
            r.sign = 0; /* exact cancel -> +0 under default NTE */
            return r;
        }
    }
    sticky |= a.sticky | b.sticky;
    r.sticky = sticky ? 1 : 0;
    return r;
}
static sn_funp funp_mul(sn_funp a, sn_funp b)
{
    sn_funp r;
    uint64_t ah, al, bh, bl;
    uint64_t p0, p1, p2, p3, mid, lo, hi;
    int sticky = 0;
    r.sign = r.exp = r.is_zero = r.is_inf = r.is_nan = r.sticky = 0;
    r.sig = 0;
    if (a.is_nan) return a;
    if (b.is_nan) return b;
    /* 0 * Inf = NaN; Inf * Inf = Inf (do not treat Inf as sig==0 zero) */
    if ((a.is_inf && b.is_zero) || (b.is_inf && a.is_zero)) {
        r.is_nan = 1; r.sig = 1; return r;
    }
    if (a.is_inf || b.is_inf) {
        r.is_inf = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }
    if (a.is_zero || b.is_zero) {
        r.is_zero = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }
    if (a.sig == 0 || b.sig == 0) {
        r.is_zero = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }
    /* 64x64 -> 128 using 32-bit parts */
    al = a.sig & 0xFFFFFFFFull; ah = a.sig >> 32;
    bl = b.sig & 0xFFFFFFFFull; bh = b.sig >> 32;
    p0 = al * bl;
    p1 = al * bh;
    p2 = ah * bl;
    p3 = ah * bh;
    mid = (p0 >> 32) + (p1 & 0xFFFFFFFFull) + (p2 & 0xFFFFFFFFull);
    lo = (p0 & 0xFFFFFFFFull) | (mid << 32);
    hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
    r.exp = a.exp + b.exp;
    r.sign = a.sign ^ b.sign;
    /* normalize product into r.sig (prefer high bits).
     * Bits shifted out become sticky; fold into LSB so float_pack sees them. */
    if (hi == 0) {
        r.sig = lo;
    } else {
        while (hi > 1ull) {
            sticky |= (int)(lo & 1ull);
            lo = (lo >> 1) | ((hi & 1ull) << 63);
            hi >>= 1;
            r.exp++;
        }
        /* hi is 0 or 1 here */
        if (hi == 1) {
            sticky |= (int)(lo & 1ull);
            r.sig = (1ull << 63) | (lo >> 1);
            r.exp++;
        } else {
            r.sig = lo;
        }
    }
    sticky |= a.sticky | b.sticky;
    r.sticky = sticky ? 1 : 0;
    return r;
}
static sn_funp funp_div(sn_funp a, sn_funp b)
{
    sn_funp r;
    uint64_t sa, sb, rem, q;
    int ea, eb, i, cy;
    r.sign = r.exp = r.is_zero = r.is_inf = r.is_nan = r.sticky = 0;
    r.sig = 0;
    if (a.is_nan) return a;
    if (b.is_nan) return b;
    /* Specials first — Inf has sig==0 and must not look like 0. */
    if (a.is_inf) {
        if (b.is_inf) { r.is_nan = 1; r.sig = 1; return r; }
        r.is_inf = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }
    if (b.is_inf) {
        r.is_zero = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }
    if (b.is_zero) {
        if (a.is_zero) { r.is_nan = 1; r.sig = 1; return r; }
        r.is_inf = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }
    if (a.is_zero || a.sig == 0) {
        r.is_zero = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }
    if (b.sig == 0) {
        /* should be covered by is_zero; treat as div-by-zero */
        r.is_inf = 1;
        r.sign = a.sign ^ b.sign;
        return r;
    }

    /* Portable full-precision division (no __int128).
     * Normalize both significands into [2^63, 2^64), then compute
     * q = floor((sa / sb) * 2^63) by restoring long division.
     * Remainder stays < sb after each step; when rem has high bit set,
     * a left shift overflows 64 bits and is handled via the carry flag. */
    sa = a.sig;
    sb = b.sig;
    ea = a.exp;
    eb = b.exp;
    while (sa < (1ull << 63)) { sa <<= 1; ea--; }
    while (sb < (1ull << 63)) { sb <<= 1; eb--; }

    rem = sa;
    q = 0;
    if (rem >= sb) {
        rem -= sb;
        q = 1ull;
    }
    for (i = 0; i < 63; i++) {
        cy = (int)(rem >> 63);
        rem <<= 1;
        q <<= 1;
        if (cy || rem >= sb) {
            rem -= sb; /* unsigned wrap is correct when cy==1 */
            q |= 1ull;
        }
    }
    if (q == 0) q = 1ull;
    r.sig = q;
    r.exp = ea - eb - 63;
    r.sign = a.sign ^ b.sign;
    /* nonzero remainder => inexact quotient below r.sig */
    r.sticky = (rem != 0 || a.sticky || b.sticky) ? 1 : 0;
    return r;
}
sn_status sn_float_add(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_funp ua, ub, ur;
    sn_status st;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_add(ctx, out, a, b, opt);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    st = float_unpack(b, &ub); if (st != SN_OK) return st;
    ur = funp_add(ua, ub, 0);
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_float_sub(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_funp ua, ub, ur;
    sn_status st;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_sub(ctx, out, a, b, opt);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    st = float_unpack(b, &ub); if (st != SN_OK) return st;
    ur = funp_add(ua, ub, 1);
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_float_mul(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_funp ua, ub, ur;
    sn_status st;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_mul(ctx, out, a, b, opt);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    st = float_unpack(b, &ub); if (st != SN_OK) return st;
    ur = funp_mul(ua, ub);
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_float_div(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_funp ua, ub, ur;
    sn_status st;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_div(ctx, out, a, b, opt);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    st = float_unpack(b, &ub); if (st != SN_OK) return st;
    if ((ub.is_zero || ub.sig == 0) && !ua.is_nan)
        sn_raise(ctx, SN_FLAG_DIVZERO);
    ur = funp_div(ua, ub);
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_float_neg(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int tb;
    (void)opt;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    st = sn_value_copy(ctx, out, a);
    if (st != SN_OK) return st;
    tb = float_total_bits(a->e_bits, a->m_bits);
    float_set_bit(out, tb - 1, !float_get_bit(out, tb - 1));
    return SN_OK;
}

sn_status sn_float_abs(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int tb;
    (void)opt;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    st = sn_value_copy(ctx, out, a);
    if (st != SN_OK) return st;
    tb = float_total_bits(a->e_bits, a->m_bits);
    float_set_bit(out, tb - 1, 0);
    return SN_OK;
}

sn_status sn_float_cmp(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b)
{
    sn_funp ua, ub;
    sn_status st;
    double da, db;
    if (!rel || !same_float_fmt(a, b)) return SN_ERR_ARG;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_cmp(ctx, rel, a, b);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    st = float_unpack(b, &ub); if (st != SN_OK) return st;
    if (ua.is_nan || ub.is_nan) {
        *rel = 0;
        return SN_ERR_INVALID;
    }
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, b, &db); if (st != SN_OK) return st;
    if (da < db) *rel = -1;
    else if (da > db) *rel = 1;
    else *rel = 0;
    return SN_OK;
}


/* ---------- FMA / sqrt / frem / cast / string ---------- */

/*
 * Narrow fused multiply-add (softfp fma_sf style):
 * keep the full 128-bit product, align the smaller operand with sticky
 * right-shift, add/sub in double-word, then collapse once for float_pack.
 * (funp_mul + funp_add loses low product bits when product << |c|.)
 */
static void funp_mul128(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
    uint64_t al = a & 0xffffffffull, ah = a >> 32;
    uint64_t bl = b & 0xffffffffull, bh = b >> 32;
    uint64_t p0 = al * bl;
    uint64_t p1 = al * bh;
    uint64_t p2 = ah * bl;
    uint64_t p3 = ah * bh;
    uint64_t mid = (p0 >> 32) + (p1 & 0xffffffffull) + (p2 & 0xffffffffull);
    *lo = (p0 & 0xffffffffull) | (mid << 32);
    *hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
}

/* Right-shift 128-bit value by n bits; OR discarded bits into *sticky. */
static void shr128_sticky(uint64_t *hi, uint64_t *lo, int n, int *sticky)
{
    uint64_t h, l;
    if (n <= 0) return;
    h = *hi;
    l = *lo;
    if (n >= 128) {
        if (h | l) *sticky = 1;
        *hi = 0;
        *lo = 0;
        return;
    }
    if (n >= 64) {
        int k = n - 64;
        if (l) *sticky = 1;
        if (k == 0) {
            l = h;
            h = 0;
        } else {
            uint64_t mask = (k >= 64) ? ~0ull : ((1ull << k) - 1ull);
            if (h & mask) *sticky = 1;
            l = (k >= 64) ? 0 : (h >> k);
            h = 0;
        }
        *hi = h;
        *lo = l;
        return;
    }
    {
        uint64_t mask = (1ull << n) - 1ull;
        if (l & mask) *sticky = 1;
        l = (l >> n) | (h << (64 - n));
        h >>= n;
        *hi = h;
        *lo = l;
    }
}

static int cmp128(uint64_t ahi, uint64_t alo, uint64_t bhi, uint64_t blo)
{
    if (ahi > bhi) return 1;
    if (ahi < bhi) return -1;
    if (alo > blo) return 1;
    if (alo < blo) return -1;
    return 0;
}

/* Collapse 128-bit integer significand to sn_funp (single 64-bit sig + sticky). */
static sn_funp funp_from128(int sign, int exp, uint64_t hi, uint64_t lo, int sticky)
{
    sn_funp r;
    r.sign = sign;
    r.is_zero = r.is_inf = r.is_nan = 0;
    r.sticky = 0;
    r.sig = 0;
    r.exp = exp;
    if (hi == 0 && lo == 0) {
        r.is_zero = 1;
        r.sign = sticky ? sign : 0;
        r.sticky = sticky ? 1 : 0;
        return r;
    }
    if (hi == 0) {
        r.sig = lo;
        r.exp = exp;
    } else {
        while (hi > 1ull) {
            sticky |= (int)(lo & 1ull);
            lo = (lo >> 1) | ((hi & 1ull) << 63);
            hi >>= 1;
            exp++;
        }
        if (hi == 1ull) {
            sticky |= (int)(lo & 1ull);
            r.sig = (1ull << 63) | (lo >> 1);
            r.exp = exp + 1;
        } else {
            r.sig = lo;
            r.exp = exp;
        }
    }
    r.sticky = sticky ? 1 : 0;
    if (r.sig == 0) {
        r.is_zero = 1;
        r.sign = sticky ? sign : 0;
    }
    return r;
}

static sn_funp funp_fma(sn_funp a, sn_funp b, sn_funp c)
{
    sn_funp r;
    uint64_t phi, plo, chi, clo, tlo, thi;
    int psign, pexp, csign, cexp;
    int sticky = 0;
    int a_zero, b_zero, c_zero;
    int cmp;
    int lost;

    r.sign = r.exp = r.is_zero = r.is_inf = r.is_nan = r.sticky = 0;
    r.sig = 0;

    if (a.is_nan) return a;
    if (b.is_nan) return b;
    if (c.is_nan) return c;

    a_zero = a.is_zero || (!a.is_inf && a.sig == 0);
    b_zero = b.is_zero || (!b.is_inf && b.sig == 0);
    c_zero = c.is_zero || (!c.is_inf && c.sig == 0);
    psign = a.sign ^ b.sign;

    if ((a.is_inf && b_zero) || (b.is_inf && a_zero)) {
        r.is_nan = 1;
        r.sig = 1;
        return r;
    }
    if (a.is_inf || b.is_inf) {
        if (c.is_inf && psign != c.sign) {
            r.is_nan = 1;
            r.sig = 1;
            return r;
        }
        r.is_inf = 1;
        r.sign = psign;
        return r;
    }
    if (c.is_inf) {
        r.is_inf = 1;
        r.sign = c.sign;
        return r;
    }

    if (a_zero || b_zero) {
        if (c_zero) {
            r.is_zero = 1;
            r.sign = (psign && c.sign) ? 1 : 0;
            return r;
        }
        return c;
    }
    if (c_zero)
        return funp_mul(a, b);

    /* Exact 64x64 -> 128 product (m<=52 => product fits in 106 bits). */
    funp_mul128(a.sig, b.sig, &phi, &plo);
    pexp = a.exp + b.exp;
    sticky = a.sticky | b.sticky | c.sticky;

    /*
     * Place c in the high half of a 128-bit field and free the low 64 bits for
     * alignment residue (softfp: c_mant in high, product low bits in mant0).
     * value = (chi*2^64 + clo) * 2^cexp_adj, with cexp_adj = c.exp - 64.
     */
    chi = c.sig;
    clo = 0;
    cexp = c.exp - 64;
    csign = c.sign;

    /* Align to the larger exponent by sticky right-shifting the smaller. */
    if (pexp > cexp) {
        lost = 0;
        shr128_sticky(&chi, &clo, pexp - cexp, &lost);
        sticky |= lost;
        cexp = pexp;
        if (lost) {
            /* rshift_rnd: fold lost bits into LSB of remaining (or unit in low). */
            if ((chi | clo) == 0) clo = 1ull;
            else clo |= 1ull;
        }
    } else if (cexp > pexp) {
        lost = 0;
        shr128_sticky(&phi, &plo, cexp - pexp, &lost);
        sticky |= lost;
        pexp = cexp;
        if (lost) {
            if ((phi | plo) == 0) plo = 1ull;
            else plo |= 1ull;
        }
    }

    cmp = cmp128(phi, plo, chi, clo);
    if (cmp < 0) {
        thi = phi; tlo = plo;
        phi = chi; plo = clo;
        chi = thi; clo = tlo;
        {
            int te = pexp, ts = psign;
            pexp = cexp;
            psign = csign;
            cexp = te;
            csign = ts;
        }
    }

    if (psign == csign) {
        tlo = plo + clo;
        thi = phi + chi + (tlo < plo ? 1ull : 0ull);
        phi = thi;
        plo = tlo;
    } else {
        tlo = plo - clo;
        thi = phi - chi - (plo < clo ? 1ull : 0ull);
        phi = thi;
        plo = tlo;
        if (phi == 0 && plo == 0) {
            r.is_zero = 1;
            r.sign = 0;
            return r;
        }
    }

    /*
     * Collapse 128-bit -> funp. The 128-bit integer is at exponent pexp, so
     * sig*2^exp representation: after packing high bits into sig, exp tracks.
     * Note c was stored with -64 bias in exp; product was not — after align
     * both share pexp. funp_from128 treats (hi,lo) as integer at `exp`.
     */
    return funp_from128(psign, pexp, phi, plo, sticky);
}

sn_status sn_float_fma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                       const sn_value *c, const sn_op_opt *opt)
{
    sn_funp ua, ub, uc, ur;
    sn_status st;
    if (!a || !b || !c || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT || c->kind != SN_KIND_FLOAT)
        return SN_ERR_TYPE;
    if (!same_float_fmt(a, b) || !same_float_fmt(a, c)) return SN_ERR_TYPE;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_fma(ctx, out, a, b, c, opt);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    st = float_unpack(b, &ub); if (st != SN_OK) return st;
    st = float_unpack(c, &uc); if (st != SN_OK) return st;
    ur = funp_fma(ua, ub, uc);
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_fma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                 const sn_value *c, const sn_op_opt *opt)
{
    if (a && a->kind == SN_KIND_FLOAT)
        return sn_float_fma(ctx, out, a, b, c, opt);
    return SN_ERR_TYPE;
}

sn_status sn_float_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_funp ua, ur;
    sn_status st;
    double da, dr;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_sqrt(ctx, out, a, opt);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    if (ua.is_nan) {
        sn_raise(ctx, SN_FLAG_INVALID);
        return float_pack(ctx, out, ua, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
    }
    if (ua.sign && !ua.is_zero && ua.sig != 0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled) {
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        }
        ur.is_nan = 1; ur.is_inf = ur.is_zero = 0; ur.sign = 0; ur.sig = 1; ur.exp = 0;
        return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
    }
    /* Prefer host sqrt via double for formats that fit (all current <=64-bit). */
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    if (da < 0.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        ur.is_nan = 1; ur.is_inf = ur.is_zero = 0; ur.sign = 0; ur.sig = 1; ur.exp = 0;
        return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
    }
    dr = sqrt(da);
    return encode_from_double(ctx, out, dr, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_float_sqrt(ctx, out, a, opt);
}

sn_status sn_float_frem(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_funp ua, ub, ur;
    sn_status st;
    double da, db, dr;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (sn_float_mp_supported(a->e_bits, a->m_bits))
        return sn_float_mp_frem(ctx, out, a, b, opt);
    st = float_unpack(a, &ua); if (st != SN_OK) return st;
    st = float_unpack(b, &ub); if (st != SN_OK) return st;
    if (ua.is_nan || ub.is_nan) {
        ur.is_nan = 1; ur.is_inf = ur.is_zero = 0; ur.sign = 0; ur.sig = 1; ur.exp = 0;
        sn_raise(ctx, SN_FLAG_INVALID);
        return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
    }
    if (ua.is_inf || ub.is_zero || ub.sig == 0) {
        ur.is_nan = 1; ur.is_inf = ur.is_zero = 0; ur.sign = 0; ur.sig = 1; ur.exp = 0;
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, ua.sign, a->e_bits, a->m_bits, a->nan_enabled);
        return float_pack(ctx, out, ur, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
    }
    if (ub.is_inf)
        return sn_value_copy(ctx, out, a);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, b, &db); if (st != SN_OK) return st;
    dr = remainder(da, db);
    return encode_from_double(ctx, out, dr, a->e_bits, a->m_bits, a->nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_frem(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (a && a->kind == SN_KIND_FLOAT)
        return sn_float_frem(ctx, out, a, b, opt);
    return SN_ERR_TYPE;
}

sn_status sn_float_to_i64(sn_ctx *ctx, const sn_value *v, int64_t *out, const sn_op_opt *opt)
{
    double d;
    sn_status st;
    sn_fpclass c;
    (void)opt;
    if (!out || !v || v->kind != SN_KIND_FLOAT) return SN_ERR_ARG;
    c = sn_fp_classify(v);
    if (c == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        *out = 0;
        return SN_ERR_INVALID;
    }
    if (c == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        *out = sn_fp_signbit(v) ? INT64_MIN : INT64_MAX;
        return SN_ERR_RANGE;
    }
    if (sn_float_mp_supported(v->e_bits, v->m_bits))
        return sn_float_mp_to_i64(ctx, v, out);

    st = sn_to_double(ctx, v, &d);
    if (st != SN_OK) return st;
    if (d >= 9223372036854775808.0) {
        sn_raise(ctx, SN_FLAG_OVERFLOW);
        *out = INT64_MAX;
        return SN_ERR_RANGE;
    }
    if (d < -9223372036854775808.0) {
        sn_raise(ctx, SN_FLAG_OVERFLOW);
        *out = INT64_MIN;
        return SN_ERR_RANGE;
    }
    *out = (int64_t)d;
    if ((double)(*out) != d) sn_raise(ctx, SN_FLAG_INEXACT);
    return SN_OK;
}

sn_status sn_float_from_num(sn_ctx *ctx, sn_value *out, const sn_value *src,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt)
{
    double d;
    sn_status st;
    if (!src) return SN_ERR_ARG;
    if (src->kind == SN_KIND_FLOAT) {
        /* Same format: copy bits. */
        if (src->e_bits == e_bits && src->m_bits == m_bits &&
            src->nan_enabled == (nan_enabled ? 1 : 0))
            return sn_value_copy(ctx, out, src);
        /* Multiprec target: prefer hex string path to avoid double truncation. */
        if (sn_float_mp_supported(e_bits, m_bits) &&
            sn_float_mp_supported(src->e_bits, src->m_bits)) {
            char *s = NULL;
            st = sn_float_to_str(ctx, &s, src);
            if (st != SN_OK) return st;
            st = sn_float_from_str(ctx, out, s, e_bits, m_bits, nan_enabled, opt);
            if (s) sn_str_free(ctx, s);
            return st;
        }
        st = sn_to_double(ctx, src, &d);
        if (st != SN_OK) return st;
        return sn_float_from_double(ctx, out, d, e_bits, m_bits, nan_enabled, opt);
    }
    if (src->kind == SN_KIND_INT || src->kind == SN_KIND_BIGINT) {
        if (sn_float_mp_supported(e_bits, m_bits))
            return sn_float_mp_from_bigint(ctx, out, src, e_bits, m_bits, nan_enabled, opt);
        {
            int64_t x;
            st = sn_to_i64(ctx, src, &x);
            if (st != SN_OK) {
                uint64_t ux;
                st = sn_to_u64(ctx, src, &ux);
                if (st != SN_OK) return st;
                d = (double)ux;
            } else {
                d = (double)x;
            }
            return sn_float_from_double(ctx, out, d, e_bits, m_bits, nan_enabled, opt);
        }
    }
    return SN_ERR_TYPE;
}

sn_status sn_cast_float(sn_ctx *ctx, sn_value *out, const sn_value *src,
                        int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt)
{
    return sn_float_from_num(ctx, out, src, e_bits, m_bits, nan_enabled, opt);
}

sn_status sn_cast_int(sn_ctx *ctx, sn_value *out, const sn_value *src,
                      int width, int is_signed, const sn_op_opt *opt)
{
    sn_status st;
    int64_t x;
    if (!src) return SN_ERR_ARG;
    if (src->kind == SN_KIND_FLOAT) {
        st = sn_float_to_i64(ctx, src, &x, opt);
        if (st != SN_OK && st != SN_ERR_RANGE && st != SN_OK) {
            if (st == SN_ERR_INVALID) return st;
        }
        if (st == SN_ERR_INVALID) return st;
        /* RANGE still packs saturating-ish via set_i64 mask */
        return sn_int_set_i64(ctx, out, x, width, is_signed);
    }
    if (src->kind == SN_KIND_INT || src->kind == SN_KIND_BIGINT) {
        st = sn_to_i64(ctx, src, &x);
        if (st != SN_OK) {
            uint64_t ux;
            st = sn_to_u64(ctx, src, &ux);
            if (st != SN_OK) return st;
            return sn_int_set_u64(ctx, out, ux, width, is_signed);
        }
        return sn_int_set_i64(ctx, out, x, width, is_signed);
    }
    return SN_ERR_TYPE;
}

/* Allocate C-string via ctx (size_t header prefix); free with sn_str_free. */
static sn_status sn_alloc_cstr(sn_ctx *ctx, char **out, const char *s)
{
    size_t n, *hdr;
    char *buf;
    if (!out || !s) return SN_ERR_ARG;
    *out = NULL;
    n = strlen(s) + 1;
    hdr = (size_t *)sn_malloc(ctx, sizeof(size_t) + n);
    if (!hdr) return SN_ERR_NOMEM;
    *hdr = sizeof(size_t) + n;
    buf = (char *)(void *)(hdr + 1);
    memcpy(buf, s, n);
    *out = buf;
    return SN_OK;
}

/* Multiprec binary hex float: [-]0x1.hhhhp+/-e (full trailing significand).
 * Bit-accurate: frexp -> abs mant in [0.5,1) -> 0x1.frac * 2^(e-1).
 * Sign is applied only to the prefix; fraction digits use |mant|.
 */
static sn_status sn_float_mp_to_hexstr(sn_ctx *ctx, char **out, const sn_value *v)
{
    sn_status st;
    sn_value mant, amant;
    int e = 0, i, digs, cap, len, pe;
    char *buf = NULL;
    size_t *hdr = NULL;
    int sign;
    sn_fpclass c;

    if (!out || !v) return SN_ERR_ARG;
    *out = NULL;
    c = sn_fp_classify(v);
    if (c == SN_FP_NAN) return sn_alloc_cstr(ctx, out, "nan");
    if (c == SN_FP_INFINITE)
        return sn_alloc_cstr(ctx, out, sn_fp_signbit(v) ? "-inf" : "inf");
    if (c == SN_FP_ZERO)
        return sn_alloc_cstr(ctx, out, sn_fp_signbit(v) ? "-0x0p+0" : "0x0p+0");

    sn_value_init(&mant);
    sn_value_init(&amant);
    st = sn_frexp(ctx, &mant, &e, v, NULL);
    if (st != SN_OK) {
        sn_value_clear(ctx, &mant);
        sn_value_clear(ctx, &amant);
        return st;
    }
    /* frexp preserves sign; fraction extraction needs |mant| in [0.5,1). */
    sign = sn_fp_signbit(v);
    if (sign) {
        st = sn_abs(ctx, &amant, &mant, NULL);
        if (st != SN_OK) {
            sn_value_clear(ctx, &mant);
            sn_value_clear(ctx, &amant);
            return st;
        }
        sn_value_clear(ctx, &mant);
        sn_value_move(&mant, &amant);
        sn_value_init(&amant);
    }

    digs = (v->m_bits + 3) / 4; /* hex digits for trailing fraction bits */
    if (digs < 1) digs = 1;
    if (digs > 100000) digs = 100000;
    cap = digs + 48;
    hdr = (size_t *)sn_malloc(ctx, sizeof(size_t) + (size_t)cap);
    if (!hdr) {
        sn_value_clear(ctx, &mant);
        sn_value_clear(ctx, &amant);
        return SN_ERR_NOMEM;
    }
    *hdr = sizeof(size_t) + (size_t)cap;
    buf = (char *)(void *)(hdr + 1);
    len = 0;
    if (sign) buf[len++] = '-';
    buf[len++] = '0'; buf[len++] = 'x'; buf[len++] = '1';
    if (digs > 0) {
        sn_value t, sixteen, one, acc;
        sn_value_init(&t);
        sn_value_init(&sixteen);
        sn_value_init(&one);
        sn_value_init(&acc);
        st = sn_float_from_i64(ctx, &sixteen, 16, v->e_bits, v->m_bits, v->nan_enabled, NULL);
        if (st == SN_OK) st = sn_float_from_i64(ctx, &one, 1, v->e_bits, v->m_bits, v->nan_enabled, NULL);
        /* frac = 2*|mant| - 1  in [0,1) */
        if (st == SN_OK) st = sn_add(ctx, &t, &mant, &mant, NULL);
        if (st == SN_OK) st = sn_sub(ctx, &acc, &t, &one, NULL);
        if (st != SN_OK) {
            sn_value_clear(ctx, &t); sn_value_clear(ctx, &sixteen);
            sn_value_clear(ctx, &one); sn_value_clear(ctx, &acc);
            sn_value_clear(ctx, &mant);
            sn_free(ctx, hdr, *hdr);
            return st;
        }
        buf[len++] = '.';
        for (i = 0; i < digs; i++) {
            int digit = 0;
            sn_value digv;
            sn_value_init(&digv);
            st = sn_mul(ctx, &acc, &acc, &sixteen, NULL);
            if (st != SN_OK) { sn_value_clear(ctx, &digv); break; }
            /* digit = floor(acc); acc -= digit */
            st = sn_floor(ctx, &digv, &acc, NULL);
            if (st != SN_OK) { sn_value_clear(ctx, &digv); break; }
            {
                int64_t di = 0;
                if (sn_to_i64(ctx, &digv, &di) == SN_OK && di >= 0 && di < 16)
                    digit = (int)di;
            }
            st = sn_sub(ctx, &acc, &acc, &digv, NULL);
            sn_value_clear(ctx, &digv);
            if (st != SN_OK) break;
            buf[len++] = (char)(digit < 10 ? '0' + digit : 'a' + (digit - 10));
        }
        sn_value_clear(ctx, &t);
        sn_value_clear(ctx, &sixteen);
        sn_value_clear(ctx, &one);
        sn_value_clear(ctx, &acc);
        if (st != SN_OK) {
            sn_value_clear(ctx, &mant);
            sn_free(ctx, hdr, *hdr);
            return st;
        }
        /* trim trailing zeros in fraction */
        while (len > 0 && buf[len - 1] == '0') len--;
        if (len > 0 && buf[len - 1] == '.') len--;
    }
    pe = e - 1; /* 0x1.xxx * 2^pe */
    {
        char expbuf[32];
        int nexp = snprintf(expbuf, sizeof(expbuf), "p%+d", pe);
        if (nexp < 0 || len + nexp + 1 > cap) {
            size_t ncap = (size_t)(len + (nexp > 0 ? nexp : 0) + 32);
            size_t *nh = (size_t *)sn_realloc(ctx, hdr, *hdr, sizeof(size_t) + ncap);
            if (!nh) {
                sn_value_clear(ctx, &mant);
                sn_free(ctx, hdr, *hdr);
                return SN_ERR_NOMEM;
            }
            *nh = sizeof(size_t) + ncap;
            hdr = nh;
            buf = (char *)(void *)(hdr + 1);
            cap = (int)ncap;
        }
        memcpy(buf + len, expbuf, (size_t)nexp);
        len += nexp;
        buf[len] = '\0';
    }
    sn_value_clear(ctx, &mant);
    sn_value_clear(ctx, &amant);
    *out = buf;
    return SN_OK;
}


/* Multiprec decimal free format: [-]d.dddde[+/-]exp
 * Enough digits for round-trip via sn_float_from_decstr_mp (~ ceil(m*log10(2))+3).
 * Uses log10 + repeated *10 extraction at elevated working precision.
 */
static sn_status sn_float_mp_to_decstr(sn_ctx *ctx, char **out, const sn_value *v)
{
    sn_status st;
    sn_value av, ten, one, scale, t, digv, pow10, ln, ln10, expf, floor_e;
    sn_fpclass c;
    int sign, e_bits, m_bits, nan_en, e_work, m_work;
    int ndig, pe, i, cap, len, exp10;
    char *buf = NULL;
    size_t *hdr = NULL;
    int64_t di;

    if (!out || !v) return SN_ERR_ARG;
    *out = NULL;
    c = sn_fp_classify(v);
    if (c == SN_FP_NAN) return sn_alloc_cstr(ctx, out, "nan");
    if (c == SN_FP_INFINITE)
        return sn_alloc_cstr(ctx, out, sn_fp_signbit(v) ? "-inf" : "inf");
    if (c == SN_FP_ZERO)
        return sn_alloc_cstr(ctx, out, sn_fp_signbit(v) ? "-0" : "0");

    sign = sn_fp_signbit(v);
    e_bits = v->e_bits;
    m_bits = v->m_bits;
    nan_en = v->nan_enabled;

    /* Significant decimal digits: floor(m*log10(2)) + 1, then one guard digit
     * extracted for final round-to-nearest (half away). Free-format round-trip. */
    ndig = (int)(((int64_t)m_bits * 30103) / 100000) + 1;
    if (ndig < 2) ndig = 2;
    if (ndig > 200000) ndig = 200000;

    /* elevate so log10/scale/digit extraction keep > target digits */
    e_work = e_bits < 18 ? 18 : e_bits;
    m_work = m_bits + 96;
    if (m_work < 96) m_work = 96;
    if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
    if (m_work <= 52) m_work = 53;

    sn_value_init(&av);
    sn_value_init(&ten);
    sn_value_init(&one);
    sn_value_init(&scale);
    sn_value_init(&t);
    sn_value_init(&digv);
    sn_value_init(&pow10);
    sn_value_init(&ln);
    sn_value_init(&ln10);
    sn_value_init(&expf);
    sn_value_init(&floor_e);

    st = sn_cast_float(ctx, &av, v, e_work, m_work, nan_en, NULL);
    if (st != SN_OK) goto done;
    if (sign) {
        st = sn_abs(ctx, &t, &av, NULL);
        if (st != SN_OK) goto done;
        sn_value_clear(ctx, &av);
        sn_value_move(&av, &t);
        sn_value_init(&t);
    }

    st = sn_float_from_i64(ctx, &ten, 10, e_work, m_work, nan_en, NULL);
    if (st != SN_OK) goto done;
    st = sn_float_from_i64(ctx, &one, 1, e_work, m_work, nan_en, NULL);
    if (st != SN_OK) goto done;

    /* exp10 = floor(log10(|v|)); scale = |v| / 10^exp10 in [1,10) */
    st = sn_log10(ctx, &ln, &av, NULL);
    if (st != SN_OK) goto done;
    st = sn_floor(ctx, &floor_e, &ln, NULL);
    if (st != SN_OK) goto done;
    st = sn_to_i64(ctx, &floor_e, &di);
    if (st != SN_OK) goto done;
    exp10 = (int)di;
    /* guard: clamp absurd exponents from log edge cases */
    if (exp10 > 100000000) exp10 = 100000000;
    if (exp10 < -100000000) exp10 = -100000000;

    /* pow10 = 10^|exp10| via integer binary exponentiation on floats */
    st = sn_value_copy(ctx, &pow10, &one);
    if (st != SN_OK) goto done;
    {
        int ek = exp10 >= 0 ? exp10 : -exp10;
        sn_value base, tmp;
        sn_value_init(&base);
        sn_value_init(&tmp);
        st = sn_value_copy(ctx, &base, &ten);
        if (st == SN_OK) {
            while (ek > 0) {
                if (ek & 1) {
                    st = sn_mul(ctx, &tmp, &pow10, &base, NULL);
                    if (st != SN_OK) break;
                    sn_value_clear(ctx, &pow10);
                    sn_value_move(&pow10, &tmp);
                    sn_value_init(&tmp);
                }
                ek >>= 1;
                if (ek) {
                    st = sn_mul(ctx, &tmp, &base, &base, NULL);
                    if (st != SN_OK) break;
                    sn_value_clear(ctx, &base);
                    sn_value_move(&base, &tmp);
                    sn_value_init(&tmp);
                }
            }
        }
        sn_value_clear(ctx, &base);
        sn_value_clear(ctx, &tmp);
        if (st != SN_OK) goto done;
    }
    if (exp10 >= 0)
        st = sn_div(ctx, &scale, &av, &pow10, NULL);
    else
        st = sn_mul(ctx, &scale, &av, &pow10, NULL);
    if (st != SN_OK) goto done;

    /* normalize scale into [1,10): fix floor(log10) off-by-one at powers of ten */
    for (i = 0; i < 8; i++) {
        int rel = 0;
        st = sn_cmp(ctx, &rel, &scale, &ten);
        if (st != SN_OK) goto done;
        if (rel < 0) break;
        st = sn_div(ctx, &scale, &scale, &ten, NULL);
        if (st != SN_OK) goto done;
        exp10 += 1;
    }
    for (i = 0; i < 8; i++) {
        int rel = 0;
        st = sn_cmp(ctx, &rel, &scale, &one);
        if (st != SN_OK) goto done;
        if (rel >= 0) break;
        st = sn_mul(ctx, &scale, &scale, &ten, NULL);
        if (st != SN_OK) goto done;
        exp10 -= 1;
    }

    cap = ndig + 48;
    hdr = (size_t *)sn_malloc(ctx, sizeof(size_t) + (size_t)cap);
    if (!hdr) { st = SN_ERR_NOMEM; goto done; }
    *hdr = sizeof(size_t) + (size_t)cap;
    buf = (char *)(void *)(hdr + 1);
    len = 0;
    if (sign) buf[len++] = '-';

    /* first digit (integer part of scale in [1,10)) */
    st = sn_floor(ctx, &digv, &scale, NULL);
    if (st != SN_OK) goto fail_buf;
    di = 0;
    st = sn_to_i64(ctx, &digv, &di);
    if (st != SN_OK) goto fail_buf;
    if (di < 0) di = 0;
    if (di > 9) di = 9;
    buf[len++] = (char)('0' + (int)di);
    st = sn_sub(ctx, &scale, &scale, &digv, NULL);
    if (st != SN_OK) goto fail_buf;

    /* Extract ndig significant digits, then one extra guard digit in scale for rounding. */
    if (ndig > 1) {
        buf[len++] = '.';
        for (i = 1; i < ndig; i++) {
            st = sn_mul(ctx, &scale, &scale, &ten, NULL);
            if (st != SN_OK) goto fail_buf;
            st = sn_floor(ctx, &digv, &scale, NULL);
            if (st != SN_OK) goto fail_buf;
            di = 0;
            st = sn_to_i64(ctx, &digv, &di);
            if (st != SN_OK) goto fail_buf;
            if (di < 0) di = 0;
            if (di > 9) di = 9;
            buf[len++] = (char)('0' + (int)di);
            st = sn_sub(ctx, &scale, &scale, &digv, NULL);
            if (st != SN_OK) goto fail_buf;
        }
    }

    /* Final round of the decimal digit string from remaining fractional scale.
     * scale is in [0,1) after last digit; round half away from 0 using floor(2*scale)>=1.
     * Carry may cascade through '9's and bump exp10. */
    {
        sn_value half, two;
        int rel = 0, do_round = 0, pos, dig;
        int dstart = sign ? 1 : 0; /* first digit index in buf */
        sn_value_init(&half);
        sn_value_init(&two);
        st = sn_float_from_i64(ctx, &two, 2, e_work, m_work, nan_en, NULL);
        if (st == SN_OK)
            st = sn_mul(ctx, &t, &scale, &two, NULL);
        if (st == SN_OK)
            st = sn_float_from_i64(ctx, &half, 1, e_work, m_work, nan_en, NULL);
        if (st == SN_OK)
            st = sn_cmp(ctx, &rel, &t, &half);
        if (st == SN_OK && rel >= 0)
            do_round = 1;
        sn_value_clear(ctx, &half);
        sn_value_clear(ctx, &two);
        if (st != SN_OK) goto fail_buf;
        if (do_round) {
            pos = len - 1;
            while (pos >= dstart) {
                if (buf[pos] == '.') { pos--; continue; }
                dig = buf[pos] - '0';
                if (dig < 0 || dig > 9) break;
                dig += 1;
                if (dig < 10) {
                    buf[pos] = (char)('0' + dig);
                    break;
                }
                buf[pos] = '0';
                pos--;
            }
            if (pos < dstart) {
                /* 9.99... rounded to 10.0... -> 1.00... and exp10++ */
                int j, dens = 0;
                for (j = dstart; j < len; j++) {
                    if (buf[j] == '.') continue;
                    dens++;
                }
                /* rewrite as 1 + zeros of 0 with same digit count */
                len = dstart;
                buf[len++] = '1';
                if (dens > 1) {
                    buf[len++] = '.';
                    for (j = 1; j < dens; j++) buf[len++] = '0';
                }
                exp10 += 1;
            }
        }
        /* trim trailing zeros in fraction */
        if (ndig > 1) {
            while (len > dstart && buf[len - 1] == '0') len--;
            if (len > dstart && buf[len - 1] == '.') len--;
        }
    }

    pe = exp10;
    {
        char expbuf[40];
        int nexp = snprintf(expbuf, sizeof(expbuf), "e%+d", pe);
        if (nexp < 0 || len + nexp + 1 > cap) {
            size_t ncap = (size_t)(len + nexp + 16);
            size_t *nh = (size_t *)sn_realloc(ctx, hdr, *hdr, sizeof(size_t) + ncap);
            if (!nh) { st = SN_ERR_NOMEM; goto fail_buf; }
            hdr = nh;
            *hdr = sizeof(size_t) + ncap;
            buf = (char *)(void *)(hdr + 1);
            cap = (int)ncap;
        }
        memcpy(buf + len, expbuf, (size_t)nexp);
        len += nexp;
    }
    buf[len] = '\0';
    *out = buf;
    st = SN_OK;
    goto done;

fail_buf:
    if (hdr) sn_free(ctx, hdr, *hdr);
    buf = NULL;
    hdr = NULL;

done:
    sn_value_clear(ctx, &av);
    sn_value_clear(ctx, &ten);
    sn_value_clear(ctx, &one);
    sn_value_clear(ctx, &scale);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &digv);
    sn_value_clear(ctx, &pow10);
    sn_value_clear(ctx, &ln);
    sn_value_clear(ctx, &ln10);
    sn_value_clear(ctx, &expf);
    sn_value_clear(ctx, &floor_e);
    return st;
}

sn_status sn_float_to_str(sn_ctx *ctx, char **out, const sn_value *v)
{
    /* Default multiprec form is hex (bit-accurate, used by cast/re-round). */
    return sn_float_to_str_base(ctx, out, v, 16);
}

/* base 10: decimal free scientific (multiprec-aware).
 * base 16 (or 0): hex binary float 0x1.hhhp+e.
 * other bases currently fall back to hex for multiprec, %.17g for host-width. */
sn_status sn_float_to_str_base(sn_ctx *ctx, char **out, const sn_value *v, int base)
{
    double d;
    sn_status st;
    sn_fpclass c;
    char tmp[128];
    if (!out || !v || v->kind != SN_KIND_FLOAT) return SN_ERR_ARG;
    *out = NULL;
    c = sn_fp_classify(v);
    if (c == SN_FP_NAN) return sn_alloc_cstr(ctx, out, "nan");
    if (c == SN_FP_INFINITE)
        return sn_alloc_cstr(ctx, out, sn_fp_signbit(v) ? "-inf" : "inf");

    if (base <= 0) base = 16;

    if (sn_float_mp_supported(v->e_bits, v->m_bits)) {
        if (base == 10)
            return sn_float_mp_to_decstr(ctx, out, v);
        /* hex or other: bit-accurate hex */
        return sn_float_mp_to_hexstr(ctx, out, v);
    }

    /* Host-width path: double formatting. */
    st = sn_to_double(ctx, v, &d);
    if (st != SN_OK) return st;
    if (base == 16)
        snprintf(tmp, sizeof(tmp), "%a", d);
    else
        snprintf(tmp, sizeof(tmp), "%.17g", d);
    return sn_alloc_cstr(ctx, out, tmp);
}

/* Parse C99 hex float for multiprec: [+-]?0xH[.H*]p[+-]?D */
static int hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static sn_status sn_float_from_hexstr(sn_ctx *ctx, sn_value *out, const char *p,
                                      int e_bits, int m_bits, int nan_enabled,
                                      const sn_op_opt *opt)
{
    sn_status st;
    sn_value mant, scale, sixteen, t, pow2, one;
    int neg = 0, exp_neg = 0, pe = 0, saw_dot = 0, frac_bits = 0, any = 0;
    int d;

    sn_value_init(&mant);
    sn_value_init(&scale);
    sn_value_init(&sixteen);
    sn_value_init(&t);
    sn_value_init(&pow2);
    sn_value_init(&one);

    if (*p == '+' || *p == '-') {
        if (*p == '-') neg = 1;
        p++;
    }
    if (!(p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))) {
        st = SN_ERR_FORMAT;
        goto done;
    }
    p += 2;

    st = sn_float_set_zero(ctx, &mant, 0, e_bits, m_bits, nan_enabled);
    if (st != SN_OK) goto done;
    st = sn_float_from_i64(ctx, &sixteen, 16, e_bits, m_bits, nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_from_i64(ctx, &one, 1, e_bits, m_bits, nan_enabled, opt);
    if (st != SN_OK) goto done;

    /* integer hex digits before optional '.' */
    while ((d = hex_digit((unsigned char)*p)) >= 0) {
        any = 1;
        st = sn_mul(ctx, &mant, &mant, &sixteen, opt); if (st != SN_OK) goto done;
        st = sn_float_from_i64(ctx, &t, (int64_t)d, e_bits, m_bits, nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_add(ctx, &mant, &mant, &t, opt); if (st != SN_OK) goto done;
        p++;
    }
    if (*p == '.') {
        saw_dot = 1;
        p++;
        while ((d = hex_digit((unsigned char)*p)) >= 0) {
            any = 1;
            st = sn_mul(ctx, &mant, &mant, &sixteen, opt); if (st != SN_OK) goto done;
            st = sn_float_from_i64(ctx, &t, (int64_t)d, e_bits, m_bits, nan_enabled, opt);
            if (st != SN_OK) goto done;
            st = sn_add(ctx, &mant, &mant, &t, opt); if (st != SN_OK) goto done;
            frac_bits += 4;
            p++;
        }
    }
    if (!any) { st = SN_ERR_FORMAT; goto done; }
    if (*p != 'p' && *p != 'P') { st = SN_ERR_FORMAT; goto done; }
    p++;
    if (*p == '+' || *p == '-') {
        if (*p == '-') exp_neg = 1;
        p++;
    }
    if (*p < '0' || *p > '9') { st = SN_ERR_FORMAT; goto done; }
    pe = 0;
    while (*p >= '0' && *p <= '9') {
        if (pe > 100000000) { st = SN_ERR_RANGE; goto done; }
        pe = pe * 10 + (*p - '0');
        p++;
    }
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\0') { st = SN_ERR_FORMAT; goto done; }
    if (exp_neg) pe = -pe;
    /* value = mant * 2^(pe - frac_bits)  because each frac hex digit is 4 bits */
    {
        int adj = pe - frac_bits;
        st = sn_ldexp(ctx, &mant, &mant, adj, opt);
        if (st != SN_OK) goto done;
    }
    if (neg) {
        st = sn_neg(ctx, out, &mant, opt);
    } else {
        st = sn_value_copy(ctx, out, &mant);
    }
done:
    sn_value_clear(ctx, &mant);
    sn_value_clear(ctx, &scale);
    sn_value_clear(ctx, &sixteen);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &pow2);
    sn_value_clear(ctx, &one);
    (void)saw_dot;
    return st;
}

/* Fast mag = mag * m + add for decimal digit accumulation. */
static sn_status sn_fstr_mul_add_u32(sn_ctx *ctx, sn_value *mag, uint32_t m, uint32_t add)
{
    sn_status st;
    sn_limb *L;
    int i, n;
    uint64_t carry;

    if (!mag) return SN_ERR_ARG;
    n = mag->nlimbs;
    if (n < 1) n = 1;
    st = sn_value_reserve(ctx, mag, n + 1);
    if (st != SN_OK) return st;
    L = SN_LIMBS(mag);
    carry = add;
    for (i = 0; i < n; i++) {
        uint64_t prod = (uint64_t)L[i] * (uint64_t)m + carry;
        L[i] = (sn_limb)(prod & SN_LIMB_MASK);
        carry = prod >> SN_LIMB_BITS;
    }
    if (carry) {
        L[n] = (sn_limb)carry;
        mag->nlimbs = n + 1;
    } else {
        mag->nlimbs = n;
        while (mag->nlimbs > 1 && L[mag->nlimbs - 1] == 0)
            mag->nlimbs--;
    }
    return SN_OK;
}

/* mag *= 10^k for k >= 0 (binary exponentiation). */
static sn_status sn_fstr_mul_pow10(sn_ctx *ctx, sn_value *mag, int k)
{
    sn_status st;
    sn_value ten, base, tmp, res;
    int e;

    if (k <= 0) return SN_OK;
    sn_value_init(&ten);
    sn_value_init(&base);
    sn_value_init(&tmp);
    sn_value_init(&res);

    ten.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &ten, 1);
    if (st != SN_OK) goto done;
    SN_LIMBS(&ten)[0] = 10;
    ten.nlimbs = 1;

    st = sn_value_copy(ctx, &base, &ten);
    if (st != SN_OK) goto done;
    /* res starts as 1 */
    res.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &res, 1);
    if (st != SN_OK) goto done;
    SN_LIMBS(&res)[0] = 1;
    res.nlimbs = 1;

    e = k;
    while (e > 0) {
        if (e & 1) {
            st = sn_limb_mul(ctx, &tmp, SN_CLIMBS(&res), res.nlimbs,
                             SN_CLIMBS(&base), base.nlimbs);
            if (st != SN_OK) goto done;
            sn_value_clear(ctx, &res);
            sn_value_move(&res, &tmp);
            sn_value_init(&tmp);
            res.kind = SN_KIND_BIGINT;
            sn_bigint_normalize(&res);
        }
        e >>= 1;
        if (e) {
            st = sn_limb_mul(ctx, &tmp, SN_CLIMBS(&base), base.nlimbs,
                             SN_CLIMBS(&base), base.nlimbs);
            if (st != SN_OK) goto done;
            sn_value_clear(ctx, &base);
            sn_value_move(&base, &tmp);
            sn_value_init(&tmp);
            base.kind = SN_KIND_BIGINT;
            sn_bigint_normalize(&base);
        }
    }
    st = sn_limb_mul(ctx, &tmp, SN_CLIMBS(mag), mag->nlimbs,
                     SN_CLIMBS(&res), res.nlimbs);
    if (st != SN_OK) goto done;
    sn_value_clear(ctx, mag);
    sn_value_move(mag, &tmp);
    sn_value_init(&tmp);
    mag->kind = SN_KIND_BIGINT;
    sn_bigint_normalize(mag);
    st = SN_OK;
done:
    sn_value_clear(ctx, &ten);
    sn_value_clear(ctx, &base);
    sn_value_clear(ctx, &tmp);
    sn_value_clear(ctx, &res);
    return st;
}

/* Multiprec decimal: [-]?digits[.digits][eE][+-]exp -> full-precision float.
 * value = (+/-) mag * 10^(exp - frac_digits), converted via BIGINT + *10/+10. */
static sn_status sn_float_from_decstr_mp(sn_ctx *ctx, sn_value *out, const char *p,
                                         int e_bits, int m_bits, int nan_enabled,
                                         const sn_op_opt *opt)
{
    sn_status st;
    sn_value mag, ten, t, pow10, one;
    int neg = 0, exp_neg = 0, pe = 0, frac_digits = 0, any = 0;
    int exp_adj;

    sn_value_init(&mag);
    sn_value_init(&ten);
    sn_value_init(&t);
    sn_value_init(&pow10);
    sn_value_init(&one);

    if (*p == '+' || *p == '-') {
        if (*p == '-') neg = 1;
        p++;
    }

    mag.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &mag, 1);
    if (st != SN_OK) goto done;
    SN_LIMBS(&mag)[0] = 0;
    mag.nlimbs = 1;
    mag.negative = 0;

    /* integer digits */
    while (*p >= '0' && *p <= '9') {
        any = 1;
        st = sn_fstr_mul_add_u32(ctx, &mag, 10u, (uint32_t)(*p - '0'));
        if (st != SN_OK) goto done;
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            any = 1;
            st = sn_fstr_mul_add_u32(ctx, &mag, 10u, (uint32_t)(*p - '0'));
            if (st != SN_OK) goto done;
            frac_digits++;
            p++;
        }
    }
    if (!any) { st = SN_ERR_FORMAT; goto done; }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            if (*p == '-') exp_neg = 1;
            p++;
        }
        if (*p < '0' || *p > '9') { st = SN_ERR_FORMAT; goto done; }
        pe = 0;
        while (*p >= '0' && *p <= '9') {
            if (pe > 100000000) { st = SN_ERR_RANGE; goto done; }
            pe = pe * 10 + (*p - '0');
            p++;
        }
    }
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\0') { st = SN_ERR_FORMAT; goto done; }
    if (exp_neg) pe = -pe;
    exp_adj = pe - frac_digits;

    /* zero */
    if (mag.nlimbs == 1 && SN_CLIMBS(&mag)[0] == 0) {
        st = sn_float_set_zero(ctx, out, neg, e_bits, m_bits, nan_enabled);
        goto done;
    }

    if (exp_adj >= 0) {
        /* mag * 10^exp_adj -> integer -> float */
        st = sn_fstr_mul_pow10(ctx, &mag, exp_adj);
        if (st != SN_OK) goto done;
        if (neg) mag.negative = 1;
        st = sn_float_mp_from_bigint(ctx, out, &mag, e_bits, m_bits, nan_enabled, opt);
        goto done;
    }

    /* mag / 10^(-exp_adj): work at elevated m so digit string is not
     * truncated before the decimal scale is applied, then re-round to target. */
    {
        int k = -exp_adj;
        int m_work, bl, need;
        char *hs = NULL;
        sn_value base, tmp, res;

        /* bit length of mag (approx) */
        bl = 0;
        if (mag.nlimbs > 0) {
            sn_limb top = SN_CLIMBS(&mag)[mag.nlimbs - 1];
            int b;
            bl = (mag.nlimbs - 1) * SN_LIMB_BITS;
            for (b = SN_LIMB_BITS - 1; b >= 0; b--)
                if ((top >> b) & 1u) { bl += b + 1; break; }
        }
        /* 10^k ~ 2^(k*log2(10)); keep guard beyond target + mag. */
        need = m_bits + 16 + (k * 3322 + 999) / 1000; /* log2(10)~3.322 */
        if (bl + 16 > need) need = bl + 16;
        m_work = need;
        if (m_work < m_bits + 8) m_work = m_bits + 8;
        if (m_work > SN_FLOAT_M_MAX) m_work = SN_FLOAT_M_MAX;
        /* stay multiprec path */
        if (m_work <= 52) m_work = 53;

        if (neg) mag.negative = 1;
        st = sn_float_mp_from_bigint(ctx, &t, &mag, e_bits, m_work, nan_enabled, opt);
        if (st != SN_OK) goto done;

        st = sn_float_from_i64(ctx, &ten, 10, e_bits, m_work, nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_float_from_i64(ctx, &one, 1, e_bits, m_work, nan_enabled, opt);
        if (st != SN_OK) goto done;

        sn_value_init(&base);
        sn_value_init(&tmp);
        sn_value_init(&res);
        st = sn_value_copy(ctx, &base, &ten);
        if (st != SN_OK) goto scale_done;
        st = sn_value_copy(ctx, &res, &one);
        if (st != SN_OK) goto scale_done;
        {
            int e = k;
            while (e > 0) {
                if (e & 1) {
                    st = sn_mul(ctx, &tmp, &res, &base, opt);
                    if (st != SN_OK) break;
                    sn_value_clear(ctx, &res);
                    sn_value_move(&res, &tmp);
                    sn_value_init(&tmp);
                }
                e >>= 1;
                if (e) {
                    st = sn_mul(ctx, &tmp, &base, &base, opt);
                    if (st != SN_OK) break;
                    sn_value_clear(ctx, &base);
                    sn_value_move(&base, &tmp);
                    sn_value_init(&tmp);
                }
            }
        }
        if (st == SN_OK)
            st = sn_div(ctx, &pow10, &t, &res, opt); /* reuse pow10 as result @ m_work */
        if (st != SN_OK) goto scale_done;

        if (m_work == m_bits) {
            st = sn_value_copy(ctx, out, &pow10);
        } else {
            /* Re-round to target format via full-precision hex (no double). */
            st = sn_float_to_str(ctx, &hs, &pow10);
            if (st == SN_OK && hs)
                st = sn_float_from_hexstr(ctx, out, hs, e_bits, m_bits, nan_enabled, opt);
            if (hs) sn_str_free(ctx, hs);
        }
scale_done:
        sn_value_clear(ctx, &base);
        sn_value_clear(ctx, &tmp);
        sn_value_clear(ctx, &res);
        if (st != SN_OK) goto done;
    }

done:
    sn_value_clear(ctx, &mag);
    sn_value_clear(ctx, &ten);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &pow10);
    sn_value_clear(ctx, &one);
    return st;
}

sn_status sn_float_from_str(sn_ctx *ctx, sn_value *out, const char *s,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt)
{
    char *end = NULL;
    double d;
    const char *p;
    int neg = 0;
    if (!s) return SN_ERR_ARG;
    p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return SN_ERR_FORMAT;
    if (strncmp(p, "nan", 3) == 0 || strncmp(p, "NaN", 3) == 0 || strncmp(p, "NAN", 3) == 0) {
        if (!nan_enabled) {
            sn_raise(ctx, SN_FLAG_INVALID);
            return sn_float_set_inf(ctx, out, 0, e_bits, m_bits, nan_enabled);
        }
        return sn_float_set_nan(ctx, out, e_bits, m_bits);
    }
    {
        const char *q = p;
        if (*q == '+' || *q == '-') { if (*q == '-') neg = 1; q++; }
        if (strncmp(q, "inf", 3) == 0 || strncmp(q, "Inf", 3) == 0 || strncmp(q, "INF", 3) == 0)
            return sn_float_set_inf(ctx, out, neg, e_bits, m_bits, nan_enabled);
    }
    /* Multiprec hex floats: full trailing significand. */
    {
        const char *q = p;
        if (*q == '+' || *q == '-') q++;
        if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X') && sn_float_mp_supported(e_bits, m_bits))
            return sn_float_from_hexstr(ctx, out, p, e_bits, m_bits, nan_enabled, opt);
    }
    /* Multiprec decimal: full digit precision (no strtod truncation). */
    if (sn_float_mp_supported(e_bits, m_bits))
        return sn_float_from_decstr_mp(ctx, out, p, e_bits, m_bits, nan_enabled, opt);
    d = strtod(p, &end);
    if (end == p) return SN_ERR_FORMAT;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return SN_ERR_FORMAT;
    return sn_float_from_double(ctx, out, d, e_bits, m_bits, nan_enabled, opt);
}

sn_status sn_from_str_float(sn_ctx *ctx, sn_value *out, const char *s,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt)
{
    return sn_float_from_str(ctx, out, s, e_bits, m_bits, nan_enabled, opt);
}

sn_status sn_float_from_double(sn_ctx *ctx, sn_value *out, double x,
                               int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt)
{
    if (sn_float_mp_supported(e_bits, m_bits))
        return sn_float_mp_from_double(ctx, out, x, e_bits, m_bits, nan_enabled, opt);
    return encode_from_double(ctx, out, x, e_bits, m_bits, nan_enabled, sn_eff_round(ctx, opt));
}

sn_status sn_totalorder(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b)
{
    int sa, sb, i, na, nb, n;
    const sn_limb *La, *Lb;
    (void)ctx;
    if (!rel || !a || !b) return SN_ERR_ARG;
    if (a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;

    /* IEEE totalOrder on raw encodings: compare as sign-magnitude order.
     * Negative numbers ordered by reverse bit pattern magnitude. */
    sa = sn_fp_signbit(a);
    sb = sn_fp_signbit(b);
    if (sa != sb) {
        *rel = sa ? -1 : 1; /* negative < positive */
        return SN_OK;
    }
    La = SN_CLIMBS(a); Lb = SN_CLIMBS(b);
    na = a->nlimbs; nb = b->nlimbs;
    n = na > nb ? na : nb;
    /* compare absolute bit payload without sign bit */
    for (i = n - 1; i >= 0; i--) {
        sn_limb xa = (i < na) ? La[i] : 0;
        sn_limb xb = (i < nb) ? Lb[i] : 0;
        /* clear sign bit if it lives in top limb */
        if (i == sn_limbs_for_bits(a->width) - 1) {
            int sbit = (a->width - 1) % SN_LIMB_BITS;
            sn_limb mask = ~((sn_limb)1u << sbit);
            if (a->width % SN_LIMB_BITS != 0 || a->width > 0) {
                xa &= mask;
                xb &= mask;
            }
        }
        if (xa != xb) {
            int cmp = (xa < xb) ? -1 : 1;
            *rel = sa ? -cmp : cmp; /* if negative, reverse */
            return SN_OK;
        }
    }
    *rel = 0;
    return SN_OK;
}
