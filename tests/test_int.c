#include <limits.h>
#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

int test_int_run(void)
{
    sn_ctx ctx;
    sn_value a, b, c;
    int64_t x;
    uint64_t ux;
    int rel;
    char *s;
    sn_op_opt opt;

    sn_ctx_init(&ctx);
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&c);

    CHECK(sn_i32(&ctx, &a, 40) == SN_OK);
    CHECK(sn_i32(&ctx, &b, 2) == SN_OK);
    CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_i64(&ctx, &c, &x) == SN_OK && x == 42);

    CHECK(sn_i64(&ctx, &a, INT64_C(-100)) == SN_OK);
    CHECK(sn_i64(&ctx, &b, INT64_C(30)) == SN_OK);
    CHECK(sn_sub(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_i64(&ctx, &c, &x) == SN_OK && x == -130);

    CHECK(sn_u64(&ctx, &a, UINT64_C(0x100000000)) == SN_OK); /* 2^32 */
    CHECK(sn_u64(&ctx, &b, UINT64_C(2)) == SN_OK);
    CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_u64(&ctx, &c, &ux) == SN_OK && ux == UINT64_C(0x200000000));

    CHECK(sn_i32(&ctx, &a, 7) == SN_OK);
    CHECK(sn_i32(&ctx, &b, 3) == SN_OK);
    CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_i64(&ctx, &c, &x) == SN_OK && x == 2);
    CHECK(sn_rem(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_i64(&ctx, &c, &x) == SN_OK && x == 1);

    /* div zero */
    sn_ctx_clear_flags(&ctx);
    CHECK(sn_i32(&ctx, &b, 0) == SN_OK);
    CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_ERR_DIVZERO);
    CHECK((sn_ctx_get_flags(&ctx) & SN_FLAG_DIVZERO) != 0);
    CHECK(sn_to_i64(&ctx, &c, &x) == SN_OK && x == 0);

    /* wrap u8 */
    sn_ctx_set_int_overflow(&ctx, SN_IOV_WRAP);
    CHECK(sn_u8(&ctx, &a, 200) == SN_OK);
    CHECK(sn_u8(&ctx, &b, 100) == SN_OK);
    CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_u64(&ctx, &c, &ux) == SN_OK && ux == 44); /* 300 mod 256 */

    /* saturate u8 */
    memset(&opt, 0, sizeof(opt));
    opt.has_int_overflow = 1;
    opt.iov = SN_IOV_SATURATE;
    sn_ctx_clear_flags(&ctx);
    CHECK(sn_u8(&ctx, &a, 200) == SN_OK);
    CHECK(sn_u8(&ctx, &b, 100) == SN_OK);
    CHECK(sn_add(&ctx, &c, &a, &b, &opt) == SN_OK);
    CHECK(sn_to_u64(&ctx, &c, &ux) == SN_OK && ux == 255);
    CHECK((sn_ctx_get_flags(&ctx) & SN_FLAG_OVERFLOW) != 0);

    /* bitwise */
    CHECK(sn_u32(&ctx, &a, 0xF0F0F0F0u) == SN_OK);
    CHECK(sn_u32(&ctx, &b, 0x0FF00FF0u) == SN_OK);
    CHECK(sn_and(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_u64(&ctx, &c, &ux) == SN_OK && ux == 0x00F000F0u);

    /* shift */
    CHECK(sn_u32(&ctx, &a, 1) == SN_OK);
    CHECK(sn_shl(&ctx, &c, &a, 8, NULL) == SN_OK);
    CHECK(sn_to_u64(&ctx, &c, &ux) == SN_OK && ux == 256);

    /* cmp */
    CHECK(sn_i32(&ctx, &a, -5) == SN_OK);
    CHECK(sn_i32(&ctx, &b, 3) == SN_OK);
    CHECK(sn_cmp(&ctx, &rel, &a, &b) == SN_OK && rel < 0);

    /* bigint */
    CHECK(sn_bigint_set_i64(&ctx, &a, INT64_C(1000000000000)) == SN_OK);
    CHECK(sn_bigint_set_i64(&ctx, &b, INT64_C(1000000000000)) == SN_OK);
    CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
    CHECK(sn_to_i64(&ctx, &c, &x) == SN_OK && x == INT64_C(2000000000000));

    /* string */
    CHECK(sn_i32(&ctx, &a, 255) == SN_OK);
    CHECK(sn_to_str(&ctx, &s, &a, 16) == SN_OK);
    CHECK(s && strcmp(s, "ff") == 0);
    sn_str_free(&ctx, s);
    CHECK(sn_from_str(&ctx, &b, "0x10", 0, 32, 1) == SN_OK);
    CHECK(sn_to_i64(&ctx, &b, &x) == SN_OK && x == 16);

    /* wide int */
    CHECK(sn_int_set_i64(&ctx, &a, 1, 96, 0) == SN_OK);
    CHECK(sn_shl(&ctx, &c, &a, 80, NULL) == SN_OK);
    CHECK(sn_bitlen(&c) == 81);


    /* very wide fixed int (memory-bound only) */
    {
        sn_value w, one, sh;
        sn_value_init(&w); sn_value_init(&one); sn_value_init(&sh);
        CHECK(sn_int_set_u64(&ctx, &one, 1, 4096, 0) == SN_OK);
        CHECK(one.width == 4096);
        CHECK(sn_shl(&ctx, &sh, &one, 4000, NULL) == SN_OK);
        CHECK(sn_bitlen(&sh) == 4001);
        CHECK(sn_int_set_u64(&ctx, &w, 3, 2048, 0) == SN_OK);
        CHECK(sn_add(&ctx, &sh, &w, &one, NULL) == SN_OK);
        /* reject absurd width that overflows limb math */
        CHECK(sn_int_new(&ctx, &w, INT_MAX, 0) != SN_OK);
        sn_value_clear(&ctx, &w);
        sn_value_clear(&ctx, &one);
        sn_value_clear(&ctx, &sh);
    }

    /* i64 preset */
    CHECK(sn_i64(&ctx, &a, INT64_MIN) == SN_OK);
    CHECK(sn_to_i64(&ctx, &a, &x) == SN_OK && x == INT64_MIN);
    CHECK(sn_u64(&ctx, &b, UINT64_MAX) == SN_OK);
    CHECK(sn_to_u64(&ctx, &b, &ux) == SN_OK && ux == UINT64_MAX);

    sn_value_clear(&ctx, &a);
    sn_value_clear(&ctx, &b);
    sn_value_clear(&ctx, &c);
    return 0;
}
