/*
 * Multiprec soft float: arbitrary E/M (memory-bound).
 * Used when m_bits > 52 OR e_bits > 30 (narrow path cannot hold exp in uint32).
 * Encoding: sign | exp(e_bits) | mant(m_bits), multi-limb LE.
 */
#include "internal/sn_impl.h"
#include <math.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#ifndef SN_FLOAT_E_MAX
#define SN_FLOAT_E_MAX (INT_MAX / 4)
#endif
#ifndef SN_FLOAT_M_MAX
#define SN_FLOAT_M_MAX (INT_MAX / 4)
#endif
/* Keep old name as alias for existing comments/docs */
#define SN_FMP_M_MAX SN_FLOAT_M_MAX

static void bit_set(sn_value *v, int bit, int on);
static int bit_get(const sn_value *v, int bit);

int sn_float_mp_supported(int e_bits, int m_bits)
{
    int64_t tb;
    if (e_bits < 2 || e_bits > SN_FLOAT_E_MAX) return 0;
    if (m_bits < 1 || m_bits > SN_FLOAT_M_MAX) return 0;
    tb = (int64_t)1 + (int64_t)e_bits + (int64_t)m_bits;
    if (tb < 3 || tb > (int64_t)INT_MAX) return 0;
    /* Multiprec when not on the narrow fast path (m<=52 && e<=30). */
    if (m_bits <= 52 && e_bits <= 30) return 0;
    return 1;
}

/* Bias / all-ones exp as int64. e_bits must be in [2, 62] for exact 2^e-1 in int64.
 * For e_bits >= 63, pack/unpack uses all-ones bit loops without computing 2^e. */
static int64_t fmp_bias64(int e_bits)
{
    if (e_bits < 2 || e_bits > 63) return 0;
    return ((int64_t)1 << (e_bits - 1)) - 1;
}

static int64_t fmp_exp_max64(int e_bits)
{
    if (e_bits < 1) return 0;
    if (e_bits >= 63) return INT64_MAX; /* sentinel: treat comparisons carefully */
    return ((int64_t)1 << e_bits) - 1;
}

static int fmp_total(int e_bits, int m_bits) { return 1 + e_bits + m_bits; }

/* Write exp field bits from low e_bits of a wide value (limb array). */
static void fmp_write_exp_u64(sn_value *out, int m_bits, int e_bits, uint64_t expf)
{
    int i;
    for (i = 0; i < e_bits; i++)
        bit_set(out, m_bits + i, (int)((expf >> i) & 1ull));
}

static void fmp_write_exp_all_ones(sn_value *out, int m_bits, int e_bits)
{
    int i;
    for (i = 0; i < e_bits; i++)
        bit_set(out, m_bits + i, 1);
}

/* Read encoded exp field into uint64 if e_bits <= 64, else only low 64 (rare path uses all-ones check). */
static uint64_t fmp_read_exp_u64(const sn_value *v, int m_bits, int e_bits)
{
    uint64_t expf = 0;
    int i, lim = e_bits < 64 ? e_bits : 64;
    for (i = 0; i < lim; i++)
        if (bit_get(v, m_bits + i)) expf |= (uint64_t)1 << i;
    return expf;
}

static int fmp_exp_is_all_ones(const sn_value *v, int m_bits, int e_bits)
{
    int i;
    for (i = 0; i < e_bits; i++)
        if (!bit_get(v, m_bits + i)) return 0;
    return 1;
}

static int fmp_exp_is_zero(const sn_value *v, int m_bits, int e_bits)
{
    int i;
    for (i = 0; i < e_bits; i++)
        if (bit_get(v, m_bits + i)) return 0;
    return 1;
}

static sn_round fmp_rnd(const sn_ctx *ctx, const sn_op_opt *opt)
{
    if (opt && opt->has_round) return opt->round;
    return ctx ? ctx->round : SN_ROUND_NTE;
}

static int fmp_round_up(sn_round rnd, int sign, int lsb, int guard, int sticky)
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

static void bit_set(sn_value *v, int bit, int on)
{
    sn_limb *L = SN_LIMBS(v);
    int li = bit / SN_LIMB_BITS, off = bit % SN_LIMB_BITS;
    if (li < 0 || li >= v->nlimbs) return;
    if (on) L[li] |= (sn_limb)1u << off;
    else L[li] &= ~((sn_limb)1u << off);
}

static int bit_get(const sn_value *v, int bit)
{
    const sn_limb *L = SN_CLIMBS(v);
    int li = bit / SN_LIMB_BITS, off = bit % SN_LIMB_BITS;
    if (li < 0 || li >= v->nlimbs) return 0;
    return (int)((L[li] >> off) & 1u);
}

static int sig_bitlen(const sn_value *s)
{
    int n = s->nlimbs;
    const sn_limb *L = SN_CLIMBS(s);
    int b;
    while (n > 1 && L[n - 1] == 0) n--;
    if (n <= 0 || (n == 1 && L[0] == 0)) return 0;
    for (b = SN_LIMB_BITS - 1; b >= 0; b--)
        if ((L[n - 1] >> b) & 1u)
            return (n - 1) * SN_LIMB_BITS + b + 1;
    return (n - 1) * SN_LIMB_BITS;
}

static int sig_getbit(const sn_value *s, int bit)
{
    const sn_limb *L = SN_CLIMBS(s);
    int li = bit / SN_LIMB_BITS, off = bit % SN_LIMB_BITS;
    if (bit < 0 || li >= s->nlimbs) return 0;
    return (int)((L[li] >> off) & 1u);
}

static sn_status sig_ensure(sn_ctx *ctx, sn_value *s, int nlimbs)
{
    sn_status st;
    sn_limb *L;
    int i;
    if (nlimbs < 1) nlimbs = 1;
    st = sn_value_reserve(ctx, s, nlimbs);
    if (st != SN_OK) return st;
    L = SN_LIMBS(s);
    for (i = s->nlimbs; i < nlimbs; i++) L[i] = 0;
    if (s->nlimbs < nlimbs) s->nlimbs = nlimbs;
    s->kind = SN_KIND_BIGINT;
    s->negative = 0;
    return SN_OK;
}

static sn_status sig_setbit(sn_ctx *ctx, sn_value *s, int bit, int on)
{
    sn_status st = sig_ensure(ctx, s, sn_limbs_for_bits(bit + 1));
    sn_limb *L;
    int li, off;
    if (st != SN_OK) return st;
    L = SN_LIMBS(s);
    li = bit / SN_LIMB_BITS;
    off = bit % SN_LIMB_BITS;
    if (on) L[li] |= (sn_limb)1u << off;
    else L[li] &= ~((sn_limb)1u << off);
    return SN_OK;
}

static sn_status sig_zero(sn_ctx *ctx, sn_value *s)
{
    sn_status st = sig_ensure(ctx, s, 1);
    if (st != SN_OK) return st;
    SN_LIMBS(s)[0] = 0;
    s->nlimbs = 1;
    return SN_OK;
}

static sn_status sig_shl(sn_ctx *ctx, sn_value *s, int64_t k)
{
    sn_value tmp;
    sn_status st;
    int limb_shift, bit_shift, n, i;
    const sn_limb *src;
    sn_limb *dst;
    if (k <= 0) return SN_OK;
    if (sig_bitlen(s) == 0) return SN_OK;
    /* Shift beyond what int limb counts can represent => OOM/unsupported */
    if (k > (int64_t)INT_MAX - SN_LIMB_BITS) return SN_ERR_NOMEM;
    limb_shift = (int)(k / SN_LIMB_BITS);
    bit_shift = (int)(k % SN_LIMB_BITS);
    if (s->nlimbs > INT_MAX - limb_shift - 1) return SN_ERR_NOMEM;
    n = s->nlimbs + limb_shift + (bit_shift ? 1 : 0);
    sn_value_init(&tmp);
    st = sn_value_reserve(ctx, &tmp, n);
    if (st != SN_OK) return st;
    dst = SN_LIMBS(&tmp);
    for (i = 0; i < n; i++) dst[i] = 0;
    src = SN_CLIMBS(s);
    if (bit_shift == 0) {
        for (i = 0; i < s->nlimbs; i++) dst[i + limb_shift] = src[i];
    } else {
        sn_limb carry = 0;
        for (i = 0; i < s->nlimbs; i++) {
            sn_limb x = src[i];
            dst[i + limb_shift] = (x << bit_shift) | carry;
            carry = x >> (SN_LIMB_BITS - bit_shift);
        }
        dst[s->nlimbs + limb_shift] = carry;
    }
    tmp.nlimbs = n;
    tmp.kind = SN_KIND_BIGINT;
    tmp.negative = 0;
    sn_bigint_normalize(&tmp);
    sn_value_clear(ctx, s);
    sn_value_move(s, &tmp);
    return SN_OK;
}

static sn_status sig_shr_sticky(sn_ctx *ctx, sn_value *s, int64_t k, int *sticky)
{
    sn_value tmp;
    sn_status st;
    int i, limb_shift, bit_shift, n, bl;
    const sn_limb *src;
    sn_limb *dst;
    int stky = sticky ? *sticky : 0;
    if (k <= 0) return SN_OK;
    bl = sig_bitlen(s);
    if (k >= (int64_t)bl) {
        if (bl > 0) stky = 1;
        st = sig_zero(ctx, s);
        if (sticky) *sticky = stky;
        return st;
    }
    /* sticky: any of low k bits set (limb-wise). Skip when caller only wants pure shift. */
    if (sticky && !stky) {
        int64_t rem = k;
        i = 0;
        while (rem > 0 && i < s->nlimbs) {
            sn_limb w = SN_CLIMBS(s)[i];
            if (rem >= SN_LIMB_BITS) {
                if (w) { stky = 1; break; }
                rem -= SN_LIMB_BITS;
            } else {
                sn_limb mask = ((sn_limb)1 << (int)rem) - 1u;
                if (w & mask) stky = 1;
                break;
            }
            i++;
        }
    }
    limb_shift = (int)(k / SN_LIMB_BITS);
    bit_shift = (int)(k % SN_LIMB_BITS);
    n = s->nlimbs - limb_shift;
    if (n < 1) n = 1;
    sn_value_init(&tmp);
    st = sn_value_reserve(ctx, &tmp, n);
    if (st != SN_OK) return st;
    dst = SN_LIMBS(&tmp);
    src = SN_CLIMBS(s);
    for (i = 0; i < n; i++) dst[i] = 0;
    if (bit_shift == 0) {
        for (i = 0; i < n; i++) {
            int si = i + limb_shift;
            dst[i] = (si < s->nlimbs) ? src[si] : 0;
        }
    } else {
        for (i = 0; i < n; i++) {
            int si = i + limb_shift;
            sn_limb lo = (si < s->nlimbs) ? src[si] : 0;
            sn_limb hi = (si + 1 < s->nlimbs) ? src[si + 1] : 0;
            dst[i] = (lo >> bit_shift) | (hi << (SN_LIMB_BITS - bit_shift));
        }
    }
    tmp.nlimbs = n;
    tmp.kind = SN_KIND_BIGINT;
    tmp.negative = 0;
    sn_bigint_normalize(&tmp);
    sn_value_clear(ctx, s);
    sn_value_move(s, &tmp);
    if (sticky) *sticky = stky;
    return SN_OK;
}

/* True if any of bits [0 .. k-1] of s is set. Limb-wise O(nlimbs). */
static int sig_low_any(const sn_value *s, int64_t k)
{
    int i;
    int64_t rem;
    if (!s || k <= 0) return 0;
    if (k >= (int64_t)sig_bitlen(s) && sig_bitlen(s) > 0) return 1;
    rem = k;
    i = 0;
    while (rem > 0 && i < s->nlimbs) {
        sn_limb w = SN_CLIMBS(s)[i];
        if (rem >= SN_LIMB_BITS) {
            if (w) return 1;
            rem -= SN_LIMB_BITS;
        } else {
            sn_limb mask = ((sn_limb)1 << (int)rem) - 1u;
            return (w & mask) != 0;
        }
        i++;
    }
    return 0;
}

static sn_status sig_add1(sn_ctx *ctx, sn_value *s)
{
    sn_value one, r;
    sn_status st;
    sn_value_init(&one);
    sn_value_init(&r);
    st = sig_zero(ctx, &one);
    if (st != SN_OK) return st;
    SN_LIMBS(&one)[0] = 1;
    st = sn_limb_add(ctx, &r, SN_CLIMBS(s), s->nlimbs, SN_CLIMBS(&one), 1);
    sn_value_clear(ctx, &one);
    if (st != SN_OK) return st;
    sn_value_clear(ctx, s);
    sn_value_move(s, &r);
    return SN_OK;
}

