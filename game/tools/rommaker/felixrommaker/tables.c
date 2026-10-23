/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fr.h"

#include <stdlib.h>
#include <string.h>

#define IMAGE_BASE 0x400000u
#define RECT_TABLE 0x418458u
#define TILE_TABLE 0x418348u
#define ANIM_TABLE 0x419800u

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

int pe_open(pe_t *pe, const uint8_t *data, size_t len)
{
    uint32_t off;
    int i, count, opt;
    if (len < 0x40)
        return 0;
    off = le32(data + 0x3C);
    if ((size_t)off + 24 > len)
        return 0;
    count = le16(data + off + 6);
    opt = le16(data + off + 20);
    if (count > 32)
        return 0;
    pe->data = data;
    pe->len = len;
    pe->nsec = count;
    for (i = 0; i < count; i++) {
        const uint8_t *s = data + off + 24 + opt + 40 * i;
        pe->vaddr[i] = le32(s + 12);
        pe->rsize[i] = le32(s + 16);
        pe->rptr[i] = le32(s + 20);
    }
    return 1;
}

long pe_offset(const pe_t *pe, uint32_t va)
{
    uint32_t rva = va - IMAGE_BASE;
    int i;
    for (i = 0; i < pe->nsec; i++)
        if (pe->vaddr[i] <= rva && rva < pe->vaddr[i] + pe->rsize[i])
            return (long)(pe->rptr[i] + rva - pe->vaddr[i]);
    die("VA %#x not in file data", va);
    return -1;
}

static int pe_short(const pe_t *pe, uint32_t va)
{
    return (int16_t)le16(pe->data + pe_offset(pe, va));
}

static void read_rects(const pe_t *pe, uint32_t table, int count, rect_t *out)
{
    int k;
    for (k = 0; k < count; k++) {
        uint32_t va = table + 10u * (uint32_t)k;
        if (pe_short(pe, va) != k)
            die("rect table %#x entry %d has index %d", table, k, pe_short(pe, va));
        out[k].x = pe_short(pe, va + 2) * 8;
        out[k].y = pe_short(pe, va + 4) * 8;
        out[k].w = pe_short(pe, va + 6) * 8;
        out[k].h = pe_short(pe, va + 8) * 8;
    }
}

void read_tables(const pe_t *pe, tables_t *t)
{
    int n, f;
    read_rects(pe, RECT_TABLE, 217, t->rects);
    read_rects(pe, TILE_TABLE, 27, t->tiles);
    for (n = 0; n < 100; n++) {
        uint32_t va = ANIM_TABLE + 12u * (uint32_t)n;
        int count = pe_short(pe, va + 2);
        uint32_t fva = le32(pe->data + pe_offset(pe, va + 8));
        anim_t *a = &t->anims[n];
        a->mode = pe_short(pe, va + 4);
        a->nframes = count;
        a->frames = xcalloc((size_t)(count > 0 ? count : 1), sizeof(frame_t));
        for (f = 0; f < count; f++) {
            uint32_t v = fva + 10u * (uint32_t)f;
            frame_t fr;
            fr.rect = pe_short(pe, v);
            fr.mirror = pe_short(pe, v + 2);
            fr.dur = pe_short(pe, v + 4);
            fr.x = pe_short(pe, v + 6);
            fr.y = pe_short(pe, v + 8);
            if ((n == 36 || n == 38) && fr.mirror != -1 && fr.y == 0 && f > 0) {
                frame_t s;
                s.rect = fr.rect;
                s.mirror = -1;
                s.dur = fr.mirror;
                s.x = fr.dur;
                s.y = fr.x;
                fr = s;
            }
            a->frames[f] = fr;
        }
    }
}

int read_bmp(const buf_t *b, sheet_t *s)
{
    const uint8_t *d = b->data;
    int offset, width, height, bits, compression, used, h, y, stride;
    if (b->len < 54 || d[0] != 'B' || d[1] != 'M')
        return 0;
    offset = (int)le32(d + 10);
    width = (int32_t)le32(d + 18);
    height = (int32_t)le32(d + 22);
    bits = le16(d + 28);
    compression = (int)le32(d + 30);
    used = (int)le32(d + 46);
    if (bits != 8 || compression != 0)
        return 0;
    if (!used)
        used = 256;
    memset(s->rgb, 0, sizeof s->rgb);
    for (y = 0; y < used; y++) {
        s->rgb[y][0] = d[56 + 4 * y];
        s->rgb[y][1] = d[55 + 4 * y];
        s->rgb[y][2] = d[54 + 4 * y];
    }
    h = height < 0 ? -height : height;
    s->width = width;
    s->height = h;
    stride = (width + 3) & ~3;
    if ((size_t)offset + (size_t)stride * (size_t)h > b->len)
        return 0;
    s->pixels = xmalloc((size_t)width * (size_t)h);
    for (y = 0; y < h; y++) {
        int row = height > 0 ? h - 1 - y : y;
        memcpy(s->pixels + (size_t)y * (size_t)width, d + offset + (size_t)row * (size_t)stride, (size_t)width);
    }
    return 1;
}

void sheet_first(const sheet_t *s, int index, int *x, int *y)
{
    size_t i, n = (size_t)s->width * (size_t)s->height;
    for (i = 0; i < n; i++)
        if (s->pixels[i] == index) {
            *x = (int)(i % (size_t)s->width);
            *y = (int)(i / (size_t)s->width);
            return;
        }
    die("colour index %d not in texture2.bmp", index);
}

int read_wav16(const char *path, int64_t **out, size_t *n)
{
    buf_t b;
    size_t pos = 12;
    int width = 0, channels = 0;
    if (!read_file(path, &b))
        die("open %s: file not found", path);
    if (b.len < 12 || memcmp(b.data, "RIFF", 4) || memcmp(b.data + 8, "WAVE", 4))
        die("%s is not a WAV file", path);
    while (pos + 8 <= b.len) {
        const uint8_t *id = b.data + pos;
        size_t size = le32(b.data + pos + 4), avail = b.len - pos - 8;
        const uint8_t *body = b.data + pos + 8;
        if (size < avail)
            avail = size;
        if (!memcmp(id, "fmt ", 4)) {
            channels = le16(body + 2);
            width = le16(body + 14) / 8;
        } else if (!memcmp(id, "data", 4)) {
            size_t i, k = 0;
            if (channels != 1)
                die("%s is not mono", path);
            if (width == 1) {
                *out = xmalloc(sizeof(int64_t) * (avail ? avail : 1));
                for (i = 0; i < avail; i++)
                    (*out)[k++] = ((int64_t)body[i] - 128) * 256;
            } else {
                *out = xmalloc(sizeof(int64_t) * (avail / 2 + 1));
                for (i = 0; i + 2 <= avail; i += 2)
                    (*out)[k++] = (int16_t)le16(body + i);
            }
            *n = k;
            return 1;
        }
        pos += 8 + size + (size & 1);
    }
    die("%s has no data chunk", path);
    return 0;
}
