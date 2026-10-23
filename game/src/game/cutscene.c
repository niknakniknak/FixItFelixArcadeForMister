/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "cutscene.h"
#include "bldg.h"
#include "bg.h"
#include "hud.h"
#include "player.h"
#include "rng.h"
#include "sound.h"
#include "service.h"
#include "exe.h"

#define OBJS 21
#define PICS 8
#define NO_TM 0xFF
#define ROW1_TOP X16(cut_row1_top)
#define SMASH_ANY 4
#define SMASH_AVOID 5
#define ROW3_TOP X16(cut_row3_top)
#define FG_BUBBLE 4
#define FALL_STEP XL(cut_fall_step)

enum {
    O_SCENERY, O_STUMP, O_BUBBLE, O_CRANE0, O_CRANE1, O_CRANE2, O_CRANE3, O_DOOR,
    O_DOZER, O_FELIX, O_HAMMER, O_RALPH, O_DUST0, O_DUST1, O_DUST2, O_DUST3,
    O_HEAD0, O_HEAD1, O_HEAD2, O_HEAD3, O_HEAD4
};

#define L_HATCH 0
#define L_MEDAL 1
#define L_CLOUD0 2
#define L_CLOUD1 3
#define L_NICE0 4

enum { MODE_NONE, MODE_CUT, MODE_END };

#define N NO_TM
static const uint8_t pic_tm[35] = {
    N, N, N, TM_MEDAL0, TM_MEDAL1, TM_MEDAL2, TM_MEDAL3, N, N, N, N, N, N,
    N, N, N, N, N, N, N, N, TM_BUBBLE_STUMP,
    TM_DOOR_CLOSED, TM_DOOR_OPEN, TM_SCENERY, N, N, N, N, N, N, TM_DUMP, TM_BUBBLE_WRECK, N,
    TM_BUBBLE_FIX,
};
#undef N
static const uint8_t draw_order[OBJS] = {
    O_BUBBLE, O_HEAD0, O_HEAD1, O_HEAD2, O_HEAD3, O_HEAD4, O_DOZER, O_CRANE2, O_CRANE0,
    O_CRANE3, O_CRANE1, O_STUMP, O_FELIX, O_RALPH, O_HAMMER, O_DUST0, O_DUST1, O_DUST2,
    O_DUST3, O_DOOR, O_SCENERY,
};
static const uint8_t draw_order_cut2[OBJS] = {
    O_BUBBLE, O_DOZER, O_CRANE2, O_CRANE0, O_CRANE3, O_CRANE1, O_STUMP, O_FELIX, O_RALPH,
    O_HAMMER, O_DUST0, O_DUST1, O_DUST2, O_DUST3, O_HEAD0, O_HEAD1, O_HEAD2, O_HEAD3,
    O_HEAD4, O_DOOR, O_SCENERY,
};
static const uint8_t end_order[9] = {
    L_HATCH, L_NICE0, L_NICE0 + 1, L_NICE0 + 2, L_NICE0 + 3, L_NICE0 + 4, L_CLOUD0, L_MEDAL,
    L_CLOUD1,
};
#define intro_objs exe_intro_objs
#define hit_rows exe_cut_hit_rows
#define hit_slots exe_cut_hit_slots

uint8_t crash_pending;

static actor_t oa[OBJS];
static int16_t ox[OBJS], oy[OBJS];
static uint8_t mode;
static void cut0(void);
static void objs_pics(void);
static void cut1(void);
static void cut2(void);
static void cut3(void);
static void (*script)(void);
static uint8_t cstep;
static uint8_t advance;
static uint8_t done;
static uint8_t flag_a, flag_b, flag_c;
static uint8_t text_on;
static uint8_t text_shown;
static uint8_t count;
static uint8_t sub;
static uint8_t f0;
static uint8_t steps;
static int16_t a4;
#define CX ox[O_CRANE0]
static fix_t throw_y, throw_vy;
#define shake_y exe_cut_shake_y
static uint8_t shake_i, shake_n;
static uint8_t scen_y;
#define hop_y exe_cut_hop_y
static uint8_t hop;

#define A(i) (&oa[i])
#define finished(i) (oa[i].flags & AF_DONE)
#define RX ox[O_RALPH]
#define RY oy[O_RALPH]
#define SX ox[O_STUMP]
#define SY oy[O_STUMP]
#define DX ox[O_DOZER]
#define FX ox[O_FELIX]
#define HX ox[O_HEAD0]
#define NX ox[L_NICE0]

static void anim(uint8_t i, uint8_t a)
{
    actor_set(A(i), a);
}

static void show(uint8_t i, uint8_t on)
{
    actor_show(A(i), on);
}

