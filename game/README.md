# Fix-It Felix Jr. for the Tobikomi board

A from-scratch rewrite of Fix-It Felix Jr. in C for the Z80, running on the "Tobikomi board rev B", an invented arcade board implemented as a MiSTer FPGA core. This repository holds the game program, the board test program and the tools that build the ROM set.

This is an unofficial fan project. Fix-It Felix Jr. and its characters belong to Disney. The repository contains no graphics, sounds, text or data from the original game. Everything that comes from the original is read from your own copy of the Windows game when the ROM set is built.

## The board in short

- Z80 at 8 MHz, 128 KB program ROM: a fixed 32 KB area (0000-7FFF) and 16 KB banks 2-7 switched in at 8000-BFFF through F818.
- 4 KB work RAM at C000, BG and FG tilemaps (32x64 entries, two pages each) at D000 and E000, 128 sprites at F000 with a size table at F200, 1 KB NVRAM at F400.
- 240x320 portrait screen, 8x8 tiles, 16x16 sprite cells with 32 and 64 pixel big sprites, 15-bit palettes.
- Seta X1-010 PCM sound with 2 MB of sample ROM in two 1 MB banks.
- Inputs, DIP switches and video, interrupt, coin counter and bank registers at F800-F819, X1-010 registers at F900.

`src/hw.h` and `src/game/board.h` list every register.

## Building

Requirements: SDCC 4.5 (`sdcc`, `sdasz80`, `makebin`), GNU make, Python 3 with Pillow.

You need the Windows build of Fix-It Felix Jr. ("FIFJ_leaked"): `FixitFelixJr.exe` with its `Data` folder. Point `FELIX_WIN` at the folder that holds them:

```
make FELIX_WIN=/path/to/FixItFelixJr
```

This writes `build/felixjr.zip` (the game) and `build/felixjr_test.zip` (the board test). Two optional inputs fill in sounds that the Windows build lacks; without them those sounds are silent:

- `FELIX_ATGAMES`: the AtGames build's `res_fif` folder (high-score entry and player-change sounds).
- `FELIX_MD`: the Mega Drive ROM "Fix-It Felix Jr. (World) (Aftermarket).md" (the two voice clips).

Every input is checked against known md5 sums, and the build stops if a file is a different version.

Other targets:

- `make game`: only the game program, `build/felix_main.bin`.
- `make game-test`: builds the game modules for the host with gcc (AddressSanitizer and UBSan) and runs `src/game/host/test_game.c` against a fake board: level tables, scoring, high scores, boot to title, a full new game, the attract demo, progress to the kill screen and random-input runs.
- `make sim Z80_H=<dir>`: builds `tools/boardsim.cpp` against the single-header Z80 emulator `z80.h` (MIT, from the `z80` package on PyPI: `pip download --no-deps --no-binary :all: z80` and unpack it), runs the board test for 330 frames with scripted inputs and renders two frames to `build/sim/`.
- `make clean`: removes `build/`.

Copy `felixjr.zip` to the MiSTer's arcade ROM folder next to the core and its MRA. The CRCs of the files in the zip depend on the build, so they will not match a CRC list made from another build.

## How the original's numbers get into the program

The game logic is our own C code, but it uses many numbers and tables from the original: level layouts, speeds, timings, positions, scores, text. None of them are written in the source. Each one is described in `tools/exedata/` as a record: where it sits in `FixitFelixJr.exe` (an address of stored data, or of an instruction operand) plus the steps that turn it into the value the Z80 program needs (scaling, fixed point, sums of other records). `tools/exedata/spec_*.py` group the records into scalars and tables.

`tools/exedata/gen.py` reads them from your exe and writes `build/gen/exe_*`: tables become const arrays in one module per ROM section, and scalars become absolute link-time symbols (`exe_rules.s`), so the compiled code only holds them as relocated operands. The game reads them through the `X8()`, `X16()` and `XL()` macros in `src/game/exe.h`.

Graphics come from `Data/texture2.bmp` and the `felix*.png` and `hammer*.png` recolour sheets, cut up by the rect, tile and animation tables in the exe. The attract-mode demo is replayed from `Data/Controls.log`.

