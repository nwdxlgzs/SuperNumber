/* Random cross-checks: sn f64 vs host math.h; multiprec identities. */
#include "sn.h"
#include "sn_flat.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

double j0(double);
double j1(double);
double y0(double);

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

/* Deterministic xorshift32 */
static uint32_t xs_state = 0xC0FFEEu;

static uint32_t xs_u32(void)
{
    uint32_t x = xs_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xs_state = x;
    return x;
}

static double xs_unit(void)
{
    /* (0,1) open-ish */
    return ((double)(xs_u32() >> 8) + 0.5) * (1.0 / 16777216.0);
}

static double xs_signed_unit(void)
{
    return xs_unit() * 2.0 - 1.0;
}

static int nearly_eq(double a, double b, double rel, double abs_tol)
{
    double d, aa, ab, tol;
    if (a != a && b != b) return 1; /* both NaN */
    if (a != a || b != b) return 0;
    if (a == b) return 1;
    /* inf mismatch */
    if ((a > 1e300 && b > 1e300) || (a < -1e300 && b < -1e300)) return 1;
    d = a - b;
    if (d < 0) d = -d;
    aa = a < 0 ? -a : a;
    ab = b < 0 ? -b : b;
    if (aa < ab) aa = ab;
    tol = abs_tol;
    if (rel * aa > tol) tol = rel * aa;
    return d <= tol;
}

static void report_mismatch(const char *tag, double x, double got, double expect)
{
    static int n;
    if (n < 8) {
        fprintf(stderr, "cross mismatch %s x=%.17g got=%.17g expect=%.17g\n",
                tag, x, got, expect);
        n++;
    }
}

static int load_f64(sn_ctx *ctx, sn_value *v, double x)
{
    return sn_f64(ctx, v, x) == SN_OK;
}

static int load_mp(sn_ctx *ctx, sn_value *v, sn_value *tmp, double x)
{
    if (sn_f64(ctx, tmp, x) != SN_OK) return 0;
    return sn_cast_float(ctx, v, tmp, 15, 80, 1, NULL) == SN_OK;
}

static void check_unary_f64(sn_ctx *ctx, sn_value *a, sn_value *r,
                            sn_status (*fn)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                            double (*ref)(double), double x,
                            double rel, double abs_tol, const char *tag)
{
    double got, expect;
    CHECK(load_f64(ctx, a, x));
    CHECK(fn(ctx, r, a, NULL) == SN_OK);
    CHECK(sn_to_double(ctx, r, &got) == SN_OK);
    expect = ref(x);
    if (!nearly_eq(got, expect, rel, abs_tol)) {
        report_mismatch(tag, x, got, expect);
        CHECK(0);
    } else {
        CHECK(1);
    }
}

static void check_binary_f64(sn_ctx *ctx, sn_value *a, sn_value *b, sn_value *r,
                             sn_status (*fn)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *),
                             double (*ref)(double, double), double x, double y,
                             double rel, double abs_tol, const char *tag)
{
    double got, expect;
    CHECK(load_f64(ctx, a, x));
    CHECK(load_f64(ctx, b, y));
    CHECK(fn(ctx, r, a, b, NULL) == SN_OK);
    CHECK(sn_to_double(ctx, r, &got) == SN_OK);
    expect = ref(x, y);
    if (!nearly_eq(got, expect, rel, abs_tol)) {
        report_mismatch(tag, x, got, expect);
        CHECK(0);
    } else {
        CHECK(1);
    }
}

/* Multiprec: compare soft result (rounded to double) against host ref */
static void check_unary_mp(sn_ctx *ctx, sn_value *a, sn_value *tmp, sn_value *r,
                           sn_status (*fn)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                           double (*ref)(double), double x,
                           double rel, double abs_tol, const char *tag)
{
    double got, expect;
    CHECK(load_mp(ctx, a, tmp, x));
    CHECK(fn(ctx, r, a, NULL) == SN_OK);
    CHECK(sn_to_double(ctx, r, &got) == SN_OK);
    expect = ref(x);
    if (!nearly_eq(got, expect, rel, abs_tol)) {
        report_mismatch(tag, x, got, expect);
        CHECK(0);
    } else {
        CHECK(1);
    }
}

