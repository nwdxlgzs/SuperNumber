/* Flat SuperNumber C API (compatibility).
 * Prefer sn_api via sn_api_bind for new code.
 * Library internals and tests include this header.
 */
#ifndef SN_FLAT_H
#define SN_FLAT_H

#include "sn.h"

/* Flat symbols: prefer sn_api for new code.
 * When SN_HIDE_FLAT is set, these declarations are SN_INTERNAL (hidden).
 */

#ifdef __cplusplus
extern "C" {
#endif

SN_INTERNAL void *sn_alloc_default(void *ud, void *ptr, size_t osize, size_t nsize);

SN_INTERNAL void     sn_ctx_init(sn_ctx *ctx); /* alloc := sn_alloc_default; no globals */
/* Release soft-math constant cache (and any future ctx-owned soft state). Safe on fresh ctx. */
SN_INTERNAL void     sn_ctx_fini(sn_ctx *ctx);

SN_INTERNAL void     sn_ctx_clear_flags(sn_ctx *ctx);

SN_INTERNAL unsigned sn_ctx_get_flags(const sn_ctx *ctx);

SN_INTERNAL void     sn_ctx_set_round(sn_ctx *ctx, sn_round r);

SN_INTERNAL void     sn_ctx_set_int_overflow(sn_ctx *ctx, sn_int_overflow m);

SN_INTERNAL void     sn_ctx_set_alloc(sn_ctx *ctx, sn_alloc_fn fn, void *ud); /* fn==NULL -> default */
SN_INTERNAL void     sn_ctx_set_rng(sn_ctx *ctx, sn_rng_fn fn, void *ud); /* fn==NULL -> default PRNG */
SN_INTERNAL void     sn_ctx_seed_rng(sn_ctx *ctx, uint64_t seed);

SN_INTERNAL void *sn_malloc(sn_ctx *ctx, size_t n);

SN_INTERNAL void *sn_realloc(sn_ctx *ctx, void *p, size_t osize, size_t nsize);

SN_INTERNAL void  sn_free(sn_ctx *ctx, void *p, size_t osize);

SN_INTERNAL void      sn_value_init(sn_value *v);

SN_INTERNAL void      sn_value_clear(sn_ctx *ctx, sn_value *v);

SN_INTERNAL sn_status sn_value_copy(sn_ctx *ctx, sn_value *out, const sn_value *src);

SN_INTERNAL void      sn_value_move(sn_value *out, sn_value *src);

SN_INTERNAL sn_status sn_int_new(sn_ctx *ctx, sn_value *out, int width, int is_signed);

SN_INTERNAL sn_status sn_int_set_i64(sn_ctx *ctx, sn_value *out, int64_t x, int width, int is_signed);

SN_INTERNAL sn_status sn_int_set_u64(sn_ctx *ctx, sn_value *out, uint64_t x, int width, int is_signed);

SN_INTERNAL sn_status sn_int_set_long(sn_ctx *ctx, sn_value *out, long x, int width, int is_signed);

SN_INTERNAL sn_status sn_bigint_set_i64(sn_ctx *ctx, sn_value *out, int64_t x);

SN_INTERNAL sn_status sn_bigint_set_u64(sn_ctx *ctx, sn_value *out, uint64_t x);

SN_INTERNAL sn_status sn_bigint_set_long(sn_ctx *ctx, sn_value *out, long x);

SN_INTERNAL sn_status sn_i8 (sn_ctx *ctx, sn_value *out, int64_t x);

SN_INTERNAL sn_status sn_u8 (sn_ctx *ctx, sn_value *out, uint64_t x);

SN_INTERNAL sn_status sn_i16(sn_ctx *ctx, sn_value *out, int64_t x);

SN_INTERNAL sn_status sn_u16(sn_ctx *ctx, sn_value *out, uint64_t x);

SN_INTERNAL sn_status sn_i32(sn_ctx *ctx, sn_value *out, int64_t x);

SN_INTERNAL sn_status sn_u32(sn_ctx *ctx, sn_value *out, uint64_t x);

SN_INTERNAL sn_status sn_i64(sn_ctx *ctx, sn_value *out, int64_t x);

SN_INTERNAL sn_status sn_u64(sn_ctx *ctx, sn_value *out, uint64_t x);

SN_INTERNAL sn_status sn_float_new(sn_ctx *ctx, sn_value *out, int e_bits, int m_bits, int nan_enabled);

SN_INTERNAL sn_status sn_f16(sn_ctx *ctx, sn_value *out, double x);

SN_INTERNAL sn_status sn_f32(sn_ctx *ctx, sn_value *out, double x);

SN_INTERNAL sn_status sn_f64(sn_ctx *ctx, sn_value *out, double x);

SN_INTERNAL sn_status sn_float_from_i64(sn_ctx *ctx, sn_value *out, int64_t x,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_float_set_zero(sn_ctx *ctx, sn_value *out, int sign,
                            int e_bits, int m_bits, int nan_enabled);

SN_INTERNAL sn_status sn_float_set_inf(sn_ctx *ctx, sn_value *out, int sign,
                           int e_bits, int m_bits, int nan_enabled);

SN_INTERNAL sn_status sn_float_set_nan(sn_ctx *ctx, sn_value *out,
                           int e_bits, int m_bits);

SN_INTERNAL sn_fpclass sn_fp_classify(const sn_value *v);

SN_INTERNAL int        sn_fp_signbit(const sn_value *v);

SN_INTERNAL sn_status  sn_to_double(sn_ctx *ctx, const sn_value *v, double *out);

/* IEEE-754 totalOrder: -NaN < -Inf < ... < +Inf < +NaN; returns rel like sn_cmp */
SN_INTERNAL sn_status sn_totalorder(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b);

SN_INTERNAL sn_status sn_fma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                 const sn_value *c, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_sqrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_frem(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cast_int(sn_ctx *ctx, sn_value *out, const sn_value *src,
                      int width, int is_signed, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cast_float(sn_ctx *ctx, sn_value *out, const sn_value *src,
                        int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);

/* Decimal / scientific float string (e.g. 1.25e-3, 0x1.8p+4). */
SN_INTERNAL sn_status sn_from_str_float(sn_ctx *ctx, sn_value *out, const char *s,
                            int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_add(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_sub(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_mul(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_div(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_rem(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_neg(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_abs(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_and(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_or (sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_xor(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_not(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_shl(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt);

/* Logical right shift (zero-fill). */
SN_INTERNAL sn_status sn_shr(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt);

/* Arithmetic right shift (sign-fill for signed INT / negative BIGINT). */
SN_INTERNAL sn_status sn_sar(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cmp(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b);

SN_INTERNAL int       sn_bitlen(const sn_value *v);

SN_INTERNAL sn_status sn_getbit(const sn_value *v, int i, int *bit);

SN_INTERNAL sn_status sn_setbit(sn_ctx *ctx, sn_value *v, int i, int bit);

SN_INTERNAL sn_status sn_to_i64(sn_ctx *ctx, const sn_value *v, int64_t *out);

SN_INTERNAL sn_status sn_to_u64(sn_ctx *ctx, const sn_value *v, uint64_t *out);

SN_INTERNAL sn_status sn_to_long(sn_ctx *ctx, const sn_value *v, long *out);

/* base: 2, 8, 10, 16, or 0 auto.
 * Decimal integers accept scientific form when base is 10 or 0: "1.5e3" -> 1500.
 * to_str allocates via ctx; free with sn_str_free (same ctx). */
SN_INTERNAL sn_status sn_from_str(sn_ctx *ctx, sn_value *out, const char *s, int base, int width, int is_signed);

SN_INTERNAL sn_status sn_from_str_bigint(sn_ctx *ctx, sn_value *out, const char *s, int base);

SN_INTERNAL sn_status sn_to_str(sn_ctx *ctx, char **out, const sn_value *v, int base);
SN_INTERNAL sn_status sn_float_to_str(sn_ctx *ctx, char **out, const sn_value *v);
SN_INTERNAL sn_status sn_float_to_str_base(sn_ctx *ctx, char **out, const sn_value *v, int base);

SN_INTERNAL void      sn_str_free(sn_ctx *ctx, char *s);

SN_INTERNAL int sn_isfinite(const sn_value *v);

SN_INTERNAL int sn_isinf(const sn_value *v);

SN_INTERNAL int sn_isnan(const sn_value *v);

SN_INTERNAL int sn_isnormal(const sn_value *v);

SN_INTERNAL sn_status sn_fabs(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_copysign(sn_ctx *ctx, sn_value *out, const sn_value *mag, const sn_value *sgn, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_fmin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_fmax(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_fdim(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_exp(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_exp2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_expm1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_log(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_log2(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_log10(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_log1p(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_pow(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cbrt(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_hypot(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_sin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_tan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_asin(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_acos(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_atan(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_atan2(sn_ctx *ctx, sn_value *out, const sn_value *y, const sn_value *x, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_sinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_tanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_asinh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_acosh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_atanh(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_ceil(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_floor(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_trunc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_fround(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_nearbyint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_rint(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_modf(sn_ctx *ctx, sn_value *ipart, sn_value *fpart, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_frexp(sn_ctx *ctx, sn_value *out, int *exp, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_ldexp(sn_ctx *ctx, sn_value *out, const sn_value *a, int exp, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_scalbn(sn_ctx *ctx, sn_value *out, const sn_value *a, int n, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_ilogb(sn_ctx *ctx, const sn_value *a, int *exp);

SN_INTERNAL sn_status sn_logb(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_fmod(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_remquo(sn_ctx *ctx, sn_value *out, int *quo, const sn_value *a, const sn_value *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_erf(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_erfc(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_tgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_lgamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

/* digamma ?(x)=?'(x)/?(x). Soft for all formats. Poles at non-positive integers. */
SN_INTERNAL sn_status sn_digamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

/* trigamma psi_1(x)=psi^{(1)}(x). Soft. Poles at non-positive integers. */
SN_INTERNAL sn_status sn_trigamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

/* polygamma psi^{(n)}(x). n=0 digamma; n>=1 via recurrence+asymptotic. Soft. */
SN_INTERNAL sn_status sn_polygamma(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_nextafter(sn_ctx *ctx, sn_value *out, const sn_value *from, const sn_value *to, const sn_op_opt *opt);

/* Bessel functions of the first/second kind (host math.h for m_bits<=52;
 * multiprec uses soft series/recurrence in sn_math_soft.c). */
SN_INTERNAL sn_status sn_j0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_j1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_jn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_y0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_y1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_yn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);

/* Modified Bessel of the first/second kind (I_n / K_n).
 * Portable soft implementation for all formats (MinGW libm has no i0/k0).
 * Domain: K requires x > 0; I is even/odd in x by order parity. */
SN_INTERNAL sn_status sn_i0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_i1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_in(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_k0(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_k1(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_kn(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);

/* Complete elliptic integrals (parameter m = k^2 in [0,1)).
 * sn_ellipk(m)=K(m), sn_ellipe(m)=E(m). Portable AGM soft for all formats. */
SN_INTERNAL sn_status sn_ellipk(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_ellipe(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt);

/* Incomplete elliptic integral of the first kind F(?|m), m=k^2.
 * Domain: m in [0,1), ? real with |?|<=?/2 (odd in ?). F(?/2|m)=K(m). Soft. */
SN_INTERNAL sn_status sn_ellipf(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt);

/* Incomplete elliptic integral of the second kind E(phi|m), m=k^2.
 * Domain: m in [0,1], |phi|<=pi/2 (odd in phi). E(pi/2|m)=E(m) complete. Soft Carlson RD. */
SN_INTERNAL sn_status sn_ellipeinc(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt);

/* Incomplete elliptic integral of the third kind Pi(n;phi|m), m=k^2.
 * Domain: m in [0,1], |phi|<=pi/2, 1-n sin^2(phi)>0. Odd in phi. Soft Carlson RJ. */
SN_INTERNAL sn_status sn_ellipiinc(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *n, const sn_value *m, const sn_op_opt *opt);

/* Regularized incomplete gamma: P(a,x)=igamma, Q(a,x)=igammac. a>0, x>=0. Soft for all formats. */
SN_INTERNAL sn_status sn_igamma(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_igammac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt);

/* Regularized incomplete beta: I_x(a,b)=ibeta, complement ibetac. a>0,b>0,x in [0,1]. Soft. */
SN_INTERNAL sn_status sn_ibeta(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_ibetac(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt);

/* Jacobi elliptic sn/cn/dn(u|m), m=k^2 in [0,1]. Soft AGM/Landen. */
SN_INTERNAL sn_status sn_jacobi_sn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_jacobi_cn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_jacobi_dn(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);

SN_INTERNAL void      sn_cplx_init(sn_cplx *z);

SN_INTERNAL void      sn_cplx_clear(sn_ctx *ctx, sn_cplx *z);

/* Copy re/im into z (same float format required). */
SN_INTERNAL sn_status sn_cplx_set(sn_ctx *ctx, sn_cplx *z, const sn_value *re, const sn_value *im);

/* Build rectangular from host doubles into format (e,m,nan). */
SN_INTERNAL sn_status sn_cplx_set_d(sn_ctx *ctx, sn_cplx *z, double re, double im,
                        int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_copy(sn_ctx *ctx, sn_cplx *out, const sn_cplx *src);

SN_INTERNAL sn_status sn_cplx_add(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_sub(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_mul(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_div(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_neg(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_conj(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_op_opt *opt);

/* |z| as real float in same format as z.re */
SN_INTERNAL sn_status sn_cplx_abs(sn_ctx *ctx, sn_value *out, const sn_cplx *z, const sn_op_opt *opt);

/* arg(z) = atan2(im, re) */
SN_INTERNAL sn_status sn_cplx_arg(sn_ctx *ctx, sn_value *out, const sn_cplx *z, const sn_op_opt *opt);

/* C99 cproj: project onto Riemann sphere (infinity -> inf+0i with sign of imag) */
SN_INTERNAL sn_status sn_cplx_proj(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

/* out = re + i*im from polar: rho * exp(i*theta); rho/theta real floats same format. */
SN_INTERNAL sn_status sn_cplx_from_polar(sn_ctx *ctx, sn_cplx *out, const sn_value *rho, const sn_value *theta,
                             const sn_op_opt *opt);

/* Elementary / transcendental complex functions (use real sn_* under the hood;
 * multiprec re/im automatically take soft paths when m_bits>52). */
SN_INTERNAL sn_status sn_cplx_sqrt(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_exp(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_log(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_pow(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_sin(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_cos(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_tan(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_sinh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_cosh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_tanh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_asin(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_acos(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_atan(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_asinh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_acosh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_cplx_atanh(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);

SN_INTERNAL sn_status sn_gcd(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b);

/* lcm(|a|,|b|) = |a|/gcd * |b|; result non-negative BIGINT. lcm(0,0)=0. */
SN_INTERNAL sn_status sn_lcm(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b);

/* Integer square root floor(sqrt(|x|)); domain error if x < 0 for signed path (we take mag). */
SN_INTERNAL sn_status sn_isqrt(sn_ctx *ctx, sn_value *out, const sn_value *a);

/* Population count of magnitude bits (absolute value limb pattern). */
SN_INTERNAL sn_status sn_popcount(sn_ctx *ctx, sn_value *out, const sn_value *a);

/* Count trailing zero bits of magnitude; 0 if value is 0. */
SN_INTERNAL sn_status sn_ctz(sn_ctx *ctx, sn_value *out, const sn_value *a);

SN_INTERNAL sn_status sn_modinv(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *m);

SN_INTERNAL sn_status sn_mulmod(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *m);

SN_INTERNAL sn_status sn_powmod(sn_ctx *ctx, sn_value *out, const sn_value *base, const sn_value *exp, const sn_value *m);

/* Constant-time-ish modular exponentiation for secret exponents (odd modulus only).
 * - Montgomery with fixed-width limbs + branch-free final reduction in mont_mul_raw.
 * - Always square + always multiply; bit selects with masks (no secret branch on bit).
 * - Scans full exponent limb buffer (padded to at least modulus nlimbs); pad further if needed.
 * - Not a full side-channel stack (heap alloc, cache, setup/from div remain non-CT).
 * - Even modulus -> SN_ERR_DOMAIN (use sn_powmod for non-secret even-m cases).
 */
SN_INTERNAL sn_status sn_powmod_ct(sn_ctx *ctx, sn_value *out, const sn_value *base, const sn_value *exp, const sn_value *m);

SN_INTERNAL void      sn_mont_init(sn_mont *mont);

SN_INTERNAL void      sn_mont_clear(sn_ctx *ctx, sn_mont *mont);

/* SN_ERR_DOMAIN if modulus even; SN_ERR_RANGE if m<=1 */
SN_INTERNAL sn_status sn_mont_setup(sn_ctx *ctx, sn_mont *mont, const sn_value *modulus);

/* Montgomery multiply: out = a*b*R^{-1} mod m (a,b may be normal or domain values) */
SN_INTERNAL sn_status sn_mont_mul(sn_ctx *ctx, sn_value *out, const sn_mont *mont,
                      const sn_value *a, const sn_value *b);

SN_INTERNAL sn_status sn_mont_from(sn_ctx *ctx, sn_value *out, const sn_mont *mont, const sn_value *x);

SN_INTERNAL sn_status sn_mont_to(sn_ctx *ctx, sn_value *out, const sn_mont *mont, const sn_value *x);

/* Default xorshift64* is NOT CSPRNG. */
SN_INTERNAL sn_status sn_random_bytes(sn_ctx *ctx, unsigned char *buf, size_t n);

SN_INTERNAL sn_status sn_random_u64(sn_ctx *ctx, uint64_t *out);

SN_INTERNAL sn_status sn_random_u64_mod(sn_ctx *ctx, sn_value *out, uint64_t bound);


/* Tensor (2D SN floats) */
SN_INTERNAL void      sn_tensor_init(sn_tensor *t);
SN_INTERNAL void      sn_tensor_clear(sn_ctx *ctx, sn_tensor *t);
SN_INTERNAL sn_status sn_tensor_create(sn_ctx *ctx, sn_tensor *out, int rows, int cols,
                            int e_bits, int m_bits, int nan_enabled);
SN_INTERNAL sn_status sn_tensor_copy(sn_ctx *ctx, sn_tensor *out, const sn_tensor *src);
SN_INTERNAL sn_status sn_tensor_from_doubles(sn_ctx *ctx, sn_tensor *out, int rows, int cols,
                                  const double *data, int n,
                                  int e_bits, int m_bits, int nan_enabled,
                                  const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_from_str(sn_ctx *ctx, sn_tensor *out, const char *s,
                              int e_bits, int m_bits, int nan_enabled,
                              const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_to_str(sn_ctx *ctx, char **out, const sn_tensor *t);
SN_INTERNAL void      sn_tensor_str_free(sn_ctx *ctx, char *s);
SN_INTERNAL sn_status sn_tensor_dims(const sn_tensor *t, int *rows, int *cols);
SN_INTERNAL sn_status sn_tensor_get(sn_ctx *ctx, sn_value *out, const sn_tensor *t, int r, int c);
SN_INTERNAL sn_status sn_tensor_set(sn_ctx *ctx, sn_tensor *t, int r, int c, const sn_value *v);
SN_INTERNAL sn_status sn_tensor_transpose(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a);
SN_INTERNAL sn_status sn_tensor_reshape(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int rows, int cols);
SN_INTERNAL sn_status sn_tensor_matmul(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_add(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_sub(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_hadamard(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_div(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_scale(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_value *s, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_unary(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int op, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_softmax_row(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_rms_norm(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a,
                              const sn_tensor *gamma, double eps, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_layer_norm(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a,
                              const sn_tensor *gamma, const sn_tensor *beta,
                              double eps, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_sin_pe(sn_ctx *ctx, sn_tensor *out, int seq, int dim,
                              double base, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_rope(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, double base, const sn_op_opt *opt);
SN_INTERNAL sn_status sn_tensor_gather(sn_ctx *ctx, sn_tensor *out, const sn_tensor *table, const int *indices, int nidx);
SN_INTERNAL sn_status sn_tensor_slice(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int r0, int r1, int c0, int c1);
SN_INTERNAL sn_status sn_tensor_concat(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, int axis);
SN_INTERNAL sn_status sn_tensor_attention_sdp(sn_ctx *ctx, sn_tensor *out, sn_tensor *weights_opt,
                                   const sn_tensor *q, const sn_tensor *k, const sn_tensor *v,
                                   int causal, double scale, const sn_op_opt *opt);

#ifdef __cplusplus
}
#endif

#endif /* SN_FLAT_H */
