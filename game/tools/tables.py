#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract the rect, tile and animation tables from FixitFelixJr.exe into build/tables.json."""
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common
from common import BUILD

IMAGE_BASE = 0x400000
RECT_TABLE = 0x418458
RECT_COUNT = 217
TILE_TABLE = 0x418348
TILE_COUNT = 27
ANIM_TABLE = 0x419800
ANIM_COUNT = 100
MODES = {0: "static", 1: "loop", 2: "once"}
SHIFTED_FRAME_ANIMS = (36, 38)
TABLES_MD5 = "eb87ccabad97d0d05bec4cf65f5ddd04"


class PeImage:
    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        count = struct.unpack_from("<H", data, pe + 6)[0]
        opt_size = struct.unpack_from("<H", data, pe + 20)[0]
        self.sections = []
        for i in range(count):
            off = pe + 24 + opt_size + 40 * i
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, off + 8)
            self.sections.append((vaddr, max(vsize, rsize), rsize, rptr))

    def offset(self, va):
        rva = va - IMAGE_BASE
        for vaddr, vsize, rsize, rptr in self.sections:
            if vaddr <= rva < vaddr + rsize:
                return rptr + rva - vaddr
        raise ValueError(f"VA {va:#x} not in file data")

    def shorts(self, va, count):
        return struct.unpack_from(f"<{count}h", self.data, self.offset(va))

    def dword(self, va):
        return struct.unpack_from("<I", self.data, self.offset(va))[0]


def read_rects(pe, table, count):
    rects = []
    for k in range(count):
        index, x, y, w, h = pe.shorts(table + 10 * k, 5)
        if index != k:
            raise ValueError(f"rect table {table:#x} entry {k} has index {index}")
        rects.append({"x": x * 8, "y": y * 8, "w": w * 8, "h": h * 8})
    return rects


def fix_shifted_frame(frame, previous):
    if frame["mirror"] == -1 or frame["y"] != 0 or previous is None:
        return frame, False
    shifted = {"rect": frame["rect"], "mirror": -1, "dur": frame["mirror"],
               "x": frame["dur"], "y": frame["x"]}
    return shifted, True


def read_anims(pe):
    anims = []
    fixes = []
    for n in range(ANIM_COUNT):
        va = ANIM_TABLE + 12 * n
        ident, count, mode, _ = pe.shorts(va, 4)
        frames_va = pe.dword(va + 8)
        frames = []
        for f in range(count):
            rect, mirror, dur, x, y = pe.shorts(frames_va + 10 * f, 5)
            frame = {"rect": rect, "mirror": mirror, "dur": dur, "x": x, "y": y}
            if n in SHIFTED_FRAME_ANIMS:
                frame, fixed = fix_shifted_frame(frame, frames[-1] if frames else None)
                if fixed:
                    fixes.append(f"anim {n} frame {f}")
            frames.append(frame)
        anims.append({"id": ident, "mode": MODES[mode], "frames": frames})
    return anims, fixes


def check(rects, tiles, anims):
    digest = hashlib.md5(json.dumps([rects, tiles, anims], sort_keys=True).encode()).hexdigest()
    if digest != TABLES_MD5:
        raise SystemExit(f"tables from {common.EXE} have md5 {digest}, expected {TABLES_MD5}: not the known version of the game")


def main():
    pe = PeImage(common.need_exe().read_bytes())
    rects = read_rects(pe, RECT_TABLE, RECT_COUNT)
    tiles = read_rects(pe, TILE_TABLE, TILE_COUNT)
    anims, fixes = read_anims(pe)
    check(rects, tiles, anims)
    BUILD.mkdir(exist_ok=True)
    out = BUILD / "tables.json"
    out.write_text(json.dumps({"rects": rects, "tiles": tiles, "anims": anims,
                               "fixed_frames": fixes}, indent=1))
    print(f"tables: {len(rects)} rects, {len(tiles)} tiles, {len(anims)} anims, "
          f"{len(fixes)} shifted frame records repaired -> {out}")


if __name__ == "__main__":
    sys.exit(main())
