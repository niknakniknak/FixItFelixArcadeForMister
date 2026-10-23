/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "page.h"

uint8_t pg_show;
static uint8_t pg_dirty[2][8];
static uint8_t pg_stale[2][8];
static uint8_t pg_dsum[2];
static uint8_t pg_ssum[2];
static uint8_t pg_buf[64];

uint8_t pg_cpu_bits(uint8_t show)
{
    return (show & PG_FG_SHOW ? 0 : PG_FG_CPU) | (show & PG_BG_SHOW ? 0 : PG_BG_CPU);
}

void game_text_print(uint8_t col, uint8_t row, uint8_t colour, const char *s)
{
    fg_touch(row);
    (text_print)(col, row, colour, s);
}

void pg_hidden(void)
{
    FG_PAGE = pg_show | pg_cpu_bits(pg_show);
}

void pg_bg_both(void)
{
    FG_PAGE = pg_show | (pg_cpu_bits(pg_show) & PG_FG_CPU) | PG_BG_BOTH;
}

void pg_init(void)
{
    pg_show = 0;
    memset(pg_dirty, 0, sizeof pg_dirty);
    memset(pg_stale, 0, sizeof pg_stale);
    pg_dsum[0] = pg_dsum[1] = pg_ssum[0] = pg_ssum[1] = 0;
    FG_PAGE = PG_FG_BOTH | PG_BG_BOTH;
}

static void row_copy(uint8_t layer, uint8_t row)
{
    uint8_t hidden = pg_cpu_bits(pg_show);
    uint8_t *map = (uint8_t *)(layer ? BG_MAP : FG_MAP) + ((uint16_t)row << 6);
    FG_PAGE = pg_show | (hidden ^ (layer ? PG_BG_CPU : PG_FG_CPU));
    memcpy(pg_buf, map, sizeof pg_buf);
    FG_PAGE = pg_show | hidden;
    memcpy(map, pg_buf, sizeof pg_buf);
}

static void unstale(uint8_t layer, uint8_t i, uint8_t bit)
{
    if (!(pg_stale[layer][i] &= ~bit))
        pg_ssum[layer] &= ~(1 << i);
}

static void mark_dirty(uint8_t layer, uint8_t i, uint8_t bit)
{
    pg_dirty[layer][i] |= bit;
    pg_dsum[layer] |= 1 << i;
}

void fg_touch(uint8_t row)
{
    uint8_t i = (row >> 3) & 7, bit = 1 << (row & 7);
    if (pg_stale[PG_FG][i] & bit) {
        unstale(PG_FG, i, bit);
        row_copy(PG_FG, row & 63);
    }
    mark_dirty(PG_FG, i, bit);
}

void fg_touch_rows(uint8_t first, uint8_t n)
{
    uint8_t i, bit;
    for (; n; n--, first++) {
        i = (first >> 3) & 7;
        bit = 1 << (first & 7);
        if (pg_stale[PG_FG][i] & bit)
            unstale(PG_FG, i, bit);
        mark_dirty(PG_FG, i, bit);
    }
}

void bg_touch_part(uint8_t row)
{
    uint8_t i = (row >> 3) & 7, bit = 1 << (row & 7);
    if (pg_stale[PG_BG][i] & bit) {
        unstale(PG_BG, i, bit);
        row_copy(PG_BG, row);
    }
    mark_dirty(PG_BG, i, bit);
}

void bg_touch_row(uint8_t row, uint8_t hidden)
{
    uint8_t i = (row >> 3) & 7, bit = 1 << (row & 7);
    if (pg_stale[PG_BG][i] & bit)
        unstale(PG_BG, i, bit);
    if (hidden)
        mark_dirty(PG_BG, i, bit);
}

static uint8_t copy_stale(uint8_t layer, uint8_t budget)
{
    uint8_t i, b, row;
    uint8_t *s;
    if (!pg_ssum[layer])
        return budget;
    for (i = 0, s = pg_stale[layer]; i < 8; i++, s++) {
        if (!*s)
            continue;
        for (b = 1, row = i * 8; b; b <<= 1, row++) {
            if (!(*s & b))
                continue;
            if (!budget)
                return 0;
            budget--;
            unstale(layer, i, b);
            row_copy(layer, row);
        }
    }
    return budget;
}

void pg_catch_up(uint8_t budget)
{
    if (!(pg_ssum[PG_FG] | pg_ssum[PG_BG]))
        return;
    budget = copy_stale(PG_FG, budget);
    copy_stale(PG_BG, budget);
    pg_hidden();
}

uint8_t pg_flips(void)
{
    uint8_t layer, flip = 0;
    for (layer = 0; layer < 2; layer++) {
        if (!pg_dsum[layer])
            continue;
        copy_stale(layer, 0xFF);
        memcpy(pg_stale[layer], pg_dirty[layer], 8);
        memset(pg_dirty[layer], 0, 8);
        pg_ssum[layer] = pg_dsum[layer];
        pg_dsum[layer] = 0;
        flip |= layer ? PG_BG_SHOW : PG_FG_SHOW;
    }
    return flip;
}
