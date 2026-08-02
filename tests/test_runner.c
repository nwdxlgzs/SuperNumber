#include <stdio.h>

int test_alloc_run(void);
int test_int_run(void);
int test_float_run(void);
int test_crypto_run(void);
int test_math_run(void);
int test_rng_run(void);
int test_extra_run(void);
int test_complex_run(void);
int test_cross_run(void);
int test_tensor_run(void);

static int g_failed = 0;
static int g_passed = 0;

void sn_test_check(int cond, const char *file, int line, const char *msg)
{
    if (cond) {
        g_passed++;
    } else {
        g_failed++;
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, msg);
    }
}

int main(void)
{
    printf("SuperNumber tests\n");
    if (test_alloc_run() != 0) return 1;
    if (test_int_run() != 0) return 1;
    if (test_float_run() != 0) return 1;
    if (test_crypto_run() != 0) return 1;
    if (test_math_run() != 0) return 1;
    if (test_rng_run() != 0) return 1;
    if (test_extra_run() != 0) return 1;
    if (test_complex_run() != 0) return 1;
    if (test_cross_run() != 0) return 1;
    if (test_tensor_run() != 0) return 1;
    printf("Result: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
