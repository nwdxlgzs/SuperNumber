/* AGM log threshold correctness+timing probe vs libbf.
 * Rebuild libsn with -DSN_SOFT_LOG_AGM_MIN_M=N then run this probe with same N. */
#include "sn.h"
#include "sn_flat.h"
#include "libbf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

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

    if (!sn_hex || !sn_hex[0]) goto done;
    st = bf_atof(&snb, sn_hex, NULL, 16, prec, BF_RNDN);
    if (st & BF_ST_MEM_ERROR) goto done;
    bf_round(&snb, (limb_t)m_bits + 8, BF_RNDN);

    bf_sub(&diff, &snb, bf_ref, prec, BF_RNDN);
    bf_set_ui(&one, 1);
    bf_set(&absd, &diff); if (absd.sign) absd.sign = 0;
    bf_set(&absr, bf_ref); if (absr.sign) absr.sign = 0;
    if (bf_cmp(&absr, &one) < 0) bf_set(&absr, &one);
    bf_div(&rel, &absd, &absr, prec, BF_RNDN);

    bf_set_ui(&two, 2);
    bf_set_si(&pow2, -exp_thr);
    bf_pow(&thr, &two, &pow2, prec, BF_RNDN);
    if (bf_cmp(&rel, &thr) <= 0) ok = 1;
    bf_get_float64(&rel, &rel_d, BF_RNDN);
    if (out_rel) *out_rel = rel_d;
done:
    bf_delete(&snb); bf_delete(&diff); bf_delete(&absd); bf_delete(&absr);
    bf_delete(&rel); bf_delete(&thr); bf_delete(&one); bf_delete(&two); bf_delete(&pow2);
    return ok;
}

int main(void)
{
    sn_ctx ctx;
    bf_context_t bfc;
    static const int ms[] = { 160, 180, 200, 220, 240, 256, 320 };
    static const double xs[] = {
        0.5, 0.75, 0.9, 1.1, 1.5, 2.0, 3.0, 10.0, 0.1, 100.0,
        1e-6, 1e3, 1.0000001, 0.9999999, 1.41421356237, 2.71828182846
    };
    int i, j, e_bits = 20;
    int tests = 0, fails = 0;
    double max_rel = 0.0;
#ifndef SN_SOFT_LOG_AGM_MIN_M
#define SN_SOFT_LOG_AGM_MIN_M 200
#endif
    printf("AGM thr probe: SN_SOFT_LOG_AGM_MIN_M=%d\n", (int)SN_SOFT_LOG_AGM_MIN_M);

    sn_ctx_init(&ctx);
    sn_ctx_set_round(&ctx, SN_ROUND_NTE);
    bf_context_init(&bfc, my_realloc, NULL);

    for (j = 0; j < (int)(sizeof(ms)/sizeof(ms[0])); j++) {
        int m_bits = ms[j];
        int slack = (m_bits >= 256) ? 2 : 1;
        clock_t t0, t1;
        double ms_time;
        int reps = (m_bits >= 300) ? 6 : (m_bits >= 220 ? 12 : 24);
        t0 = clock();
        for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
            int r;
            for (r = 0; r < reps; r++) {
                sn_value a, out;
                bf_t ba, br;
                char *s = NULL;
                double rel = 0.0;
                limb_t prec = (limb_t)m_bits + 64;
                if (r == 0) tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &br);
                if (!sn_set_hex_mp(&ctx, &a, xs[i], e_bits, m_bits) ||
                    sn_log(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    if (r == 0) { printf("FAIL sn log x=%g m=%d\n", xs[i], m_bits); fails++; }
                } else if (r == 0) {
                    bf_set_float64(&ba, xs[i]);
                    bf_log(&br, &ba, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("FAIL residual x=%g m=%d rel=%.3e sn=%s\n", xs[i], m_bits, rel, s);
                        fails++;
                    }
                    if (rel > max_rel) max_rel = rel;
                }
                if (s) sn_str_free(&ctx, s);
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&br);
            }
        }
        t1 = clock();
        ms_time = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
        printf("m=%d slack=%d time=%.1fms unique_tests=%d max_rel_so_far=%.3e\n",
               m_bits, slack, ms_time, (int)(sizeof(xs)/sizeof(xs[0])), max_rel);
    }

    printf("agm-thr-probe: thr=%d tests=%d fails=%d max_rel=%.3e\n",
           (int)SN_SOFT_LOG_AGM_MIN_M, tests, fails, max_rel);
    bf_context_end(&bfc);
    sn_ctx_fini(&ctx);
    return fails ? 1 : 0;
}
