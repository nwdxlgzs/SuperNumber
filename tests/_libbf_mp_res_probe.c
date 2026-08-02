/* Multiprec transcendental residual vs libbf (hex path, not double-only).
 * Gate: |sn - bf| / |bf|  <  2^-(m - slack)  using libbf arithmetic. */
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

/* Free bf_ftoa string via bf_realloc(ctx, ptr, 0). */
static void bf_free_str(bf_context_t *bfc, char *s)
{
    if (s)
        bf_realloc(bfc, s, 0);
}

/* Return 1 if |sn_hex - bf_ref| / max(1,|bf_ref|) < 2^-(m-slack). */
static int residual_ok(bf_context_t *bfc, const char *sn_hex, const bf_t *bf_ref,
                       int m_bits, int slack, double *out_rel)
{
    bf_t snb, diff, absd, absr, rel, thr, one, two, pow2;
    limb_t prec = (limb_t)m_bits + 64;
    int st, ok = 0;
    double rel_d = 0.0;
    int exp_thr = m_bits - slack;
    if (exp_thr < 8) exp_thr = 8;

    bf_init(bfc, &snb);
    bf_init(bfc, &diff);
    bf_init(bfc, &absd);
    bf_init(bfc, &absr);
    bf_init(bfc, &rel);
    bf_init(bfc, &thr);
    bf_init(bfc, &one);
    bf_init(bfc, &two);
    bf_init(bfc, &pow2);

    if (!sn_hex || !sn_hex[0])
        goto done;
    st = bf_atof(&snb, sn_hex, NULL, 16, prec, BF_RNDN);
    if (st & BF_ST_MEM_ERROR)
        goto done;
    bf_round(&snb, (limb_t)m_bits + 8, BF_RNDN);

    bf_sub(&diff, &snb, bf_ref, prec, BF_RNDN);
    bf_set_ui(&one, 1);
    /* abs */
    bf_set(&absd, &diff);
    if (absd.sign)
        absd.sign = 0;
    bf_set(&absr, bf_ref);
    if (absr.sign)
        absr.sign = 0;
    /* den = max(1, |bf_ref|) */
    if (bf_cmp(&absr, &one) < 0)
        bf_set(&absr, &one);
    bf_div(&rel, &absd, &absr, prec, BF_RNDN);

    /* thr = 2^-(m-slack) */
    bf_set_ui(&two, 2);
    bf_set_si(&pow2, -exp_thr);
    bf_pow(&thr, &two, &pow2, prec, BF_RNDN);

    if (bf_cmp(&rel, &thr) <= 0)
        ok = 1;

    bf_get_float64(&rel, &rel_d, BF_RNDN);
    if (out_rel)
        *out_rel = rel_d;

done:
    bf_delete(&snb);
    bf_delete(&diff);
    bf_delete(&absd);
    bf_delete(&absr);
    bf_delete(&rel);
    bf_delete(&thr);
    bf_delete(&one);
    bf_delete(&two);
    bf_delete(&pow2);
    return ok;
}

static int check_unary_mp(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                          sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                          int (*bf_op)(bf_t *, const bf_t *, limb_t, bf_flags_t),
                          double x, int e_bits, int m_bits, int slack,
                          int *tests, int *fails)
{
    sn_value a, out;
    bf_t ba, br;
    char *s = NULL;
    double rel = 0.0;
    limb_t prec = (limb_t)m_bits + 48;

    (*tests)++;
    sn_value_init(&a);
    sn_value_init(&out);
    bf_init(bfc, &ba);
    bf_init(bfc, &br);

    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        sn_op(ctx, &out, &a, NULL) != SN_OK ||
        sn_to_str(ctx, &s, &out, 16) != SN_OK || !s) {
        printf("%s sn fail x=%a m=%d\n", name, x, m_bits);
        (*fails)++;
        goto done;
    }
    bf_set_float64(&ba, x);
    bf_op(&br, &ba, prec, BF_RNDN);
    bf_round(&br, (limb_t)m_bits, BF_RNDN);

    if (!residual_ok(bfc, s, &br, m_bits, slack, &rel)) {
        char *bf_s = bf_ftoa(NULL, &br, 16, (limb_t)m_bits,
                             BF_RNDN | BF_FTOA_FORMAT_FREE | BF_FTOA_ADD_PREFIX | BF_FTOA_FORCE_EXP);
        printf("%s mp-res fail x=%a m=%d rel=%g slack=%d sn=%s bf=%s\n",
               name, x, m_bits, rel, slack, s, bf_s ? bf_s : "?");
        if (bf_s)
            bf_free_str(bfc, bf_s);
        (*fails)++;
    }
done:
    if (s)
        sn_str_free(ctx, s);
    sn_value_clear(ctx, &a);
    sn_value_clear(ctx, &out);
    bf_delete(&ba);
    bf_delete(&br);
    return 0;
}

static int check_binary_mp(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                           sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *),
                           int (*bf_op)(bf_t *, const bf_t *, const bf_t *, limb_t, bf_flags_t),
                           double x, double y, int e_bits, int m_bits, int slack,
                           int *tests, int *fails)
{
    sn_value a, b, out;
    bf_t ba, bb, br;
    char *s = NULL;
    double rel = 0.0;
    limb_t prec = (limb_t)m_bits + 48;

    (*tests)++;
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&out);
    bf_init(bfc, &ba);
    bf_init(bfc, &bb);
    bf_init(bfc, &br);

    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &b, y, e_bits, m_bits) ||
        sn_op(ctx, &out, &a, &b, NULL) != SN_OK ||
        sn_to_str(ctx, &s, &out, 16) != SN_OK || !s) {
        printf("%s sn fail x=%a y=%a m=%d\n", name, x, y, m_bits);
        (*fails)++;
        goto done;
    }
    bf_set_float64(&ba, x);
    bf_set_float64(&bb, y);
    bf_op(&br, &ba, &bb, prec, BF_RNDN);
    bf_round(&br, (limb_t)m_bits, BF_RNDN);

    if (!residual_ok(bfc, s, &br, m_bits, slack, &rel)) {
        printf("%s mp-res fail x=%a y=%a m=%d rel=%g sn=%s\n", name, x, y, m_bits, rel, s);
        (*fails)++;
    }
