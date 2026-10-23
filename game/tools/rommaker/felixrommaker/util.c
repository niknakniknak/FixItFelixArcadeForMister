/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fr.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

void die(const char *fmt, ...)
{
    va_list ap;
    fputs("error: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

void *xcalloc(size_t n, size_t size)
{
    void *p = calloc(n ? n : 1, size ? size : 1);
    if (!p)
        die("out of memory");
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    return memcpy(xmalloc(n), s, n);
}

char *strf(const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;
    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    s = xmalloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

void buf_put(buf_t *b, const void *p, size_t n)
{
    b->data = xrealloc(b->data, b->len + n);
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

void buf_u8(buf_t *b, int v)
{
    uint8_t c = (uint8_t)v;
    buf_put(b, &c, 1);
}

void buf_u16(buf_t *b, int v)
{
    uint8_t c[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    buf_put(b, c, 2);
}

#ifdef _WIN32
static wchar_t *wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = xmalloc(sizeof(wchar_t) * (size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static FILE *open_file(const char *path, const char *mode)
{
    wchar_t *w = wide(path), *m = wide(mode);
    FILE *f = _wfopen(w, m);
    free(w);
    free(m);
    return f;
}
#else
static FILE *open_file(const char *path, const char *mode)
{
    return fopen(path, mode);
}
#endif

int read_file(const char *path, buf_t *out)
{
    FILE *f = open_file(path, "rb");
    uint8_t chunk[65536];
    size_t n;
    if (!f)
        return 0;
    out->data = NULL;
    out->len = 0;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0)
        buf_put(out, chunk, n);
    fclose(f);
    if (!out->data)
        out->data = xmalloc(1);
    return 1;
}

FILE *fr_open_write(const char *path)
{
    return open_file(path, "wb");
}

static int is_sep(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

char *path_join(const char *dir, const char *name)
{
    size_t n = strlen(dir);
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (n && is_sep(dir[n - 1]))
        return strf("%s%s", dir, name);
    return strf("%s%c%s", dir, sep, name);
}

char *path_dir(const char *path)
{
    size_t n = strlen(path);
    while (n && !is_sep(path[n - 1]))
        n--;
    if (!n)
        return xstrdup(".");
    if (n > 1)
        n--;
    {
        char *d = xmalloc(n + 1);
        memcpy(d, path, n);
        d[n] = 0;
        if (n == 2 && d[1] == ':')
            return strf("%s\\", d);
        return d;
    }
}

const char *path_base(const char *path)
{
    const char *b = path;
    for (; *path; path++)
        if (is_sep(*path))
            b = path + 1;
    return b;
}

#ifdef _WIN32
int is_dir(const char *path)
{
    wchar_t *w = wide(path);
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int file_size(const char *path, long long *size)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    wchar_t *w = wide(path);
    int ok = GetFileAttributesExW(w, GetFileExInfoStandard, &d);
    free(w);
    if (!ok)
        return 0;
    *size = ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    return 1;
}
#else
int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int file_size(const char *path, long long *size)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    *size = (long long)st.st_size;
    return 1;
}
#endif

static const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
static const uint8_t md5_r[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

static uint32_t rol(uint32_t x, int c)
{
    return (x << c) | (x >> (32 - c));
}

static void md5_block(md5_ctx *c, const uint8_t *p)
{
    uint32_t m[16], a = c->s[0], b = c->s[1], cc = c->s[2], d = c->s[3];
    int i;
    for (i = 0; i < 16; i++)
        m[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
    for (i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        if (i < 16) {
            f = (b & cc) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & cc);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ cc ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = cc ^ (b | ~d);
            g = (7 * i) & 15;
        }
        f = f + a + md5_k[i] + m[g];
        a = d;
        d = cc;
        cc = b;
        b = b + rol(f, md5_r[i]);
    }
    c->s[0] += a;
    c->s[1] += b;
    c->s[2] += cc;
    c->s[3] += d;
}

void md5_init(md5_ctx *c)
{
    c->s[0] = 0x67452301;
    c->s[1] = 0xefcdab89;
    c->s[2] = 0x98badcfe;
    c->s[3] = 0x10325476;
    c->len = 0;
}

void md5_update(md5_ctx *c, const uint8_t *p, size_t n)
{
    while (n) {
        size_t at = c->len & 63, k = 64 - at < n ? 64 - at : n;
        memcpy(c->buf + at, p, k);
        c->len += k;
        p += k;
        n -= k;
        if (!(c->len & 63))
            md5_block(c, c->buf);
    }
}

void md5_final(md5_ctx *c, uint8_t out[16])
{
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80, zero = 0, len[8];
    int i;
    md5_update(c, &pad, 1);
    while ((c->len & 63) != 56)
        md5_update(c, &zero, 1);
    for (i = 0; i < 8; i++)
        len[i] = (uint8_t)(bits >> (8 * i));
    md5_update(c, len, 8);
    for (i = 0; i < 16; i++)
        out[i] = (uint8_t)(c->s[i / 4] >> (8 * (i % 4)));
}

void md5_hex(const uint8_t *p, size_t n, char out[33])
{
    md5_ctx c;
    uint8_t d[16];
    int i;
    md5_init(&c);
    md5_update(&c, p, n);
    md5_final(&c, d);
    for (i = 0; i < 16; i++)
        sprintf(out + 2 * i, "%02x", d[i]);
}

static void sha1_block(sha1_ctx *c, const uint8_t *p)
{
    uint32_t w[80], a = c->s[0], b = c->s[1], cc = c->s[2], d = c->s[3], e = c->s[4];
    int i;
    for (i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (i = 16; i < 80; i++)
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    for (i = 0; i < 80; i++) {
        uint32_t f, k, t;
        if (i < 20) {
            f = (b & cc) | (~b & d);
            k = 0x5a827999;
        } else if (i < 40) {
            f = b ^ cc ^ d;
            k = 0x6ed9eba1;
        } else if (i < 60) {
            f = (b & cc) | (b & d) | (cc & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ cc ^ d;
            k = 0xca62c1d6;
        }
        t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = cc;
        cc = rol(b, 30);
        b = a;
        a = t;
    }
    c->s[0] += a;
    c->s[1] += b;
    c->s[2] += cc;
    c->s[3] += d;
    c->s[4] += e;
}

void sha1_init(sha1_ctx *c)
{
    c->s[0] = 0x67452301;
    c->s[1] = 0xefcdab89;
    c->s[2] = 0x98badcfe;
    c->s[3] = 0x10325476;
    c->s[4] = 0xc3d2e1f0;
    c->len = 0;
}

void sha1_update(sha1_ctx *c, const uint8_t *p, size_t n)
{
    while (n) {
        size_t at = c->len & 63, k = 64 - at < n ? 64 - at : n;
        memcpy(c->buf + at, p, k);
        c->len += k;
        p += k;
        n -= k;
        if (!(c->len & 63))
            sha1_block(c, c->buf);
    }
}

void sha1_final(sha1_ctx *c, uint8_t out[20])
{
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80, zero = 0, len[8];
    int i;
    sha1_update(c, &pad, 1);
    while ((c->len & 63) != 56)
        sha1_update(c, &zero, 1);
    for (i = 0; i < 8; i++)
        len[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_update(c, len, 8);
    for (i = 0; i < 20; i++)
        out[i] = (uint8_t)(c->s[i / 4] >> (24 - 8 * (i % 4)));
}

static uint64_t fnv(const uint8_t *p, int n)
{
    uint64_t h = 1469598103934665603ULL;
    int i;
    for (i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

typedef struct {
    uint8_t *key;
    int len, val, used;
} slot_t;

void hmap_init(hmap_t *m)
{
    m->cap = 1024;
    m->count = 0;
    m->keys = xcalloc(m->cap, sizeof(slot_t));
    m->klen = NULL;
    m->vals = NULL;
}

static slot_t *find(hmap_t *m, const uint8_t *key, int len)
{
    slot_t *s = (slot_t *)m->keys;
    size_t i = (size_t)fnv(key, len) & (m->cap - 1);
    for (;; i = (i + 1) & (m->cap - 1))
        if (!s[i].used || (s[i].len == len && !memcmp(s[i].key, key, (size_t)len)))
            return &s[i];
}

int hmap_get(hmap_t *m, const uint8_t *key, int len, int *val)
{
    slot_t *s = find(m, key, len);
    if (!s->used)
        return 0;
    *val = s->val;
    return 1;
}

void hmap_put(hmap_t *m, const uint8_t *key, int len, int val)
{
    slot_t *s;
    if ((m->count + 1) * 2 > m->cap) {
        slot_t *old = (slot_t *)m->keys;
        size_t n = m->cap, i;
        m->cap *= 2;
        m->keys = xcalloc(m->cap, sizeof(slot_t));
        for (i = 0; i < n; i++)
            if (old[i].used)
                *find(m, old[i].key, old[i].len) = old[i];
        free(old);
    }
    s = find(m, key, len);
    if (!s->used) {
        s->used = 1;
        s->key = memcpy(xmalloc((size_t)len), key, (size_t)len);
        s->len = len;
        m->count++;
    }
    s->val = val;
}
