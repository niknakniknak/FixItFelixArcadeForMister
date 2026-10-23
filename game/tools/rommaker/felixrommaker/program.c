/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fr.h"

#include <stdlib.h>
#include <string.h>

#define MAX_RUN 64

static int run_length(const int *row, int n, int i, int step)
{
    int j = i + 1;
    while (j < n && row[j] == row[j - 1] + step && j - i < MAX_RUN)
        j++;
    return j - i;
}

static void encode_row(const int *row, int n, buf_t *out)
{
    int *lit = xmalloc(sizeof(int) * (size_t)(n + 1)), nl = 0, i = 0, k;
    out->data = NULL;
    out->len = 0;
    while (i < n) {
        int same = run_length(row, n, i, 0), seq = run_length(row, n, i, 1), len = seq, op = 0x80;
        if (same >= seq) {
            len = same;
            op = 0xC0;
        }
        if (len >= 2) {
            int at = 0;
            while (at < nl) {
                int c = nl - at > MAX_RUN ? MAX_RUN : nl - at;
                buf_u8(out, c - 1);
                for (k = 0; k < c; k++)
                    buf_u16(out, lit[at + k]);
                at += c;
            }
            nl = 0;
            buf_u8(out, op + len - 1);
            buf_u16(out, row[i]);
            i += len;
        } else {
            lit[nl++] = row[i++];
        }
    }
    {
        int at = 0;
        while (at < nl) {
            int c = nl - at > MAX_RUN ? MAX_RUN : nl - at;
            buf_u8(out, c - 1);
            for (k = 0; k < c; k++)
                buf_u16(out, lit[at + k]);
            at += c;
        }
    }
    free(lit);
}

void gfx_tables(const tables_t *t, const gfx_t *g, int base, buf_t *out)
{
    buf_t parts = { 0 }, msprites = { 0 }, frames = { 0 }, anims = { 0 }, offsets = { 0 }, rows = { 0 };
    buf_t tilemaps = { 0 }, bigparts = { 0 }, bigpics = { 0 }, tail = { 0 };
    hmap_t index;
    int r, i, k, frames_at, count = 0, tm_start, *map_at;
    for (r = 0; r < 217; r++) {
        const meta_t *m = &g->metas[r];
        buf_u16(&msprites, (int)(parts.len / 4));
        buf_u8(&msprites, m->nparts);
        buf_u8(&msprites, m->nover);
        buf_u8(&msprites, t->rects[r].w / 8);
        buf_u8(&msprites, t->rects[r].h / 8);
        for (i = 0; i < m->nover + m->nparts; i++) {
            const spart_t *p = i < m->nover ? &m->over[i] : &m->parts[i - m->nover];
            if (p->code >= 1024)
                die("metasprite cell code %d needs the size byte", p->code);
            buf_u8(&parts, p->x);
            buf_u8(&parts, p->y);
            buf_u8(&parts, p->code & 0xFF);
            buf_u8(&parts, p->code >> 8 | p->pal << 3);
        }
    }
    frames_at = base + (int)parts.len + (int)msprites.len;
    for (i = 0; i < 100; i++)
        for (k = 0; k < t->anims[i].nframes; k++) {
            const frame_t *f = &t->anims[i].frames[k];
            buf_u8(&frames, f->rect);
            buf_u8(&frames, f->mirror < 0 ? 255 : f->mirror);
            buf_u8(&frames, f->dur);
            buf_u16(&frames, f->x);
            buf_u16(&frames, f->y);
        }
    for (i = 0; i < 100; i++) {
        buf_u16(&anims, frames_at + 7 * count);
        buf_u8(&anims, t->anims[i].nframes);
        buf_u8(&anims, t->anims[i].mode);
        count += t->anims[i].nframes;
    }
    hmap_init(&index);
    map_at = xcalloc((size_t)g->nmaps, sizeof(int));
    tm_start = frames_at + (int)frames.len + (int)anims.len;
    for (i = 0; i < g->nmaps; i++) {
        const tmap_t *m = &g->maps[i];
        if (!m->has_map)
            continue;
        map_at[i] = tm_start + (int)offsets.len;
        for (r = 0; r < m->h; r++) {
            buf_t stream;
            int at;
            encode_row(m->entries + r * m->w, m->w, &stream);
            if (!hmap_get(&index, stream.data ? stream.data : (const uint8_t *)"", (int)stream.len, &at)) {
                at = (int)rows.len;
                hmap_put(&index, stream.data ? stream.data : (const uint8_t *)"", (int)stream.len, at);
                if (stream.len)
                    buf_put(&rows, stream.data, stream.len);
            }
            buf_u16(&offsets, at);
            free(stream.data);
        }
    }
    for (i = 0; i < g->nmaps; i++) {
        buf_u8(&tilemaps, g->maps[i].w);
        buf_u8(&tilemaps, g->maps[i].h);
        buf_u16(&tilemaps, g->maps[i].has_map ? map_at[i] : 0);
    }
    for (k = 0; k < 12; k++) {
        buf_u8(&bigpics, (int)(bigparts.len / 5));
        buf_u8(&bigpics, g->nbig[k]);
        for (i = 0; i < g->nbig[k]; i++) {
            const bpart_t *p = &g->big[k][i];
            buf_u8(&bigparts, p->x);
            buf_u8(&bigparts, p->y);
            buf_u8(&bigparts, p->code & 0xFF);
            buf_u8(&bigparts, ((p->code >> 8) & 3) | p->pal << 3);
            buf_u8(&bigparts, p->size);
        }
    }
    for (i = 0; i < 15; i++)
        buf_u8(&tail, g->cycle[i][0] | g->cycle[i][1] << 4);
    for (r = 0; r < 4; r++)
        for (i = 0; i < 5; i++)
            for (k = 0; k < 4; k++) {
                int n;
                for (n = 0; n < g->nmaps && strcmp(g->maps[n].name, g->win[r][i][k]); n++)
                    ;
                buf_u8(&tail, n);
            }
    out->data = NULL;
    out->len = 0;
    buf_put(out, parts.data, parts.len);
    buf_put(out, msprites.data, msprites.len);
    buf_put(out, frames.data, frames.len);
    buf_put(out, anims.data, anims.len);
    buf_put(out, offsets.data, offsets.len);
    buf_put(out, rows.data, rows.len);
    buf_put(out, tilemaps.data, tilemaps.len);
    buf_put(out, bigparts.data, bigparts.len);
    buf_put(out, bigpics.data, bigpics.len);
    buf_put(out, tail.data, tail.len);
}

void demo_table(const buf_t *log, buf_t *out)
{
    size_t i;
    out->data = NULL;
    out->len = 0;
    for (i = 0; i + 20 <= log->len; i += 20) {
        uint8_t v = (uint8_t)(log->data[i + 4] & 0x6F);
        if (out->len && out->data[out->len - 2] == v && out->data[out->len - 1] < 255)
            out->data[out->len - 1]++;
        else {
            buf_u8(out, v);
            buf_u8(out, 1);
        }
    }
}
