/* Multiprec string/ULP gates vs libbf: uses sn_to_str (hex) + sn_str_free. */
#include "sn.h"
#include "sn_flat.h"
#include "libbf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void *my_realloc(void *opaque, void *ptr, size_t size) {
  (void)opaque;
  return realloc(ptr, size);
}

static int sn_set_d(sn_ctx *ctx, sn_value *v, double d, int e, int m) {
  char buf[64];
  snprintf(buf, sizeof buf, "%a", d);
  return sn_from_str_float(ctx, v, buf, e, m, 1, NULL) == SN_OK;
}

/* Compare SN multiprec hex string to libbf free hex-ish via relative double gate
 * after high-prec computation ? primary gate is identity + libbf float64 export
 * with tight slack; secondary is round-trip sn_to_str -> sn_from_str. */
static int check_unary_rt(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                          sn_status (*sn_op)(sn_ctx*, sn_value*, const sn_value*, const sn_op_opt*),
                          int (*bf_op)(bf_t*, const bf_t*, limb_t, bf_flags_t),
                          double x, int e, int m, int *tests, int *fails) {
  sn_value a, out, back;
  bf_t ba, br;
  char *s = NULL;
  double sn_d, bf_d, back_d;
  limb_t prec = (limb_t)m + 64;
  int ok = 1;
  (*tests)++;
  sn_value_init(&a); sn_value_init(&out); sn_value_init(&back);
  bf_init(bfc, &ba); bf_init(bfc, &br);
  if (!sn_set_d(ctx, &a, x, e, m) ||
      sn_op(ctx, &out, &a, NULL) != SN_OK) {
    printf("%s sn fail x=%a e=%d m=%d\n", name, x, e, m);
    (*fails)++;
    goto done;
  }
  /* round-trip via multiprec hex string */
  if (sn_to_str(ctx, &s, &out, 10) != SN_OK || !s) {
    printf("%s to_str fail x=%a m=%d\n", name, x, m);
    (*fails)++;
    goto done;
  }
  if (sn_from_str_float(ctx, &back, s, e, m, 1, NULL) != SN_OK) {
    printf("%s from_str fail s=%s m=%d\n", name, s, m);
    (*fails)++;
    ok = 0;
  } else if (sn_to_double(ctx, &out, &sn_d) != SN_OK ||
             sn_to_double(ctx, &back, &back_d) != SN_OK) {
    printf("%s to_double fail m=%d\n", name, m);
    (*fails)++;
    ok = 0;
  } else if (!(isnan(sn_d) && isnan(back_d)) &&
             !(isinf(sn_d) && isinf(back_d) && signbit(sn_d)==signbit(back_d)) &&
             fabs(sn_d - back_d) > ldexp(1.0, -48) * fmax(1.0, fabs(sn_d))) {
    printf("%s roundtrip fail x=%a sn=%a back=%a s=%s\n", name, x, sn_d, back_d, s);
    (*fails)++;
    ok = 0;
  }
  if (s) { sn_str_free(ctx, s); s = NULL; }

  bf_set_float64(&ba, x);
  bf_op(&br, &ba, prec, BF_RNDN);
  bf_round(&br, (limb_t)m, BF_RNDN);
  bf_get_float64(&br, &bf_d, BF_RNDN);
  if (sn_to_double(ctx, &out, &sn_d) != SN_OK) {
    printf("%s sn to_d fail\n", name);
    (*fails)++;
  } else {
    int gate = m > 53 ? 53 : m;
    double tol = ldexp(1.0, -(gate - 2));
    if (tol < 1e-15) tol = 1e-15;
    if (isnan(sn_d) && isnan(bf_d)) { /* ok */ }
    else if (isinf(sn_d) && isinf(bf_d) && signbit(sn_d)==signbit(bf_d)) { /* ok */ }
    else if (!isfinite(sn_d) || !isfinite(bf_d) ||
             (fabs(sn_d - bf_d) > tol * fmax(1.0, fabs(bf_d)) &&
              fabs(sn_d - bf_d) > ldexp(1.0, -gate + 2))) {
      printf("%s vs libbf fail x=%a sn=%a bf=%a m=%d\n", name, x, sn_d, bf_d, m);
      (*fails)++;
    }
  }
  (void)ok;
done:
  sn_value_clear(ctx, &a); sn_value_clear(ctx, &out); sn_value_clear(ctx, &back);
  bf_delete(&ba); bf_delete(&br);
  return 0;
}

