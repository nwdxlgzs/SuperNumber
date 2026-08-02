/*
 * sn_tensor.c — 2D row-major float tensors of sn_value elements.
 * All arithmetic uses SN float ops (same E/M/NaN as session format).
 * No __int128; no library globals.
 */
#include "internal/sn_impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

void sn_tensor_init(sn_tensor *t)
{
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

static void sn_tensor_free_data(sn_ctx *ctx, sn_tensor *t)
{
    int i, n;
    if (!t || !t->data) return;
    n = t->n;
    for (i = 0; i < n; i++) sn_value_clear(ctx, &t->data[i]);
    sn_free(ctx, t->data, (size_t)n * sizeof(sn_value));
    t->data = NULL;
    t->n = 0;
}

void sn_tensor_clear(sn_ctx *ctx, sn_tensor *t)
{
    if (!t) return;
    sn_tensor_free_data(ctx, t);
    memset(t, 0, sizeof(*t));
}

static int sn_tensor_ok(const sn_tensor *t)
{
    return t && t->rows >= 0 && t->cols >= 0 &&
           t->e_bits > 0 && t->m_bits > 0 &&
           t->n == t->rows * t->cols &&
           (t->n == 0 || t->data != NULL);
}

static sn_status sn_tensor_alloc(sn_ctx *ctx, sn_tensor *t, int rows, int cols,
                                 int e_bits, int m_bits, int nan_enabled)
{
    int n, i;
    sn_value *p;
    if (!ctx || !t) return SN_ERR_ARG;
    if (rows < 0 || cols < 0) return SN_ERR_RANGE;
    if (e_bits < 1 || m_bits < 1) return SN_ERR_RANGE;
    if (rows > 0 && cols > 0 && rows > (INT_MAX / cols)) return SN_ERR_RANGE;
    n = rows * cols;
    sn_tensor_free_data(ctx, t);
    t->rows = rows;
    t->cols = cols;
    t->e_bits = e_bits;
    t->m_bits = m_bits;
    t->nan_enabled = nan_enabled ? 1 : 0;
    t->n = n;
    t->data = NULL;
    if (n == 0) return SN_OK;
    p = (sn_value *)sn_malloc(ctx, (size_t)n * sizeof(sn_value));
    if (!p) return SN_ERR_NOMEM;
    for (i = 0; i < n; i++) sn_value_init(&p[i]);
    t->data = p;
    return SN_OK;
}

sn_status sn_tensor_create(sn_ctx *ctx, sn_tensor *out, int rows, int cols,
                           int e_bits, int m_bits, int nan_enabled)
{
    sn_status st;
    int i;
    if (!out) return SN_ERR_ARG;
    st = sn_tensor_alloc(ctx, out, rows, cols, e_bits, m_bits, nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < out->n; i++) {
        st = sn_float_set_zero(ctx, &out->data[i], 0, e_bits, m_bits, nan_enabled);
        if (st != SN_OK) {
            sn_tensor_clear(ctx, out);
            return st;
        }
    }
    return SN_OK;
}

sn_status sn_tensor_copy(sn_ctx *ctx, sn_tensor *out, const sn_tensor *src)
{
    sn_status st;
    int i;
    if (!out || !sn_tensor_ok(src)) return SN_ERR_ARG;
    st = sn_tensor_alloc(ctx, out, src->rows, src->cols, src->e_bits, src->m_bits, src->nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < src->n; i++) {
        st = sn_value_copy(ctx, &out->data[i], &src->data[i]);
        if (st != SN_OK) {
            sn_tensor_clear(ctx, out);
            return st;
        }
    }
    return SN_OK;
}

sn_status sn_tensor_from_doubles(sn_ctx *ctx, sn_tensor *out, int rows, int cols,
                                 const double *data, int n,
                                 int e_bits, int m_bits, int nan_enabled,
                                 const sn_op_opt *opt)
{
    sn_status st;
    int i, need;
    if (!out || !data) return SN_ERR_ARG;
    if (rows < 0 || cols < 0) return SN_ERR_RANGE;
    need = rows * cols;
    if (n < need) return SN_ERR_RANGE;
    st = sn_tensor_alloc(ctx, out, rows, cols, e_bits, m_bits, nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < need; i++) {
        st = sn_float_from_double(ctx, &out->data[i], data[i], e_bits, m_bits, nan_enabled, opt);
        if (st != SN_OK) {
            sn_tensor_clear(ctx, out);
            return st;
        }
    }
    return SN_OK;
}

/* Parse "1,2,3; 4,5,6" or whitespace/comma separated with optional ; row breaks. */
sn_status sn_tensor_from_str(sn_ctx *ctx, sn_tensor *out, const char *s,
                             int e_bits, int m_bits, int nan_enabled,
                             const sn_op_opt *opt)
{
    const char *p;
    int rows = 0, cols = 0, ccol = 0, cap = 0, n = 0, i;
    double *buf = NULL;
    sn_status st = SN_OK;
    char tok[256];
    int ti;

    if (!out || !s) return SN_ERR_ARG;
    p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') p++;
        if (!*p) break;
        if (*p == ';') {
            if (ccol > 0) {
                if (rows == 0) cols = ccol;
                else if (ccol != cols) { st = SN_ERR_FORMAT; goto done; }
                rows++;
                ccol = 0;
            }
            p++;
            continue;
        }
        ti = 0;
        if (*p == '+' || *p == '-') {
            if (ti < (int)sizeof(tok) - 1) tok[ti++] = *p;
            p++;
        }
        while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            if (ti < (int)sizeof(tok) - 1) tok[ti++] = *p;
            p++;
        }
        tok[ti] = 0;
        if (ti == 0) continue;
        if (n >= cap) {
            int ncap = cap ? cap * 2 : 16;
            double *nb = (double *)sn_realloc(ctx, buf, (size_t)cap * sizeof(double),
                                              (size_t)ncap * sizeof(double));
            if (!nb) { st = SN_ERR_NOMEM; goto done; }
            buf = nb;
            cap = ncap;
        }
        {
            char *end = NULL;
            buf[n++] = strtod(tok, &end);
            if (end == tok) { st = SN_ERR_FORMAT; goto done; }
        }
        ccol++;
    }
    if (ccol > 0) {
        if (rows == 0) cols = ccol;
        else if (ccol != cols) { st = SN_ERR_FORMAT; goto done; }
        rows++;
    }
    if (rows == 0) {
        /* treat as empty 0x0 or 1-row if any? empty */
        st = sn_tensor_create(ctx, out, 0, 0, e_bits, m_bits, nan_enabled);
        goto done;
    }
    if (cols <= 0) cols = n;
    if (rows * cols != n) {
        /* single row if no semicolon */
        rows = 1;
        cols = n;
    }
    st = sn_tensor_from_doubles(ctx, out, rows, cols, buf, n, e_bits, m_bits, nan_enabled, opt);
