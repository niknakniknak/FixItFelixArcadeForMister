#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the boot garbage table: random text-layer entries like the original's boot screen."""
import json
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import BUILD

ROW_CELLS = 32
ROWS = 40
WINDOW = 2048
SEED = 0x7E1A


def main():
    layout = json.loads((BUILD / "gfx.json").read_text())["layout"]
    font_tile, font_pal = layout["font_tile"], layout["font_pal"]
    rnd = random.Random(SEED)
    words = []
    for _ in range(ROW_CELLS * ROWS + WINDOW):
        ch = rnd.randrange(93)
        colour = rnd.randrange(7)
        words.append((font_tile + ch - 0x20) | (font_pal + colour) << 11 if 0x20 < ch <= 0x5C else 0)
    lines = ["\t.module garbage", "\t.globl\t_garbage_table", "", "\t.area\t_BANK5", "_garbage_table::"]
    for i in range(0, len(words), 8):
        lines.append("\t.dw\t" + ", ".join("0x%04x" % w for w in words[i:i + 8]))
    out = BUILD / "gen" / "garbage.s"
    out.write_text("\n".join(lines) + "\n")
    print(f"garbage: {len(words)} entries")


if __name__ == "__main__":
    sys.exit(main())
