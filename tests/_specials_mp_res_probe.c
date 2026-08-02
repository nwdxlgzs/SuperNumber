/* Multiprec residual for lgamma / tgamma / digamma (fast gate set).
 * Identities + limited self-elev. Target near full-m residual (~8 ulp slack) after Bernoulli expand + thr scale.
 */
#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests, fails;

static int sn_set_hex_mp(sn_ctx *ctx, sn_value *v, double d, int e_bits, int m_bits)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%a", d);
    return sn_from_str_float(ctx, v, buf, e_bits, m_bits, 1, NULL) == SN_OK;
}

static int residual_sn(sn_ctx *ctx, const sn_value *snv, const sn_value *ref,
                       int m_bits, int slack, double *out_rel)
{
    sn_value diff, absd, absr, one, thr, relv;
    double rel = 1e300;
    int exp_thr = m_bits - slack;
    int ok = 0;
    sn_status st;

    if (exp_thr < 8) exp_thr = 8;
    sn_value_init(&diff); sn_value_init(&absd); sn_value_init(&absr);
    sn_value_init(&one); sn_value_init(&thr); sn_value_init(&relv);

    st = sn_sub(ctx, &diff, snv, ref, NULL); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &absd, &diff, NULL); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &absr, ref, NULL); if (st != SN_OK) goto done;
    st = sn_from_str_float(ctx, &one, "1.0", snv->e_bits, snv->m_bits, 1, NULL);
    if (st != SN_OK) goto done;
    {
        int cmp = 0;
        if (sn_cmp(ctx, &cmp, &absr, &one) != SN_OK) goto done;
        if (cmp < 0) {
            sn_value_clear(ctx, &absr);
            sn_value_init(&absr);
            st = sn_value_copy(ctx, &absr, &one); if (st != SN_OK) goto done;
        }
    }
    st = sn_div(ctx, &relv, &absd, &absr, NULL); if (st != SN_OK) goto done;
    st = sn_to_double(ctx, &relv, &rel); if (st != SN_OK) goto done;
    if (out_rel) *out_rel = rel;
    {
        char tbuf[80];
        snprintf(tbuf, sizeof(tbuf), "0x1p-%d", exp_thr);
        st = sn_from_str_float(ctx, &thr, tbuf, snv->e_bits, snv->m_bits, 1, NULL);
        if (st != SN_OK) goto done;
    }
    {
        int cmp = 0;
        if (sn_cmp(ctx, &cmp, &relv, &thr) != SN_OK) goto done;
        ok = (cmp <= 0);
    }
done:
    sn_value_clear(ctx, &diff); sn_value_clear(ctx, &absd); sn_value_clear(ctx, &absr);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &thr); sn_value_clear(ctx, &relv);
    return ok;
}

static void check_self_elev(const char *name, sn_ctx *ctx,
                            sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                            double x, int e_bits, int m_bits, int elev, int slack)
{
    sn_value a_lo, a_hi, o_lo, o_hi, o_cast;
    double rel = 0.0;
    int m_hi = m_bits + elev;
    if (m_hi > 512) m_hi = 512;
    tests++;
    sn_value_init(&a_lo); sn_value_init(&a_hi);
    sn_value_init(&o_lo); sn_value_init(&o_hi); sn_value_init(&o_cast);
    if (!sn_set_hex_mp(ctx, &a_lo, x, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &a_hi, x, e_bits, m_hi) ||
        sn_op(ctx, &o_lo, &a_lo, NULL) != SN_OK ||
        sn_op(ctx, &o_hi, &a_hi, NULL) != SN_OK ||
        sn_cast_float(ctx, &o_cast, &o_hi, e_bits, m_bits, 1, NULL) != SN_OK) {
        printf("%s self-elev sn fail x=%a m=%d\n", name, x, m_bits);
        fails++;
        goto done;
    }
    if (!residual_sn(ctx, &o_lo, &o_cast, m_bits, slack, &rel)) {
        printf("%s self-elev FAIL x=%a m=%d elev=%d rel=%.3e\n", name, x, m_bits, elev, rel);
        fails++;
    }
done:
    sn_value_clear(ctx, &a_lo); sn_value_clear(ctx, &a_hi);
    sn_value_clear(ctx, &o_lo); sn_value_clear(ctx, &o_hi); sn_value_clear(ctx, &o_cast);
}

