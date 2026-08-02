#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>

int main(void)
{
    sn_ctx ctx;
    sn_value a, b, c;
    int64_t x;
    double d;
    char *s;

    sn_ctx_init(&ctx);
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&c);

    sn_i64(&ctx, &a, 40);
    sn_i64(&ctx, &b, 2);
    sn_add(&ctx, &c, &a, &b, NULL);
    sn_to_i64(&ctx, &c, &x);
    printf("40 + 2 = %lld\n", (long long)x);

    sn_f64(&ctx, &a, 1.25);
    sn_f64(&ctx, &b, 2.5);
    sn_mul(&ctx, &c, &a, &b, NULL);
    sn_to_double(&ctx, &c, &d);
    printf("1.25 * 2.5 = %g\n", d);

    sn_to_str(&ctx, &s, &c, 10);
    printf("float bits as int? use to_str on int: ");
    sn_i32(&ctx, &a, 255);
    sn_to_str(&ctx, &s, &a, 16);
    printf("255 = 0x%s\n", s);
    sn_str_free(&ctx, s);

    sn_value_clear(&ctx, &a);
    sn_value_clear(&ctx, &b);
    sn_value_clear(&ctx, &c);
    return 0;
}
