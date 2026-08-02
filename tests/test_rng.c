#include "sn.h"
#include "sn_flat.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

typedef struct {
    unsigned char *src;
    size_t len;
    size_t off;
} seq_rng;

static int seq_fill(void *ud, unsigned char *buf, size_t n)
{
    seq_rng *s = (seq_rng *)ud;
    size_t i;
    for (i = 0; i < n; i++) {
        if (s->off >= s->len) return -1;
        buf[i] = s->src[s->off++];
    }
    return 0;
}

int test_rng_run(void)
{
    sn_ctx ctx;
    unsigned char buf[16];
    unsigned char buf2[16];
    uint64_t u, u2;
    sn_value v;
    int64_t x;
    int i;

    sn_ctx_init(&ctx);
    sn_value_init(&v);

    sn_ctx_seed_rng(&ctx, 0x123456789abcdef0ull);
    CHECK(sn_random_bytes(&ctx, buf, sizeof(buf)) == SN_OK);
    sn_ctx_seed_rng(&ctx, 0x123456789abcdef0ull);
    CHECK(sn_random_bytes(&ctx, buf2, sizeof(buf2)) == SN_OK);
    CHECK(memcmp(buf, buf2, sizeof(buf)) == 0);

    sn_ctx_seed_rng(&ctx, 1);
    CHECK(sn_random_u64(&ctx, &u) == SN_OK);
    sn_ctx_seed_rng(&ctx, 1);
    CHECK(sn_random_u64(&ctx, &u2) == SN_OK);
    CHECK(u == u2);

    /* mod range */
    sn_ctx_seed_rng(&ctx, 42);
    for (i = 0; i < 20; i++) {
        CHECK(sn_random_u64_mod(&ctx, &v, 10) == SN_OK);
        CHECK(sn_to_i64(&ctx, &v, &x) == SN_OK);
        CHECK(x >= 0 && x < 10);
    }

    /* injected RNG */
    {
        unsigned char seq[] = {1,2,3,4,5,6,7,8};
        seq_rng s;
        s.src = seq; s.len = sizeof(seq); s.off = 0;
        sn_ctx_set_rng(&ctx, seq_fill, &s);
        CHECK(sn_random_bytes(&ctx, buf, 4) == SN_OK);
        CHECK(buf[0] == 1 && buf[1] == 2 && buf[2] == 3 && buf[3] == 4);
    }

    sn_value_clear(&ctx, &v);
    return 0;
}