static void hide(uint8_t i)
{
    show(i, 0);
}

static void unhide(uint8_t i)
{
    show(i, 1);
}

static void show_range(uint8_t first, uint8_t last, uint8_t on)
{
    for (; first <= last; first++)
        show(first, on);
}

static void adv(void)
{
    advance = 1;
}

static void spawn(uint8_t i, uint8_t a, int16_t x, int16_t y)
{
    anim(i, a);
    ox[i] = x;
    oy[i] = y;
    unhide(i);
}


static void step_done(void)
{
    advance = 1;
    f0 = 0;
}

static uint8_t waited(uint8_t n)
{
    return ++f0 > n;
}

static uint8_t move_x(int8_t d, int16_t lim)
{
    RX += d;
    if (d > 0 ? RX <= lim : RX >= lim)
        return 0;
    RX = lim;
    return 1;
}

static void pan_follow(void)
{
    if (RX > X16(cut_pan_x))
        scroll_x = X16(cut_pan_x) - RX;
}

static uint8_t climb_to(int16_t lim)
{
    RY -= X8(cut_climb_dy2);
    if (RY >= lim)
        return 0;
    RY = lim;
    return 1;
}

static uint8_t rect_tm(uint8_t rect)
{
    rect -= 178;
    return rect < sizeof pic_tm ? pic_tm[rect] : NO_TM;
}

static uint8_t bubble_said;

static void pic_off(uint8_t i)
{
    if (i < PICS && bg_pics[i].on)
        bg_pic_off(i);
    if (i == O_BUBBLE) {
        fg_pic_off(FG_BUBBLE);
        bubble_said = 0;
    }
}

static void bubble_voice(uint8_t tm)
{
    if (tm == bubble_said)
        return;
    bubble_said = tm;
    if (tm == TM_BUBBLE_WRECK && !opt[OPT_VOICES])
        sfx_play(SND_VOICE_WRECK);
}

static void pic_place(uint8_t i, uint8_t tm, int8_t col, int16_t row)
{
    bg_pic_t *p = &bg_pics[i];
    if (tm == TM_BUBBLE_STUMP || tm == TM_BUBBLE_WRECK || tm == TM_BUBBLE_FIX) {
        fg_pic_set(FG_BUBBLE, tm, col + (scroll_x >> 3), row + (scroll_y >> 3));
        bubble_voice(tm);
        return;
    }
    if (p->on && p->tm == tm && p->col == col && p->row == row)
        return;
    bg_pic_set(i, tm, col, row);
}


static const anim_frame_t *rf;
static actor_t *ra;
static uint8_t ri, rrect, pics_pass;
static int16_t rx, ry;

static void obj_render(void)
{
    ra = &oa[ri];
    rx = ox[ri];
    ry = oy[ri];
    if (ri >= PICS || !(ra->flags & AF_VISIBLE)) {
        if (pics_pass)
            pic_off(ri);
        else
            actor_draw(ra, rx, ry);
        return;
    }
    rf = &anims[ra->anim].frames[ra->frame];
    rrect = rf->rect;
    if (ra->flags & AF_MIRROR)
        rrect = rf->mirror;
    rrect = rect_tm(rrect);
    if (rrect == NO_TM) {
        if (pics_pass)
            pic_off(ri);
        else
            actor_draw(ra, ox[ri], ry);
        return;
    }
    if (pics_pass)
        pic_place(ri, rrect, (int8_t)((rx - rf->hx) >> 3), (ry - rf->hy) >> 3);
}

static void objs_reset(void)
{
    uint8_t i;
    for (i = 0; i < OBJS; i++) {
        pic_off(i);
        oa[i].flags = 0;
    }
}

static void objs_init(const cinit_t *t, uint8_t n, int16_t base)
{
    static uint8_t i;
    for (i = 0; i < n; i++, t++) {
        actor_set(&oa[i], t->anim);
        oa[i].flags = t->flags;
        ox[i] = t->x;
        oy[i] = t->y + base;
    }
}

static void ground_edges(void)
{
    if (ground_on && rows_shown)
        bg_pic_set(BG_PIC_GROUND, TM_GROUND_BUSHES, 3, 38);
    else
        bg_pic_off(BG_PIC_GROUND);
}

static void ground_show(uint8_t on)
{
    ground_on = on;
    ground_edges();
    bg_rows_dirty(38, 2);
}

static void player_actors_show(uint8_t on)
{
    actor_show(&pl.felix.act, on);
    actor_show(&pl.ralph.act, on);
}


