#include "internal/sn_impl.h"
#include <math.h>
#include <float.h>
#include <errno.h>
#include <limits.h>

/* Bessel of first/second kind: not always exposed under strict C99 feature macros.
 * Declare for host path; MinGW/glibc provide them in libm. */
double j0(double);
double j1(double);
double jn(int, double);
double y0(double);
double y1(double);
double yn(int, double);

/* Phase 4: C math.h-style library on SN_KIND_FLOAT.
 * Strategy: host double as working precision (formats total bits <= 64),
 * re-encode with sn_float_from_double / existing float pack.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int same_float_fmt(const sn_value *a, const sn_value *b)
{
    return a && b && a->kind == SN_KIND_FLOAT && b->kind == SN_KIND_FLOAT &&
           a->e_bits == b->e_bits && a->m_bits == b->m_bits &&
           a->nan_enabled == b->nan_enabled;
}

static sn_status require_float(const sn_value *a)
{
    if (!a || a->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    return SN_OK;
}

static void clear_math_errno(void)
{
    errno = 0;
}

/* Map host result: raise flags from domain/pole; pack to target format. */
static sn_status pack_result(sn_ctx *ctx, sn_value *out, const sn_value *fmt,
                             double r, const sn_op_opt *opt, int domain_like)
{
    if (domain_like) sn_raise(ctx, SN_FLAG_INVALID);
    if (errno == EDOM) sn_raise(ctx, SN_FLAG_INVALID);
    if (errno == ERANGE) {
        if (r == 0.0 || r == -0.0) sn_raise(ctx, SN_FLAG_UNDERFLOW);
        else sn_raise(ctx, SN_FLAG_OVERFLOW);
    }
    if (r != r) { /* NaN */
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!fmt->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, fmt->e_bits, fmt->m_bits, fmt->nan_enabled);
    }
    return sn_float_from_double(ctx, out, r, fmt->e_bits, fmt->m_bits, fmt->nan_enabled, opt);
}

/* Unary: double (*fn)(double) */
static sn_status unary_host(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt,
                            double (*fn)(double))
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    st = sn_to_double(ctx, a, &da);
    if (st != SN_OK) return st;
    clear_math_errno();
    dr = fn(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

/* Binary: double (*fn)(double,double) same format */
static sn_status binary_host(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                             const sn_op_opt *opt, double (*fn)(double, double))
{
    sn_status st;
    double da, db, dr;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, b, &db); if (st != SN_OK) return st;
    clear_math_errno();
    dr = fn(da, db);
    return pack_result(ctx, out, a, dr, opt, 0);
}

/* ---------- classification ---------- */

int sn_isfinite(const sn_value *v)
{
    sn_fpclass c = sn_fp_classify(v);
    return c == SN_FP_ZERO || c == SN_FP_SUBNORMAL || c == SN_FP_NORMAL;
}

int sn_isinf(const sn_value *v)
{
    return sn_fp_classify(v) == SN_FP_INFINITE;
}

int sn_isnan(const sn_value *v)
{
    return sn_fp_classify(v) == SN_FP_NAN;
}

int sn_isnormal(const sn_value *v)
{
    return sn_fp_classify(v) == SN_FP_NORMAL;
}

/* ---------- abs / sign / minmax ---------- */

sn_status sn_fabs(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_float_abs(ctx, out, a, opt);
}

sn_status sn_copysign(sn_ctx *ctx, sn_value *out, const sn_value *mag, const sn_value *sgn,
                      const sn_op_opt *opt)
{
    sn_status st;
    int tb, sbit;
    (void)opt;
    if (!same_float_fmt(mag, sgn)) return SN_ERR_TYPE;
    st = sn_value_copy(ctx, out, mag);
    if (st != SN_OK) return st;
    tb = 1 + mag->e_bits + mag->m_bits;
    sbit = sn_fp_signbit(sgn) ? 1 : 0;
    {
        sn_limb *L = SN_LIMBS(out);
        int li = (tb - 1) / SN_LIMB_BITS;
        int off = (tb - 1) % SN_LIMB_BITS;
        if (sbit) L[li] |= ((sn_limb)1u << off);
        else L[li] &= ~((sn_limb)1u << off);
    }
    return SN_OK;
}

sn_status sn_fmin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    (void)opt;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (sn_isnan(a)) return sn_value_copy(ctx, out, b);
    if (sn_isnan(b)) return sn_value_copy(ctx, out, a);
    {
        int rel;
        sn_status st = sn_float_cmp(ctx, &rel, a, b);
        if (st != SN_OK) return st;
        return sn_value_copy(ctx, out, rel <= 0 ? a : b);
    }
}

