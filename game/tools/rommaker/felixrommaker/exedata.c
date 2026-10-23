/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fr.h"
#include "embed.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { V_INT, V_FLOAT, V_STR, V_LIST };

typedef struct val {
    int t;
    int64_t i;
    double f;
    char *s;
    int n;
    struct val *v;
} val_t;

static char err_buf[256];

static val_t vint(int64_t i)
{
    val_t v;
    memset(&v, 0, sizeof v);
    v.t = V_INT;
    v.i = i;
    return v;
}

static val_t vfloat(double f)
{
    val_t v;
    memset(&v, 0, sizeof v);
    v.t = V_FLOAT;
    v.f = f;
    return v;
}

static val_t vlist(int n)
{
    val_t v;
    memset(&v, 0, sizeof v);
    v.t = V_LIST;
    v.n = n;
    v.v = xcalloc((size_t)(n ? n : 1), sizeof(val_t));
    return v;
}

static val_t vstr(const char *s, int n)
{
    val_t v;
    memset(&v, 0, sizeof v);
    v.t = V_STR;
    v.n = n;
    v.s = xmalloc((size_t)n + 1);
    memcpy(v.s, s, (size_t)n);
    v.s[n] = 0;
    return v;
}

static double as_float(val_t v)
{
    if (v.t == V_INT)
        return (double)v.i;
    if (v.t == V_FLOAT)
        return v.f;
    die("game number: not a number");
    return 0;
}

static int64_t as_int(val_t v)
{
    if (v.t == V_INT)
        return v.i;
    die("game number: not an integer");
    return 0;
}

static int64_t floordiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        q--;
    return q;
}

static val_t c_round(val_t v)
{
    double f;
    if (v.t == V_INT)
        return v;
    f = as_float(v);
    if (f >= 0)
        return vint((int64_t)floor(f + 0.5));
    return vint(-(int64_t)floor(-f + 0.5));
}

static val_t arith(int op, val_t a, val_t b)
{
    if (a.t == V_INT && b.t == V_INT) {
        switch (op) {
        case X_ADD:
            return vint(a.i + b.i);
        case X_SUB:
            return vint(a.i - b.i);
        case X_MUL:
            return vint(a.i * b.i);
        default:
            return vint(floordiv(a.i, b.i));
        }
    }
    {
        double x = as_float(a), y = as_float(b);
        switch (op) {
        case X_ADD:
            return vfloat(x + y);
        case X_SUB:
            return vfloat(x - y);
        case X_MUL:
            return vfloat(x * y);
        default:
            return vfloat(floor(x / y));
        }
    }
}

static val_t xf_arg(const xf_t *x)
{
    if (x->argkind == A_FLOAT)
        return vfloat(x->f);
    return vint(x->i);
}

static val_t apply1(const xf_t *x, val_t v)
{
    switch (x->op) {
    case X_FIX16:
        return c_round(v.t == V_INT ? vint(v.i * 65536) : vfloat(v.f * 65536));
    case X_FIX16UP:
        return v.t == V_INT ? vint(v.i * 65536) : vint((int64_t)ceil(v.f * 65536));
    case X_SQRT:
        return vfloat(sqrt(as_float(v)));
    case X_FIX8:
        return c_round(v.t == V_INT ? vint(v.i * 256) : vfloat(v.f * 256));
    case X_ROUND:
        return c_round(v);
    case X_TRUNC:
        return v.t == V_INT ? v : vint((int64_t)v.f);
    case X_ADD:
    case X_SUB:
    case X_MUL:
    case X_DIV:
        return arith(x->op, v, xf_arg(x));
    case X_NEG:
        return v.t == V_INT ? vint(-v.i) : vfloat(-v.f);
    case X_AND:
        return vint(as_int(v) & x->i);
    case X_SHR:
        return vint(as_int(v) >> x->i);
    case X_LOWBIT:
        return vint(as_int(v) & -as_int(v));
    case X_LOG2: {
        int64_t x = as_int(v), n = -1;
        while (x) {
            x >>= 1;
            n++;
        }
        return vint(n);
    }
    case X_ODDPART:
        return vint(floordiv(as_int(v), as_int(v) & -as_int(v)));
    case X_INV16: {
        int64_t a = as_int(v) & 0xFFFF, x = 1;
        int k;
        for (k = 0; k < 5; k++)
            x = (x * (2 - a * x)) & 0xFFFF;
        if (((a * x) & 0xFFFF) != 1)
            die("game number: %lld has no inverse", (long long)a);
        return vint(x);
    }
    case X_RFLOORDIV:
        return vint(floordiv(x->i, as_int(v)));
    case X_MAGICDIV:
        return c_round(vfloat(ldexp(1.0, (int)(32 + x->i)) / as_float(v)));
    }
    die("game number: bad transform %d", x->op);
    return v;
}

