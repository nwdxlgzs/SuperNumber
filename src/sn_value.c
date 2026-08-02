#include "internal/sn_impl.h"
#include <limits.h>
#include <stddef.h>

void sn_raise(sn_ctx *ctx, unsigned flag)
{
    if (ctx) ctx->flags |= flag;
}

sn_int_overflow sn_eff_iov(const sn_ctx *ctx, const sn_op_opt *opt)
{
    if (opt && opt->has_int_overflow) return opt->iov;
    return ctx ? ctx->iov : SN_IOV_WRAP;
}

int sn_limbs_for_bits(int bits)
{
    if (bits <= 0) return 1;
    /* Memory-bound only: reject bit counts that overflow limb index math. */
    if (bits > INT_MAX - (SN_LIMB_BITS - 1))
        return -1;
    return (bits + SN_LIMB_BITS - 1) / SN_LIMB_BITS;
}

void sn_limbs_from_u64(sn_limb *d, int nd, uint64_t x)
{
    int i;
    for (i = 0; i < nd; i++) {
        d[i] = (sn_limb)(x & SN_LIMB_MASK);
        x >>= SN_LIMB_BITS;
    }
}

uint64_t sn_limbs_to_u64(const sn_limb *s, int ns)
{
    uint64_t x = 0;
    int i;
    int n = ns;
    if (n > 2) n = 2;
    for (i = n - 1; i >= 0; i--)
        x = (x << SN_LIMB_BITS) | (uint64_t)(s[i] & SN_LIMB_MASK);
    return x;
}

int sn_value_is_num(const sn_value *v)
{
    return v && (v->kind == SN_KIND_INT || v->kind == SN_KIND_BIGINT);
}

void sn_value_init(sn_value *v)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->kind = SN_KIND_NULL;
}

void sn_value_clear(sn_ctx *ctx, sn_value *v)
{
    if (!v) return;
    if (v->heap && v->cap > 0)
        sn_free(ctx, v->heap, (size_t)v->cap * sizeof(sn_limb));
    sn_value_init(v);
}

sn_status sn_value_reserve(sn_ctx *ctx, sn_value *v, int nlimbs)
{
    sn_limb *p;
    int i, oldcap;
    size_t obytes, nbytes;

    if (!v || nlimbs < 0) return SN_ERR_ARG;
    /* size_t byte count must not overflow; keep nlimbs within int-safe range */
    if ((size_t)nlimbs > (SIZE_MAX / sizeof(sn_limb))) return SN_ERR_NOMEM;
    if (nlimbs <= SN_INLINE_LIMBS && v->cap == 0)
        return SN_OK;
    if (v->cap >= nlimbs)
        return SN_OK;
    if (nlimbs < SN_INLINE_LIMBS)
        nlimbs = SN_INLINE_LIMBS;

    oldcap = v->cap;
    obytes = (size_t)oldcap * sizeof(sn_limb);
    nbytes = (size_t)nlimbs * sizeof(sn_limb);

    if (oldcap == 0) {
        p = (sn_limb *)sn_malloc(ctx, nbytes);
        if (!p) return SN_ERR_NOMEM;
        for (i = 0; i < SN_INLINE_LIMBS; i++)
            p[i] = v->inline_limbs[i];
        for (; i < nlimbs; i++)
            p[i] = 0;
        v->heap = p;
        v->cap = nlimbs;
        return SN_OK;
    }

    p = (sn_limb *)sn_realloc(ctx, v->heap, obytes, nbytes);
    if (!p) return SN_ERR_NOMEM;
    for (i = oldcap; i < nlimbs; i++)
        p[i] = 0;
    v->heap = p;
    v->cap = nlimbs;
    return SN_OK;
}

sn_status sn_value_copy(sn_ctx *ctx, sn_value *out, const sn_value *src)
{
    sn_status st;
    sn_value tmp;
    const sn_limb *sl;
    sn_limb *dl;
    int i, n;

    if (!out || !src) return SN_ERR_ARG;
    if (out == src) return SN_OK;

    if (src->kind == SN_KIND_NULL) {
        sn_value_clear(ctx, out);
        return SN_OK;
    }

    sn_value_init(&tmp);
    tmp.kind = src->kind;
    tmp.width = src->width;
    tmp.is_signed = src->is_signed;
    tmp.e_bits = src->e_bits;
    tmp.m_bits = src->m_bits;
    tmp.nan_enabled = src->nan_enabled;
    tmp.negative = src->negative;
    n = src->nlimbs;
    if (n < 1) n = 1;
    tmp.nlimbs = n;

    st = sn_value_reserve(ctx, &tmp, n);
    if (st != SN_OK) {
        sn_value_clear(ctx, &tmp);
        return st;
    }
    sl = SN_CLIMBS(src);
    dl = SN_LIMBS(&tmp);
    for (i = 0; i < n; i++)
        dl[i] = (i < src->nlimbs && sl) ? sl[i] : 0;

    sn_value_clear(ctx, out);
    sn_value_move(out, &tmp);
    return SN_OK;
}

void sn_value_move(sn_value *out, sn_value *src)
{
    if (!out || !src) return;
    if (out == src) return;
    *out = *src;
    sn_value_init(src);
}

