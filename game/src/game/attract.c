/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "states.h"
#include "bg.h"
#include "bldg.h"
#include "player.h"
#include "hiscore.h"
#include "hud.h"
#include "input.h"
#include "sound.h"
#include "ralph.h"
#include "exe.h"
#include "page.h"
#include "service.h"

#define TITLE_IDLE X16(title_idle)
#define HISCORE_FRAMES X16(attract_hiscore_frames)
#define HISCORE_AFTER_GAME X16(attract_hiscore_after_game)
#define GAME_OVER_FRAMES X16(attract_game_over_frames)
#define ENTRY_FRAMES X16(attract_entry_frames)
#define ENTRY_REPEAT X8(attract_entry_repeat)
#define HOWTO_STEP X8(attract_howto_step)
#define HOWTO_HOLD X16(attract_howto_hold)
#define HOWTO_OBJECTS 9
#define GARBAGE_WINDOW 2047
#define ROM_SLICES 32
#define ALPHABET_LEN 38
#define NONE 0xFF
#define EGG_LEN 6
#define EGG_SHOWS 1
#define EGG_HOLD 10

static const uint16_t howto_reveal[8] = { 0x01, 0, 0x1C, 0x20, 0x40, 0x180, 0, 0 };

static uint8_t blink;
static uint8_t shown_text;
static uint8_t shown_colour;
static uint8_t letter;
static uint8_t letter_idx;
static uint8_t repeat;
static char initials[4];
static uint16_t garbage_seed;
static uint8_t garbage_on;
static uint8_t egg_n, egg_row, egg_col;
static uint8_t rom_sum;
static uint16_t howto_visible;
static actor_t howto_act[HOWTO_OBJECTS];
static uint8_t howto_f, howto_h;
static uint8_t howto_cf, howto_ch;

static void screen_reset(void)
{
    scroll_x = scroll_y = 0;
    bg_reset();
    fg_clear();
    shown_text = NONE;
}

static void print_entry(uint8_t i, uint8_t colour)
{
    char line[4];
    memcpy(line, hiscores[i].name, 4);
    line[3] = 0;
    text_print(X8(attract_table_name_col), X8(attract_table_first_row) + X8(attract_table_row_step) * i, colour, line);
    print_num(X8(attract_table_score_col), X8(attract_table_first_row) + X8(attract_table_row_step) * i, colour, hiscores[i].score, 6);
}

static void title_enter(void)
{
    music_stop();
    shards_clear();
    player_free();
    bricks_clear();
    idle = 0;
    nv_reload();
    screen_reset();
    bg_pic_set(0, TM_LOGO, X8(attract_logo_col), X8(attract_logo_row));
    bg_pic_set(1, TM_COPYRIGHT, X8(attract_copyright_col), X8(attract_copyright_row));
}

static void hiscores_enter(void)
{
    uint8_t i;
    timer = after_game ? HISCORE_AFTER_GAME : HISCORE_FRAMES;
    screen_reset();
    text_print(X8(attract_high_scores_col), X8(attract_high_scores_row), X8(attract_table_red), exe_attract_high_scores);
    text_print(X8(attract_name_score_col), X8(attract_name_score_row), X8(attract_table_red), exe_attract_name_score);
    for (i = 0; i < HISCORES; i++)
        print_entry(i, exe_attract_table_colours[i]);
}

static void game_over_enter(void)
{
    player_free();
    bricks_clear();
    pl.level = 0;
    timer = GAME_OVER_FRAMES;
    if (pl.score > hiscore)
        hiscore = pl.score;
    music_stop();
    screen_reset();
    text_print(X8(attract_game_over_col), X8(attract_game_over_row), X8(attract_game_over_colour), exe_attract_game_over);
}

static void entry_enter(void)
{
    sfx_play(SND_HISCORE_ENTRY);
    player_free();
    bricks_clear();
    timer = ENTRY_FRAMES;
    initials[0] = initials[1] = initials[2] = X8(attract_initial_letter);
    initials[3] = 0;
    letter = 0;
    letter_idx = X8(attract_entry_letter_idx);
    repeat = 0;
    screen_reset();
    text_print(X8(attract_entry_red), X8(attract_enter_initials_row), X8(attract_entry_red), exe_attract_enter_initials);
    text_print(X8(attract_entry_name_score_col), X8(attract_entry_name_score_row), X8(attract_entry_red), exe_attract_name_score);
    text_print(X8(attract_choose_col), X8(attract_choose_row), X8(attract_entry_time_red), exe_attract_choose);
    text_print(X8(attract_joystick_col), X8(attract_joystick_row), X8(attract_entry_time_red), exe_attract_joystick);
    text_print(X8(attract_entry_time_red), X8(attract_press_jump_row), X8(attract_entry_time_red), exe_attract_press_jump);
}

