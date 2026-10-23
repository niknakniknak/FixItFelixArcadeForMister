# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
def c(name):
    return {"rec": name if name.startswith("cutscene_") else "cutscene." + name}


SCALAR_RECS = {
    "cut_row1_top": "cutscene_row1_top", "cut_row3_top": "cutscene_row3_top", "cut_pan_x": "cut2_pan_x", "cut_climb_dy2": "cut2_climb_dy2", "cut_smash_mask": "smash_mask",
    "cut_smash_row_mod": "smash_row_mod", "cut_smash_row_half": "smash_row_half",
    "cut_smash_avoid_mask": "smash_avoid_mask", "cut_smash_slot_mod": "smash_slot_mod",
    "cut_dust_step": "dust_step", "cut_dust_x0": "cut1_dust_x0", "cut_dust_row0_x0": "dust_row0_x0",
    "cut_dust_dy": "dust_dy", "cut_door_y": "cutscene_door_y", "cut_a4_start": "intro_dozer_x",
    "cut_ralph_y": "cut2_start_y", "cut_enter_anim": "cut2_start_anim", "cut2_start_x": "cut2_start_x",
    "cut_shake_period": "cut0_shake_period", "cut_shake_last": "cutscene_shake_last",
    "cut0_ralph_stop_x": "cut0_ralph_stop_x", "cut0_ralph_anim_lift": "cut0_ralph_anim_lift",
    "cut0_ralph_anim_carry": "cut0_ralph_anim_carry", "cut0_ralph_exit_y": "cut0_ralph_exit_y",
    "cut0_stump_anim": "cut0_stump_anim", "cut0_dozer_dx": "cut0_dozer_dx", "cut0_dozer_wrap": "cut0_dozer_wrap",
    "cut0_scenery_anim": "cut0_scenery_anim", "cut0_dozer_x2": "cut0_dozer_x2", "cut0_stump_x2": "cut0_stump_x2",
    "cut0_push_x": "cut0_push_x", "cut0_push_back": "cutscene_push_back", "cut0_sink_x": "cut0_sink_x",
    "cut0_dozer_turn": "cut0_dozer_turn", "cut0_dozer_out": "cut0_dozer_out", "cut0_wait4": "cut0_wait4",
    "cut0_stump_anim2": "cut0_stump_anim2", "cut0_bubble_frame": "cut0_bubble_frame", "cut0_wait6": "cut0_wait6",
    "cut1_hop_low_x": "cut1_hop_low_x", "cut1_hop_low_dy": "cut1_hop_low_dy", "cut1_hop_mod": "cut1_hop_mod",
    "cut1_head_behind_x": "cut1_head_behind_x", "cut_head_dx": "intro_head_dx",
    "cut1_hop_tick_mask": "cut1_hop_tick_mask", "cut1_row_wait": "cut1_row_wait",
    "cut1_rows_done": "cut1_rows_done", "cut1_dust_anim": "cut1_dust_anim",
    "cut1_crane_slow_x": "cut1_crane_slow_x", "cut1_door_open_x": "cut1_door_open_x",
    "cut1_door_anim_open": "cut1_door_anim_open", "cut1_heads_end": "cut1_heads_end",
    "cut1_door_anim_close": "cut1_door_anim_close", "cut_crane1_back": "cutscene_crane1_back",
    "cut1_crane2_x": "cut1_crane2_x", "cut1_crane3_x": "cut1_crane3_x", "cut1_door_wait": "cut1_door_wait",
    "cut2_pause": "cut2_pause", "cut2_scroll_dx": "cut2_scroll_dx", "cut2_scroll_max": "cut2_scroll_max",
    "cut2_walkin_x": "cut2_walkin_x", "cut2_anim_stop": "cut2_anim_stop", "cut2_wait_bubble": "cut2_wait_bubble",
    "cut2_bubble_anim": "cut2_bubble_anim", "cut2_anim_stand": "cut2_anim_stand", "cut2_walk_dx": "cut2_walk_dx",
    "cut2_walk_right_x": "cut2_walk_right_x", "cut_pan_back": "cutscene_pan_back",
    "cut2_anim_walk2": "cut2_anim_walk2", "cut2_smash_min_x": "cut2_smash_min_x",
    "cut_smash_cut2": "smash_threshold_cut2", "cut2_heads_anim0": "cut2_heads_anim0", "cut2_head_dx": "head_xy_dx", "cut2_head_dy": "head_xy_dy",
    "cut2_rampage_wait": "cut2_rampage_wait", "cut2_rampage_hits": "cut2_rampage_hits", "cut2_wait14": "cut2_wait14",
    "cut2_felix_scroll": "cut2_felix_scroll", "cut2_felix_x": "cut2_felix_x", "cut2_felix_anim": "cut2_felix_anim",
    "cut2_wait13": "cut2_wait13", "cut2_bubble89_anim": "cut2_bubble89_anim", "cut2_bubble89_x": "cut2_bubble89_x",
    "cut2_bubble89_dy": "cut2_bubble89_dy", "cut2_felix_dx": "cut2_felix_dx",
    "cut_felix_stop_next": "cutscene_felix_stop_next", "cut2_felix_stop": "cut2_felix_stop",
    "cut2_felix_anim16": "cut2_felix_anim16", "cut2_wait16": "cut2_wait16", "cut2_felix_anim9": "cut2_felix_anim9",
    "cut2_exit_x": "cut2_exit_x", "cut2_wait1": "cut2_wait1", "cut2_anim_walk": "cut2_anim_walk",
    "cut_walk_back": "cutscene_walk_back", "cut2_left_x": "cut2_left_x", "cut2_right_x": "cut2_right_x",
    "cut2_anim_climb": "cut2_anim_climb", "cut2_climb_y0": "cut2_climb_y0", "cut2_climb_top": "cut2_climb_top",
    "cut2_climb_wait_top": "cut2_climb_wait_top", "cut2_anim_top": "cut2_anim_top",
    "cut2_climb_wait": "cut2_climb_wait", "cut2_climb_dy": "cut2_climb_dy", "cut_climb_top_next": "cutscene_climb_top_next",
    "cut2_hit_row": "cut2_hit_row", "cut2_hit_slot": "cut2_hit_slot", "cut2_head_anim47": "cut2_head_anim47",
    "cut2_head47_x": "cut2_head47_x", "cut2_head47_y": "cut2_head47_y", "cut2_anim_smash": "cut2_anim_smash",
    "cut2_impact_frame": "cut2_impact_frame", "cut2_head88_anim": "cut2_head88_anim", "cut2_head88_x": "cut2_head88_x",
    "cut2_head88_y": "cut2_head88_y", "cut2_head88_dx": "cut2_head88_dx", "cut2_center_x": "cut2_center_x",
    "cut2_anim_rampage": "cut2_anim_rampage", "cut_cut3_dx": "cutscene_cut3_dx", "cut_cut3_center_x": "cutscene_cut3_center_x",
    "cut_smash_cut3": "smash_threshold_cut3", "cut3_anim_done": "cut3_anim_done", "cut_scen_y": "intro_scenery_y",
    "cut_caption_blink_mask": "caption_blink_mask", "cut_caption_blink_on": "caption_blink_on",
    "cut_caption_col": "caption_col", "cut_caption_row": "caption_row", "cut_nice_vis_x": "render_nice_vis_x",
    "cut_phase2_col": "phase2_col", "cut_phase2_level_row": "phase2_level_row",
    "cut_phase2_complete_row": "phase2_complete_row", "cut_phase1_scroll_y": "phase1_scroll_y",
    "cut_phase1_felix_anim": "phase1_felix_anim", "cut_phase1_ralph_x": "phase1_ralph_x",
    "cut_phase1_ralph_dy": "phase1_ralph_dy", "cut_phase1_felix_x": "phase1_felix_x",
    "cut_phase1_ralph_anim": "phase1_ralph_anim", "cut_open_felix_anim5": "open_felix_anim5",
    "cut_open_step_wait": "open_step_wait", "cut_open_step_dx": "open_step_dx", "cut_end_nice_x1": "end_nice_x1",
    "cut_open_felix_anim3": "open_felix_anim3", "cut_cloud_wait": "cloud_wait", "cut_cloud_dx": "cloud_dx",
    "cut_open_felix_anim18": "open_felix_anim18", "cut_open_wait3_end": "cutscene_open_wait3_end",
    "cut_open_wait3": "open_wait3", "cut_open_nice1_anim62": "open_nice1_anim62",
    "cut_open_felix_anim17": "open_felix_anim17", "cut_open_felix_anim6": "open_felix_anim6",
    "cut_open_nice1_anim51": "open_nice1_anim51", "cut_open_nice0_anim63": "open_nice0_anim63",
    "cut_open_medal_frame": "open_medal_frame", "cut_open_felix_anim7": "open_felix_anim7",
    "cut_open_wait5": "open_wait5", "cut_open_carry_anim0": "open_carry_anim0", "cut_carry_wait": "carry_wait",
    "cut_carry_sub": "carry_sub", "cut_carry_steps": "carry_steps", "cut_carry_lift_x": "carry_lift_x",
    "cut_carry_dx": "carry_dx", "cut_carry_end_x": "carry_end_x", "cut_carry_ralph_x": "carry_ralph_x",
    "cut_carry_ralph_anim": "carry_ralph_anim", "cut_carry_ralph_dx": "carry_ralph_dx",
    "cut_carry_scroll_back": "cutscene_carry_scroll_back", "cut_throw_x": "throw_x", "cut_throw_vx": "ralph_throw_vx", "cut_throw_end_x": "ralph_throw_end_x",
    "cut_fall_y0": "ralph_fall_y0", "cut_fall_scroll_x": "fall_scroll_x", "cut_fall_anim": "fall_ralph_anim31",
    "cut_land_anim": "land_ralph_anim32", "cut_land_wait": "land_wait",
    "cut_end_felix_x": "end_felix_x", "cut_end_felix_y": "end_felix_y", "cut_end_felix_anim": "end_felix_anim",
    "cut_end_scroll_x": "end_scroll_x", "cut_end_scroll_y": "end_scroll_y", "cut_phase0_wait": "phase0_wait",
    "cut_fixed_colour_mask": "fixed_colour_mask", "cut_fixed_colour_max": "fixed_colour_max",
    "cut_phase2_wait": "phase2_wait", "cut_kill_level": "kill_level", "cut_fixed_col": "fixed_col",
    "cut_fixed_row": "fixed_row", "cut_fixed_colour_base": "fixed_colour_base",
}
SCALARS = {name: c(rec) for name, rec in SCALAR_RECS.items()}