typedef struct sn_fmp {
    int sign, is_zero, is_inf, is_nan, sticky;
    int64_t exp;
    sn_value sig;
} sn_fmp;

static void fmp_init(sn_fmp *u)
{
    memset(u, 0, sizeof(*u));
    sn_value_init(&u->sig);
}

static void fmp_clear(sn_ctx *ctx, sn_fmp *u)
{
    sn_value_clear(ctx, &u->sig);
    memset(u, 0, sizeof(*u));
    sn_value_init(&u->sig);
}

static sn_status fmp_copy(sn_ctx *ctx, sn_fmp *d, const sn_fmp *s)
{
    sn_status st;
    fmp_clear(ctx, d);
    d->sign = s->sign;
    d->exp = s->exp;
    d->is_zero = s->is_zero;
    d->is_inf = s->is_inf;
    d->is_nan = s->is_nan;
    d->sticky = s->sticky;
    if (!s->is_zero && !s->is_inf && !s->is_nan && sig_bitlen(&s->sig) > 0) {
        st = sn_value_copy(ctx, &d->sig, &s->sig);
        if (st != SN_OK) return st;
    }
    return SN_OK;
}

static sn_status fmp_write(sn_ctx *ctx, sn_value *out, int sign, uint64_t expf,
                           const sn_limb *mant, int nmant, int e_bits, int m_bits, int nan_en)
{
    sn_status st;
    int tb, n, i;
    sn_limb *L;
    st = sn_float_new(ctx, out, e_bits, m_bits, nan_en);
    if (st != SN_OK) return st;
    tb = fmp_total(e_bits, m_bits);
    n = sn_limbs_for_bits(tb);
    L = SN_LIMBS(out);
    for (i = 0; i < n; i++) L[i] = 0;
    for (i = 0; i < m_bits; i++) {
        int li = i / SN_LIMB_BITS, off = i % SN_LIMB_BITS;
        if (li < nmant && ((mant[li] >> off) & 1u)) bit_set(out, i, 1);
    }
    if (e_bits <= 64)
        fmp_write_exp_u64(out, m_bits, e_bits, expf);
    else {
        /* only low 64 bits of expf written; callers for all-ones use fmp_write_exp_all_ones path */
        fmp_write_exp_u64(out, m_bits, 64, expf);
        for (i = 64; i < e_bits; i++) bit_set(out, m_bits + i, 0);
    }
    if (sign) bit_set(out, tb - 1, 1);
    out->nlimbs = n;
    return SN_OK;
}

/* Write with all-ones exp (Inf/NaN), any e_bits. */
static sn_status fmp_write_expmax(sn_ctx *ctx, sn_value *out, int sign,
                                  const sn_limb *mant, int nmant, int e_bits, int m_bits, int nan_en)
{
    sn_status st;
    int tb, n, i;
    sn_limb *L;
    st = sn_float_new(ctx, out, e_bits, m_bits, nan_en);
    if (st != SN_OK) return st;
    tb = fmp_total(e_bits, m_bits);
    n = sn_limbs_for_bits(tb);
    L = SN_LIMBS(out);
    for (i = 0; i < n; i++) L[i] = 0;
    for (i = 0; i < m_bits; i++) {
        int li = i / SN_LIMB_BITS, off = i % SN_LIMB_BITS;
        if (li < nmant && ((mant[li] >> off) & 1u)) bit_set(out, i, 1);
    }
    fmp_write_exp_all_ones(out, m_bits, e_bits);
    if (sign) bit_set(out, tb - 1, 1);
    out->nlimbs = n;
    return SN_OK;
}

static sn_status fmp_unpack(sn_ctx *ctx, const sn_value *v, sn_fmp *u)
{
    int e_bits = v->e_bits, m_bits = v->m_bits;
    int64_t bias = fmp_bias64(e_bits);
    int tb = fmp_total(e_bits, m_bits);
    uint64_t expf = 0;
    int i, mant_nz = 0;
    sn_status st;
    int64_t exp_i;

    fmp_init(u);
    u->sign = bit_get(v, tb - 1);

    st = sig_zero(ctx, &u->sig);
    if (st != SN_OK) return st;
    for (i = 0; i < m_bits; i++) {
        if (bit_get(v, i)) {
            mant_nz = 1;
            st = sig_setbit(ctx, &u->sig, i, 1);
            if (st != SN_OK) return st;
        }
    }

    if (fmp_exp_is_all_ones(v, m_bits, e_bits)) {
        if (mant_nz) u->is_nan = 1;
        else u->is_inf = 1;
        return SN_OK;
    }
    if (fmp_exp_is_zero(v, m_bits, e_bits)) {
        if (!mant_nz) { u->is_zero = 1; return SN_OK; }
        /* subnormal: working exp = 1 - bias - m_bits */
        if (e_bits <= 62) {
            u->exp = (int64_t)1 - bias - (int64_t)m_bits;
        } else if (e_bits <= 64) {
            /* bias = 2^(e-1)-1 fits uint64; result is largely negative */
            uint64_t ubias = (e_bits == 64)
                ? (UINT64_C(0x7fffffffffffffff)) /* 2^63-1 */
                : (((uint64_t)1 << (e_bits - 1)) - 1u);
            if (ubias >= (uint64_t)INT64_MAX)
                u->exp = INT64_MIN / 4;
            else
                u->exp = (int64_t)1 - (int64_t)ubias - (int64_t)m_bits;
        } else {
            /* bias exceeds int64; subnormals sit at extreme min */
            u->exp = INT64_MIN / 4;
        }
        return SN_OK;
    }

    st = sig_setbit(ctx, &u->sig, m_bits, 1);
    if (st != SN_OK) return st;

    /* Normal: working exp = exp_field - bias - m_bits.
     * For e_bits<=62, bias/exp_field fit int64.
     * For wider e: exp_field = H*2^(e-1) + L, bias = 2^(e-1)-1
     *   uexp = (H-1)*2^(e-1) + L + 1
     *   H=1 => uexp = L+1
     *   H=0 => uexp = L+1-2^(e-1)
     * This replaces the old bug that treated any exp bit >=62 as Inf. */
    if (e_bits <= 62) {
        expf = fmp_read_exp_u64(v, m_bits, e_bits);
        exp_i = (int64_t)expf - bias - (int64_t)m_bits;
        u->exp = exp_i;
        return SN_OK;
    }

    {
        int H = bit_get(v, m_bits + e_bits - 1);
        int lim = e_bits - 1; /* L uses bits [0..e-2] */
        uint64_t Llo = 0;
        int hi_set = 0;

        for (i = 0; i < lim; i++) {
            if (bit_get(v, m_bits + i)) {
                if (i < 63) Llo |= (uint64_t)1 << i;
                else hi_set = 1;
            }
        }

        if (H) {
            /* uexp = L + 1 */
            if (hi_set || Llo == UINT64_MAX) {
                u->exp = INT64_MAX / 4;
            } else {
                u->exp = (int64_t)(Llo + 1u) - (int64_t)m_bits;
            }
        } else {
            /* uexp = L + 1 - 2^(e-1) = -((2^(e-1)-1) - L) */
            if (e_bits <= 64) {
                uint64_t ubias = (e_bits == 64)
                    ? UINT64_C(0x7fffffffffffffff)
                    : (((uint64_t)1 << (e_bits - 1)) - 1u);
                if (Llo > ubias) {
                    u->exp = INT64_MAX / 4;
                } else {
                    int64_t uexp;
                    uint64_t delta = ubias - Llo;
                    if (delta > (uint64_t)INT64_MAX)
                        uexp = INT64_MIN / 4;
                    else
                        uexp = -(int64_t)delta;
                    u->exp = uexp - (int64_t)m_bits;
                }
            } else {
                /* e>64: only near-bias (high L bits all 1) yields int64-range uexp */
                int near_bias = 1;
                for (i = 63; i < lim; i++) {
                    if (!bit_get(v, m_bits + i)) { near_bias = 0; break; }
                }
                if (!near_bias) {
                    u->exp = INT64_MIN / 4;
                } else {
                    /* bits 63..e-2 of L are 1 => delta = (2^63-1) - Llo */
                    uint64_t max63 = (UINT64_C(1) << 63) - 1u;
                    int64_t uexp = (int64_t)Llo - (int64_t)max63;
                    u->exp = uexp - (int64_t)m_bits;
                }
            }
        }
    }
    return SN_OK;
}

