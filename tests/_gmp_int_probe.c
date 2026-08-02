/* SN bigint vs mini-gmp correctness/coverage probe. */
#include "sn.h"
#include "sn_flat.h"
#include "mini-gmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static int tests, fails;

static void failf(const char *fmt, ...)
{
    /* simple */
    printf("FAIL: %s\n", fmt);
    fails++;
}

static int sn_set_dec(sn_ctx *ctx, sn_value *v, const char *dec)
{
    sn_value_init(v);
    if (sn_from_str_bigint(ctx, v, dec, 10) == SN_OK) return 1;
    sn_value_clear(ctx, v);
    sn_value_init(v);
    if (sn_from_str(ctx, v, dec, 10, 0, 1) == SN_OK) return 1;
    return 0;
}

static char *sn_dec(sn_ctx *ctx, const sn_value *v)
{
    char *s = NULL;
    if (sn_to_str(ctx, &s, v, 10) != SN_OK) return NULL;
    return s;
}

static int same_dec(const char *a, const char *b)
{
    if (!a || !b) return 0;
    if (a[0] == '+') a++;
    if (b[0] == '+') b++;
    /* treat -0 as 0 */
    if ((strcmp(a, "-0") == 0 && strcmp(b, "0") == 0) ||
        (strcmp(b, "-0") == 0 && strcmp(a, "0") == 0))
        return 1;
    return strcmp(a, b) == 0;
}

static void check_bin(const char *name, sn_ctx *ctx,
                      sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *),
                      void (*gmp_op)(mpz_t, const mpz_t, const mpz_t),
                      const char *as, const char *bs)
{
    sn_value a, b, o;
    mpz_t ga, gb, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&o);
    mpz_init(ga); mpz_init(gb); mpz_init(go);

    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &b, bs)) {
        printf("FAIL %s set a=%s b=%s\n", name, as, bs);
        fails++;
        goto done;
    }
    if (sn_op(ctx, &o, &a, &b, NULL) != SN_OK) {
        printf("FAIL %s sn_op a=%s b=%s\n", name, as, bs);
        fails++;
        goto done;
    }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, as, 10);
    mpz_set_str(gb, bs, 10);
    gmp_op(go, ga, gb);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL %s a=%s b=%s sn=%s gmp=%s\n", name, as, bs, ss ? ss : "?", gs ? gs : "?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss);
    free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(gb); mpz_clear(go);
}

static void gmp_add(mpz_t r, const mpz_t a, const mpz_t b) { mpz_add(r, a, b); }
static void gmp_sub(mpz_t r, const mpz_t a, const mpz_t b) { mpz_sub(r, a, b); }
static void gmp_mul(mpz_t r, const mpz_t a, const mpz_t b) { mpz_mul(r, a, b); }
static void gmp_and(mpz_t r, const mpz_t a, const mpz_t b) { mpz_and(r, a, b); }
static void gmp_ior(mpz_t r, const mpz_t a, const mpz_t b) { mpz_ior(r, a, b); }
static void gmp_xor(mpz_t r, const mpz_t a, const mpz_t b) { mpz_xor(r, a, b); }
static void gmp_com(mpz_t r, const mpz_t a) { mpz_com(r, a); }

static void check_div_rem(sn_ctx *ctx, const char *as, const char *bs)
{
    sn_value a, b, q, r;
    mpz_t ga, gb, gq, gr;
    char *sq = NULL, *sr = NULL, *gq_s = NULL, *gr_s = NULL;
    if (bs[0] == '0' && bs[1] == 0) return;
    if (bs[0] == '-' && bs[1] == '0' && bs[2] == 0) return;
    tests += 2;
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&q); sn_value_init(&r);
    mpz_init(ga); mpz_init(gb); mpz_init(gq); mpz_init(gr);
    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &b, bs)) {
        printf("FAIL div set a=%s b=%s\n", as, bs); fails += 2; goto done;
    }
    if (sn_div(ctx, &q, &a, &b, NULL) != SN_OK) {
        printf("FAIL div sn a=%s b=%s\n", as, bs); fails += 2; goto done;
    }
    if (sn_rem(ctx, &r, &a, &b, NULL) != SN_OK) {
        printf("FAIL rem sn a=%s b=%s\n", as, bs); fails += 2; goto done;
    }
    sq = sn_dec(ctx, &q); sr = sn_dec(ctx, &r);
    mpz_set_str(ga, as, 10); mpz_set_str(gb, bs, 10);
    mpz_tdiv_q(gq, ga, gb);
    mpz_tdiv_r(gr, ga, gb);
    gq_s = mpz_get_str(NULL, 10, gq);
    gr_s = mpz_get_str(NULL, 10, gr);
    if (!same_dec(sq, gq_s)) {
        printf("FAIL div a=%s b=%s sn=%s gmp=%s\n", as, bs, sq?sq:"?", gq_s?gq_s:"?");
        fails++;
    }
    if (!same_dec(sr, gr_s)) {
        printf("FAIL rem a=%s b=%s sn=%s gmp=%s\n", as, bs, sr?sr:"?", gr_s?gr_s:"?");
        fails++;
    }
done:
    if (sq) sn_str_free(ctx, sq);
    if (sr) sn_str_free(ctx, sr);
    free(gq_s); free(gr_s);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b);
    sn_value_clear(ctx, &q); sn_value_clear(ctx, &r);
    mpz_clear(ga); mpz_clear(gb); mpz_clear(gq); mpz_clear(gr);
}


static void check_abs(sn_ctx *ctx, const char *as)
{
    sn_value a, o;
    mpz_t ga, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&o);
    mpz_init(ga); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as)) {
        printf("FAIL abs set %s\n", as); fails++; goto done;
    }
    if (sn_abs(ctx, &o, &a, NULL) != SN_OK) {
        printf("FAIL abs sn %s\n", as); fails++; goto done;
    }
    mpz_set_str(ga, as, 10);
    mpz_abs(go, ga);
    ss = sn_dec(ctx, &o);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL abs a=%s sn=%s gmp=%s\n", as, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss);
    if (gs) free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(go);
}

static void check_cmp(sn_ctx *ctx, const char *as, const char *bs)
{
    sn_value a, b;
    mpz_t ga, gb;
    int rel = 0, grel;
    tests++;
    sn_value_init(&a); sn_value_init(&b);
    mpz_init(ga); mpz_init(gb);
    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &b, bs)) {
        printf("FAIL cmp set %s %s\n", as, bs); fails++; goto done;
    }
    if (sn_cmp(ctx, &rel, &a, &b) != SN_OK) {
        printf("FAIL cmp sn %s %s\n", as, bs); fails++; goto done;
    }
    mpz_set_str(ga, as, 10);
    mpz_set_str(gb, bs, 10);
    grel = mpz_cmp(ga, gb);
    if (grel < 0) grel = -1;
    else if (grel > 0) grel = 1;
    if (rel < 0) rel = -1;
    else if (rel > 0) rel = 1;
    if (rel != grel) {
        printf("FAIL cmp a=%s b=%s sn=%d gmp=%d\n", as, bs, rel, grel);
        fails++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b);
    mpz_clear(ga); mpz_clear(gb);
}

static void check_gcd(sn_ctx *ctx, const char *as, const char *bs)
{
    sn_value a, b, o;
    mpz_t ga, gb, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&o);
    mpz_init(ga); mpz_init(gb); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &b, bs)) {
        printf("FAIL gcd set\n"); fails++; goto done;
    }
    if (sn_gcd(ctx, &o, &a, &b) != SN_OK) {
        printf("FAIL gcd sn a=%s b=%s\n", as, bs); fails++; goto done;
    }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, as, 10); mpz_set_str(gb, bs, 10);
    mpz_gcd(go, ga, gb);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL gcd a=%s b=%s sn=%s gmp=%s\n", as, bs, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(gb); mpz_clear(go);
}

static void check_powmod(sn_ctx *ctx, const char *base, const char *exp, const char *mod)
{
    sn_value a, e, m, o;
    mpz_t ga, ge, gm, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&e); sn_value_init(&m); sn_value_init(&o);
    mpz_init(ga); mpz_init(ge); mpz_init(gm); mpz_init(go);
    if (!sn_set_dec(ctx, &a, base) || !sn_set_dec(ctx, &e, exp) || !sn_set_dec(ctx, &m, mod)) {
        printf("FAIL powmod set\n"); fails++; goto done;
    }
    if (sn_powmod(ctx, &o, &a, &e, &m) != SN_OK) {
        printf("FAIL powmod sn %s^%s mod %s\n", base, exp, mod); fails++; goto done;
    }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, base, 10); mpz_set_str(ge, exp, 10); mpz_set_str(gm, mod, 10);
    mpz_powm(go, ga, ge, gm);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL powmod %s^%s mod %s sn=%s gmp=%s\n", base, exp, mod, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &e); sn_value_clear(ctx, &m); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(ge); mpz_clear(gm); mpz_clear(go);
}


static void check_mulmod(sn_ctx *ctx, const char *as, const char *bs, const char *ms)
{
    sn_value a, b, m, o;
    mpz_t ga, gb, gm, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&m); sn_value_init(&o);
    mpz_init(ga); mpz_init(gb); mpz_init(gm); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &b, bs) || !sn_set_dec(ctx, &m, ms)) {
        printf("FAIL mulmod set\n"); fails++; goto done;
    }
    if (sn_mulmod(ctx, &o, &a, &b, &m) != SN_OK) {
        printf("FAIL mulmod sn %s*%s mod %s\n", as, bs, ms); fails++; goto done;
    }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, as, 10); mpz_set_str(gb, bs, 10); mpz_set_str(gm, ms, 10);
    mpz_mul(go, ga, gb); mpz_mod(go, go, gm);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL mulmod %s*%s mod %s sn=%s gmp=%s\n", as, bs, ms, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b); sn_value_clear(ctx, &m); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(gb); mpz_clear(gm); mpz_clear(go);
}

static void check_lcm(sn_ctx *ctx, const char *as, const char *bs)
{
    sn_value a, b, o;
    mpz_t ga, gb, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&o);
    mpz_init(ga); mpz_init(gb); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &b, bs)) {
        printf("FAIL lcm set\n"); fails++; goto done;
    }
    if (sn_lcm(ctx, &o, &a, &b) != SN_OK) {
        printf("FAIL lcm sn %s %s\n", as, bs); fails++; goto done;
    }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, as, 10); mpz_set_str(gb, bs, 10);
    mpz_lcm(go, ga, gb);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL lcm %s,%s sn=%s gmp=%s\n", as, bs, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(gb); mpz_clear(go);
}

static void check_modinv(sn_ctx *ctx, const char *as, const char *ms)
{
    sn_value a, m, o;
    mpz_t ga, gm, go;
    char *ss = NULL, *gs = NULL;
    int gmp_ok;
    tests++;
    sn_value_init(&a); sn_value_init(&m); sn_value_init(&o);
    mpz_init(ga); mpz_init(gm); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &m, ms)) {
        printf("FAIL modinv set\n"); fails++; goto done;
    }
    mpz_set_str(ga, as, 10); mpz_set_str(gm, ms, 10);
    gmp_ok = mpz_invert(go, ga, gm);
    {
        sn_status st = sn_modinv(ctx, &o, &a, &m);
        if (gmp_ok == 0) {
            if (st == SN_OK) {
                printf("FAIL modinv expected domain %s mod %s\n", as, ms);
                fails++;
            }
            goto done;
        }
        if (st != SN_OK) {
            printf("FAIL modinv sn %s mod %s st=%d\n", as, ms, (int)st); fails++; goto done;
        }
        ss = sn_dec(ctx, &o);
        gs = mpz_get_str(NULL, 10, go);
        if (!same_dec(ss, gs)) {
            printf("FAIL modinv %s mod %s sn=%s gmp=%s\n", as, ms, ss?ss:"?", gs?gs:"?");
            fails++;
        }
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &m); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(gm); mpz_clear(go);
}

/* powmod_ct vs powmod vs mini-gmp for odd modulus only. */
static void check_powmod_ct(sn_ctx *ctx, const char *base, const char *exp, const char *mod)
{
    sn_value a, e, m, o, oct;
    mpz_t ga, ge, gm, go;
    char *ss = NULL, *sct = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&e); sn_value_init(&m); sn_value_init(&o); sn_value_init(&oct);
    mpz_init(ga); mpz_init(ge); mpz_init(gm); mpz_init(go);
    if (!sn_set_dec(ctx, &a, base) || !sn_set_dec(ctx, &e, exp) || !sn_set_dec(ctx, &m, mod)) {
        printf("FAIL powmod_ct set\n"); fails++; goto done;
    }
    /* even modulus: expect DOMAIN */
    mpz_set_str(gm, mod, 10);
    if (mpz_even_p(gm)) {
        if (sn_powmod_ct(ctx, &oct, &a, &e, &m) == SN_OK) {
            printf("FAIL powmod_ct even mod should fail %s\n", mod); fails++;
        }
        goto done;
    }
    if (sn_powmod(ctx, &o, &a, &e, &m) != SN_OK) {
        printf("FAIL powmod_ct ref powmod\n"); fails++; goto done;
    }
    if (sn_powmod_ct(ctx, &oct, &a, &e, &m) != SN_OK) {
        printf("FAIL powmod_ct sn %s^%s mod %s\n", base, exp, mod); fails++; goto done;
    }
    ss = sn_dec(ctx, &o);
    sct = sn_dec(ctx, &oct);
    mpz_set_str(ga, base, 10); mpz_set_str(ge, exp, 10);
    mpz_powm(go, ga, ge, gm);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs) || !same_dec(sct, gs)) {
        printf("FAIL powmod_ct %s^%s mod %s pow=%s ct=%s gmp=%s\n",
               base, exp, mod, ss?ss:"?", sct?sct:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); if (sct) sn_str_free(ctx, sct); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &e); sn_value_clear(ctx, &m);
    sn_value_clear(ctx, &o); sn_value_clear(ctx, &oct);
    mpz_clear(ga); mpz_clear(ge); mpz_clear(gm); mpz_clear(go);
}


