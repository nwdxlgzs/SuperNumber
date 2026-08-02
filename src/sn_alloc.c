#include "sn.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef SN_DEBUG_ALLOC
/* Debug header placed before user pointer: [magic][size][user...] */
#define SN_DBG_MAGIC 0x534E414Cu /* SNAL */

typedef struct sn_dbg_hdr {
    uint32_t magic;
    uint32_t pad;
    size_t   size;
} sn_dbg_hdr;

static void sn_dbg_abort(const char *msg)
{
    fprintf(stderr, "sn SN_DEBUG_ALLOC: %s\n", msg);
    abort();
}

static sn_dbg_hdr *sn_dbg_from_user(void *ptr)
{
    return (sn_dbg_hdr *)((unsigned char *)ptr - sizeof(sn_dbg_hdr));
}

static void *sn_dbg_to_user(sn_dbg_hdr *h)
{
    return (unsigned char *)h + sizeof(sn_dbg_hdr);
}

static void sn_dbg_check_hdr(sn_dbg_hdr *h, size_t expected_osize, int have_osize)
{
    if (!h) sn_dbg_abort("null header");
    if (h->magic != SN_DBG_MAGIC)
        sn_dbg_abort("bad magic (use after free or wrong free?)");
    if (have_osize && h->size != expected_osize)
        sn_dbg_abort("osize mismatch (caller passed wrong old size)");
}
#endif

/* Pure function: no library-global allocator state. Multi-instance safe. */
void *sn_alloc_default(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
#ifndef SN_DEBUG_ALLOC
    (void)osize;
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    if (ptr == NULL)
        return malloc(nsize);
    return realloc(ptr, nsize);
#else
    sn_dbg_hdr *h;
    if (nsize == 0) {
        if (!ptr) return NULL;
        h = sn_dbg_from_user(ptr);
        sn_dbg_check_hdr(h, osize, 1);
        h->magic = 0;
        free(h);
        return NULL;
    }
    if (ptr == NULL) {
        h = (sn_dbg_hdr *)malloc(sizeof(sn_dbg_hdr) + nsize);
        if (!h) return NULL;
        h->magic = SN_DBG_MAGIC;
        h->pad = 0;
        h->size = nsize;
        return sn_dbg_to_user(h);
    }
    h = sn_dbg_from_user(ptr);
    sn_dbg_check_hdr(h, osize, 1);
    h = (sn_dbg_hdr *)realloc(h, sizeof(sn_dbg_hdr) + nsize);
    if (!h) return NULL;
    h->magic = SN_DBG_MAGIC;
    h->pad = 0;
    h->size = nsize;
    return sn_dbg_to_user(h);
#endif
}

void *sn_malloc(sn_ctx *ctx, size_t n)
{
    if (!ctx || !ctx->alloc) return NULL;
    if (n == 0) n = 1;
    return ctx->alloc(ctx->alloc_ud, NULL, 0, n);
}

void *sn_realloc(sn_ctx *ctx, void *p, size_t osize, size_t nsize)
{
    if (!ctx || !ctx->alloc) return NULL;
    return ctx->alloc(ctx->alloc_ud, p, osize, nsize);
}

void sn_free(sn_ctx *ctx, void *p, size_t osize)
{
    if (!ctx || !ctx->alloc || !p) return;
    (void)ctx->alloc(ctx->alloc_ud, p, osize, 0);
}
