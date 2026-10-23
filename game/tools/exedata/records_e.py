# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
RECORDS = [
    {"name": "attract.entry_name_score_col", "kind": "calc", "op": "sum",
     "from": ["attract.entry_red", "attract.entry_name_score_col_delta"]},
    {"name": "attract.entry_player_colour_delta", "kind": "imm", "va": 0x4082D9, "off": 2, "type": "i8"},
    {"name": "attract.entry_player_colour", "kind": "calc", "op": "sum",
     "from": ["attract.entry_base", "attract.entry_player_colour_delta"]},
    {"name": "attract.entry_player_blink_mask", "kind": "imm", "va": 0x4082BD, "off": 2, "type": "u8"},
    {"name": "attract.entry_player_blink_on", "kind": "imm", "va": 0x4082C0, "off": 2, "type": "u8"},
    {"name": "attract.initial_red", "kind": "imm", "va": 0x408330, "off": 1, "type": "u32"},
    {"name": "attract.initial_white", "kind": "imm", "va": 0x408352, "off": 1, "type": "u32"},
    {"name": "attract.initial_blink_mask", "kind": "imm", "va": 0x408344, "off": 2, "type": "u8"},
    {"name": "attract.initial_blink_on", "kind": "imm", "va": 0x408348, "off": 2, "type": "u8"},
    {"name": "state.player_text", "kind": "data", "va": 0x416470, "type": "str", "count": 7},
    {"name": "state.ready_text", "kind": "data", "va": 0x41647A, "type": "str", "count": 5},
]
