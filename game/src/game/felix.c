/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "felix.h"
#include "bldg.h"
#include "input.h"
#include "sound.h"
#include "ralph.h"
#include "actors.h"
#include "hiscore.h"
#include "player.h"
#include "hud.h"
#include "service.h"

static void floor_cleared(void);
static void continue_tick(void);

#define ANIM_STAND X8(felix_anim_stand)
#define ANIM_WALK X8(felix_anim_walk)
#define ANIM_HAMMER X8(felix_anim_hammer)
#define ANIM_AIR X8(felix_anim_air)
#define ANIM_LAND X8(felix_anim_land)
#define ANIM_HIT X8(felix_anim_hit)
#define ANIM_DEATH X8(felix_anim_death)

#define START_X_FIX XL(felix_start_x_fix)
#define WALK_IN_X_FIX XL(felix_walk_in_x_fix)
#define MIN_X_FIX XL(felix_min_x_fix)
#define MAX_X_FIX XL(felix_max_x_fix)
#define HEAD_ROOM_FIX XL(felix_head_room_fix)
#define HIT_HEIGHT_FIX XL(felix_hit_height_fix)
#define LEFT_LIMIT_FIX XL(felix_left_limit_fix)
#define RIGHT_LIMIT_FIX XL(felix_right_limit_fix)
#define WALK_IN_SPEED XL(felix_walk_in_speed)
#define WALK_IN_VY XL(felix_walk_in_vy)
#define WALK_MAX XL(felix_walk_max)
#define WALK_MIN XL(felix_walk_min)
#define UP_STEP XL(felix_up_step)
#define UP_CLAMP XL(felix_up_clamp)
#define FALL_MAX XL(felix_fall_max)
#define JUMP_VY XL(felix_jump_vy)
#define DEATH_FREEZE_FRAME X8(felix_death_freeze_frame)
#define CONTINUE_FRAMES 600
#define CONTINUE_ROW 18
#define CONTINUE_COL 9
#define JUMP_T128 X16(felix_jump_t128)

#define F (pl.felix)

static const uint16_t super_frames[4] = { 0, 300, 720, 0 };
static uint16_t cont_timer;

int16_t felix_x(void)
{
    return fix_round(F.x);
}

int16_t felix_y(void)
{
    return fix_round(F.y);
}

fix_t box_x, box_y;
uint8_t box_w, box_h, box_felix_w;

uint8_t felix_hit(void)
{
    fix_t hw = FIX(box_w), hh = FIX(box_h), fw = FIX(box_felix_w);
    if (invulnerable())
        return 0;
    return box_x - hw < F.x + fw && F.x - fw < box_x + hw && F.y - HIT_HEIGHT_FIX < box_y + hh &&
           box_y - hh < F.y;
}

static void move(void)
{
    F.x += F.vx;
    F.y += F.vy;
}

static void hop_move(void)
{
    F.vy += FIX_0_5;
    move();
}

static void sink(void)
{
    F.vy += FIX_0_33;
    F.y += F.vy;
}

static void pie_check(uint8_t loud)
{
    if (!pl.pie.ready || F.row != pl.pie.row || F.slot != pl.pie.slot)
        return;
    pie_hide();
    if (loud) {
        sfx_play(SND_PIE_EAT);
        sfx_play(SND_SUPER);
    }
    pl.rage_request = 1;
    F.super_timer = super_frames[play_opt[OPT_SUPER]];
    if (!F.super_timer)
        F.super_timer = X16(felix_super_frames);
    F.super_on = 1;
}

static uint8_t window_has_obstacle(uint8_t row, uint8_t slot)
{
    return pl.win[row][slot].st & WIN_OBSTACLE;
}

static void walk_start(uint8_t key, int16_t limit, fix_t v)
{
    key_latch &= (uint8_t)~key;
    if (F.x == FIX(limit))
        return;
    if (!F.vx)
        F.vx = v;
    felix_set_state(FELIX_WALK);
}

