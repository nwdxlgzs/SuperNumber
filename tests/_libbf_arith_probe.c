#include "sn.h"
#include "sn_flat.h"
#include "libbf.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static void *my_realloc(void *opaque, void *ptr, size_t size) {
  (void)opaque; return realloc(ptr, size);
}

static int sn_set_d(sn_ctx *ctx, sn_value *v, double d, int e, int m) {
  char buf[64];
  snprintf(buf, sizeof buf, "%a", d);
  return sn_from_str_float(ctx, v, buf, e, m, 1, NULL) == SN_OK;
}

/* Compare via double export with tight slack for arithmetic (should be near bit-exact at m>=53). */
static int almost(double sn_d, double bf_d, int m_bits, int slack) {
  int gate = m_bits > 53 ? 53 : m_bits;
  double rel, tol, abs_tol;
  if (isnan(sn_d) && isnan(bf_d)) return 1;
  if (isinf(sn_d) && isinf(bf_d) && signbit(sn_d) == signbit(bf_d)) return 1;
  if (!isfinite(sn_d) || !isfinite(bf_d)) return 0;
  if (bf_d == 0.0) return fabs(sn_d) <= ldexp(1.0, -(gate - 2));
  rel = fabs(sn_d - bf_d) / fabs(bf_d);
  tol = ldexp(1.0, -(gate - slack));
  if (tol < 1e-15) tol = 1e-15;
  abs_tol = ldexp(1.0, -gate + (slack > 4 ? 2 : 1)) * fmax(1.0, fabs(bf_d));
  return !(rel > tol && fabs(sn_d - bf_d) > abs_tol);
}

static int check_bin(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                     sn_status (*sn_op)(sn_ctx*, sn_value*, const sn_value*, const sn_value*, const sn_op_opt*),
                     int (*bf_op)(bf_t*, const bf_t*, const bf_t*, limb_t, bf_flags_t),
                     double x, double y, int e, int m, int slack,
                     int *tests, int *fails) {
  sn_value a,b,out; bf_t ba,bb,br; double sn_d, bf_d; limb_t prec = (limb_t)m + 8;
  (*tests)++;
  sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
  bf_init(bfc, &ba); bf_init(bfc, &bb); bf_init(bfc, &br);
  if (!sn_set_d(ctx, &a, x, e, m) || !sn_set_d(ctx, &b, y, e, m) ||
      sn_op(ctx, &out, &a, &b, NULL) != SN_OK || sn_to_double(ctx, &out, &sn_d) != SN_OK) {
    printf("%s sn fail x=%a y=%a e=%d m=%d\n", name, x, y, e, m);
    (*fails)++;
    goto done;
  }
  bf_set_float64(&ba, x);
  bf_set_float64(&bb, y);
  bf_op(&br, &ba, &bb, prec, BF_RNDN);
  bf_round(&br, (limb_t)m, BF_RNDN);
  bf_get_float64(&br, &bf_d, BF_RNDN);
  if (!almost(sn_d, bf_d, m, slack)) {
    printf("%s fail x=%a y=%a e=%d m=%d sn=%a bf=%a\n", name, x, y, e, m, sn_d, bf_d);
    (*fails)++;
  }
done:
  sn_value_clear(ctx, &a); sn_value_clear(ctx, &b); sn_value_clear(ctx, &out);
  bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
  return 0;
}

static int check_un(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                    sn_status (*sn_op)(sn_ctx*, sn_value*, const sn_value*, const sn_op_opt*),
                    int (*bf_op)(bf_t*, const bf_t*, limb_t, bf_flags_t),
                    double x, int e, int m, int slack,
                    int *tests, int *fails) {
  sn_value a,out; bf_t ba,br; double sn_d, bf_d; limb_t prec = (limb_t)m + 8;
  (*tests)++;
  sn_value_init(&a); sn_value_init(&out);
  bf_init(bfc, &ba); bf_init(bfc, &br);
  if (!sn_set_d(ctx, &a, x, e, m) || sn_op(ctx, &out, &a, NULL) != SN_OK ||
      sn_to_double(ctx, &out, &sn_d) != SN_OK) {
    printf("%s sn fail x=%a e=%d m=%d\n", name, x, e, m);
    (*fails)++;
    goto done;
  }
  bf_set_float64(&ba, x);
  bf_op(&br, &ba, prec, BF_RNDN);
  bf_round(&br, (limb_t)m, BF_RNDN);
  bf_get_float64(&br, &bf_d, BF_RNDN);
  if (!almost(sn_d, bf_d, m, slack)) {
    printf("%s fail x=%a e=%d m=%d sn=%a bf=%a\n", name, x, e, m, sn_d, bf_d);
    (*fails)++;
  }
done:
  sn_value_clear(ctx, &a); sn_value_clear(ctx, &out);
  bf_delete(&ba); bf_delete(&br);
  return 0;
}

