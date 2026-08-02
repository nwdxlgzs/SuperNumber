/* Compare SuperNumber f64 against Bellard softfp64 (playground/libbf). */
#include "sn.h"
#include "sn_flat.h"
#include "softfp.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

static uint64_t bits(double x)
{
    uint64_t u;
    memcpy(&u, &x, 8);
    return u;
}

static double from_bits(uint64_t u)
{
    double x;
    memcpy(&x, &u, 8);
    return x;
}

static int is_nan_bits(uint64_t u)
{
    return ((u & 0x7ff0000000000000ull) == 0x7ff0000000000000ull) &&
           ((u & 0x000fffffffffffffull) != 0);
}

/* SN NaN payload/sign need not match softfp qNaN; only isnan. */
static int same_result(uint64_t sn, uint64_t sf)
{
    if (is_nan_bits(sn) && is_nan_bits(sf)) return 1;
    return sn == sf;
}

static sn_status sn_bin(sn_ctx *ctx, sn_value *out, double xa, double xb,
                        sn_status (*op)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *))
{
    sn_value a, b;
    sn_status st;
    sn_value_init(&a); sn_value_init(&b);
    st = sn_f64(ctx, &a, xa);
    if (st == SN_OK) st = sn_f64(ctx, &b, xb);
    if (st == SN_OK) st = op(ctx, out, &a, &b, NULL);
    sn_value_clear(ctx, &a);
    sn_value_clear(ctx, &b);
    return st;
}

static int check_bin(const char *name, sn_ctx *ctx,
                     sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *),
                     sfloat64 (*sf_op)(sfloat64, sfloat64, RoundingModeEnum, uint32_t *),
                     double xa, double xb, int *tests, int *fails)
{
    sn_value out;
    double sn;
    uint64_t snb, sfb;
    uint32_t fflags = 0;
    sn_value_init(&out);
    (*tests)++;
    if (sn_bin(ctx, &out, xa, xb, sn_op) != SN_OK || sn_to_double(ctx, &out, &sn) != SN_OK) {
        printf("%s status fail %a %a\n", name, xa, xb);
        (*fails)++;
        sn_value_clear(ctx, &out);
        return 0;
    }
    snb = bits(sn);
    sfb = sf_op(bits(xa), bits(xb), RM_RNE, &fflags);
    if (!same_result(snb, sfb)) {
        printf("%s mismatch %a %a sn=%a(%llx) softfp=%a(%llx)\n",
               name, xa, xb, sn, (unsigned long long)snb,
               from_bits(sfb), (unsigned long long)sfb);
        (*fails)++;
        sn_value_clear(ctx, &out);
        return 0;
    }
    sn_value_clear(ctx, &out);
    return 1;
}

static int check_unary(const char *name, sn_ctx *ctx,
                       sn_status (*sn_op)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *),
                       sfloat64 (*sf_op)(sfloat64, RoundingModeEnum, uint32_t *),
                       double xa, int *tests, int *fails)
{
    sn_value a, out;
    double sn;
    uint64_t snb, sfb;
    uint32_t fflags = 0;
    sn_value_init(&a); sn_value_init(&out);
    (*tests)++;
    if (sn_f64(ctx, &a, xa) != SN_OK || sn_op(ctx, &out, &a, NULL) != SN_OK ||
        sn_to_double(ctx, &out, &sn) != SN_OK) {
        printf("%s status fail %a\n", name, xa);
        (*fails)++;
        sn_value_clear(ctx, &a); sn_value_clear(ctx, &out);
        return 0;
    }
    snb = bits(sn);
    sfb = sf_op(bits(xa), RM_RNE, &fflags);
    if (!same_result(snb, sfb)) {
        printf("%s mismatch %a sn=%a(%llx) softfp=%a(%llx)\n",
               name, xa, sn, (unsigned long long)snb,
               from_bits(sfb), (unsigned long long)sfb);
        (*fails)++;
    }
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &out);
    return 1;
}

static int check_fma(sn_ctx *ctx, double xa, double xb, double xc, int *tests, int *fails)
{
    sn_value a, b, c, out;
    double sn;
    uint64_t snb, sfb;
    uint32_t fflags = 0;
    sn_value_init(&a); sn_value_init(&b); sn_value_init(&c); sn_value_init(&out);
    (*tests)++;
    if (sn_f64(ctx, &a, xa) != SN_OK || sn_f64(ctx, &b, xb) != SN_OK || sn_f64(ctx, &c, xc) != SN_OK ||
        sn_fma(ctx, &out, &a, &b, &c, NULL) != SN_OK || sn_to_double(ctx, &out, &sn) != SN_OK) {
        printf("fma status fail\n");
        (*fails)++;
        goto done;
    }
    snb = bits(sn);
    sfb = fma_sf64(bits(xa), bits(xb), bits(xc), RM_RNE, &fflags);
    if (!same_result(snb, sfb)) {
        printf("fma mismatch %a %a %a sn=%a(%llx) softfp=%a(%llx)\n",
               xa, xb, xc, sn, (unsigned long long)snb,
               from_bits(sfb), (unsigned long long)sfb);
        (*fails)++;
    }
done:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &b);
    sn_value_clear(ctx, &c); sn_value_clear(ctx, &out);
    return 1;
}

