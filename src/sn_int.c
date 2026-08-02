#include "internal/sn_impl.h"
#include <limits.h>

static int int_is_neg(const sn_value *v)
{
    const sn_limb *limbs;
    int top, rem;
    if (!v) return 0;
    if (v->kind == SN_KIND_BIGINT)
        return v->negative && !(v->nlimbs == 1 && SN_CLIMBS(v)[0] == 0);
    if (v->kind != SN_KIND_INT || !v->is_signed) return 0;
    limbs = SN_CLIMBS(v);
    top = (v->width - 1) / SN_LIMB_BITS;
    rem = (v->width - 1) % SN_LIMB_BITS;
    if (top < v->nlimbs && (limbs[top] & ((sn_limb)1 << rem)))
        return 1;
    return 0;
}

static int value_sgn(const sn_value *v)
{
    const sn_limb *limbs;
    int i;
    if (!v) return 0;
    if (v->kind == SN_KIND_BIGINT) {
        if (v->nlimbs == 1 && SN_CLIMBS(v)[0] == 0) return 0;
        return v->negative ? -1 : 1;
    }
    if (v->kind != SN_KIND_INT) return 0;
    if (int_is_neg(v)) return -1;
    limbs = SN_CLIMBS(v);
    for (i = 0; i < v->nlimbs; i++)
        if (limbs[i]) return 1;
    return 0;
}

static void int_neg_tc_inplace(sn_value *v)
{
    sn_limb *limbs = SN_LIMBS(v);
    uint64_t carry = 1;
    int i;
    for (i = 0; i < v->nlimbs; i++) {
        uint64_t x = ((uint64_t)(~limbs[i]) & SN_LIMB_MASK) + carry;
        limbs[i] = (sn_limb)(x & SN_LIMB_MASK);
        carry = x >> SN_LIMB_BITS;
    }
    sn_int_mask(v);
}

static sn_status to_mag(sn_ctx *ctx, sn_value *mag, const sn_value *v)
{
    sn_status st = sn_value_copy(ctx, mag, v);
    if (st != SN_OK) return st;
    if (v->kind == SN_KIND_INT) {
        if (int_is_neg(v)) int_neg_tc_inplace(mag);
        mag->kind = SN_KIND_BIGINT;
        mag->width = 0;
        mag->is_signed = 0;
        mag->negative = 0;
        sn_bigint_normalize(mag);
    } else if (v->kind == SN_KIND_BIGINT) {
        mag->negative = 0;
        sn_bigint_normalize(mag);
    } else {
        return SN_ERR_TYPE;
    }
    return SN_OK;
}

static sn_status pack_int(sn_ctx *ctx, sn_value *out, int width, int is_signed,
                          const sn_value *mag, int negative, sn_int_overflow iov)
{
    sn_status st;
    const sn_limb *ml;
    sn_limb *dl;
    int n, mn, i, overflow = 0;
    sn_value tmp;

    if (!mag || mag->kind != SN_KIND_BIGINT) return SN_ERR_ARG;
    n = sn_limbs_for_bits(width);
    if (n < 1) return SN_ERR_NOMEM;
    mn = mag->nlimbs;
    ml = SN_CLIMBS(mag);

    sn_value_init(&tmp);
    st = sn_value_copy(ctx, &tmp, mag);
    if (st != SN_OK) return st;
    tmp.kind = SN_KIND_INT;
    tmp.width = width;
    tmp.is_signed = is_signed ? 1 : 0;
    st = sn_value_reserve(ctx, &tmp, n > mn ? n : mn);
    if (st != SN_OK) { sn_value_clear(ctx, &tmp); return st; }
    dl = SN_LIMBS(&tmp);
    for (i = 0; i < (n > mn ? n : mn); i++)
        dl[i] = (i < mn) ? ml[i] : 0;
    tmp.nlimbs = n > mn ? n : mn;

    if (negative) {
        uint64_t carry = 1;
        tmp.nlimbs = tmp.nlimbs > n ? tmp.nlimbs : n;
        for (i = 0; i < tmp.nlimbs; i++) {
            uint64_t x = ((uint64_t)(~dl[i]) & SN_LIMB_MASK) + carry;
            dl[i] = (sn_limb)(x & SN_LIMB_MASK);
            carry = x >> SN_LIMB_BITS;
        }
        if (carry) {
            st = sn_value_reserve(ctx, &tmp, tmp.nlimbs + 1);
            if (st != SN_OK) { sn_value_clear(ctx, &tmp); return st; }
            dl = SN_LIMBS(&tmp);
            dl[tmp.nlimbs] = 1;
            tmp.nlimbs++;
        }
    }

    {
        int full = width / SN_LIMB_BITS;
        int rem = width % SN_LIMB_BITS;
        for (i = n; i < tmp.nlimbs; i++)
            if (dl[i] != 0) overflow = 1;
        if (rem != 0 && full < tmp.nlimbs) {
            sn_limb mask = ((sn_limb)1 << rem) - 1u;
            if (dl[full] & ~mask) overflow = 1;
        }
        /* Signed: magnitude must fit two's-complement range before width mask. */
        if (!overflow && is_signed && width > 0) {
            /* positive: mag <= 2^(w-1)-1; negative: mag <= 2^(w-1) */
            sn_value lim; sn_limb *ll; int ln, j, cmp;
            sn_value_init(&lim);
            ln = sn_limbs_for_bits(width);
            if (sn_value_reserve(ctx, &lim, ln + 1) == SN_OK) {
                lim.kind = SN_KIND_BIGINT; lim.nlimbs = ln + 1; lim.negative = 0;
                ll = SN_LIMBS(&lim);
                for (j = 0; j < ln + 1; j++) ll[j] = 0;
                /* 2^(width-1) */
                ll[(width - 1) / SN_LIMB_BITS] = (sn_limb)1 << ((width - 1) % SN_LIMB_BITS);
                lim.nlimbs = ((width - 1) / SN_LIMB_BITS) + 1;
                /* compare |mag| (before 2's complement) via original mag */
                cmp = sn_limb_cmp(SN_CLIMBS(mag), mag->nlimbs, SN_CLIMBS(&lim), lim.nlimbs);
                if (!negative) {
                    /* need mag < 2^(w-1) i.e. cmp < 0 */
                    if (cmp >= 0) overflow = 1;
                } else {
                    /* mag <= 2^(w-1) ok; mag > 2^(w-1) overflows */
                    if (cmp > 0) overflow = 1;
                }
            }
            sn_value_clear(ctx, &lim);
        }
    }
    if (overflow) sn_raise(ctx, SN_FLAG_OVERFLOW);

    if (overflow && iov == SN_IOV_SATURATE) {
        st = sn_int_set_zero(ctx, out, width, is_signed);
        sn_value_clear(ctx, &tmp);
        if (st != SN_OK) return st;
        dl = SN_LIMBS(out);
        if (!is_signed) {
            for (i = 0; i < n; i++) dl[i] = SN_LIMB_MASK;
            sn_int_mask(out);
        } else if (negative) {
            for (i = 0; i < n; i++) dl[i] = 0;
            dl[(width - 1) / SN_LIMB_BITS] = (sn_limb)1 << ((width - 1) % SN_LIMB_BITS);
        } else {
            for (i = 0; i < n; i++) dl[i] = SN_LIMB_MASK;
            sn_int_mask(out);
            dl[(width - 1) / SN_LIMB_BITS] &= ~((sn_limb)1 << ((width - 1) % SN_LIMB_BITS));
        }
        return SN_OK;
    }

    st = sn_int_set_zero(ctx, out, width, is_signed);
    if (st != SN_OK) { sn_value_clear(ctx, &tmp); return st; }
    dl = SN_LIMBS(out);
    ml = SN_CLIMBS(&tmp);
    for (i = 0; i < n; i++)
        dl[i] = (i < tmp.nlimbs) ? ml[i] : 0;
    out->nlimbs = n;
    sn_int_mask(out);
    sn_value_clear(ctx, &tmp);
    return SN_OK;
}