void sn_int_mask(sn_value *v)
{
    sn_limb *limbs;
    int n, full, rem, i;

    if (!v || v->kind != SN_KIND_INT || v->width <= 0) return;
    n = sn_limbs_for_bits(v->width);
    limbs = SN_LIMBS(v);
    if (v->nlimbs < n) {
        for (i = v->nlimbs; i < n; i++) {
            if (v->cap > 0) {
                if (i < v->cap) limbs[i] = 0;
            } else if (i < SN_INLINE_LIMBS) {
                limbs[i] = 0;
            }
        }
    }
    v->nlimbs = n;
    full = v->width / SN_LIMB_BITS;
    rem = v->width % SN_LIMB_BITS;
    if (rem != 0)
        limbs[full] &= ((sn_limb)1 << rem) - 1u;
    for (i = full + (rem ? 1 : 0); i < n; i++)
        limbs[i] = 0;
}

void sn_bigint_normalize(sn_value *v)
{
    sn_limb *limbs;
    if (!v || v->kind != SN_KIND_BIGINT) return;
    limbs = SN_LIMBS(v);
    while (v->nlimbs > 1 && limbs[v->nlimbs - 1] == 0)
        v->nlimbs--;
    if (v->nlimbs <= 0) {
        v->nlimbs = 1;
        limbs[0] = 0;
    }
    if (v->nlimbs == 1 && limbs[0] == 0)
        v->negative = 0;
}