static sn_status fmp_pack(sn_ctx *ctx, sn_value *out, sn_fmp *u,
                          int e_bits, int m_bits, int nan_en, sn_round rnd)
{
    sn_status st;
    int64_t bias, exp_max;
    int bl, target, sticky, guard, lsb, i, nm;
    int64_t shift;
    int64_t uexp64;
    uint64_t expf;
    sn_value mant;
    sn_limb *M;
    sn_limb z = 0;

    if (e_bits < 2 || e_bits > SN_FLOAT_E_MAX || m_bits < 1 || m_bits > SN_FLOAT_M_MAX)
        return SN_ERR_RANGE;
    bias = fmp_bias64(e_bits);
    exp_max = fmp_exp_max64(e_bits);

    if (u->is_nan) {
        if (!nan_en) {
            sn_raise(ctx, SN_FLAG_INVALID);
            return fmp_write_expmax(ctx, out, u->sign, &z, 0, e_bits, m_bits, nan_en);
        }
        sn_value_init(&mant);
        st = sig_zero(ctx, &mant);
        if (st != SN_OK) return st;
        if (m_bits > 0) {
            st = sig_setbit(ctx, &mant, m_bits - 1, 1);
            if (st != SN_OK) { sn_value_clear(ctx, &mant); return st; }
        } else {
            SN_LIMBS(&mant)[0] = 1;
        }
        st = fmp_write_expmax(ctx, out, u->sign, SN_CLIMBS(&mant), mant.nlimbs,
                              e_bits, m_bits, nan_en);
        sn_value_clear(ctx, &mant);
        return st;
    }
    if (u->is_inf)
        return fmp_write_expmax(ctx, out, u->sign, &z, 0, e_bits, m_bits, nan_en);
    if (u->is_zero || sig_bitlen(&u->sig) == 0) {
        /* cancelled/underflow residual sticky still counts as inexact (softfp) */
        if (u->sticky) sn_raise(ctx, SN_FLAG_INEXACT);
        return fmp_write(ctx, out, u->sign, 0, &z, 0, e_bits, m_bits, nan_en);
    }

    sticky = u->sticky;
    guard = 0;
    target = m_bits + 1;
    bl = sig_bitlen(&u->sig);

    /* Normalize to m_bits+1 integer significand WITHOUT rounding.
     * Softfp-style: denorm decision happens before any RNE (no double-round). */
    if (bl < target) {
        st = sig_shl(ctx, &u->sig, target - bl);
        if (st != SN_OK) return st;
        u->exp -= (target - bl);
    } else if (bl > target) {
        int excess = bl - target;
        /* bits [0 .. excess-2] -> sticky; bit [excess-1] -> guard; pure shr excess */
        if (excess >= 2) {
            if (sig_low_any(&u->sig, (int64_t)excess - 1)) sticky = 1;
        }
        guard = sig_getbit(&u->sig, excess - 1);
        st = sig_shr_sticky(ctx, &u->sig, excess, NULL); /* pure shift */
        if (st != SN_OK) return st;
        u->exp += excess;
    }
    /* else: exact width; guard stays 0, sticky is residual from ops */

    uexp64 = (int64_t)u->exp + (int64_t)m_bits;
    {
        int is_ovf = 0;
        int is_unf = 0;
        if (e_bits <= 62) {
            if (uexp64 + bias >= exp_max) is_ovf = 1;
            else if (uexp64 + bias <= 0) is_unf = 1;
        } else {
            if (u->exp > INT64_MAX / 8) is_ovf = 1;
            else if (u->exp < INT64_MIN / 8) is_unf = 1;
            else if (e_bits <= 64) {
                uint64_t ubias = ((uint64_t)1 << (e_bits - 1)) - 1u;
                uint64_t umax = (e_bits == 64) ? ~0ull : (((uint64_t)1 << e_bits) - 1u);
                if (uexp64 >= 0 && (uint64_t)uexp64 + ubias >= umax) is_ovf = 1;
                if (uexp64 < 0 || (uint64_t)(uexp64 + (int64_t)ubias) == 0) {
                    if (uexp64 + (int64_t)ubias <= 0) is_unf = 1;
                }
            }
        }

        /* ---- subnormal: shift first, then single round (softfp round_pack_sf) ---- */
        if (is_unf) {
            if (e_bits <= 62)
                shift = 1 - bias - uexp64;
            else if (e_bits <= 64) {
                uint64_t ubias = ((uint64_t)1 << (e_bits - 1)) - 1u;
                shift = 1 - (int64_t)ubias - uexp64;
            } else {
                shift = (int64_t)INT_MAX / 4;
            }
            if (shift < 0) shift = (int64_t)INT_MAX / 4;

            /* fold existing GRS into sticky, then denorm-shift with new G/S */
            sticky |= guard;
            guard = 0;
            if (shift > 0) {
                if (shift >= (int64_t)sig_bitlen(&u->sig) + 2) {
                    /* everything becomes sticky residue */
                    if (sig_bitlen(&u->sig) > 0 || sticky) sticky = 1;
                    st = sig_zero(ctx, &u->sig);
                    if (st != SN_OK) return st;
                    guard = 0;
                } else {
                    /* shift-1 pure bits into sticky, last bit is guard */
                    if (shift >= 2) {
                        if (sig_low_any(&u->sig, shift - 1)) sticky = 1;
                    }
                    guard = sig_getbit(&u->sig, (int)shift - 1);
                    st = sig_shr_sticky(ctx, &u->sig, shift, NULL);
                    if (st != SN_OK) return st;
                }
            }
            lsb = sig_getbit(&u->sig, 0);
            if (fmp_round_up(rnd, u->sign, lsb, guard, sticky)) {
                st = sig_add1(ctx, &u->sig);
                if (st != SN_OK) return st;
                if (sig_getbit(&u->sig, m_bits)) {
                    st = sig_setbit(ctx, &u->sig, m_bits, 0);
                    if (st != SN_OK) return st;
                    sn_raise(ctx, SN_FLAG_UNDERFLOW | SN_FLAG_INEXACT);
                    return fmp_write(ctx, out, u->sign, 1u, SN_CLIMBS(&u->sig), u->sig.nlimbs,
                                     e_bits, m_bits, nan_en);
                }
            }
            if (guard || sticky)
                sn_raise(ctx, SN_FLAG_UNDERFLOW | SN_FLAG_INEXACT);
            else if (sig_bitlen(&u->sig) > 0)
                sn_raise(ctx, SN_FLAG_UNDERFLOW);
            return fmp_write(ctx, out, u->sign, 0, SN_CLIMBS(&u->sig), u->sig.nlimbs,
                             e_bits, m_bits, nan_en);
        }

        /* ---- normal: single round ---- */
        lsb = sig_getbit(&u->sig, 0);
        if (fmp_round_up(rnd, u->sign, lsb, guard, sticky)) {
            st = sig_add1(ctx, &u->sig);
            if (st != SN_OK) return st;
            if (sig_bitlen(&u->sig) > target) {
                st = sig_shr_sticky(ctx, &u->sig, 1, NULL);
                if (st != SN_OK) return st;
                u->exp += 1;
                uexp64 = (int64_t)u->exp + (int64_t)m_bits;
                /* recompute overflow after carry-out */
                if (e_bits <= 62) {
                    if (uexp64 + bias >= exp_max) is_ovf = 1;
                } else if (e_bits <= 64) {
                    uint64_t ubias = ((uint64_t)1 << (e_bits - 1)) - 1u;
                    uint64_t umax = (e_bits == 64) ? ~0ull : (((uint64_t)1 << e_bits) - 1u);
                    if (uexp64 >= 0 && (uint64_t)uexp64 + ubias >= umax) is_ovf = 1;
                }
            }
        }
        if (guard || sticky) sn_raise(ctx, SN_FLAG_INEXACT);

        if (is_ovf) {
            sn_raise(ctx, SN_FLAG_OVERFLOW | SN_FLAG_INEXACT);
            if (rnd == SN_ROUND_TZ ||
                (rnd == SN_ROUND_DN && u->sign == 0) ||
                (rnd == SN_ROUND_UP && u->sign != 0)) {
                sn_value_init(&mant);
                st = sig_ensure(ctx, &mant, sn_limbs_for_bits(m_bits));
                if (st != SN_OK) return st;
                M = SN_LIMBS(&mant);
                nm = sn_limbs_for_bits(m_bits);
                for (i = 0; i < nm; i++) M[i] = SN_LIMB_MASK;
                if (m_bits % SN_LIMB_BITS)
                    M[nm - 1] &= (sn_limb)((1u << (m_bits % SN_LIMB_BITS)) - 1u);
                mant.nlimbs = nm;
                if (e_bits <= 62) {
                    st = fmp_write(ctx, out, u->sign, (uint64_t)(exp_max - 1),
                                   SN_CLIMBS(&mant), nm, e_bits, m_bits, nan_en);
                } else if (e_bits <= 64) {
                    uint64_t umax = (e_bits == 64) ? ~0ull : (((uint64_t)1 << e_bits) - 1u);
                    st = fmp_write(ctx, out, u->sign, umax - 1u,
                                   SN_CLIMBS(&mant), nm, e_bits, m_bits, nan_en);
                } else {
                    st = fmp_write_expmax(ctx, out, u->sign, SN_CLIMBS(&mant), nm, e_bits, m_bits, nan_en);
                    if (st == SN_OK) bit_set(out, m_bits, 0);
                }
                sn_value_clear(ctx, &mant);
                return st;
            }
            return fmp_write_expmax(ctx, out, u->sign, &z, 0, e_bits, m_bits, nan_en);
        }
    }

    /* Normal: biased exponent field = bias + uexp */
    if (e_bits <= 62)
        expf = (uint64_t)(uexp64 + bias);
    else if (e_bits <= 64) {
        uint64_t ubias = ((uint64_t)1 << (e_bits - 1)) - 1u;
        if (uexp64 >= 0)
            expf = ubias + (uint64_t)uexp64;
        else {
            uint64_t mag = (uint64_t)(-uexp64);
            if (mag > ubias)
                expf = 0; /* should have been underflow; keep safe */
            else
                expf = ubias - mag;
        }
    } else {
        expf = 0; /* written specially below */
    }
    sn_value_init(&mant);
    st = sn_value_copy(ctx, &mant, &u->sig);
    if (st != SN_OK) return st;
    st = sig_setbit(ctx, &mant, m_bits, 0);
    if (st != SN_OK) { sn_value_clear(ctx, &mant); return st; }
    nm = sn_limbs_for_bits(m_bits);
    st = sig_ensure(ctx, &mant, nm);
    if (st != SN_OK) { sn_value_clear(ctx, &mant); return st; }
    M = SN_LIMBS(&mant);
    if (m_bits % SN_LIMB_BITS)
        M[nm - 1] &= (sn_limb)((1u << (m_bits % SN_LIMB_BITS)) - 1u);
    for (i = nm; i < mant.nlimbs; i++) M[i] = 0;
    mant.nlimbs = nm;
    sn_bigint_normalize(&mant);
    if (e_bits <= 64) {
        st = fmp_write(ctx, out, u->sign, expf, SN_CLIMBS(&mant), mant.nlimbs, e_bits, m_bits, nan_en);
    } else {
        /* bias = 2^(e-1)-1: bits [0..e-2] ones; then add signed uexp into e-bit field */
        int tb, n, bi;
        sn_limb *L;
        int carry, all_ones;
        st = sn_float_new(ctx, out, e_bits, m_bits, nan_en);
        if (st != SN_OK) { sn_value_clear(ctx, &mant); return st; }
        tb = fmp_total(e_bits, m_bits);
        n = sn_limbs_for_bits(tb);
        L = SN_LIMBS(out);
        for (i = 0; i < n; i++) L[i] = 0;
        for (i = 0; i < m_bits; i++) {
            int li = i / SN_LIMB_BITS, off = i % SN_LIMB_BITS;
            if (li < mant.nlimbs && ((SN_CLIMBS(&mant)[li] >> off) & 1u))
                bit_set(out, i, 1);
        }
        for (i = 0; i < e_bits - 1; i++)
            bit_set(out, m_bits + i, 1); /* bias */
        if (uexp64 >= 0) {
            uint64_t add = (uint64_t)uexp64;
            carry = 0;
            for (bi = 0; bi < e_bits; bi++) {
                int bit = bit_get(out, m_bits + bi);
                int ab = (bi < 64) ? (int)((add >> bi) & 1ull) : 0;
                int sum = bit + ab + carry;
                bit_set(out, m_bits + bi, sum & 1);
                carry = sum >> 1;
            }
            if (carry) {
                sn_value_clear(ctx, &mant);
                sn_raise(ctx, SN_FLAG_OVERFLOW | SN_FLAG_INEXACT);
                return fmp_write_expmax(ctx, out, u->sign, &z, 0, e_bits, m_bits, nan_en);
            }
        } else {
            /* subtract |uexp| from bias field */
            uint64_t sub;
            int borrow = 0;
            if (uexp64 == INT64_MIN)
                sub = (uint64_t)INT64_MAX + 1u;
            else
                sub = (uint64_t)(-uexp64);
            for (bi = 0; bi < e_bits; bi++) {
                int bit = bit_get(out, m_bits + bi);
                int sb = (bi < 64) ? (int)((sub >> bi) & 1ull) : 0;
                int tmp = bit - sb - borrow;
                if (tmp < 0) {
                    bit_set(out, m_bits + bi, tmp + 2);
                    borrow = 1;
                } else {
                    bit_set(out, m_bits + bi, tmp);
                    borrow = 0;
                }
            }
            if (borrow || fmp_exp_is_zero(out, m_bits, e_bits)) {
                sn_value_clear(ctx, &mant);
                sn_raise(ctx, SN_FLAG_UNDERFLOW | SN_FLAG_INEXACT);
                return fmp_write(ctx, out, u->sign, 0, &z, 0, e_bits, m_bits, nan_en);
            }
        }
        all_ones = fmp_exp_is_all_ones(out, m_bits, e_bits);
        if (all_ones) {
            sn_value_clear(ctx, &mant);
            sn_raise(ctx, SN_FLAG_OVERFLOW | SN_FLAG_INEXACT);
            return fmp_write_expmax(ctx, out, u->sign, &z, 0, e_bits, m_bits, nan_en);
        }
        if (u->sign) bit_set(out, tb - 1, 1);
        out->nlimbs = n;
        st = SN_OK;
    }
    sn_value_clear(ctx, &mant);
    return st;
}


