/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_TILE_ENTRY 22
#define FONT_COLOURS 7
#define FONT_GLYPHS 64
#define MAX_TILES 2048
#define MAX_CELLS 2048
#define PALETTES 32
#define PENS 16
#define CYCLE_VARIANTS 4
#define BUILDING_COL 1
#define WINDOW_VARIANTS 4
#define GROUND_REPEAT 16
#define GROUND_LEFT_END 32
#define GROUND_RIGHT_START 208
#define CUT_DOOR_X 96
#define LIVES_H 8
#define LIVES_MAX 8
#define SCREEN_W 240
#define NBIG 12
#define CYCLE_KEY 0x10000

static const char *tile_names[27] = {
    "BAND", "GREEN_BAR", "DOOR0", "DOOR1", "DOOR2", "DOOR3", "ARCH", "ARCH_BROKEN", "LOGO", "ROOF", "FLOOR_A",
    "FLOOR_B", "GROUND_A", "GROUND_B", "FRAME3", "FRAME4", "FRAME0", "LIVES_HEAD", "COPYRIGHT", "HIGH_SCORE",
    "ONE_UP", "PRESS_START_RED", "FONT", "PRESS_START", "GAME_OVER", "CREDITS", "BLACK_BAR",
};
static const int super_anims[8] = { 4, 9, 10, 11, 12, 13, 14, 16 };
static const int big_sizes[3] = { 64, 32, 16 };
static const int picture_ids[11] = { 181, 182, 183, 184, 199, 200, 201, 202, 209, 210, 212 };
static const char *picture_names[11] = {
    "MEDAL0", "MEDAL1", "MEDAL2", "MEDAL3", "BUBBLE_STUMP", "DOOR_CLOSED", "DOOR_OPEN", "SCENERY", "DUMP",
    "BUBBLE_WRECK", "BUBBLE_FIX",
};
static const char *section_maps[4] = { "GROUND_A", "GROUND_B", "FLOOR_A", "FLOOR_B" };
static int slot_kinds[4][5];
static const int kind_count[5] = { 1, 4, 2, 1, 1 };
static const char *kind_pictures[5][4] = {
    { "FRAME0" }, { "DOOR0", "DOOR1", "DOOR2", "DOOR3" }, { "ARCH", "ARCH_BROKEN" }, { "FRAME3" }, { "FRAME4" },
};
static int slot_tx[4][5];
static int slot_ty[4][5];
static int kind_shift[5];
static int lives_x[LIVES_MAX], lives_n, lives_left, lives_w;
static int transparent, drawn_black, clothing[4], hammer_index[3];
static const int service_ramps[4][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1 } };

static const sheet_t *S;
static const tables_t *T;

static int is_clothing(int v)
{
    return v == clothing[0] || v == clothing[1] || v == clothing[2] || v == clothing[3];
}

static int is_hammer(int v)
{
    return v == hammer_index[0] || v == hammer_index[1] || v == hammer_index[2];
}

static void rgb555(const int c[3], int out[3])
{
    int i;
    for (i = 0; i < 3; i++)
        out[i] = (c[i] * 31 + 127) / 255;
}

static int word555(const int c[3])
{
    return c[0] << 10 | c[1] << 5 | c[2];
}

static int rgb_key(const int c[3])
{
    int r[3];
    rgb555(c, r);
    return r[0] << 10 | r[1] << 5 | r[2];
}

static void key_str(int key, char *out)
{
    if (key & CYCLE_KEY)
        sprintf(out, "('c', %d)", key & 0xFF);
    else
        sprintf(out, "(%d, %d, %d)", key >> 10, (key >> 5) & 31, key & 31);
}

static int key_cmp(const void *a, const void *b)
{
    char sa[32], sb[32];
    key_str(*(const int *)a, sa);
    key_str(*(const int *)b, sb);
    return strcmp(sa, sb);
}

static int key_colour(int key)
{
    if (key & CYCLE_KEY) {
        int c[3];
        rgb555(S->rgb[key & 0xFF], c);
        return word555(c);
    }
    return key;
}

static int key_of(int index, int cycle)
{
    if (index == transparent)
        return -1;
    if (index == drawn_black)
        return 0;
    if (cycle && (index == hammer_index[0] || index == clothing[0]))
        return CYCLE_KEY | clothing[0];
    if (cycle && (index == clothing[1] || index == clothing[2] || index == clothing[3]))
        return CYCLE_KEY | index;
    return rgb_key(S->rgb[index]);
}

typedef struct {
    int n;
    int k[256];
} keyset_t;

static void keys_of(const uint8_t *b, int len, int cycle, keyset_t *ks)
{
    uint8_t seen[256];
    int i, j;
    memset(seen, 0, sizeof seen);
    ks->n = 0;
    for (i = 0; i < len; i++) {
        int key;
        if (seen[b[i]])
            continue;
        seen[b[i]] = 1;
        key = key_of(b[i], cycle);
        if (key < 0)
            continue;
        for (j = 0; j < ks->n && ks->k[j] != key; j++)
            ;
        if (j == ks->n)
            ks->k[ks->n++] = key;
    }
}

static int keyset_has(const keyset_t *ks, int key)
{
    int i;
    for (i = 0; i < ks->n; i++)
        if (ks->k[i] == key)
            return 1;
    return 0;
}

static void keyset_add(keyset_t *ks, const keyset_t *other)
{
    int i;
    for (i = 0; i < other->n; i++)
        if (!keyset_has(ks, other->k[i]))
            ks->k[ks->n++] = other->k[i];
}

typedef struct {
    int n;
    int keys[PENS];
} palette_t;

typedef struct {
    int n, cap;
    palette_t *p;
} pool_t;

static int pool_seed(pool_t *pool, const int *keys, int n)
{
    palette_t *p;
    if (pool->n == pool->cap) {
        pool->cap = pool->cap ? pool->cap * 2 : 16;
        pool->p = xrealloc(pool->p, sizeof(palette_t) * (size_t)pool->cap);
    }
    p = &pool->p[pool->n];
    p->n = n;
    memcpy(p->keys, keys, sizeof(int) * (size_t)n);
    return pool->n++;
}

static int pal_has(const palette_t *p, int key)
{
    int i;
    for (i = 0; i < p->n; i++)
        if (p->keys[i] == key)
            return 1;
    return 0;
}

