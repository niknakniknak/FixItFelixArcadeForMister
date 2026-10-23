/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "states.h"
#include "bldg.h"
#include "bg.h"
#include "felix.h"
#include "ralph.h"
#include "actors.h"
#include "player.h"
#include "hiscore.h"
#include "hud.h"
#include "input.h"
#include "sound.h"
#include "rng.h"
#include "cutscene.h"
#include "page.h"
#include "service.h"
#include "exe.h"

#ifdef __SDCC
#define IRQ_OFF() __asm__("di")
#define IRQ_ON() __asm__("ei")
#else
#define IRQ_OFF()
#define IRQ_ON()
#endif

#define VIEW_NONE 0xFF
#define SWITCH_FRAMES X16(state_switch_frames)
#define MUSIC_LOOP X8(state_music_loop)
#define CRASH_TEXT_LEN 8
#define CRASH_LAST_COL XS8(state_crash_last_col)

uint8_t game_state;
uint8_t next_state;
uint8_t state_pending;
uint16_t ticks;
uint8_t step;
int16_t timer;
uint16_t idle;
uint8_t demo;
uint8_t next_level;
uint8_t after_game;
uint8_t players;
uint8_t cur;
int16_t scroll_y;
int16_t scroll_x;
uint8_t level_end_pending;
uint8_t service_pressed;
uint8_t view_player;
uint8_t felix_f, felix_h;

static volatile uint8_t vbl_count;
static volatile uint8_t pres;
static volatile uint8_t pres_flip;
static volatile uint8_t hold;
static uint8_t reg_scroll_x;
static uint16_t reg_scroll_y;
static uint8_t reg_video;
static uint8_t crashing;
static uint8_t play_frames;
static int8_t crash_col;
static uint8_t crash_row;
static uint8_t crash_idx;

void vblank_handler(void)
{
    if (pres) {
        VIDEO_CTRL = reg_video;
        BG_SCROLL_X = reg_scroll_x;
        BG_SCROLL_Y_LO = (uint8_t)reg_scroll_y;
        BG_SCROLL_Y_HI = (uint8_t)(reg_scroll_y >> 8);
        SPR_COMMIT = 0;
        pres = 0;
        pres_flip = 0;
    }
    if (hold)
        hold--;
    vbl_count++;
}

void state_request(uint8_t s)
{
    next_state = s;
    state_pending = 1;
}

static uint8_t in_attract(void)
{
    if (demo)
        return 1;
    switch (game_state) {
    case ST_TITLE:
    case ST_HOWTO:
    case ST_BOOT:
    case ST_SERVICE:
        return 1;
    case ST_HISCORES:
        return !after_game;
    case ST_CUTSCENE:
        return !pl.needs_intro && !next_level;
    }
    return 0;
}

static void play_enter(void)
{
    key_latch = 0;
    view_player = cur;
    scroll_y = pl.scroll;
    if (demo)
        demo_input_start();
    music_play(SND_MUSIC_GAME, MUSIC_LOOP);
    fg_clear();
}

static void switch_enter(void)
{
    music_stop();
    key_latch = 0;
    timer = 0;
    scroll_y = 0;
    player_select(cur ^ 1);
    view_player = VIEW_NONE;
    fg_clear();
    hud_switch_text(player_out_other());
}

static void scroll_enter(void)
{
    step = 0;
    ralph_set_state(RALPH_CLIMB);
}

static void levelend_enter(void)
{
    felix_f = felix_h = 0;
    sfx_stop(SND_SUPER);
    lvlend_init();
}

static void cutscene_enter(void)
{
    fg_clear();
    cut_setup();
    if (next_level)
        obstacles_create();
}

static void state_enter(uint8_t s)
{
    game_state = s;
    statics_dirty = 1;
    scroll_x = 0;
    switch (s) {
    case ST_CUTSCENE: cutscene_enter(); break;
    case ST_PLAY: play_enter(); break;
    case ST_SWITCH: switch_enter(); break;
    case ST_SCROLL: scroll_enter(); break;
    case ST_LEVEL_END: levelend_enter(); break;
    default: attract_enter(s); break;
    }
}

static uint8_t attract_start(void)
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

static void end_demo(void)
{
    demo = 0;
    music_stop();
    demo_input_stop();
    shards_clear();
    player_free();
    bricks_clear();
}

static void play_actors_update(void)
{
    felix_update();
    ralph_update();
    windows_update();
    shards_update();
    birds_update();
}

static void pie_timer_update(void)
{
    if (pl.felix.state == FELIX_DEAD || pl.ralph.state > RALPH_SMASH)
        return;
    if (!pl.pie.timer) {
        pie_place();
    } else if (!pl.pie.active && !--pl.pie.timer && pl.broken < X8(state_pie_min_broken)) {
        pl.pie.timer = X16(state_pie_retry);
    }
}

static void play_demo_exit(void)
{
    if (sys_new & S_COIN) {
        end_demo();
        state_request(ST_TITLE);
    } else if ((sys_new & S_START1) && credits) {
        end_demo();
        game_start(1);
    } else if ((sys_new & S_START2) && credits > 1) {
        end_demo();
        game_start(2);
    } else if (demo_input_over() && pl.ralph.state != RALPH_CLIMB) {
        end_demo();
        state_request(ST_HISCORES);
    }
}