static sn_status pack_bigint(sn_ctx *ctx, sn_value *out, const sn_value *mag, int negative)
{
    sn_status st = sn_value_copy(ctx, out, mag);
    if (st != SN_OK) return st;
    out->kind = SN_KIND_BIGINT;
    out->width = 0;
    out->is_signed = 0;
    out->negative = negative ? 1 : 0;
    sn_bigint_normalize(out);
    return SN_OK;
}

static sn_status resolve_bin(const sn_value *a, const sn_value *b,
                             int *out_kind, int *width, int *is_signed)
{
    if (!sn_value_is_num(a) || !sn_value_is_num(b)) return SN_ERR_TYPE;
    if (a->kind == SN_KIND_BIGINT || b->kind == SN_KIND_BIGINT) {
        *out_kind = SN_KIND_BIGINT; *width = 0; *is_signed = 0; return SN_OK;
    }
    *out_kind = SN_KIND_INT;
    *width = a->width > b->width ? a->width : b->width;
    *is_signed = (a->is_signed || b->is_signed) ? 1 : 0;
    return SN_OK;
}

sn_status sn_int_new(sn_ctx *ctx, sn_value *out, int width, int is_signed)
{ return sn_int_set_zero(ctx, out, width, is_signed); }

sn_status sn_int_set_u64(sn_ctx *ctx, sn_value *out, uint64_t x, int width, int is_signed)
{
    sn_status st; sn_limb limbs[2]; sn_value mag;
    if (!out || width < 1) return SN_ERR_ARG;
    sn_limbs_from_u64(limbs, 2, x);
    sn_value_init(&mag); mag.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &mag, 2); if (st != SN_OK) return st;
    SN_LIMBS(&mag)[0] = limbs[0]; SN_LIMBS(&mag)[1] = limbs[1]; mag.nlimbs = 2;
    sn_bigint_normalize(&mag);
    st = pack_int(ctx, out, width, is_signed, &mag, 0, sn_eff_iov(ctx, NULL));
    sn_value_clear(ctx, &mag); return st;
}

sn_status sn_int_set_i64(sn_ctx *ctx, sn_value *out, int64_t x, int width, int is_signed)
{
    sn_status st; int neg = 0; uint64_t ux; sn_limb limbs[2]; sn_value mag;
    if (!out || width < 1) return SN_ERR_ARG;
    if (x < 0) { neg = 1; ux = (uint64_t)(-(x + 1)) + 1u; } else ux = (uint64_t)x;
    sn_limbs_from_u64(limbs, 2, ux);
    sn_value_init(&mag); mag.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &mag, 2); if (st != SN_OK) return st;
    SN_LIMBS(&mag)[0] = limbs[0]; SN_LIMBS(&mag)[1] = limbs[1]; mag.nlimbs = 2;
    sn_bigint_normalize(&mag);
    st = pack_int(ctx, out, width, is_signed, &mag, neg, sn_eff_iov(ctx, NULL));
    sn_value_clear(ctx, &mag); return st;
}