static int pool_place(pool_t *pool, const keyset_t *ks)
{
    int best = -1, best_shared = 0, best_union = 0, n, i;
    for (n = 0; n < pool->n; n++) {
        const palette_t *p = &pool->p[n];
        int shared = 0, uni = p->n;
        for (i = 0; i < ks->n; i++) {
            if (pal_has(p, ks->k[i]))
                shared++;
            else
                uni++;
        }
        if (uni > PENS - 1)
            continue;
        if (best < 0 || shared > best_shared || (shared == best_shared && uni < best_union)) {
            best = n;
            best_shared = shared;
            best_union = uni;
        }
    }
    if (best < 0) {
        int sorted[256];
        memcpy(sorted, ks->k, sizeof(int) * (size_t)ks->n);
        qsort(sorted, (size_t)ks->n, sizeof(int), key_cmp);
        if (ks->n > PENS)
            die("palette with %d colours", ks->n);
        return pool_seed(pool, sorted, ks->n);
    }
    {
        palette_t *p = &pool->p[best];
        int extra[256], ne = 0;
        for (i = 0; i < ks->n; i++)
            if (!pal_has(p, ks->k[i]))
                extra[ne++] = ks->k[i];
        qsort(extra, (size_t)ne, sizeof(int), key_cmp);
        for (i = 0; i < ne; i++)
            p->keys[p->n++] = extra[i];
    }
    return best;
}

static uint8_t *pool_pens(const pool_t *pool, int number, const uint8_t *b, int len, int cycle)
{
    const palette_t *p = &pool->p[number];
    uint8_t *out = xcalloc((size_t)len, 1);
    int i, j;
    for (i = 0; i < len; i++) {
        int key = key_of(b[i], cycle);
        if (key < 0)
            continue;
        for (j = 0; j < p->n; j++)
            if (p->keys[j] == key) {
                out[i] = (uint8_t)(j + 1);
                break;
            }
    }
    return out;
}

typedef struct {
    int size, limit, n, cap;
    uint8_t **blocks;
    hmap_t codes, runs;
} graphic_t;

static void graphic_append(graphic_t *g, uint8_t *b)
{
    if (g->n == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 1024;
        g->blocks = xrealloc(g->blocks, sizeof(uint8_t *) * (size_t)g->cap);
    }
    g->blocks[g->n++] = b;
}

static void graphic_init(graphic_t *g, int size, int limit)
{
    uint8_t *zero = xcalloc((size_t)(size * size), 1);
    memset(g, 0, sizeof *g);
    g->size = size;
    g->limit = limit;
    hmap_init(&g->codes);
    hmap_init(&g->runs);
    hmap_put(&g->codes, zero, size * size, 0);
    graphic_append(g, zero);
}

static int graphic_code(graphic_t *g, uint8_t *p)
{
    int v, len = g->size * g->size;
    if (hmap_get(&g->codes, p, len, &v))
        return v;
    hmap_put(&g->codes, p, len, g->n);
    graphic_append(g, p);
    return g->n - 1;
}

static int graphic_run(graphic_t *g, uint8_t **cells, int n)
{
    int len = g->size * g->size, i, v;
    uint8_t *key = xmalloc((size_t)(len * n));
    for (i = 0; i < n; i++)
        memcpy(key + i * len, cells[i], (size_t)len);
    if (hmap_get(&g->runs, key, len * n, &v)) {
        free(key);
        return v;
    }
    v = g->n;
    hmap_put(&g->runs, key, len * n, v);
    free(key);
    for (i = 0; i < n; i++) {
        int dummy;
        if (!hmap_get(&g->codes, cells[i], len, &dummy))
            hmap_put(&g->codes, cells[i], len, g->n);
        graphic_append(g, cells[i]);
    }
    return v;
}

static void graphic_encode(const graphic_t *g, buf_t *out)
{
    int n = g->size, b, col, y;
    size_t total = (size_t)g->limit * (size_t)(n * n / 2);
    if (g->n > g->limit)
        die("%d blocks exceed %d", g->n, g->limit);
    out->data = xcalloc(total, 1);
    out->len = 0;
    for (b = 0; b < g->n; b++) {
        const uint8_t *p = g->blocks[b];
        for (col = 0; col < n; col++)
            for (y = 0; y < n; y += 2)
                out->data[out->len++] = (uint8_t)(p[y * n + col] | p[(y + 1) * n + col] << 4);
    }
    out->len = total;
}

typedef struct {
    int x, y, w, h;
    uint8_t *pix;
    int opaque;
} pic_t;

static int sidx(int x, int y)
{
    return S->pixels[(size_t)y * (size_t)S->width + (size_t)x];
}

static int midx(int masked, int x, int y)
{
    int i = sidx(x, y);
    if (masked && !is_hammer(i))
        return transparent;
    return i;
}

static uint8_t *sheet_block(const pic_t *r, int dx, int dy, int size, int masked)
{
    uint8_t *out = xmalloc((size_t)(size * size));
    int x, y;
    memset(out, transparent, (size_t)(size * size));
    for (y = dy; y < dy + size; y++) {
        int lo, hi;
        if (y < 0 || y >= r->h)
            continue;
        lo = dx < 0 ? 0 : dx;
        hi = dx + size > r->w ? r->w : dx + size;
        for (x = lo; x < hi; x++)
            out[(y - dy) * size + (x - dx)] = (uint8_t)midx(masked, r->x + x, r->y + y);
    }
    return out;
}

static int is_empty(const uint8_t *b, int len)
{
    int i;
    for (i = 0; i < len; i++)
        if (b[i] != transparent)
            return 0;
    return 1;
}

static uint8_t *opaque_block(const uint8_t *b, int len)
{
    uint8_t *out = xmalloc((size_t)len);
    int i;
    for (i = 0; i < len; i++)
        out[i] = b[i] == transparent ? drawn_black : b[i];
    return out;
}

typedef struct {
    int n, cap;
    int *x, *y;
} posl_t;

static void pos_add(posl_t *l, int x, int y)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->x = xrealloc(l->x, sizeof(int) * (size_t)l->cap);
        l->y = xrealloc(l->y, sizeof(int) * (size_t)l->cap);
    }
    l->x[l->n] = x;
    l->y[l->n++] = y;
}

static void grid_positions(const pic_t *r, int size, int masked, posl_t *out)
{
    int x, y;
    for (y = 0; y < r->h; y += size)
        for (x = 0; x < r->w; x += size) {
            uint8_t *b = sheet_block(r, x, y, size, masked);
            if (!is_empty(b, size * size))
                pos_add(out, x, y);
            free(b);
        }
}