sn_status sn_fmax(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    (void)opt;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (sn_isnan(a)) return sn_value_copy(ctx, out, b);
    if (sn_isnan(b)) return sn_value_copy(ctx, out, a);
    {
        int rel;
        sn_status st = sn_float_cmp(ctx, &rel, a, b);
        if (st != SN_OK) return st;
        return sn_value_copy(ctx, out, rel >= 0 ? a : b);
    }
}

sn_status sn_fdim(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a) && sn_math_need_soft(b))
        return sn_float_mp_fdim(ctx, out, a, b, opt);
    return binary_host(ctx, out, a, b, opt, fdim);
}

/* ---------- exp / log ---------- */

sn_status sn_exp(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_exp(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, exp);
}

sn_status sn_exp2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_exp2(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, exp2);
}

sn_status sn_expm1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_expm1(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, expm1);
}

sn_status sn_log(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_log(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    clear_math_errno();
    if (da < 0.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (da == 0.0) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
    }
    dr = log(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_log2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_log2(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    clear_math_errno();
    if (da < 0.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (da == 0.0) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
    }
    dr = log2(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_log10(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_log10(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    clear_math_errno();
    if (da < 0.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (da == 0.0) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
    }
    dr = log10(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_log1p(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_log1p(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    clear_math_errno();
    if (da < -1.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (da == -1.0) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
    }
    dr = log1p(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

/* ---------- power / roots ---------- */

sn_status sn_pow(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a) && sn_math_need_soft(b))
        return sn_soft_pow(ctx, out, a, b, opt);
    return binary_host(ctx, out, a, b, opt, pow);
}

sn_status sn_cbrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_cbrt(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, cbrt);
}

sn_status sn_hypot(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a) && sn_math_need_soft(b))
        return sn_soft_hypot(ctx, out, a, b, opt);
    return binary_host(ctx, out, a, b, opt, hypot);
}

/* ---------- trig ---------- */

sn_status sn_sin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_sin(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, sin);
}

sn_status sn_cos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_cos(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, cos);
}

sn_status sn_tan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_tan(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, tan);
}

sn_status sn_asin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_asin(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    if (da < -1.0 || da > 1.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    clear_math_errno();
    dr = asin(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_acos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_acos(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    if (da < -1.0 || da > 1.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    clear_math_errno();
    dr = acos(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_atan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_atan(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, atan);
}

sn_status sn_atan2(sn_ctx *ctx, sn_value *out, const sn_value *y, const sn_value *x, const sn_op_opt *opt)
{
    if (sn_math_need_soft(y) && sn_math_need_soft(x))
        return sn_soft_atan2(ctx, out, y, x, opt);
    return binary_host(ctx, out, y, x, opt, atan2);
}

/* ---------- hyperbolic ---------- */

sn_status sn_sinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_sinh(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, sinh);
}

sn_status sn_cosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_cosh(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, cosh);
}

sn_status sn_tanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_tanh(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, tanh);
}

sn_status sn_asinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_asinh(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, asinh);
}

sn_status sn_acosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_acosh(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    if (da < 1.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    clear_math_errno();
    dr = acosh(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_atanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_soft_atanh(ctx, out, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    if (da < -1.0 || da > 1.0) {
        sn_raise(ctx, SN_FLAG_INVALID);
        if (!a->nan_enabled)
            return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
        return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
    }
    if (da == 1.0 || da == -1.0) {
        sn_raise(ctx, SN_FLAG_DIVZERO);
        return sn_float_set_inf(ctx, out, da < 0 ? 1 : 0, a->e_bits, a->m_bits, a->nan_enabled);
    }
    clear_math_errno();
    dr = atanh(da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

/* ---------- rounding ---------- */

sn_status sn_ceil(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_float_mp_ceil(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, ceil);
}

sn_status sn_floor(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_float_mp_floor(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, floor);
}

sn_status sn_trunc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_float_mp_trunc(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, trunc);
}

sn_status sn_fround(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_float_mp_round(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, round);
}

sn_status sn_nearbyint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_float_mp_nearbyint(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, nearbyint);
}

sn_status sn_rint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_float_mp_rint(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, rint);
}

/* ---------- decompose / scale ---------- */

sn_status sn_modf(sn_ctx *ctx, sn_value *ipart, sn_value *fpart, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, di, df;
    if ((st = require_float(a)) != SN_OK) return st;
    if (!ipart || !fpart) return SN_ERR_ARG;
    if (sn_math_need_soft(a)) return sn_float_mp_modf(ctx, ipart, fpart, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    df = modf(da, &di);
    st = sn_float_from_double(ctx, ipart, di, a->e_bits, a->m_bits, a->nan_enabled, opt);
    if (st != SN_OK) return st;
    return sn_float_from_double(ctx, fpart, df, a->e_bits, a->m_bits, a->nan_enabled, opt);
}

sn_status sn_frexp(sn_ctx *ctx, sn_value *out, int *exp, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    int e;
    if ((st = require_float(a)) != SN_OK) return st;
    if (!exp) return SN_ERR_ARG;
    if (sn_math_need_soft(a)) return sn_float_mp_frexp(ctx, out, exp, a, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    dr = frexp(da, &e);
    *exp = e;
    return sn_float_from_double(ctx, out, dr, a->e_bits, a->m_bits, a->nan_enabled, opt);
}

sn_status sn_ldexp(sn_ctx *ctx, sn_value *out, const sn_value *a, int exp, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_float_mp_ldexp(ctx, out, a, exp, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    clear_math_errno();
    dr = ldexp(da, exp);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_scalbn(sn_ctx *ctx, sn_value *out, const sn_value *a, int n, const sn_op_opt *opt)
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) return sn_float_mp_ldexp(ctx, out, a, n, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    clear_math_errno();
    dr = scalbn(da, n);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_ilogb(sn_ctx *ctx, const sn_value *a, int *exp)
{
    sn_status st;
    double da;
    int e;
    if ((st = require_float(a)) != SN_OK) return st;
    if (!exp) return SN_ERR_ARG;
    if (sn_math_need_soft(a)) return sn_float_mp_ilogb(ctx, a, exp);
    if (sn_isnan(a) || sn_isinf(a) || sn_fp_classify(a) == SN_FP_ZERO) {
        sn_raise(ctx, SN_FLAG_INVALID);
        *exp = FP_ILOGB0;
        return SN_ERR_DOMAIN;
    }
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    e = ilogb(da);
    *exp = e;
    return SN_OK;
}

sn_status sn_logb(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    sn_status st;
    int e;
    if ((st = require_float(a)) != SN_OK) return st;
    if (sn_math_need_soft(a)) {
        st = sn_float_mp_ilogb(ctx, a, &e);
        if (st != SN_OK) {
            /* domain: map to nan/inf like host logb */
            if (sn_fp_classify(a) == SN_FP_ZERO) {
                sn_raise(ctx, SN_FLAG_DIVZERO);
                return sn_float_set_inf(ctx, out, 1, a->e_bits, a->m_bits, a->nan_enabled);
            }
            if (sn_isinf(a))
                return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
            sn_raise(ctx, SN_FLAG_INVALID);
            if (!a->nan_enabled)
                return sn_float_set_inf(ctx, out, 0, a->e_bits, a->m_bits, a->nan_enabled);
            return sn_float_set_nan(ctx, out, a->e_bits, a->m_bits);
        }
        return sn_float_from_i64(ctx, out, (int64_t)e, a->e_bits, a->m_bits, a->nan_enabled, opt);
    }
    return unary_host(ctx, out, a, opt, logb);
}

/* ---------- remainder ---------- */

sn_status sn_fmod(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a) && sn_math_need_soft(b))
        return sn_float_mp_fmod(ctx, out, a, b, opt);
    return binary_host(ctx, out, a, b, opt, fmod);
}

sn_status sn_remquo(sn_ctx *ctx, sn_value *out, int *quo, const sn_value *a, const sn_value *b,
                    const sn_op_opt *opt)
{
    sn_status st;
    double da, db, dr;
    int q;
    if (!same_float_fmt(a, b)) return SN_ERR_TYPE;
    if (!quo) return SN_ERR_ARG;
    if (sn_math_need_soft(a) && sn_math_need_soft(b))
        return sn_float_mp_remquo(ctx, out, quo, a, b, opt);
    st = sn_to_double(ctx, a, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, b, &db); if (st != SN_OK) return st;
    clear_math_errno();
    dr = remquo(da, db, &q);
    *quo = q;
    return pack_result(ctx, out, a, dr, opt, 0);
}

/* ---------- special ---------- */

sn_status sn_erf(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_erf(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, erf);
}

sn_status sn_erfc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_erfc(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, erfc);
}

sn_status sn_tgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_tgamma(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, tgamma);
}

sn_status sn_lgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_lgamma(ctx, out, a, opt);
    return unary_host(ctx, out, a, opt, lgamma);
}

sn_status sn_nextafter(sn_ctx *ctx, sn_value *out, const sn_value *from, const sn_value *to,
                       const sn_op_opt *opt)
{
    sn_status st;
    double da, db, dr;
    if (!same_float_fmt(from, to)) return SN_ERR_TYPE;
    if (sn_math_need_soft(from))
        return sn_float_mp_nextafter(ctx, out, from, to, opt);
    st = sn_to_double(ctx, from, &da); if (st != SN_OK) return st;
    st = sn_to_double(ctx, to, &db); if (st != SN_OK) return st;
    /* Use nextafter for binary64; for narrower formats re-encode may collapse.
     * Prefer float nextafter when format is binary32-like (e=8,m=23). */
    if (from->e_bits == 8 && from->m_bits == 23) {
        float fa = (float)da, fb = (float)db, fr;
        fr = nextafterf(fa, fb);
        dr = (double)fr;
    } else {
        dr = nextafter(da, db);
    }
    return sn_float_from_double(ctx, out, dr, from->e_bits, from->m_bits, from->nan_enabled, opt);
}


/* ---------- Bessel (host for m<=52; soft multiprec otherwise) ---------- */

static sn_status bessel_host_unary(sn_ctx *ctx, sn_value *out, const sn_value *a,
                                   const sn_op_opt *opt, double (*fn)(double))
{
    return unary_host(ctx, out, a, opt, fn);
}

static sn_status bessel_host_n(sn_ctx *ctx, sn_value *out, int n, const sn_value *a,
                               const sn_op_opt *opt, double (*fn)(int, double))
{
    sn_status st;
    double da, dr;
    if ((st = require_float(a)) != SN_OK) return st;
    st = sn_to_double(ctx, a, &da);
    if (st != SN_OK) return st;
    clear_math_errno();
    dr = fn(n, da);
    return pack_result(ctx, out, a, dr, opt, 0);
}

sn_status sn_j0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_j0(ctx, out, a, opt);
    return bessel_host_unary(ctx, out, a, opt, j0);
}

sn_status sn_j1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_j1(ctx, out, a, opt);
    return bessel_host_unary(ctx, out, a, opt, j1);
}

sn_status sn_jn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_jn(ctx, out, n, a, opt);
    return bessel_host_n(ctx, out, n, a, opt, jn);
}

sn_status sn_y0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_y0(ctx, out, a, opt);
    return bessel_host_unary(ctx, out, a, opt, y0);
}

sn_status sn_y1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_y1(ctx, out, a, opt);
    return bessel_host_unary(ctx, out, a, opt, y1);
}

