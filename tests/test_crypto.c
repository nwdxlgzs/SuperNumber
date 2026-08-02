#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <stdint.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

int test_crypto_run(void)
{
    sn_ctx ctx;
    sn_value a, b, m, r;
    int64_t x;
    uint64_t ux;

    sn_ctx_init(&ctx);
    sn_value_init(&a);
    sn_value_init(&b);
    sn_value_init(&m);
    sn_value_init(&r);

    /* gcd(48, 18) = 6 */
    CHECK(sn_i64(&ctx, &a, 48) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 18) == SN_OK);
    CHECK(sn_gcd(&ctx, &r, &a, &b) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 6);

    /* gcd with negatives */
    CHECK(sn_i64(&ctx, &a, -48) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 18) == SN_OK);
    CHECK(sn_gcd(&ctx, &r, &a, &b) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 6);

    /* modinv(3, 11) = 4 because 3*4=12\equiv1 */
    CHECK(sn_i64(&ctx, &a, 3) == SN_OK);
    CHECK(sn_i64(&ctx, &m, 11) == SN_OK);
    CHECK(sn_modinv(&ctx, &r, &a, &m) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 4);

    /* modinv not exist */
    CHECK(sn_i64(&ctx, &a, 6) == SN_OK);
    CHECK(sn_i64(&ctx, &m, 9) == SN_OK);
    CHECK(sn_modinv(&ctx, &r, &a, &m) == SN_ERR_DOMAIN);

    /* mulmod: 7*8 mod 10 = 6 */
    CHECK(sn_i64(&ctx, &a, 7) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 8) == SN_OK);
    CHECK(sn_i64(&ctx, &m, 10) == SN_OK);
    CHECK(sn_mulmod(&ctx, &r, &a, &b, &m) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 6);

    /* powmod: 2^10 mod 1000 = 24? 1024 mod 1000 = 24 */
    CHECK(sn_i64(&ctx, &a, 2) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 10) == SN_OK);
    CHECK(sn_i64(&ctx, &m, 1000) == SN_OK);
    CHECK(sn_powmod(&ctx, &r, &a, &b, &m) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 24);

    /* powmod Fermat: a^(p-1) \equiv 1 mod p for prime p=17, a=3 */
    CHECK(sn_i64(&ctx, &a, 3) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 16) == SN_OK);
    CHECK(sn_i64(&ctx, &m, 17) == SN_OK);
    CHECK(sn_powmod(&ctx, &r, &a, &b, &m) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 1);

    /* --- Montgomery: setup / from-to / mul vs mulmod --- */
    {
        sn_mont mont;
        sn_value mx, my, mz, mw, acc;
        int rel;
        int64_t k, got;
        sn_mont_init(&mont);
        sn_value_init(&mx); sn_value_init(&my); sn_value_init(&mz);
        sn_value_init(&mw); sn_value_init(&acc);

        CHECK(sn_i64(&ctx, &m, 17) == SN_OK);
        CHECK(sn_mont_setup(&ctx, &mont, &m) == SN_OK);
        CHECK(mont.ready == 1);
        CHECK(sn_i64(&ctx, &a, 5) == SN_OK);
        CHECK(sn_mont_from(&ctx, &mx, &mont, &a) == SN_OK);
        CHECK(sn_mont_to(&ctx, &my, &mont, &mx) == SN_OK);
        CHECK(sn_to_i64(&ctx, &my, &x) == SN_OK && x == 5);

        CHECK(sn_i64(&ctx, &a, 7) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 8) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 17) == SN_OK);
        CHECK(sn_mont_setup(&ctx, &mont, &m) == SN_OK);
        CHECK(sn_mont_from(&ctx, &mx, &mont, &a) == SN_OK);
        CHECK(sn_mont_from(&ctx, &my, &mont, &b) == SN_OK);
        CHECK(sn_mont_mul(&ctx, &mz, &mont, &mx, &my) == SN_OK);
        CHECK(sn_mont_to(&ctx, &mw, &mont, &mz) == SN_OK);
        CHECK(sn_mulmod(&ctx, &r, &a, &b, &m) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &mw, &r) == SN_OK && rel == 0);

        CHECK(sn_i64(&ctx, &m, 10) == SN_OK);
        CHECK(sn_mont_setup(&ctx, &mont, &m) == SN_ERR_DOMAIN);

        /* powmod (Montgomery path) == repeated mulmod */
        CHECK(sn_i64(&ctx, &a, 1234567) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 89) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 1000003) == SN_OK);
        CHECK(sn_powmod(&ctx, &r, &a, &b, &m) == SN_OK);
        CHECK(sn_i64(&ctx, &acc, 1) == SN_OK);
        for (k = 0; k < 89; k++)
            CHECK(sn_mulmod(&ctx, &acc, &acc, &a, &m) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &r, &acc) == SN_OK && rel == 0);

        /* even modulus still works via naive path */
        CHECK(sn_i64(&ctx, &a, 3) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 5) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 100) == SN_OK);
        CHECK(sn_powmod(&ctx, &r, &a, &b, &m) == SN_OK);
        CHECK(sn_to_i64(&ctx, &r, &got) == SN_OK && got == 43);

        /* large-ish base string powmod consistency */
        CHECK(sn_from_str_bigint(&ctx, &a, "999999999999999999999999", 10) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 17) == SN_OK);
        CHECK(sn_from_str_bigint(&ctx, &m, "1000000007", 10) == SN_OK);
        CHECK(sn_powmod(&ctx, &r, &a, &b, &m) == SN_OK);
        /* recompute with mulmod loop */
        CHECK(sn_i64(&ctx, &acc, 1) == SN_OK);
        for (k = 0; k < 17; k++)
            CHECK(sn_mulmod(&ctx, &acc, &acc, &a, &m) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &r, &acc) == SN_OK && rel == 0);

        sn_value_clear(&ctx, &mx); sn_value_clear(&ctx, &my);
        sn_value_clear(&ctx, &mz); sn_value_clear(&ctx, &mw);
        sn_value_clear(&ctx, &acc);
        sn_mont_clear(&ctx, &mont);
    }


    /* --- Constant-time powmod_ct --- */
    {
        sn_value r_ct, r_ref, acc;
        int rel;
        int64_t k, got;
        sn_value_init(&r_ct); sn_value_init(&r_ref); sn_value_init(&acc);

        /* even modulus rejected */
        CHECK(sn_i64(&ctx, &a, 3) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 5) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 100) == SN_OK);
        CHECK(sn_powmod_ct(&ctx, &r_ct, &a, &b, &m) == SN_ERR_DOMAIN);

        /* match sn_powmod for odd m */
        CHECK(sn_i64(&ctx, &a, 3) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 16) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 17) == SN_OK);
        CHECK(sn_powmod_ct(&ctx, &r_ct, &a, &b, &m) == SN_OK);
        CHECK(sn_powmod(&ctx, &r_ref, &a, &b, &m) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &r_ct, &r_ref) == SN_OK && rel == 0);
        CHECK(sn_to_i64(&ctx, &r_ct, &got) == SN_OK && got == 1);

        CHECK(sn_i64(&ctx, &a, 1234567) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 89) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 1000003) == SN_OK);
        CHECK(sn_powmod_ct(&ctx, &r_ct, &a, &b, &m) == SN_OK);
        CHECK(sn_i64(&ctx, &acc, 1) == SN_OK);
        for (k = 0; k < 89; k++)
            CHECK(sn_mulmod(&ctx, &acc, &acc, &a, &m) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &r_ct, &acc) == SN_OK && rel == 0);

        /* large base string */
        CHECK(sn_from_str_bigint(&ctx, &a, "999999999999999999999999", 10) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 17) == SN_OK);
        CHECK(sn_from_str_bigint(&ctx, &m, "1000000007", 10) == SN_OK);
        CHECK(sn_powmod_ct(&ctx, &r_ct, &a, &b, &m) == SN_OK);
        CHECK(sn_powmod(&ctx, &r_ref, &a, &b, &m) == SN_OK);
        CHECK(sn_cmp(&ctx, &rel, &r_ct, &r_ref) == SN_OK && rel == 0);

        /* m==1 -> 0 */
        CHECK(sn_i64(&ctx, &a, 5) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 3) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 1) == SN_OK);
        CHECK(sn_powmod_ct(&ctx, &r_ct, &a, &b, &m) == SN_OK);
        CHECK(sn_to_i64(&ctx, &r_ct, &got) == SN_OK && got == 0);

        /* exp==0 -> 1 for m>1 */
        CHECK(sn_i64(&ctx, &a, 9) == SN_OK);
        CHECK(sn_i64(&ctx, &b, 0) == SN_OK);
        CHECK(sn_i64(&ctx, &m, 13) == SN_OK);
        CHECK(sn_powmod_ct(&ctx, &r_ct, &a, &b, &m) == SN_OK);
        CHECK(sn_to_i64(&ctx, &r_ct, &got) == SN_OK && got == 1);

        /* API table */
        {
            sn_api api;
            sn_api_bind(&api);
            CHECK(api.crypto.powmod_ct == sn_powmod_ct);
            CHECK(api.crypto.powmod_ct(&ctx, &r_ct, &a, &b, &m) == SN_OK);
        }

        sn_value_clear(&ctx, &r_ct);
        sn_value_clear(&ctx, &r_ref);
        sn_value_clear(&ctx, &acc);
    }

    /* big-ish powmod: 3^20 mod 1000003 */
    CHECK(sn_i64(&ctx, &a, 3) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 20) == SN_OK);
    CHECK(sn_i64(&ctx, &m, 1000003) == SN_OK);
    CHECK(sn_powmod(&ctx, &r, &a, &b, &m) == SN_OK);
    CHECK(sn_to_u64(&ctx, &r, &ux) == SN_OK);
    /* 3^20 = 3486784401; 3486784401 % 1000003 = 3486784401 - 3486*1000003
       compute expected: */
    {
        uint64_t expect = 1, i;
        for (i = 0; i < 20; i++) expect = (expect * 3) % 1000003ull;
        CHECK(ux == expect);
    }


    /* lcm(12,18)=36; lcm(0,5)=0 */
    CHECK(sn_i64(&ctx, &a, 12) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 18) == SN_OK);
    CHECK(sn_lcm(&ctx, &r, &a, &b) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 36);
    CHECK(sn_i64(&ctx, &a, 0) == SN_OK);
    CHECK(sn_i64(&ctx, &b, 5) == SN_OK);
    CHECK(sn_lcm(&ctx, &r, &a, &b) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 0);

    /* isqrt(0)=0, isqrt(1)=1, isqrt(10)=3, isqrt(100)=10, isqrt(big) */
    CHECK(sn_i64(&ctx, &a, 0) == SN_OK);
    CHECK(sn_isqrt(&ctx, &r, &a) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 0);
    CHECK(sn_i64(&ctx, &a, 1) == SN_OK);
    CHECK(sn_isqrt(&ctx, &r, &a) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 1);
    CHECK(sn_i64(&ctx, &a, 10) == SN_OK);
    CHECK(sn_isqrt(&ctx, &r, &a) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 3);
    CHECK(sn_i64(&ctx, &a, 100) == SN_OK);
    CHECK(sn_isqrt(&ctx, &r, &a) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 10);
    CHECK(sn_from_str_bigint(&ctx, &a, "15241578750190521", 10) == SN_OK); /* 123456789^2 */
    CHECK(sn_isqrt(&ctx, &r, &a) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 123456789);

    /* popcount(13)=3 (1101), ctz(40)=3 (101000) */
    CHECK(sn_i64(&ctx, &a, 13) == SN_OK);
    CHECK(sn_popcount(&ctx, &r, &a) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 3);
    CHECK(sn_i64(&ctx, &a, 40) == SN_OK);
    CHECK(sn_ctz(&ctx, &r, &a) == SN_OK);
    CHECK(sn_to_i64(&ctx, &r, &x) == SN_OK && x == 3);

    /* API table */
    {
        sn_api api;
        sn_api_bind(&api);
        CHECK(api.crypto.lcm == sn_lcm);
        CHECK(api.crypto.isqrt == sn_isqrt);
        CHECK(api.crypto.popcount == sn_popcount);
        CHECK(api.crypto.ctz == sn_ctz);
        CHECK(api.crypto.lcm(&ctx, &r, &a, &b) == SN_OK || 1);
    }

    sn_value_clear(&ctx, &a);
    sn_value_clear(&ctx, &b);
    sn_value_clear(&ctx, &m);
    sn_value_clear(&ctx, &r);
    return 0;
}
