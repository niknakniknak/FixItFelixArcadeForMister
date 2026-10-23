/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FR_H
#define FR_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t len;
} buf_t;

void die(const char *fmt, ...);
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *strf(const char *fmt, ...);
void buf_put(buf_t *b, const void *p, size_t n);
void buf_u8(buf_t *b, int v);
void buf_u16(buf_t *b, int v);
int read_file(const char *path, buf_t *out);
char *path_join(const char *dir, const char *name);
char *path_dir(const char *path);
const char *path_base(const char *path);
int is_dir(const char *path);
int file_size(const char *path, long long *size);
FILE *fr_open_write(const char *path);

void md5_hex(const uint8_t *p, size_t n, char out[33]);
typedef struct {
    uint32_t s[4];
    uint64_t len;
    uint8_t buf[64];
} md5_ctx;
void md5_init(md5_ctx *c);
void md5_update(md5_ctx *c, const uint8_t *p, size_t n);
void md5_final(md5_ctx *c, uint8_t out[16]);
typedef struct {
    uint32_t s[5];
    uint64_t len;
    uint8_t buf[64];
} sha1_ctx;
void sha1_init(sha1_ctx *c);
void sha1_update(sha1_ctx *c, const uint8_t *p, size_t n);
void sha1_final(sha1_ctx *c, uint8_t out[20]);

typedef struct {
    uint8_t *keys;
    int *klen;
    int *vals;
    size_t cap, count;
} hmap_t;
void hmap_init(hmap_t *m);
int hmap_get(hmap_t *m, const uint8_t *key, int len, int *val);
void hmap_put(hmap_t *m, const uint8_t *key, int len, int val);

typedef struct {
    int x, y, w, h;
} rect_t;
typedef struct {
    int rect, mirror, dur, x, y;
} frame_t;
typedef struct {
    int mode, nframes;
    frame_t *frames;
} anim_t;
typedef struct {
    rect_t rects[217];
    rect_t tiles[27];
    anim_t anims[100];
} tables_t;

typedef struct {
    const uint8_t *data;
    size_t len;
    int nsec;
    uint32_t vaddr[32], rsize[32], rptr[32];
} pe_t;
int pe_open(pe_t *pe, const uint8_t *data, size_t len);
long pe_offset(const pe_t *pe, uint32_t va);
void read_tables(const pe_t *pe, tables_t *t);

typedef struct {
    int width, height;
    uint8_t *pixels;
    int rgb[256][3];
} sheet_t;
int read_bmp(const buf_t *b, sheet_t *s);
void sheet_first(const sheet_t *s, int index, int *x, int *y);

typedef struct {
    int yellow[3], clothes[3];
} felix_variant_t;

typedef struct {
    int x, y, code, pal;
} spart_t;
typedef struct {
    int nover, nparts;
    spart_t *over, *parts;
} meta_t;
typedef struct {
    char *name;
    int w, h, has_map, n;
    int *entries;
} tmap_t;
typedef struct {
    int x, y, code, pal, size;
} bpart_t;
typedef struct {
    buf_t tiles, sprites, pal;
    meta_t metas[217];
    int nmaps;
    tmap_t *maps;
    int nbig[12];
    bpart_t *big[12];
    char *win[4][5][4];
    int cycle[15][2];
} gfx_t;
void gfx_exe(const pe_t *pe);
int gfx_index(const char *which, int k);
void build_gfx(const sheet_t *s, const tables_t *t, int hammer[4][3], felix_variant_t felix[4], gfx_t *g);

void gfx_tables(const tables_t *t, const gfx_t *g, int base, buf_t *out);
void demo_table(const buf_t *log, buf_t *out);

typedef struct {
    const char *win, *atg;
    const uint8_t *md;
    size_t mdlen;
} sound_in_t;
int sample_rom(const sound_in_t *in, buf_t *out, char sha1hex[41]);
int read_wav16(const char *path, int64_t **out, size_t *n);

int fill_exe(uint8_t *rom, const uint8_t *exe, size_t exelen, char *err, size_t errlen);
int exe_table(const pe_t *pe, const char *name, int *out, int max);
int exe_record(const pe_t *pe, const char *name, int *out, int max);

#endif
