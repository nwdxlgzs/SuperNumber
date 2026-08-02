/*
 * sn_web_bridge.c — minimal Emscripten bridge for SuperNumber web playground.
 * Exports a small stable surface; all math goes through sn_api tables.
 * No library-global allocator state.
 */
#include "sn.h"
#include "sn_flat.h"

#include <emscripten.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

typedef struct snw_slot {
    sn_value v;
    int used;
} snw_slot;

typedef struct snw_cslot {
    sn_cplx z;
    int used;
} snw_cslot;

typedef struct snw_tslot {
    sn_tensor t;
    int used;
} snw_tslot;

typedef struct snw_session {
    sn_ctx ctx;
    sn_api api;
    snw_slot *slots;
    int nslots;
    snw_cslot *cslots;
    int ncslots;
    snw_tslot *tslots;
    int ntslots;
    int e_bits;
    int m_bits;
    int nan_enabled;
    char err[256];
} snw_session;

static void snw_set_err(snw_session *s, const char *msg)
{
    if (!s) return;
    if (!msg) { s->err[0] = 0; return; }
    snprintf(s->err, sizeof(s->err), "%s", msg);
}

static int snw_grow(snw_session *s)
{
    int n = s->nslots ? s->nslots * 2 : 16;
    snw_slot *p = (snw_slot *)realloc(s->slots, (size_t)n * sizeof(snw_slot));
    if (!p) return -1;
    memset(p + s->nslots, 0, (size_t)(n - s->nslots) * sizeof(snw_slot));
    s->slots = p;
    s->nslots = n;
    return 0;
}

static int snw_cgrow(snw_session *s)
{
    int n = s->ncslots ? s->ncslots * 2 : 16;
    snw_cslot *p = (snw_cslot *)realloc(s->cslots, (size_t)n * sizeof(snw_cslot));
    if (!p) return -1;
    memset(p + s->ncslots, 0, (size_t)(n - s->ncslots) * sizeof(snw_cslot));
    s->cslots = p;
    s->ncslots = n;
    return 0;
}

static int snw_tgrow(snw_session *s)
{
    int n = s->ntslots ? s->ntslots * 2 : 16;
    snw_tslot *p = (snw_tslot *)realloc(s->tslots, (size_t)n * sizeof(snw_tslot));
    if (!p) return -1;
    memset(p + s->ntslots, 0, (size_t)(n - s->ntslots) * sizeof(snw_tslot));
    s->tslots = p;
    s->ntslots = n;
    return 0;
}

static int snw_alloc_id(snw_session *s)
{
    int i;
    for (i = 0; i < s->nslots; i++) {
        if (!s->slots[i].used) {
            s->slots[i].used = 1;
            sn_value_init(&s->slots[i].v);
            return i;
        }
    }
    if (snw_grow(s) != 0) return -1;
    for (i = 0; i < s->nslots; i++) {
        if (!s->slots[i].used) {
            s->slots[i].used = 1;
            sn_value_init(&s->slots[i].v);
            return i;
        }
    }
    return -1;
}

static sn_value *snw_get(snw_session *s, int id)
{
    if (!s || id < 0 || id >= s->nslots || !s->slots[id].used) return NULL;
    return &s->slots[id].v;
}

static int snw_calloc_id(snw_session *s)
{
    int i;
    for (i = 0; i < s->ncslots; i++) {
        if (!s->cslots[i].used) {
            s->cslots[i].used = 1;
            s->api.cplx.init(&s->cslots[i].z);
            return i;
        }
    }
    if (snw_cgrow(s) != 0) return -1;
    for (i = 0; i < s->ncslots; i++) {
        if (!s->cslots[i].used) {
            s->cslots[i].used = 1;
            s->api.cplx.init(&s->cslots[i].z);
            return i;
        }
    }
    return -1;
}

static sn_cplx *snw_cget(snw_session *s, int id)
{
    if (!s || id < 0 || id >= s->ncslots || !s->cslots[id].used) return NULL;
    return &s->cslots[id].z;
}

static int snw_talloc_id(snw_session *s)
{
    int i;
    for (i = 0; i < s->ntslots; i++) {
        if (!s->tslots[i].used) {
            s->tslots[i].used = 1;
            s->api.tensor.init(&s->tslots[i].t);
            return i;
        }
    }
    if (snw_tgrow(s) != 0) return -1;
    for (i = 0; i < s->ntslots; i++) {
        if (!s->tslots[i].used) {
            s->tslots[i].used = 1;
            s->api.tensor.init(&s->tslots[i].t);
            return i;
        }
    }
    return -1;
}

static sn_tensor *snw_tget(snw_session *s, int id)
{
    if (!s || id < 0 || id >= s->ntslots || !s->tslots[id].used) return NULL;
    return &s->tslots[id].t;
}

/* Copy ctx-allocated string into plain malloc buffer for JS free() simplicity. */
static char *snw_dup_cstr(const char *src)
{
    size_t n;
    char *out;
    if (!src) return NULL;
    n = strlen(src) + 1;
    out = (char *)malloc(n);
    if (!out) return NULL;
    memcpy(out, src, n);
    return out;
}

EMSCRIPTEN_KEEPALIVE
snw_session *snw_create(int e_bits, int m_bits, int nan_enabled)
{
    snw_session *s = (snw_session *)calloc(1, sizeof(snw_session));
    if (!s) return NULL;
    sn_api_bind(&s->api);
    s->api.ctx.init(&s->ctx);
    s->e_bits = e_bits > 0 ? e_bits : 11;
    s->m_bits = m_bits > 0 ? m_bits : 52;
    s->nan_enabled = nan_enabled ? 1 : 0;
    if (snw_grow(s) != 0 || snw_cgrow(s) != 0 || snw_tgrow(s) != 0) {
        free(s->slots);
        free(s->cslots);
        free(s->tslots);
        s->api.ctx.fini(&s->ctx);
        free(s);
        return NULL;
    }
    return s;
}