static void strip_positions(const pic_t *r, int size, int masked, posl_t *out)
{
    int first = -1, last = -1, x, y, xx;
    int *rows = xmalloc(sizeof(int) * (size_t)(r->h + 1));
    for (x = 0; x < r->w; x++)
        for (y = 0; y < r->h; y++)
            if (midx(masked, r->x + x, r->y + y) != transparent) {
                if (first < 0)
                    first = x;
                last = x;
                break;
            }
    if (first >= 0) {
        for (x = first; x < last + 1; x += size) {
            int nr = 0, hi = x + size > r->w ? r->w : x + size, i, k;
            for (y = 0; y < r->h; y++)
                for (xx = x; xx < hi; xx++)
                    if (midx(masked, r->x + xx, r->y + y) != transparent) {
                        rows[nr++] = y;
                        break;
                    }
            i = 0;
            while (i < nr) {
                int top = rows[i];
                pos_add(out, x, top);
                k = i;
                while (k < nr && rows[k] < top + size)
                    k++;
                i = k;
            }
        }
    }
    free(rows);
}

static void cut_positions(const pic_t *r, int size, int masked, posl_t *out)
{
    posl_t grid = { 0 }, strips = { 0 };
    grid_positions(r, size, masked, &grid);
    strip_positions(r, size, masked, &strips);
    *out = strips.n < grid.n ? strips : grid;
}

static pic_t as_pic(rect_t r)
{
    pic_t p;
    memset(&p, 0, sizeof p);
    p.x = r.x;
    p.y = r.y;
    p.w = r.w;
    p.h = r.h;
    return p;
}

static void anim_rects(const int *numbers, int count, uint8_t *set)
{
    int i, f;
    for (i = 0; i < count; i++) {
        const anim_t *a = &T->anims[numbers[i]];
        for (f = 0; f < a->nframes; f++) {
            set[a->frames[f].rect] = 1;
            if (a->frames[f].mirror >= 0)
                set[a->frames[f].mirror] = 1;
        }
    }
}

typedef struct {
    int x, y, cycle;
    uint8_t *block;
} cell_t;

typedef struct {
    int n;
    cell_t *c;
} cells_t;

static void cells_add(cells_t *l, int x, int y, uint8_t *b, int cycle)
{
    l->c = xrealloc(l->c, sizeof(cell_t) * (size_t)(l->n + 1));
    l->c[l->n].x = x;
    l->c[l->n].y = y;
    l->c[l->n].block = b;
    l->c[l->n++].cycle = cycle;
}

typedef struct {
    uint8_t used[256], super[256];
    pool_t cycle_pool, pool;
    int hammer_seed[4];
    cells_t parts[217], overlays[217];
    hmap_t pattern_index;
    int npat, cappat;
    uint8_t **pat_block;
    int *pat_cycle, *pat_number;
    int cycle_groups, general_base;
} sprites_t;

static uint8_t *pattern_key(const uint8_t *b, int cycle, int *len)
{
    uint8_t *k = xmalloc(257);
    memcpy(k, b, 256);
    k[256] = (uint8_t)cycle;
    *len = 257;
    return k;
}

static int pattern_find(sprites_t *sp, const uint8_t *b, int cycle)
{
    int len, v;
    uint8_t *k = pattern_key(b, cycle, &len);
    if (!hmap_get(&sp->pattern_index, k, len, &v))
        v = -1;
    free(k);
    return v;
}

static void sprites_init(sprites_t *sp, int hammer[4][3])
{
    int all[100], i;
    memset(sp, 0, sizeof *sp);
    for (i = 0; i < 100; i++)
        all[i] = i;
    anim_rects(all, 100, sp->used);
    for (i = 0; i < 11; i++)
        sp->used[picture_ids[i]] = 0;
    for (i = 0; i < NBIG; i++)
        sp->used[187 + i] = 0;
    anim_rects(super_anims, 8, sp->super);
    for (i = 0; i < 4; i++) {
        int key = rgb_key(hammer[i]);
        sp->hammer_seed[i] = pool_seed(&sp->pool, &key, 1);
    }
    hmap_init(&sp->pattern_index);
}

static int is_cycle(const sprites_t *sp, int r, const uint8_t *b)
{
    int i;
    if (!sp->super[r])
        return 0;
    for (i = 0; i < 256; i++)
        if (is_clothing(b[i]))
            return 1;
    return 0;
}

static void sprites_collect(sprites_t *sp)
{
    int r, i;
    for (r = 0; r < 217; r++) {
        pic_t rc;
        posl_t pos;
        if (!sp->used[r])
            continue;
        rc = as_pic(T->rects[r]);
        cut_positions(&rc, 16, 0, &pos);
        for (i = 0; i < pos.n; i++) {
            uint8_t *b = sheet_block(&rc, pos.x[i], pos.y[i], 16, 0);
            int cycle = is_cycle(sp, r, b);
            cells_add(&sp->parts[r], pos.x[i], pos.y[i], b, cycle);
            if (pattern_find(sp, b, cycle) < 0) {
                int len;
                uint8_t *k = pattern_key(b, cycle, &len);
                if (sp->npat == sp->cappat) {
                    sp->cappat = sp->cappat ? sp->cappat * 2 : 1024;
                    sp->pat_block = xrealloc(sp->pat_block, sizeof(uint8_t *) * (size_t)sp->cappat);
                    sp->pat_cycle = xrealloc(sp->pat_cycle, sizeof(int) * (size_t)sp->cappat);
                    sp->pat_number = xrealloc(sp->pat_number, sizeof(int) * (size_t)sp->cappat);
                }
                hmap_put(&sp->pattern_index, k, len, sp->npat);
                free(k);
                sp->pat_block[sp->npat] = b;
                sp->pat_cycle[sp->npat] = cycle;
                sp->pat_number[sp->npat++] = -1;
            }
        }
        if (sp->super[r]) {
            posl_t op;
            cut_positions(&rc, 16, 1, &op);
            for (i = 0; i < op.n; i++)
                cells_add(&sp->overlays[r], op.x[i], op.y[i], sheet_block(&rc, op.x[i], op.y[i], 16, 1), 0);
        }
    }
}

typedef struct {
    int size, order;
} sortkey_t;

static int sortkey_cmp(const void *a, const void *b)
{
    const sortkey_t *x = a, *y = b;
    if (x->size != y->size)
        return y->size - x->size;
    return x->order - y->order;
}

