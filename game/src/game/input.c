/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "input.h"
#include "sound.h"
#include "demo_log.h"
#include "service.h"
#include "exe.h"

#define MAX_CREDITS 99
#define COIN_FREE 9
#define DSW1_ATTRACT 0xC0

static const uint8_t coins_needed[9] = { 1, 2, 3, 4, 1, 1, 1, 1, 1 };
static const uint8_t credits_given[9] = { 1, 1, 1, 1, 2, 3, 4, 5, 6 };
static const uint8_t board_to_key[16] = {
    0, K_RIGHT, K_LEFT, K_LEFT | K_RIGHT,
    K_DOWN, K_DOWN | K_RIGHT, K_DOWN | K_LEFT, K_DOWN | K_LEFT | K_RIGHT,
    K_UP, K_UP | K_RIGHT, K_UP | K_LEFT, K_UP | K_LEFT | K_RIGHT,
    K_UP | K_DOWN, K_UP | K_DOWN | K_RIGHT, K_UP | K_DOWN | K_LEFT, K_DIRS,
};

uint8_t key_held;
uint8_t key_new;
uint8_t key_latch;
uint8_t sys_new;
uint8_t credits;

uint8_t dsw1, dsw2, dsw3;
uint8_t play_opt[OPT_PLAY];

static uint8_t key_prev;
static uint8_t sys_prev;
static uint8_t coin_count[2];
static uint8_t coin_pulse;
static uint8_t demo_on;
static uint16_t demo_frame;
static uint8_t demo_run;
static uint8_t demo_left;

void input_init(void) BANKED
{
    key_prev = key_held = key_new = key_latch = 0;
    sys_prev = IN2;
    credits = 0;
}

uint8_t free_play(void) BANKED
{
    return (dsw2 & 0x0F) >= COIN_FREE;
}

void play_settings(void) BANKED
{
    uint8_t i;
    dsw1 = demo ? DSW1 & DSW1_ATTRACT : DSW1;
    for (i = 0; i < OPT_PLAY; i++)
        play_opt[i] = demo ? 0 : opt[i];
}

uint8_t invulnerable(void) BANKED
{
    return !demo_on && (dsw3 & 1);
}

static uint8_t board_keys(uint8_t port)
{
    uint8_t k = board_to_key[port & 0x0F];
    if (port & IN_JUMP)
        k |= K_JUMP;
    if (port & IN_FIX)
        k |= K_FIX;
    return k;
}

static uint8_t demo_keys(void)
{
    uint8_t k;
    if (demo_frame == 0 || demo_frame > DEMO_FRAMES)
        return 0;
    if (!demo_left) {
        demo_left = demo_log[demo_run * 2 + 1];
        demo_run++;
    }
    k = demo_log[(demo_run - 1) * 2];
    demo_left--;
    return k;
}

void demo_input_start(void) BANKED
{
    demo_on = 1;
    demo_frame = 0;
    demo_run = 0;
    demo_left = 0;
}

void demo_input_stop(void) BANKED
{
    demo_on = 0;
}

uint8_t demo_input_over(void) BANKED
{
    return demo_on && demo_frame > DEMO_FRAMES;
}

static void add_coin(uint8_t slot, uint8_t setting)
{
    if (setting > 8)
        setting = 0;
    coin_pulse |= 1 << slot;
    nv_count(BK_COIN_A + slot);
    if (++coin_count[slot] < coins_needed[setting])
        return;
    coin_count[slot] = 0;
    credits += credits_given[setting];
    if (credits > MAX_CREDITS)
        credits = MAX_CREDITS;
}

static void coins(uint8_t sys, uint8_t accept)
{
    COIN_COUNTER = coin_pulse;
    coin_pulse = 0;
    if (!accept || !(sys & (SYS_COIN_A | SYS_COIN_B | SYS_SERVICE)))
        return;
    if (sys & SYS_COIN_A)
        add_coin(0, dsw2 & 0x0F);
    if (sys & SYS_COIN_B)
        add_coin(1, dsw2 >> 4);
    if ((sys & SYS_SERVICE) && credits < MAX_CREDITS) {
        credits++;
        nv_count(BK_SERVICE);
    }
    sys_new |= S_COIN;
    sfx_play(SND_COIN);
}

void input_read(uint8_t accept_coins) BANKED
{
    uint8_t sys_raw = IN2;
    uint8_t sys_edge = sys_raw & (uint8_t)~sys_prev;
    uint8_t keys;
    sys_prev = sys_raw;
    play_settings();
    dsw2 = DSW2;
    dsw3 = DSW3;
    sys_new = 0;
    if (sys_edge & SYS_START1)
        sys_new |= S_START1;
    if (sys_edge & SYS_START2)
        sys_new |= S_START2;
    if (sys_edge & SYS_TEST)
        sys_new |= S_SERVICE;
    coins(sys_edge, accept_coins);
    if (demo_on) {
        demo_frame++;
        keys = demo_keys();
    } else {
        keys = board_keys(cur && players > 1 ? IN1 : IN0);
    }
    key_held = keys;
    key_new = keys & (uint8_t)~key_prev;
    key_latch |= key_new;
    key_prev = keys;
    if (free_play())
        credits = X8(input_free_credits);
}