int sn_limb_cmp(const sn_limb *a, int na, const sn_limb *b, int nb)
{
    int i;
    while (na > 1 && a[na - 1] == 0) na--;
    while (nb > 1 && b[nb - 1] == 0) nb--;
    if (na != nb) return (na < nb) ? -1 : 1;
    for (i = na - 1; i >= 0; i--) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

sn_status sn_int_set_zero(sn_ctx *ctx, sn_value *out, int width, int is_signed)
{
    sn_status st;
    sn_limb *limbs;
    int n, i;

    if (!out || width < 1) return SN_ERR_ARG;
    n = sn_limbs_for_bits(width);
    if (n < 1) return SN_ERR_NOMEM;
    sn_value_clear(ctx, out);
    out->kind = SN_KIND_INT;
    out->width = width;
    out->is_signed = is_signed ? 1 : 0;
    st = sn_value_reserve(ctx, out, n);
    if (st != SN_OK) return st;
    limbs = SN_LIMBS(out);
    for (i = 0; i < n; i++) limbs[i] = 0;
    out->nlimbs = n;
    return SN_OK;
}

sn_status sn_limb_add(sn_ctx *ctx, sn_value *r, const sn_limb *a, int na, const sn_limb *b, int nb)
{
    int n = (na > nb ? na : nb) + 1;
    sn_status st;
    sn_limb *d;
    int i;
    uint64_t carry = 0;

    st = sn_value_reserve(ctx, r, n);
    if (st != SN_OK) return st;
    d = SN_LIMBS(r);
    for (i = 0; i < n; i++) {
        uint64_t s = carry;
        if (i < na) s += a[i];
        if (i < nb) s += b[i];
        d[i] = (sn_limb)(s & SN_LIMB_MASK);
        carry = s >> SN_LIMB_BITS;
    }
    r->nlimbs = n;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    return SN_OK;
}

sn_status sn_limb_sub(sn_ctx *ctx, sn_value *r, const sn_limb *a, int na, const sn_limb *b, int nb)
{
    int n = na > nb ? na : nb;
    sn_status st;
    sn_limb *d;
    int i;
    int64_t borrow = 0;

    if (n < 1) n = 1;
    st = sn_value_reserve(ctx, r, n);
    if (st != SN_OK) return st;
    d = SN_LIMBS(r);
    for (i = 0; i < n; i++) {
        int64_t s = borrow;
        s += (i < na) ? (int64_t)a[i] : 0;
        s -= (i < nb) ? (int64_t)b[i] : 0;
        if (s < 0) {
            s += (int64_t)SN_LIMB_MASK + 1;
            borrow = -1;
        } else {
            borrow = 0;
        }
        d[i] = (sn_limb)s;
    }
    r->nlimbs = n;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    return SN_OK;
}

/* Schoolbook multiply into zeroed d[0..na+nb).
 * Outer loop over the shorter factor improves cache locality.
 * Carry uses split high/low so (a*b)+d+c never overflows uint64. */
static void sn_limb_mul_basecase(sn_limb *d, const sn_limb *a, int na, const sn_limb *b, int nb)
{
    int i, j, n = na + nb;
    const sn_limb *x, *y;
    int nx, ny;

    for (i = 0; i < n; i++) d[i] = 0;

    /* Prefer shorter outer operand. */
    if (na <= nb) {
        x = a; nx = na;
        y = b; ny = nb;
    } else {
        x = b; nx = nb;
        y = a; ny = na;
    }

    for (i = 0; i < nx; i++) {
        uint64_t carry = 0;
        sn_limb xi = x[i];
        sn_limb *di;
        if (xi == 0) continue;
        di = d + i;
        /* Unroll by 2 when possible. */
        j = 0;
        for (; j + 1 < ny; j += 2) {
            uint64_t prod0 = (uint64_t)xi * (uint64_t)y[j];
            uint64_t sum0 = (uint64_t)di[j] + (prod0 & SN_LIMB_MASK) + carry;
            di[j] = (sn_limb)(sum0 & SN_LIMB_MASK);
            carry = (prod0 >> SN_LIMB_BITS) + (sum0 >> SN_LIMB_BITS);

            {
                uint64_t prod1 = (uint64_t)xi * (uint64_t)y[j + 1];
                uint64_t sum1 = (uint64_t)di[j + 1] + (prod1 & SN_LIMB_MASK) + carry;
                di[j + 1] = (sn_limb)(sum1 & SN_LIMB_MASK);
                carry = (prod1 >> SN_LIMB_BITS) + (sum1 >> SN_LIMB_BITS);
            }
        }
        for (; j < ny; j++) {
            uint64_t prod = (uint64_t)xi * (uint64_t)y[j];
            uint64_t sum = (uint64_t)di[j] + (prod & SN_LIMB_MASK) + carry;
            di[j] = (sn_limb)(sum & SN_LIMB_MASK);
            carry = (prod >> SN_LIMB_BITS) + (sum >> SN_LIMB_BITS);
        }
        {
            int k = ny;
            while (carry) {
                uint64_t sum = (uint64_t)di[k] + carry;
                di[k] = (sn_limb)(sum & SN_LIMB_MASK);
                carry = sum >> SN_LIMB_BITS;
                k++;
            }
        }
    }
}

/* d[0..n) += s[0..ns) << (shift limbs); n >= ns+shift, d may have room for carry. */
static void sn_limb_add_shifted(sn_limb *d, int n, const sn_limb *s, int ns, int shift)
{
    int i;
    uint64_t carry = 0;
    for (i = 0; i < ns; i++) {
        int k = i + shift;
        uint64_t sum = (uint64_t)d[k] + (uint64_t)s[i] + carry;
        d[k] = (sn_limb)(sum & SN_LIMB_MASK);
        carry = sum >> SN_LIMB_BITS;
    }
    i = ns + shift;
    while (carry && i < n) {
        uint64_t sum = (uint64_t)d[i] + carry;
        d[i] = (sn_limb)(sum & SN_LIMB_MASK);
        carry = sum >> SN_LIMB_BITS;
        i++;
    }
}

/* t[0..max(na,nb)+1) = a + b (unsigned). Returns limb count. */
static int sn_limb_add_raw(sn_limb *t, const sn_limb *a, int na, const sn_limb *b, int nb)
{
    int n = na > nb ? na : nb;
    int i;
    uint64_t carry = 0;
    for (i = 0; i < n; i++) {
        uint64_t s = carry;
        if (i < na) s += a[i];
        if (i < nb) s += b[i];
        t[i] = (sn_limb)(s & SN_LIMB_MASK);
        carry = s >> SN_LIMB_BITS;
    }
    if (carry) {
        t[n] = (sn_limb)carry;
        return n + 1;
    }
    return n > 0 ? n : 1;
}


/*
 * Multi-modulus NTT multiply (optional large-size path).
 * Split each 32-bit limb into two 16-bit digits; convolve under three
 * NTT-friendly primes (primitive root 3), CRT reconstruct, then carry.
 * Threshold is high so schoolbook/Karatsuba win on soft-float sizes.
 */
/* Pure-C multi-mod NTT has high fixed cost; only win past ~few-kbit.
 * 800-digit (~83 limbs) must stay on Karatsuba/schoolbook. */
#define SN_NTT_THRESHOLD 256
#define SN_NTT_DIGIT_BITS 16
#define SN_NTT_DIGIT_MASK 0xFFFFu

/* 167772161 = 2^25*5+1, 469762049 = 2^26*7+1, 998244353 = 2^23*119+1 */
static const uint32_t sn_ntt_mod[3] = {
    167772161u, 469762049u, 998244353u
};
/* Garner: inv(p0) mod p1, inv(p0*p1) mod p2; p0*p1 fits in 64 bits. */
static const uint32_t sn_ntt_inv01 = 104391568u;
static const uint32_t sn_ntt_inv012 = 575867115u;
static const uint64_t sn_ntt_p0p1 = 78812994116517889ull;

#if defined(__SIZEOF_INT128__) || (defined(__GNUC__) && defined(__x86_64__))
__extension__ typedef unsigned __int128 sn_u128;
#  define SN_HAVE_U128 1
#else
#  define SN_HAVE_U128 0
#endif

static uint32_t sn_mod_mul_u32(uint32_t a, uint32_t b, uint32_t mod)
{
    return (uint32_t)(((uint64_t)a * (uint64_t)b) % (uint64_t)mod);
}

static uint32_t sn_mod_pow_u32(uint32_t a, uint32_t e, uint32_t mod)
{
    uint32_t r = 1u;
    while (e) {
        if (e & 1u) r = sn_mod_mul_u32(r, a, mod);
        a = sn_mod_mul_u32(a, a, mod);
        e >>= 1;
    }
    return r;
}

/* In-place radix-2 DIT NTT; root is a primitive n-th root of unity mod mod. */
static void sn_ntt_dit(uint32_t *a, int n, int invert, uint32_t mod, uint32_t proot)
{
    int i, j, len;
    uint32_t n_u;

    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            uint32_t t = a[i];
            a[i] = a[j];
            a[j] = t;
        }
    }

    for (len = 2; len <= n; len <<= 1) {
        uint32_t wlen = sn_mod_pow_u32(proot, (mod - 1u) / (uint32_t)len, mod);
        if (invert)
            wlen = sn_mod_pow_u32(wlen, mod - 2u, mod);
        for (i = 0; i < n; i += len) {
            uint32_t w = 1u;
            int half = len >> 1;
            for (j = 0; j < half; j++) {
                uint32_t u = a[i + j];
                uint32_t v = sn_mod_mul_u32(a[i + j + half], w, mod);
                uint32_t s = u + v;
                uint32_t d = (u >= v) ? (u - v) : (u + mod - v);
                if (s >= mod) s -= mod;
                a[i + j] = s;
                a[i + j + half] = d;
                w = sn_mod_mul_u32(w, wlen, mod);
            }
        }
    }

    if (invert) {
        n_u = sn_mod_pow_u32((uint32_t)n, mod - 2u, mod);
        for (i = 0; i < n; i++)
            a[i] = sn_mod_mul_u32(a[i], n_u, mod);
    }
}

