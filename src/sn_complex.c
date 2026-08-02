/*
 * Complex numbers: rectangular arithmetic on sn_value re/im pairs.
 * Uses existing float ops (works for narrow and multiprec formats).
 * Transcendentals compose sn_exp/log/sin/cos/... so multiprec soft paths
 * are picked automatically when m_bits>52.
 */
#include "internal/sn_impl.h"
#include <string.h>

void sn_cplx_init(sn_cplx *z)
{
    if (!z) return;
    sn_value_init(&z->re);
    sn_value_init(&z->im);
}

void sn_cplx_clear(sn_ctx *ctx, sn_cplx *z)
{
    if (!z) return;
    sn_value_clear(ctx, &z->re);
    sn_value_clear(ctx, &z->im);
    sn_value_init(&z->re);
    sn_value_init(&z->im);
}

static int same_fmt_vv(const sn_value *a, const sn_value *b)
{
    return a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT &&
           a->e_bits == b->e_bits && a->m_bits == b->m_bits &&
           a->nan_enabled == b->nan_enabled;
}

static int cplx_same_fmt(const sn_cplx *a, const sn_cplx *b)
{
    return a && b && same_fmt_vv(&a->re, &a->im) && same_fmt_vv(&b->re, &b->im) &&
           same_fmt_vv(&a->re, &b->re);
}

static int cplx_ok(const sn_cplx *z)
{
    return z && same_fmt_vv(&z->re, &z->im);
}

sn_status sn_cplx_set(sn_ctx *ctx, sn_cplx *z, const sn_value *re, const sn_value *im)
{
    sn_status st;
    if (!z || !re || !im) return SN_ERR_ARG;
    if (!same_fmt_vv(re, im)) return SN_ERR_TYPE;
    st = sn_value_copy(ctx, &z->re, re);
    if (st != SN_OK) return st;
    return sn_value_copy(ctx, &z->im, im);
}

sn_status sn_cplx_set_d(sn_ctx *ctx, sn_cplx *z, double re, double im,
                        int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt)
{
    sn_status st;
    sn_value r, i;
    if (!z) return SN_ERR_ARG;
    sn_value_init(&r);
    sn_value_init(&i);
    st = sn_float_from_double(ctx, &r, re, e_bits, m_bits, nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &i, im, e_bits, m_bits, nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, z, &r, &i);
done:
    sn_value_clear(ctx, &r);
    sn_value_clear(ctx, &i);
    return st;
}

sn_status sn_cplx_copy(sn_ctx *ctx, sn_cplx *out, const sn_cplx *src)
{
    sn_status st;
    if (!out || !src) return SN_ERR_ARG;
    st = sn_value_copy(ctx, &out->re, &src->re);
    if (st != SN_OK) return st;
    return sn_value_copy(ctx, &out->im, &src->im);
}