done:
    if (buf) sn_free(ctx, buf, (size_t)cap * sizeof(double));
    (void)i;
    return st;
}

sn_status sn_tensor_to_str(sn_ctx *ctx, char **out, const sn_tensor *t)
{
    size_t cap = 256, len = 0;
    char *buf, *cell;
    int r, c;
    sn_status st;
    if (!out || !sn_tensor_ok(t)) return SN_ERR_ARG;
    *out = NULL;
    buf = (char *)sn_malloc(ctx, cap);
    if (!buf) return SN_ERR_NOMEM;
    buf[0] = 0;
    for (r = 0; r < t->rows; r++) {
        if (r) {
            if (len + 2 >= cap) {
                size_t ncap = cap * 2;
                char *nb = (char *)sn_realloc(ctx, buf, cap, ncap);
                if (!nb) { sn_free(ctx, buf, cap); return SN_ERR_NOMEM; }
                buf = nb; cap = ncap;
            }
            buf[len++] = ';';
            buf[len++] = ' ';
            buf[len] = 0;
        }
        for (c = 0; c < t->cols; c++) {
            if (c) {
                if (len + 2 >= cap) {
                    size_t ncap = cap * 2;
                    char *nb = (char *)sn_realloc(ctx, buf, cap, ncap);
                    if (!nb) { sn_free(ctx, buf, cap); return SN_ERR_NOMEM; }
                    buf = nb; cap = ncap;
                }
                buf[len++] = ',';
                buf[len++] = ' ';
                buf[len] = 0;
            }
            st = sn_float_to_str(ctx, &cell, &t->data[r * t->cols + c]);
            if (st != SN_OK) {
                /* fallback double */
                double d = 0;
                char tmp[64];
                sn_to_double(ctx, &t->data[r * t->cols + c], &d);
                snprintf(tmp, sizeof(tmp), "%.17g", d);
                cell = NULL;
                {
                    size_t cl = strlen(tmp);
                    while (len + cl + 1 >= cap) {
                        size_t ncap = cap * 2;
                        char *nb = (char *)sn_realloc(ctx, buf, cap, ncap);
                        if (!nb) { sn_free(ctx, buf, cap); return SN_ERR_NOMEM; }
                        buf = nb; cap = ncap;
                    }
                    memcpy(buf + len, tmp, cl + 1);
                    len += cl;
                }
            } else {
                size_t cl = strlen(cell);
                while (len + cl + 1 >= cap) {
                    size_t ncap = cap * 2;
                    char *nb = (char *)sn_realloc(ctx, buf, cap, ncap);
                    if (!nb) {
                        sn_str_free(ctx, cell);
                        sn_free(ctx, buf, cap);
                        return SN_ERR_NOMEM;
                    }
                    buf = nb; cap = ncap;
                }
                memcpy(buf + len, cell, cl + 1);
                len += cl;
                sn_str_free(ctx, cell);
            }
        }
    }
    *out = buf;
    return SN_OK;
}

void sn_tensor_str_free(sn_ctx *ctx, char *s)
{
    if (!s) return;
    sn_free(ctx, s, strlen(s) + 1);
}

sn_status sn_tensor_dims(const sn_tensor *t, int *rows, int *cols)
{
    if (!sn_tensor_ok(t)) return SN_ERR_ARG;
    if (rows) *rows = t->rows;
    if (cols) *cols = t->cols;
    return SN_OK;
}

sn_status sn_tensor_get(sn_ctx *ctx, sn_value *out, const sn_tensor *t, int r, int c)
{
    if (!ctx || !out || !sn_tensor_ok(t)) return SN_ERR_ARG;
    if (r < 0 || c < 0 || r >= t->rows || c >= t->cols) return SN_ERR_RANGE;
    return sn_value_copy(ctx, out, &t->data[r * t->cols + c]);
}