/* Pack na limbs -> 16-bit digits into buf[0..n) (zero-padded). */
static void sn_ntt_pack_digits(uint32_t *buf, int n, const sn_limb *a, int na)
{
    int i, nd = na * 2;
    for (i = 0; i < n; i++) buf[i] = 0;
    for (i = 0; i < na; i++) {
        sn_limb x = a[i];
        if (2 * i < n) buf[2 * i] = (uint32_t)(x & SN_NTT_DIGIT_MASK);
        if (2 * i + 1 < n) buf[2 * i + 1] = (uint32_t)((x >> SN_NTT_DIGIT_BITS) & SN_NTT_DIGIT_MASK);
    }
    (void)nd;
}

/* Garner CRT for three residues -> value in [0, p0*p1*p2). */
#if SN_HAVE_U128
static sn_u128 sn_ntt_crt3(uint32_t r0, uint32_t r1, uint32_t r2)
{
    uint32_t p0 = sn_ntt_mod[0], p1 = sn_ntt_mod[1], p2 = sn_ntt_mod[2];
    uint32_t t1, t2, x1mod;
    uint64_t x1;
    sn_u128 x;

    t1 = (uint32_t)(((uint64_t)((r1 + p1 - (r0 % p1)) % p1) * (uint64_t)sn_ntt_inv01) % p1);
    x1 = (uint64_t)r0 + (uint64_t)p0 * (uint64_t)t1;
    x1mod = (uint32_t)(x1 % (uint64_t)p2);
    t2 = (uint32_t)(((uint64_t)((r2 + p2 - x1mod) % p2) * (uint64_t)sn_ntt_inv012) % p2);
    x = (sn_u128)x1 + (sn_u128)sn_ntt_p0p1 * (sn_u128)t2;
    return x;
}
#else
/* 96-bit fallback: lo64 + hi32 (no __int128) */
static void sn_ntt_crt3_96(uint32_t r0, uint32_t r1, uint32_t r2, uint64_t *lo, uint32_t *hi)
{
    uint32_t p0 = sn_ntt_mod[0], p1 = sn_ntt_mod[1], p2 = sn_ntt_mod[2];
    uint32_t t1, t2, x1mod;
    uint64_t x1, p0p1 = sn_ntt_p0p1;
    uint64_t a0, a1, p0l, p1l, prod_lo, prod_hi, old;

    t1 = (uint32_t)(((uint64_t)((r1 + p1 - (r0 % p1)) % p1) * (uint64_t)sn_ntt_inv01) % p1);
    x1 = (uint64_t)r0 + (uint64_t)p0 * (uint64_t)t1;
    x1mod = (uint32_t)(x1 % (uint64_t)p2);
    t2 = (uint32_t)(((uint64_t)((r2 + p2 - x1mod) % p2) * (uint64_t)sn_ntt_inv012) % p2);

    /* 64x32 -> 96-bit: p0p1 * t2 */
    a0 = (uint32_t)p0p1;
    a1 = p0p1 >> 32;
    p0l = a0 * (uint64_t)t2;
    p1l = a1 * (uint64_t)t2;
    prod_lo = p0l;
    prod_hi = 0;
    old = prod_lo;
    prod_lo += (p1l & 0xFFFFFFFFu) << 32;
    if (prod_lo < old) prod_hi++;
    prod_hi += (p1l >> 32);
    old = prod_lo;
    prod_lo += x1;
    if (prod_lo < old) prod_hi++;
    *lo = prod_lo;
    *hi = (uint32_t)prod_hi;
}
#endif