static void smash(uint8_t threshold, uint8_t mode)
{
    uint8_t row, slot;
    if ((rng() & X8(cut_smash_mask)) <= threshold)
        return;
    do {
        if (mode < SMASH_ANY)
            row = mode + (rng() % X8(cut_smash_row_mod) >= X8(cut_smash_row_half));
        else
            row = rng() & X8(cut_smash_avoid_mask);
        slot = rng() % X8(cut_smash_slot_mod);
    } while (mode == SMASH_AVOID && row == hit_rows[0] && slot == hit_slots[0]);
    window_damage(row, slot, 0, 1);
}

static void dust_at(uint8_t row)
{
    static uint8_t i;
    for (i = O_DUST0; i <= O_DUST3; i++) {
        ox[i] = (i - O_DUST0) * X8(cut_dust_step) + (row ? X8(cut_dust_x0) : X8(cut_dust_row0_x0));
        oy[i] = row_top_y(row) + X8(cut_dust_dy);
    }
}

static void cut1_start(void)
{
    level_build();
    ground_show(1);
    script = cut1;
    dust_at(0);
    show_range(O_CRANE0, O_CRANE3, 1);
    show_range(O_DUST0, O_DUST3, 1);
    oy[O_DOOR] = X16(cut_door_y);
    count = 0;
    cstep = 0;
    f0 = 0;
    a4 = X16(cut_a4_start);
}

static void ralph_enter(int16_t x)
{
    RX = x;
    RY = X16(cut_ralph_y);
    hide(O_STUMP);
    hide(O_SCENERY);
    oa[O_RALPH].flags = AF_VISIBLE;
    anim(O_RALPH, X8(cut_enter_anim));
    f0 = 0;
    cstep = 0;
    rows_shown = pl.rows;
    ground_edges();
    bg_invalidate();
}

static void cut2_start(void)
{
    script = cut2;
    ralph_enter(X16(cut2_start_x));
    flag_a = 1;
    music_play(SND_MUSIC_INTRO, 0);
}

static void cut3_start(void)
{
    script = cut3;
    ralph_enter(0);
}

static void shake_step(void)
{
    if (++shake_n < X8(cut_shake_period))
        return;
    shake_n = 0;
    shake_i = shake_i == X8(cut_shake_last) ? 0 : shake_i + 1;
    scen_y = shake_y[shake_i];
}

static void cut0(void)
{
    switch (cstep) {
    case 0:
        if (--RX <= X8(cut0_ralph_stop_x)) {
            RX = X8(cut0_ralph_stop_x);
            hide(O_STUMP);
            anim(O_RALPH, X8(cut0_ralph_anim_lift));
            flag_a = 1;
            adv();
        }
        break;
    case 1:
        if (flag_a) {
            if (finished(O_RALPH)) {
                flag_a = 0;
                unhide(O_STUMP);
                anim(O_RALPH, X8(cut0_ralph_anim_carry));
            }
            return;
        }
        if (++RY > X16(cut0_ralph_exit_y)) {
            unhide(O_DOZER);
            hide(O_RALPH);
            anim(O_STUMP, X8(cut0_stump_anim));
            A(O_STUMP)->flags |= AF_FROZEN;
            shake_i = 0;
            shake_n = 0;
            step_done();
        }
        break;
    case 2:
        DX -= X8(cut0_dozer_dx);
        if (DX < XS8(cut0_dozer_wrap)) {
            anim(O_SCENERY, X8(cut0_scenery_anim));
            DX = X16(cut0_dozer_x2);
            SX = X16(cut0_stump_x2);
            flag_b = 0;
            shake_n = 0;
            step_done();
        } else if (DX < X8(cut0_push_x)) {
            SX = DX - X8(cut0_push_back);
        }
        shake_step();
        return;
    case 3:
        if (!flag_b) {
            DX -= X8(cut0_dozer_dx);
            SX -= X8(cut0_dozer_dx);
            if (SX < X8(cut0_sink_x) && (ticks & 1))
                SY--;
            if (DX < X8(cut0_dozer_turn))
                flag_b = 1;
            shake_step();
            return;
        }
        DX += X8(cut0_dozer_dx);
        if (DX > X8(cut0_dozer_out))
            step_done();
        return;
    case 4:
        if (waited(X8(cut0_wait4))) {
            anim(O_STUMP, X8(cut0_stump_anim));
            adv();
        }
        return;
    case 5:
        if (flag_c && finished(O_STUMP)) {
            hide(O_BUBBLE);
            anim(O_STUMP, X8(cut0_stump_anim2));
            music_play(SND_MUSIC_STORY2, 0);
            step_done();
        } else if (A(O_STUMP)->frame > X8(cut0_bubble_frame)) {
            ox[O_BUBBLE] = SX;
            oy[O_BUBBLE] = SY;
            unhide(O_BUBBLE);
            flag_c = 1;
        }
        return;
    default:
        if (waited(X8(cut0_wait6))) {
            show_range(O_SCENERY, O_STUMP, 0);
            hide(O_DOZER);
            cut1_start();
        }
        return;
    }
}

