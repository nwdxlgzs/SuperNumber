from pathlib import Path
import re

# 1) Expand specials: more xs + higher m
sp = Path('tests/_specials_mp_res_probe.c')
st = sp.read_text(encoding='utf-8', errors='replace')
old_xs = '''    static const double xs_pos[] = { 0.6, 1.0, 1.5, 2.0, 3.5, 5.0, 8.0 };
    static const double xs_refl[] = { 0.2, 0.3, 0.4 };
    static const double xs_neg[] = { -0.5, -1.5 };
    static const int ms[] = { 64, 80, 112 };'''
new_xs = '''    static const double xs_pos[] = { 0.6, 0.75, 1.0, 1.25, 1.5, 2.0, 2.5, 3.5, 5.0, 6.5, 8.0, 10.0 };
    static const double xs_refl[] = { 0.15, 0.2, 0.3, 0.4, 0.55 };
    static const double xs_neg[] = { -0.5, -1.5, -2.5 };
    static const int ms[] = { 64, 80, 112, 160 };'''
if old_xs not in st:
    raise SystemExit('specials xs block not found')
st = st.replace(old_xs, new_xs, 1)
# self-elev currently limited m_bits<=80; allow 112 as well for a few points
st2 = st.replace('if (m_bits <= 80 && (i == 0 || i == 2 || i == 4))',
                 'if (m_bits <= 112 && (i == 0 || i == 2 || i == 4 || i == 8))', 1)
if st2 == st:
    print('warn: self-elev condition not updated')
else:
    st = st2
sp.write_text(st, encoding='utf-8')
print('specials expanded')

# 2) Expand libbf residual with sinh/cosh composition before atan2 block
lp = Path('tests/_libbf_mp_res_probe.c')
lt = lp.read_text(encoding='utf-8', errors='replace')
if 'sinh residual' in lt:
    print('sinh already present')
else:
    needle = '    /* atan2 residual vs libbf bf_atan2 */'
    if needle not in lt:
        raise SystemExit('atan2 needle missing')
    block = r'''
    /* sinh / cosh residual vs libbf composition */
    {
        static const double hxs[] = { 0.0, 0.1, -0.1, 0.5, -0.5, 1.0, -1.0, 1.5, 2.0, -2.0 };
        int hi, hj;
        for (hj = 0; hj < (int)(sizeof(ms) / sizeof(ms[0])); hj++) {
            m_bits = ms[hj];
            e_bits = (m_bits >= 160) ? 20 : 15;
            for (hi = 0; hi < (int)(sizeof(hxs) / sizeof(hxs[0])); hi++) {
                double hx = hxs[hi];
                int slack = slacks[hj] + 1;
                sn_value a, out;
                bf_t ba, be, bei, bt, br, two;
                char *s = NULL;
                limb_t prec = (limb_t)m_bits + 64;
                double rel;
                /* sinh */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &be); bf_init(&bfc, &bei);
                bf_init(&bfc, &bt); bf_init(&bfc, &br); bf_init(&bfc, &two);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_sinh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("sinh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_exp(&be, &ba, prec, BF_RNDN);
                    bf_set_si(&bt, -1);
                    bf_mul(&bt, &ba, &bt, prec, BF_RNDN);
                    bf_exp(&bei, &bt, prec, BF_RNDN);
                    bf_sub(&br, &be, &bei, prec, BF_RNDN);
                    bf_set_ui(&two, 2);
                    bf_div(&br, &br, &two, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("sinh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&be); bf_delete(&bei);
                bf_delete(&bt); bf_delete(&br); bf_delete(&two);

                /* cosh */
                tests++;
                sn_value_init(&a); sn_value_init(&out);
                bf_init(&bfc, &ba); bf_init(&bfc, &be); bf_init(&bfc, &bei);
                bf_init(&bfc, &bt); bf_init(&bfc, &br); bf_init(&bfc, &two);
                if (!sn_set_hex_mp(&ctx, &a, hx, e_bits, m_bits) ||
                    sn_cosh(&ctx, &out, &a, NULL) != SN_OK ||
                    sn_to_str(&ctx, &s, &out, 16) != SN_OK || !s) {
                    printf("cosh sn fail x=%a m=%d\n", hx, m_bits); fails++;
                } else {
                    bf_set_float64(&ba, hx);
                    bf_exp(&be, &ba, prec, BF_RNDN);
                    bf_set_si(&bt, -1);
                    bf_mul(&bt, &ba, &bt, prec, BF_RNDN);
                    bf_exp(&bei, &bt, prec, BF_RNDN);
                    bf_add(&br, &be, &bei, prec, BF_RNDN);
                    bf_set_ui(&two, 2);
                    bf_div(&br, &br, &two, prec, BF_RNDN);
                    bf_round(&br, (limb_t)m_bits + 8, BF_RNDN);
                    if (!residual_ok(&bfc, s, &br, m_bits, slack, &rel)) {
                        printf("cosh residual FAIL x=%a m=%d rel=%.3e sn=%s\n", hx, m_bits, rel, s);
                        fails++;
                    }
                }
                if (s) sn_str_free(&ctx, s); s = NULL;
                sn_value_clear(&ctx, &a); sn_value_clear(&ctx, &out);
                bf_delete(&ba); bf_delete(&be); bf_delete(&bei);
                bf_delete(&bt); bf_delete(&br); bf_delete(&two);
            }
            if (fails > 80) break;
        }
    }

'''
    lt = lt.replace(needle, block + needle, 1)
    lp.write_text(lt, encoding='utf-8')
    print('sinh/cosh residual inserted')