int main(void) {
  sn_ctx ctx; bf_context_t bfc;
  int tests = 0, fails = 0, i, j, ei;
  static const double xs[] = {
    0.0, -0.0, 1.0, -1.0, 2.0, 0.5, 0.1, -0.1, 3.141592653589793,
    1e-6, 1e6, 1e-200, 1e200, 0.3333333333333333, -2.5, 10.0, -7.0,
    1e-15, 1e15, 1.0 + 0x1p-52, 0x1p-1022
  };
  static const int em[][2] = {
    {31, 52}, /* forced multiprec f64-like */
    {15, 64},
    {15, 80},
    {15, 112},
    {20, 90},
    {11, 53}
  };
  uint64_t seed = 0xA5A5A5A5C0FFEEuLL;

  sn_ctx_init(&ctx);
  sn_ctx_set_round(&ctx, SN_ROUND_NTE);
  bf_context_init(&bfc, my_realloc, NULL);

  for (ei = 0; ei < (int)(sizeof em / sizeof em[0]); ei++) {
    int e = em[ei][0], m = em[ei][1];
    int n = (int)(sizeof xs / sizeof xs[0]);
    for (i = 0; i < n; i++) {
      for (j = 0; j < n; j++) {
        double x = xs[i], y = xs[j];
        /* skip ops that are pure NaN/Inf noise unless both finite-ish for tight gate */
        check_bin("add", &ctx, &bfc, sn_add, bf_add, x, y, e, m, 2, &tests, &fails);
        check_bin("sub", &ctx, &bfc, sn_sub, bf_sub, x, y, e, m, 2, &tests, &fails);
        check_bin("mul", &ctx, &bfc, sn_mul, bf_mul, x, y, e, m, 2, &tests, &fails);
        if (y != 0.0 || x == 0.0)
          check_bin("div", &ctx, &bfc, sn_div, bf_div, x, y, e, m, 3, &tests, &fails);
        if (fails > 40) goto end;
      }
      if (xs[i] >= 0.0)
        check_un("sqrt", &ctx, &bfc, sn_sqrt, bf_sqrt, xs[i], e, m, 3, &tests, &fails);
    }
    /* random finite pairs in [0.5,2) */
    for (i = 0; i < 400; i++) {
      uint64_t ua, ub; double x, y;
      seed = seed * 6364136223846793005ULL + 1;
      ua = (seed & 0x000fffffffffffffULL) | 0x3ff0000000000000ULL;
      seed = seed * 6364136223846793005ULL + 1;
      ub = (seed & 0x000fffffffffffffULL) | 0x3fe0000000000000ULL;
      memcpy(&x, &ua, 8); memcpy(&y, &ub, 8);
      check_bin("radd", &ctx, &bfc, sn_add, bf_add, x, y, e, m, 2, &tests, &fails);
      check_bin("rmul", &ctx, &bfc, sn_mul, bf_mul, x, y, e, m, 2, &tests, &fails);
      check_bin("rsub", &ctx, &bfc, sn_sub, bf_sub, x, y, e, m, 2, &tests, &fails);
      check_bin("rdiv", &ctx, &bfc, sn_div, bf_div, x, y, e, m, 3, &tests, &fails);
      if (fails > 40) goto end;
    }
  }

  /* atan2 vs libbf at multiprec */
  {
    static const double ys[] = {0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -3.0, 1e-8, -1e-8};
    static const double xs2[] = {1.0, -1.0, 0.0, -0.0, 0.5, -2.0, 1e-8, 3.0};
    int e = 15, m = 80;
    for (i = 0; i < (int)(sizeof ys / sizeof ys[0]); i++) {
      for (j = 0; j < (int)(sizeof xs2 / sizeof xs2[0]); j++) {
        sn_value a, b, out; bf_t by, bx, br; double sn_d, bf_d;
        limb_t prec = (limb_t)m + 32;
        tests++;
        sn_value_init(&a); sn_value_init(&b); sn_value_init(&out);
        bf_init(&bfc, &by); bf_init(&bfc, &bx); bf_init(&bfc, &br);
        if (!sn_set_d(&ctx, &a, ys[i], e, m) || !sn_set_d(&ctx, &b, xs2[j], e, m) ||
            sn_atan2(&ctx, &out, &a, &b, NULL) != SN_OK ||
            sn_to_double(&ctx, &out, &sn_d) != SN_OK) {
          printf("atan2 sn fail y=%a x=%a\n", ys[i], xs2[j]); fails++;
        } else {
          bf_set_float64(&by, ys[i]);
          bf_set_float64(&bx, xs2[j]);
          bf_atan2(&br, &by, &bx, prec, BF_RNDN);
          bf_round(&br, (limb_t)m, BF_RNDN);
          bf_get_float64(&br, &bf_d, BF_RNDN);
          if (!almost(sn_d, bf_d, m, 10)) {
            printf("atan2 fail y=%a x=%a sn=%a bf=%a\n", ys[i], xs2[j], sn_d, bf_d);
            fails++;
          }
        }
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &out);
        bf_delete(&by); bf_delete(&bx); bf_delete(&br);
        if (fails > 40) goto end;
      }
    }
  }

end:
  printf("libbf arith probe: tests=%d fails=%d\n", tests, fails);
  sn_ctx_fini(&ctx);
  bf_context_end(&bfc);
  return fails ? 1 : 0;
}
