#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render a boardsim/gamesim memory dump as the Tobikomi rev B video hardware would, in playfield orientation."""
import argparse
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import BUILD
from preview import Roms

WIDTH, HEIGHT = 240, 320
CELL_BUDGET = 48
BG_MAP, FG_MAP, SPRITE_RAM, SIZE_RAM = 0xD000, 0xE000, 0xF000, 0xF200
REG_SCROLL_Y_LO, REG_SCROLL_Y_HI, REG_SCROLL_X, REG_VIDEO = 0, 1, 2, 3


def map_entry(mem, base, col, row):
    offset = base + (row * 32 + col) * 2
    return mem[offset] | mem[offset + 1] << 8


def tile_pixel(roms, mem, base, x, y):
    entry = map_entry(mem, base, x >> 3, y >> 3)
    pen = roms.tile_pen(entry & 0x7FF, x & 7, y & 7)
    return pen, (entry >> 11) * 16 + pen


def decode(mem, n):
    sx, sy, code, attr = mem[SPRITE_RAM + 4 * n:SPRITE_RAM + 4 * n + 4]
    size = mem[SIZE_RAM + n]
    wc, hc = min(size & 3, 2), min((size >> 2) & 3, 2)
    return (sx | (size & 0x10) << 4, sy | (attr & 4) << 6, code | (attr & 3) << 8 | (size & 0x20) << 5,
            attr >> 3, wc, hc, (size >> 6) & 1)


def sprites_on_column(mem, x):
    chosen, cost = [], 0
    for n in range(128):
        sx, sy, code, pal, wc, hc, behind = decode(mem, n)
        h = 16 << hc
        if not (sy < HEIGHT or sy > 512 - h) or ((x - sx) & 511) >= 16 << wc:
            continue
        if cost + (1 << hc) > CELL_BUDGET:
            break
        cost += 1 << hc
        col = (x - sx) & 511
        chosen.append((col, sy, code + (col >> 4) * (1 << hc), pal, h, behind))
        if cost == CELL_BUDGET:
            break
    return chosen, cost


def render(roms, dump):
    mem, regs = dump[:0x10000], dump[0x10000:]
    scroll_y = regs[REG_SCROLL_Y_LO] | (regs[REG_SCROLL_Y_HI] & 1) << 8
    scroll_x = regs[REG_SCROLL_X]
    video = regs[REG_VIDEO]
    image = Image.new("RGB", (WIDTH, HEIGHT))
    busiest = 0
    for x in range(WIDTH):
        column, cost = sprites_on_column(mem, x)
        busiest = max(busiest, cost)
        for y in range(HEIGHT):
            colour = (0, 0, 0)
            bg_pen = 0
            if video & 2:
                bg_pen, index = tile_pixel(roms, mem, BG_MAP, (x + scroll_x) & 255, (y + scroll_y) & 511)
                colour = roms.colours[index][:3]
            if video & 8:
                for cx, sy, code, pal, h, behind in column:
                    ry = (y - sy) & 511
                    if ry < h:
                        pen = roms.cell_pen((code + (ry >> 4)) & 2047, cx & 15, ry & 15)
                        if pen:
                            if not (behind and bg_pen and video & 2):
                                colour = roms.colours[512 + pal * 16 + pen][:3]
                            break
            if video & 4:
                pen, index = tile_pixel(roms, mem, FG_MAP, x, y)
                if pen:
                    colour = roms.colours[index][:3]
            image.putpixel((x, y), colour)
    return image, busiest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path)
    parser.add_argument("png", type=Path)
    args = parser.parse_args()
    image, busiest = render(Roms(), args.dump.read_bytes())
    image.resize((WIDTH * 2, HEIGHT * 2), Image.NEAREST).save(args.png)
    print(f"render: {args.png.name}, busiest column has {busiest} cell fetches")


if __name__ == "__main__":
    sys.exit(main())
