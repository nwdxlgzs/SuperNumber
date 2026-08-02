#include "sn.h"
#include "sn_flat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

double j0(double);
double j1(double);
double jn(int, double);
double y0(double);
double y1(double);
double yn(int, double);

/* internal helper also used by library float path */
sn_status sn_float_from_double(sn_ctx *ctx, sn_value *out, double x,
                               int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

static int nearly_eq(double a, double b, double rel)
{
    double d, aa, ab;
    if (a != a && b != b) return 1;
    if (a != a || b != b) return 0;
    if (a == b) return 1;
    d = a - b;
    if (d < 0) d = -d;
    aa = a < 0 ? -a : a;
    ab = b < 0 ? -b : b;
    if (aa < ab) aa = ab;
    /* absolute floor: compare-to-zero and subnormals */
    if (aa < 1e-12) return d < 1e-12;
    return d <= rel * aa;
}

int test_complex_run(void)
{
    sn_ctx ctx;
    sn_cplx a, b, c;
    sn_value r;
    sn_api api;
    double d, re, im;

    sn_ctx_init(&ctx);
    sn_cplx_init(&a);
    sn_cplx_init(&b);
    sn_cplx_init(&c);
    sn_value_init(&r);
    sn_api_bind(&api);

    /* 3+4i abs = 5 */
    CHECK(sn_cplx_set_d(&ctx, &a, 3.0, 4.0, 11, 52, 1, NULL) == SN_OK);
    CHECK(sn_cplx_abs(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 5.0, 1e-12));

    /* conj */
    CHECK(sn_cplx_conj(&ctx, &b, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &b.re, &re) == SN_OK && nearly_eq(re, 3.0, 1e-15));
    CHECK(sn_to_double(&ctx, &b.im, &im) == SN_OK && nearly_eq(im, -4.0, 1e-15));

    /* mul: (1+2i)*(3+4i) = -5+10i */
    CHECK(sn_cplx_set_d(&ctx, &a, 1.0, 2.0, 11, 52, 1, NULL) == SN_OK);
    CHECK(sn_cplx_set_d(&ctx, &b, 3.0, 4.0, 11, 52, 1, NULL) == SN_OK);
    CHECK(sn_cplx_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c.re, &re) == SN_OK && nearly_eq(re, -5.0, 1e-12));
    CHECK(sn_to_double(&ctx, &c.im, &im) == SN_OK && nearly_eq(im, 10.0, 1e-12));

    /* div: inverse of mul */
    CHECK(sn_cplx_div(&ctx, &c, &c, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c.re, &re) == SN_OK && nearly_eq(re, 1.0, 1e-12));
    CHECK(sn_to_double(&ctx, &c.im, &im) == SN_OK && nearly_eq(im, 2.0, 1e-12));

    /* add / sub / neg */
    CHECK(sn_cplx_add(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c.re, &re) == SN_OK && nearly_eq(re, 4.0, 1e-15));
    CHECK(sn_to_double(&ctx, &c.im, &im) == SN_OK && nearly_eq(im, 6.0, 1e-15));
    CHECK(sn_cplx_sub(&ctx, &c, &b, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c.re, &re) == SN_OK && nearly_eq(re, 2.0, 1e-15));
    CHECK(sn_cplx_neg(&ctx, &c, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c.re, &re) == SN_OK && nearly_eq(re, -1.0, 1e-15));
    CHECK(sn_to_double(&ctx, &c.im, &im) == SN_OK && nearly_eq(im, -2.0, 1e-15));

    /* from_polar: rho=2, theta=pi/2 -> ~ 0+2i */
    {
        sn_value rho, th;
        sn_value_init(&rho);
        sn_value_init(&th);
        CHECK(sn_f64(&ctx, &rho, 2.0) == SN_OK);
        CHECK(sn_f64(&ctx, &th, 1.5707963267948966) == SN_OK);
        CHECK(sn_cplx_from_polar(&ctx, &c, &rho, &th, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c.re, &re) == SN_OK && re > -1e-12 && re < 1e-12);
        CHECK(sn_to_double(&ctx, &c.im, &im) == SN_OK && nearly_eq(im, 2.0, 1e-12));
        sn_value_clear(&ctx, &rho);
        sn_value_clear(&ctx, &th);
    }

    /* multiprec complex mul */
    CHECK(sn_cplx_set_d(&ctx, &a, 2.0, 3.0, 15, 80, 1, NULL) == SN_OK);
    CHECK(sn_cplx_set_d(&ctx, &b, 4.0, 5.0, 15, 80, 1, NULL) == SN_OK);
    CHECK(sn_cplx_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
    /* (2+3i)(4+5i)=8+10i+12i+15i^2=-7+22i */
    CHECK(sn_to_double(&ctx, &c.re, &re) == SN_OK && nearly_eq(re, -7.0, 1e-12));
    CHECK(sn_to_double(&ctx, &c.im, &im) == SN_OK && nearly_eq(im, 22.0, 1e-12));

    /* multiprec soft sqrt / exp / log / sin / cos (e=15,m=80) */
    {
        sn_value x, y, z, base;
        sn_value_init(&x);
        sn_value_init(&y);
        sn_value_init(&z);
        sn_value_init(&base);
        CHECK(sn_float_from_i64(&ctx, &x, 4, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_sqrt(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 2.0, 1e-12));
        CHECK(sn_float_from_i64(&ctx, &x, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_sqrt(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, sqrt(2.0), 1e-12));
        CHECK(sn_mul(&ctx, &r, &y, &y, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 2.0, 1e-10));

        /* 5/4 = 1.25 via integer ops then cast-ish from_i64 path for 5/4 */
        CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 4, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &x, &x, &base, NULL) == SN_OK);
        CHECK(sn_exp(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, exp(1.25), 1e-10));
        CHECK(sn_log(&ctx, &z, &y, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &z, &d) == SN_OK && nearly_eq(d, 1.25, 1e-9));

        CHECK(sn_float_from_i64(&ctx, &x, 1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &x, &x, &base, NULL) == SN_OK); /* 0.5 */
        CHECK(sn_sin(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, sin(0.5), 1e-10));
        CHECK(sn_cos(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, cos(0.5), 1e-10));

        /* soft tan / asin / pow / hypot (multiprec) */
        CHECK(sn_tan(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, tan(0.5), 1e-9));
        CHECK(sn_asin(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, asin(0.5), 1e-9));

        CHECK(sn_float_from_i64(&ctx, &x, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_pow(&ctx, &y, &x, &base, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 8.0, 1e-10));

        CHECK(sn_float_from_i64(&ctx, &x, 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 4, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_hypot(&ctx, &y, &x, &base, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 5.0, 1e-12));

        /* soft acos / atan / atan2 / hyperbolics (multiprec) */
        CHECK(sn_float_from_i64(&ctx, &x, 1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &x, &x, &base, NULL) == SN_OK); /* 0.5 */
        CHECK(sn_acos(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, acos(0.5), 1e-9));
        CHECK(sn_atan(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, atan(0.5), 1e-9));
        CHECK(sn_float_from_i64(&ctx, &base, 1, 15, 80, 1, NULL) == SN_OK); /* y=0.5, x=1 */
        CHECK(sn_atan2(&ctx, &y, &x, &base, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, atan2(0.5, 1.0), 1e-9));

        CHECK(sn_float_from_i64(&ctx, &x, 1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &x, &x, &base, NULL) == SN_OK); /* 0.5 */
        CHECK(sn_sinh(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, sinh(0.5), 1e-9));
        CHECK(sn_cosh(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, cosh(0.5), 1e-9));
        CHECK(sn_tanh(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, tanh(0.5), 1e-9));
        CHECK(sn_asinh(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, asinh(0.5), 1e-9));
        CHECK(sn_atanh(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, atanh(0.5), 1e-9));

        /* acosh(2) */
        CHECK(sn_float_from_i64(&ctx, &x, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_acosh(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, acosh(2.0), 1e-9));

        /* bootstrapped constants via series/AGM: exp(ln2)~2, sin(pi)~0, cos(pi)~-1 */
        {
            sn_value two, r2;
            sn_value_init(&two);
            sn_value_init(&r2);
            CHECK(sn_float_from_i64(&ctx, &two, 2, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_log(&ctx, &y, &two, NULL) == SN_OK); /* soft ln2 bootstrap used in log */
            CHECK(sn_exp(&ctx, &z, &y, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &z, &d) == SN_OK && nearly_eq(d, 2.0, 1e-12));
            /* log(2) should match host ln2 */
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, log(2.0), 1e-12));
            /* pi via AGM: acos(-1) or atan2(0,-1) */
            CHECK(sn_float_from_i64(&ctx, &x, -1, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_acos(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, acos(-1.0), 1e-12));
            CHECK(sn_sin(&ctx, &z, &y, NULL) == SN_OK);
            /* residual ~ ulp after AGM pi / soft reduce; absolute near zero */
            CHECK(sn_to_double(&ctx, &z, &d) == SN_OK && nearly_eq(d, 0.0, 1e-10));
            CHECK(sn_cos(&ctx, &z, &y, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &z, &d) == SN_OK && nearly_eq(d, -1.0, 1e-12));
            sn_value_clear(&ctx, &two);
            sn_value_clear(&ctx, &r2);
        }

        /* multiprec soft FMA: 2*3+4 = 10 */
        CHECK(sn_float_from_i64(&ctx, &x, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &z, 4, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_fma(&ctx, &y, &x, &base, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 10.0, 1e-12));

        /* multiprec soft frem: 7 rem 3 = 1 */
        CHECK(sn_float_from_i64(&ctx, &x, 7, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_frem(&ctx, &y, &x, &base, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12));

        /* frem (IEEE remainder style, nearest n): 5.5 rem 2 -> -0.5 */
        CHECK(sn_float_from_i64(&ctx, &x, 11, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &x, &x, &base, NULL) == SN_OK); /* 5.5 */
        CHECK(sn_float_from_i64(&ctx, &base, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_frem(&ctx, &y, &x, &base, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, -0.5, 1e-12));

        /* fused FMA: (1+2^{-50})^2 - (1+2^{-49}) = 2^{-100}
         * mul-then-add rounds product to m=80 and cancels to 0; fused keeps cross term. */
        {
            sn_value one, p50, p49, eps, a, c, rmul, radd;
            sn_value_init(&one); sn_value_init(&p50); sn_value_init(&p49);
            sn_value_init(&eps); sn_value_init(&a); sn_value_init(&c);
            sn_value_init(&rmul); sn_value_init(&radd);
            CHECK(sn_float_from_i64(&ctx, &one, 1, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_float_from_i64(&ctx, &p50, 1LL << 50, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_float_from_i64(&ctx, &p49, 1LL << 49, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_div(&ctx, &eps, &one, &p50, NULL) == SN_OK); /* 2^{-50} */
            CHECK(sn_add(&ctx, &a, &one, &eps, NULL) == SN_OK);    /* 1+2^{-50} */
            CHECK(sn_div(&ctx, &eps, &one, &p49, NULL) == SN_OK); /* 2^{-49} */
            CHECK(sn_add(&ctx, &c, &one, &eps, NULL) == SN_OK);
            CHECK(sn_neg(&ctx, &c, &c, NULL) == SN_OK);            /* -(1+2^{-49}) */
            CHECK(sn_fma(&ctx, &y, &a, &a, &c, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, ldexp(1.0, -100), 1e-6));
            CHECK(sn_fp_classify(&y) != SN_FP_ZERO);
            CHECK(sn_mul(&ctx, &rmul, &a, &a, NULL) == SN_OK);
            CHECK(sn_add(&ctx, &radd, &rmul, &c, NULL) == SN_OK);
            /* separate mul+add loses 2^{-100} and becomes 0 */
            CHECK(sn_fp_classify(&radd) == SN_FP_ZERO);
            sn_value_clear(&ctx, &one); sn_value_clear(&ctx, &p50); sn_value_clear(&ctx, &p49);
            sn_value_clear(&ctx, &eps); sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &c);
            sn_value_clear(&ctx, &rmul); sn_value_clear(&ctx, &radd);
        }

        /* large-quotient pure soft frem: (2^60+1) rem 2.
         * q = (2^60+1)/2 = 2^59 + 0.5; IEEE ties-to-even keeps even 2^59 => rem = +1.
         * (|q| >> 2^53; pure soft path, no host double). */
        CHECK(sn_float_from_i64(&ctx, &x, (1LL << 60) + 1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_frem(&ctx, &y, &x, &base, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12));

        /* another large rem: (2^55+7) rem 5 -> 0  (2^55≡3 mod 5, +7≡2 => 0; pure soft) */
        CHECK(sn_float_from_i64(&ctx, &x, (1LL << 55) + 7, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_frem(&ctx, &y, &x, &base, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.0, 1e-12));

        /* large non-zero rem: (2^60+3) rem 7  (no host double for expected) */
        CHECK(sn_float_from_i64(&ctx, &x, (1LL << 60) + 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &base, 7, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_frem(&ctx, &y, &x, &base, NULL) == SN_OK);
        {
            /* 2^60 mod 7: 2^3=8≡1, so 2^60=(2^3)^20≡1; +3 => 4.  |r|<=3.5 so r=4-7=-3 (nearest n). */
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, -3.0, 1e-12));
        }

        sn_value_clear(&ctx, &x);
        sn_value_clear(&ctx, &y);
        sn_value_clear(&ctx, &z);
        sn_value_clear(&ctx, &base);
    }

    /* api table complex */
    CHECK(api.cplx.abs == sn_cplx_abs);
    CHECK(api.cplx.mul == sn_cplx_mul);
    CHECK(api.cplx.set_d(&ctx, &a, 0.0, 1.0, 11, 52, 1, NULL) == SN_OK);
    CHECK(api.cplx.abs(&ctx, &r, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &r, &d) == SN_OK && nearly_eq(d, 1.0, 1e-15));


    /* ---- complex transcendentals (f64) ---- */
    {
        sn_cplx z, w, u;
        sn_value argv;
        sn_cplx_init(&z); sn_cplx_init(&w); sn_cplx_init(&u);
        sn_value_init(&argv);

        CHECK(sn_cplx_set_d(&ctx, &z, 1.0, 1.0, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_arg(&ctx, &argv, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &argv, &d) == SN_OK && nearly_eq(d, 0.7853981633974483, 1e-12));

        CHECK(sn_cplx_set_d(&ctx, &z, 0.0, 1.5707963267948966, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_exp(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &w.re, &re) == SN_OK && nearly_eq(re, 0.0, 1e-12));
        CHECK(sn_to_double(&ctx, &w.im, &im) == SN_OK && nearly_eq(im, 1.0, 1e-12));

        CHECK(sn_cplx_set_d(&ctx, &z, 0.5, 0.25, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_exp(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_cplx_log(&ctx, &u, &w, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &u.re, &re) == SN_OK && nearly_eq(re, 0.5, 1e-10));
        CHECK(sn_to_double(&ctx, &u.im, &im) == SN_OK && nearly_eq(im, 0.25, 1e-10));

        CHECK(sn_cplx_set_d(&ctx, &z, -1.0, 0.0, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_sqrt(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &w.re, &re) == SN_OK && nearly_eq(re, 0.0, 1e-12));
        CHECK(sn_to_double(&ctx, &w.im, &im) == SN_OK && nearly_eq(im, 1.0, 1e-12));

        CHECK(sn_cplx_set_d(&ctx, &z, 0.0, 1.0, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_set_d(&ctx, &w, 2.0, 0.0, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_pow(&ctx, &u, &z, &w, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &u.re, &re) == SN_OK && nearly_eq(re, -1.0, 1e-10));
        CHECK(sn_to_double(&ctx, &u.im, &im) == SN_OK && nearly_eq(im, 0.0, 1e-10));

        CHECK(sn_cplx_set_d(&ctx, &z, 0.3, 0.4, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_sin(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_cplx_cos(&ctx, &u, &z, NULL) == SN_OK);
        {
            sn_cplx s2, c2, one;
            sn_cplx_init(&s2); sn_cplx_init(&c2); sn_cplx_init(&one);
            CHECK(sn_cplx_mul(&ctx, &s2, &w, &w, NULL) == SN_OK);
            CHECK(sn_cplx_mul(&ctx, &c2, &u, &u, NULL) == SN_OK);
            CHECK(sn_cplx_add(&ctx, &one, &s2, &c2, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &one.re, &re) == SN_OK && nearly_eq(re, 1.0, 1e-10));
            CHECK(sn_to_double(&ctx, &one.im, &im) == SN_OK && nearly_eq(im, 0.0, 1e-10));
            sn_cplx_clear(&ctx, &s2); sn_cplx_clear(&ctx, &c2); sn_cplx_clear(&ctx, &one);
        }

        CHECK(sn_cplx_sinh(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_cplx_cosh(&ctx, &u, &z, NULL) == SN_OK);
        {
            sn_cplx sh2, ch2, diff;
            sn_cplx_init(&sh2); sn_cplx_init(&ch2); sn_cplx_init(&diff);
            CHECK(sn_cplx_mul(&ctx, &sh2, &w, &w, NULL) == SN_OK);
            CHECK(sn_cplx_mul(&ctx, &ch2, &u, &u, NULL) == SN_OK);
            CHECK(sn_cplx_sub(&ctx, &diff, &ch2, &sh2, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &diff.re, &re) == SN_OK && nearly_eq(re, 1.0, 1e-10));
            CHECK(sn_to_double(&ctx, &diff.im, &im) == SN_OK && nearly_eq(im, 0.0, 1e-10));
            sn_cplx_clear(&ctx, &sh2); sn_cplx_clear(&ctx, &ch2); sn_cplx_clear(&ctx, &diff);
        }

        CHECK(sn_cplx_set_d(&ctx, &z, 0.2, 0.1, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_sin(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_cplx_asin(&ctx, &u, &w, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &u.re, &re) == SN_OK && nearly_eq(re, 0.2, 1e-8));
        CHECK(sn_to_double(&ctx, &u.im, &im) == SN_OK && nearly_eq(im, 0.1, 1e-8));

        CHECK(sn_cplx_set_d(&ctx, &z, 0.75, -0.5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_cplx_exp(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_cplx_log(&ctx, &u, &w, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &u.re, &re) == SN_OK && nearly_eq(re, 0.75, 1e-10));
        CHECK(sn_to_double(&ctx, &u.im, &im) == SN_OK && nearly_eq(im, -0.5, 1e-10));

        CHECK(api.cplx.exp == sn_cplx_exp);
        CHECK(api.cplx.log == sn_cplx_log);
        CHECK(api.cplx.arg == sn_cplx_arg);
        CHECK(api.math.j0 == sn_j0);

        sn_cplx_clear(&ctx, &z); sn_cplx_clear(&ctx, &w); sn_cplx_clear(&ctx, &u);
        sn_value_clear(&ctx, &argv);
    }

    /* ---- Bessel j0/j1/jn y0/y1/yn vs host ---- */
    {
        sn_value x, y;
        double hx;
        sn_value_init(&x); sn_value_init(&y);
        hx = 1.5;
        CHECK(sn_f64(&ctx, &x, hx) == SN_OK);
        CHECK(sn_j0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, j0(hx), 1e-12));
        CHECK(sn_j1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, j1(hx), 1e-12));
        CHECK(sn_jn(&ctx, &y, 3, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, jn(3, hx), 1e-12));
        CHECK(sn_y0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, y0(hx), 1e-12));
        CHECK(sn_y1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, y1(hx), 1e-12));
        CHECK(sn_yn(&ctx, &y, 2, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, yn(2, hx), 1e-12));

        CHECK(sn_from_str_float(&ctx, &x, "1.0", 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_j0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, j0(1.0), 1e-10));
        CHECK(sn_j1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, j1(1.0), 1e-10));
        CHECK(sn_jn(&ctx, &y, 2, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, jn(2, 1.0), 1e-9));
        CHECK(sn_y0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, y0(1.0), 1e-8));
        CHECK(sn_y1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, y1(1.0), 1e-8));

        sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &y);
    }

    /* ---- Modified Bessel I0/I1/In K0/K1/Kn (portable soft; hard-coded refs) ---- */
    {
        sn_value x, y;
        /* Reference values at x=1 (A&S / multiprec tables) */
        const double I0_1 = 1.2660658777520083356;
        const double I1_1 = 0.5651591039924850272;
        const double K0_1 = 0.4210244382407083334;
        const double K1_1 = 0.6019072301972345747;
        const double I2_1 = 0.1357476697670382812;
        const double K2_1 = 1.6248388986351774830;

        sn_value_init(&x); sn_value_init(&y);

        CHECK(sn_f64(&ctx, &x, 1.0) == SN_OK);
        CHECK(sn_i0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I0_1, 1e-12));
        CHECK(sn_i1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I1_1, 1e-12));
        CHECK(sn_in(&ctx, &y, 2, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I2_1, 1e-11));
        CHECK(sn_in(&ctx, &y, -2, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I2_1, 1e-11));
        CHECK(sn_k0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K0_1, 1e-11));
        CHECK(sn_k1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K1_1, 1e-11));
        CHECK(sn_kn(&ctx, &y, 2, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K2_1, 1e-10));
        CHECK(sn_kn(&ctx, &y, -2, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K2_1, 1e-10));

        /* I even/odd in x */
        CHECK(sn_f64(&ctx, &x, -1.0) == SN_OK);
        CHECK(sn_i0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I0_1, 1e-12));
        CHECK(sn_i1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, -I1_1, 1e-12));

        /* K domain: x<=0 -> NaN + INVALID */
        CHECK(sn_f64(&ctx, &x, 0.0) == SN_OK);
        CHECK(sn_k0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        CHECK(sn_f64(&ctx, &x, -1.0) == SN_OK);
        CHECK(sn_k1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_isnan(&y));

        /* multiprec (15,80) */
        CHECK(sn_float_from_double(&ctx, &x, 1.0, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_i0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I0_1, 1e-12));
        CHECK(sn_k0(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K0_1, 1e-10));
        CHECK(sn_i1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I1_1, 1e-12));
        CHECK(sn_k1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K1_1, 1e-10));

        /* Wronskian: I0*K1 + I1*K0 = 1/x at x=1 */
        {
            sn_value i0v, i1v, k0v, k1v, t, s;
            sn_value_init(&i0v); sn_value_init(&i1v); sn_value_init(&k0v);
            sn_value_init(&k1v); sn_value_init(&t); sn_value_init(&s);
            CHECK(sn_f64(&ctx, &x, 1.0) == SN_OK);
            CHECK(sn_i0(&ctx, &i0v, &x, NULL) == SN_OK);
            CHECK(sn_i1(&ctx, &i1v, &x, NULL) == SN_OK);
            CHECK(sn_k0(&ctx, &k0v, &x, NULL) == SN_OK);
            CHECK(sn_k1(&ctx, &k1v, &x, NULL) == SN_OK);
            CHECK(sn_mul(&ctx, &t, &i0v, &k1v, NULL) == SN_OK);
            CHECK(sn_mul(&ctx, &s, &i1v, &k0v, NULL) == SN_OK);
            CHECK(sn_add(&ctx, &t, &t, &s, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &t, &d) == SN_OK && nearly_eq(d, 1.0, 1e-10));
            sn_value_clear(&ctx, &i0v); sn_value_clear(&ctx, &i1v);
            sn_value_clear(&ctx, &k0v); sn_value_clear(&ctx, &k1v);
            sn_value_clear(&ctx, &t); sn_value_clear(&ctx, &s);
        }

        CHECK(api.math.i0 == sn_i0);
        CHECK(api.math.k0 == sn_k0);

        /* Large |x| asymptotic path (|x|>20): I0/I1/K0/K1 vs A&S-style refs */
        {
            /* A&S 9.7 asymptotic (4 terms) reference at x=25;
             * also cross-checked by Wronskian I0*K1+I1*K0=1/x below. */
            const double I0_25 = 5.774558807996206e+09;
            const double I1_25 = 5.657867432057606e+09;
            const double K0_25 = 3.464160636391026e-12;
            const double K1_25 = 3.532779267946652e-12;
            CHECK(sn_f64(&ctx, &x, 25.0) == SN_OK);
            CHECK(sn_i0(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I0_25, 1e-9));
            CHECK(sn_i1(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I1_25, 1e-9));
            CHECK(sn_k0(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K0_25, 1e-8));
            CHECK(sn_k1(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K1_25, 1e-8));
            /* Wronskian at x=25: I0*K1 + I1*K0 = 1/x */
            {
                sn_value i0v, i1v, k0v, k1v, t, s;
                sn_value_init(&i0v); sn_value_init(&i1v); sn_value_init(&k0v);
                sn_value_init(&k1v); sn_value_init(&t); sn_value_init(&s);
                CHECK(sn_i0(&ctx, &i0v, &x, NULL) == SN_OK);
                CHECK(sn_i1(&ctx, &i1v, &x, NULL) == SN_OK);
                CHECK(sn_k0(&ctx, &k0v, &x, NULL) == SN_OK);
                CHECK(sn_k1(&ctx, &k1v, &x, NULL) == SN_OK);
                CHECK(sn_mul(&ctx, &t, &i0v, &k1v, NULL) == SN_OK);
                CHECK(sn_mul(&ctx, &s, &i1v, &k0v, NULL) == SN_OK);
                CHECK(sn_add(&ctx, &t, &t, &s, NULL) == SN_OK);
                /* truncated asymptotic series: relative error ~1e-7 at x=25 */
                CHECK(sn_to_double(&ctx, &t, &d) == SN_OK && nearly_eq(d, 1.0/25.0, 2e-7));
                sn_value_clear(&ctx, &i0v); sn_value_clear(&ctx, &i1v);
                sn_value_clear(&ctx, &k0v); sn_value_clear(&ctx, &k1v);
                sn_value_clear(&ctx, &t); sn_value_clear(&ctx, &s);
            }
            /* In(n=2) large-x recurrence from asymptotic seeds */
            {
                /* I2 from I recurrence with asymptotic seeds: I2 = I0 - (2/x)I1 */
                const double I2_25 = 5.321929413431598e+09;
                CHECK(sn_in(&ctx, &y, 2, &x, NULL) == SN_OK);
                CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I2_25, 1e-8));
            }
        }

        sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &y);
    }

    /* ---- Complete elliptic integrals K(m), E(m) (AGM soft) ---- */
    {
        sn_value m, y;
        const double PI_2 = 1.5707963267948966;
        /* parameter m=k^2=0.5 (not modulus k=0.5); K=pi/(2*AGM(1,sqrt(1-m))) */
        const double K_half = 1.8540746773013719;
        const double E_half = 1.3506438810476765;

        sn_value_init(&m); sn_value_init(&y);

        /* K(0)=E(0)=pi/2 */
        CHECK(sn_f64(&ctx, &m, 0.0) == SN_OK);
        CHECK(sn_ellipk(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, PI_2, 1e-14));
        CHECK(sn_ellipe(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, PI_2, 1e-14));

        /* m=0.5 classic values */
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_ellipk(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K_half, 1e-12));
        CHECK(sn_ellipe(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, E_half, 1e-12));

        /* m=1: K=+inf, E=1 */
        CHECK(sn_f64(&ctx, &m, 1.0) == SN_OK);
        CHECK(sn_ellipk(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_isinf(&y));
        CHECK(!sn_fp_signbit(&y));
        CHECK(sn_ellipe(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-14));

        /* domain: m<0, m>1 -> NaN + INVALID */
        CHECK(sn_f64(&ctx, &m, -0.1) == SN_OK);
        CHECK(sn_ellipk(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        CHECK(sn_f64(&ctx, &m, 1.5) == SN_OK);
        CHECK(sn_ellipe(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_isnan(&y));

        /* multiprec (15,80) m=0.5 */
        CHECK(sn_float_from_double(&ctx, &m, 0.5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_ellipk(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, K_half, 1e-12));
        CHECK(sn_ellipe(&ctx, &y, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, E_half, 1e-12));

        CHECK(api.math.ellipk == sn_ellipk);
        CHECK(api.math.ellipe == sn_ellipe);

        sn_value_clear(&ctx, &m); sn_value_clear(&ctx, &y);
    }

    /* ---- Miller In for large n / modest x ---- */
    {
        sn_value x, y;
        /* I10(1) ~ 2.751212930959549e-10 (tables / scipy) */
        const double I10_1 = 2.752948039836874e-10;
        sn_value_init(&x); sn_value_init(&y);
        CHECK(sn_f64(&ctx, &x, 1.0) == SN_OK);
        CHECK(sn_in(&ctx, &y, 10, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I10_1, 5e-6));
        /* I5(2) ~ 0.003739772971647 (approx) */
        {
            const double I5_2 = 0.0098256793231317;
            CHECK(sn_f64(&ctx, &x, 2.0) == SN_OK);
            CHECK(sn_in(&ctx, &y, 5, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, I5_2, 1e-6));
        }
        sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &y);
    }

    /* ---- Regularized incomplete gamma P/Q ---- */
    {
        sn_value a, x, y, z;
        /* P(1,x)=1-e^{-x}, Q(1,x)=e^{-x} */
        sn_value_init(&a); sn_value_init(&x); sn_value_init(&y); sn_value_init(&z);
        CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &x, 1.0) == SN_OK);
        CHECK(sn_igamma(&ctx, &y, &a, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0 - exp(-1.0), 1e-12));
        CHECK(sn_igammac(&ctx, &z, &a, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &z, &d) == SN_OK && nearly_eq(d, exp(-1.0), 1e-12));
        /* P+Q ~ 1 */
        CHECK(sn_add(&ctx, &y, &y, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12));
        /* a=2.5, x=1.5 series region; a=2.5,x=5 CF region — P+Q=1 */
        CHECK(sn_f64(&ctx, &a, 2.5) == SN_OK);
        CHECK(sn_f64(&ctx, &x, 1.5) == SN_OK);
        CHECK(sn_igamma(&ctx, &y, &a, &x, NULL) == SN_OK);
        CHECK(sn_igammac(&ctx, &z, &a, &x, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &y, &y, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-10));
        CHECK(sn_f64(&ctx, &x, 5.0) == SN_OK);
        CHECK(sn_igamma(&ctx, &y, &a, &x, NULL) == SN_OK);
        CHECK(sn_igammac(&ctx, &z, &a, &x, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &y, &y, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-10));
        /* domain a<=0 */
        CHECK(sn_f64(&ctx, &a, -1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &x, 1.0) == SN_OK);
        CHECK(sn_igamma(&ctx, &y, &a, &x, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        CHECK(api.math.igamma == sn_igamma);
        CHECK(api.math.igammac == sn_igammac);
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &x);
        sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &z);
    }

    /* ---- Regularized incomplete beta I_x(a,b) ---- */
    {
        sn_value a, b, x, y, z;
        sn_value_init(&a); sn_value_init(&b); sn_value_init(&x);
        sn_value_init(&y); sn_value_init(&z);
        /* I_x(1,1) = x */
        CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &x, 0.3) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.3, 1e-12));
        CHECK(sn_f64(&ctx, &x, 0.5) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.5, 1e-12));
        CHECK(sn_f64(&ctx, &x, 0.7) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.7, 1e-12));
        /* I_x(1,1) + I_x^c(1,1) ~ 1 */
        CHECK(sn_ibetac(&ctx, &z, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &y, &y, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12));
        /* I_{0.5}(2,2)=0.5 by symmetry */
        CHECK(sn_f64(&ctx, &a, 2.0) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 2.0) == SN_OK);
        CHECK(sn_f64(&ctx, &x, 0.5) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.5, 1e-10));
        /* I_x(2,2)=x^2(3-2x); at x=0.25: 0.0625*2.5=0.15625 */
        CHECK(sn_f64(&ctx, &x, 0.25) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.15625, 1e-10));
        /* endpoints */
        CHECK(sn_f64(&ctx, &x, 0.0) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.0, 1e-15));
        CHECK(sn_f64(&ctx, &x, 1.0) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 1.0, 1e-15));
        /* domain: a<=0 */
        CHECK(sn_f64(&ctx, &a, -1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &x, 0.5) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        /* domain: x>1 */
        CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &x, 1.5) == SN_OK);
        CHECK(sn_ibeta(&ctx, &y, &a, &b, &x, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        CHECK(api.math.ibeta == sn_ibeta);
        CHECK(api.math.ibetac == sn_ibetac);
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &x);
        sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &z);
    }

    /* ---- Jacobi elliptic sn/cn/dn ---- */
    {
        sn_value u, m, snv, cnv, dnv, t, one;
        double sn2, cn2, dn2, mval;
        sn_value_init(&u); sn_value_init(&m); sn_value_init(&snv);
        sn_value_init(&cnv); sn_value_init(&dnv); sn_value_init(&t); sn_value_init(&one);
        /* u=0 -> sn=0, cn=1, dn=1 for any m */
        CHECK(sn_f64(&ctx, &u, 0.0) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_jacobi_sn(&ctx, &snv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_cn(&ctx, &cnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_dn(&ctx, &dnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &snv, &d) == SN_OK && nearly_eq(d, 0.0, 1e-12));
        CHECK(sn_to_double(&ctx, &cnv, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12));
        CHECK(sn_to_double(&ctx, &dnv, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12));
        /* m=0: sn=sin, cn=cos, dn=1 */
        CHECK(sn_f64(&ctx, &m, 0.0) == SN_OK);
        CHECK(sn_f64(&ctx, &u, 1.0) == SN_OK);
        CHECK(sn_jacobi_sn(&ctx, &snv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_cn(&ctx, &cnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_dn(&ctx, &dnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &snv, &d) == SN_OK && nearly_eq(d, sin(1.0), 1e-12));
        CHECK(sn_to_double(&ctx, &cnv, &d) == SN_OK && nearly_eq(d, cos(1.0), 1e-12));
        CHECK(sn_to_double(&ctx, &dnv, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12));
        /* m=1: sn=tanh, cn=dn=sech */
        CHECK(sn_f64(&ctx, &m, 1.0) == SN_OK);
        CHECK(sn_jacobi_sn(&ctx, &snv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_cn(&ctx, &cnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_dn(&ctx, &dnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &snv, &d) == SN_OK && nearly_eq(d, tanh(1.0), 1e-10));
        CHECK(sn_to_double(&ctx, &cnv, &d) == SN_OK && nearly_eq(d, 1.0 / cosh(1.0), 1e-10));
        CHECK(sn_to_double(&ctx, &dnv, &d) == SN_OK && nearly_eq(d, 1.0 / cosh(1.0), 1e-10));
        /* m=0.5, u=1: sn^2+cn^2=1, dn^2+m sn^2=1 */
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_f64(&ctx, &u, 1.0) == SN_OK);
        CHECK(sn_jacobi_sn(&ctx, &snv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_cn(&ctx, &cnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_dn(&ctx, &dnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &snv, &sn2) == SN_OK);
        CHECK(sn_to_double(&ctx, &cnv, &cn2) == SN_OK);
        CHECK(sn_to_double(&ctx, &dnv, &dn2) == SN_OK);
        CHECK(nearly_eq(sn2 * sn2 + cn2 * cn2, 1.0, 1e-10));
        CHECK(nearly_eq(dn2 * dn2 + 0.5 * sn2 * sn2, 1.0, 1e-10));
        /* m=0.3, u=0.7 identities */
        CHECK(sn_f64(&ctx, &m, 0.3) == SN_OK);
        CHECK(sn_f64(&ctx, &u, 0.7) == SN_OK);
        CHECK(sn_jacobi_sn(&ctx, &snv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_cn(&ctx, &cnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_jacobi_dn(&ctx, &dnv, &u, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &snv, &sn2) == SN_OK);
        CHECK(sn_to_double(&ctx, &cnv, &cn2) == SN_OK);
        CHECK(sn_to_double(&ctx, &dnv, &dn2) == SN_OK);
        mval = 0.3;
        CHECK(nearly_eq(sn2 * sn2 + cn2 * cn2, 1.0, 1e-9));
        CHECK(nearly_eq(dn2 * dn2 + mval * sn2 * sn2, 1.0, 1e-9));
        /* domain m>1 */
        CHECK(sn_f64(&ctx, &m, 1.5) == SN_OK);
        CHECK(sn_jacobi_sn(&ctx, &snv, &u, &m, NULL) == SN_OK);
        CHECK(sn_isnan(&snv));
        CHECK(api.math.jacobi_sn == sn_jacobi_sn);
        CHECK(api.math.jacobi_cn == sn_jacobi_cn);
        CHECK(api.math.jacobi_dn == sn_jacobi_dn);
        sn_value_clear(&ctx, &u); sn_value_clear(&ctx, &m);
        sn_value_clear(&ctx, &snv); sn_value_clear(&ctx, &cnv); sn_value_clear(&ctx, &dnv);
        sn_value_clear(&ctx, &t); sn_value_clear(&ctx, &one);
        (void)t; (void)one;
    }

    /* ---- Incomplete elliptic E(phi|m) ---- */
    {
        sn_value phi, m, y, e;
        const double pi_2 = 1.5707963267948966;
        sn_value_init(&phi); sn_value_init(&m); sn_value_init(&y); sn_value_init(&e);
        /* E(phi|0) = phi */
        CHECK(sn_f64(&ctx, &phi, 0.7) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.0) == SN_OK);
        CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.7, 1e-12));
        /* E(0|m)=0 */
        CHECK(sn_f64(&ctx, &phi, 0.0) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.0, 1e-15));
        /* E(pi/2|m) = complete E(m) */
        CHECK(sn_f64(&ctx, &phi, pi_2) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.3) == SN_OK);
        CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_ellipe(&ctx, &e, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(sn_to_double(&ctx, &e, &re) == SN_OK && nearly_eq(d, re, 1e-10));
        /* m=1: E(phi|1)=sin(phi) */
        CHECK(sn_f64(&ctx, &phi, 0.6) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 1.0) == SN_OK);
        CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, sin(0.6), 1e-12));
        /* odd in phi */
        CHECK(sn_f64(&ctx, &m, 0.4) == SN_OK);
        CHECK(sn_f64(&ctx, &phi, 0.5) == SN_OK);
        CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(sn_f64(&ctx, &phi, -0.5) == SN_OK);
        CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &re) == SN_OK && nearly_eq(re, -d, 1e-10));
        /* E <= F for m in (0,1), phi in (0,pi/2) */
        CHECK(sn_f64(&ctx, &phi, 0.8) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_ellipeinc(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_ellipf(&ctx, &e, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(sn_to_double(&ctx, &e, &re) == SN_OK && d <= re + 1e-12);
        CHECK(api.math.ellipeinc == sn_ellipeinc);
        sn_value_clear(&ctx, &phi); sn_value_clear(&ctx, &m);
        sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &e);
    }


    /* ---- Incomplete elliptic Pi(n;phi|m) ---- */
    {
        sn_value phi, n, m, y, f;
        const double pi_2 = 1.5707963267948966;
        sn_value_init(&phi); sn_value_init(&n); sn_value_init(&m);
        sn_value_init(&y); sn_value_init(&f);
        /* n=0 -> F(phi|m) */
        CHECK(sn_f64(&ctx, &phi, 0.8) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 0.0) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_ellipf(&ctx, &f, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(sn_to_double(&ctx, &f, &re) == SN_OK && nearly_eq(d, re, 1e-12));
        /* phi=0 -> 0 */
        CHECK(sn_f64(&ctx, &phi, 0.0) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 0.3) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.0, 1e-15));
        /* known values via Carlson (scipy elliprf/rj cross-check) */
        CHECK(sn_f64(&ctx, &phi, 0.5) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 0.2) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.3) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.5144750854888095, 1e-9));
        CHECK(sn_f64(&ctx, &phi, 0.8) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 0.5) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.9416692002173205, 1e-9));
        /* negative n */
        CHECK(sn_f64(&ctx, &phi, 0.6) == SN_OK);
        CHECK(sn_f64(&ctx, &n, -0.2) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.4) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.6008144715694514, 1e-9));
        /* complete Pi(n|m) at phi=pi/2 */
        CHECK(sn_f64(&ctx, &phi, pi_2) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 0.5) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.3) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 2.461255352272421, 1e-8));
        /* odd in phi */
        CHECK(sn_f64(&ctx, &phi, 0.5) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 0.2) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.3) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(sn_f64(&ctx, &phi, -0.5) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &re) == SN_OK && nearly_eq(re, -d, 1e-10));
        /* pole: n sin^2 >= 1 -> domain NaN */
        CHECK(sn_f64(&ctx, &phi, pi_2) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 1.0) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.3) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        /* domain m>1 */
        CHECK(sn_f64(&ctx, &phi, 0.5) == SN_OK);
        CHECK(sn_f64(&ctx, &n, 0.2) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 1.5) == SN_OK);
        CHECK(sn_ellipiinc(&ctx, &y, &phi, &n, &m, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        CHECK(api.math.ellipiinc == sn_ellipiinc);
        sn_value_clear(&ctx, &phi); sn_value_clear(&ctx, &n);
        sn_value_clear(&ctx, &m); sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &f);
    }

    /* ---- digamma ?(x) ---- */
    {
        sn_value a, y;
        /* ?(1) = -? ? -0.5772156649015329 */
        const double gamma_e = 0.5772156649015328606;
        sn_value_init(&a); sn_value_init(&y);
        CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
        CHECK(sn_digamma(&ctx, &y, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, -gamma_e, 1e-10));
        /* ?(2) = -? + 1 */
        CHECK(sn_f64(&ctx, &a, 2.0) == SN_OK);
        CHECK(sn_digamma(&ctx, &y, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, -gamma_e + 1.0, 1e-10));
        /* recurrence ?(x+1)=?(x)+1/x at x=1.5 */
        {
            sn_value x, xp1, p, q, inv;
            sn_value_init(&x); sn_value_init(&xp1); sn_value_init(&p);
            sn_value_init(&q); sn_value_init(&inv);
            CHECK(sn_f64(&ctx, &x, 1.5) == SN_OK);
            CHECK(sn_f64(&ctx, &xp1, 2.5) == SN_OK);
            CHECK(sn_digamma(&ctx, &p, &x, NULL) == SN_OK);
            CHECK(sn_digamma(&ctx, &q, &xp1, NULL) == SN_OK);
            CHECK(sn_f64(&ctx, &inv, 1.0 / 1.5) == SN_OK);
            CHECK(sn_add(&ctx, &p, &p, &inv, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &p, &d) == SN_OK);
            CHECK(sn_to_double(&ctx, &q, &re) == SN_OK && nearly_eq(d, re, 1e-10));
            sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &xp1);
            sn_value_clear(&ctx, &p); sn_value_clear(&ctx, &q); sn_value_clear(&ctx, &inv);
        }
        /* pole at 0 */
        CHECK(sn_f64(&ctx, &a, 0.0) == SN_OK);
        CHECK(sn_digamma(&ctx, &y, &a, NULL) == SN_OK);
        CHECK(sn_isnan(&y) || sn_isinf(&y));
        CHECK(api.math.digamma == sn_digamma);
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &y);
    }


    /* ---- trigamma / polygamma ---- */
    {
        sn_value a, y;
        const double pi2_6 = 1.6449340668482264; /* pi^2/6 */
        const double pi4_15 = 6.493939402266829; /* pi^4/15 = 6*zeta(4) = psi^{(3)}(1) */
        sn_value_init(&a); sn_value_init(&y);
        /* psi_1(1) = pi^2/6 */
        CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
        CHECK(sn_trigamma(&ctx, &y, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, pi2_6, 1e-10));
        /* polygamma(1,1) same as trigamma(1) */
        CHECK(sn_polygamma(&ctx, &y, 1, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, pi2_6, 1e-10));
        /* psi_1(2) = pi^2/6 - 1 */
        CHECK(sn_f64(&ctx, &a, 2.0) == SN_OK);
        CHECK(sn_trigamma(&ctx, &y, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, pi2_6 - 1.0, 1e-10));
        /* recurrence psi_1(x)=psi_1(x+1)+1/x^2 at x=1.5 */
        {
            sn_value x, xp1, p, q, inv2;
            sn_value_init(&x); sn_value_init(&xp1); sn_value_init(&p);
            sn_value_init(&q); sn_value_init(&inv2);
            CHECK(sn_f64(&ctx, &x, 1.5) == SN_OK);
            CHECK(sn_f64(&ctx, &xp1, 2.5) == SN_OK);
            CHECK(sn_trigamma(&ctx, &p, &x, NULL) == SN_OK);
            CHECK(sn_trigamma(&ctx, &q, &xp1, NULL) == SN_OK);
            CHECK(sn_f64(&ctx, &inv2, 1.0 / (1.5 * 1.5)) == SN_OK);
            CHECK(sn_add(&ctx, &q, &q, &inv2, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &p, &d) == SN_OK);
            CHECK(sn_to_double(&ctx, &q, &re) == SN_OK && nearly_eq(d, re, 1e-10));
            sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &xp1);
            sn_value_clear(&ctx, &p); sn_value_clear(&ctx, &q); sn_value_clear(&ctx, &inv2);
        }
        /* polygamma(3,1)=pi^4/90 */
        CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
        CHECK(sn_polygamma(&ctx, &y, 3, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, pi4_15, 1e-8));
        /* polygamma(0,.) == digamma */
        {
            sn_value g, p;
            sn_value_init(&g); sn_value_init(&p);
            CHECK(sn_f64(&ctx, &a, 1.5) == SN_OK);
            CHECK(sn_digamma(&ctx, &g, &a, NULL) == SN_OK);
            CHECK(sn_polygamma(&ctx, &p, 0, &a, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &g, &d) == SN_OK);
            CHECK(sn_to_double(&ctx, &p, &re) == SN_OK && nearly_eq(d, re, 1e-12));
            sn_value_clear(&ctx, &g); sn_value_clear(&ctx, &p);
        }
        /* finite-diff digamma ~ trigamma at x=2.5 */
        {
            sn_value xh, xl, ph, pl, diff, h;
            double hval = 1e-5;
            sn_value_init(&xh); sn_value_init(&xl); sn_value_init(&ph);
            sn_value_init(&pl); sn_value_init(&diff); sn_value_init(&h);
            CHECK(sn_f64(&ctx, &xh, 2.5 + hval) == SN_OK);
            CHECK(sn_f64(&ctx, &xl, 2.5 - hval) == SN_OK);
            CHECK(sn_digamma(&ctx, &ph, &xh, NULL) == SN_OK);
            CHECK(sn_digamma(&ctx, &pl, &xl, NULL) == SN_OK);
            CHECK(sn_sub(&ctx, &diff, &ph, &pl, NULL) == SN_OK);
            CHECK(sn_f64(&ctx, &h, 2.0 * hval) == SN_OK);
            CHECK(sn_div(&ctx, &diff, &diff, &h, NULL) == SN_OK);
            CHECK(sn_f64(&ctx, &a, 2.5) == SN_OK);
            CHECK(sn_trigamma(&ctx, &y, &a, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &diff, &d) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &re) == SN_OK && nearly_eq(d, re, 1e-4));
            sn_value_clear(&ctx, &xh); sn_value_clear(&ctx, &xl);
            sn_value_clear(&ctx, &ph); sn_value_clear(&ctx, &pl);
            sn_value_clear(&ctx, &diff); sn_value_clear(&ctx, &h);
        }
        /* pole at 0 */
        CHECK(sn_f64(&ctx, &a, 0.0) == SN_OK);
        CHECK(sn_trigamma(&ctx, &y, &a, NULL) == SN_OK);
        CHECK(sn_isnan(&y) || sn_isinf(&y));
        /* +inf -> 0 */
        CHECK(sn_float_set_inf(&ctx, &a, 0, 11, 52, 1) == SN_OK);
        CHECK(sn_trigamma(&ctx, &y, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.0, 1e-15));
        CHECK(api.math.trigamma == sn_trigamma);
        CHECK(api.math.polygamma == sn_polygamma);
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &y);
    }

    /* ---- Incomplete elliptic F(?|m) ---- */
    {
        sn_value phi, m, y, k;
        const double pi_2 = 1.5707963267948966;
        sn_value_init(&phi); sn_value_init(&m); sn_value_init(&y); sn_value_init(&k);
        /* F(?|0) = ? */
        CHECK(sn_f64(&ctx, &phi, 0.7) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.0) == SN_OK);
        CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.7, 1e-12));
        /* F(0|m)=0 */
        CHECK(sn_f64(&ctx, &phi, 0.0) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.5) == SN_OK);
        CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, 0.0, 1e-15));
        /* F(?/2|m) = K(m) */
        CHECK(sn_f64(&ctx, &phi, pi_2) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.3) == SN_OK);
        CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_ellipk(&ctx, &k, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(sn_to_double(&ctx, &k, &re) == SN_OK && nearly_eq(d, re, 1e-10));
        /* F(?|m) odd in ? */
        CHECK(sn_f64(&ctx, &phi, 0.5) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 0.4) == SN_OK);
        CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(sn_f64(&ctx, &phi, -0.5) == SN_OK);
        CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &re) == SN_OK && nearly_eq(re, -d, 1e-10));
        /* m=1: F(?|1)=artanh(sin ?) */
        CHECK(sn_f64(&ctx, &phi, 0.6) == SN_OK);
        CHECK(sn_f64(&ctx, &m, 1.0) == SN_OK);
        CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK && nearly_eq(d, atanh(sin(0.6)), 1e-10));
        /* domain m>1 */
        CHECK(sn_f64(&ctx, &m, 1.5) == SN_OK);
        CHECK(sn_ellipf(&ctx, &y, &phi, &m, NULL) == SN_OK);
        CHECK(sn_isnan(&y));
        CHECK(api.math.ellipf == sn_ellipf);
        sn_value_clear(&ctx, &phi); sn_value_clear(&ctx, &m);
        sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &k);
    }

    /* ---- Complex inverse trig branch cuts ---- */
    {
        sn_cplx z, w;
        double aim;
        sn_cplx_init(&z); sn_cplx_init(&w);

        /* asin(2): Re = pi/2, |Im| = acosh(2) */
        CHECK(sn_cplx_set_d(&ctx, &z, 2.0, 0.0, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_asin(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &w.re, &re) == SN_OK && nearly_eq(re, 1.5707963267948966, 1e-10));
        CHECK(sn_to_double(&ctx, &w.im, &im) == SN_OK);
        aim = im < 0 ? -im : im;
        CHECK(nearly_eq(aim, acosh(2.0), 1e-8));

        /* asin(-2): Re = -pi/2, |Im| = acosh(2) */
        CHECK(sn_cplx_set_d(&ctx, &z, -2.0, 0.0, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_asin(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &w.re, &re) == SN_OK && nearly_eq(re, -1.5707963267948966, 1e-10));
        CHECK(sn_to_double(&ctx, &w.im, &im) == SN_OK);
        aim = im < 0 ? -im : im;
        CHECK(nearly_eq(aim, acosh(2.0), 1e-8));

        /* acos(2): Re ~ 0, |Im| = acosh(2) */
        CHECK(sn_cplx_set_d(&ctx, &z, 2.0, 0.0, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_acos(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &w.re, &re) == SN_OK && nearly_eq(re, 0.0, 1e-8));
        CHECK(sn_to_double(&ctx, &w.im, &im) == SN_OK);
        aim = im < 0 ? -im : im;
        CHECK(nearly_eq(aim, acosh(2.0), 1e-8));

        /* lower half-plane: asin(0.5 - 0.1i) has Im < 0 */
        CHECK(sn_cplx_set_d(&ctx, &z, 0.5, -0.1, 11, 52, 1, NULL) == SN_OK);
        CHECK(sn_cplx_asin(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &w.im, &im) == SN_OK && im < 0.0);

        /* multiprec neighborhood of branch cut */
        CHECK(sn_cplx_set_d(&ctx, &z, 1.5, 0.0, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_cplx_asin(&ctx, &w, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &w.re, &re) == SN_OK && nearly_eq(re, 1.5707963267948966, 1e-8));
        CHECK(sn_to_double(&ctx, &w.im, &im) == SN_OK);
        aim = im < 0 ? -im : im;
        CHECK(nearly_eq(aim, acosh(1.5), 1e-7));

        sn_cplx_clear(&ctx, &z); sn_cplx_clear(&ctx, &w);
    }

    sn_cplx_clear(&ctx, &a);
    sn_cplx_clear(&ctx, &b);
    sn_cplx_clear(&ctx, &c);
    sn_value_clear(&ctx, &r);
    return 0;
}
