/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include "../game.h"
#include "../states.h"
#include "../rng.h"
#include "../bldg.h"
#include "../hiscore.h"
#include "../input.h"
#include "../player.h"
#include "../felix.h"
#include "../service.h"

uint8_t host_column_load(uint8_t *worst_x);
void vblank_handler(void);

static int failures;
static uint8_t worst_load;
static uint8_t worst_load_state;

#define CHECK(cond, ...)                         \
    do {                                         \
        if (!(cond)) {                           \
            failures++;                          \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                 \
            printf("\n");                        \
        }                                        \
    } while (0)

static void frame(void)
{
    uint8_t x, n;
    game_frame();
    vblank_handler();
    n = host_column_load(&x);
    if (n > worst_load) {
        worst_load = n;
        worst_load_state = game_state;
    }
}

static void run(unsigned n)
{
    while (n--)
        frame();
}

static unsigned run_until_state(uint8_t s, unsigned limit)
{
    unsigned n = 0;
    while (game_state != s && n < limit) {
        frame();
        n++;
    }
    return n;
}

static void press_sys(uint8_t bit)
{
    fake_io[2] |= bit;
    run(2);
    fake_io[2] &= (uint8_t)~bit;
    run(2);
}

static void test_rng(void)
{
    rng_seed(1);
    CHECK(rng() == 41, "msvc rand 1");
    CHECK(rng() == 18467, "msvc rand 2");
    CHECK(rng() == 6334, "msvc rand 3");
    rng_seed(DEMO_SEED);
    printf("demo seed first rands: %u %u %u %u\n", rng(), rng(), rng(), rng());
}

static void test_levels(void)
{
    uint8_t l;
    for (l = 0; l < 12; l++) {
        const level_t *e = &exe_levels[l < 9 ? l : 9];
        pl.level = l;
        bldg_create();
        CHECK(pl.rows == e->rows, "level %u rows %u", l, pl.rows);
        CHECK(pl.floors_left == (e->rows - 1) / 4, "level %u floors", l);
        CHECK(pl.bird_pairs == e->birds && pl.obstacles == e->obstacles && pl.chunk_max == e->chunks,
              "level %u table", l);
    }
    pl.level = 0;
    bldg_create();
    CHECK(win_sill(0, 0) == row_top_y(0) + EXEV_bldg_win_sill && win_sill(0, 2) == EXEV_bldg_door_sill &&
          win_sill(1, 2) == EXEV_bldg_arch_sill, "sills");
    CHECK(win_left(0, 0) == EXEV_felix_left_limit && win_right(0, 4) == EXEV_felix_right_limit, "walk limits");
    CHECK(row_top_y(8) == (40 - 4 * EXEV_bldg_row_height_other - 4 * EXEV_bldg_row_height_upper_a -
                           EXEV_bldg_roof_height) * 8, "roof top");
}

static void test_scoring(void)
{
    uint8_t i;
    player_new();
    pl.level = 0;
    bldg_create();
    dsw1 = 0;
    demo = 0;
    pl.lives = 2;
    pl.score = exe_bonus_step[0] - EXEV_bldg_points_window - EXEV_bldg_points_last;
    pl.broken = 2;
    pl.win[0][0].st = 1;
    pl.win[0][1].st = 1;
    window_fix(0, 0);
    for (i = 0; i < EXEV_bldg_repair_frames; i++)
        windows_update();
    CHECK(pl.score == exe_bonus_step[0] - EXEV_bldg_points_last, "window points (%lu)", (unsigned long)pl.score);
    window_fix(0, 1);
    for (i = 0; i < EXEV_bldg_repair_frames; i++)
        windows_update();
    CHECK(pl.score == exe_bonus_step[0], "last window points (%lu)", (unsigned long)pl.score);
    CHECK(pl.lives == 3, "extra life at the first bonus step (%u)", pl.lives);
    dsw1 = 0x0C;
    pl.score = 2 * exe_bonus_step[0] - EXEV_bldg_points_window;
    pl.broken = 2;
    pl.win[0][3].st = 1;
    window_fix(0, 3);
    for (i = 0; i < EXEV_bldg_repair_frames; i++)
        windows_update();
    CHECK(pl.lives == 3, "no extra life with bonus DIP off");
    dsw1 = 0;
}

static void test_hiscore(void)
{
    hiscore_defaults();
    pl.score = exe_default_scores[2] + 1;
    CHECK(hiscore_insert() && entry_pos == 2, "insert at rank 3");
    CHECK(hiscores[3].score == exe_default_scores[2] && hiscores[4].score == exe_default_scores[3], "table shift");
    hiscore_defaults();
}