static void cut1_heads(void)
{
    static uint8_t i, h;
    static int16_t x;
    x = a4;
    h = hop;
    for (i = O_HEAD0; i <= O_HEAD4; i++) {
        ox[i] = x;
        oy[i] = hop_y[h] - (x < X8(cut1_hop_low_x) ? X8(cut1_hop_low_dy) : 0);
        if (++h == X8(cut1_hop_mod))
            h = 0;
        if (x < X8(cut1_head_behind_x))
            hide(i);
        x += X8(cut_head_dx);
    }
    if (!(ticks & X8(cut1_hop_tick_mask)) && ++hop == X8(cut1_hop_mod))
        hop = 0;
}

static void cut1(void)
{
    uint8_t i;
    if (cstep == 0) {
        if (!waited(X8(cut1_row_wait)))
            return;
        f0 = 0;
        if (rows_shown > X8(cut1_rows_done)) {
            for (i = O_DUST0; i <= O_DUST3; i++)
                anim(i, X8(cut1_dust_anim));
            show_range(O_HEAD0, O_HEAD4, 1);
            for (i = O_CRANE0; i <= O_CRANE3; i++)
                A(i)->flags &= (uint8_t)~AF_FROZEN;
            adv();
            return;
        }
        bg_rows_dirty(pl.row_top[rows_shown], row_height(rows_shown));
        dust_at(++rows_shown);
        ground_edges();
        return;
    }
    if (cstep == 1) {
        if (--CX < X8(cut1_crane_slow_x))
            a4--;
        if (CX < XS8(cut1_door_open_x) && !flag_a) {
            anim(O_DOOR, X8(cut1_door_anim_open));
            unhide(O_DOOR);
            flag_a = 1;
        }
        cut1_heads();
        if (a4 < XS8(cut1_heads_end)) {
            anim(O_DOOR, X8(cut1_door_anim_close));
            step_done();
        }
        ox[O_CRANE1] = CX - X8(cut_crane1_back);
        ox[O_CRANE2] = X8(cut1_crane2_x) - CX;
        ox[O_CRANE3] = X16(cut1_crane3_x) - CX;
        return;
    }
    if (!f0) {
        if (finished(O_DOOR)) {
            hide(O_DOOR);
            f0++;
        }
    } else if (waited(X8(cut1_door_wait))) {
        show_range(O_CRANE0, O_CRANE3, 0);
        cut2_start();
    }
}

static void cut2_walk_in(void)
{
    if (flag_a) {
        if (waited(X8(cut2_pause))) {
            flag_a = 0;
            f0 = 0;
        }
    } else if (!f0) {
        scroll_x += X8(cut2_scroll_dx);
        if (scroll_x >= X8(cut2_scroll_max))
            scroll_x = X8(cut2_scroll_max);
        if (++RX >= XS8(cut2_walkin_x)) {
            anim(O_RALPH, X8(cut2_anim_stop));
            f0++;
        }
    } else if (waited(X8(cut2_wait_bubble)) && finished(O_RALPH)) {
        spawn(O_BUBBLE, X8(cut2_bubble_anim), RX, RY);
        anim(O_RALPH, X8(cut2_anim_stand));
        step_done();
    }
}

static void walk_right(uint8_t pan)
{
    int16_t old = RX;
    if (move_x(X8(cut2_walk_dx), X8(cut2_walk_right_x))) {
        adv();
    } else {
        if (pan) {
            scroll_x -= X8(cut_pan_back);
            if (scroll_x < 1)
                scroll_x = 0;
        }
        if (RX > 0 && old <= 0)
            anim(O_RALPH, X8(cut2_anim_walk2));
        if (RX <= X8(cut2_smash_min_x))
            return;
    }
    smash(X8(cut_smash_cut2), 0);
}

static void cut2_heads(void)
{
    uint8_t i;
    anim(O_RALPH, X8(cut2_anim_stand));
    for (i = 0; i < 4; i++) {
        spawn(O_HEAD0 + i, X8(cut2_heads_anim0) + i, pane_x(hit_rows[i], hit_slots[i]) + X8(cut2_head_dx),
              pane_y(hit_rows[i], 1) + X8(cut2_head_dy));
    }
    step_done();
}

static void cut2_rampage(void)
{
    if (waited(X8(cut2_rampage_wait))) {
        f0 = 0;
        if (count < X8(cut2_rampage_hits)) {
            window_damage(hit_rows[count], hit_slots[count], 1, 1);
            count++;
        } else {
            cut2_heads();
        }
    }
    smash(X8(cut_smash_cut2), SMASH_AVOID);
}