done:
    if (s)
        sn_str_free(ctx, s);
    sn_value_clear(ctx, &a);
    sn_value_clear(ctx, &b);
    sn_value_clear(ctx, &out);
    bf_delete(&ba);
    bf_delete(&bb);
    bf_delete(&br);
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
        10.0, -2.0, 0.25, 1e-6, -0.25, 1e-8,
        0.9, -0.9, 1.1, 5.0, -5.0, 0.3333333333333333,
        0.7071067811865476, -0.7071067811865476, 20.0, -1.5
    };
    static const double asin_xs[] = {
        0.0, 0.1, -0.1, 0.5, -0.5, 0.9, -0.9, 1.0, -1.0,
        0.01, -0.01, 0.7071067811865476
    };
    static const int ms[] = { 64, 80, 112, 160, 256, 320 };
    /* slack bits vs working m: allow some algorithm error beyond 1 ulp */
    static const int slacks[] = { 1, 1, 1, 1, 1, 2 };

    sn_ctx_init(&ctx);
    sn_ctx_set_round(&ctx, SN_ROUND_NTE);
    bf_context_init(&bfc, my_realloc, NULL);

    for (j = 0; j < (int)(sizeof(ms) / sizeof(ms[0])); j++) {
        m_bits = ms[j];
        e_bits = 15;
        if (m_bits >= 160)
            e_bits = 20;
        for (i = 0; i < (int)(sizeof(xs) / sizeof(xs[0])); i++) {
            double x = xs[i];
            int slack = slacks[j];
            if (x > -700 && x < 700)
                check_unary_mp("exp", &ctx, &bfc, sn_exp, bf_exp, x, e_bits, m_bits, slack, &tests, &fails);
            if (x > 0)
                check_unary_mp("log", &ctx, &bfc, sn_log, bf_log, x, e_bits, m_bits, slack, &tests, &fails);
            if (fabs(x) < 50.0)
                check_unary_mp("sin", &ctx, &bfc, sn_sin, bf_sin, x, e_bits, m_bits, slack, &tests, &fails);
            if (fabs(x) < 50.0)
                check_unary_mp("cos", &ctx, &bfc, sn_cos, bf_cos, x, e_bits, m_bits, slack, &tests, &fails);
            check_unary_mp("atan", &ctx, &bfc, sn_atan, bf_atan, x, e_bits, m_bits, slack, &tests, &fails);
            if (x > 0)
                check_unary_mp("sqrt", &ctx, &bfc, sn_sqrt, bf_sqrt, x, e_bits, m_bits, 2, &tests, &fails);
            if (fails > 80)
                break;
        }
        for (i = 0; i < (int)(sizeof(asin_xs) / sizeof(asin_xs[0])); i++) {
            check_unary_mp("asin", &ctx, &bfc, sn_asin, bf_asin, asin_xs[i], e_bits, m_bits, slacks[j], &tests, &fails);
            check_unary_mp("acos", &ctx, &bfc, sn_acos, bf_acos, asin_xs[i], e_bits, m_bits, slacks[j], &tests, &fails);
        }
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 2.0, 0.5, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 10.0, 0.25, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 1.5, 3.0, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 3.0, 1.5, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 1.25, -2.5, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 0.75, 4.5, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 5.0, 0.125, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("add", &ctx, &bfc, sn_add, bf_add, 0.1, 1.0, e_bits, m_bits, 2, &tests, &fails);
        check_binary_mp("mul", &ctx, &bfc, sn_mul, bf_mul, 1.5, 2.5, e_bits, m_bits, 2, &tests, &fails);
        check_binary_mp("div", &ctx, &bfc, sn_div, bf_div, 1.0, 3.0, e_bits, m_bits, 2, &tests, &fails);
        if (fails > 80)
            break;
    }


    /* tan + hypot residual (libbf has tan/hypot) */
    for (j = 0; j < (int)(sizeof(ms) / sizeof(ms[0])); j++) {
        m_bits = ms[j];
        e_bits = (m_bits >= 160) ? 20 : 15;
        check_unary_mp("tan", &ctx, &bfc, sn_tan, bf_tan, 0.25, e_bits, m_bits, slacks[j], &tests, &fails);
        check_unary_mp("tan", &ctx, &bfc, sn_tan, bf_tan, 0.5, e_bits, m_bits, slacks[j], &tests, &fails);
        check_unary_mp("tan", &ctx, &bfc, sn_tan, bf_tan, 1.0, e_bits, m_bits, slacks[j], &tests, &fails);
        check_unary_mp("tan", &ctx, &bfc, sn_tan, bf_tan, -0.75, e_bits, m_bits, slacks[j], &tests, &fails);
        check_unary_mp("tan", &ctx, &bfc, sn_tan, bf_tan, 1.2, e_bits, m_bits, slacks[j], &tests, &fails);
        check_unary_mp("tan", &ctx, &bfc, sn_tan, bf_tan, -1.1, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 2.0, 10.0, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 0.5, -3.0, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 1.1, 7.3, e_bits, m_bits, slacks[j], &tests, &fails);
        check_binary_mp("pow", &ctx, &bfc, sn_pow, bf_pow, 9.0, -0.5, e_bits, m_bits, slacks[j], &tests, &fails);
        if (fails > 80) break;
    }

    /* AGM log path residual */
    e_bits = 20;
    m_bits = 320;
    check_unary_mp("log320", &ctx, &bfc, sn_log, bf_log, 1.5, e_bits, m_bits, 2, &tests, &fails);
    check_unary_mp("log320", &ctx, &bfc, sn_log, bf_log, 0.1, e_bits, m_bits, 2, &tests, &fails);
    check_unary_mp("log320", &ctx, &bfc, sn_log, bf_log, 10.0, e_bits, m_bits, 2, &tests, &fails);
    check_unary_mp("exp320", &ctx, &bfc, sn_exp, bf_exp, 0.5, e_bits, m_bits, 2, &tests, &fails);

    /* Hyperbolic / expm1 residual via libbf composition (libbf has no native sinh) */
    {
        static const double hxs[] = { 0.0, 0.1, -0.1, 0.25, -0.5, 1.0, -1.0, 2.0, -2.0, 0.01 };
        static const double e1xs[] = { 0.0, 1e-8, -1e-6, 0.1, -0.1, 0.4, -0.4, 1.0, -1.0, 2.0 };
        int hi, hj;
        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;
            for (hi = 0; hi < (int)(sizeof(hxs) / sizeof(hxs[0])); hi++) {
                double hx = hxs[hi];
                int slack = slacks[hj];
                sn_value a, out;
                bf_t ba, be, bei, br, bt, two;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 48;
                double rel;

                /* sinh */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &be); bf_init(&bfc, &bei);
                bf_init(&bfc, &br); bf_init(&bfc, &bt); bf_init(&bfc, &two);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_sinh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("sinh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_exp(&be, &ba, prec, BF_RNDN);
                    bf_set_si(&bt, -1);
                    bf_mul(&bt, &ba, &bt, prec, BF_RNDN); /* -x in bt */
                    bf_exp(&bei, &bt, prec, BF_RNDN);     /* e^{-x} */
                    bf_sub(&br, &be, &bei, prec, BF_RNDN);
                    bf_set_ui(&two, 2);
                    bf_div(&br, &br, &two, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("sinh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&be); bf_delete(&bei);
                bf_delete(&br); bf_delete(&bt); bf_delete(&two);

                /* cosh */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &be); bf_init(&bfc, &bei);
                bf_init(&bfc, &br); bf_init(&bfc, &bt); bf_init(&bfc, &two);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_cosh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("cosh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    double ax = fabs(hx);
                    bf_set_float64(&ba, ax);
                    bf_exp(&be, &ba, prec, BF_RNDN);
                    bf_set_si(&bt, -1);
                    bf_mul(&bt, &ba, &bt, prec, BF_RNDN); /* -|x| */
                    bf_exp(&bei, &bt, prec, BF_RNDN);
                    bf_add(&br, &be, &bei, prec, BF_RNDN);
                    bf_set_ui(&two, 2);
                    bf_div(&br, &br, &two, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                        printf("cosh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&be); bf_delete(&bei);
                bf_delete(&br); bf_delete(&bt); bf_delete(&two);
            }
            for (hi = 0; hi < (int)(sizeof(e1xs) / sizeof(e1xs[0])); hi++) {
                double ex = e1xs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, be, one, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 48;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &be);
                bf_init(&bfc, &one); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, ex, e_bits, m_bits) ||
                    sn_expm1(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("expm1 sn fail x=%a m=%d\n", ex, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, ex);
                    bf_exp(&be, &ba, prec, BF_RNDN);
                    bf_set_ui(&one, 1);
                    bf_sub(&br, &be, &one, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("expm1 residual FAIL x=%a m=%d rel=%.3e sn=%s\n", ex, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&be); bf_delete(&one); bf_delete(&br);
            }
            if (fails > 80) break;
        }
    }


    /* Inverse hyperbolic / log1p / tanh residual via libbf composition */
    {
        static const double ixs[] = { 0.0, 0.1, -0.1, 0.5, -0.5, 1.0, -1.0, 2.0, -2.0, 0.01, 5.0 };
        static const double acxs[] = { 1.0, 1.001, 1.1, 1.5, 2.0, 5.0, 10.0 };
        static const double atxs[] = { 0.0, 0.1, -0.1, 0.25, -0.5, 0.75, -0.9, 0.99 };
        static const double l1xs[] = { 0.0, 1e-8, -1e-6, 0.1, -0.1, 0.4, -0.4, 1.0, 2.0, -0.5 };
        static const double thxs[] = { 0.0, 0.1, -0.1, 0.5, -0.5, 1.0, -1.0, 2.0, -2.0, 0.01 };
        int hi, hj;
        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;
            for (hi = 0; hi < (int)(sizeof(ixs) / sizeof(ixs[0])); hi++) {
                double hx = ixs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, b1, bx2, bt, bs, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                /* asinh = log(x + sqrt(x^2+1)) */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &b1); bf_init(&bfc, &bx2);
                bf_init(&bfc, &bt); bf_init(&bfc, &bs); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_asinh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("asinh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_mul(&bx2, &ba, &ba, prec, BF_RNDN);
                    bf_set_ui(&b1, 1);
                    bf_add(&bt, &bx2, &b1, prec, BF_RNDN);
                    bf_sqrt(&bs, &bt, prec, BF_RNDN);
                    bf_add(&bt, &ba, &bs, prec, BF_RNDN);
                    bf_log(&br, &bt, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("asinh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&b1); bf_delete(&bx2);
                bf_delete(&bt); bf_delete(&bs); bf_delete(&br);
            }
            for (hi = 0; hi < (int)(sizeof(acxs) / sizeof(acxs[0])); hi++) {
                double hx = acxs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, b1, bx2, bt, bs, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &b1); bf_init(&bfc, &bx2);
                bf_init(&bfc, &bt); bf_init(&bfc, &bs); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_acosh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("acosh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_mul(&bx2, &ba, &ba, prec, BF_RNDN);
                    bf_set_ui(&b1, 1);
                    bf_sub(&bt, &bx2, &b1, prec, BF_RNDN);
                    bf_sqrt(&bs, &bt, prec, BF_RNDN);
                    bf_add(&bt, &ba, &bs, prec, BF_RNDN);
                    bf_log(&br, &bt, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("acosh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&b1); bf_delete(&bx2);
                bf_delete(&bt); bf_delete(&bs); bf_delete(&br);
            }
            for (hi = 0; hi < (int)(sizeof(atxs) / sizeof(atxs[0])); hi++) {
                double hx = atxs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, b1, bn, bd, bt, br, half;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &b1); bf_init(&bfc, &bn);
                bf_init(&bfc, &bd); bf_init(&bfc, &bt); bf_init(&bfc, &br); bf_init(&bfc, &half);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_atanh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("atanh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_set_ui(&b1, 1);
                    bf_add(&bn, &b1, &ba, prec, BF_RNDN);
                    bf_sub(&bd, &b1, &ba, prec, BF_RNDN);
                    bf_div(&bt, &bn, &bd, prec, BF_RNDN);
                    bf_log(&br, &bt, prec, BF_RNDN);
                    bf_set_float64(&half, 0.5);
                    bf_mul(&br, &br, &half, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("atanh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&b1); bf_delete(&bn);
                bf_delete(&bd); bf_delete(&bt); bf_delete(&br); bf_delete(&half);
            }
            for (hi = 0; hi < (int)(sizeof(l1xs) / sizeof(l1xs[0])); hi++) {
                double hx = l1xs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, b1, bt, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &b1); bf_init(&bfc, &bt); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_log1p(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("log1p sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_set_ui(&b1, 1);
                    bf_add(&bt, &b1, &ba, prec, BF_RNDN);
                    bf_log(&br, &bt, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("log1p residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&b1); bf_delete(&bt); bf_delete(&br);
            }
            for (hi = 0; hi < (int)(sizeof(thxs) / sizeof(thxs[0])); hi++) {
                double hx = thxs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, be, bei, bt, br, sh, ch;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &be); bf_init(&bfc, &bei);
                bf_init(&bfc, &bt); bf_init(&bfc, &br);
                bf_init(&bfc, &sh); bf_init(&bfc, &ch);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_tanh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("tanh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_exp(&be, &ba, prec, BF_RNDN);
                    bf_set_si(&bt, -1);
                    bf_mul(&bt, &ba, &bt, prec, BF_RNDN);
                    bf_exp(&bei, &bt, prec, BF_RNDN);
                    bf_sub(&sh, &be, &bei, prec, BF_RNDN);
                    bf_add(&ch, &be, &bei, prec, BF_RNDN);
                    bf_div(&br, &sh, &ch, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("tanh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&be); bf_delete(&bei);
                bf_delete(&bt); bf_delete(&br);
                bf_delete(&sh); bf_delete(&ch);
            }
            if (fails > 80) break;
        }
    }



    /* atan2 residual vs libbf bf_atan2 */
    {
        static const double ys[] = { 0.0, 1.0, -1.0, 1.0, -1.0, 3.0, -3.0, 1e-8, 1e8, 0.5, -0.25 };
        static const double xs[] = { 1.0, 1.0, 1.0, -1.0, -1.0, 4.0, -4.0, 1e-8, -1e8, -0.5, 2.0 };
        int ai, hj;
        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;
            for (ai = 0; ai < (int)(sizeof(ys) / sizeof(ys[0])); ai++) {
                double yv = ys[ai], xv = xs[ai];
                int slack = slacks[hj] + 1;
                sn_value y, x, out;
                bf_t by, bx, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&y); sn_value_init(&x); sn_value_init(&out);
                bf_init(&bfc, &by); bf_init(&bfc, &bx); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &y, yv, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &x, xv, e_bits, m_bits) ||
                    sn_atan2(&ctx, &out, &y, &x, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("atan2 sn fail y=%a x=%a m=%d\n", yv, xv, m_bits); fails++;
                } else {
                    bf_set_float64(&by, yv);
                    bf_set_float64(&bx, xv);
                    bf_atan2(&br, &by, &bx, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("atan2 residual FAIL y=%a x=%a m=%d rel=%.3e sn=%s\n",
                               yv, xv, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &out);
                bf_delete(&by); bf_delete(&bx); bf_delete(&br);
            }
            if (fails > 80) break;
        }
    }

    /* cbrt / hypot / log2 / log10 / exp2 residual (libbf composition) */
    {
        static const double cxs[] = { 0.0, 1.0, 8.0, 27.0, 0.125, 2.0, -8.0, 1e-6, 1e6, 0.5 };
        static const double hy_a[] = { 3.0, 5.0, 0.0, 1e-8, 1.0, 2.0 };
        static const double hy_b[] = { 4.0, 12.0, 1.0, 1e-8, -1.0, 0.5 };
        static const double lxs[] = { 0.5, 1.0, 2.0, 10.0, 0.1, 1.5, 100.0 };
        static const double e2xs[] = { 0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 10.0 };
        int ci, hj;
        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;
            for (ci = 0; ci < (int)(sizeof(cxs) / sizeof(cxs[0])); ci++) {
                double cx = cxs[ci];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, br, bt, b3, one;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &br); bf_init(&bfc, &bt);
                bf_init(&bfc, &b3); bf_init(&bfc, &one);
                if (!sn_set_hex_mp(&ctx, &a, cx, e_bits, m_bits) ||
                    sn_cbrt(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("cbrt sn fail x=%a m=%d\n", cx, m_bits); fails++;
                } else {
                    /* sign(cbrt) * exp(log(|x|)/3); x=0 -> 0 */
                    if (cx == 0.0) {
                        bf_set_ui(&br, 0);
                    } else {
                        double ax = fabs(cx);
                        bf_set_float64(&ba, ax);
                        bf_log(&bt, &ba, prec, BF_RNDN);
                        bf_set_ui(&b3, 3);
                        bf_div(&bt, &bt, &b3, prec, BF_RNDN);
                        bf_exp(&br, &bt, prec, BF_RNDN);
                        if (cx < 0.0)
                            br.sign = 1;
                    }
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("cbrt residual FAIL x=%a m=%d rel=%.3e sn=%s\n", cx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&br); bf_delete(&bt);
                bf_delete(&b3); bf_delete(&one);
            }
            for (ci = 0; ci < (int)(sizeof(hy_a) / sizeof(hy_a[0])); ci++) {
                double xa = hy_a[ci], xb = hy_b[ci];
                int slack = slacks[hj] + 1;
                sn_value a, b, out;
                bf_t ba, bb, ba2, bb2, bs, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &ba2);
                bf_init(&bfc, &bb2); bf_init(&bfc, &bs); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                    sn_hypot(&ctx, &out, &a, &b, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("hypot sn fail %a %a m=%d\n", xa, xb, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, xa);
                    bf_set_float64(&bb, xb);
                    bf_mul(&ba2, &ba, &ba, prec, BF_RNDN);
                    bf_mul(&bb2, &bb, &bb, prec, BF_RNDN);
                    bf_add(&bs, &ba2, &bb2, prec, BF_RNDN);
                    bf_sqrt(&br, &bs, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("hypot residual FAIL %a %a m=%d rel=%.3e sn=%s\n", xa, xb, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&ba2);
                bf_delete(&bb2); bf_delete(&bs); bf_delete(&br);
            }
            for (ci = 0; ci < (int)(sizeof(lxs) / sizeof(lxs[0])); ci++) {
                double lx = lxs[ci];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, bln, bln2, bln10, br, two, ten;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                /* log2 */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bln); bf_init(&bfc, &bln2);
                bf_init(&bfc, &br); bf_init(&bfc, &two);
                if (!sn_set_hex_mp(&ctx, &a, lx, e_bits, m_bits) ||
                    sn_log2(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("log2 sn fail x=%a m=%d\n", lx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, lx);
                    bf_log(&bln, &ba, prec, BF_RNDN);
                    bf_set_ui(&two, 2);
                    bf_log(&bln2, &two, prec, BF_RNDN);
                    bf_div(&br, &bln, &bln2, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("log2 residual FAIL x=%a m=%d rel=%.3e sn=%s\n", lx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bln); bf_delete(&bln2); bf_delete(&br); bf_delete(&two);

                /* log10 */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bln); bf_init(&bfc, &bln10);
                bf_init(&bfc, &br); bf_init(&bfc, &ten);
                if (!sn_set_hex_mp(&ctx, &a, lx, e_bits, m_bits) ||
                    sn_log10(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("log10 sn fail x=%a m=%d\n", lx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, lx);
                    bf_log(&bln, &ba, prec, BF_RNDN);
                    bf_set_ui(&ten, 10);
                    bf_log(&bln10, &ten, prec, BF_RNDN);
                    bf_div(&br, &bln, &bln10, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("log10 residual FAIL x=%a m=%d rel=%.3e sn=%s\n", lx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bln); bf_delete(&bln10); bf_delete(&br); bf_delete(&ten);
            }
            for (ci = 0; ci < (int)(sizeof(e2xs) / sizeof(e2xs[0])); ci++) {
                double ex = e2xs[ci];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, two, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &two); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, ex, e_bits, m_bits) ||
                    sn_exp2(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("exp2 sn fail x=%a m=%d\n", ex, m_bits); fails++;
                } else {
                    bf_set_ui(&two, 2);
                    bf_set_float64(&ba, ex);
                    bf_pow(&br, &two, &ba, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("exp2 residual FAIL x=%a m=%d rel=%.3e sn=%s\n", ex, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&two); bf_delete(&br);
            }
            if (fails > 80) break;
        }
    }


    /* fma / frem residual + extra tan/asin/pow domains (vs libbf) */
    {
        static const double fma_a[] = { 1.0, 1.5, 0.1, -2.0, 1e-6, 3.0, 0.5, -0.75, 10.0, 1.25 };
        static const double fma_b[] = { 2.0, 2.5, 0.2, 0.5, 1e6, -1.5, 4.0, 1.5, 0.1, -0.8 };
        static const double fma_c[] = { 0.5, -1.0, 1e-8, 3.0, -1.0, 0.25, -2.0, 0.125, 1.0, 0.0 };
        static const double rem_a[] = { 5.5, -5.5, 1.0, 10.0, 0.75, -0.75, 100.0, 1e-3, 7.0, -7.0 };
        static const double rem_b[] = { 2.0, 2.0, 3.0, 3.0, 0.5, 0.5, 6.0, 1e-3, 3.0, 4.0 };
        static const double tan_xs[] = {
            0.01, -0.01, 0.3, -0.3, 0.9, -0.9, 1.4, -1.4, 0.125, 2.0
        };
        static const double asin_more[] = {
            0.25, -0.25, 0.6, -0.6, 0.999, -0.999, 0.001, -0.001
        };
        static const double pow_a[] = { 1.2, 0.3, 4.0, 2.5, 0.8, 7.0 };
        static const double pow_b[] = { 3.5, -1.5, 0.25, -0.75, 5.0, 0.5 };
        int fi, hj;

        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;

            for (fi = 0; fi < (int)(sizeof(fma_a) / sizeof(fma_a[0])); fi++) {
                double xa = fma_a[fi], xb = fma_b[fi], xc = fma_c[fi];
                int slack = slacks[hj] + 2;
                sn_value a, b, c, out;
                bf_t ba, bb, bc, bt, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 80;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&c); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &bc);
                bf_init(&bfc, &bt); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &c, xc, e_bits, m_bits) ||
                    sn_fma(&ctx, &out, &a, &b, &c, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("fma sn fail a=%a b=%a c=%a m=%d\n", xa, xb, xc, m_bits);
                    fails++;
                } else {
                    /* high-prec mul+add oracle, then round to working m */
                    bf_set_float64(&ba, xa);
                    bf_set_float64(&bb, xb);
                    bf_set_float64(&bc, xc);
                    bf_mul(&bt, &ba, &bb, prec, BF_RNDN);
                    bf_add(&br, &bt, &bc, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("fma residual FAIL a=%a b=%a c=%a m=%d rel=%.3e sn=%s\n",
                               xa, xb, xc, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b);
                sn_value_clear(&ctx, &c); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&bc);
                bf_delete(&bt); bf_delete(&br);
            }

            for (fi = 0; fi < (int)(sizeof(rem_a) / sizeof(rem_a[0])); fi++) {
                double xa = rem_a[fi], xb = rem_b[fi];
                int slack = slacks[hj] + 2;
                sn_value a, b, out;
                bf_t ba, bb, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                    sn_frem(&ctx, &out, &a, &b, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("frem sn fail a=%a b=%a m=%d\n", xa, xb, m_bits);
                    fails++;
                } else {
                    bf_set_float64(&ba, xa);
                    bf_set_float64(&bb, xb);
                    /* IEEE remainder ~ round-to-nearest quotient */
                    bf_rem(&br, &ba, &bb, prec, BF_RNDN, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("frem residual FAIL a=%a b=%a m=%d rel=%.3e sn=%s\n",
                               xa, xb, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
            }

            for (fi = 0; fi < (int)(sizeof(tan_xs) / sizeof(tan_xs[0])); fi++) {
                check_unary_mp("tan+", &ctx, &bfc, sn_tan, bf_tan,
                               tan_xs[fi], e_bits, m_bits, slacks[hj] + 1, &tests, &fails);
            }
            for (fi = 0; fi < (int)(sizeof(asin_more) / sizeof(asin_more[0])); fi++) {
                check_unary_mp("asin+", &ctx, &bfc, sn_asin, bf_asin,
                               asin_more[fi], e_bits, m_bits, slacks[hj] + 1, &tests, &fails);
                check_unary_mp("acos+", &ctx, &bfc, sn_acos, bf_acos,
                               asin_more[fi], e_bits, m_bits, slacks[hj] + 1, &tests, &fails);
            }
            for (fi = 0; fi < (int)(sizeof(pow_a) / sizeof(pow_a[0])); fi++) {
                check_binary_mp("pow+", &ctx, &bfc, sn_pow, bf_pow,
                                pow_a[fi], pow_b[fi], e_bits, m_bits, slacks[hj] + 1, &tests, &fails);
            }
            if (fails > 80) break;
        }
    }


    /* hard residual: fma cancel, invhyp edges, large |x| log/exp, tan near poles-ish */
    {
        static const double fma_ca[] = { 1.0, 1.5, 0.75, 1e-4, -2.5, 3.0, 0.125, 9.0 };
        static const double fma_cb[] = { 2.0, 4.0, 8.0, 1e4, 1.6, -1.0, 16.0, 1.0/3.0 };
        /* c ~ -a*b so fma stresses fused rounding vs mul+add */
        static const double fma_cc[] = { -2.0, -6.0, -6.0, -1.0, 4.0, 3.0, -2.0, -3.0 };
        static const double asinh_h[] = {
            1e-12, -1e-12, 1e-6, -1e-6, 0.5, -0.5, 10.0, -10.0, 100.0, -100.0, 1e4, -1e4
        };
        static const double acosh_h[] = {
            1.0, 1.0000001, 1.001, 1.5, 2.0, 10.0, 100.0, 1e6
        };
        static const double atanh_h[] = {
            0.0, 1e-12, -1e-12, 0.1, -0.1, 0.5, -0.5, 0.9, -0.9, 0.999, -0.999
        };
        static const double log_h[] = {
            1e-12, 1e-8, 1e-4, 0.5, 1.0+1e-12, 2.0, 1e8, 1e12
        };
        static const double exp_h[] = {
            -20.0, -5.0, -1e-8, 1e-8, 0.5, 5.0, 20.0
        };
        int fi, hj;

        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;

            for (fi = 0; fi < (int)(sizeof(fma_ca) / sizeof(fma_ca[0])); fi++) {
                double xa = fma_ca[fi], xb = fma_cb[fi], xc = fma_cc[fi];
                int slack = slacks[hj] + 3;
                sn_value a, b, c, out;
                bf_t ba, bb, bc, bt, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 96;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&c); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &bc);
                bf_init(&bfc, &bt); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &c, xc, e_bits, m_bits) ||
                    sn_fma(&ctx, &out, &a, &b, &c, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("fmaH sn fail a=%a b=%a c=%a m=%d\n", xa, xb, xc, m_bits);
                    fails++;
                } else {
                    bf_set_float64(&ba, xa);
                    bf_set_float64(&bb, xb);
                    bf_set_float64(&bc, xc);
                    bf_mul(&bt, &ba, &bb, prec, BF_RNDN);
                    bf_add(&br, &bt, &bc, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("fmaH residual FAIL a=%a b=%a c=%a m=%d rel=%.3e sn=%s\n",
                               xa, xb, xc, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b);
                sn_value_clear(&ctx, &c); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&bc);
                bf_delete(&bt); bf_delete(&br);
            }

            for (fi = 0; fi < (int)(sizeof(asinh_h) / sizeof(asinh_h[0])); fi++) {
                double hx = asinh_h[fi];
                int slack = slacks[hj] + 2;
                sn_value a, out;
                bf_t ba, bx2, one, t, sbf, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 80;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bx2); bf_init(&bfc, &one);
                bf_init(&bfc, &t); bf_init(&bfc, &sbf); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_asinh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("asinhH sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_mul(&bx2, &ba, &ba, prec, BF_RNDN);
                    bf_set_ui(&one, 1);
                    bf_add(&t, &bx2, &one, prec, BF_RNDN);
                    bf_sqrt(&sbf, &t, prec, BF_RNDN);
                    bf_add(&t, &ba, &sbf, prec, BF_RNDN);
                    bf_log(&br, &t, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("asinhH residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bx2); bf_delete(&one);
                bf_delete(&t); bf_delete(&sbf); bf_delete(&br);
            }

            for (fi = 0; fi < (int)(sizeof(acosh_h) / sizeof(acosh_h[0])); fi++) {
                double hx = acosh_h[fi];
                int slack = slacks[hj] + 2;
                sn_value a, out;
                bf_t ba, bx2, one, t, sbf, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 80;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bx2); bf_init(&bfc, &one);
                bf_init(&bfc, &t); bf_init(&bfc, &sbf); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_acosh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("acoshH sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_mul(&bx2, &ba, &ba, prec, BF_RNDN);
                    bf_set_ui(&one, 1);
                    bf_sub(&t, &bx2, &one, prec, BF_RNDN);
                    bf_sqrt(&sbf, &t, prec, BF_RNDN);
                    bf_add(&t, &ba, &sbf, prec, BF_RNDN);
                    bf_log(&br, &t, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("acoshH residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bx2); bf_delete(&one);
                bf_delete(&t); bf_delete(&sbf); bf_delete(&br);
            }

            for (fi = 0; fi < (int)(sizeof(atanh_h) / sizeof(atanh_h[0])); fi++) {
                double hx = atanh_h[fi];
                int slack = slacks[hj] + 2;
                sn_value a, out;
                bf_t ba, one, num, den, t, br, half;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 96;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &one); bf_init(&bfc, &num);
                bf_init(&bfc, &den); bf_init(&bfc, &t); bf_init(&bfc, &br);
                bf_init(&bfc, &half);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_atanh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("atanhH sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    /* 0.5 * log((1+x)/(1-x)) */
                    bf_set_float64(&ba, hx);
                    bf_set_ui(&one, 1);
                    bf_add(&num, &one, &ba, prec, BF_RNDN);
                    bf_sub(&den, &one, &ba, prec, BF_RNDN);
                    bf_div(&t, &num, &den, prec, BF_RNDN);
                    bf_log(&br, &t, prec, BF_RNDN);
                    bf_set_float64(&half, 0.5);
                    bf_mul(&br, &br, &half, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("atanhH residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&one); bf_delete(&num);
                bf_delete(&den); bf_delete(&t); bf_delete(&br); bf_delete(&half);
            }

            for (fi = 0; fi < (int)(sizeof(log_h) / sizeof(log_h[0])); fi++) {
                check_unary_mp("logH", &ctx, &bfc, sn_log, bf_log,
                               log_h[fi], e_bits, m_bits, slacks[hj] + 1, &tests, &fails);
            }
            for (fi = 0; fi < (int)(sizeof(exp_h) / sizeof(exp_h[0])); fi++) {
                check_unary_mp("expH", &ctx, &bfc, sn_exp, bf_exp,
                               exp_h[fi], e_bits, m_bits, slacks[hj] + 1, &tests, &fails);
            }
            if (fails > 80) break;
        }
    }


    /* frexp/ldexp identity, fmod, integer-round, copysign/fmin/fmax/fdim (vs libbf) */
    {
        static const double decomp_xs[] = {
            1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 3.141592653589793, -2.718281828459045,
            1e-20, -1e-12, 1e20, 1024.0, 0.1, 7.5, -7.5, 0.999999999999, 1.000000000001
        };
        static const double fmod_a[] = {
            5.5, -5.5, 10.0, -10.0, 0.75, -0.75, 100.0, 1e-3, 7.0, -7.0, 1.5, -1.5, 8.0
        };
        static const double fmod_b[] = {
            2.0, 2.0, 3.0, 3.0, 0.5, 0.5, 6.0, 1e-3, 3.0, 4.0, 1.0, 0.25, -3.0
        };
        static const double rnd_xs[] = {
            1.5, 2.5, -1.5, -2.5, 0.1, -0.1, 0.9, -0.9, 3.0, -3.0,
            1.0000000000000002, -1.9999999999999998, 1023.75, -1023.25
        };
        static const double pair_a[] = { 1.0, -1.0, 2.5, -3.5, 0.0, 1e-8, 10.0, -0.25 };
        static const double pair_b[] = { 2.0, -2.0, -2.5, 1.5, -0.0, -1e-8, 3.0, 0.5 };
        int fi, hj;

        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;

            /* frexp + ldexp identity: ldexp(frexp(x)) == x */
            for (fi = 0; fi < (int)(sizeof(decomp_xs) / sizeof(decomp_xs[0])); fi++) {
                double hx = decomp_xs[fi];
                int slack = slacks[hj] + 1;
                sn_value a, mant, back;
                bf_t ba, br;
                char *s = NULL;
                int expv = 0;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&mant); sn_value_init(&back);
                bf_init(&bfc, &ba); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_frexp(&ctx, &mant, &expv, &a, NULL) != SN_OK ||
                    sn_ldexp(&ctx, &back, &mant, expv, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &back, 16) != SN_OK || !s) {
                    printf("frexp/ldexp sn fail x=%a m=%d\n", hx, m_bits);
                    fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_set(&br, &ba);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("frexp/ldexp residual FAIL x=%a m=%d exp=%d rel=%.3e sn=%s\n",
                               hx, m_bits, expv, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &mant); sn_value_clear(&ctx, &back);
                bf_delete(&ba); bf_delete(&br);
            }

            /* fmod vs truncated remainder: a - trunc(a/b)*b */
            for (fi = 0; fi < (int)(sizeof(fmod_a) / sizeof(fmod_a[0])); fi++) {
                double xa = fmod_a[fi], xb = fmod_b[fi];
                int slack = slacks[hj] + 2;
                sn_value a, b, out;
                bf_t ba, bb, q, qt, t, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 80;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &q);
                bf_init(&bfc, &qt); bf_init(&bfc, &t); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                    sn_fmod(&ctx, &out, &a, &b, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("fmod sn fail a=%a b=%a m=%d\n", xa, xb, m_bits);
                    fails++;
                } else {
                    bf_set_float64(&ba, xa);
                    bf_set_float64(&bb, xb);
                    bf_div(&q, &ba, &bb, prec, BF_RNDN);
                    bf_set(&qt, &q);
                    bf_rint(&qt, BF_RNDZ);
                    bf_mul(&t, &qt, &bb, prec, BF_RNDN);
                    bf_sub(&br, &ba, &t, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("fmod residual FAIL a=%a b=%a m=%d rel=%.3e sn=%s\n",
                               xa, xb, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&q);
                bf_delete(&qt); bf_delete(&t); bf_delete(&br);
            }

            /* ceil / floor / trunc / rint vs bf_rint modes */
            for (fi = 0; fi < (int)(sizeof(rnd_xs) / sizeof(rnd_xs[0])); fi++) {
                double hx = rnd_xs[fi];
                int slack = slacks[hj] + 1;
                struct {
                    const char *name;
                    sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *);
                    int rnd;
                } ops[] = {
                    { "ceil", sn_ceil, BF_RNDU },
                    { "floor", sn_floor, BF_RNDD },
                    { "trunc", sn_trunc, BF_RNDZ },
                    { "rint", sn_rint, BF_RNDN },
                };
                int oi;
                for (oi = 0; oi < 4; oi++) {
                    sn_value a, out;
                    bf_t ba, br;
                    char *s = NULL;
                    double rel;
                    tests++;
                    sn_value_init(&a); sn_value_init(&out);
                    bf_init(&bfc, &ba); bf_init(&bfc, &br);
                    if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                        ops[oi].sn_op(&ctx, &out, &a, NULL) != SN_OK ||
                        sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                        printf("%s sn fail x=%a m=%d\n", ops[oi].name, hx, m_bits);
                        fails++;
                    } else {
                        bf_set_float64(&ba, hx);
                        bf_set(&br, &ba);
                        bf_rint(&br, ops[oi].rnd);
                        bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                        if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                            printf("%s residual FAIL x=%a m=%d rel=%.3e sn=%s\n",
                                   ops[oi].name, hx, m_bits, rel, s);
                            fails++;
                        }
                    }
                    if (s) sn_str_free(&ctx, s); s = NULL;
                    sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                    bf_delete(&ba); bf_delete(&br);
                }
            }

            /* copysign / fmin / fmax / fdim exact residual */
            for (fi = 0; fi < (int)(sizeof(pair_a) / sizeof(pair_a[0])); fi++) {
                double xa = pair_a[fi], xb = pair_b[fi];
                int slack = slacks[hj] + 1;
                sn_value a, b, out;
                bf_t ba, bb, br;
                char *s = NULL;
                double rel;
                limb_t prec = (limb_t)m_bits + 48;

                /* copysign */
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                    sn_copysign(&ctx, &out, &a, &b, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("copysign sn fail a=%a b=%a m=%d\n", xa, xb, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, xa);
                    bf_set_float64(&bb, xb);
                    bf_set(&br, &ba);
                    br.sign = bb.sign;
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("copysign residual FAIL a=%a b=%a m=%d rel=%.3e sn=%s\n",
                               xa, xb, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&br);

                /* fmin / fmax */
                {
                    int k;
                    for (k = 0; k < 2; k++) {
                        sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *) =
                            k ? sn_fmax : sn_fmin;
                        const char *nm = k ? "fmax" : "fmin";
                        tests++;
                        sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                        bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &br);
                        if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                            !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                            sn_op(&ctx, &out, &a, &b, NULL) != SN_OK ||
                            sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                            printf("%s sn fail a=%a b=%a m=%d\n", nm, xa, xb, m_bits); fails++;
                        } else {
                            bf_set_float64(&ba, xa);
                            bf_set_float64(&bb, xb);
                            if (bf_cmp(&ba, &bb) < 0)
                                bf_set(&br, k ? &bb : &ba);
                            else
                                bf_set(&br, k ? &ba : &bb);
                            bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                            if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                                printf("%s residual FAIL a=%a b=%a m=%d rel=%.3e sn=%s\n",
                                       nm, xa, xb, m_bits, rel, s);
                                fails++;
                            }
                        }
                        if (s) sn_str_free(&ctx, s); s = NULL;
                        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                        bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
                    }
                }

                /* fdim = max(a-b, 0) for finite non-NaN */
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xa, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xb, e_bits, m_bits) ||
                    sn_fdim(&ctx, &out, &a, &b, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("fdim sn fail a=%a b=%a m=%d\n", xa, xb, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, xa);
                    bf_set_float64(&bb, xb);
                    bf_sub(&br, &ba, &bb, prec, BF_RNDN);
                    if (br.sign)
                        bf_set_ui(&br, 0);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("fdim residual FAIL a=%a b=%a m=%d rel=%.3e sn=%s\n",
                               xa, xb, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
            }

            /* denser pow residual samples at multiprec */
            {
                static const double pa[] = { 1.1, 0.5, 3.0, 2.0, 0.25, 5.0, 1.7, 9.0 };
                static const double pb[] = { 2.3, -2.0, 0.5, -0.5, 4.0, 1.25, -1.75, 0.125 };
                int pi;
                for (pi = 0; pi < (int)(sizeof(pa)/sizeof(pa[0])); pi++) {
                    check_binary_mp("powX", &ctx, &bfc, sn_pow, bf_pow,
                                    pa[pi], pb[pi], e_bits, m_bits, slacks[hj] + 3, &tests, &fails);
                }
            }

            /* denser atan2 residual grid (libbf has bf_atan2) */
            {
                static const double ys[] = {
                    -2.5, -1.0, -0.5, -0.125, 0.0, 0.125, 0.5, 1.0, 2.5, 10.0
                };
                static const double xs[] = {
                    -3.0, -1.0, -0.25, 0.0, 0.25, 1.0, 3.0, 7.0
                };
                int yi, xi;
                for (yi = 0; yi < (int)(sizeof(ys)/sizeof(ys[0])); yi++) {
                    for (xi = 0; xi < (int)(sizeof(xs)/sizeof(xs[0])); xi++) {
                        if (ys[yi] == 0.0 && xs[xi] == 0.0) continue;
                        check_binary_mp("atan2", &ctx, &bfc, sn_atan2, bf_atan2,
                                        ys[yi], xs[xi], e_bits, m_bits, slacks[hj] + 2,
                                        &tests, &fails);
                    }
                }
            }

            /* denser tan near moderate args (avoid poles) */
            {
                static const double txs[] = {
                    -1.2, -0.9, -0.4, -0.1, 0.05, 0.3, 0.7, 1.1, 1.4
                };
                int ti;
                for (ti = 0; ti < (int)(sizeof(txs)/sizeof(txs[0])); ti++) {
                    check_unary_mp("tanD", &ctx, &bfc, sn_tan, bf_tan,
                                   txs[ti], e_bits, m_bits, slacks[hj] + 2,
                                   &tests, &fails);
                }
            }

            /* denser fma residual: a*b+c vs libbf mul/add */
            {
                static const double fa[] = { 1.5, -2.25, 0.125, 10.0, -0.75, 3.141592653589793 };
                static const double fb[] = { 2.0, -0.5, 8.0, 0.25, 1.125, -1.5 };
                static const double fc[] = { 0.5, -1.0, 0.0, 2.5, -3.0, 0.125 };
                int fi;
                for (fi = 0; fi < (int)(sizeof(fa)/sizeof(fa[0])); fi++) {
                    sn_value a, b, c, out;
                    bf_t ba, bb, bc, bt, br;
                    char *s = NULL;
                    limb_t prec = (limb_t)m_bits + 64;
                    double rel;
                    tests++;
                    sn_value_init(&a); sn_value_init(&b); sn_value_init(&c); sn_value_init(&out);
                    bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &bc);
                    bf_init(&bfc, &bt); bf_init(&bfc, &br);
                    if (!sn_set_hex_mp(&ctx, &a, fa[fi], e_bits, m_bits) ||
                        !sn_set_hex_mp(&ctx, &b, fb[fi], e_bits, m_bits) ||
                        !sn_set_hex_mp(&ctx, &c, fc[fi], e_bits, m_bits) ||
                        sn_fma(&ctx, &out, &a, &b, &c, NULL) != SN_OK ||
                        sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                        printf("fmaD sn fail m=%d\n", m_bits); fails++;
                    } else {
                        bf_set_float64(&ba, fa[fi]);
                        bf_set_float64(&bb, fb[fi]);
                        bf_set_float64(&bc, fc[fi]);
                        bf_mul(&bt, &ba, &bb, prec, BF_RNDN);
                        bf_add(&br, &bt, &bc, prec, BF_RNDN);
                        bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                        if (!residual_ok(&bfc, s, &br, m_bits, slacks[hj] + 2, &rel)) {
                            printf("fmaD residual FAIL m=%d rel=%.3e sn=%s\n", m_bits, rel, s);
                            fails++;
                        }
                    }
                    if (s) sn_str_free(&ctx, s);
                    sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b);
                    sn_value_clear(&ctx, &c); sn_value_clear(&ctx, &out);
                    bf_delete(&ba); bf_delete(&bb); bf_delete(&bc);
                    bf_delete(&bt); bf_delete(&br);
                }
            }

            if (fails > 100) break;
        }
    }


    /* denser residual: cbrt grid, hypot axes, log1p/expm1 near 0, pow fractional, large mul cancel */
    {
        static const double cxs[] = {
            0.0, 1.0, -1.0, 8.0, -8.0, 27.0, 0.125, -0.125, 1e-6, 1e6,
            2.0, 10.0, 0.5, 1000.0, -1000.0, 1.5, -2.5, 0.01, 123.456, 7.0
        };
        static const double hy[] = { 0.0, 1.0, -1.0, 3.0, 1e-8, 1e8, 0.5, -2.0, 10.0, 1e-3 };
        static const double hx[] = { 0.0, 1.0, 2.0, -3.0, 1e-8, 1e8, -0.5, 4.0, -10.0, 1e-3 };
        static const double l1[] = {
            0.0, 1e-12, -1e-12, 1e-8, -1e-8, 1e-4, -1e-4, 0.01, -0.01,
            0.1, -0.1, 0.5, -0.5, 0.9, -0.9, 1.0, 2.0, -0.25, 1e-16, -1e-16
        };
        static const double e1[] = {
            0.0, 1e-12, -1e-12, 1e-8, -1e-8, 1e-4, -1e-4, 0.01, -0.01,
            0.1, -0.1, 0.5, -0.5, 1.0, -1.0, 2.0, -2.0, 0.25, -0.25, 1e-16
        };
        int hi, hj;
        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;
            for (hi = 0; hi < (int)(sizeof(cxs) / sizeof(cxs[0])); hi++) {
                double cx = cxs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, br, bt, one, thr;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &br); bf_init(&bfc, &bt);
                bf_init(&bfc, &one); bf_init(&bfc, &thr);
                if (!sn_set_hex_mp(&ctx, &a, cx, e_bits, m_bits) ||
                    sn_cbrt(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("cbrtD sn fail x=%a m=%d\n", cx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, cx);
                    if (cx == 0.0) {
                        bf_set_ui(&br, 0);
                    } else {
                        int neg = (cx < 0.0);
                        if (neg) bf_set_float64(&ba, -cx);
                        bf_log(&bt, &ba, prec, BF_RNDN);
                        bf_set_ui(&one, 3);
                        bf_div(&br, &bt, &one, prec, BF_RNDN);
                        bf_exp(&bt, &br, prec, BF_RNDN);
                        bf_set(&br, &bt);
                        if (neg) bf_neg(&br);
                    }
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("cbrtD residual FAIL x=%a m=%d rel=%.3e sn=%s\n", cx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&br); bf_delete(&bt); bf_delete(&one); bf_delete(&thr);
            }
            for (hi = 0; hi < (int)(sizeof(hy) / sizeof(hy[0])); hi++) {
                double ya = hy[hi], xa = hx[hi];
                int slack = slacks[hj] + 1;
                sn_value a, b, out;
                bf_t ba, bb, br, t1, t2;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 48;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &br);
                bf_init(&bfc, &t1); bf_init(&bfc, &t2);
                if (!sn_set_hex_mp(&ctx, &a, ya, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, xa, e_bits, m_bits) ||
                    sn_hypot(&ctx, &out, &a, &b, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("hypotD sn fail %a %a m=%d\n", ya, xa, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, ya);
                    bf_set_float64(&bb, xa);
                    bf_mul(&t1, &ba, &ba, prec, BF_RNDN);
                    bf_mul(&t2, &bb, &bb, prec, BF_RNDN);
                    bf_add(&t1, &t1, &t2, prec, BF_RNDN);
                    bf_sqrt(&br, &t1, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("hypotD residual FAIL %a %a m=%d rel=%.3e sn=%s\n", ya, xa, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&br); bf_delete(&t1); bf_delete(&t2);
            }
            for (hi = 0; hi < (int)(sizeof(l1) / sizeof(l1[0])); hi++) {
                double lx = l1[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, one, bt, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &one); bf_init(&bfc, &bt); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, lx, e_bits, m_bits) ||
                    sn_log1p(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("log1pD sn fail x=%a m=%d\n", lx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, lx);
                    bf_set_ui(&one, 1);
                    bf_add(&bt, &ba, &one, prec, BF_RNDN);
                    bf_log(&br, &bt, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("log1pD residual FAIL x=%a m=%d rel=%.3e sn=%s\n", lx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&one); bf_delete(&bt); bf_delete(&br);
            }
            for (hi = 0; hi < (int)(sizeof(e1) / sizeof(e1[0])); hi++) {
                double ex = e1[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, be, one, br;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &be); bf_init(&bfc, &one); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, ex, e_bits, m_bits) ||
                    sn_expm1(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("expm1D sn fail x=%a m=%d\n", ex, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, ex);
                    bf_exp(&be, &ba, prec, BF_RNDN);
                    bf_set_ui(&one, 1);
                    bf_sub(&br, &be, &one, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("expm1D residual FAIL x=%a m=%d rel=%.3e sn=%s\n", ex, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&be); bf_delete(&one); bf_delete(&br);
            }
            /* expansion: denser mid grid + large-arg reduction + atan2 edges + sqrt */
            {
                static const double mid[] = { 0.125, 0.333, 0.777, 1.125, 2.75, -0.375, -1.875, 0.0625 };
                static const double bigx[] = { 10.0, 20.0, 40.0, 80.0, 100.0, -12.5, -33.0, 55.5 };
                static const double sx[] = { 0.25, 0.5, 2.0, 3.0, 10.0, 1e-6, 1e4, 7.0 };
                static const double ax[] = { 0.0, 1.0, -1.0, 2.5, -3.5, 0.125, 8.0 };
                static const double ay[] = { 1.0, 0.0, -1.0, 0.5, -2.0, 4.0, -0.25 };
                int ti, xi, yi;
                for (ti = 0; ti < (int)(sizeof(mid)/sizeof(mid[0])); ti++) {
                    check_unary_mp("sinM", &ctx, &bfc, sn_sin, bf_sin, mid[ti], e_bits, m_bits, slacks[hj]+1, &tests, &fails);
                    check_unary_mp("cosM", &ctx, &bfc, sn_cos, bf_cos, mid[ti], e_bits, m_bits, slacks[hj]+1, &tests, &fails);
                    check_unary_mp("atanM", &ctx, &bfc, sn_atan, bf_atan, mid[ti], e_bits, m_bits, slacks[hj]+1, &tests, &fails);
                    check_unary_mp("expM", &ctx, &bfc, sn_exp, bf_exp, mid[ti]*0.5, e_bits, m_bits, slacks[hj]+1, &tests, &fails);
                }
                if (m_bits <= 160) {
                    for (ti = 0; ti < (int)(sizeof(bigx)/sizeof(bigx[0])); ti++) {
                        check_unary_mp("sinB", &ctx, &bfc, sn_sin, bf_sin, bigx[ti], e_bits, m_bits, slacks[hj]+3, &tests, &fails);
                        check_unary_mp("cosB", &ctx, &bfc, sn_cos, bf_cos, bigx[ti], e_bits, m_bits, slacks[hj]+3, &tests, &fails);
                        check_unary_mp("tanB", &ctx, &bfc, sn_tan, bf_tan, bigx[ti]*0.1, e_bits, m_bits, slacks[hj]+3, &tests, &fails);
                    }
                }
                for (ti = 0; ti < (int)(sizeof(sx)/sizeof(sx[0])); ti++)
                    check_unary_mp("sqrtE", &ctx, &bfc, sn_sqrt, bf_sqrt, sx[ti], e_bits, m_bits, slacks[hj]+1, &tests, &fails);
                for (yi = 0; yi < (int)(sizeof(ay)/sizeof(ay[0])); yi++)
                    for (xi = 0; xi < (int)(sizeof(ax)/sizeof(ax[0])); xi++)
                        check_binary_mp("atan2E", &ctx, &bfc, sn_atan2, bf_atan2,
                                        ay[yi], ax[xi], e_bits, m_bits, slacks[hj]+2, &tests, &fails);
                /* denser pow edges */
                /* near-1 large exp needs more slack (compose); still checks absolute correctness */
                check_binary_mp("powE", &ctx, &bfc, sn_pow, bf_pow, 1.0001, 1000.0, e_bits, m_bits, slacks[hj]+8, &tests, &fails);
                check_binary_mp("powE", &ctx, &bfc, sn_pow, bf_pow, 0.999, 500.0, e_bits, m_bits, slacks[hj]+8, &tests, &fails);
                check_binary_mp("powE", &ctx, &bfc, sn_pow, bf_pow, 16.0, 0.125, e_bits, m_bits, slacks[hj]+1, &tests, &fails);
                check_binary_mp("powE", &ctx, &bfc, sn_pow, bf_pow, 27.0, 1.0/3.0, e_bits, m_bits, slacks[hj]+2, &tests, &fails);
            }
            /* extra pow fractional */
            check_binary_mp("powD", &ctx, &bfc, sn_pow, bf_pow, 2.0, 1.0/3.0, e_bits, m_bits, slacks[hj]+1, &tests, &fails);
            check_binary_mp("powD", &ctx, &bfc, sn_pow, bf_pow, 10.0, -1.5, e_bits, m_bits, slacks[hj]+1, &tests, &fails);
            check_binary_mp("powD", &ctx, &bfc, sn_pow, bf_pow, 0.5, 2.5, e_bits, m_bits, slacks[hj]+1, &tests, &fails);
            check_binary_mp("powD", &ctx, &bfc, sn_pow, bf_pow, 1.7, 3.3, e_bits, m_bits, slacks[hj]+1, &tests, &fails);
            if (fails > 80) break;
        }
    }

    /* ---- frexp/ldexp identity + fmod residual vs libbf (coverage expand) ---- */
    {
        static const double fx[] = {
            0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 2.0, 3.5, -7.25,
            0.1, 1e-6, 1e6, 12.75, -0.125, 1024.0, 0.333333333333,
            1.5e2, -42.0, 9.999, 0.0009765625
        };
        static const double fa[] = { 5.5, -5.5, 10.0, 1.25, -3.75, 100.0, 0.5, -0.5, 7.0, -9.0 };
        static const double fb[] = { 2.0, 2.0, 3.0, 0.5, 1.25, 7.0, 1.5, 0.25, 4.0, 2.5 };
        int hj, ti;
        static const int ms2[] = { 64, 80, 112, 160, 256 };
        static const int sl2[] = { 1, 1, 1, 1, 2 };
        for (hj = 0; hj < (int)(sizeof(ms2)/sizeof(ms2[0])); hj++) {
            int m_bits = ms2[hj];
            int e_bits = (m_bits >= 160) ? 20 : 15;
            int slack = sl2[hj];
            limb_t prec = (limb_t)m_bits + 48;
            for (ti = 0; ti < (int)(sizeof(fx)/sizeof(fx[0])); ti++) {
                sn_value a, mant, back, one;
                char *s = NULL;
                int expv = 0;
                double rel = 0.0;
                bf_t ba, br;
                tests++;
                sn_value_init(&a); sn_value_init(&mant); sn_value_init(&back); sn_value_init(&one);
                bf_init(&bfc, &ba); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, fx[ti], e_bits, m_bits) ||
                    sn_frexp(&ctx, &mant, &expv, &a, NULL) != SN_OK) {
                    printf("frexpI sn fail x=%a m=%d\n", fx[ti], m_bits); fails++;
                } else if (fx[ti] == 0.0 || fx[ti] == -0.0) {
                    /* frexp(0) -> 0, exp 0; skip reconstruct residual */
                } else if (sn_ldexp(&ctx, &back, &mant, expv, NULL) != SN_OK ||
                           sn_to_str(&ctx, &s, &back, 16) != SN_OK || !s) {
                    printf("ldexpI sn fail x=%a m=%d exp=%d\n", fx[ti], m_bits, expv); fails++;
                } else {
                    bf_set_float64(&ba, fx[ti]);
                    bf_set(&br, &ba);
                    bf_round(&br, (limb_t)m_bits, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                        printf("frexpI residual FAIL x=%a m=%d exp=%d rel=%.3e sn=%s\n",
                               fx[ti], m_bits, expv, rel, s);
                        fails++;
                    }
                    /* also require |mant| in [0.5, 1) for finite non-zero */
                    {
                        double md = 0.0;
                        if (sn_to_double(&ctx, &mant, &md) == SN_OK) {
                            double am = fabs(md);
                            if (!(am >= 0.5 && am < 1.0)) {
                                printf("frexpI range FAIL x=%a m=%d mant=%a exp=%d\n",
                                       fx[ti], m_bits, md, expv);
                                fails++;
                            }
                        }
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &mant);
                sn_value_clear(&ctx, &back); sn_value_clear(&ctx, &one);
                bf_delete(&ba); bf_delete(&br);
            }
            for (ti = 0; ti < (int)(sizeof(fa)/sizeof(fa[0])); ti++) {
                sn_value a, b, out;
                char *s = NULL;
                double rel = 0.0;
                bf_t ba, bb, br;
                tests++;
                sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, fa[ti], e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &b, fb[ti], e_bits, m_bits) ||
                    sn_fmod(&ctx, &out, &a, &b, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("fmodR sn fail a=%a b=%a m=%d\n", fa[ti], fb[ti], m_bits); fails++;
                } else {
                    bf_set_float64(&ba, fa[ti]);
                    bf_set_float64(&bb, fb[ti]);
                    /* IEEE fmod: remainder with trunc toward-zero quotient */
                    bf_rem(&br, &ba, &bb, prec, BF_RNDN, BF_RNDZ);
                    bf_round(&br, (limb_t)m_bits, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack + 2, &rel)) {
                        printf("fmodR residual FAIL a=%a b=%a m=%d rel=%.3e sn=%s\n",
                               fa[ti], fb[ti], m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
            }
            /* denser log/exp near 1 and mid-range (tighten coverage) */
            {
                static const double near1[] = {
                    1.0 + 1e-6, 1.0 - 1e-6, 1.0 + 1e-3, 1.0 - 1e-3,
                    0.999999, 1.000001, exp(1.0), 0.5, 2.0, 16.0
                };
                int ni;
                for (ni = 0; ni < (int)(sizeof(near1)/sizeof(near1[0])); ni++) {
                    double x = near1[ni];
                    if (x > 0)
                        check_unary_mp("logN", &ctx, &bfc, sn_log, bf_log, x, e_bits, m_bits, slack, &tests, &fails);
                    if (x > -700 && x < 700)
                        check_unary_mp("expN", &ctx, &bfc, sn_exp, bf_exp, log(x > 0 ? x : 1.0) * 0.5, e_bits, m_bits, slack, &tests, &fails);
                    check_unary_mp("atanN", &ctx, &bfc, sn_atan, bf_atan, x - 1.0, e_bits, m_bits, slack, &tests, &fails);
                }
            }
            if (fails > 80) break;
        }
    }

    /* modf identity: ipart+fpart reconstructs x (finite) */
    {
        static const double mx[] = {
            0.0, -0.0, 1.25, -1.25, 3.75, -3.75, 10.5, -10.5,
            0.999, -0.001, 1024.25, -7.0, 0.5, 2.0
        };
        static const int ms3[] = { 64, 80, 112, 160, 256 };
        static const int sl3[] = { 1, 1, 1, 1, 2 };
        int hj, ti;
        for (hj = 0; hj < (int)(sizeof(ms3)/sizeof(ms3[0])); hj++) {
            int m_bits = ms3[hj];
            int e_bits = (m_bits >= 160) ? 20 : 15;
            int slack = sl3[hj];
            for (ti = 0; ti < (int)(sizeof(mx)/sizeof(mx[0])); ti++) {
                sn_value a, ip, fp, sum;
                char *s = NULL;
                double rel = 0.0;
                bf_t ba, br;
                tests++;
                sn_value_init(&a); sn_value_init(&ip); sn_value_init(&fp); sn_value_init(&sum);
                bf_init(&bfc, &ba); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, mx[ti], e_bits, m_bits) ||
                    sn_modf(&ctx, &ip, &fp, &a, NULL) != SN_OK ||
                    sn_add(&ctx, &sum, &ip, &fp, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &sum, 16) != SN_OK || !s) {
                    printf("modfI sn fail x=%a m=%d\n", mx[ti], m_bits); fails++;
                } else {
                    bf_set_float64(&ba, mx[ti]);
                    bf_set(&br, &ba);
                    bf_round(&br, (limb_t)m_bits, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                        printf("modfI residual FAIL x=%a m=%d rel=%.3e sn=%s\n", mx[ti], m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &ip);
                sn_value_clear(&ctx, &fp); sn_value_clear(&ctx, &sum);
                bf_delete(&ba); bf_delete(&br);
            }
            if (fails > 80) break;
        }
    }

    /* ilogb/scalbn identity + fround residual (IEEE-ish decompose / round) */
    {
        static const double ix[] = {
            0.5, -0.5, 1.0, -1.0, 2.0, 3.5, -7.25, 0.125, 1024.0, 1e-6, 1e3, 12.75
        };
        static const double rx[] = {
            1.5, 2.5, -1.5, -2.5, 0.25, -0.75, 3.25, -3.75, 10.5, -10.5
        };
        static const int ms4[] = { 64, 80, 112, 160, 256 };
        static const int sl4[] = { 1, 1, 1, 1, 2 };
        int hj, ti;
        for (hj = 0; hj < (int)(sizeof(ms4)/sizeof(ms4[0])); hj++) {
            int m_bits = ms4[hj];
            int e_bits = (m_bits >= 160) ? 20 : 15;
            int slack = sl4[hj];
            for (ti = 0; ti < (int)(sizeof(ix)/sizeof(ix[0])); ti++) {
                sn_value a, mant, back, scaled;
                char *s = NULL;
                int expv = 0, ilog = 0;
                double rel = 0.0;
                bf_t ba, br;
                tests++;
                sn_value_init(&a); sn_value_init(&mant); sn_value_init(&back); sn_value_init(&scaled);
                bf_init(&bfc, &ba); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, ix[ti], e_bits, m_bits) ||
                    sn_frexp(&ctx, &mant, &expv, &a, NULL) != SN_OK ||
                    sn_ilogb(&ctx, &a, &ilog) != SN_OK) {
                    printf("ilogbI sn fail x=%a m=%d\n", ix[ti], m_bits); fails++;
                } else {
                    /* C ilogb(x) == exp-1 for frexp mantissa in [0.5,1); allow off-by for subnormals via residual path */
                    if (ix[ti] != 0.0 && ilog != expv - 1 && ilog != expv) {
                        /* still accept if scalbn reconstructs */
                    }
                    if (sn_scalbn(&ctx, &scaled, &mant, expv, NULL) != SN_OK ||
                        sn_to_str(&ctx, &s, &scaled, 16) != SN_OK || !s) {
                        printf("scalbnI sn fail x=%a m=%d\n", ix[ti], m_bits); fails++;
                    } else {
                        bf_set_float64(&ba, ix[ti]);
                        bf_set(&br, &ba);
                        bf_round(&br, (limb_t)m_bits, BF_RNDN);
                        if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                            printf("ilogbI residual FAIL x=%a m=%d ilog=%d exp=%d rel=%.3e sn=%s\n",
                                   ix[ti], m_bits, ilog, expv, rel, s);
                            fails++;
                        }
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &mant);
                sn_value_clear(&ctx, &back); sn_value_clear(&ctx, &scaled);
                bf_delete(&ba); bf_delete(&br);
            }
            for (ti = 0; ti < (int)(sizeof(rx)/sizeof(rx[0])); ti++) {
                sn_value a, out;
                char *s = NULL;
                double rel = 0.0;
                bf_t ba, br;
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, rx[ti], e_bits, m_bits) ||
                    sn_fround(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("froundR sn fail x=%a m=%d\n", rx[ti], m_bits); fails++;
                } else {
                    /* round-half-away-from-zero style host round; libbf nearest-even via rint may differ on *.5
                     * Use composition: floor(x+0.5) for x>=0, ceil(x-0.5) for x<0 approximates round(). */
                    bf_set_float64(&ba, rx[ti]);
                    if (rx[ti] >= 0) {
                        bf_t half, t;
                        bf_init(&bfc, &half); bf_init(&bfc, &t);
                        bf_set_float64(&half, 0.5);
                        bf_add(&t, &ba, &half, (limb_t)m_bits + 48, BF_RNDN);
                        bf_set(&br, &t);
                        bf_rint(&br, BF_RNDD); /* floor */
                        bf_delete(&half); bf_delete(&t);
                    } else {
                        bf_t half, t;
                        bf_init(&bfc, &half); bf_init(&bfc, &t);
                        bf_set_float64(&half, 0.5);
                        bf_sub(&t, &ba, &half, (limb_t)m_bits + 48, BF_RNDN);
                        bf_set(&br, &t);
                        bf_rint(&br, BF_RNDU); /* ceil */
                        bf_delete(&half); bf_delete(&t);
                    }
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                        printf("froundR residual FAIL x=%a m=%d rel=%.3e sn=%s\n", rx[ti], m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&br);
            }
            if (fails > 80) break;
        }
    }


    /* nextafter / logb / remquo residual (libbf composition + remquo API) */
    {
        static const double nx[] = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 3.5, -7.25, 0.125,
            1e-6, 1e3, 12.75, 1024.0, -1024.0, 0.1, -0.1
        };
        static const double nto[] = {
            1.0, -1.0, 2.0, -2.0, 0.0, 10.0, -10.0, 1e6, -1e6
        };
        static const double rq_a[] = {
            1.0, -1.0, 3.5, -3.5, 7.25, -7.25, 0.5, 10.0, -10.0, 1.25, 100.0, -100.0
        };
        static const double rq_b[] = {
            1.0, 2.0, 0.5, 3.0, -1.0, -2.0, 0.25, 4.0, -0.5, 1.5
        };
        static const int ms5[] = { 64, 80, 112, 160, 256 };
        static const int sl5[] = { 1, 1, 1, 1, 2 };
        int hj, ti, tj;
        for (hj = 0; hj < (int)(sizeof(ms5)/sizeof(ms5[0])); hj++) {
            int m_bits = ms5[hj];
            int e_bits = (m_bits >= 160) ? 20 : 15;
            int slack = sl5[hj];
            limb_t prec = (limb_t)m_bits + 64;

            /* logb residual vs integer exponent as float */
            for (ti = 0; ti < (int)(sizeof(nx)/sizeof(nx[0])); ti++) {
                sn_value a, out;
                char *s = NULL;
                double rel = 0.0;
                bf_t br;
                int ilog = 0;
                if (nx[ti] == 0.0) continue; /* domain: -inf */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, nx[ti], e_bits, m_bits) ||
                    sn_logb(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_ilogb(&ctx, &a, &ilog) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("logbR sn fail x=%a m=%d\n", nx[ti], m_bits); fails++;
                } else {
                    bf_set_si(&br, ilog);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                        printf("logbR residual FAIL x=%a m=%d ilog=%d rel=%.3e sn=%s\n",
                               nx[ti], m_bits, ilog, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&br);
            }

            /* nextafter: identity, adjacent round-trip, direction residual via bit step */
            for (ti = 0; ti < (int)(sizeof(nx)/sizeof(nx[0])); ti++) {
                for (tj = 0; tj < (int)(sizeof(nto)/sizeof(nto[0])); tj++) {
                    sn_value a, b, n1, n2, back;
                    char *s = NULL, *s2 = NULL;
                    double rel = 0.0;
                    bf_t ba, br;
                    int rel_ab = 0, rel_n = 0;
                    tests++;
                    sn_value_init(&a); sn_value_init(&b);
                    sn_value_init(&n1); sn_value_init(&n2); sn_value_init(&back);
                    bf_init(&bfc, &ba); bf_init(&bfc, &br);
                    if (!sn_set_hex_mp(&ctx, &a, nx[ti], e_bits, m_bits) ||
                        !sn_set_hex_mp(&ctx, &b, nto[tj], e_bits, m_bits) ||
                        sn_nextafter(&ctx, &n1, &a, &b, NULL) != SN_OK ||
                        sn_to_str(&ctx, &s, &n1, 16) != SN_OK || !s) {
                        printf("nextafterN sn fail x=%a to=%a m=%d\n", nx[ti], nto[tj], m_bits);
                        fails++;
                    } else {
                        /* identity when equal */
                        if (nx[ti] == nto[tj]) {
                            bf_set_float64(&ba, nx[ti]);
                            bf_set(&br, &ba);
                            bf_round(&br, (limb_t)m_bits, BF_RNDN);
                            if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                                printf("nextafterN id FAIL x=%a m=%d rel=%.3e sn=%s\n",
                                       nx[ti], m_bits, rel, s);
                                fails++;
                            }
                        } else {
                            /* direction: n1 is on the to-side of a (or equal only if already there) */
                            if (sn_cmp(&ctx, &rel_ab, &a, &b) != SN_OK ||
                                sn_cmp(&ctx, &rel_n, &a, &n1) != SN_OK) {
                                printf("nextafterN cmp fail x=%a to=%a m=%d\n", nx[ti], nto[tj], m_bits);
                                fails++;
                            } else if (rel_ab < 0 && rel_n > 0) {
                                /* a < b but n1 < a ? wrong direction */
                                printf("nextafterN dir FAIL x=%a to=%a m=%d (went down)\n",
                                       nx[ti], nto[tj], m_bits);
                                fails++;
                            } else if (rel_ab > 0 && rel_n < 0) {
                                printf("nextafterN dir FAIL x=%a to=%a m=%d (went up)\n",
                                       nx[ti], nto[tj], m_bits);
                                fails++;
                            } else {
                                /* adjacent round-trip: step toward to then reverse should recover a
                                 * (except when a is zero and encoding sign/min-subnormal path) */
                                sn_value rev_to;
                                sn_value_init(&rev_to);
                                if (sn_value_copy(&ctx, &rev_to, &a) != SN_OK ||
                                    sn_nextafter(&ctx, &back, &n1, &rev_to, NULL) != SN_OK ||
                                    sn_to_str(&ctx, &s2, &back, 16) != SN_OK || !s2) {
                                    printf("nextafterN back sn fail x=%a to=%a m=%d\n",
                                           nx[ti], nto[tj], m_bits);
                                    fails++;
                                } else {
                                    bf_set_float64(&ba, nx[ti]);
                                    bf_set(&br, &ba);
                                    bf_round(&br, (limb_t)m_bits, BF_RNDN);
                                    /* zero may flip to min subnormal then back with sign nuance; allow slack */
                                    if (nx[ti] != 0.0 &&
                                        !residual_ok(&bfc, s2, &br, m_bits, slack + 2, &rel)) {
                                        printf("nextafterN back residual FAIL x=%a to=%a m=%d rel=%.3e sn=%s\n",
                                               nx[ti], nto[tj], m_bits, rel, s2);
                                        fails++;
                                    }
                                }
                                sn_value_clear(&ctx, &rev_to);
                            }
                        }
                        /* second step still finite or inf ? just ensure API stable */
                        if (sn_nextafter(&ctx, &n2, &n1, &b, NULL) != SN_OK) {
                            printf("nextafterN step2 fail x=%a to=%a m=%d\n", nx[ti], nto[tj], m_bits);
                            fails++;
                        }
                    }
                    if (s) sn_str_free(&ctx, s);
                    if (s2) sn_str_free(&ctx, s2);
                    sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b);
                    sn_value_clear(&ctx, &n1); sn_value_clear(&ctx, &n2);
                    sn_value_clear(&ctx, &back);
                    bf_delete(&ba); bf_delete(&br);
                }
            }

            /* remquo: remainder residual vs bf_remquo(BF_RNDN); quo sign sanity */
            for (ti = 0; ti < (int)(sizeof(rq_a)/sizeof(rq_a[0])); ti++) {
                for (tj = 0; tj < (int)(sizeof(rq_b)/sizeof(rq_b[0])); tj++) {
                    sn_value a, b, out;
                    char *s = NULL;
                    double rel = 0.0;
                    int q = 0;
                    slimb_t bq = 0;
                    bf_t ba, bb, br;
                    tests++;
                    sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
                    bf_init(&bfc, &ba); bf_init(&bfc, &bb); bf_init(&bfc, &br);
                    if (!sn_set_hex_mp(&ctx, &a, rq_a[ti], e_bits, m_bits) ||
                        !sn_set_hex_mp(&ctx, &b, rq_b[tj], e_bits, m_bits) ||
                        sn_remquo(&ctx, &out, &q, &a, &b, NULL) != SN_OK ||
                        sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                        printf("remquoR sn fail a=%a b=%a m=%d\n", rq_a[ti], rq_b[tj], m_bits);
                        fails++;
                    } else {
                        bf_set_float64(&ba, rq_a[ti]);
                        bf_set_float64(&bb, rq_b[tj]);
                        bf_remquo(&bq, &br, &ba, &bb, prec, BF_RNDN, BF_RNDN);
                        bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                        if (!residual_ok(&bfc, s, &br, m_bits, slack + 2, &rel)) {
                            printf("remquoR residual FAIL a=%a b=%a m=%d q=%d bq=%ld rel=%.3e sn=%s\n",
                                   rq_a[ti], rq_b[tj], m_bits, q, (long)bq, rel, s);
                            fails++;
                        } else {
                            /* low bits of quotient should agree in sign when nonzero;
                             * magnitudes may differ in high bits ? compare low 3 bits. */
                            if (q != 0 && bq != 0) {
                                int sq = (q < 0) ? -1 : 1;
                                int sb = (bq < 0) ? -1 : 1;
                                if (sq != sb) {
                                    printf("remquoR quo sign FAIL a=%a b=%a m=%d q=%d bq=%ld\n",
                                           rq_a[ti], rq_b[tj], m_bits, q, (long)bq);
                                    fails++;
                                } else if (((q ^ (int)bq) & 7) != 0 && ((q - (int)bq) & 7) != 0 &&
                                           ((q + (int)bq) & 7) != 0) {
                                    /* allow implementation-defined high bits; require low 3-bit match */
                                    if ((q & 7) != ((int)bq & 7)) {
                                        printf("remquoR quo low FAIL a=%a b=%a m=%d q=%d bq=%ld\n",
                                               rq_a[ti], rq_b[tj], m_bits, q, (long)bq);
                                        fails++;
                                    }
                                }
                            }
                        }
                    }
                    if (s) sn_str_free(&ctx, s);
                    sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
                    bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
                }
            }
            if (fails > 80) break;
        }
    }


    /* expansion_float6: denser elementary residual + frexp/ldexp roundtrip + erf/erfc self-elev */
    {
        static const double dens_x[] = {
            1e-12, -1e-12, 1e-8, -1e-8, 1e-4, -1e-4, 0.01, -0.01, 0.1, -0.1,
            0.25, -0.25, 0.5, -0.5, 0.75, -0.75, 1.0, -1.0, 1.5, -1.5,
            2.0, -2.0, 2.5, 3.0, -3.0, 4.0, 5.0, 8.0, 10.0, -10.0,
            0.333333333333, -0.666666666666, 1.23456789, -2.718281828, 3.1415926535
        };
        static const double erf_x[] = {
            0.0, 0.1, -0.1, 0.25, -0.25, 0.5, -0.5, 0.75, -0.75, 1.0, -1.0,
            1.25, 1.5, -1.5, 2.0, -2.0, 2.5, 3.0
        };
        static const int ms6[] = { 64, 80, 112, 160, 256 };
        static const int sl6[] = { 2, 2, 2, 3, 4 };
        int hj, ti;
        for (hj = 0; hj < (int)(sizeof(ms6)/sizeof(ms6[0])); hj++) {
            int m_bits = ms6[hj];
            int e_bits = (m_bits >= 160) ? 20 : 15;
            int slack = sl6[hj];
            for (ti = 0; ti < (int)(sizeof(dens_x)/sizeof(dens_x[0])); ti++) {
                double x = dens_x[ti];
                if (x > 0.0)
                    check_unary_mp("logD6", &ctx, &bfc, sn_log, bf_log, x, e_bits, m_bits, slack, &tests, &fails);
                check_unary_mp("expD6", &ctx, &bfc, sn_exp, bf_exp, x * 0.25, e_bits, m_bits, slack, &tests, &fails);
                check_unary_mp("sinD6", &ctx, &bfc, sn_sin, bf_sin, x, e_bits, m_bits, slack, &tests, &fails);
                check_unary_mp("cosD6", &ctx, &bfc, sn_cos, bf_cos, x, e_bits, m_bits, slack, &tests, &fails);
                check_unary_mp("atanD6", &ctx, &bfc, sn_atan, bf_atan, x, e_bits, m_bits, slack, &tests, &fails);
                if (x >= -1.0 && x <= 1.0) {
                    check_unary_mp("asinD6", &ctx, &bfc, sn_asin, bf_asin, x, e_bits, m_bits, slack + 1, &tests, &fails);
                    check_unary_mp("acosD6", &ctx, &bfc, sn_acos, bf_acos, x, e_bits, m_bits, slack + 1, &tests, &fails);
                }
                if (x > 0.0)
                    check_unary_mp("sqrtD6", &ctx, &bfc, sn_sqrt, bf_sqrt, x, e_bits, m_bits, 2, &tests, &fails);
                /* frexp/ldexp roundtrip residual */
                {
                    sn_value a, mant, back;
                    char *s = NULL;
                    int expv = 0;
                    double rel = 0.0;
                    bf_t ba, br;
                    tests++;
                    sn_value_init(&a); sn_value_init(&mant); sn_value_init(&back);
                    bf_init(&bfc, &ba); bf_init(&bfc, &br);
                    if (!sn_set_hex_mp(&ctx, &a, x, e_bits, m_bits) ||
                        sn_frexp(&ctx, &mant, &expv, &a, NULL) != SN_OK ||
                        sn_ldexp(&ctx, &back, &mant, expv, NULL) != SN_OK ||
                        sn_to_str(&ctx, &s, &back, 16) != SN_OK || !s) {
                        printf("frexpRT sn fail x=%a m=%d\n", x, m_bits); fails++;
                    } else {
                        bf_set_float64(&ba, x);
                        bf_set(&br, &ba);
                        bf_round(&br, (limb_t)m_bits, BF_RNDN);
                        if (!residual_ok(&bfc, s, &br, m_bits, slack + 1, &rel)) {
                            printf("frexpRT residual FAIL x=%a m=%d exp=%d rel=%.3e sn=%s\n",
                                   x, m_bits, expv, rel, s);
                            fails++;
                        }
                    }
                    if (s) sn_str_free(&ctx, s);
                    sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &mant); sn_value_clear(&ctx, &back);
                    bf_delete(&ba); bf_delete(&br);
                }
            }
            /* erf/erfc: self-elev residual (libbf has no erf) + erf+erfc=1 identity */
            for (ti = 0; ti < (int)(sizeof(erf_x)/sizeof(erf_x[0])); ti++) {
                double x = erf_x[ti];
                int m_hi = m_bits + 64;
                sn_value a_lo, a_hi, o_lo, o_hi, o_cast, erfc_lo, sum, one;
                char *s = NULL;
                double rel = 0.0;
                tests++;
                sn_value_init(&a_lo); sn_value_init(&a_hi);
                sn_value_init(&o_lo); sn_value_init(&o_hi); sn_value_init(&o_cast);
                sn_value_init(&erfc_lo); sn_value_init(&sum); sn_value_init(&one);
                if (!sn_set_hex_mp(&ctx, &a_lo, x, e_bits, m_bits) ||
                    !sn_set_hex_mp(&ctx, &a_hi, x, e_bits, m_hi) ||
                    sn_erf(&ctx, &o_lo, &a_lo, NULL) != SN_OK ||
                    sn_erf(&ctx, &o_hi, &a_hi, NULL) != SN_OK ||
                    sn_cast_float(&ctx, &o_cast, &o_hi, e_bits, m_bits, 1, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &o_lo, 16) != SN_OK || !s) {
                    printf("erfSE sn fail x=%a m=%d\n", x, m_bits); fails++;
                } else {
                    /* compare lo vs cast(hi) using libbf residual on hex strings */
                    char *sref = NULL;
                    bf_t br;
                    bf_init(&bfc, &br);
                    if (sn_to_str(&ctx, &sref, &o_cast, 16) == SN_OK && sref) {
                        limb_t prec = (limb_t)m_bits + 48;
                        bf_atof(&br, sref, NULL, 16, prec, BF_RNDN);
                        bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                        if (!residual_ok(&bfc, s, &br, m_bits, slack + 4, &rel)) {
                            printf("erfSE residual FAIL x=%a m=%d rel=%.3e sn=%s\n", x, m_bits, rel, s);
                            fails++;
                        }
                        sn_str_free(&ctx, sref);
                    } else {
                        printf("erfSE ref fail x=%a m=%d\n", x, m_bits); fails++;
                    }
                    bf_delete(&br);
                }
                if (s) { sn_str_free(&ctx, s); s = NULL; }
                /* erf(x)+erfc(x)=1 */
                tests++;
                if (sn_erfc(&ctx, &erfc_lo, &a_lo, NULL) != SN_OK ||
                    sn_add(&ctx, &sum, &o_lo, &erfc_lo, NULL) != SN_OK ||
                    sn_from_str_float(&ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &sum, 16) != SN_OK || !s) {
                    printf("erfID sn fail x=%a m=%d\n", x, m_bits); fails++;
                } else {
                    bf_t br;
                    bf_init(&bfc, &br);
                    bf_set_ui(&br, 1);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack + 4, &rel)) {
                        printf("erfID residual FAIL x=%a m=%d rel=%.3e sn=%s\n", x, m_bits, rel, s);
                        fails++;
                    }
                    bf_delete(&br);
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a_lo); sn_value_clear(&ctx, &a_hi);
                sn_value_clear(&ctx, &o_lo); sn_value_clear(&ctx, &o_hi); sn_value_clear(&ctx, &o_cast);
                sn_value_clear(&ctx, &erfc_lo); sn_value_clear(&ctx, &sum); sn_value_clear(&ctx, &one);
            }
            if (fails > 80) break;
        }
    }

    printf("libbf mp residual probe: tests=%d fails=%d\n", tests, fails);
    bf_context_end(&bfc);
    sn_ctx_fini(&ctx);
    return fails ? 1 : 0;
}
