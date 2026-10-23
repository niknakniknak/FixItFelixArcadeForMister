/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "ralph.h"
#include "bldg.h"
#include "felix.h"
#include "rng.h"
#include "sound.h"
#include "actors.h"
#include "input.h"
#include "service.h"

#define ANIM_STAND X8(ralph_anim_stand)
#define ANIM_ROAR X8(ralph_anim_roar)
#define ANIM_SMASH X8(ralph_anim_smash)
#define ANIM_BACK X8(ralph_anim_back)
#define ANIM_FISTS X8(ralph_anim_fists)
#define ANIM_CLIMB X8(ralph_anim_climb)
#define ANIM_BRICK X8(ralph_anim_brick)

#define BRICKS 28
#define LEFT_TARGET X8(ralph_left_target)
#define RIGHT_TARGET X8(ralph_right_target)
#define RECT_FIST_A X8(ralph_rect_fist_a)
#define RECT_FIST_B X8(ralph_rect_fist_b)
#define RECT_RAGE_A X8(ralph_rect_rage_a)
#define RECT_RAGE_B X8(ralph_rect_rage_b)
#define RAGE_POSE X8(ralph_rage_pose)
#define RAGE_POUND X8(ralph_rage_pound)
#define CLIMB_DELAY X8(ralph_climb_delay)
#define CLIMB_DAMAGE_EVERY X8(ralph_climb_damage_every)
#define BRICK_WINDOW_BREAKS X8(ralph_brick_window_breaks)

typedef struct {
    actor_t act;
    uint8_t active;
    uint8_t bouncing;
    uint8_t deflected;
    uint8_t blinks;
    uint8_t last;
    uint8_t wslot, wrows, wn;
    int16_t wnext;
    fix_t x, y, vx, vy;
} brick_t;

static brick_t bricks[BRICKS];
#define slot_x_lo exe_slot_x_lo
#define slot_x_hi exe_slot_x_hi

#define HI16(v) (((int16_t *)&(v))[1])
#define LO16(v) (((uint16_t *)&(v))[0])
static uint8_t brick_hi;
static const uint8_t interval_num[4] = { 1, 3, 3, 3 };
static const uint8_t interval_den[4] = { 1, 2, 4, 5 };

static uint8_t speed_level(void)
{
    return play_opt[OPT_SPEED] ? (pl.level < 8 ? pl.level : 8) : 0;
}

static uint8_t brick_interval(uint8_t frames)
{
    uint8_t d = (dsw1 >> 4) & 3;
    frames = (uint8_t)(frames * interval_num[d] / interval_den[d]);
    frames -= frames * speed_level() / 16;
    return frames ? frames : 1;
}

#define R (pl.ralph)

void ralph_place(uint8_t row) BANKED
{
    R.row = row;
    R.x = RALPH_CENTER;
    R.y = row_bottom_y(row);
    R.at_center = 1;
    ralph_set_state(RALPH_WALK);
}

static void walk_setup(void)
{
    if (!R.at_center) {
        R.target = RALPH_CENTER;
        R.dx = R.x > RALPH_CENTER ? -1 : 1;
    } else if (!R.alt) {
        R.dx = -1;
        R.target = LEFT_TARGET;
        R.alt = 1;
    } else {
        R.target = RIGHT_TARGET;
        R.alt = 0;
        R.dx = 1;
    }
    actor_set(&R.act, ANIM_ROAR);
}

static void smash_setup(void)
{
    actor_set(&R.act, ANIM_SMASH);
    R.timer = X8(ralph_smash_first_timer);
    R.phase = 0;
    if (!--R.smashes) {
        R.smashes = X8(ralph_smashes_per_chunk);
        R.chunk_timer = X8(ralph_chunk_timer);
    }
}

static void rage_setup(void)
{
    actor_set(&R.act, ANIM_FISTS);
    pl.rage_request = 0;
    R.timer = 0;
    R.phase = 0;
}

