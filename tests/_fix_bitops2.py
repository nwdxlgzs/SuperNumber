from pathlib import Path
p = Path('tests/_gmp_int_probe.c')
t = p.read_bytes().decode('utf-8', errors='replace').replace('\r\n','\n')
start = t.find('static void check_bitops(sn_ctx *ctx, const char *as)')
if start < 0:
    raise SystemExit('start not found')
end = t.find('static void check_isqrt(sn_ctx *ctx, const char *as)', start)
if end < 0:
    raise SystemExit('end not found')
new = '''static void check_bitops(sn_ctx *ctx, const char *as)
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
        printf("bitops setup FAIL %s\\n", as); fails++; goto done_bo;
    }
    sn_bl = sn_bitlen(&a);
    g_bl = (mpz_sgn(ga) == 0) ? 0 : (int)mpz_sizeinbase(ga, 2);
    if (sn_bl != g_bl) {
        printf("bitlen mismatch %s sn=%d gmp=%d\\n", as, sn_bl, g_bl);
        fails++; goto done_bo;
    }
    if (sn_popcount(ctx, &opc, &a) != SN_OK || sn_to_i64(ctx, &opc, &sn_pc64) != SN_OK) {
        printf("popcount status FAIL %s\\n", as); fails++; goto done_bo;
    }
    g_pc = mpz_popcount(ga);
    if ((unsigned long)sn_pc64 != g_pc) {
        printf("popcount mismatch %s sn=%lld gmp=%lu\\n", as, (long long)sn_pc64, g_pc);
        fails++; goto done_bo;
    }
    if (sn_ctz(ctx, &otz, &a) != SN_OK || sn_to_i64(ctx, &otz, &sn_tz64) != SN_OK) {
        printf("ctz status FAIL %s\\n", as); fails++; goto done_bo;
    }
    if (mpz_sgn(ga) == 0) {
        if (sn_tz64 != 0) {
            printf("ctz zero mismatch %s sn=%lld\\n", as, (long long)sn_tz64); fails++;
        }
    } else {
        g_tz = mpz_scan1(ga, 0);
        if ((unsigned long)sn_tz64 != g_tz) {
            printf("ctz mismatch %s sn=%lld gmp=%lu\\n", as, (long long)sn_tz64, g_tz);
            fails++;
        }
    }
done_bo:
    sn_value_clear(ctx, &a); sn_value_clear(ctx, &opc); sn_value_clear(ctx, &otz);
    mpz_clear(ga);
}

'''
t2 = t[:start] + new + t[end:]
p.write_bytes(t2.replace('\n','\r\n').encode('utf-8'))
print('fixed', start, end)