sn_status sn_tensor_set(sn_ctx *ctx, sn_tensor *t, int r, int c, const sn_value *v)
{
    if (!ctx || !t || !v || !sn_tensor_ok(t)) return SN_ERR_ARG;
    if (r < 0 || c < 0 || r >= t->rows || c >= t->cols) return SN_ERR_RANGE;
    if (v->kind != SN_KIND_FLOAT) return SN_ERR_TYPE;
    if (v->e_bits != t->e_bits || v->m_bits != t->m_bits) return SN_ERR_TYPE;
    return sn_value_copy(ctx, &t->data[r * t->cols + c], v);
}

sn_status sn_tensor_transpose(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a)
{
    sn_status st;
    int r, c;
    sn_tensor tmp;
    if (!sn_tensor_ok(a)) return SN_ERR_ARG;
    sn_tensor_init(&tmp);
    st = sn_tensor_alloc(ctx, &tmp, a->cols, a->rows, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) return st;
    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            st = sn_value_copy(ctx, &tmp.data[c * tmp.cols + r], &a->data[r * a->cols + c]);
            if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
        }
    }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

sn_status sn_tensor_reshape(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int rows, int cols)
{
    sn_status st;
    int i;
    if (!sn_tensor_ok(a) || rows <= 0 || cols <= 0) return SN_ERR_ARG;
    if (rows * cols != a->n) return SN_ERR_RANGE;
    st = sn_tensor_alloc(ctx, out, rows, cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < a->n; i++) {
        st = sn_value_copy(ctx, &out->data[i], &a->data[i]);
        if (st != SN_OK) { sn_tensor_clear(ctx, out); return st; }
    }
    return SN_OK;
}

sn_status sn_tensor_matmul(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b,
                           const sn_op_opt *opt)
{
    sn_status st;
    int i, j, k;
    sn_tensor tmp;
    sn_value acc, prod;
    if (!sn_tensor_ok(a) || !sn_tensor_ok(b)) return SN_ERR_ARG;
    if (a->cols != b->rows) return SN_ERR_RANGE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits || a->nan_enabled != b->nan_enabled)
        return SN_ERR_TYPE;
    sn_tensor_init(&tmp);
    sn_value_init(&acc);
    sn_value_init(&prod);
    st = sn_tensor_create(ctx, &tmp, a->rows, b->cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < a->rows; i++) {
        for (j = 0; j < b->cols; j++) {
            st = sn_float_set_zero(ctx, &acc, 0, a->e_bits, a->m_bits, a->nan_enabled);
            if (st != SN_OK) goto fail;
            for (k = 0; k < a->cols; k++) {
                st = sn_mul(ctx, &prod, &a->data[i * a->cols + k], &b->data[k * b->cols + j], opt);
                if (st != SN_OK) goto fail;
                st = sn_add(ctx, &acc, &acc, &prod, opt);
                if (st != SN_OK) goto fail;
            }
            st = sn_value_copy(ctx, &tmp.data[i * tmp.cols + j], &acc);
            if (st != SN_OK) goto fail;
        }
    }
    sn_value_clear(ctx, &acc);
    sn_value_clear(ctx, &prod);
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
fail:
    sn_value_clear(ctx, &acc);
    sn_value_clear(ctx, &prod);
    sn_tensor_clear(ctx, &tmp);
    return st;
}

