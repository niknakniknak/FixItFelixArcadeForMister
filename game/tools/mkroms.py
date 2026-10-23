#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble a program binary, the graphics and the samples into a zipped ROM set."""
import argparse
import sys
import zipfile
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import BUILD

MAIN_SIZE = 0x20000
SPRITE_SIZE = 0x40000
CHECKSUM_AT = MAIN_SIZE - 1
CHIP = 0x8000
SIZES = {
    "felix_tiles.bin": 0x10000,
    "felix_pal.bin": 0x800,
}
SAMPLE_CHIP = 0x80000
SAMPLE_MAX = 4 * SAMPLE_CHIP
CHIPS = [
    ("felix_main.bin",    0x8000,      ["fj-p1.1d", "fj-p2.1e", "fj-p3.1f", "fj-p4.1g"]),
    ("felix_tiles.bin",   0x8000,      ["fj-t1.5h", "fj-t2.5j"]),
    ("felix_sprites.bin", 0x8000,      ["fj-s1.7a", "fj-s2.7b", "fj-s3.7c", "fj-s4.7d",
                                        "fj-s5.7e", "fj-s6.7f", "fj-s7.7g", "fj-s8.7h"]),
    ("felix_samples.bin", SAMPLE_CHIP, ["fj-v1.3a", "fj-v2.3b", "fj-v3.3c", "fj-v4.3d"]),
    ("felix_pal.bin",     0x400,       ["fj-pl.6b", "fj-ph.6c"]),
]


def main_rom(program):
    if len(program) > MAIN_SIZE:
        raise ValueError(f"program is {len(program):#x} bytes, limit {MAIN_SIZE:#x}")
    rom = bytearray(program + b"\xff" * (MAIN_SIZE - len(program)))
    for start in range(0, MAIN_SIZE, CHIP):
        end = start + CHIP - 1
        rom[end] = (-sum(rom[start:end])) & 0xFF
    return bytes(rom)


def checked(name, data, size):
    if len(data) != size:
        raise ValueError(f"{name} is {len(data):#x} bytes, expected {size:#x}")
    return data


def regions(program):
    data = {"felix_main.bin": main_rom(program)}
    for name, size in SIZES.items():
        data[name] = checked(name, (BUILD / name).read_bytes(), size)
    sprites = (BUILD / "felix_sprites.bin").read_bytes()
    if len(sprites) > SPRITE_SIZE or len(sprites) % 0x20000:
        raise ValueError(f"felix_sprites.bin is {len(sprites):#x} bytes")
    data["felix_sprites.bin"] = sprites + bytes(SPRITE_SIZE - len(sprites))
    samples = (BUILD / "felix_samples.bin").read_bytes()
    if len(samples) > SAMPLE_MAX:
        raise ValueError(f"felix_samples.bin is {len(samples):#x} bytes, limit {SAMPLE_MAX:#x}")
    data["felix_samples.bin"] = samples + b"\xff" * (SAMPLE_MAX - len(samples))
    return data


def rom_set(program):
    data = regions(program)
    files = {}
    for region, size, names in CHIPS:
        blob = checked(region, data[region], size * len(names))
        for i, name in enumerate(names):
            files[name] = blob[i * size:(i + 1) * size]
    return files


def write_zip(path, files):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(name, data)
    lines = [f"{name:20s} size {len(data):#08x} ({len(data)}) crc32 {zlib.crc32(data):08x}"
             for name, data in files.items()]
    path.with_suffix(".txt").write_text("\n".join(lines) + "\n")
    print(f"{path.name}:")
    for line in lines:
        print("  " + line)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path, help="Z80 program binary (makebin -p output)")
    parser.add_argument("zip", help="output zip name in build/, e.g. felixjr.zip")
    args = parser.parse_args()
    program = args.program.read_bytes()
    print(f"program {args.program.name}: {len(program):#x} of {CHECKSUM_AT:#x} bytes used")
    write_zip(BUILD / args.zip, rom_set(program))


if __name__ == "__main__":
    sys.exit(main())