static void sprites_pack(sprites_t *sp)
{
    sortkey_t *order = xmalloc(sizeof(sortkey_t) * (size_t)(sp->npat ? sp->npat : 1));
    int i;
    for (i = 0; i < sp->npat; i++) {
        keyset_t ks;
        keys_of(sp->pat_block[i], 256, sp->pat_cycle[i], &ks);
        order[i].size = ks.n;
        order[i].order = i;
    }
    qsort(order, (size_t)sp->npat, sizeof(sortkey_t), sortkey_cmp);
    for (i = 0; i < sp->npat; i++) {
        int p = order[i].order;
        keyset_t ks;
        keys_of(sp->pat_block[p], 256, sp->pat_cycle[p], &ks);
        if (ks.n > PENS - 1)
            die("sprite cell with too many colours");
        sp->pat_number[p] = pool_place(sp->pat_cycle[p] ? &sp->cycle_pool : &sp->pool, &ks);
    }
    sp->cycle_groups = sp->cycle_pool.n;
    sp->general_base = sp->cycle_groups * (1 + CYCLE_VARIANTS);
    if (sp->general_base + sp->pool.n > PALETTES)
        die("too many sprite palettes");
    free(order);
}

static void sprites_build(sprites_t *sp, graphic_t *cells, meta_t *metas)
{
    int r, i, j;
    for (r = 0; r < 217; r++) {
        meta_t *m = &metas[r];
        m->nover = sp->overlays[r].n;
        m->over = xcalloc((size_t)(m->nover ? m->nover : 1), sizeof(spart_t));
        for (i = 0; i < m->nover; i++) {
            cell_t *o = &sp->overlays[r].c[i];
            uint8_t *p = xcalloc(256, 1);
            for (j = 0; j < 256; j++)
                if (o->block[j] != transparent)
                    p[j] = 1;
            m->over[i].x = o->x;
            m->over[i].y = o->y;
            m->over[i].code = graphic_code(cells, p);
            m->over[i].pal = sp->general_base + sp->hammer_seed[0];
        }
        m->nparts = sp->parts[r].n;
        m->parts = xcalloc((size_t)(m->nparts ? m->nparts : 1), sizeof(spart_t));
        for (i = 0; i < m->nparts; i++) {
            cell_t *c = &sp->parts[r].c[i];
            int pat = pattern_find(sp, c->block, c->cycle), n = sp->pat_number[pat];
            uint8_t *p = pool_pens(c->cycle ? &sp->cycle_pool : &sp->pool, n, c->block, 256, c->cycle);
            m->parts[i].x = c->x;
            m->parts[i].y = c->y;
            m->parts[i].code = graphic_code(cells, p);
            m->parts[i].pal = c->cycle ? n * (1 + CYCLE_VARIANTS) : sp->general_base + n;
        }
    }
}

static void palette_row(const palette_t *p, const felix_variant_t *felix, int *row)
{
    int i;
    row[0] = 0;
    for (i = 0; i < p->n; i++) {
        int key = p->keys[i], c[3];
        if (felix && (key & CYCLE_KEY)) {
            rgb555((key & 0xFF) == clothing[0] ? felix->yellow : felix->clothes, c);
            row[i + 1] = word555(c);
        } else {
            row[i + 1] = key_colour(key);
        }
    }
    for (i = p->n + 1; i < PENS; i++)
        row[i] = -1;
}

typedef struct {
    int n;
    int rows[PALETTES][PENS];
} rows_t;

static void rows_add(rows_t *rs, const int *row)
{
    if (rs->n >= PALETTES)
        die("too many palettes");
    memcpy(rs->rows[rs->n++], row, sizeof(int) * PENS);
}

static void sprite_palettes(const sprites_t *sp, felix_variant_t *felix, rows_t *rs)
{
    int i, v, row[PENS];
    for (i = 0; i < sp->cycle_pool.n; i++) {
        palette_row(&sp->cycle_pool.p[i], NULL, row);
        rows_add(rs, row);
        for (v = 0; v < CYCLE_VARIANTS; v++) {
            palette_row(&sp->cycle_pool.p[i], &felix[v], row);
            rows_add(rs, row);
        }
    }
    for (i = 0; i < sp->pool.n; i++) {
        palette_row(&sp->pool.p[i], NULL, row);
        rows_add(rs, row);
    }
}

typedef struct {
    char *name;
    pic_t r;
} named_t;

typedef struct {
    int n;
    named_t *p;
} pics_t;

static void pics_add(pics_t *l, const char *name, pic_t r)
{
    l->p = xrealloc(l->p, sizeof(named_t) * (size_t)(l->n + 1));
    l->p[l->n].name = xstrdup(name);
    l->p[l->n++].r = r;
}

typedef struct {
    pool_t pool;
    graphic_t tiles;
    int font_rows[FONT_COLOURS], font_x, font_pal[FONT_COLOURS];
    char *win[4][5][4];
    rows_t service;
    int service_pal;
} tilesb_t;

static rect_t named(const char *name)
{
    int i;
    for (i = 0; i < 27; i++)
        if (!strcmp(tile_names[i], name))
            return T->tiles[i];
    die("no tile %s", name);
    return T->tiles[0];
}

static int font_colour(tilesb_t *tb, int c)
{
    int y = tb->font_rows[c], x, dy, found = -1;
    for (x = tb->font_x; x < tb->font_x + 512; x++)
        for (dy = 0; dy < 8; dy++) {
            int i = sidx(x, y + dy);
            if (i == transparent)
                continue;
            if (found >= 0 && found != i)
                die("font colour %d uses more than one index", c);
            found = i;
        }
    if (found < 0)
        die("font colour %d uses no index", c);
    return key_of(found, 0);
}

static void tiles_init(tilesb_t *tb)
{
    rect_t font = T->tiles[FONT_TILE_ENTRY];
    int c;
    memset(tb, 0, sizeof *tb);
    graphic_init(&tb->tiles, 8, MAX_TILES);
    for (c = 0; c < FONT_COLOURS; c++)
        tb->font_rows[c] = font.y + (FONT_COLOURS - 1 - c) * 8;
    tb->font_x = font.x;
    for (c = 0; c < FONT_COLOURS; c++) {
        int keys[2];
        keys[0] = font_colour(tb, c);
        keys[1] = key_of(drawn_black, 0);
        tb->font_pal[c] = pool_seed(&tb->pool, keys, 2);
    }
}

