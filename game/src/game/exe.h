/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_EXE_H
#define FELIX_EXE_H

#include <stdint.h>
#include "exe_scalars.h"
#include "exe_tables.h"

#if defined(__SDCC) && !defined(EXE_LITERAL)
#define X8(n) ((uint8_t)(uint16_t)exe_##n)
#define XS8(n) ((int8_t)(uint16_t)exe_##n)
#define X16(n) ((int16_t)(uint16_t)exe_##n)
#define XU16(n) ((uint16_t)exe_##n)
#define XL(n) ((int32_t)((uint32_t)(uint16_t)exe_##n##_hi << 16 | (uint16_t)exe_##n##_lo))
#else
#define X8(n) EXEV_##n
#define XS8(n) EXEV_##n
#define X16(n) EXEV_##n
#define XU16(n) EXEV_##n
#define XL(n) EXEV_##n
#endif

#endif
