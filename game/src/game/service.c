/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "states.h"
#include "service.h"
#include "bg.h"
#include "bldg.h"
#include "hiscore.h"
#include "hud.h"
#include "input.h"
#include "sound.h"
#include "page.h"
#include "player.h"
#include "cutscene.h"
#include "exe.h"
#include "samples.h"

#ifdef __SDCC
#define IRQ_OFF() __asm__("di")
#define IRQ_ON() __asm__("ei")
#else
#define IRQ_OFF()
#define IRQ_ON()
#endif

#define NV_VERSION 1
#define NV_SET 0x00
#define NV_SET_LEN 16
#define NV_BK 0x10
#define NV_BK_LEN 32
#define NV_HI 0x30
#define NV_HI_LEN 48

#define K_R IN_RIGHT
#define K_L IN_LEFT
#define K_D IN_DOWN
#define K_U IN_UP
#define K_J IN_JUMP
#define K_F IN_FIX
#define REPEAT_FIRST 20
#define REPEAT_NEXT 6
#define TEXT_ROWS_ALL 40
#define NONE 0xFF

enum {
    PG_MENU, PG_RAMROM, PG_INPUT, PG_COINS, PG_SOUND, PG_SCREEN, PG_DIPS, PG_SETTINGS, PG_BOOKS, PG_HISCORE,
    PG_ABOUT, PG_EXIT
};

#define MENU_ITEMS 11
#define MENU_ROW 9
#define RAMS 7
#define CHIPS 4
#define SOUND_ROW 4
#define PATTERNS 9
#define SET_ROW 5
#define SET_OPTS (OPTS - 1)
#define SET_ITEMS (SET_OPTS + 1)
#define SET_VALUE_COL 18
#define BK_ROW 6

uint8_t opt[OPTS];

static const uint8_t opt_count[OPTS] = { 4, 4, 2, 4, 3, 2, 2, 2, 3, 2, 2 };

static uint8_t page;
static uint8_t sel;
static uint8_t menu_sel;
static uint8_t held, press, rep;
static uint8_t sys_prev, sys_press;
static uint8_t confirm;
static uint8_t step_n;
static uint16_t off;
static uint16_t fail;
static uint8_t sum;
static uint8_t shown[4];
static uint8_t pulse_timer, pulse_bit;
static uint16_t pulses[2];

#define NV_WAIT_FRAMES 90

static uint8_t nv_ready, nv_wait;

static uint8_t nv_sum(uint16_t at, uint8_t len)
{
    volatile uint8_t *p = NVRAM + at;
    uint8_t s = 0;
    while (len--)
        s += *p++;
    return s;
}

static uint8_t nv_valid(uint16_t at, uint8_t len)
{
    return NVRAM[at] == NV_VERSION && !nv_sum(at, len);
}

static void nv_seal(uint16_t at, uint8_t len)
{
    NVRAM[at] = NV_VERSION;
    NVRAM[at + len - 1] = 0;
    NVRAM[at + len - 1] = (uint8_t)-nv_sum(at, len);
}

static void nv_clear(uint16_t at, uint8_t len)
{
    uint8_t i;
    if (!nv_ready)
        return;
    for (i = 0; i < len; i++)
        NVRAM[at + i] = 0;
    nv_seal(at, len);
}

static void nv_load(void)
{
    uint8_t i, v, ok = nv_valid(NV_SET, NV_SET_LEN);
    for (i = 0; i < OPTS; i++) {
        v = ok ? NVRAM[NV_SET + 1 + i] : 0;
        opt[i] = v < opt_count[i] ? v : 0;
    }
    opt[OPT_BOOT] = 0;
}

static void nv_save_settings(void)
{
    uint8_t i;
    if (!nv_ready)
        return;
    for (i = 0; i < OPTS; i++)
        NVRAM[NV_SET + 1 + i] = opt[i];
    for (i = OPTS + 1; i < NV_SET_LEN; i++)
        NVRAM[NV_SET + i] = 0;
    nv_seal(NV_SET, NV_SET_LEN);
}

