#include "internal/sn_impl.h"
#include <ctype.h>
#include <string.h>

static int digval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

/* Fast in-place mag = mag * m + add (single-limb factor). */
static sn_status mul_add_u32(sn_ctx *ctx, sn_value *mag, uint32_t m, uint32_t add)
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
        uint64_t p = (uint64_t)L[i] * (uint64_t)m + carry;
        L[i] = (sn_limb)(p & SN_LIMB_MASK);
        carry = p >> SN_LIMB_BITS;
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

static sn_status parse_mag(sn_ctx *ctx, sn_value *mag, const char *s, int base, int *neg_out)
{
    const char *p = s;
    int neg = 0;
    sn_status st;
    int any = 0;
    int frac_digits = 0;
    int exp_val = 0;
    int exp_neg = 0;
    int saw_dot = 0;
    int saw_exp = 0;

    if (!s) return SN_ERR_ARG;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') {
        if (*p == '-') neg = 1;
        p++;
    }
    if (base == 0) {
        base = 10;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16; p += 2;
        } else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
            base = 2; p += 2;
        }
    } else {
        if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        if (base == 2 && p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) p += 2;
    }
    if (base < 2 || base > 16) return SN_ERR_ARG;
    if (!*p) return SN_ERR_FORMAT;

    sn_value_clear(ctx, mag);
    mag->kind = SN_KIND_BIGINT;
    mag->negative = 0;
    st = sn_value_reserve(ctx, mag, 1);
    if (st != SN_OK) return st;
    SN_LIMBS(mag)[0] = 0;
    mag->nlimbs = 1;

    for (; *p; p++) {
        int d;
        if (*p == '_') continue;
        if (isspace((unsigned char)*p)) break;
        /* scientific: only decimal base, once */
        if (base == 10 && (*p == 'e' || *p == 'E')) {
            saw_exp = 1;
            p++;
            if (*p == '+' || *p == '-') {
                if (*p == '-') exp_neg = 1;
                p++;
            }
            if (!*p || digval((unsigned char)*p) < 0) return SN_ERR_FORMAT;
            for (; *p; p++) {
                if (*p == '_') continue;
                if (isspace((unsigned char)*p)) break;
                d = digval((unsigned char)*p);
                if (d < 0 || d >= 10) return SN_ERR_FORMAT;
                if (exp_val > 100000000) return SN_ERR_RANGE;
                exp_val = exp_val * 10 + d;
            }
            break;
        }
        if (base == 10 && *p == '.' && !saw_dot && !saw_exp) {
            saw_dot = 1;
            continue;
        }
        d = digval((unsigned char)*p);
        if (d < 0 || d >= base) return SN_ERR_FORMAT;
        st = mul_add_u32(ctx, mag, (uint32_t)base, (uint32_t)d);
        if (st != SN_OK) return st;
        if (saw_dot) frac_digits++;
        any = 1;
    }
    if (!any) return SN_ERR_FORMAT;

    /* Apply decimal exponent: value = mag * 10^(exp - frac_digits) */
    if (base == 10 && (saw_dot || saw_exp)) {
        int e = exp_neg ? -exp_val : exp_val;
        e -= frac_digits;
        if (e < 0) {
            /* require exact integer: magnitude must be divisible by 10^{-e} */
            int k;
            for (k = 0; k < -e; k++) {
                sn_value q, r, ten;
                sn_value_init(&q); sn_value_init(&r); sn_value_init(&ten);
                ten.kind = SN_KIND_BIGINT;
                st = sn_value_reserve(ctx, &ten, 1);
                if (st != SN_OK) { sn_value_clear(ctx,&ten); return st; }
                SN_LIMBS(&ten)[0] = 10; ten.nlimbs = 1;
                st = sn_limb_divmod(ctx, &q, &r, SN_CLIMBS(mag), mag->nlimbs, SN_CLIMBS(&ten), 1);
                if (st != SN_OK) {
                    sn_value_clear(ctx,&q); sn_value_clear(ctx,&r); sn_value_clear(ctx,&ten);
                    return st;
                }
                if (!(r.nlimbs == 1 && SN_CLIMBS(&r)[0] == 0)) {
                    sn_value_clear(ctx,&q); sn_value_clear(ctx,&r); sn_value_clear(ctx,&ten);
                    return SN_ERR_FORMAT; /* non-integer scientific for integer parse */
                }
                sn_value_clear(ctx, mag);
                sn_value_move(mag, &q);
                sn_value_clear(ctx, &r);
                sn_value_clear(ctx, &ten);
            }
        } else if (e > 0) {
            int k;
            for (k = 0; k < e; k++) {
                st = mul_add_u32(ctx, mag, 10u, 0);
                if (st != SN_OK) return st;
            }
        }
    }

    sn_bigint_normalize(mag);
    *neg_out = neg;
    return SN_OK;
}

