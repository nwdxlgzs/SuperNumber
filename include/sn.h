/*
 * SuperNumber (sn_) — public API
 * C99 + stdint.h.
 *
 * Design:
 *  - No library-global mutable state (multi-instance / crash-isolation friendly).
 *  - Public access via function-pointer tables (sn_api + sn_api_bind).
 *  - Flat sn_* symbols live in sn_flat.h for library internals and tests only.
 */
#ifndef SN_H
#define SN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Symbol visibility (optional shared-library / ELF builds).
 * Define SN_HIDE_FLAT and compile with -fvisibility=hidden (GCC/Clang on ELF)
 * so only SN_PUBLIC (sn_api_bind) stay default-visible; flat sn_* are hidden.
 * MinGW/MSVC PE and static libsn.a: attributes are no-ops (no warning spam).
 * In-tree tests always link objects directly and keep using sn_flat.h.
 */
#if defined(SN_HIDE_FLAT) && (defined(__GNUC__) || defined(__clang__)) \
    && defined(__ELF__) && !defined(_WIN32) && !defined(__CYGWIN__)
#  define SN_PUBLIC   __attribute__((visibility("default")))
#  define SN_INTERNAL __attribute__((visibility("hidden")))
#else
#  define SN_PUBLIC
#  define SN_INTERNAL
#endif


/* -------------------------------------------------------------------------- */
/* Status                                                                     */
/* -------------------------------------------------------------------------- */

typedef enum sn_status {
    SN_OK = 0,
    SN_ERR_NOMEM,
    SN_ERR_TYPE,
    SN_ERR_RANGE,
    SN_ERR_DOMAIN,
    SN_ERR_FORMAT,
    SN_ERR_DIVZERO,
    SN_ERR_INVALID,
    SN_ERR_ARG
} sn_status;

#define SN_FLAG_INVALID   1u
#define SN_FLAG_DIVZERO   2u
#define SN_FLAG_OVERFLOW  4u
#define SN_FLAG_UNDERFLOW 8u
#define SN_FLAG_INEXACT   16u

/* -------------------------------------------------------------------------- */
/* Allocator (Lua-style). Per-context only — no process-global default.       */
/* Define SN_DEBUG_ALLOC to validate osize on free/realloc in sn_alloc_default. */
/* -------------------------------------------------------------------------- */

typedef void *(*sn_alloc_fn)(void *ud, void *ptr, size_t osize, size_t nsize);

/* RNG fill: return 0 on success, non-zero on failure. */
typedef int (*sn_rng_fn)(void *ud, unsigned char *buf, size_t n);

/* -------------------------------------------------------------------------- */
/* Context                                                                    */
/* -------------------------------------------------------------------------- */

typedef enum sn_round {
    SN_ROUND_NTE = 0, /* nearest, ties to even */
    SN_ROUND_TZ,      /* toward zero */
    SN_ROUND_UP,      /* toward +inf */
    SN_ROUND_DN,      /* toward -inf */
    SN_ROUND_NA       /* nearest, ties away from zero */
} sn_round;

typedef enum sn_int_overflow {
    SN_IOV_WRAP = 0,
    SN_IOV_SATURATE
} sn_int_overflow;

typedef struct sn_ctx {
    sn_alloc_fn      alloc;
    void            *alloc_ud;
    sn_round         round;
    sn_int_overflow  iov;
    unsigned         flags;
    sn_rng_fn        rng;
    void            *rng_ud;
    uint64_t         rng_state; /* default PRNG state when using built-in RNG */
    void            *soft_cache; /* opaque multiprec math constants; freed by sn_ctx_fini */
} sn_ctx;

/* -------------------------------------------------------------------------- */
/* Value                                                                      */
/* -------------------------------------------------------------------------- */

typedef enum sn_kind {
    SN_KIND_NULL = 0,
    SN_KIND_INT,
    SN_KIND_BIGINT,
    SN_KIND_FLOAT
} sn_kind;