sn_status sn_int_set_long(sn_ctx *ctx, sn_value *out, long x, int width, int is_signed)
{ return sn_int_set_i64(ctx, out, (int64_t)x, width, is_signed); }

sn_status sn_bigint_set_u64(sn_ctx *ctx, sn_value *out, uint64_t x)
{
    sn_status st; sn_limb limbs[2];
    if (!out) return SN_ERR_ARG;
    sn_value_clear(ctx, out); out->kind = SN_KIND_BIGINT; out->negative = 0;
    sn_limbs_from_u64(limbs, 2, x);
    st = sn_value_reserve(ctx, out, 2); if (st != SN_OK) return st;
    SN_LIMBS(out)[0] = limbs[0]; SN_LIMBS(out)[1] = limbs[1]; out->nlimbs = 2;
    sn_bigint_normalize(out); return SN_OK;
}

sn_status sn_bigint_set_i64(sn_ctx *ctx, sn_value *out, int64_t x)
{
    sn_status st;
    if (x < 0) {
        uint64_t ux = (uint64_t)(-(x + 1)) + 1u;
        st = sn_bigint_set_u64(ctx, out, ux); if (st != SN_OK) return st;
        out->negative = 1; sn_bigint_normalize(out); return SN_OK;
    }
    return sn_bigint_set_u64(ctx, out, (uint64_t)x);
}

sn_status sn_bigint_set_long(sn_ctx *ctx, sn_value *out, long x)
{ return sn_bigint_set_i64(ctx, out, (int64_t)x); }

sn_status sn_i8 (sn_ctx *ctx, sn_value *out, int64_t x)  { return sn_int_set_i64(ctx, out, x, 8, 1); }
sn_status sn_u8 (sn_ctx *ctx, sn_value *out, uint64_t x) { return sn_int_set_u64(ctx, out, x, 8, 0); }
sn_status sn_i16(sn_ctx *ctx, sn_value *out, int64_t x)  { return sn_int_set_i64(ctx, out, x, 16, 1); }
sn_status sn_u16(sn_ctx *ctx, sn_value *out, uint64_t x) { return sn_int_set_u64(ctx, out, x, 16, 0); }
sn_status sn_i32(sn_ctx *ctx, sn_value *out, int64_t x)  { return sn_int_set_i64(ctx, out, x, 32, 1); }
sn_status sn_u32(sn_ctx *ctx, sn_value *out, uint64_t x) { return sn_int_set_u64(ctx, out, x, 32, 0); }
sn_status sn_i64(sn_ctx *ctx, sn_value *out, int64_t x)  { return sn_int_set_i64(ctx, out, x, 64, 1); }
sn_status sn_u64(sn_ctx *ctx, sn_value *out, uint64_t x) { return sn_int_set_u64(ctx, out, x, 64, 0); }

static sn_status bin_arith(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                           const sn_op_opt *opt, int op)
{
    sn_status st; int kind, width, is_signed; sn_value ma, mb, mr, mq;
    int sa, sb, sr; sn_int_overflow iov;
    if (!ctx || !out || !a || !b) return SN_ERR_ARG;
    st = resolve_bin(a, b, &kind, &width, &is_signed); if (st != SN_OK) return st;
    iov = sn_eff_iov(ctx, opt);
    sn_value_init(&ma); sn_value_init(&mb); sn_value_init(&mr); sn_value_init(&mq);
    st = to_mag(ctx, &ma, a); if (st != SN_OK) goto done;
    st = to_mag(ctx, &mb, b); if (st != SN_OK) goto done;
    sa = value_sgn(a); sb = value_sgn(b);

    if (op == 0 || op == 1) {
        int sign_a = (sa < 0);
        int sign_b = (sb < 0) ^ (op == 1);
        if (sign_a == sign_b) {
            st = sn_limb_add(ctx, &mr, SN_CLIMBS(&ma), ma.nlimbs, SN_CLIMBS(&mb), mb.nlimbs);
            sr = sign_a;
        } else {
            int cmp = sn_limb_cmp(SN_CLIMBS(&ma), ma.nlimbs, SN_CLIMBS(&mb), mb.nlimbs);
            if (cmp >= 0) {
                st = sn_limb_sub(ctx, &mr, SN_CLIMBS(&ma), ma.nlimbs, SN_CLIMBS(&mb), mb.nlimbs);
                sr = sign_a;
            } else {
                st = sn_limb_sub(ctx, &mr, SN_CLIMBS(&mb), mb.nlimbs, SN_CLIMBS(&ma), ma.nlimbs);
                sr = sign_b;
            }
            if (mr.nlimbs == 1 && SN_CLIMBS(&mr)[0] == 0) sr = 0;
        }
        if (st != SN_OK) goto done;
    } else if (op == 2) {
        st = sn_limb_mul(ctx, &mr, SN_CLIMBS(&ma), ma.nlimbs, SN_CLIMBS(&mb), mb.nlimbs);
        sr = (sa == 0 || sb == 0) ? 0 : ((sa < 0) ^ (sb < 0));
        if (st != SN_OK) goto done;
    } else {
        if (sb == 0) {
            sn_raise(ctx, SN_FLAG_DIVZERO);
            if (kind == SN_KIND_INT) st = sn_int_set_zero(ctx, out, width, is_signed);
            else st = sn_bigint_set_u64(ctx, out, 0);
            if (st == SN_OK) st = SN_ERR_DIVZERO;
            goto done;
        }
        st = sn_limb_divmod(ctx, &mq, &mr, SN_CLIMBS(&ma), ma.nlimbs, SN_CLIMBS(&mb), mb.nlimbs);
        if (st != SN_OK) goto done;
        if (op == 3) {
            sn_value_clear(ctx, &mr); sn_value_move(&mr, &mq); sn_value_init(&mq);
            sr = (sa == 0 || sb == 0) ? 0 : ((sa < 0) ^ (sb < 0));
        } else {
            sr = (sa < 0) ? 1 : 0;
            if (mr.nlimbs == 1 && SN_CLIMBS(&mr)[0] == 0) sr = 0;
        }
    }
    mr.kind = SN_KIND_BIGINT;
    if (kind == SN_KIND_INT) st = pack_int(ctx, out, width, is_signed, &mr, sr, iov);
    else st = pack_bigint(ctx, out, &mr, sr);
done:
    sn_value_clear(ctx, &ma); sn_value_clear(ctx, &mb);
    sn_value_clear(ctx, &mr); sn_value_clear(ctx, &mq);
    return st;
}