void ralph_set_state(uint8_t s)
{
    R.state = s;
    switch (s) {
    case RALPH_WALK:
        walk_setup();
        break;
    case RALPH_SMASH:
        smash_setup();
        break;
    case RALPH_FLOOR_DONE:
        actor_set(&R.act, pl.floors_left == 1 ? ANIM_STAND : ANIM_FISTS);
        break;
    case RALPH_CLIMB:
        R.climbing = 1;
        R.delay = CLIMB_DELAY;
        R.timer = CLIMB_DAMAGE_EVERY;
        break;
    case RALPH_LEVEL_DONE:
        actor_set(&R.act, ANIM_FISTS);
        break;
    case RALPH_RAGE:
        rage_setup();
        break;
    }
}

static uint8_t brick_rows(uint8_t x, uint8_t s);

static uint8_t brick_slot(int16_t bx)
{
    uint8_t x, s;
    if ((uint16_t)(bx - LEFT_LIMIT) >= 160)
        return SLOTS;
    x = (uint8_t)bx;
    s = (uint8_t)(x - LEFT_LIMIT) >> 5;
    return x < slot_x_lo[s] || x >= slot_x_hi[s] ? SLOTS : s;
}

static void brick_spawn(void)
{
    uint8_t i;
    brick_t *b = bricks;
    for (i = 0; i < BRICKS; i++, b++) {
        if (b->active)
            continue;
        b->x = FIX((int16_t)(rng() & X8(ralph_brick_dx_mask)) + XS8(ralph_brick_dx_min) + R.x);
        b->y = FIX(R.y + X8(ralph_brick_dy));
        b->wslot = brick_slot(HI16(b->x));
        if (b->wslot != SLOTS)
            b->wrows = brick_rows((uint8_t)HI16(b->x), b->wslot);
        b->wnext = -32768;
        b->wn = 0xFF;
        b->blinks = X8(ralph_brick_blinks);
        b->active = 1;
        if (i >= brick_hi)
            brick_hi = i + 1;
        b->bouncing = 0;
        b->act.flags = AF_VISIBLE;
        actor_set(&b->act, ANIM_BRICK);
        return;
    }
}

static void pound_sound(uint8_t a, uint8_t b)
{
    uint8_t rect;
    if (!(R.act.flags & AF_STEPPED))
        return;
    rect = actor_rect(&R.act);
    if (rect == a || rect == b)
        sfx_play(SND_POUND);
}

static void walk(void)
{
    R.x += R.dx;
    if ((R.dx < 0 && R.x <= R.target) || (R.dx >= 0 && R.x >= R.target)) {
        R.x = R.target;
        R.at_center = R.target == RALPH_CENTER;
        ralph_set_state(RALPH_SMASH);
    }
    if (pl.rage_request)
        ralph_set_state(RALPH_RAGE);
}

static void chunk_throw(void)
{
    uint8_t col;
    if (!R.chunk_timer || --R.chunk_timer)
        return;
    if (R.at_center)
        col = (rng() & 1) + 1;
    else if (R.x < RALPH_CENTER)
        col = rng() & 1;
    else
        col = (rng() & 1) + X8(ralph_chunk_col_right);
    chunk_spawn(col);
}

static void smash_tick(void)
{
    if (pl.rage_request) {
        ralph_set_state(RALPH_RAGE);
        return;
    }
    pound_sound(RECT_FIST_A, RECT_FIST_B);
    if (R.phase == 0) {
        if (R.act.frame > X8(ralph_smash_start_frame))
            R.phase = 1;
        return;
    }
    if (!--R.timer) {
        brick_spawn();
        R.timer = brick_interval((rng() & X8(ralph_brick_rand_mask)) + X8(ralph_brick_rand_base));
    }
    chunk_throw();
    if (R.act.flags & AF_DONE)
        ralph_set_state(RALPH_WALK);
}

