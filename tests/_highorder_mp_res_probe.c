/* Multiprec residual for high-order specials (no libbf oracle).
 * Identity + limited self-elev gates for ellip / igamma / ibeta / jacobi / I-K.
 * Pure soft paths; no asm.
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

static void check_self_elev_un(const char *name, sn_ctx *ctx,
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

/* F(phi|0)=phi, E(phi|0)=phi, Pi(0;phi|m)=F, F(pi/2|m)=K, E(pi/2|m)=E */
static void check_ellip_ids(sn_ctx *ctx, double phi, double m, int e_bits, int m_bits, int slack)
{
    sn_value vphi, vm, vn0, f, einc, piinc, k, e, halfpi;
    double rel = 0.0;
    const double pi_2 = 1.57079632679489661923;

    sn_value_init(&vphi); sn_value_init(&vm); sn_value_init(&vn0);
    sn_value_init(&f); sn_value_init(&einc); sn_value_init(&piinc);
    sn_value_init(&k); sn_value_init(&e); sn_value_init(&halfpi);

    /* m=0 identities */
    tests += 2;
    if (!sn_set_hex_mp(ctx, &vphi, phi, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &vm, 0.0, e_bits, m_bits) ||
        sn_ellipf(ctx, &f, &vphi, &vm, NULL) != SN_OK ||
        sn_ellipeinc(ctx, &einc, &vphi, &vm, NULL) != SN_OK) {
        printf("ellip m0 sn fail phi=%a m=%d\n", phi, m_bits); fails += 2;
    } else {
        if (!residual_sn(ctx, &f, &vphi, m_bits, slack, &rel)) {
            printf("ellipf m0 FAIL phi=%a m=%d rel=%.3e\n", phi, m_bits, rel); fails++;
        }
        if (!residual_sn(ctx, &einc, &vphi, m_bits, slack, &rel)) {
            printf("ellipeinc m0 FAIL phi=%a m=%d rel=%.3e\n", phi, m_bits, rel); fails++;
        }
    }

    /* Pi(n=0)=F and complete limits */
    tests += 3;
    if (!sn_set_hex_mp(ctx, &vm, m, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &vn0, 0.0, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &vphi, phi, e_bits, m_bits) ||
        sn_ellipf(ctx, &f, &vphi, &vm, NULL) != SN_OK ||
        sn_ellipiinc(ctx, &piinc, &vphi, &vn0, &vm, NULL) != SN_OK ||
        !sn_set_hex_mp(ctx, &halfpi, pi_2, e_bits, m_bits) ||
        sn_ellipf(ctx, &k, &halfpi, &vm, NULL) != SN_OK || /* reuse k as F(pi/2) */
        sn_ellipk(ctx, &e, &vm, NULL) != SN_OK) {
        printf("ellip complete/pi sn fail mparam=%a m=%d\n", m, m_bits); fails += 3;
    } else {
        if (!residual_sn(ctx, &piinc, &f, m_bits, slack + 2, &rel)) {
            printf("ellipi n0 FAIL phi=%a mparam=%a m=%d rel=%.3e\n", phi, m, m_bits, rel); fails++;
        }
        if (!residual_sn(ctx, &k, &e, m_bits, slack + 2, &rel)) {
            printf("ellipf-K FAIL mparam=%a m=%d rel=%.3e\n", m, m_bits, rel); fails++;
        }
        /* E(pi/2|m)=E(m) */
        if (sn_ellipeinc(ctx, &einc, &halfpi, &vm, NULL) != SN_OK ||
            sn_ellipe(ctx, &f, &vm, NULL) != SN_OK) {
            printf("ellipe complete sn fail mparam=%a m=%d\n", m, m_bits); fails++;
        } else if (!residual_sn(ctx, &einc, &f, m_bits, slack + 2, &rel)) {
            printf("ellipeinc-E FAIL mparam=%a m=%d rel=%.3e\n", m, m_bits, rel); fails++;
        } else {
            /* already counted in tests+=3; if path ok no extra fail */
        }
    }

    sn_value_clear(ctx, &vphi); sn_value_clear(ctx, &vm); sn_value_clear(ctx, &vn0);
    sn_value_clear(ctx, &f); sn_value_clear(ctx, &einc); sn_value_clear(ctx, &piinc);
    sn_value_clear(ctx, &k); sn_value_clear(ctx, &e); sn_value_clear(ctx, &halfpi);
}

