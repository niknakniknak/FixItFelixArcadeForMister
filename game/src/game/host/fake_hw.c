/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include "board.h"

volatile uint8_t fake_io[0x800];
volatile uint16_t fake_bg[2048];
volatile uint16_t fake_fg[2048];
volatile uint8_t fake_spr[1024];
volatile uint8_t fake_nv[0x400] = { [0 ... 0x3FF] = 0xFF };
uint8_t fake_rom[0xC000];

volatile uint8_t *spr_out;
uint8_t spr_free;
static uint8_t spr_window;
uint8_t spr_big_flags;
static uint8_t big_used[SPRITE_COUNT];

void map_fill(volatile uint16_t *map, uint16_t entry)
{
    uint16_t i;
    for (i = 0; i < MAP_COLS * MAP_ROWS; i++)
        map[i] = entry;
}

void map_draw(volatile uint16_t *map, uint8_t tilemap, uint8_t col, uint8_t row)
{
    (void)map;
    (void)tilemap;
    (void)col;
    (void)row;
}

void text_print(uint8_t col, uint8_t row, uint8_t colour, const char *s)
{
    volatile uint16_t *dst = &FG_MAP[MAP_INDEX(col, row)];
    while (*s) {
        char c = *s++;
        *dst++ = FONT_ENTRY(c, colour);
    }
}

void text_hex8(uint8_t col, uint8_t row, uint8_t colour, uint8_t value)
{
    (void)col;
    (void)row;
    (void)colour;
    (void)value;
}

void text_hex16(uint8_t col, uint8_t row, uint8_t colour, uint16_t value)
{
    (void)col;
    (void)row;
    (void)colour;
    (void)value;
}

void spr_begin(uint8_t first, uint8_t count)
{
    uint8_t i;
    spr_out = &fake_spr[first * 4];
    spr_free = count;
    spr_window = count;
    for (i = first; i < first + count && i < SPRITE_COUNT; i++)
        if (big_used[i]) {
            big_used[i] = 0;
            SPR_SIZE[i] = 0;
        }
}

void spr_big(uint8_t rect, int16_t x, int16_t y)
{
    const big_pic_t *pic = &big_pics[rect - BIG_RECT_FIRST];
    const big_part_t *p = &big_parts[pic->first];
    uint8_t n;
    for (n = pic->count; n && spr_free; n--, p++) {
        int w = 16 << (p->size & 3), h = 16 << ((p->size >> 2) & 3);
        int sx = x + p->dx, sy = y + p->dy;
        uint8_t i = (uint8_t)((spr_out - fake_spr) >> 2);
        if (sx >= SCREEN_W || sx <= -w || sy >= SCREEN_H || sy <= -h)
            continue;
        spr_out[0] = (uint8_t)sx;
        spr_out[1] = (uint8_t)sy;
        spr_out[2] = p->code;
        spr_out[3] = p->attr | ((sy & 0x100) ? SPR_ATTR_Y8 : 0);
        SPR_SIZE[i] = p->size | spr_big_flags | ((sx & 0x100) ? SPR_X8 : 0);
        big_used[i] = 1;
        spr_out += 4;
        spr_free--;
    }
}

static void put_parts(const spr_part_t *part, uint8_t n, int16_t x, int16_t y, uint8_t add, uint8_t below)
{
    for (; n && spr_free; n--, part++) {
        uint16_t sx = (uint16_t)(x + part->dx);
        uint16_t sy = (uint16_t)(y + part->dy);
        uint8_t attr = part->attr;
        if ((sx & 0xFF00) || sy >= SCREEN_H)
            continue;
        if (attr < below)
            attr += add;
        if (sy & 0x100)
            attr |= SPR_ATTR_Y8;
        spr_out[0] = (uint8_t)sx;
        spr_out[1] = (uint8_t)sy;
        spr_out[2] = part->code;
        spr_out[3] = attr;
        spr_out += 4;
        spr_free--;
    }
}