static void climb_damage(void)
{
    uint8_t slot, row = R.row;
    uint16_t r;
    slot = rng() & X8(ralph_climb_slot_mask);
    R.timer = CLIMB_DAMAGE_EVERY;
    if (rng() % X8(ralph_climb_pct) >= X8(ralph_climb_row_up_pct) && row + 1 < pl.rows)
        row++;
    if (R.x >= RALPH_CENTER)
        slot++;
    r = rng();
    if (r % X8(ralph_climb_pct) < X8(ralph_climb_damage_pct))
        window_damage(row, slot, 0, 1);
}

static void climb(void)
{
    if (R.delay) {
        if (!--R.delay)
            actor_set(&R.act, ANIM_CLIMB);
        return;
    }
    if (!R.climbing)
        return;
    if (R.x < RALPH_CENTER)
        R.x++;
    else if (R.x > RALPH_CENTER)
        R.x--;
    if (!--R.timer)
        climb_damage();
    if (R.x != RALPH_CENTER)
        return;
    R.y--;
    if (R.y < (pl.row_top[R.row] - 2 * row_height(R.row)) * 8) {
        uint8_t above = pl.floor_row + FLOOR_ROWS;
        R.x = RALPH_CENTER;
        R.row = above;
        R.y = row_bottom_y(above);
        R.at_center = 1;
        R.climbing = 0;
        if (row_type(above) == X8(ralph_climb_roof_type))
            R.y -= X8(ralph_climb_roof_dy);
        actor_set(&R.act, ANIM_FISTS);
    }
}

static void rage(void)
{
    if (R.phase == 0) {
        if (++R.timer > RAGE_POSE) {
            actor_set(&R.act, ANIM_BACK);
            R.phase = 1;
            R.timer = 0;
        }
        return;
    }
    pound_sound(RECT_RAGE_A, RECT_RAGE_B);
    if (++R.timer <= RAGE_POUND) {
        if (!(ticks & X8(ralph_rage_brick_mask)))
            brick_spawn();
        return;
    }
    if (!R.at_center) {
        R.target = RALPH_CENTER;
        R.dx = R.x > RALPH_CENTER ? -1 : 1;
    } else if (!R.alt) {
        R.dx = -1;
        R.target = LEFT_TARGET;
        R.alt = 1;
    } else {
        R.target = RIGHT_TARGET;
        R.alt = 0;
        R.dx = 1;
    }
    actor_set(&R.act, ANIM_ROAR);
    R.state = RALPH_WALK;
}


#define hit_x0 exe_hit_x0
#define hit_x1 exe_hit_x1
#define hit_y0 exe_hit_y0
#define hit_y1 exe_hit_y1
#define hit_dy exe_hit_dy

typedef struct {
    int16_t ylo, yhi;
} wbox_t;

static wbox_t wbox[FLOOR_ROWS * SLOTS];
static uint8_t wrow_type[FLOOR_ROWS];
static const uint8_t times5[MAX_ROWS] = { 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80 };
static uint8_t wrow_floor = 0xFF, wrow_level, wrow_cur;

static void win_brick_setup(void)
{
    static uint8_t n, s, r, t;
    static int16_t top, lo, hi;
    static wbox_t *e;
    if (wrow_floor == pl.floor_row && wrow_level == pl.level && wrow_cur == cur)
        return;
    wrow_floor = pl.floor_row;
    wrow_level = pl.level;
    wrow_cur = cur;
    r = pl.floor_row;
    e = wbox;
    for (n = 0; n < FLOOR_ROWS; n++, r++) {
        t = pl.row_types[r];
        wrow_type[n] = t;
        top = (int16_t)pl.row_top[r] * 8;
        for (s = 0; s < SLOTS; s++, e++) {
            lo = hit_y0[t][s] + hit_dy[t][s];
            hi = hit_y1[t][s] + hit_dy[t][s];
            if (lo < X8(bldg_brick_rows_top))
                lo = X8(bldg_brick_rows_top);
            if (hi > X8(bldg_brick_rows_top) + X8(bldg_brick_rows_span))
                hi = X8(bldg_brick_rows_top) + X8(bldg_brick_rows_span);
            if (hi < lo)
                hi = lo;
            e->ylo = top + lo;
            e->yhi = top + hi;
        }
    }
}

