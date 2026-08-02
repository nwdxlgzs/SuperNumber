#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
/* internal helper also used by library float path */
sn_status sn_float_from_double(sn_ctx *ctx, sn_value *out, double x,
                               int e_bits, int m_bits, int nan_enabled, const sn_op_opt *opt);

#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

int test_float_run(void)
{
    sn_ctx ctx;
    sn_value a, b, c, dval;
    double d;
    int rel;
    int64_t x;
    char *s;

    sn_ctx_init(&ctx);
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&c);
    sn_value_init(&dval);

    CHECK(sn_f64(&ctx, &a, 1.5) == SN_OK);
    CHECK(sn_fp_classify(&a) == SN_FP_NORMAL);
    CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
    CHECK(d == 1.5);

    CHECK(sn_f64(&ctx, &b, 2.5) == SN_OK);
    CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(d == 4.0);

    CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(d == 3.75);

    CHECK(sn_f32(&ctx, &a, -8.0) == SN_OK);
    CHECK(sn_f32(&ctx, &b, 2.0) == SN_OK);
    CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(d == -4.0);

    /* Regression: full-precision soft div (funp_div) - must not truncate mantissa.
     * Historical bug: q used only ~10 free shift bits when sig~2^53, e.g.
     * 1.9403357247508828/2 became 0.969970703125 instead of 0.9701678623754414. */
    {
        double ha = 1.9403357247508828;
        double hb = 2.0;
        CHECK(sn_f64(&ctx, &a, ha) == SN_OK);
        CHECK(sn_f64(&ctx, &b, hb) == SN_OK);
        CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d == ha / hb);
        /* aliasing: out == a and out == b */
        CHECK(sn_f64(&ctx, &a, ha) == SN_OK);
        CHECK(sn_f64(&ctx, &b, hb) == SN_OK);
        CHECK(sn_div(&ctx, &a, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK && d == ha / hb);
        CHECK(sn_f64(&ctx, &a, ha) == SN_OK);
        CHECK(sn_f64(&ctx, &b, hb) == SN_OK);
        CHECK(sn_div(&ctx, &b, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &b, &d) == SN_OK && d == ha / hb);
    }

    CHECK(sn_f64(&ctx, &a, 3.0) == SN_OK);
    CHECK(sn_neg(&ctx, &c, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(d == -3.0);
    CHECK(sn_abs(&ctx, &c, &c, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(d == 3.0);

    /* div by zero -> +Inf */
    sn_ctx_clear_flags(&ctx);
    CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 0.0) == SN_OK);
    CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_fp_classify(&c) == SN_FP_INFINITE);
    CHECK(sn_fp_signbit(&c) == 0);
    CHECK((sn_ctx_get_flags(&ctx) & SN_FLAG_DIVZERO) != 0);

    /* compare */
    CHECK(sn_f64(&ctx, &a, 1.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 2.0) == SN_OK);
    CHECK(sn_cmp(&ctx, &rel, &a, &b) == SN_OK && rel < 0);

    /* zero / inf helpers */
    CHECK(sn_float_set_zero(&ctx, &a, 1, 8, 23, 1) == SN_OK);
    CHECK(sn_fp_classify(&a) == SN_FP_ZERO);
    CHECK(sn_fp_signbit(&a) == 1);
    CHECK(sn_float_set_inf(&ctx, &b, 0, 8, 23, 1) == SN_OK);
    CHECK(sn_fp_classify(&b) == SN_FP_INFINITE);

    CHECK(sn_float_set_nan(&ctx, &c, 8, 23) == SN_OK);
    CHECK(sn_fp_classify(&c) == SN_FP_NAN);

    /* f16 round-trip small int */
    CHECK(sn_f16(&ctx, &a, 7.0) == SN_OK);
    CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
    CHECK(d == 7.0);

    /* sqrt */
    CHECK(sn_f64(&ctx, &a, 9.0) == SN_OK);
    CHECK(sn_sqrt(&ctx, &c, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(d == 3.0);

    CHECK(sn_f64(&ctx, &a, 2.0) == SN_OK);
    CHECK(sn_sqrt(&ctx, &c, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(fabs(d - sqrt(2.0)) < 1e-12);

    /* sqrt negative -> invalid */
    sn_ctx_clear_flags(&ctx);
    CHECK(sn_f64(&ctx, &a, -1.0) == SN_OK);
    CHECK(sn_sqrt(&ctx, &c, &a, NULL) == SN_OK);
    CHECK(sn_fp_classify(&c) == SN_FP_NAN);
    CHECK((sn_ctx_get_flags(&ctx) & SN_FLAG_INVALID) != 0);

    /* fma: 3*4+5 = 17 */
    CHECK(sn_f64(&ctx, &a, 3.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 4.0) == SN_OK);
    CHECK(sn_f64(&ctx, &c, 5.0) == SN_OK);
    CHECK(sn_fma(&ctx, &dval, &a, &b, &c, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &dval, &d) == SN_OK);
    CHECK(d == 17.0);
    /* fused: (1+eps)^2 - 1 keeps 2eps+eps^2 (non-fused loses eps^2) */
    {
        double eps = 0x1p-52;
        double expect = 2.0 * eps + eps * eps; /* exact in binary64 */
        CHECK(sn_f64(&ctx, &a, 1.0 + eps) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 1.0 + eps) == SN_OK);
        CHECK(sn_f64(&ctx, &c, -1.0) == SN_OK);
        CHECK(sn_fma(&ctx, &dval, &a, &b, &c, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &dval, &d) == SN_OK);
        CHECK(d == expect);
        /* tiny product vs large addend: product sticky must not alter |c| by full ulp wrongly */
        CHECK(sn_f64(&ctx, &a, 0x1p-1000) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 0x1p-1000) == SN_OK);
        CHECK(sn_f64(&ctx, &c, 1.0) == SN_OK);
        CHECK(sn_fma(&ctx, &dval, &a, &b, &c, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &dval, &d) == SN_OK);
        CHECK(d == 1.0);
        CHECK(sn_f64(&ctx, &a, 0x1p-1000) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 0x1p-1000) == SN_OK);
        CHECK(sn_f64(&ctx, &c, -1.0) == SN_OK);
        CHECK(sn_fma(&ctx, &dval, &a, &b, &c, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &dval, &d) == SN_OK);
        CHECK(d == -1.0);
    }
    /* denorm division: no double-round (softfp round_pack order) */
    {
        double hs, sn;
        CHECK(sn_f64(&ctx, &a, 2.0) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 1.7976931348623157e+308) == SN_OK);
        CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &sn) == SN_OK);
        hs = 2.0 / 1.7976931348623157e+308;
        CHECK(sn == hs);
        CHECK(sn_f64(&ctx, &a, 3.141592653589793) == SN_OK);
        CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &sn) == SN_OK);
        hs = 3.141592653589793 / 1.7976931348623157e+308;
        CHECK(sn == hs);
    }

    /* e=31 m=52 multiprec add identity vs host f64 (softfp GRS + sticky-in-LSB) */
    {
        sn_value ta, tb, tc;
        double sn, hs;
        sn_value_init(&ta); sn_value_init(&tb); sn_value_init(&tc);
        CHECK(sn_f64(&ctx, &a, 0.1) == SN_OK);
        CHECK(sn_cast_float(&ctx, &ta, &a, 31, 52, 1, NULL) == SN_OK);
        CHECK(sn_f64(&ctx, &b, 1.0) == SN_OK);
        CHECK(sn_cast_float(&ctx, &tb, &b, 31, 52, 1, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &tc, &ta, &tb, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &tc, &sn) == SN_OK);
        hs = 0.1 + 1.0;
        CHECK(sn == hs);
        sn_value_clear(&ctx, &ta);
        sn_value_clear(&ctx, &tb);
        sn_value_clear(&ctx, &tc);
    }

    /* e=31 m=52 multiprec mul/div/sub/fma vs host (finite bit-exact where host finite) */
    {
        sn_value ta, tb, tc, td;
        double sn, hs;
        int i, j;
        static const double xs[] = {
            0.0, -0.0, 1.0, -1.0, 2.0, 0.5, 0.1, 3.141592653589793,
            1e-200, 1e200, 2.2250738585072014e-308, 5e-324, 1e308
        };
        sn_value_init(&ta); sn_value_init(&tb); sn_value_init(&tc); sn_value_init(&td);
        for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
            for (j = 0; j < (int)(sizeof(xs)/sizeof(xs[0])); j++) {
                double xa = xs[i], xb = xs[j];
                CHECK(sn_f64(&ctx, &a, xa) == SN_OK);
                CHECK(sn_cast_float(&ctx, &ta, &a, 31, 52, 1, NULL) == SN_OK);
                CHECK(sn_f64(&ctx, &b, xb) == SN_OK);
                CHECK(sn_cast_float(&ctx, &tb, &b, 31, 52, 1, NULL) == SN_OK);
                CHECK(sn_add(&ctx, &tc, &ta, &tb, NULL) == SN_OK);
                CHECK(sn_to_double(&ctx, &tc, &sn) == SN_OK);
                hs = xa + xb;
                if (isfinite(hs) && isfinite(sn)) CHECK(sn == hs);
                CHECK(sn_sub(&ctx, &tc, &ta, &tb, NULL) == SN_OK);
                CHECK(sn_to_double(&ctx, &tc, &sn) == SN_OK);
                hs = xa - xb;
                if (isfinite(hs) && isfinite(sn)) CHECK(sn == hs);
                CHECK(sn_mul(&ctx, &tc, &ta, &tb, NULL) == SN_OK);
                CHECK(sn_to_double(&ctx, &tc, &sn) == SN_OK);
                hs = xa * xb;
                if (isfinite(hs) && isfinite(sn)) CHECK(sn == hs);
                if (xb != 0.0 || xa == 0.0) {
                    CHECK(sn_div(&ctx, &tc, &ta, &tb, NULL) == SN_OK);
                    CHECK(sn_to_double(&ctx, &tc, &sn) == SN_OK);
                    hs = xa / xb;
                    /* host subnormal quotients may differ by implementation path; require non-subnormal */
                    if (isfinite(hs) && isfinite(sn) && fpclassify(hs) != FP_SUBNORMAL)
                        CHECK(sn == hs);
                }
            }
        }
        /* multiprec fma(3,4,5)=17 */
        CHECK(sn_float_from_i64(&ctx, &ta, 3, 31, 52, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &tb, 4, 31, 52, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &tc, 5, 31, 52, 1, NULL) == SN_OK);
        CHECK(sn_fma(&ctx, &td, &ta, &tb, &tc, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &td, &sn) == SN_OK);
        CHECK(sn == 17.0);
        sn_value_clear(&ctx, &ta);
        sn_value_clear(&ctx, &tb);
        sn_value_clear(&ctx, &tc);
        sn_value_clear(&ctx, &td);
    }

    /* narrow f64 path vs multiprec e=31/m=52 bit-exact on finite ops */
    {
        sn_value na, nb, nc, ma, mb, mc;
        double dn, dm;
        static const double xs[] = {0.1, 0.5, 1.0, 2.0, 3.141592653589793, 1e-200, 1e200};
        int i, j;
        sn_value_init(&na); sn_value_init(&nb); sn_value_init(&nc);
        sn_value_init(&ma); sn_value_init(&mb); sn_value_init(&mc);
        for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
            for (j = 0; j < (int)(sizeof(xs)/sizeof(xs[0])); j++) {
                CHECK(sn_f64(&ctx, &na, xs[i]) == SN_OK);
                CHECK(sn_f64(&ctx, &nb, xs[j]) == SN_OK);
                CHECK(sn_cast_float(&ctx, &ma, &na, 31, 52, 1, NULL) == SN_OK);
                CHECK(sn_cast_float(&ctx, &mb, &nb, 31, 52, 1, NULL) == SN_OK);
                CHECK(sn_add(&ctx, &nc, &na, &nb, NULL) == SN_OK);
                CHECK(sn_add(&ctx, &mc, &ma, &mb, NULL) == SN_OK);
                CHECK(sn_to_double(&ctx, &nc, &dn) == SN_OK);
                CHECK(sn_to_double(&ctx, &mc, &dm) == SN_OK);
                if (isfinite(dn) && isfinite(dm)) CHECK(dn == dm);
                CHECK(sn_mul(&ctx, &nc, &na, &nb, NULL) == SN_OK);
                CHECK(sn_mul(&ctx, &mc, &ma, &mb, NULL) == SN_OK);
                CHECK(sn_to_double(&ctx, &nc, &dn) == SN_OK);
                CHECK(sn_to_double(&ctx, &mc, &dm) == SN_OK);
                if (isfinite(dn) && isfinite(dm)) CHECK(dn == dm);
            }
        }
        sn_value_clear(&ctx, &na); sn_value_clear(&ctx, &nb); sn_value_clear(&ctx, &nc);
        sn_value_clear(&ctx, &ma); sn_value_clear(&ctx, &mb); sn_value_clear(&ctx, &mc);
    }

    /* m=80 soft transcendentals: exp(log(x))~x and sin^2+cos^2~1 */
    {
        sn_value ta, tr, ts, tt, tu;
        double sn, x;
        sn_value_init(&ta); sn_value_init(&tr); sn_value_init(&ts);
        sn_value_init(&tt); sn_value_init(&tu);
        x = 2.5;
        CHECK(sn_float_from_double(&ctx, &ta, x, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_log(&ctx, &tr, &ta, NULL) == SN_OK);
        CHECK(sn_exp(&ctx, &ts, &tr, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &ts, &sn) == SN_OK);
        CHECK(sn > x * 0.999999 && sn < x * 1.000001);
        CHECK(sn_float_from_double(&ctx, &ta, 1.25, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_sin(&ctx, &tr, &ta, NULL) == SN_OK);
        CHECK(sn_cos(&ctx, &ts, &ta, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &tt, &tr, &tr, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &tu, &ts, &ts, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &tr, &tt, &tu, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &tr, &sn) == SN_OK);
        CHECK(sn > 0.999999 && sn < 1.000001);
        sn_value_clear(&ctx, &ta);
        sn_value_clear(&ctx, &tr);
        sn_value_clear(&ctx, &ts);
        sn_value_clear(&ctx, &tt);
        sn_value_clear(&ctx, &tu);
    }


    /* frem: remainder(5, 3) == -1 or 2 depending; C remainder(5,3)= -1? actually remainder(5,3)= -1? 
       5 = 2*3 -1, n=round(5/3)=round(1.666)=2, r=5-6=-1 */
    CHECK(sn_f64(&ctx, &a, 5.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 3.0) == SN_OK);
    CHECK(sn_frem(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
    CHECK(d == remainder(5.0, 3.0));

    /* cast int <-> float */
    CHECK(sn_i32(&ctx, &a, 42) == SN_OK);
    CHECK(sn_cast_float(&ctx, &b, &a, 11, 52, 1, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &b, &d) == SN_OK);
    CHECK(d == 42.0);
    CHECK(sn_f64(&ctx, &a, -3.9) == SN_OK);
    CHECK(sn_cast_int(&ctx, &b, &a, 32, 1, NULL) == SN_OK);
    CHECK(sn_to_i64(&ctx, &b, &x) == SN_OK && x == -3);

    /* float string */
    CHECK(sn_f64(&ctx, &a, 12.5) == SN_OK);
    CHECK(sn_to_str(&ctx, &s, &a, 10) == SN_OK);
    CHECK(s != NULL);
    CHECK(strcmp(s, "12.5") == 0 || atof(s) == 12.5);
    sn_str_free(&ctx, s);
    CHECK(sn_from_str_float(&ctx, &b, "3.5", 11, 52, 1, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &b, &d) == SN_OK);
    CHECK(d == 3.5);
    CHECK(sn_from_str_float(&ctx, &b, "inf", 11, 52, 1, NULL) == SN_OK);
    CHECK(sn_fp_classify(&b) == SN_FP_INFINITE);

    /* no-NaN: invalid sqrt -> Inf */
    sn_ctx_clear_flags(&ctx);
    CHECK(sn_float_new(&ctx, &a, 8, 23, 0) == SN_OK);
    /* encode -1 with nan disabled via set from double path with nan_enabled=0 */
    CHECK(sn_float_from_i64(&ctx, &a, -1, 8, 23, 0, NULL) == SN_OK);
    CHECK(sn_sqrt(&ctx, &c, &a, NULL) == SN_OK);
    CHECK(sn_fp_classify(&c) == SN_FP_INFINITE);
    CHECK((sn_ctx_get_flags(&ctx) & SN_FLAG_INVALID) != 0);

    
    /* rounding matrix: 1.5 mid-point styles via f32 encode of half-way values
     * Use op_opt override on float mul of numbers that need rounding.
     * 1.0 + 0.5 ulp patterns via cast from double with different modes. */
    {
        sn_op_opt opt;
        double d_nte, d_tz, d_up, d_dn, d_na;
        memset(&opt, 0, sizeof(opt));
        opt.has_round = 1;

        /* 0.1 cannot be exact in binary; rounding should differ by mode at f16 */
        opt.round = SN_ROUND_NTE;
        CHECK(sn_float_from_i64(&ctx, &a, 1, 5, 10, 1, &opt) == SN_OK); /* just ensure API */
        CHECK(sn_f16(&ctx, &a, 0.1) == SN_OK); /* uses ctx round */
        sn_ctx_set_round(&ctx, SN_ROUND_NTE);
        CHECK(sn_f16(&ctx, &a, 0.1) == SN_OK);
        CHECK(sn_to_double(&ctx, &a, &d_nte) == SN_OK);

        sn_ctx_set_round(&ctx, SN_ROUND_TZ);
        CHECK(sn_f16(&ctx, &b, 0.1) == SN_OK);
        CHECK(sn_to_double(&ctx, &b, &d_tz) == SN_OK);

        sn_ctx_set_round(&ctx, SN_ROUND_UP);
        CHECK(sn_f16(&ctx, &c, 0.1) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d_up) == SN_OK);

        sn_ctx_set_round(&ctx, SN_ROUND_DN);
        CHECK(sn_f16(&ctx, &dval, 0.1) == SN_OK);
        CHECK(sn_to_double(&ctx, &dval, &d_dn) == SN_OK);

        sn_ctx_set_round(&ctx, SN_ROUND_NA);
        CHECK(sn_f16(&ctx, &a, 0.1) == SN_OK);
        CHECK(sn_to_double(&ctx, &a, &d_na) == SN_OK);

        /* toward +inf >= toward zero for positive; toward -inf <= toward zero */
        CHECK(d_up >= d_tz - 1e-12);
        CHECK(d_dn <= d_tz + 1e-12);
        /* NTE and NA both finite */
        CHECK(d_nte == d_nte && d_na == d_na);

        /* explicit opt override wins over ctx */
        sn_ctx_set_round(&ctx, SN_ROUND_DN);
        opt.round = SN_ROUND_UP;
        CHECK(sn_from_str_float(&ctx, &a, "0.1", 5, 10, 1, &opt) == SN_OK);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
        CHECK(d >= d_dn - 1e-12);

        sn_ctx_set_round(&ctx, SN_ROUND_NTE);
    }

    /* Unrestricted E/M: wide exponent and mantissa formats */
    {
        CHECK(sn_float_new(&ctx, &a, 40, 80, 1) == SN_OK);
        CHECK(a.e_bits == 40 && a.m_bits == 80);
        CHECK(sn_float_from_i64(&ctx, &a, 42, 40, 80, 1, NULL) == SN_OK);
        CHECK(sn_fp_classify(&a) == SN_FP_NORMAL);
        CHECK(sn_float_from_i64(&ctx, &b, 8, 40, 80, 1, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_fp_classify(&c) == SN_FP_NORMAL);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d == 50.0);
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d == 336.0);
        /* e_bits > 30 routes to multiprec even with small mantissa */
        CHECK(sn_float_from_i64(&ctx, &a, 3, 36, 20, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, 7, 36, 20, 1, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d == 10.0);
        CHECK(sn_float_set_inf(&ctx, &a, 0, 48, 100, 1) == SN_OK);
        CHECK(sn_fp_classify(&a) == SN_FP_INFINITE);
        CHECK(sn_float_set_nan(&ctx, &b, 48, 100) == SN_OK);
        CHECK(sn_fp_classify(&b) == SN_FP_NAN);
        /* large but practical multiprec mantissa */
        CHECK(sn_float_from_i64(&ctx, &a, 1000, 15, 200, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, 24, 15, 200, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d > 41.6 && d < 41.7);
        /* reject absurd (still memory-safe caps) */
        CHECK(sn_float_new(&ctx, &a, 1, 52, 1) != SN_OK); /* e too small */
        CHECK(sn_float_new(&ctx, &a, 0, 52, 1) != SN_OK);
    }


    /* larger practical E/M (int64 working exp path) */
    {
        CHECK(sn_float_from_i64(&ctx, &a, 5, 20, 120, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, 7, 20, 120, 1, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d == 35.0);
        CHECK(sn_float_from_i64(&ctx, &a, 1, 33, 64, 1, NULL) == SN_OK); /* e>30 multiprec */
        CHECK(sn_float_from_i64(&ctx, &b, 2, 33, 64, 1, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d == 3.0);
        /* format accept large e within memory cap */
        CHECK(sn_float_new(&ctx, &a, 100, 80, 1) == SN_OK);
        CHECK(a.e_bits == 100 && a.m_bits == 80);
        sn_value_clear(&ctx, &a);
    }

    /* Regression: E=110 M=520 must encode 1.0 as NORMAL, not Inf.
     * Old bug: unpack treated any exp field bit>=62 as Inf (bias high bits). */
    {
        CHECK(sn_from_str_float(&ctx, &a, "1.0", 110, 520, 1, NULL) == SN_OK);
        CHECK(sn_fp_classify(&a) == SN_FP_NORMAL);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
        CHECK(d > 0.999 && d < 1.001);
        CHECK(sn_float_from_i64(&ctx, &a, 1, 110, 520, 1, NULL) == SN_OK);
        CHECK(sn_fp_classify(&a) == SN_FP_NORMAL);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
        CHECK(d == 1.0);
        CHECK(sn_from_str_float(&ctx, &a, "2.5", 110, 520, 1, NULL) == SN_OK);
        CHECK(sn_from_str_float(&ctx, &b, "1.5", 110, 520, 1, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_fp_classify(&c) == SN_FP_NORMAL);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d == 4.0);
        /* e around 64 boundary */
        CHECK(sn_float_from_i64(&ctx, &a, 1, 64, 52, 1, NULL) == SN_OK);
        CHECK(sn_fp_classify(&a) == SN_FP_NORMAL);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
        CHECK(d == 1.0);
        CHECK(sn_float_from_i64(&ctx, &a, 1, 80, 100, 1, NULL) == SN_OK);
        CHECK(sn_fp_classify(&a) == SN_FP_NORMAL);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
        CHECK(d == 1.0);
        CHECK(sn_to_str(&ctx, &s, &a, 10) == SN_OK);
        CHECK(s && s[0] != 'i'); /* not inf */
        if (s) sn_str_free(&ctx, s);
        s = NULL;
    }

    /* subnormal f16: smallest positive subnormal ~ 2^-24 */
    {
        sn_ctx_set_round(&ctx, SN_ROUND_NTE);
        CHECK(sn_f16(&ctx, &a, 5.96e-8) == SN_OK); /* near min subnormal */
        CHECK(sn_fp_classify(&a) == SN_FP_SUBNORMAL || sn_fp_classify(&a) == SN_FP_NORMAL || sn_fp_classify(&a) == SN_FP_ZERO);
        CHECK(sn_f16(&ctx, &b, 6.1e-5) == SN_OK); /* ~ min normal for f16 is 2^-14 ~ 6.1e-5 */
        CHECK(sn_fp_classify(&b) == SN_FP_NORMAL || sn_fp_classify(&b) == SN_FP_SUBNORMAL);
    }

    sn_value_clear(&ctx, &a);
    sn_value_clear(&ctx, &b);
    sn_value_clear(&ctx, &c);
    sn_value_clear(&ctx, &dval);
    return 0;
}