static sn_status fmp_add_sub(sn_ctx *ctx, sn_fmp *r, const sn_fmp *a_in, const sn_fmp *b_in, int sub)
{
    sn_fmp a, b;
    sn_status st;
    int ys, sticky = 0, cmp;
    sn_value sum;

    fmp_init(&a);
    fmp_init(&b);
    fmp_init(r);

    if (a_in->is_nan) { st = fmp_copy(ctx, r, a_in); goto done; }
    if (b_in->is_nan) {
        st = fmp_copy(ctx, r, b_in);
        if (st == SN_OK) r->sign = b_in->sign ^ (sub ? 1 : 0);
        goto done;
    }
    ys = b_in->sign ^ (sub ? 1 : 0);
    if (a_in->is_inf && b_in->is_inf && a_in->sign != ys) { r->is_nan = 1; st = SN_OK; goto done; }
    if (a_in->is_inf) { st = fmp_copy(ctx, r, a_in); goto done; }
    if (b_in->is_inf) { st = fmp_copy(ctx, r, b_in); if (st == SN_OK) r->sign = ys; goto done; }

    if ((a_in->is_zero || sig_bitlen(&a_in->sig) == 0) &&
        (b_in->is_zero || sig_bitlen(&b_in->sig) == 0)) {
        r->is_zero = 1;
        r->sign = (a_in->sign && ys) ? 1 : 0;
        st = SN_OK;
        goto done;
    }
    if (a_in->is_zero || sig_bitlen(&a_in->sig) == 0) {
        st = fmp_copy(ctx, r, b_in);
        if (st == SN_OK) r->sign = ys;
        goto done;
    }
    if (b_in->is_zero || sig_bitlen(&b_in->sig) == 0) {
        st = fmp_copy(ctx, r, a_in);
        goto done;
    }

    if (a_in->exp >= b_in->exp) {
        st = fmp_copy(ctx, &a, a_in); if (st != SN_OK) goto done;
        st = fmp_copy(ctx, &b, b_in); if (st != SN_OK) goto done;
        b.sign = ys;
    } else {
        st = fmp_copy(ctx, &a, b_in); if (st != SN_OK) goto done;
        a.sign = ys;
        st = fmp_copy(ctx, &b, a_in); if (st != SN_OK) goto done;
    }
    /* preserve inexact residue from prior unpacked ops (softfp sticky-in-LSB chain) */
    sticky = (a.sticky | b.sticky) ? 1 : 0;

    /*
     * softfp add_sf: inject 3 GRS bits of headroom, align with sticky-in-LSB
     * (rshift_rnd), then pack does the single RNE. Multiprec can hold extra bits.
     */
    st = sig_shl(ctx, &a.sig, 3); if (st != SN_OK) goto done;
    a.exp -= 3;
    st = sig_shl(ctx, &b.sig, 3); if (st != SN_OK) goto done;
    b.exp -= 3;

    if (a.exp > b.exp) {
        int64_t d = a.exp - b.exp;
        st = sig_shr_sticky(ctx, &b.sig, d, &sticky);
        if (st != SN_OK) goto done;
        b.exp = a.exp;
        /* rshift_rnd: OR lost bits into LSB */
        if (sticky && sig_bitlen(&b.sig) != 0) {
            st = sig_setbit(ctx, &b.sig, 0, 1);
            if (st != SN_OK) goto done;
            sticky = 0;
        }
        if (sig_bitlen(&b.sig) == 0) {
            st = fmp_copy(ctx, r, &a);
            r->sticky |= sticky;
            goto done;
        }
    }

    sn_value_init(&sum);
    if (a.sign == b.sign) {
        st = sn_limb_add(ctx, &sum, SN_CLIMBS(&a.sig), a.sig.nlimbs,
                         SN_CLIMBS(&b.sig), b.sig.nlimbs);
        if (st != SN_OK) { sn_value_clear(ctx, &sum); goto done; }
        r->sign = a.sign;
        r->exp = a.exp;
        sn_value_move(&r->sig, &sum);
    } else {
        cmp = sn_limb_cmp(SN_CLIMBS(&a.sig), a.sig.nlimbs, SN_CLIMBS(&b.sig), b.sig.nlimbs);
        if (cmp == 0) {
            r->is_zero = 1;
            r->sign = 0;
            r->sticky = sticky; /* cancelled magnitude may still be inexact */
            sn_value_clear(ctx, &sum);
            st = SN_OK;
            goto done;
        }
        if (cmp > 0) {
            st = sn_limb_sub(ctx, &sum, SN_CLIMBS(&a.sig), a.sig.nlimbs,
                             SN_CLIMBS(&b.sig), b.sig.nlimbs);
            r->sign = a.sign;
        } else {
            st = sn_limb_sub(ctx, &sum, SN_CLIMBS(&b.sig), b.sig.nlimbs,
                             SN_CLIMBS(&a.sig), a.sig.nlimbs);
            r->sign = b.sign;
        }
        if (st != SN_OK) { sn_value_clear(ctx, &sum); goto done; }
        r->exp = a.exp;
        sn_value_move(&r->sig, &sum);
    }
    r->sticky = sticky;
    st = SN_OK;
done:
    fmp_clear(ctx, &a);
    fmp_clear(ctx, &b);
    return st;
}

/*
 * Full-precision add/sub for fused FMA: align by left-shifting the larger-exp
 * operand so the smaller (often the long product) never loses low bits.
 */
static sn_status fmp_add_sub_wide(sn_ctx *ctx, sn_fmp *r, const sn_fmp *a_in, const sn_fmp *b_in, int sub)
{
    sn_fmp a, b;
    sn_status st;
    int ys, cmp;
    sn_value sum;

    fmp_init(&a);
    fmp_init(&b);
    fmp_init(r);

    if (a_in->is_nan) { st = fmp_copy(ctx, r, a_in); goto done; }
    if (b_in->is_nan) {
        st = fmp_copy(ctx, r, b_in);
        if (st == SN_OK) r->sign = b_in->sign ^ (sub ? 1 : 0);
        goto done;
    }
    ys = b_in->sign ^ (sub ? 1 : 0);
    if (a_in->is_inf && b_in->is_inf && a_in->sign != ys) { r->is_nan = 1; st = SN_OK; goto done; }
    if (a_in->is_inf) { st = fmp_copy(ctx, r, a_in); goto done; }
    if (b_in->is_inf) { st = fmp_copy(ctx, r, b_in); if (st == SN_OK) r->sign = ys; goto done; }

    if ((a_in->is_zero || sig_bitlen(&a_in->sig) == 0) &&
        (b_in->is_zero || sig_bitlen(&b_in->sig) == 0)) {
        r->is_zero = 1;
        r->sign = (a_in->sign && ys) ? 1 : 0;
        st = SN_OK;
        goto done;
    }
    if (a_in->is_zero || sig_bitlen(&a_in->sig) == 0) {
        st = fmp_copy(ctx, r, b_in);
        if (st == SN_OK) r->sign = ys;
        goto done;
    }
    if (b_in->is_zero || sig_bitlen(&b_in->sig) == 0) {
        st = fmp_copy(ctx, r, a_in);
        goto done;
    }

    st = fmp_copy(ctx, &a, a_in); if (st != SN_OK) goto done;
    st = fmp_copy(ctx, &b, b_in); if (st != SN_OK) goto done;
    b.sign = ys;

    /* Align to the smaller exp by left-shifting the larger-exp significand. */
    if (a.exp > b.exp) {
        st = sig_shl(ctx, &a.sig, a.exp - b.exp); if (st != SN_OK) goto done;
        a.exp = b.exp;
    } else if (b.exp > a.exp) {
        st = sig_shl(ctx, &b.sig, b.exp - a.exp); if (st != SN_OK) goto done;
        b.exp = a.exp;
    }

    sn_value_init(&sum);
    if (a.sign == b.sign) {
        st = sn_limb_add(ctx, &sum, SN_CLIMBS(&a.sig), a.sig.nlimbs,
                         SN_CLIMBS(&b.sig), b.sig.nlimbs);
        if (st != SN_OK) { sn_value_clear(ctx, &sum); goto done; }
        r->sign = a.sign;
        r->exp = a.exp;
        sn_value_move(&r->sig, &sum);
    } else {
        cmp = sn_limb_cmp(SN_CLIMBS(&a.sig), a.sig.nlimbs, SN_CLIMBS(&b.sig), b.sig.nlimbs);
        if (cmp == 0) {
            r->is_zero = 1;
            r->sign = 0;
            /* exact cancel still inexact if either side had sticky residue */
            r->sticky = (a.sticky | b.sticky) ? 1 : 0;
            sn_value_clear(ctx, &sum);
            st = SN_OK;
            goto done;
        }
        if (cmp > 0) {
            st = sn_limb_sub(ctx, &sum, SN_CLIMBS(&a.sig), a.sig.nlimbs,
                             SN_CLIMBS(&b.sig), b.sig.nlimbs);
            r->sign = a.sign;
        } else {
            st = sn_limb_sub(ctx, &sum, SN_CLIMBS(&b.sig), b.sig.nlimbs,
                             SN_CLIMBS(&a.sig), a.sig.nlimbs);
            r->sign = b.sign;
        }
        if (st != SN_OK) { sn_value_clear(ctx, &sum); goto done; }
        r->exp = a.exp;
        sn_value_move(&r->sig, &sum);
    }
    r->sticky = (a.sticky | b.sticky) ? 1 : 0;
    st = SN_OK;
done:
    fmp_clear(ctx, &a);
    fmp_clear(ctx, &b);
    return st;
}

static sn_status fmp_mul(sn_ctx *ctx, sn_fmp *r, const sn_fmp *a, const sn_fmp *b)
{
    sn_status st;
    sn_value prod;
    fmp_init(r);
    if (a->is_nan) return fmp_copy(ctx, r, a);
    if (b->is_nan) return fmp_copy(ctx, r, b);
    if ((a->is_inf && (b->is_zero || sig_bitlen(&b->sig) == 0)) ||
        (b->is_inf && (a->is_zero || sig_bitlen(&a->sig) == 0))) {
        r->is_nan = 1; return SN_OK;
    }
    if (a->is_inf || b->is_inf) {
        r->is_inf = 1; r->sign = a->sign ^ b->sign; return SN_OK;
    }
    if (a->is_zero || b->is_zero || sig_bitlen(&a->sig) == 0 || sig_bitlen(&b->sig) == 0) {
        r->is_zero = 1; r->sign = a->sign ^ b->sign; return SN_OK;
    }
    sn_value_init(&prod);
    st = sn_limb_mul(ctx, &prod, SN_CLIMBS(&a->sig), a->sig.nlimbs,
                     SN_CLIMBS(&b->sig), b->sig.nlimbs);
    if (st != SN_OK) return st;
    r->sign = a->sign ^ b->sign;
    if ((b->exp > 0 && a->exp > INT64_MAX - b->exp) ||
        (b->exp < 0 && a->exp < INT64_MIN - b->exp)) {
        sn_value_clear(ctx, &prod);
        r->is_inf = 1; r->sign = a->sign ^ b->sign; return SN_OK;
    }
    r->exp = a->exp + b->exp;
    r->sticky = (a->sticky | b->sticky) ? 1 : 0;
    sn_value_move(&r->sig, &prod);
    return SN_OK;
}

static sn_status fmp_div(sn_ctx *ctx, sn_fmp *r, const sn_fmp *a, const sn_fmp *b, int m_bits)
{
    sn_status st;
    sn_value num, den, q, rem;
    int shift;
    fmp_init(r);
    if (a->is_nan) return fmp_copy(ctx, r, a);
    if (b->is_nan) return fmp_copy(ctx, r, b);
    if (b->is_zero || sig_bitlen(&b->sig) == 0) {
        if (a->is_zero || sig_bitlen(&a->sig) == 0) { r->is_nan = 1; return SN_OK; }
        r->is_inf = 1; r->sign = a->sign ^ b->sign; return SN_OK;
    }
    if (a->is_inf) {
        if (b->is_inf) { r->is_nan = 1; return SN_OK; }
        r->is_inf = 1; r->sign = a->sign ^ b->sign; return SN_OK;
    }
    if (b->is_inf) { r->is_zero = 1; r->sign = a->sign ^ b->sign; return SN_OK; }
    if (a->is_zero || sig_bitlen(&a->sig) == 0) {
        r->is_zero = 1; r->sign = a->sign ^ b->sign; return SN_OK;
    }

    sn_value_init(&num);
    sn_value_init(&den);
    sn_value_init(&q);
    sn_value_init(&rem);
    st = sn_value_copy(ctx, &num, &a->sig); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &den, &b->sig); if (st != SN_OK) goto done;
    shift = m_bits + 2;
    st = sig_shl(ctx, &num, shift); if (st != SN_OK) goto done;
    st = sn_limb_divmod(ctx, &q, &rem, SN_CLIMBS(&num), num.nlimbs, SN_CLIMBS(&den), den.nlimbs);
    if (st != SN_OK) goto done;
    r->sign = a->sign ^ b->sign;
    r->exp = a->exp - b->exp - shift;
    r->sticky = ((sig_bitlen(&rem) != 0) || a->sticky || b->sticky) ? 1 : 0;
    sn_value_move(&r->sig, &q);
    if (sig_bitlen(&r->sig) == 0)
        st = sig_setbit(ctx, &r->sig, 0, 1);
done:
    sn_value_clear(ctx, &num);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &q);
    sn_value_clear(ctx, &rem);
    return st;
}

sn_status sn_float_mp_add(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_fmp ua, ub, ur;
    sn_status st;
    fmp_init(&ua); fmp_init(&ub); fmp_init(&ur);
    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    st = fmp_add_sub(ctx, &ur, &ua, &ub, 0); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &ur);
    return st;
}

sn_status sn_float_mp_sub(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_fmp ua, ub, ur;
    sn_status st;
    fmp_init(&ua); fmp_init(&ub); fmp_init(&ur);
    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    st = fmp_add_sub(ctx, &ur, &ua, &ub, 1); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &ur);
    return st;
}

sn_status sn_float_mp_mul(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_fmp ua, ub, ur;
    sn_status st;
    fmp_init(&ua); fmp_init(&ub); fmp_init(&ur);
    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    st = fmp_mul(ctx, &ur, &ua, &ub); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &ur);
    return st;
}

sn_status sn_float_mp_div(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_fmp ua, ub, ur;
    sn_status st;
    fmp_init(&ua); fmp_init(&ub); fmp_init(&ur);
    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    if ((ub.is_zero || sig_bitlen(&ub.sig) == 0) && !ua.is_nan)
        sn_raise(ctx, SN_FLAG_DIVZERO);
    st = fmp_div(ctx, &ur, &ua, &ub, a->m_bits); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &ur);
    return st;
}

