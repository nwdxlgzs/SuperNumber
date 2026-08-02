#include "sn.h"
#include "sn_flat.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

static int nearly_eq(double a, double b, double rel)
{
    double d, aa, ab;
    if (a != a && b != b) return 1; /* both NaN */
    if (a != a || b != b) return 0;
    if (a == b) return 1;
    d = a - b;
    if (d < 0) d = -d;
    aa = a < 0 ? -a : a;
    ab = b < 0 ? -b : b;
    if (aa < ab) aa = ab;
    if (aa < 1e-300) return d < 1e-12;
    return d <= rel * aa;
}

static void check_unary(sn_ctx *ctx, sn_value *a, sn_value *r,
                        sn_status (*fn)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                        double (*ref)(double), double x, double rel)
{
    double got, expect;
    CHECK(sn_f64(ctx, a, x) == SN_OK);
    CHECK(fn(ctx, r, a, NULL) == SN_OK);
    CHECK(sn_to_double(ctx, r, &got) == SN_OK);
    expect = ref(x);
    CHECK(nearly_eq(got, expect, rel));
}

int test_math_run(void)
{
    sn_ctx ctx;
    sn_value a, b, r, s;
    double d;
    int e, q;
    int64_t i;

    sn_ctx_init(&ctx);
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&r);
    sn_value_init(&s);

    /* classify */
    CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
    CHECK(sn_isfinite(&a));
    CHECK(sn_isnormal(&a));
    CHECK(!sn_isnan(&a));
    CHECK(!sn_isinf(&a));
    CHECK(sn_float_set_inf(&ctx, &a, 0, 11, 52, 1) == SN_OK);
    CHECK(sn_isinf(&a));
    CHECK(!sn_isfinite(&a));
    CHECK(sn_float_set_nan(&ctx, &a, 11, 52) == SN_OK);
    CHECK(sn_isnan(&a));

    /* fabs / copysign */
    CHECK(sn_f64(&ctx, &a, -3.5) == SN_OK);
    CHECK(sn_fabs(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 3.5, 1e-15));
    CHECK(sn_f64(&ctx, &b, -1.0) == SN_OK);
    CHECK(sn_f64(&ctx, &a, 2.0) == SN_OK);
    CHECK(sn_copysign(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, -2.0, 1e-15));

    /* fmin / fmax / fdim */
    CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 2.0) == SN_OK);
    CHECK(sn_fmin(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1.0, 1e-15));
    CHECK(sn_fmax(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 2.0, 1e-15));
    CHECK(sn_fdim(&ctx, &r, &b, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1.0, 1e-15));

    /* exp / log family */
    check_unary(&ctx, &a, &r, sn_exp, exp, 1.0, 1e-12);
    check_unary(&ctx, &a, &r, sn_exp2, exp2, 3.0, 1e-12);
    check_unary(&ctx, &a, &r, sn_expm1, expm1, 0.1, 1e-12);
    check_unary(&ctx, &a, &r, sn_log, log, 2.718281828459045, 1e-12);
    check_unary(&ctx, &a, &r, sn_log2, log2, 8.0, 1e-12);
    check_unary(&ctx, &a, &r, sn_log10, log10, 1000.0, 1e-12);
    check_unary(&ctx, &a, &r, sn_log1p, log1p, 0.0, 1e-15);

    /* log domain */
    CHECK(sn_f64(&ctx, &a, -1.0) == SN_OK);
    sn_ctx_clear_flags(&ctx);
    CHECK(sn_log(&ctx, &r, &a, NULL) == SN_OK); /* packs NaN or Inf */
    CHECK(sn_ctx_get_flags(&ctx) & SN_FLAG_INVALID);

    /* pow / cbrt / hypot */
    CHECK(sn_f64(&ctx, &a, 2.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 10.0) == SN_OK);
    CHECK(sn_pow(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1024.0, 1e-12));
    check_unary(&ctx, &a, &r, sn_cbrt, cbrt, 27.0, 1e-12);
    CHECK(sn_f64(&ctx, &a, 3.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 4.0) == SN_OK);
    CHECK(sn_hypot(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 5.0, 1e-12));

    /* trig */
    check_unary(&ctx, &a, &r, sn_sin, sin, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_cos, cos, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_tan, tan, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_asin, asin, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_acos, acos, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_atan, atan, 0.5, 1e-12);
    CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 1.0) == SN_OK);
    CHECK(sn_atan2(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, atan2(1.0, 1.0), 1e-12));

    /* hyperbolic */
    check_unary(&ctx, &a, &r, sn_sinh, sinh, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_cosh, cosh, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_tanh, tanh, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_asinh, asinh, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_acosh, acosh, 2.0, 1e-12);
    check_unary(&ctx, &a, &r, sn_atanh, atanh, 0.5, 1e-12);

    /* rounding */
    CHECK(sn_f64(&ctx, &a, 1.6) == SN_OK);
    CHECK(sn_ceil(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 2.0, 1e-15));
    CHECK(sn_floor(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1.0, 1e-15));
    CHECK(sn_trunc(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1.0, 1e-15));
    CHECK(sn_fround(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 2.0, 1e-15));

    /* modf / frexp / ldexp / scalbn / ilogb */
    CHECK(sn_f64(&ctx, &a, 3.75) == SN_OK);
    CHECK(sn_modf(&ctx, &r, &s, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 3.0, 1e-15));
    CHECK(sn_to_double(&ctx, &s, &d) == SN_OK && nearly_eq(d, 0.75, 1e-15));
    CHECK(sn_frexp(&ctx, &r, &e, &a, NULL) == SN_OK);
    CHECK(e == 2);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 0.9375, 1e-15));
    CHECK(sn_ldexp(&ctx, &r, &a, -1, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1.875, 1e-15));
    CHECK(sn_scalbn(&ctx, &r, &a, 1, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 7.5, 1e-15));
    CHECK(sn_ilogb(&ctx, &a, &e) == SN_OK);
    CHECK(e == ilogb(3.75));

    /* fmod / remquo */
    CHECK(sn_f64(&ctx, &a, 5.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 2.0) == SN_OK);
    CHECK(sn_fmod(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1.0, 1e-15));
    CHECK(sn_remquo(&ctx, &r, &q, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, remainder(5.0, 2.0), 1e-15));

    /* special: erf / tgamma */
    check_unary(&ctx, &a, &r, sn_erf, erf, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_erfc, erfc, 0.5, 1e-12);
    check_unary(&ctx, &a, &r, sn_tgamma, tgamma, 5.0, 1e-12); /* 4! = 24 */
    CHECK(sn_f64(&ctx, &a, 5.0) == SN_OK);
    CHECK(sn_tgamma(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 24.0, 1e-10));

    /* nextafter binary64 */
    CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 2.0) == SN_OK);
    CHECK(sn_nextafter(&ctx, &r, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && d == nextafter(1.0, 2.0));

    /* f32 path: sin */
    CHECK(sn_f32(&ctx, &a, 0.5f) == SN_OK);
    CHECK(sn_sin(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK);
    CHECK(nearly_eq(d, (double)sinf(0.5f), 1e-5));

    (void)i;
    sn_value_clear(&ctx, &a);
    sn_value_clear(&ctx, &b);
    sn_value_clear(&ctx, &r);
    sn_value_clear(&ctx, &s);
    return 0;
}
