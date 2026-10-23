#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Turn a banked SDCC link (Intel HEX with extended addresses) into the Tobikomi program ROM image."""
import sys
from pathlib import Path

ROM_SIZE = 0x20000
HOME_END = 0xC000
FIXED_END = 0x8000
WINDOW = 0x8000
BANK = 0x4000
CHIP = 0x8000
RESERVED = {c + CHIP - 1 for c in range(0, ROM_SIZE, CHIP)}


def rom_offset(v):
    if v < 0x10000:
        if v >= FIXED_END:
            raise ValueError(f"byte at {v:#x} is above the fixed area")
        return v
    bank, a = v >> 16, v & 0xFFFF
    if bank < 2 or not WINDOW <= a < WINDOW + BANK:
        raise ValueError(f"byte at {v:#x} is outside banks 2-7")
    return bank * BANK + a - WINDOW


def main():
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    rom = bytearray(b"\xff" * ROM_SIZE)
    used = bytearray(ROM_SIZE)
    upper = 0
    top = {}
    for line in src.read_text().split():
        n, a, t = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
        data = bytes.fromhex(line[9:9 + 2 * n])
        if t == 4:
            upper = int.from_bytes(data, "big") << 16
        elif t == 0:
            for i, b in enumerate(data):
                o = rom_offset(upper + a + i)
                if used[o] or o in RESERVED:
                    raise ValueError(f"ROM byte {o:#x} written twice or reserved")
                used[o] = 1
                rom[o] = b
                k = 1 if o < FIXED_END else o // BANK
                top[k] = max(top.get(k, 0), o)
    for end in sorted(RESERVED):
        rom[end] = (-sum(rom[end + 1 - CHIP:end])) & 0xFF
    dst.write_bytes(rom)
    fixed = top.get(1, 0) + 1
    print(f"{dst.name}: fixed 0000-{fixed - 1:04X} ({FIXED_END - fixed} bytes free)", end="")
    for k in sorted(x for x in top if x > 1):
        free = (k + 1) * BANK - 1 - top[k]
        print(f", bank {k} {free} bytes free", end="")
    print()


if __name__ == "__main__":
    sys.exit(main())