static sn_status sn_limb_mul_ntt(sn_ctx *ctx, sn_value *r,
                                 const sn_limb *a, int na,
                                 const sn_limb *b, int nb)
{
    int nd_a = na * 2, nd_b = nb * 2;
    int need = nd_a + nd_b; /* convolution length (exact: nd_a+nd_b-1, pad to pow2) */
    int n = 1, i, k;
    int nout;
    sn_status st = SN_OK;
    uint32_t *fa = NULL, *fb = NULL;
    uint32_t *res[3];
    sn_limb *d;
    size_t bytes;

    res[0] = res[1] = res[2] = NULL;
    if (need < 1) need = 1;
    while (n < need) {
        if (n > (1 << 22)) /* cap: avoid absurd sizes / prime order limits */
            return SN_ERR_RANGE;
        n <<= 1;
    }
    /* Max supported by smallest 2-power factor among primes: 2^23 */
    if (n > (1 << 23))
        return SN_ERR_RANGE;

    bytes = (size_t)n * sizeof(uint32_t);
    fa = (uint32_t *)sn_malloc(ctx, bytes);
    fb = (uint32_t *)sn_malloc(ctx, bytes);
    if (!fa || !fb) {
        st = SN_ERR_NOMEM;
        goto cleanup;
    }
    for (k = 0; k < 3; k++) {
        res[k] = (uint32_t *)sn_malloc(ctx, bytes);
        if (!res[k]) {
            st = SN_ERR_NOMEM;
            goto cleanup;
        }
    }

    for (k = 0; k < 3; k++) {
        uint32_t mod = sn_ntt_mod[k];
        uint32_t proot = 3u; /* primitive root for all three primes */
        sn_ntt_pack_digits(fa, n, a, na);
        sn_ntt_pack_digits(fb, n, b, nb);
        /* reduce digits mod p (already < 2^16 < p) */
        sn_ntt_dit(fa, n, 0, mod, proot);
        sn_ntt_dit(fb, n, 0, mod, proot);
        for (i = 0; i < n; i++)
            res[k][i] = sn_mod_mul_u32(fa[i], fb[i], mod);
        sn_ntt_dit(res[k], n, 1, mod, proot);
    }

    nout = na + nb;
    st = sn_value_reserve(ctx, r, nout > 0 ? nout : 1);
    if (st != SN_OK) goto cleanup;
    d = SN_LIMBS(r);
    for (i = 0; i < nout; i++) d[i] = 0;

#if SN_HAVE_U128
    {
        sn_u128 acc = 0;
        int di;
        /* need = nd_a+nd_b digits of product in base 2^16; process n positions */
        for (di = 0; di < n; di++) {
            sn_u128 v = sn_ntt_crt3(res[0][di], res[1][di], res[2][di]);
            acc += v;
            {
                uint32_t dig = (uint32_t)(acc & (sn_u128)SN_NTT_DIGIT_MASK);
                int limb_i = di >> 1;
                if (limb_i < nout) {
                    if (di & 1)
                        d[limb_i] |= (sn_limb)(dig << SN_NTT_DIGIT_BITS);
                    else
                        d[limb_i] |= (sn_limb)dig;
                }
                acc >>= SN_NTT_DIGIT_BITS;
            }
        }
        /* residual carry (should be 0 for correct length, but flush) */
        {
            int di = n;
            while (acc) {
                uint32_t dig = (uint32_t)(acc & (sn_u128)SN_NTT_DIGIT_MASK);
                int limb_i = di >> 1;
                if (limb_i >= nout) {
                    /* need one more limb */
                    st = sn_value_reserve(ctx, r, limb_i + 1);
                    if (st != SN_OK) goto cleanup;
                    d = SN_LIMBS(r);
                    for (i = nout; i <= limb_i; i++) d[i] = 0;
                    nout = limb_i + 1;
                }
                if (di & 1)
                    d[limb_i] |= (sn_limb)(dig << SN_NTT_DIGIT_BITS);
                else
                    d[limb_i] |= (sn_limb)dig;
                acc >>= SN_NTT_DIGIT_BITS;
                di++;
            }
        }
    }
#else
    {
        uint64_t acc_lo = 0;
        uint32_t acc_hi = 0;
        int di;
        for (di = 0; di < n; di++) {
            uint64_t v_lo;
            uint32_t v_hi;
            uint64_t sum;
            sn_ntt_crt3_96(res[0][di], res[1][di], res[2][di], &v_lo, &v_hi);
            sum = acc_lo + v_lo;
            acc_hi = (uint32_t)(acc_hi + v_hi + (sum < acc_lo ? 1u : 0u));
            acc_lo = sum;
            {
                uint32_t dig = (uint32_t)(acc_lo & SN_NTT_DIGIT_MASK);
                int limb_i = di >> 1;
                if (limb_i < nout) {
                    if (di & 1)
                        d[limb_i] |= (sn_limb)(dig << SN_NTT_DIGIT_BITS);
                    else
                        d[limb_i] |= (sn_limb)dig;
                }
                /* acc >>= 16 */
                acc_lo = (acc_lo >> 16) | ((uint64_t)(acc_hi & 0xFFFFu) << 48);
                acc_hi >>= 16;
            }
        }
        {
            int di = n;
            while (acc_lo || acc_hi) {
                uint32_t dig = (uint32_t)(acc_lo & SN_NTT_DIGIT_MASK);
                int limb_i = di >> 1;
                if (limb_i >= nout) {
                    st = sn_value_reserve(ctx, r, limb_i + 1);
                    if (st != SN_OK) goto cleanup;
                    d = SN_LIMBS(r);
                    for (i = nout; i <= limb_i; i++) d[i] = 0;
                    nout = limb_i + 1;
                }
                if (di & 1)
                    d[limb_i] |= (sn_limb)(dig << SN_NTT_DIGIT_BITS);
                else
                    d[limb_i] |= (sn_limb)dig;
                acc_lo = (acc_lo >> 16) | ((uint64_t)(acc_hi & 0xFFFFu) << 48);
                acc_hi >>= 16;
                di++;
            }
        }
    }
#endif

    r->nlimbs = nout;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    st = SN_OK;

cleanup:
    if (fa) sn_free(ctx, fa, bytes);
    if (fb) sn_free(ctx, fb, bytes);
    for (k = 0; k < 3; k++) {
        if (res[k]) sn_free(ctx, res[k], bytes);
    }
    return st;
}

/*
 * Karatsuba: split at m = ceil(max(na,nb)/2).
 * Threshold chosen so basecase wins for small n (typical soft-float limbs).
 * NTT (above) handles very large operands; Karatsuba covers mid sizes.
 */
/* Basecase wins below ~16 limbs; mid sizes use recursive Karatsuba. */
#define SN_KARATSUBA_THRESHOLD 20

static sn_status sn_limb_mul_karatsuba(sn_ctx *ctx, sn_value *r,
                                       const sn_limb *a, int na,
                                       const sn_limb *b, int nb);