sn_status sn_float_mp_cmp(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b)
{
    sn_fmp ua, ub, x, y;
    sn_status st;
    int cmp;
    fmp_init(&ua); fmp_init(&ub); fmp_init(&x); fmp_init(&y);
    if (!rel || !a || !b) return SN_ERR_ARG;
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    if (ua.is_nan || ub.is_nan) { *rel = 0; sn_raise(ctx, SN_FLAG_INVALID); st = SN_OK; goto done; }
    if ((ua.is_zero || sig_bitlen(&ua.sig) == 0) && (ub.is_zero || sig_bitlen(&ub.sig) == 0)) {
        *rel = 0; st = SN_OK; goto done;
    }
    if (ua.sign != ub.sign) { *rel = ua.sign ? -1 : 1; st = SN_OK; goto done; }
    if (ua.is_inf && ub.is_inf) { *rel = 0; st = SN_OK; goto done; }
    if (ua.is_inf) { *rel = ua.sign ? -1 : 1; st = SN_OK; goto done; }
    if (ub.is_inf) { *rel = ua.sign ? 1 : -1; st = SN_OK; goto done; }

    st = fmp_copy(ctx, &x, &ua); if (st != SN_OK) goto done;
    st = fmp_copy(ctx, &y, &ub); if (st != SN_OK) goto done;
    /* value = sig * 2^exp; align to the smaller exp by left-shifting the larger */
    if (x.exp > y.exp) {
        st = sig_shl(ctx, &x.sig, x.exp - y.exp); if (st != SN_OK) goto done;
    } else if (y.exp > x.exp) {
        st = sig_shl(ctx, &y.sig, y.exp - x.exp); if (st != SN_OK) goto done;
    }
    cmp = sn_limb_cmp(SN_CLIMBS(&x.sig), x.sig.nlimbs, SN_CLIMBS(&y.sig), y.sig.nlimbs);
    if (cmp == 0) *rel = 0;
    else *rel = ua.sign ? -cmp : cmp;
    st = SN_OK;
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &x); fmp_clear(ctx, &y);
    return st;
}

sn_status sn_float_mp_from_i64(sn_ctx *ctx, sn_value *out, int64_t x,
                               int e_bits, int m_bits, int nan_en, const sn_op_opt *opt)
{
    sn_fmp u;
    sn_status st;
    uint64_t mag;
    fmp_init(&u);
    if (x == 0) {
        u.is_zero = 1;
        st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
        fmp_clear(ctx, &u);
        return st;
    }
    u.sign = x < 0;
    if (x == INT64_MIN) mag = (uint64_t)1 << 63;
    else mag = (uint64_t)(x < 0 ? -x : x);
    st = sig_ensure(ctx, &u.sig, 2);
    if (st != SN_OK) { fmp_clear(ctx, &u); return st; }
    sn_limbs_from_u64(SN_LIMBS(&u.sig), 2, mag);
    u.sig.nlimbs = (mag > 0xFFFFFFFFull) ? 2 : 1;
    sn_bigint_normalize(&u.sig);
    u.exp = 0;
    st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
    fmp_clear(ctx, &u);
    return st;
}

sn_status sn_float_mp_from_double(sn_ctx *ctx, sn_value *out, double x,
                                  int e_bits, int m_bits, int nan_en, const sn_op_opt *opt)
{
    sn_fmp u;
    sn_status st;
    int exp;
    double frac, scaled;
    uint64_t sig;
    fmp_init(&u);
    if (x != x) {
        u.is_nan = 1;
        st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
        fmp_clear(ctx, &u);
        return st;
    }
    u.sign = signbit(x) ? 1 : 0;
    if (x == 0.0) {
        u.is_zero = 1;
        st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
        fmp_clear(ctx, &u);
        return st;
    }
    if (isinf(x)) {
        u.is_inf = 1;
        st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
        fmp_clear(ctx, &u);
        return st;
    }
    frac = frexp(fabs(x), &exp);
    scaled = ldexp(frac, 53);
    sig = (uint64_t)scaled;
    if (!sig) sig = 1;
    st = sig_ensure(ctx, &u.sig, 2);
    if (st != SN_OK) { fmp_clear(ctx, &u); return st; }
    sn_limbs_from_u64(SN_LIMBS(&u.sig), 2, sig);
    u.sig.nlimbs = 2;
    sn_bigint_normalize(&u.sig);
    u.exp = exp - 53;
    if (scaled != (double)sig) sn_raise(ctx, SN_FLAG_INEXACT);
    st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
    fmp_clear(ctx, &u);
    return st;
}


/* Pack INT/BIGINT magnitude into multiprec float (full limb precision; no double). */
sn_status sn_float_mp_from_bigint(sn_ctx *ctx, sn_value *out, const sn_value *src,
                                  int e_bits, int m_bits, int nan_en, const sn_op_opt *opt)
{
    sn_fmp u;
    sn_status st;
    sn_value mag;
    int neg = 0;
    int i, n;

    if (!src) return SN_ERR_ARG;
    if (src->kind != SN_KIND_INT && src->kind != SN_KIND_BIGINT) return SN_ERR_TYPE;
    if (!sn_float_mp_supported(e_bits, m_bits)) return SN_ERR_RANGE;

    fmp_init(&u);
    sn_value_init(&mag);

    /* Magnitude as unsigned BIGINT (two's complement INT -> abs mag). */
    if (src->kind == SN_KIND_BIGINT) {
        neg = src->negative && !(src->nlimbs == 1 && SN_CLIMBS(src)[0] == 0);
        st = sn_value_copy(ctx, &mag, src);
        if (st != SN_OK) goto done;
        mag.negative = 0;
        mag.kind = SN_KIND_BIGINT;
        sn_bigint_normalize(&mag);
    } else {
        /* INT: copy then convert TC to magnitude if signed-negative. */
        const sn_limb *limbs;
        int top, rem, is_neg = 0;
        st = sn_value_copy(ctx, &mag, src);
        if (st != SN_OK) goto done;
        if (src->is_signed && src->width > 0) {
            limbs = SN_CLIMBS(&mag);
            top = (src->width - 1) / SN_LIMB_BITS;
            rem = (src->width - 1) % SN_LIMB_BITS;
            if (top < mag.nlimbs && (limbs[top] & ((sn_limb)1u << rem)))
                is_neg = 1;
        }
        if (is_neg) {
            sn_limb *L = SN_LIMBS(&mag);
            uint64_t carry = 1;
            for (i = 0; i < mag.nlimbs; i++) {
                uint64_t x = ((uint64_t)(~L[i]) & SN_LIMB_MASK) + carry;
                L[i] = (sn_limb)(x & SN_LIMB_MASK);
                carry = x >> SN_LIMB_BITS;
            }
            /* Mask to width bits (two's-complement abs of fixed-width). */
            {
                int w = src->width;
                int ln = sn_limbs_for_bits(w);
                if (mag.nlimbs > ln) mag.nlimbs = ln;
                if (w % SN_LIMB_BITS) {
                    sn_limb m = ((sn_limb)1u << (w % SN_LIMB_BITS)) - 1u;
                    if (mag.nlimbs > 0)
                        SN_LIMBS(&mag)[mag.nlimbs - 1] &= m;
                }
            }
            neg = 1;
        }
        mag.kind = SN_KIND_BIGINT;
        mag.width = 0;
        mag.is_signed = 0;
        mag.negative = 0;
        sn_bigint_normalize(&mag);
    }

    if (mag.nlimbs == 1 && SN_CLIMBS(&mag)[0] == 0) {
        u.is_zero = 1;
        u.sign = 0;
        st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
        goto done;
    }

    n = mag.nlimbs;
    if (n < 1) n = 1;
    st = sig_ensure(ctx, &u.sig, n);
    if (st != SN_OK) goto done;
    for (i = 0; i < n; i++)
        SN_LIMBS(&u.sig)[i] = SN_CLIMBS(&mag)[i];
    u.sig.nlimbs = n;
    sn_bigint_normalize(&u.sig);
    u.sign = neg ? 1 : 0;
    u.exp = 0; /* integer significand; fmp_pack normalizes */
    st = fmp_pack(ctx, out, &u, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));

done:
    sn_value_clear(ctx, &mag);
    fmp_clear(ctx, &u);
    return st;
}

sn_fpclass sn_float_mp_classify(const sn_value *v)
{
    int e_bits, m_bits, i, mant_nz = 0;
    if (!v || v->kind != SN_KIND_FLOAT) return SN_FP_NAN;
    e_bits = v->e_bits;
    m_bits = v->m_bits;
    for (i = 0; i < m_bits; i++)
        if (bit_get(v, i)) { mant_nz = 1; break; }
    if (fmp_exp_is_all_ones(v, m_bits, e_bits)) return mant_nz ? SN_FP_NAN : SN_FP_INFINITE;
    if (fmp_exp_is_zero(v, m_bits, e_bits)) return mant_nz ? SN_FP_SUBNORMAL : SN_FP_ZERO;
    return SN_FP_NORMAL;
}

int sn_float_mp_signbit(const sn_value *v)
{
    if (!v || v->kind != SN_KIND_FLOAT) return 0;
    return bit_get(v, fmp_total(v->e_bits, v->m_bits) - 1);
}

sn_status sn_float_mp_to_double(sn_ctx *ctx, const sn_value *v, double *out)
{
    sn_fmp u;
    sn_status st;
    int bl, take, i;
    uint64_t top;
    fmp_init(&u);
    st = fmp_unpack(ctx, v, &u);
    if (st != SN_OK) return st;
    if (u.is_nan) { *out = nan(""); }
    else if (u.is_inf) { *out = u.sign ? -INFINITY : INFINITY; }
    else if (u.is_zero || sig_bitlen(&u.sig) == 0) { *out = u.sign ? -0.0 : 0.0; }
    else {
        bl = sig_bitlen(&u.sig);
        take = bl < 53 ? bl : 53;
        top = 0;
        for (i = 0; i < take; i++)
            if (sig_getbit(&u.sig, bl - take + i)) top |= (1ull << i);
        *out = ldexp((double)top, u.exp + (bl - take));
        if (u.sign) *out = -*out;
    }
    fmp_clear(ctx, &u);
    return SN_OK;
}


/* Newton sqrt on multiprec float values (via public ±×÷). */
sn_status sn_float_mp_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_fmp ua;
    sn_status st;
    sn_value y, t, half, two;
    double da, dr;
    int i, iters, e_bits, m_bits, nan_en, rel;

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (!sn_float_mp_supported(a->e_bits, a->m_bits)) return SN_ERR_RANGE;
    e_bits = a->e_bits;
    m_bits = a->m_bits;
    nan_en = a->nan_enabled;

    fmp_init(&ua);
    st = fmp_unpack(ctx, a, &ua);
    if (st != SN_OK) { fmp_clear(ctx, &ua); return st; }

    if (ua.is_nan) {
        sn_raise(ctx, SN_FLAG_INVALID);
        st = fmp_pack(ctx, out, &ua, e_bits, m_bits, nan_en, fmp_rnd(ctx, opt));
        fmp_clear(ctx, &ua);
        return st;
    }
    if (ua.sign && !ua.is_zero) {
        sn_raise(ctx, SN_FLAG_INVALID);
        fmp_clear(ctx, &ua);
        if (!nan_en)
            return sn_float_set_inf(ctx, out, 0, e_bits, m_bits, nan_en);
        return sn_float_set_nan(ctx, out, e_bits, m_bits);
    }
    if (ua.is_zero) {
        /* IEEE: sqrt(+-0) = +-0 */
        fmp_clear(ctx, &ua);
        return sn_value_copy(ctx, out, a);
    }
    if (ua.is_inf) {
        fmp_clear(ctx, &ua);
        return sn_float_set_inf(ctx, out, 0, e_bits, m_bits, nan_en);
    }
    fmp_clear(ctx, &ua);

    /* Initial guess from host double (truncated). */
    st = sn_float_mp_to_double(ctx, a, &da);
    if (st != SN_OK) return st;
    if (da < 0.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!nan_en) return sn_float_set_inf(ctx, out, 0, e_bits, m_bits, nan_en);
        return sn_float_set_nan(ctx, out, e_bits, m_bits);
    }
    dr = sqrt(da);
    if (dr == 0.0 && da > 0.0) dr = ldexp(1.0, (int)(0.5 * log(da) / log(2.0))); /* overflow guard */
    if (!isfinite(dr) || dr <= 0.0) {
        /* crude power-of-two guess */
        dr = 1.0;
    }

    sn_value_init(&y);
    sn_value_init(&t);
    sn_value_init(&half);
    sn_value_init(&two);
    st = sn_float_mp_from_double(ctx, &y, dr, e_bits, m_bits, nan_en, opt); if (st != SN_OK) goto done;
    st = sn_float_mp_from_double(ctx, &half, 0.5, e_bits, m_bits, nan_en, opt); if (st != SN_OK) goto done;
    st = sn_float_mp_from_double(ctx, &two, 2.0, e_bits, m_bits, nan_en, opt); if (st != SN_OK) goto done;

    /* iters ~ log2(m_bits) + few for double seed */
    iters = 6;
    while ((1 << (iters - 4)) < m_bits) iters++;
    if (iters < 8) iters = 8;
    if (iters > 40) iters = 40;

    for (i = 0; i < iters; i++) {
        /* y = (y + a/y) * 0.5 */
        st = sn_float_mp_div(ctx, &t, a, &y, opt); if (st != SN_OK) goto done;
        st = sn_float_mp_add(ctx, &t, &y, &t, opt); if (st != SN_OK) goto done;
        st = sn_float_mp_mul(ctx, &y, &t, &half, opt); if (st != SN_OK) goto done;
    }

    /* Ensure non-negative (should already be). */
    if (sn_float_mp_signbit(&y)) {
        st = sn_float_mp_from_double(ctx, &t, 0.0, e_bits, m_bits, nan_en, opt); if (st != SN_OK) goto done;
        st = sn_float_mp_sub(ctx, &y, &t, &y, opt); if (st != SN_OK) goto done;
    }

    /* One final Heron polish already done; copy out */
    st = sn_value_copy(ctx, out, &y);

    /* Verify y*y ~ a within a few ulps by optional check skipped for speed */
    (void)rel;
    (void)two;
