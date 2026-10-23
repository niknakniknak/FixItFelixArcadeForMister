/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_INPUT_H
#define FELIX_INPUT_H

#include <stdint.h>
#include "bank.h"

#define K_UP 0x01
#define K_DOWN 0x02
#define K_LEFT 0x04
#define K_RIGHT 0x08
#define K_JUMP 0x20
#define K_FIX 0x40
#define K_DIRS 0x0F

#define S_COIN 0x01
#define S_START1 0x02
#define S_START2 0x04
#define S_SERVICE 0x08

extern uint8_t key_held;
extern uint8_t key_new;
extern uint8_t key_latch;
extern uint8_t sys_new;
extern uint8_t credits;
extern uint8_t dsw1, dsw2, dsw3;

void input_init(void) BANKED;
void input_read(uint8_t accept_coins) BANKED;
void demo_input_start(void) BANKED;
void demo_input_stop(void) BANKED;
uint8_t demo_input_over(void) BANKED;
uint8_t free_play(void) BANKED;
uint8_t invulnerable(void) BANKED;
void play_settings(void) BANKED;

#endif
