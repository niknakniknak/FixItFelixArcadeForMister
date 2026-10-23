# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
ATTRACT = [
    "hiscore_frames", "hiscore_after_game", "game_over_frames", "entry_frames", "entry_repeat", "howto_step",
    "howto_hold", "initial_letter", "entry_letter_idx", "howto_step0_inc", "howto_cycle_mask", "howto_cycle_step_h",
    "howto_cycle_step_f", "howto_cycle_mod", "bonus0", "boot_wait", "boot_garbage_at", "boot_garbage_frames",
    "boot_blank_last", "boot_ok_frames", "boot_colour", "boot_col", "ram_ok_row", "rom_ok_row", "logo_col",
    "logo_row", "copyright_col", "copyright_row", "insert_coin_col", "title_prompt_row", "press_1p_col",
    "press_1p2p_col", "prompt_on_colour", "coin_off_colour", "start_off_colour", "blink_mask", "blink_on",
    "hiscore_prompt_row", "table_red", "high_scores_col", "high_scores_row", "name_score_col", "name_score_row",
    "table_first_row", "table_row_step", "table_name_col", "entry_white", "table_score_col", "game_over_colour",
    "game_over_col", "game_over_row", "entry_base", "entry_player_col", "entry_player_row", "entry_player_colour",
    "entry_player_blink_mask", "entry_player_blink_on", "entry_red", "enter_initials_row", "entry_name_score_col",
    "entry_name_score_row", "entry_initial_col", "entry_row_step", "initial_red", "initial_white",
    "initial_blink_mask", "initial_blink_on", "entry_time_red", "entry_time_col", "entry_time_row", "entry_time_div",
    "choose_col", "choose_row", "joystick_col", "joystick_row", "press_jump_row", "howto_colour", "fix_col",
    "fix_window_row", "fix_last_row", "window_500_row", "avoid_col", "avoid_row", "block_col", "block_row",
    "jump_over_col", "jump_over_row", "collect_col", "collect_row", "become_col", "become_row", "bonus_life_col",
    "bonus_life_row", "bonus_num_col", "bonus_num_row",
]
STATE = [
    "switch_frames", "music_loop", "crash_per_frame", "crash_colours", "crash_rows", "crash_last_col",
    "crash_start_col", "switch_red", "gop_col", "gop_row", "gop_number_base", "ready_col", "ready_row",
    "scroll_speed", "pie_min_broken", "pie_retry",
]
HUD = [
    "lives_col", "max_lives_shown", "hiscore_pic_col", "hiscore_pic_row", "hiscore_colour", "hiscore_col",
    "hiscore_row", "up_colour", "up_col", "up_row", "score_colour", "score_col", "score_row", "credits_bar_col",
    "credits_bar_row", "credits_colour", "credits_col", "credits_row",
]
RECORD_NAMES = {
    "attract.hiscore_frames": "attract.HISCORE_FRAMES", "attract.hiscore_after_game": "attract.HISCORE_AFTER_GAME",
    "attract.game_over_frames": "attract.GAME_OVER_FRAMES", "attract.entry_frames": "attract.ENTRY_FRAMES",
    "attract.entry_repeat": "attract.ENTRY_REPEAT", "attract.howto_step": "attract.HOWTO_STEP",
    "attract.howto_hold": "attract.HOWTO_HOLD", "state.switch_frames": "state.SWITCH_FRAMES",
    "state.music_loop": "state.MUSIC_LOOP", "state.crash_last_col": "state.CRASH_LAST_COL",
    "hud.lives_col": "hud.LIVES_COL", "hud.max_lives_shown": "hud.MAX_LIVES_SHOWN",
}


def _scalars():
    out = {}
    for stem, names in (("attract", ATTRACT), ("state", STATE), ("hud", HUD)):
        for n in names:
            rec = f"{stem}.{n}"
            out[f"{stem}_{n}"] = (RECORD_NAMES.get(rec, rec), None)
    return out


SCALARS = _scalars()