static val_t transform(const xf_t *x, val_t v)
{
    int i;
    switch (x->op) {
    case X_PICK: {
        val_t out = vlist(x->n);
        for (i = 0; i < x->n; i++)
            out.v[i] = v.v[embed_ints[x->list + i]];
        return out;
    }
    case X_SLICE: {
        int lo = (int)embed_ints[x->list], hi = (int)embed_ints[x->list + 1];
        int len = v.t == V_STR ? v.n : v.n;
        if (hi > len)
            hi = len;
        if (lo > hi)
            lo = hi;
        if (v.t == V_STR)
            return vstr(v.s + lo, hi - lo);
        {
            val_t out = vlist(hi - lo);
            for (i = lo; i < hi; i++)
                out.v[i - lo] = v.v[i];
            return out;
        }
    }
    case X_EVERY: {
        int start = (int)embed_ints[x->list], step = (int)embed_ints[x->list + 1], n = 0;
        val_t out = vlist(v.n);
        for (i = start; i < v.n; i += step)
            out.v[n++] = v.v[i];
        out.n = n;
        return out;
    }
    case X_REVERSE: {
        val_t out = vlist(v.n);
        for (i = 0; i < v.n; i++)
            out.v[v.n - 1 - i] = v.v[i];
        return out;
    }
    case X_STRIP0:
        return vstr(v.s, (int)strlen(v.s));
    }
    if (v.t == V_LIST) {
        val_t out = vlist(v.n);
        for (i = 0; i < v.n; i++)
            out.v[i] = apply1(x, v.v[i]);
        return out;
    }
    return apply1(x, v);
}

static val_t calc(int op, val_t *in, int n)
{
    int i, j, k;
    switch (op) {
    case C_CONCAT: {
        int total = 0;
        val_t out;
        for (i = 0; i < n; i++)
            total += in[i].t == V_LIST ? in[i].n : 1;
        out = vlist(total);
        k = 0;
        for (i = 0; i < n; i++) {
            if (in[i].t == V_LIST)
                for (j = 0; j < in[i].n; j++)
                    out.v[k++] = in[i].v[j];
            else
                out.v[k++] = in[i];
        }
        return out;
    }
    case C_SUM: {
        val_t total = vint(0);
        for (i = 0; i < n; i++)
            total = arith(X_ADD, total, in[i]);
        return total;
    }
    case C_DIFF:
        return arith(X_SUB, in[0], in[1]);
    case C_VADD: {
        int m = in[0].n < in[1].n ? in[0].n : in[1].n;
        val_t out = vlist(m);
        for (i = 0; i < m; i++)
            out.v[i] = arith(X_ADD, in[0].v[i], in[1].v[i]);
        return out;
    }
    case C_QUOT:
        return vfloat(as_float(in[0]) / as_float(in[1]));
    case C_PRODUCT:
        return arith(X_MUL, in[0], in[1]);
    case C_VMIN:
    case C_VMAX: {
        int m = -1;
        val_t out;
        for (i = 0; i < n; i++)
            if (m < 0 || in[i].n < m)
                m = in[i].n;
        out = vlist(m);
        for (j = 0; j < m; j++) {
            out.v[j] = in[0].v[j];
            for (i = 1; i < n; i++) {
                int64_t a = as_int(in[i].v[j]), b = as_int(out.v[j]);
                if (op == C_VMIN ? a < b : a > b)
                    out.v[j] = in[i].v[j];
            }
        }
        return out;
    }
    case C_ZIP: {
        int m = -1;
        val_t out;
        for (i = 0; i < n; i++)
            if (m < 0 || in[i].n < m)
                m = in[i].n;
        out = vlist(m * n);
        k = 0;
        for (j = 0; j < m; j++)
            for (i = 0; i < n; i++)
                out.v[k++] = in[i].v[j];
        return out;
    }
    }
    die("game number: bad calc %d", op);
    return vint(0);
}