static void build_font(tilesb_t *tb)
{
    int pass, c, g;
    for (pass = 0; pass < 2; pass++) {
        int base = tb->tiles.n;
        for (c = 0; c < FONT_COLOURS; c++)
            for (g = 0; g < FONT_GLYPHS; g++) {
                pic_t r;
                uint8_t *block, *p;
                int code = base + g, dummy;
                memset(&r, 0, sizeof r);
                r.x = tb->font_x + g * 8;
                r.y = tb->font_rows[c];
                r.w = 8;
                r.h = 8;
                block = sheet_block(&r, 0, 0, 8, 0);
                if (pass == 1)
                    block = opaque_block(block, 64);
                p = pool_pens(&tb->pool, tb->font_pal[c], block, 64, 0);
                if (c == 0) {
                    graphic_append(&tb->tiles, p);
                    if (!hmap_get(&tb->tiles.codes, p, 64, &dummy))
                        hmap_put(&tb->tiles.codes, p, 64, code);
                } else if (memcmp(tb->tiles.blocks[code], p, 64)) {
                    die("font glyph %d differs in colour %d", g, c);
                }
            }
    }
}

static int wall_index(int row_type, int x, int y)
{
    rect_t wall = named(section_maps[row_type]);
    if (row_type == 1) {
        rect_t door = named(kind_pictures[slot_kinds[0][2]][0]);
        int spill = -slot_ty[0][2] * 8;
        int px = x - (slot_tx[0][2] - BUILDING_COL) * 8, py = y - (wall.h - spill);
        if (0 <= px && px < door.w && 0 <= py && py < spill) {
            int i = sidx(door.x + px, door.y + py);
            if (i != transparent)
                return i;
        }
    }
    return sidx(wall.x + x, wall.y + y);
}

static pic_t section_picture(int row_type)
{
    rect_t wall = named(section_maps[row_type]);
    pic_t p;
    int x, y;
    memset(&p, 0, sizeof p);
    p.w = wall.w;
    p.h = wall.h;
    p.pix = xmalloc((size_t)(wall.w * wall.h));
    for (y = 0; y < wall.h; y++)
        for (x = 0; x < wall.w; x++)
            p.pix[y * wall.w + x] = (uint8_t)wall_index(row_type, x, y);
    return p;
}

typedef struct {
    int h;
    uint8_t *pix;
} slotpic_t;

static slotpic_t slot_picture(int row_type, const char *name, int tx, int ty, int kind, int w)
{
    rect_t pic = named(name), wall = named(section_maps[row_type]);
    int dy = ty * 8, h = dy + pic.h, ox, dx, x, y;
    slotpic_t sp;
    if (wall.h < h)
        h = wall.h;
    ox = (tx - BUILDING_COL) * 8;
    dx = kind_shift[kind];
    sp.h = h;
    sp.pix = xmalloc((size_t)(w * (h > 0 ? h : 1)));
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int i = transparent;
            if (0 <= x - dx && x - dx < pic.w && 0 <= y - dy && y - dy < pic.h)
                i = sidx(pic.x + x - dx, pic.y + y - dy);
            if (i == transparent)
                i = wall_index(row_type, ox + x, y);
            sp.pix[y * w + x] = (uint8_t)i;
        }
    return sp;
}

static slotpic_t slot_cache[4][5];
static int slot_cached[4][5];

static int building_index(int row_type, int bx, int by)
{
    int s;
    for (s = 0; s < 5; s++) {
        int kind = slot_kinds[row_type][s], tx = slot_tx[row_type][s];
        const char *name = kind_pictures[kind][0];
        rect_t pic = named(name);
        int w = pic.w + 2 * kind_shift[kind], ox = (tx - BUILDING_COL) * 8;
        if (ox <= bx && bx < ox + w) {
            if (!slot_cached[row_type][s]) {
                slot_cache[row_type][s] = slot_picture(row_type, name, tx, slot_ty[row_type][s], kind, w);
                slot_cached[row_type][s] = 1;
            }
            if (by < slot_cache[row_type][s].h)
                return slot_cache[row_type][s].pix[by * w + bx - ox];
        }
    }
    return wall_index(row_type, bx, by);
}

static pic_t cut_door(int r)
{
    rect_t rc = T->rects[r];
    int bx = CUT_DOOR_X - BUILDING_COL * 8, x, y;
    pic_t p;
    memset(&p, 0, sizeof p);
    p.w = rc.w;
    p.h = rc.h;
    p.pix = xmalloc((size_t)(rc.w * rc.h));
    for (y = 0; y < rc.h; y++)
        for (x = 0; x < rc.w; x++) {
            int i = sidx(rc.x + x, rc.y + y);
            if (i == transparent)
                i = y < 8 ? building_index(1, bx + x, 48 + y) : building_index(0, bx + x, y - 8);
            p.pix[y * rc.w + x] = (uint8_t)i;
        }
    return p;
}

static pic_t bushes(void)
{
    rect_t bush = named("GREEN_BAR");
    int w = bush.w * GROUND_REPEAT, x, y;
    pic_t p;
    memset(&p, 0, sizeof p);
    p.w = w;
    p.h = bush.h;
    p.pix = xmalloc((size_t)(w * bush.h));
    for (y = 0; y < bush.h; y++)
        for (x = 0; x < w; x++)
            p.pix[y * w + x] = (uint8_t)sidx(bush.x + x % bush.w, bush.y + y);
    return p;
}

static pic_t ground_bushes(void)
{
    rect_t bush = named("GREEN_BAR"), wall = named(section_maps[0]);
    int x0 = GROUND_LEFT_END - 8, w = GROUND_RIGHT_START - GROUND_LEFT_END + 16, h = bush.h, x, y;
    pic_t p;
    memset(&p, 0, sizeof p);
    p.w = w;
    p.h = h;
    p.pix = xmalloc((size_t)(w * h));
    memset(p.pix, transparent, (size_t)(w * h));
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int sx, i;
            if (x >= 8 && x < w - 8)
                continue;
            sx = x0 + x;
            i = wall_index(0, sx - BUILDING_COL * 8, wall.h - h + y);
            if (i == transparent)
                i = sidx(bush.x + sx % bush.w, bush.y + y);
            p.pix[y * w + x] = (uint8_t)i;
        }
    return p;
}

static pic_t lives_strip(int n)
{
    rect_t head = named("LIVES_HEAD");
    int k, x, y;
    pic_t p;
    memset(&p, 0, sizeof p);
    p.w = lives_w;
    p.h = LIVES_H;
    p.opaque = 1;
    p.pix = xmalloc(lives_w * LIVES_H);
    memset(p.pix, transparent, lives_w * LIVES_H);
    for (k = 0; k < n; k++)
        for (y = 0; y < LIVES_H; y++)
            for (x = 0; x < head.w; x++) {
                int px = lives_x[k] - lives_left + x, i = sidx(head.x + x, head.y + y);
                if (px < lives_w && i != transparent)
                    p.pix[y * lives_w + px] = (uint8_t)i;
            }
    return p;
}