K0, VIS, FROZ, VISFROZ, FROZMIR = "cutscene_k0", "cutscene_flags_vis", "cutscene_flags_frozen", "cutscene_flags_vis_frozen", "cutscene_flags_frozen_mirror"
INTRO = [
    ("intro_scenery_anim", VIS, K0, "intro_scenery_y"),
    ("intro_stump_anim", VIS, "intro_stump_x", "intro_ralph_y"),
    ("intro_bubble_anim", K0, K0, K0),
    ("intro_crane_anim", FROZ, "intro_stump_x", "intro_crane_y"),
    ("intro_crane_anim", FROZ, "crane1_x", "intro_crane_y"),
    ("intro_crane_anim", FROZMIR, "intro_stump_x", "intro_crane_y"),
    ("intro_crane_anim", FROZMIR, "crane3_x", "intro_crane_y"),
    ("intro_door_anim", K0, "door_x", K0),
    ("intro_dozer_anim", K0, "intro_dozer_x", "intro_dozer_y"),
    ("intro_felix_anim", K0, K0, "cut2_felix_y"),
    ("intro_hammer_anim", K0, "cut2_hammer_x", "cut2_felix_y"),
    ("intro_ralph_anim", VIS, "intro_ralph_x", "intro_ralph_y"),
] + [("intro_dust_anim", K0, ("dust_x", i), K0) for i in range(4)] + [
    ("intro_head_anim0", K0, "intro_head_x0", "cutscene_head_y")] + [
    (f"cutscene_head_anim{i}", K0, f"cutscene_head_x{i}", "cutscene_head_y") for i in range(1, 5)]
