#include "sn.h"
#include "sn_flat.h"
#include "libbf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static void *my_realloc(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    return realloc(ptr, size);
}

static int sn_set_hex_mp(sn_ctx *ctx, sn_value *v, double d, int e_bits, int m_bits)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%a", d);
    return sn_from_str_float(ctx, v, buf, e_bits, m_bits, 1, NULL) == SN_OK;
}

static int almost_equal(double sn_d, double bf_d, int m_bits, int tight)
{
    double rel, tol, abs_tol;
    int slack = tight ? 6 : 10;
    int gate = m_bits > 53 ? 53 : m_bits;
    if ((isnan(sn_d) && isnan(bf_d))) return 1;
    if (isinf(sn_d) && isinf(bf_d) && signbit(sn_d) == signbit(bf_d)) return 1;
    if (!isfinite(sn_d) || !isfinite(bf_d)) return 0;
    if (bf_d == 0.0) return fabs(sn_d) <= ldexp(1.0, -(gate - 4));
    rel = fabs(sn_d - bf_d) / fabs(bf_d);
    tol = ldexp(1.0, -(gate - slack));
    if (tol < 1e-15) tol = 1e-15;
    abs_tol = ldexp(1.0, -gate + (tight ? 2 : 4)) * fmax(1.0, fabs(bf_d));
    return !(rel > tol && fabs(sn_d - bf_d) > abs_tol);
}

/* Relative gate using working-precision residual |sn-bf|/|bf| via double export.
 * For multiprec, also allow up to ~2^-(min(m,53)-slack) ULP-scale on host double. */
static int almost_equal_rel(double sn_d, double ref, int m_bits, int slack)
{
    double rel, tol;
    int gate = m_bits > 53 ? 53 : m_bits;
    if ((isnan(sn_d) && isnan(ref))) return 1;
    if (isinf(sn_d) && isinf(ref) && signbit(sn_d) == signbit(ref)) return 1;
    if (!isfinite(sn_d) || !isfinite(ref)) return 0;
    if (ref == 0.0) return fabs(sn_d) <= ldexp(1.0, -(gate / 2));
    rel = fabs(sn_d - ref) / fabs(ref);
    tol = ldexp(1.0, -(gate - slack));
    if (tol < 1e-14) tol = 1e-14;
    return rel <= tol || fabs(sn_d - ref) < 1e-30;
}

static int check_unary(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                       sn_status (*sn_op)(sn_ctx*, sn_value*, const sn_value*, const sn_op_opt*),
                       int (*bf_op)(bf_t*, const bf_t*, limb_t, bf_flags_t),
                       double x, int e_bits, int m_bits, int tight,
                       int *tests, int *fails)
{
    sn_value a, out;
    bf_t ba, br;
    double sn_d, bf_d;
    limb_t prec = (limb_t)m_bits + 32;
    bf_flags_t fl = BF_RNDN;

    (*tests)++;
    sn_value_init(&a); sn_value_init(&out);
    bf_init(bfc, &ba); bf_init(bfc, &br);

    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        sn_op(ctx, &out, &a, NULL) != SN_OK ||
        sn_to_double(ctx, &out, &sn_d) != SN_OK) {
        printf("%s sn fail x=%a\n", name, x);
        (*fails)++;
        goto done;
    }
    bf_set_float64(&ba, x);
    bf_op(&br, &ba, prec, fl);
    bf_round(&br, (limb_t)m_bits, fl);
    bf_get_float64(&br, &bf_d, BF_RNDN);

    if (!almost_equal(sn_d, bf_d, m_bits, tight)) {
        printf("%s fail x=%a sn=%a bf=%a\n", name, x, sn_d, bf_d);
        (*fails)++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &out);
    bf_delete(&ba); bf_delete(&br);
    return 0;
}