static void check_tgamma_rec(sn_ctx *ctx, double z, int e_bits, int m_bits, int slack)
{
    sn_value a, ap1, g, gp1, prod, one;
    double rel = 0.0;
    tests++;
    sn_value_init(&a); sn_value_init(&ap1); sn_value_init(&g);
    sn_value_init(&gp1); sn_value_init(&prod); sn_value_init(&one);
    /* Use multiprec a+1, not encode(z+1.0): double (z+1) is not a+1 at m>53. */
    if (!sn_set_hex_mp(ctx, &a, z, e_bits, m_bits) ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_add(ctx, &ap1, &a, &one, NULL) != SN_OK ||
        sn_tgamma(ctx, &g, &a, NULL) != SN_OK ||
        sn_tgamma(ctx, &gp1, &ap1, NULL) != SN_OK ||
        sn_mul(ctx, &prod, &a, &g, NULL) != SN_OK) {
        printf("tgamma rec sn fail z=%a m=%d\n", z, m_bits);
        fails++;
        goto done;
    }
    if (!residual_sn(ctx, &gp1, &prod, m_bits, slack, &rel)) {
        printf("tgamma rec FAIL z=%a m=%d rel=%.3e\n", z, m_bits, rel);
        fails++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &ap1); sn_value_clear(ctx, &g);
    sn_value_clear(ctx, &gp1); sn_value_clear(ctx, &prod); sn_value_clear(ctx, &one);
}

static void check_lgamma_exp(sn_ctx *ctx, double x, int e_bits, int m_bits, int slack)
{
    sn_value a, lg, elg, tg, atg;
    double rel = 0.0;
    tests++;
    sn_value_init(&a); sn_value_init(&lg); sn_value_init(&elg);
    sn_value_init(&tg); sn_value_init(&atg);
    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        sn_lgamma(ctx, &lg, &a, NULL) != SN_OK ||
        sn_exp(ctx, &elg, &lg, NULL) != SN_OK ||
        sn_tgamma(ctx, &tg, &a, NULL) != SN_OK ||
        sn_abs(ctx, &atg, &tg, NULL) != SN_OK) {
        printf("lgamma exp sn fail x=%a m=%d\n", x, m_bits);
        fails++;
        goto done;
    }
    if (!residual_sn(ctx, &elg, &atg, m_bits, slack, &rel)) {
        printf("lgamma exp FAIL x=%a m=%d rel=%.3e\n", x, m_bits, rel);
        fails++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &lg); sn_value_clear(ctx, &elg);
    sn_value_clear(ctx, &tg); sn_value_clear(ctx, &atg);
}

static void check_digamma_rec(sn_ctx *ctx, double z, int e_bits, int m_bits, int slack)
{
    sn_value a, ap1, p, pp, inv, sum, one;
    double rel = 0.0;
    tests++;
    sn_value_init(&a); sn_value_init(&ap1); sn_value_init(&p);
    sn_value_init(&pp); sn_value_init(&inv); sn_value_init(&sum); sn_value_init(&one);
    if (!sn_set_hex_mp(ctx, &a, z, e_bits, m_bits) ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_add(ctx, &ap1, &a, &one, NULL) != SN_OK ||
        sn_digamma(ctx, &p, &a, NULL) != SN_OK ||
        sn_digamma(ctx, &pp, &ap1, NULL) != SN_OK ||
        sn_div(ctx, &inv, &one, &a, NULL) != SN_OK ||
        sn_add(ctx, &sum, &p, &inv, NULL) != SN_OK) {
        printf("digamma rec sn fail z=%a m=%d\n", z, m_bits);
        fails++;
        goto done;
    }
    if (!residual_sn(ctx, &pp, &sum, m_bits, slack, &rel)) {
        printf("digamma rec FAIL z=%a m=%d rel=%.3e\n", z, m_bits, rel);
        fails++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &ap1); sn_value_clear(ctx, &p);
    sn_value_clear(ctx, &pp); sn_value_clear(ctx, &inv); sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &one);
}

