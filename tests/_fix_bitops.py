from pathlib import Path
p = Path('tests/_gmp_int_probe.c')
t = p.read_bytes().decode('utf-8', errors='replace')
old = '''static void check_bitops(sn_ctx *ctx, const char *as)
{
    sn_value a;
    mpz_t ga;
    int sn_pc, sn_tz, g_pc, g_tz;
    int sn_bl, g_bl;
    sn_value_init(&a);
    mpz_init(ga);
    tests++;
    if (!sn_set_dec(ctx, &a, as) || mpz_set_str(ga, as, 10) != 0) {
        printf("getsetbit setup FAIL %s\\n", as); fails++; goto done_bo;
    }
    sn_bl = sn_bitlen(&a);
    g_bl = (mpz_sgn(ga) == 0) ? 0 : (int)mpz_sizeinbase(ga, 2);
    if (sn_bl != g_bl) {
        printf("bitlen mismatch %s sn=%d gmp=%d\\n", as, sn_bl, g_bl);
        fails++; goto done_bo;
    }
    if (sn_popcount(&a, &sn_pc) != SN_OK) {
        printf("popcount status FAIL %s\\n", as); fails++; goto done_bo;
    }
    g_pc = (int)mpz_popcount(ga);
    if (sn_pc != g_pc) {
        printf("popcount mismatch %s sn=%d gmp=%d\\n", as, sn_pc, g_pc);
        fails++; goto done_bo;
    }
    if (sn_ctz(&a, &sn_tz) != SN_OK) {
        printf("ctz status FAIL %s\\n", as); fails++; goto done_bo;
    }
    if (mpz_sgn(ga) == 0) {
        /* SN and GMP may differ on zero; require non-negative SN result */
        if (sn_tz < 0) {
            printf("ctz zero negative %s sn=%d\\n", as, sn_tz); fails++;
        }
    } else {
        g_tz = (int)mpz_scan1(ga, 0);
        if (sn_tz != g_tz) {
            printf("ctz mismatch %s sn=%d gmp=%d\\n", as, sn_tz, g_tz);
            fails++;
        }
    }
done_bo:
    sn_value_clear(ctx, &a);
    mpz_clear(ga);
}
'''
# actual text uses setup FAIL not getsetbit
old = None
import re
m = re.search(r'static void check_bitops\(sn_ctx \*ctx, const char \*as\)\s*\{[\s\S]*?\n\}\n\n\nstatic void check_isqrt', t)
if not m:
    m = re.search(r'static void check_bitops\(sn_ctx \*ctx, const char \*as\)\s*\{[\s\S]*?\n\}\r?\n\r?\nstatic void check_isqrt', t)
if not m:
    raise SystemExit('check_bitops block not found')
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

static void check_isqrt'''
# replace matched block including check_isqrt start
end = m.group(0)
# keep trailing static void check_isqrt
if not end.rstrip().endswith('check_isqrt'):
    # find with group
    pass
t2 = t[:m.start()] + new + t[m.end()-len('static void check_isqrt'):]
# safer: reconstruct
t2 = t[:m.start()] + new + t[m.end():].lstrip()
# if we already included check_isqrt in new, strip duplicate
# m.end includes up to check_isqrt, and new ends with check_isqrt, then t[m.end():] starts after check_isqrt
# Actually m ends at check_isqrt start inclusive? pattern ends with check_isqrt
# Let's re-do carefully
full = m.group(0)
# full ends with 'static void check_isqrt'
assert full.rstrip().endswith('check_isqrt') or 'check_isqrt' in full[-40:]
t2 = t[:m.start()] + new + t[m.end():]
# if pattern consumed 'static void check_isqrt' without following, restore:
if not t2[m.start():m.start()+30].startswith('static void check_bitops'):
    pass
# Ensure check_isqrt remains
if 'static void check_isqrt' not in t2:
    raise SystemExit('lost check_isqrt')
p.write_bytes(t2.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))
print('fixed check_bitops API')
# verify compile signature fragment
print('has sn_popcount(ctx' , 'sn_popcount(ctx, &opc, &a)' in t2)