typedef uint32_t sn_limb;
#define SN_LIMB_BITS 32
#define SN_LIMB_MASK 0xFFFFFFFFu
#define SN_INLINE_LIMBS 4

typedef struct sn_value {
    sn_kind  kind;
    int      width;       /* INT bit width (>=1) */
    int      is_signed;   /* INT */
    int      e_bits;      /* FLOAT exponent bits */
    int      m_bits;      /* FLOAT trailing significand bits (no implicit bit) */
    int      nan_enabled; /* FLOAT: 1 = NaN allowed, 0 = map invalid to Inf */
    int      negative;    /* BIGINT sign (magnitude limbs); FLOAT sign bit path */
    int      nlimbs;
    int      cap;         /* heap limb capacity; 0 = inline */
    sn_limb  inline_limbs[SN_INLINE_LIMBS];
    sn_limb *heap;
} sn_value;

typedef struct sn_op_opt {
    int             has_round;
    sn_round        round;
    int             has_int_overflow;
    sn_int_overflow iov;
} sn_op_opt;

/* -------------------------------------------------------------------------- */
/* Construction — integers                                                    */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Construction — float                                                       */
/* Total bits = 1 + e_bits + m_bits; multi-limb. e/m memory-bound (no small hard cap). */
/* -------------------------------------------------------------------------- */

typedef enum sn_fpclass {
    SN_FP_NAN = 0,
    SN_FP_INFINITE,
    SN_FP_ZERO,
    SN_FP_SUBNORMAL,
    SN_FP_NORMAL
} sn_fpclass;

/* -------------------------------------------------------------------------- */
/* Arithmetic (INT / BIGINT / FLOAT where applicable)                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Math library (phase 4) - SN_KIND_FLOAT, C math.h style                      */
/* Host double working precision when total bits <= 64; wider formats for
 * basic arithmetic may use soft multi-limb path (transcendentals: RANGE).  */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Complex numbers (optional submodule sn_c*)                                  */
/* re/im must share the same float format. No heap inside sn_cplx itself;      */
/* limbs live in the two sn_value fields (ctx-managed).                        */
/* -------------------------------------------------------------------------- */

typedef struct sn_cplx {
    sn_value re;
    sn_value im;
} sn_cplx;


/* -------------------------------------------------------------------------- */
/* Crypto / number-theory                                                     */
/* -------------------------------------------------------------------------- */

/* Montgomery domain (odd modulus only). Values are xR mod m, R=2^(n*SN_LIMB_BITS).
 * sn_powmod uses this automatically for odd m. Public API for multi-step modular work. */
typedef struct sn_mont {
    sn_value m;     /* modulus magnitude, padded to nlimbs */
    sn_value rr;    /* R^2 mod m, R = 2^(nlimbs*SN_LIMB_BITS) */
    sn_limb  n0;    /* -m^{-1} mod 2^SN_LIMB_BITS */
    int      nlimbs;
    int      ready;
} sn_mont;

/* -------------------------------------------------------------------------- */
/* Function-pointer API tables (preferred; one bind reduces conceptual surface) */
/* -------------------------------------------------------------------------- */

typedef struct sn_api_ctx {
    void     (*init)(sn_ctx *ctx);
    void     (*fini)(sn_ctx *ctx);
    void     (*clear_flags)(sn_ctx *ctx);
    unsigned (*get_flags)(const sn_ctx *ctx);
    void     (*set_round)(sn_ctx *ctx, sn_round r);
    void     (*set_int_overflow)(sn_ctx *ctx, sn_int_overflow m);
    void     (*set_alloc)(sn_ctx *ctx, sn_alloc_fn fn, void *ud);
    void     (*set_rng)(sn_ctx *ctx, sn_rng_fn fn, void *ud);
    void     (*seed_rng)(sn_ctx *ctx, uint64_t seed);
    void    *(*malloc_fn)(sn_ctx *ctx, size_t n);
    void    *(*realloc_fn)(sn_ctx *ctx, void *p, size_t osize, size_t nsize);
    void     (*free_fn)(sn_ctx *ctx, void *p, size_t osize);
    void    *(*alloc_default)(void *ud, void *ptr, size_t osize, size_t nsize);
} sn_api_ctx;

