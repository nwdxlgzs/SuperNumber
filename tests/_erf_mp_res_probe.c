/* Multiprec residual for erf/erfc (libbf has no erf).
 * Gates:
 *  1) identity: erf(x)+erfc(x) == 1
 *  2) oddness:  erf(-x) == -erf(x); erfc(-x) == 2-erfc(x)
 *  3) self-elevation: sn_erf at m vs cast(sn_erf at m+elev)
 *  4) host double erf sanity for m near 53 (optional, |x| modest)
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

/* |a-b| / max(1, |b|) via hex strings + sn ops at high prec. */
static int residual_sn(sn_ctx *ctx, const sn_value *snv, const sn_value *ref,
                       int m_bits, int slack, double *out_rel)
{
    sn_value diff, absd, absr, one, thr, two, pow2, relv;
    char *srel = NULL;
    double rel = 1e300;
    int exp_thr = m_bits - slack;
    int ok = 0;
    sn_status st;

    if (exp_thr < 8) exp_thr = 8;
    sn_value_init(&diff); sn_value_init(&absd); sn_value_init(&absr);
    sn_value_init(&one); sn_value_init(&thr); sn_value_init(&two);
    sn_value_init(&pow2); sn_value_init(&relv);

    st = sn_sub(ctx, &diff, snv, ref, NULL); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &absd, &diff, NULL); if (st != SN_OK) goto done;
    st = sn_abs(ctx, &absr, ref, NULL); if (st != SN_OK) goto done;
    st = sn_from_str_float(ctx, &one, "1.0", snv->e_bits, snv->m_bits, 1, NULL);
    if (st != SN_OK) goto done;
    /* den = max(1, |ref|) */
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

    /* thr = 2^-(m-slack) via hex float literal */
    {
        char tbuf[80];
        snprintf(tbuf, sizeof(tbuf), "0x1p-%d", exp_thr);
        st = sn_from_str_float(ctx, &thr, tbuf, snv->e_bits, snv->m_bits, 1, NULL);
        if (st != SN_OK) goto done;
    }
    (void)two;
    {
        int cmp = 0;
        if (sn_cmp(ctx, &cmp, &relv, &thr) != SN_OK) goto done;
        ok = (cmp <= 0);
    }
done:
    if (srel) sn_str_free(ctx, srel);
    sn_value_clear(ctx, &diff); sn_value_clear(ctx, &absd); sn_value_clear(ctx, &absr);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &thr); sn_value_clear(ctx, &two);
    sn_value_clear(ctx, &pow2); sn_value_clear(ctx, &relv);
    return ok;
}

static void check_identity_sum1(sn_ctx *ctx, double x, int e_bits, int m_bits, int slack)
{
    sn_value a, er, ec, sum, one;
    char *s = NULL;
    double rel = 0.0;
    tests++;
    sn_value_init(&a); sn_value_init(&er); sn_value_init(&ec);
    sn_value_init(&sum); sn_value_init(&one);
    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        sn_erf(ctx, &er, &a, NULL) != SN_OK ||
        sn_erfc(ctx, &ec, &a, NULL) != SN_OK ||
        sn_add(ctx, &sum, &er, &ec, NULL) != SN_OK ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK) {
        printf("erf+erfc sn fail x=%a m=%d\n", x, m_bits);
        fails++;
        goto done;
    }
    if (!residual_sn(ctx, &sum, &one, m_bits, slack, &rel)) {
        sn_to_str(ctx, &s, &sum, 16);
        printf("erf+erfc!=1 FAIL x=%a m=%d rel=%.3e sum=%s\n", x, m_bits, rel, s ? s : "?");
        fails++;
    }
done:
    if (s) sn_str_free(ctx, s);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &er); sn_value_clear(ctx, &ec);
    sn_value_clear(ctx, &sum); sn_value_clear(ctx, &one);
}

static void check_oddness(sn_ctx *ctx, double x, int e_bits, int m_bits, int slack)
{
    sn_value a, am, er, ern, t, ec, ecm, two, ref;
    double rel = 0.0;
    tests += 2;
    sn_value_init(&a); sn_value_init(&am); sn_value_init(&er); sn_value_init(&ern);
    sn_value_init(&t); sn_value_init(&ec); sn_value_init(&ecm);
    sn_value_init(&two); sn_value_init(&ref);
    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &am, -x, e_bits, m_bits) ||
        sn_erf(ctx, &er, &a, NULL) != SN_OK ||
        sn_erf(ctx, &ern, &am, NULL) != SN_OK ||
        sn_neg(ctx, &t, &er, NULL) != SN_OK) {
        printf("erf odd sn fail x=%a m=%d\n", x, m_bits);
        fails += 2;
        goto done;
    }
    if (!residual_sn(ctx, &ern, &t, m_bits, slack, &rel)) {
        printf("erf odd FAIL x=%a m=%d rel=%.3e\n", x, m_bits, rel);
        fails++;
    }
    if (sn_erfc(ctx, &ec, &a, NULL) != SN_OK ||
        sn_erfc(ctx, &ecm, &am, NULL) != SN_OK ||
        sn_from_str_float(ctx, &two, "2.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_sub(ctx, &ref, &two, &ec, NULL) != SN_OK) {
        printf("erfc refl sn fail x=%a m=%d\n", x, m_bits);
        fails++;
        goto done;
    }
    if (!residual_sn(ctx, &ecm, &ref, m_bits, slack, &rel)) {
        printf("erfc refl FAIL x=%a m=%d rel=%.3e\n", x, m_bits, rel);
        fails++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &am); sn_value_clear(ctx, &er);
    sn_value_clear(ctx, &ern); sn_value_clear(ctx, &t); sn_value_clear(ctx, &ec);
    sn_value_clear(ctx, &ecm); sn_value_clear(ctx, &two); sn_value_clear(ctx, &ref);
}