END = [
    ("end_hatch_anim", VISFROZ, "end_hatch_x", K0),
    ("end_medal_anim", FROZ, "end_medal_x", "end_medal_y"),
    ("end_cloud_anim", VIS, "end_cloud0_x", "end_cloud0_y"),
    ("end_cloud_anim", VIS, "end_cloud1_x", "end_cloud1_y"),
    ("end_nice_anim0", K0, "end_nice_x0", "end_nice_y"),
] + [(f"cutscene_nice_anim{i}", K0, "end_nice_x0", "end_nice_y") for i in range(1, 5)]


def parts(rows):
    out = []
    for row in rows:
        for item in row:
            if isinstance(item, tuple):
                out.append(dict(c(item[0]), idx=item[1]))
            else:
                out.append(c(item))
    return out


def table(name, fields, dims, prts, was, typ=None):
    t = {"name": name, "section": "BANK4", "fields": fields, "dims": dims, "parts": prts, "file": "cutscene.c", "was": was}
    if typ:
        t["type"] = typ
    return t


def text(name, rec, width):
    return {"name": name, "section": "BANK4", "type": "char", "fields": ["u8"], "dims": [width],
            "parts": [dict(c(rec), str=width)]}


TABLES = [
    table("exe_intro_objs", ["u8", "u8", "s16", "s16"], [21], parts(INTRO), "intro_objs", "cinit_t"),
    table("exe_cut_hit_rows", ["u8"], [4], [c("hit_rows")], "hit_rows"),
    table("exe_cut_hit_slots", ["u8"], [4], [c("hit_slots")], "hit_slots"),
    table("exe_cut_shake_y", ["u8"], [3], [c("shake_y")], "shake_y"),
    table("exe_cut_hop_y", ["s16"], [6], [c("hop_y")], "hop_y"),
    table("exe_end_objs", ["u8", "u8", "s16", "s16"], [9], parts(END), "end_objs", "cinit_t"),
    table("exe_cut_spread_out", ["u8"], [5], parts([[K0, "spread_1", "spread_2", "spread_3", "spread_4"]]), "spread_out"),
    table("exe_cut_carry", ["u8"], [5], parts([[K0, "carry_1", "carry_2", "carry_3", "carry_4"]]), "carry"),
    {"name": "exe_cut_fall_step", "section": "BANK4", "type": "int32_t", "fields": ["s32"], "dims": [],
     "parts": [c("cutscene_fall_step")]},
    {"name": "exe_cut_throw_vy", "section": "BANK4", "type": "int32_t", "fields": ["s32"], "dims": [],
     "parts": [c("cutscene_throw_vy")]},
    text("exe_cut_caption", "str_caption", 14),
    text("exe_cut_fixed", "str_fixed", 14),
    text("exe_cut_level", "str_level", 7),
    text("exe_cut_complete", "str_complete", 9),
]