sn_status sn_from_str_bigint(sn_ctx *ctx, sn_value *out, const char *s, int base)
{
    sn_value mag;
    int neg = 0;
    sn_status st;
    sn_value_init(&mag);
    st = parse_mag(ctx, &mag, s, base, &neg);
    if (st != SN_OK) {
        sn_value_clear(ctx, &mag);
        return st;
    }
    sn_value_clear(ctx, out);
    sn_value_move(out, &mag);
    out->kind = SN_KIND_BIGINT;
    out->negative = neg;
    sn_bigint_normalize(out);
    return SN_OK;
}

sn_status sn_from_str(sn_ctx *ctx, sn_value *out, const char *s, int base, int width, int is_signed)
{
    sn_value mag;
    int neg = 0;
    sn_status st;
    sn_limb *ml, *dl;
    int n, i;

    if (width < 1) return SN_ERR_ARG;
    sn_value_init(&mag);
    st = parse_mag(ctx, &mag, s, base, &neg);
    if (st != SN_OK) {
        sn_value_clear(ctx, &mag);
        return st;
    }

    /* Prefer exact i64 path when fits */
    {
        sn_value bi;
        int64_t v64;
        sn_value_init(&bi);
        sn_value_move(&bi, &mag);
        bi.kind = SN_KIND_BIGINT;
        bi.negative = neg;
        sn_bigint_normalize(&bi);
        if (sn_to_i64(ctx, &bi, &v64) == SN_OK) {
            st = sn_int_set_i64(ctx, out, v64, width, is_signed);
            sn_value_clear(ctx, &bi);
            return st;
        }

        st = sn_int_set_zero(ctx, out, width, is_signed);
        if (st != SN_OK) {
            sn_value_clear(ctx, &bi);
            return st;
        }
        n = sn_limbs_for_bits(width);
        if (neg) {
            /* two's complement of magnitude into out */
            uint64_t carry = 1;
            st = sn_value_reserve(ctx, &bi, n + 1);
            if (st != SN_OK) {
                sn_value_clear(ctx, &bi);
                return st;
            }
            ml = SN_LIMBS(&bi);
            for (i = bi.nlimbs; i < n; i++) ml[i] = 0;
            if (bi.nlimbs < n) bi.nlimbs = n;
            for (i = 0; i < bi.nlimbs; i++) {
                uint64_t x = ((uint64_t)(~ml[i]) & SN_LIMB_MASK) + carry;
                ml[i] = (sn_limb)(x & SN_LIMB_MASK);
                carry = x >> SN_LIMB_BITS;
            }
        }
        ml = SN_LIMBS(&bi);
        dl = SN_LIMBS(out);
        for (i = 0; i < n; i++)
            dl[i] = (i < bi.nlimbs) ? ml[i] : 0;
        out->nlimbs = n;
        sn_int_mask(out);
        sn_value_clear(ctx, &bi);
        return SN_OK;
    }
}

static char digchar(int d)
{
    return (char)(d < 10 ? '0' + d : 'a' + (d - 10));
}

/* Forward: multiprec decimal (defined in sn_float.c). */
sn_status sn_float_to_str_base(sn_ctx *ctx, char **out, const sn_value *v, int base);