static void test_game_start(void)
{
    unsigned n;
    game_init();
    run(1);
    n = run_until_state(ST_TITLE, 1000);
    CHECK(game_state == ST_TITLE, "boot reaches title (%u frames)", n);
    printf("boot took %u frames\n", n);
    press_sys(SYS_COIN_A);
    CHECK(credits == 1, "coin gives a credit (%u)", credits);
    press_sys(SYS_START1);
    CHECK(credits == 0, "start uses a credit");
    n = run_until_state(ST_PLAY, 5000);
    CHECK(game_state == ST_PLAY, "intro reaches play (%u frames)", n);
    printf("new-game intro took %u frames\n", n);
    CHECK(pl.lives == exe_spare_lives[0] && pl.level == 0 && pl.rows == exe_levels[0].rows &&
          pl.floors_left == (exe_levels[0].rows - 1) / 4, "new game state");
    printf("broken windows after intro: %u\n", pl.broken);
    run(400);
    printf("felix state %u at (%d,%d) row %u slot %u\n", pl.felix.state, felix_x(), felix_y(),
           pl.felix.row, pl.felix.slot);
    CHECK(pl.felix.row == 0 && pl.felix.slot == 4, "felix walked in to the bottom-right window");
}

static void test_demo(void)
{
    unsigned n, play = 0;
    game_init();
    run(1);
    run_until_state(ST_TITLE, 1000);
    n = run_until_state(ST_PLAY, 20000);
    CHECK(game_state == ST_PLAY && demo, "attract reaches demo play (%u frames)", n);
    printf("attract cutscenes took %u frames, broken %u\n", n, pl.broken);
    {
        uint8_t r, w;
        for (r = 0; r < 4; r++)
            for (w = 0; w < SLOTS; w++)
                if (pl.win[r][w].st & 3)
                    printf("  broken r%u s%u st %u dmg %02x\n", r, w, pl.win[r][w].st & 3, pl.win[r][w].dmg);
    }
    while ((game_state == ST_PLAY || game_state == ST_SCROLL) && play < 4000) {
        uint8_t st = pl.felix.state, b = pl.broken;
        frame();
        play++;
        if (getenv("TRACE") && (st != pl.felix.state || b != pl.broken))
            printf("t%4u keys %02x felix %u row %u slot %u x %d y %d broken %u ralph %u x %d\n", play,
                   key_held, pl.felix.state, pl.felix.row, pl.felix.slot, felix_x(), felix_y(), pl.broken,
                   pl.ralph.state, pl.ralph.x);
    }
    {
        uint8_t r, w;
        for (r = 0; r < 4; r++)
            for (w = 0; w < SLOTS; w++)
                if (pl.win[r][w].st & 3)
                    printf("  still broken r%u s%u\n", r, w);
    }
    printf("demo ended after %u frames in state %u (10 = cleared floor, 2 = log ran out), broken %u\n",
           play, game_state, pl.broken);
}

static uint8_t cheat_floor(void)
{
    uint8_t r, s, fr = NO_ROW, fs = 0;
    for (r = pl.floor_row; r < pl.floor_row + FLOOR_ROWS; r++)
        for (s = 0; s < SLOTS; s++) {
            if (!(pl.win[r][s].st & 3))
                continue;
            if (fr == NO_ROW) {
                fr = r;
                fs = s;
                continue;
            }
            pl.win[r][s].st &= WIN_OBSTACLE;
            pl.win[r][s].dmg = slot_kind(r, s) == SLOT_DOOR ? 2 : slot_kind(r, s) == SLOT_ARCH ? 6 : 0;
        }
    if (fr == NO_ROW)
        return 0;
    pl.broken = 1;
    pl.felix.row = fr;
    pl.felix.slot = fs;
    pl.felix.x = FIX(win_center(fr, fs));
    pl.felix.y = FIX(win_sill(fr, fs));
    felix_set_state(FELIX_IDLE);
    pl.felix.super_on = 1;
    pl.felix.super_timer = 60000;
    return 1;
}

static void test_progress(uint8_t levels)
{
    unsigned guard = 0, floors = 0;
    uint8_t start_lives;
    game_init();
    run(1);
    run_until_state(ST_TITLE, 1000);
    press_sys(SYS_COIN_A);
    press_sys(SYS_START1);
    start_lives = 0;
    while (pl.level < levels && guard < 400000 && game_state != ST_BOOT) {
        if (game_state == ST_PLAY && pl.felix.state == FELIX_IDLE && !level_end_pending &&
            pl.felix.row != NO_ROW && cheat_floor()) {
            fake_io[0] = IN_FIX;
            run(3);
            fake_io[0] = 0;
            floors++;
            if (!start_lives)
                start_lives = pl.lives + 1;
        }
        frame();
        guard++;
    }
    printf("progress: reached level %u after %u frames, %u floors cleared, state %u\n", pl.level + 1, guard,
           floors, game_state);
    CHECK(pl.level >= levels || game_state == ST_BOOT, "progressed through levels");
}

