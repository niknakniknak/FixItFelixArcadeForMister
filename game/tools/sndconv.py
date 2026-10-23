#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert the game's sounds for the X1-010 the way the ROM maker does and write the sample ROM and its C table.

The Windows sounds come from FELIX_WIN/Data. FELIX_ATGAMES (the AtGames res_fif folder) and FELIX_MD (the Mega Drive
ROM) are optional; without them those sounds are silent.
"""
import hashlib
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rommaker"))
sys.path.insert(1, str(HERE))
import common
import rommaker

SOUNDS = len(rommaker.RECIPE)


def inputs():
    exe = common.need_exe()
    problems = rommaker.check_inputs(exe.parent, rommaker.WINDOWS_MD5, "Windows build")
    found = {"win": exe.parent / "Data"}
    if os.environ.get("FELIX_ATGAMES"):
        folder = Path(os.environ["FELIX_ATGAMES"])
        problems += rommaker.check_inputs(folder, rommaker.ATGAMES_MD5, "AtGames res_fif")
        found["atg"] = folder
    if os.environ.get("FELIX_MD"):
        data = Path(os.environ["FELIX_MD"]).read_bytes()
        if hashlib.md5(data).hexdigest() != rommaker.MEGADRIVE_MD5:
            problems.append(f"Mega Drive ROM md5 {hashlib.md5(data).hexdigest()} is not the known good {rommaker.MEGADRIVE_MD5}")
        found["md"] = data
    if problems:
        sys.exit("\n".join(problems))
    return found


def table(place, lengths):
    lines = ["#ifndef FELIX_SAMPLES_H", "#define FELIX_SAMPLES_H", "",
             "#define SAMPLE_BANK_ANY 2", "",
             "#define SAMPLE_TABLE { \\"]
    for n, entry in enumerate(rommaker.RECIPE):
        page, bank = place[n]
        end = 0x100 - (page + rommaker.pages_of(lengths[n]))
        lines.append(f"    {{ 0x{page:02X}, 0x{end:02X}, {entry[4]}, {bank} }}, \\")
    lines += ["}", "", f"#define SAMPLES {SOUNDS}", "", "#endif", ""]
    return "\n".join(lines)


def main():
    found = inputs()
    rom, place, lengths, _ = rommaker.sample_rom(found)
    common.BUILD.mkdir(exist_ok=True)
    (common.BUILD / "felix_samples.bin").write_bytes(rom)
    common.GEN.mkdir(parents=True, exist_ok=True)
    (common.GEN / "samples.h").write_text(table(place, lengths))
    silent = [k for k in ("atg", "md") if k not in found]
    print(f"sndconv: {SOUNDS} samples, file {len(rom):#x}" + (f", silent: {' '.join(silent)}" if silent else ""))


if __name__ == "__main__":
    sys.exit(main())