## The ROM maker

`tools/rommaker/` builds the same ROM set for people who only have the game, with no compiler:

- `rommaker.py dist` (developer step) takes the linked program and blanks every part that comes from the original: the exe tables, every relocated scalar operand (found by linking probe builds in which each scalar is moved by a known amount, `make probes`), and the graphics and demo tables. The result, `felix_prog.bin`, is our program with holes, plus `holes.json` and `exe.json`, which say where each hole is and which record fills it.
- `rommaker.py build --exe FixitFelixJr.exe [--atgames res_fif] [--megadrive file.md]` fills the holes from the user's files, converts the graphics and sounds and writes `felixjr.zip`. It uses only the Python standard library (`imagelite.py` stands in for the small part of Pillow the converters use).
- `felixrommaker/` is the same tool in C for Windows (`felixrommaker.exe`, with stb_image and miniz vendored in `vendor/`). `embed.py` builds the program template and the record descriptions into it.
- `mkrelease.sh` builds the game, the template and both tools, and checks that for every combination of optional inputs the Python and C tools write identical zips, that the program and graphics match the normal build, and that the checksums of the result are the ones the tool expects. It needs `REF`, a folder with `windows/FixitFelixJr.exe`, `fifj_upload/res_fif` and `megadrive/`, and writes the release to `OUT` (default `build/release`).

`tools/rommaker/README.txt` is the user guide that ships with `felixrommaker.exe`.

## Source layout

- `src/crt0.s`: reset, the vblank interrupt handler, the banked-call trampoline.
- `src/boot.s`: the boot screen fill and the sliced ROM checksum.
- `src/video.c`, `src/sprite.s`, `src/bigspr.s`, `src/bgfast.s`: tilemap and text drawing, metasprite and big-sprite output, packed tilemap decoding.
- `src/test.c`, `src/test_stub.s`: the board test (RAM and ROM tests, colours, sprites, scrolling, sound, inputs and coin counters).
- `src/game/`: the game. The fixed area holds `actor`, `bg`, `bldg`, `fixed`, `page`, `player`, `rng`, `state`, `tmap` and `main`; bank 2 holds the gameplay modules (`felix`, `ralph`, `actors`, `input`, `hiscore`) and the demo log; bank 3 `attract` and `hud`; bank 4 `cutscene`; bank 5 `sound` and the boot screen table; bank 6 `service`. Functions called across banks are declared `BANKED` (`bank.h`).
- `src/game/service.c`: service mode (test switch, or the OSD's service option): RAM/ROM, input, coin counter, sound and screen tests, DIP switch readout, operator settings, bookkeeping and high scores kept in NVRAM.
- `tools/tables.py`, `tools/gfxconv.py`, `tools/gencode.py`, `tools/preview.py`, `tools/tmpack.py`: read the exe's rect, tile and animation tables, convert the graphics to the board's tile, sprite and palette ROMs and C tables, and check every converted picture against the original pixel for pixel (`build/preview_*.png`).
- `tools/sndconv.py`: converts the sounds for the X1-010 (resampled to the chip's rates, laid out on 4 KB pages in two banks) and writes the sample ROM and `build/gen/samples.h`.
- `tools/ctlpack.py`, `tools/garbage.py`: the packed attract demo and the boot screen table.
- `tools/ihx2rom.py`, `tools/bankcheck.py`, `tools/profmap.py`: the banked program image, a check that banked code only reaches what is mapped, and a link map with static functions.
- `tools/mkroms.py`: splits the ROM regions into the zip's chip files.
- `tools/tobikomi.h`, `tools/boardsim.cpp`, `tools/gamesim.cpp`, `tools/render.py`, `tools/profile.sh`: a board model for running the test program and profiling the game frame by frame, and a renderer for its video dumps.

## Licence

GPL-3.0-or-later, see `LICENSE`. `tools/rommaker/felixrommaker/vendor/` keeps its own licences (stb_image: public domain or MIT; miniz: MIT).