static void pictures(tilesb_t *tb, pics_t *out)
{
    int i, rt, s, v;
    memset(out, 0, sizeof *out);
    for (i = 0; i < 27; i++) {
        pic_t r = as_pic(T->tiles[i]);
        r.opaque = !strcmp(tile_names[i], "HIGH_SCORE") || !strcmp(tile_names[i], "LIVES_HEAD") ||
                   !strcmp(tile_names[i], "BLACK_BAR");
        if (!strcmp(tile_names[i], section_maps[1]))
            r = section_picture(1);
        pics_add(out, tile_names[i], r);
    }
    for (i = 0; i < 11; i++) {
        int r = picture_ids[i];
        pics_add(out, picture_names[i], (r == 200 || r == 201) ? cut_door(r) : as_pic(T->rects[r]));
    }
    for (rt = 0; rt < 4; rt++)
        for (s = 0; s < 5; s++) {
            int kind = slot_kinds[rt][s], tx = slot_tx[rt][s], ty = slot_ty[rt][s];
            char *label = NULL;
            for (v = 0; v < kind_count[kind]; v++) {
                const char *name = kind_pictures[kind][v];
                rect_t pic = named(name);
                int w = pic.w + 2 * kind_shift[kind], k, seen = 0;
                slotpic_t sp = slot_picture(rt, name, tx, ty, kind, w);
                pic_t p;
                label = strf("WIN_%s_%d_%s", section_maps[rt], tx, name);
                memset(&p, 0, sizeof p);
                p.w = w;
                p.h = sp.h;
                p.pix = sp.pix;
                for (k = 0; k < out->n; k++)
                    if (!strcmp(out->p[k].name, label))
                        seen = 1;
                if (!seen)
                    pics_add(out, label, p);
                tb->win[rt][s][v] = label;
            }
            for (; v < WINDOW_VARIANTS; v++)
                tb->win[rt][s][v] = label;
        }
    pics_add(out, "BUSHES", bushes());
    pics_add(out, "GROUND_BUSHES", ground_bushes());
    for (i = 1; i <= lives_n; i++) {
        char name[24];
        sprintf(name, "LIVES_%d", i);
        pics_add(out, name, lives_strip(i));
    }
}

static uint8_t *py_slice(const uint8_t *b, int len, int lo, int hi, int *n)
{
    if (lo > len)
        lo = len;
    if (hi > len)
        hi = len;
    if (hi < lo)
        hi = lo;
    *n = hi - lo;
    return (uint8_t *)b + lo;
}

static uint8_t **blocks_of(const pic_t *r, int *count, int **lens)
{
    int rows = (r->h + 7) / 8, cols = (r->w + 7) / 8, x, y, k = 0;
    uint8_t **out = xmalloc(sizeof(uint8_t *) * (size_t)(rows * cols + 1));
    *lens = xmalloc(sizeof(int) * (size_t)(rows * cols + 1));
    for (y = 0; y < r->h; y += 8)
        for (x = 0; x < r->w; x += 8) {
            uint8_t *blk;
            int len = 0;
            if (r->pix) {
                int dy, n;
                blk = xmalloc(64);
                for (dy = 0; dy < 8; dy++) {
                    uint8_t *s = py_slice(r->pix, r->w * r->h, (y + dy) * r->w + x, (y + dy) * r->w + x + 8, &n);
                    memcpy(blk + len, s, (size_t)n);
                    len += n;
                }
            } else {
                blk = sheet_block(r, x, y, 8, 0);
                len = 64;
            }
            if (r->opaque)
                blk = opaque_block(blk, len);
            out[k] = blk;
            (*lens)[k++] = len;
        }
    *count = k;
    return out;
}

static void build_maps(tilesb_t *tb, int *nmaps, tmap_t **maps)
{
    pics_t pics;
    hmap_t patterns;
    int late, i, j, k;
    pictures(tb, &pics);
    hmap_init(&patterns);
    for (late = 0; late < 2; late++) {
        int nb = 0, cap = 256;
        uint8_t **batch = xmalloc(sizeof(uint8_t *) * (size_t)cap);
        int *blen = xmalloc(sizeof(int) * (size_t)cap);
        hmap_t inbatch;
        sortkey_t *order;
        hmap_init(&inbatch);
        for (i = 0; i < pics.n; i++) {
            uint8_t **bl;
            int nbl, *lens, dummy;
            if (!strcmp(pics.p[i].name, "FONT"))
                continue;
            if ((!strcmp(pics.p[i].name, "GROUND_BUSHES")) != late)
                continue;
            bl = blocks_of(&pics.p[i].r, &nbl, &lens);
            for (j = 0; j < nbl; j++) {
                if (is_empty(bl[j], lens[j]))
                    continue;
                if (hmap_get(&patterns, bl[j], lens[j], &dummy) || hmap_get(&inbatch, bl[j], lens[j], &dummy))
                    continue;
                hmap_put(&inbatch, bl[j], lens[j], nb);
                if (nb == cap) {
                    cap *= 2;
                    batch = xrealloc(batch, sizeof(uint8_t *) * (size_t)cap);
                    blen = xrealloc(blen, sizeof(int) * (size_t)cap);
                }
                batch[nb] = bl[j];
                blen[nb++] = lens[j];
            }
        }
        order = xmalloc(sizeof(sortkey_t) * (size_t)(nb ? nb : 1));
        for (i = 0; i < nb; i++) {
            keyset_t ks;
            keys_of(batch[i], blen[i], 0, &ks);
            order[i].size = ks.n;
            order[i].order = i;
        }
        qsort(order, (size_t)nb, sizeof(sortkey_t), sortkey_cmp);
        for (i = 0; i < nb; i++) {
            int b = order[i].order;
            keyset_t ks;
            keys_of(batch[b], blen[b], 0, &ks);
            if (ks.n > PENS - 1)
                die("tile with too many colours");
            hmap_put(&patterns, batch[b], blen[b], pool_place(&tb->pool, &ks));
        }
    }
    if (tb->pool.n > PALETTES)
        die("too many tile palettes");
    pictures(tb, &pics);
    *nmaps = pics.n;
    *maps = xcalloc((size_t)pics.n, sizeof(tmap_t));
    for (i = 0; i < pics.n; i++) {
        tmap_t *m = &(*maps)[i];
        m->name = pics.p[i].name;
        m->w = pics.p[i].r.w / 8;
        m->h = pics.p[i].r.h / 8;
        if (!strcmp(m->name, "FONT"))
            continue;
        {
            uint8_t **bl;
            int nbl, *lens;
            m->has_map = 1;
            bl = blocks_of(&pics.p[i].r, &nbl, &lens);
            m->n = nbl;
            m->entries = xmalloc(sizeof(int) * (size_t)(nbl ? nbl : 1));
            for (k = 0; k < nbl; k++) {
                int number;
                uint8_t *p;
                if (is_empty(bl[k], lens[k])) {
                    m->entries[k] = 0;
                    continue;
                }
                hmap_get(&patterns, bl[k], lens[k], &number);
                p = pool_pens(&tb->pool, number, bl[k], lens[k], 0);
                if (lens[k] != 64)
                    die("tile block of %d pixels", lens[k]);
                m->entries[k] = graphic_code(&tb->tiles, p) | number << 11;
            }
        }
    }
}