static uint8_t brick_rows(uint8_t x, uint8_t s)
{
    uint8_t n, t, m = 0;
    win_brick_setup();
    for (n = 0; n < FLOOR_ROWS; n++) {
        t = wrow_type[n];
        if (x >= hit_x0[t][s] && x < hit_x1[t][s])
            m |= 1 << n;
    }
    return m;
}

static void brick_next_box(brick_t *b, int16_t y)
{
    static uint8_t n, m;
    static wbox_t *e;
    b->wnext = 32767;
    b->wn = 0xFF;
    for (n = 0, m = b->wrows, e = &wbox[b->wslot]; m; n++, m >>= 1, e += SLOTS)
        if ((m & 1) && y < e->yhi && e->ylo < b->wnext) {
            b->wnext = e->ylo;
            b->wn = n;
        }
}

static uint8_t brick_window(brick_t *b, uint8_t *slot)
{
    static uint8_t s, id, r, d, i, first;
    static int16_t y;
    static uint16_t hit;
    if (pl.floor_done || b->wslot == SLOTS)
        return NO_ROW;
    y = HI16(b->y);
    if (y < 0 && LO16(b->y))
        y++;
    first = times5[pl.floor_row];
    s = b->wslot;
    if (y < b->wnext)
        goto none;
    if (b->wn == 0xFF || y >= wbox[times5[b->wn] + s].yhi) {
        brick_next_box(b, y);
        if (y < b->wnext)
            goto none;
    }
    r = pl.floor_row + b->wn;
    id = times5[r] + s;
    if ((b->last == first && id == first) || ((&pl.win[0][0])[id].st & WIN_BROKEN) == 2)
        goto none;
    d = r - pl.win_row;
    if (d < FLOOR_ROWS) {
        i = times5[d] + s;
        if (pl.popup_on[i] || pl.repair[i])
            goto none;
    }
    hit = rng() + XU16(ralph_hit_add);
    if ((hit & X8(ralph_hit_mask)) || (uint16_t)((hit >> X8(ralph_hit_shift)) * XU16(ralph_hit_inv)) > XU16(ralph_hit_limit))
        goto none;
    b->last = id;
    *slot = s;
    return r;
none:
    b->last = first + FLOOR_ROWS * SLOTS - 1;
    return NO_ROW;
}

static void brick_bounce(brick_t *b, uint8_t deflected)
{
    b->vx = rng() % X8(ralph_bounce_pct) < X8(ralph_bounce_left_pct) ? FIX_1_3 : -FIX_1_3;
    b->vy = XL(ralph_bounce_vy);
    b->deflected = deflected;
    b->bouncing = 1;
}

static void brick_kill(brick_t *b)
{
    b->active = 0;
    actor_show(&b->act, 0);
}

static int16_t felix_px;
static int16_t kill_hi;
static fix_t fall_v;

static void brick_fall(brick_t *b)
{
    uint8_t row, slot;
    b->y += fall_v;
    if (HI16(b->y) < GROUND_Y) {
        if (HI16(b->y) > kill_hi || (HI16(b->y) == kill_hi && LO16(b->y)))
            brick_kill(b);
    } else {
        b->y = FIX(GROUND_Y);
        brick_bounce(b, 0);
    }
    if ((uint16_t)(HI16(b->x) - felix_px + 10) <= 20) {
        box_x = b->x;
        box_y = b->y;
        if (felix_hit()) {
            if (!pl.felix.super_on) {
                brick_kill(b);
                sfx_play(SND_FELIX_HIT);
                felix_kill();
            } else {
                brick_bounce(b, 1);
                sfx_play(SND_DEFLECT);
            }
        }
    }
    if (pl.level > 0 && R.breaks < BRICK_WINDOW_BREAKS) {
        row = brick_window(b, &slot);
        if (row != NO_ROW) {
            brick_kill(b);
            window_damage(row, slot, 0, 1);
            R.breaks++;
        }
    }
}

