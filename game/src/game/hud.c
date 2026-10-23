/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "hud.h"
#include "input.h"
#include "tmap.h"
#include "bg.h"
#include "page.h"
#include "exe.h"
#include "sound.h"

#define BAND_TILE FONT_OPAQUE_ENTRY(' ', 0)
#define BAND_ROWS 3
#define LIVES_ROW 0
#define LIVES_COL X8(hud_lives_col)
#define LIVES_COLS 9
#define MAX_LIVES_SHOWN X8(hud_max_lives_shown)
#define NONE 0xFFFFFFFFUL

uint32_t hiscore;

static uint32_t shown_hiscore;
static uint32_t shown_score;
static uint8_t shown_lives;
#define FREE_SHOWN 0xFE

static uint8_t shown_credits;
static uint8_t shown_player;

#ifdef __SDCC
extern uint8_t fcr_first, fcr_n;
void fcr_fill(void);

void fg_clear_rows(uint8_t first, uint8_t last) BANKED
{
    if (last < first)
        return;
    fg_touch_rows(first, last - first + 1);
    fcr_first = first;
    fcr_n = last - first + 1;
    fcr_fill();
}
#else
void fg_clear_rows(uint8_t first, uint8_t last) BANKED
{
    uint8_t row, col;
    if (last >= first)
        fg_touch_rows(first, last - first + 1);
    for (row = first; row <= last; row++) {
        volatile uint16_t *dst = &FG_MAP[MAP_INDEX(0, row)];
        for (col = 0; col < TEXT_COLS; col++)
            dst[col] = 0;
    }
}
#endif

void fg_band(void) BANKED
{
    uint8_t row, col;
    fg_touch_rows(0, BAND_ROWS);
    for (row = 0; row < BAND_ROWS; row++) {
        volatile uint16_t *dst = &FG_MAP[MAP_INDEX(0, row)];
        for (col = 0; col < TEXT_COLS; col++)
            dst[col] = BAND_TILE;
    }
}

void fg_clear(void) BANKED
{
    fg_band();
    fg_clear_rows(BAND_ROWS, TEXT_LAST_ROW);
    fg_pics_forget();
    hud_reset();
}

void fg_picture(uint8_t tilemap, uint8_t col, uint8_t row) BANKED
{
    uint8_t h;
    for (h = tilemaps[tilemap].h; h; h--)
        fg_touch(row + h - 1);
    tm_draw(FG_MAP, tilemap, col, row);
}

static void band_print(uint8_t col, uint8_t row, uint8_t colour, const char *s)
{
    volatile uint16_t *dst = &FG_MAP[MAP_INDEX(col, row)];
    fg_touch(row);
    while (*s)
        *dst++ = FONT_OPAQUE_ENTRY(*s++, colour);
}

void opaque_print(uint8_t col, uint8_t row, uint8_t colour, const char *s) BANKED
{
    band_print(col, row, colour, s);
}

static const uint32_t pow10[10] = {
    1UL, 10UL, 100UL, 1000UL, 10000UL, 100000UL, 1000000UL, 10000000UL, 100000000UL, 1000000000UL,
};

void print_num(uint8_t col, uint8_t row, uint8_t colour, uint32_t value, uint8_t digits) BANKED
{
    static char buf[11];
    static uint8_t i;
    static uint32_t p;
    for (i = 9; i >= digits; i--) {
        p = pow10[i];
        while (value >= p)
            value -= p;
    }
    for (i = 0; i < digits; i++) {
        char c = '0';
        p = pow10[digits - 1 - i];
        while (value >= p) {
            value -= p;
            c++;
        }
        buf[i] = c;
    }
    buf[digits] = 0;
    if (row < BAND_ROWS || row == TEXT_LAST_ROW)
        band_print(col, row, colour, buf);
    else
        text_print(col, row, colour, buf);
}

void print_blank(uint8_t col, uint8_t row, uint8_t count) BANKED
{
    volatile uint16_t *dst = &FG_MAP[MAP_INDEX(col, row)];
    fg_touch(row);
    while (count--)
        *dst++ = row < BAND_ROWS ? BAND_TILE : 0;
}

void hud_reset(void) BANKED
{
    shown_hiscore = NONE;
    shown_score = NONE;
    shown_lives = 0xFF;
    shown_credits = 0xFF;
    shown_player = 0xFF;
}

void hud_hiscore(void) BANKED
{
    if (shown_hiscore == hiscore)
        return;
    if (shown_hiscore == NONE)
        fg_picture(TM_HIGH_SCORE, X8(hud_hiscore_pic_col), X8(hud_hiscore_pic_row));
    shown_hiscore = hiscore;
    print_num(X8(hud_hiscore_col), X8(hud_hiscore_row), X8(hud_hiscore_colour), hiscore, 6);
}