sn_status sn_to_str(sn_ctx *ctx, char **out, const sn_value *v, int base)
{
    if (v && v->kind == SN_KIND_FLOAT) {
        return sn_float_to_str_base(ctx, out, v, base);
    }
    sn_value mag, tmp, q, r, bv;
    sn_status st;
    char *buf;
    int cap = 64, len = 0;
    int neg = 0;

    if (!out || !sn_value_is_num(v)) return SN_ERR_ARG;
    if (base < 2 || base > 16) return SN_ERR_ARG;
    *out = NULL;

    sn_value_init(&mag);
    sn_value_init(&tmp);
    sn_value_init(&q);
    sn_value_init(&r);
    sn_value_init(&bv);

    if (v->kind == SN_KIND_BIGINT) {
        neg = v->negative && !(v->nlimbs == 1 && SN_CLIMBS(v)[0] == 0);
        st = sn_value_copy(ctx, &mag, v);
        if (st != SN_OK) goto done;
        mag.negative = 0;
    } else {
        sn_value absv;
        int64_t sx;
        sn_value_init(&absv);
        if (sn_to_i64(ctx, v, &sx) == SN_OK && sx < 0)
            neg = 1;
        st = sn_abs(ctx, &absv, v, NULL);
        if (st != SN_OK) {
            sn_value_clear(ctx, &absv);
            goto done;
        }
        absv.kind = SN_KIND_BIGINT;
        absv.negative = 0;
        sn_value_move(&mag, &absv);
    }
    sn_bigint_normalize(&mag);

    {
        size_t *hdr = (size_t *)sn_malloc(ctx, sizeof(size_t) + (size_t)cap);
        if (!hdr) { st = SN_ERR_NOMEM; goto done; }
        *hdr = sizeof(size_t) + (size_t)cap;
        buf = (char *)(void *)(hdr + 1);
    }

    if (mag.nlimbs == 1 && SN_LIMBS(&mag)[0] == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        *out = buf;
        st = SN_OK;
        goto done;
    }

    bv.kind = SN_KIND_BIGINT;
    st = sn_value_reserve(ctx, &bv, 1);
    if (st != SN_OK) {
        if (buf) {
            size_t *oh = (size_t *)(void *)(buf - sizeof(size_t));
            sn_free(ctx, oh, *oh);
            buf = NULL;
        }
        goto done;
    }
    SN_LIMBS(&bv)[0] = (sn_limb)base;
    bv.nlimbs = 1;

    sn_value_move(&tmp, &mag);
    sn_value_init(&mag);

    while (!(tmp.nlimbs == 1 && SN_LIMBS(&tmp)[0] == 0)) {
        st = sn_limb_divmod(ctx, &q, &r, SN_LIMBS(&tmp), tmp.nlimbs, SN_LIMBS(&bv), 1);
        if (st != SN_OK) {
            if (buf) {
                size_t *oh = (size_t *)(void *)(buf - sizeof(size_t));
                sn_free(ctx, oh, *oh);
                buf = NULL;
            }
            goto done;
        }
        {
            int d = (int)SN_LIMBS(&r)[0];
            if (len + 2 >= cap) {
                size_t oldsz = sizeof(size_t) + (size_t)cap;
                size_t newcap = (size_t)cap * 2;
                size_t *oh = (size_t *)(void *)(buf - sizeof(size_t));
                size_t *nh = (size_t *)sn_realloc(ctx, oh, oldsz, sizeof(size_t) + newcap);
                if (!nh) {
                    sn_free(ctx, oh, oldsz);
                    buf = NULL;
                    st = SN_ERR_NOMEM;
                    goto done;
                }
                *nh = sizeof(size_t) + newcap;
                buf = (char *)(void *)(nh + 1);
                cap = (int)newcap;
            }
            buf[len++] = digchar(d);
        }
        sn_value_clear(ctx, &tmp);
        sn_value_move(&tmp, &q);
        sn_value_init(&q);
        sn_value_clear(ctx, &r);
    }

    if (neg) {
        if (len + 2 >= cap) {
            size_t oldsz = sizeof(size_t) + (size_t)cap;
            size_t newcap = (size_t)cap * 2;
            size_t *oh = (size_t *)(void *)(buf - sizeof(size_t));
            size_t *nh = (size_t *)sn_realloc(ctx, oh, oldsz, sizeof(size_t) + newcap);
            if (!nh) {
                sn_free(ctx, oh, oldsz);
                buf = NULL;
                st = SN_ERR_NOMEM;
                goto done;
            }
            *nh = sizeof(size_t) + newcap;
            buf = (char *)(void *)(nh + 1);
            cap = (int)newcap;
        }
        buf[len++] = '-';
    }
    {
        int i, j;
        for (i = 0, j = len - 1; i < j; i++, j--) {
            char c = buf[i];
            buf[i] = buf[j];
            buf[j] = c;
        }
        buf[len] = '\0';
    }
    *out = buf;
    st = SN_OK;

done:
    sn_value_clear(ctx, &mag);
    sn_value_clear(ctx, &tmp);
    sn_value_clear(ctx, &q);
    sn_value_clear(ctx, &r);
    sn_value_clear(ctx, &bv);
    return st;
}

void sn_str_free(sn_ctx *ctx, char *s)
{
    size_t *hdr;
    if (!s) return;
    hdr = (size_t *)(void *)(s - sizeof(size_t));
    sn_free(ctx, hdr, *hdr);
}