static void boot_enter(void)
{
    step = 0;
    timer = 0;
    idle = 0;
    scroll_x = scroll_y = 0;
    bg_reset();
    fg_clear_rows(0, TEXT_LAST_ROW);
    hud_reset();
    garbage_on = 0;
    egg_n = 0;
    rom_sum = 0;
}

static void howto_enter(void)
{
    uint8_t i;
    music_stop();
    timer = 0;
    step = 0;
    howto_visible = 0;
    screen_reset();
    for (i = 0; i < HOWTO_OBJECTS; i++) {
        howto_act[i].flags = AF_VISIBLE;
        actor_set(&howto_act[i], exe_attract_howto_objs[i].anim);
    }
}

void attract_enter(uint8_t s) BANKED
{
    switch (s) {
    case ST_TITLE: title_enter(); break;
    case ST_HISCORES: hiscores_enter(); break;
    case ST_GAME_OVER: game_over_enter(); break;
    case ST_ENTRY: entry_enter(); break;
    case ST_BOOT: boot_enter(); break;
    case ST_HOWTO: howto_enter(); break;
    }
}

static uint8_t start_game(void)
{
    if ((sys_new & S_START1) && credits) {
        game_start(1);
        return 1;
    }
    if ((sys_new & S_START2) && credits > 1) {
        game_start(2);
        return 1;
    }
    return 0;
}

static void title_tick(void)
{
    if (start_game())
        return;
    if (++idle >= TITLE_IDLE)
        demo_setup();
}

static void hiscores_tick(void)
{
    if (sys_new & S_START1) {
        if (credits)
            game_start(1);
        else
            state_request(ST_TITLE);
    } else if (sys_new & S_START2) {
        if (credits > 1)
            game_start(2);
        else
            state_request(ST_TITLE);
    } else if (--timer < 0) {
        state_request(after_game ? ST_GAME_OVER : ST_TITLE);
        after_game = 0;
    }
    if (sys_new & S_COIN)
        state_request(ST_TITLE);
}

static void game_over_tick(void)
{
    if ((sys_new & S_START1) && credits) {
        hiscore_insert();
        game_start(1);
    } else if ((sys_new & S_START2) && credits > 1) {
        hiscore_insert();
        game_start(2);
    } else if (--timer < 1) {
        state_request(ST_TITLE);
    }
}

static void entry_commit(void)
{
    memcpy(hiscores[entry_pos].name, initials, 3);
    nv_hiscore_save();
    if (players > 1) {
        if (cur == 0) {
            player_select(1);
            if (hiscore_insert()) {
                state_request(ST_ENTRY);
                return;
            }
        }
        after_game = 1;
    }
    state_request(ST_HISCORES);
}

static void entry_tick(void)
{
    if (--timer < 1) {
        entry_commit();
        return;
    }
    if (key_new & (K_JUMP | K_FIX)) {
        if (letter != 2) {
            letter++;
            letter_idx = X8(attract_entry_letter_idx);
            return;
        }
        entry_commit();
        return;
    }
    if (!(key_held & K_DIRS)) {
        repeat = 0;
    } else if (repeat) {
        repeat--;
        return;
    }
    if (key_held & (K_DOWN | K_RIGHT)) {
        if (++letter_idx >= ALPHABET_LEN)
            letter_idx = 0;
        initials[letter] = exe_attract_alphabet[letter_idx];
        repeat = ENTRY_REPEAT;
    }
    if (key_held & (K_UP | K_LEFT)) {
        letter_idx = letter_idx ? letter_idx - 1 : ALPHABET_LEN - 1;
        initials[letter] = exe_attract_alphabet[letter_idx];
        repeat = ENTRY_REPEAT;
    }
}

static uint16_t garbage_rand(void)
{
    garbage_seed = garbage_seed * 25173 + 13849;
    return garbage_seed >> 4;
}

static const char egg_text[EGG_LEN] = { 'N', 'I', 'K', 'N', 'A', 'K' };
static const uint8_t egg_colours[4] = { C_WHITE, C_YELLOW, C_GREEN, C_RED };
static const uint8_t egg_start[EGG_SHOWS] = { 20 };
static uint8_t egg_c;