sn_status sn_cplx_add(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt)
{
    sn_status st;
    sn_value re, im;
    if (!cplx_same_fmt(a, b)) return SN_ERR_TYPE;
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_add(ctx, &re, &a->re, &b->re, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &im, &a->im, &b->im, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

sn_status sn_cplx_sub(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt)
{
    sn_status st;
    sn_value re, im;
    if (!cplx_same_fmt(a, b)) return SN_ERR_TYPE;
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_sub(ctx, &re, &a->re, &b->re, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &im, &a->im, &b->im, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

/* (ar+ai i)(br+bi i) = (ar*br - ai*bi) + (ar*bi + ai*br)i */
sn_status sn_cplx_mul(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt)
{
    sn_status st;
    sn_value t1, t2, re, im;
    if (!cplx_same_fmt(a, b)) return SN_ERR_TYPE;
    sn_value_init(&t1);
    sn_value_init(&t2);
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_mul(ctx, &t1, &a->re, &b->re, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t2, &a->im, &b->im, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &re, &t1, &t2, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t1, &a->re, &b->im, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t2, &a->im, &b->re, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &im, &t1, &t2, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &t1);
    sn_value_clear(ctx, &t2);
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

/* a/b = a * conj(b) / |b|^2 */
sn_status sn_cplx_div(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt)
{
    sn_status st;
    sn_value t1, t2, den, re, im;
    if (!cplx_same_fmt(a, b)) return SN_ERR_TYPE;
    sn_value_init(&t1);
    sn_value_init(&t2);
    sn_value_init(&den);
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_mul(ctx, &t1, &b->re, &b->re, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t2, &b->im, &b->im, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &den, &t1, &t2, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t1, &a->re, &b->re, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t2, &a->im, &b->im, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &re, &t1, &t2, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &re, &re, &den, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t1, &a->im, &b->re, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &t2, &a->re, &b->im, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &im, &t1, &t2, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &im, &im, &den, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &t1);
    sn_value_clear(ctx, &t2);
    sn_value_clear(ctx, &den);
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

sn_status sn_cplx_neg(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value re, im;
    if (!cplx_ok(a)) return SN_ERR_TYPE;
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_neg(ctx, &re, &a->re, opt); if (st != SN_OK) goto done;
    st = sn_neg(ctx, &im, &a->im, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

sn_status sn_cplx_conj(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_op_opt *opt)
{
    sn_status st;
    sn_value im;
    if (!cplx_ok(a)) return SN_ERR_TYPE;
    sn_value_init(&im);
    st = sn_neg(ctx, &im, &a->im, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &a->re, &im);
done:
    sn_value_clear(ctx, &im);
    return st;
}

sn_status sn_cplx_abs(sn_ctx *ctx, sn_value *out, const sn_cplx *z, const sn_op_opt *opt)
{
    if (!z || !same_fmt_vv(&z->re, &z->im)) return SN_ERR_TYPE;
    return sn_hypot(ctx, out, &z->re, &z->im, opt);
}

sn_status sn_cplx_arg(sn_ctx *ctx, sn_value *out, const sn_cplx *z, const sn_op_opt *opt)
{
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    return sn_atan2(ctx, out, &z->im, &z->re, opt);
}

/* cproj(z): if any part infinite, return inf + i*copysign(0, im); else z */
sn_status sn_cplx_proj(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value re, im, zero;
    int s;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    if (!sn_isinf(&z->re) && !sn_isinf(&z->im))
        return sn_cplx_copy(ctx, out, z);
    sn_value_init(&re);
    sn_value_init(&im);
    sn_value_init(&zero);
    st = sn_float_set_inf(ctx, &re, 0, z->re.e_bits, z->re.m_bits, z->re.nan_enabled);
    if (st != SN_OK) goto done;
    s = sn_fp_signbit(&z->im) ? 1 : 0;
    st = sn_float_set_zero(ctx, &zero, s, z->re.e_bits, z->re.m_bits, z->re.nan_enabled);
    if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &zero);
    (void)opt;
done:
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    sn_value_clear(ctx, &zero);
    return st;
}

sn_status sn_cplx_from_polar(sn_ctx *ctx, sn_cplx *out, const sn_value *rho, const sn_value *theta,
                             const sn_op_opt *opt)
{
    sn_status st;
    sn_value c, s, re, im;
    if (!same_fmt_vv(rho, theta)) return SN_ERR_TYPE;
    sn_value_init(&c);
    sn_value_init(&s);
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_cos(ctx, &c, theta, opt); if (st != SN_OK) goto done;
    st = sn_sin(ctx, &s, theta, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &re, rho, &c, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &im, rho, &s, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

/* ---------- transcendentals ---------- */

/* csqrt: principal branch. w = sqrt((|z|+re)/2) + i*sign(im)*sqrt((|z|-re)/2) */
sn_status sn_cplx_sqrt(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value rho, t, half, re, im, two;
    int im_neg;
    if (!cplx_ok(z)) return SN_ERR_TYPE;

    /* Special cases: pure real non-negative -> real sqrt; pure real negative -> pure imag */
    sn_value_init(&rho);
    sn_value_init(&t);
    sn_value_init(&half);
    sn_value_init(&re);
    sn_value_init(&im);
    sn_value_init(&two);

    st = sn_cplx_abs(ctx, &rho, z, opt); if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &two, 2.0, z->re.e_bits, z->re.m_bits, z->re.nan_enabled, opt);
    if (st != SN_OK) goto done;

    im_neg = sn_fp_signbit(&z->im);

    /* re_out = sqrt((rho + re)/2) */
    st = sn_add(ctx, &t, &rho, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &t, &two, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &re, &t, opt); if (st != SN_OK) goto done;

    /* im_out = sign(im) * sqrt((rho - re)/2) */
    st = sn_sub(ctx, &t, &rho, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_div(ctx, &t, &t, &two, opt); if (st != SN_OK) goto done;
    st = sn_sqrt(ctx, &im, &t, opt); if (st != SN_OK) goto done;
    if (im_neg) {
        st = sn_neg(ctx, &im, &im, opt); if (st != SN_OK) goto done;
    }

    /* If re was negative and im==0, classic formula can yield +0 imag; keep sign of im. */
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &rho);
    sn_value_clear(ctx, &t);
    sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    sn_value_clear(ctx, &two);
    return st;
}

/* cexp(z) = exp(re) * (cos(im) + i*sin(im)) */
sn_status sn_cplx_exp(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value er, c, s, re, im;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_value_init(&er);
    sn_value_init(&c);
    sn_value_init(&s);
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_exp(ctx, &er, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_cos(ctx, &c, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_sin(ctx, &s, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &re, &er, &c, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &im, &er, &s, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &er);
    sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &s);
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

/* clog(z) = log(|z|) + i*arg(z) */
sn_status sn_cplx_log(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value rho, re, im;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_value_init(&rho);
    sn_value_init(&re);
    sn_value_init(&im);
    st = sn_cplx_abs(ctx, &rho, z, opt); if (st != SN_OK) goto done;
    st = sn_log(ctx, &re, &rho, opt); if (st != SN_OK) goto done;
    st = sn_cplx_arg(ctx, &im, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &rho);
    sn_value_clear(ctx, &re);
    sn_value_clear(ctx, &im);
    return st;
}

/* cpow(a,b) = cexp(b * clog(a)) */
sn_status sn_cplx_pow(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx la, t;
    if (!cplx_same_fmt(a, b)) return SN_ERR_TYPE;
    sn_cplx_init(&la);
    sn_cplx_init(&t);
    st = sn_cplx_log(ctx, &la, a, opt); if (st != SN_OK) goto done;
    st = sn_cplx_mul(ctx, &t, b, &la, opt); if (st != SN_OK) goto done;
    st = sn_cplx_exp(ctx, out, &t, opt);
done:
    sn_cplx_clear(ctx, &la);
    sn_cplx_clear(ctx, &t);
    return st;
}

/* csin(z) = sin(re)cosh(im) + i cos(re)sinh(im) */
sn_status sn_cplx_sin(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value s, c, sh, ch, re, im;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_value_init(&s); sn_value_init(&c);
    sn_value_init(&sh); sn_value_init(&ch);
    sn_value_init(&re); sn_value_init(&im);
    st = sn_sin(ctx, &s, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_cos(ctx, &c, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_sinh(ctx, &sh, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_cosh(ctx, &ch, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &re, &s, &ch, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &im, &c, &sh, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &sh); sn_value_clear(ctx, &ch);
    sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    return st;
}

/* ccos(z) = cos(re)cosh(im) - i sin(re)sinh(im) */
sn_status sn_cplx_cos(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value s, c, sh, ch, re, im;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_value_init(&s); sn_value_init(&c);
    sn_value_init(&sh); sn_value_init(&ch);
    sn_value_init(&re); sn_value_init(&im);
    st = sn_sin(ctx, &s, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_cos(ctx, &c, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_sinh(ctx, &sh, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_cosh(ctx, &ch, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &re, &c, &ch, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &im, &s, &sh, opt); if (st != SN_OK) goto done;
    st = sn_neg(ctx, &im, &im, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &sh); sn_value_clear(ctx, &ch);
    sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    return st;
}

sn_status sn_cplx_tan(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx s, c;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&s);
    sn_cplx_init(&c);
    st = sn_cplx_sin(ctx, &s, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_cos(ctx, &c, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_div(ctx, out, &s, &c, opt);
done:
    sn_cplx_clear(ctx, &s);
    sn_cplx_clear(ctx, &c);
    return st;
}

/* csinh(z) = sinh(re)cos(im) + i cosh(re)sin(im) */
sn_status sn_cplx_sinh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value sh, ch, s, c, re, im;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_value_init(&sh); sn_value_init(&ch);
    sn_value_init(&s); sn_value_init(&c);
    sn_value_init(&re); sn_value_init(&im);
    st = sn_sinh(ctx, &sh, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_cosh(ctx, &ch, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_sin(ctx, &s, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_cos(ctx, &c, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &re, &sh, &c, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &im, &ch, &s, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &sh); sn_value_clear(ctx, &ch);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    return st;
}

/* ccosh(z) = cosh(re)cos(im) + i sinh(re)sin(im) */
sn_status sn_cplx_cosh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_value sh, ch, s, c, re, im;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_value_init(&sh); sn_value_init(&ch);
    sn_value_init(&s); sn_value_init(&c);
    sn_value_init(&re); sn_value_init(&im);
    st = sn_sinh(ctx, &sh, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_cosh(ctx, &ch, &z->re, opt); if (st != SN_OK) goto done;
    st = sn_sin(ctx, &s, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_cos(ctx, &c, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &re, &ch, &c, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &im, &sh, &s, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_value_clear(ctx, &sh); sn_value_clear(ctx, &ch);
    sn_value_clear(ctx, &s); sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    return st;
}

sn_status sn_cplx_tanh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx s, c;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&s);
    sn_cplx_init(&c);
    st = sn_cplx_sinh(ctx, &s, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_cosh(ctx, &c, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_div(ctx, out, &s, &c, opt);
done:
    sn_cplx_clear(ctx, &s);
    sn_cplx_clear(ctx, &c);
    return st;
}

/* casin(z) = -i * clog(i*z + csqrt(1 - z^2)) */
sn_status sn_cplx_asin(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx iz, z2, one, t, s, w, logw;
    sn_value one_v, zero_v;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&iz); sn_cplx_init(&z2); sn_cplx_init(&one);
    sn_cplx_init(&t); sn_cplx_init(&s); sn_cplx_init(&w); sn_cplx_init(&logw);
    sn_value_init(&one_v); sn_value_init(&zero_v);

    st = sn_float_from_double(ctx, &one_v, 1.0, z->re.e_bits, z->re.m_bits, z->re.nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_set_zero(ctx, &zero_v, 0, z->re.e_bits, z->re.m_bits, z->re.nan_enabled);
    if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, &one, &one_v, &zero_v); if (st != SN_OK) goto done;

    /* iz = i*z = -im + i*re */
    st = sn_neg(ctx, &iz.re, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &iz.im, &z->re); if (st != SN_OK) goto done;

    st = sn_cplx_mul(ctx, &z2, z, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_sub(ctx, &t, &one, &z2, opt); if (st != SN_OK) goto done;
    st = sn_cplx_sqrt(ctx, &s, &t, opt); if (st != SN_OK) goto done;
    st = sn_cplx_add(ctx, &w, &iz, &s, opt); if (st != SN_OK) goto done;
    st = sn_cplx_log(ctx, &logw, &w, opt); if (st != SN_OK) goto done;
    /* -i * logw = im + (-re)i  because -i*(x+iy)= y - i x */
    {
        sn_value re, im;
        sn_value_init(&re); sn_value_init(&im);
        st = sn_value_copy(ctx, &re, &logw.im);
        if (st == SN_OK) st = sn_neg(ctx, &im, &logw.re, opt);
        if (st == SN_OK) st = sn_cplx_set(ctx, out, &re, &im);
        sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    }
done:
    sn_cplx_clear(ctx, &iz); sn_cplx_clear(ctx, &z2); sn_cplx_clear(ctx, &one);
    sn_cplx_clear(ctx, &t); sn_cplx_clear(ctx, &s); sn_cplx_clear(ctx, &w); sn_cplx_clear(ctx, &logw);
    sn_value_clear(ctx, &one_v); sn_value_clear(ctx, &zero_v);
    return st;
}

/* cacos(z) = pi/2 - casin(z) */
sn_status sn_cplx_acos(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx as;
    sn_value half_pi, zero_v, re;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&as);
    sn_value_init(&half_pi); sn_value_init(&zero_v); sn_value_init(&re);
    st = sn_cplx_asin(ctx, &as, z, opt); if (st != SN_OK) goto done;
    /* pi/2 via atan(1)*2 */
    st = sn_float_from_double(ctx, &half_pi, 1.0, z->re.e_bits, z->re.m_bits, z->re.nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_atan(ctx, &half_pi, &half_pi, opt); if (st != SN_OK) goto done;
    st = sn_add(ctx, &half_pi, &half_pi, &half_pi, opt); if (st != SN_OK) goto done;
    st = sn_sub(ctx, &re, &half_pi, &as.re, opt); if (st != SN_OK) goto done;
    {
        sn_value imn;
        sn_value_init(&imn);
        st = sn_neg(ctx, &imn, &as.im, opt);
        if (st == SN_OK) st = sn_cplx_set(ctx, out, &re, &imn);
        sn_value_clear(ctx, &imn);
    }
done:
    sn_cplx_clear(ctx, &as);
    sn_value_clear(ctx, &half_pi); sn_value_clear(ctx, &zero_v); sn_value_clear(ctx, &re);
    return st;
}

/* catan(z) = (i/2) * clog((i+z)/(i-z)) */
sn_status sn_cplx_atan(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx iunit, num, den, q, lq;
    sn_value zero_v, one_v, half, re, im;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&iunit); sn_cplx_init(&num); sn_cplx_init(&den);
    sn_cplx_init(&q); sn_cplx_init(&lq);
    sn_value_init(&zero_v); sn_value_init(&one_v);
    sn_value_init(&half); sn_value_init(&re); sn_value_init(&im);

    st = sn_float_set_zero(ctx, &zero_v, 0, z->re.e_bits, z->re.m_bits, z->re.nan_enabled);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &one_v, 1.0, z->re.e_bits, z->re.m_bits, z->re.nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, &iunit, &zero_v, &one_v); if (st != SN_OK) goto done;
    st = sn_cplx_add(ctx, &num, &iunit, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_sub(ctx, &den, &iunit, z, opt); if (st != SN_OK) goto done;
    st = sn_cplx_div(ctx, &q, &num, &den, opt); if (st != SN_OK) goto done;
    st = sn_cplx_log(ctx, &lq, &q, opt); if (st != SN_OK) goto done;
    /* (i/2)*lq = i*(0.5*re + i*0.5*im) = -0.5*im + i*0.5*re */
    st = sn_float_from_double(ctx, &half, 0.5, z->re.e_bits, z->re.m_bits, z->re.nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_mul(ctx, &im, &half, &lq.re, opt); if (st != SN_OK) goto done;
    st = sn_mul(ctx, &re, &half, &lq.im, opt); if (st != SN_OK) goto done;
    st = sn_neg(ctx, &re, &re, opt); if (st != SN_OK) goto done;
    st = sn_cplx_set(ctx, out, &re, &im);
done:
    sn_cplx_clear(ctx, &iunit); sn_cplx_clear(ctx, &num); sn_cplx_clear(ctx, &den);
    sn_cplx_clear(ctx, &q); sn_cplx_clear(ctx, &lq);
    sn_value_clear(ctx, &zero_v); sn_value_clear(ctx, &one_v);
    sn_value_clear(ctx, &half); sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    return st;
}

/* casinh(z) = -i * casin(i*z) */
sn_status sn_cplx_asinh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx iz, a;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&iz); sn_cplx_init(&a);
    st = sn_neg(ctx, &iz.re, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &iz.im, &z->re); if (st != SN_OK) goto done;
    st = sn_cplx_asin(ctx, &a, &iz, opt); if (st != SN_OK) goto done;
    /* -i * a = im_a - i * re_a */
    {
        sn_value re, im;
        sn_value_init(&re); sn_value_init(&im);
        st = sn_value_copy(ctx, &re, &a.im);
        if (st == SN_OK) st = sn_neg(ctx, &im, &a.re, opt);
        if (st == SN_OK) st = sn_cplx_set(ctx, out, &re, &im);
        sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    }
done:
    sn_cplx_clear(ctx, &iz); sn_cplx_clear(ctx, &a);
    return st;
}

/* cacosh(z) = i * cacos(z)  (principal branch, re>=0) */
sn_status sn_cplx_acosh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx a;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&a);
    st = sn_cplx_acos(ctx, &a, z, opt); if (st != SN_OK) goto done;
    /* i * a = -im + i*re */
    {
        sn_value re, im;
        sn_value_init(&re); sn_value_init(&im);
        st = sn_neg(ctx, &re, &a.im, opt);
        if (st == SN_OK) st = sn_value_copy(ctx, &im, &a.re);
        if (st == SN_OK) st = sn_cplx_set(ctx, out, &re, &im);
        sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    }
done:
    sn_cplx_clear(ctx, &a);
    return st;
}

/* catanh(z) = -i * catan(i*z) */
sn_status sn_cplx_atanh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt)
{
    sn_status st;
    sn_cplx iz, a;
    if (!cplx_ok(z)) return SN_ERR_TYPE;
    sn_cplx_init(&iz); sn_cplx_init(&a);
    st = sn_neg(ctx, &iz.re, &z->im, opt); if (st != SN_OK) goto done;
    st = sn_value_copy(ctx, &iz.im, &z->re); if (st != SN_OK) goto done;
    st = sn_cplx_atan(ctx, &a, &iz, opt); if (st != SN_OK) goto done;
    {
        sn_value re, im;
        sn_value_init(&re); sn_value_init(&im);
        st = sn_value_copy(ctx, &re, &a.im);
        if (st == SN_OK) st = sn_neg(ctx, &im, &a.re, opt);
        if (st == SN_OK) st = sn_cplx_set(ctx, out, &re, &im);
        sn_value_clear(ctx, &re); sn_value_clear(ctx, &im);
    }
done:
    sn_cplx_clear(ctx, &iz); sn_cplx_clear(ctx, &a);
    return st;
}