static void on_idle(void)
{
    if (F.stopped)
        return;
    pie_check(1);
    if (key_new & K_JUMP) {
        felix_set_state(FELIX_JUMP);
        return;
    }
    if ((key_held & K_FIX) && F.can_fix) {
        if (win_state(F.row, F.slot)) {
            F.fix_row = F.row;
            F.fix_slot = F.slot;
            actor_mirror(&F.act, F.x > FIX(win_center(F.row, F.slot)));
            sfx_play(SND_HAMMER);
        }
        felix_set_state(FELIX_HAMMER);
        return;
    }
    if (key_latch & K_LEFT) {
        walk_start(K_LEFT, LEFT_LIMIT, -FIX_0_2);
        return;
    }
    if (key_latch & K_RIGHT) {
        walk_start(K_RIGHT, RIGHT_LIMIT, FIX_0_2);
        return;
    }
    if ((key_held & K_DOWN) && F.can_updown && F.row != pl.floor_row &&
        !window_has_obstacle(F.row, F.slot)) {
        felix_set_state(FELIX_DROP);
        return;
    }
    if ((key_held & K_UP) && F.can_updown)
        felix_set_state(FELIX_JUMP_UP);
}

static void on_walk_in(void)
{
    if (F.act.anim == ANIM_AIR) {
        F.vy += FIX_0_5;
        F.y += F.vy;
        if (F.vy > 0) {
            fix_t sill = FIX(win_sill(pl.floor_row, 4));
            if (F.y >= sill) {
                F.y = sill;
                F.row = pl.floor_row;
                F.slot = 4;
                felix_set_state(FELIX_IDLE);
            }
        }
        return;
    }
    F.x -= WALK_IN_SPEED;
    if (F.x <= RIGHT_LIMIT_FIX) {
        F.x = RIGHT_LIMIT_FIX;
        F.vy = WALK_IN_VY;
        actor_set(&F.act, ANIM_AIR);
    }
}

static uint8_t outside_window(void)
{
    return F.row != NO_ROW &&
           (F.x < FIX(win_left(F.row, F.slot)) || F.x > FIX(win_right(F.row, F.slot)));
}

static void on_walk(void)
{
    pie_check(0);
    if (key_new & K_JUMP) {
        felix_set_state(FELIX_JUMP);
        return;
    }
    if (((key_held & K_RIGHT) && F.vx < 0) || (!(key_held & K_RIGHT) && (key_held & K_LEFT) && F.vx > 0))
        F.vx = -F.vx;
    if (F.vx <= 0) {
        F.vx -= FIX_0_2;
        if (F.vx < WALK_MIN)
            F.vx = WALK_MIN;
    } else {
        F.vx += FIX_0_2;
        if (F.vx > WALK_MAX)
            F.vx = WALK_MAX;
    }
    F.x += F.vx;
    if (F.x > LEFT_LIMIT_FIX && F.x <= RIGHT_LIMIT_FIX) {
        if (outside_window()) {
            F.vx = F.vx < 0 ? WALK_MIN : WALK_MAX;
            actor_mirror(&F.act, F.vx < 0);
            felix_set_state(FELIX_STEP);
        }
        return;
    }
    F.x = F.x <= LEFT_LIMIT_FIX ? LEFT_LIMIT_FIX : RIGHT_LIMIT_FIX;
    felix_set_state(FELIX_IDLE);
}

#define HAMMER_BY_DAMAGE 3

static const uint8_t pane_level[6] = { 0, 2, 1, 1, 2, 3 };
static const uint8_t pane_lighter[6] = { 0, 3, 0, 0, 3, 1 };

static uint8_t window_step(uint8_t row, uint8_t slot)
{
    window_t *w = &pl.win[row][slot];
    uint8_t k = slot_kind(row, slot), upper = w->dmg & 7, lower = w->dmg >> 3;
    if (k == SLOT_DOOR || k == SLOT_ARCH || (pane_level[upper] < 2 && pane_level[lower] < 2))
        return 0;
    upper = pane_lighter[upper];
    lower = pane_lighter[lower];
    w->dmg = upper | (lower << 3);
    w->st = (w->st & WIN_OBSTACLE) | (upper ? (lower ? 2 : 1) : (lower ? 1 : 0));
    statics_row(row);
    return 1;
}