static void test_kill_screen(void)
{
    unsigned guard = 0;
    test_progress(EXEV_cut_kill_level);
    while (game_state != ST_BOOT && guard < 200000) {
        if (game_state == ST_PLAY && pl.felix.state == FELIX_IDLE && !level_end_pending &&
            pl.felix.row != NO_ROW && cheat_floor()) {
            fake_io[0] = IN_FIX;
            run(3);
            fake_io[0] = 0;
        }
        frame();
        guard++;
    }
    CHECK(game_state == ST_BOOT, "kill screen after the last level returns to boot (state %u level %u)", game_state,
          pl.level + 1);
}

static void test_monkey(uint8_t level, unsigned frames, unsigned seed)
{
    static const uint8_t moves[8] = { 0, IN_LEFT, IN_RIGHT, IN_UP, IN_DOWN, IN_JUMP, IN_FIX, IN_FIX };
    unsigned f, visits[11] = { 0 }, levels = 0;
    uint8_t last_level = level;
    srand(seed);
    game_init();
    run(1);
    run_until_state(ST_TITLE, 1000);
    press_sys(SYS_COIN_A);
    fake_io[2] |= SYS_START1;
    frame();
    fake_io[2] &= (uint8_t)~SYS_START1;
    pl.level = level;
    for (f = 0; f < frames; f++) {
        if (!(f & 7))
            fake_io[0] = moves[rand() & 7];
        if (game_state == ST_TITLE || game_state == ST_GAME_OVER) {
            press_sys(SYS_COIN_A);
            fake_io[2] |= SYS_START1;
        } else {
            fake_io[2] &= (uint8_t)~SYS_START1;
        }
        frame();
        visits[game_state]++;
        if (pl.level != last_level) {
            last_level = pl.level;
            levels++;
        }
        CHECK(pl.broken <= FLOOR_WINDOWS, "broken counter sane (%u)", pl.broken);
        CHECK(pl.lives < 30, "lives sane");
    }
    fake_io[0] = 0;
    printf("monkey level %u: states", level);
    for (f = 0; f < 11; f++)
        printf(" %u:%u", f, visits[f]);
    printf(" level changes %u\n", levels);
}

static void test_two_players(void)
{
    static const uint8_t moves[4] = { IN_LEFT, IN_RIGHT, IN_UP, IN_FIX };
    unsigned f, switches = 0, entries = 0;
    uint8_t last = 0xFF, seen_p2 = 0;
    srand(7);
    game_init();
    run(1);
    run_until_state(ST_TITLE, 1000);
    press_sys(SYS_COIN_A);
    press_sys(SYS_COIN_A);
    press_sys(SYS_START2);
    CHECK(players == 2 && credits == 0, "2P start uses two credits");
    for (f = 0; f < 60000 && game_state != ST_TITLE; f++) {
        if (!(f & 15))
            fake_io[0] = fake_io[1] = moves[rand() & 3];
        frame();
        if (game_state != last) {
            if (game_state == ST_SWITCH)
                switches++;
            if (game_state == ST_ENTRY)
                entries++;
            last = game_state;
        }
        if (cur == 1)
            seen_p2 = 1;
    }
    fake_io[0] = fake_io[1] = 0;
    printf("2P game: %u turn switches, %u initials entries, ended in state %u after %u frames\n", switches,
           entries, game_state, f);
    CHECK(switches >= 2 && seen_p2, "players alternate");
}

static void press_key(uint8_t bit)
{
    fake_io[0] |= bit;
    run(2);
    fake_io[0] &= (uint8_t)~bit;
    run(2);
}

static void test_service(void)
{
    uint8_t i, nv_ok, sum = 0;
    game_init();
    run(1);
    run_until_state(ST_TITLE, 1000);
    press_sys(SYS_TEST);
    CHECK(game_state == ST_SERVICE, "TEST in attract enters service mode (state %u)", game_state);
    for (i = 0; i < 6; i++)
        press_key(IN_DOWN);
    press_key(IN_JUMP);
    press_key(IN_RIGHT);
    press_key(IN_DOWN);
    press_key(IN_RIGHT);
    press_key(IN_RIGHT);
    CHECK(opt[OPT_HAMMER] == 1 && opt[OPT_START] == 2, "settings changed (%u %u)", opt[OPT_HAMMER],
          opt[OPT_START]);
    for (i = 0; i < 16; i++)
        sum += fake_nv[i];
    nv_ok = fake_nv[0] == 1 && sum == 0 && fake_nv[1] == 1 && fake_nv[2] == 2;
    CHECK(nv_ok, "settings stored in NVRAM with version and checksum");
    press_key(IN_FIX);
    press_sys(SYS_TEST);
    CHECK(game_state != ST_SERVICE, "TEST leaves service mode");
    memset(opt, 0xEE, sizeof opt);
    game_init();
    run(1);
    CHECK(opt[OPT_HAMMER] == 1 && opt[OPT_START] == 2, "settings reload from NVRAM at power-on");
    run_until_state(ST_TITLE, 1000);
    press_sys(SYS_COIN_A);
    press_sys(SYS_START1);
    run_until_state(ST_PLAY, 5000);
    CHECK(pl.level == 4, "starting level 5 (level index %u)", pl.level);
    run(400);
    if (cheat_floor()) {
        uint8_t r = pl.felix.row, sl = pl.felix.slot, fr = pl.floor_row;
        fake_io[0] = IN_FIX;
        run(3);
        fake_io[0] = 0;
        run(60);
        CHECK(win_state(r, sl), "one hit leaves the window broken with hammer hits 2");
        fake_io[0] = IN_FIX;
        run(3);
        fake_io[0] = 0;
        run(60);
        CHECK(!win_state(r, sl) || pl.floor_row != fr, "second hit fixes it");
    } else {
        CHECK(0, "no broken window to test the hammer");
    }
    memset((void *)fake_nv, 0xFF, sizeof fake_nv);
    memset(opt, 0, sizeof opt);
    printf("service mode: settings, NVRAM reload, starting level and hammer hits checked\n");
}