static void cut2_felix(void)
{
    if (!waited(X8(cut2_wait14)))
        return;
    music_play(SND_MUSIC_INTRO_END, 0);
    hide(O_BUBBLE);
    show_range(O_HEAD0, O_HEAD3, 0);
    scroll_x = X16(cut2_felix_scroll);
    FX = X16(cut2_felix_x);
    anim(O_FELIX, X8(cut2_felix_anim));
    A(O_FELIX)->flags |= AF_MIRROR | AF_VISIBLE;
    unhide(O_HAMMER);
    ground_show(0);
    bg_invalidate();
    adv();
}

static void cut2_tail(void)
{
    switch (cstep) {
    case 13:
        if (waited(X8(cut2_wait13))) {
            spawn(O_BUBBLE, X8(cut2_bubble89_anim), X8(cut2_bubble89_x), RY + X8(cut2_bubble89_dy));
            step_done();
        }
        break;
    case 14:
        cut2_felix();
        break;
    case 15:
        FX -= X8(cut2_felix_dx);
        if (FX < X16(cut_felix_stop_next)) {
            FX = X16(cut2_felix_stop);
            anim(O_FELIX, X8(cut2_felix_anim16));
            hide(O_HAMMER);
            step_done();
        }
        break;
    case 16:
        if (!text_on) {
            if (finished(O_FELIX)) {
                text_on = 1;
                if (!opt[OPT_VOICES])
                    sfx_play(SND_VOICE_FIX);
            }
        } else if (waited(X8(cut2_wait16))) {
            anim(O_FELIX, X8(cut2_felix_anim9));
            text_on = 0;
            adv();
        }
        break;
    default:
        FX -= X8(cut2_felix_dx);
        if (FX < X16(cut2_exit_x)) {
            hide(O_FELIX);
            hide(O_RALPH);
            done = 1;
        }
        break;
    }
}

static void cut2(void)
{
    switch (cstep) {
    case 0:
        cut2_walk_in();
        break;
    case 1:
        if (waited(X8(cut2_wait1))) {
            hide(O_BUBBLE);
            anim(O_RALPH, X8(cut2_anim_walk));
            adv();
        }
        break;
    case 2:
        walk_right(1);
        break;
    case 3:
        if (move_x(XS8(cut_walk_back), X8(cut2_left_x)))
            adv();
        smash(X8(cut_smash_cut2), 0);
        break;
    case 4:
        if (move_x(X8(cut2_walk_dx), X8(cut2_right_x))) {
            anim(O_RALPH, X8(cut2_anim_climb));
            RY = X16(cut2_climb_y0);
            step_done();
        }
        pan_follow();
        smash(X8(cut_smash_cut2), 0);
        break;
    case 5:
        if (RY == X8(cut2_climb_top)) {
            if (waited(X8(cut2_climb_wait_top))) {
                anim(O_RALPH, X8(cut2_anim_top));
                adv();
            }
        } else if (waited(X8(cut2_climb_wait))) {
            RY -= X8(cut2_climb_dy);
            f0 = 0;
            if (RY < X8(cut_climb_top_next))
                RY = X8(cut2_climb_top);
        }
        break;
    case 6:
        if (A(O_RALPH)->frame == 1) {
            window_damage(X8(cut2_hit_row), X8(cut2_hit_slot), 1, 1);
            spawn(O_HEAD0, X8(cut2_head_anim47), X8(cut2_head47_x), X8(cut2_head47_y));
            A(O_HEAD0)->frame = 1;
            adv();
        }
        break;
    case 7:
        if (finished(O_RALPH)) {
            anim(O_RALPH, X8(cut2_anim_smash));
            hide(O_HEAD0);
            adv();
        }
        break;
    case 8:
        if (A(O_RALPH)->frame == X8(cut2_impact_frame)) {
            sfx_play(SND_INTRO_IMPACT);
            spawn(O_HEAD0, X8(cut2_head88_anim), X8(cut2_head88_x), X8(cut2_head88_y));
            adv();
        }
        break;
    case 9:
        HX -= X8(cut2_head88_dx);
        if (HX < 0) {
            RY = ROW1_TOP;
            hide(O_HEAD0);
            anim(O_RALPH, X8(cut2_anim_walk2));
            adv();
        }
        break;
    case 10:
        if (move_x(XS8(cut_walk_back), X8(cut2_center_x))) {
            scroll_x = 0;
            adv();
        }
        pan_follow();
        smash(X8(cut_smash_cut2), 2);
        break;
    case 11:
        if (climb_to(ROW3_TOP)) {
            anim(O_RALPH, X8(cut2_anim_rampage));
            count = 0;
            step_done();
        }
        smash(X8(cut_smash_cut2), 2);
        break;
    case 12:
        cut2_rampage();
        break;
    default:
        cut2_tail();
        return;
    }
}