static int check_binary(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                        sn_status (*sn_op)(sn_ctx*, sn_value*, const sn_value*, const sn_value*, const sn_op_opt*),
                        int (*bf_op)(bf_t*, const bf_t*, const bf_t*, limb_t, bf_flags_t),
                        double x, double y, int e_bits, int m_bits, int tight,
                        int *tests, int *fails)
{
    sn_value a, b, out;
    bf_t ba, bb, br;
    double sn_d, bf_d;
    limb_t prec = (limb_t)m_bits + 32;
    bf_flags_t fl = BF_RNDN;

    (*tests)++;
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
    bf_init(bfc, &ba); bf_init(bfc, &bb); bf_init(bfc, &br);

    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &b, y, e_bits, m_bits) ||
        sn_op(ctx, &out, &a, &b, NULL) != SN_OK ||
        sn_to_double(ctx, &out, &sn_d) != SN_OK) {
        printf("%s sn fail x=%a y=%a\n", name, x, y);
        (*fails)++;
        goto done;
    }
    bf_set_float64(&ba, x);
    bf_set_float64(&bb, y);
    bf_op(&br, &ba, &bb, prec, fl);
    bf_round(&br, (limb_t)m_bits, fl);
    bf_get_float64(&br, &bf_d, BF_RNDN);
    if (!almost_equal(sn_d, bf_d, m_bits, tight)) {
        printf("%s fail x=%a y=%a sn=%a bf=%a\n", name, x, y, sn_d, bf_d);
        (*fails)++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b); sn_value_clear(ctx, &out);
    bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
    return 0;
}

static int check_unary_host(const char *name, sn_ctx *ctx,
                            sn_status (*sn_op)(sn_ctx*, sn_value*, const sn_value*, const sn_op_opt*),
                            double (*host_op)(double),
                            double x, int e_bits, int m_bits, int *tests, int *fails)
{
    sn_value a, out;
    double sn_d, hf;
    (*tests)++;
    sn_value_init(&a); sn_value_init(&out);
    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        sn_op(ctx, &out, &a, NULL) != SN_OK ||
        sn_to_double(ctx, &out, &sn_d) != SN_OK) {
        printf("%s sn fail x=%a\n", name, x);
        (*fails)++;
        goto done;
    }
    hf = host_op(x);
    if (!almost_equal(sn_d, hf, m_bits, 1)) {
        double tol = ldexp(1.0, -48);
        if (!(isfinite(sn_d) && isfinite(hf) &&
              (fabs(sn_d - hf) <= tol * fmax(1.0, fabs(hf)) ||
               fabs(sn_d - hf) < 1e-12))) {
            printf("%s host-ref fail x=%a sn=%a host=%a\n", name, x, sn_d, hf);
            (*fails)++;
        }
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &out);
    return 0;
}

/* Identity: exp(log(x)) ~ x for x>0; log(exp(x)) ~ x for moderate x;
 * sin^2+cos^2 ~ 1; pow(x,2) ~ x*x for x>0. */