static void egg_draw(void)
{
    uint8_t i, step;
    volatile uint16_t *dst;
    for (i = 0; i < EGG_SHOWS; i++)
        if ((uint8_t)(egg_n - egg_start[i]) < EGG_HOLD)
            break;
    if (i == EGG_SHOWS)
        return;
    step = (uint8_t)(egg_n - egg_start[i]);
    if (!step) {
        egg_row = TEXT_ROWS - 3;
        egg_col = (TEXT_COLS - EGG_LEN) / 2;
        egg_c = garbage_rand() % 4;
    }
    step >>= 1;
    dst = &FG_MAP[MAP_INDEX(egg_col, egg_row)];
    for (i = 0; i < EGG_LEN; i++)
        dst[i] = FONT_ENTRY(egg_text[i], egg_colours[(uint8_t)(egg_c + i + 4 - (step & 3)) % 4]);
}

static uint8_t garbage_frame(void)
{
    garbage_fill(garbage_rand() & GARBAGE_WINDOW);
    fg_touch_rows(0, TEXT_ROWS);
    egg_draw();
    egg_n++;
    return 1;
}

static void boot_tick(void)
{
    switch (step) {
    case 0:
        if (++timer >= X8(attract_boot_wait)) {
            timer = 0;
            step++;
        } else if (timer >= X8(attract_boot_garbage_at) && !garbage_on) {
            garbage_on = garbage_frame();
        }
        break;
    case 1:
        if (timer + 1 < X8(attract_boot_garbage_frames)) {
            timer++;
            garbage_frame();
        } else {
            timer = 0;
            step++;
        }
        break;
    case 2:
        if (timer < ROM_SLICES)
            rom_sum += rom_sum_slice((uint8_t)timer);
        if (++timer > X8(attract_boot_blank_last)) {
            fg_clear_rows(0, TEXT_LAST_ROW);
            step++;
            timer = 0;
            text_print(X8(attract_boot_col), X8(attract_ram_ok_row), X8(attract_boot_colour), exe_attract_ram_ok);
            text_print(X8(attract_boot_col), X8(attract_rom_ok_row), X8(attract_boot_colour), rom_sum ? "ROM NG" : exe_attract_rom_ok);
        }
        break;
    default:
        if (++timer > X8(attract_boot_ok_frames))
            state_request(ST_TITLE);
        break;
    }
}

static void howto_cycle(void)
{
    if (ticks & X8(attract_howto_cycle_mask))
        return;
    howto_ch += X8(attract_howto_cycle_step_h);
    howto_cf += X8(attract_howto_cycle_step_f);
    if (howto_ch % X8(attract_howto_cycle_mod) == 0)
        howto_ch++;
    howto_f = howto_cf % X8(attract_howto_cycle_mod);
    howto_h = howto_ch % X8(attract_howto_cycle_mod);
}

static void howto_text(uint8_t s)
{
    static const uint16_t bonus[4] = { 0, 10000, 20000, 0 };
    uint8_t i = (dsw1 >> 2) & 3;
    uint16_t b = i ? bonus[i] : XU16(attract_bonus0);
    switch (s) {
    case 2:
        text_print(X8(attract_fix_col), X8(attract_fix_window_row), X8(attract_howto_colour), exe_attract_fix_window);
        text_print(X8(attract_fix_col), X8(attract_fix_last_row), X8(attract_howto_colour), exe_attract_fix_last);
        text_print(X8(attract_fix_col), X8(attract_window_500_row), X8(attract_howto_colour), exe_attract_window_500);
        break;
    case 3:
        text_print(X8(attract_avoid_col), X8(attract_avoid_row), X8(attract_howto_colour), exe_attract_avoid);
        break;
    case 4:
        text_print(X8(attract_block_col), X8(attract_block_row), X8(attract_howto_colour), exe_attract_block);
        break;
    case 5:
        text_print(X8(attract_jump_over_col), X8(attract_jump_over_row), X8(attract_howto_colour), exe_attract_jump_over);
        break;
    case 6:
        text_print(X8(attract_collect_col), X8(attract_collect_row), X8(attract_howto_colour), exe_attract_collect);
        text_print(X8(attract_become_col), X8(attract_become_row), X8(attract_howto_colour), exe_attract_become);
        break;
    case 7:
        if (!b)
            break;
        text_print(X8(attract_bonus_life_col), X8(attract_bonus_life_row), X8(attract_howto_colour), exe_attract_bonus_life);
        print_num(b < 10000 ? X8(attract_bonus_num_col) : 10, X8(attract_bonus_num_row), X8(attract_howto_colour), b, b < 10000 ? 4 : 5);
        text_print(b < 10000 ? 16 : 16, X8(attract_bonus_num_row), X8(attract_howto_colour), exe_attract_pts);
        break;
    }
}

static void howto_tick(void)
{
    howto_cycle();
    if (sys_new & S_COIN) {
        state_request(ST_TITLE);
        return;
    }
    if (start_game())
        return;
    if (step < 7) {
        if (timer + 1 < HOWTO_STEP) {
            timer++;
            return;
        }
        howto_visible |= howto_reveal[step];
        step += step == 0 ? X8(attract_howto_step0_inc) : 1;
        timer = 0;
        howto_text(step);
        return;
    }
    if (++timer >= HOWTO_HOLD)
        state_request(ST_HISCORES);
}