done:
    sn_value_clear(ctx, &y);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &two);
    return st;
}


/* Round fmp value to nearest integer (IEEE ties-to-even). Result has exp=0, sig = |n|.
 * Used by remainder/remquo nearest quotient (not C round()/SN_ROUND_NA). */
static sn_status fmp_round_nearest_int(sn_ctx *ctx, sn_fmp *r, const sn_fmp *a)
{
    sn_status st;
    sn_fmp t;
    int k, bl, guard, sticky, round_up;

    fmp_init(r);
    if (a->is_nan) return fmp_copy(ctx, r, a);
    if (a->is_inf) return fmp_copy(ctx, r, a);
    if (a->is_zero || sig_bitlen(&a->sig) == 0) {
        r->is_zero = 1;
        r->sign = a->sign;
        return SN_OK;
    }

    fmp_init(&t);
    st = fmp_copy(ctx, &t, a); if (st != SN_OK) { fmp_clear(ctx, &t); return st; }

    if (t.exp >= 0) {
        /* exact integer if significand has no fractional scale left */
        st = sig_shl(ctx, &t.sig, t.exp); if (st != SN_OK) goto done;
        t.exp = 0;
        t.sticky = 0;
        st = fmp_copy(ctx, r, &t);
        goto done;
    }

    k = -t.exp; /* fractional bits */
    bl = sig_bitlen(&t.sig);
    if (k > bl) {
        /* |a| < 0.5 */
        r->is_zero = 1;
        r->sign = a->sign;
        st = SN_OK;
        goto done;
    }
    if (k == bl) {
        /* 0.5 <= |a| < 1  (top bit contributes 2^{bl-1-k} = 2^{-1}) */
        sticky = (bl > 1) ? sig_low_any(&t.sig, (int64_t)bl - 1) : 0;
        if (!sticky) {
            /* exact +/-0.5: ties to even => 0 */
            r->is_zero = 1;
            r->sign = a->sign;
            st = SN_OK;
        } else {
            /* > 0.5 => round away to +/-1 */
            r->sign = a->sign;
            r->exp = 0;
            st = sig_zero(ctx, &r->sig);
            if (st == SN_OK) st = sig_setbit(ctx, &r->sig, 0, 1);
        }
        goto done;
    }

    /* bits [0, k) are fractional; bit (k-1) is guard (0.5 place) */
    guard = sig_getbit(&t.sig, k - 1);
    sticky = (k > 1) ? sig_low_any(&t.sig, (int64_t)k - 1) : 0;
    st = sig_shr_sticky(ctx, &t.sig, k, &sticky); if (st != SN_OK) goto done;
    t.exp = 0;
    /* IEEE nearest, ties to even */
    round_up = 0;
    if (guard && sticky)
        round_up = 1;
    else if (guard && !sticky)
        round_up = (sig_getbit(&t.sig, 0) != 0); /* odd => make even */
    if (round_up) {
        st = sig_add1(ctx, &t.sig); if (st != SN_OK) goto done;
    }
    if (sig_bitlen(&t.sig) == 0) {
        r->is_zero = 1;
        r->sign = a->sign;
        st = SN_OK;
        goto done;
    }
    t.sticky = 0;
    st = fmp_copy(ctx, r, &t);
done:
    fmp_clear(ctx, &t);
    return st;
}

/*
 * Nearest integer to a/b (IEEE ties-to-even), pure soft.
 * Used by sn_frem / sn_remquo. Full-precision integer division when aligned
 * numerator is modest; otherwise progressive fmp_div + ties-to-even round.
 */
static sn_status fmp_nearest_quot(sn_ctx *ctx, sn_fmp *n, const sn_fmp *a, const sn_fmp *b)
{
    sn_status st;
    sn_value numa, numb, q, rem;
    sn_fmp ua, ub, uq;
    int bla, blb, shift, cmp, need_inc;
    int max_exact_bits = 1 << 20; /* ~1M bits: avoid pathological memory */

    fmp_init(n);
    if (a->is_nan) return fmp_copy(ctx, n, a);
    if (b->is_nan) return fmp_copy(ctx, n, b);
    if (b->is_zero || sig_bitlen(&b->sig) == 0) {
        if (a->is_zero || sig_bitlen(&a->sig) == 0) { n->is_nan = 1; return SN_OK; }
        n->is_inf = 1; n->sign = a->sign ^ b->sign; return SN_OK;
    }
    if (a->is_inf) {
        if (b->is_inf) { n->is_nan = 1; return SN_OK; }
        n->is_inf = 1; n->sign = a->sign ^ b->sign; return SN_OK;
    }
    if (b->is_inf || a->is_zero || sig_bitlen(&a->sig) == 0) {
        n->is_zero = 1; n->sign = a->sign ^ b->sign; return SN_OK;
    }

    bla = sig_bitlen(&a->sig);
    blb = sig_bitlen(&b->sig);
    /* |a/b| < 0.5 when (bla-1+a.exp) - (blb-1+b.exp) < -1 */
    {
        long e_diff = (long)bla + (long)a->exp - (long)blb - (long)b->exp;
        if (e_diff < -1) {
            n->is_zero = 1;
            n->sign = a->sign ^ b->sign;
            return SN_OK;
        }
    }

    sn_value_init(&numa);
    sn_value_init(&numb);
    sn_value_init(&q);
    sn_value_init(&rem);

    st = sn_value_copy(ctx, &numa, &a->sig); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &numb, &b->sig); if (st != SN_OK) goto done;

    /* a/b = (sa * 2^{ea-eb}) / sb  (or sa / (sb * 2^{eb-ea})) */
    shift = a->exp - b->exp;
    if (shift >= 0) {
        st = sig_shl(ctx, &numa, shift); if (st != SN_OK) goto done;
    } else {
        st = sig_shl(ctx, &numb, -shift); if (st != SN_OK) goto done;
    }

    {
        int nbl = sig_bitlen(&numa);
        int dbl = sig_bitlen(&numb);
        if (nbl - dbl > max_exact_bits || nbl > max_exact_bits + 64) {
            int need = nbl - dbl + 4;
            if (need < 8) need = 8;
            if (need > max_exact_bits) need = max_exact_bits;
            fmp_init(&ua); fmp_init(&ub); fmp_init(&uq);
            st = fmp_copy(ctx, &ua, a);
            if (st == SN_OK) st = fmp_copy(ctx, &ub, b);
            if (st == SN_OK) st = fmp_div(ctx, &uq, &ua, &ub, need);
            if (st == SN_OK) st = fmp_round_nearest_int(ctx, n, &uq);
            fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &uq);
            goto done;
        }
    }

    st = sn_limb_divmod(ctx, &q, &rem, SN_CLIMBS(&numa), numa.nlimbs,
                        SN_CLIMBS(&numb), numb.nlimbs);
    if (st != SN_OK) goto done;

    /* IEEE nearest, ties to even: if 2*rem > denom inc; if equal, inc only if q odd */
    need_inc = 0;
    if (sig_bitlen(&rem) != 0) {
        st = sig_shl(ctx, &rem, 1); if (st != SN_OK) goto done;
        cmp = sn_limb_cmp(SN_CLIMBS(&rem), rem.nlimbs, SN_CLIMBS(&numb), numb.nlimbs);
        if (cmp > 0)
            need_inc = 1;
        else if (cmp == 0)
            need_inc = (sig_getbit(&q, 0) != 0);
    }
    if (need_inc) {
        st = sig_add1(ctx, &q); if (st != SN_OK) goto done;
    }

    if (sig_bitlen(&q) == 0) {
        n->is_zero = 1;
        n->sign = a->sign ^ b->sign;
        st = SN_OK;
        goto done;
    }
    n->sign = a->sign ^ b->sign;
    n->exp = 0;
    n->sticky = 0;
    sn_value_move(&n->sig, &q);
    st = SN_OK;
done:
    sn_value_clear(ctx, &numa);
    sn_value_clear(ctx, &numb);
    sn_value_clear(ctx, &q);
    sn_value_clear(ctx, &rem);
    return st;
}

/*
 * Fused multiprec FMA: compute (a*b + c) with a single final pack/round.
 * Product and sum keep full-precision significands (no intermediate format round).
 */
sn_status sn_float_mp_fma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                          const sn_value *c, const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp ua, ub, uc, up, ur;

    if (!a || !b || !c) return SN_ERR_ARG;
    if (a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT || c->kind != SN_KIND_FLOAT)
        return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits || a->e_bits != c->e_bits ||
        a->m_bits != c->m_bits || a->nan_enabled != b->nan_enabled || a->nan_enabled != c->nan_enabled)
        return SN_ERR_TYPE;
    if (!sn_float_mp_supported(a->e_bits, a->m_bits)) return SN_ERR_RANGE;

    fmp_init(&ua); fmp_init(&ub); fmp_init(&uc); fmp_init(&up); fmp_init(&ur);
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, c, &uc); if (st != SN_OK) goto done;
    st = fmp_mul(ctx, &up, &ua, &ub); if (st != SN_OK) goto done;
    st = fmp_add_sub_wide(ctx, &ur, &up, &uc, 0); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &uc);
    fmp_clear(ctx, &up); fmp_clear(ctx, &ur);
    return st;
}

/*
 * Soft remainder: a - n*b, n = nearest integer to a/b (IEEE ties-to-even).
 * Pure soft integer quotient via fmp_nearest_quot (no host double; works for |a/b| >> 2^53).
 */
