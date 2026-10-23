# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
ifndef FELIX_GAME_MK
FELIX_GAME_MK := 1
GAME_ALLOCS ?= 50000
EXE_SECTIONS := fixed bank2 bank3 bank4 bank5 bank6
EXE_OBJS = $(EXE_SECTIONS:%=$(OBJ)/game/exe_%.rel)
EXE_GEN := $(GEN)/exe_scalars.h $(GEN)/exe_tables.h $(EXE_SECTIONS:%=$(GEN)/exe_%.c) $(GEN)/exe_rules.s $(GEN)/exe_probes.mk
-include $(GEN)/exe_probes.mk
GAME_DIR := src/game
GAME_OBJ := $(OBJ)/game
GAME_C := $(wildcard $(GAME_DIR)/*.c)
GAME_FIXED := actor bg bldg fixed page player rng state tmap main
GAME_HOME := $(filter-out $(GAME_FIXED) attract cutscene service,$(GAME_C:$(GAME_DIR)/%.c=%))
GAME_ALL_OBJS := $(GAME_FIXED:%=$(GAME_OBJ)/%.rel) $(GAME_HOME:%=$(GAME_OBJ)/%.rel) $(GAME_OBJ)/demo_log.rel \
    $(GAME_OBJ)/attract.rel $(GAME_OBJ)/cutscene.rel $(GAME_OBJ)/service.rel
GAME_LINK_OBJS := $(OBJ)/crt0.rel $(OBJ)/sprite.rel $(OBJ)/bigspr.rel $(OBJ)/video.rel $(OBJ)/boot.rel $(OBJ)/bgfast.rel $(GAME_OBJ)/gfx_game.rel $(GAME_ALL_OBJS) \
    $(GAME_OBJ)/garbage.rel $(EXE_OBJS)
$(foreach m,$(GAME_HOME),$(eval BANK_$(m) := --codeseg BANK2))
BANK_attract := --codeseg BANK3
BANK_hud := --codeseg BANK3
BANK_cutscene := --codeseg BANK4
BANK_service := --codeseg BANK6
BANK_sound := --codeseg BANK5 --constseg BANK5
GAME_BANKS := -Wl-b_BANK2=0x28000 -Wl-b_BANK3=0x38000 -Wl-b_BANK4=0x48000 -Wl-b_BANK5=0x58000 -Wl-b_BANK6=0x68000
GAME_CFLAGS := -mz80 --std-c11 --opt-code-size --max-allocs-per-node $(GAME_ALLOCS) -Isrc -I$(GAME_DIR) -I$(GEN) $(GAME_EXTRA)
GAME_HEADERS := $(GEN)/exe_scalars.h $(GEN)/exe_tables.h $(wildcard $(GAME_DIR)/*.h) $(GEN)/gfx.h $(GEN)/demo_log.h $(GEN)/samples.h

.PHONY: game
game: $(BUILD)/felix_main.bin

$(GEN)/demo_log.c $(GEN)/demo_log.h &: tools/ctlpack.py $(REF)/Data/Controls.log
	$(PY) tools/ctlpack.py

$(GAME_OBJ)/%.rel: $(GAME_DIR)/%.c $(GAME_HEADERS)
	@mkdir -p $(GAME_OBJ)
	$(CC) $(GAME_CFLAGS) $(BANK_$*) -c -o $@ $<

$(GEN)/gfx_game.c: tools/tmpack.py $(GEN)/gfx.c
	$(PY) tools/tmpack.py

$(GAME_OBJ)/gfx_game.rel: $(GEN)/gfx_game.c $(GEN)/gfx.h
	@mkdir -p $(GAME_OBJ)
	$(CC) $(GAME_CFLAGS) -c -o $@ $<

$(GEN)/garbage.s: tools/garbage.py $(BUILD)/gfx.json
	$(PY) tools/garbage.py

$(GAME_OBJ)/garbage.rel: $(GEN)/garbage.s
	@mkdir -p $(GAME_OBJ)
	$(AS) -plosgff -o $@ $<

$(GAME_OBJ)/demo_log.rel: $(GEN)/demo_log.c $(GEN)/demo_log.h
	@mkdir -p $(GAME_OBJ)
	$(CC) $(GAME_CFLAGS) --codeseg BANK2 -c -o $@ $<

$(EXE_GEN) &: tools/exedata/gen.py tools/exedata/exefw.py tools/exedata/records.py $(wildcard tools/exedata/spec_*.py tools/exedata/records_*.py) $(REF)/FixitFelixJr.exe
	$(PY) tools/exedata/gen.py

$(GAME_OBJ)/exe_fixed.rel: $(GEN)/exe_fixed.c $(GAME_DIR)/exe_types.h
	@mkdir -p $(GAME_OBJ)
	$(CC) $(GAME_CFLAGS) -c -o $@ $<

$(GAME_OBJ)/exe_bank%.rel: $(GEN)/exe_bank%.c $(GAME_DIR)/exe_types.h
	@mkdir -p $(GAME_OBJ)
	$(CC) $(GAME_CFLAGS) --codeseg BANK$* -c -o $@ $<

$(GAME_OBJ)/exe_rules.rel: $(GEN)/exe_rules.s
	@mkdir -p $(GAME_OBJ)
	$(AS) -plosgff -o $@ $<

$(GAME_OBJ)/exe_rules_probe%.rel: $(GEN)/exe_rules.s
	@mkdir -p $(GAME_OBJ)
	$(AS) -plosgff -o $@ $(GEN)/exe_rules_probe$*.s

$(BUILD)/felix_main.ihx: $(GAME_LINK_OBJS) $(GAME_OBJ)/exe_rules.rel
	$(CC) $(LDFLAGS) $(GAME_BANKS) -o $@ $(GAME_LINK_OBJS) $(GAME_OBJ)/exe_rules.rel

$(BUILD)/probe/felix_probe%.ihx: $(GAME_LINK_OBJS) $(GAME_OBJ)/exe_rules_probe%.rel
	@mkdir -p $(BUILD)/probe
	$(CC) $(LDFLAGS) $(GAME_BANKS) -o $@ $(GAME_LINK_OBJS) $(GAME_OBJ)/exe_rules_probe$*.rel

$(BUILD)/probe/felix_probe%.bin: $(BUILD)/probe/felix_probe%.ihx tools/ihx2rom.py
	$(PY) tools/ihx2rom.py $< $@ > /dev/null

.PHONY: probes
probes: $(EXE_PROBES:%=$(BUILD)/probe/felix_probe%.bin)

$(BUILD)/felix_main.bin: $(BUILD)/felix_main.ihx tools/ihx2rom.py tools/bankcheck.py tools/profmap.py
	$(PY) tools/ihx2rom.py $< $@
	$(PY) tools/profmap.py $(BUILD)/felix_main.map $(BUILD)/felix_main.prof.map $(OBJ) $(GAME_OBJ)
	$(PY) tools/bankcheck.py $(BUILD)/felix_main.prof.map $(OBJ) $(GAME_OBJ)

.PHONY: game-test
game-test: $(GEN)/gfx_game.c $(GEN)/demo_log.c $(GEN)/gfx.h $(GEN)/samples.h $(EXE_GEN)
	sh tools/hosttest.sh

endif
