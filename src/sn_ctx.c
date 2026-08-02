#include "internal/sn_impl.h"
#include "sn.h"

void sn_ctx_init(sn_ctx *ctx)
{
    if (!ctx) return;
    /* No library globals: each ctx owns its allocator (default = sn_alloc_default). */
    ctx->alloc = sn_alloc_default;
    ctx->alloc_ud = NULL;
    ctx->round = SN_ROUND_NTE;
    ctx->iov = SN_IOV_WRAP;
    ctx->flags = 0;
    ctx->rng = NULL;
    ctx->rng_ud = NULL;
    ctx->rng_state = 0xC0FFEE5EED1234ull;
    ctx->soft_cache = NULL;
}

void sn_ctx_fini(sn_ctx *ctx)
{
    if (!ctx) return;
    sn_soft_cache_free(ctx);
}

void sn_ctx_clear_flags(sn_ctx *ctx)
{
    if (ctx) ctx->flags = 0;
}

unsigned sn_ctx_get_flags(const sn_ctx *ctx)
{
    return ctx ? ctx->flags : 0;
}

void sn_ctx_set_round(sn_ctx *ctx, sn_round r)
{
    if (ctx) ctx->round = r;
}

void sn_ctx_set_int_overflow(sn_ctx *ctx, sn_int_overflow m)
{
    if (ctx) ctx->iov = m;
}

void sn_ctx_set_alloc(sn_ctx *ctx, sn_alloc_fn fn, void *ud)
{
    if (!ctx) return;
    /* Allocator change invalidates any limbs held in soft_cache. */
    sn_soft_cache_free(ctx);
    if (!fn) {
        ctx->alloc = sn_alloc_default;
        ctx->alloc_ud = NULL;
    } else {
        ctx->alloc = fn;
        ctx->alloc_ud = ud;
    }
}