static int type_size(int t)
{
    switch (t) {
    case T_U8:
    case T_I8:
    case T_STR:
        return 1;
    case T_U16:
    case T_I16:
        return 2;
    case T_F64:
        return 8;
    default:
        return 4;
    }
}

static val_t read_typed(const pe_t *pe, int t, uint32_t va)
{
    const uint8_t *d = pe->data + pe_offset(pe, va);
    uint32_t u = (uint32_t)d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 24;
    switch (t) {
    case T_U8:
        return vint(d[0]);
    case T_I8:
        return vint((int8_t)d[0]);
    case T_U16:
        return vint((uint16_t)(d[0] | d[1] << 8));
    case T_I16:
        return vint((int16_t)(d[0] | d[1] << 8));
    case T_U32:
        return vint(u);
    case T_I32:
        return vint((int32_t)u);
    case T_F32: {
        float f;
        memcpy(&f, &u, 4);
        return vfloat((double)f);
    }
    case T_F64: {
        double f;
        uint64_t w = u | (uint64_t)((uint32_t)d[4] | (uint32_t)d[5] << 8 | (uint32_t)d[6] << 16 | (uint32_t)d[7] << 24) << 32;
        memcpy(&f, &w, 8);
        return vfloat(f);
    }
    }
    die("game number: bad type %d", t);
    return vint(0);
}

static val_t *extract(const pe_t *pe)
{
    val_t *done = xcalloc((size_t)embed_nrecords, sizeof(val_t));
    int r, k;
    for (r = 0; r < embed_nrecords; r++) {
        const record_t *rec = &embed_records[r];
        val_t v;
        if (rec->kind == K_CALC) {
            val_t *in = xcalloc((size_t)(rec->nfrom ? rec->nfrom : 1), sizeof(val_t));
            for (k = 0; k < rec->nfrom; k++)
                in[k] = done[embed_from[rec->from + k]];
            v = calc(rec->calc, in, rec->nfrom);
        } else if (rec->type == T_STR) {
            const uint8_t *d = pe->data + pe_offset(pe, rec->va);
            v = vstr((const char *)d, rec->count);
        } else if (rec->kind == K_IMM) {
            v = vlist(1);
            v.v[0] = read_typed(pe, rec->type, rec->va + (uint32_t)rec->off);
        } else {
            int stride = rec->stride ? rec->stride : type_size(rec->type);
            v = vlist(rec->count);
            for (k = 0; k < rec->count; k++)
                v.v[k] = read_typed(pe, rec->type, rec->va + (uint32_t)(k * stride));
        }
        for (k = 0; k < rec->nxf; k++)
            v = transform(&embed_xf[rec->xf + k], v);
        if (v.t == V_LIST && rec->count == 1 && rec->kind != K_CALC && v.n == 1)
            v = v.v[0];
        done[r] = v;
    }
    return done;
}

static int64_t *part_values(const part_t *p, const val_t *values, int *count)
{
    val_t v = values[p->rec];
    val_t *list;
    int n, i, k = 0;
    int64_t *flat, *out;
    if (v.t == V_STR || (v.t == V_LIST && v.n && v.v[0].t == V_STR)) {
        int items = v.t == V_STR ? 1 : v.n, total = 0, j;
        for (i = 0; i < items; i++) {
            const val_t *s = v.t == V_STR ? &v : &v.v[i];
            total += p->str >= 0 ? (s->n > p->str ? s->n : p->str) : s->n;
        }
        flat = xmalloc(sizeof(int64_t) * (size_t)(total ? total : 1));
        for (i = 0; i < items; i++) {
            const val_t *s = v.t == V_STR ? &v : &v.v[i];
            if (p->str >= 0 && s->n > p->str && v.t == V_STR)
                die("game number: string longer than %d", p->str);
            for (j = 0; j < s->n; j++)
                flat[k++] = (uint8_t)s->s[j];
            for (; p->str >= 0 && j < p->str; j++)
                flat[k++] = 0;
        }
        n = k;
    } else {
        if (v.t == V_LIST) {
            list = v.v;
            n = v.n;
        } else {
            list = &v;
            n = 1;
        }
        flat = xmalloc(sizeof(int64_t) * (size_t)(n ? n : 1));
        for (i = 0; i < n; i++) {
            if (list[i].t != V_INT)
                die("game number: value is not an integer");
            flat[i] = list[i].i;
        }
    }
    if (p->idxkind == 0) {
        out = flat;
        *count = n;
    } else if (p->idxkind == 1) {
        out = xmalloc(sizeof(int64_t));
        out[0] = flat[p->idx];
        *count = 1;
    } else {
        out = xmalloc(sizeof(int64_t) * (size_t)p->nidx);
        for (i = 0; i < p->nidx; i++)
            out[i] = flat[embed_ints[p->idxs + i]];
        *count = p->nidx;
    }
    if (p->word >= 0)
        for (i = 0; i < *count; i++)
            out[i] = (out[i] >> (16 * p->word)) & 0xFFFF;
    return out;
}