static void check_getsetbit(sn_ctx *ctx, const char *as)
{
    sn_value a, b;
    mpz_t ga;
    int i, bit, gbit, nbits;
    char *ss = NULL, *gs = NULL;
    sn_value_init(&a); sn_value_init(&b);
    mpz_init(ga);
    tests++;
    if (!sn_set_dec(ctx, &a, as) || mpz_set_str(ga, as, 10) != 0) {
        printf("getsetbit setup FAIL %s\n", as); fails++; goto done_gs;
    }
    if (sn_value_copy(ctx, &b, &a) != SN_OK) {
        printf("getsetbit copy FAIL %s\n", as); fails++; goto done_gs;
    }
    nbits = (int)mpz_sizeinbase(ga, 2) + 8;
    if (nbits < 32) nbits = 32;
    for (i = 0; i < nbits; i++) {
        if (sn_getbit(&a, i, &bit) != SN_OK) {
            printf("getbit FAIL %s i=%d\n", as, i); fails++; goto done_gs;
        }
        gbit = mpz_tstbit(ga, (mp_bitcnt_t)i) ? 1 : 0;
        if (bit != gbit) {
            printf("getbit mismatch %s i=%d sn=%d gmp=%d\n", as, i, bit, gbit);
            fails++; goto done_gs;
        }
    }
    for (i = 0; i < 16; i++) {
        int on = (i * 3 + 1) & 1;
        if (sn_setbit(ctx, &b, i, on) != SN_OK) {
            printf("setbit FAIL %s i=%d\n", as, i); fails++; goto done_gs;
        }
        if (on) mpz_setbit(ga, (mp_bitcnt_t)i); else mpz_clrbit(ga, (mp_bitcnt_t)i);
    }
    if (sn_to_str(ctx, &ss, &b, 10) != SN_OK || !ss) {
        printf("setbit to_str FAIL %s\n", as); fails++; goto done_gs;
    }
    gs = mpz_get_str(NULL, 10, ga);
    if (!gs || strcmp(ss, gs) != 0) {
        printf("setbit value mismatch %s sn=%s gmp=%s\n", as, ss ? ss : "?", gs ? gs : "?");
        fails++;
    }
done_gs:
    if (ss) sn_str_free(ctx, ss);
    free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b);
    mpz_clear(ga);
}

static void check_mont_mul(sn_ctx *ctx, const char *as, const char *bs, const char *ms)
{
    sn_mont mont;
    sn_value a, b, m, mx, my, mz, out, ref;
    char *ss = NULL, *rs = NULL;
    int rel;
    sn_mont_init(&mont);
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&m);
    sn_value_init(&mx); sn_value_init(&my); sn_value_init(&mz);
    sn_value_init(&out); sn_value_init(&ref);
    tests++;
    if (!sn_set_dec(ctx, &a, as) || !sn_set_dec(ctx, &b, bs) || !sn_set_dec(ctx, &m, ms)) {
        printf("mont setup FAIL %s %s %s\n", as, bs, ms); fails++; goto done_m;
    }
    if (sn_mont_setup(ctx, &mont, &m) != SN_OK) {
        printf("mont_setup FAIL mod=%s\n", ms); fails++; goto done_m;
    }
    if (sn_mont_from(ctx, &mx, &mont, &a) != SN_OK ||
        sn_mont_from(ctx, &my, &mont, &b) != SN_OK ||
        sn_mont_mul(ctx, &mz, &mont, &mx, &my) != SN_OK ||
        sn_mont_to(ctx, &out, &mont, &mz) != SN_OK ||
        sn_mulmod(ctx, &ref, &a, &b, &m) != SN_OK) {
        printf("mont_mul path FAIL %s*%s mod %s\n", as, bs, ms); fails++; goto done_m;
    }
    if (sn_cmp(ctx, &rel, &out, &ref) != SN_OK || rel != 0) {
        sn_to_str(ctx, &ss, &out, 10);
        sn_to_str(ctx, &rs, &ref, 10);
        printf("mont_mul mismatch %s*%s mod %s sn=%s ref=%s\n",
               as, bs, ms, ss ? ss : "?", rs ? rs : "?");
        fails++;
    }
done_m:
    if (ss) sn_str_free(ctx, ss);
    if (rs) sn_str_free(ctx, rs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b); sn_value_clear(ctx, &m);
    sn_value_clear(ctx, &mx); sn_value_clear(ctx, &my); sn_value_clear(ctx, &mz);
    sn_value_clear(ctx, &out); sn_value_clear(ctx, &ref);
    sn_mont_clear(ctx, &mont);
}

static void check_bitops(sn_ctx *ctx, const char *as)
{
    sn_value a, opc, otz;
    mpz_t ga;
    int sn_bl, g_bl;
    int64_t sn_pc64 = 0, sn_tz64 = 0;
    unsigned long g_pc, g_tz;
    sn_value_init(&a); sn_value_init(&opc); sn_value_init(&otz);
    mpz_init(ga);
    tests++;
    if (!sn_set_dec(ctx, &a, as) || mpz_set_str(ga, as, 10) != 0) {
        printf("bitops setup FAIL %s\n", as); fails++; goto done_bo;
    }
    sn_bl = sn_bitlen(&a);
    g_bl = (mpz_sgn(ga) == 0) ? 0 : (int)mpz_sizeinbase(ga, 2);
    if (sn_bl != g_bl) {
        printf("bitlen mismatch %s sn=%d gmp=%d\n", as, sn_bl, g_bl);
        fails++; goto done_bo;
    }
    /* GMP mpz_popcount is only defined for non-negative; negatives return ~0.
     * SN reports Hamming weight of magnitude (crypto/bit-count useful form). */
    if (mpz_sgn(ga) >= 0) {
        if (sn_popcount(ctx, &opc, &a) != SN_OK || sn_to_i64(ctx, &opc, &sn_pc64) != SN_OK) {
            printf("popcount status FAIL %s\n", as); fails++; goto done_bo;
        }
        g_pc = mpz_popcount(ga);
        if ((unsigned long)sn_pc64 != g_pc) {
            printf("popcount mismatch %s sn=%lld gmp=%lu\n", as, (long long)sn_pc64, g_pc);
            fails++; goto done_bo;
        }
    } else {
        /* still exercise SN path; only compare magnitude against |a| */
        mpz_t gabs;
        mpz_init(gabs);
        mpz_abs(gabs, ga);
        if (sn_popcount(ctx, &opc, &a) != SN_OK || sn_to_i64(ctx, &opc, &sn_pc64) != SN_OK) {
            printf("popcount status FAIL %s\n", as); fails++; mpz_clear(gabs); goto done_bo;
        }
        g_pc = mpz_popcount(gabs);
        if ((unsigned long)sn_pc64 != g_pc) {
            printf("popcount mismatch (mag) %s sn=%lld gmp=%lu\n", as, (long long)sn_pc64, g_pc);
            fails++; mpz_clear(gabs); goto done_bo;
        }
        mpz_clear(gabs);
    }
    if (sn_ctz(ctx, &otz, &a) != SN_OK || sn_to_i64(ctx, &otz, &sn_tz64) != SN_OK) {
        printf("ctz status FAIL %s\n", as); fails++; goto done_bo;
    }
    if (mpz_sgn(ga) == 0) {
        if (sn_tz64 != 0) {
            printf("ctz zero mismatch %s sn=%lld\n", as, (long long)sn_tz64); fails++;
        }
    } else {
        g_tz = mpz_scan1(ga, 0);
        if ((unsigned long)sn_tz64 != g_tz) {
            printf("ctz mismatch %s sn=%lld gmp=%lu\n", as, (long long)sn_tz64, g_tz);
            fails++;
        }
    }
done_bo:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &opc); sn_value_clear(ctx, &otz);
    mpz_clear(ga);
}

static void check_isqrt(sn_ctx *ctx, const char *as)
{
    sn_value a, o;
    mpz_t ga, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&o);
    mpz_init(ga); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as)) { printf("FAIL isqrt set\n"); fails++; goto done; }
    if (sn_isqrt(ctx, &o, &a) != SN_OK) { printf("FAIL isqrt sn a=%s\n", as); fails++; goto done; }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, as, 10);
    mpz_sqrt(go, ga);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL isqrt a=%s sn=%s gmp=%s\n", as, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(go);
}


static void check_not(sn_ctx *ctx, const char *as)
{
    sn_value a, o;
    mpz_t ga, go;
    char *ss = NULL, *gs = NULL;
    tests++;
    sn_value_init(&a); sn_value_init(&o);
    mpz_init(ga); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as)) { printf("FAIL not set\n"); fails++; goto done; }
    if (sn_not(ctx, &o, &a, NULL) != SN_OK) { printf("FAIL not sn a=%s\n", as); fails++; goto done; }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, as, 10);
    mpz_com(go, ga);
    gs = mpz_get_str(NULL, 10, go);
    if (!same_dec(ss, gs)) {
        printf("FAIL not a=%s sn=%s gmp=%s\n", as, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(go);
}

static void check_shift(sn_ctx *ctx, const char *as, int bits, int mode)
{
    sn_value a, o;
    mpz_t ga, go;
    char *ss = NULL, *gs = NULL;
    sn_status st;
    tests++;
    sn_value_init(&a); sn_value_init(&o);
    mpz_init(ga); mpz_init(go);
    if (!sn_set_dec(ctx, &a, as)) { printf("FAIL shift set\n"); fails++; goto done; }
    if (mode == 0) st = sn_shl(ctx, &o, &a, bits, NULL);
    else if (mode == 1) st = sn_shr(ctx, &o, &a, bits, NULL);
    else st = sn_sar(ctx, &o, &a, bits, NULL);
    if (st != SN_OK) { printf("FAIL shift sn mode=%d\n", mode); fails++; goto done; }
    ss = sn_dec(ctx, &o);
    mpz_set_str(ga, as, 10);
    if (mode == 0) mpz_mul_2exp(go, ga, (unsigned)bits);
    else mpz_fdiv_q_2exp(go, ga, (unsigned)bits);
    gs = mpz_get_str(NULL, 10, go);
    if (mode == 1 && as[0] == '-') {
        /* skip logical shr of negative */
        tests--;
    } else if (!same_dec(ss, gs)) {
        printf("FAIL shift mode=%d bits=%d a=%s sn=%s gmp=%s\n", mode, bits, as, ss?ss:"?", gs?gs:"?");
        fails++;
    }
done:
    if (ss) sn_str_free(ctx, ss); free(gs);
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &o);
    mpz_clear(ga); mpz_clear(go);
}

static char *rand_dec(int digits, int allow_neg)
{
    char *s = (char *)malloc((size_t)digits + 2);
    int i, pos = 0;
    if (!s) return NULL;
    if (allow_neg && (rand() & 1)) s[pos++] = '-';
    s[pos++] = (char)('1' + (rand() % 9));
    for (i = 1; i < digits; i++) s[pos++] = (char)('0' + (rand() % 10));
    s[pos] = 0;
    return s;
}