static void cut3(void)
{
    if (cstep == 0) {
        walk_right(0);
    } else if (cstep == 1) {
        if (move_x(XS8(cut_cut3_dx), X8(cut_cut3_center_x)))
            adv();
        smash(X8(cut_smash_cut3), 0);
    } else {
        if (climb_to(ROW3_TOP)) {
            anim(O_RALPH, X8(cut3_anim_done));
            done = 1;
            adv();
        }
        smash(X8(cut_smash_cut3), SMASH_ANY);
    }
}

static uint8_t intro_n = 4;

void cut_setup(void) BANKED
{
    mode = MODE_CUT;
    advance = done = 0;
    flag_a = flag_b = flag_c = 0;
    text_on = text_shown = 0;
    count = sub = 0;
    f0 = 0;
    cstep = 0;
    scroll_x = scroll_y = 0;
    a4 = X16(cut_a4_start);
    scen_y = X8(cut_scen_y);
    objs_reset();
    objs_init(intro_objs, OBJS, 0);
    ground_show(pl.needs_intro || next_level);
    if (ground_on) {
        level_build();
        player_actors_show(0);
        if (pl.needs_intro)
            cut2_start();
        else
            cut3_start();
        return;
    }
    if (opt[OPT_INTRO] == 2)
        intro_n = intro_n < 4 ? intro_n + 1 : 0;
    if (opt[OPT_INTRO] == 1 || (opt[OPT_INTRO] == 2 && intro_n)) {
        level_build();
        ground_show(1);
        cut2_start();
        return;
    }
    script = cut0;
    music_play(SND_MUSIC_STORY1, 0);
}

void cut_tick(void) BANKED
{
    if (advance || done || mode != MODE_CUT)
        return;
    script();
}

static uint8_t caption_colour(void)
{
    if (!text_on)
        return 0;
    return (ticks & X8(cut_caption_blink_mask)) < X8(cut_caption_blink_on) ? C_WHITE : C_YELLOW;
}

static void caption(void)
{
    text_shown = caption_colour();
    if (text_on)
        text_print(X8(cut_caption_col), X8(cut_caption_row), text_shown, exe_cut_caption);
    else
        print_blank(X8(cut_caption_col), X8(cut_caption_row), sizeof exe_cut_caption - 1);
}

static void commit(void)
{
    if (advance) {
        cstep++;
        advance = 0;
    }
}

void cut_render(void) BANKED
{
    if (script == cut0 && mode == MODE_CUT)
        bg_dy = X8(cut_scen_y) - scen_y;
    commit();
    if (caption_colour() != text_shown)
        caption();
    if (mode == MODE_CUT)
        objs_pics();
}

uint8_t cut_done(void) BANKED
{
    return done;
}

void cut_free(void) BANKED
{
    objs_reset();
    text_on = 0;
    if (mode == MODE_CUT && text_shown)
        caption();
    text_shown = 0;
    if (mode == MODE_CUT)
        ground_show(1);
    mode = MODE_NONE;
    scroll_x = 0;
    player_actors_show(1);
}

static void objs_draw(const uint8_t *order, uint8_t n)
{
    uint8_t i;
    for (i = 0; i < n; i++) {
        ri = order[i];
        obj_render();
    }
}

static void objs_pics(void)
{
    pics_pass = 1;
    for (ri = 0; ri < PICS; ri++)
        obj_render();
    pics_pass = 0;
}

void cut_draw(void) BANKED
{
    objs_draw(script == cut2 ? draw_order_cut2 : draw_order, OBJS);
}

static void objs_tick(void)
{
    uint8_t i;
    for (i = 0; i < OBJS; i++)
        actor_tick(&oa[i]);
}

void cut_anim_tick(void) BANKED
{
    if (mode)
        objs_tick();
}

static int16_t roof_y;
static uint8_t phase;
static uint8_t colour_step, colour_shown;

static void felix_anim(uint8_t a)
{
    actor_set(&pl.felix.act, a);
}

static void ralph_anim(uint8_t a)
{
    actor_set(&pl.ralph.act, a);
}

static void fix_set(fix_t *f, int16_t v)
{
    ((int16_t *)f)[0] = 0;
    ((int16_t *)f)[1] = v;
}

static void felix_at(int16_t x, int16_t y)
{
    fix_set(&pl.felix.x, x);
    fix_set(&pl.felix.y, y);
}