EMSCRIPTEN_KEEPALIVE
void snw_destroy(snw_session *s)
{
    int i;
    if (!s) return;
    for (i = 0; i < s->nslots; i++) {
        if (s->slots[i].used) {
            s->api.value.clear(&s->ctx, &s->slots[i].v);
            s->slots[i].used = 0;
        }
    }
    for (i = 0; i < s->ncslots; i++) {
        if (s->cslots[i].used) {
            s->api.cplx.clear(&s->ctx, &s->cslots[i].z);
            s->cslots[i].used = 0;
        }
    }
    for (i = 0; i < s->ntslots; i++) {
        if (s->tslots[i].used) {
            s->api.tensor.clear(&s->ctx, &s->tslots[i].t);
            s->tslots[i].used = 0;
        }
    }
    free(s->slots);
    free(s->cslots);
    free(s->tslots);
    s->api.ctx.fini(&s->ctx);
    free(s);
}

EMSCRIPTEN_KEEPALIVE
const char *snw_last_error(snw_session *s)
{
    return s ? s->err : "null session";
}

EMSCRIPTEN_KEEPALIVE
void snw_clear_error(snw_session *s)
{
    snw_set_err(s, NULL);
}

EMSCRIPTEN_KEEPALIVE
int snw_set_format(snw_session *s, int e_bits, int m_bits, int nan_enabled)
{
    if (!s) return -1;
    s->e_bits = e_bits;
    s->m_bits = m_bits;
    s->nan_enabled = nan_enabled ? 1 : 0;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_new_value(snw_session *s)
{
    if (!s) return -1;
    return snw_alloc_id(s);
}

EMSCRIPTEN_KEEPALIVE
void snw_free_value(snw_session *s, int id)
{
    sn_value *v;
    if (!s) return;
    v = snw_get(s, id);
    if (!v) return;
    s->api.value.clear(&s->ctx, v);
    s->slots[id].used = 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_set_f64(snw_session *s, int id, double x)
{
    sn_value *v = snw_get(s, id);
    sn_status st;
    if (!v) { snw_set_err(s, "bad value id"); return -1; }
    s->api.value.clear(&s->ctx, v);
    sn_value_init(v);
    if (s->e_bits == 11 && s->m_bits == 52) {
        st = s->api.flt.f64(&s->ctx, v, x);
    } else if (s->e_bits == 8 && s->m_bits == 23) {
        st = s->api.flt.f32(&s->ctx, v, x);
    } else if (s->e_bits == 5 && s->m_bits == 10) {
        st = s->api.flt.f16(&s->ctx, v, x);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", x);
        st = s->api.flt.from_str(&s->ctx, v, buf, s->e_bits, s->m_bits, s->nan_enabled, NULL);
    }
    if (st != SN_OK) { snw_set_err(s, "set_f64 failed"); return (int)st; }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_set_from_str(snw_session *s, int id, const char *str, int as_int)
{
    sn_value *v = snw_get(s, id);
    sn_status st;
    if (!v || !str) { snw_set_err(s, "bad args"); return -1; }
    s->api.value.clear(&s->ctx, v);
    sn_value_init(v);
    if (as_int) {
        st = s->api.integer.from_str_bigint(&s->ctx, v, str, 0);
    } else {
        st = s->api.flt.from_str(&s->ctx, v, str, s->e_bits, s->m_bits, s->nan_enabled, NULL);
    }
    if (st != SN_OK) {
        snw_set_err(s, "parse failed");
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_set_i64(snw_session *s, int id, int32_t hi, uint32_t lo)
{
    sn_value *v = snw_get(s, id);
    int64_t x;
    sn_status st;
    if (!v) { snw_set_err(s, "bad value id"); return -1; }
    x = ((int64_t)hi << 32) | (int64_t)(uint64_t)lo;
    s->api.value.clear(&s->ctx, v);
    sn_value_init(v);
    st = s->api.integer.i64(&s->ctx, v, x);
    if (st != SN_OK) { snw_set_err(s, "set_i64 failed"); return (int)st; }
    return 0;
}

/* Return pointer to malloc'd UTF-8 string; caller must snw_free_str. */
EMSCRIPTEN_KEEPALIVE
char *snw_to_str(snw_session *s, int id, int base)
{
    sn_value *v = snw_get(s, id);
    char *tmp = NULL;
    char *out = NULL;
    sn_status st;
    if (!v) { snw_set_err(s, "bad value id"); return NULL; }
    st = sn_to_str(&s->ctx, &tmp, v, base <= 0 ? 10 : base);
    if (st != SN_OK || !tmp) {
        if (v->kind == SN_KIND_FLOAT) {
            double d;
            char buf[128];
            if (s->api.flt.to_double(&s->ctx, v, &d) == SN_OK) {
                snprintf(buf, sizeof(buf), "%.17g", d);
                return snw_dup_cstr(buf);
            }
            snprintf(buf, sizeof(buf), "<float e=%d m=%d>", v->e_bits, v->m_bits);
            return snw_dup_cstr(buf);
        }
        snw_set_err(s, "to_str failed");
        return NULL;
    }
    out = snw_dup_cstr(tmp);
    sn_str_free(&s->ctx, tmp);
    if (!out) snw_set_err(s, "oom");
    return out;
}

EMSCRIPTEN_KEEPALIVE
void snw_free_str(char *p)
{
    free(p);
}

EMSCRIPTEN_KEEPALIVE
double snw_to_f64(snw_session *s, int id)
{
    sn_value *v = snw_get(s, id);
    double d = NAN;
    if (!v) return d;
    if (v->kind == SN_KIND_FLOAT) {
        s->api.flt.to_double(&s->ctx, v, &d);
        return d;
    }
    {
        int64_t x = 0;
        if (s->api.integer.to_i64(&s->ctx, v, &x) == SN_OK) return (double)x;
    }
    return d;
}

EMSCRIPTEN_KEEPALIVE
int snw_kind(snw_session *s, int id)
{
    sn_value *v = snw_get(s, id);
    return v ? (int)v->kind : -1;
}

EMSCRIPTEN_KEEPALIVE
unsigned snw_flags(snw_session *s)
{
    return s ? s->api.ctx.get_flags(&s->ctx) : 0;
}

EMSCRIPTEN_KEEPALIVE
void snw_clear_flags(snw_session *s)
{
    if (s) s->api.ctx.clear_flags(&s->ctx);
}

typedef sn_status (*snw_un_fn)(sn_ctx *, sn_value *, const sn_value *, const sn_op_opt *);
typedef sn_status (*snw_bin_fn)(sn_ctx *, sn_value *, const sn_value *, const sn_value *, const sn_op_opt *);

static snw_un_fn snw_lookup_un(snw_session *s, const char *op)
{
    sn_api *a = &s->api;
    if (!op) return NULL;
    if (!strcmp(op, "neg")) return a->arith.neg;
    if (!strcmp(op, "not")) return a->arith.not_;
    if (!strcmp(op, "abs") || !strcmp(op, "fabs")) return a->arith.abs;
    if (!strcmp(op, "sin")) return a->math.sin;
    if (!strcmp(op, "cos")) return a->math.cos;
    if (!strcmp(op, "tan")) return a->math.tan;
    if (!strcmp(op, "asin")) return a->math.asin;
    if (!strcmp(op, "acos")) return a->math.acos;
    if (!strcmp(op, "atan")) return a->math.atan;
    if (!strcmp(op, "sinh")) return a->math.sinh;
    if (!strcmp(op, "cosh")) return a->math.cosh;
    if (!strcmp(op, "tanh")) return a->math.tanh;
    if (!strcmp(op, "asinh")) return a->math.asinh;
    if (!strcmp(op, "acosh")) return a->math.acosh;
    if (!strcmp(op, "atanh")) return a->math.atanh;
    if (!strcmp(op, "exp")) return a->math.exp;
    if (!strcmp(op, "exp2")) return a->math.exp2;
    if (!strcmp(op, "expm1")) return a->math.expm1;
    if (!strcmp(op, "log")) return a->math.log;
    if (!strcmp(op, "log2")) return a->math.log2;
    if (!strcmp(op, "log10")) return a->math.log10;
    if (!strcmp(op, "log1p")) return a->math.log1p;
    if (!strcmp(op, "sqrt")) return a->math.sqrt;
    if (!strcmp(op, "cbrt")) return a->math.cbrt;
    if (!strcmp(op, "ceil")) return a->math.ceil;
    if (!strcmp(op, "floor")) return a->math.floor;
    if (!strcmp(op, "trunc")) return a->math.trunc;
    if (!strcmp(op, "rint")) return a->math.rint;
    if (!strcmp(op, "nearbyint")) return a->math.nearbyint;
    if (!strcmp(op, "erf")) return a->math.erf;
    if (!strcmp(op, "erfc")) return a->math.erfc;
    if (!strcmp(op, "tgamma")) return a->math.tgamma;
    if (!strcmp(op, "lgamma")) return a->math.lgamma;
    if (!strcmp(op, "digamma")) return a->math.digamma;
    if (!strcmp(op, "trigamma")) return a->math.trigamma;
    if (!strcmp(op, "j0")) return a->math.j0;
    if (!strcmp(op, "j1")) return a->math.j1;
    if (!strcmp(op, "y0")) return a->math.y0;
    if (!strcmp(op, "y1")) return a->math.y1;
    if (!strcmp(op, "i0")) return a->math.i0;
    if (!strcmp(op, "i1")) return a->math.i1;
    if (!strcmp(op, "k0")) return a->math.k0;
    if (!strcmp(op, "k1")) return a->math.k1;
    if (!strcmp(op, "ellipk")) return a->math.ellipk;
    if (!strcmp(op, "ellipe")) return a->math.ellipe;
    return NULL;
}

static snw_bin_fn snw_lookup_bin(snw_session *s, const char *op)
{
    sn_api *a = &s->api;
    if (!op) return NULL;
    if (!strcmp(op, "add")) return a->arith.add;
    if (!strcmp(op, "sub")) return a->arith.sub;
    if (!strcmp(op, "mul")) return a->arith.mul;
    if (!strcmp(op, "div")) return a->arith.div;
    if (!strcmp(op, "rem") || !strcmp(op, "fmod")) return a->arith.rem;
    if (!strcmp(op, "pow")) return a->math.pow;
    if (!strcmp(op, "hypot")) return a->math.hypot;
    if (!strcmp(op, "atan2")) return a->math.atan2;
    if (!strcmp(op, "fmin") || !strcmp(op, "min")) return a->math.fmin;
    if (!strcmp(op, "fmax") || !strcmp(op, "max")) return a->math.fmax;
    if (!strcmp(op, "fdim")) return a->math.fdim;
    if (!strcmp(op, "ellipf")) return a->math.ellipf;
    if (!strcmp(op, "ellipeinc")) return a->math.ellipeinc;
    if (!strcmp(op, "igamma")) return a->math.igamma;
    if (!strcmp(op, "igammac")) return a->math.igammac;
    if (!strcmp(op, "jacobi_sn")) return a->math.jacobi_sn;
    if (!strcmp(op, "jacobi_cn")) return a->math.jacobi_cn;
    if (!strcmp(op, "jacobi_dn")) return a->math.jacobi_dn;
    if (!strcmp(op, "and")) return a->arith.and_;
    if (!strcmp(op, "or")) return a->arith.or_;
    if (!strcmp(op, "xor")) return a->arith.xor_;
    if (!strcmp(op, "copysign")) return a->math.copysign;
    if (!strcmp(op, "nextafter")) return a->math.nextafter;
    return NULL;
}

EMSCRIPTEN_KEEPALIVE
int snw_unary(snw_session *s, int out_id, const char *op, int a_id)
{
    sn_value *out, *a;
    snw_un_fn fn;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    a = snw_get(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad value id"); return -1; }
    fn = snw_lookup_un(s, op);
    if (!fn) { snw_set_err(s, "unknown unary op"); return -2; }
    st = fn(&s->ctx, out, a, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "unary %s status=%d", op ? op : "?", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_binary(snw_session *s, int out_id, const char *op, int a_id, int b_id)
{
    sn_value *out, *a, *b;
    snw_bin_fn fn;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    a = snw_get(s, a_id);
    b = snw_get(s, b_id);
    if (!out || !a || !b) { snw_set_err(s, "bad value id"); return -1; }
    fn = snw_lookup_bin(s, op);
    if (!fn) { snw_set_err(s, "unknown binary op"); return -2; }
    st = fn(&s->ctx, out, a, b, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "binary %s status=%d", op ? op : "?", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_ternary(snw_session *s, int out_id, const char *op, int a_id, int b_id, int c_id)
{
    sn_value *out, *a, *b, *c;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    a = snw_get(s, a_id);
    b = snw_get(s, b_id);
    c = snw_get(s, c_id);
    if (!out || !a || !b || !c) { snw_set_err(s, "bad value id"); return -1; }
    if (!op) { snw_set_err(s, "null op"); return -2; }
    if (!strcmp(op, "fma")) {
        st = s->api.flt.fma(&s->ctx, out, a, b, c, NULL);
    } else if (!strcmp(op, "ibeta")) {
        st = s->api.math.ibeta(&s->ctx, out, a, b, c, NULL);
    } else if (!strcmp(op, "ibetac")) {
        st = s->api.math.ibetac(&s->ctx, out, a, b, c, NULL);
    } else if (!strcmp(op, "ellipiinc")) {
        st = s->api.math.ellipiinc(&s->ctx, out, a, b, c, NULL);
    } else {
        snw_set_err(s, "unknown ternary op");
        return -2;
    }
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "ternary %s status=%d", op, (int)st);
        return (int)st;
    }
    return 0;
}


EMSCRIPTEN_KEEPALIVE
int snw_new_cplx(snw_session *s)
{
    if (!s) return -1;
    return snw_calloc_id(s);
}

EMSCRIPTEN_KEEPALIVE
void snw_free_cplx(snw_session *s, int id)
{
    sn_cplx *z;
    if (!s) return;
    z = snw_cget(s, id);
    if (!z) return;
    s->api.cplx.clear(&s->ctx, z);
    s->cslots[id].used = 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_set_cplx_d(snw_session *s, int id, double re, double im)
{
    sn_cplx *z = snw_cget(s, id);
    sn_status st;
    if (!z) { snw_set_err(s, "bad cplx id"); return -1; }
    s->api.cplx.clear(&s->ctx, z);
    s->api.cplx.init(z);
    st = s->api.cplx.set_d(&s->ctx, z, re, im, s->e_bits, s->m_bits, s->nan_enabled, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "set_cplx_d status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_set_reim(snw_session *s, int id, int re_id, int im_id)
{
    sn_cplx *z = snw_cget(s, id);
    sn_value *re = snw_get(s, re_id);
    sn_value *im = snw_get(s, im_id);
    sn_status st;
    if (!z || !re || !im) { snw_set_err(s, "bad id"); return -1; }
    s->api.cplx.clear(&s->ctx, z);
    s->api.cplx.init(z);
    st = s->api.cplx.set(&s->ctx, z, re, im);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx_set_reim status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
char *snw_cplx_to_str(snw_session *s, int id)
{
    sn_cplx *z = snw_cget(s, id);
    char *re_s = NULL, *im_s = NULL;
    char buf[512];
    sn_status st;
    if (!z) { snw_set_err(s, "bad cplx id"); return NULL; }
    st = sn_to_str(&s->ctx, &re_s, &z->re, 10);
    if (st != SN_OK || !re_s) {
        double re = 0, im = 0;
        char rbuf[128], ibuf[128];
        s->api.flt.to_double(&s->ctx, &z->re, &re);
        s->api.flt.to_double(&s->ctx, &z->im, &im);
        snprintf(rbuf, sizeof(rbuf), "%.17g", re);
        snprintf(ibuf, sizeof(ibuf), "%.17g", im);
        if (ibuf[0] == '-' || ibuf[0] == '+')
            snprintf(buf, sizeof(buf), "%s%si", rbuf, ibuf);
        else
            snprintf(buf, sizeof(buf), "%s+%si", rbuf, ibuf);
        return snw_dup_cstr(buf);
    }
    st = sn_to_str(&s->ctx, &im_s, &z->im, 10);
    if (st != SN_OK || !im_s) {
        sn_str_free(&s->ctx, re_s);
        snw_set_err(s, "cplx im to_str failed");
        return NULL;
    }
    if (im_s[0] == '-' || im_s[0] == '+')
        snprintf(buf, sizeof(buf), "%s%si", re_s, im_s);
    else
        snprintf(buf, sizeof(buf), "%s+%si", re_s, im_s);
    sn_str_free(&s->ctx, re_s);
    sn_str_free(&s->ctx, im_s);
    return snw_dup_cstr(buf);
}

typedef sn_status (*snw_cun_fn)(sn_ctx *, sn_cplx *, const sn_cplx *, const sn_op_opt *);
typedef sn_status (*snw_cbin_fn)(sn_ctx *, sn_cplx *, const sn_cplx *, const sn_cplx *, const sn_op_opt *);

static snw_cun_fn snw_lookup_cun(snw_session *s, const char *op)
{
    sn_api_complex *c = &s->api.cplx;
    if (!op) return NULL;
    if (!strcmp(op, "neg")) return c->neg;
    if (!strcmp(op, "conj")) return c->conj;
    if (!strcmp(op, "proj")) return c->proj;
    if (!strcmp(op, "sqrt")) return c->sqrt;
    if (!strcmp(op, "exp")) return c->exp;
    if (!strcmp(op, "log")) return c->log;
    if (!strcmp(op, "sin")) return c->sin;
    if (!strcmp(op, "cos")) return c->cos;
    if (!strcmp(op, "tan")) return c->tan;
    if (!strcmp(op, "sinh")) return c->sinh;
    if (!strcmp(op, "cosh")) return c->cosh;
    if (!strcmp(op, "tanh")) return c->tanh;
    if (!strcmp(op, "asin")) return c->asin;
    if (!strcmp(op, "acos")) return c->acos;
    if (!strcmp(op, "atan")) return c->atan;
    if (!strcmp(op, "asinh")) return c->asinh;
    if (!strcmp(op, "acosh")) return c->acosh;
    if (!strcmp(op, "atanh")) return c->atanh;
    return NULL;
}

static snw_cbin_fn snw_lookup_cbin(snw_session *s, const char *op)
{
    sn_api_complex *c = &s->api.cplx;
    if (!op) return NULL;
    if (!strcmp(op, "add")) return c->add;
    if (!strcmp(op, "sub")) return c->sub;
    if (!strcmp(op, "mul")) return c->mul;
    if (!strcmp(op, "div")) return c->div;
    if (!strcmp(op, "pow")) return c->pow;
    return NULL;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_unary(snw_session *s, int out_id, const char *op, int a_id)
{
    sn_cplx *out, *a;
    snw_cun_fn fn;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_cget(s, out_id);
    a = snw_cget(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad cplx id"); return -1; }
    fn = snw_lookup_cun(s, op);
    if (!fn) { snw_set_err(s, "unknown cplx unary op"); return -2; }
    st = fn(&s->ctx, out, a, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx unary %s status=%d", op ? op : "?", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_binary(snw_session *s, int out_id, const char *op, int a_id, int b_id)
{
    sn_cplx *out, *a, *b;
    snw_cbin_fn fn;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_cget(s, out_id);
    a = snw_cget(s, a_id);
    b = snw_cget(s, b_id);
    if (!out || !a || !b) { snw_set_err(s, "bad cplx id"); return -1; }
    fn = snw_lookup_cbin(s, op);
    if (!fn) { snw_set_err(s, "unknown cplx binary op"); return -2; }
    st = fn(&s->ctx, out, a, b, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx binary %s status=%d", op ? op : "?", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_abs(snw_session *s, int out_id, int z_id)
{
    sn_value *out = snw_get(s, out_id);
    sn_cplx *z = snw_cget(s, z_id);
    sn_status st;
    if (!out || !z) { snw_set_err(s, "bad id"); return -1; }
    snw_clear_flags(s);
    st = s->api.cplx.abs(&s->ctx, out, z, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx abs status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_arg(snw_session *s, int out_id, int z_id)
{
    sn_value *out = snw_get(s, out_id);
    sn_cplx *z = snw_cget(s, z_id);
    sn_status st;
    if (!out || !z) { snw_set_err(s, "bad id"); return -1; }
    snw_clear_flags(s);
    st = s->api.cplx.arg(&s->ctx, out, z, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx arg status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_re(snw_session *s, int out_id, int z_id)
{
    sn_value *out = snw_get(s, out_id);
    sn_cplx *z = snw_cget(s, z_id);
    sn_status st;
    if (!out || !z) { snw_set_err(s, "bad id"); return -1; }
    st = s->api.value.copy(&s->ctx, out, &z->re);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx re status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_im(snw_session *s, int out_id, int z_id)
{
    sn_value *out = snw_get(s, out_id);
    sn_cplx *z = snw_cget(s, z_id);
    sn_status st;
    if (!out || !z) { snw_set_err(s, "bad id"); return -1; }
    st = s->api.value.copy(&s->ctx, out, &z->im);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx im status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cplx_from_polar(snw_session *s, int out_id, int rho_id, int theta_id)
{
    sn_cplx *out = snw_cget(s, out_id);
    sn_value *rho = snw_get(s, rho_id);
    sn_value *theta = snw_get(s, theta_id);
    sn_status st;
    if (!out || !rho || !theta) { snw_set_err(s, "bad id"); return -1; }
    snw_clear_flags(s);
    st = s->api.cplx.from_polar(&s->ctx, out, rho, theta, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cplx from_polar status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_set_int(snw_session *s, int id, const char *str, int base, int width, int is_signed)
{
    sn_value *v = snw_get(s, id);
    sn_status st;
    if (!v || !str) { snw_set_err(s, "bad args"); return -1; }
    s->api.value.clear(&s->ctx, v);
    sn_value_init(v);
    if (width <= 0) {
        st = s->api.integer.from_str_bigint(&s->ctx, v, str, base);
    } else {
        st = s->api.integer.from_str(&s->ctx, v, str, base, width, is_signed ? 1 : 0);
    }
    if (st != SN_OK) {
        snw_set_err(s, "set_int parse failed");
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_set_float(snw_session *s, int id, const char *str, int e_bits, int m_bits, int nan_enabled)
{
    sn_value *v = snw_get(s, id);
    sn_status st;
    int e = e_bits > 0 ? e_bits : s->e_bits;
    int m = m_bits > 0 ? m_bits : s->m_bits;
    int nan = nan_enabled < 0 ? s->nan_enabled : (nan_enabled ? 1 : 0);
    if (!v || !str) { snw_set_err(s, "bad args"); return -1; }
    s->api.value.clear(&s->ctx, v);
    sn_value_init(v);
    st = s->api.flt.from_str(&s->ctx, v, str, e, m, nan, NULL);
    if (st != SN_OK) {
        snw_set_err(s, "set_float parse failed");
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_set_i64_width(snw_session *s, int id, int32_t hi, uint32_t lo, int width, int is_signed)
{
    sn_value *v = snw_get(s, id);
    int64_t x;
    sn_status st;
    if (!v) { snw_set_err(s, "bad value id"); return -1; }
    x = ((int64_t)hi << 32) | (int64_t)(uint64_t)lo;
    s->api.value.clear(&s->ctx, v);
    sn_value_init(v);
    if (width <= 0) {
        st = s->api.integer.bigint_set_i64(&s->ctx, v, x);
    } else {
        st = s->api.integer.set_i64(&s->ctx, v, x, width, is_signed ? 1 : 0);
    }
    if (st != SN_OK) { snw_set_err(s, "set_i64_width failed"); return (int)st; }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_shift(snw_session *s, int out_id, const char *op, int a_id, int bits)
{
    sn_value *out, *a;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    a = snw_get(s, a_id);
    if (!out || !a || !op) { snw_set_err(s, "bad shift args"); return -1; }
    if (!strcmp(op, "shl")) st = s->api.arith.shl(&s->ctx, out, a, bits, NULL);
    else if (!strcmp(op, "shr")) st = s->api.arith.shr(&s->ctx, out, a, bits, NULL);
    else if (!strcmp(op, "sar")) st = s->api.arith.sar(&s->ctx, out, a, bits, NULL);
    else { snw_set_err(s, "unknown shift op"); return -2; }
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "shift %s status=%d", op, (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_crypto(snw_session *s, int out_id, const char *op, int a_id, int b_id, int c_id)
{
    sn_value *out, *a, *b, *c;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    a = snw_get(s, a_id);
    b = snw_get(s, b_id);
    if (!out || !a || !op) { snw_set_err(s, "bad crypto args"); return -1; }
    if (!strcmp(op, "gcd")) {
        if (!b) { snw_set_err(s, "gcd needs b"); return -1; }
        st = s->api.crypto.gcd(&s->ctx, out, a, b);
    } else if (!strcmp(op, "lcm")) {
        if (!b) { snw_set_err(s, "lcm needs b"); return -1; }
        st = s->api.crypto.lcm(&s->ctx, out, a, b);
    } else if (!strcmp(op, "isqrt") || !strcmp(op, "popcount") || !strcmp(op, "ctz")) {
        if (!strcmp(op, "isqrt"))
            st = s->api.crypto.isqrt(&s->ctx, out, a);
        else if (!strcmp(op, "popcount"))
            st = s->api.crypto.popcount(&s->ctx, out, a);
        else
            st = s->api.crypto.ctz(&s->ctx, out, a);
    } else if (!strcmp(op, "modinv")) {
        if (!b) { snw_set_err(s, "modinv needs m"); return -1; }
        st = s->api.crypto.modinv(&s->ctx, out, a, b);
    } else if (!strcmp(op, "mulmod")) {
        c = snw_get(s, c_id);
        if (!b || !c) { snw_set_err(s, "mulmod needs b,m"); return -1; }
        st = s->api.crypto.mulmod(&s->ctx, out, a, b, c);
    } else if (!strcmp(op, "powmod") || !strcmp(op, "powmod_ct")) {
        c = snw_get(s, c_id);
        if (!b || !c) { snw_set_err(s, "powmod needs exp,m"); return -1; }
        if (!strcmp(op, "powmod_ct"))
            st = s->api.crypto.powmod_ct(&s->ctx, out, a, b, c);
        else
            st = s->api.crypto.powmod(&s->ctx, out, a, b, c);
    } else {
        snw_set_err(s, "unknown crypto op");
        return -2;
    }
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "crypto %s status=%d", op, (int)st);
        return (int)st;
    }
    return 0;
}


EMSCRIPTEN_KEEPALIVE
int snw_copy(snw_session *s, int out_id, int src_id)
{
    sn_value *out, *src;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    src = snw_get(s, src_id);
    if (!out || !src) { snw_set_err(s, "bad copy args"); return -1; }
    st = s->api.value.copy(&s->ctx, out, src);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "copy status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cast(snw_session *s, int out_id, int src_id, const char *mode, int a, int b, int c)
{
    sn_value *out, *src;
    sn_status st = SN_ERR_INVALID;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    src = snw_get(s, src_id);
    if (!out || !src || !mode) { snw_set_err(s, "bad cast args"); return -1; }
    if (!strcmp(mode, "int")) {
        if (src->kind == SN_KIND_FLOAT) {
            st = s->api.flt.cast_int(&s->ctx, out, src, a, b ? 1 : 0, NULL);
        } else {
            char *tmp = NULL;
            st = sn_to_str(&s->ctx, &tmp, src, 10);
            if (st == SN_OK && tmp) {
                s->api.value.clear(&s->ctx, out);
                sn_value_init(out);
                if (a <= 0)
                    st = s->api.integer.from_str_bigint(&s->ctx, out, tmp, 10);
                else
                    st = s->api.integer.from_str(&s->ctx, out, tmp, 10, a, b ? 1 : 0);
                sn_str_free(&s->ctx, tmp);
            }
        }
    } else if (!strcmp(mode, "float")) {
        st = s->api.flt.cast_float(&s->ctx, out, src, a, b, c ? 1 : 0, NULL);
    } else {
        snw_set_err(s, "unknown cast mode");
        return -2;
    }
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "cast %s status=%d", mode, (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_cmp(snw_session *s, int a_id, int b_id)
{
    sn_value *a = snw_get(s, a_id);
    sn_value *b = snw_get(s, b_id);
    int rel = 0;
    if (!a || !b) return 0;
    s->api.arith.cmp(&s->ctx, &rel, a, b);
    return rel;
}

/* ---------- RNG (session-local; no library globals) ---------- */
EMSCRIPTEN_KEEPALIVE
int snw_seed_rng(snw_session *s, uint32_t seed_hi, uint32_t seed_lo)
{
    uint64_t seed;
    if (!s) return -1;
    seed = ((uint64_t)seed_hi << 32) | (uint64_t)seed_lo;
    s->api.ctx.seed_rng(&s->ctx, seed);
    return 0;
}

/* Fill out_id with a random u64 as BIGINT. Returns 0 ok. */
EMSCRIPTEN_KEEPALIVE
int snw_random_u64(snw_session *s, int out_id)
{
    sn_value *out;
    uint64_t r = 0;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    if (!out) { snw_set_err(s, "bad random_u64 out"); return -1; }
    st = s->api.crypto.random_u64(&s->ctx, &r);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "random_u64 status=%d", (int)st);
        return (int)st;
    }
    st = s->api.integer.bigint_set_u64(&s->ctx, out, r);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "random_u64 store status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

/* Uniform integer in [0, bound) as BIGINT in out_id. bound split as hi/lo uint32. */
EMSCRIPTEN_KEEPALIVE
int snw_random_u64_mod(snw_session *s, int out_id, uint32_t bound_hi, uint32_t bound_lo)
{
    sn_value *out;
    uint64_t bound;
    sn_status st;
    if (!s) return -1;
    snw_clear_flags(s);
    out = snw_get(s, out_id);
    if (!out) { snw_set_err(s, "bad random_u64_mod out"); return -1; }
    bound = ((uint64_t)bound_hi << 32) | (uint64_t)bound_lo;
    if (bound == 0) { snw_set_err(s, "random_u64_mod bound=0"); return -1; }
    st = s->api.crypto.random_u64_mod(&s->ctx, out, bound);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "random_u64_mod status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE

/* ---- Tensor slots (SN-backed 2D float matrices) ---- */

EMSCRIPTEN_KEEPALIVE
int snw_new_tensor(snw_session *s)
{
    if (!s) return -1;
    return snw_talloc_id(s);
}

EMSCRIPTEN_KEEPALIVE
void snw_free_tensor(snw_session *s, int id)
{
    sn_tensor *t;
    if (!s) return;
    t = snw_tget(s, id);
    if (!t) return;
    s->api.tensor.clear(&s->ctx, t);
    s->tslots[id].used = 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_from_str(snw_session *s, int id, const char *str)
{
    sn_tensor *t;
    sn_status st;
    if (!s || !str) return -1;
    t = snw_tget(s, id);
    if (!t) { snw_set_err(s, "bad tensor id"); return -1; }
    snw_clear_flags(s);
    st = s->api.tensor.from_str(&s->ctx, t, str, s->e_bits, s->m_bits, s->nan_enabled, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_from_str status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
char *snw_tensor_to_str(snw_session *s, int id)
{
    sn_tensor *t;
    char *out = NULL, *dup;
    sn_status st;
    if (!s) return NULL;
    t = snw_tget(s, id);
    if (!t) { snw_set_err(s, "bad tensor id"); return NULL; }
    st = s->api.tensor.to_str(&s->ctx, &out, t);
    if (st != SN_OK || !out) {
        snprintf(s->err, sizeof(s->err), "tensor_to_str status=%d", (int)st);
        return NULL;
    }
    dup = snw_dup_cstr(out);
    s->api.tensor.str_free(&s->ctx, out);
    return dup;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_dims(snw_session *s, int id, int *rows, int *cols)
{
    sn_tensor *t;
    int r = 0, c = 0;
    if (!s) return -1;
    t = snw_tget(s, id);
    if (!t) { snw_set_err(s, "bad tensor id"); return -1; }
    if (s->api.tensor.dims(t, &r, &c) != SN_OK) return -1;
    if (rows) *rows = r;
    if (cols) *cols = c;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_copy(snw_session *s, int out_id, int src_id)
{
    sn_tensor *out, *src;
    sn_status st;
    if (!s) return -1;
    out = snw_tget(s, out_id);
    src = snw_tget(s, src_id);
    if (!out || !src) { snw_set_err(s, "bad tensor id"); return -1; }
    snw_clear_flags(s);
    st = s->api.tensor.copy(&s->ctx, out, src);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_copy status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_unary(snw_session *s, int out_id, const char *op, int a_id)
{
    sn_tensor *out, *a;
    sn_status st;
    int uop = -1;
    if (!s || !op) return -1;
    out = snw_tget(s, out_id);
    a = snw_tget(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad tensor id"); return -1; }
    if (!strcmp(op, "neg")) uop = 0;
    else if (!strcmp(op, "exp")) uop = 1;
    else if (!strcmp(op, "tanh")) uop = 2;
    else if (!strcmp(op, "relu")) uop = 3;
    else if (!strcmp(op, "gelu")) uop = 4;
    else if (!strcmp(op, "silu") || !strcmp(op, "swish")) uop = 5;
    else if (!strcmp(op, "sqrt")) uop = 6;
    else if (!strcmp(op, "abs")) uop = 7;
    else if (!strcmp(op, "transpose")) {
        snw_clear_flags(s);
        st = s->api.tensor.transpose(&s->ctx, out, a);
        goto fin;
    }
    else if (!strcmp(op, "softmax_row") || !strcmp(op, "softmax")) {
        snw_clear_flags(s);
        st = s->api.tensor.softmax_row(&s->ctx, out, a, NULL);
        goto fin;
    }
    else { snw_set_err(s, "unknown tensor unary"); return -2; }
    snw_clear_flags(s);
    st = s->api.tensor.unary(&s->ctx, out, a, uop, NULL);
fin:
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_unary %s status=%d", op, (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_binary(snw_session *s, int out_id, const char *op, int a_id, int b_id)
{
    sn_tensor *out, *a, *b;
    sn_status st;
    if (!s || !op) return -1;
    out = snw_tget(s, out_id);
    a = snw_tget(s, a_id);
    b = snw_tget(s, b_id);
    if (!out || !a || !b) { snw_set_err(s, "bad tensor id"); return -1; }
    snw_clear_flags(s);
    if (!strcmp(op, "add") || !strcmp(op, "mat_add") || !strcmp(op, "residual_add"))
        st = s->api.tensor.add(&s->ctx, out, a, b, NULL);
    else if (!strcmp(op, "sub") || !strcmp(op, "mat_sub"))
        st = s->api.tensor.sub(&s->ctx, out, a, b, NULL);
    else if (!strcmp(op, "mul") || !strcmp(op, "hadamard") || !strcmp(op, "mat_hadamard"))
        st = s->api.tensor.hadamard(&s->ctx, out, a, b, NULL);
    else if (!strcmp(op, "div"))
        st = s->api.tensor.div(&s->ctx, out, a, b, NULL);
    else if (!strcmp(op, "matmul") || !strcmp(op, "mat_mul"))
        st = s->api.tensor.matmul(&s->ctx, out, a, b, NULL);
    else if (!strcmp(op, "concat0"))
        st = s->api.tensor.concat(&s->ctx, out, a, b, 0);
    else if (!strcmp(op, "concat1") || !strcmp(op, "concat"))
        st = s->api.tensor.concat(&s->ctx, out, a, b, 1);
    else { snw_set_err(s, "unknown tensor binary"); return -2; }
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_binary %s status=%d", op, (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_scale(snw_session *s, int out_id, int a_id, double scale)
{
    sn_tensor *out, *a;
    sn_value sc;
    sn_status st;
    char buf[64];
    if (!s) return -1;
    out = snw_tget(s, out_id);
    a = snw_tget(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad tensor id"); return -1; }
    sn_value_init(&sc);
    snw_clear_flags(s);
    snprintf(buf, sizeof(buf), "%.17g", scale);
    st = s->api.flt.from_str(&s->ctx, &sc, buf, s->e_bits, s->m_bits, s->nan_enabled, NULL);
    if (st == SN_OK) st = s->api.tensor.scale(&s->ctx, out, a, &sc, NULL);
    s->api.value.clear(&s->ctx, &sc);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_scale status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_rms_norm(snw_session *s, int out_id, int a_id, int gamma_id, double eps)
{
    sn_tensor *out, *a, *g = NULL;
    sn_status st;
    if (!s) return -1;
    out = snw_tget(s, out_id);
    a = snw_tget(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad tensor id"); return -1; }
    if (gamma_id >= 0) g = snw_tget(s, gamma_id);
    snw_clear_flags(s);
    st = s->api.tensor.rms_norm(&s->ctx, out, a, g, eps, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_rms_norm status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_rope(snw_session *s, int out_id, int a_id, double base)
{
    sn_tensor *out, *a;
    sn_status st;
    if (!s) return -1;
    out = snw_tget(s, out_id);
    a = snw_tget(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad tensor id"); return -1; }
    snw_clear_flags(s);
    st = s->api.tensor.rope(&s->ctx, out, a, base, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_rope status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_layer_norm(snw_session *s, int out_id, int a_id, int gamma_id, int beta_id, double eps)
{
    sn_tensor *out, *a, *g = NULL, *b = NULL;
    sn_status st;
    if (!s) return -1;
    out = snw_tget(s, out_id);
    a = snw_tget(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad tensor id"); return -1; }
    if (gamma_id >= 0) g = snw_tget(s, gamma_id);
    if (beta_id >= 0) b = snw_tget(s, beta_id);
    snw_clear_flags(s);
    st = s->api.tensor.layer_norm(&s->ctx, out, a, g, b, eps, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_layer_norm status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_sin_pe(snw_session *s, int out_id, int seq, int dim, double base)
{
    sn_tensor *out;
    sn_status st;
    if (!s) return -1;
    out = snw_tget(s, out_id);
    if (!out) { snw_set_err(s, "bad tensor id"); return -1; }
    snw_clear_flags(s);
    st = s->api.tensor.sin_pe(&s->ctx, out, seq, dim, base, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_sin_pe status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_reshape(snw_session *s, int out_id, int a_id, int rows, int cols)
{
    sn_tensor *out, *a;
    sn_status st;
    if (!s) return -1;
    out = snw_tget(s, out_id);
    a = snw_tget(s, a_id);
    if (!out || !a) { snw_set_err(s, "bad tensor id"); return -1; }
    snw_clear_flags(s);
    st = s->api.tensor.reshape(&s->ctx, out, a, rows, cols);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_reshape status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_attention_sdp(snw_session *s, int out_id, int w_id, int q_id, int k_id, int v_id,
                             int causal, double scale)
{
    sn_tensor *out, *w = NULL, *q, *k, *v;
    sn_status st;
    if (!s) return -1;
    out = snw_tget(s, out_id);
    q = snw_tget(s, q_id);
    k = snw_tget(s, k_id);
    v = snw_tget(s, v_id);
    if (!out || !q || !k || !v) { snw_set_err(s, "bad tensor id"); return -1; }
    if (w_id >= 0) w = snw_tget(s, w_id);
    snw_clear_flags(s);
    st = s->api.tensor.attention_sdp(&s->ctx, out, w, q, k, v, causal, scale, NULL);
    if (st != SN_OK) {
        snprintf(s->err, sizeof(s->err), "tensor_attention_sdp status=%d", (int)st);
        return (int)st;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int snw_tensor_get_f64(snw_session *s, int id, int r, int c, double *out)
{
    sn_tensor *t;
    sn_value v;
    sn_status st;
    double d = 0;
    if (!s || !out) return -1;
    t = snw_tget(s, id);
    if (!t) { snw_set_err(s, "bad tensor id"); return -1; }
    sn_value_init(&v);
    st = s->api.tensor.get(&s->ctx, &v, t, r, c);
    if (st == SN_OK) st = s->api.flt.to_double(&s->ctx, &v, &d);
    s->api.value.clear(&s->ctx, &v);
    if (st != SN_OK) return (int)st;
    *out = d;
    return 0;
}

const char *snw_version(void)
{
    return "SuperNumber web bridge 1.7";
}
