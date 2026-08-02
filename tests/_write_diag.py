code = r'''/* Tight residual diagnostic */
#include " sn.h\
#include \sn_flat.h\
#include \libbf.h\
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void *my_realloc(void *o, void *p, size_t s) { (void)o; return realloc(p, s); }

static int sn_set(sn_ctx *ctx, sn_value *v, double d, int e, int m) {
 char buf[64]; snprintf(buf, sizeof(buf), \%a\, d);
 return sn_from_str_float(ctx, v, buf, e, m, 1, NULL) == SN_OK;
}

static double rel_bits(bf_context_t *bfc, const char *sn_hex, const bf_t *ref, int m) {
 bf_t snb, diff, absd, absr, rel, one;
 limb_t prec = (limb_t)m + 64;
 double rel_d = 1.0;
 bf_init(bfc, &snb); bf_init(bfc, &diff); bf_init(bfc, &absd);
 bf_init(bfc, &absr); bf_init(bfc, &rel); bf_init(bfc, &one);
 if (bf_atof(&snb, sn_hex, NULL, 16, prec, BF_RNDN) & BF_ST_MEM_ERROR) goto done;
 bf_round(&snb, (limb_t)m + 8, BF_RNDN);
 bf_sub(&diff, &snb, ref, prec, BF_RNDN);
 bf_set(&absd, &diff); if (absd.sign) absd.sign = 0;
 bf_set(&absr, ref); if (absr.sign) absr.sign = 0;
 bf_set_ui(&one, 1);
 if (bf_cmp(&absr, &one) < 0) bf_set(&absr, &one);
 bf_div(&rel, &absd, &absr, prec, BF_RNDN);
 bf_get_float64(&rel, &rel_d, BF_RNDN);
done:
 bf_delete(&snb); bf_delete(&diff); bf_delete(&absd);
 bf_delete(&absr); bf_delete(&rel); bf_delete(&one);
 if (rel_d <= 0) return 999;
 return -log2(rel_d);
}

static void un(const char *name, sn_ctx *ctx, bf_context_t *bfc,
 sn_status (*sn_op)(sn_ctx*,sn_value*,const sn_value*,const sn_op_opt*),
 int (*bf_op)(bf_t*,const bf_t*,limb_t,bf_flags_t),
 double x, int e, int m, double *worst) {
 sn_value a, out; bf_t ba, br; char *s = NULL;
 sn_value_init(&a); sn_value_init(&out);
 bf_init(bfc, &ba); bf_init(bfc, &br);
 if (!sn_set(ctx, &a, x, e, m) || sn_op(ctx, &out, &a, NULL) != SN_OK ||
 sn_to_str(ctx, &s, &out, 16) != SN_OK || !s) {
 printf(\FAIL %s x=%g m=%d sn\\n\, name, x, m);
 } else {
 limb_t prec = (limb_t)m + 48;
 bf_set_float64(&ba, x);
 bf_op(&br, &ba, prec, BF_RNDN);
 bf_round(&br, (limb_t)m, BF_RNDN);
 double bits = rel_bits(bfc, s, &br, m);
 double lost = (double)m - bits;
 if (lost > *worst) *worst = lost;
 if (lost > 2.5)
 printf(" %s x=%g m=%d correct_bits=%.1f lost=%.1f\\n\, name, x, m, bits, lost);
  }
  if (s) sn_str_free(ctx, s);
  sn_value_clear(ctx, &a); sn_value_clear(ctx, &out);
  bf_delete(&ba); bf_delete(&br);
}

int main(void) {
  sn_ctx ctx; bf_context_t bfc;
  static const double xs[] = {
    0.0, 0.1, -0.1, 0.5, 1.0, 1.5, 2.0, -0.5,
    0.01, 0.001, 3.141592653589793, 0.7853981633974483,
    10.0, -2.0, 0.25, 1e-6, -0.25, 1e-8,
    0.9, -0.9, 1.1, 5.0, -5.0, 0.3333333333333333,
    0.7071067811865476, -0.7071067811865476, 20.0, -1.5
  };
  static const int ms[] = { 64, 80, 112, 160, 256, 320 };
  int j, i;
  sn_ctx_init(&ctx); sn_ctx_set_round(&ctx, SN_ROUND_NTE);
  bf_context_init(&bfc, my_realloc, NULL);
  for (j = 0; j < 6; j++) {
    int m = ms[j], e = m >= 160 ? 20 : 15;
    double w_exp=0, w_log=0, w_sin=0, w_cos=0, w_atan=0, w_sqrt=0, w_tan=0;
    printf(\=== m=%d ===\\n\, m);
    for (i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); i++) {
      double x = xs[i];
      if (x > -700 && x < 700) un(\exp\, &ctx, &bfc, sn_exp, bf_exp, x, e, m, &w_exp);
      if (x > 0) un(\log\, &ctx, &bfc, sn_log, bf_log, x, e, m, &w_log);
      if (fabs(x) < 50) {
        un(\sin\, &ctx, &bfc, sn_sin, bf_sin, x, e, m, &w_sin);
        un(\cos\, &ctx, &bfc, sn_cos, bf_cos, x, e, m, &w_cos);
      }
      un(\atan\, &ctx, &bfc, sn_atan, bf_atan, x, e, m, &w_atan);
      if (x > 0) un(\sqrt\, &ctx, &bfc, sn_sqrt, bf_sqrt, x, e, m, &w_sqrt);
    }
    un(\tan\, &ctx, &bfc, sn_tan, bf_tan, 0.25, e, m, &w_tan);
    un(\tan\, &ctx, &bfc, sn_tan, bf_tan, 0.5, e, m, &w_tan);
    un(\tan\, &ctx, &bfc, sn_tan, bf_tan, 1.0, e, m, &w_tan);
    un(\tan\, &ctx, &bfc, sn_tan, bf_tan, -0.75, e, m, &w_tan);
    printf(\WORST lost_bits m=%d exp=%.1f log=%.1f sin=%.1f cos=%.1f atan=%.1f sqrt=%.1f tan=%.1f\\n\,
           m, w_exp, w_log, w_sin, w_cos, w_atan, w_sqrt, w_tan);
  }
  bf_context_end(&bfc); sn_ctx_fini(&ctx);
  return 0;
}
'''
open('tests/_res_diag.c','w',encoding='utf-8',newline='\n').write(code)
print('wrote', len(code))