static void check_digamma_refl(sn_ctx *ctx, double z, int e_bits, int m_bits, int slack)
{
    sn_value a, a1, pz, p1z, diff, pi, piz, s, c, cot, rhs, one, four;
    double rel = 0.0;
    tests++;
    sn_value_init(&a); sn_value_init(&a1); sn_value_init(&pz); sn_value_init(&p1z);
    sn_value_init(&diff); sn_value_init(&pi); sn_value_init(&piz);
    sn_value_init(&s); sn_value_init(&c); sn_value_init(&cot); sn_value_init(&rhs);
    sn_value_init(&one); sn_value_init(&four);
    /* pi = 4*atan(1) at working precision */
    if (!sn_set_hex_mp(ctx, &a, z, e_bits, m_bits) ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_sub(ctx, &a1, &one, &a, NULL) != SN_OK ||
        sn_digamma(ctx, &pz, &a, NULL) != SN_OK ||
        sn_digamma(ctx, &p1z, &a1, NULL) != SN_OK ||
        sn_sub(ctx, &diff, &p1z, &pz, NULL) != SN_OK ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_atan(ctx, &s, &one, NULL) != SN_OK ||
        sn_from_str_float(ctx, &four, "4.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_mul(ctx, &pi, &four, &s, NULL) != SN_OK ||
        sn_mul(ctx, &piz, &pi, &a, NULL) != SN_OK ||
        sn_sin(ctx, &s, &piz, NULL) != SN_OK ||
        sn_cos(ctx, &c, &piz, NULL) != SN_OK ||
        sn_div(ctx, &cot, &c, &s, NULL) != SN_OK ||
        sn_mul(ctx, &rhs, &pi, &cot, NULL) != SN_OK) {
        printf("digamma refl sn fail z=%a m=%d\n", z, m_bits);
        fails++;
        goto done;
    }
    if (!residual_sn(ctx, &diff, &rhs, m_bits, slack, &rel)) {
        printf("digamma refl FAIL z=%a m=%d rel=%.3e\n", z, m_bits, rel);
        fails++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &a1); sn_value_clear(ctx, &pz);
    sn_value_clear(ctx, &p1z); sn_value_clear(ctx, &diff); sn_value_clear(ctx, &pi);
    sn_value_clear(ctx, &piz); sn_value_clear(ctx, &s); sn_value_clear(ctx, &c);
    sn_value_clear(ctx, &cot); sn_value_clear(ctx, &rhs);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &four);
}

