#ifndef SN_IMPL_H
#define SN_IMPL_H

#include "sn.h"
#include "sn_flat.h"
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#  define SN_UNUSED_FN __attribute__((unused))
#else
#  define SN_UNUSED_FN
#endif

SN_UNUSED_FN static sn_limb *sn_limbs_mut(sn_value *v)
{
    return (v->cap > 0 && v->heap != NULL) ? v->heap : v->inline_limbs;
}

SN_UNUSED_FN static const sn_limb *sn_limbs_const(const sn_value *v)
{
    return (v->cap > 0 && v->heap != NULL) ? v->heap : v->inline_limbs;
}

#define SN_LIMBS(v) (sn_limbs_mut(v))
#define SN_CLIMBS(v) (sn_limbs_const(v))

int  sn_limbs_for_bits(int bits);
sn_status sn_value_reserve(sn_ctx *ctx, sn_value *v, int nlimbs);
void sn_int_mask(sn_value *v);
void sn_bigint_normalize(sn_value *v);
sn_status sn_int_set_zero(sn_ctx *ctx, sn_value *out, int width, int is_signed);
int  sn_limb_cmp(const sn_limb *a, int na, const sn_limb *b, int nb);
sn_int_overflow sn_eff_iov(const sn_ctx *ctx, const sn_op_opt *opt);
void sn_raise(sn_ctx *ctx, unsigned flag);

sn_status sn_limb_add(sn_ctx *ctx, sn_value *r, const sn_limb *a, int na, const sn_limb *b, int nb);
sn_status sn_limb_sub(sn_ctx *ctx, sn_value *r, const sn_limb *a, int na, const sn_limb *b, int nb);
sn_status sn_limb_mul(sn_ctx *ctx, sn_value *r, const sn_limb *a, int na, const sn_limb *b, int nb);
sn_status sn_limb_divmod(sn_ctx *ctx, sn_value *q, sn_value *r,
                         const sn_limb *u, int nu, const sn_limb *v, int nv);

void sn_limbs_from_u64(sn_limb *d, int nd, uint64_t x);
uint64_t sn_limbs_to_u64(const sn_limb *s, int ns);

int sn_value_is_num(const sn_value *v);

/* float ops (sn_float.c) */
sn_status sn_float_add(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_sub(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mul(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_div(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_neg(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_abs(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_cmp(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b);
sn_status sn_float_fma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                       const sn_value *c, const sn_op_opt *opt);
sn_status sn_float_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_frem(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_to_str(sn_ctx *ctx, char **out, const sn_value *v);
sn_status sn_float_from_str(sn_ctx *ctx, sn_value *out, const char *s,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
sn_status sn_float_to_i64(sn_ctx *ctx, const sn_value *v, int64_t *out, const sn_op_opt *opt);
sn_status sn_float_from_num(sn_ctx *ctx, sn_value *out, const sn_value *src,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
sn_status sn_float_from_double(sn_ctx *ctx, sn_value *out, double x,
                               int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);


/* multiprec soft float (sn_float_mp.c) — m_bits > 52 */
int sn_float_mp_supported(int e_bits, int m_bits);
sn_status sn_float_mp_add(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mp_sub(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mp_mul(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mp_div(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mp_cmp(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b);
sn_status sn_float_mp_from_i64(sn_ctx *ctx, sn_value *out, int64_t x,
                               int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
sn_status sn_float_mp_from_double(sn_ctx *ctx, sn_value *out, double x,
                                  int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
/* Integer/BIGINT magnitude -> multiprec float without double truncation. */
sn_status sn_float_mp_from_bigint(sn_ctx *ctx, sn_value *out, const sn_value *src,
                                  int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
sn_fpclass sn_float_mp_classify(const sn_value *v);
int sn_float_mp_signbit(const sn_value *v);
sn_status sn_float_mp_to_double(sn_ctx *ctx, const sn_value *v, double *out);
sn_status sn_float_mp_to_i64(sn_ctx *ctx, const sn_value *v, int64_t *out);
sn_status sn_float_mp_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_fma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                          const sn_value *c, const sn_op_opt *opt);
sn_status sn_float_mp_frem(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mp_ceil(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_floor(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_trunc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_round(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_nearbyint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_rint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_nextafter(sn_ctx *ctx, sn_value *out, const sn_value *from, const sn_value *to, const sn_op_opt *opt);
sn_status sn_float_mp_ilogb(sn_ctx *ctx, const sn_value *a, int *exp_out);
sn_status sn_float_mp_frexp(sn_ctx *ctx, sn_value *mant, int *exp_out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_ldexp(sn_ctx *ctx, sn_value *out, const sn_value *a, int n, const sn_op_opt *opt);
sn_status sn_float_mp_modf(sn_ctx *ctx, sn_value *ipart, sn_value *fpart, const sn_value *a, const sn_op_opt *opt);
sn_status sn_float_mp_fmod(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mp_fdim(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_float_mp_remquo(sn_ctx *ctx, sn_value *out, int *quo, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

/* Opaque per-ctx soft constant cache (owned by sn_ctx.soft_cache). */
void sn_soft_cache_free(sn_ctx *ctx);

/* Pure soft math for multiprec (m_bits > 52); implemented in sn_math_soft.c */
int sn_math_need_soft(const sn_value *a);
sn_status sn_soft_exp(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_log(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_sin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_cos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_tan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_asin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_acos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_atan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_atan2(sn_ctx *ctx, sn_value *out, const sn_value *y, const sn_value *x, const sn_op_opt *opt);
sn_status sn_soft_sinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_cosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_tanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_asinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_acosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_atanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_pow(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_soft_hypot(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
sn_status sn_soft_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_erf(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_erfc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_tgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_lgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_exp2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_expm1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_log2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_log10(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_log1p(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_cbrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_j0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_j1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_jn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_y0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_y1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_yn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_i0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_i1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_in(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_k0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_k1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_kn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_ellipk(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt);
sn_status sn_soft_ellipe(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt);
sn_status sn_soft_igamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt);
sn_status sn_soft_igammac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt);
sn_status sn_soft_ibeta(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt);
sn_status sn_soft_ibetac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt);
sn_status sn_soft_jacobi_sn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);
sn_status sn_soft_jacobi_cn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);
sn_status sn_soft_jacobi_dn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);
sn_status sn_soft_digamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_trigamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_polygamma(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
sn_status sn_soft_ellipf(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt);
sn_status sn_soft_ellipeinc(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt);
sn_status sn_soft_ellipiinc(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *n, const sn_value *m, const sn_op_opt *opt);


#endif /* SN_IMPL_H */