static void check_igamma_sum1(sn_ctx *ctx, double a, double x, int e_bits, int m_bits, int slack)
{
    sn_value va, vx, p, q, sum, one;
    double rel = 0.0;
    tests++;
    sn_value_init(&va); sn_value_init(&vx); sn_value_init(&p);
    sn_value_init(&q); sn_value_init(&sum); sn_value_init(&one);
    if (!sn_set_hex_mp(ctx, &va, a, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &vx, x, e_bits, m_bits) ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_igamma(ctx, &p, &va, &vx, NULL) != SN_OK ||
        sn_igammac(ctx, &q, &va, &vx, NULL) != SN_OK ||
        sn_add(ctx, &sum, &p, &q, NULL) != SN_OK) {
        printf("igamma sum sn fail a=%a x=%a m=%d\n", a, x, m_bits); fails++;
        goto done;
    }
    if (!residual_sn(ctx, &sum, &one, m_bits, slack, &rel)) {
        printf("igamma P+Q FAIL a=%a x=%a m=%d rel=%.3e\n", a, x, m_bits, rel); fails++;
    }
done:
    sn_value_clear(ctx, &va); sn_value_clear(ctx, &vx); sn_value_clear(ctx, &p);
    sn_value_clear(ctx, &q); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &one);
}

static void check_ibeta_ids(sn_ctx *ctx, double a, double b, double x, int e_bits, int m_bits, int slack)
{
    sn_value va, vb, vx, vy, s1, s2, sum, one;
    double rel = 0.0;
    tests += 2;
    sn_value_init(&va); sn_value_init(&vb); sn_value_init(&vx); sn_value_init(&vy);
    sn_value_init(&s1); sn_value_init(&s2); sn_value_init(&sum); sn_value_init(&one);
    /* y = 1-x in multiprec (NOT host double 1.0-x): binary 0.1+0.9 != 1. */
    if (!sn_set_hex_mp(ctx, &va, a, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &vb, b, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &vx, x, e_bits, m_bits) ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_sub(ctx, &vy, &one, &vx, NULL) != SN_OK ||
        sn_ibeta(ctx, &s1, &va, &vb, &vx, NULL) != SN_OK ||
        sn_ibeta(ctx, &s2, &vb, &va, &vy, NULL) != SN_OK ||
        sn_add(ctx, &sum, &s1, &s2, NULL) != SN_OK) {
        printf("ibeta sym sn fail a=%a b=%a x=%a m=%d\n", a, b, x, m_bits); fails += 2;
        goto done;
    }
    if (!residual_sn(ctx, &sum, &one, m_bits, slack, &rel)) {
        printf("ibeta sym FAIL a=%a b=%a x=%a m=%d rel=%.3e\n", a, b, x, m_bits, rel); fails++;
    }
    if (sn_ibetac(ctx, &s2, &va, &vb, &vx, NULL) != SN_OK ||
        sn_add(ctx, &sum, &s1, &s2, NULL) != SN_OK) {
        printf("ibetac sn fail a=%a b=%a x=%a m=%d\n", a, b, x, m_bits); fails++;
        goto done;
    }
    if (!residual_sn(ctx, &sum, &one, m_bits, slack, &rel)) {
        printf("ibeta+c FAIL a=%a b=%a x=%a m=%d rel=%.3e\n", a, b, x, m_bits, rel); fails++;
    }
done:
    sn_value_clear(ctx, &va); sn_value_clear(ctx, &vb); sn_value_clear(ctx, &vx);
    sn_value_clear(ctx, &vy); sn_value_clear(ctx, &s1); sn_value_clear(ctx, &s2);
    sn_value_clear(ctx, &sum); sn_value_clear(ctx, &one);
}