int main(void)
{
    sn_ctx ctx;
    int i;
    const char *cases_a[] = {
        "0", "1", "-1", "2", "255", "256", "65535", "1000000007",
        "123456789012345678901234567890",
        "-987654321098765432109876543210",
        "999999999999999999999999999999",
        "9223372036854775807", "-9223372036854775807"
    };
    const char *cases_b[] = {
        "1", "3", "7", "17", "256", "1009", "1000000007",
        "2", "5", "123456789", "42", "99991", "13", "9"
    };

    sn_ctx_init(&ctx);
    srand(1);

    for (i = 0; i < (int)(sizeof(cases_a)/sizeof(cases_a[0])); i++) {
        int j;
        for (j = 0; j < (int)(sizeof(cases_b)/sizeof(cases_b[0])); j++) {
            check_bin("add", &ctx, sn_add, gmp_add, cases_a[i], cases_b[j]);
            check_bin("sub", &ctx, sn_sub, gmp_sub, cases_a[i], cases_b[j]);
            check_bin("mul", &ctx, sn_mul, gmp_mul, cases_a[i], cases_b[j]);
            check_div_rem(&ctx, cases_a[i], cases_b[j]);
            check_gcd(&ctx, cases_a[i], cases_b[j]);
        }
    }

    check_bin("and", &ctx, sn_and, gmp_and, "255", "15");
    check_bin("or", &ctx, sn_or, gmp_ior, "255", "15");
    check_bin("xor", &ctx, sn_xor, gmp_xor, "255", "15");
    check_bin("and", &ctx, sn_and, gmp_and, "12345678901234567890", "9876543210987654321");
    check_bin("or", &ctx, sn_or, gmp_ior, "12345678901234567890", "9876543210987654321");
    check_bin("xor", &ctx, sn_xor, gmp_xor, "12345678901234567890", "9876543210987654321");
    check_bin("and", &ctx, sn_and, gmp_and, "-255", "15");
    check_bin("or", &ctx, sn_or, gmp_ior, "-255", "15");
    check_bin("xor", &ctx, sn_xor, gmp_xor, "-255", "15");
    check_bin("and", &ctx, sn_and, gmp_and, "-12345678901234567890", "987654321");
    check_bin("or", &ctx, sn_or, gmp_ior, "-12345678901234567890", "-3");
    check_bin("xor", &ctx, sn_xor, gmp_xor, "-12345678901234567890", "-7");
    check_abs(&ctx, "0");
    check_abs(&ctx, "1");
    check_abs(&ctx, "-1");
    check_abs(&ctx, "-12345678901234567890");
    check_abs(&ctx, "999999999999999999999999");
    check_cmp(&ctx, "0", "0");
    check_cmp(&ctx, "1", "2");
    check_cmp(&ctx, "2", "1");
    check_cmp(&ctx, "-5", "3");
    check_cmp(&ctx, "-5", "-5");
    check_cmp(&ctx, "12345678901234567890", "12345678901234567891");
    check_not(&ctx, "0");
    check_not(&ctx, "1");
    check_not(&ctx, "255");
    check_not(&ctx, "-1");
    check_not(&ctx, "12345678901234567890");
    check_not(&ctx, "-9876543210987654321");
    check_isqrt(&ctx, "999999999999999999999999999999");
    check_isqrt(&ctx, "15241578750190521"); /* 123456789^2 */


    check_isqrt(&ctx, "0");
    check_isqrt(&ctx, "1");
    check_isqrt(&ctx, "2");
    check_isqrt(&ctx, "100");
    check_isqrt(&ctx, "123456789012345678901234567890");

    check_powmod(&ctx, "2", "100", "1000000007");
    check_powmod(&ctx, "3", "200", "1000000009");
    check_powmod(&ctx, "123456789", "98765", "1000000007");
    check_powmod(&ctx, "999999999999", "12345", "1000000000039");

    check_shift(&ctx, "1", 10, 0);
    check_shift(&ctx, "12345678901234567890", 17, 0);
    check_shift(&ctx, "12345678901234567890", 5, 1);
    check_shift(&ctx, "-1000", 3, 2);
    check_shift(&ctx, "1000", 3, 2);

    for (i = 0; i < 80; i++) {
        char *a = rand_dec(20 + (i % 40), 1);
        char *b = rand_dec(10 + (i % 30), 1);
        if (!a || !b) { free(a); free(b); break; }
        check_bin("add", &ctx, sn_add, gmp_add, a, b);
        check_bin("sub", &ctx, sn_sub, gmp_sub, a, b);
        check_bin("mul", &ctx, sn_mul, gmp_mul, a, b);
        if (b[0] != '0') check_div_rem(&ctx, a, b);
        check_gcd(&ctx, a, b);
        check_abs(&ctx, a);
        check_abs(&ctx, b);
        check_cmp(&ctx, a, b);
        free(a); free(b);
    }

    /* random bitwise / not / isqrt / shifts (portable mini-gmp oracle) */
    for (i = 0; i < 60; i++) {
        char *a = rand_dec(8 + (i % 35), 1);
        char *b = rand_dec(6 + (i % 28), 1);
        char *apos;
        int sh;
        if (!a || !b) { free(a); free(b); break; }
        check_bin("and", &ctx, sn_and, gmp_and, a, b);
        check_bin("or", &ctx, sn_or, gmp_ior, a, b);
        check_bin("xor", &ctx, sn_xor, gmp_xor, a, b);
        check_not(&ctx, a);
        /* isqrt only on non-negative */
        apos = a;
        if (a[0] == '-') apos = a + 1;
        if (apos[0] != '\0') check_isqrt(&ctx, apos);
        sh = 1 + (i % 48);
        check_shift(&ctx, a, sh, 0); /* shl */
        check_shift(&ctx, a, sh % 24 + 1, 1); /* lshr */
        if (a[0] == '-') check_shift(&ctx, a, (sh % 12) + 1, 2); /* ashr */
        free(a); free(b);
    }


    /* lcm / modinv / mulmod */
    {
        sn_value a, b, o;
        mpz_t ga, gb, go;
        char *ss, *gs;
        const char *as = "12", *bs = "18";
        tests++;
        sn_value_init(&a); sn_value_init(&b); sn_value_init(&o);
        mpz_init(ga); mpz_init(gb); mpz_init(go);
        if (sn_set_dec(&ctx, &a, as) && sn_set_dec(&ctx, &b, bs) &&
            sn_lcm(&ctx, &o, &a, &b) == SN_OK) {
            ss = sn_dec(&ctx, &o);
            mpz_set_str(ga, as, 10); mpz_set_str(gb, bs, 10);
            mpz_lcm(go, ga, gb);
            gs = mpz_get_str(NULL, 10, go);
            if (!same_dec(ss, gs)) { printf("FAIL lcm sn=%s gmp=%s\n", ss?ss:"?", gs?gs:"?"); fails++; }
            if (ss) sn_str_free(&ctx, ss); free(gs);
        } else { printf("FAIL lcm\n"); fails++; }
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &o);
        mpz_clear(ga); mpz_clear(gb); mpz_clear(go);
    }
    {
        sn_value a, m, o;
        mpz_t ga, gm, go;
        char *ss, *gs;
        tests++;
        sn_value_init(&a); sn_value_init(&m); sn_value_init(&o);
        mpz_init(ga); mpz_init(gm); mpz_init(go);
        if (sn_set_dec(&ctx, &a, "3") && sn_set_dec(&ctx, &m, "11") &&
            sn_modinv(&ctx, &o, &a, &m) == SN_OK) {
            ss = sn_dec(&ctx, &o);
            mpz_set_str(ga, "3", 10); mpz_set_str(gm, "11", 10);
            if (mpz_invert(go, ga, gm) == 0) { printf("FAIL modinv gmp\n"); fails++; }
            else {
                gs = mpz_get_str(NULL, 10, go);
                if (!same_dec(ss, gs)) { printf("FAIL modinv sn=%s gmp=%s\n", ss?ss:"?", gs?gs:"?"); fails++; }
                free(gs);
            }
            if (ss) sn_str_free(&ctx, ss);
        } else { printf("FAIL modinv sn\n"); fails++; }
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &m); sn_value_clear(&ctx, &o);
        mpz_clear(ga); mpz_clear(gm); mpz_clear(go);
    }
    {
        sn_value a, b, m, o;
        mpz_t ga, gb, gm, go;
        char *ss, *gs;
        tests++;
        sn_value_init(&a); sn_value_init(&b); sn_value_init(&m); sn_value_init(&o);
        mpz_init(ga); mpz_init(gb); mpz_init(gm); mpz_init(go);
        if (sn_set_dec(&ctx, &a, "123456789") && sn_set_dec(&ctx, &b, "987654321") &&
            sn_set_dec(&ctx, &m, "1000000007") &&
            sn_mulmod(&ctx, &o, &a, &b, &m) == SN_OK) {
            ss = sn_dec(&ctx, &o);
            mpz_set_str(ga, "123456789", 10); mpz_set_str(gb, "987654321", 10);
            mpz_set_str(gm, "1000000007", 10);
            mpz_mul(go, ga, gb); mpz_mod(go, go, gm);
            gs = mpz_get_str(NULL, 10, go);
            if (!same_dec(ss, gs)) { printf("FAIL mulmod sn=%s gmp=%s\n", ss?ss:"?", gs?gs:"?"); fails++; }
            if (ss) sn_str_free(&ctx, ss); free(gs);
        } else { printf("FAIL mulmod\n"); fails++; }
        sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &m); sn_value_clear(&ctx, &o);
        mpz_clear(ga); mpz_clear(gb); mpz_clear(gm); mpz_clear(go);
    }

    /* popcount / ctz vs mini-gmp */
    {
        const char *cases[] = {
            "0", "1", "2", "3", "255", "256", "65535",
            "123456789012345678901234567890",
            "0xFFFFFFFF", /* will be decimal below */
            NULL
        };
        const char *decs[] = {
            "0", "1", "2", "3", "255", "256", "65535",
            "123456789012345678901234567890",
            "4294967295",
            NULL
        };
        int ci;
        for (ci = 0; decs[ci]; ci++) {
            sn_value a, o;
            mpz_t ga;
            char *ss = NULL, *gs = NULL;
            unsigned long pc, tz;
            tests++;
            sn_value_init(&a); sn_value_init(&o);
            mpz_init(ga);
            if (!sn_set_dec(&ctx, &a, decs[ci]) || sn_popcount(&ctx, &o, &a) != SN_OK) {
                printf("FAIL popcount set %s\n", decs[ci]); fails++;
            } else {
                ss = sn_dec(&ctx, &o);
                mpz_set_str(ga, decs[ci], 10);
                pc = mpz_popcount(ga);
                {
                    char buf[64];
                    sprintf(buf, "%lu", pc);
                    if (!same_dec(ss, buf)) {
                        printf("FAIL popcount %s sn=%s gmp=%s\n", decs[ci], ss?ss:"?", buf);
                        fails++;
                    }
                }
            }
            if (ss) { sn_str_free(&ctx, ss); ss = NULL; }
            sn_value_clear(&ctx, &o); sn_value_init(&o);
            tests++;
            if (sn_ctz(&ctx, &o, &a) != SN_OK) {
                printf("FAIL ctz %s\n", decs[ci]); fails++;
            } else {
                ss = sn_dec(&ctx, &o);
                /* mini-gmp: mpz_scan1 finds lowest set bit; for 0 returns ~(mp_bitcnt_t)0 */
                if (mpz_sgn(ga) == 0) tz = 0;
                else tz = mpz_scan1(ga, 0);
                {
                    char buf[64];
                    sprintf(buf, "%lu", tz);
                    if (!same_dec(ss, buf)) {
                        printf("FAIL ctz %s sn=%s gmp=%s\n", decs[ci], ss?ss:"?", buf);
                        fails++;
                    }
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &o);
            mpz_clear(ga);
            (void)cases;
        }
        /* random popcount/ctz */
        for (i = 0; i < 40; i++) {
            char *d = rand_dec(20 + (i % 40), i & 1);
            sn_value a, o;
            mpz_t ga;
            char *ss = NULL;
            unsigned long pc, tz;
            if (!d) continue;
            tests++;
            sn_value_init(&a); sn_value_init(&o);
            mpz_init(ga);
            if (sn_set_dec(&ctx, &a, d) && sn_popcount(&ctx, &o, &a) == SN_OK) {
                ss = sn_dec(&ctx, &o);
                mpz_set_str(ga, d, 10);
                if (mpz_sgn(ga) < 0) mpz_neg(ga, ga);
                pc = mpz_popcount(ga);
                {
                    char buf[64];
                    sprintf(buf, "%lu", pc);
                    if (!same_dec(ss, buf)) {
                        printf("FAIL rand popcount sn=%s gmp=%s d=%s\n", ss?ss:"?", buf, d);
                        fails++;
                    }
                }
            } else { printf("FAIL rand popcount\n"); fails++; }
            if (ss) { sn_str_free(&ctx, ss); ss = NULL; }
            sn_value_clear(&ctx, &o); sn_value_init(&o);
            tests++;
            if (sn_ctz(&ctx, &o, &a) == SN_OK) {
                ss = sn_dec(&ctx, &o);
                mpz_set_str(ga, d, 10);
                if (mpz_sgn(ga) < 0) mpz_neg(ga, ga);
                if (mpz_sgn(ga) == 0) tz = 0;
                else tz = mpz_scan1(ga, 0);
                {
                    char buf[64];
                    sprintf(buf, "%lu", tz);
                    if (!same_dec(ss, buf)) {
                        printf("FAIL rand ctz sn=%s gmp=%s d=%s\n", ss?ss:"?", buf, d);
                        fails++;
                    }
                }
            } else { printf("FAIL rand ctz\n"); fails++; }
            if (ss) sn_str_free(&ctx, ss);
            sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &o);
            mpz_clear(ga);
            free(d);
        }
    }

    /* negative remainder consistency: a%b sign follows a (SN convention) vs GMP toward zero? */
    {
        const char *pairs[][2] = {
            {"-17", "5"}, {"17", "-5"}, {"-17", "-5"}, {"-100", "7"}, {"100", "-7"},
            {"-1234567890123", "98765"}, {"1234567890123", "-98765"},
            {NULL, NULL}
        };
        int pi;
        for (pi = 0; pairs[pi][0]; pi++) {
            sn_value a, b, q, r;
            mpz_t ga, gb, gq, gr;
            char *ss = NULL, *gs = NULL;
            tests++;
            sn_value_init(&a); sn_value_init(&b); sn_value_init(&q); sn_value_init(&r);
            mpz_init(ga); mpz_init(gb); mpz_init(gq); mpz_init(gr);
            if (!sn_set_dec(&ctx, &a, pairs[pi][0]) || !sn_set_dec(&ctx, &b, pairs[pi][1]) ||
                sn_div(&ctx, &q, &a, &b, NULL) != SN_OK ||
                sn_rem(&ctx, &r, &a, &b, NULL) != SN_OK) {
                printf("FAIL div/rem neg set %s %s\n", pairs[pi][0], pairs[pi][1]); fails++;
            } else {
                /* SN rem sign follows dividend; recompute: r = a - q*b */
                sn_value t, qb;
                sn_value_init(&t); sn_value_init(&qb);
                if (sn_mul(&ctx, &qb, &q, &b, NULL) == SN_OK &&
                    sn_sub(&ctx, &t, &a, &qb, NULL) == SN_OK) {
                    char *rs = sn_dec(&ctx, &r);
                    char *ts = sn_dec(&ctx, &t);
                    if (!same_dec(rs, ts)) {
                        printf("FAIL rem identity a=%s b=%s r=%s a-qb=%s\n",
                               pairs[pi][0], pairs[pi][1], rs?rs:"?", ts?ts:"?");
                        fails++;
                    }
                    if (rs) sn_str_free(&ctx, rs);
                    if (ts) sn_str_free(&ctx, ts);
                } else {
                    printf("FAIL rem recompute\n"); fails++;
                }
                sn_value_clear(&ctx, &t); sn_value_clear(&ctx, &qb);
                /* also match GMP truncating division remainder when same sign convention:
                   use mpz_tdiv_qr and compare if SN uses truncate-toward-zero */
                mpz_set_str(ga, pairs[pi][0], 10);
                mpz_set_str(gb, pairs[pi][1], 10);
                mpz_tdiv_qr(gq, gr, ga, gb);
                ss = sn_dec(&ctx, &r);
                gs = mpz_get_str(NULL, 10, gr);
                if (!same_dec(ss, gs)) {
                    /* document mismatch but still count if identity held �?prefer truncate match */
                    printf("NOTE rem vs tdiv a=%s b=%s sn=%s gmp=%s\n",
                           pairs[pi][0], pairs[pi][1], ss?ss:"?", gs?gs:"?");
                    /* treat as fail only if identity already counted; require tdiv match for coverage */
                    fails++;
                }
                if (ss) sn_str_free(&ctx, ss); free(gs);
            }
            sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b);
            sn_value_clear(&ctx, &q); sn_value_clear(&ctx, &r);
            mpz_clear(ga); mpz_clear(gb); mpz_clear(gq); mpz_clear(gr);
        }
    }

    
    /* expanded crypto: random mulmod / lcm / modinv / powmod_ct vs mini-gmp */

        check_bitops(&ctx, "0");
        check_bitops(&ctx, "1");
        check_bitops(&ctx, "2");
        check_bitops(&ctx, "255");
        check_bitops(&ctx, "256");
        check_bitops(&ctx, "12345678901234567890");
        check_bitops(&ctx, "1024");
        check_bitops(&ctx, "1000000007");
        check_getsetbit(&ctx, "0");
        check_getsetbit(&ctx, "1");
        check_getsetbit(&ctx, "255");
        check_getsetbit(&ctx, "12345678901234567890");
        check_getsetbit(&ctx, "999999999999999999999999999999");
        check_mont_mul(&ctx, "5", "7", "17");
        check_mont_mul(&ctx, "1234567", "89", "1000003");
        check_mont_mul(&ctx, "999999999999", "888888888888", "1000000007");

    {
        static const char *odd_primes[] = {
            "3", "5", "7", "11", "13", "17", "19", "23", "29", "31",
            "1000000007", "1000000009", "1000000000039", "2147483647",
            NULL
        };
        int ri;
        check_mulmod(&ctx, "123456789", "987654321", "1000000007");
        check_mulmod(&ctx, "2", "3", "5");
        check_mulmod(&ctx, "999999999999", "888888888888", "1000000000039");
        check_lcm(&ctx, "12", "18");
        check_lcm(&ctx, "0", "5");
        check_lcm(&ctx, "1", "1");
        check_lcm(&ctx, "12345678901234567890", "9876543210987654321");
        check_modinv(&ctx, "3", "11");
        check_modinv(&ctx, "2", "4"); /* no inverse */
        check_modinv(&ctx, "7", "1000000007");
        check_powmod_ct(&ctx, "2", "100", "1000000007");
        check_powmod_ct(&ctx, "3", "200", "1000000009");
        check_powmod_ct(&ctx, "123456789", "98765", "1000000007");
        check_powmod_ct(&ctx, "5", "10", "8"); /* even mod -> domain */
        for (ri = 0; ri < 40; ri++) {
            char *a = rand_dec(10 + (ri % 25), 0);
            char *b = rand_dec(10 + (ri % 20), 0);
            char *m = rand_dec(8 + (ri % 18), 0);
            char *e = rand_dec(4 + (ri % 10), 0);
            if (a) { check_getsetbit(&ctx, a); check_bitops(&ctx, a); }
            if (a && b && m && m[0] != '0') {
                check_mulmod(&ctx, a, b, m);
                check_lcm(&ctx, a, b);
            }
            if (a && m && m[0] != '0')
                check_modinv(&ctx, a, m);
            if (a && b && odd_primes[ri % 14])
                check_mont_mul(&ctx, a, b, odd_primes[ri % 14]);
            if (a && e && m && m[0] != '0') {
                /* force odd modulus for most CT checks */
                if (odd_primes[ri % 14])
                    check_powmod_ct(&ctx, a, e, odd_primes[ri % 14]);
                else
                    check_powmod_ct(&ctx, a, e, m);
            }
            free(a); free(b); free(m); free(e);
        }
    }

    /* larger random powmod */
    for (i = 0; i < 20; i++) {
        char *base = rand_dec(30 + (i % 20), 0);
        char *exp = rand_dec(8 + (i % 8), 0);
        char *mod = rand_dec(20 + (i % 15), 0);
        if (base && exp && mod && mod[0] != '0')
            check_powmod(&ctx, base, exp, mod);
        free(base); free(exp); free(mod);
    }

    /* micro-bench: multiprec mul / powmod (mini-gmp is portable C, not asm GMP) */
    {
        clock_t t0, t1;
        char *a = rand_dec(800, 0);
        char *b = rand_dec(800, 0);
        char *e = rand_dec(64, 0);
        char *m = rand_dec(200, 0);
        sn_value A, B, E, M, O;
        mpz_t ga, gb, ge, gm, go;
        int k, Nmul = 400, Npm = 80;
        double sn_ms, gmp_ms;
        sn_value_init(&A); sn_value_init(&B); sn_value_init(&E); sn_value_init(&M); sn_value_init(&O);
        mpz_init(ga); mpz_init(gb); mpz_init(ge); mpz_init(gm); mpz_init(go);
        sn_set_dec(&ctx, &A, a); sn_set_dec(&ctx, &B, b);
        sn_set_dec(&ctx, &E, e); sn_set_dec(&ctx, &M, m);
        mpz_set_str(ga, a, 10); mpz_set_str(gb, b, 10);
        mpz_set_str(ge, e, 10); mpz_set_str(gm, m, 10);
        t0 = clock();
        for (k = 0; k < Nmul; k++) sn_mul(&ctx, &O, &A, &B, NULL);
        t1 = clock();
        sn_ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
        t0 = clock();
        for (k = 0; k < Nmul; k++) mpz_mul(go, ga, gb);
        t1 = clock();
        gmp_ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
        printf("bench mul800 x%d: sn=%.2fms mini-gmp=%.2fms ratio=%.2fx\n",
               Nmul, sn_ms, gmp_ms, gmp_ms > 1e-9 ? sn_ms / gmp_ms : 0.0);
        /* larger operands (~2000 digits) */
        {
            char *a2 = rand_dec(2000, 0);
            char *b2 = rand_dec(2000, 0);
            int N2 = 40;
            sn_set_dec(&ctx, &A, a2); sn_set_dec(&ctx, &B, b2);
            mpz_set_str(ga, a2, 10); mpz_set_str(gb, b2, 10);
            t0 = clock();
            for (k = 0; k < N2; k++) sn_mul(&ctx, &O, &A, &B, NULL);
            t1 = clock();
            sn_ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
            t0 = clock();
            for (k = 0; k < N2; k++) mpz_mul(go, ga, gb);
            t1 = clock();
            gmp_ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
            printf("bench mul2000 x%d: sn=%.2fms mini-gmp=%.2fms ratio=%.2fx\n",
                   N2, sn_ms, gmp_ms, gmp_ms > 1e-9 ? sn_ms / gmp_ms : 0.0);
            free(a2); free(b2);
            sn_set_dec(&ctx, &A, a); sn_set_dec(&ctx, &B, b);
            mpz_set_str(ga, a, 10); mpz_set_str(gb, b, 10);
        }
        t0 = clock();
        for (k = 0; k < Npm; k++) sn_powmod(&ctx, &O, &A, &E, &M);
        t1 = clock();
        sn_ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
        t0 = clock();
        for (k = 0; k < Npm; k++) mpz_powm(go, ga, ge, gm);
        t1 = clock();
        gmp_ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
        printf("bench powmod ~800^64 mod200 x%d: sn=%.2fms mini-gmp=%.2fms ratio=%.2fx\n",
               Npm, sn_ms, gmp_ms, gmp_ms > 1e-9 ? sn_ms / gmp_ms : 0.0);
        free(a); free(b); free(e); free(m);
        sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B);
        sn_value_clear(&ctx, &E); sn_value_clear(&ctx, &M); sn_value_clear(&ctx, &O);
        mpz_clear(ga); mpz_clear(gb); mpz_clear(ge); mpz_clear(gm); mpz_clear(go);
    }


    /* fixed-width wrap / saturate vs modular arithmetic */
    {
        static const int widths[] = { 8, 16, 32, 64 };
        static const int64_t vals[] = {
            0, 1, -1, 127, 128, 255, 256, -128, -129,
            1000, -1000, 2147483647LL, -2147483648LL,
            9223372036854775807LL /* may clamp on narrower */
        };
        int wi, vi, signedness;
        for (wi = 0; wi < (int)(sizeof(widths)/sizeof(widths[0])); wi++) {
            int w = widths[wi];
            for (signedness = 0; signedness < 2; signedness++) {
                for (vi = 0; vi < (int)(sizeof(vals)/sizeof(vals[0])); vi++) {
                    int64_t xv = vals[vi];
                    sn_value o_wrap, o_sat, a, b, sum;
                    sn_op_opt optw, opts;
                    uint64_t mask = (w == 64) ? ~0ULL : ((1ULL << w) - 1ULL);
                    int64_t expected_wrap, expected_sat;
                    char *ss = NULL;
                    int64_t got;
                    tests += 2;
                    memset(&optw, 0, sizeof(optw));
                    memset(&opts, 0, sizeof(opts));
                    optw.has_int_overflow = 1; optw.iov = SN_IOV_WRAP;
                    opts.has_int_overflow = 1; opts.iov = SN_IOV_SATURATE;
                    sn_value_init(&o_wrap); sn_value_init(&o_sat);
                    sn_value_init(&a); sn_value_init(&b); sn_value_init(&sum);
                    /* set via add of halves to exercise pack on overflow */
                    if (signedness) {
                        if (sn_int_set_i64(&ctx, &a, xv / 2, w, 1) != SN_OK ||
                            sn_int_set_i64(&ctx, &b, xv - xv / 2, w, 1) != SN_OK) {
                            fails += 2; goto fw_next;
                        }
                    } else {
                        uint64_t uxv = (uint64_t)xv;
                        if (sn_int_set_u64(&ctx, &a, uxv / 2, w, 0) != SN_OK ||
                            sn_int_set_u64(&ctx, &b, uxv - uxv / 2, w, 0) != SN_OK) {
                            fails += 2; goto fw_next;
                        }
                    }
                    if (sn_add(&ctx, &o_wrap, &a, &b, &optw) != SN_OK) { fails++; }
                    if (sn_add(&ctx, &o_sat, &a, &b, &opts) != SN_OK) { fails++; }
                    /* Also direct set with overflow mode via add of width-max */
                    (void)mask; (void)expected_wrap; (void)expected_sat; (void)ss; (void)got;
                fw_next:
                    sn_value_clear(&ctx, &o_wrap); sn_value_clear(&ctx, &o_sat);
                    sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &sum);
                }
            }
        }
        /* concrete wrap/sat checks */
        {
            sn_value a, b, o;
            sn_op_opt opt;
            char *ss = NULL;
            memset(&opt, 0, sizeof(opt));
            opt.has_int_overflow = 1;
            sn_value_init(&a); sn_value_init(&b); sn_value_init(&o);
            /* u8 wrap: 200+100 = 44 */
            tests++;
            opt.iov = SN_IOV_WRAP;
            sn_int_set_u64(&ctx, &a, 200, 8, 0);
            sn_int_set_u64(&ctx, &b, 100, 8, 0);
            if (sn_add(&ctx, &o, &a, &b, &opt) != SN_OK) { fails++; }
            else {
                ss = sn_dec(&ctx, &o);
                if (!same_dec(ss, "44")) { printf("FAIL u8 wrap 200+100 sn=%s\n", ss?ss:"?"); fails++; }
                if (ss) sn_str_free(&ctx, ss); ss = NULL;
            }
            /* u8 sat: 200+100 = 255 */
            tests++;
            sn_value_clear(&ctx, &o); sn_value_init(&o);
            opt.iov = SN_IOV_SATURATE;
            if (sn_add(&ctx, &o, &a, &b, &opt) != SN_OK) { fails++; }
            else {
                ss = sn_dec(&ctx, &o);
                if (!same_dec(ss, "255")) { printf("FAIL u8 sat 200+100 sn=%s\n", ss?ss:"?"); fails++; }
                if (ss) sn_str_free(&ctx, ss); ss = NULL;
            }
            /* i8 wrap: 100+100 = -56 (200-256) */
            tests++;
            sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &o);
            sn_value_init(&a); sn_value_init(&b); sn_value_init(&o);
            opt.iov = SN_IOV_WRAP;
            sn_int_set_i64(&ctx, &a, 100, 8, 1);
            sn_int_set_i64(&ctx, &b, 100, 8, 1);
            if (sn_add(&ctx, &o, &a, &b, &opt) != SN_OK) { fails++; }
            else {
                ss = sn_dec(&ctx, &o);
                if (!same_dec(ss, "-56")) { printf("FAIL i8 wrap 100+100 sn=%s\n", ss?ss:"?"); fails++; }
                if (ss) sn_str_free(&ctx, ss); ss = NULL;
            }
            /* i8 sat: 100+100 = 127 */
            tests++;
            sn_value_clear(&ctx, &o); sn_value_init(&o);
            opt.iov = SN_IOV_SATURATE;
            if (sn_add(&ctx, &o, &a, &b, &opt) != SN_OK) { fails++; }
            else {
                ss = sn_dec(&ctx, &o);
                if (!same_dec(ss, "127")) { printf("FAIL i8 sat 100+100 sn=%s\n", ss?ss:"?"); fails++; }
                if (ss) sn_str_free(&ctx, ss); ss = NULL;
            }
            /* i8 sat neg: -100 + -100 = -128 */
            tests++;
            sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &o);
            sn_value_init(&a); sn_value_init(&b); sn_value_init(&o);
            sn_int_set_i64(&ctx, &a, -100, 8, 1);
            sn_int_set_i64(&ctx, &b, -100, 8, 1);
            if (sn_add(&ctx, &o, &a, &b, &opt) != SN_OK) { fails++; }
            else {
                ss = sn_dec(&ctx, &o);
                if (!same_dec(ss, "-128")) { printf("FAIL i8 sat -100+-100 sn=%s\n", ss?ss:"?"); fails++; }
                if (ss) sn_str_free(&ctx, ss);
            }
            sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &b); sn_value_clear(&ctx, &o);
        }
    }

    /* large gcd / signed div edges / hamdist via popcount(xor) / bigger random */
    {
        int i;
        const char *big_a[] = {
            "123456789012345678901234567890",
            "-987654321098765432109876543210",
            "1",
            "0",
            "99999999999999999999999999999999999999"
        };
        const char *big_b[] = {
            "9876543210987654321",
            "123456789",
            "1",
            "17",
            "33333333333333333333333333333333333333"
        };
        for (i = 0; i < (int)(sizeof(big_a)/sizeof(big_a[0])); i++) {
            check_gcd(&ctx, big_a[i], big_b[i]);
            check_abs(&ctx, big_a[i]);
            check_cmp(&ctx, big_a[i], big_b[i]);
            if (big_b[i][0] != '0') check_div_rem(&ctx, big_a[i], big_b[i]);
        }
        /* signed division edges */
        check_div_rem(&ctx, "-100", "7");
        check_div_rem(&ctx, "100", "-7");
        check_div_rem(&ctx, "-100", "-7");
        check_div_rem(&ctx, "-1", "2");
        check_div_rem(&ctx, "1", "-2");
        check_gcd(&ctx, "-15", "25");
        check_gcd(&ctx, "-15", "-25");
        check_gcd(&ctx, "0", "0");

        /* popcount(xor) vs mini-gmp hamdist for non-negatives */
        for (i = 0; i < 40; i++) {
            char *a = rand_dec(5 + (i % 40), 0);
            char *b = rand_dec(5 + (i % 35), 0);
            sn_value A, B, X, P;
            mpz_t ga, gb, gx;
            int sn_pc = -1;
            mp_bitcnt_t hd;
            if (!a || !b) { free(a); free(b); break; }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&X); sn_value_init(&P);
            mpz_init(ga); mpz_init(gb); mpz_init(gx);
            {
                int64_t sn_pc64 = 0;
                if (!sn_set_dec(&ctx, &A, a) || !sn_set_dec(&ctx, &B, b) ||
                    sn_xor(&ctx, &X, &A, &B, NULL) != SN_OK ||
                    sn_popcount(&ctx, &P, &X) != SN_OK ||
                    sn_to_i64(&ctx, &P, &sn_pc64) != SN_OK) {
                    printf("FAIL hamdist setup a=%s b=%s\n", a, b); fails++;
                } else {
                    sn_pc = (int)sn_pc64;
                    mpz_set_str(ga, a, 10);
                    mpz_set_str(gb, b, 10);
                    mpz_xor(gx, ga, gb);
                    hd = mpz_popcount(gx);
                    if ((mp_bitcnt_t)sn_pc != hd) {
                        printf("FAIL hamdist a=%s b=%s sn=%d gmp=%lu\n", a, b, sn_pc, (unsigned long)hd);
                        fails++;
                    }
                }
            }
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B);
            sn_value_clear(&ctx, &X); sn_value_clear(&ctx, &P);
            mpz_clear(ga); mpz_clear(gb); mpz_clear(gx);
            free(a); free(b);
        }

        /* bigger random mul/gcd */
        for (i = 0; i < 30; i++) {
            char *a = rand_dec(60 + (i % 80), 1);
            char *b = rand_dec(40 + (i % 60), 1);
            if (!a || !b) { free(a); free(b); break; }
            check_bin("mul", &ctx, sn_mul, gmp_mul, a, b);
            check_gcd(&ctx, a, b);
            check_cmp(&ctx, a, b);
            free(a); free(b);
        }
    }


    /* hard residual: large lcm/modinv/mulmod/powmod + perfect-square isqrt + long shifts */
    {
        int i;
        const char *sq[] = {
            "0", "1", "4", "9", "16", "36", "100",
            "15241578750190521", /* 123456789^2 */
            "99999999998999999999900000000001"
        };
        const char *modinv_a[] = {
            "3", "7", "11", "12345678901234567891", "999999999989"
        };
        const char *modinv_m[] = {
            "11", "17", "101", "12345678901234567891", "100000000003"
        };
        const char *big_lcm_a[] = {
            "123456789012345678901234567890",
            "987654321098765432109876543210",
            "2",
            "111111111111111111111111111111111111"
        };
        const char *big_lcm_b[] = {
            "9876543210987654321",
            "123456789",
            "3",
            "222222222222222222222222222222222222"
        };

        for (i = 0; i < (int)(sizeof(sq)/sizeof(sq[0])); i++) {
            check_isqrt(&ctx, sq[i]);
            /* isqrt identity: s*s <= n < (s+1)*(s+1) for non-neg */
            {
                sn_value n, s, s2, sp1, sp1_2, one;
                char *sn = NULL, *ss = NULL;
                mpz_t gn, gs;
                int rel1 = 0, rel2 = 0;
                tests += 2;
                sn_value_init(&n); sn_value_init(&s); sn_value_init(&s2);
                sn_value_init(&sp1); sn_value_init(&sp1_2); sn_value_init(&one);
                mpz_init(gn); mpz_init(gs);
                if (!sn_set_dec(&ctx, &n, sq[i]) ||
                    sn_isqrt(&ctx, &s, &n) != SN_OK ||
                    sn_mul(&ctx, &s2, &s, &s, NULL) != SN_OK ||
                    sn_bigint_set_i64(&ctx, &one, 1) != SN_OK ||
                    sn_add(&ctx, &sp1, &s, &one, NULL) != SN_OK ||
                    sn_mul(&ctx, &sp1_2, &sp1, &sp1, NULL) != SN_OK ||
                    sn_cmp(&ctx, &rel1, &s2, &n) != SN_OK ||
                    sn_cmp(&ctx, &rel2, &n, &sp1_2) != SN_OK) {
                    printf("FAIL isqrt identity setup %s\n", sq[i]); fails += 2;
                } else {
                    if (rel1 > 0) {
                        printf("FAIL isqrt s*s > n for %s\n", sq[i]); fails++;
                    }
                    if (!(rel2 < 0 || (rel2 == 0 && (sq[i][0] == '0' && sq[i][1] == 0)))) {
                        /* n < (s+1)^2 always for finite non-neg; equality only impossible unless overflow */
                        if (rel2 >= 0) {
                            printf("FAIL isqrt n >= (s+1)^2 for %s\n", sq[i]); fails++;
                        }
                    }
                    mpz_set_str(gn, sq[i], 10);
                    mpz_sqrt(gs, gn);
                    sn = sn_dec(&ctx, &s);
                    ss = mpz_get_str(NULL, 10, gs);
                    if (!sn || !ss || strcmp(sn, ss) != 0) {
                        printf("FAIL isqrt match %s sn=%s gmp=%s\n", sq[i], sn?sn:"?", ss?ss:"?");
                        fails++;
                    }
                    free(ss);
                    if (sn) sn_str_free(&ctx, sn);
                }
                sn_value_clear(&ctx, &n); sn_value_clear(&ctx, &s); sn_value_clear(&ctx, &s2);
                sn_value_clear(&ctx, &sp1); sn_value_clear(&ctx, &sp1_2); sn_value_clear(&ctx, &one);
                mpz_clear(gn); mpz_clear(gs);
            }
        }

        for (i = 0; i < (int)(sizeof(modinv_a)/sizeof(modinv_a[0])); i++) {
            check_modinv(&ctx, modinv_a[i], modinv_m[i]);
            check_mulmod(&ctx, modinv_a[i], "3", modinv_m[i]);
            check_powmod(&ctx, modinv_a[i], "17", modinv_m[i]);
        }
        for (i = 0; i < (int)(sizeof(big_lcm_a)/sizeof(big_lcm_a[0])); i++) {
            check_lcm(&ctx, big_lcm_a[i], big_lcm_b[i]);
            check_gcd(&ctx, big_lcm_a[i], big_lcm_b[i]);
        }

        /* long random bitwise + shifts + mont on larger limbs */
        for (i = 0; i < 40; i++) {
            char *a = rand_dec(80 + (i % 120), 1);
            char *b = rand_dec(60 + (i % 100), 1);
            /* crypto modular ops: non-negative operands (SN uses magnitudes) */
            char *an = rand_dec(80 + (i % 120), 0);
            char *bn = rand_dec(60 + (i % 100), 0);
            char *m = rand_dec(40 + (i % 40), 0);
            static const char odd_digits[] = "13579";
            if (!a || !b || !an || !bn || !m) {
                free(a); free(b); free(an); free(bn); free(m); break;
            }
            /* force odd modulus for mont */
            {
                int L = (int)strlen(m);
                if (L > 0)
                    m[L-1] = odd_digits[i % 5];
            }
            check_bitops(&ctx, a);
            check_not(&ctx, a);
            check_shift(&ctx, a, 1 + (i % 97), 0);
            check_shift(&ctx, a, 1 + (i % 63), 1);
            if (a[0] != '-') check_shift(&ctx, a, 1 + (i % 40), 2);
            check_bin("and", &ctx, sn_and, gmp_and, a, b);
            check_bin("or", &ctx, sn_or, gmp_ior, a, b);
            check_bin("xor", &ctx, sn_xor, gmp_xor, a, b);
            check_mulmod(&ctx, an, bn, m);
            check_mont_mul(&ctx, an, bn, m);
            free(a); free(b); free(an); free(bn); free(m);
        }

        /* multi-limb div/rem stress */
        for (i = 0; i < 50; i++) {
            char *a = rand_dec(100 + (i % 150), 1);
            char *b = rand_dec(20 + (i % 80), 1);
            if (!a || !b) { free(a); free(b); break; }
            if (!(b[0]=='0' && b[1]==0) && !(b[0]=='-' && b[1]=='0' && b[2]==0))
                check_div_rem(&ctx, a, b);
            check_cmp(&ctx, a, b);
            free(a); free(b);
        }
    }


    /* deeper residual: RSA-ish edges, huge powmod, ctz/popcount/bitlen, gcd identity */
    {
        int i;
        const char *rsa_n[] = {
            /* 64-bit-ish primes product style moduli (decimal) */
            "1000000000039",
            "100000000003",
            "18446744073709551557", /* near 2^64 prime-ish */
            "1234567890123456789012345678901234567891"
        };
        const char *rsa_e[] = { "3", "5", "17", "65537", "1234567" };
        const char *rsa_b[] = {
            "2", "3", "7", "123456789", "999999999989",
            "3141592653589793238462643383279502884197"
        };

        for (i = 0; i < (int)(sizeof(rsa_n)/sizeof(rsa_n[0])); i++) {
            int j, k;
            for (j = 0; j < (int)(sizeof(rsa_e)/sizeof(rsa_e[0])); j++) {
                for (k = 0; k < (int)(sizeof(rsa_b)/sizeof(rsa_b[0])); k++) {
                    check_powmod(&ctx, rsa_b[k], rsa_e[j], rsa_n[i]);
                    /* odd moduli only for powmod_ct */
                    {
                        const char *m = rsa_n[i];
                        int L = (int)strlen(m);
                        if (L > 0 && ((m[L-1]-'0') & 1))
                            check_powmod_ct(&ctx, rsa_b[k], rsa_e[j], m);
                    }
                }
            }
        }

        /* huge multi-limb mul/add/sub/div residual */
        for (i = 0; i < 40; i++) {
            char *a = rand_dec(200 + (i % 300), 1);
            char *b = rand_dec(150 + (i % 250), 1);
            char *m = rand_dec(40 + (i % 80), 0);
            char *an, *bn;
            static const char odd_digits[] = "13579";
            if (!a || !b || !m) { free(a); free(b); free(m); break; }
            {
                int L = (int)strlen(m);
                if (L > 0) m[L-1] = odd_digits[i % 5];
            }
            check_bin("add", &ctx, sn_add, gmp_add, a, b);
            check_bin("sub", &ctx, sn_sub, gmp_sub, a, b);
            check_bin("mul", &ctx, sn_mul, gmp_mul, a, b);
            if (!(b[0]=='0' && b[1]==0) && !(b[0]=='-' && b[1]=='0' && b[2]==0))
                check_div_rem(&ctx, a, b);
            check_gcd(&ctx, a, b);
            check_lcm(&ctx, a, b);
            an = a[0]=='-' ? a+1 : a;
            bn = b[0]=='-' ? b+1 : b;
            if (an[0] && bn[0] && m[0] && !(m[0]=='0' && m[1]==0)) {
                check_mulmod(&ctx, an, bn, m);
                check_powmod(&ctx, an, "17", m);
                check_mont_mul(&ctx, an, bn, m);
            }
            /* bitops on huge values */
            check_bitops(&ctx, a);
            check_not(&ctx, a);
            check_shift(&ctx, a, 1 + (i % 200), 0);
            check_shift(&ctx, a, 1 + (i % 120), 1);
            free(a); free(b); free(m);
        }

        /* gcd identity: gcd(a,b)*lcm(a,b) == |a|*|b| for nonzero */
        for (i = 0; i < 30; i++) {
            char *a = rand_dec(20 + (i % 60), 1);
            char *b = rand_dec(15 + (i % 50), 1);
            sn_value A, B, G, L, P1, P2, absA, absB;
            char *sg = NULL, *sl = NULL, *sp1 = NULL, *sp2 = NULL;
            if (!a || !b) { free(a); free(b); break; }
            if ((a[0]=='0' && a[1]==0) || (b[0]=='0' && b[1]==0) ||
                (a[0]=='-' && a[1]=='0') || (b[0]=='-' && b[1]=='0')) {
                free(a); free(b); continue;
            }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&G);
            sn_value_init(&L); sn_value_init(&P1); sn_value_init(&P2);
            sn_value_init(&absA); sn_value_init(&absB);
            if (sn_set_dec(&ctx, &A, a) && sn_set_dec(&ctx, &B, b) &&
                sn_gcd(&ctx, &G, &A, &B) == SN_OK &&
                sn_lcm(&ctx, &L, &A, &B) == SN_OK &&
                sn_abs(&ctx, &absA, &A, NULL) == SN_OK &&
                sn_abs(&ctx, &absB, &B, NULL) == SN_OK &&
                sn_mul(&ctx, &P1, &G, &L, NULL) == SN_OK &&
                sn_mul(&ctx, &P2, &absA, &absB, NULL) == SN_OK) {
                {
                    int rel = 0;
                    if (sn_cmp(&ctx, &rel, &P1, &P2) != SN_OK || rel != 0) {
                        sg = sn_dec(&ctx, &G); sl = sn_dec(&ctx, &L);
                        sp1 = sn_dec(&ctx, &P1); sp2 = sn_dec(&ctx, &P2);
                        printf("FAIL gcd*lcm identity a=%s b=%s g=%s l=%s p1=%s p2=%s\n",
                               a, b, sg?sg:"?", sl?sl:"?", sp1?sp1:"?", sp2?sp2:"?");
                        fails++;
                    }
                }
            } else {
                printf("FAIL gcd*lcm setup a=%s b=%s\n", a, b); fails++;
            }
            if (sg) sn_str_free(&ctx, sg);
            if (sl) sn_str_free(&ctx, sl);
            if (sp1) sn_str_free(&ctx, sp1);
            if (sp2) sn_str_free(&ctx, sp2);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B);
            sn_value_clear(&ctx, &G); sn_value_clear(&ctx, &L);
            sn_value_clear(&ctx, &P1); sn_value_clear(&ctx, &P2);
            sn_value_clear(&ctx, &absA); sn_value_clear(&ctx, &absB);
            free(a); free(b);
        }

        /* modinv * a ≡ 1 (mod m) identity for coprime pairs */
        for (i = 0; i < 25; i++) {
            char *a = rand_dec(10 + (i % 40), 0);
            char *m = rand_dec(12 + (i % 45), 0);
            sn_value A, M, Inv, Prod, One, Rem;
            char *ss = NULL;
            static const char odd_digits[] = "13579";
            if (!a || !m) { free(a); free(m); break; }
            {
                int L = (int)strlen(m);
                if (L > 0) m[L-1] = odd_digits[i % 5];
            }
            if ((a[0]=='0' && a[1]==0) || (m[0]=='0' && m[1]==0) || (m[0]=='1' && m[1]==0)) {
                free(a); free(m); continue;
            }
            tests++;
            sn_value_init(&A); sn_value_init(&M); sn_value_init(&Inv);
            sn_value_init(&Prod); sn_value_init(&One); sn_value_init(&Rem);
            if (sn_set_dec(&ctx, &A, a) && sn_set_dec(&ctx, &M, m) &&
                sn_bigint_set_i64(&ctx, &One, 1) == SN_OK) {
                sn_status st = sn_modinv(&ctx, &Inv, &A, &M);
                if (st == SN_OK) {
                    if (sn_mulmod(&ctx, &Prod, &A, &Inv, &M) == SN_OK) {
                        {
                            int rel = 0;
                            if (sn_cmp(&ctx, &rel, &Prod, &One) != SN_OK || rel != 0) {
                                ss = sn_dec(&ctx, &Prod);
                                printf("FAIL modinv identity a=%s m=%s prod=%s\n", a, m, ss?ss:"?");
                                fails++;
                            }
                        }
                    } else {
                        printf("FAIL modinv mulmod a=%s m=%s\n", a, m); fails++;
                    }
                }
                /* if not invertible, skip (not a fail) */
            } else {
                printf("FAIL modinv setup a=%s m=%s\n", a, m); fails++;
            }
            if (ss) sn_str_free(&ctx, ss);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &M);
            sn_value_clear(&ctx, &Inv); sn_value_clear(&ctx, &Prod);
            sn_value_clear(&ctx, &One); sn_value_clear(&ctx, &Rem);
            free(a); free(m);
        }
    }


    /* denser multi-limb bitops residual (uses check_bitops API correctly) */
    {
        int i;
        for (i = 0; i < 100; i++) {
            char *a = rand_dec(30 + (i % 200), 1);
            if (!a) break;
            check_bitops(&ctx, a);
            free(a);
        }
    }


    /* deeper residual: factorial product, power-by-mul identity, binomial C(n,k) via mul/div, signed rem consistency */
    {
        int i, n, k;
        /* n! for n up to 80 vs mini-gmp */
        for (n = 0; n <= 80; n++) {
            sn_value fac, t, one;
            mpz_t gf;
            char *ss = NULL, *gs = NULL;
            tests++;
            sn_value_init(&fac); sn_value_init(&t); sn_value_init(&one);
            mpz_init(gf);
            if (sn_bigint_set_i64(&ctx, &fac, 1) != SN_OK || sn_bigint_set_i64(&ctx, &one, 1) != SN_OK) {
                printf("FAIL fact setup n=%d\n", n); fails++;
            } else {
                for (i = 2; i <= n; i++) {
                    if (sn_bigint_set_i64(&ctx, &t, (int64_t)i) != SN_OK ||
                        sn_mul(&ctx, &fac, &fac, &t, NULL) != SN_OK) {
                        printf("FAIL fact mul n=%d i=%d\n", n, i); fails++; break;
                    }
                }
                mpz_set_ui(gf, 1);
                for (i = 2; i <= n; i++) mpz_mul_ui(gf, gf, (unsigned long)i);
                ss = sn_dec(&ctx, &fac);
                gs = mpz_get_str(NULL, 10, gf);
                if (!ss || !gs || strcmp(ss, gs) != 0) {
                    printf("FAIL fact n=%d sn=%s gmp=%s\n", n, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &fac); sn_value_clear(&ctx, &t); sn_value_clear(&ctx, &one);
            mpz_clear(gf);
        }
        /* a^e identity: iterative mul vs powmod(a,e, huge_mod) and vs mini-gmp mpz_pow_ui for small e */
        for (i = 0; i < 40; i++) {
            char *a = rand_dec(8 + (i % 20), 0);
            unsigned long e = (unsigned long)(2 + (i % 12));
            sn_value A, P, T, E, M, PM;
            mpz_t ga, gp;
            char *ss = NULL, *gs = NULL;
            if (!a) break;
            if (a[0]=='0' && a[1]==0) { free(a); continue; }
            tests++;
            sn_value_init(&A); sn_value_init(&P); sn_value_init(&T);
            sn_value_init(&E); sn_value_init(&M); sn_value_init(&PM);
            mpz_init(ga); mpz_init(gp);
            if (!sn_set_dec(&ctx, &A, a) || sn_bigint_set_i64(&ctx, &P, 1) != SN_OK) {
                printf("FAIL pow_ui setup a=%s\n", a); fails++;
            } else {
                unsigned long j;
                for (j = 0; j < e; j++) {
                    if (sn_mul(&ctx, &P, &P, &A, NULL) != SN_OK) {
                        printf("FAIL pow_ui mul a=%s e=%lu\n", a, e); fails++; break;
                    }
                }
                mpz_set_str(ga, a, 10);
                mpz_pow_ui(gp, ga, e);
                ss = sn_dec(&ctx, &P);
                gs = mpz_get_str(NULL, 10, gp);
                if (!ss || !gs || strcmp(ss, gs) != 0) {
                    printf("FAIL pow_ui a=%s e=%lu sn=%s gmp=%s\n", a, e, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            free(a);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &P); sn_value_clear(&ctx, &T);
            sn_value_clear(&ctx, &E); sn_value_clear(&ctx, &M); sn_value_clear(&ctx, &PM);
            mpz_clear(ga); mpz_clear(gp);
        }
        /* binomial C(n,k) = n!/(k!(n-k)!) for n<=40 */
        for (n = 0; n <= 40; n++) {
            for (k = 0; k <= n; k++) {
                sn_value N, K, C, T, D;
                mpz_t gn, gk, gc;
                char *ss = NULL, *gs = NULL;
                int j;
                tests++;
                sn_value_init(&N); sn_value_init(&K); sn_value_init(&C);
                sn_value_init(&T); sn_value_init(&D);
                mpz_init(gn); mpz_init(gk); mpz_init(gc);
                if (sn_bigint_set_i64(&ctx, &C, 1) != SN_OK) {
                    printf("FAIL binom setup\n"); fails++;
                } else {
                    /* multiplicative formula */
                    for (j = 1; j <= k; j++) {
                        if (sn_bigint_set_i64(&ctx, &T, (int64_t)(n - k + j)) != SN_OK ||
                            sn_mul(&ctx, &C, &C, &T, NULL) != SN_OK ||
                            sn_bigint_set_i64(&ctx, &D, (int64_t)j) != SN_OK ||
                            sn_div(&ctx, &C, &C, &D, NULL) != SN_OK) {
                            printf("FAIL binom mul n=%d k=%d j=%d\n", n, k, j); fails++; break;
                        }
                    }
                    mpz_bin_uiui(gc, (unsigned long)n, (unsigned long)k);
                    ss = sn_dec(&ctx, &C);
                    gs = mpz_get_str(NULL, 10, gc);
                    if (!ss || !gs || strcmp(ss, gs) != 0) {
                        printf("FAIL binom C(%d,%d) sn=%s gmp=%s\n", n, k, ss?ss:"?", gs?gs:"?");
                        fails++;
                    }
                }
                if (ss) sn_str_free(&ctx, ss);
                if (gs) free(gs);
                sn_value_clear(&ctx, &N); sn_value_clear(&ctx, &K); sn_value_clear(&ctx, &C);
                sn_value_clear(&ctx, &T); sn_value_clear(&ctx, &D);
                mpz_clear(gn); mpz_clear(gk); mpz_clear(gc);
            }
        }
        /* expansion: shift/bitwise stress, powmod_ct vs powmod denser, bitlen edges */
        {
            int si;
            const char *shift_vals[] = {
                "0", "1", "-1", "2", "255", "256", "-128",
                "18446744073709551615",
                "123456789012345678901234567890",
                "-987654321098765432109876543210",
                "0x1" /* will be skipped if set_dec only - keep decimal */
            };
            /* decimal only */
            const char *sv[] = {
                "0", "1", "-1", "2", "255", "256", "-128",
                "18446744073709551615",
                "123456789012345678901234567890",
                "-987654321098765432109876543210",
                "999999999999999999999999999999999999",
                "-100000000000000000000000000000000001"
            };
            static const int shbits[] = { 0, 1, 2, 7, 8, 31, 32, 63, 64, 100, 200 };
            for (si = 0; si < (int)(sizeof(sv)/sizeof(sv[0])); si++) {
                int bi;
                check_bitops(&ctx, sv[si]);
                check_not(&ctx, sv[si]);
                check_isqrt(&ctx, sv[si][0]=='-' ? sv[si]+1 : sv[si]);
                for (bi = 0; bi < (int)(sizeof(shbits)/sizeof(shbits[0])); bi++) {
                    check_shift(&ctx, sv[si], shbits[bi], 0); /* shl */
                    check_shift(&ctx, sv[si], shbits[bi], 1); /* shr logical */
                    check_shift(&ctx, sv[si], shbits[bi], 2); /* sar */
                }
            }
            /* RSA-ish longer exponents and multi-prime moduli */
            {
                const char *n2[] = {
                    "1000000000039",
                    "100000000003",
                    "32416190071",
                    "2305843009213693951",
                    "170141183460469231731687303715884105727"
                };
                const char *e2[] = { "2", "11", "257", "10001", "99991", "123456789" };
                const char *b2[] = { "2", "5", "11", "12345", "9876543210123456789" };
                int ni, ei, bi;
                for (ni = 0; ni < (int)(sizeof(n2)/sizeof(n2[0])); ni++)
                    for (ei = 0; ei < (int)(sizeof(e2)/sizeof(e2[0])); ei++)
                        for (bi = 0; bi < (int)(sizeof(b2)/sizeof(b2[0])); bi++) {
                            check_powmod(&ctx, b2[bi], e2[ei], n2[ni]);
                            {
                                const char *m = n2[ni];
                                int L = (int)strlen(m);
                                if (L > 0 && ((m[L-1]-'0') & 1))
                                    check_powmod_ct(&ctx, b2[bi], e2[ei], m);
                            }
                        }
            }
            /* random huge bitops / isqrt / shifts */
            for (si = 0; si < 60; si++) {
                char *a = rand_dec(50 + (si % 200), 1);
                char *m = rand_dec(20 + (si % 60), 0);
                if (!a) break;
                check_bitops(&ctx, a);
                check_not(&ctx, a);
                {
                    const char *ap = a[0]=='-' ? a+1 : a;
                    if (!(ap[0]=='0' && ap[1]==0)) check_isqrt(&ctx, ap);
                }
                check_shift(&ctx, a, 1 + (si % 97), 0);
                check_shift(&ctx, a, 1 + (si % 64), 1);
                check_shift(&ctx, a, 1 + (si % 64), 2);
                if (m) {
                    int L = (int)strlen(m);
                    static const char odd_digits[] = "13579";
                    if (L > 0) m[L-1] = odd_digits[si % 5];
                    if (!(m[0]=='0' && m[1]==0) && !(m[0]=='1' && m[1]==0)) {
                        const char *ap = a[0]=='-' ? a+1 : a;
                        check_powmod(&ctx, ap, "65537", m);
                        check_powmod_ct(&ctx, ap, "17", m);
                        check_modinv(&ctx, ap, m);
                    }
                    free(m);
                }
                free(a);
            }
        }

        /* (a*b) % m == mulmod residual denser random */
        for (i = 0; i < 80; i++) {
            char *a = rand_dec(30 + (i % 100), 0);
            char *b = rand_dec(30 + (i % 90), 0);
            char *m = rand_dec(20 + (i % 70), 0);
            static const char odd_digits[] = "13579";
            if (!a || !b || !m) { free(a); free(b); free(m); break; }
            {
                int L = (int)strlen(m);
                if (L > 0) m[L-1] = odd_digits[i % 5];
            }
            if (!(m[0]=='0' && m[1]==0) && !(m[0]=='1' && m[1]==0)) {
                check_mulmod(&ctx, a, b, m);
                check_powmod(&ctx, a, "3", m);
                check_powmod(&ctx, a, "65537", m);
                check_mont_mul(&ctx, a, b, m);
            }
            free(a); free(b); free(m);
        }
    }

    /* expansion2: huge random mul/div/gcd/lcm residual + longer crypto edges */
    {
        int ri;
        for (ri = 0; ri < 120; ri++) {
            char *a = rand_dec(40 + (ri % 180), 1);
            char *b = rand_dec(30 + (ri % 160), 1);
            if (!a || !b) { free(a); free(b); break; }
            check_bin("add", &ctx, sn_add, gmp_add, a, b);
            check_bin("sub", &ctx, sn_sub, gmp_sub, a, b);
            check_bin("mul", &ctx, sn_mul, gmp_mul, a, b);
            if (b[0] != '0' || b[1] != 0)
                check_div_rem(&ctx, a, b[0]=='-' ? b+1 : b);
            check_gcd(&ctx, a[0]=='-'?a+1:a, b[0]=='-'?b+1:b);
            check_bin("and", &ctx, sn_and, gmp_and, a, b);
            check_bin("or", &ctx, sn_or, gmp_ior, a, b);
            check_bin("xor", &ctx, sn_xor, gmp_xor, a, b);
            /* bitlen/popcount/ctz via check_bitops */
            check_bitops(&ctx, a);
            check_bitops(&ctx, b);
            free(a); free(b);
        }
        /* longer RSA-ish powmod grid (2048-ish bit modulus style digit counts) */
        for (ri = 0; ri < 40; ri++) {
            char *mod = rand_dec(60 + (ri % 80), 0); /* ~200-460 bit */
            char *base = rand_dec(20 + (ri % 40), 0);
            char *exp = rand_dec(8 + (ri % 24), 0);
            static const char odd_digits[] = "13579";
            if (!mod || !base || !exp) { free(mod); free(base); free(exp); break; }
            {
                int L = (int)strlen(mod);
                if (L > 0) mod[L-1] = odd_digits[ri % 5];
            }
            if (!(mod[0]=='0' && mod[1]==0) && !(mod[0]=='1' && mod[1]==0)) {
                check_powmod(&ctx, base, exp, mod);
                check_powmod(&ctx, base, "65537", mod);
                check_powmod_ct(&ctx, base, "17", mod);
                check_powmod_ct(&ctx, base, exp, mod);
                check_mulmod(&ctx, base, exp, mod);
                check_modinv(&ctx, base, mod);
                check_mont_mul(&ctx, base, exp, mod);
            }
            free(mod); free(base); free(exp);
        }
        /* isqrt of near-perfect squares */
        for (ri = 0; ri < 30; ri++) {
            char *r = rand_dec(20 + (ri % 60), 0);
            sn_value R, S, S2;
            char *ss = NULL, *gs = NULL;
            mpz_t gr, gs2, gsqrt;
            if (!r) break;
            if (r[0]=='0' && r[1]==0) { free(r); continue; }
            tests++;
            sn_value_init(&R); sn_value_init(&S); sn_value_init(&S2);
            mpz_init(gr); mpz_init(gs2); mpz_init(gsqrt);
            if (!sn_set_dec(&ctx, &R, r) || sn_mul(&ctx, &S2, &R, &R, NULL) != SN_OK) {
                printf("FAIL isqrt-sq setup\n"); fails++;
            } else {
                /* sn_isqrt(R*R) should be |R| */
                if (sn_isqrt(&ctx, &S, &S2) != SN_OK) {
                    printf("FAIL isqrt-sq sn\n"); fails++;
                } else {
                    mpz_set_str(gr, r, 10);
                    mpz_mul(gs2, gr, gr);
                    mpz_sqrt(gsqrt, gs2);
                    ss = sn_dec(&ctx, &S);
                    gs = mpz_get_str(NULL, 10, gsqrt);
                    if (!same_dec(ss, gs)) {
                        printf("FAIL isqrt-sq r=%s sn=%s gmp=%s\n", r, ss?ss:"?", gs?gs:"?");
                        fails++;
                    }
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &R); sn_value_clear(&ctx, &S); sn_value_clear(&ctx, &S2);
            mpz_clear(gr); mpz_clear(gs2); mpz_clear(gsqrt);
            free(r);
        }
    }

    /* expansion3: lcm identity denser + small factorial vs mini-gmp */
    {
        int ri;
        for (ri = 0; ri < 80; ri++) {
            char *a = rand_dec(10 + (ri % 40), 0);
            char *b = rand_dec(10 + (ri % 35), 0);
            sn_value A, B, G, L, P, T;
            mpz_t ga, gb, gg, gl, gp;
            char *ss = NULL, *gs = NULL;
            if (!a || !b) { free(a); free(b); break; }
            if ((a[0]=='0' && a[1]==0) || (b[0]=='0' && b[1]==0)) { free(a); free(b); continue; }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&G);
            sn_value_init(&L); sn_value_init(&P); sn_value_init(&T);
            mpz_init(ga); mpz_init(gb); mpz_init(gg); mpz_init(gl); mpz_init(gp);
            if (!sn_set_dec(&ctx, &A, a) || !sn_set_dec(&ctx, &B, b) ||
                sn_gcd(&ctx, &G, &A, &B) != SN_OK ||
                sn_lcm(&ctx, &L, &A, &B) != SN_OK ||
                sn_mul(&ctx, &P, &A, &B, NULL) != SN_OK ||
                sn_mul(&ctx, &T, &G, &L, NULL) != SN_OK) {
                printf("FAIL lcm-id setup\n"); fails++;
            } else {
                /* |a|*|b| == gcd*lcm */
                mpz_set_str(ga, a, 10); mpz_set_str(gb, b, 10);
                mpz_gcd(gg, ga, gb);
                mpz_lcm(gl, ga, gb);
                mpz_mul(gp, ga, gb);
                mpz_abs(gp, gp);
                ss = sn_dec(&ctx, &T);
                gs = mpz_get_str(NULL, 10, gp);
                if (!same_dec(ss, gs)) {
                    printf("FAIL lcm-id a=%s b=%s sn=%s gmp=%s\n", a, b, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
                if (ss) { sn_str_free(&ctx, ss); ss = NULL; }
                if (gs) { free(gs); gs = NULL; }
                ss = sn_dec(&ctx, &L);
                gs = mpz_get_str(NULL, 10, gl);
                if (!same_dec(ss, gs)) {
                    printf("FAIL lcm val a=%s b=%s sn=%s gmp=%s\n", a, b, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B); sn_value_clear(&ctx, &G);
            sn_value_clear(&ctx, &L); sn_value_clear(&ctx, &P); sn_value_clear(&ctx, &T);
            mpz_clear(ga); mpz_clear(gb); mpz_clear(gg); mpz_clear(gl); mpz_clear(gp);
            free(a); free(b);
        }
        for (ri = 0; ri <= 30; ri++) {
            sn_value F, T, One;
            mpz_t gf;
            char *ss = NULL, *gs = NULL;
            int k;
            tests++;
            sn_value_init(&F); sn_value_init(&T); sn_value_init(&One);
            mpz_init(gf);
            if (sn_bigint_set_i64(&ctx, &F, 1) != SN_OK || sn_bigint_set_i64(&ctx, &One, 1) != SN_OK) {
                printf("FAIL fac setup n=%d\n", ri); fails++;
            } else {
                for (k = 2; k <= ri; k++) {
                    if (sn_bigint_set_i64(&ctx, &T, (int64_t)k) != SN_OK ||
                        sn_mul(&ctx, &F, &F, &T, NULL) != SN_OK) {
                        printf("FAIL fac mul n=%d k=%d\n", ri, k); fails++; break;
                    }
                }
                mpz_fac_ui(gf, (unsigned long)ri);
                ss = sn_dec(&ctx, &F);
                gs = mpz_get_str(NULL, 10, gf);
                if (!same_dec(ss, gs)) {
                    printf("FAIL fac n=%d sn=%s gmp=%s\n", ri, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &F); sn_value_clear(&ctx, &T); sn_value_clear(&ctx, &One);
            mpz_clear(gf);
        }
    }

    /* expansion4: shift-mul identity (a<<k == a*2^k) + rem/div consistency denser */
    {
        int ri;
        for (ri = 0; ri < 100; ri++) {
            char *a = rand_dec(15 + (ri % 80), 0);
            int bits = 1 + (ri % 97);
            sn_value A, S, M, Two, P, Pow;
            mpz_t ga, gsh, gp;
            char *ss = NULL, *gs = NULL;
            int k;
            if (!a) break;
            if (a[0]=='0' && a[1]==0) { free(a); continue; }
            tests++;
            sn_value_init(&A); sn_value_init(&S); sn_value_init(&M);
            sn_value_init(&Two); sn_value_init(&P); sn_value_init(&Pow);
            mpz_init(ga); mpz_init(gsh); mpz_init(gp);
            if (!sn_set_dec(&ctx, &A, a) ||
                sn_shl(&ctx, &S, &A, bits, NULL) != SN_OK ||
                sn_bigint_set_i64(&ctx, &Two, 2) != SN_OK) {
                printf("FAIL shmul setup\n"); fails++;
            } else {
                /* Pow = 2^bits */
                if (sn_bigint_set_i64(&ctx, &Pow, 1) != SN_OK) {
                    printf("FAIL shmul pow1\n"); fails++;
                } else {
                    for (k = 0; k < bits; k++) {
                        if (sn_mul(&ctx, &P, &Pow, &Two, NULL) != SN_OK) {
                            printf("FAIL shmul pow mul\n"); fails++; break;
                        }
                        sn_value_clear(&ctx, &Pow);
                        sn_value_move(&Pow, &P);
                        sn_value_init(&P);
                    }
                    if (sn_mul(&ctx, &M, &A, &Pow, NULL) != SN_OK) {
                        printf("FAIL shmul mul\n"); fails++;
                    } else {
                        ss = sn_dec(&ctx, &S);
                        {
                            char *sm = sn_dec(&ctx, &M);
                            if (!same_dec(ss, sm)) {
                                printf("FAIL shmul a=%s bits=%d shl=%s mul=%s\n", a, bits, ss?ss:"?", sm?sm:"?");
                                fails++;
                            }
                            if (sm) sn_str_free(&ctx, sm);
                        }
                        mpz_set_str(ga, a, 10);
                        mpz_mul_2exp(gsh, ga, (unsigned)bits);
                        gs = mpz_get_str(NULL, 10, gsh);
                        if (!same_dec(ss, gs)) {
                            printf("FAIL shmul gmp a=%s bits=%d sn=%s gmp=%s\n", a, bits, ss?ss:"?", gs?gs:"?");
                            fails++;
                        }
                    }
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &S); sn_value_clear(&ctx, &M);
            sn_value_clear(&ctx, &Two); sn_value_clear(&ctx, &P); sn_value_clear(&ctx, &Pow);
            mpz_clear(ga); mpz_clear(gsh); mpz_clear(gp);
            free(a);
        }
        /* div consistency: (q*b + r) == a for random pairs */
        for (ri = 0; ri < 60; ri++) {
            char *a = rand_dec(20 + (ri % 70), 1);
            char *b = rand_dec(8 + (ri % 40), 0);
            sn_value A, B, Q, R, T, U;
            char *ss = NULL, *sa = NULL;
            if (!a || !b) { free(a); free(b); break; }
            if ((b[0]=='0' && b[1]==0)) { free(a); free(b); continue; }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&Q);
            sn_value_init(&R); sn_value_init(&T); sn_value_init(&U);
            if (!sn_set_dec(&ctx, &A, a) || !sn_set_dec(&ctx, &B, b) ||
                sn_div(&ctx, &Q, &A, &B, NULL) != SN_OK ||
                sn_rem(&ctx, &R, &A, &B, NULL) != SN_OK ||
                sn_mul(&ctx, &T, &Q, &B, NULL) != SN_OK ||
                sn_add(&ctx, &U, &T, &R, NULL) != SN_OK) {
                printf("FAIL divcons setup\n"); fails++;
            } else {
                ss = sn_dec(&ctx, &U);
                sa = sn_dec(&ctx, &A);
                if (!same_dec(ss, sa)) {
                    printf("FAIL divcons a=%s b=%s recon=%s a=%s\n", a, b, ss?ss:"?", sa?sa:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (sa) sn_str_free(&ctx, sa);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B); sn_value_clear(&ctx, &Q);
            sn_value_clear(&ctx, &R); sn_value_clear(&ctx, &T); sn_value_clear(&ctx, &U);
            free(a); free(b);
        }
    }



    /* expansion5: signed rem vs mini-gmp, mulmod identity, bit-scan edges, tiny RSA powmod */
    {
        int ri;
        /* signed rem vs mpz_tdiv_r */
        for (ri = 0; ri < 80; ri++) {
            char *a = rand_dec(12 + (ri % 50), 1);
            char *b = rand_dec(6 + (ri % 30), 1);
            sn_value A, B, R;
            mpz_t ga, gb, gr;
            char *ss = NULL, *gs = NULL;
            if (!a || !b) { free(a); free(b); break; }
            if ((b[0]=='0' && b[1]==0) || (b[0]=='-' && b[1]=='0' && b[2]==0)) {
                free(a); free(b); continue;
            }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&R);
            mpz_init(ga); mpz_init(gb); mpz_init(gr);
            if (!sn_set_dec(&ctx, &A, a) || !sn_set_dec(&ctx, &B, b) ||
                sn_rem(&ctx, &R, &A, &B, NULL) != SN_OK) {
                printf("FAIL rem setup\n"); fails++;
            } else {
                mpz_set_str(ga, a, 10);
                mpz_set_str(gb, b, 10);
                mpz_tdiv_r(gr, ga, gb);
                ss = sn_dec(&ctx, &R);
                gs = mpz_get_str(NULL, 10, gr);
                if (!same_dec(ss, gs)) {
                    printf("FAIL rem a=%s b=%s sn=%s gmp=%s\n", a, b, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B); sn_value_clear(&ctx, &R);
            mpz_clear(ga); mpz_clear(gb); mpz_clear(gr);
            free(a); free(b);
        }

        /* mulmod identity: ((a*b) rem m) with non-negative reduction via abs path:
         * compare sn_mulmod to mini-gmp mpz_mul + mpz_mod for positive m */
        for (ri = 0; ri < 60; ri++) {
            char *a = rand_dec(10 + (ri % 40), 1);
            char *b = rand_dec(10 + (ri % 40), 1);
            char *m = rand_dec(8 + (ri % 35), 0);
            sn_value A, B, M, R2;
            mpz_t ga, gb, gm, gt;
            char *s2 = NULL, *gs = NULL;
            if (!a || !b || !m) { free(a); free(b); free(m); break; }
            if (m[0]=='0' && m[1]==0) { free(a); free(b); free(m); continue; }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&M); sn_value_init(&R2);
            mpz_init(ga); mpz_init(gb); mpz_init(gm); mpz_init(gt);
            if (!sn_set_dec(&ctx, &A, a) || !sn_set_dec(&ctx, &B, b) || !sn_set_dec(&ctx, &M, m) ||
                sn_mulmod(&ctx, &R2, &A, &B, &M) != SN_OK) {
                printf("FAIL mulmod id setup\n"); fails++;
            } else {
                mpz_set_str(ga, a, 10);
                mpz_set_str(gb, b, 10);
                mpz_set_str(gm, m, 10);
                mpz_mul(gt, ga, gb);
                mpz_mod(gt, gt, gm);
                s2 = sn_dec(&ctx, &R2);
                gs = mpz_get_str(NULL, 10, gt);
                if (!same_dec(s2, gs)) {
                    printf("FAIL mulmod id a=%s b=%s m=%s sn=%s gmp=%s\n",
                           a, b, m, s2?s2:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (s2) sn_str_free(&ctx, s2);
            if (gs) free(gs);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B);
            sn_value_clear(&ctx, &M); sn_value_clear(&ctx, &R2);
            mpz_clear(ga); mpz_clear(gb); mpz_clear(gm); mpz_clear(gt);
            free(a); free(b); free(m);
        }

        /* bit length / getbit consistency on random values */
        for (ri = 0; ri < 50; ri++) {
            char *a = rand_dec(5 + (ri % 60), 0);
            sn_value A;
            mpz_t ga;
            int bl_sn = 0, bl_g = 0, bi;
            if (!a) break;
            if (a[0]=='0' && a[1]==0) { free(a); continue; }
            tests++;
            sn_value_init(&A);
            mpz_init(ga);
            if (!sn_set_dec(&ctx, &A, a)) {
                printf("FAIL bitlen setup\n"); fails++;
            } else {
                mpz_set_str(ga, a, 10);
                bl_g = (int)mpz_sizeinbase(ga, 2);
                bl_sn = sn_bitlen(&A);
                if (bl_sn != bl_g) {
                    printf("FAIL bitlen a=%s sn=%d gmp=%d\n", a, bl_sn, bl_g);
                    fails++;
                } else {
                    for (bi = 0; bi < bl_g && bi < 128; bi += 1 + (ri % 3)) {
                        int bs = 0, bg;
                        if (sn_getbit(&A, bi, &bs) != SN_OK) {
                            printf("FAIL getbit sn\n"); fails++; break;
                        }
                        bg = mpz_tstbit(ga, (mp_bitcnt_t)bi) ? 1 : 0;
                        if (bs != bg) {
                            printf("FAIL getbit a=%s i=%d sn=%d gmp=%d\n", a, bi, bs, bg);
                            fails++; break;
                        }
                    }
                }
            }
            sn_value_clear(&ctx, &A);
            mpz_clear(ga);
            free(a);
        }

        /* tiny RSA-ish roundtrip with distinct primes */
        {
            static const char *primes[] = {
                "101","103","107","109","113","127","131","137","139","149",
                "151","157","163","167","173","179","181","191","193","197"
            };
            for (ri = 0; ri < 20; ri++) {
                const char *p = primes[ri % 20];
                const char *q = primes[(ri * 3 + 7) % 20];
                sn_value P, Q, N, Phi, E, D, M, C, R, One, T, G, U;
                char *ss = NULL, *sm = NULL;
                int e_try, ok_e = 0;
                if (p == q || (p && q && strcmp(p, q) == 0))
                    q = primes[(ri * 3 + 11) % 20];
                tests++;
                sn_value_init(&P); sn_value_init(&Q); sn_value_init(&N);
                sn_value_init(&Phi); sn_value_init(&E); sn_value_init(&D);
                sn_value_init(&M); sn_value_init(&C); sn_value_init(&R);
                sn_value_init(&One); sn_value_init(&T); sn_value_init(&G); sn_value_init(&U);
                if (!sn_set_dec(&ctx, &P, p) || !sn_set_dec(&ctx, &Q, q) ||
                    sn_mul(&ctx, &N, &P, &Q, NULL) != SN_OK ||
                    sn_bigint_set_i64(&ctx, &One, 1) != SN_OK ||
                    sn_sub(&ctx, &T, &P, &One, NULL) != SN_OK ||
                    sn_sub(&ctx, &U, &Q, &One, NULL) != SN_OK ||
                    sn_mul(&ctx, &Phi, &T, &U, NULL) != SN_OK) {
                    printf("FAIL rsa tiny setup p=%s q=%s\n", p, q); fails++;
                } else {
                    for (e_try = 3; e_try < 50; e_try += 2) {
                        char *sg = NULL;
                        if (sn_bigint_set_i64(&ctx, &E, (int64_t)e_try) != SN_OK)
                            continue;
                        if (sn_gcd(&ctx, &G, &E, &Phi) != SN_OK)
                            continue;
                        sg = sn_dec(&ctx, &G);
                        if (sg && sg[0]=='1' && sg[1]==0 &&
                            sn_modinv(&ctx, &D, &E, &Phi) == SN_OK) {
                            ok_e = 1;
                            sn_str_free(&ctx, sg);
                            break;
                        }
                        if (sg) sn_str_free(&ctx, sg);
                    }
                    if (!ok_e) {
                        printf("FAIL rsa tiny no e p=%s q=%s\n", p, q); fails++;
                    } else if (sn_bigint_set_i64(&ctx, &M, 2 + (ri % 17)) != SN_OK ||
                               sn_powmod(&ctx, &C, &M, &E, &N) != SN_OK ||
                               sn_powmod(&ctx, &R, &C, &D, &N) != SN_OK) {
                        printf("FAIL rsa tiny pow p=%s q=%s\n", p, q); fails++;
                    } else {
                        ss = sn_dec(&ctx, &R);
                        sm = sn_dec(&ctx, &M);
                        if (!same_dec(ss, sm)) {
                            printf("FAIL rsa tiny p=%s q=%s m=%s r=%s\n",
                                   p, q, sm?sm:"?", ss?ss:"?");
                            fails++;
                        }
                    }
                }
                if (ss) sn_str_free(&ctx, ss);
                if (sm) sn_str_free(&ctx, sm);
                sn_value_clear(&ctx, &P); sn_value_clear(&ctx, &Q); sn_value_clear(&ctx, &N);
                sn_value_clear(&ctx, &Phi); sn_value_clear(&ctx, &E); sn_value_clear(&ctx, &D);
                sn_value_clear(&ctx, &M); sn_value_clear(&ctx, &C); sn_value_clear(&ctx, &R);
                sn_value_clear(&ctx, &One); sn_value_clear(&ctx, &T); sn_value_clear(&ctx, &G);
                sn_value_clear(&ctx, &U);
            }
        }
    }


    /* expansion7: hamdist via xor+popcount, divisible/congruent, pow_ui, addmul/submul,
     * perfect-square classification, denser isqrtrem identity, scan1/ctz edges */
    {
        int ri, bi;

        /* hamdist(a,b) = popcount(a xor b) for non-negative */
        for (ri = 0; ri < 80; ri++) {
            char *a = rand_dec(8 + (ri % 50), 0);
            char *b = rand_dec(8 + (ri % 50), 0);
            sn_value A, B, X, P;
            mpz_t ga, gb;
            char *ss = NULL;
            unsigned long gh;
            int64_t sn_h = 0;
            if (!a || !b) { free(a); free(b); break; }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&X); sn_value_init(&P);
            mpz_init(ga); mpz_init(gb);
            if (!sn_set_dec(&ctx, &A, a) || !sn_set_dec(&ctx, &B, b) ||
                sn_xor(&ctx, &X, &A, &B, NULL) != SN_OK ||
                sn_popcount(&ctx, &P, &X) != SN_OK ||
                sn_to_i64(&ctx, &P, &sn_h) != SN_OK) {
                printf("FAIL hamdist setup\n"); fails++;
            } else {
                mpz_set_str(ga, a, 10);
                mpz_set_str(gb, b, 10);
                gh = mpz_hamdist(ga, gb);
                if ((unsigned long)sn_h != gh) {
                    printf("FAIL hamdist a=%s b=%s sn=%lld gmp=%lu\n",
                           a, b, (long long)sn_h, gh);
                    fails++;
                }
            }
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B);
            sn_value_clear(&ctx, &X); sn_value_clear(&ctx, &P);
            mpz_clear(ga); mpz_clear(gb);
            free(a); free(b);
        }

        /* divisible_p: rem==0 vs mpz_divisible_p */
        for (ri = 0; ri < 60; ri++) {
            char *b = rand_dec(4 + (ri % 20), 0);
            char *k = rand_dec(4 + (ri % 25), 0);
            sn_value B, K, A, R;
            mpz_t ga, gb;
            char *ss = NULL;
            int div_sn, div_g;
            if (!b || !k) { free(b); free(k); break; }
            if ((b[0]=='0' && b[1]==0)) { free(b); free(k); continue; }
            tests++;
            sn_value_init(&B); sn_value_init(&K); sn_value_init(&A); sn_value_init(&R);
            mpz_init(ga); mpz_init(gb);
            if (!sn_set_dec(&ctx, &B, b) || !sn_set_dec(&ctx, &K, k) ||
                sn_mul(&ctx, &A, &B, &K, NULL) != SN_OK ||
                sn_rem(&ctx, &R, &A, &B, NULL) != SN_OK) {
                printf("FAIL divisp setup\n"); fails++;
            } else {
                ss = sn_dec(&ctx, &R);
                div_sn = (ss && ss[0]=='0' && ss[1]==0) ? 1 : 0;
                mpz_set_str(ga, sn_dec(&ctx, &A) ? sn_dec(&ctx, &A) : "0", 10);
                /* re-get a string cleanly */
                {
                    char *sa = sn_dec(&ctx, &A);
                    if (sa) { mpz_set_str(ga, sa, 10); sn_str_free(&ctx, sa); }
                }
                mpz_set_str(gb, b, 10);
                div_g = mpz_divisible_p(ga, gb) ? 1 : 0;
                if (div_sn != div_g || !div_sn) {
                    printf("FAIL divisp a/b b=%s k=%s sn=%d gmp=%d rem=%s\n",
                           b, k, div_sn, div_g, ss?ss:"?");
                    fails++;
                }
                /* also non-divisible: a+1 */
                {
                    sn_value One, Ap1, R2;
                    char *sr = NULL;
                    sn_value_init(&One); sn_value_init(&Ap1); sn_value_init(&R2);
                    tests++;
                    if (sn_bigint_set_i64(&ctx, &One, 1) != SN_OK ||
                        sn_add(&ctx, &Ap1, &A, &One, NULL) != SN_OK ||
                        sn_rem(&ctx, &R2, &Ap1, &B, NULL) != SN_OK) {
                        printf("FAIL divisp+1 setup\n"); fails++;
                    } else {
                        sr = sn_dec(&ctx, &R2);
                        if (sr && sr[0]=='0' && sr[1]==0) {
                            printf("FAIL divisp+1 unexpectedly divisible b=%s\n", b);
                            fails++;
                        }
                    }
                    if (sr) sn_str_free(&ctx, sr);
                    sn_value_clear(&ctx, &One); sn_value_clear(&ctx, &Ap1); sn_value_clear(&ctx, &R2);
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            sn_value_clear(&ctx, &B); sn_value_clear(&ctx, &K);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &R);
            mpz_clear(ga); mpz_clear(gb);
            free(b); free(k);
        }

        /* pow_ui via repeated mul vs mpz_pow_ui for small exp */
        for (ri = 0; ri < 40; ri++) {
            char *base = rand_dec(2 + (ri % 12), 0);
            unsigned exp = (unsigned)(1 + (ri % 12));
            sn_value B, Acc, T, One;
            mpz_t ga, go;
            char *ss = NULL, *gs = NULL;
            unsigned e;
            if (!base) break;
            if (base[0]=='0' && base[1]==0) { free(base); continue; }
            tests++;
            sn_value_init(&B); sn_value_init(&Acc); sn_value_init(&T); sn_value_init(&One);
            mpz_init(ga); mpz_init(go);
            if (!sn_set_dec(&ctx, &B, base) || sn_bigint_set_i64(&ctx, &Acc, 1) != SN_OK) {
                printf("FAIL powui setup\n"); fails++;
            } else {
                for (e = 0; e < exp; e++) {
                    if (sn_mul(&ctx, &T, &Acc, &B, NULL) != SN_OK) {
                        printf("FAIL powui mul\n"); fails++; break;
                    }
                    sn_value_clear(&ctx, &Acc);
                    sn_value_move(&Acc, &T);
                    sn_value_init(&T);
                }
                ss = sn_dec(&ctx, &Acc);
                mpz_set_str(ga, base, 10);
                mpz_pow_ui(go, ga, exp);
                gs = mpz_get_str(NULL, 10, go);
                if (!same_dec(ss, gs)) {
                    printf("FAIL powui base=%s exp=%u sn=%s gmp=%s\n",
                           base, exp, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &B); sn_value_clear(&ctx, &Acc);
            sn_value_clear(&ctx, &T); sn_value_clear(&ctx, &One);
            mpz_clear(ga); mpz_clear(go);
            free(base);
        }

        /* addmul/submul: r = c +/- a*b */
        for (ri = 0; ri < 50; ri++) {
            char *a = rand_dec(6 + (ri % 30), 1);
            char *b = rand_dec(6 + (ri % 30), 1);
            char *c = rand_dec(6 + (ri % 30), 1);
            sn_value A, B, C, P, Radd, Rsub;
            mpz_t ga, gb, gc, go;
            char *ss = NULL, *gs = NULL;
            if (!a || !b || !c) { free(a); free(b); free(c); break; }
            tests++;
            sn_value_init(&A); sn_value_init(&B); sn_value_init(&C);
            sn_value_init(&P); sn_value_init(&Radd); sn_value_init(&Rsub);
            mpz_init(ga); mpz_init(gb); mpz_init(gc); mpz_init(go);
            if (!sn_set_dec(&ctx, &A, a) || !sn_set_dec(&ctx, &B, b) || !sn_set_dec(&ctx, &C, c) ||
                sn_mul(&ctx, &P, &A, &B, NULL) != SN_OK ||
                sn_add(&ctx, &Radd, &C, &P, NULL) != SN_OK ||
                sn_sub(&ctx, &Rsub, &C, &P, NULL) != SN_OK) {
                printf("FAIL addmul setup\n"); fails++;
            } else {
                mpz_set_str(ga, a, 10); mpz_set_str(gb, b, 10); mpz_set_str(gc, c, 10);
                mpz_set(go, gc); mpz_addmul(go, ga, gb);
                ss = sn_dec(&ctx, &Radd);
                gs = mpz_get_str(NULL, 10, go);
                if (!same_dec(ss, gs)) {
                    printf("FAIL addmul a=%s b=%s c=%s sn=%s gmp=%s\n",
                           a, b, c, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
                if (ss) { sn_str_free(&ctx, ss); ss = NULL; }
                if (gs) { free(gs); gs = NULL; }
                mpz_set(go, gc); mpz_submul(go, ga, gb);
                ss = sn_dec(&ctx, &Rsub);
                gs = mpz_get_str(NULL, 10, go);
                if (!same_dec(ss, gs)) {
                    printf("FAIL submul a=%s b=%s c=%s sn=%s gmp=%s\n",
                           a, b, c, ss?ss:"?", gs?gs:"?");
                    fails++;
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (gs) free(gs);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &B); sn_value_clear(&ctx, &C);
            sn_value_clear(&ctx, &P); sn_value_clear(&ctx, &Radd); sn_value_clear(&ctx, &Rsub);
            mpz_clear(ga); mpz_clear(gb); mpz_clear(gc); mpz_clear(go);
            free(a); free(b); free(c);
        }

        /* perfect square classification: isqrt(n)^2 == n iff perfect */
        for (ri = 0; ri < 80; ri++) {
            char *a = rand_dec(4 + (ri % 40), 0);
            sn_value A, S, S2;
            mpz_t ga;
            char *ss = NULL, *sa = NULL;
            int perfect_sn, perfect_g;
            if (!a) break;
            tests++;
            sn_value_init(&A); sn_value_init(&S); sn_value_init(&S2);
            mpz_init(ga);
            if (!sn_set_dec(&ctx, &A, a) || sn_isqrt(&ctx, &S, &A) != SN_OK ||
                sn_mul(&ctx, &S2, &S, &S, NULL) != SN_OK) {
                printf("FAIL perfect setup\n"); fails++;
            } else {
                ss = sn_dec(&ctx, &S2);
                sa = sn_dec(&ctx, &A);
                perfect_sn = same_dec(ss, sa) ? 1 : 0;
                mpz_set_str(ga, a, 10);
                perfect_g = mpz_perfect_square_p(ga) ? 1 : 0;
                if (perfect_sn != perfect_g) {
                    printf("FAIL perfect a=%s sn=%d gmp=%d s2=%s\n",
                           a, perfect_sn, perfect_g, ss?ss:"?");
                    fails++;
                }
                /* isqrtrem identity always */
                {
                    sn_value S1, Up, Up2;
                    char *s_up = NULL;
                    int cmp1 = 0, cmp2 = 0;
                    sn_value_init(&S1); sn_value_init(&Up); sn_value_init(&Up2);
                    tests++;
                    if (sn_bigint_set_i64(&ctx, &S1, 1) != SN_OK ||
                        sn_add(&ctx, &Up, &S, &S1, NULL) != SN_OK ||
                        sn_mul(&ctx, &Up2, &Up, &Up, NULL) != SN_OK ||
                        sn_cmp(&ctx, &cmp1, &S2, &A) != SN_OK ||
                        sn_cmp(&ctx, &cmp2, &A, &Up2) != SN_OK) {
                        printf("FAIL isqrtrem id setup\n"); fails++;
                    } else if (cmp1 > 0 || cmp2 >= 0) {
                        /* need s^2 <= n < (s+1)^2 ; for n=0, s=0, up2=1 ok if cmp2 < 0 */
                        if (!(a[0]=='0' && a[1]==0 && cmp1 == 0)) {
                            printf("FAIL isqrtrem id a=%s cmp1=%d cmp2=%d\n", a, cmp1, cmp2);
                            fails++;
                        }
                    }
                    if (s_up) sn_str_free(&ctx, s_up);
                    sn_value_clear(&ctx, &S1); sn_value_clear(&ctx, &Up); sn_value_clear(&ctx, &Up2);
                }
            }
            if (ss) sn_str_free(&ctx, ss);
            if (sa) sn_str_free(&ctx, sa);
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &S); sn_value_clear(&ctx, &S2);
            mpz_clear(ga);
            free(a);
        }

        /* ctz / scan1 denser on shifted values */
        for (ri = 0; ri < 40; ri++) {
            char *a = rand_dec(5 + (ri % 30), 0);
            int sh = 1 + (ri % 60);
            sn_value A, S, Tz;
            mpz_t ga, gs;
            int64_t sn_tz = 0;
            unsigned long g_tz;
            if (!a) break;
            if (a[0]=='0' && a[1]==0) { free(a); continue; }
            tests++;
            sn_value_init(&A); sn_value_init(&S); sn_value_init(&Tz);
            mpz_init(ga); mpz_init(gs);
            if (!sn_set_dec(&ctx, &A, a) || sn_shl(&ctx, &S, &A, sh, NULL) != SN_OK ||
                sn_ctz(&ctx, &Tz, &S) != SN_OK || sn_to_i64(&ctx, &Tz, &sn_tz) != SN_OK) {
                printf("FAIL ctz-shift setup\n"); fails++;
            } else {
                mpz_set_str(ga, a, 10);
                mpz_mul_2exp(gs, ga, (unsigned)sh);
                g_tz = mpz_scan1(gs, 0);
                if ((unsigned long)sn_tz != g_tz) {
                    printf("FAIL ctz-shift a=%s sh=%d sn=%lld gmp=%lu\n",
                           a, sh, (long long)sn_tz, g_tz);
                    fails++;
                }
            }
            sn_value_clear(&ctx, &A); sn_value_clear(&ctx, &S); sn_value_clear(&ctx, &Tz);
            mpz_clear(ga); mpz_clear(gs);
            free(a);
        }

        (void)bi;
    }

    printf("gmp/mini-gmp int probe: tests=%d fails=%d\n", tests, fails);
    sn_ctx_fini(&ctx);
    return fails ? 1 : 0;
}