static void play_tick(void)
{
    play_actors_update();
    obstacles_update();
    pie_update();
    chunks_update();
    if (level_end_pending && !pl.popup_on[(pl.last_fix_row - pl.win_row) * SLOTS + pl.last_fix_slot] &&
        --timer < 1) {
        level_end_pending = 0;
        state_request(ST_LEVEL_END);
    }
    pie_timer_update();
    if (demo)
        play_demo_exit();
}

static void scroll_tick(void)
{
    play_actors_update();
    chunks_update();
    if (step == 0) {
        if (pl.ralph.state == RALPH_CLIMB && !pl.ralph.climbing) {
            if (demo) {
                end_demo();
                step++;
                state_request(ST_HOWTO);
                return;
            }
            sfx_play(SND_RALPH_CLIMB);
            step++;
        }
    } else if (step == 1) {
        pl.scroll += X8(state_scroll_speed);
        if (pl.scroll >= pl.scroll_target) {
            pl.scroll = pl.scroll_target;
            felix_place_after_scroll();
            step++;
        }
        scroll_y = pl.scroll;
    } else if (step == 2 && pl.felix.scroll_jump && pl.felix.state == FELIX_IDLE) {
        pl.floor_done = 0;
        chunks_clear();
        ralph_reset();
        felix_reset_flags();
        pl.win_row = pl.floor_row;
        state_request(ST_PLAY);
    }
}

static void switch_tick(void)
{
    if (++timer > SWITCH_FRAMES) {
        scroll_y = pl.scroll;
        view_player = cur;
        state_request(pl.needs_intro ? ST_CUTSCENE : ST_PLAY);
    }
}

static void cutscene_finish_game(void)
{
    cut_free();
    ralph_reset();
    felix_start_walk_in();
    birds_create();
    if (pl.needs_intro)
        obstacles_create();
    pl.needs_intro = 0;
    next_level = 0;
    state_request(ST_PLAY);
}

static void cutscene_tick(void)
{
    if (pl.needs_intro || next_level) {
        cut_tick();
        shards_update();
        if (cut_done())
            cutscene_finish_game();
        return;
    }
    if (sys_new & (S_START1 | S_START2 | S_COIN)) {
        cut_free();
        if (!attract_start())
            state_request(ST_TITLE);
        return;
    }
    cut_tick();
    shards_update();
    if (cut_done()) {
        cut_free();
        felix_start_walk_in();
        state_request(ST_PLAY);
    }
}

static void levelend_tick(void)
{
    lvlend_tick();
    if (lvlend_done()) {
        lvlend_free();
        level_advance();
    }
}

static void state_tick(void)
{
    switch (game_state) {
    case ST_CUTSCENE: cutscene_tick(); break;
    case ST_PLAY: play_tick(); break;
    case ST_SWITCH: switch_tick(); break;
    case ST_SCROLL: scroll_tick(); break;
    case ST_LEVEL_END: levelend_tick(); break;
    default: attract_tick(); break;
    }
}

static void anim_tick_all(void)
{
    actor_tick(&pl.felix.act);
    actor_tick(&pl.ralph.act);
    bricks_tick();
    birds_tick();
    pie_tick();
    cut_anim_tick();
    lvlend_anim_tick();
    attract_anim_tick();
}

static void draw_gameplay(void)
{
    popups_draw();
    felix_draw();
    ralph_draw();
    bricks_draw();
    shards_draw();
    birds_draw();
    chunks_draw();
    pie_draw();
}

static void hud_gameplay(void)
{
    if (demo) {
        hud_credits();
    } else {
        hud_score(1);
        hud_lives();
    }
    hud_hiscore();
}

static uint8_t statics_shown(void)
{
    switch (game_state) {
    case ST_PLAY:
    case ST_SCROLL:
    case ST_LEVEL_END:
    case ST_CUTSCENE:
        return building_on;
    }
    return 0;
}

static void present_wait(void)
{
    while (pres || (IN2 & SYS_COMMIT_PENDING))
        ;
}

static void render_sprites(void)
{
    present_wait();
    if (state_pending)
        return;
    statics_update(statics_shown());
    sprites_begin();
    switch (game_state) {
    case ST_PLAY:
    case ST_SCROLL:
        draw_gameplay();
        break;
    case ST_LEVEL_END:
        lvlend_draw();
        popups_draw();
        ralph_draw();
        bricks_draw();
        shards_draw();
        birds_draw();
        chunks_draw();
        pie_draw();
        felix_draw();
        break;
    case ST_CUTSCENE:
        cut_draw();
        if (pl.needs_intro || next_level)
            draw_gameplay();
        else
            shards_draw();
        break;
    default:
        attract_draw();
        break;
    }
    sprites_finish();
}

