#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <math.h>
static void try_one(const char *s, int e, int m) {
  sn_ctx ctx; sn_value v; double d; char *str=NULL;
  sn_ctx_init(&ctx); sn_value_init(&v);
  sn_status st = sn_from_str_float(&ctx,&v,s,e,m,1,NULL);
  printf("from_str(%s,e=%d,m=%d) st=%d class=%d", s,e,m,(int)st,(int)sn_fp_classify(&v));
  if (st==SN_OK) {
    st = sn_to_double(&ctx,&v,&d);
    printf(" d=%.17g to_d_st=%d", d, (int)st);
    st = sn_to_str(&ctx,&str,&v,10);
    printf(" str=%s", str?str:"?");
    if (str) sn_str_free(&ctx,str);
  }
  printf("\n");
  sn_value_clear(&ctx,&v); sn_ctx_fini(&ctx);
}
int main(void){
  try_one("1.0", 11, 52);
  try_one("1.0", 64, 52);
  try_one("1.0", 80, 100);
  try_one("1.0", 110, 520);
  try_one("1.0", 200, 100);
  try_one("1.0", 512, 64);
  try_one("1.0", 1024, 128);
  try_one("2.5", 110, 520);
  try_one("0.5", 110, 520);
  try_one("1e10", 40, 80);
  try_one("-3.25", 100, 200);
  return 0;
}