sn_status sn_yn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    if (sn_math_need_soft(a)) return sn_soft_yn(ctx, out, n, a, opt);
    return bessel_host_n(ctx, out, n, a, opt, yn);
}

/* Modified Bessel I/K: always portable soft (no host i0/k0 on many libm). */
sn_status sn_i0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_i0(ctx, out, a, opt);
}

sn_status sn_i1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_i1(ctx, out, a, opt);
}

sn_status sn_in(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_in(ctx, out, n, a, opt);
}

sn_status sn_k0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_k0(ctx, out, a, opt);
}

sn_status sn_k1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_k1(ctx, out, a, opt);
}

sn_status sn_kn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_kn(ctx, out, n, a, opt);
}

/* Complete elliptic integrals: always portable soft AGM. */
sn_status sn_ellipk(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_ellipk(ctx, out, m, opt);
}

sn_status sn_ellipe(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_ellipe(ctx, out, m, opt);
}

/* Regularized incomplete gamma: always portable soft. */
sn_status sn_igamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt)
{
    return sn_soft_igamma(ctx, out, a, x, opt);
}

sn_status sn_igammac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt)
{
    return sn_soft_igammac(ctx, out, a, x, opt);
}

/* Regularized incomplete beta: always portable soft. */
sn_status sn_ibeta(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt)
{
    return sn_soft_ibeta(ctx, out, a, b, x, opt);
}

