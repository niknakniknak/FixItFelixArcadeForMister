/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "video.h"

extern volatile uint8_t *spr_out;
extern uint8_t spr_free;
extern const spr_part_t *spr_part;
extern uint8_t spr_count;
extern int16_t spr_x, spr_y;
extern uint8_t spr_add, spr_add_below;
void spr_put_parts(void);

static uint8_t spr_window;
extern const big_part_t *big_part;
extern uint8_t big_n, big_first, big_cnt, big_used;
extern int16_t big_x, big_y;
void big_put(void);
void big_clear(void);

void map_fill(volatile uint16_t *map, uint16_t entry)
{
    uint16_t i;
    for (i = 0; i < MAP_COLS * MAP_ROWS; i++)
        map[i] = entry;
}

void map_draw(volatile uint16_t *map, uint8_t tilemap, uint8_t col, uint8_t row)
{
    const tilemap_t *tm = &tilemaps[tilemap];
    const uint16_t *src = tm->map;
    uint8_t x, y;
    for (y = 0; y < tm->h; y++) {
        volatile uint16_t *dst = &map[MAP_INDEX(0, (row + y) & (MAP_ROWS - 1))];
        for (x = 0; x < tm->w; x++)
            dst[(col + x) & (MAP_COLS - 1)] = *src++;
    }
}

static uint16_t glyph(char c, uint8_t colour)
{
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    if (c < 0x20 || c > 0x5F)
        c = '?';
    return FONT_ENTRY(c, colour);
}

void text_print(uint8_t col, uint8_t row, uint8_t colour, const char *s)
{
    volatile uint16_t *dst = &FG_MAP[MAP_INDEX(col, row)];
    while (*s)
        *dst++ = glyph(*s++, colour);
}

static char hex_digit(uint8_t v)
{
    return v < 10 ? '0' + v : 'A' + v - 10;
}

void text_hex8(uint8_t col, uint8_t row, uint8_t colour, uint8_t value)
{
    volatile uint16_t *dst = &FG_MAP[MAP_INDEX(col, row)];
    dst[0] = glyph(hex_digit(value >> 4), colour);
    dst[1] = glyph(hex_digit(value & 15), colour);
}

void text_hex16(uint8_t col, uint8_t row, uint8_t colour, uint16_t value)
{
    text_hex8(col, row, colour, value >> 8);
    text_hex8(col + 2, row, colour, (uint8_t)value);
}

void spr_begin(uint8_t first, uint8_t count)
{
    spr_out = (volatile uint8_t *)&SPRITES[first];
    spr_free = count;
    spr_window = count;
    if (big_used) {
        big_first = first;
        big_cnt = count;
        big_clear();
    }
}

void spr_big(uint8_t rect, int16_t x, int16_t y)
{
    const big_pic_t *pic = &big_pics[rect - BIG_RECT_FIRST];
    big_part = &big_parts[pic->first];
    big_n = pic->count;
    big_x = x;
    big_y = y;
    big_put();
}

void spr_rect(uint8_t rect, int16_t x, int16_t y, uint8_t felix, uint8_t hammer)
{
    const msprite_t *m;
    if ((uint8_t)(rect - BIG_RECT_FIRST) < BIG_RECT_COUNT) {
        spr_big(rect, x, y);
        return;
    }
    m = &msprites[rect];
    spr_x = x;
    spr_y = y;
    spr_part = &spr_parts[m->first];
    if (hammer) {
        spr_count = m->overlay;
        spr_add = (hammer - 1) << SPR_PAL_SHIFT;
        spr_add_below = 0xFF;
        spr_put_parts();
    }
    spr_part = &spr_parts[m->first + m->overlay];
    spr_count = m->count;
    spr_add = felix << SPR_PAL_SHIFT;
    spr_add_below = SPR_PAL_CYCLE_END << SPR_PAL_SHIFT;
    spr_put_parts();
}

void spr_frame(const anim_frame_t *frame, int16_t x, int16_t y, uint8_t mirrored,
               uint8_t felix, uint8_t hammer)
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
    uint8_t used = spr_window - spr_free;
    for (; spr_free; spr_free--) {
        *spr_out++ = SPR_HIDDEN_X;
        *spr_out++ = (uint8_t)SPR_HIDDEN_Y;
        *spr_out++ = 0;
        *spr_out++ = SPR_ATTR_Y8;
    }
    return used;
}