static sn_status sn_limb_mul_dispatch(sn_ctx *ctx, sn_value *r,
                                      const sn_limb *a, int na,
                                      const sn_limb *b, int nb)
{
    int n;
    sn_status st;
    sn_limb *d;
    int nmin = na < nb ? na : nb;
    int nmax = na > nb ? na : nb;

    /* Large both-sides: multi-modulus NTT; OOM falls through to Karatsuba. */
    if (nmin >= SN_NTT_THRESHOLD && nmax >= SN_NTT_THRESHOLD) {
        st = sn_limb_mul_ntt(ctx, r, a, na, b, nb);
        if (st == SN_OK) return SN_OK;
        if (st != SN_ERR_NOMEM && st != SN_ERR_RANGE)
            return st;
    }

    if (nmin < SN_KARATSUBA_THRESHOLD || nmax < SN_KARATSUBA_THRESHOLD) {
        n = na + nb;
        st = sn_value_reserve(ctx, r, n);
        if (st != SN_OK) return st;
        d = SN_LIMBS(r);
        sn_limb_mul_basecase(d, a, na, b, nb);
        r->nlimbs = n;
        r->kind = SN_KIND_BIGINT;
        r->negative = 0;
        sn_bigint_normalize(r);
        return SN_OK;
    }
    return sn_limb_mul_karatsuba(ctx, r, a, na, b, nb);
}

static sn_status sn_limb_mul_karatsuba(sn_ctx *ctx, sn_value *r,
                                       const sn_limb *a, int na,
                                       const sn_limb *b, int nb)
{
    sn_status st;
    int m, na0, na1, nb0, nb1, n, ns0, ns1, nz;
    const sn_limb *a0, *a1, *b0, *b1;
    sn_value z0, z1, z2, s1, s2;
    sn_limb *d;
    int ntmp;

    m = (na > nb ? na : nb);
    m = (m + 1) / 2;
    if (m < 1) m = 1;

    a0 = a;
    na0 = na < m ? na : m;
    a1 = a + na0;
    na1 = na - na0;
    b0 = b;
    nb0 = nb < m ? nb : m;
    b1 = b + nb0;
    nb1 = nb - nb0;
    if (na0 < 1) { na0 = 1; /* a is zero-padded conceptually */ }
    /* If high part empty, fall back (should be rare after normalize). */
    if (na1 <= 0) na1 = 0;
    if (nb1 <= 0) nb1 = 0;
    if (na1 == 0 || nb1 == 0) {
        /* one operand shorter than split: basecase is fine */
        n = na + nb;
        st = sn_value_reserve(ctx, r, n);
        if (st != SN_OK) return st;
        sn_limb_mul_basecase(SN_LIMBS(r), a, na, b, nb);
        r->nlimbs = n;
        r->kind = SN_KIND_BIGINT;
        r->negative = 0;
        sn_bigint_normalize(r);
        return SN_OK;
    }

    sn_value_init(&z0);
    sn_value_init(&z1);
    sn_value_init(&z2);
    sn_value_init(&s1);
    sn_value_init(&s2);

    /* z0 = a0*b0, z2 = a1*b1 */
    st = sn_limb_mul_dispatch(ctx, &z0, a0, na0, b0, nb0); if (st != SN_OK) goto done;
    st = sn_limb_mul_dispatch(ctx, &z2, a1, na1, b1, nb1); if (st != SN_OK) goto done;

    /* s1 = a0+a1, s2 = b0+b1 */
    ntmp = (na0 > na1 ? na0 : na1) + 1;
    st = sn_value_reserve(ctx, &s1, ntmp); if (st != SN_OK) goto done;
    ns0 = sn_limb_add_raw(SN_LIMBS(&s1), a0, na0, a1, na1);
    s1.nlimbs = ns0;
    s1.kind = SN_KIND_BIGINT;
    s1.negative = 0;

    ntmp = (nb0 > nb1 ? nb0 : nb1) + 1;
    st = sn_value_reserve(ctx, &s2, ntmp); if (st != SN_OK) goto done;
    ns1 = sn_limb_add_raw(SN_LIMBS(&s2), b0, nb0, b1, nb1);
    s2.nlimbs = ns1;
    s2.kind = SN_KIND_BIGINT;
    s2.negative = 0;

    /* z1 = (a0+a1)*(b0+b1) */
    st = sn_limb_mul_dispatch(ctx, &z1, SN_CLIMBS(&s1), s1.nlimbs, SN_CLIMBS(&s2), s2.nlimbs);
    if (st != SN_OK) goto done;

    /* z1 = z1 - z0 - z2 */
    {
        sn_value t;
        sn_value_init(&t);
        st = sn_limb_sub(ctx, &t, SN_CLIMBS(&z1), z1.nlimbs, SN_CLIMBS(&z0), z0.nlimbs);
        if (st != SN_OK) { sn_value_clear(ctx, &t); goto done; }
        sn_value_clear(ctx, &z1);
        sn_value_move(&z1, &t);
        sn_value_init(&t);
        st = sn_limb_sub(ctx, &t, SN_CLIMBS(&z1), z1.nlimbs, SN_CLIMBS(&z2), z2.nlimbs);
        if (st != SN_OK) { sn_value_clear(ctx, &t); goto done; }
        sn_value_clear(ctx, &z1);
        sn_value_move(&z1, &t);
    }

    /* r = z0 + (z1 << m) + (z2 << 2m) */
    n = na + nb;
    if (n < z0.nlimbs) n = z0.nlimbs;
    if (n < z1.nlimbs + m) n = z1.nlimbs + m;
    if (n < z2.nlimbs + 2 * m) n = z2.nlimbs + 2 * m;
    n += 1; /* carry slack */
    st = sn_value_reserve(ctx, r, n);
    if (st != SN_OK) goto done;
    d = SN_LIMBS(r);
    for (nz = 0; nz < n; nz++) d[nz] = 0;
    sn_limb_add_shifted(d, n, SN_CLIMBS(&z0), z0.nlimbs, 0);
    sn_limb_add_shifted(d, n, SN_CLIMBS(&z1), z1.nlimbs, m);
    sn_limb_add_shifted(d, n, SN_CLIMBS(&z2), z2.nlimbs, 2 * m);
    r->nlimbs = n;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    st = SN_OK;
done:
    sn_value_clear(ctx, &z0);
    sn_value_clear(ctx, &z1);
    sn_value_clear(ctx, &z2);
    sn_value_clear(ctx, &s1);
    sn_value_clear(ctx, &s2);
    return st;
}