ATTRACT_TEXTS = [
    ("high_scores", "attract.high_scores_text"), ("name_score", "attract.name_score_text"),
    ("game_over", "attract.game_over_text"), ("enter_initials", "attract.enter_initials_text"),
    ("choose", "attract.choose_text"), ("joystick", "attract.joystick_text"),
    ("press_jump", "attract.press_jump_text"), ("ram_ok", "attract.ram_ok_text"), ("rom_ok", "attract.rom_ok_text"),
    ("insert_coin", "attract.insert_coin_text"), ("press_1p", "attract.press_1p_text"),
    ("press_1p2p", "attract.press_1p2p_text"), ("player", "attract.player_fmt"),
    ("time_left", "attract.time_left_fmt"), ("fix_window", "attract.fix_window_text"),
    ("fix_last", "attract.fix_last_text"), ("window_500", "attract.window_500_text"), ("avoid", "attract.avoid_text"),
    ("block", "attract.block_text"), ("jump_over", "attract.jump_over_text"), ("collect", "attract.collect_text"),
    ("become", "attract.become_text"), ("bonus_life", "attract.bonus_life_text"), ("pts", "attract.pts_text"),
]
STATE_TEXTS = [("game_over_player", "state.game_over_player_fmt"), ("player", "state.player_text"),
               ("ready", "state.ready_text")]
HUD_TEXTS = [("credits", "hud.credits_fmt")]
LENGTHS = {
    "attract.high_scores_text": 11, "attract.name_score_text": 15, "attract.game_over_text": 9,
    "attract.enter_initials_text": 19, "attract.choose_text": 18, "attract.joystick_text": 8,
    "attract.press_jump_text": 20, "attract.ram_ok_text": 6, "attract.rom_ok_text": 6,
    "attract.insert_coin_text": 11, "attract.press_1p_text": 14, "attract.press_1p2p_text": 20,
    "attract.player_fmt": 7, "attract.time_left_fmt": 11, "attract.fix_window_text": 18, "attract.fix_last_text": 8,
    "attract.window_500_text": 18, "attract.avoid_text": 5, "attract.block_text": 15, "attract.jump_over_text": 9,
    "attract.collect_text": 16, "attract.become_text": 18, "attract.bonus_life_text": 16, "attract.pts_text": 3,
    "state.game_over_player_fmt": 17, "state.player_text": 7, "state.ready_text": 5, "hud.credits_fmt": 8,
}


def _texts(prefix, items, section, file):
    out = []
    for name, rec in items:
        width = LENGTHS[rec] + 1
        out.append({"name": f"exe_{prefix}_{name}", "section": section, "type": "char", "fields": ["u8"],
                    "dims": [width], "parts": [{"rec": rec, "str": width}], "file": file})
    return out


TABLES = [
    {"name": "exe_attract_alphabet", "section": "BANK3", "type": "char", "fields": ["u8"], "dims": [39],
     "parts": [{"rec": "attract.alphabet", "str": 39}], "file": "attract.c", "was": "alphabet"},
    {"name": "exe_attract_table_colours", "section": "BANK3", "fields": ["u8"], "dims": [5],
     "parts": [{"rec": "attract.table_colours"}], "file": "attract.c", "was": "table_colours"},
    {"name": "exe_attract_howto_objs", "section": "BANK3", "type": "howto_obj_t", "fields": ["u8", "u8", "s16"],
     "dims": [9], "parts": [{"rec": "attract.howto_objs"}], "file": "attract.c", "was": "howto_objs"},
    {"name": "exe_state_crash_text", "section": "fixed", "type": "char", "fields": ["u8"], "dims": [8],
     "parts": [{"rec": "state.crash_text", "str": 8}], "file": "state.c", "was": "crash_text"},
] + _texts("attract", ATTRACT_TEXTS, "BANK3", "attract.c") + _texts("state", STATE_TEXTS, "BANK3", "hud.c") \
    + _texts("hud", HUD_TEXTS, "BANK3", "hud.c")