static void brick_fly(brick_t *b)
{
    b->vy += FIX_0_4;
    b->x += b->vx;
    b->y += b->vy;
    if (b->deflected) {
        if (fix_trunc(b->y) + scroll_y > X16(ralph_deflect_kill_y))
            brick_kill(b);
        return;
    }
    if ((ticks & X8(ralph_blink_mask)) < X8(ralph_blink_on)) {
        actor_show(&b->act, 1);
    } else {
        if ((b->act.flags & AF_VISIBLE) && !--b->blinks)
            b->active = 0;
        actor_show(&b->act, 0);
    }
    if (b->y > FIX(GROUND_Y))
        brick_kill(b);
}

static void bricks_update(void)
{
    uint8_t i;
    brick_t *b = bricks;
    felix_px = (int16_t)(pl.felix.x >> 16);
    kill_hi = R.y + X16(ralph_brick_kill_dy);
    fall_v = FIX_1_9;
    if (play_opt[OPT_SPEED])
        for (i = speed_level(); i; i--)
            fall_v += FIX_0_1;
    box_w = box_h = 0;
    box_felix_w = X8(ralph_box_felix_w);
    for (i = brick_hi; i; i--, b++) {
        if (!b->active)
            continue;
        if (!b->bouncing)
            brick_fall(b);
        else
            brick_fly(b);
    }
    while (brick_hi && !bricks[brick_hi - 1].active)
        brick_hi--;
}

void ralph_update(void)
{
    if (!R.frozen) {
        switch (R.state) {
        case RALPH_WALK: walk(); break;
        case RALPH_SMASH: smash_tick(); break;
        case RALPH_CLIMB: climb(); break;
        case RALPH_RAGE: rage(); break;
        }
        bricks_update();
    }
}

void bricks_clear(void) BANKED
{
    uint8_t i;
    brick_hi = 0;
    for (i = 0; i < BRICKS; i++) {
        bricks[i].active = 0;
        bricks[i].act.flags = 0;
    }
}

void ralph_freeze(void)
{
    uint8_t i;
    for (i = 0; i < brick_hi; i++)
        if (bricks[i].active)
            bricks[i].act.flags |= AF_FROZEN;
    R.act.flags |= AF_FROZEN;
    R.frozen = 1;
}

void ralph_reset(void)
{
    R.frozen = 0;
    R.alt = 0;
    bricks_clear();
    R.act.flags &= (uint8_t)~AF_FROZEN;
    if (R.state != RALPH_LEVEL_DONE)
        ralph_set_state(RALPH_WALK);
    actor_show(&R.act, 1);
}

void ralph_floor_done(void)
{
    bricks_clear();
    R.breaks = 0;
    ralph_set_state(RALPH_FLOOR_DONE);
}

void ralph_level_done(void)
{
    bricks_clear();
    ralph_set_state(RALPH_LEVEL_DONE);
    actor_show(&R.act, 0);
}

void ralph_draw(void)
{
    actor_draw(&R.act, R.x, R.y);
}

static fix_t round_v;

static int16_t brick_px(fix_t v)
{
    int16_t hi;
    round_v = v + FIX_0_5;
    hi = HI16(round_v);
    if (hi < 0 && LO16(round_v))
        hi++;
    return hi;
}

void bricks_draw(void)
{
    uint8_t i;
    brick_t *b = bricks;
    for (i = brick_hi; i; i--, b++) {
        if (b->active)
            actor_draw(&b->act, b->bouncing ? brick_px(b->x) : HI16(b->x), brick_px(b->y));
    }
}

void bricks_tick(void)
{
    uint8_t i;
    brick_t *b = bricks;
    for (i = brick_hi; i; i--, b++)
        if (b->active)
            actor_tick(&b->act);
}