sn_status sn_limb_mul(sn_ctx *ctx, sn_value *r, const sn_limb *a, int na, const sn_limb *b, int nb)
{
    int n, i;
    sn_status st;
    sn_limb *d;

    while (na > 1 && a[na - 1] == 0) na--;
    while (nb > 1 && b[nb - 1] == 0) nb--;
    if (na < 1) na = 1;
    if (nb < 1) nb = 1;

    /* Fast path: * 0 */
    if ((na == 1 && a[0] == 0) || (nb == 1 && b[0] == 0)) {
        st = sn_value_reserve(ctx, r, 1);
        if (st != SN_OK) return st;
        SN_LIMBS(r)[0] = 0;
        r->nlimbs = 1;
        r->kind = SN_KIND_BIGINT;
        r->negative = 0;
        return SN_OK;
    }

    /* Fast path: single-limb * multi-limb */
    if (na == 1 || nb == 1) {
        const sn_limb *big = (na == 1) ? b : a;
        int nbig = (na == 1) ? nb : na;
        sn_limb s = (na == 1) ? a[0] : b[0];
        uint64_t carry = 0;
        n = nbig + 1;
        st = sn_value_reserve(ctx, r, n);
        if (st != SN_OK) return st;
        d = SN_LIMBS(r);
        for (i = 0; i < nbig; i++) {
            uint64_t prod = (uint64_t)big[i] * (uint64_t)s + carry;
            d[i] = (sn_limb)(prod & SN_LIMB_MASK);
            carry = prod >> SN_LIMB_BITS;
        }
        d[nbig] = (sn_limb)carry;
        r->nlimbs = n;
        r->kind = SN_KIND_BIGINT;
        r->negative = 0;
        sn_bigint_normalize(r);
        return SN_OK;
    }

    return sn_limb_mul_dispatch(ctx, r, a, na, b, nb);
}

/* Count leading zero bits in a limb (0 -> 32). */
static int sn_limb_clz(sn_limb x)
{
    int n = 0;
    if (x == 0) return SN_LIMB_BITS;
    if (x <= 0x0000FFFFu) { n += 16; x <<= 16; }
    if (x <= 0x00FFFFFFu) { n += 8;  x <<= 8; }
    if (x <= 0x0FFFFFFFu) { n += 4;  x <<= 4; }
    if (x <= 0x3FFFFFFFu) { n += 2;  x <<= 2; }
    if (x <= 0x7FFFFFFFu) { n += 1; }
    return n;
}

/* d[0..ns] = s << k, 0 < k < SN_LIMB_BITS */
static void sn_limb_shl_small(sn_limb *d, const sn_limb *s, int ns, int k)
{
    int i;
    sn_limb carry = 0;
    for (i = 0; i < ns; i++) {
        sn_limb x = s[i];
        d[i] = (sn_limb)((x << k) | carry);
        carry = (sn_limb)(x >> (SN_LIMB_BITS - k));
    }
    d[ns] = carry;
}

/* in-place s >>= k for ns limbs, 0 < k < SN_LIMB_BITS */
static void sn_limb_shr_small_ip(sn_limb *s, int ns, int k)
{
    int i;
    for (i = 0; i < ns - 1; i++)
        s[i] = (sn_limb)((s[i] >> k) | ((uint64_t)s[i + 1] << (SN_LIMB_BITS - k)));
    if (ns > 0) s[ns - 1] >>= k;
}

