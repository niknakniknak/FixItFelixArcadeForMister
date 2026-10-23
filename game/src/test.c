/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hw.h"
#include "gfx.h"
#include "video.h"
#include "samples.h"

#define WHITE 6
#define RED 5
#define YELLOW 4
#define CYAN 3
#define GREEN 2
#define BLUE 0

#define ANIM_FELIX_IDLE 4
#define ANIM_RALPH_STAND 20
#define ANIM_NICELANDER_BOB 50
#define ANIM_PIE 64
#define ANIM_NICELANDER_POP 44
#define NICELANDERS 6
#define ANIM_BULLDOZER 72

#define SAMPLE_COIN 4
#define SAMPLE_MUSIC 14
#define SAMPLE_HAMMER 1
#define SAMPLE_JUMP 15
#define SAMPLE_START 20
#define MUSIC_VOICE 0
#define FIRST_SFX_VOICE 1
#define WORK_TEST_BASE 0xC400
#define WORK_TEST_LEN 0x0A00
#define KEY_OFF_TRIES 8
#define WAIT_LOOPS 8

#define PIE_COLUMNS 8
#define PIE_ROWS 6
#define DYNAMIC_SPRITES 80
#define BEHIND_SPRITE (DYNAMIC_SPRITES - 1)
#define RECT_CRANE 191
#define FIXED_END 0x8000
#define WINDOW 0x8000
#define BANK_BYTES 0x4000
#define FIRST_BANK 2
#define BANKS 8
#define HOME_BANK 2
#define PAGE_ROW 1

static volatile uint16_t frame_count;
static uint8_t sfx_voice;
static uint8_t video_ctrl;
static uint8_t pages;

void vblank_handler(void)
{
    frame_count++;
}

static void wait_frame(void)
{
    uint16_t now = frame_count;
    while (frame_count == now)
        ;
}

static uint8_t ram_test(volatile uint8_t *mem, uint16_t size)
{
    uint16_t i;
    for (i = 0; i < size; i++)
        mem[i] = (uint8_t)(i ^ (i >> 8) ^ 0x5A);
    for (i = 0; i < size; i++)
        if (mem[i] != (uint8_t)(i ^ (i >> 8) ^ 0x5A))
            return 0;
    for (i = 0; i < size; i++)
        mem[i] = 0;
    return 1;
}

static uint8_t sum_bytes(const uint8_t *p, uint16_t n, uint8_t sum)
{
    while (n--)
        sum += *p++;
    return sum;
}

static uint8_t rom_sum(void)
{
    uint8_t bank, sum = sum_bytes((const uint8_t *)ROM_BASE, FIXED_END, 0);
    for (bank = FIRST_BANK; bank < BANKS; bank++) {
        ROM_BANK = bank;
        sum = sum_bytes((const uint8_t *)WINDOW, BANK_BYTES, sum);
    }
    ROM_BANK = HOME_BANK;
    return sum;
}

static uint8_t page_test(void)
{
    volatile uint16_t *probe = &FG_MAP[MAP_INDEX(31, 63)];
    uint8_t ok;
    FG_PAGE = FG_CPU | (FG_CPU << 3);
    ok = ram_test((volatile uint8_t *)FG_MAP, 0x1000) & ram_test((volatile uint8_t *)BG_MAP, 0x1000);
    FG_PAGE = 0;
    probe[0] = 0x1234;
    FG_PAGE = FG_CPU;
    if (probe[0] == 0x1234)
        ok = 0;
    FG_PAGE = FG_BOTH;
    probe[-1] = 0x4321;
    FG_PAGE = FG_CPU;
    if (probe[-1] != 0x4321)
        ok = 0;
    FG_PAGE = FG_BOTH;
    probe[0] = 0;
    probe[-1] = 0;
    FG_PAGE = FG_BOTH | (FG_BOTH << 3);
    return ok;
}

static void report(uint8_t row, const char *name, uint8_t ok)
{
    text_print(1, row, WHITE, name);
    text_print(14, row, ok ? GREEN : RED, ok ? "OK" : "NG");
}

static void self_test(void)
{
    uint8_t sum;
    report(2, "WORK RAM", ram_test((volatile uint8_t *)WORK_TEST_BASE, WORK_TEST_LEN));
    report(3, "BG RAM", ram_test((volatile uint8_t *)BG_MAP, 0x1000));
    report(4, "SPRITE RAM", ram_test((volatile uint8_t *)SPRITES, 0x400));
    report(6, "PAGE RAM", page_test());
    sum = rom_sum();
    report(5, "ROM", sum == 0);
    text_hex8(17, 5, CYAN, sum);
}

static void draw_pages(void)
{
    FG_PAGE = 0;
    text_print(1, PAGE_ROW, RED, "FG PAGE 0");
    FG_PAGE = FG_CPU;
    text_print(1, PAGE_ROW, GREEN, "FG PAGE 1");
    FG_PAGE = FG_BOTH;
}