sn_status sn_float_mp_frem(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp ua, ub, un, ut, ur;
    int ca, cb;

    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    if (!sn_float_mp_supported(a->e_bits, a->m_bits)) return SN_ERR_RANGE;

    ca = sn_float_mp_classify(a);
    cb = sn_float_mp_classify(b);
    if (ca == SN_FP_NAN || cb == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (ca == SN_FP_INFINITE || cb == SN_FP_ZERO) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, sn_float_mp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (cb == SN_FP_INFINITE || ca == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);

    fmp_init(&ua); fmp_init(&ub); fmp_init(&un); fmp_init(&ut); fmp_init(&ur);
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    st = fmp_nearest_quot(ctx, &un, &ua, &ub); if (st != SN_OK) goto done;
    if (un.is_nan) {
        sn_raise(ctx, SN_FLAG_INVALID);
        st = fmp_pack(ctx, out, &un, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
        goto done;
    }
    if (un.is_inf) {
        /* |a/b| overflowed integer scale: rem ~ 0 with sign of a */
        st = sn_float_set_zero(ctx, out, sn_float_mp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
        goto done;
    }
    st = fmp_mul(ctx, &ut, &un, &ub); if (st != SN_OK) goto done;
    st = fmp_add_sub(ctx, &ur, &ua, &ut, 1); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub);
    fmp_clear(ctx, &un); fmp_clear(ctx, &ut); fmp_clear(ctx, &ur);
    return st;
}


/* -------------------------------------------------------------------------- */
/* Integer rounding / nextafter / frexp / ldexp / fmod (multiprec)             */
/* -------------------------------------------------------------------------- */

/* Directional truncate of finite nonzero a toward mode:
 * mode 0=trunc(toward 0), 1=floor(-inf), 2=ceil(+inf), 3=round half away from 0 */
static sn_status fmp_round_int_mode(sn_ctx *ctx, sn_fmp *r, const sn_fmp *a, int mode)
{
    sn_status st;
    sn_fmp t;
    int k, bl, frac_nz, guard, sticky, round_up;

    fmp_init(r);
    if (a->is_nan || a->is_inf)
        return fmp_copy(ctx, r, a);
    if (a->is_zero || sig_bitlen(&a->sig) == 0) {
        r->is_zero = 1;
        r->sign = a->sign;
        return SN_OK;
    }

    fmp_init(&t);
    st = fmp_copy(ctx, &t, a);
    if (st != SN_OK) { fmp_clear(ctx, &t); return st; }

    if (t.exp >= 0) {
        st = sig_shl(ctx, &t.sig, t.exp);
        if (st != SN_OK) goto done;
        t.exp = 0;
        t.sticky = 0;
        st = fmp_copy(ctx, r, &t);
        goto done;
    }

    k = -t.exp;
    bl = sig_bitlen(&t.sig);
    if (k > bl) {
        /* |a| < 1 */
        frac_nz = 1;
        guard = 0;
        sticky = 1;
        st = sig_zero(ctx, &t.sig);
        if (st != SN_OK) goto done;
        t.exp = 0;
    } else {
        frac_nz = (k > 0) ? sig_low_any(&t.sig, (int64_t)k) : 0;
        sticky = (k > 1) ? sig_low_any(&t.sig, (int64_t)k - 1) : 0;
        guard = (k > 0) ? sig_getbit(&t.sig, k - 1) : 0;
        /* Drop frac bits without OR-ing guard into sticky (NTE needs exact-half). */
        {
            int dump = 0;
            st = sig_shr_sticky(ctx, &t.sig, k, &dump);
            if (st != SN_OK) goto done;
        }
        t.exp = 0;
    }

    if (!frac_nz) {
        if (sig_bitlen(&t.sig) == 0) {
            r->is_zero = 1;
            r->sign = a->sign;
            st = SN_OK;
        } else {
            t.sticky = 0;
            st = fmp_copy(ctx, r, &t);
        }
        goto done;
    }

    round_up = 0;
    if (mode == 0) {
        round_up = 0; /* trunc / toward zero */
    } else if (mode == 1) {
        round_up = (a->sign != 0); /* floor: toward -inf */
    } else if (mode == 2) {
        round_up = (a->sign == 0); /* ceil: toward +inf */
    } else if (mode == 4) {
        /* nearest, ties to even (IEEE default / rint+FE_TONEAREST) */
        if (guard && sticky)
            round_up = 1;
        else if (guard && !sticky) {
            /* exact half: round so LSB of result is even */
            int lsb = sig_getbit(&t.sig, 0);
            round_up = (lsb != 0);
        } else
            round_up = 0;
    } else {
        /* mode 3: half away from zero (C round / SN_ROUND_NA) */
        round_up = guard != 0;
    }

    if (round_up) {
        st = sig_add1(ctx, &t.sig);
        if (st != SN_OK) goto done;
    }
    if (sig_bitlen(&t.sig) == 0) {
        r->is_zero = 1;
        r->sign = a->sign;
        st = SN_OK;
    } else {
        t.sticky = 0;
        st = fmp_copy(ctx, r, &t);
    }
done:
    fmp_clear(ctx, &t);
    return st;
}

static sn_status sn_float_mp_round_mode(sn_ctx *ctx, sn_value *out, const sn_value *a,
                                        const sn_op_opt *opt, int mode)
{
    sn_status st;
    sn_fmp ua, ur;
    sn_fpclass c;
    (void)opt;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    c = sn_float_mp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE || c == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);
    fmp_init(&ua); fmp_init(&ur);
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_round_int_mode(ctx, &ur, &ua, mode); if (st != SN_OK) goto done;
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, SN_ROUND_TZ);
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ur);
    return st;
}

sn_status sn_float_mp_ceil(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{ return sn_float_mp_round_mode(ctx, out, a, opt, 2); }

sn_status sn_float_mp_floor(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{ return sn_float_mp_round_mode(ctx, out, a, opt, 1); }

sn_status sn_float_mp_trunc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{ return sn_float_mp_round_mode(ctx, out, a, opt, 0); }

sn_status sn_float_mp_round(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{ return sn_float_mp_round_mode(ctx, out, a, opt, 3); }

/* Map sn_round to integer-round mode for nearbyint/rint. */
static int fmp_mode_from_sn_round(sn_round rnd)
{
    switch (rnd) {
    case SN_ROUND_TZ: return 0;
    case SN_ROUND_DN: return 1;
    case SN_ROUND_UP: return 2;
    case SN_ROUND_NA: return 3;
    case SN_ROUND_NTE:
    default:          return 4;
    }
}

/*
 * Round to integer using current floating-point rounding direction
 * (ctx->round / opt->round). Raises SN_FLAG_INEXACT when the result
 * is not equal to the input (rint semantics).
 */
sn_status sn_float_mp_rint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp ua, ur;
    sn_fpclass c;
    int mode, frac_nz;
    sn_round rnd = fmp_rnd(ctx, opt);

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    c = sn_float_mp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE || c == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);

    fmp_init(&ua); fmp_init(&ur);
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;

    /* detect fractional part for INEXACT */
    frac_nz = 0;
    if (!ua.is_zero && !ua.is_nan && !ua.is_inf) {
        if (ua.exp < 0) {
            int k = -ua.exp;
            int bl = sig_bitlen(&ua.sig);
            int lim = k < bl ? k : bl;
            if (lim > 0 && sig_low_any(&ua.sig, (int64_t)lim)) frac_nz = 1;
            if (!frac_nz && k > bl && bl > 0) frac_nz = 1;
        } else if (ua.sticky) {
            frac_nz = 1;
        }
    }

    mode = fmp_mode_from_sn_round(rnd);
    st = fmp_round_int_mode(ctx, &ur, &ua, mode); if (st != SN_OK) goto done;
    if (frac_nz) sn_raise(ctx, SN_FLAG_INEXACT);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, SN_ROUND_TZ);
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ur);
    return st;
}

/* nearbyint: same rounding as rint but without raising INEXACT. */
sn_status sn_float_mp_nearbyint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp ua, ur;
    sn_fpclass c;
    int mode;
    sn_round rnd = fmp_rnd(ctx, opt);

    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    c = sn_float_mp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE || c == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);

    fmp_init(&ua); fmp_init(&ur);
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    mode = fmp_mode_from_sn_round(rnd);
    st = fmp_round_int_mode(ctx, &ur, &ua, mode); if (st != SN_OK) goto done;
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, SN_ROUND_TZ);
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ur);
    return st;
}

/* Bit-increment / decrement the float encoding as an unsigned integer (low bit = mant LSB). */
/* Inc/dec magnitude bits [0, tb-2]; preserve sign bit at tb-1. */
static sn_status fmp_bits_incdec(sn_ctx *ctx, sn_value *v, int inc)
{
    int tb = fmp_total(v->e_bits, v->m_bits);
    int mag_bits = tb - 1; /* exclude sign */
    int n = sn_limbs_for_bits(tb);
    sn_limb *L;
    int i, carry, sign;
    sn_status st = sn_value_reserve(ctx, v, n);
    if (st != SN_OK) return st;
    L = SN_LIMBS(v);
    for (i = v->nlimbs; i < n; i++) L[i] = 0;
    v->nlimbs = n;
    sign = bit_get(v, tb - 1);
    /* clear sign while arithmetic on magnitude */
    if (sign) bit_set(v, tb - 1, 0);
    if (inc) {
        carry = 1;
        for (i = 0; i < n && carry; i++) {
            uint64_t s = (uint64_t)L[i] + 1u;
            L[i] = (sn_limb)(s & SN_LIMB_MASK);
            carry = (s > SN_LIMB_MASK) ? 1 : 0;
        }
    } else {
        carry = 1;
        for (i = 0; i < n && carry; i++) {
            if (L[i] > 0) { L[i]--; carry = 0; }
            else { L[i] = SN_LIMB_MASK; }
        }
    }
    /* clear any bits at/above mag_bits (incl. accidental sign/exp overflow junk) then restore sign */
    {
        int li = mag_bits / SN_LIMB_BITS, off = mag_bits % SN_LIMB_BITS;
        if (off) L[li] &= (sn_limb)((1u << off) - 1u);
        else if (li < n) {
            /* mag_bits multiple of limb: clear from li upward */
        }
        for (i = li + (off ? 1 : 0); i < n; i++) L[i] = 0;
        if (!off && li < n) {
            for (i = li; i < n; i++) L[i] = 0;
        }
    }
    if (sign) bit_set(v, tb - 1, 1);
    (void)mag_bits;
    return SN_OK;
}

sn_status sn_float_mp_nextafter(sn_ctx *ctx, sn_value *out, const sn_value *from,
                                const sn_value *to, const sn_op_opt *opt)
{
    sn_status st;
    int rel, sign, tb;
    sn_fpclass cf, ct;
    sn_limb z = 0;
    (void)opt;
    if (!from || !to || from->kind != SN_KIND_FLOAT || to->kind != SN_KIND_FLOAT)
        return SN_ERR_TYPE;
    if (from->e_bits != to->e_bits || from->m_bits != to->m_bits ||
        from->nan_enabled != to->nan_enabled)
        return SN_ERR_TYPE;

    cf = sn_float_mp_classify(from);
    ct = sn_float_mp_classify(to);
    if (cf == SN_FP_NAN)
        return sn_value_copy(ctx, out, from);
    if (ct == SN_FP_NAN) {
        if (!from->nan_enabled) {
            sn_raise(ctx, SN_FLAG_INVALID);
            return sn_float_set_inf(ctx, out, 0, from->e_bits, from->m_bits, from->nan_enabled);
        }
        return sn_value_copy(ctx, out, to);
    }

    st = sn_float_mp_cmp(ctx, &rel, from, to);
    if (st != SN_OK) return st;
    if (rel == 0)
        return sn_value_copy(ctx, out, from);

    st = sn_value_copy(ctx, out, from);
    if (st != SN_OK) return st;

    /* +0 next toward - : produce -min_subnormal; -0 toward + : +min_subnormal */
    if (cf == SN_FP_ZERO) {
        int toward_pos = (rel < 0); /* from < to => go up */
        /* from is zero; if to > from (rel<0 means from<to), step positive */
        toward_pos = (rel < 0);
        tb = fmp_total(from->e_bits, from->m_bits);
        st = sn_float_new(ctx, out, from->e_bits, from->m_bits, from->nan_enabled);
        if (st != SN_OK) return st;
        {
            sn_limb *L = SN_LIMBS(out);
            int n = sn_limbs_for_bits(tb), i;
            for (i = 0; i < n; i++) L[i] = 0;
            out->nlimbs = n;
            bit_set(out, 0, 1); /* min subnormal */
            if (!toward_pos) bit_set(out, tb - 1, 1);
        }
        return SN_OK;
    }

    sign = sn_float_mp_signbit(from);
    /* IEEE nextafter bit step on sign-magnitude:
     * positive values: toward +inf => bits++, toward -inf => bits--
     * negative values: toward -inf => bits++, toward +inf => bits--
     */
    if (sign == 0) {
        /* want larger if from < to (rel < 0) */
        st = fmp_bits_incdec(ctx, out, rel < 0);
    } else {
        st = fmp_bits_incdec(ctx, out, rel > 0); /* from > to means go more negative */
    }
    if (st != SN_OK) return st;

    /* overflow to inf if exp became all-ones with zero mant already handled by wrap;
     * if we stepped past max finite, encoding becomes inf (exp max, mant 0) automatically
     * when incrementing max finite. Good. */
    (void)z;
    return SN_OK;
}

sn_status sn_float_mp_ilogb(sn_ctx *ctx, const sn_value *a, int *exp_out)
{
    sn_status st;
    sn_fmp u;
    int bl;
    sn_fpclass c;
    if (!exp_out) return SN_ERR_ARG;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    c = sn_float_mp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE || c == SN_FP_ZERO) {
        sn_raise(ctx, SN_FLAG_INVALID);
        *exp_out = (c == SN_FP_ZERO) ? FP_ILOGB0 : INT_MAX;
        return SN_ERR_DOMAIN;
    }
    fmp_init(&u);
    st = fmp_unpack(ctx, a, &u);
    if (st != SN_OK) { fmp_clear(ctx, &u); return st; }
    bl = sig_bitlen(&u.sig);
    /* value = sig * 2^exp with sig integer; true binary exponent of [1,2) mantissa:
     * a = m * 2^e, m in [1,2) => e = exp + (bl - 1) */
    *exp_out = u.exp + (bl - 1);
    fmp_clear(ctx, &u);
    return SN_OK;
}