sn_status sn_ibetac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt)
{
    return sn_soft_ibetac(ctx, out, a, b, x, opt);
}

/* Jacobi elliptic: always portable soft. */
sn_status sn_jacobi_sn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_jacobi_sn(ctx, out, u, m, opt);
}

sn_status sn_jacobi_cn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_jacobi_cn(ctx, out, u, m, opt);
}

sn_status sn_jacobi_dn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_jacobi_dn(ctx, out, u, m, opt);
}

/* digamma ?: always portable soft. */
sn_status sn_digamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_digamma(ctx, out, a, opt);
}

/* trigamma psi_1: always portable soft. */
sn_status sn_trigamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_trigamma(ctx, out, a, opt);
}

/* polygamma psi^{(n)}; n=0 is digamma. Always portable soft. */
sn_status sn_polygamma(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt)
{
    return sn_soft_polygamma(ctx, out, n, a, opt);
}


/* Incomplete elliptic F(?|m): always portable soft. */
sn_status sn_ellipf(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_ellipf(ctx, out, phi, m, opt);
}

/* Incomplete elliptic E(phi|m): always portable soft. */
sn_status sn_ellipeinc(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_ellipeinc(ctx, out, phi, m, opt);
}

/* Incomplete elliptic Pi(n;phi|m): always portable soft. */
sn_status sn_ellipiinc(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *n, const sn_value *m, const sn_op_opt *opt)
{
    return sn_soft_ellipiinc(ctx, out, phi, n, m, opt);
}

