#!/bin/sh
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
set -e
GAME=$(cd "$(dirname "$0")/.." && pwd)
WORK=$GAME/build/hosttest
rm -rf "$WORK"
mkdir -p "$WORK/host"
cp "$GAME"/src/game/*.c "$GAME"/src/game/*.h "$WORK"/
cp "$GAME"/src/game/host/* "$WORK"/host/
cp "$WORK"/host/board.h "$WORK"/board.h
rm -f "$WORK"/main.c
gcc -std=c11 -O1 -g -Wall -Wno-unused-function -fsanitize=address,undefined -I"$WORK" -I"$GAME"/build/gen \
    -o "$WORK"/hosttest "$WORK"/*.c "$WORK"/host/*.c "$GAME"/build/gen/gfx_game.c "$GAME"/build/gen/demo_log.c "$GAME"/build/gen/exe_*.c
"$WORK"/hosttest
