/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_HISCORE_H
#define FELIX_HISCORE_H

#include <stdint.h>
#include "bank.h"

#define HISCORES 5

typedef struct {
    char name[4];
    uint32_t score;
} hiscore_t;

extern hiscore_t hiscores[HISCORES];
extern uint8_t entry_pos;

void hiscore_defaults(void) BANKED;
uint8_t hiscore_insert(void) BANKED;

#endif
