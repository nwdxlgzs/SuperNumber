#include "sn.h"
#include "sn_flat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int tests, fails;

static int sn_set(sn_ctx *ctx, sn_value *v, double d, int e, int m) {
  char buf[64];
  sn_value_init(v);
  snprintf(buf, sizeof(buf), "%a", d);
  return sn_from_str_float(ctx, v, buf, e, m, 1, NULL) == SN_OK;
}

static int residual_ok(sn_ctx *ctx, const sn_value *a, const sn_value *b, int m, int slack, double *out_rel) {
  sn_value diff, absd, absr, one, thr, relv;
  double rel = 1e300;
  int exp_thr = m - slack;
  int ok = 0;
  sn_status st;
  if (exp_thr < 8) exp_thr = 8;
  sn_value_init(&diff); sn_value_init(&absd); sn_value_init(&absr);
  sn_value_init(&one); sn_value_init(&thr); sn_value_init(&relv);
  st = sn_sub(ctx, &diff, a, b, NULL); if (st != SN_OK) goto done;
  st = sn_abs(ctx, &absd, &diff, NULL); if (st != SN_OK) goto done;
  st = sn_abs(ctx, &absr, b, NULL); if (st != SN_OK) goto done;
  st = sn_from_str_float(ctx, &one, "1.0", a->e_bits, a->m_bits, 1, NULL); if (st != SN_OK) goto done;
  {
    int cmp = 0;
    if (sn_cmp(ctx, &cmp, &absr, &one) != SN_OK) goto done;
    if (cmp < 0) {
      sn_value_clear(ctx, &absr); sn_value_init(&absr);
      st = sn_value_copy(ctx, &absr, &one); if (st != SN_OK) goto done;
    }
  }
  st = sn_div(ctx, &relv, &absd, &absr, NULL); if (st != SN_OK) goto done;
  st = sn_to_double(ctx, &relv, &rel); if (st != SN_OK) goto done;
  if (out_rel) *out_rel = rel;
  {
    char tbuf[80];
    snprintf(tbuf, sizeof(tbuf), "0x1p-%d", exp_thr);
    st = sn_from_str_float(ctx, &thr, tbuf, a->e_bits, a->m_bits, 1, NULL); if (st != SN_OK) goto done;
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

static void check_tgamma_rec(sn_ctx *ctx, double z, int e, int m, int slack) {
  sn_value vz, vz1, gz, gz1, prod, one;
  double rel = 0;
  tests++;
  sn_value_init(&vz); sn_value_init(&vz1); sn_value_init(&gz);
  sn_value_init(&gz1); sn_value_init(&prod); sn_value_init(&one);
  if (!sn_set(ctx, &vz, z, e, m) || !sn_set(ctx, &one, 1.0, e, m)) { fails++; printf("tgamma rec sn fail z=%a m=%d\n", z, m); goto done; }
  if (sn_add(ctx, &vz1, &vz, &one, NULL) != SN_OK ||
      sn_tgamma(ctx, &gz, &vz, NULL) != SN_OK ||
      sn_tgamma(ctx, &gz1, &vz1, NULL) != SN_OK ||
      sn_mul(ctx, &prod, &gz, &vz, NULL) != SN_OK) {
    fails++; printf("tgamma rec sn fail z=%a m=%d\n", z, m); goto done;
  }
  if (!residual_ok(ctx, &gz1, &prod, m, slack, &rel)) {
    fails++; printf("tgamma rec FAIL z=%a m=%d rel=%.3e\n", z, m, rel);
  }
done:
  sn_value_clear(ctx, &vz); sn_value_clear(ctx, &vz1); sn_value_clear(ctx, &gz);
  sn_value_clear(ctx, &gz1); sn_value_clear(ctx, &prod); sn_value_clear(ctx, &one);
}

static void check_lgamma_exp(sn_ctx *ctx, double x, int e, int m, int slack) {
  sn_value vx, lg, elg, tg;
  double rel = 0;
  tests++;
  sn_value_init(&vx); sn_value_init(&lg); sn_value_init(&elg); sn_value_init(&tg);
  if (!sn_set(ctx, &vx, x, e, m) ||
      sn_lgamma(ctx, &lg, &vx, NULL) != SN_OK ||
      sn_exp(ctx, &elg, &lg, NULL) != SN_OK ||
      sn_tgamma(ctx, &tg, &vx, NULL) != SN_OK) {
    fails++; printf("lgamma exp sn fail x=%a m=%d\n", x, m); goto done;
  }
  if (!residual_ok(ctx, &elg, &tg, m, slack, &rel)) {
    fails++; printf("lgamma exp FAIL x=%a m=%d rel=%.3e\n", x, m, rel);
  }
done:
  sn_value_clear(ctx, &vx); sn_value_clear(ctx, &lg); sn_value_clear(ctx, &elg); sn_value_clear(ctx, &tg);
}

static void check_ibeta_sym(sn_ctx *ctx, double a, double b, double x, int e, int m, int slack) {
  sn_value va, vb, vx, one, y, ix, iy, sum;
  double rel = 0;
  tests++;
  sn_value_init(&va); sn_value_init(&vb); sn_value_init(&vx);
  sn_value_init(&one); sn_value_init(&y); sn_value_init(&ix);
  sn_value_init(&iy); sn_value_init(&sum);
  if (!sn_set(ctx, &va, a, e, m) || !sn_set(ctx, &vb, b, e, m) || !sn_set(ctx, &vx, x, e, m) || !sn_set(ctx, &one, 1.0, e, m)) {
    fails++; printf("ibeta sym sn fail\n"); goto done;
  }
  if (sn_sub(ctx, &y, &one, &vx, NULL) != SN_OK ||
      sn_ibeta(ctx, &ix, &va, &vb, &vx, NULL) != SN_OK ||
      sn_ibeta(ctx, &iy, &vb, &va, &y, NULL) != SN_OK ||
      sn_add(ctx, &sum, &ix, &iy, NULL) != SN_OK) {
    fails++; printf("ibeta sym sn fail a=%a b=%a x=%a m=%d\n", a, b, x, m); goto done;
  }
  if (!residual_ok(ctx, &sum, &one, m, slack, &rel)) {
    fails++; printf("ibeta sym FAIL a=%a b=%a x=%a m=%d rel=%.3e\n", a, b, x, m, rel);
  }
done:
  sn_value_clear(ctx, &va); sn_value_clear(ctx, &vb); sn_value_clear(ctx, &vx);
  sn_value_clear(ctx, &one); sn_value_clear(ctx, &y); sn_value_clear(ctx, &ix);
  sn_value_clear(ctx, &iy); sn_value_clear(ctx, &sum);
}

int main(void) {
  sn_ctx ctx;
  static const int ms[] = {64, 80, 112, 200, 256};
  static const double zs[] = {0.15, 0.5, 0.75, 1.0, 1.5, 2.0, 2.3, 3.0, 3.7, 8.25, 12.0};
  static const double ints[] = {1.0, 2.0, 3.0, 4.0, 8.0, 12.0};
  static const double ib_a[] = {0.5, 1.5, 2.0, 0.8, 3.5};
  static const double ib_b[] = {0.5, 2.5, 3.0, 1.2, 1.5};
  static const double ib_x[] = {0.25, 0.3, 0.4, 0.6, 0.7};
  int j, i, e = 16;
  setvbuf(stdout, NULL, _IONBF, 0);
  sn_ctx_init(&ctx);
  sn_ctx_set_round(&ctx, SN_ROUND_NTE);
  for (j = 0; j < (int)(sizeof(ms)/sizeof(ms[0])); j++) {
    int m = ms[j];
    int slack = (m >= 200) ? 10 : 8;
    printf(".. m=%d\n", m);
    for (i = 0; i < (int)(sizeof(zs)/sizeof(zs[0])); i++) {
      check_tgamma_rec(&ctx, zs[i], e, m, slack);
      check_lgamma_exp(&ctx, zs[i], e, m, slack + 4);
    }
    for (i = 0; i < (int)(sizeof(ints)/sizeof(ints[0])); i++)
      check_lgamma_exp(&ctx, ints[i], e, m, slack + 4);
    for (i = 0; i < 5; i++)
      check_ibeta_sym(&ctx, ib_a[i], ib_b[i], ib_x[i], e, m, slack + 28);
  }
  printf("tight residual gate: tests=%d fails=%d\n", tests, fails);
  sn_ctx_fini(&ctx);
  return fails ? 1 : 0;
}