static void check_host_unary(const char *name, sn_ctx *ctx,
                             sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                             double (*host_op)(double),
                             double x, int e_bits, int m_bits)
{
    sn_value a, o, ref;
    double host, rel = 0.0;
    char buf[64];
    int sl;
    tests++;
    host = host_op(x);
    if (!isfinite(host)) { tests--; return; }
    sn_value_init(&a); sn_value_init(&o); sn_value_init(&ref);
    snprintf(buf, sizeof(buf), "%a", host);
    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        sn_op(ctx, &o, &a, NULL) != SN_OK ||
        sn_from_str_float(ctx, &ref, buf, e_bits, m_bits, 1, NULL) != SN_OK) {
        printf("host %s sn fail x=%a\n", name, x);
        fails++;
        goto done;
    }
    sl = 8;
    if (m_bits > 53) sl = m_bits - 46;
    if (sl < 6) sl = 6;
    if (!residual_sn(ctx, &o, &ref, m_bits, sl, &rel)) {
        printf("host %s FAIL x=%a m=%d rel=%.3e host=%a\n", name, x, m_bits, rel, host);
        fails++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &o); sn_value_clear(ctx, &ref);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    sn_ctx ctx;
    static const double xs_pos[] = { 0.6, 0.75, 0.9, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 3.5, 5.0, 6.5, 8.0, 10.0, 12.0, 0.65, 1.1, 1.35, 2.25, 4.0, 7.0, 9.5, 15.0, 20.0 };
    static const double xs_refl[] = { 0.15, 0.2, 0.3, 0.4, 0.55, 0.12, 0.25, 0.35, 0.45, 0.6 };
    static const double xs_neg[] = { -0.5, -1.5, -2.5, -0.75, -1.25, -3.5, -0.3 };
    static const int ms[] = { 64, 80, 112, 160, 256 }; /* denser args; sparse high-m */
    int i, j, e_bits, m_bits, id_slack, se_slack, refl_slack;

    sn_ctx_init(&ctx);
    sn_ctx_set_round(&ctx, SN_ROUND_NTE);

    for (j = 0; j < (int)(sizeof(ms) / sizeof(ms[0])); j++) {
        m_bits = ms[j];
        printf("specials m=%d ...\n", m_bits); fflush(stdout);
        e_bits = 15;
        /* Require ~48 good bits (Stirling multiprec still maturing). */
        /* Full-m residual honesty: denser grid; keep ~6 ulp base, relax high m. */
        /* denser args + m=320 */
        id_slack = 5;
        if (m_bits >= 256) id_slack = 6;
        if (m_bits < 80) id_slack = 7;
        if (m_bits < 64) id_slack = 9;
        se_slack = 8;
        if (m_bits < 80) se_slack = 10;
        if (m_bits < 64) se_slack = 12;
        refl_slack = 8;
        if (m_bits < 80) refl_slack = 10;
        if (m_bits < 64) refl_slack = 12;

        {
            int npos = (int)(sizeof(xs_pos) / sizeof(xs_pos[0]));
            int step = (m_bits >= 256) ? 4 : ((m_bits >= 160) ? 2 : 1);
            for (i = 0; i < npos; i += step) {
            double x = xs_pos[i];
            check_tgamma_rec(&ctx, x, e_bits, m_bits, id_slack);
            check_lgamma_exp(&ctx, x, e_bits, m_bits, id_slack + 4);
            check_digamma_rec(&ctx, x, e_bits, m_bits, id_slack);
            if (m_bits <= 80 && (i == 0 || i == 4 || i == 8)) { /* self-elev costly */
                check_self_elev("lgamma", &ctx, sn_lgamma, x, e_bits, m_bits, 64, se_slack);
                check_self_elev("tgamma", &ctx, sn_tgamma, x, e_bits, m_bits, 64, se_slack);
                check_self_elev("digamma", &ctx, sn_digamma, x, e_bits, m_bits, 64, se_slack);
            }
            if (m_bits == 64 && x > 0.5 && x < 12.0) {
                check_host_unary("lgamma", &ctx, sn_lgamma, lgamma, x, e_bits, m_bits);
                check_host_unary("tgamma", &ctx, sn_tgamma, tgamma, x, e_bits, m_bits);
            }
        }
        }
        for (i = 0; i < (int)(sizeof(xs_refl) / sizeof(xs_refl[0])); i++) {
            double z = xs_refl[i];
            check_digamma_refl(&ctx, z, e_bits, m_bits, refl_slack);
            check_tgamma_rec(&ctx, z, e_bits, m_bits, id_slack);
            check_lgamma_exp(&ctx, z, e_bits, m_bits, id_slack + 4);
        }
        for (i = 0; i < (int)(sizeof(xs_neg) / sizeof(xs_neg[0])); i++) {
            double z = xs_neg[i];
            check_tgamma_rec(&ctx, z, e_bits, m_bits, id_slack + 4);
        }
    }

    printf("specials (lgamma/tgamma/digamma) mp residual: tests=%d fails=%d\n", tests, fails);
    sn_ctx_fini(&ctx);
    return fails ? 1 : 0;
}