typedef struct sn_api_value {
    void      (*init)(sn_value *v);
    void      (*clear)(sn_ctx *ctx, sn_value *v);
    sn_status (*copy)(sn_ctx *ctx, sn_value *out, const sn_value *src);
    void      (*move)(sn_value *out, sn_value *src);
} sn_api_value;

typedef struct sn_api_int {
    sn_status (*int_new)(sn_ctx *ctx, sn_value *out, int width, int is_signed);
    sn_status (*set_i64)(sn_ctx *ctx, sn_value *out, int64_t x, int width, int is_signed);
    sn_status (*set_u64)(sn_ctx *ctx, sn_value *out, uint64_t x, int width, int is_signed);
    sn_status (*bigint_set_i64)(sn_ctx *ctx, sn_value *out, int64_t x);
    sn_status (*bigint_set_u64)(sn_ctx *ctx, sn_value *out, uint64_t x);
    sn_status (*i8)(sn_ctx *ctx, sn_value *out, int64_t x);
    sn_status (*u8)(sn_ctx *ctx, sn_value *out, uint64_t x);
    sn_status (*i16)(sn_ctx *ctx, sn_value *out, int64_t x);
    sn_status (*u16)(sn_ctx *ctx, sn_value *out, uint64_t x);
    sn_status (*i32)(sn_ctx *ctx, sn_value *out, int64_t x);
    sn_status (*u32)(sn_ctx *ctx, sn_value *out, uint64_t x);
    sn_status (*i64)(sn_ctx *ctx, sn_value *out, int64_t x);
    sn_status (*u64)(sn_ctx *ctx, sn_value *out, uint64_t x);
    sn_status (*to_i64)(sn_ctx *ctx, const sn_value *v, int64_t *out);
    sn_status (*to_u64)(sn_ctx *ctx, const sn_value *v, uint64_t *out);
    sn_status (*from_str)(sn_ctx *ctx, sn_value *out, const char *s, int base, int width, int is_signed);
    sn_status (*from_str_bigint)(sn_ctx *ctx, sn_value *out, const char *s, int base);
    sn_status (*to_str)(sn_ctx *ctx, char **out, const sn_value *v, int base);
    void      (*str_free)(sn_ctx *ctx, char *s);
    sn_status (*set_long)(sn_ctx *ctx, sn_value *out, long x, int width, int is_signed);
    sn_status (*bigint_set_long)(sn_ctx *ctx, sn_value *out, long x);
    sn_status (*to_long)(sn_ctx *ctx, const sn_value *v, long *out);
    int       (*bitlen)(const sn_value *v);
    sn_status (*getbit)(const sn_value *v, int i, int *bit);
    sn_status (*setbit)(sn_ctx *ctx, sn_value *v, int i, int bit);
} sn_api_int;

typedef struct sn_api_arith {
    sn_status (*add)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*sub)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*mul)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*div)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*rem)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*neg)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*abs)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*and_)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*or_)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*xor_)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*not_)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*shl)(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt);
    sn_status (*shr)(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt);
    sn_status (*sar)(sn_ctx *ctx, sn_value *out, const sn_value *a, int bits, const sn_op_opt *opt);
    sn_status (*cmp)(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b);
} sn_api_arith;