static void on_hammer(void)
{
    pie_check(0);
    if (!(F.act.flags & AF_DONE))
        return;
    if (F.fix_row != NO_ROW && play_opt[OPT_HAMMER] == HAMMER_BY_DAMAGE) {
        if (window_step(F.fix_row, F.fix_slot))
            F.fix_row = NO_ROW;
    } else if (F.fix_row != NO_ROW && play_opt[OPT_HAMMER]) {
        if (F.fix_row != F.hit_row || F.fix_slot != F.hit_slot) {
            F.hit_row = F.fix_row;
            F.hit_slot = F.fix_slot;
            F.hits = 0;
        }
        if (++F.hits <= play_opt[OPT_HAMMER])
            F.fix_row = NO_ROW;
        else
            F.hit_row = NO_ROW;
    }
    if (F.fix_row != NO_ROW) {
        pl.last_fix_row = F.fix_row;
        pl.last_fix_slot = F.fix_slot;
        window_fix(F.fix_row, F.fix_slot);
        F.fix_row = NO_ROW;
        if (!pl.floor_done && !pl.broken) {
            ralph_floor_done();
            birds_free_current();
            pl.floor_done = 1;
        }
    }
    F.can_fix = 0;
    if (F.airborne) {
        felix_set_state(FELIX_LAND);
        return;
    }
    if (pl.floor_done) {
        F.stopped = 1;
        floor_cleared();
    }
    felix_set_state(FELIX_IDLE);
}

static void land_on(uint8_t row, uint8_t slot)
{
    F.row = row;
    F.slot = slot;
    F.airborne = 0;
    F.y = FIX(win_sill(row, slot));
    F.vy = 0;
}

static uint8_t find_window(int16_t x, int16_t y, int16_t prev_y, uint8_t *slot)
{
    uint8_t r, s, k;
    int16_t base, sill;
    for (r = pl.floor_row; r < pl.floor_row + FLOOR_ROWS; r++) {
        if (!win_active(r))
            continue;
        base = win_sill(r, 0);
        for (s = 0; s < SLOTS; s++) {
            k = slot_kind(r, s);
            sill = k == SLOT_DOOR || k == SLOT_ARCH ? win_sill(r, s) : base;
            if (y >= sill && prev_y < sill && x >= win_left(r, s) && x <= win_right(r, s)) {
                *slot = s;
                return r;
            }
        }
    }
    return NO_ROW;
}

static void floor_cleared(void)
{
    uint8_t r;
    sfx_stop(SND_EXTRA_LIFE);
    music_play(SND_MUSIC_FLOOR_CLEAR, 0);
    pl.pie.active = 0;
    pl.pie.ready = 0;
    actor_show(&pl.pie.pie, 0);
    actor_show(&pl.pie.nicelander, 0);
    pl.floors_left--;
    pl.felix.cycle_f = pl.felix.cycle_h = 0;
    pl.pie_flag = 0;
    if (!pl.floors_left) {
        timer = X8(felix_level_end_timer);
        level_end_pending = 1;
        return;
    }
    for (r = pl.floor_row; r < pl.floor_row + FLOOR_ROWS; r++)
        pl.scroll_target += row_height(r) * 8;
    pl.floor_row += FLOOR_ROWS;
    floor_damage_top();
    state_request(ST_SCROLL);
}

static uint8_t probe(uint8_t *slot)
{
    return find_window(fix_round(F.x), fix_round(F.y), fix_round(F.prev_y), slot);
}

static void on_jump_side(void)
{
    uint8_t row, slot;
    hop_move();
    if (F.x < LEFT_LIMIT_FIX)
        F.x = LEFT_LIMIT_FIX;
    if (F.x > RIGHT_LIMIT_FIX)
        F.x = RIGHT_LIMIT_FIX;
    if (F.vy <= 0)
        return;
    row = probe(&slot);
    if (row == NO_ROW)
        return;
    land_on(row, slot);
    F.vx = F.vx < 0 ? WALK_MIN : WALK_MAX;
    felix_set_state(FELIX_IDLE);
}

static uint8_t obstacle_blocks(uint8_t row)
{
    if (row >= pl.rows || !window_has_obstacle(row, F.slot))
        return 0;
    if (FIX(win_sill(row, F.slot) + X8(felix_obstacle_sill_dy)) >= F.y - HEAD_ROOM_FIX) {
        obstacle_bonk(row, F.slot);
        F.vy = 0;
        felix_set_state(FELIX_LAND);
        return 1;
    }
    return 0;
}