sn_status sn_add(sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT)
        return sn_float_add(c, o, a, b, opt);
    return bin_arith(c, o, a, b, opt, 0);
}
sn_status sn_sub(sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT)
        return sn_float_sub(c, o, a, b, opt);
    return bin_arith(c, o, a, b, opt, 1);
}
sn_status sn_mul(sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT)
        return sn_float_mul(c, o, a, b, opt);
    return bin_arith(c, o, a, b, opt, 2);
}
sn_status sn_div(sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT)
        return sn_float_div(c, o, a, b, opt);
    return bin_arith(c, o, a, b, opt, 3);
}
sn_status sn_rem(sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (a && a->kind == SN_KIND_FLOAT) return SN_ERR_TYPE; /* use sn_frem later */
    return bin_arith(c, o, a, b, opt, 4);
}

sn_status sn_neg(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st; sn_value z;
    if (a && a->kind == SN_KIND_FLOAT) return sn_float_neg(ctx, out, a, opt);
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;
    sn_value_init(&z);
    if (a->kind == SN_KIND_INT) st = sn_int_set_u64(ctx, &z, 0, a->width, a->is_signed);
    else st = sn_bigint_set_u64(ctx, &z, 0);
    if (st != SN_OK) return st;
    st = sn_sub(ctx, out, &z, a, opt);
    sn_value_clear(ctx, &z); return st;
}

sn_status sn_abs(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (a && a->kind == SN_KIND_FLOAT) return sn_float_abs(ctx, out, a, opt);
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;
    if (value_sgn(a) < 0) return sn_neg(ctx, out, a, opt);
    return sn_value_copy(ctx, out, a);
}

/* BIGINT bitwise with GMP two's-complement sign extension (mini-gmp style).
 * op: 0=and, 1=or, 2=xor. No asm / no __int128. */
