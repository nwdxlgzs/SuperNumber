#include "sn.h"
#include "sn_flat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void sn_test_check(int cond, const char *file, int line, const char *msg);
#define CHECK(c) sn_test_check((c), __FILE__, __LINE__, #c)

static int nearly_eq(double a, double b, double rel)
{
    double d, aa, ab;
    if (a != a && b != b) return 1;
    if (a != a || b != b) return 0;
    if (a == b) return 1;
    d = a - b;
    if (d < 0) d = -d;
    aa = a < 0 ? -a : a;
    ab = b < 0 ? -b : b;
    if (aa < ab) aa = ab;
    if (aa < 1e-12) return d < 1e-12;
    return d <= rel * aa;
}

int test_tensor_run(void)
{
    sn_ctx ctx;
    sn_api api;
    sn_tensor A, B, C, G;
    sn_value v;
    double d;
    char *s = NULL;
    int rows, cols;
    const double a_data[] = {1.0, 2.0, 3.0, 4.0};
    const double b_data[] = {5.0, 6.0, 7.0, 8.0};
    const double g_data[] = {1.0, 1.0};

    sn_ctx_init(&ctx);
    sn_api_bind(&api);
    sn_tensor_init(&A);
    sn_tensor_init(&B);
    sn_tensor_init(&C);
    sn_tensor_init(&G);
    sn_value_init(&v);

    CHECK(api.tensor.from_doubles(&ctx, &A, 2, 2, a_data, 4, 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.from_doubles(&ctx, &B, 2, 2, b_data, 4, 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.dims(&A, &rows, &cols) == SN_OK && rows == 2 && cols == 2);

    /* matmul: [[1,2],[3,4]] * [[5,6],[7,8]] = [[19,22],[43,50]] */
    CHECK(api.tensor.matmul(&ctx, &C, &A, &B, NULL) == SN_OK);
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 0) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, 19.0, 1e-12));
    CHECK(api.tensor.get(&ctx, &v, &C, 1, 1) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, 50.0, 1e-12));

    /* hadamard */
    CHECK(api.tensor.hadamard(&ctx, &C, &A, &B, NULL) == SN_OK);
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 0) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, 5.0, 1e-12));

    /* softmax row of [1,2] ~ [0.26894, 0.73106] */
    CHECK(api.tensor.from_str(&ctx, &A, "1,2; 3,4", 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.softmax_row(&ctx, &C, &A, NULL) == SN_OK);
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 0) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, exp(1)/(exp(1)+exp(2)), 1e-9));
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 1) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, exp(2)/(exp(1)+exp(2)), 1e-9));

    /* rms_norm trivial: vector of ones stays ones with gamma=1 */
    CHECK(api.tensor.from_str(&ctx, &A, "1,1; 2,2", 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.from_doubles(&ctx, &G, 1, 2, g_data, 2, 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.rms_norm(&ctx, &C, &A, &G, 1e-6, NULL) == SN_OK);
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 0) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, 1.0, 1e-6));

    /* layer_norm mean0 var1 for [1,2,3,4] rough check first element finite */
    CHECK(api.tensor.from_str(&ctx, &A, "1,2,3,4", 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.layer_norm(&ctx, &C, &A, NULL, NULL, 1e-5, NULL) == SN_OK);
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 0) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK);
    CHECK(nearly_eq(d, (1.0 - 2.5) / sqrt(1.25 + 1e-5), 1e-5));

    /* sin_pe shape + finite */
    CHECK(api.tensor.sin_pe(&ctx, &C, 3, 4, 10000.0, NULL) == SN_OK);
    CHECK(api.tensor.dims(&C, &rows, &cols) == SN_OK && rows == 3 && cols == 4);
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 0) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, 0.0, 1e-12)); /* sin(0) */
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 1) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, 1.0, 1e-12)); /* cos(0) */

    /* unary silu / gelu path */
    CHECK(api.tensor.from_str(&ctx, &A, "0,1", 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.unary(&ctx, &C, &A, 5, NULL) == SN_OK); /* silu */
    CHECK(api.tensor.get(&ctx, &v, &C, 0, 0) == SN_OK);
    CHECK(sn_to_double(&ctx, &v, &d) == SN_OK && nearly_eq(d, 0.0, 1e-12));

    /* rope does not crash and keeps shape */
    CHECK(api.tensor.from_str(&ctx, &A, "0.1,0.2,0.3,0.4; 0.5,0.6,0.7,0.8", 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.rope(&ctx, &C, &A, 10000.0, NULL) == SN_OK);
    CHECK(api.tensor.dims(&C, &rows, &cols) == SN_OK && rows == 2 && cols == 4);

    /* SDPA self-attn identity-ish shape */
    CHECK(api.tensor.from_str(&ctx, &A, "1,0; 0,1", 11, 52, 1, NULL) == SN_OK);
    CHECK(api.tensor.attention_sdp(&ctx, &C, NULL, &A, &A, &A, 0, 0.0, NULL) == SN_OK);
    CHECK(api.tensor.dims(&C, &rows, &cols) == SN_OK && rows == 2 && cols == 2);

    CHECK(api.tensor.to_str(&ctx, &s, &C) == SN_OK && s != NULL);
    api.tensor.str_free(&ctx, s);

    api.tensor.clear(&ctx, &A);
    api.tensor.clear(&ctx, &B);
    api.tensor.clear(&ctx, &C);
    api.tensor.clear(&ctx, &G);
    sn_value_clear(&ctx, &v);
    sn_ctx_fini(&ctx);
    return 0;
}