static void one_window(uint8_t r, uint8_t s, uint8_t dmg)
{
    uint8_t rr, ss;
    for (rr = pl.floor_row; rr < pl.floor_row + FLOOR_ROWS; rr++)
        for (ss = 0; ss < SLOTS; ss++) {
            pl.win[rr][ss].st &= WIN_OBSTACLE;
            pl.win[rr][ss].dmg = slot_kind(rr, ss) == SLOT_DOOR ? 2 : slot_kind(rr, ss) == SLOT_ARCH ? 6 : 0;
        }
    pl.win[r][s].dmg = dmg;
    pl.win[r][s].st = (pl.win[r][s].st & WIN_OBSTACLE) | ((dmg & 7) && (dmg >> 3) ? 2 : 1);
    statics_dirty = 1;
    pl.broken = 2;
    pl.felix.row = r;
    pl.felix.slot = s;
    pl.felix.x = FIX(win_center(r, s));
    pl.felix.y = FIX(win_sill(r, s));
    felix_set_state(FELIX_IDLE);
    pl.felix.super_on = 1;
    pl.felix.super_timer = 60000;
}

static void hammer_hit(void)
{
    fake_io[0] = IN_FIX;
    run(3);
    fake_io[0] = 0;
    run(60);
}

static void test_by_damage(void)
{
    uint8_t r, s = 0;
    game_init();
    run(1);
    run_until_state(ST_TITLE, 1000);
    opt[OPT_HAMMER] = 3;
    press_sys(SYS_COIN_A);
    press_sys(SYS_START1);
    run_until_state(ST_PLAY, 5000);
    run(400);
    r = pl.floor_row + 1;
    one_window(r, s, 2);
    hammer_hit();
    CHECK(!win_state(r, s), "BY DAMAGE: a light window is fixed in one hit");
    one_window(r, s, 5 | (5 << 3));
    hammer_hit();
    CHECK(win_state(r, s) && pl.win[r][s].dmg == (1 | (1 << 3)), "BY DAMAGE: heavy panes step to medium (dmg %02x)",
          pl.win[r][s].dmg);
    hammer_hit();
    CHECK(win_state(r, s) && pl.win[r][s].dmg == (3 | (3 << 3)), "BY DAMAGE: medium panes step to light (dmg %02x)",
          pl.win[r][s].dmg);
    hammer_hit();
    CHECK(!win_state(r, s), "BY DAMAGE: a heavy window is fixed on the third hit");
    one_window(r, s, 4 | (2 << 3));
    hammer_hit();
    CHECK(win_state(r, s) && pl.win[r][s].dmg == 3, "BY DAMAGE: worst pane sets the hits (dmg %02x)",
          pl.win[r][s].dmg);
    hammer_hit();
    CHECK(!win_state(r, s), "BY DAMAGE: medium window fixed on the second hit");
    opt[OPT_HAMMER] = 0;
    printf("hammer hits by damage: light 1 hit, medium 2, heavy 3, panes step down on each hit\n");
}

int main(void)
{
    test_rng();
    test_levels();
    test_scoring();
    test_hiscore();
    test_game_start();
    test_demo();
    test_two_players();
    test_progress(5);
    test_kill_screen();
    test_monkey(0, 30000, 1);
    test_monkey(4, 30000, 2);
    test_monkey(8, 30000, 3);
    test_service();
    test_by_damage();
    printf("worst 16-px cell fetches per column: %u (state %u)\n", worst_load, worst_load_state);
    printf(failures ? "%d FAILURES\n" : "all host tests passed\n", failures);
    return failures != 0;
}