static void build_service(tilesb_t *tb)
{
    int row[PENS], i, pen, flags;
    static const int bars[15][3] = {
        { 31, 31, 31 }, { 31, 31, 0 }, { 0, 31, 31 }, { 0, 31, 0 }, { 31, 0, 31 }, { 31, 0, 0 }, { 0, 0, 31 },
        { 0, 0, 0 }, { 4, 4, 4 }, { 8, 8, 8 }, { 12, 12, 12 }, { 16, 16, 16 }, { 20, 20, 20 }, { 24, 24, 24 },
        { 28, 28, 28 },
    };
    tb->service_pal = tb->pool.n;
    row[0] = 0;
    for (i = 0; i < 15; i++)
        row[i + 1] = word555(bars[i]);
    rows_add(&tb->service, row);
    for (i = 0; i < 4; i++) {
        for (pen = 0; pen < PENS; pen++) {
            int v = (pen * 31 + 7) / 15, c[3];
            c[0] = service_ramps[i][0] * v;
            c[1] = service_ramps[i][1] * v;
            c[2] = service_ramps[i][2] * v;
            row[pen] = word555(c);
        }
        rows_add(&tb->service, row);
    }
    for (pen = 1; pen < PENS; pen++) {
        uint8_t *b = xmalloc(64);
        memset(b, pen, 64);
        graphic_append(&tb->tiles, b);
    }
    for (flags = 0; flags < 32; flags++) {
        uint8_t *pix = xcalloc(64, 1);
        for (i = 0; i < 8; i++) {
            if (flags & 1)
                pix[i] = 1;
            if (flags & 4)
                pix[56 + i] = 1;
            if (flags & 2)
                pix[i * 8] = 1;
            if (flags & 8)
                pix[i * 8 + 7] = 1;
        }
        if (flags & 16)
            pix[0] = 1;
        graphic_append(&tb->tiles, pix);
    }
}

static void tile_palettes(const tilesb_t *tb, rows_t *rs)
{
    int i, row[PENS];
    for (i = 0; i < tb->pool.n; i++) {
        palette_row(&tb->pool.p[i], NULL, row);
        rows_add(rs, row);
    }
    for (i = 0; i < tb->service.n; i++)
        rows_add(rs, tb->service.rows[i]);
}

static int big_opaque(rect_t r, int x, int y, int w, int h)
{
    int x0 = x < 0 ? 0 : x, x1 = x + w > r.w ? r.w : x + w, y0 = y < 0 ? 0 : y, y1 = y + h > r.h ? r.h : y + h;
    int xx, yy;
    for (yy = y0; yy < y1; yy++)
        for (xx = x0; xx < x1; xx++)
            if (sidx(r.x + xx, r.y + yy) != transparent)
                return 1;
    return 0;
}

static int first_size(int left)
{
    int i;
    for (i = 0; i < 3; i++)
        if (big_sizes[i] < left + 16 || big_sizes[i] == 16)
            return big_sizes[i];
    return 16;
}

static int size_code(int n)
{
    return n == 64 ? 2 : n == 32 ? 1 : 0;
}

typedef struct {
    int x, y, w, h;
    keyset_t ks;
} bblock_t;

typedef struct {
    int n;
    bblock_t *b;
} bblocks_t;

static uint8_t **big_cells(rect_t r, int x, int y, int w, int h, int *n)
{
    pic_t p = as_pic(r);
    int cx, cy, k = 0;
    uint8_t **out = xmalloc(sizeof(uint8_t *) * (size_t)((w / 16) * (h / 16) + 1));
    for (cx = 0; cx < w; cx += 16)
        for (cy = 0; cy < h; cy += 16)
            out[k++] = sheet_block(&p, x + cx, y + cy, 16, 0);
    *n = k;
    return out;
}

static void big_split(rect_t r, int x, int y, int w, int h, bblocks_t *out)
{
    uint8_t **cells;
    int n, i, empty = 0;
    keyset_t ks;
    if (!big_opaque(r, x, y, w, h))
        return;
    cells = big_cells(r, x, y, w, h, &n);
    ks.n = 0;
    for (i = 0; i < n; i++) {
        keyset_t k;
        keys_of(cells[i], 256, 0, &k);
        keyset_add(&ks, &k);
        if (is_empty(cells[i], 256))
            empty++;
    }
    if ((ks.n <= PENS - 1 && empty * 2 < n) || (w == 16 && h == 16)) {
        out->b = xrealloc(out->b, sizeof(bblock_t) * (size_t)(out->n + 1));
        out->b[out->n].x = x;
        out->b[out->n].y = y;
        out->b[out->n].w = w;
        out->b[out->n].h = h;
        out->b[out->n++].ks = ks;
        return;
    }
    if (w >= h && w > 16) {
        big_split(r, x, y, w / 2, h, out);
        big_split(r, x + w / 2, y, w / 2, h, out);
    } else {
        big_split(r, x, y, w, h / 2, out);
        big_split(r, x, y + h / 2, w, h / 2, out);
    }
}