sn_status sn_float_mp_frexp(sn_ctx *ctx, sn_value *mant, int *exp_out, const sn_value *a,
                            const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp u;
    int bl, e;
    sn_fpclass c;
    if (!exp_out || !mant) return SN_ERR_ARG;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    c = sn_float_mp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE || c == SN_FP_ZERO) {
        *exp_out = 0;
        return sn_value_copy(ctx, mant, a);
    }
    fmp_init(&u);
    st = fmp_unpack(ctx, a, &u);
    if (st != SN_OK) { fmp_clear(ctx, &u); return st; }
    bl = sig_bitlen(&u.sig);
    e = u.exp + bl; /* a = (sig/2^bl) * 2^{exp+bl}, mant in [0.5,1) */
    *exp_out = e;
    /* set u to significand with exp such that value in [0.5, 1) */
    u.exp = -bl;
    u.sticky = 0;
    st = fmp_pack(ctx, mant, &u, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
    fmp_clear(ctx, &u);
    return st;
}

sn_status sn_float_mp_ldexp(sn_ctx *ctx, sn_value *out, const sn_value *a, int n,
                            const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp u;
    sn_fpclass c;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    c = sn_float_mp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE || c == SN_FP_ZERO)
        return sn_value_copy(ctx, out, a);
    if (n == 0)
        return sn_value_copy(ctx, out, a);
    fmp_init(&u);
    st = fmp_unpack(ctx, a, &u);
    if (st != SN_OK) { fmp_clear(ctx, &u); return st; }
    /* protect overflow of int64 working exp */
    if (n > 0 && u.exp > (int64_t)INT64_MAX - (int64_t)n) {
        sn_raise(ctx, SN_FLAG_OVERFLOW);
        st = sn_float_set_inf(ctx, out, u.sign, a->e_bits, a->m_bits, a->nan_enabled);
        fmp_clear(ctx, &u);
        return st;
    }
    if (n < 0 && u.exp < (int64_t)INT64_MIN - (int64_t)n) {
        sn_raise(ctx, SN_FLAG_UNDERFLOW);
        st = sn_float_set_zero(ctx, out, u.sign, a->e_bits, a->m_bits, a->nan_enabled);
        fmp_clear(ctx, &u);
        return st;
    }
    u.exp += (int64_t)n;
    st = fmp_pack(ctx, out, &u, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
    fmp_clear(ctx, &u);
    return st;
}

sn_status sn_float_mp_modf(sn_ctx *ctx, sn_value *ipart, sn_value *fpart, const sn_value *a,
                           const sn_op_opt *opt)
{
    sn_status st;
    sn_value ip;
    sn_fpclass c;
    if (!ipart || !fpart) return SN_ERR_ARG;
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    c = sn_float_mp_classify(a);
    if (c == SN_FP_NAN || c == SN_FP_INFINITE) {
        st = sn_value_copy(ctx, ipart, a);
        if (st != SN_OK) return st;
        return sn_float_set_zero(ctx, fpart, sn_float_mp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
    }
    if (c == SN_FP_ZERO) {
        st = sn_value_copy(ctx, ipart, a);
        if (st != SN_OK) return st;
        return sn_value_copy(ctx, fpart, a);
    }
    sn_value_init(&ip);
    st = sn_float_mp_trunc(ctx, &ip, a, opt);
    if (st != SN_OK) { sn_value_clear(ctx, &ip); return st; }
    st = sn_value_copy(ctx, ipart, &ip);
    if (st != SN_OK) { sn_value_clear(ctx, &ip); return st; }
    st = sn_float_mp_sub(ctx, fpart, a, &ip, opt);
    sn_value_clear(ctx, &ip);
    return st;
}

/* fmod: a - trunc(a/b)*b, sign of a.
 * Stay in unpacked fmp form (div+trunc toward 0) to avoid intermediate pack. */
sn_status sn_float_mp_fmod(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                           const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp ua, ub, uq, uqt, ut, ur;
    sn_fpclass ca, cb;
    int need;
    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    ca = sn_float_mp_classify(a);
    cb = sn_float_mp_classify(b);
    if (ca == SN_FP_NAN || cb == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (cb == SN_FP_ZERO || ca == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, sn_float_mp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (ca == SN_FP_ZERO || cb == SN_FP_INFINITE)
        return sn_value_copy(ctx, out, a);

    fmp_init(&ua); fmp_init(&ub); fmp_init(&uq); fmp_init(&uqt); fmp_init(&ut); fmp_init(&ur);
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    /* enough bits for integer part of quotient + fraction guard */
    {
        int bla = sig_bitlen(&ua.sig);
        int blb = sig_bitlen(&ub.sig);
        long e_diff = (long)bla + (long)ua.exp - (long)blb - (long)ub.exp;
        need = a->m_bits + 8;
        if (e_diff > 0 && e_diff + 8 > need) need = (int)e_diff + 8;
        if (need < 16) need = 16;
        if (need > (1 << 20)) need = 1 << 20;
    }
    st = fmp_div(ctx, &uq, &ua, &ub, need); if (st != SN_OK) goto done;
    st = fmp_round_int_mode(ctx, &uqt, &uq, 0); if (st != SN_OK) goto done; /* trunc toward 0 */
    st = fmp_mul(ctx, &ut, &uqt, &ub); if (st != SN_OK) goto done;
    st = fmp_add_sub(ctx, &ur, &ua, &ut, 1); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub); fmp_clear(ctx, &uq);
    fmp_clear(ctx, &uqt); fmp_clear(ctx, &ut); fmp_clear(ctx, &ur);
    return st;
}


/* Positive difference: max(a - b, +0). NaN propagates. */
sn_status sn_float_mp_fdim(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                           const sn_op_opt *opt)
{
    sn_status st;
    int rel;
    sn_fpclass ca, cb;
    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits || a->nan_enabled != b->nan_enabled)
        return SN_ERR_TYPE;
    ca = sn_float_mp_classify(a);
    cb = sn_float_mp_classify(b);
    if (ca == SN_FP_NAN || cb == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    st = sn_float_mp_cmp(ctx, &rel, a, b);
    if (st != SN_OK) return st;
    if (rel > 0)
        return sn_float_mp_sub(ctx, out, a, b, opt);
    return sn_float_set_zero(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
}

/*
 * remquo: remainder as nearest-integer quotient (IEEE remainder / frem),
 * store low bits of quotient (signed, at least 3 bits) into *quo.
 */
sn_status sn_float_mp_remquo(sn_ctx *ctx, sn_value *out, int *quo, const sn_value *a, const sn_value *b,
                             const sn_op_opt *opt)
{
    sn_status st;
    sn_fmp ua, ub, un, ut, ur;
    sn_fpclass ca, cb;
    int qsign;
    uint64_t mag;
    int i, bl, bits;

    if (!quo) return SN_ERR_ARG;
    if (!a || !b || a->kind != SN_KIND_FLOAT || b->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;

    *quo = 0;
    ca = sn_float_mp_classify(a);
    cb = sn_float_mp_classify(b);
    if (ca == SN_FP_NAN || cb == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (cb == SN_FP_ZERO || ca == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, sn_float_mp_signbit(a), a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (ca == SN_FP_ZERO || cb == SN_FP_INFINITE) {
        *quo = 0;
        return sn_value_copy(ctx, out, a);
    }

    fmp_init(&ua); fmp_init(&ub); fmp_init(&un); fmp_init(&ut); fmp_init(&ur);
    st = fmp_unpack(ctx, a, &ua); if (st != SN_OK) goto done;
    st = fmp_unpack(ctx, b, &ub); if (st != SN_OK) goto done;
    st = fmp_nearest_quot(ctx, &un, &ua, &ub); if (st != SN_OK) goto done;

    /* extract low bits of integer |n| into *quo with sign of a/b */
    qsign = ua.sign ^ ub.sign;
    mag = 0;
    if (!un.is_zero && !un.is_nan && !un.is_inf) {
        bl = sig_bitlen(&un.sig);
        bits = bl < 31 ? bl : 31;
        for (i = 0; i < bits; i++) {
            if (sig_getbit(&un.sig, i))
                mag |= (uint64_t)1 << i;
        }
        /* if n has exp != 0 (scaled), shift would be needed; nearest quot uses exp=0 */
        if (un.exp > 0 && un.exp < 31) {
            mag <<= un.exp;
            mag &= 0x7fffffffu;
        }
    }
    if (qsign && mag != 0)
        *quo = -(int)mag;
    else
        *quo = (int)mag;

    st = fmp_mul(ctx, &ut, &un, &ub); if (st != SN_OK) goto done;
    st = fmp_add_sub(ctx, &ur, &ua, &ut, 1); if (st != SN_OK) goto done;
    if (ur.is_nan) sn_raise(ctx, SN_FLAG_INVALID);
    st = fmp_pack(ctx, out, &ur, a->e_bits, a->m_bits, a->nan_enabled, fmp_rnd(ctx, opt));
done:
    fmp_clear(ctx, &ua); fmp_clear(ctx, &ub);
    fmp_clear(ctx, &un); fmp_clear(ctx, &ut); fmp_clear(ctx, &ur);
    return st;
}



/* Exact-ish convert multiprec float to int64 (trunc toward zero).
 * Pure soft for exact integers; avoids host double beyond 2^53. */
sn_status sn_float_mp_to_i64(sn_ctx *ctx, const sn_value *v, int64_t *out)
{
    sn_fmp u, tr;
    sn_status st;
    int bl, i, bits;
    uint64_t mag;
    sn_fpclass c;

    if (!out || !v || v->kind != SN_KIND_FLOAT) return SN_ERR_ARG;
    c = sn_float_mp_classify(v);
    if (c == SN_FP_NAN) {
        sn_raise(ctx, SN_FLAG_INVALID);
        *out = 0;
        return SN_ERR_INVALID;
    }
    if (c == SN_FP_INFINITE) {
        sn_raise(ctx, SN_FLAG_INVALID);
        *out = sn_float_mp_signbit(v) ? INT64_MIN : INT64_MAX;
        return SN_ERR_RANGE;
    }
    if (c == SN_FP_ZERO) {
        *out = 0;
        return SN_OK;
    }

    fmp_init(&u);
    fmp_init(&tr);
    st = fmp_unpack(ctx, v, &u);
    if (st != SN_OK) goto done;
    /* trunc toward zero into integer significand (exp=0) */
    st = fmp_round_int_mode(ctx, &tr, &u, 0); /* 0 = trunc */
    if (st != SN_OK) goto done;
    if (tr.is_zero || sig_bitlen(&tr.sig) == 0) {
        *out = 0;
        st = SN_OK;
        goto done;
    }
    /* integer |n| with exp scale */
    bl = sig_bitlen(&tr.sig);
    if (tr.exp < 0) {
        /* should not happen after trunc */
        *out = 0;
        st = SN_OK;
        goto done;
    }
    if (tr.exp > 0) {
        /* value = sig * 2^exp; bit length = bl + exp */
        if (bl > 63 || tr.exp > 63 || bl + (int)tr.exp > 63) {
            sn_raise(ctx, SN_FLAG_OVERFLOW);
            *out = tr.sign ? INT64_MIN : INT64_MAX;
            st = SN_ERR_RANGE;
            goto done;
        }
        st = sig_shl(ctx, &tr.sig, (int)tr.exp);
        if (st != SN_OK) goto done;
        tr.exp = 0;
        bl = sig_bitlen(&tr.sig);
    }
    if (bl > 63) {
        sn_raise(ctx, SN_FLAG_OVERFLOW);
        *out = tr.sign ? INT64_MIN : INT64_MAX;
        st = SN_ERR_RANGE;
        goto done;
    }
    mag = 0;
    bits = bl < 64 ? bl : 64;
    for (i = 0; i < bits; i++) {
        if (sig_getbit(&tr.sig, i))
            mag |= (uint64_t)1 << i;
    }
    if (!tr.sign) {
        if (mag > (uint64_t)INT64_MAX) {
            sn_raise(ctx, SN_FLAG_OVERFLOW);
            *out = INT64_MAX;
            st = SN_ERR_RANGE;
            goto done;
        }
        *out = (int64_t)mag;
    } else {
        if (mag > (uint64_t)INT64_MAX + 1ull) {
            sn_raise(ctx, SN_FLAG_OVERFLOW);
            *out = INT64_MIN;
            st = SN_ERR_RANGE;
            goto done;
        }
        if (mag == (uint64_t)INT64_MAX + 1ull)
            *out = INT64_MIN;
        else
            *out = -(int64_t)mag;
    }
    st = SN_OK;
done:
    fmp_clear(ctx, &u);
    fmp_clear(ctx, &tr);
    return st;
}