/* Compare sn_op(x @ m) vs cast(sn_op(x @ m+elev) -> m). */
static void check_self_elev(const char *name, sn_ctx *ctx,
                            sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                            double x, int e_bits, int m_bits, int elev, int slack)
{
    sn_value a_lo, a_hi, o_lo, o_hi, o_cast;
    double rel = 0.0;
    int m_hi = m_bits + elev;
    if (m_hi > 4096) m_hi = 4096;
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
        char *slo = NULL, *shi = NULL;
        sn_to_str(ctx, &slo, &o_lo, 16);
        sn_to_str(ctx, &shi, &o_cast, 16);
        printf("%s self-elev FAIL x=%a m=%d elev=%d rel=%.3e sn=%s ref=%s\n",
               name, x, m_bits, elev, rel, slo ? slo : "?", shi ? shi : "?");
        if (slo) sn_str_free(ctx, slo);
        if (shi) sn_str_free(ctx, shi);
        fails++;
    }
done:
    sn_value_clear(ctx, &a_lo); sn_value_clear(ctx, &a_hi);
    sn_value_clear(ctx, &o_lo); sn_value_clear(ctx, &o_hi); sn_value_clear(ctx, &o_cast);
}

static void check_host_erf(sn_ctx *ctx, double x, int e_bits, int m_bits, int slack)
{
    sn_value a, o, ref;
    double host, rel = 0.0;
    char buf[64];
    tests++;
    host = erf(x);
    sn_value_init(&a); sn_value_init(&o); sn_value_init(&ref);
    snprintf(buf, sizeof(buf), "%a", host);
    if (!sn_set_hex_mp(ctx, &a, x, e_bits, m_bits) ||
        sn_erf(ctx, &o, &a, NULL) != SN_OK ||
        sn_from_str_float(ctx, &ref, buf, e_bits, m_bits, 1, NULL) != SN_OK) {
        printf("host erf sn fail x=%a\n", x);
        fails++;
        goto done;
    }
    /* host double only ~53 bits; slack must allow that when m larger */
    {
        int sl = slack;
        if (m_bits > 53) sl = m_bits - 48; /* compare only ~48 good bits of host */
        if (sl < 4) sl = 4;
        if (!residual_sn(ctx, &o, &ref, m_bits, sl, &rel)) {
            printf("host erf FAIL x=%a m=%d rel=%.3e host=%a\n", x, m_bits, rel, host);
            fails++;
        }
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &o); sn_value_clear(ctx, &ref);
}

int main(void)
{
    sn_ctx ctx;
    static const double xs[] = {
        0.0, 0.1, -0.1, 0.25, -0.25, 0.5, 1.0, -1.0, 1.5, 2.0, -2.0,
        0.01, 0.001, 3.0, -3.0, 4.0, 0.75, -0.75, 2.5, 5.0, -5.0,
        0.3333333333333333, 1.2345, -1.2345, 7.0, 10.0, -0.5
    };
    static const int ms[] = { 64, 80, 112, 160, 256 };
    int i, j, e_bits, m_bits;

    sn_ctx_init(&ctx);
    sn_ctx_set_round(&ctx, SN_ROUND_NTE);

    for (j = 0; j < (int)(sizeof(ms) / sizeof(ms[0])); j++) {
        m_bits = ms[j];
        e_bits = (m_bits >= 160) ? 20 : 15;
        for (i = 0; i < (int)(sizeof(xs) / sizeof(xs[0])); i++) {
            double x = xs[i];
            int slack = 4; /* series/asymp residual margin */
            check_identity_sum1(&ctx, x, e_bits, m_bits, slack);
            if (x != 0.0)
                check_oddness(&ctx, x, e_bits, m_bits, slack);
            { int se = (fabs(x) >= 1.2) ? (m_bits / 2 + 16) : 12; check_self_elev("erf", &ctx, sn_erf, x, e_bits, m_bits, 160, se); }
            { int se = (fabs(x) >= 1.2) ? (m_bits / 2 + 16) : 12; check_self_elev("erfc", &ctx, sn_erfc, x, e_bits, m_bits, 160, se); }
            if (m_bits <= 80 && fabs(x) < 6.0)
                check_host_erf(&ctx, x, e_bits, m_bits, 6);
            if (fails > 60) break;
        }
        if (fails > 60) break;
    }

    printf("erf/erfc mp residual probe: tests=%d fails=%d\n", tests, fails);
    sn_ctx_fini(&ctx);
    return fails ? 1 : 0;
}