static void build_big(sprites_t *sp, graphic_t *cells, gfx_t *g)
{
    int k;
    for (k = 0; k < NBIG; k++) {
        rect_t r = T->rects[187 + k];
        int x, first = -1, last = -1, np = 0;
        bpart_t *parts = NULL;
        for (x = 0; x < r.w; x++)
            if (big_opaque(r, x, 0, 1, r.h)) {
                if (first < 0)
                    first = x;
                last = x;
            }
        x = first;
        while (x <= last) {
            int w = first_size(last + 1 - x), y, *rows = xmalloc(sizeof(int) * (size_t)(r.h + 1)), nr = 0, i;
            while (w > 16 && !big_opaque(r, x + w / 2, 0, w / 2, r.h))
                w /= 2;
            for (y = 0; y < r.h; y++)
                if (big_opaque(r, x, y, w, 1))
                    rows[nr++] = y;
            i = 0;
            while (i < nr) {
                int top = rows[i], h = first_size(rows[nr - 1] + 1 - top), b;
                bblocks_t bl = { 0 };
                while (h > 16 && !big_opaque(r, x, top + h / 2, w, h / 2))
                    h /= 2;
                big_split(r, x, top, w, h, &bl);
                for (b = 0; b < bl.n; b++) {
                    int n = pool_place(&sp->pool, &bl.b[b].ks), nc, c, code;
                    uint8_t **cs = big_cells(r, bl.b[b].x, bl.b[b].y, bl.b[b].w, bl.b[b].h, &nc);
                    for (c = 0; c < nc; c++)
                        cs[c] = pool_pens(&sp->pool, n, cs[c], 256, 0);
                    code = graphic_run(cells, cs, nc);
                    parts = xrealloc(parts, sizeof(bpart_t) * (size_t)(np + 1));
                    parts[np].x = bl.b[b].x;
                    parts[np].y = bl.b[b].y;
                    parts[np].code = code;
                    parts[np].pal = sp->general_base + n;
                    parts[np++].size = size_code(bl.b[b].w) | size_code(bl.b[b].h) << 2 | ((code >> 10) & 1) << 5;
                }
                while (i < nr && rows[i] < top + h)
                    i++;
            }
            free(rows);
            x += w;
            while (x <= last && !big_opaque(r, x, 0, 1, r.h))
                x++;
        }
        g->nbig[k] = np;
        g->big[k] = parts;
    }
    if (sp->general_base + sp->pool.n > PALETTES)
        die("too many sprite palettes");
}

static void palette_rom(const rows_t *tiles, const rows_t *sprites, buf_t *out)
{
    int words[2 * PALETTES * PENS], layer, n, p;
    memset(words, 0, sizeof words);
    for (layer = 0; layer < 2; layer++) {
        const rows_t *rs = layer ? sprites : tiles;
        for (n = 0; n < rs->n; n++)
            for (p = 0; p < PENS; p++)
                if (rs->rows[n][p] >= 0)
                    words[layer * 512 + n * PENS + p] = rs->rows[n][p];
    }
    out->data = xmalloc(sizeof words / sizeof words[0] * 2);
    out->len = 2 * PALETTES * PENS * 2;
    for (n = 0; n < 2 * PALETTES * PENS; n++) {
        out->data[n] = (uint8_t)(words[n] & 0xFF);
        out->data[2 * PALETTES * PENS + n] = (uint8_t)(words[n] >> 8);
    }
}

static int one(const pe_t *pe, const char *name)
{
    int v;
    if (exe_record(pe, name, &v, 1) != 1)
        die("game number %s is not one value", name);
    return v;
}

void gfx_exe(const pe_t *pe)
{
    int kinds[25], low[5], high[5], r, k, col, half, w;
    if (exe_table(pe, "exe_kinds", kinds, 25) != 25 || exe_table(pe, "exe_tx_low", low, 5) != 5
        || exe_table(pe, "exe_tx_high", high, 5) != 5)
        die("the building layout tables have the wrong size");
    w = one(pe, "bldg.row_win_ty");
    for (r = 0; r < 4; r++) {
        for (k = 0; k < 5; k++) {
            slot_kinds[r][k] = kinds[5 * r + k];
            slot_tx[r][k] = r < 2 ? low[k] : high[k];
            slot_ty[r][k] = r & 1 ? w : 0;
        }
    }
    slot_ty[0][2] = one(pe, "bldg.door_slot_ty");
    kind_shift[0] = one(pe, "bldg.k0_left");
    kind_shift[3] = one(pe, "bldg.k3_left");
    kind_shift[4] = one(pe, "bldg.k4_left");
    transparent = one(pe, "pal.transparent");
    drawn_black = one(pe, "pal.drawn_black");
    if (exe_record(pe, "pal.clothing", clothing, 4) != 4 || exe_record(pe, "pal.hammer", hammer_index, 3) != 3)
        die("the palette index lists have the wrong size");
    lives_n = one(pe, "hud.MAX_LIVES_SHOWN");
    if (lives_n < 1 || lives_n > LIVES_MAX)
        die("%d lives shown", lives_n);
    col = one(pe, "hud.lives_col_raw");
    half = one(pe, "hud.lives_half");
    lives_left = SCREEN_W;
    for (k = 0; k < lives_n; k++) {
        lives_x[k] = col * 8 + (k & 1 ? 0 : half);
        col -= k & 1 ? 2 : 1;
        if (lives_x[k] < lives_left)
            lives_left = lives_x[k];
    }
    lives_w = SCREEN_W - lives_left;
}

int gfx_index(const char *which, int k)
{
    return !strcmp(which, "hammer") ? hammer_index[k] : clothing[k];
}

void build_gfx(const sheet_t *s, const tables_t *t, int hammer[4][3], felix_variant_t felix[4], gfx_t *g)
{
    static tilesb_t tb;
    static sprites_t sp;
    static graphic_t cells;
    rows_t trows, srows;
    int i, felix_c = 0, hammer_c = 0;
    S = s;
    T = t;
    memset(g, 0, sizeof *g);
    tiles_init(&tb);
    build_font(&tb);
    build_maps(&tb, &g->nmaps, &g->maps);
    build_service(&tb);
    sprites_init(&sp, hammer);
    sprites_collect(&sp);
    sprites_pack(&sp);
    graphic_init(&cells, 16, MAX_CELLS);
    sprites_build(&sp, &cells, g->metas);
    build_big(&sp, &cells, g);
    memset(&trows, 0, sizeof trows);
    memset(&srows, 0, sizeof srows);
    tile_palettes(&tb, &trows);
    sprite_palettes(&sp, felix, &srows);
    graphic_encode(&tb.tiles, &g->tiles);
    graphic_encode(&cells, &g->sprites);
    palette_rom(&trows, &srows, &g->pal);
    memcpy(g->win, tb.win, sizeof g->win);
    for (i = 0; i < 16; i++) {
        felix_c += 3;
        hammer_c += 3;
        if (hammer_c % 5 == 0)
            hammer_c++;
        if (i >= 1) {
            g->cycle[i - 1][0] = felix_c % 5;
            g->cycle[i - 1][1] = hammer_c % 5;
        }
    }
}