static sn_status sn_tensor_bin_elem(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b,
                                    int op, const sn_op_opt *opt)
{
    sn_status st;
    int i;
    sn_tensor tmp;
    if (!sn_tensor_ok(a) || !sn_tensor_ok(b)) return SN_ERR_ARG;
    if (a->rows != b->rows || a->cols != b->cols) return SN_ERR_RANGE;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    sn_tensor_init(&tmp);
    st = sn_tensor_alloc(ctx, &tmp, a->rows, a->cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < a->n; i++) {
        if (op == 0) st = sn_add(ctx, &tmp.data[i], &a->data[i], &b->data[i], opt);
        else if (op == 1) st = sn_sub(ctx, &tmp.data[i], &a->data[i], &b->data[i], opt);
        else if (op == 2) st = sn_mul(ctx, &tmp.data[i], &a->data[i], &b->data[i], opt);
        else st = sn_div(ctx, &tmp.data[i], &a->data[i], &b->data[i], opt);
        if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

sn_status sn_tensor_add(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt)
{ return sn_tensor_bin_elem(ctx, out, a, b, 0, opt); }
sn_status sn_tensor_sub(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt)
{ return sn_tensor_bin_elem(ctx, out, a, b, 1, opt); }
sn_status sn_tensor_hadamard(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt)
{ return sn_tensor_bin_elem(ctx, out, a, b, 2, opt); }
sn_status sn_tensor_div(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, const sn_op_opt *opt)
{ return sn_tensor_bin_elem(ctx, out, a, b, 3, opt); }

sn_status sn_tensor_scale(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_value *s, const sn_op_opt *opt)
{
    sn_status st;
    int i;
    sn_tensor tmp;
    if (!sn_tensor_ok(a) || !s) return SN_ERR_ARG;
    sn_tensor_init(&tmp);
    st = sn_tensor_alloc(ctx, &tmp, a->rows, a->cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < a->n; i++) {
        st = sn_mul(ctx, &tmp.data[i], &a->data[i], s, opt);
        if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

/* unary: 0=neg,1=exp,2=tanh,3=relu,4=gelu,5=silu,6=sqrt,7=abs */
sn_status sn_tensor_unary(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, int op, const sn_op_opt *opt)
{
    sn_status st;
    int i;
    sn_tensor tmp;
    sn_value zero, half, k, t1, t2, t3;
    if (!sn_tensor_ok(a)) return SN_ERR_ARG;
    sn_tensor_init(&tmp);
    sn_value_init(&zero); sn_value_init(&half); sn_value_init(&k);
    sn_value_init(&t1); sn_value_init(&t2); sn_value_init(&t3);
    st = sn_tensor_alloc(ctx, &tmp, a->rows, a->cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) goto done;
    st = sn_float_set_zero(ctx, &zero, 0, a->e_bits, a->m_bits, a->nan_enabled); if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &half, 0.5, a->e_bits, a->m_bits, a->nan_enabled, opt); if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &k, 0.7978845608028654, a->e_bits, a->m_bits, a->nan_enabled, opt); if (st != SN_OK) goto done;
    for (i = 0; i < a->n; i++) {
        const sn_value *x = &a->data[i];
        sn_value *y = &tmp.data[i];
        if (op == 0) { st = sn_neg(ctx, y, x, opt); }
        else if (op == 1) { st = sn_exp(ctx, y, x, opt); }
        else if (op == 2) { st = sn_tanh(ctx, y, x, opt); }
        else if (op == 3) {
            int rel = 0;
            st = sn_cmp(ctx, &rel, x, &zero);
            if (st != SN_OK) break;
            if (rel > 0) st = sn_value_copy(ctx, y, x);
            else st = sn_value_copy(ctx, y, &zero);
        } else if (op == 4) {
            /* gelu approx: 0.5*x*(1+tanh(k*(x+0.044715*x^3))) */
            st = sn_mul(ctx, &t1, x, x, opt); if (st != SN_OK) break;
            st = sn_mul(ctx, &t1, &t1, x, opt); if (st != SN_OK) break;
            st = sn_float_from_double(ctx, &t2, 0.044715, a->e_bits, a->m_bits, a->nan_enabled, opt); if (st != SN_OK) break;
            st = sn_mul(ctx, &t1, &t1, &t2, opt); if (st != SN_OK) break;
            st = sn_add(ctx, &t1, x, &t1, opt); if (st != SN_OK) break;
            st = sn_mul(ctx, &t1, &t1, &k, opt); if (st != SN_OK) break;
            st = sn_tanh(ctx, &t2, &t1, opt); if (st != SN_OK) break;
            st = sn_float_from_double(ctx, &t3, 1.0, a->e_bits, a->m_bits, a->nan_enabled, opt); if (st != SN_OK) break;
            st = sn_add(ctx, &t2, &t3, &t2, opt); if (st != SN_OK) break;
            st = sn_mul(ctx, &t1, x, &t2, opt); if (st != SN_OK) break;
            st = sn_mul(ctx, y, &t1, &half, opt);
        } else if (op == 5) {
            /* silu = x * sigmoid(x) = x / (1+exp(-x)) */
            st = sn_neg(ctx, &t1, x, opt); if (st != SN_OK) break;
            st = sn_exp(ctx, &t1, &t1, opt); if (st != SN_OK) break;
            st = sn_float_from_double(ctx, &t2, 1.0, a->e_bits, a->m_bits, a->nan_enabled, opt); if (st != SN_OK) break;
            st = sn_add(ctx, &t1, &t2, &t1, opt); if (st != SN_OK) break;
            st = sn_div(ctx, y, x, &t1, opt);
        } else if (op == 6) { st = sn_sqrt(ctx, y, x, opt); }
        else if (op == 7) { st = sn_abs(ctx, y, x, opt); }
        else { st = SN_ERR_ARG; }
        if (st != SN_OK) break;
    }
done:
    sn_value_clear(ctx, &zero); sn_value_clear(ctx, &half); sn_value_clear(ctx, &k);
    sn_value_clear(ctx, &t1); sn_value_clear(ctx, &t2); sn_value_clear(ctx, &t3);
    if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

sn_status sn_tensor_softmax_row(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_op_opt *opt)
{
    sn_status st;
    int r, c;
    sn_tensor tmp;
    sn_value maxv, sum, e, t;
    if (!sn_tensor_ok(a) || a->cols <= 0) return SN_ERR_ARG;
    sn_tensor_init(&tmp);
    sn_value_init(&maxv); sn_value_init(&sum); sn_value_init(&e); sn_value_init(&t);
    st = sn_tensor_alloc(ctx, &tmp, a->rows, a->cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) goto done;
    for (r = 0; r < a->rows; r++) {
        st = sn_value_copy(ctx, &maxv, &a->data[r * a->cols]);
        if (st != SN_OK) goto done;
        for (c = 1; c < a->cols; c++) {
            int rel = 0;
            st = sn_cmp(ctx, &rel, &a->data[r * a->cols + c], &maxv);
            if (st != SN_OK) goto done;
            if (rel > 0) {
                st = sn_value_copy(ctx, &maxv, &a->data[r * a->cols + c]);
                if (st != SN_OK) goto done;
            }
        }
        st = sn_float_set_zero(ctx, &sum, 0, a->e_bits, a->m_bits, a->nan_enabled);
        if (st != SN_OK) goto done;
        for (c = 0; c < a->cols; c++) {
            st = sn_sub(ctx, &t, &a->data[r * a->cols + c], &maxv, opt); if (st != SN_OK) goto done;
            st = sn_exp(ctx, &e, &t, opt); if (st != SN_OK) goto done;
            st = sn_value_copy(ctx, &tmp.data[r * tmp.cols + c], &e); if (st != SN_OK) goto done;
            st = sn_add(ctx, &sum, &sum, &e, opt); if (st != SN_OK) goto done;
        }
        for (c = 0; c < a->cols; c++) {
            st = sn_div(ctx, &tmp.data[r * tmp.cols + c], &tmp.data[r * tmp.cols + c], &sum, opt);
            if (st != SN_OK) goto done;
        }
    }
done:
    sn_value_clear(ctx, &maxv); sn_value_clear(ctx, &sum);
    sn_value_clear(ctx, &e); sn_value_clear(ctx, &t);
    if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

sn_status sn_tensor_rms_norm(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a,
                             const sn_tensor *gamma, double eps, const sn_op_opt *opt)
{
    sn_status st;
    int r, c;
    sn_tensor tmp;
    sn_value sum, mean, inv, t, epsv, one;
    if (!sn_tensor_ok(a) || a->cols <= 0) return SN_ERR_ARG;
    if (gamma && sn_tensor_ok(gamma)) {
        if (!(gamma->n == a->cols || (gamma->rows == 1 && gamma->cols == a->cols) ||
              (gamma->cols == 1 && gamma->rows == a->cols)))
            return SN_ERR_RANGE;
    }
    sn_tensor_init(&tmp);
    sn_value_init(&sum); sn_value_init(&mean); sn_value_init(&inv);
    sn_value_init(&t); sn_value_init(&epsv); sn_value_init(&one);
    st = sn_tensor_alloc(ctx, &tmp, a->rows, a->cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &epsv, eps > 0 ? eps : 1e-6, a->e_bits, a->m_bits, a->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &one, 1.0, a->e_bits, a->m_bits, a->nan_enabled, opt);
    if (st != SN_OK) goto done;
    for (r = 0; r < a->rows; r++) {
        st = sn_float_set_zero(ctx, &sum, 0, a->e_bits, a->m_bits, a->nan_enabled);
        if (st != SN_OK) goto done;
        for (c = 0; c < a->cols; c++) {
            st = sn_mul(ctx, &t, &a->data[r * a->cols + c], &a->data[r * a->cols + c], opt);
            if (st != SN_OK) goto done;
            st = sn_add(ctx, &sum, &sum, &t, opt);
            if (st != SN_OK) goto done;
        }
        st = sn_float_from_double(ctx, &mean, (double)a->cols, a->e_bits, a->m_bits, a->nan_enabled, opt);
        if (st != SN_OK) goto done;
        st = sn_div(ctx, &mean, &sum, &mean, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &mean, &mean, &epsv, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &mean, &mean, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &inv, &one, &mean, opt); if (st != SN_OK) goto done;
        for (c = 0; c < a->cols; c++) {
            st = sn_mul(ctx, &t, &a->data[r * a->cols + c], &inv, opt); if (st != SN_OK) goto done;
            if (gamma && sn_tensor_ok(gamma) && gamma->n > 0) {
                const sn_value *g = &gamma->data[c % gamma->n];
                st = sn_mul(ctx, &t, &t, g, opt); if (st != SN_OK) goto done;
            }
            st = sn_value_copy(ctx, &tmp.data[r * tmp.cols + c], &t); if (st != SN_OK) goto done;
        }
    }
done:
    sn_value_clear(ctx, &sum); sn_value_clear(ctx, &mean); sn_value_clear(ctx, &inv);
    sn_value_clear(ctx, &t); sn_value_clear(ctx, &epsv); sn_value_clear(ctx, &one);
    if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

/* RoPE on last dim: pairs (2i,2i+1). theta = pos / base^(2i/dim) */
sn_status sn_tensor_rope(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, double base, const sn_op_opt *opt)
{
    sn_status st;
    int pos, i, dim;
    sn_tensor tmp;
    sn_value x0, x1, cosv, sinv, t0, t1, ang;
    if (!sn_tensor_ok(a) || a->cols < 2) return SN_ERR_ARG;
    if (!(base > 0.0)) base = 10000.0;
    dim = a->cols;
    sn_tensor_init(&tmp);
    sn_value_init(&x0); sn_value_init(&x1); sn_value_init(&cosv); sn_value_init(&sinv);
    sn_value_init(&t0); sn_value_init(&t1); sn_value_init(&ang);
    st = sn_tensor_copy(ctx, &tmp, a);
    if (st != SN_OK) goto done;
    for (pos = 0; pos < a->rows; pos++) {
        for (i = 0; i + 1 < dim; i += 2) {
            double inv_freq = 1.0 / pow(base, (double)i / (double)dim);
            double angle = (double)pos * inv_freq;
            st = sn_value_copy(ctx, &x0, &a->data[pos * dim + i]); if (st != SN_OK) goto done;
            st = sn_value_copy(ctx, &x1, &a->data[pos * dim + i + 1]); if (st != SN_OK) goto done;
            st = sn_float_from_double(ctx, &ang, angle, a->e_bits, a->m_bits, a->nan_enabled, opt); if (st != SN_OK) goto done;
            st = sn_cos(ctx, &cosv, &ang, opt); if (st != SN_OK) goto done;
            st = sn_sin(ctx, &sinv, &ang, opt); if (st != SN_OK) goto done;
            /* out0 = x0*cos - x1*sin; out1 = x0*sin + x1*cos */
            st = sn_mul(ctx, &t0, &x0, &cosv, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t1, &x1, &sinv, opt); if (st != SN_OK) goto done;
            st = sn_sub(ctx, &tmp.data[pos * dim + i], &t0, &t1, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t0, &x0, &sinv, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t1, &x1, &cosv, opt); if (st != SN_OK) goto done;
            st = sn_add(ctx, &tmp.data[pos * dim + i + 1], &t0, &t1, opt); if (st != SN_OK) goto done;
        }
    }
done:
    sn_value_clear(ctx, &x0); sn_value_clear(ctx, &x1);
    sn_value_clear(ctx, &cosv); sn_value_clear(ctx, &sinv);
    sn_value_clear(ctx, &t0); sn_value_clear(ctx, &t1); sn_value_clear(ctx, &ang);
    if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

sn_status sn_tensor_gather(sn_ctx *ctx, sn_tensor *out, const sn_tensor *table,
                           const int *indices, int nidx)
{
    sn_status st;
    int i, c;
    sn_tensor tmp;
    if (!sn_tensor_ok(table) || !indices || nidx < 0) return SN_ERR_ARG;
    sn_tensor_init(&tmp);
    st = sn_tensor_alloc(ctx, &tmp, nidx, table->cols, table->e_bits, table->m_bits, table->nan_enabled);
    if (st != SN_OK) return st;
    for (i = 0; i < nidx; i++) {
        int r = indices[i];
        if (r < 0 || r >= table->rows) { sn_tensor_clear(ctx, &tmp); return SN_ERR_RANGE; }
        for (c = 0; c < table->cols; c++) {
            st = sn_value_copy(ctx, &tmp.data[i * tmp.cols + c], &table->data[r * table->cols + c]);
            if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
        }
    }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

sn_status sn_tensor_slice(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a,
                          int r0, int r1, int c0, int c1)
{
    sn_status st;
    int r, c, nr, nc;
    sn_tensor tmp;
    if (!sn_tensor_ok(a)) return SN_ERR_ARG;
    if (r0 < 0) r0 = 0;
    if (c0 < 0) c0 = 0;
    if (r1 > a->rows) r1 = a->rows;
    if (c1 > a->cols) c1 = a->cols;
    if (r0 >= r1 || c0 >= c1) return SN_ERR_RANGE;
    nr = r1 - r0; nc = c1 - c0;
    sn_tensor_init(&tmp);
    st = sn_tensor_alloc(ctx, &tmp, nr, nc, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) return st;
    for (r = 0; r < nr; r++) {
        for (c = 0; c < nc; c++) {
            st = sn_value_copy(ctx, &tmp.data[r * nc + c], &a->data[(r0 + r) * a->cols + (c0 + c)]);
            if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
        }
    }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

sn_status sn_tensor_concat(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a, const sn_tensor *b, int axis)
{
    sn_status st;
    int r, c;
    sn_tensor tmp;
    if (!sn_tensor_ok(a) || !sn_tensor_ok(b)) return SN_ERR_ARG;
    if (a->e_bits != b->e_bits || a->m_bits != b->m_bits) return SN_ERR_TYPE;
    sn_tensor_init(&tmp);
    if (axis == 0) {
        if (a->cols != b->cols) return SN_ERR_RANGE;
        st = sn_tensor_alloc(ctx, &tmp, a->rows + b->rows, a->cols, a->e_bits, a->m_bits, a->nan_enabled);
        if (st != SN_OK) return st;
        for (r = 0; r < a->rows; r++)
            for (c = 0; c < a->cols; c++) {
                st = sn_value_copy(ctx, &tmp.data[r * tmp.cols + c], &a->data[r * a->cols + c]);
                if (st != SN_OK) goto fail;
            }
        for (r = 0; r < b->rows; r++)
            for (c = 0; c < b->cols; c++) {
                st = sn_value_copy(ctx, &tmp.data[(a->rows + r) * tmp.cols + c], &b->data[r * b->cols + c]);
                if (st != SN_OK) goto fail;
            }
    } else {
        if (a->rows != b->rows) return SN_ERR_RANGE;
        st = sn_tensor_alloc(ctx, &tmp, a->rows, a->cols + b->cols, a->e_bits, a->m_bits, a->nan_enabled);
        if (st != SN_OK) return st;
        for (r = 0; r < a->rows; r++) {
            for (c = 0; c < a->cols; c++) {
                st = sn_value_copy(ctx, &tmp.data[r * tmp.cols + c], &a->data[r * a->cols + c]);
                if (st != SN_OK) goto fail;
            }
            for (c = 0; c < b->cols; c++) {
                st = sn_value_copy(ctx, &tmp.data[r * tmp.cols + a->cols + c], &b->data[r * b->cols + c]);
                if (st != SN_OK) goto fail;
            }
        }
    }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
fail:
    sn_tensor_clear(ctx, &tmp);
    return st;
}

/* SDPA: out = softmax(scale * Q K^T) V ; optional causal mask */
sn_status sn_tensor_attention_sdp(sn_ctx *ctx, sn_tensor *out, sn_tensor *weights_opt,
                                  const sn_tensor *q, const sn_tensor *k, const sn_tensor *v,
                                  int causal, double scale, const sn_op_opt *opt)
{
    sn_status st;
    sn_tensor kt, scores, w, o;
    sn_value sc;
    int i;
    if (!sn_tensor_ok(q) || !sn_tensor_ok(k) || !sn_tensor_ok(v)) return SN_ERR_ARG;
    if (q->cols != k->cols || k->rows != v->rows) return SN_ERR_RANGE;
    if (!(scale > 0.0)) scale = 1.0 / sqrt((double)(q->cols > 0 ? q->cols : 1));
    sn_tensor_init(&kt); sn_tensor_init(&scores); sn_tensor_init(&w); sn_tensor_init(&o);
    sn_value_init(&sc);
    st = sn_tensor_transpose(ctx, &kt, k); if (st != SN_OK) goto done;
    st = sn_tensor_matmul(ctx, &scores, q, &kt, opt); if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &sc, scale, q->e_bits, q->m_bits, q->nan_enabled, opt); if (st != SN_OK) goto done;
    st = sn_tensor_scale(ctx, &scores, &scores, &sc, opt); if (st != SN_OK) goto done;
    if (causal) {
        sn_value neg;
        sn_value_init(&neg);
        st = sn_float_from_double(ctx, &neg, -1.0e30, q->e_bits, q->m_bits, q->nan_enabled, opt);
        if (st != SN_OK) { sn_value_clear(ctx, &neg); goto done; }
        for (i = 0; i < scores.rows; i++) {
            int c;
            for (c = i + 1; c < scores.cols; c++) {
                st = sn_value_copy(ctx, &scores.data[i * scores.cols + c], &neg);
                if (st != SN_OK) { sn_value_clear(ctx, &neg); goto done; }
            }
        }
        sn_value_clear(ctx, &neg);
    }
    st = sn_tensor_softmax_row(ctx, &w, &scores, opt); if (st != SN_OK) goto done;
    st = sn_tensor_matmul(ctx, &o, &w, v, opt); if (st != SN_OK) goto done;
    if (weights_opt) {
        sn_tensor_clear(ctx, weights_opt);
        st = sn_tensor_copy(ctx, weights_opt, &w);
        if (st != SN_OK) goto done;
    }
    sn_tensor_clear(ctx, out);
    *out = o;
    sn_tensor_init(&o);
done:
    sn_value_clear(ctx, &sc);
    sn_tensor_clear(ctx, &kt);
    sn_tensor_clear(ctx, &scores);
    sn_tensor_clear(ctx, &w);
    sn_tensor_clear(ctx, &o);
    return st;
}


/* LayerNorm over last dim: y = gamma * (x-mean)/sqrt(var+eps) + beta */
sn_status sn_tensor_layer_norm(sn_ctx *ctx, sn_tensor *out, const sn_tensor *a,
                               const sn_tensor *gamma, const sn_tensor *beta,
                               double eps, const sn_op_opt *opt)
{
    sn_status st = SN_OK;
    int r, c;
    sn_tensor tmp;
    sn_value sum, mean, var, inv, t, epsv, one, ncols;
    if (!sn_tensor_ok(a) || a->cols <= 0) return SN_ERR_ARG;
    if (gamma && sn_tensor_ok(gamma)) {
        if (!(gamma->n == a->cols || (gamma->rows == 1 && gamma->cols == a->cols) ||
              (gamma->cols == 1 && gamma->rows == a->cols)))
            return SN_ERR_RANGE;
    }
    if (beta && sn_tensor_ok(beta)) {
        if (!(beta->n == a->cols || (beta->rows == 1 && beta->cols == a->cols) ||
              (beta->cols == 1 && beta->rows == a->cols)))
            return SN_ERR_RANGE;
    }
    sn_tensor_init(&tmp);
    sn_value_init(&sum); sn_value_init(&mean); sn_value_init(&var);
    sn_value_init(&inv); sn_value_init(&t); sn_value_init(&epsv);
    sn_value_init(&one); sn_value_init(&ncols);
    st = sn_tensor_alloc(ctx, &tmp, a->rows, a->cols, a->e_bits, a->m_bits, a->nan_enabled);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &epsv, eps > 0 ? eps : 1e-5, a->e_bits, a->m_bits, a->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &one, 1.0, a->e_bits, a->m_bits, a->nan_enabled, opt);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &ncols, (double)a->cols, a->e_bits, a->m_bits, a->nan_enabled, opt);
    if (st != SN_OK) goto done;
    for (r = 0; r < a->rows; r++) {
        st = sn_float_set_zero(ctx, &sum, 0, a->e_bits, a->m_bits, a->nan_enabled);
        if (st != SN_OK) goto done;
        for (c = 0; c < a->cols; c++) {
            st = sn_add(ctx, &sum, &sum, &a->data[r * a->cols + c], opt);
            if (st != SN_OK) goto done;
        }
        st = sn_div(ctx, &mean, &sum, &ncols, opt); if (st != SN_OK) goto done;
        st = sn_float_set_zero(ctx, &var, 0, a->e_bits, a->m_bits, a->nan_enabled);
        if (st != SN_OK) goto done;
        for (c = 0; c < a->cols; c++) {
            st = sn_sub(ctx, &t, &a->data[r * a->cols + c], &mean, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t, &t, &t, opt); if (st != SN_OK) goto done;
            st = sn_add(ctx, &var, &var, &t, opt); if (st != SN_OK) goto done;
        }
        st = sn_div(ctx, &var, &var, &ncols, opt); if (st != SN_OK) goto done;
        st = sn_add(ctx, &var, &var, &epsv, opt); if (st != SN_OK) goto done;
        st = sn_sqrt(ctx, &var, &var, opt); if (st != SN_OK) goto done;
        st = sn_div(ctx, &inv, &one, &var, opt); if (st != SN_OK) goto done;
        for (c = 0; c < a->cols; c++) {
            st = sn_sub(ctx, &t, &a->data[r * a->cols + c], &mean, opt); if (st != SN_OK) goto done;
            st = sn_mul(ctx, &t, &t, &inv, opt); if (st != SN_OK) goto done;
            if (gamma && sn_tensor_ok(gamma) && gamma->n > 0) {
                const sn_value *g = &gamma->data[c % gamma->n];
                st = sn_mul(ctx, &t, &t, g, opt); if (st != SN_OK) goto done;
            }
            if (beta && sn_tensor_ok(beta) && beta->n > 0) {
                const sn_value *b = &beta->data[c % beta->n];
                st = sn_add(ctx, &t, &t, b, opt); if (st != SN_OK) goto done;
            }
            st = sn_value_copy(ctx, &tmp.data[r * tmp.cols + c], &t); if (st != SN_OK) goto done;
        }
    }
done:
    sn_value_clear(ctx, &sum); sn_value_clear(ctx, &mean); sn_value_clear(ctx, &var);
    sn_value_clear(ctx, &inv); sn_value_clear(ctx, &t); sn_value_clear(ctx, &epsv);
    sn_value_clear(ctx, &one); sn_value_clear(ctx, &ncols);
    if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

/* Sinusoidal positional encoding: out[pos,i] = sin/cos(pos / 10000^(2*floor(i/2)/dim)) */
sn_status sn_tensor_sin_pe(sn_ctx *ctx, sn_tensor *out, int seq, int dim,
                           double base, const sn_op_opt *opt)
{
    sn_status st = SN_OK;
    int pos, i;
    sn_tensor tmp;
    sn_value ang, v, half, den, two, basev, dimv, posv, iv;
    if (seq <= 0 || dim <= 0) return SN_ERR_ARG;
    if (!(base > 0.0)) base = 10000.0;
    sn_tensor_init(&tmp);
    sn_value_init(&ang); sn_value_init(&v); sn_value_init(&half);
    sn_value_init(&den); sn_value_init(&two); sn_value_init(&basev);
    sn_value_init(&dimv); sn_value_init(&posv); sn_value_init(&iv);
    /* create uses default float format from host session callers — use FP64-ish defaults */
    st = sn_tensor_create(ctx, &tmp, seq, dim, 11, 52, 1);
    if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &two, 2.0, 11, 52, 1, opt); if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &basev, base, 11, 52, 1, opt); if (st != SN_OK) goto done;
    st = sn_float_from_double(ctx, &dimv, (double)dim, 11, 52, 1, opt); if (st != SN_OK) goto done;
    for (pos = 0; pos < seq; pos++) {
        st = sn_float_from_double(ctx, &posv, (double)pos, 11, 52, 1, opt); if (st != SN_OK) goto done;
        for (i = 0; i < dim; i++) {
            int h = i / 2;
            double ratio = (2.0 * (double)h) / (double)dim;
            double den_d = pow(base, ratio);
            double ang_d = (double)pos / den_d;
            double val = (i % 2 == 0) ? sin(ang_d) : cos(ang_d);
            st = sn_float_from_double(ctx, &v, val, 11, 52, 1, opt); if (st != SN_OK) goto done;
            st = sn_value_copy(ctx, &tmp.data[pos * dim + i], &v); if (st != SN_OK) goto done;
            (void)half; (void)den; (void)iv; (void)ang;
        }
    }
done:
    sn_value_clear(ctx, &ang); sn_value_clear(ctx, &v); sn_value_clear(ctx, &half);
    sn_value_clear(ctx, &den); sn_value_clear(ctx, &two); sn_value_clear(ctx, &basev);
    sn_value_clear(ctx, &dimv); sn_value_clear(ctx, &posv); sn_value_clear(ctx, &iv);
    if (st != SN_OK) { sn_tensor_clear(ctx, &tmp); return st; }
    sn_tensor_clear(ctx, out);
    *out = tmp;
    return SN_OK;
}