static sn_status bin_bit_bigint(sn_ctx *ctx, sn_value *out,
                                const sn_value *a, const sn_value *b, int op)
{
    sn_status st;
    sn_value ma, mb, tmp;
    const sn_limb *la, *lb;
    sn_limb *ld;
    int un, vn, rn, i;
    sn_limb uc, vc, rc;   /* 0 or 1: still converting from sign-magnitude */
    sn_limb ux, vx, rx;   /* 0 or all-ones mask for sign extension */
    int sa, sb, sneg;

    sa = value_sgn(a) < 0;
    sb = value_sgn(b) < 0;
    sn_value_init(&ma); sn_value_init(&mb); sn_value_init(&tmp);

    st = to_mag(ctx, &ma, a); if (st != SN_OK) goto done;
    st = to_mag(ctx, &mb, b); if (st != SN_OK) goto done;

    un = ma.nlimbs; if (un < 1) un = 1;
    vn = mb.nlimbs; if (vn < 1) vn = 1;
    /* Ensure ma is the longer magnitude (swap roles if needed). */
    if (un < vn) {
        sn_value t = ma; ma = mb; mb = t;
        { int ts = sa; sa = sb; sb = ts; }
        { int tn = un; un = vn; vn = tn; }
    }

    uc = sa ? 1u : 0u;
    vc = sb ? 1u : 0u;
    if (op == 0) rc = (sn_limb)(uc & vc);       /* and */
    else if (op == 1) rc = (sn_limb)(uc | vc);  /* or  */
    else rc = (sn_limb)(uc ^ vc);               /* xor */
    ux = uc ? SN_LIMB_MASK : 0;
    vx = vc ? SN_LIMB_MASK : 0;
    rx = rc ? SN_LIMB_MASK : 0;

    /* Result limb count (mirrors mini-gmp): */
    if (op == 0) rn = vx ? un : vn;             /* and: if smaller positive, higher don't matter */
    else if (op == 1) rn = vx ? vn : un;        /* or: if smaller negative, higher don't matter */
    else rn = un;                               /* xor: always full longer */

    tmp.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &tmp, rn + 1); if (st != SN_OK) goto done;
    la = SN_CLIMBS(&ma); lb = SN_CLIMBS(&mb); ld = SN_LIMBS(&tmp);

    for (i = 0; i < vn; i++) {
        sn_limb ul = (sn_limb)(((la[i] ^ ux) + uc) & SN_LIMB_MASK);
        sn_limb uc_next = (sn_limb)((((uint64_t)(la[i] ^ ux) + uc) >> SN_LIMB_BITS) & 1u);
        sn_limb vl = (sn_limb)(((lb[i] ^ vx) + vc) & SN_LIMB_MASK);
        sn_limb vc_next = (sn_limb)((((uint64_t)(lb[i] ^ vx) + vc) >> SN_LIMB_BITS) & 1u);
        sn_limb bits, rl, rc_next;
        if (op == 0) bits = (sn_limb)(ul & vl);
        else if (op == 1) bits = (sn_limb)(ul | vl);
        else bits = (sn_limb)(ul ^ vl);
        rl = (sn_limb)(((bits ^ rx) + rc) & SN_LIMB_MASK);
        rc_next = (sn_limb)((((uint64_t)(bits ^ rx) + rc) >> SN_LIMB_BITS) & 1u);
        ld[i] = rl;
        uc = uc_next; vc = vc_next; rc = rc_next;
    }
    /* smaller exhausted: vc must be 0 after finishing shorter mag (GMP asserts). */
    for (; i < rn; i++) {
        sn_limb ul = (sn_limb)(((la[i] ^ ux) + uc) & SN_LIMB_MASK);
        sn_limb uc_next = (sn_limb)((((uint64_t)(la[i] ^ ux) + uc) >> SN_LIMB_BITS) & 1u);
        sn_limb bits, rl, rc_next, comb;
        if (op == 0) {
            bits = (sn_limb)(ul & vx);
            comb = (sn_limb)(bits ^ rx);
        } else if (op == 1) {
            bits = (sn_limb)(ul | vx);
            comb = (sn_limb)(bits ^ rx);
        } else {
            /* mini-gmp xor tail: rl = (ul ^ ux) + rc  (rx already folded into ux/rc) */
            comb = (sn_limb)(ul ^ ux);
        }
        rl = (sn_limb)((comb + rc) & SN_LIMB_MASK);
        rc_next = (sn_limb)((((uint64_t)comb + rc) >> SN_LIMB_BITS) & 1u);
        ld[i] = rl;
        uc = uc_next; rc = rc_next;
    }
    if (rc) {
        ld[rn++] = rc;
    }
    tmp.nlimbs = rn;
    tmp.negative = 0;
    sn_bigint_normalize(&tmp);
    sneg = (rx != 0);
    st = pack_bigint(ctx, out, &tmp, sneg);
done:
    sn_value_clear(ctx, &ma); sn_value_clear(ctx, &mb); sn_value_clear(ctx, &tmp);
    return st;
}

static sn_status bin_bit(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                         const sn_op_opt *opt, int op)
{
    sn_status st; int kind, width, is_signed; int n, i;
    sn_value aa, bb, tmp; sn_limb *ld; const sn_limb *la, *lb;
    (void)opt;
    st = resolve_bin(a, b, &kind, &width, &is_signed); if (st != SN_OK) return st;

    if (kind == SN_KIND_BIGINT)
        return bin_bit_bigint(ctx, out, a, b, op);

    n = sn_limbs_for_bits(width);
    if (n < 1) return SN_ERR_NOMEM;
    sn_value_init(&aa); sn_value_init(&bb); sn_value_init(&tmp);
    {
        sn_value ma; sn_value_init(&ma);
        st = to_mag(ctx, &ma, a);
        if (st == SN_OK) st = pack_int(ctx, &aa, width, is_signed, &ma, value_sgn(a)<0, SN_IOV_WRAP);
        sn_value_clear(ctx, &ma); sn_value_init(&ma);
        if (st == SN_OK) st = to_mag(ctx, &ma, b);
        if (st == SN_OK) st = pack_int(ctx, &bb, width, is_signed, &ma, value_sgn(b)<0, SN_IOV_WRAP);
        sn_value_clear(ctx, &ma);
    }
    if (st != SN_OK) { sn_value_clear(ctx,&aa); sn_value_clear(ctx,&bb); return st; }
    st = sn_int_set_zero(ctx, &tmp, width, is_signed);
    if (st != SN_OK) { sn_value_clear(ctx,&aa); sn_value_clear(ctx,&bb); return st; }
    la = SN_CLIMBS(&aa); lb = SN_CLIMBS(&bb); ld = SN_LIMBS(&tmp);
    for (i = 0; i < n; i++)
        ld[i] = (op==0) ? (la[i]&lb[i]) : (op==1) ? (la[i]|lb[i]) : (la[i]^lb[i]);
    sn_int_mask(&tmp);
    st = sn_value_copy(ctx, out, &tmp);
    sn_value_clear(ctx, &aa); sn_value_clear(ctx, &bb); sn_value_clear(ctx, &tmp);
    return st;
}

sn_status sn_and(sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt){return bin_bit(c,o,a,b,opt,0);}
sn_status sn_or (sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt){return bin_bit(c,o,a,b,opt,1);}
sn_status sn_xor(sn_ctx *c, sn_value *o, const sn_value *a, const sn_value *b, const sn_op_opt *opt){return bin_bit(c,o,a,b,opt,2);}