void hud_score(uint8_t always) BANKED
{
    if (shown_score == pl.score && shown_player == cur)
        return;
    shown_score = pl.score;
    shown_player = cur;
    if (!pl.score && !always) {
        print_blank(X8(hud_up_col), X8(hud_up_row), 3);
        print_blank(X8(hud_score_col), X8(hud_score_row), 6);
        return;
    }
    band_print(X8(hud_up_col), X8(hud_up_row), X8(hud_up_colour), cur ? "2UP" : "1UP");
    print_num(X8(hud_score_col), X8(hud_score_row), X8(hud_score_colour), pl.score, 6);
}

void hud_lives(void) BANKED
{
    uint8_t n = pl.lives < MAX_LIVES_SHOWN ? pl.lives : MAX_LIVES_SHOWN;
    uint8_t i;
    volatile uint16_t *row = &FG_MAP[MAP_INDEX(0, LIVES_ROW)];
    if (shown_lives == n)
        return;
    shown_lives = n;
    fg_touch(LIVES_ROW);
    if (n) {
        tm_row(TM_LIVES_1 + n - 1, 0, row, LIVES_COL, LIVES_COL, LIVES_COL + LIVES_COLS, 1);
        return;
    }
    for (i = LIVES_COL; i < LIVES_COL + LIVES_COLS; i++)
        row[i] = BAND_TILE;
}

#define PAUSE_COL 10
#define PAUSE_ROW 19
#define PAUSE_W 10
#define PAUSE_H 3
#define PAUSE_TEXT_COL 12
#define PAUSE_LEN 6
#define PAUSE_STEP_FRAMES 4
#define PAUSE_COLOURS 5

static const uint8_t pause_colours[PAUSE_COLOURS] = { C_WHITE, C_YELLOW, C_GREEN, C_CYAN, C_RED };
static const char pause_text[PAUSE_LEN] = { 'P', 'A', 'U', 'S', 'E', 'D' };
static uint16_t pause_saved[PAUSE_H][PAUSE_W];
static uint8_t pause_on, pause_frames, pause_step;

static void pause_box(uint8_t save)
{
    uint8_t r, c;
    for (r = 0; r < PAUSE_H; r++) {
        volatile uint16_t *dst = &FG_MAP[MAP_INDEX(PAUSE_COL, PAUSE_ROW + r)];
        fg_touch(PAUSE_ROW + r);
        for (c = 0; c < PAUSE_W; c++) {
            if (save) {
                pause_saved[r][c] = dst[c];
                dst[c] = BAND_TILE;
            } else {
                dst[c] = pause_saved[r][c];
            }
        }
    }
}

static void pause_letters(void)
{
    uint8_t i, k = pause_step;
    volatile uint16_t *dst = &FG_MAP[MAP_INDEX(PAUSE_TEXT_COL, PAUSE_ROW + 1)];
    fg_touch(PAUSE_ROW + 1);
    for (i = 0; i < PAUSE_LEN; i++) {
        dst[i] = FONT_OPAQUE_ENTRY(pause_text[i], pause_colours[k]);
        if (++k == PAUSE_COLOURS)
            k = 0;
    }
    pause_step = pause_step ? pause_step - 1 : PAUSE_COLOURS - 1;
}

uint8_t pause_update(void) BANKED
{
    if (!(PAUSE & PAUSE_ON)) {
        if (pause_on) {
            pause_on = 0;
            pause_box(0);
            sound_pause(0);
        }
        return 0;
    }
    if (!pause_on) {
        pause_on = 1;
        pause_frames = 0;
        pause_step = 0;
        sound_pause(1);
        pause_box(1);
    }
    if (!pause_frames--) {
        pause_frames = PAUSE_STEP_FRAMES - 1;
        pause_letters();
    }
    return 1;
}

void hud_credits(void) BANKED
{
    uint8_t show = free_play() ? FREE_SHOWN : credits;
    if (shown_credits == show)
        return;
    if (show == FREE_SHOWN) {
        if (shown_credits != 0xFF)
            print_blank(X8(hud_credits_bar_col), X8(hud_credits_bar_row), tilemaps[TM_BLACK_BAR].w);
        shown_credits = show;
        band_print(X8(hud_credits_col), X8(hud_credits_row), C_YELLOW, "FREE PLAY");
        return;
    }
    if (shown_credits >= FREE_SHOWN)
        fg_picture(TM_BLACK_BAR, X8(hud_credits_bar_col), X8(hud_credits_bar_row));
    shown_credits = show;
    band_print(X8(hud_credits_col), X8(hud_credits_row), X8(hud_credits_colour), exe_hud_credits);
    print_num(10, X8(hud_credits_row), X8(hud_credits_colour), credits, 2);
}

void hud_switch_text(uint8_t out_other) BANKED
{
    if (out_other) {
        text_print(X8(state_gop_col), X8(state_gop_row), X8(state_switch_red), exe_state_game_over_player);
        print_num(23, X8(state_gop_row), X8(state_switch_red), X8(state_gop_number_base) - cur, 1);
    }
    text_print(X8(state_ready_col), X8(state_ready_row), X8(state_switch_red), exe_state_player);
    print_num(15, X8(state_ready_row), X8(state_switch_red), cur + 1, 1);
    text_print(17, X8(state_ready_row), X8(state_switch_red), exe_state_ready);
}