static void nicelanders_at(const uint8_t *dx)
{
    static uint8_t i;
    for (i = L_NICE0; i < L_NICE0 + 5; i++) {
        ox[i] = NX + *dx++;
        if (ox[i] > X8(cut_nice_vis_x))
            unhide(i);
    }
}

#define end_objs exe_end_objs
#define spread_out exe_cut_spread_out
#define carry exe_cut_carry

static void phase_set(uint8_t p)
{
    phase = p;
    music_play(SND_MUSIC_CLEAR1 + p, 0);
    f0 = 0;
    cstep = 0;
    fg_clear_rows(3, 38);
    colour_step = 0;
    colour_shown = 0xFF;
    if (p == 2) {
        text_print(X8(cut_phase2_col), X8(cut_phase2_level_row), C_WHITE, exe_cut_level);
        print_num(X8(cut_phase2_col) + sizeof exe_cut_level - 1, X8(cut_phase2_level_row), C_WHITE, pl.level + 1, pl.level < 9 ? 1 : pl.level < 99 ? 2 : 3);
        text_print(X8(cut_phase2_col), X8(cut_phase2_complete_row), C_WHITE, exe_cut_complete);
    }
    if (p != 1)
        return;
    scroll_x = 0;
    scroll_y = X8(cut_phase1_scroll_y) - roof_y;
    A(L_HATCH)->flags &= (uint8_t)~AF_FROZEN;
    felix_anim(X8(cut_phase1_felix_anim));
    pl.felix.act.flags &= (uint8_t)~AF_MIRROR;
    pl.ralph.x = X8(cut_phase1_ralph_x);
    pl.ralph.y = roof_y + X8(cut_phase1_ralph_dy);
    felix_at(X8(cut_phase1_felix_x), pl.ralph.y);
    ralph_anim(X8(cut_phase1_ralph_anim));
    pl.ralph.act.flags |= AF_VISIBLE;
}


static void end_open(void)
{
    switch (cstep) {
    case 0:
        if (finished(L_HATCH)) {
            felix_anim(X8(cut_open_felix_anim5));
            step_done();
        }
        break;
    case 1:
        if (!waited(X8(cut_open_step_wait)))
            break;
        NX += X8(cut_open_step_dx);
        f0 = 0;
        if (NX >= X8(cut_end_nice_x1)) {
            NX = X8(cut_end_nice_x1);
            felix_anim(X8(cut_open_felix_anim3));
            A(L_MEDAL)->flags = (A(L_MEDAL)->flags | AF_VISIBLE) & (uint8_t)~AF_FROZEN;
            adv();
        }
        nicelanders_at(spread_out);
        break;
    case 2:
        if (waited(X8(cut_cloud_wait))) {
            f0 = 0;
            ox[L_CLOUD0] -= X8(cut_cloud_dx);
            ox[L_CLOUD1] += X8(cut_cloud_dx);
        }
        if (finished(L_MEDAL)) {
            hide(L_MEDAL);
            felix_anim(X8(cut_open_felix_anim18));
            step_done();
        }
        break;
    case 3:
        if (f0 < X8(cut_open_wait3_end)) {
            if (waited(X8(cut_open_wait3)))
                anim(L_NICE0 + 1, X8(cut_open_nice1_anim62));
        } else if (finished(L_NICE0 + 1)) {
            felix_anim(X8(cut_open_felix_anim17));
            adv();
        }
        break;
    case 4:
        if (pl.felix.act.flags & AF_DONE) {
            felix_anim(X8(cut_open_felix_anim6));
            anim(L_NICE0 + 1, X8(cut_open_nice1_anim51));
            anim(L_NICE0, X8(cut_open_nice0_anim63));
            flag_a = 0;
            step_done();
        }
        break;
    default:
        if (!finished(L_NICE0)) {
            if (!flag_a && A(L_NICE0)->frame == X8(cut_open_medal_frame)) {
                felix_anim(X8(cut_open_felix_anim7));
                sfx_play(SND_MEDAL);
                flag_a = 1;
            }
        } else if (waited(X8(cut_open_wait5))) {
            uint8_t i;
            for (i = 0; i < 5; i++)
                anim(L_NICE0 + i, X8(cut_open_carry_anim0) + i);
            sub = 0;
            step_done();
        }
        break;
    }
}