static void render(void)
{
    bg_dy = 0;
    switch (game_state) {
    case ST_PLAY:
    case ST_SCROLL:
        hud_gameplay();
        break;
    case ST_LEVEL_END:
        lvlend_render();
        hud_score(1);
        hud_hiscore();
        hud_lives();
        break;
    case ST_CUTSCENE:
        cut_render();
        if (pl.needs_intro || next_level) {
            hud_score(1);
            hud_lives();
        } else {
            hud_credits();
        }
        hud_hiscore();
        break;
    case ST_SWITCH:
        hud_score(1);
        hud_lives();
        hud_hiscore();
        break;
    default:
        attract_render();
        break;
    }
    doors_refresh();
    pg_catch_up(4);
    bg_sync();
    render_sprites();
}

void game_reset(void)
{
    crashing = 0;
    crash_pending = 0;
    service_pressed = 0;
    cut_free();
    lvlend_free();
    shards_clear();
    player_select(1);
    player_free();
    player_new();
    pl.out = 1;
    player_select(0);
    player_free();
    player_new();
    pl.out = 1;
    hiscore_defaults();
    demo = 0;
    next_level = 0;
    after_game = 0;
    level_end_pending = 0;
    state_request(ST_BOOT);
}

static void crash_tick(void)
{
    uint8_t n;
    for (n = 0; n < X8(state_crash_per_frame); n++) {
        if (crash_col >= 0) {
            volatile uint16_t *dst = &FG_MAP[MAP_INDEX(crash_col, crash_row)];
            fg_touch(crash_row);
            *dst = FONT_OPAQUE_ENTRY(exe_state_crash_text[crash_idx], rng() % X8(state_crash_colours));
        }
        if (++crash_idx == CRASH_TEXT_LEN)
            crash_idx = 0;
        if (++crash_row > X8(state_crash_rows)) {
            crash_row = 0;
            if (--crash_col < CRASH_LAST_COL) {
                game_reset();
                return;
            }
        }
    }
}

static void crash_start(void)
{
    crash_pending = 0;
    crashing = 1;
    crash_col = X8(state_crash_start_col);
    crash_row = 0;
    crash_idx = 0;
    sound_stop_all();
}

static void present(void)
{
    uint8_t flip = (dsw1 & 0x80) && cur && players > 1 && !in_attract();
    uint8_t show, page;
    reg_video = VIDEO_BG | VIDEO_FG | VIDEO_SPRITES | VIDEO_COMMIT | (flip ? VIDEO_FLIP : 0);
    reg_scroll_x = (uint8_t)(-scroll_x);
    reg_scroll_y = (uint16_t)(-scroll_y + bg_dy) & 0x1FF;
    bg_shown(reg_scroll_x, reg_scroll_y);
    flip = pg_flips();
    if (!flip) {
        pres = 1;
        return;
    }
    show = pg_show ^ flip;
    page = show | pg_cpu_bits(show);
    for (;;) {
        IRQ_OFF();
        if (!(IN2 & SYS_VBLANK))
            break;
        IRQ_ON();
        while (IN2 & SYS_VBLANK)
            ;
    }
    FG_PAGE = page;
    pres_flip = 1;
    pres = 1;
    if (IN2 & SYS_VBLANK)
        hold = 2;
    IRQ_ON();
    pg_show = show;
}

uint8_t present_busy(void)
{
    return pres_flip | hold;
}

void game_frame(void)
{
    pg_hidden();
    nv_tick();
    if (game_state == ST_SERVICE) {
        svc_frame();
        if (!state_pending)
            present();
        return;
    }
    if (pause_update()) {
        present();
        return;
    }
    input_read(game_state != ST_BOOT && !crashing);
    if (crashing) {
        present_wait();
        crash_tick();
        present();
        return;
    }
    if (sys_new & S_COIN)
        idle = 0;
    if (sys_new & S_SERVICE) {
        if (in_attract()) {
            svc_enter();
            present();
            return;
        }
        service_pressed = 1;
    }
    fg_pics_sync();
    if (state_pending) {
        state_pending = 0;
        state_enter(next_state);
    }
    state_tick();
    if (!in_attract() && ++play_frames >= 60) {
        play_frames = 0;
        nv_count(BK_SECONDS);
    }
    sound_tick();
    anim_tick_all();
    ticks++;
    sound_mute((dsw1 & 0x40) && in_attract());
    if (crash_pending)
        crash_start();
    if (state_pending)
        return;
    render();
    present();
}

static void hardware_init(void)
{
    uint8_t i;
    INT_CTRL = 0;
    VIDEO_CTRL = 0;
    pg_init();
    map_fill(BG_MAP, 0);
    map_fill(FG_MAP, 0);
    pg_hidden();
    for (i = 0; i < SPRITE_COUNT; i++)
        SPR_SIZE[i] = 0;
    sprites_begin();
    sprites_finish();
    reg_video = VIDEO_BG | VIDEO_FG | VIDEO_SPRITES | VIDEO_COMMIT;
    VIDEO_CTRL = reg_video;
    SPR_COMMIT = 0;
}

void game_init(void)
{
    hardware_init();
    panes_init();
    sound_init();
    input_init();
    bg_reset();
    svc_boot();
}

uint8_t vblank_count(void)
{
    return vbl_count;
}