/* ~x for INT: width mask. For BIGINT: GMP mpz_com = -(x+1). */
sn_status sn_not(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st; sn_value tmp, one; const sn_limb *l; sn_limb *d; int n,i;
    (void)opt;
    if (!a || !sn_value_is_num(a)) return SN_ERR_TYPE;
    if (a->kind == SN_KIND_BIGINT) {
        /* ~x == -x - 1 == -(x + 1) */
        sn_value_init(&one); sn_value_init(&tmp);
        st = sn_bigint_set_u64(ctx, &one, 1);
        if (st == SN_OK) st = sn_add(ctx, &tmp, a, &one, NULL);
        if (st == SN_OK) st = sn_neg(ctx, out, &tmp, NULL);
        sn_value_clear(ctx, &one); sn_value_clear(ctx, &tmp);
        return st;
    }
    if (a->kind != SN_KIND_INT) return SN_ERR_TYPE;
    n = sn_limbs_for_bits(a->width);
    if (n < 1) return SN_ERR_NOMEM;
    sn_value_init(&tmp);
    st = sn_int_set_zero(ctx, &tmp, a->width, a->is_signed); if (st != SN_OK) return st;
    l = SN_CLIMBS(a); d = SN_LIMBS(&tmp);
    for (i = 0; i < n; i++) d[i] = (i < a->nlimbs) ? (~l[i] & SN_LIMB_MASK) : SN_LIMB_MASK;
    sn_int_mask(&tmp);
    st = sn_value_copy(ctx, out, &tmp); sn_value_clear(ctx, &tmp); return st;
}

sn_status sn_shl(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt)
{
    sn_status st; sn_value mag, shifted; const sn_limb *src; sn_limb *dst;
    int n,i,limb_shift,bit_shift; sn_int_overflow iov;
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;
    if (bits < 0) return sn_shr(ctx, out, a, -bits, opt);
    if (bits == 0) return sn_value_copy(ctx, out, a);
    iov = sn_eff_iov(ctx, opt);
    sn_value_init(&mag); sn_value_init(&shifted);
    st = to_mag(ctx, &mag, a); if (st != SN_OK) goto done;
    limb_shift = bits / SN_LIMB_BITS; bit_shift = bits % SN_LIMB_BITS;
    n = mag.nlimbs + limb_shift + 1;
    shifted.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &shifted, n); if (st != SN_OK) goto done;
    dst = SN_LIMBS(&shifted); src = SN_CLIMBS(&mag);
    for (i = 0; i < n; i++) dst[i] = 0;
    for (i = 0; i < mag.nlimbs; i++) {
        uint64_t x = (uint64_t)src[i] << bit_shift;
        dst[i + limb_shift] |= (sn_limb)(x & SN_LIMB_MASK);
        if (bit_shift) dst[i + limb_shift + 1] |= (sn_limb)(x >> SN_LIMB_BITS);
    }
    shifted.nlimbs = n; sn_bigint_normalize(&shifted);
    if (a->kind == SN_KIND_INT)
        st = pack_int(ctx, out, a->width, a->is_signed, &shifted, value_sgn(a)<0, iov);
    else
        st = pack_bigint(ctx, out, &shifted, value_sgn(a)<0);
done:
    sn_value_clear(ctx, &mag); sn_value_clear(ctx, &shifted); return st;
}

/* Logical right shift: zero-fill high bits (INT 2's complement bit pattern).
 * BIGINT: shift absolute magnitude right; keep sign (no sign-fill). */
sn_status sn_shr(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt)
{
    sn_status st; sn_value res; const sn_limb *src; sn_limb *buf;
    int n,i,limb_shift,bit_shift;
    (void)opt;
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;
    if (bits < 0) return sn_shl(ctx, out, a, -bits, opt);
    if (bits == 0) return sn_value_copy(ctx, out, a);

    if (a->kind == SN_KIND_BIGINT) {
        sn_value mag, shifted;
        sn_value_init(&mag); sn_value_init(&shifted);
        st = to_mag(ctx, &mag, a); if (st != SN_OK) { sn_value_clear(ctx,&mag); return st; }
        limb_shift = bits / SN_LIMB_BITS; bit_shift = bits % SN_LIMB_BITS;
        n = mag.nlimbs; if (n < 1) n = 1;
        shifted.kind = SN_KIND_BIGINT;
        st = sn_value_reserve(ctx, &shifted, n); if (st != SN_OK) { sn_value_clear(ctx,&mag); return st; }
        buf = SN_LIMBS(&shifted); src = SN_CLIMBS(&mag);
        for (i = 0; i < n; i++) {
            int j = i + limb_shift; uint64_t x = 0;
            if (j < mag.nlimbs) x = src[j];
            if (bit_shift && j + 1 < mag.nlimbs) x |= (uint64_t)src[j+1] << SN_LIMB_BITS;
            buf[i] = (sn_limb)((x >> bit_shift) & SN_LIMB_MASK);
        }
        shifted.nlimbs = n; sn_bigint_normalize(&shifted);
        st = pack_bigint(ctx, out, &shifted, value_sgn(a)<0);
        sn_value_clear(ctx, &mag); sn_value_clear(ctx, &shifted); return st;
    }

    n = sn_limbs_for_bits(a->width);
    if (n < 1) return SN_ERR_NOMEM;
    sn_value_init(&res);
    st = sn_int_set_zero(ctx, &res, a->width, a->is_signed); if (st != SN_OK) return st;
    src = SN_CLIMBS(a); buf = SN_LIMBS(&res);
    limb_shift = bits / SN_LIMB_BITS; bit_shift = bits % SN_LIMB_BITS;
    for (i = 0; i < n; i++) {
        int j = i + limb_shift; uint64_t x = 0;
        if (j < n) x = src[j];
        if (bit_shift && j + 1 < n) x |= (uint64_t)src[j+1] << SN_LIMB_BITS;
        buf[i] = (sn_limb)((x >> bit_shift) & SN_LIMB_MASK);
    }
    sn_int_mask(&res);
    st = sn_value_copy(ctx, out, &res); sn_value_clear(ctx, &res); return st;
}