typedef struct sn_api_float {
    sn_status  (*float_new)(sn_ctx *ctx, sn_value *out, int e_bits, int m_bits, int nan_enabled);
    sn_status  (*f16)(sn_ctx *ctx, sn_value *out, double x);
    sn_status  (*f32)(sn_ctx *ctx, sn_value *out, double x);
    sn_status  (*f64)(sn_ctx *ctx, sn_value *out, double x);
    sn_fpclass (*classify)(const sn_value *v);
    int        (*signbit)(const sn_value *v);
    sn_status  (*to_double)(sn_ctx *ctx, const sn_value *v, double *out);
    sn_status  (*totalorder)(sn_ctx *ctx, int *rel, const sn_value *a, const sn_value *b);
    sn_status  (*fma)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b,
                      const sn_value *c, const sn_op_opt *opt);
    sn_status  (*sqrt)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status  (*frem)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status  (*cast_int)(sn_ctx *ctx, sn_value *out, const sn_value *src,
                           int width, int is_signed, const sn_op_opt *opt);
    sn_status  (*cast_float)(sn_ctx *ctx, sn_value *out, const sn_value *src,
                             int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
    sn_status  (*from_str)(sn_ctx *ctx, sn_value *out, const char *s,
                           int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
    sn_status  (*from_i64)(sn_ctx *ctx, sn_value *out, int64_t x, int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
    sn_status  (*set_zero)(sn_ctx *ctx, sn_value *out, int sign, int e_bits, int m_bits, int nan_enabled);
    sn_status  (*set_inf)(sn_ctx *ctx, sn_value *out, int sign, int e_bits, int m_bits, int nan_enabled);
    sn_status  (*set_nan)(sn_ctx *ctx, sn_value *out, int e_bits, int m_bits);
} sn_api_float;

typedef struct sn_api_math {
    int (*isfinite_)(const sn_value *v);
    int (*isinf_)(const sn_value *v);
    int (*isnan_)(const sn_value *v);
    int (*isnormal_)(const sn_value *v);
    sn_status (*fabs)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*sin)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*cos)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*tan)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*exp)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*log)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*pow)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*sqrt)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*j0)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*j1)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*jn)(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
    sn_status (*y0)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*y1)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*yn)(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
    sn_status (*i0)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*i1)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*in)(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
    sn_status (*k0)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*k1)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*kn)(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
    sn_status (*ellipk)(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt);
    sn_status (*ellipe)(sn_ctx *ctx, sn_value *out, const sn_value *m, const sn_op_opt *opt);
    sn_status (*ellipf)(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt);
    sn_status (*ellipeinc)(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *m, const sn_op_opt *opt);
    sn_status (*ellipiinc)(sn_ctx *ctx, sn_value *out, const sn_value *phi, const sn_value *n, const sn_value *m, const sn_op_opt *opt);
    sn_status (*digamma)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*trigamma)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*polygamma)(sn_ctx *ctx, sn_value *out, int n, const sn_value *a, const sn_op_opt *opt);
    sn_status (*igamma)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt);
    sn_status (*igammac)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *x, const sn_op_opt *opt);
    sn_status (*ibeta)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt);
    sn_status (*ibetac)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *x, const sn_op_opt *opt);
    sn_status (*jacobi_sn)(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);
    sn_status (*jacobi_cn)(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);
    sn_status (*jacobi_dn)(sn_ctx *ctx, sn_value *out, const sn_value *u, const sn_value *m, const sn_op_opt *opt);

    sn_status (*exp2)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*expm1)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*log2)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*log10)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*log1p)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*cbrt)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*hypot)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*asin)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*acos)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*atan)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*atan2)(sn_ctx *ctx, sn_value *out, const sn_value *y, const sn_value *x, const sn_op_opt *opt);
    sn_status (*sinh)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*cosh)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*tanh)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*asinh)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*acosh)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*atanh)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*ceil)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*floor)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*trunc)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*fround)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*nearbyint)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*rint)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*modf)(sn_ctx *ctx, sn_value *ipart, sn_value *fpart, const sn_value *a, const sn_op_opt *opt);
    sn_status (*frexp)(sn_ctx *ctx, sn_value *out, int *exp, const sn_value *a, const sn_op_opt *opt);
    sn_status (*ldexp)(sn_ctx *ctx, sn_value *out, const sn_value *a, int exp, const sn_op_opt *opt);
    sn_status (*scalbn)(sn_ctx *ctx, sn_value *out, const sn_value *a, int n, const sn_op_opt *opt);
    sn_status (*ilogb)(sn_ctx *ctx, const sn_value *a, int *exp);
    sn_status (*logb)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*fmod)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*remquo)(sn_ctx *ctx, sn_value *out, int *quo, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*erf)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*erfc)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*tgamma)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*lgamma)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_op_opt *opt);
    sn_status (*nextafter)(sn_ctx *ctx, sn_value *out, const sn_value *from, const sn_value *to, const sn_op_opt *opt);
    sn_status (*copysign)(sn_ctx *ctx, sn_value *out, const sn_value *mag, const sn_value *sgn, const sn_op_opt *opt);
    sn_status (*fmin)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*fmax)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    sn_status (*fdim)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_op_opt *opt);
    /* Full math.h-style set. m_bits>52 uses soft multiprec for elementary paths.
     * Special functions (ellip/digamma/trigamma/polygamma/igamma/ibeta/jacobi/Bessel I-K) always soft. */
} sn_api_math;

