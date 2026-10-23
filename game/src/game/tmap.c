/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "tmap.h"

#define OP_MASK 0xC0
#define OP_LITERAL 0x00
#define OP_SEQUENCE 0x80
#define COUNT_MASK 0x3F

extern const uint8_t tm_rows[];

#ifdef __SDCC
extern const uint8_t *tmr_src;
extern uint8_t tmr_w, tmr_i0, tmr_n, tmr_c0, tmr_opaque;
extern volatile uint16_t *tmr_dst;
void tmr_decode(void);
void tmr_put(void);
void tmr_clear(void);

extern const uint8_t *tmr_last;

void tm_row(uint8_t tm, uint8_t src_row, volatile uint16_t *dst, int16_t col, int16_t lo, int16_t hi,
            uint8_t opaque)
{
    const tilemap_t *m = &tilemaps[tm];
    const uint8_t *src;
    int16_t a = col > lo ? col : lo;
    int16_t b = col + m->w;
    if (b > hi)
        b = hi;
    if (a >= b)
        return;
    src = tm_rows + m->map[src_row];
    if (src != tmr_last) {
        tmr_src = src;
        tmr_w = m->w;
        tmr_decode();
        tmr_last = src;
    }
    tmr_dst = dst;
    tmr_i0 = (uint8_t)(a - col);
    tmr_n = (uint8_t)(b - a);
    tmr_c0 = (uint8_t)a & 31;
    tmr_opaque = opaque;
    tmr_put();
}

void tm_clear(volatile uint16_t *dst, int16_t lo, int16_t hi)
{
    if (lo >= hi)
        return;
    tmr_dst = dst;
    tmr_n = (uint8_t)(hi - lo);
    tmr_c0 = (uint8_t)lo & 31;
    tmr_clear();
}
#else
void tm_clear(volatile uint16_t *dst, int16_t lo, int16_t hi)
{
    for (; lo < hi; lo++)
        dst[lo & 31] = 0;
}

void tm_row(uint8_t tm, uint8_t src_row, volatile uint16_t *dst, int16_t col, int16_t lo, int16_t hi,
            uint8_t opaque)
{
    const tilemap_t *m = &tilemaps[tm];
    const uint8_t *p = tm_rows + m->map[src_row];
    int16_t end = col + m->w;
    if (end > hi)
        end = hi;
    while (col < end) {
        uint8_t c = *p++;
        uint8_t n = (c & COUNT_MASK) + 1;
        uint8_t op = c & OP_MASK;
        uint16_t v;
        if (col + n <= lo) {
            p += op == OP_LITERAL ? n * 2 : 2;
            col += n;
            continue;
        }
        v = p[0] | (p[1] << 8);
        if (op == OP_LITERAL) {
            for (; n; n--, col++, p += 2) {
                v = p[0] | (p[1] << 8);
                if (col >= lo && col < end && (v || opaque))
                    dst[col & 31] = v;
            }
            continue;
        }
        p += 2;
        if (col < lo) {
            uint8_t skip = (uint8_t)(lo - col);
            n -= skip;
            col = lo;
            if (op == OP_SEQUENCE)
                v += skip;
        }
        if (!v && !opaque) {
            col += n;
            continue;
        }
        for (; n && col < end; n--, col++) {
            dst[col & 31] = v;
            if (op == OP_SEQUENCE)
                v++;
        }
        col += n;
    }
}

#endif

void tm_draw(volatile uint16_t *map, uint8_t tm, uint8_t col, uint8_t row)
{
    uint8_t y, h = tilemaps[tm].h;
    for (y = 0; y < h; y++)
        tm_row(tm, y, &map[MAP_INDEX(0, (row + y) & 63)], col, col, col + tilemaps[tm].w, 1);
}