static void on_jump_up(void)
{
    if (!F.up_clamped) {
        F.vy -= UP_STEP;
        if (F.vy <= UP_CLAMP) {
            F.vy = UP_CLAMP;
            F.up_clamped = 1;
        }
    }
    hop_move();
    if (F.vy < 0 && !F.scroll_jump && F.row != pl.floor_row + FLOOR_ROWS - 1) {
        if (obstacle_blocks(F.row + 1))
            return;
        if (row_type(F.row) != 4 && obstacle_blocks(F.row + 2))
            return;
    }
    if (F.vy >= 0)
        felix_set_state(FELIX_LAND);
}

static void on_step_hop(void)
{
    uint8_t row, slot;
    hop_move();
    row = probe(&slot);
    if (row == NO_ROW)
        return;
    land_on(row, slot);
    F.vx = 0;
    felix_set_state(FELIX_IDLE);
}

static void on_fall(void)
{
    uint8_t row, slot;
    move();
    row = probe(&slot);
    if (row == NO_ROW) {
        F.vy += FIX_0_33;
        if (F.vy > FALL_MAX)
            F.vy = FALL_MAX;
        return;
    }
    land_on(row, slot);
    if (pl.floor_done && !F.scroll_jump) {
        floor_cleared();
        return;
    }
    felix_set_state(FELIX_IDLE);
}

static void respawn(void)
{
    F.row = pl.floor_row;
    F.x = FIX(win_center(F.row, F.slot));
    F.y = FIX(win_sill(F.row, F.slot));
    pl.lives--;
    ralph_reset();
    felix_reset_flags();
    felix_set_state(FELIX_IDLE);
    if (players == 2 && !player_out_other()) {
        sfx_play(SND_PLAYER_CHANGE);
        state_request(ST_SWITCH);
    }
}

static void die_with_lives(void)
{
    if (F.die_phase == 0) {
        if (!(F.act.flags & AF_DONE))
            return;
        sink();
        if (F.y > FIX(row_bottom_y(pl.floor_row) + X8(felix_die_fall_dy))) {
            F.die_timer = X8(felix_die_wait);
            F.die_phase = 1;
        }
        return;
    }
    if (F.die_phase == 1 && !--F.die_timer) {
        birds_reset();
        respawn();
    }
}

static void die_last(void)
{
    fix_t sill;
    switch (F.die_phase) {
    case 0:
        if (F.act.frame == DEATH_FREEZE_FRAME) {
            F.act.flags |= AF_FROZEN;
            F.die_phase = 1;
        }
        break;
    case 1:
        sink();
        sill = FIX(win_sill(F.row, F.slot));
        if (F.y > sill) {
            F.y = sill;
            F.act.flags &= (uint8_t)~AF_FROZEN;
            F.die_phase = 2;
            F.die_timer = X8(felix_die_last_wait);
        }
        break;
    case 2:
        if (--F.die_timer)
            break;
        if (!play_opt[OPT_CONTINUE] || demo) {
            state_request(game_over_route());
            break;
        }
        F.die_phase = 3;
        cont_timer = CONTINUE_FRAMES;
        opaque_print(CONTINUE_COL, CONTINUE_ROW, C_WHITE, " CONTINUE? ");
        opaque_print(CONTINUE_COL, CONTINUE_ROW + 1, C_WHITE, "           ");
        break;
    default:
        continue_tick();
        break;
    }
}

static void continue_clear(void)
{
    uint8_t r;
    for (r = 0; r < 3; r++)
        print_blank(CONTINUE_COL, CONTINUE_ROW + r, 11);
}

static void continue_tick(void)
{
    if ((sys_new & (S_START1 | S_START2)) && (credits || free_play())) {
        if (!free_play())
            credits--;
        continue_clear();
        nv_count(BK_CONTINUE);
        sfx_play(SND_START);
        music_play(SND_MUSIC_GAME, 1);
        pl.out = 0;
        pl.lives = exe_spare_lives[dsw1 & 3] + 1;
        birds_reset();
        respawn();
        return;
    }
    if (!(cont_timer % 60)) {
        char line[12] = "           ";
        uint8_t n = cont_timer / 60;
        line[5] = '0' + n / 10;
        line[6] = '0' + n % 10;
        opaque_print(CONTINUE_COL, CONTINUE_ROW + 2, C_YELLOW, line);
    }
    if (!--cont_timer) {
        continue_clear();
        state_request(game_over_route());
    }
}

