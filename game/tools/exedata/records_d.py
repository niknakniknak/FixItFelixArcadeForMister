# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
def const(name, k):
    return {"name": name, "kind": "calc", "op": "sum", "from": ["cutscene.intro_ralph_x"], "xf": [["and", 0], ["add", k]]}


def calc(name, op, frm, xf=None):
    r = {"name": name, "kind": "calc", "op": op, "from": frm}
    if xf:
        r["xf"] = xf
    return r


RECORDS = [
    {"name": "cutscene_fall_step", "kind": "data", "va": 0x416BB0, "type": "f64", "xf": [["fix16"]]},
    {"name": "cutscene_throw_vy", "kind": "data", "va": 0x416B44, "type": "f32", "xf": [["fix16"]]},
    const("cutscene_k0", 0),
    const("cutscene_flags_vis", 1),
    const("cutscene_flags_frozen", 8),
    const("cutscene_flags_vis_frozen", 9),
    const("cutscene_flags_frozen_mirror", 12),
    {"name": "cutscene_head_y", "kind": "imm", "va": 0x4036C6, "off": 1, "type": "u32"},
    calc("cutscene_rh_neg", "sum", ["bldg.row_height_other"], [["neg"]]),
    calc("cutscene_ra_neg", "sum", ["bldg.row_height_upper_a"], [["neg"]]),
    calc("cutscene_row0_top", "sum", ["bldg.BUILDING_BASE_TILE", "cutscene_rh_neg"], [["mul", 8]]),
    calc("cutscene_row1_top", "sum", ["bldg.BUILDING_BASE_TILE", "cutscene_rh_neg", "cutscene_rh_neg"], [["mul", 8]]),
    calc("cutscene_row3_top", "sum", ["bldg.BUILDING_BASE_TILE", "cutscene_rh_neg", "cutscene_rh_neg",
                                      "cutscene_ra_neg", "cutscene_rh_neg"], [["mul", 8]]),
    calc("cutscene_door_y", "sum", ["cutscene_row0_top", "cutscene.door_dy"]),
    calc("cutscene_shake_last", "sum", ["cutscene.cut0_shake_mod"], [["sub", 1]]),
    calc("cutscene_push_back", "sum", ["cutscene.cut0_push_dx"], [["neg"]]),
    calc("cutscene_crane1_back", "sum", ["cutscene.intro_crane1_dx"], [["neg"]]),
    calc("cutscene_pan_back", "sum", ["cutscene.cut2_pan_dx"], [["neg"]]),
    calc("cutscene_walk_back", "sum", ["cutscene.cut2_walk_dx"], [["neg"]]),
    calc("cutscene_felix_stop_next", "sum", ["cutscene.cut2_felix_stop"], [["add", 1]]),
    calc("cutscene_climb_top_next", "sum", ["cutscene.cut2_climb_top"], [["add", 1]]),
    calc("cutscene_open_wait3_end", "sum", ["cutscene.open_wait3"], [["add", 1]]),
    calc("cutscene_carry_scroll_back", "sum", ["cutscene.carry_scroll_dx"], [["neg"]]),
    {"name": "cutscene_cut3_center_x", "kind": "imm", "va": 0x40263D, "off": 2, "type": "i8"},
    {"name": "cutscene_cut3_dx", "kind": "imm", "va": 0x40263A, "off": 2, "type": "i8", "xf": [["neg"]]},
    calc("cutscene_head_anim1", "sum", ["cutscene.intro_head_anim0"], [["add", 1]]),
    calc("cutscene_head_anim2", "sum", ["cutscene.intro_head_anim0"], [["add", 2]]),
    calc("cutscene_head_anim3", "sum", ["cutscene.intro_head_anim0"], [["add", 3]]),
    calc("cutscene_head_anim4", "sum", ["cutscene.intro_head_anim0"], [["add", 4]]),
    calc("cutscene_head_x1", "sum", ["cutscene.intro_head_x0", "cutscene.intro_head_dx"]),
    calc("cutscene_head_x2", "sum", ["cutscene_head_x1", "cutscene.intro_head_dx"]),
    calc("cutscene_head_x3", "sum", ["cutscene_head_x2", "cutscene.intro_head_dx"]),
    calc("cutscene_head_x4", "sum", ["cutscene_head_x3", "cutscene.intro_head_dx"]),
    calc("cutscene_nice_anim1", "sum", ["cutscene.end_nice_anim0"], [["add", 1]]),
    calc("cutscene_nice_anim2", "sum", ["cutscene.end_nice_anim0"], [["add", 2]]),
    calc("cutscene_nice_anim3", "sum", ["cutscene.end_nice_anim0"], [["add", 3]]),
    calc("cutscene_nice_anim4", "sum", ["cutscene.end_nice_anim0"], [["add", 4]]),
]