static void draw_bg_page1(void)
{
    uint8_t row, col;
    FG_PAGE = FG_BOTH | (FG_CPU << 3);
    for (row = 0; row < MAP_ROWS; row++)
        for (col = 0; col < MAP_COLS; col++)
            BG_MAP[MAP_INDEX(col, row)] = FONT_ENTRY('0' + ((row + col) & 7), (row >> 3) % 7);
    FG_PAGE = FG_BOTH;
}

static void behind_sprite(int16_t x, int16_t y)
{
    const spr_part_t *p = &spr_parts[msprites[anims[ANIM_NICELANDER_BOB].frames[0].rect].first];
    sprite_set(BEHIND_SPRITE, (uint8_t)x, (uint16_t)y, p->code | ((p->attr & SPR_ATTR_CODE_HI) << 8),
               p->attr >> SPR_PAL_SHIFT);
    SPR_SIZE[BEHIND_SPRITE] = SPR_BEHIND;
}

static void draw_building(void)
{
    static const uint8_t sections[] = {
        TM_ROOF, TM_FLOOR_A, TM_FLOOR_B, TM_FLOOR_A, TM_FLOOR_B, TM_GROUND_A, TM_GROUND_B,
    };
    uint8_t i, w, row = 0;
    map_fill(BG_MAP, 0);
    for (i = 0; i < sizeof sections; i++) {
        map_draw(BG_MAP, sections[i], 1, row);
        if (sections[i] == TM_FLOOR_A)
            for (w = 0; w < 5; w++)
                map_draw(BG_MAP, TM_FRAME0, 4 + w * 5, row);
        row += tilemaps[sections[i]].h;
    }
    map_draw(BG_MAP, TM_DOOR0, 12, row - 8);
}

static void draw_labels(void)
{
    map_fill(FG_MAP, 0);
    text_print(1, 0, YELLOW, "TOBIKOMI BOARD TEST");
    text_print(1, 34, BLUE, "BLUE");
    text_print(6, 34, GREEN, "GREEN");
    text_print(12, 34, CYAN, "CYAN");
    text_print(17, 34, YELLOW, "YELLOW");
    text_print(24, 34, RED, "RED");
    text_print(1, 36, WHITE, "IN0");
    text_print(8, 36, WHITE, "IN1");
    text_print(15, 36, WHITE, "IN2");
    text_print(1, 37, WHITE, "DSW1");
    text_print(9, 37, WHITE, "DSW2");
    text_print(17, 37, WHITE, "SND");
    text_print(1, 38, WHITE, "FRAME");
    text_print(12, 38, WHITE, "SPRITES");
}

typedef struct {
    uint8_t start;
    uint8_t end;
    uint8_t step;
    uint8_t bank;
} sample_info_t;

static const sample_info_t sample_info[SAMPLES] = SAMPLE_TABLE;

static void key_wait(void)
{
    volatile uint8_t i;
    for (i = 0; i < WAIT_LOOPS; i++) {
    }
}

static void sound_start(uint8_t voice, uint8_t sample)
{
    const sample_info_t *info = &sample_info[sample];
    uint8_t tries = KEY_OFF_TRIES;
    while ((X1_CTRL(voice) & X1_KEYON) && tries--) {
        X1_CTRL(voice) = 0;
        key_wait();
    }
    if (info->bank != SAMPLE_BANK_ANY)
        SOUND_BANK = info->bank;
    X1_VOL(voice) = X1_VOL_FULL;
    X1_FREQ(voice) = info->step;
    X1_START(voice) = info->start;
    X1_END(voice) = info->end;
    X1_CTRL(voice) = X1_KEYON;
}

static uint8_t sound_status(void)
{
    uint8_t v, status = 0;
    for (v = 0; v < VOICES; v++)
        if (X1_CTRL(v) & X1_KEYON)
            status |= (uint8_t)(1 << v);
    return status;
}

static void show_inputs(uint8_t sprites_used)
{
    text_hex8(5, 36, CYAN, IN0);
    text_hex8(12, 36, CYAN, IN1);
    text_hex8(19, 36, CYAN, IN2);
    text_hex8(6, 37, CYAN, DSW1);
    text_hex8(14, 37, CYAN, DSW2);
    text_hex8(21, 37, CYAN, sound_status());
    text_hex16(7, 38, CYAN, frame_count);
    text_hex8(20, 38, CYAN, sprites_used);
}

static void play_sfx(uint8_t sample)
{
    sound_start(FIRST_SFX_VOICE + sfx_voice, sample);
    if (++sfx_voice == VOICES - FIRST_SFX_VOICE)
        sfx_voice = 0;
}

static uint8_t pressed(uint8_t now, uint8_t before, uint8_t mask)
{
    return (now & mask) && !(before & mask);
}