static void on_dead(void)
{
    if (pl.lives)
        die_with_lives();
    else
        die_last();
}

static uint16_t flight_time(uint8_t to_slot)
{
    int8_t dy = win_sill(F.row, to_slot) - win_sill(F.row, F.slot);
    switch (dy) {
    case 12: return X16(felix_flight_p12);
    case -12: return X16(felix_flight_m12);
    case 9: return X16(felix_flight_p9);
    case -9: return X16(felix_flight_m9);
    }
    return X16(felix_flight_0);
}

static void start_side_jump(void)
{
    fix_t target = F.x;
    uint16_t t128 = JUMP_T128;
    actor_set(&F.act, ANIM_AIR);
    F.vy = JUMP_VY;
    if (F.vx > 0) {
        if (F.slot == 4) {
            target = FIX(win_right(F.row, F.slot) - 1);
        } else {
            t128 = flight_time(F.slot + 1);
            target = FIX(win_center(F.row, F.slot + 1));
        }
    } else if (F.vx < 0) {
        if (F.slot == 0) {
            target = FIX(win_left(F.row, F.slot) + 1);
        } else {
            t128 = flight_time(F.slot - 1);
            target = FIX(win_center(F.row, F.slot - 1));
        }
    }
    F.airborne = 1;
    F.can_jump = 0;
    F.vx = (target - F.x) * 128 / (int32_t)t128;
    sfx_play(SND_JUMP);
}

static void start_step(void)
{
    uint8_t k = slot_kind(F.row, F.slot);
    uint8_t s = F.slot;
    uint8_t right = F.vx >= 0;
    uint8_t edge = (s == 1 && right) || (s == 3 && !right);
    uint8_t vx = 0, vy = 0;
    actor_set(&F.act, ANIM_AIR);
    if (k == SLOT_DOOR) {
        vx = 2;
        vy = 4;
    } else if (k == SLOT_ARCH) {
        vx = 3;
        vy = 2;
    } else if (k == SLOT_UPPER_MID) {
        vy = 3;
    } else if (k == SLOT_UPPER) {
        vy = edge ? 3 : 1;
    } else if (edge) {
        if (row_type(F.row) == 1) {
            vy = 1;
        } else {
            vx = 1;
            vy = 2;
        }
    }
    F.vx = right ? exe_step_vx[vx] : -exe_step_vx[vx];
    F.vy = exe_step_vy[vy];
}

static void start_hammer(void)
{
    actor_set(&F.act, ANIM_HAMMER);
    F.vx = F.vy = 0;
    F.can_fix = 0;
    actor_mirror(&F.act, F.x > FIX(win_center(F.row, F.slot)));
}

void felix_set_state(uint8_t s)
{
    F.up_clamped = 0;
    F.state = s;
    switch (s) {
    case FELIX_IDLE:
        actor_set(&F.act, ANIM_STAND);
        F.vx = F.vy = 0;
        break;
    case FELIX_WALK_IN:
        actor_set(&F.act, ANIM_WALK);
        break;
    case FELIX_WALK:
        actor_set(&F.act, ANIM_WALK);
        break;
    case FELIX_JUMP:
        start_side_jump();
        break;
    case FELIX_JUMP_UP:
        actor_set(&F.act, ANIM_AIR);
        F.vx = 0;
        F.airborne = 1;
        F.can_updown = 0;
        sfx_play(SND_JUMP);
        break;
    case FELIX_DROP:
        actor_set(&F.act, ANIM_AIR);
        F.vx = 0;
        F.airborne = 1;
        F.can_updown = 0;
        sfx_play(SND_DROP);
        break;
    case FELIX_STEP:
        start_step();
        break;
    case FELIX_HAMMER:
        start_hammer();
        break;
    case FELIX_LAND:
        actor_set(&F.act, ANIM_LAND);
        break;
    default:
        felix_kill();
        break;
    }
}