typedef struct sn_api_crypto {
    sn_status (*gcd)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b);
    sn_status (*lcm)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b);
    sn_status (*isqrt)(sn_ctx *ctx, sn_value *out, const sn_value *a);
    sn_status (*popcount)(sn_ctx *ctx, sn_value *out, const sn_value *a);
    sn_status (*ctz)(sn_ctx *ctx, sn_value *out, const sn_value *a);
    sn_status (*modinv)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *m);
    sn_status (*mulmod)(sn_ctx *ctx, sn_value *out, const sn_value *a, const sn_value *b, const sn_value *m);
    sn_status (*powmod)(sn_ctx *ctx, sn_value *out, const sn_value *base, const sn_value *exp, const sn_value *m);
    sn_status (*powmod_ct)(sn_ctx *ctx, sn_value *out, const sn_value *base, const sn_value *exp, const sn_value *m);
    void      (*mont_init)(sn_mont *mont);
    void      (*mont_clear)(sn_ctx *ctx, sn_mont *mont);
    sn_status (*mont_setup)(sn_ctx *ctx, sn_mont *mont, const sn_value *modulus);
    sn_status (*mont_mul)(sn_ctx *ctx, sn_value *out, const sn_mont *mont,
                          const sn_value *a, const sn_value *b);
    sn_status (*mont_from)(sn_ctx *ctx, sn_value *out, const sn_mont *mont, const sn_value *x);
    sn_status (*mont_to)(sn_ctx *ctx, sn_value *out, const sn_mont *mont, const sn_value *x);
    sn_status (*random_bytes)(sn_ctx *ctx, unsigned char *buf, size_t n);
    sn_status (*random_u64)(sn_ctx *ctx, uint64_t *out);
    sn_status (*random_u64_mod)(sn_ctx *ctx, sn_value *out, uint64_t bound);
} sn_api_crypto;


typedef struct sn_api_complex {
    void      (*init)(sn_cplx *z);
    void      (*clear)(sn_ctx *ctx, sn_cplx *z);
    sn_status (*set)(sn_ctx *ctx, sn_cplx *z, const sn_value *re, const sn_value *im);
    sn_status (*set_d)(sn_ctx *ctx, sn_cplx *z, double re, double im,
                       int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);
    sn_status (*copy)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *src);
    sn_status (*add)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);
    sn_status (*sub)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);
    sn_status (*mul)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);
    sn_status (*div)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);
    sn_status (*neg)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_op_opt *opt);
    sn_status (*conj)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_op_opt *opt);
    sn_status (*abs)(sn_ctx *ctx, sn_value *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*arg)(sn_ctx *ctx, sn_value *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*proj)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*from_polar)(sn_ctx *ctx, sn_cplx *out, const sn_value *rho, const sn_value *theta,
                            const sn_op_opt *opt);
    sn_status (*sqrt)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*exp)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*log)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*pow)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *a, const sn_cplx *b, const sn_op_opt *opt);
    sn_status (*sin)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*cos)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*tan)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*sinh)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*cosh)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*tanh)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*asin)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*acos)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*atan)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*asinh)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*acosh)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
    sn_status (*atanh)(sn_ctx *ctx, sn_cplx *out, const sn_cplx *z, const sn_op_opt *opt);
} sn_api_complex;