static const int field_size[6] = { 1, 1, 2, 2, 4, 4 };

int exe_record(const pe_t *pe, const char *name, int *out, int max)
{
    val_t *values = extract(pe), v;
    int r, k;
    for (r = 0; r < embed_nrecords; r++) {
        if (strcmp(embed_record_names[r], name))
            continue;
        v = values[r];
        if (v.t == V_INT) {
            if (max < 1)
                die("game number %s does not fit", name);
            out[0] = (int)v.i;
            return 1;
        }
        if (v.t != V_LIST || v.n > max)
            die("game number %s is not a list of up to %d integers", name, max);
        for (k = 0; k < v.n; k++) {
            if (v.v[k].t != V_INT)
                die("game number %s is not a list of integers", name);
            out[k] = (int)v.v[k].i;
        }
        return v.n;
    }
    die("game number %s is missing", name);
    return 0;
}

int exe_table(const pe_t *pe, const char *name, int *out, int max)
{
    val_t *values = extract(pe);
    int t, p, i, k = 0;
    for (t = 0; t < embed_ntables; t++) {
        const etable_t *tb = &embed_tables[t];
        if (strcmp(embed_table_names[t], name))
            continue;
        for (p = 0; p < tb->nparts; p++) {
            int n;
            int64_t *v = part_values(&embed_parts[tb->parts + p], values, &n);
            for (i = 0; i < n; i++, k++) {
                if (k >= max)
                    die("game table %s has more than %d values", name, max);
                out[k] = (int)v[i];
            }
        }
        return k;
    }
    die("game table %s is missing", name);
    return 0;
}

int fill_exe(uint8_t *rom, const uint8_t *exe, size_t exelen, char *err, size_t errlen)
{
    pe_t pe;
    val_t *values;
    int64_t *scalars;
    int s, t, p, i, b;
    if (!pe_open(&pe, exe, exelen)) {
        snprintf(err, errlen, "FixitFelixJr.exe is not a Windows program");
        return 0;
    }
    values = extract(&pe);
    for (s = 0; s < embed_nsections; s++) {
        buf_t blob = { 0 };
        for (t = 0; t < embed_ntables; t++) {
            const etable_t *tb = &embed_tables[t];
            int k = 0;
            if (tb->section != embed_sections[s].section)
                continue;
            for (p = 0; p < tb->nparts; p++) {
                int n;
                int64_t *v = part_values(&embed_parts[tb->parts + p], values, &n);
                for (i = 0; i < n; i++, k++) {
                    int size = field_size[embed_fields[tb->fields + k % tb->nfields]];
                    for (b = 0; b < size; b++)
                        buf_u8(&blob, (int)(v[i] >> (8 * b)));
                }
            }
        }
        if ((int)blob.len != embed_sections[s].length) {
            snprintf(err, errlen, "exe tables are %d bytes, the program expects %d", (int)blob.len, embed_sections[s].length);
            return 0;
        }
        memcpy(rom + embed_sections[s].offset, blob.data, blob.len);
    }
    scalars = xmalloc(sizeof(int64_t) * (size_t)embed_nscalars);
    for (s = 0; s < embed_nscalars; s++) {
        int n;
        int64_t *v = part_values(&embed_scalars[s], values, &n);
        if (n != 1) {
            snprintf(err, errlen, "scalar %d is not one value", s);
            return 0;
        }
        scalars[s] = v[0];
    }
    for (i = 0; i < embed_nrelocs; i++) {
        const reloc_t *r = &embed_relocs[i];
        int64_t v = (scalars[r->scalar] + r->addend) & 0xFFFF;
        if (r->kind == 2) {
            rom[r->offset] = (uint8_t)(v >> 8);
        } else {
            rom[r->offset] = (uint8_t)v;
            if (r->kind == 0)
                rom[r->offset + 1] = (uint8_t)(v >> 8);
        }
    }
    (void)err_buf;
    return 1;
}