void felix_kill(void)
{
    if (!pl.lives) {
        music_play(SND_MUSIC_GAME_OVER, 0);
        actor_set(&F.act, ANIM_DEATH);
        actor_mirror(&F.act, 0);
        pl.out = 1;
    } else {
        actor_set(&F.act, ANIM_HIT);
    }
    F.up_clamped = 0;
    F.vx = F.vy = 0;
    F.die_phase = 0;
    F.super_on = 0;
    ralph_freeze();
    F.state = FELIX_DEAD;
}

void felix_reset_flags(void)
{
    key_latch = 0;
    F.scroll_jump = 0;
    F.super_on = 0;
    F.can_fix = F.can_jump = F.can_updown = 0;
    F.up_clamped = F.airborne = F.stopped = 0;
    actor_show(&F.act, 1);
}

void felix_start_walk_in(void)
{
    F.up_clamped = 0;
    actor_set(&F.act, ANIM_WALK);
    F.state = FELIX_WALK_IN;
}

void felix_create(void) BANKED
{
    memset(&F, 0, sizeof F);
    F.act.flags = AF_VISIBLE | AF_MIRROR;
    F.x = START_X_FIX;
    F.y = FIX(GROUND_Y);
    F.row = F.fix_row = NO_ROW;
    F.can_fix = F.can_jump = F.can_updown = 1;
    felix_set_state(FELIX_IDLE);
}

void felix_level_start(void)
{
    actor_mirror(&F.act, 1);
    F.x = WALK_IN_X_FIX;
    F.y = FIX(GROUND_Y);
    felix_reset_flags();
    F.row = F.fix_row = NO_ROW;
    F.can_updown = F.can_jump = F.can_fix = 1;
    F.act.flags &= (uint8_t)~AF_FROZEN;
    felix_start_walk_in();
}

void felix_place_after_scroll(void)
{
    uint8_t below = pl.floor_row - 1;
    F.x = FIX(win_right(below, 4) - 1);
    F.y = FIX(win_sill(below, 4));
    actor_mirror(&F.act, 1);
    F.scroll_jump = 1;
    F.super_on = 0;
    felix_set_state(FELIX_JUMP_UP);
}

static void (*const handlers[11])(void) = {
    on_idle, on_walk_in, on_idle, on_walk, on_jump_side, on_jump_up,
    on_fall, on_step_hop, on_hammer, on_fall, on_dead,
};

static void run_handler(void)
{
    handlers[F.state]();
}

static void super_colours(void)
{
    if (!F.super_on) {
        felix_f = felix_h = 0;
        return;
    }
    if (ticks & X8(felix_super_cycle_mask))
        return;
    F.cycle_h += X8(felix_super_cycle_step);
    F.cycle_f += X8(felix_super_cycle_step);
    if (F.cycle_h % X8(felix_super_cycle_mod) == 0)
        F.cycle_h++;
    felix_h = F.cycle_h % X8(felix_super_cycle_mod);
    felix_f = F.cycle_f % X8(felix_super_cycle_mod);
}

void felix_update(void)
{
    F.prev_y = F.y;
    if (!F.airborne) {
        if (!(key_held & K_JUMP))
            F.can_jump = 1;
        if (!(key_held & (K_UP | K_DOWN)))
            F.can_updown = 1;
        if (!(key_held & K_FIX))
            F.can_fix = 1;
    }
    run_handler();
    if (F.super_on) {
        if (ticks & X8(felix_super_odd_mask))
            run_handler();
        if (!--F.super_timer)
            F.super_on = 0;
    }
    if (F.vx < 0)
        actor_mirror(&F.act, 1);
    else if (F.vx > 0)
        actor_mirror(&F.act, 0);
    if (F.x < MIN_X_FIX)
        F.x = MIN_X_FIX;
    if (F.x > MAX_X_FIX)
        F.x = MAX_X_FIX;
    super_colours();
}

void felix_draw(void)
{
    actor_draw_super(&F.act, felix_x(), felix_y(), felix_f, felix_h);
}

uint8_t game_over_route(void)
{
    if (players != 1) {
        if (!player_out_other()) {
            sfx_play(SND_PLAYER_CHANGE);
            return ST_SWITCH;
        }
        player_select(0);
        if (hiscore_insert())
            return ST_ENTRY;
        player_select(1);
    }
    return hiscore_insert() ? ST_ENTRY : ST_GAME_OVER;
}