/* Arithmetic right shift: sign-fill for signed INT; BIGINT floor-div by 2^bits. */
sn_status sn_sar(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt)
{
    sn_status st; sn_value res; const sn_limb *src; sn_limb *buf;
    int n,i,limb_shift,bit_shift,neg;
    (void)opt;
    if (!sn_value_is_num(a)) return SN_ERR_TYPE;
    if (bits < 0) return sn_shl(ctx, out, a, -bits, opt);
    if (bits == 0) return sn_value_copy(ctx, out, a);

    if (a->kind == SN_KIND_BIGINT) {
        sn_value mag, shifted;
        int sticky = 0;
        sn_value_init(&mag); sn_value_init(&shifted);
        st = to_mag(ctx, &mag, a); if (st != SN_OK) { sn_value_clear(ctx,&mag); return st; }
        neg = value_sgn(a) < 0;
        limb_shift = bits / SN_LIMB_BITS; bit_shift = bits % SN_LIMB_BITS;
        n = mag.nlimbs; if (n < 1) n = 1;
        src = SN_CLIMBS(&mag);
        if (neg) {
            /* bits discarded? for floor toward -inf when negative */
            for (i = 0; i < limb_shift && i < mag.nlimbs; i++)
                if (src[i]) sticky = 1;
            if (bit_shift && limb_shift < mag.nlimbs) {
                sn_limb mask = (sn_limb)((1u << bit_shift) - 1u);
                if (src[limb_shift] & mask) sticky = 1;
            }
        }
        shifted.kind = SN_KIND_BIGINT;
        st = sn_value_reserve(ctx, &shifted, n + 1); if (st != SN_OK) { sn_value_clear(ctx,&mag); return st; }
        buf = SN_LIMBS(&shifted);
        for (i = 0; i < n; i++) {
            int j = i + limb_shift; uint64_t x = 0;
            if (j < mag.nlimbs) x = src[j];
            if (bit_shift && j + 1 < mag.nlimbs) x |= (uint64_t)src[j+1] << SN_LIMB_BITS;
            buf[i] = (sn_limb)((x >> bit_shift) & SN_LIMB_MASK);
        }
        shifted.nlimbs = n;
        if (neg && sticky) {
            /* add 1 to magnitude so result is more negative (floor) */
            uint64_t c = 1;
            for (i = 0; i < n && c; i++) {
                c += buf[i];
                buf[i] = (sn_limb)(c & SN_LIMB_MASK);
                c >>= SN_LIMB_BITS;
            }
            if (c) {
                buf[n] = (sn_limb)c;
                shifted.nlimbs = n + 1;
            }
        }
        sn_bigint_normalize(&shifted);
        st = pack_bigint(ctx, out, &shifted, neg);
        sn_value_clear(ctx, &mag); sn_value_clear(ctx, &shifted); return st;
    }

    /* Fixed-width INT: signed negative => sign fill; else same as logical */
    n = sn_limbs_for_bits(a->width);
    if (n < 1) return SN_ERR_NOMEM;
    sn_value_init(&res);
    st = sn_int_set_zero(ctx, &res, a->width, a->is_signed); if (st != SN_OK) return st;
    src = SN_CLIMBS(a); buf = SN_LIMBS(&res);
    limb_shift = bits / SN_LIMB_BITS; bit_shift = bits % SN_LIMB_BITS;
    for (i = 0; i < n; i++) {
        int j = i + limb_shift; uint64_t x = 0;
        if (j < n) x = src[j];
        if (bit_shift && j + 1 < n) x |= (uint64_t)src[j+1] << SN_LIMB_BITS;
        buf[i] = (sn_limb)((x >> bit_shift) & SN_LIMB_MASK);
    }
    if (a->is_signed && int_is_neg(a)) {
        int keep = a->width - bits;
        if (keep <= 0) {
            for (i = 0; i < n; i++) buf[i] = SN_LIMB_MASK;
        } else {
            for (i = keep; i < a->width; i++) {
                int li = i / SN_LIMB_BITS, bb = i % SN_LIMB_BITS;
                if (li < n) buf[li] |= (sn_limb)1 << bb;
            }
        }
    }
    sn_int_mask(&res);
    st = sn_value_copy(ctx, out, &res); sn_value_clear(ctx, &res); return st;
}

