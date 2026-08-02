#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

typedef struct {
    size_t live;
    size_t peak;
    size_t fails_after;
    size_t alloc_count;
} mem_stats;

typedef struct {
    size_t size;
} hdr;

static void *counting_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    mem_stats *st = (mem_stats *)ud;
    unsigned char *raw;
    hdr *h;

    (void)osize;
    if (nsize == 0) {
        if (!ptr) return NULL;
        raw = (unsigned char *)ptr - sizeof(hdr);
        h = (hdr *)raw;
        if (st->live >= h->size) st->live -= h->size;
        else st->live = 0;
        free(raw);
        return NULL;
    }

    st->alloc_count++;
    if (st->fails_after && st->alloc_count >= st->fails_after)
        return NULL;

    if (!ptr) {
        raw = (unsigned char *)malloc(sizeof(hdr) + nsize);
        if (!raw) return NULL;
        h = (hdr *)raw;
        h->size = nsize;
        st->live += nsize;
        if (st->live > st->peak) st->peak = st->live;
        return raw + sizeof(hdr);
    }

    raw = (unsigned char *)ptr - sizeof(hdr);
    h = (hdr *)raw;
    if (st->live >= h->size) st->live -= h->size;
    else st->live = 0;
    raw = (unsigned char *)realloc(raw, sizeof(hdr) + nsize);
    if (!raw) return NULL;
    h = (hdr *)raw;
    h->size = nsize;
    st->live += nsize;
    if (st->live > st->peak) st->peak = st->live;
    return raw + sizeof(hdr);
}

int test_alloc_run(void)
{
    sn_ctx ctx;
    mem_stats st;
    void *p;
    sn_value v;

    sn_ctx_init(&ctx);
    p = sn_malloc(&ctx, 32);
    CHECK(p != NULL);
    sn_free(&ctx, p, 32);

    memset(&st, 0, sizeof(st));
    sn_ctx_set_alloc(&ctx, counting_alloc, &st);
    p = sn_malloc(&ctx, 100);
    CHECK(p != NULL);
    CHECK(st.live == 100);
    p = sn_realloc(&ctx, p, 100, 200);
    CHECK(p != NULL);
    CHECK(st.live == 200);
    CHECK(st.peak >= 200);
    sn_free(&ctx, p, 200);
    CHECK(st.live == 0);

    memset(&st, 0, sizeof(st));
    st.fails_after = 1;
    sn_ctx_set_alloc(&ctx, counting_alloc, &st);
    sn_value_init(&v);
    {
        sn_status s = sn_int_new(&ctx, &v, 1024, 0);
        CHECK(s == SN_ERR_NOMEM);
        sn_value_clear(&ctx, &v);
    }

    /* No library-global allocator: re-init uses sn_alloc_default per ctx. */
    sn_ctx_init(&ctx);
    p = sn_malloc(&ctx, 8);
    CHECK(p != NULL);
    sn_free(&ctx, p, 8);

    /* Independent contexts do not share alloc state */
    {
        sn_ctx a, b;
        mem_stats sa, sb;
        void *pa, *pb;
        memset(&sa, 0, sizeof(sa));
        memset(&sb, 0, sizeof(sb));
        sn_ctx_init(&a);
        sn_ctx_init(&b);
        sn_ctx_set_alloc(&a, counting_alloc, &sa);
        sn_ctx_set_alloc(&b, counting_alloc, &sb);
        pa = sn_malloc(&a, 16);
        pb = sn_malloc(&b, 32);
        CHECK(pa != NULL && pb != NULL);
        CHECK(sa.live == 16 && sb.live == 32);
        sn_free(&a, pa, 16);
        sn_free(&b, pb, 32);
        CHECK(sa.live == 0 && sb.live == 0);
    }

    return 0;
}
