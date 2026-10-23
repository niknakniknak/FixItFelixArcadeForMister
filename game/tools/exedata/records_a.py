# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
def const(name, value):
    return {"name": name, "kind": "calc", "op": "diff", "from": ["bldg.level_cap", "bldg.level_cap"],
            "xf": [["add", value]]}


PAL_BASE = 0x41DC00
PAL_INDEX = [["sub", PAL_BASE], ["div", 4]]

RECORDS = [
    const("bldg.zero", 0),
    const("bldg.slot_roof", 5),
    const("bldg.bonus_step1", 10000),
    const("bldg.bonus_step2", 20000),
    const("player.spare_lives1", 1),
    const("player.spare_lives2", 3),
    const("player.spare_lives3", 4),
    {"name": "bldg.door_hit_dy", "kind": "imm", "va": 0x40565F, "off": 2, "type": "i8", "xf": [["mul", -8]]},
    {"name": "bldg.door_hit_top", "kind": "calc", "op": "sum", "from": ["bldg.k1_hit_y", "bldg.k1_hit_h"]},
    {"name": "bldg.brick_rows_span", "kind": "calc", "op": "diff", "from": ["bldg.door_hit_top", "bldg.k3_hit_y"]},
    {"name": "ralph.slot_x_lo", "kind": "calc", "op": "vmin", "from": ["bldg.hit_x0_r0", "bldg.hit_x0_r1", "bldg.hit_x0_r2"]},
    {"name": "ralph.slot_x_hi", "kind": "calc", "op": "vmax", "from": ["bldg.hit_x1_r0", "bldg.hit_x1_r1", "bldg.hit_x1_r2"]},
    {"name": "ralph.hit_add", "kind": "calc", "op": "diff", "from": ["bldg.brick_break_mod", "bldg.brick_break_hit"]},
    {"name": "ralph.hit_mask", "kind": "calc", "op": "sum", "from": ["bldg.brick_break_mod"], "xf": [["lowbit"], ["sub", 1]]},
    {"name": "ralph.hit_shift", "kind": "calc", "op": "sum", "from": ["bldg.brick_break_mod"], "xf": [["lowbit"], ["log2"]]},
    {"name": "ralph.hit_inv", "kind": "calc", "op": "sum", "from": ["bldg.brick_break_mod"], "xf": [["oddpart"], ["inv16"]]},
    {"name": "ralph.hit_limit", "kind": "calc", "op": "sum", "from": ["bldg.brick_break_mod"], "xf": [["oddpart"], ["rfloordiv", 65535]]},
    {"name": "pal.transparent", "kind": "imm", "va": 0x40E726, "off": 2, "type": "u32", "xf": PAL_INDEX},
    {"name": "pal.drawn_black", "kind": "imm", "va": 0x40E742, "off": 2, "type": "u32", "xf": PAL_INDEX},
    {"name": "pal.clothing0", "kind": "imm", "va": 0x40E7B5, "off": 2, "type": "u32", "xf": PAL_INDEX},
    {"name": "pal.clothing1", "kind": "imm", "va": 0x40E7CE, "off": 2, "type": "u32", "xf": PAL_INDEX},
    {"name": "pal.clothing2", "kind": "imm", "va": 0x40E7E8, "off": 2, "type": "u32", "xf": PAL_INDEX},
    {"name": "pal.clothing3", "kind": "imm", "va": 0x40E802, "off": 2, "type": "u32", "xf": PAL_INDEX},
    {"name": "pal.clothing", "kind": "calc", "op": "concat", "from": ["pal.clothing0", "pal.clothing1", "pal.clothing2", "pal.clothing3"]},
    {"name": "pal.hammer0", "kind": "imm", "va": 0x40EA04, "off": 1, "type": "u32"},
    {"name": "pal.hammer1", "kind": "imm", "va": 0x40EA0B, "off": 1, "type": "u32"},
    {"name": "pal.hammer2", "kind": "imm", "va": 0x40EA12, "off": 1, "type": "u32"},
    {"name": "pal.hammer", "kind": "calc", "op": "concat", "from": ["pal.hammer0", "pal.hammer1", "pal.hammer2"]},
    {"name": "bldg.row_win_ty", "kind": "imm", "va": 0x40565F, "off": 2, "type": "i8"},
    {"name": "bldg.door_slot_ty", "kind": "imm", "va": 0x405609, "off": 0, "type": "u8", "xf": [["neg"], ["add", 0x47], ["div", 4]]},
    {"name": "hud.lives_col_raw", "kind": "imm", "va": 0x405FBB, "off": 1, "type": "u32"},
    {"name": "hud.lives_half", "kind": "imm", "va": 0x406017, "off": 3, "type": "i32"},
]
