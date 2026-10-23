# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parent.parent
BUILD = GAME / "build"
GEN = BUILD / "gen"
REFERENCE = Path(os.environ["FELIX_WIN"]) if os.environ.get("FELIX_WIN") else None
EXE = REFERENCE / "FixitFelixJr.exe" if REFERENCE else None
DATA = REFERENCE / "Data" if REFERENCE else None


def need_exe():
    if EXE is None:
        sys.exit("set FELIX_WIN to the folder that holds FixitFelixJr.exe and its Data folder")
    if not EXE.is_file():
        sys.exit(f"{EXE} not found: FELIX_WIN must be the folder that holds FixitFelixJr.exe and its Data folder")
    return EXE
