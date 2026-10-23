# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
PY := python3 -I
BUILD := build
GEN := $(BUILD)/gen
OBJ := $(BUILD)/obj
FELIX_WIN ?=
FELIX_ATGAMES ?=
FELIX_MD ?=
REF := $(FELIX_WIN)
Z80_H ?=
export FELIX_WIN FELIX_ATGAMES FELIX_MD

ifneq ($(filter-out clean,$(or $(MAKECMDGOALS),all)),)
ifeq ($(wildcard $(FELIX_WIN)/FixitFelixJr.exe),)
$(error set FELIX_WIN to the folder that holds FixitFelixJr.exe and its Data folder, e.g. make FELIX_WIN=/path/to/game)
endif
endif

CC := sdcc
AS := sdasz80
CFLAGS := -mz80 --std-c11 --opt-code-size -Isrc -I$(GEN)
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x0100 --data-loc 0xC000

GFX_BINS := $(BUILD)/felix_tiles.bin $(BUILD)/felix_sprites.bin $(BUILD)/felix_pal.bin
ROM_INPUTS := tools/mkroms.py $(GFX_BINS) $(BUILD)/felix_samples.bin
COMMON_OBJS := $(OBJ)/crt0.rel $(OBJ)/sprite.rel $(OBJ)/bigspr.rel $(OBJ)/video.rel $(OBJ)/gfx.rel

.PHONY: all clean sim
all: $(BUILD)/felixjr.zip $(BUILD)/felixjr_test.zip

$(BUILD)/tables.json: tools/tables.py tools/common.py $(REF)/FixitFelixJr.exe
	$(PY) tools/tables.py

$(GEN)/gfx.c $(GEN)/gfx.h $(GFX_BINS) &: tools/gfxconv.py tools/gencode.py tools/preview.py $(BUILD)/tables.json $(REF)/Data/texture2.bmp
	$(PY) tools/gfxconv.py
	$(PY) tools/preview.py

$(BUILD)/felix_samples.bin $(GEN)/samples.h &: tools/sndconv.py tools/rommaker/rommaker.py $(REF)/FixitFelixJr.exe
	$(PY) tools/sndconv.py

$(OBJ)/%.rel: src/%.s
	@mkdir -p $(OBJ)
	$(AS) -plosgff -o $@ $<

$(OBJ)/%.rel: src/%.c src/hw.h src/video.h $(GEN)/gfx.h $(GEN)/samples.h
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/gfx.rel: $(GEN)/gfx.c $(GEN)/gfx.h
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/felix_test.ihx: $(COMMON_OBJS) $(OBJ)/test.rel $(OBJ)/test_stub.rel
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD)/%.bin: $(BUILD)/%.ihx
	makebin -p $< $@

$(BUILD)/felixjr_test.zip: $(BUILD)/felix_test.bin $(ROM_INPUTS)
	$(PY) tools/mkroms.py $< felixjr_test.zip

$(BUILD)/felixjr.zip: $(BUILD)/felix_main.bin $(ROM_INPUTS)
	$(PY) tools/mkroms.py $< felixjr.zip

$(BUILD)/boardsim: tools/boardsim.cpp tools/tobikomi.h
	@test -n "$(Z80_H)" || (echo "set Z80_H to the directory holding z80.h"; exit 1)
	g++ -O2 -std=c++11 -Wall -I$(Z80_H) -Itools -o $@ $<

$(BUILD)/gamesim: tools/gamesim.cpp tools/tobikomi.h
	@test -n "$(Z80_H)" || (echo "set Z80_H to the directory holding z80.h"; exit 1)
	g++ -O2 -std=c++11 -Wall -I$(Z80_H) -Itools -o $@ $<

sim: $(BUILD)/boardsim $(BUILD)/felixjr_test.zip
	mkdir -p $(BUILD)/sim
	cd $(BUILD)/sim && unzip -oq ../felixjr_test.zip
	cd $(BUILD)/sim && cat fj-p1.1d fj-p2.1e fj-p3.1f fj-p4.1g > felix_main.bin
	cd $(BUILD)/sim && cat fj-v1.3a fj-v2.3b fj-v3.3c fj-v4.3d > felix_samples.bin
	cd $(BUILD)/sim && ../boardsim felix_main.bin felix_samples.bin 330 dump 300,320 00 80 \
		"180:0=09,240:0=12,250:0=00,200:2=01,205:2=00,210:2=04,215:2=00,310:0=10"
	$(PY) tools/render.py $(BUILD)/sim/dump300.bin $(BUILD)/sim/frame300.png
	$(PY) tools/render.py $(BUILD)/sim/dump320.bin $(BUILD)/sim/frame320.png

clean:
	rm -rf $(BUILD)

include src/game/game.mk