sn_status sn_cmp(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b)
{
    int sa, sb, c; sn_value ma, mb; sn_status st;
    if (a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT)
        return sn_float_cmp(ctx, rel, a, b);
    if (!rel || !sn_value_is_num(a) || !sn_value_is_num(b)) return SN_ERR_ARG;
    sa = value_sgn(a); sb = value_sgn(b);
    if (sa < sb) { *rel = -1; return SN_OK; }
    if (sa > sb) { *rel = 1; return SN_OK; }
    if (sa == 0) { *rel = 0; return SN_OK; }
    sn_value_init(&ma); sn_value_init(&mb);
    st = to_mag(ctx, &ma, a); if (st == SN_OK) st = to_mag(ctx, &mb, b);
    if (st != SN_OK) { sn_value_clear(ctx,&ma); sn_value_clear(ctx,&mb); return st; }
    c = sn_limb_cmp(SN_CLIMBS(&ma), ma.nlimbs, SN_CLIMBS(&mb), mb.nlimbs);
    if (sa < 0) c = -c;
    *rel = c;
    sn_value_clear(ctx, &ma); sn_value_clear(ctx, &mb); return SN_OK;
}

int sn_bitlen(const sn_value *v)
{
    const sn_limb *limbs; int i; sn_limb x; int b;
    if (!sn_value_is_num(v)) return 0;
    if (v->kind == SN_KIND_INT && int_is_neg(v)) return v->width;
    limbs = SN_CLIMBS(v);
    for (i = v->nlimbs - 1; i >= 0; i--) {
        if (!limbs[i]) continue;
        x = limbs[i]; b = 0; while (x) { b++; x >>= 1; }
        return i * SN_LIMB_BITS + b;
    }
    return 0;
}

sn_status sn_getbit(const sn_value *v, int i, int *bit)
{
    const sn_limb *limbs; int li, bb;
    if (!v || !bit || i < 0) return SN_ERR_ARG;
    if (!sn_value_is_num(v)) return SN_ERR_TYPE;
    if (v->kind == SN_KIND_INT && i >= v->width) { *bit = int_is_neg(v) ? 1 : 0; return SN_OK; }
    li = i / SN_LIMB_BITS; bb = i % SN_LIMB_BITS;
    limbs = SN_CLIMBS(v);
    if (li >= v->nlimbs) { *bit = 0; return SN_OK; }
    *bit = (int)((limbs[li] >> bb) & 1u); return SN_OK;
}

sn_status sn_setbit(sn_ctx *ctx, sn_value *v, int i, int bit)
{
    sn_limb *limbs; int li, bb, need; sn_status st;
    if (!v || i < 0) return SN_ERR_ARG;
    if (!sn_value_is_num(v)) return SN_ERR_TYPE;
    if (v->kind == SN_KIND_INT && i >= v->width) return SN_ERR_RANGE;
    li = i / SN_LIMB_BITS; bb = i % SN_LIMB_BITS; need = li + 1;
    st = sn_value_reserve(ctx, v, need); if (st != SN_OK) return st;
    limbs = SN_LIMBS(v);
    while (v->nlimbs < need) { limbs[v->nlimbs] = 0; v->nlimbs++; }
    if (bit) limbs[li] |= (sn_limb)1 << bb; else limbs[li] &= ~((sn_limb)1 << bb);
    if (v->kind == SN_KIND_INT) sn_int_mask(v); else sn_bigint_normalize(v);
    return SN_OK;
}

sn_status sn_to_u64(sn_ctx *ctx, const sn_value *v, uint64_t *out)
{
    sn_value mag; sn_status st;
    if (!out || !sn_value_is_num(v)) return SN_ERR_ARG;
    if (value_sgn(v) < 0) return SN_ERR_RANGE;
    sn_value_init(&mag);
    st = to_mag(ctx, &mag, v); if (st != SN_OK) return st;
    if (mag.nlimbs > 2) { sn_value_clear(ctx, &mag); return SN_ERR_RANGE; }
    *out = sn_limbs_to_u64(SN_CLIMBS(&mag), mag.nlimbs);
    sn_value_clear(ctx, &mag); return SN_OK;
}

sn_status sn_to_i64(sn_ctx *ctx, const sn_value *v, int64_t *out)
{
    if (v && v->kind == SN_KIND_FLOAT) return sn_float_to_i64(ctx, v, out, NULL);

    sn_value mag; sn_status st; int neg; uint64_t ux;
    if (!out || !sn_value_is_num(v)) return SN_ERR_ARG;
    neg = value_sgn(v) < 0;
    sn_value_init(&mag);
    st = to_mag(ctx, &mag, v); if (st != SN_OK) return st;
    if (mag.nlimbs > 2) { sn_value_clear(ctx, &mag); return SN_ERR_RANGE; }
    ux = sn_limbs_to_u64(SN_CLIMBS(&mag), mag.nlimbs);
    sn_value_clear(ctx, &mag);
    if (!neg) {
        if (ux > (uint64_t)INT64_MAX) return SN_ERR_RANGE;
        *out = (int64_t)ux;
    } else {
        if (ux > (uint64_t)INT64_MAX + 1ull) return SN_ERR_RANGE;
        if (ux == (uint64_t)INT64_MAX + 1ull) *out = INT64_MIN;
        else *out = -(int64_t)ux;
    }
    return SN_OK;
}

sn_status sn_to_long(sn_ctx *ctx, const sn_value *v, long *out)
{
    int64_t x; sn_status st = sn_to_i64(ctx, v, &x);
    if (st != SN_OK) return st;
    if (x < (int64_t)LONG_MIN || x > (int64_t)LONG_MAX) return SN_ERR_RANGE;
    *out = (long)x; return SN_OK;
}