sn_status sn_limb_divmod(sn_ctx *ctx, sn_value *q, sn_value *r,
                         const sn_limb *u, int nu, const sn_limb *v, int nv)
{
    sn_status st;
    sn_limb *ql, *rl;
    int i, j, m, n, shift;
    sn_value un, vn;
    sn_limb *U, *V;

    while (nv > 1 && v[nv - 1] == 0) nv--;
    while (nu > 1 && u[nu - 1] == 0) nu--;

    if (nv < 1 || (nv == 1 && v[0] == 0))
        return SN_ERR_DIVZERO;
    if (nu < 1) nu = 1;

    if (sn_limb_cmp(u, nu, v, nv) < 0) {
        st = sn_value_reserve(ctx, q, 1);
        if (st != SN_OK) return st;
        SN_LIMBS(q)[0] = 0;
        q->nlimbs = 1;
        q->kind = SN_KIND_BIGINT;
        q->negative = 0;
        st = sn_value_reserve(ctx, r, nu);
        if (st != SN_OK) return st;
        rl = SN_LIMBS(r);
        for (i = 0; i < nu; i++) rl[i] = u[i];
        r->nlimbs = nu;
        r->kind = SN_KIND_BIGINT;
        r->negative = 0;
        sn_bigint_normalize(r);
        return SN_OK;
    }

    if (nv == 1) {
        uint32_t div = v[0];
        uint64_t acc = 0;
        st = sn_value_reserve(ctx, q, nu);
        if (st != SN_OK) return st;
        ql = SN_LIMBS(q);
        for (i = nu - 1; i >= 0; i--) {
            acc = (acc << SN_LIMB_BITS) | (uint64_t)u[i];
            ql[i] = (sn_limb)(acc / div);
            acc = acc % div;
        }
        q->nlimbs = nu;
        q->kind = SN_KIND_BIGINT;
        q->negative = 0;
        sn_bigint_normalize(q);
        st = sn_value_reserve(ctx, r, 1);
        if (st != SN_OK) return st;
        SN_LIMBS(r)[0] = (sn_limb)acc;
        r->nlimbs = 1;
        r->kind = SN_KIND_BIGINT;
        r->negative = 0;
        return SN_OK;
    }

    /* Knuth Algorithm D */
    n = nv;
    m = nu - n;
    shift = sn_limb_clz(v[n - 1]);

    sn_value_init(&un);
    sn_value_init(&vn);
    st = sn_value_reserve(ctx, &un, nu + 1);
    if (st != SN_OK) return st;
    st = sn_value_reserve(ctx, &vn, n + 1);
    if (st != SN_OK) { sn_value_clear(ctx, &un); return st; }
    U = SN_LIMBS(&un);
    V = SN_LIMBS(&vn);
    for (i = 0; i < nu + 1; i++) U[i] = 0;
    for (i = 0; i < n + 1; i++) V[i] = 0;

    if (shift == 0) {
        for (i = 0; i < nu; i++) U[i] = u[i];
        for (i = 0; i < n; i++) V[i] = v[i];
    } else {
        sn_limb_shl_small(U, u, nu, shift);
        sn_limb_shl_small(V, v, n, shift);
    }

    st = sn_value_reserve(ctx, q, m + 1);
    if (st != SN_OK) {
        sn_value_clear(ctx, &un);
        sn_value_clear(ctx, &vn);
        return st;
    }
    ql = SN_LIMBS(q);
    for (i = 0; i <= m; i++) ql[i] = 0;
    q->nlimbs = m + 1;
    q->kind = SN_KIND_BIGINT;
    q->negative = 0;

    for (j = m; j >= 0; j--) {
        /* D3: estimate qhat = U[j+n..j+n-1] / V[n-1] */
        uint64_t vn1 = (uint64_t)V[n - 1];
        uint64_t uj_n = (uint64_t)U[j + n];
        uint64_t uj_n1 = (uint64_t)U[j + n - 1];
        uint64_t numerator = (uj_n << SN_LIMB_BITS) | uj_n1;
        uint64_t qhat, rhat;

        if (uj_n >= vn1) {
            /* qhat would be >= B; clamp. rhat may be >= B ? do NOT use rhat<<32. */
            qhat = SN_LIMB_MASK;
            rhat = numerator - qhat * vn1;
        } else {
            qhat = numerator / vn1;
            rhat = numerator % vn1;
        }

        /* At most two corrections using next digits (Hacker's Delight / Knuth D3).
         * When rhat >= B after clamp, the test is vacuously false (qhat cannot be
         * too large in the next-digit sense until rhat fits in a limb). */
        if (n >= 2) {
            uint64_t vn2 = (uint64_t)V[n - 2];
            uint64_t uj_n2 = (uint64_t)U[j + n - 2];
            while (rhat <= SN_LIMB_MASK &&
                   qhat * vn2 > (rhat << SN_LIMB_BITS) + uj_n2) {
                qhat--;
                rhat += vn1;
                if (rhat > SN_LIMB_MASK)
                    break;
            }
        }

        /* D4: U[j..j+n] -= qhat * V[0..n-1]  (unsigned borrow form) */
        {
            uint64_t carry = 0;
            uint64_t borrow = 0;
            for (i = 0; i < n; i++) {
                uint64_t prod = qhat * (uint64_t)V[i] + carry;
                uint64_t pl = prod & SN_LIMB_MASK;
                uint64_t ui, t;
                carry = prod >> SN_LIMB_BITS;
                ui = (uint64_t)U[j + i];
                t = pl + borrow;
                if (ui < t) {
                    U[j + i] = (sn_limb)(ui + ((uint64_t)1 << SN_LIMB_BITS) - t);
                    borrow = 1;
                } else {
                    U[j + i] = (sn_limb)(ui - t);
                    borrow = 0;
                }
            }
            {
                uint64_t ui = (uint64_t)U[j + n];
                uint64_t t = carry + borrow;
                if (ui < t) {
                    /* D6: qhat one too large ? add V back */
                    uint64_t c = 0;
                    for (i = 0; i < n; i++) {
                        uint64_t s = (uint64_t)U[j + i] + (uint64_t)V[i] + c;
                        U[j + i] = (sn_limb)(s & SN_LIMB_MASK);
                        c = s >> SN_LIMB_BITS;
                    }
                    /* High digit after failed sub then add-back:
                     * remainder high = ui - t + B + c, but B cancels the borrow
                     * into the next (unused) place; result is ui - t + c with
                     * modular wrap in limb width... use signed recovery: */
                    U[j + n] = (sn_limb)((ui - t) + c);
                    qhat--;
                } else {
                    U[j + n] = (sn_limb)(ui - t);
                }
            }
        }
        ql[j] = (sn_limb)qhat;
    }


    st = sn_value_reserve(ctx, r, n);
    if (st != SN_OK) {
        sn_value_clear(ctx, &un);
        sn_value_clear(ctx, &vn);
        return st;
    }
    rl = SN_LIMBS(r);
    for (i = 0; i < n; i++) rl[i] = U[i];
    if (shift) sn_limb_shr_small_ip(rl, n, shift);
    r->nlimbs = n;
    r->kind = SN_KIND_BIGINT;
    r->negative = 0;
    sn_bigint_normalize(r);
    sn_bigint_normalize(q);

    sn_value_clear(ctx, &un);
    sn_value_clear(ctx, &vn);
    return SN_OK;
}