static int check_bin_rt(const char *name, sn_ctx *ctx, bf_context_t *bfc,
                        sn_status (*sn_op)(sn_ctx*, sn_value*, const sn_value*, const sn_value*, const sn_op_opt*),
                        int (*bf_op)(bf_t*, const bf_t*, const bf_t*, limb_t, bf_flags_t),
                        double x, double y, int e, int m, int *tests, int *fails) {
  sn_value a, b, out, back;
  bf_t ba, bb, br;
  char *s = NULL;
  double sn_d, bf_d, back_d;
  limb_t prec = (limb_t)m + 64;
  (*tests)++;
  sn_value_init(&a); sn_value_init(&b); sn_value_init(&out); sn_value_init(&back);
  bf_init(bfc, &ba); bf_init(bfc, &bb); bf_init(bfc, &br);
  if (!sn_set_d(ctx, &a, x, e, m) || !sn_set_d(ctx, &b, y, e, m) ||
      sn_op(ctx, &out, &a, &b, NULL) != SN_OK) {
    printf("%s sn fail x=%a y=%a m=%d\n", name, x, y, m);
    (*fails)++;
    goto done;
  }
  if (sn_to_str(ctx, &s, &out, 10) != SN_OK || !s ||
      sn_from_str_float(ctx, &back, s, e, m, 1, NULL) != SN_OK ||
      sn_to_double(ctx, &out, &sn_d) != SN_OK ||
      sn_to_double(ctx, &back, &back_d) != SN_OK) {
    printf("%s str path fail m=%d s=%s\n", name, m, s ? s : "(null)");
    (*fails)++;
  } else if (!(isnan(sn_d)&&isnan(back_d)) &&
             !(isinf(sn_d)&&isinf(back_d)&&signbit(sn_d)==signbit(back_d)) &&
             fabs(sn_d-back_d) > ldexp(1.0,-48)*fmax(1.0,fabs(sn_d))) {
    printf("%s roundtrip fail sn=%a back=%a s=%s\n", name, sn_d, back_d, s);
    (*fails)++;
  }
  if (s) sn_str_free(ctx, s);
  bf_set_float64(&ba, x); bf_set_float64(&bb, y);
  bf_op(&br, &ba, &bb, prec, BF_RNDN);
  bf_round(&br, (limb_t)m, BF_RNDN);
  bf_get_float64(&br, &bf_d, BF_RNDN);
  if (sn_to_double(ctx, &out, &sn_d) == SN_OK) {
    int gate = m > 53 ? 53 : m;
    double tol = ldexp(1.0, -(gate - 2));
    if (!(isnan(sn_d)&&isnan(bf_d)) &&
        !(isinf(sn_d)&&isinf(bf_d)&&signbit(sn_d)==signbit(bf_d)) &&
        (!isfinite(sn_d)||!isfinite(bf_d) ||
         (fabs(sn_d-bf_d) > tol*fmax(1.0,fabs(bf_d)) &&
          fabs(sn_d-bf_d) > ldexp(1.0,-gate+2)))) {
      printf("%s vs libbf fail x=%a y=%a sn=%a bf=%a m=%d\n", name, x, y, sn_d, bf_d, m);
      (*fails)++;
    }
  }
done:
  sn_value_clear(ctx, &a); sn_value_clear(ctx, &b);
  sn_value_clear(ctx, &out); sn_value_clear(ctx, &back);
  bf_delete(&ba); bf_delete(&bb); bf_delete(&br);
  return 0;
}

