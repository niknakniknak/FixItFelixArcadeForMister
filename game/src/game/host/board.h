/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_BOARD_H
#define FELIX_BOARD_H

#include <stdint.h>

extern volatile uint8_t fake_io[0x800];
extern volatile uint16_t fake_bg[2048];
extern volatile uint16_t fake_fg[2048];
extern volatile uint8_t fake_spr[1024];
extern volatile uint8_t fake_nv[0x400];
#define REG8(addr) (fake_io[(addr) - 0xF800])

extern uint8_t fake_rom[0xC000];
#define ROM_BASE ((uintptr_t)fake_rom)
#define ROM_SIZE 0xC000
#define RAM_BASE 0xC000
#define RAM_SIZE 0x1000

#define MAP_COLS 32
#define MAP_ROWS 64
#define BG_MAP fake_bg
#define FG_MAP fake_fg
#define MAP_INDEX(col, row) ((uint16_t)(row) * MAP_COLS + (col))
#define SCREEN_W 240
#define SCREEN_H 320
#define TEXT_COLS 30
#define TEXT_ROWS 40

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t code;
    uint8_t attr;
} sprite_t;

#define SPRITES ((volatile sprite_t *)fake_spr)
#define SPR_SIZE (fake_spr + 0x200)
#define SPRITE_COUNT 128
#define NVRAM fake_nv
#define NVRAM_SIZE 0x400
#define SPR_ATTR_Y8 0x04
#define SPR_ATTR_CODE_HI 0x03
#define SPR_PAL_SHIFT 3
#define SPR_HIDDEN_X 240
#define SPR_HIDDEN_Y 320

#define IN0 REG8(0xF800)
#define IN1 REG8(0xF801)
#define IN2 REG8(0xF802)
#define DSW1 REG8(0xF803)
#define DSW2 REG8(0xF804)
#define DSW3 REG8(0xF806)
#define PAUSE REG8(0xF807)
#define PAUSE_ON 0x01

#define IN_RIGHT 0x01
#define IN_LEFT 0x02
#define IN_DOWN 0x04
#define IN_UP 0x08
#define IN_JUMP 0x10
#define IN_FIX 0x20

#define SYS_COIN_A 0x01
#define SYS_COIN_B 0x02
#define SYS_START1 0x04
#define SYS_START2 0x08
#define SYS_SERVICE 0x10
#define SYS_TEST 0x20
#define SYS_INPUTS 0x3F
#define SYS_VBLANK 0x80

#define BG_SCROLL_Y_LO REG8(0xF810)
#define BG_SCROLL_Y_HI REG8(0xF811)
#define BG_SCROLL_X REG8(0xF812)
#define VIDEO_CTRL REG8(0xF813)
#define INT_CTRL REG8(0xF814)
#define COIN_COUNTER REG8(0xF815)
#define SPR_COMMIT REG8(0xF816)
#define FG_PAGE REG8(0xF817)
#define ROM_BANK REG8(0xF818)

#define VIDEO_COMMIT 0x10
#define SYS_COMMIT_PENDING 0x40
#define FG_SHOW 0x01
#define FG_CPU 0x02
#define FG_BOTH 0x04
#define SPR_W32 0x01
#define SPR_W64 0x02
#define SPR_H32 0x04
#define SPR_H64 0x08
#define SPR_X8 0x10
#define SPR_CODE10 0x20
#define SPR_BEHIND 0x40

#define VIDEO_FLIP 0x01
#define VIDEO_BG 0x02
#define VIDEO_FG 0x04
#define VIDEO_SPRITES 0x08

#define INT_ENABLE 0x01

#define VOICES 6
#define X1_REG(voice, reg) REG8(0xF900 + (voice) * 8 + (reg))
#define X1_CTRL(voice) X1_REG(voice, 0)
#define X1_VOL(voice) X1_REG(voice, 1)
#define X1_FREQ(voice) X1_REG(voice, 2)
#define X1_START(voice) X1_REG(voice, 4)
#define X1_END(voice) X1_REG(voice, 5)
#define X1_KEYON 0x01
#define X1_VOL_FULL 0xFF
#define SOUND_BANK REG8(0xF819)

#include "gfx.h"

void map_fill(volatile uint16_t *map, uint16_t entry);
void text_print(uint8_t col, uint8_t row, uint8_t colour, const char *s);
void spr_begin(uint8_t first, uint8_t count);
void spr_big(uint8_t rect, int16_t x, int16_t y);
extern uint8_t spr_big_flags;
void spr_rect(uint8_t rect, int16_t x, int16_t y, uint8_t felix, uint8_t hammer);
void spr_frame(const anim_frame_t *frame, int16_t x, int16_t y, uint8_t mirrored,
               uint8_t felix, uint8_t hammer);

#endif
