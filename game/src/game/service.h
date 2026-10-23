/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_SERVICE_H
#define FELIX_SERVICE_H

#include <stdint.h>
#include "bank.h"

enum {
    OPT_HAMMER, OPT_START, OPT_SPEED, OPT_SUPER, OPT_HAZARDS, OPT_KILL, OPT_CONTINUE,
    OPT_BOOT, OPT_INTRO, OPT_VOICES, OPT_SAVE_HI, OPTS
};

enum { BK_COIN_A, BK_COIN_B, BK_SERVICE, BK_PLAY1, BK_PLAY2, BK_CONTINUE, BK_SECONDS, BK_COUNTERS };

#define OPT_PLAY OPT_BOOT

extern uint8_t opt[OPTS];
extern uint8_t play_opt[OPT_PLAY];

void svc_boot(void) BANKED;
void svc_enter(void) BANKED;
void svc_frame(void) BANKED;
void nv_count(uint8_t counter) BANKED;
void nv_hiscore_load(void) BANKED;
void nv_hiscore_save(void) BANKED;
void nv_reload(void) BANKED;
void nv_tick(void) BANKED;

#endif
