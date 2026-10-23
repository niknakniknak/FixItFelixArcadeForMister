#!/bin/sh
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
set -e
BIN=$1
MAP=$2
OUT=$3
SIM=$(dirname "$0")/../build/gamesim
SMP=$(dirname "$0")/../build/felix_samples.bin
PL=$(awk '$2 == "_pl" { print $1 }' "$MAP")
LIVES=$(printf "%x" $((0x$PL + 4)))
LEVEL=$(printf "%x" $((0x$PL + 5)))
mkdir -p "$OUT"
keep_alive() {
    f=$1
    while [ $f -lt $2 ]; do printf ",%d:%s=9" $f $LIVES; f=$((f + 200)); done
}
COIN1="700:2=01,705:2=00,730:2=04,735:2=00"
COIN2="700:2=01,705:2=00,710:2=01,715:2=00,730:2=08,735:2=00"
run() {
    name=$1; frames=$2; inputs=$3
    STATS="$OUT/$name.frames.txt" "$SIM" "$BIN" "$SMP" "$MAP" $frames "$OUT/$name" "" 00 00 "$inputs" > "$OUT/$name.log"
    echo "== $name"
    sed -n '/^ran /,$p' "$OUT/$name.log" | sed -n '1,/cycles per call/p' | grep -v "cycles per call"
}
[ -n "$SKIP_ATTRACT" ] || run attract 6000 ""
run game1p 5000 "$COIN1$(keep_alive 740 5000)"
run late1p 5000 "$COIN1,740:$LEVEL=6$(keep_alive 740 5000)"
run game2p 5000 "$COIN2$(keep_alive 740 5000)"