# 3) Expand gmp with bitlen/popcount/ctz
gp = Path('tests/_gmp_int_probe.c')
gt = gp.read_bytes().decode('utf-8', errors='replace')
if 'check_bitops' in gt:
    print('bitops already present')
else:
    helper = r'''
static void check_bitops(sn_ctx *ctx, const char *as)
{
    sn_value a;
    mpz_t ga;
    int sn_pc, sn_tz, g_pc, g_tz;
    int sn_bl, g_bl;
    sn_value_init(&a);
    mpz_init(ga);
    tests++;
    if (!sn_set_dec(ctx, &a, as) || mpz_set_str(ga, as, 10) != 0) {
        printf("bitops setup FAIL %s\n", as); fails++; goto done_bo;
    }
    sn_bl = sn_bitlen(&a);
    g_bl = (mpz_sgn(ga) == 0) ? 0 : (int)mpz_sizeinbase(ga, 2);
    if (sn_bl != g_bl) {
        printf("bitlen mismatch %s sn=%d gmp=%d\n", as, sn_bl, g_bl);
        fails++; goto done_bo;
    }
    if (sn_popcount(&a, &sn_pc) != SN_OK) {
        printf("popcount status FAIL %s\n", as); fails++; goto done_bo;
    }
    g_pc = (int)mpz_popcount(ga);
    if (sn_pc != g_pc) {
        printf("popcount mismatch %s sn=%d gmp=%d\n", as, sn_pc, g_pc);
        fails++; goto done_bo;
    }
    if (sn_ctz(&a, &sn_tz) != SN_OK) {
        printf("ctz status FAIL %s\n", as); fails++; goto done_bo;
    }
    if (mpz_sgn(ga) == 0) {
        /* SN and GMP may differ on zero; require non-negative SN result */
        if (sn_tz < 0) {
            printf("ctz zero negative %s sn=%d\n", as, sn_tz); fails++;
        }
    } else {
        g_tz = (int)mpz_scan1(ga, 0);
        if (sn_tz != g_tz) {
            printf("ctz mismatch %s sn=%d gmp=%d\n", as, sn_tz, g_tz);
            fails++;
        }
    }
done_bo:
    sn_value_clear(ctx, &a);
    mpz_clear(ga);
}

'''
    m = re.search(r'static void check_mont_mul\(sn_ctx \*ctx,[\s\S]*?\n\}\r?\n', gt)
    if not m:
        # try after check_getsetbit
        m = re.search(r'static void check_getsetbit\(sn_ctx \*ctx,[\s\S]*?\n\}\r?\n', gt)
    if not m:
        raise SystemExit('insert point for bitops helper not found')
    # insert after mont helper if present else after getsetbit
    if 'check_mont_mul' in gt:
        m2 = re.search(r'static void check_mont_mul\(sn_ctx \*ctx,[\s\S]*?\n\}\r?\n', gt)
        gt = gt[:m2.end()] + helper + gt[m2.end():]
    else:
        gt = gt[:m.end()] + helper + gt[m.end():]
    mark = '        check_getsetbit(&ctx, "0");'
    if mark not in gt:
        raise SystemExit('call mark not found')
    calls = '''        check_bitops(&ctx, "0");
        check_bitops(&ctx, "1");
        check_bitops(&ctx, "2");
        check_bitops(&ctx, "255");
        check_bitops(&ctx, "256");
        check_bitops(&ctx, "12345678901234567890");
        check_bitops(&ctx, "1024");
        check_bitops(&ctx, "1000000007");
'''
    gt = gt.replace(mark, calls + mark, 1)
    # random loop
    gt_n = gt.replace('\r\n','\n')
    old = '            if (a) check_getsetbit(&ctx, a);'
    new = '            if (a) { check_getsetbit(&ctx, a); check_bitops(&ctx, a); }'
    if old not in gt_n:
        raise SystemExit('random getset call not found')
    gt_n = gt_n.replace(old, new, 1)
    Path('tests/_gmp_int_probe.c').write_bytes(gt_n.replace('\n','\r\n').encode('utf-8'))
    print('bitops expanded')
print('all patches ok')
