#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

int test_extra_run(void)
{
    sn_ctx ctx;
    sn_value a, b, c;
    sn_api api;
    int64_t x;
    int rel;
    double d;

    sn_ctx_init(&ctx);
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&c);

    /* --- API table bind --- */
    memset(&api, 0, sizeof(api));
    sn_api_bind(&api);
    CHECK(api.ctx.init != NULL);
    CHECK(api.arith.add != NULL);
    CHECK(api.arith.sar != NULL);
    CHECK(api.flt.totalorder != NULL);
    {
        sn_ctx c2;
        api.ctx.init(&c2);
        CHECK(api.integer.i32(&c2, &a, 7) == SN_OK);
        CHECK(api.integer.i32(&c2, &b, 3) == SN_OK);
        CHECK(api.arith.add(&c2, &c, &a, &b, NULL) == SN_OK);
        CHECK(api.integer.to_i64(&c2, &c, &x) == SN_OK && x == 10);
        api.value.clear(&c2, &a);
        api.value.clear(&c2, &b);
        api.value.clear(&c2, &c);
    }

    /* --- logical vs arithmetic right shift --- */
    CHECK(sn_i32(&ctx, &a, -8) == SN_OK); /* 0xFFFFFFF8 */
    CHECK(sn_shr(&ctx, &b, &a, 1, NULL) == SN_OK); /* logical -> 0x7FFFFFFC */
    CHECK(sn_to_i64(&ctx, &b, &x) == SN_OK && x == 0x7FFFFFFC);
    CHECK(sn_sar(&ctx, &c, &a, 1, NULL) == SN_OK); /* arithmetic -> -4 */
    CHECK(sn_to_i64(&ctx, &c, &x) == SN_OK && x == -4);

    CHECK(sn_u32(&ctx, &a, 0x80000000u) == SN_OK);
    CHECK(sn_shr(&ctx, &b, &a, 1, NULL) == SN_OK);
    {
        uint64_t ux;
        CHECK(sn_to_u64(&ctx, &b, &ux) == SN_OK && ux == 0x40000000u);
        CHECK(sn_sar(&ctx, &c, &a, 1, NULL) == SN_OK); /* unsigned: same as logical */
        CHECK(sn_to_u64(&ctx, &c, &ux) == SN_OK && ux == 0x40000000u);
    }

    /* --- scientific integer strings --- */
    CHECK(sn_from_str(&ctx, &a, "1.5e3", 10, 32, 1) == SN_OK);
    CHECK(sn_to_i64(&ctx, &a, &x) == SN_OK && x == 1500);
    CHECK(sn_from_str_bigint(&ctx, &b, "2e10", 10) == SN_OK);
    CHECK(sn_to_i64(&ctx, &b, &x) == SN_OK && x == 20000000000LL);
    CHECK(sn_from_str(&ctx, &a, "1.25e2", 0, 32, 1) == SN_OK);
    CHECK(sn_to_i64(&ctx, &a, &x) == SN_OK && x == 125);
    /* non-integer scientific must fail for integer parse */
    CHECK(sn_from_str(&ctx, &a, "1.5e0", 10, 32, 1) == SN_ERR_FORMAT);

    /* --- float scientific (existing path via strtod) --- */
    CHECK(sn_from_str_float(&ctx, &a, "1.25e-1", 8, 23, 1, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
    CHECK(d > 0.124 && d < 0.126);

    /* --- wide float: total bits > 64 with m_bits<=52 --- */
    /* e=15, m=52 => 1+15+52 = 68 bits */
    CHECK(sn_float_new(&ctx, &a, 15, 52, 1) == SN_OK);
    CHECK(a.width == 68);
    CHECK(sn_float_from_i64(&ctx, &a, 42, 15, 52, 1, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &a, &d) == SN_OK && d == 42.0);
    CHECK(sn_float_set_inf(&ctx, &b, 0, 15, 52, 1) == SN_OK);
    CHECK(sn_fp_classify(&b) == SN_FP_INFINITE);
    CHECK(sn_add(&ctx, &c, &a, &a, NULL) == SN_OK);
    CHECK(sn_to_double(&ctx, &c, &d) == SN_OK && d == 84.0);

    /* totalOrder: -1 < +1, -0 < +0 ordering of signs */
    CHECK(sn_f64(&ctx, &a, -1.0) == SN_OK);
    CHECK(sn_f64(&ctx, &b, 1.0) == SN_OK);
    CHECK(sn_totalorder(&ctx, &rel, &a, &b) == SN_OK && rel < 0);
    CHECK(sn_totalorder(&ctx, &rel, &b, &a) == SN_OK && rel > 0);

    /* bigint mul correctness larger numbers */
    CHECK(sn_from_str_bigint(&ctx, &a, "12345678901234567890", 10) == SN_OK);
    CHECK(sn_from_str_bigint(&ctx, &b, "98765432109876543210", 10) == SN_OK);
    CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
    {
        char *s = NULL;
        CHECK(sn_to_str(&ctx, &s, &c, 10) == SN_OK);
        CHECK(s != NULL);
        /* 12345678901234567890 * 98765432109876543210
         * = 1219326311370217952237463801111263526900 */
        CHECK(strcmp(s, "1219326311370217952237463801111263526900") == 0);
        sn_str_free(&ctx, s);
    }

    /* --- multiprec float m_bits > 52 --- */
    /* cast_float / from_num must reach multiprec path (m>52) */
    {
        double d;
        CHECK(sn_f64(&ctx, &a, 1.75) == SN_OK);
        CHECK(sn_cast_float(&ctx, &b, &a, 15, 80, 1, NULL) == SN_OK);
        CHECK(b.m_bits == 80);
        CHECK(sn_to_double(&ctx, &b, &d) == SN_OK && fabs(d - 1.75) < 1e-15);
        CHECK(sn_exp(&ctx, &c, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(fabs(d - exp(1.75)) < 1e-9);
    }

    /* e=15, m=80 => total 96 bits; pure soft multiprec */
    {
        int rel;
        double d;
        CHECK(sn_float_from_i64(&ctx, &a, 1000, 15, 80, 1, NULL) == SN_OK);
        CHECK(a.m_bits == 80);
        CHECK(sn_fp_classify(&a) == SN_FP_NORMAL);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK && d == 1000.0);
        CHECK(sn_float_from_i64(&ctx, &b, 7, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK && d == 1007.0);
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK && d == 7000.0);
        CHECK(sn_sub(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK && d == 993.0);
        CHECK(sn_div(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK);
        CHECK(d > 142.8 && d < 142.9); /* 1000/7 */
        CHECK(sn_cmp(&ctx, &rel, &a, &b) == SN_OK && rel > 0);
        CHECK(sn_float_from_i64(&ctx, &a, -5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_fp_signbit(&a) == 1);
        CHECK(sn_float_set_inf(&ctx, &b, 0, 15, 80, 1) == SN_OK);
        CHECK(sn_fp_classify(&b) == SN_FP_INFINITE);
        CHECK(sn_float_set_nan(&ctx, &c, 15, 80) == SN_OK);
        CHECK(sn_fp_classify(&c) == SN_FP_NAN);
        /* exact integer that fits in 80-bit mantissa */
        CHECK(sn_float_from_i64(&ctx, &a, 123456789012345LL, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &a, &d) == SN_OK);
        CHECK(d == (double)123456789012345LL);
        /* a+a */
        CHECK(sn_float_from_i64(&ctx, &a, 42, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &c, &a, &a, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK && d == 84.0);
        /* cancellation */
        CHECK(sn_float_from_i64(&ctx, &a, 100, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, 100, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_sub(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_fp_classify(&c) == SN_FP_ZERO);
        /* cmp: equal, reverse, negatives, power-of-two scale */
        CHECK(sn_float_from_i64(&ctx, &a, 1000, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, 7, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &b, &a) == SN_OK && rel < 0);
        CHECK(sn_cmp(&ctx, &rel, &a, &a) == SN_OK && rel == 0);
        CHECK(sn_float_from_i64(&ctx, &a, -3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, -10, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &a, &b) == SN_OK && rel > 0); /* -3 > -10 */
        CHECK(sn_float_from_i64(&ctx, &a, 1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &c, &b) == SN_OK && rel == 0);
        /* wider mantissa m=128 */
        CHECK(sn_float_from_i64(&ctx, &a, 999999, 11, 128, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &b, 3, 11, 128, 1, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK && d == 2999997.0);
        CHECK(sn_div(&ctx, &c, &c, &b, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &c, &d) == SN_OK && d == 999999.0);
        CHECK(sn_cmp(&ctx, &rel, &a, &b) == SN_OK && rel > 0);
    }

    /* multi-limb div: (2^96-1) / (2^32+1) style and rem consistency */
    {
        sn_value q, rem, prod, sum;
        char *s = NULL;
        sn_value_init(&q); sn_value_init(&rem); sn_value_init(&prod); sn_value_init(&sum);
        CHECK(sn_from_str_bigint(&ctx, &a, "79228162514264337593543950335", 10) == SN_OK); /* 2^96-1 */
        CHECK(sn_from_str_bigint(&ctx, &b, "4294967297", 10) == SN_OK); /* 2^32+1 */
        CHECK(sn_div(&ctx, &q, &a, &b, NULL) == SN_OK);
        CHECK(sn_rem(&ctx, &rem, &a, &b, NULL) == SN_OK);
        CHECK(sn_mul(&ctx, &prod, &q, &b, NULL) == SN_OK);
        CHECK(sn_add(&ctx, &sum, &prod, &rem, NULL) == SN_OK);
        {
            int rel;
            CHECK(sn_cmp(&ctx, &rel, &sum, &a) == SN_OK && rel == 0);
        }
        /* single-limb * multi fast path: 0 * big */
        CHECK(sn_from_str_bigint(&ctx, &a, "0", 10) == SN_OK);
        CHECK(sn_from_str_bigint(&ctx, &b, "123456789012345678901234567890", 10) == SN_OK);
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_to_str(&ctx, &s, &c, 10) == SN_OK);
        CHECK(s && strcmp(s, "0") == 0);
        sn_str_free(&ctx, s); s = NULL;
        /* 1-limb * multi */
        CHECK(sn_from_str_bigint(&ctx, &a, "1000000007", 10) == SN_OK);
        CHECK(sn_from_str_bigint(&ctx, &b, "999999999999999999999999", 10) == SN_OK);
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &q, &c, &a, NULL) == SN_OK);
        {
            int rel;
            CHECK(sn_cmp(&ctx, &rel, &q, &b) == SN_OK && rel == 0);
        }
        sn_value_clear(&ctx, &q); sn_value_clear(&ctx, &rem);
        sn_value_clear(&ctx, &prod); sn_value_clear(&ctx, &sum);
    }

    /* Karatsuba-path mul: operands with >=16 limbs (~512+ bits) */
    {
        sn_value q, rem, prod, sum, one;
        char *s = NULL;
        int rel;
        sn_value_init(&q); sn_value_init(&rem); sn_value_init(&prod);
        sn_value_init(&sum); sn_value_init(&one);
        /* (2^600-1) * (2^600+1) = 2^1200 - 1 */
        CHECK(sn_from_str_bigint(&ctx, &a,
            "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
            0) == SN_OK);
        /* build 2^600-1 via string of 150 hex f? 600/4=150 hex digits of f */
        /* simpler: square a large number and check a*b via div roundtrip */
        CHECK(sn_from_str_bigint(&ctx, &a,
            "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",
            10) == SN_OK);
        CHECK(sn_from_str_bigint(&ctx, &b,
            "98765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210",
            10) == SN_OK);
        CHECK(a.nlimbs >= 8 || b.nlimbs >= 8); /* large enough to stress mul */
        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &q, &c, &a, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &q, &b) == SN_OK && rel == 0);
        CHECK(sn_rem(&ctx, &rem, &c, &a, NULL) == SN_OK);
        CHECK(sn_to_str(&ctx, &s, &rem, 10) == SN_OK);
        CHECK(s && strcmp(s, "0") == 0);
        sn_str_free(&ctx, s); s = NULL;
        /* (2^n-1)^2 via repeated mul of all-ones multi-limb */
        CHECK(sn_from_str_bigint(&ctx, &a, "0x" "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff"
            "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff"
            "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff"
            "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff", 0) == SN_OK);
        /* 16*16 hex digits of f = 256 hex = 1024 bits = 32 limbs -> definitely Karatsuba */
        CHECK(a.nlimbs >= SN_INLINE_LIMBS);
        CHECK(sn_mul(&ctx, &c, &a, &a, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &q, &c, &a, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &q, &a) == SN_OK && rel == 0);
        sn_value_clear(&ctx, &q); sn_value_clear(&ctx, &rem);
        sn_value_clear(&ctx, &prod); sn_value_clear(&ctx, &sum); sn_value_clear(&ctx, &one);
    }

    /* NTT-path mul: both operands >= 64 limbs — div roundtrip + identities */
    {
        sn_value q, rem, prod, one, big;
        char *hex = NULL;
        int rel, i;
        size_t nhex;
        sn_value_init(&q); sn_value_init(&rem); sn_value_init(&prod);
        sn_value_init(&one); sn_value_init(&big);
        /* 70 limbs * 8 hex digits = 560 hex of 'f' => >=64 limbs */
        nhex = 70u * 8u;
        hex = (char *)malloc(nhex + 3);
        CHECK(hex != NULL);
        hex[0] = '0'; hex[1] = 'x';
        for (i = 0; i < (int)nhex; i++) hex[2 + i] = 'f';
        hex[2 + nhex] = '\0';
        CHECK(sn_from_str_bigint(&ctx, &a, hex, 0) == SN_OK);
        CHECK(a.nlimbs >= 64);
        for (i = 0; i < (int)nhex; i++)
            hex[2 + i] = (i % 2 == 0) ? 'a' : '5';
        CHECK(sn_from_str_bigint(&ctx, &b, hex, 0) == SN_OK);
        CHECK(b.nlimbs >= 64);
        free(hex); hex = NULL;

        CHECK(sn_mul(&ctx, &c, &a, &b, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &q, &c, &a, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &q, &b) == SN_OK && rel == 0);
        CHECK(sn_rem(&ctx, &rem, &c, &a, NULL) == SN_OK);
        {
            char *s = NULL;
            CHECK(sn_to_str(&ctx, &s, &rem, 10) == SN_OK);
            CHECK(s && strcmp(s, "0") == 0);
            sn_str_free(&ctx, s);
        }
        /* square all-ones multi-limb */
        CHECK(sn_mul(&ctx, &c, &a, &a, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &q, &c, &a, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &q, &a) == SN_OK && rel == 0);
        /* *1 identity */
        CHECK(sn_from_str_bigint(&ctx, &one, "1", 10) == SN_OK);
        CHECK(sn_mul(&ctx, &prod, &a, &one, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &prod, &a) == SN_OK && rel == 0);
        CHECK(sn_mul(&ctx, &big, &c, &one, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &big, &c) == SN_OK && rel == 0);
        /* vs basecase path: small square then grow — 32-limb already karatsuba; 64+ NTT */
        {
            sn_value s32, s64, p32, p64, two;
            sn_value_init(&s32); sn_value_init(&s64); sn_value_init(&p32);
            sn_value_init(&p64); sn_value_init(&two);
            CHECK(sn_from_str_bigint(&ctx, &s32,
                "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 0) == SN_OK);
            CHECK(sn_mul(&ctx, &p32, &s32, &s32, NULL) == SN_OK);
            CHECK(sn_div(&ctx, &q, &p32, &s32, NULL) == SN_OK);
            CHECK(sn_cmp(&ctx, &rel, &q, &s32) == SN_OK && rel == 0);
            sn_value_clear(&ctx, &s32); sn_value_clear(&ctx, &s64);
            sn_value_clear(&ctx, &p32); sn_value_clear(&ctx, &p64);
            sn_value_clear(&ctx, &two);
        }
        sn_value_clear(&ctx, &q); sn_value_clear(&ctx, &rem);
        sn_value_clear(&ctx, &prod); sn_value_clear(&ctx, &one);
        sn_value_clear(&ctx, &big);
    }

    /* soft constant cache: second sin/log should reuse cached pi/ln2; sn_ctx_fini frees */
    {
        sn_value x, y, z;
        double d1, d2;
        sn_value_init(&x); sn_value_init(&y); sn_value_init(&z);
        CHECK(sn_float_from_i64(&ctx, &x, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_log(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_log(&ctx, &z, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d1) == SN_OK);
        CHECK(sn_to_double(&ctx, &z, &d2) == SN_OK);
        CHECK(d1 == d2);
        CHECK(sn_float_from_i64(&ctx, &x, -1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_acos(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_sin(&ctx, &z, &y, NULL) == SN_OK);
        CHECK(sn_sin(&ctx, &z, &y, NULL) == SN_OK); /* second hit uses cached pi */
        CHECK(sn_to_double(&ctx, &z, &d1) == SN_OK);
        CHECK(d1 > -1e-10 && d1 < 1e-10);
        sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &z);
        sn_ctx_fini(&ctx);
        sn_ctx_init(&ctx); /* re-init for remaining tests */
    }


    /* multiprec soft erf / tgamma (m_bits=80) */
    {
        sn_value x, y;
        double d;
        sn_value_init(&x); sn_value_init(&y);
        CHECK(sn_float_from_i64(&ctx, &x, 0, 15, 80, 1, NULL) == SN_OK);
        /* x = 0.5 via 1/2 */
        {
            sn_value half, one, two;
            sn_value_init(&half); sn_value_init(&one); sn_value_init(&two);
            CHECK(sn_float_from_i64(&ctx, &one, 1, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_float_from_i64(&ctx, &two, 2, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_div(&ctx, &x, &one, &two, NULL) == SN_OK);
            CHECK(sn_erf(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
            CHECK(fabs(d - 0.5204998778130465) < 1e-10);
            CHECK(sn_erfc(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
            CHECK(fabs(d - 0.4795001221869535) < 1e-10);
            /* tgamma(5) = 24 */
            CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_tgamma(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
            CHECK(fabs(d - 24.0) < 1e-8);
            /* tgamma(0.5) ~ sqrt(pi) */
            CHECK(sn_div(&ctx, &x, &one, &two, NULL) == SN_OK);
            CHECK(sn_tgamma(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
            CHECK(fabs(d - 1.772453850905516) < 1e-8);
            /* lgamma(5) = ln(24) */
            CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_lgamma(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
            CHECK(fabs(d - log(24.0)) < 1e-8);
            /* erf large ~ 1 */
            CHECK(sn_float_from_i64(&ctx, &x, 3, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_erf(&ctx, &y, &x, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
            CHECK(d > 0.9999 && d <= 1.0);
            sn_value_clear(&ctx, &half); sn_value_clear(&ctx, &one); sn_value_clear(&ctx, &two);
        }
        sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &y);
    }

    /* multiprec soft integer-round / frexp / ldexp / nextafter / cbrt / exp2 / fmod */
    {
        sn_value x, y, z, ip, fp;
        double d;
        int e, rel;
        sn_value_init(&x); sn_value_init(&y); sn_value_init(&z);
        sn_value_init(&ip); sn_value_init(&fp);
        /* ceil/floor/trunc on 2.5 */
        CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* 2.5 */
        CHECK(sn_ceil(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 3.0) < 1e-12);
        CHECK(sn_floor(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 2.0) < 1e-12);
        CHECK(sn_trunc(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 2.0) < 1e-12);
        CHECK(sn_fround(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 3.0) < 1e-12); /* half away from zero */

    /* nearbyint/rint multiprec round modes (ctx->round) */
    {
        double d;
        unsigned fl;
        /* default NTE: 2.5 -> 2, 3.5 -> 4 (ties to even) */
        sn_ctx_set_round(&ctx, SN_ROUND_NTE);
        sn_ctx_clear_flags(&ctx);
        CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* 2.5 */
        CHECK(sn_nearbyint(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 2.0) < 1e-12);
        CHECK(sn_float_from_i64(&ctx, &x, 7, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* 3.5 */
        CHECK(sn_nearbyint(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 4.0) < 1e-12);
        /* SN_ROUND_NA: half away -> 2.5 -> 3 */
        sn_ctx_set_round(&ctx, SN_ROUND_NA);
        CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK);
        CHECK(sn_nearbyint(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 3.0) < 1e-12);
        /* SN_ROUND_TZ: 2.9 -> 2, -2.9 -> -2 */
        sn_ctx_set_round(&ctx, SN_ROUND_TZ);
        CHECK(sn_float_from_i64(&ctx, &x, 29, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 10, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK);
        CHECK(sn_nearbyint(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 2.0) < 1e-12);
        /* rint raises INEXACT when not integer; nearbyint does not */
        sn_ctx_set_round(&ctx, SN_ROUND_NTE);
        sn_ctx_clear_flags(&ctx);
        CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK);
        CHECK(sn_nearbyint(&ctx, &x, &z, NULL) == SN_OK);
        fl = sn_ctx_get_flags(&ctx);
        CHECK((fl & SN_FLAG_INEXACT) == 0);
        sn_ctx_clear_flags(&ctx);
        CHECK(sn_rint(&ctx, &x, &z, NULL) == SN_OK);
        fl = sn_ctx_get_flags(&ctx);
        CHECK((fl & SN_FLAG_INEXACT) != 0);
        /* exact integer: rint no INEXACT */
        sn_ctx_clear_flags(&ctx);
        CHECK(sn_float_from_i64(&ctx, &x, 4, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_rint(&ctx, &y, &x, NULL) == SN_OK);
        fl = sn_ctx_get_flags(&ctx);
        CHECK((fl & SN_FLAG_INEXACT) == 0);
        sn_ctx_set_round(&ctx, SN_ROUND_NTE);
    }


        /* frexp/ldexp roundtrip: 12 = m * 2^e */
        CHECK(sn_float_from_i64(&ctx, &x, 12, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_frexp(&ctx, &y, &e, &x, NULL) == SN_OK);
        CHECK(sn_ldexp(&ctx, &z, &y, e, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &z, &x) == SN_OK && rel == 0);
        CHECK(sn_ilogb(&ctx, &x, &e) == SN_OK);
        CHECK(e == 3); /* ilogb(12) = 3 */
        /* nextafter 1 -> 2 should be > 1 */
        CHECK(sn_float_from_i64(&ctx, &x, 1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_nextafter(&ctx, &z, &x, &y, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &z, &x) == SN_OK && rel > 0);
        CHECK(sn_cmp(&ctx, &rel, &z, &y) == SN_OK && rel < 0);
        /* cbrt(8)=2, exp2(3)=8, log2(8)=3 */
        CHECK(sn_float_from_i64(&ctx, &x, 8, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_cbrt(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d - 2.0) < 1e-10);
        CHECK(sn_float_from_i64(&ctx, &x, 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_exp2(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d - 8.0) < 1e-8);
        CHECK(sn_float_from_i64(&ctx, &x, 8, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_log2(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d - 3.0) < 1e-8);
        CHECK(sn_log10(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d - 0.9030899869919434) < 1e-8);
        /* expm1(0)=0, log1p(0)=0 */
        CHECK(sn_float_from_i64(&ctx, &x, 0, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_expm1(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d) < 1e-12);
        CHECK(sn_log1p(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d) < 1e-12);
        /* fmod(7.5, 2.0) = 1.5 */
        CHECK(sn_float_from_i64(&ctx, &x, 15, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* 7.5 */
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_fmod(&ctx, &x, &z, &y, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 1.5) < 1e-10);
        /* modf(2.5) — rebuild 2.5; z still holds 7.5 from fmod setup */
        CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* 2.5 */
        CHECK(sn_modf(&ctx, &ip, &fp, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &ip, &d) == SN_OK);
        CHECK(fabs(d - 2.0) < 1e-12);
        CHECK(sn_to_double(&ctx, &fp, &d) == SN_OK);
        CHECK(fabs(d - 0.5) < 1e-12);
        /* fdim(5, 2)=3, fdim(2, 5)=0 */
        CHECK(sn_float_from_i64(&ctx, &x, 5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_fdim(&ctx, &z, &x, &y, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &z, &d) == SN_OK);
        CHECK(fabs(d - 3.0) < 1e-12);
        CHECK(sn_fdim(&ctx, &z, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &z, &d) == SN_OK);
        CHECK(fabs(d) < 1e-12);
        /* remquo(7.5, 2) remainder  -0.5? remainder uses nearest; 7.5/2=3.75 -> n=4, rem=-0.5 */
        {
            int q = 99;
            CHECK(sn_float_from_i64(&ctx, &x, 15, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* 7.5 */
            CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
            CHECK(sn_remquo(&ctx, &x, &q, &z, &y, NULL) == SN_OK);
            CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
            CHECK(fabs(d - (-0.5)) < 1e-10);
            CHECK(q == 4 || q == -4); /* magnitude 4; sign of 7.5/2 positive => +4 */
            CHECK(q == 4);
        }
        /* negative floor/ceil */
        CHECK(sn_float_from_i64(&ctx, &x, -5, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* -2.5 */
        CHECK(sn_floor(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - (-3.0)) < 1e-12);
        CHECK(sn_ceil(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - (-2.0)) < 1e-12);
        CHECK(sn_trunc(&ctx, &x, &z, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - (-2.0)) < 1e-12);
        /* scalbn identity: scalbn(1.5, 3) = 12 */
        CHECK(sn_float_from_i64(&ctx, &x, 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 2, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_div(&ctx, &z, &x, &y, NULL) == SN_OK); /* 1.5 */
        CHECK(sn_scalbn(&ctx, &x, &z, 3, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &x, &d) == SN_OK);
        CHECK(fabs(d - 12.0) < 1e-10);
        /* nextafter toward zero from 1 is < 1 */
        CHECK(sn_float_from_i64(&ctx, &x, 1, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 0, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_nextafter(&ctx, &z, &x, &y, NULL) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &z, &x) == SN_OK && rel < 0);
        CHECK(sn_cmp(&ctx, &rel, &z, &y) == SN_OK && rel > 0);
        /* hypot(3,4)=5 multiprec */
        CHECK(sn_float_from_i64(&ctx, &x, 3, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_float_from_i64(&ctx, &y, 4, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_hypot(&ctx, &z, &x, &y, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &z, &d) == SN_OK);
        CHECK(fabs(d - 5.0) < 1e-8);
        /* erf(0)=0, erfc(0)=1 */
        CHECK(sn_float_from_i64(&ctx, &x, 0, 15, 80, 1, NULL) == SN_OK);
        CHECK(sn_erf(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d) < 1e-12);
        CHECK(sn_erfc(&ctx, &y, &x, NULL) == SN_OK);
        CHECK(sn_to_double(&ctx, &y, &d) == SN_OK);
        CHECK(fabs(d - 1.0) < 1e-10);
        sn_value_clear(&ctx, &x); sn_value_clear(&ctx, &y); sn_value_clear(&ctx, &z);
        sn_value_clear(&ctx, &ip); sn_value_clear(&ctx, &fp);
    }

    /* more scientific integer strings */
    CHECK(sn_from_str(&ctx, &a, "9.0e1", 10, 32, 1) == SN_OK);
    CHECK(sn_to_i64(&ctx, &a, &x) == SN_OK && x == 90);
    CHECK(sn_from_str_bigint(&ctx, &b, "1e20", 10) == SN_OK);
    {
        char *s = NULL;
        CHECK(sn_to_str(&ctx, &s, &b, 10) == SN_OK);
        CHECK(s && strcmp(s, "100000000000000000000") == 0);
        sn_str_free(&ctx, s);
    }

    /* no shared global: two ctx independent allocators already in test_alloc */

    sn_value_clear(&ctx, &a);
    sn_value_clear(&ctx, &b);
    sn_value_clear(&ctx, &c);
    return 0;
}