static int check_identities(sn_ctx *ctx, int e_bits, int m_bits, int *tests, int *fails)
{
    static const double xs[] = {
        0.1, 0.5, 1.0, 1.5, 2.0, 0.01, 10.0, 0.25, 3.0, 0.9, 1.1, 7.0, 0.3333333333333333
    };
    static const double txs[] = {
        0.0, 0.1, -0.1, 0.5, -0.5, 1.0, -1.0, 1.5, -2.0, 3.141592653589793, 0.7853981633974483
    };
    int i;
    for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
        sn_value a, t, u;
        double xd = xs[i], sn_d;
        sn_value_init(&a); sn_value_init(&t); sn_value_init(&u);
        (*tests)++;
        if (!sn_set_hex_mp(ctx, &a, xd, e_bits, m_bits) ||
            sn_log(ctx, &t, &a, NULL) != SN_OK ||
            sn_exp(ctx, &u, &t, NULL) != SN_OK ||
            sn_to_double(ctx, &u, &sn_d) != SN_OK ||
            !almost_equal_rel(sn_d, xd, m_bits, 8)) {
            printf("id exp(log) fail x=%a sn=%a\n", xd, sn_d);
            (*fails)++;
        }
        sn_value_clear(ctx, &a); sn_value_clear(ctx, &t); sn_value_clear(ctx, &u);
    }
    for (i = 0; i < (int)(sizeof(txs)/sizeof(txs[0])); i++) {
        sn_value a, s, c, s2, c2, one;
        double xd = txs[i], sn_d;
        if (fabs(xd) > 20.0) continue;
        sn_value_init(&a); sn_value_init(&s); sn_value_init(&c);
        sn_value_init(&s2); sn_value_init(&c2); sn_value_init(&one);
        (*tests)++;
        if (!sn_set_hex_mp(ctx, &a, xd, e_bits, m_bits) ||
            sn_sin(ctx, &s, &a, NULL) != SN_OK ||
            sn_cos(ctx, &c, &a, NULL) != SN_OK ||
            sn_mul(ctx, &s2, &s, &s, NULL) != SN_OK ||
            sn_mul(ctx, &c2, &c, &c, NULL) != SN_OK ||
            sn_add(ctx, &one, &s2, &c2, NULL) != SN_OK ||
            sn_to_double(ctx, &one, &sn_d) != SN_OK ||
            !almost_equal_rel(sn_d, 1.0, m_bits, 10)) {
            printf("id sin2+cos2 fail x=%a sn=%a\n", xd, sn_d);
            (*fails)++;
        }
        sn_value_clear(ctx, &a); sn_value_clear(ctx, &s); sn_value_clear(ctx, &c);
        sn_value_clear(ctx, &s2); sn_value_clear(ctx, &c2); sn_value_clear(ctx, &one);
    }
    /* log(exp(x)) for |x| small enough not to overflow */
    {
        static const double exs[] = { 0.0, 0.5, -0.5, 1.0, -1.0, 2.0, -2.0, 0.1, -0.25, 3.0 };
        for (i = 0; i < (int)(sizeof(exs)/sizeof(exs[0])); i++) {
            sn_value a, t, u;
            double xd = exs[i], sn_d;
            sn_value_init(&a); sn_value_init(&t); sn_value_init(&u);
            (*tests)++;
            if (!sn_set_hex_mp(ctx, &a, xd, e_bits, m_bits) ||
                sn_exp(ctx, &t, &a, NULL) != SN_OK ||
                sn_log(ctx, &u, &t, NULL) != SN_OK ||
                sn_to_double(ctx, &u, &sn_d) != SN_OK ||
                !almost_equal_rel(sn_d, xd, m_bits, 8)) {
                printf("id log(exp) fail x=%a sn=%a\n", xd, sn_d);
                (*fails)++;
            }
            sn_value_clear(ctx, &a); sn_value_clear(ctx, &t); sn_value_clear(ctx, &u);
        }
    }
    return 0;
}

