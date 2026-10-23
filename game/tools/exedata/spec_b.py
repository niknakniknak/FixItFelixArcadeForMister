# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
SCALARS = {
    "felix_left_limit": ("felix.LEFT_LIMIT", None),
    "felix_right_limit": ("felix.RIGHT_LIMIT", None),
    "felix_anim_stand": ("felix.ANIM_STAND", None),
    "felix_anim_walk": ("felix.ANIM_WALK", None),
    "felix_anim_hammer": ("felix.ANIM_HAMMER", None),
    "felix_anim_air": ("felix.ANIM_AIR", None),
    "felix_anim_land": ("felix.ANIM_LAND", None),
    "felix_anim_hit": ("felix.ANIM_HIT", None),
    "felix_anim_death": ("felix.ANIM_DEATH", None),
    "felix_death_freeze_frame": ("felix.DEATH_FREEZE_FRAME", None),
    "felix_jump_t128": ("felix.JUMP_T128", None),
    "felix_super_frames": ("felix.super_frames0", None),
    "felix_level_end_timer": ("felix.level_end_timer", None),
    "felix_obstacle_sill_dy": ("felix.obstacle_sill_dy", None),
    "felix_die_fall_dy": ("felix.die_fall_dy", None),
    "felix_die_wait": ("felix.die_wait", None),
    "felix_die_last_wait": ("felix.die_last_wait", None),
    "felix_flight_p12": ("felix.flight_p12", None),
    "felix_flight_m12": ("felix.flight_m12", None),
    "felix_flight_p9": ("felix.flight_p9", None),
    "felix_flight_m9": ("felix.flight_m9", None),
    "felix_flight_0": ("felix.flight_0", None),
    "felix_super_cycle_mask": ("felix.super_cycle_mask", None),
    "felix_super_cycle_step": ("felix.super_cycle_step", None),
    "felix_super_cycle_mod": ("felix.super_cycle_mod", None),
    "felix_super_odd_mask": ("felix.super_odd_mask", None),
}


def _fix(name, rec, section):
    return {"name": name, "section": section, "type": "int32_t", "fields": ["s32"], "dims": [], "parts": [{"rec": rec}]}


TABLES = [_fix(f"exe_fixed_fix_{n}", f"fixed.FIX_{n}", "fixed")
          for n in ("0_1", "0_2", "0_33", "0_4", "0_5", "1_3", "1_9")]
TABLES += [_fix(f"exe_felix_{n}", f"felix.{r}", "BANK2") for n, r in (
    ("start_x_fix", "START_X_fix"), ("walk_in_x_fix", "WALK_IN_X_fix"), ("min_x_fix", "MIN_X_fix"),
    ("max_x_fix", "MAX_X_fix"), ("head_room_fix", "HEAD_ROOM_fix"), ("hit_height_fix", "hit_height_fix"),
    ("left_limit_fix", "LEFT_LIMIT_fix"), ("right_limit_fix", "RIGHT_LIMIT_fix"),
    ("walk_in_speed", "walk_in_speed"), ("walk_in_vy", "walk_in_vy"), ("walk_max", "walk_max"),
    ("walk_min", "walk_min"), ("up_step", "up_step"), ("up_clamp", "up_clamp"), ("fall_max", "fall_max"),
    ("jump_vy", "jump_vy"))]
TABLES += [
    {"name": "exe_step_vx", "section": "BANK2", "type": "int32_t", "fields": ["s32"], "dims": [4],
     "parts": [{"rec": "felix.step_vx"}], "file": "felix.c", "was": "step_vx"},
    {"name": "exe_step_vy", "section": "BANK2", "type": "int32_t", "fields": ["s32"], "dims": [5],
     "parts": [{"rec": "felix.step_vy"}], "file": "felix.c", "was": "step_vy"},
]