void attract_tick(void) BANKED
{
    switch (game_state) {
    case ST_TITLE: title_tick(); break;
    case ST_HISCORES: hiscores_tick(); break;
    case ST_GAME_OVER: game_over_tick(); break;
    case ST_ENTRY: entry_tick(); break;
    case ST_BOOT: boot_tick(); break;
    case ST_HOWTO: howto_tick(); break;
    }
}

void attract_draw(void) BANKED
{
    uint8_t i;
    if (game_state != ST_HOWTO)
        return;
    for (i = 0; i < HOWTO_OBJECTS; i++)
        if (howto_visible & (1 << i))
            actor_draw_super(&howto_act[i], exe_attract_howto_objs[i].x, exe_attract_howto_objs[i].y,
                             i == HOWTO_OBJECTS - 1 ? howto_f : 0,
                             i == HOWTO_OBJECTS - 1 ? howto_h : 0);
}

void attract_anim_tick(void) BANKED
{
    uint8_t i;
    if (game_state != ST_HOWTO)
        return;
    for (i = 0; i < HOWTO_OBJECTS; i++)
        if (howto_visible & (1 << i))
            actor_tick(&howto_act[i]);
}

static void prompt(uint8_t row)
{
    uint8_t kind = credits == 0 ? 0 : credits == 1 ? 1 : 2;
    uint8_t on = (blink & X8(attract_blink_mask)) < X8(attract_blink_on);
    uint8_t colour;
    blink++;
    if (row == X8(attract_title_prompt_row) && kind == 0)
        colour = on ? X8(attract_prompt_on_colour) : X8(attract_coin_off_colour);
    else
        colour = on ? X8(attract_prompt_on_colour) : X8(attract_start_off_colour);
    if (kind == 0 && row != X8(attract_title_prompt_row))
        colour = NONE;
    if (shown_text == kind && shown_colour == colour)
        return;
    print_blank(X8(attract_press_1p2p_col), row, 20);
    shown_text = kind;
    shown_colour = colour;
    if (colour == NONE)
        return;
    if (kind == 0)
        text_print(X8(attract_insert_coin_col), row, colour, exe_attract_insert_coin);
    else if (kind == 1)
        text_print(X8(attract_press_1p_col), row, colour, exe_attract_press_1p);
    else
        text_print(X8(attract_press_1p2p_col), row, colour, exe_attract_press_1p2p);
}

static void entry_render(void)
{
    uint8_t i, row = X8(attract_entry_base) + X8(attract_entry_row_step) * entry_pos;
    char ch[2];
    blink++;
    if (players > 1) {
        if ((blink & X8(attract_entry_player_blink_mask)) >= X8(attract_entry_player_blink_on)) {
            text_print(X8(attract_entry_player_col), X8(attract_entry_player_row), X8(attract_entry_player_colour), exe_attract_player);
            print_num(18, X8(attract_entry_player_row), X8(attract_entry_player_colour), cur + 1, 1);
        } else {
            print_blank(X8(attract_entry_player_col), X8(attract_entry_player_row), 8);
        }
    }
    ch[1] = 0;
    for (i = 0; i < HISCORES; i++)
        if (i != entry_pos)
            print_entry(i, exe_attract_table_colours[i]);
    for (i = 0; i < 3; i++) {
        ch[0] = initials[i];
        text_print(X8(attract_entry_initial_col) + i, row, i == letter && (blink & X8(attract_initial_blink_mask)) >= X8(attract_initial_blink_on) ? X8(attract_initial_white) : X8(attract_initial_red), ch);
    }
    print_num(X8(attract_table_score_col), row, X8(attract_entry_white), hiscores[entry_pos].score, 6);
    text_print(X8(attract_entry_time_col), X8(attract_entry_time_row), X8(attract_entry_time_red), exe_attract_time_left);
    print_num(19, X8(attract_entry_time_row), X8(attract_entry_time_red), (uint16_t)timer / X8(attract_entry_time_div), 2);
}

void attract_render(void) BANKED
{
    switch (game_state) {
    case ST_BOOT:
        return;
    case ST_TITLE:
        prompt(X8(attract_title_prompt_row));
        break;
    case ST_HISCORES:
        prompt(X8(attract_hiscore_prompt_row));
        break;
    case ST_ENTRY:
        entry_render();
        hud_hiscore();
        hud_score(0);
        return;
    }
    hud_credits();
    hud_hiscore();
    hud_score(game_state == ST_GAME_OVER);
}