int main(void)
{
    sn_ctx ctx;
    int tests = 0, fails = 0, i, j, n, r;
    uint64_t seed = 0xDEADBEEFCAFEBABEULL;
    double xs[] = {
        0.0, -0.0, 1.0, -1.0, 2.0, 0.5, 0.1, 3.141592653589793,
        1e-200, 1e200, 2.2250738585072014e-308, 5e-324, 1e308,
        INFINITY, -INFINITY,
        1.0 + 0x1p-52, 0x1.fffffffffffffp-1,
        0x1p-1074, -0x1p-1074, 0x1.fffffffffffffp1023,
        NAN, -NAN
    };

    sn_ctx_init(&ctx);
    sn_ctx_set_round(&ctx, SN_ROUND_NTE);
    n = (int)(sizeof(xs) / sizeof(xs[0]));

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            double xa = xs[i], xb = xs[j];
            check_bin("add", &ctx, sn_add, add_sf64, xa, xb, &tests, &fails);
            check_bin("sub", &ctx, sn_sub, sub_sf64, xa, xb, &tests, &fails);
            check_bin("mul", &ctx, sn_mul, mul_sf64, xa, xb, &tests, &fails);
            if (!(xb == 0.0 && xa == 0.0) || 1) /* always try div; softfp handles 0/0 */
                check_bin("div", &ctx, sn_div, div_sf64, xa, xb, &tests, &fails);
            if (fails > 40) goto done;
        }
        check_unary("sqrt", &ctx, sn_sqrt, sqrt_sf64, xs[i], &tests, &fails);
    }

    /* random dense finite range */
    for (r = 0; r < 2500; r++) {
        uint64_t ua, ub, uc;
        double xa, xb, xc;
        seed = seed * 6364136223846793005ULL + 1ULL; ua = seed;
        seed = seed * 6364136223846793005ULL + 1ULL; ub = seed;
        seed = seed * 6364136223846793005ULL + 1ULL; uc = seed;
        /* mix normals + occasional subnormals */
        if ((r % 17) == 0) {
            ua = (ua & 0x000fffffffffffffull) | ((ua & 1) ? 0x8000000000000000ull : 0);
        } else if ((r % 11) == 0) {
            ua = (ua & 0x000fffffffffffffull) | 0x0010000000000000ull; /* min normal-ish */
        } else {
            ua = (ua & 0x800fffffffffffffull) | (((ua >> 52) % 0x7fe) << 52);
            if (((ua >> 52) & 0x7ff) == 0) ua |= 0x3ff0000000000000ull;
            if (((ua >> 52) & 0x7ff) == 0x7ff) ua = (ua & ~0x7ff0000000000000ull) | 0x3fe0000000000000ull;
        }
        ub = (ub & 0x800fffffffffffffull) | ((((ub >> 52) % 0x7fe) + 1) << 52);
        uc = (uc & 0x800fffffffffffffull) | ((((uc >> 52) % 0x7fe) + 1) << 52);
        memcpy(&xa, &ua, 8); memcpy(&xb, &ub, 8); memcpy(&xc, &uc, 8);
        check_bin("add", &ctx, sn_add, add_sf64, xa, xb, &tests, &fails);
        check_bin("mul", &ctx, sn_mul, mul_sf64, xa, xb, &tests, &fails);
        check_bin("div", &ctx, sn_div, div_sf64, xa, xb, &tests, &fails);
        check_fma(&ctx, xa, xb, xc, &tests, &fails);
        if ((r % 5) == 0) check_unary("sqrt", &ctx, sn_sqrt, sqrt_sf64, fabs(xa), &tests, &fails);
        if (fails > 40) goto done;
    }

    /* classic denorm edge */
    check_bin("div", &ctx, sn_div, div_sf64, 2.0, 1.7976931348623157e+308, &tests, &fails);
    check_bin("div", &ctx, sn_div, div_sf64, 3.141592653589793, 1.7976931348623157e+308, &tests, &fails);
    check_fma(&ctx, 3.0, 4.0, 5.0, &tests, &fails);
    check_fma(&ctx, 1.0 + 0x1p-52, 1.0 + 0x1p-52, -1.0, &tests, &fails);

done:
    sn_ctx_fini(&ctx);
    printf("softfp64 vs SN: tests=%d fails=%d\n", tests, fails);
    return fails ? 1 : 0;
}