static void check_binary_mp(sn_ctx *ctx, sn_value *a, sn_value *b, sn_value *tmp, sn_value *r,
                            sn_status (*fn)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *),
                            double (*ref)(double, double), double x, double y,
                            double rel, double abs_tol, const char *tag)
{
    double got, expect;
    CHECK(load_mp(ctx, a, tmp, x));
    CHECK(load_mp(ctx, b, tmp, y));
    CHECK(fn(ctx, r, a, b, NULL) == SN_OK);
    CHECK(sn_to_double(ctx, r, &got) == SN_OK);
    expect = ref(x, y);
    if (!nearly_eq(got, expect, rel, abs_tol)) {
        report_mismatch(tag, x, got, expect);
        CHECK(0);
    } else {
        CHECK(1);
    }
}

static double clamp_trig(double x)
{
    /* keep away from huge args (argument reduction stress) but allow moderate range */
    if (x > 40.0) x = 40.0;
    if (x < -40.0) x = -40.0;
    return x;
}

static double safe_tan_x(double u)
{
    /* map u in (-1,1) to (-pi/2+eps, pi/2-eps) */
    const double halfpi = 1.5707963267948966;
    return u * (halfpi - 1e-3);
}

int test_cross_run(void)
{
    sn_ctx ctx;
    sn_value a, b, r, s, t, tmp;
    int i, e, e2, q, qh;
    double x, y, got, g2, expect;
    const int N = 48; /* samples per family; keeps suite fast */

    sn_ctx_init(&ctx);
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&r);
    sn_value_init(&s);
    sn_value_init(&t);
    sn_value_init(&tmp);
    xs_state = 0xC0FFEEu;

    printf("  cross: f64 vs host + multiprec identities (N=%d)\n", N);

    /* ========== f64 vs host ========== */
    for (i = 0; i < N; i++) {
        x = xs_signed_unit() * 8.0; /* [-8,8] */
        check_unary_f64(&ctx, &a, &r, sn_exp, exp, x, 1e-12, 1e-14, "exp");
        check_unary_f64(&ctx, &a, &r, sn_expm1, expm1, x * 0.25, 1e-12, 1e-14, "expm1");
        check_unary_f64(&ctx, &a, &r, sn_exp2, exp2, x * 0.5, 1e-12, 1e-14, "exp2");
        check_unary_f64(&ctx, &a, &r, sn_sinh, sinh, x * 0.5, 1e-11, 1e-13, "sinh");
        check_unary_f64(&ctx, &a, &r, sn_cosh, cosh, x * 0.5, 1e-11, 1e-13, "cosh");
        check_unary_f64(&ctx, &a, &r, sn_tanh, tanh, x, 1e-12, 1e-14, "tanh");
        check_unary_f64(&ctx, &a, &r, sn_asinh, asinh, x, 1e-12, 1e-14, "asinh");
        check_unary_f64(&ctx, &a, &r, sn_erf, erf, x * 0.5, 1e-12, 1e-14, "erf");
        check_unary_f64(&ctx, &a, &r, sn_erfc, erfc, x * 0.5, 1e-12, 1e-14, "erfc");
        check_unary_f64(&ctx, &a, &r, sn_cbrt, cbrt, x, 1e-12, 1e-14, "cbrt");
        check_unary_f64(&ctx, &a, &r, sn_floor, floor, x * 3.7, 0.0, 0.0, "floor");
        check_unary_f64(&ctx, &a, &r, sn_ceil, ceil, x * 3.7, 0.0, 0.0, "ceil");
        check_unary_f64(&ctx, &a, &r, sn_trunc, trunc, x * 3.7, 0.0, 0.0, "trunc");
    }

    for (i = 0; i < N; i++) {
        x = xs_unit() * 50.0 + 1e-8; /* positive for log */
        check_unary_f64(&ctx, &a, &r, sn_log, log, x, 1e-12, 1e-14, "log");
        check_unary_f64(&ctx, &a, &r, sn_log2, log2, x, 1e-12, 1e-14, "log2");
        check_unary_f64(&ctx, &a, &r, sn_log10, log10, x, 1e-12, 1e-14, "log10");
        check_unary_f64(&ctx, &a, &r, sn_sqrt, sqrt, x, 1e-15, 1e-16, "sqrt");
        y = xs_unit() * 0.5; /* small for log1p */
        check_unary_f64(&ctx, &a, &r, sn_log1p, log1p, y, 1e-12, 1e-14, "log1p");
        y = 1.0 + xs_unit() * 20.0; /* acosh domain */
        check_unary_f64(&ctx, &a, &r, sn_acosh, acosh, y, 1e-11, 1e-13, "acosh");
    }

    for (i = 0; i < N; i++) {
        x = xs_signed_unit() * 0.999; /* asin/acos/atanh domain */
        check_unary_f64(&ctx, &a, &r, sn_asin, asin, x, 1e-12, 1e-14, "asin");
        check_unary_f64(&ctx, &a, &r, sn_acos, acos, x, 1e-12, 1e-14, "acos");
        check_unary_f64(&ctx, &a, &r, sn_atan, atan, x * 10.0, 1e-12, 1e-14, "atan");
        check_unary_f64(&ctx, &a, &r, sn_atanh, atanh, x, 1e-11, 1e-13, "atanh");
    }

    for (i = 0; i < N; i++) {
        x = clamp_trig(xs_signed_unit() * 12.0);
        check_unary_f64(&ctx, &a, &r, sn_sin, sin, x, 1e-12, 1e-14, "sin");
        check_unary_f64(&ctx, &a, &r, sn_cos, cos, x, 1e-12, 1e-14, "cos");
        y = safe_tan_x(xs_signed_unit());
        check_unary_f64(&ctx, &a, &r, sn_tan, tan, y, 1e-10, 1e-12, "tan");
    }

    for (i = 0; i < N; i++) {
        x = xs_unit() * 8.0 + 0.1;
        y = xs_signed_unit() * 4.0;
        /* pow: avoid negative base with non-integer y for portability */
        check_binary_f64(&ctx, &a, &b, &r, sn_pow, pow, x, y, 1e-10, 1e-12, "pow");
        check_binary_f64(&ctx, &a, &b, &r, sn_hypot, hypot, x, y, 1e-12, 1e-14, "hypot");
        check_binary_f64(&ctx, &a, &b, &r, sn_fdim, fdim, x, y, 1e-15, 0.0, "fdim");
        check_binary_f64(&ctx, &a, &b, &r, sn_atan2, atan2, y, x, 1e-12, 1e-14, "atan2");
        y = xs_unit() * 5.0 + 0.25;
        check_binary_f64(&ctx, &a, &b, &r, sn_fmod, fmod, x * 3.0, y, 1e-12, 1e-14, "fmod");
    }

    /* frexp / ldexp / ilogb / nextafter / remquo on f64 */
    for (i = 0; i < N; i++) {
        int he;
        double hm, hx;
        x = (xs_signed_unit() * 100.0);
        if (x == 0.0) x = 1.0;
        CHECK(load_f64(&ctx, &a, x));
        CHECK(sn_frexp(&ctx, &r, &e, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &r, &got) == SN_OK);
        hm = frexp(x, &he);
        if (!nearly_eq(got, hm, 1e-15, 0.0) || e != he) {
            report_mismatch("frexp", x, got, hm);
            CHECK(0);
        } else {
            CHECK(1);
        }
        CHECK(sn_ldexp(&ctx, &s, &r, e, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &s, &g2) == SN_OK);
        CHECK(nearly_eq(g2, x, 1e-15, 0.0));

        CHECK(sn_ilogb(&ctx, &a, &e2) == SN_OK);
        CHECK(e2 == ilogb(x));

        y = x + xs_signed_unit();
        CHECK(load_f64(&ctx, &b, y));
        CHECK(sn_nextafter(&ctx, &r, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &r, &got) == SN_OK);
        expect = nextafter(x, y);
        if (!nearly_eq(got, expect, 0.0, 0.0)) {
            report_mismatch("nextafter", x, got, expect);
            CHECK(0);
        } else {
            CHECK(1);
        }

        /* remquo: compare remainder; quotient low bits (common 3-bit check) */
        hx = xs_unit() * 20.0 + 0.5;
        y = xs_unit() * 3.0 + 0.5;
        CHECK(load_f64(&ctx, &a, hx));
        CHECK(load_f64(&ctx, &b, y));
        q = 0;
        CHECK(sn_remquo(&ctx, &r, &q, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &r, &got) == SN_OK);
        qh = 0;
        expect = remquo(hx, y, &qh);
        if (!nearly_eq(got, expect, 1e-12, 1e-14)) {
            report_mismatch("remquo-rem", hx, got, expect);
            CHECK(0);
        } else {
            CHECK(1);
        }
        /* C remquo returns at least 3 bits of quotient; match low 3 bits with sign */
        CHECK((q & 7) == (qh & 7));
    }

    /* ========== multiprec (e=15,m=80) vs host + identities ========== */
    for (i = 0; i < N; i++) {
        x = xs_signed_unit() * 4.0;
        check_unary_mp(&ctx, &a, &tmp, &r, sn_exp, exp, x, 1e-10, 1e-12, "mp-exp");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_expm1, expm1, x * 0.2, 1e-9, 1e-11, "mp-expm1");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_sinh, sinh, x * 0.5, 1e-9, 1e-11, "mp-sinh");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_cosh, cosh, x * 0.5, 1e-9, 1e-11, "mp-cosh");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_tanh, tanh, x, 1e-9, 1e-11, "mp-tanh");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_cbrt, cbrt, x, 1e-9, 1e-11, "mp-cbrt");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_floor, floor, x * 5.0, 1e-12, 1e-12, "mp-floor");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_ceil, ceil, x * 5.0, 1e-12, 1e-12, "mp-ceil");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_trunc, trunc, x * 5.0, 1e-12, 1e-12, "mp-trunc");
    }

    for (i = 0; i < N; i++) {
        x = xs_unit() * 20.0 + 1e-6;
        check_unary_mp(&ctx, &a, &tmp, &r, sn_log, log, x, 1e-10, 1e-12, "mp-log");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_log2, log2, x, 1e-9, 1e-11, "mp-log2");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_log10, log10, x, 1e-9, 1e-11, "mp-log10");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_sqrt, sqrt, x, 1e-12, 1e-14, "mp-sqrt");
    }

    for (i = 0; i < N; i++) {
        x = clamp_trig(xs_signed_unit() * 6.0);
        check_unary_mp(&ctx, &a, &tmp, &r, sn_sin, sin, x, 1e-9, 1e-11, "mp-sin");
        check_unary_mp(&ctx, &a, &tmp, &r, sn_cos, cos, x, 1e-9, 1e-11, "mp-cos");
        y = safe_tan_x(xs_signed_unit());
        check_unary_mp(&ctx, &a, &tmp, &r, sn_tan, tan, y, 1e-8, 1e-10, "mp-tan");
    }

    for (i = 0; i < N; i++) {
        x = xs_unit() * 5.0 + 0.25;
        y = xs_unit() * 2.0 + 0.25;
        check_binary_mp(&ctx, &a, &b, &tmp, &r, sn_hypot, hypot, x, y, 1e-9, 1e-11, "mp-hypot");
        check_binary_mp(&ctx, &a, &b, &tmp, &r, sn_fmod, fmod, x * 3.0, y, 1e-9, 1e-11, "mp-fmod");
        check_binary_mp(&ctx, &a, &b, &tmp, &r, sn_fdim, fdim, x, y, 1e-12, 1e-14, "mp-fdim");
        check_binary_mp(&ctx, &a, &b, &tmp, &r, sn_pow, pow, x, y * 0.5, 1e-8, 1e-10, "mp-pow");
    }

    /* identities: exp(log(x))~x, sin^2+cos^2~1, frexp/ldexp roundtrip, fmod */
    for (i = 0; i < N; i++) {
        x = xs_unit() * 10.0 + 0.01;
        CHECK(load_mp(&ctx, &a, &tmp, x));
        CHECK(sn_log(&ctx, &r, &a, NULL) == SN_OK);
        CHECK(sn_exp(&ctx, &s, &r, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &s, &got) == SN_OK);
        if (!nearly_eq(got, x, 1e-9, 1e-11)) {
            report_mismatch("mp-exp-log", x, got, x);
            CHECK(0);
        } else {
            CHECK(1);
        }

        y = clamp_trig(xs_signed_unit() * 5.0);
        CHECK(load_mp(&ctx, &a, &tmp, y));
        CHECK(sn_sin(&ctx, &r, &a, NULL) == SN_OK);
        CHECK(sn_cos(&ctx, &s, &a, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &t, &r, &r, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &tmp, &s, &s, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &b, &t, &tmp, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &b, &got) == SN_OK);
        if (!nearly_eq(got, 1.0, 1e-9, 1e-11)) {
            report_mismatch("mp-sin2cos2", y, got, 1.0);
            CHECK(0);
        } else {
            CHECK(1);
        }

        /* frexp / ilogb */
        CHECK(load_mp(&ctx, &a, &tmp, x));
        CHECK(sn_frexp(&ctx, &r, &e, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &r, &got) == SN_OK);
        CHECK(got >= 0.5 && got < 1.0 + 1e-12);
        CHECK(sn_ldexp(&ctx, &s, &r, e, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &s, &g2) == SN_OK);
        if (!nearly_eq(g2, x, 1e-12, 1e-14)) {
            report_mismatch("mp-frexp-ldexp", x, g2, x);
            CHECK(0);
        } else {
            CHECK(1);
        }
        CHECK(sn_ilogb(&ctx, &a, &e2) == SN_OK);
        CHECK(e2 == ilogb(x) || e2 == e - 1); /* allow unit in last place on boundary */

        y = xs_unit() * 3.0 + 0.5;
        CHECK(load_mp(&ctx, &a, &tmp, x * 2.7));
        CHECK(load_mp(&ctx, &b, &tmp, y));
        CHECK(sn_fmod(&ctx, &r, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &r, &got) == SN_OK);
        expect = fmod(x * 2.7, y);
        if (!nearly_eq(got, expect, 1e-9, 1e-11)) {
            report_mismatch("mp-fmod-id", x * 2.7, got, expect);
            CHECK(0);
        } else {
            CHECK(1);
        }
    }

    sn_value_clear(&ctx, &a);
    sn_value_clear(&ctx, &b);
    sn_value_clear(&ctx, &r);
  
    /* extended: bessel j0 vs host + I/K identity */
    for (i = 0; i < N; i++) {
        x = xs_unit() * 8.0 + 0.05; /* positive modest for Y/J/I/K */
        check_unary_f64(&ctx, &a, &r, sn_j0, j0, x, 1e-11, 1e-13, "j0");
        check_unary_f64(&ctx, &a, &r, sn_j1, j1, x, 1e-11, 1e-13, "j1");
        if (x > 0.05) {
            check_unary_f64(&ctx, &a, &r, sn_y0, y0, x, 1e-10, 1e-12, "y0");
        }
        /* I/K: I0 even + Wronskian I0*K1+I1*K0=1/x */
        {
            sn_value i0v, i1v, k0v, k1v, t, s;
            double wgot, wx;
            wx = 0.2 + xs_unit() * 4.8;
            sn_value_init(&i0v); sn_value_init(&i1v); sn_value_init(&k0v);
            sn_value_init(&k1v); sn_value_init(&t); sn_value_init(&s);
            CHECK(load_f64(&ctx, &a, wx));
            CHECK(sn_i0(&ctx, &i0v, &a, NULL) == SN_OK);
            CHECK(sn_i1(&ctx, &i1v, &a, NULL) == SN_OK);
            CHECK(sn_k0(&ctx, &k0v, &a, NULL) == SN_OK);
            CHECK(sn_k1(&ctx, &k1v, &a, NULL) == SN_OK);
            CHECK(sn_mul(&ctx, &t, &i0v, &k1v, NULL) == SN_OK);
            CHECK(sn_mul(&ctx, &s, &i1v, &k0v, NULL) == SN_OK);
            CHECK(sn_add(&ctx, &t, &t, &s, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &t, &wgot) == SN_OK);
            if (!nearly_eq(wgot, 1.0 / wx, 1e-9, 1e-12)) {
                report_mismatch("IK-wronskian", wx, wgot, 1.0 / wx);
                CHECK(0);
            } else {
                CHECK(1);
            }
            /* I0 even */
            CHECK(load_f64(&ctx, &a, -wx));
            CHECK(sn_i0(&ctx, &r, &a, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &r, &got) == SN_OK);
            CHECK(sn_to_double(&ctx, &i0v, &expect) == SN_OK);
            CHECK(nearly_eq(got, expect, 1e-12, 1e-14));
            sn_value_clear(&ctx, &i0v); sn_value_clear(&ctx, &i1v);
            sn_value_clear(&ctx, &k0v); sn_value_clear(&ctx, &k1v);
            sn_value_clear(&ctx, &t); sn_value_clear(&ctx, &s);
        }
    }


    /* extended: digamma/trigamma recurrence + elliptic/ibeta identities */
    for (i = 0; i < N; i++) {
        double xd;
        sn_value p, q, inv;
        sn_value_init(&p); sn_value_init(&q); sn_value_init(&inv);
        xd = 0.6 + xs_unit() * 4.0; /* avoid poles */
        CHECK(load_f64(&ctx, &a, xd));
        CHECK(load_f64(&ctx, &b, xd + 1.0));
        CHECK(sn_digamma(&ctx, &p, &a, NULL) == SN_OK);
        CHECK(sn_digamma(&ctx, &q, &b, NULL) == SN_OK);
        CHECK(sn_f64(&ctx, &inv, 1.0 / xd) == SN_OK);
        CHECK(sn_add(&ctx, &p, &p, &inv, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &p, &got) == SN_OK);
        CHECK(sn_to_double(&ctx, &q, &expect) == SN_OK);
        if (!nearly_eq(got, expect, 1e-10, 1e-12)) {
            report_mismatch("psi-rec", xd, got, expect);
            CHECK(0);
        } else {
            CHECK(1);
        }
        /* trigamma: psi1(x)=psi1(x+1)+1/x^2 */
        CHECK(sn_trigamma(&ctx, &p, &a, NULL) == SN_OK);
        CHECK(sn_trigamma(&ctx, &q, &b, NULL) == SN_OK);
        CHECK(sn_f64(&ctx, &inv, 1.0 / (xd * xd)) == SN_OK);
        CHECK(sn_add(&ctx, &q, &q, &inv, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &p, &got) == SN_OK);
        CHECK(sn_to_double(&ctx, &q, &expect) == SN_OK);
        if (!nearly_eq(got, expect, 1e-10, 1e-12)) {
            report_mismatch("psi1-rec", xd, got, expect);
            CHECK(0);
        } else {
            CHECK(1);
        }
        sn_value_clear(&ctx, &p); sn_value_clear(&ctx, &q);
        sn_value_clear(&ctx, &inv);
    }
    /* digamma finite difference ~ trigamma */
    for (i = 0; i < N; i++) {
        double h = 1e-5, xd, fd;
        sn_value xh, xl, ph, pl, diff, hv, tg;
        sn_value_init(&xh); sn_value_init(&xl); sn_value_init(&ph);
        sn_value_init(&pl); sn_value_init(&diff); sn_value_init(&hv); sn_value_init(&tg);
        xd = 1.2 + xs_unit() * 3.0;
        CHECK(load_f64(&ctx, &xh, xd + h));
        CHECK(load_f64(&ctx, &xl, xd - h));
        CHECK(sn_digamma(&ctx, &ph, &xh, NULL) == SN_OK);
        CHECK(sn_digamma(&ctx, &pl, &xl, NULL) == SN_OK);
        CHECK(sn_sub(&ctx, &diff, &ph, &pl, NULL) == SN_OK);
        CHECK(sn_f64(&ctx, &hv, 2.0 * h) == SN_OK);
        CHECK(sn_div(&ctx, &diff, &diff, &hv, NULL) == SN_OK);
        CHECK(load_f64(&ctx, &a, xd));
        CHECK(sn_trigamma(&ctx, &tg, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &diff, &fd) == SN_OK);
        CHECK(sn_to_double(&ctx, &tg, &got) == SN_OK);
        if (!nearly_eq(fd, got, 1e-4, 1e-5)) {
            report_mismatch("psi-fd-psi1", xd, fd, got);
            CHECK(0);
        } else {
            CHECK(1);
        }
        sn_value_clear(&ctx, &xh); sn_value_clear(&ctx, &xl);
        sn_value_clear(&ctx, &ph); sn_value_clear(&ctx, &pl);
        sn_value_clear(&ctx, &diff); sn_value_clear(&ctx, &hv); sn_value_clear(&ctx, &tg);
    }
    /* elliptic identities */
    {
        const double pi_2 = 1.5707963267948966;
        sn_value phi, m, n, y, k, f;
        sn_value_init(&phi); sn_value_init(&m); sn_value_init(&n);
        sn_value_init(&y); sn_value_init(&k); sn_value_init(&f);
        for (i = 0; i < N; i++) {
            double ph = (0.05 + xs_unit() * 1.4);
            double mm = xs_unit() * 0.85;
            CHECK(load_f64(&ctx, &phi, ph));
            CHECK(load_f64(&ctx, &m, 0.0));
            CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &got) == SN_OK);
            if (!nearly_eq(got, ph, 1e-12, 1e-14)) {
                report_mismatch("ellipf-m0", ph, got, ph);
                CHECK(0);
            } else {
                CHECK(1);
            }
            CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &got) == SN_OK);
            if (!nearly_eq(got, ph, 1e-12, 1e-14)) {
                report_mismatch("ellipe-m0", ph, got, ph);
                CHECK(0);
            } else {
                CHECK(1);
            }
            CHECK(load_f64(&ctx, &m, mm));
            CHECK(load_f64(&ctx, &n, 0.0));
            CHECK(sn_ellipf(&ctx, &f, &phi, &m, NULL) == SN_OK);
            CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &f, &expect) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &got) == SN_OK);
            if (!nearly_eq(got, expect, 1e-10, 1e-12)) {
                report_mismatch("ellipi-n0", ph, got, expect);
                CHECK(0);
            } else {
                CHECK(1);
            }
            CHECK(load_f64(&ctx, &phi, pi_2));
            CHECK(sn_ellipf(&ctx, &f, &phi, &m, NULL) == SN_OK);
            CHECK(sn_ellipk(&ctx, &k, &m, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &f, &got) == SN_OK);
            CHECK(sn_to_double(&ctx, &k, &expect) == SN_OK);
            if (!nearly_eq(got, expect, 1e-10, 1e-12)) {
                report_mismatch("ellipf-K", mm, got, expect);
                CHECK(0);
            } else {
                CHECK(1);
            }
            CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
            CHECK(sn_ellipe(&ctx, &k, &m, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &got) == SN_OK);
            CHECK(sn_to_double(&ctx, &k, &expect) == SN_OK);
            if (!nearly_eq(got, expect, 1e-10, 1e-12)) {
                report_mismatch("ellipe-E", mm, got, expect);
                CHECK(0);
            } else {
                CHECK(1);
            }
        }
        sn_value_clear(&ctx, &phi); sn_value_clear(&ctx, &m); sn_value_clear(&ctx, &n);
        sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &k); sn_value_clear(&ctx, &f);
    }
    /* incomplete beta identities */
    for (i = 0; i < N; i++) {
        double aa, bb, xx;
        sn_value av, bv, xv, yv, s1, s2, sum;
        sn_value_init(&av); sn_value_init(&bv); sn_value_init(&xv);
        sn_value_init(&yv); sn_value_init(&s1); sn_value_init(&s2);
        sn_value_init(&sum);
        aa = 0.5 + xs_unit() * 3.5;
        bb = 0.5 + xs_unit() * 3.5;
        xx = 0.05 + xs_unit() * 0.9;
        CHECK(load_f64(&ctx, &av, aa));
        CHECK(load_f64(&ctx, &bv, bb));
        CHECK(load_f64(&ctx, &xv, xx));
        CHECK(load_f64(&ctx, &yv, 1.0 - xx));
        CHECK(sn_ibeta(&ctx, &s1, &av, &bv, &xv, NULL) == SN_OK);
        CHECK(sn_ibeta(&ctx, &s2, &bv, &av, &yv, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &sum, &s1, &s2, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &sum, &got) == SN_OK);
        if (!nearly_eq(got, 1.0, 1e-9, 1e-11)) {
            report_mismatch("ibeta-sym", xx, got, 1.0);
            CHECK(0);
        } else {
            CHECK(1);
        }
        CHECK(sn_ibetac(&ctx, &s2, &av, &bv, &xv, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &sum, &s1, &s2, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &sum, &got) == SN_OK);
        if (!nearly_eq(got, 1.0, 1e-9, 1e-11)) {
            report_mismatch("ibeta-c", xx, got, 1.0);
            CHECK(0);
        } else {
            CHECK(1);
        }
        sn_value_clear(&ctx, &av); sn_value_clear(&ctx, &bv); sn_value_clear(&ctx, &xv);
        sn_value_clear(&ctx, &yv); sn_value_clear(&ctx, &s1); sn_value_clear(&ctx, &s2);
        sn_value_clear(&ctx, &sum);
    }


  sn_value_clear(&ctx, &s);
    sn_value_clear(&ctx, &t);
    sn_value_clear(&ctx, &tmp);
    sn_ctx_fini(&ctx);
    return 0;
}