static void handle_buttons(uint8_t p1, uint8_t p1_old, uint8_t sys, uint8_t sys_old)
{
    if (pressed(sys, sys_old, SYS_COIN_A | SYS_COIN_B | SYS_SERVICE)) {
        play_sfx(SAMPLE_COIN);
        COIN_COUNTER = sys & (SYS_COIN_A | SYS_COIN_B);
    } else {
        COIN_COUNTER = 0;
    }
    if (pressed(sys, sys_old, SYS_START1 | SYS_START2))
        play_sfx(SAMPLE_START);
    if (pressed(p1, p1_old, IN_FIX))
        play_sfx(SAMPLE_HAMMER);
    if (pressed(p1, p1_old, IN_JUMP))
        play_sfx(SAMPLE_JUMP);
}

static const anim_frame_t *anim_frame(uint8_t anim, uint16_t ticks)
{
    const anim_t *a = &anims[anim];
    return &a->frames[(ticks / (a->frames[0].dur ? a->frames[0].dur : 1)) % a->count];
}

static void draw_window_heads(void)
{
    uint8_t n;
    for (n = 0; n < NICELANDERS; n++)
        spr_frame(&anims[ANIM_NICELANDER_POP + n].frames[1], 40 + n * 32, 186, 0, 0, 0);
}

static void draw_pies(void)
{
    const anim_frame_t *f = anim_frame(ANIM_PIE, 0);
    uint8_t row, col;
    spr_begin(DYNAMIC_SPRITES, SPRITE_COUNT - DYNAMIC_SPRITES);
    for (row = 0; row < PIE_ROWS; row++)
        for (col = 0; col < PIE_COLUMNS; col++)
            spr_frame(f, 12 + col * 28 + (row & 1) * 12, 40 + row * 18, 0, 0, 0);
    spr_end();
}

void main(void)
{
    int16_t felix_x = 120, felix_y = 260;
    uint8_t mirrored = 0, step = 0, p1_old = 0, sys_old = 0, used = 0;

    uint8_t i;
    int16_t crane_x = -160;

    video_ctrl = 0;
    VIDEO_CTRL = video_ctrl;
    FG_PAGE = FG_BOTH;
    for (i = 0; i < SPRITE_COUNT; i++)
        SPR_SIZE[i] = 0;
    spr_begin(0, SPRITE_COUNT);
    spr_end();
    FG_PAGE = FG_BOTH | (FG_BOTH << 3);
    draw_labels();
    self_test();
    draw_building();
    draw_bg_page1();
    draw_pages();
    draw_pies();
    video_ctrl = VIDEO_BG | VIDEO_FG | VIDEO_SPRITES | VIDEO_COMMIT;
    interrupts_on();

    SOUND_BANK = 0;
    sound_start(MUSIC_VOICE, SAMPLE_MUSIC);
    play_sfx(SAMPLE_COIN);

    for (;;) {
        uint8_t p1 = IN0, sys = IN2;
        uint8_t cycle = super_cycle[step];
        wait_frame();
        if (!(X1_CTRL(MUSIC_VOICE) & X1_KEYON))
            sound_start(MUSIC_VOICE, SAMPLE_MUSIC);
        VIDEO_CTRL = video_ctrl | (DSW1 & 0x80 ? VIDEO_FLIP : 0);
        bg_scroll(0, (frame_count >> 1) & 0x1FF);
        if ((frame_count & 31) == 0)
            pages ^= 1;
        if ((frame_count & 255) == 0)
            pages ^= FG_SHOW << 3;
        FG_PAGE = pages | FG_BOTH;
        if (++crane_x > 240)
            crane_x = -160;

        spr_begin(0, DYNAMIC_SPRITES - 1);
        if (p1 & IN_JUMP)
            cycle = 0;
        spr_frame(anim_frame(ANIM_FELIX_IDLE, 0), felix_x, felix_y, mirrored, cycle & 15, cycle >> 4);
        spr_frame(anim_frame(ANIM_RALPH_STAND, 0), 120, 200, 0, 0, 0);
        spr_frame(anim_frame(ANIM_NICELANDER_BOB, frame_count), 30, 230, 0, 0, 0);
        spr_frame(anim_frame(ANIM_BULLDOZER, frame_count), 100, 318, 0, 0, 0);
        spr_rect(RECT_CRANE, crane_x, 110, 0, 0);
        draw_window_heads();
        used = spr_end();
        behind_sprite(felix_x - 40, felix_y - 60);
        SPR_COMMIT = 0;

        show_inputs(used);
        handle_buttons(p1, p1_old, sys, sys_old);
        if ((p1 & IN_LEFT) && felix_x > 12) {
            felix_x--;
            mirrored = 1;
        }
        if ((p1 & IN_RIGHT) && felix_x < 228) {
            felix_x++;
            mirrored = 0;
        }
        if ((p1 & IN_UP) && felix_y > 72)
            felix_y--;
        if ((p1 & IN_DOWN) && felix_y < SCREEN_H - 1)
            felix_y++;
        if ((frame_count & 3) == 0 && ++step == SUPER_CYCLE_LEN)
            step = 0;
        p1_old = p1;
        sys_old = sys;
    }
}