int main(void) {
  sn_ctx ctx;
  bf_context_t bfc;
  int tests = 0, fails = 0, e = 15, m, i;
  static const double xs[] = {
    0.5, 1.0, 2.0, 0.1, 3.0, 10.0, 0.9, 1.5, 1e-6, 1e3, 0.25, -0.5, 7.0
  };
  static const int ms[] = { 64, 80, 112, 160, 256 };
  sn_ctx_init(&ctx);
  sn_ctx_set_round(&ctx, SN_ROUND_NTE);
  bf_context_init(&bfc, my_realloc, NULL);

  for (i = 0; i < (int)(sizeof(ms)/sizeof(ms[0])); i++) {
    int j;
    m = ms[i];
    for (j = 0; j < (int)(sizeof(xs)/sizeof(xs[0])); j++) {
      double x = xs[j];
      if (x > -700 && x < 700)
        check_unary_rt("exp", &ctx, &bfc, sn_exp, bf_exp, x, e, m, &tests, &fails);
      if (x > 0)
        check_unary_rt("log", &ctx, &bfc, sn_log, bf_log, x, e, m, &tests, &fails);
      check_unary_rt("sin", &ctx, &bfc, sn_sin, bf_sin, x, e, m, &tests, &fails);
      check_unary_rt("atan", &ctx, &bfc, sn_atan, bf_atan, x, e, m, &tests, &fails);
      if (fabs(x) <= 1.0)
        check_unary_rt("asin", &ctx, &bfc, sn_asin, bf_asin, x, e, m, &tests, &fails);
      if (fails > 40) break;
    }
    check_bin_rt("add", &ctx, &bfc, sn_add, bf_add, 0.1, 1.0, e, m, &tests, &fails);
    check_bin_rt("mul", &ctx, &bfc, sn_mul, bf_mul, 1.5, 2.5, e, m, &tests, &fails);
    check_bin_rt("div", &ctx, &bfc, sn_div, bf_div, 1.0, 3.0, e, m, &tests, &fails);
    check_unary_rt("sqrt", &ctx, &bfc, sn_sqrt, bf_sqrt, 2.0, e, m, &tests, &fails);
    if (fails > 40) break;
  }

  
  /* Decimal multiprec from_str vs libbf bf_atof (full digits, no strtod). */
  {
    static const char *decs[] = {
      "0.1",
      "0.2",
      "0.3",
      "1.0",
      "2.5",
      "-3.5e2",
      "1.25e-3",
      "6.02214076e23",
      "1.000000000000000000000000000001",
      "0.123456789012345678901234567890",
      "3.14159265358979323846264338327950288",
      "2.71828182845904523536028747135266250",
      "9.999999999999999999e-1",
      "1e-30",
      "1e30",
      "123456789012345678901234567890",
      "0.000000000000000000000000000001",
      "-0.1",
      "4.2e+1",
      "1.234567890123456789e-20"
    };
    static const int dms[] = { 64, 80, 112, 160, 256 };
    int di, dj;
    for (di = 0; di < (int)(sizeof(dms)/sizeof(dms[0])); di++) {
      m = dms[di];
      for (dj = 0; dj < (int)(sizeof(decs)/sizeof(decs[0])); dj++) {
        const char *ds = decs[dj];
        sn_value snv, sn_back;
        bf_t ba;
        char *s = NULL;
        double sn_d, bf_d;
        limb_t prec = (limb_t)m + 8;
        int bf_st;
        int ok = 1;
        tests++;
        sn_value_init(&snv); sn_value_init(&sn_back);
        bf_init(&bfc, &ba);
        if (sn_from_str_float(&ctx, &snv, ds, e, m, 1, NULL) != SN_OK) {
          printf("dec_atof sn fail m=%d s=%s\n", m, ds);
          fails++;
          ok = 0;
        } else {
          /* bf_atof returns IEEE-like status bits; INEXACT is normal, only MEM_ERROR is fatal */
          bf_st = bf_atof(&ba, ds, NULL, 10, prec, BF_RNDN);
          if (bf_st & BF_ST_MEM_ERROR) {
            printf("dec_atof bf memfail m=%d s=%s st=%d\n", m, ds, bf_st);
            fails++;
            ok = 0;
          } else {
            bf_round(&ba, (limb_t)m, BF_RNDN);
            if (sn_to_double(&ctx, &snv, &sn_d) != SN_OK) {
              printf("dec_atof sn export fail m=%d s=%s\n", m, ds);
              fails++;
              ok = 0;
            } else {
              /* bf_get_float64 also returns status bits; value is always written */
              bf_get_float64(&ba, &bf_d, BF_RNDN);
              {
                int gate = m > 53 ? 53 : m;
                double tol = ldexp(1.0, -(gate - 3));
                if (tol < 1e-15) tol = 1e-15;
                if (isnan(sn_d) && isnan(bf_d)) { /* ok */ }
                else if (isinf(sn_d) && isinf(bf_d) && signbit(sn_d)==signbit(bf_d)) { /* ok */ }
                else if (!isfinite(sn_d) || !isfinite(bf_d) ||
                         (fabs(sn_d - bf_d) > tol * fmax(1.0, fabs(bf_d)) &&
                          fabs(sn_d - bf_d) > ldexp(1.0, -gate + 3))) {
                  printf("dec_atof mismatch m=%d s=%s sn=%a bf=%a\n", m, ds, sn_d, bf_d);
                  fails++;
                  ok = 0;
                }
              }
            }
            if (ok && sn_to_str(&ctx, &s, &snv, 10) == SN_OK && s) {
              if (sn_from_str_float(&ctx, &sn_back, s, e, m, 1, NULL) != SN_OK) {
                printf("dec_atof hex-rt fail m=%d s=%s hex=%s\n", m, ds, s);
                fails++;
              } else {
                int rel = 0;
                if (sn_cmp(&ctx, &rel, &snv, &sn_back) != SN_OK || rel != 0) {
                  printf("dec_atof hex-rt unequal m=%d s=%s hex=%s\n", m, ds, s);
                  fails++;
                }
              }
              /* Optional: parse SN hex into a second bf and cmp bits within 1 ulp via double at high gate already;
               * for m<=112 also re-load SN hex into bf and check relative close after round. */
              if (ok && m <= 112) {
                bf_t bsn;
                char *bfhex = NULL;
                size_t plen = 0;
                bf_init(&bfc, &bsn);
                /* Load SN hex string into libbf (radix 16) for cross-check */
                if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                  int st2 = bf_atof(&bsn, s, NULL, 16, (limb_t)m + 8, BF_RNDN);
                  if (!(st2 & BF_ST_MEM_ERROR)) {
                    bf_round(&bsn, (limb_t)m, BF_RNDN);
                    /* compare ba (decimal path) vs bsn (SN hex) via float64 */
                    {
                      double d_dec, d_snhex;
                      bf_get_float64(&ba, &d_dec, BF_RNDN);
                      bf_get_float64(&bsn, &d_snhex, BF_RNDN);
                      if (isfinite(d_dec) && isfinite(d_snhex)) {
                        double tol = ldexp(1.0, -50);
                        if (fabs(d_dec - d_snhex) > tol * fmax(1.0, fabs(d_dec)) &&
                            fabs(d_dec - d_snhex) > ldexp(1.0, -50)) {
                          printf("dec_atof sn-hex vs bf-dec m=%d s=%s snhex=%s ddec=%a dsn=%a\n",
                                 m, ds, s, d_dec, d_snhex);
                          fails++;
                          ok = 0;
                        }
                      }
                    }
                  }
                }
                bf_delete(&bsn);
                (void)bfhex; (void)plen;
              }
              sn_str_free(&ctx, s); s = NULL;
            }
          }
        }
        sn_value_clear(&ctx, &snv); sn_value_clear(&ctx, &sn_back);
        bf_delete(&ba);
        if (fails > 60) break;
      }
      if (fails > 60) break;
    }
  }

  /* AGM log path */
  m = 320;
  check_unary_rt("log320", &ctx, &bfc, sn_log, bf_log, 1.5, e, m, &tests, &fails);
  check_unary_rt("log320", &ctx, &bfc, sn_log, bf_log, 0.1, e, m, &tests, &fails);
  check_unary_rt("log320", &ctx, &bfc, sn_log, bf_log, 10.0, e, m, &tests, &fails);

  printf("libbf str/ulp probe: tests=%d fails=%d\n", tests, fails);
  bf_context_end(&bfc);
  sn_ctx_fini(&ctx);
  return fails ? 1 : 0;
}