int main(void)
{
    sn_ctx ctx;
    bf_context_t bfc;
    int tests = 0, fails = 0;
    int e_bits, m_bits, i, j;
    static const double xs[] = {
        0.0, 0.1, -0.1, 0.5, 1.0, 1.5, 2.0, -0.5,
        0.01, 0.001, 3.141592653589793, 0.7853981633974483,
        10.0, -2.0, 0.25, 1e-6, 20.0, -0.25, 1e-8, 1e-12,
        0.9, -0.9, 1.1, 5.0, -5.0, 0.3333333333333333,
        1e-3, -1e-3, 1e-4, 7.0, -3.5, 12.5,
        0.7071067811865476, -0.7071067811865476, 1e-15, 30.0
    };
    static const double asin_xs[] = {
        0.0, 0.1, -0.1, 0.5, -0.5, 0.9, -0.9, 1.0, -1.0,
        0.01, -0.01, 0.7071067811865476, -0.7071067811865476
    };
    static const double pow_bases[] = { 0.5, 1.0, 2.0, 10.0, 0.1, 1.5, 3.0, -2.0, -0.5 };
    static const double pow_exps[]  = { 0.0, 1.0, 2.0, 0.5, -1.0, 3.0, 0.25, -0.5, 4.0, -3.0 };

    sn_ctx_init(&ctx);
    sn_ctx_set_round(&ctx, SN_ROUND_NTE);
    bf_context_init(&bfc, my_realloc, NULL);

    e_bits = 15; m_bits = 80;
    for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
        double x = xs[i];
        if (x > -700 && x < 700)
            check_unary("exp", &ctx, &bfc, sn_exp, bf_exp, x, e_bits, m_bits, 1, &tests, &fails);
        if (x > 0)
            check_unary("log", &ctx, &bfc, sn_log, bf_log, x, e_bits, m_bits, 1, &tests, &fails);
        check_unary("sin", &ctx, &bfc, sn_sin, bf_sin, x, e_bits, m_bits, 1, &tests, &fails);
        check_unary("cos", &ctx, &bfc, sn_cos, bf_cos, x, e_bits, m_bits, 1, &tests, &fails);
        if (fabs(x) < 1.4)
            check_unary("tan", &ctx, &bfc, sn_tan, bf_tan, x, e_bits, m_bits, 1, &tests, &fails);
        check_unary("atan", &ctx, &bfc, sn_atan, bf_atan, x, e_bits, m_bits, 1, &tests, &fails);
        if (fails > 80) break;
    }
    for (i = 0; i < (int)(sizeof(asin_xs)/sizeof(asin_xs[0])); i++) {
        check_unary("asin", &ctx, &bfc, sn_asin, bf_asin, asin_xs[i], e_bits, m_bits, 1, &tests, &fails);
        check_unary("acos", &ctx, &bfc, sn_acos, bf_acos, asin_xs[i], e_bits, m_bits, 1, &tests, &fails);
    }
    for (i = 0; i < (int)(sizeof(pow_bases)/sizeof(pow_bases[0])); i++) {
        for (j = 0; j < (int)(sizeof(pow_exps)/sizeof(pow_exps[0])); j++) {
            double base = pow_bases[i], expv = pow_exps[j];
            if (base < 0.0) {
                double intpart;
                if (modf(expv, &intpart) != 0.0 || expv < -1e9 || expv > 1e9)
                    continue;
                {
                    sn_value a, b, out;
                    double sn_d, hf;
                    tests++;
                    sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                    if (sn_set_hex_mp(&ctx, &a, base, e_bits, m_bits) &&
                        sn_set_hex_mp(&ctx, &b, expv, e_bits, m_bits) &&
                        sn_pow(&ctx, &out, &a, &b, NULL) == SN_OK &&
                        sn_to_double(&ctx, &out, &sn_d) == SN_OK) {
                        hf = pow(base, expv);
                        if (!almost_equal(sn_d, hf, 53, 1)) {
                            printf("powneg fail %a^%a sn=%a host=%a\n", base, expv, sn_d, hf);
                            fails++;
                        }
                    } else {
                        printf("powneg sn fail %a^%a\n", base, expv);
                        fails++;
                    }
                    sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                    continue;
                }
            }
            check_binary("pow", &ctx, &bfc, sn_pow, bf_pow,
                         base, expv, e_bits, m_bits, 1, &tests, &fails);
            if (fails > 80) break;
        }
        if (fails > 80) break;
    }
    {
        static const double smalls[] = {
            0.0, 1e-16, -1e-16, 1e-12, -1e-12, 1e-8, -1e-8,
            1e-4, -1e-4, 0.01, -0.01, 0.1, -0.1, 0.3, -0.3, 0.5, -0.5
        };
        for (i = 0; i < (int)(sizeof(smalls)/sizeof(smalls[0])); i++) {
            check_unary_host("expm1", &ctx, sn_expm1, expm1, smalls[i], e_bits, m_bits, &tests, &fails);
            if (smalls[i] > -1.0)
                check_unary_host("log1p", &ctx, sn_log1p, log1p, smalls[i], e_bits, m_bits, &tests, &fails);
        }
    }
    check_identities(&ctx, e_bits, m_bits, &tests, &fails);

    /* forced multiprec soft at m=52 */
    e_bits = 31; m_bits = 52;
    for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
        double x = xs[i];
        if (x > -700 && x < 700)
            check_unary("exp52", &ctx, &bfc, sn_exp, bf_exp, x, e_bits, m_bits, 1, &tests, &fails);
        if (x > 0)
            check_unary("log52", &ctx, &bfc, sn_log, bf_log, x, e_bits, m_bits, 1, &tests, &fails);
        check_unary("sin52", &ctx, &bfc, sn_sin, bf_sin, x, e_bits, m_bits, 1, &tests, &fails);
        check_unary("atan52", &ctx, &bfc, sn_atan, bf_atan, x, e_bits, m_bits, 1, &tests, &fails);
        if (fails > 100) break;
    }
    for (i = 0; i < (int)(sizeof(pow_bases)/sizeof(pow_bases[0])); i++) {
        if (pow_bases[i] < 0) continue;
        for (j = 0; j < 5; j++) {
            check_binary("pow52", &ctx, &bfc, sn_pow, bf_pow,
                         pow_bases[i], pow_exps[j], e_bits, m_bits, 1, &tests, &fails);
        }
    }
    check_identities(&ctx, e_bits, m_bits, &tests, &fails);

    /* m=120 smoke + identities */
    e_bits = 15; m_bits = 120;
    {
        static const double hx[] = { 0.5, 1.0, 2.0, 0.1, -0.5, 3.0, 1e-6, 0.9 };
        for (i = 0; i < (int)(sizeof(hx)/sizeof(hx[0])); i++) {
            double x = hx[i];
            if (x > -700 && x < 700)
                check_unary("exp120", &ctx, &bfc, sn_exp, bf_exp, x, e_bits, m_bits, 1, &tests, &fails);
            if (x > 0)
                check_unary("log120", &ctx, &bfc, sn_log, bf_log, x, e_bits, m_bits, 1, &tests, &fails);
            check_unary("sin120", &ctx, &bfc, sn_sin, bf_sin, x, e_bits, m_bits, 1, &tests, &fails);
            check_unary("atan120", &ctx, &bfc, sn_atan, bf_atan, x, e_bits, m_bits, 1, &tests, &fails);
        }
        check_binary("pow120", &ctx, &bfc, sn_pow, bf_pow, 2.0, 0.5, e_bits, m_bits, 1, &tests, &fails);
        check_binary("pow120", &ctx, &bfc, sn_pow, bf_pow, 10.0, 3.0, e_bits, m_bits, 1, &tests, &fails);
        check_binary("pow120", &ctx, &bfc, sn_pow, bf_pow, 10.0, 0.25, e_bits, m_bits, 1, &tests, &fails);
    }
    check_identities(&ctx, e_bits, m_bits, &tests, &fails);

        /* hyperbolic vs host (libbf has no sinh/cosh/tanh) */
    e_bits = 15; m_bits = 80;
    {
        static const double hx[] = { 0.0, 0.1, -0.1, 0.5, 1.0, -1.0, 2.0, -0.5 };
        for (i = 0; i < (int)(sizeof(hx)/sizeof(hx[0])); i++) {
            check_unary_host("sinh", &ctx, sn_sinh, sinh, hx[i], e_bits, m_bits, &tests, &fails);
            check_unary_host("cosh", &ctx, sn_cosh, cosh, hx[i], e_bits, m_bits, &tests, &fails);
            check_unary_host("tanh", &ctx, sn_tanh, tanh, hx[i], e_bits, m_bits, &tests, &fails);
        }
    }

    /* Large-argument sin/cos vs libbf (argument reduction stress) */
    e_bits = 15; m_bits = 80;
    {
        static const double large_xs[] = {
            100.0, 1000.0, 1e4, 1e5, 1e6, 1e7, 1e8, 1e10, 1e12, 1e15,
            -1e6, -1e10, 1234567.89, -987654.321,
            6.283185307179586e9
        };
        for (i = 0; i < (int)(sizeof(large_xs)/sizeof(large_xs[0])); i++) {
            check_unary("sin_large", &ctx, &bfc, sn_sin, bf_sin,
                        large_xs[i], e_bits, m_bits, 0, &tests, &fails);
            check_unary("cos_large", &ctx, &bfc, sn_cos, bf_cos,
                        large_xs[i], e_bits, m_bits, 0, &tests, &fails);
        }
    }

    /* m=400: force AGM log path (SN_SOFT_LOG_AGM_MIN_M default 240) */
    e_bits = 15; m_bits = 400;
    {
        static const double hx[] = { 0.5, 1.0, 2.0, 0.1, 3.0, 10.0, 0.9, 1.5, 1e-6, 1e3 };
        for (i = 0; i < (int)(sizeof(hx)/sizeof(hx[0])); i++) {
            double x = hx[i];
            if (x > 0)
                check_unary("log400", &ctx, &bfc, sn_log, bf_log, x, e_bits, m_bits, 1, &tests, &fails);
            if (x > -700 && x < 700)
                check_unary("exp400", &ctx, &bfc, sn_exp, bf_exp, x, e_bits, m_bits, 1, &tests, &fails);
        }
        check_identities(&ctx, e_bits, m_bits, &tests, &fails);
    }


    /* Host-gated soft specials: cbrt / erf / erfc / tgamma (libbf has no erf/tgamma). */
    {
        static const double cx[] = {
            0.0, 1.0, 8.0, 27.0, -8.0, 0.125, 2.0, 10.0, 0.001, 1000.0, -0.001, 0.5, -27.0
        };
        static const double ex[] = {
            0.0, 0.1, -0.1, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.0, -2.0, 0.01, -0.01, 3.0
        };
        static const double gx[] = {
            0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 6.0, 0.1, 0.25, 7.0, 0.75, 1.25
        };
        int mi, ei;
        static const int mm[] = { 80, 112 };
        for (mi = 0; mi < (int)(sizeof(mm)/sizeof(mm[0])); mi++) {
            e_bits = 15; m_bits = mm[mi];
            for (ei = 0; ei < (int)(sizeof(cx)/sizeof(cx[0])); ei++)
                check_unary_host("cbrt", &ctx, sn_cbrt, cbrt, cx[ei], e_bits, m_bits, &tests, &fails);
            for (ei = 0; ei < (int)(sizeof(ex)/sizeof(ex[0])); ei++) {
                check_unary_host("erf", &ctx, sn_erf, erf, ex[ei], e_bits, m_bits, &tests, &fails);
                check_unary_host("erfc", &ctx, sn_erfc, erfc, ex[ei], e_bits, m_bits, &tests, &fails);
            }
            for (ei = 0; ei < (int)(sizeof(gx)/sizeof(gx[0])); ei++)
                check_unary_host("tgamma", &ctx, sn_tgamma, tgamma, gx[ei], e_bits, m_bits, &tests, &fails);
            {
                static const double bx[] = {
                    0.1, 0.5, 1.0, 1.5, 2.0, 3.0, 5.0, 0.01, 8.0, 0.25, 4.0, 10.0
                };
                for (ei = 0; ei < (int)(sizeof(bx)/sizeof(bx[0])); ei++) {
                    check_unary_host("j0", &ctx, sn_j0, j0, bx[ei], e_bits, m_bits, &tests, &fails);
                    check_unary_host("j1", &ctx, sn_j1, j1, bx[ei], e_bits, m_bits, &tests, &fails);
                    if (bx[ei] > 0.0)
                        check_unary_host("y0", &ctx, sn_y0, y0, bx[ei], e_bits, m_bits, &tests, &fails);
                }
            }

            /* identities: erf odd; erf+erfc=1; factorial gamma for small n */
            {
                sn_value a, b, c, one, neg, t;
                double xd, sn_d;
                int k;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&c);
                sn_value_init(&one); sn_value_init(&neg); sn_value_init(&t);
                for (k = 0; k < 5; k++) {
                    xd = 0.2 + 0.3 * (double)k;
                    tests++;
                    if (!sn_set_hex_mp(&ctx, &a, xd, e_bits, m_bits) ||
                        sn_erf(&ctx, &b, &a, NULL) != SN_OK ||
                        sn_neg(&ctx, &neg, &a, NULL) != SN_OK ||
                        sn_erf(&ctx, &c, &neg, NULL) != SN_OK ||
                        sn_add(&ctx, &t, &b, &c, NULL) != SN_OK ||
                        sn_to_double(&ctx, &t, &sn_d) != SN_OK ||
                        fabs(sn_d) > 1e-12) {
                        printf("id erf-odd fail x=%a sn_sum=%a m=%d\n", xd, sn_d, m_bits);
                        fails++;
                    }
                    tests++;
                    if (sn_erfc(&ctx, &c, &a, NULL) != SN_OK ||
                        sn_add(&ctx, &t, &b, &c, NULL) != SN_OK ||
                        sn_to_double(&ctx, &t, &sn_d) != SN_OK ||
                        fabs(sn_d - 1.0) > 1e-12) {
                        printf("id erf+erfc fail x=%a sn=%a m=%d\n", xd, sn_d, m_bits);
                        fails++;
                    }
                }
                {
                    double fact = 1.0;
                    for (k = 1; k <= 8; k++) {
                        fact *= (double)k;
                        tests++;
                        if (!sn_set_hex_mp(&ctx, &a, (double)(k + 1), e_bits, m_bits) ||
                            sn_tgamma(&ctx, &b, &a, NULL) != SN_OK ||
                            sn_to_double(&ctx, &b, &sn_d) != SN_OK) {
                            printf("id gamma-fact sn fail n=%d m=%d\n", k, m_bits);
                            fails++;
                        } else if (fabs(sn_d - fact) > 1e-9 * fmax(1.0, fact)) {
                            printf("id gamma-fact fail n=%d sn=%a ref=%a m=%d\n", k, sn_d, fact, m_bits);
                            fails++;
                        }
                    }
                }
                for (ei = 0; ei < (int)(sizeof(cx)/sizeof(cx[0])); ei++) {
                    xd = cx[ei];
                    if (xd == 0.0) continue;
                    tests++;
                    if (!sn_set_hex_mp(&ctx, &a, xd, e_bits, m_bits) ||
                        sn_cbrt(&ctx, &b, &a, NULL) != SN_OK ||
                        sn_mul(&ctx, &c, &b, &b, NULL) != SN_OK ||
                        sn_mul(&ctx, &t, &c, &b, NULL) != SN_OK ||
                        sn_to_double(&ctx, &t, &sn_d) != SN_OK) {
                        printf("id cbrt3 sn fail x=%a m=%d\n", xd, m_bits);
                        fails++;
                    } else if (fabs(sn_d - xd) > 1e-10 * fmax(1.0, fabs(xd))) {
                        printf("id cbrt3 fail x=%a sn=%a m=%d\n", xd, sn_d, m_bits);
                        fails++;
                    }
                }
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &c);
                sn_value_clear(&ctx, &one); sn_value_clear(&ctx, &neg); sn_value_clear(&ctx, &t);
            }
            if (fails > 120) break;
        }
    }

    /* Extra formats: cos/tan/asin/acos already covered at m=80; add m=112 trig + inv */
    e_bits = 15; m_bits = 112;
    for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
        double x = xs[i];
        if (fabs(x) > 50.0) continue;
        check_unary("sin112", &ctx, &bfc, sn_sin, bf_sin, x, e_bits, m_bits, 1, &tests, &fails);
        check_unary("cos112", &ctx, &bfc, sn_cos, bf_cos, x, e_bits, m_bits, 1, &tests, &fails);
        if (fabs(x) < 1.4)
            check_unary("tan112", &ctx, &bfc, sn_tan, bf_tan, x, e_bits, m_bits, 1, &tests, &fails);
        check_unary("atan112", &ctx, &bfc, sn_atan, bf_atan, x, e_bits, m_bits, 1, &tests, &fails);
        if (fails > 120) break;
    }
    for (i = 0; i < (int)(sizeof(asin_xs)/sizeof(asin_xs[0])); i++) {
        check_unary("asin112", &ctx, &bfc, sn_asin, bf_asin, asin_xs[i], e_bits, m_bits, 1, &tests, &fails);
        check_unary("acos112", &ctx, &bfc, sn_acos, bf_acos, asin_xs[i], e_bits, m_bits, 1, &tests, &fails);
    }

    sn_ctx_fini(&ctx);

    bf_context_end(&bfc);
    printf("libbf soft math probe: tests=%d fails=%d\n", tests, fails);
    return fails ? 1 : 0;
}