static uint32_t nv_long(uint16_t at)
{
    volatile uint8_t *p = NVRAM + at;
    return p[0] | ((uint16_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void nv_put_long(uint16_t at, uint32_t v)
{
    volatile uint8_t *p = NVRAM + at;
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t bk_get(uint8_t i)
{
    return nv_valid(NV_BK, NV_BK_LEN) ? nv_long(NV_BK + 1 + i * 4) : 0;
}

void nv_count(uint8_t counter) BANKED
{
    uint16_t at = NV_BK + 1 + counter * 4;
    if (!nv_ready)
        return;
    if (!nv_valid(NV_BK, NV_BK_LEN))
        nv_clear(NV_BK, NV_BK_LEN);
    nv_put_long(at, nv_long(at) + 1);
    nv_seal(NV_BK, NV_BK_LEN);
}

void nv_hiscore_load(void) BANKED
{
    uint8_t i, j;
    uint16_t at = NV_HI + 1;
    if (!opt[OPT_SAVE_HI] || !nv_valid(NV_HI, NV_HI_LEN))
        return;
    for (i = 0; i < HISCORES; i++) {
        for (j = 0; j < 3; j++) {
            char c = NVRAM[at++];
            hiscores[i].name[j] = c >= 0x20 && c < 0x60 ? c : ' ';
        }
        hiscores[i].name[3] = 0;
        hiscores[i].score = nv_long(at);
        at += 4;
    }
    hiscore = hiscores[0].score;
}

void nv_reload(void) BANKED
{
    nv_load();
    nv_ready = 1;
    nv_hiscore_load();
}

void nv_tick(void) BANKED
{
    if (!nv_ready && ++nv_wait >= NV_WAIT_FRAMES)
        nv_reload();
}

void nv_hiscore_save(void) BANKED
{
    uint8_t i, j;
    uint16_t at = NV_HI + 1;
    if (!opt[OPT_SAVE_HI] || !nv_ready)
        return;
    for (i = 0; i < HISCORES; i++) {
        for (j = 0; j < 3; j++)
            NVRAM[at++] = hiscores[i].name[j];
        nv_put_long(at, hiscores[i].score);
        at += 4;
    }
    nv_seal(NV_HI, NV_HI_LEN);
}

#ifdef __SDCC
static uint16_t ram_test(uint16_t addr, uint16_t len) __naked
{
    (void)addr;
    (void)len;
    __asm
1$:
    ld a, (hl)
    ld c, a
    ld (hl), #0x55
    ld a, (hl)
    cp a, #0x55
    jr nz, 2$
    ld (hl), #0xAA
    ld a, (hl)
    cp a, #0xAA
    jr nz, 2$
    ld (hl), c
    inc hl
    dec de
    ld a, d
    or a, e
    jr nz, 1$
    ret
2$:
    ld (hl), c
    ex de, hl
    ret
    __endasm;
}
#else
static uint16_t ram_test(uint16_t addr, uint16_t len)
{
    (void)addr;
    (void)len;
    return 0;
}
#endif

static void pr(uint8_t col, uint8_t row, uint8_t colour, const char *s)
{
    text_print(col, row, colour, s);
}

static void centre(uint8_t row, uint8_t colour, const char *s)
{
    pr((TEXT_COLS - (uint8_t)strlen(s)) / 2, row, colour, s);
}

static void hex(uint8_t col, uint8_t row, uint8_t colour, uint16_t v, uint8_t digits)
{
    char buf[5];
    uint8_t i = digits, d;
    buf[digits] = 0;
    while (i--) {
        d = v & 15;
        buf[i] = d < 10 ? '0' + d : 'A' - 10 + d;
        v >>= 4;
    }
    pr(col, row, colour, buf);
}

static void bits(uint8_t col, uint8_t row, uint8_t v)
{
    char buf[9];
    uint8_t i;
    for (i = 0; i < 8; i++, v >>= 1)
        buf[i] = v & 1 ? '1' : '0';
    buf[8] = 0;
    pr(col, row, C_WHITE, buf);
}

static void field(uint8_t col, uint8_t row, uint8_t colour, uint8_t width, const char *s)
{
    print_blank(col, row, width);
    pr(col, row, colour, s);
}

static void cls(void)
{
    fg_clear_rows(0, TEXT_LAST_ROW);
}

static void cursor(uint8_t col, uint8_t row0, uint8_t spacing, uint8_t from, uint8_t to)
{
    if (from != NONE)
        pr(col, row0 + from * spacing, C_WHITE, " ");
    pr(col, row0 + to * spacing, C_YELLOW, ">");
}

static uint8_t move_sel(uint8_t count)
{
    uint8_t old = sel;
    if (press & K_U)
        sel = sel ? sel - 1 : count - 1;
    if (press & K_D)
        sel = sel + 1 < count ? sel + 1 : 0;
    return old;
}

static void hints(const char *a, const char *b)
{
    centre(36, C_CYAN, a);
    centre(38, C_CYAN, b);
}

static const char *const menu_items[MENU_ITEMS] = {
    "RAM AND ROM TEST", "INPUT TEST", "COIN COUNTER TEST", "SOUND TEST", "SCREEN TEST",
    "DIP SWITCHES", "EXTRA SETTINGS", "BOOKKEEPING", "HIGH SCORES", "ABOUT", "EXIT",
};

static void menu_enter(void)
{
    uint8_t i;
    centre(3, C_WHITE, "FIX-IT FELIX JR.");
    centre(5, C_RED, "SERVICE MODE");
    for (i = 0; i < MENU_ITEMS; i++)
        pr(7, MENU_ROW + 2 * i, C_WHITE, menu_items[i]);
    sel = menu_sel;
    cursor(5, MENU_ROW, 2, NONE, sel);
    centre(32, C_GREEN, "TOBIKOMI BOARD REV B");
    hints("UP DOWN SELECT  JUMP ENTER", "TEST SWITCH LEAVES");
}

static void page_enter(uint8_t p);

static void menu_tick(void)
{
    uint8_t old = move_sel(MENU_ITEMS);
    if (old != sel)
        cursor(5, MENU_ROW, 2, old, sel);
    if (press & K_J) {
        menu_sel = sel;
        page_enter(PG_RAMROM + sel);
    }
}

typedef struct {
    const char *name;
    uint16_t addr;
    uint16_t len;
    uint8_t page;
} ram_t;

static const ram_t rams[RAMS] = {
    { "WORK RAM", 0xC000, 0x1000, NONE }, { "BG RAM 0", 0xD000, 0x1000, 0 },
    { "BG RAM 1", 0xD000, 0x1000, 1 },    { "FG RAM 0", 0xE000, 0x1000, 0 },
    { "FG RAM 1", 0xE000, 0x1000, 1 },    { "OBJ RAM", 0xF000, 0x0400, NONE },
    { "NVRAM", 0xF400, 0x0400, NONE },
};
static const char *const chips[CHIPS] = { "FJ-P1.1D", "FJ-P2.1E", "FJ-P3.1F", "FJ-P4.1G" };

#define RT_ROW 6
#define RT_ROM_ROW 22
#define RAM_CHUNK 512

static uint8_t rt_row(uint8_t item)
{
    return item < RAMS ? RT_ROW + 2 * item : RT_ROM_ROW + 2 * (item - RAMS);
}

static void ramrom_start(void)
{
    uint8_t i;
    for (i = 0; i < RAMS + CHIPS; i++)
        print_blank(22, rt_row(i), 8);
    step_n = 0;
    off = 0;
    fail = 0;
    sum = 0;
}

static void ramrom_enter(void)
{
    uint8_t i, row;
    centre(2, C_WHITE, "RAM AND ROM TEST");
    for (i = 0; i < RAMS; i++) {
        row = rt_row(i);
        pr(1, row, C_WHITE, rams[i].name);
        hex(11, row, C_WHITE, rams[i].addr, 4);
        pr(15, row, C_WHITE, "-");
        hex(16, row, C_WHITE, rams[i].addr + rams[i].len - 1, 4);
    }
    for (i = 0; i < CHIPS; i++) {
        row = rt_row(RAMS + i);
        pr(1, row, C_WHITE, chips[i]);
        hex(10, row, C_WHITE, (uint16_t)i << 3, 2);
        pr(12, row, C_WHITE, "000-");
        hex(16, row, C_WHITE, ((uint16_t)i << 3) + 7, 2);
        pr(18, row, C_WHITE, "FFF");
    }
    hints("JUMP TEST AGAIN", "FIX RETURN");
    ramrom_start();
}

static void ramrom_result(uint8_t ok)
{
    uint8_t row = rt_row(step_n);
    if (ok) {
        pr(22, row, C_GREEN, "OK");
    } else if (step_n < RAMS) {
        pr(22, row, C_RED, "NG");
        hex(25, row, C_RED, fail, 4);
    } else {
        pr(22, row, C_RED, "NG");
        hex(25, row, C_RED, sum, 2);
    }
    step_n++;
    off = 0;
    fail = 0;
    sum = 0;
}

static void ramrom_tick(void)
{
    const ram_t *r;
    uint16_t bad;
    if (press & K_J) {
        ramrom_start();
        return;
    }
    if (step_n < RAMS) {
        r = &rams[step_n];
        if (!off)
            pr(22, rt_row(step_n), C_YELLOW, "..");
        IRQ_OFF();
        if (r->page != NONE)
            FG_PAGE = pg_show | (r->page ? PG_FG_CPU | PG_BG_CPU : 0);
        bad = ram_test(r->addr + off, RAM_CHUNK);
        pg_hidden();
        IRQ_ON();
        if (bad && !fail)
            fail = bad;
        off += RAM_CHUNK;
        if (off >= r->len)
            ramrom_result(!fail);
    } else if (step_n < RAMS + CHIPS) {
        if (!off)
            pr(22, rt_row(step_n), C_YELLOW, "..");
        sum += rom_sum_slice((uint8_t)((step_n - RAMS) * 8 + off));
        if (++off == 8)
            ramrom_result(!sum);
    }
}

static const uint8_t pad_bits[6] = { IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_JUMP, IN_FIX };
static const char *const pad_names[6] = { "UP", "DOWN", "LEFT", "RIGHT", "JUMP", "FIX" };
static const uint8_t sys_bits[6] = { SYS_COIN_A, SYS_COIN_B, SYS_SERVICE, SYS_START1, SYS_START2, SYS_TEST };
static const char *const sys_names[6] = { "COIN A", "COIN B", "SERVICE", "1P START", "2P START", "TEST" };

#define IN_ROW 7
#define SYS_ROW 21

static void on_off(uint8_t col, uint8_t row, uint8_t on)
{
    pr(col, row, on ? C_YELLOW : C_BLUE, on ? "ON" : "--");
}

static uint8_t hold;

static void input_enter(void)
{
    uint8_t i;
    centre(2, C_WHITE, "INPUT TEST");
    pr(13, 5, C_WHITE, "1P    2P");
    for (i = 0; i < 6; i++) {
        pr(3, IN_ROW + 2 * i, C_WHITE, pad_names[i]);
        pr(3, SYS_ROW + 2 * i, C_WHITE, sys_names[i]);
    }
    pr(3, 34, C_WHITE, "IN0");
    pr(10, 34, C_WHITE, "IN1");
    pr(17, 34, C_WHITE, "IN2");
    hints("HOLD JUMP + FIX", "1 SECOND RETURN");
    shown[0] = shown[1] = shown[2] = NONE;
    hold = 0;
}

static void input_tick(void)
{
    uint8_t in0 = IN0, in1 = IN1, in2 = IN2 & SYS_INPUTS, i;
    if ((in0 & (IN_JUMP | IN_FIX)) != (IN_JUMP | IN_FIX))
        hold = 0;
    else if (++hold == 60) {
        page_enter(PG_MENU);
        return;
    }
    if (in0 == shown[0] && in1 == shown[1] && in2 == shown[2])
        return;
    shown[0] = in0;
    shown[1] = in1;
    shown[2] = in2;
    for (i = 0; i < 6; i++) {
        on_off(13, IN_ROW + 2 * i, in0 & pad_bits[i]);
        on_off(19, IN_ROW + 2 * i, in1 & pad_bits[i]);
        on_off(13, SYS_ROW + 2 * i, in2 & sys_bits[i]);
    }
    hex(7, 34, C_YELLOW, in0, 2);
    hex(14, 34, C_YELLOW, in1, 2);
    hex(21, 34, C_YELLOW, in2, 2);
}

#define CTR_ROW 10
#define PULSE_FRAMES 6

static void coins_show(uint8_t i)
{
    print_num(22, CTR_ROW + 2 * i, C_YELLOW, pulses[i], 4);
}

static void coins_enter(void)
{
    centre(2, C_WHITE, "COIN COUNTER TEST");
    pr(7, 6, C_WHITE, "COUNTER");
    pr(21, 6, C_WHITE, "PULSES");
    pr(7, CTR_ROW, C_WHITE, "COIN A");
    pr(7, CTR_ROW + 2, C_WHITE, "COIN B");
    coins_show(0);
    coins_show(1);
    sel = 0;
    cursor(5, CTR_ROW, 2, NONE, 0);
    hints("JUMP PULSES THE COUNTER", "FIX RETURN");
}

static void coins_tick(void)
{
    uint8_t old = move_sel(2);
    if (old != sel)
        cursor(5, CTR_ROW, 2, old, sel);
    if ((press & K_J) && !pulse_timer) {
        pulse_bit = 1 << sel;
        pulse_timer = PULSE_FRAMES;
        pulses[sel]++;
        coins_show(sel);
    }
}

static const char *const sound_names[SAMPLES] = {
    "FELIX HIT", "HAMMER", "PIE EATEN", "BRICK DEFLECTED", "COIN", "WINDOW BREAKS", "HEAD BONK",
    "STORY MUSIC 1", "STORY MUSIC 2", "INTRO MUSIC", "INTRO END", "ROOF MUSIC 1", "ROOF MUSIC 2",
    "LEVEL COMPLETE", "GAME MUSIC", "JUMP", "RALPH POUNDS", "DROP", "RALPH LANDS", "RALPH FALLS",
    "START", "GAME OVER", "FLOOR CLEAR", "RALPH THROWN", "SUPER FELIX", "EXTRA LIFE", "MEDAL",
    "NICELANDER STEP", "INTRO IMPACT", "RALPH CLIMBS", "NAME ENTRY", "PLAYER CHANGE",
    "VOICE: WRECK IT", "VOICE: FIX IT",
};

static void sound_enter(void)
{
    uint8_t i;
    centre(1, C_WHITE, "SOUND TEST");
    for (i = 0; i < SAMPLES; i++) {
        print_num(3, SOUND_ROW + i, C_WHITE, i, 2);
        pr(6, SOUND_ROW + i, C_WHITE, sound_names[i]);
    }
    sel = 0;
    cursor(2, SOUND_ROW, 1, NONE, 0);
    centre(39, C_CYAN, "JUMP PLAYS  FIX RETURNS");
    shown[0] = NONE;
}

static void sound_page_tick(void)
{
    uint8_t old = move_sel(SAMPLES), busy = music_current() != NONE && (X1_CTRL(0) & X1_KEYON);
    if (old != sel)
        cursor(2, SOUND_ROW, 1, old, sel);
    if (press & K_J) {
        music_stop();
        music_play(sel, 0);
    }
    if (busy != shown[0]) {
        shown[0] = busy;
        field(23, 1, C_YELLOW, 7, busy ? "PLAYING" : "");
    }
}

#define SOLID(pen, pal) ((uint16_t)(SVC_SOLID_TILE + (pen) - 1) | ((uint16_t)(pal) << 11))

static const uint8_t bar_pens[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t full_pens[4] = { 1, 6, 4, 7 };

static void pattern(uint8_t n)
{
    static uint16_t line[TEXT_COLS];
    uint8_t row, col, f;
    uint16_t e = 0;
    volatile uint16_t *dst;
    fg_touch_rows(0, TEXT_ROWS_ALL);
    for (row = 0; row < TEXT_ROWS_ALL; row++) {
        switch (n) {
        case 0:
            f = (row & 1 ? 16 : 1) | (row == TEXT_LAST_ROW ? 4 : 0);
            for (col = 0; col < TEXT_COLS; col++)
                line[col] = (uint16_t)(SVC_GRID_TILE + (col & 1 ? f & 21 : (f & 5) | 2)) | ((uint16_t)SVC_PAL << 11);
            line[TEXT_COLS - 1] |= 8;
            break;
        case 1:
            if (row % 5 == 0)
                e = SOLID(bar_pens[row / 5], SVC_PAL);
            break;
        case 2:
            if (row % 10 == 0)
                for (col = 0; col < TEXT_COLS; col++)
                    line[col] = SOLID(col / 2 + 1, SVC_PAL + 1 + row / 10);
            break;
        case 3:
            for (col = 0; col < TEXT_COLS; col++)
                line[col] = row >= 4 && row < 36 && col > 6 && col < 22 ? SOLID(col - 6, row - 4) : 0;
            break;
        case 8:
            e = 0;
            break;
        default:
            e = SOLID(full_pens[n - 4], SVC_PAL);
            break;
        }
        dst = &FG_MAP[MAP_INDEX(0, row)];
        if (n == 0 || n == 2 || n == 3) {
            for (col = 0; col < TEXT_COLS; col++)
                *dst++ = line[col];
        } else {
            for (col = 0; col < TEXT_COLS; col++)
                *dst++ = e;
        }
    }
    if (n == 3) {
        centre(1, C_WHITE, "TILE PALETTES");
        pr(6, 2, C_WHITE, "0123456789ABCDEF");
        for (row = 0; row < 32; row++)
            hex(2, row + 4, C_WHITE, row, 2);
    }
}

static void screen_enter(void)
{
    sel = 0;
    pattern(0);
}

static void screen_tick(void)
{
    if (press & (K_J | K_R | K_D)) {
        sel = sel + 1 < PATTERNS ? sel + 1 : 0;
        pattern(sel);
    } else if (press & (K_L | K_U)) {
        sel = sel ? sel - 1 : PATTERNS - 1;
        pattern(sel);
    }
}

static const char *const lives_names[4] = { exe_service_lives_text, "2", "4", "5" };
static const char *const bonus_names[4] = { exe_service_bonus_text, "EVERY 10000", "EVERY 20000", "NONE" };
static const char *const diff_names[4] = { "NORMAL", "EASY", "HARD", "HARDEST" };
static const char *const coin_names[10] = {
    "1 COIN 1 CREDIT", "2 COINS 1 CREDIT", "3 COINS 1 CREDIT", "4 COINS 1 CREDIT", "1 COIN 2 CREDITS",
    "1 COIN 3 CREDITS", "1 COIN 4 CREDITS", "1 COIN 5 CREDITS", "1 COIN 6 CREDITS", "FREE PLAY",
};

#define DIP_VALUE_COL 14
#define DIP_W (TEXT_COLS - DIP_VALUE_COL)

static void dip_value(uint8_t row, const char *s)
{
    field(DIP_VALUE_COL, row, C_YELLOW, DIP_W, s);
}

static void dips_enter(void)
{
    centre(2, C_WHITE, "DIP SWITCHES");
    pr(DIP_VALUE_COL, 5, C_CYAN, "12345678");
    pr(1, 7, C_WHITE, "DSW1");
    pr(1, 9, C_WHITE, "LIVES");
    pr(1, 11, C_WHITE, "BONUS LIFE");
    pr(1, 13, C_WHITE, "DIFFICULTY");
    pr(1, 15, C_WHITE, "DEMO SOUNDS");
    pr(1, 17, C_WHITE, "CABINET");
    pr(1, 20, C_WHITE, "DSW2");
    pr(1, 22, C_WHITE, "COIN A");
    pr(1, 24, C_WHITE, "COIN B");
    pr(1, 27, C_WHITE, "DSW3");
    pr(1, 29, C_WHITE, "INVULNERABLE");
    centre(32, C_GREEN, "1 = SWITCH ON");
    hints("SET WITH THE DIP SWITCHES", "FIX RETURN");
    shown[0] = shown[1] = shown[2] = NONE;
    shown[3] = 0;
}

static void dips_tick(void)
{
    uint8_t d1 = DSW1, d2 = DSW2, d3 = DSW3, a = d2 & 15, b = d2 >> 4;
    if (d1 != shown[0] || !shown[3]) {
        bits(DIP_VALUE_COL, 7, d1);
        dip_value(9, lives_names[d1 & 3]);
        dip_value(11, bonus_names[(d1 >> 2) & 3]);
        dip_value(13, diff_names[(d1 >> 4) & 3]);
        dip_value(15, d1 & 0x40 ? "OFF" : "ON");
        dip_value(17, d1 & 0x80 ? "COCKTAIL" : "UPRIGHT");
    }
    if (d2 != shown[1] || !shown[3]) {
        bits(DIP_VALUE_COL, 20, d2);
        dip_value(22, coin_names[a > 9 ? 9 : a]);
        dip_value(24, coin_names[b > 8 ? 0 : b]);
    }
    if (d3 != shown[2] || !shown[3]) {
        bits(DIP_VALUE_COL, 27, d3);
        dip_value(29, d3 & 1 ? "ON" : "OFF");
    }
    shown[0] = d1;
    shown[1] = d2;
    shown[2] = d3;
    shown[3] = 1;
}

static const char *const set_names[SET_ITEMS] = {
    "HAMMER HITS", "STARTING LEVEL", "LEVEL SPEED-UP", "SUPER FELIX", "BIRDS, BLOCKS", "KILL SCREEN",
    "CONTINUE", "ATTRACT INTRO", "VOICES", "SAVE HI SCORES", "RESTORE DEFAULTS",
};
static const uint8_t set_opt[SET_OPTS] = {
    OPT_HAMMER, OPT_START, OPT_SPEED, OPT_SUPER, OPT_HAZARDS, OPT_KILL, OPT_CONTINUE, OPT_INTRO, OPT_VOICES,
    OPT_SAVE_HI,
};
static const char *const set_values[] = {
    "1", "2", "3", "BY DAMAGE",
    "1", "3", "5", "9",
    "OFF", "ON",
    exe_service_super_text, "5 SEC", "12 SEC", "NO PIES",
    "PER LEVEL", "OFF", "FROM LEVEL 1",
    exe_service_kill_text, "OFF",
    "NO", "YES",
    "FULL", "SHORT", "1 IN 5 FULL",
    "ON", "OFF",
    "OFF", "ON",
};
static const uint8_t set_first[SET_OPTS] = { 0, 4, 8, 10, 14, 17, 19, 21, 24, 26 };

static void set_show(uint8_t i)
{
    uint8_t v = opt[set_opt[i]];
    field(SET_VALUE_COL, SET_ROW + 2 * i, v ? C_YELLOW : C_WHITE, TEXT_COLS - SET_VALUE_COL,
          set_values[set_first[i] + v]);
}

static void settings_enter(void)
{
    uint8_t i;
    centre(2, C_WHITE, "EXTRA SETTINGS");
    for (i = 0; i < SET_ITEMS; i++)
        pr(3, SET_ROW + 2 * i, i < SET_OPTS ? C_WHITE : C_RED, set_names[i]);
    for (i = 0; i < SET_OPTS; i++)
        set_show(i);
    sel = 0;
    cursor(2, SET_ROW, 2, NONE, 0);
    centre(30, C_GREEN, "WHITE = ORIGINAL GAME");
    hints("LEFT RIGHT CHANGES", "FIX RETURN");
}

static void settings_tick(void)
{
    uint8_t old = move_sel(SET_ITEMS), n, o;
    if (old != sel)
        cursor(2, SET_ROW, 2, old, sel);
    if (sel == SET_OPTS) {
        if (press & K_J) {
            for (n = 0; n < OPTS; n++)
                opt[n] = 0;
            for (n = 0; n < SET_OPTS; n++)
                set_show(n);
            nv_save_settings();
        }
        return;
    }
    o = set_opt[sel];
    n = opt_count[o];
    if (press & K_R)
        opt[o] = opt[o] + 1 < n ? opt[o] + 1 : 0;
    else if (press & K_L)
        opt[o] = opt[o] ? opt[o] - 1 : n - 1;
    else
        return;
    set_show(sel);
    nv_save_settings();
}

static const char *const bk_names[BK_COUNTERS - 1] = {
    "COIN A", "COIN B", "SERVICE CREDITS", "1P GAMES", "2P GAMES", "CONTINUES",
};

static void time_show(uint8_t row, uint32_t secs)
{
    uint32_t h = secs / 3600;
    uint16_t rest = (uint16_t)(secs - h * 3600), m = rest / 60;
    print_num(19, row, C_YELLOW, h, 4);
    pr(23, row, C_YELLOW, ":");
    print_num(24, row, C_YELLOW, m, 2);
    pr(26, row, C_YELLOW, ":");
    print_num(27, row, C_YELLOW, rest - m * 60, 2);
}

static void books_show(void)
{
    uint8_t i;
    uint32_t games = bk_get(BK_PLAY1) + bk_get(BK_PLAY2), secs = bk_get(BK_SECONDS);
    for (i = 0; i < BK_COUNTERS - 1; i++)
        print_num(21, BK_ROW + 2 * i, C_YELLOW, bk_get(i), 8);
    time_show(BK_ROW + 12, secs);
    time_show(BK_ROW + 14, games ? secs / games : 0);
}

static void books_enter(void)
{
    uint8_t i;
    centre(2, C_WHITE, "BOOKKEEPING");
    for (i = 0; i < BK_COUNTERS - 1; i++)
        pr(2, BK_ROW + 2 * i, C_WHITE, bk_names[i]);
    pr(2, BK_ROW + 12, C_WHITE, "PLAY TIME");
    pr(2, BK_ROW + 14, C_WHITE, "AVERAGE GAME");
    pr(19, BK_ROW + 16, C_GREEN, "HOURS:MM:SS");
    pr(4, 28, C_RED, "RESET COUNTERS");
    cursor(2, 28, 0, NONE, 0);
    confirm = 0;
    books_show();
    hints("JUMP TWICE RESETS", "FIX RETURN");
}

static uint8_t confirmed(uint8_t row)
{
    if (!(press & K_J))
        return 0;
    if (!confirm) {
        confirm = 1;
        centre(row, C_YELLOW, "JUMP AGAIN TO CONFIRM");
        return 0;
    }
    confirm = 0;
    print_blank(0, row, TEXT_COLS);
    return 1;
}

static void books_tick(void)
{
    if (confirmed(30)) {
        nv_clear(NV_BK, NV_BK_LEN);
        books_show();
    }
}

static const uint8_t table_colours[HISCORES] = { C_WHITE, C_YELLOW, C_CYAN, C_GREEN, C_BLUE };

static void hiscore_show(void)
{
    uint8_t i;
    char line[4];
    for (i = 0; i < HISCORES; i++) {
        memcpy(line, hiscores[i].name, 3);
        line[3] = 0;
        pr(8, 8 + 2 * i, table_colours[i], line);
        print_num(15, 8 + 2 * i, table_colours[i], hiscores[i].score, 6);
    }
}

static void hiscore_enter(void)
{
    centre(2, C_WHITE, "HIGH SCORES");
    pr(8, 6, C_RED, "NAME   SCORE");
    hiscore_show();
    pr(3, 21, C_WHITE, "SAVED IN NVRAM");
    pr(19, 21, C_YELLOW, opt[OPT_SAVE_HI] ? "YES" : "NO");
    pr(4, 26, C_RED, "RESET HIGH SCORES");
    cursor(2, 26, 0, NONE, 0);
    confirm = 0;
    hints("JUMP TWICE RESETS", "FIX RETURN");
}

static void hiscore_tick(void)
{
    if (confirmed(28)) {
        NVRAM[NV_HI] = 0;
        hiscore_defaults();
        nv_hiscore_save();
        hiscore_show();
    }
}

static void svc_leave(void)
{
    COIN_COUNTER = 0;
    sound_stop_all();
    game_reset();
    game_state = ST_BOOT;
}

#define ABOUT_LINES 14
#define ABOUT_ROW 6

static const char *const about_text[ABOUT_LINES] = {
    "FIX-IT FELIX JR.",
    "TOBIKOMI BOARD REV B",
    "",
    "GAME CONVERTED FROM THE",
    "ORIGINAL WINDOWS VERSION",
    "",
    "BOARD DESIGN, MISTER CORE",
    "AND ROM CONVERSION BY",
    "CHRIS WATSON (NIKNAK) 2026",
    "",
    "BUILT ON JTFRAME BY",
    "JOSE TEJADA (JOTEGO)",
    "SOUND: SETA X1-010 MODULE",
    "BY JOSE TEJADA (JOTEGO)",
};

static void about_enter(void)
{
    uint8_t i;
    centre(2, C_WHITE, "ABOUT");
    for (i = 0; i < ABOUT_LINES; i++)
        centre(ABOUT_ROW + 2 * i, C_YELLOW, about_text[i]);
    centre(38, C_CYAN, "FIX RETURNS");
}

static void page_enter(uint8_t p)
{
    if (page == PG_SOUND)
        sound_stop_all();
    page = p;
    cls();
    switch (p) {
    case PG_MENU: menu_enter(); break;
    case PG_RAMROM: ramrom_enter(); break;
    case PG_INPUT: input_enter(); break;
    case PG_COINS: coins_enter(); break;
    case PG_SOUND: sound_enter(); break;
    case PG_SCREEN: screen_enter(); break;
    case PG_DIPS: dips_enter(); break;
    case PG_SETTINGS: settings_enter(); break;
    case PG_BOOKS: books_enter(); break;
    case PG_HISCORE: hiscore_enter(); break;
    case PG_ABOUT: about_enter(); break;
    default: svc_leave(); break;
    }
}

static void read_keys(void)
{
    uint8_t raw = IN0 | IN1, sys = IN2 & SYS_INPUTS;
    press = raw & (uint8_t)~held;
    if (!(raw & (K_U | K_D | K_L | K_R)) || (press & (K_U | K_D | K_L | K_R))) {
        rep = REPEAT_FIRST;
    } else if (!--rep) {
        rep = REPEAT_NEXT;
        press |= raw & (K_U | K_D | K_L | K_R);
    }
    held = raw;
    sys_press = sys & (uint8_t)~sys_prev;
    sys_prev = sys;
}

static void screen_update(void)
{
    bg_sync();
    pg_catch_up(4);
    while (IN2 & SYS_COMMIT_PENDING)
        ;
    statics_update(0);
    sprites_begin();
    sprites_finish();
}

static void enter_now(void)
{
    sound_stop_all();
    sound_mute(0);
    demo_input_stop();
    demo = 0;
    cut_free();
    lvlend_free();
    shards_clear();
    player_free();
    bg_reset();
    scroll_x = scroll_y = 0;
    bg_dy = 0;
    COIN_COUNTER = 0;
    fg_clear_rows(0, TEXT_LAST_ROW);
    fg_pics_forget();
    hud_reset();
    held = IN0 | IN1;
    sys_prev = IN2 & SYS_INPUTS;
    rep = REPEAT_FIRST;
    pulse_timer = 0;
    game_state = ST_SERVICE;
    state_pending = 0;
    menu_sel = 0;
    page = PG_MENU;
    page_enter(PG_MENU);
}

void svc_enter(void) BANKED
{
    enter_now();
    screen_update();
}

void svc_boot(void) BANKED
{
    nv_load();
    game_reset();
    if (IN2 & SYS_TEST)
        enter_now();
}

void svc_frame(void) BANKED
{
    read_keys();
    dsw1 = DSW1;
    dsw2 = DSW2;
    dsw3 = DSW3;
    if ((sys_press & SYS_TEST) && page != PG_INPUT) {
        svc_leave();
        return;
    }
    if (page != PG_MENU && page != PG_INPUT && (press & K_F)) {
        page_enter(PG_MENU);
    } else {
        switch (page) {
        case PG_MENU: menu_tick(); break;
        case PG_RAMROM: ramrom_tick(); break;
        case PG_INPUT: input_tick(); break;
        case PG_COINS: coins_tick(); break;
        case PG_SOUND: sound_page_tick(); break;
        case PG_SCREEN: screen_tick(); break;
        case PG_DIPS: dips_tick(); break;
        case PG_SETTINGS: settings_tick(); break;
        case PG_BOOKS: books_tick(); break;
        case PG_HISCORE: hiscore_tick(); break;
        }
    }
    if (game_state != ST_SERVICE)
        return;
    if (pulse_timer)
        pulse_timer--;
    COIN_COUNTER = pulse_timer ? pulse_bit : 0;
    sound_tick();
    screen_update();
}
