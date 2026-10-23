/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "player.h"
#include "bldg.h"
#include "bg.h"
#include "felix.h"
#include "ralph.h"
#include "actors.h"
#include "rng.h"
#include "sound.h"
#include "input.h"
#include "service.h"

#define RALPH_SMASHES X8(player_ralph_smashes)

static const uint8_t start_levels[4] = { 0, 2, 4, 8 };

player_t pl;
player_t saved[2];

void player_select(uint8_t n)
{
    if (n == cur)
        return;
    memcpy(&saved[cur], &pl, sizeof pl);
    memcpy(&pl, &saved[n], sizeof pl);
    cur = n;
}

uint8_t player_out_other(void)
{
    return saved[cur ^ 1].out;
}

static void ralph_create(void)
{
    pl.ralph.act.flags = AF_VISIBLE;
    actor_set(&pl.ralph.act, X8(player_ralph_anim));
    pl.ralph.chunk_timer = 0;
    pl.ralph.smashes = RALPH_SMASHES;
}

void player_free(void)
{
    building_on = 0;
    pl.rows = 0;
    pl.bird_count = 0;
    pie_hide();
    chunks_clear();
    obstacles_clear();
    bricks_clear();
}

void player_new(void)
{
    memset(&pl, 0, sizeof pl);
    pl.row_types = 0;
    pl.lives = exe_spare_lives[dsw1 & 3];
    pie_init();
    felix_create();
    ralph_create();
}

void game_start(uint8_t n)
{
    uint8_t i;
    credits = credits >= n ? credits - n : 0;
    sfx_play(SND_START);
    view_player = 0;
    players = n;
    demo = 0;
    after_game = 0;
    play_settings();
    for (i = 0; i < n; i++) {
        player_select(i);
        player_free();
        player_new();
        pl.level = start_levels[play_opt[OPT_START]];
        pl.needs_intro = 1;
    }
    nv_count(BK_PLAY1 - 1 + n);
    state_request(n > 1 ? ST_SWITCH : ST_CUTSCENE);
    if (n > 1)
        return;
    player_select(0);
}

void demo_setup(void)
{
    player_select(0);
    demo = 1;
    after_game = 0;
    play_settings();
    players = 1;
    player_free();
    player_new();
    pl.lives = X8(player_demo_lives);
    view_player = 0;
    state_request(ST_CUTSCENE);
}

void level_advance(void)
{
    pl.level++;
    scroll_x = 0;
    next_level = 1;
    scroll_y = 0;
    pl.scroll = 0;
    pl.floor_done = 0;
    pl.broken = 0;
    pl.scroll_target = 0;
    pl.pie.timer = pie_delay();
    pl.pie_flag = 0;
    felix_level_start();
    ralph_level_done();
    state_request(ST_CUTSCENE);
}

void level_build(void)
{
    rng_seed(demo ? DEMO_SEED : rng_entropy() + ticks);
    player_free();
    bldg_create();
    building_on = 1;
    rows_shown = 0;
    bg_invalidate();
}