static void end_carry(void)
{
    if ((cstep == 6 && !waited(X8(cut_carry_wait))) || ++sub <= X8(cut_carry_sub))
        return;
    sub = 0;
    if (steps++ > X8(cut_carry_steps)) {
        sfx_play(SND_NICELANDER_STEP);
        steps = 0;
    }
    if (cstep == 6) {
        if (NX < X8(cut_carry_lift_x)) {
            NX += X8(cut_carry_dx);
            if (NX > X8(cut_carry_lift_x))
                NX = X8(cut_carry_lift_x);
            return;
        }
        NX += X8(cut_carry_dx);
        if (NX >= X8(cut_carry_end_x)) {
            pl.ralph.x = X8(cut_carry_ralph_x);
            ralph_anim(X8(cut_carry_ralph_anim));
            adv();
        }
    } else {
        pl.ralph.x += X8(cut_carry_ralph_dx);
        scroll_x -= X8(cut_carry_scroll_back);
        NX += X8(cut_carry_dx);
        if (NX >= X8(cut_throw_x)) {
            throw_y = FIX(roof_y + X8(cut_phase1_ralph_dy));
            throw_vy = XL(cut_throw_vy);
            adv();
            sfx_play(SND_RALPH_THROWN);
        }
    }
    nicelanders_at(carry);
}

static void end_fall(void)
{
    if (cstep == 8) {
        pl.ralph.x += X8(cut_throw_vx);
        throw_y += throw_vy;
        throw_vy += FIX_0_1;
        pl.ralph.y = fix_trunc(throw_y);
        if (pl.ralph.x > X16(cut_throw_end_x)) {
            pl.ralph.y = X8(cut_fall_y0);
            scroll_y = 0;
            scroll_x = X16(cut_fall_scroll_x);
            ralph_anim(X8(cut_fall_anim));
            pl.ralph.x = X16(cut_throw_end_x);
            flag_b = 0;
            throw_y = FIX(X8(cut_fall_y0));
            step_done();
            sfx_play(SND_RALPH_FALL);
        }
    } else if (!flag_b) {
        throw_y += FALL_STEP;
        pl.ralph.y = fix_trunc(throw_y);
        if (throw_y >= FIX(GROUND_Y)) {
            pl.ralph.y = GROUND_Y;
            flag_b = 1;
            ralph_anim(X8(cut_land_anim));
            sfx_stop(SND_RALPH_FALL);
            sfx_play(SND_RALPH_LAND);
        }
    } else if (waited(X8(cut_land_wait))) {
        phase_set(2);
    }
}

void lvlend_init(void) BANKED
{
    objs_reset();
    mode = MODE_END;
    advance = done = 0;
    flag_a = flag_b = 0;
    steps = sub = 0;
    roof_y = row_top_y(pl.rows - 1);
    objs_init(end_objs, sizeof end_objs / sizeof end_objs[0], roof_y);
    felix_at(X16(cut_end_felix_x), X8(cut_end_felix_y));
    felix_anim(X8(cut_end_felix_anim));
    pl.felix.act.flags = AF_VISIBLE;
    ground_show(1);
    phase_set(0);
    scroll_x = X16(cut_end_scroll_x);
    scroll_y = X8(cut_end_scroll_y);
}

void lvlend_tick(void) BANKED
{
    if (mode != MODE_END || advance)
        return;
    if (phase == 0) {
        if (waited(X8(cut_phase0_wait)))
            phase_set(1);
        else if (!(ticks & X8(cut_fixed_colour_mask)) && ++colour_step > X8(cut_fixed_colour_max))
            colour_step = 0;
        return;
    }
    if (phase == 1) {
        if (cstep < 6)
            end_open();
        else if (cstep < 8)
            end_carry();
        else
            end_fall();
        return;
    }
    if (waited(X8(cut_phase2_wait))) {
        if ((pl.level == X8(cut_kill_level) && !play_opt[OPT_KILL]) || service_pressed)
            crash_pending = 1;
        else
            done = 1;
    }
}

void lvlend_render(void) BANKED
{
    commit();
    if (mode != MODE_END)
        return;
    objs_pics();
    if (phase == 0 && colour_step != colour_shown) {
        colour_shown = colour_step;
        if (colour_step < X8(cut_fixed_colour_max))
            text_print(X8(cut_fixed_col), X8(cut_fixed_row), X8(cut_fixed_colour_base) + colour_step, exe_cut_fixed);
        else
            print_blank(X8(cut_fixed_col), X8(cut_fixed_row), sizeof exe_cut_fixed - 1);
    }
}

uint8_t lvlend_done(void) BANKED
{
    return done;
}

void lvlend_free(void) BANKED
{
    if (mode == MODE_END) {
        fg_clear_rows(3, 38);
        objs_reset();
        mode = MODE_NONE;
    }
    scroll_x = 0;
}

void lvlend_draw(void) BANKED
{
    objs_draw(end_order, sizeof end_order);
}

void lvlend_anim_tick(void) BANKED
{
}
