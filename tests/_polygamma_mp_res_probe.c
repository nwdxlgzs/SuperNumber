#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <math.h>
static int tests, fails;
static int set_hex(sn_ctx *c, sn_value *v, double d, int e, int m) {
  char b[64]; snprintf(b,sizeof b,"%a",d);
  return sn_from_str_float(c,v,b,e,m,1,NULL)==SN_OK;
}
/* trigamma rec: psi1(z) = psi1(z+1) + 1/z^2 */
static void check_tri_rec(sn_ctx *ctx, double z, int e, int m) {
  sn_value a,ap1,p,pp,one,inv,inv2,sum; double rel; tests++;
  sn_value_init(&a);sn_value_init(&ap1);sn_value_init(&p);sn_value_init(&pp);
  sn_value_init(&one);sn_value_init(&inv);sn_value_init(&inv2);sn_value_init(&sum);
  if(!set_hex(ctx,&a,z,e,m)||sn_from_str_float(ctx,&one,"1.0",e,m,1,NULL)!=SN_OK||
     sn_add(ctx,&ap1,&a,&one,NULL)!=SN_OK||sn_trigamma(ctx,&p,&a,NULL)!=SN_OK||
     sn_trigamma(ctx,&pp,&ap1,NULL)!=SN_OK||sn_div(ctx,&inv,&one,&a,NULL)!=SN_OK||
     sn_mul(ctx,&inv2,&inv,&inv,NULL)!=SN_OK||sn_add(ctx,&sum,&pp,&inv2,NULL)!=SN_OK){
    printf("tri rec sn fail z=%a m=%d\n",z,m); fails++; goto done; }
  { sn_value d,ad,ar,thr,relv; int exp_thr=m-12; if(exp_thr<8)exp_thr=8; char tb[80];
    sn_value_init(&d);sn_value_init(&ad);sn_value_init(&ar);sn_value_init(&thr);sn_value_init(&relv);
    sn_sub(ctx,&d,&p,&sum,NULL); sn_abs(ctx,&ad,&d,NULL); sn_abs(ctx,&ar,&sum,NULL);
    sn_div(ctx,&relv,&ad,&ar,NULL); sn_to_double(ctx,&relv,&rel);
    snprintf(tb,sizeof tb,"0x1p-%d",exp_thr);
    sn_from_str_float(ctx,&thr,tb,e,m,1,NULL);
    { int cmp=0; sn_cmp(ctx,&cmp,&relv,&thr);
      if(cmp>0){ printf("tri rec FAIL z=%a m=%d rel=%.3e\n",z,m,rel); fails++; }
    }
    sn_value_clear(ctx,&d);sn_value_clear(ctx,&ad);sn_value_clear(ctx,&ar);
    sn_value_clear(ctx,&thr);sn_value_clear(ctx,&relv);
  }
done:
  sn_value_clear(ctx,&a);sn_value_clear(ctx,&ap1);sn_value_clear(ctx,&p);sn_value_clear(ctx,&pp);
  sn_value_clear(ctx,&one);sn_value_clear(ctx,&inv);sn_value_clear(ctx,&inv2);sn_value_clear(ctx,&sum);
}
/* polygamma n=2: psi2(z)=psi2(z+1)-2/z^3 */
static void check_p2_rec(sn_ctx *ctx, double z, int e, int m) {
  sn_value a,ap1,p,pp,one,inv,inv3,term,sum,two; double rel; tests++;
  sn_value_init(&a);sn_value_init(&ap1);sn_value_init(&p);sn_value_init(&pp);
  sn_value_init(&one);sn_value_init(&inv);sn_value_init(&inv3);sn_value_init(&term);
  sn_value_init(&sum);sn_value_init(&two);
  if(!set_hex(ctx,&a,z,e,m)||sn_from_str_float(ctx,&one,"1.0",e,m,1,NULL)!=SN_OK||
     sn_from_str_float(ctx,&two,"2.0",e,m,1,NULL)!=SN_OK||
     sn_add(ctx,&ap1,&a,&one,NULL)!=SN_OK||sn_polygamma(ctx,&p,2,&a,NULL)!=SN_OK||
     sn_polygamma(ctx,&pp,2,&ap1,NULL)!=SN_OK||sn_div(ctx,&inv,&one,&a,NULL)!=SN_OK||
     sn_mul(ctx,&inv3,&inv,&inv,NULL)!=SN_OK||sn_mul(ctx,&inv3,&inv3,&inv,NULL)!=SN_OK||
     sn_mul(ctx,&term,&two,&inv3,NULL)!=SN_OK||sn_sub(ctx,&sum,&pp,&term,NULL)!=SN_OK){
    printf("p2 rec sn fail z=%a m=%d\n",z,m); fails++; goto done; }
  { sn_value d,ad,ar,thr,relv; int exp_thr=m-14; if(exp_thr<8)exp_thr=8; char tb[80]; int cmp=0;
    sn_value_init(&d);sn_value_init(&ad);sn_value_init(&ar);sn_value_init(&thr);sn_value_init(&relv);
    sn_sub(ctx,&d,&p,&sum,NULL); sn_abs(ctx,&ad,&d,NULL); sn_abs(ctx,&ar,&sum,NULL);
    sn_div(ctx,&relv,&ad,&ar,NULL); sn_to_double(ctx,&relv,&rel);
    snprintf(tb,sizeof tb,"0x1p-%d",exp_thr);
    sn_from_str_float(ctx,&thr,tb,e,m,1,NULL);
    sn_cmp(ctx,&cmp,&relv,&thr);
    if(cmp>0){ printf("p2 rec FAIL z=%a m=%d rel=%.3e\n",z,m,rel); fails++; }
    sn_value_clear(ctx,&d);sn_value_clear(ctx,&ad);sn_value_clear(ctx,&ar);
    sn_value_clear(ctx,&thr);sn_value_clear(ctx,&relv);
  }
done:
  sn_value_clear(ctx,&a);sn_value_clear(ctx,&ap1);sn_value_clear(ctx,&p);sn_value_clear(ctx,&pp);
  sn_value_clear(ctx,&one);sn_value_clear(ctx,&inv);sn_value_clear(ctx,&inv3);
  sn_value_clear(ctx,&term);sn_value_clear(ctx,&sum);sn_value_clear(ctx,&two);
}
int main(void){
  sn_ctx ctx; static const double xs[]={0.7,1.0,1.5,2.5,5.0,8.0};
  static const int ms[]={64,80,112}; int i,j;
  sn_ctx_init(&ctx); sn_ctx_set_round(&ctx, SN_ROUND_NTE);
  for(j=0;j<(int)(sizeof ms/sizeof ms[0]);j++)
    for(i=0;i<(int)(sizeof xs/sizeof xs[0]);i++){
      check_tri_rec(&ctx,xs[i],15,ms[j]);
      check_p2_rec(&ctx,xs[i],15,ms[j]);
    }
  printf("polygamma mp residual: tests=%d fails=%d\n", tests, fails);
  sn_ctx_fini(&ctx); return fails?1:0;
}
