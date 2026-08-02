#include "internal/sn_impl.h"

/* Pluggable RNG + default xorshift64* PRNG.
 * Default RNG is NOT cryptographically secure. Inject a CSPRNG for production.
 * Constant-time: not provided for mulmod/powmod (variable-time limb ops).
 */

static uint64_t sn_xorshift64star(uint64_t *state)
{
    uint64_t x = *state;
    if (x == 0) x = 0x9E3779B97F4A7C15ull;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 0x2545F4914F6CDD1Dull;
}

static int sn_default_rng_fill(void *ud, unsigned char *buf, size_t n)
{
    uint64_t *st = (uint64_t *)ud;
    size_t i = 0;
    if (!st || !buf) return -1;
    while (i < n) {
        uint64_t r = sn_xorshift64star(st);
        size_t k;
        for (k = 0; k < 8 && i < n; k++, i++)
            buf[i] = (unsigned char)(r >> (k * 8));
    }
    return 0;
}

void sn_ctx_set_rng(sn_ctx *ctx, sn_rng_fn fn, void *ud)
{
    if (!ctx) return;
    if (!fn) {
        ctx->rng = sn_default_rng_fill;
        if (ctx->rng_state == 0)
            ctx->rng_state = 0xC0FFEE5EED1234ull;
        ctx->rng_ud = &ctx->rng_state;
    } else {
        ctx->rng = fn;
        ctx->rng_ud = ud;
    }
}

void sn_ctx_seed_rng(sn_ctx *ctx, uint64_t seed)
{
    if (!ctx) return;
    ctx->rng_state = seed ? seed : 0xC0FFEE5EED1234ull;
    if (!ctx->rng) {
        ctx->rng = sn_default_rng_fill;
        ctx->rng_ud = &ctx->rng_state;
    } else if (ctx->rng == sn_default_rng_fill) {
        ctx->rng_ud = &ctx->rng_state;
    }
}

static void sn_ensure_rng(sn_ctx *ctx)
{
    if (!ctx->rng) {
        ctx->rng = sn_default_rng_fill;
        if (ctx->rng_state == 0)
            ctx->rng_state = 0xC0FFEE5EED1234ull;
        ctx->rng_ud = &ctx->rng_state;
    }
}

sn_status sn_random_bytes(sn_ctx *ctx, unsigned char *buf, size_t n)
{
    if (!ctx || !buf) return SN_ERR_ARG;
    if (n == 0) return SN_OK;
    sn_ensure_rng(ctx);
    if (ctx->rng(ctx->rng_ud, buf, n) != 0)
        return SN_ERR_INVALID;
    return SN_OK;
}

sn_status sn_random_u64(sn_ctx *ctx, uint64_t *out)
{
    unsigned char b[8];
    sn_status st;
    int i;
    if (!out) return SN_ERR_ARG;
    st = sn_random_bytes(ctx, b, 8);
    if (st != SN_OK) return st;
    *out = 0;
    for (i = 0; i < 8; i++)
        *out |= ((uint64_t)b[i]) << (8 * i);
    return SN_OK;
}

/* Uniform integer in [0, bound) as BIGINT. bound must be in 1..2^64. */
sn_status sn_random_u64_mod(sn_ctx *ctx, sn_value *out, uint64_t bound)
{
    uint64_t r, lim, x;
    sn_status st;

    if (!ctx || !out || bound == 0) return SN_ERR_ARG;

    if (bound == 1)
        return sn_bigint_set_u64(ctx, out, 0);

    lim = (UINT64_MAX / bound) * bound;
    do {
        st = sn_random_u64(ctx, &r);
        if (st != SN_OK) return st;
    } while (r >= lim);
    x = r % bound;
    return sn_bigint_set_u64(ctx, out, x);
}