/* -------------------------------------------------------------------------- */
/* Tensor (2D row-major matrix of SN floats; Qwen3/Transformer building block) */
/* Element storage uses ctx allocator. No process-global state.               */
/* -------------------------------------------------------------------------- */

typedef struct sn_tensor {
    int       rows;
    int       cols;
    int       e_bits;
    int       m_bits;
    int       nan_enabled;
    int       n;       /* rows*cols */
    sn_value *data;    /* length n; NULL if n==0 */
} sn_tensor;

typedef struct sn_api_tensor {
    void      (*init)(sn_tensor *t);
    void      (*clear)(sn_ctx *ctx, sn_tensor *t);
    sn_status (*create)(sn_ctx *ctx, sn_tensor *out, int rows, int cols,
                        int e_bits, int m_bits, int nan_enabled);
    sn_status (*copy)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *src);
    sn_status (*from_doubles)(sn_ctx *ctx, sn_tensor *out, int rows, int cols,
                              const double *data, int n,
                              int e_bits, int m_bits, int nan_enabled,
                              const sn_op_opt *opt);
    sn_status (*from_str)(sn_ctx *ctx, sn_tensor *out, const char *s,
                          int e_bits, int m_bits, int nan_enabled,
                          const sn_op_opt *opt);
    sn_status (*to_str)(sn_ctx *ctx, char **out, const sn_tensor *t);
    void      (*str_free)(sn_ctx *ctx, char *s);
    sn_status (*dims)(const sn_tensor *t, int *rows, int *cols);
    sn_status (*get)(sn_ctx *ctx, sn_value *out, const sn_tensor *t, int r, int c);
    sn_status (*set)(sn_ctx *ctx, sn_tensor *t, int r, int c, const sn_value *v);
    sn_status (*transpose)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a);
    sn_status (*reshape)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int rows, int cols);
    sn_status (*matmul)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
    sn_status (*add)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
    sn_status (*sub)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
    sn_status (*hadamard)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
    sn_status (*div)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt);
    sn_status (*scale)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_value *s, const sn_op_opt *opt);
    /* op: 0=neg 1=exp 2=tanh 3=relu 4=gelu 5=silu 6=sqrt 7=abs */
    sn_status (*unary)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int op, const sn_op_opt *opt);
    sn_status (*softmax_row)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_op_opt *opt);
    sn_status (*rms_norm)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a,
                          const sn_tensor *gamma, double eps, const sn_op_opt *opt);
    sn_status (*layer_norm)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a,
                            const sn_tensor *gamma, const sn_tensor *beta,
                            double eps, const sn_op_opt *opt);
    sn_status (*sin_pe)(sn_ctx *ctx, sn_tensor *out, int seq, int dim, double base, const sn_op_opt *opt);
    sn_status (*rope)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, double base, const sn_op_opt *opt);
    sn_status (*gather)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *table, const int *indices, int nidx);
    sn_status (*slice)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int r0, int r1, int c0, int c1);
    sn_status (*concat)(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, int axis);
    sn_status (*attention_sdp)(sn_ctx *ctx, sn_tensor *out, sn_tensor *weights_opt,
                               const sn_tensor *q, const sn_tensor *k, const sn_tensor *v,
                               int causal, double scale, const sn_op_opt *opt);
} sn_api_tensor;

typedef struct sn_api {
    sn_api_ctx    ctx;
    sn_api_value  value;
    sn_api_int    integer;
    sn_api_arith  arith;
    sn_api_float  flt;
    sn_api_math    math;
    sn_api_crypto  crypto;
    sn_api_complex cplx;
    sn_api_tensor  tensor;
} sn_api;

/* Fill all function pointers. Safe to call per-thread / per-module; no globals. */
SN_PUBLIC void sn_api_bind(sn_api *api);

#ifdef __cplusplus
}
#endif

#endif /* SN_H */
