#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
int main(void){
  sn_ctx ctx; sn_value v; double d; char *s=NULL;
  sn_ctx_init(&ctx); sn_value_init(&v);
  sn_status st = sn_from_str_float(&ctx,&v,"1.0",110,520,1,NULL);
  printf("st=%d class=%d\n",(int)st,(int)sn_fp_classify(&v));
  st = sn_to_double(&ctx,&v,&d); printf("to_double st=%d d=%.17g\n",(int)st,d);
  st = sn_to_str(&ctx,&s,&v,10); printf("to_str st=%d s=%s\n",(int)st,s?s:"?");
  if(s) sn_str_free(&ctx,s);
  sn_value_clear(&ctx,&v); sn_ctx_fini(&ctx);
  return 0;
}