/* sn^2+cn^2=1 ; dn^2 + m sn^2 = 1 */
static void check_jacobi_ids(sn_ctx *ctx, double u, double m, int e_bits, int m_bits, int slack)
{
    sn_value vu, vm, snv, cnv, dnv, t1, t2, one;
    double rel = 0.0;
    tests += 2;
    sn_value_init(&vu); sn_value_init(&vm); sn_value_init(&snv);
    sn_value_init(&cnv); sn_value_init(&dnv); sn_value_init(&t1);
    sn_value_init(&t2); sn_value_init(&one);
    if (!sn_set_hex_mp(ctx, &vu, u, e_bits, m_bits) ||
        !sn_set_hex_mp(ctx, &vm, m, e_bits, m_bits) ||
        sn_from_str_float(ctx, &one, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_jacobi_sn(ctx, &snv, &vu, &vm, NULL) != SN_OK ||
        sn_jacobi_cn(ctx, &cnv, &vu, &vm, NULL) != SN_OK ||
        sn_jacobi_dn(ctx, &dnv, &vu, &vm, NULL) != SN_OK ||
        sn_mul(ctx, &t1, &snv, &snv, NULL) != SN_OK ||
        sn_mul(ctx, &t2, &cnv, &cnv, NULL) != SN_OK ||
        sn_add(ctx, &t1, &t1, &t2, NULL) != SN_OK) {
        printf("jacobi sn fail u=%a mparam=%a m=%d\n", u, m, m_bits); fails += 2;
        goto done;
    }
    if (!residual_sn(ctx, &t1, &one, m_bits, slack, &rel)) {
        printf("jacobi sn2+cn2 FAIL u=%a mparam=%a m=%d rel=%.3e\n", u, m, m_bits, rel); fails++;
    }
    if (sn_mul(ctx, &t1, &dnv, &dnv, NULL) != SN_OK ||
        sn_mul(ctx, &t2, &snv, &snv, NULL) != SN_OK ||
        sn_mul(ctx, &t2, &t2, &vm, NULL) != SN_OK ||
        sn_add(ctx, &t1, &t1, &t2, NULL) != SN_OK) {
        printf("jacobi dn sn fail u=%a mparam=%a m=%d\n", u, m, m_bits); fails++;
        goto done;
    }
    if (!residual_sn(ctx, &t1, &one, m_bits, slack, &rel)) {
        printf("jacobi dn2+m sn2 FAIL u=%a mparam=%a m=%d rel=%.3e\n", u, m, m_bits, rel); fails++;
    }
done:
    sn_value_clear(ctx, &vu); sn_value_clear(ctx, &vm); sn_value_clear(ctx, &snv);
    sn_value_clear(ctx, &cnv); sn_value_clear(ctx, &dnv); sn_value_clear(ctx, &t1);
    sn_value_clear(ctx, &t2); sn_value_clear(ctx, &one);
}

/* Wronskian: I1(x)K0(x)+I0(x)K1(x) = 1/x  (x>0) */
static void check_ik_wronskian(sn_ctx *ctx, double x, int e_bits, int m_bits, int slack)
{
    sn_value vx, i0, i1, k0, k1, t1, t2, sum, inv;
    double rel = 0.0;
    tests++;
    sn_value_init(&vx); sn_value_init(&i0); sn_value_init(&i1);
    sn_value_init(&k0); sn_value_init(&k1); sn_value_init(&t1);
    sn_value_init(&t2); sn_value_init(&sum); sn_value_init(&inv);
    if (!sn_set_hex_mp(ctx, &vx, x, e_bits, m_bits) ||
        sn_i0(ctx, &i0, &vx, NULL) != SN_OK ||
        sn_i1(ctx, &i1, &vx, NULL) != SN_OK ||
        sn_k0(ctx, &k0, &vx, NULL) != SN_OK ||
        sn_k1(ctx, &k1, &vx, NULL) != SN_OK ||
        sn_mul(ctx, &t1, &i1, &k0, NULL) != SN_OK ||
        sn_mul(ctx, &t2, &i0, &k1, NULL) != SN_OK ||
        sn_add(ctx, &sum, &t1, &t2, NULL) != SN_OK ||
        sn_from_str_float(ctx, &inv, "1.0", e_bits, m_bits, 1, NULL) != SN_OK ||
        sn_div(ctx, &inv, &inv, &vx, NULL) != SN_OK) {
        printf("IK wronskian sn fail x=%a m=%d\n", x, m_bits); fails++;
        goto done;
    }
    if (!residual_sn(ctx, &sum, &inv, m_bits, slack, &rel)) {
        printf("IK wronskian FAIL x=%a m=%d rel=%.3e\n", x, m_bits, rel); fails++;
    }
done:
    sn_value_clear(ctx, &vx); sn_value_clear(ctx, &i0); sn_value_clear(ctx, &i1);
    sn_value_clear(ctx, &k0); sn_value_clear(ctx, &k1); sn_value_clear(ctx, &t1);
    sn_value_clear(ctx, &t2); sn_value_clear(ctx, &sum); sn_value_clear(ctx, &inv);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    sn_ctx ctx;
    /* Practical matrix: identity coverage without multi-minute ibeta blowup. */
    static const int ms[] = { 64, 80, 112, 160 };
    static const double phis[] = { 0.1, 0.3, 0.5, 0.8, 1.0, 1.2 };
    static const double ms_ell[] = { 0.05, 0.1, 0.5, 0.75, 0.9 };
    static const double ig_a[] = { 0.5, 1.0, 1.5, 2.5, 3.0 };
    static const double ig_x[] = { 0.25, 0.75, 1.0, 2.0, 3.5 };
    static const double ib_a[] = { 0.5, 1.0, 2.0 };
    static const double ib_b[] = { 0.75, 1.0, 1.5 };
    static const double ib_x[] = { 0.1, 0.5, 0.7 }; /* keep denser than original 2 pts */
    static const double ju[] = { 0.1, 0.25, 0.5, 1.0, 1.5 };
    static const double jm[] = { 0.0, 0.25, 0.5, 0.8 };
    static const double ikx[] = { 0.25, 0.5, 1.0, 2.0, 3.5, 5.0, 7.0 };
    static const double elev_x[] = { 0.25, 0.5, 0.75 };
    int i, j, k, e_bits, m_bits, slack;

    sn_ctx_init(&ctx);
    sn_ctx_set_round(&ctx, SN_ROUND_NTE);

    for (j = 0; j < (int)(sizeof(ms) / sizeof(ms[0])); j++) {
        m_bits = ms[j];
        printf("highorder m=%d ...\n", m_bits); fflush(stdout);
        e_bits = 15;
        slack = 10;
        if (m_bits >= 112) slack = 12;
        if (m_bits >= 160) slack = 14;

        for (i = 0; i < (int)(sizeof(phis) / sizeof(phis[0])); i++) {
            for (k = 0; k < (int)(sizeof(ms_ell) / sizeof(ms_ell[0])); k++)
                check_ellip_ids(&ctx, phis[i], ms_ell[k], e_bits, m_bits, slack);
        }
        for (i = 0; i < (int)(sizeof(ig_a) / sizeof(ig_a[0])); i++) {
            for (k = 0; k < (int)(sizeof(ig_x) / sizeof(ig_x[0])); k++)
                check_igamma_sum1(&ctx, ig_a[i], ig_x[k], e_bits, m_bits, slack + 2);
        }
        if (m_bits <= 112) {
            for (i = 0; i < (int)(sizeof(ib_a) / sizeof(ib_a[0])); i++) {
                for (k = 0; k < (int)(sizeof(ib_b) / sizeof(ib_b[0])); k++) {
                    int t;
                    for (t = 0; t < (int)(sizeof(ib_x) / sizeof(ib_x[0])); t++)
                        check_ibeta_ids(&ctx, ib_a[i], ib_b[k], ib_x[t], e_bits, m_bits, slack + 28); /* compose residual */
                }
            }
        }
        for (i = 0; i < (int)(sizeof(ju) / sizeof(ju[0])); i++) {
            for (k = 0; k < (int)(sizeof(jm) / sizeof(jm[0])); k++)
                check_jacobi_ids(&ctx, ju[i], jm[k], e_bits, m_bits, slack + 2);
        }
        for (i = 0; i < (int)(sizeof(ikx) / sizeof(ikx[0])); i++)
            check_ik_wronskian(&ctx, ikx[i], e_bits, m_bits, slack + 4);

        /* self-elev: moderate cost, only subset of points */
        if (m_bits <= 80) {
            for (i = 0; i < (int)(sizeof(elev_x) / sizeof(elev_x[0])); i++) {
                check_self_elev_un("ellipk", &ctx, sn_ellipk, elev_x[i], e_bits, m_bits, 48, slack + 2);
                check_self_elev_un("ellipe", &ctx, sn_ellipe, elev_x[i], e_bits, m_bits, 48, slack + 2);
                check_self_elev_un("j0", &ctx, sn_j0, elev_x[i] * 2.0, e_bits, m_bits, 48, slack + 4);
            }
        }
    }

    printf("highorder (ellip/igamma/ibeta/jacobi/IK) mp residual: tests=%d fails=%d\n", tests, fails);
    sn_ctx_fini(&ctx);
    return fails ? 1 : 0;
}