void spr_rect(uint8_t rect, int16_t x, int16_t y, uint8_t felix, uint8_t hammer)
{
    const msprite_t *m = &msprites[rect];
    const spr_part_t *part = &spr_parts[m->first];
    if ((uint8_t)(rect - BIG_RECT_FIRST) < BIG_RECT_COUNT) {
        spr_big(rect, x, y);
        return;
    }
    if (hammer)
        put_parts(part, m->overlay, x, y, (hammer - 1) << SPR_PAL_SHIFT, 0xFF);
    put_parts(part + m->overlay, m->count, x, y, felix << SPR_PAL_SHIFT, SPR_PAL_CYCLE_END << SPR_PAL_SHIFT);
}

void spr_frame(const anim_frame_t *frame, int16_t x, int16_t y, uint8_t mirrored, uint8_t felix,
               uint8_t hammer)
{
    if (mirrored && frame->mirror != MIRROR_NONE) {
        uint8_t rect = frame->mirror;
        spr_rect(rect, x + frame->hx - msprites[rect].w8 * 8, y - frame->hy, felix, hammer);
    } else {
        spr_rect(frame->rect, x - frame->hx, y - frame->hy, felix, hammer);
    }
}

uint8_t spr_end(void)
{
    return spr_window - spr_free;
}

uint8_t host_column_load(uint8_t *worst_x)
{
    uint8_t best = 0, x;
    uint16_t i;
    for (x = 0; x < SCREEN_W; x++) {
        uint8_t n = 0;
        for (i = 0; i < SPRITE_COUNT; i++) {
            const volatile uint8_t *e = &fake_spr[i * 4];
            uint8_t size = SPR_SIZE[i];
            int wc = (size & 3) > 2 ? 2 : (size & 3), hc = ((size >> 2) & 3) > 2 ? 2 : ((size >> 2) & 3);
            int h = 16 << hc;
            uint16_t y = e[1] | ((e[3] & SPR_ATTR_Y8) ? 0x100 : 0);
            uint16_t sx = e[0] | ((size & SPR_X8) ? 0x100 : 0);
            if ((y < SCREEN_H || y > 512 - h) && ((x - sx) & 511) < (16 << wc))
                n += 1 << hc;
        }
        if (n > best) {
            best = n;
            *worst_x = x;
        }
    }
    return best;
}

extern const uint8_t tm_rows[];
const uint16_t *fgp_map;
int8_t fgp_col, fgp_row;
uint8_t fgp_w, fgp_h, fgp_mode;

static void fg_pic_row(const uint8_t *p, int8_t row)
{
    uint16_t vals[64];
    int x = 0, f = -1, l = -1, c, lo = row == 39 ? 12 : 0;
    while (x < fgp_w) {
        uint8_t op = *p++, n = (op & 0x3F) + 1, i;
        uint16_t v = 0;
        if (op & 0xC0) {
            v = p[0] | p[1] << 8;
            p += 2;
        }
        for (i = 0; i < n; i++, x++) {
            if (!(op & 0xC0)) {
                v = p[0] | p[1] << 8;
                p += 2;
            }
            vals[x] = v;
            if (v) {
                if (f < 0)
                    f = x;
                l = x;
            }
            if ((op & 0xC0) == 0x80)
                v++;
        }
    }
    if (f < 0)
        return;
    if (fgp_mode == 2) {
        for (x = 0; x < fgp_w; x++) {
            c = fgp_col + x;
            if (vals[x] && c >= lo && c < 30)
                FG_MAP[row * 32 + c] = vals[x];
        }
        return;
    }
    for (x = f - 1; x <= l + 1; x++) {
        c = fgp_col + x;
        if (c >= lo && c < 30)
            FG_MAP[row * 32 + c] = fgp_mode || x < 0 || x >= fgp_w ? 0 : vals[x];
    }
}

void fg_pic(void)
{
    uint8_t y;
    for (y = 0; y < fgp_h && fgp_row + y < 40; y++)
        fg_pic_row(tm_rows + fgp_map[y], fgp_row + y);
}

void garbage_fill(uint16_t offset)
{
    uint16_t i;
    for (i = 0; i < MAP_COLS * TEXT_ROWS; i++)
        FG_MAP[i] = (uint16_t)((offset + i * 7) & 0x3F);
}

uint8_t rom_sum_slice(uint8_t slice)
{
    (void)slice;
    return 0;
}
