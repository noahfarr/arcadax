import numpy as np
from ._util import p32, p8, pad_mask, pu8

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    num_levels, num_slots = d.num_levels, d.num_slots
    ph, pw = d.patch_shape

    player_slot = const(env._player_slot)
    htk_slot = const(env._htk_slot)
    htk2_slot = const(env._htk2_slot)
    hint_glow_slot = const(env._hint_glow_slot)

    pitch_x = const(env._pitch_x)
    pitch_y = const(env._pitch_y)
    player_start_x = const(env._player_start_x)
    player_start_y = const(env._player_start_y)

    budget = const(env._budget)
    decrement = const(env._decrement)
    fog = const(env._fog, np.uint8)

    start_shape = const(env._start_shape)
    start_color = const(env._start_color)
    start_rot = const(env._start_rot)

    variant = np.asarray(env._htk_variant, np.int8)
    vh, vw = variant.shape[-2:]
    htk_variant = np.full((6, 4, 4, ph, pw), -1, np.int8)
    htk_variant[:, :, :, :vh, :vw] = variant
    htk_variant = np.ascontiguousarray(htk_variant)
    keep.append(htk_variant)

    tag_code = const(env._tag_code)
    goal_index = const(env._goal_index)

    restorable_slots, restorable_count, max_restorable = pad_mask(np.asarray(env._restorable))
    keep += [restorable_slots, restorable_count]

    goal_slot = const(env._goal_slot)
    marker_slot = const(env._marker_slot)
    ring_slot = const(env._ring_slot)
    frame_slot = const(env._frame_slot)
    goal_is_final = const(env._goal_is_final, np.uint8)
    want_shape = const(env._want_shape)
    want_color = const(env._want_color)
    want_rot = const(env._want_rot)
    goal_x = const(env._goal_x)
    goal_y = const(env._goal_y)
    num_goals = const(env._num_goals)

    patrol_slot = const(env._patrol_slot)
    patrol_area_slot = const(env._patrol_area_slot)
    patrol_start_x = const(env._patrol_start_x)
    patrol_start_y = const(env._patrol_start_y)

    pushable_slots, pushable_count, max_pushable = pad_mask(np.asarray(env._is_pushable))
    keep += [pushable_slots, pushable_count]
    wall_step_dx = const(env._wall_step_dx)
    wall_step_dy = const(env._wall_step_dy)

    st = dict(
        num_levels=num_levels, num_slots=num_slots,
        player_slot=p32(player_slot), htk_slot=p32(htk_slot), htk2_slot=p32(htk2_slot),
        hint_glow_slot=p32(hint_glow_slot), flash_slot=int(env._flash_slot),
        pitch_x=p32(pitch_x), pitch_y=p32(pitch_y),
        player_start_x=p32(player_start_x), player_start_y=p32(player_start_y),
        budget=p32(budget), decrement=p32(decrement), fog=pu8(fog),
        start_shape=p32(start_shape), start_color=p32(start_color), start_rot=p32(start_rot),
        htk_variant=p8(htk_variant),
        tag_code=p32(tag_code), goal_index=p32(goal_index),
        max_restorable=max_restorable, restorable_slots=p32(restorable_slots),
        restorable_count=p32(restorable_count),
        goal_slot=p32(goal_slot), marker_slot=p32(marker_slot), ring_slot=p32(ring_slot),
        frame_slot=p32(frame_slot), goal_is_final=pu8(goal_is_final),
        want_shape=p32(want_shape), want_color=p32(want_color), want_rot=p32(want_rot),
        goal_x=p32(goal_x), goal_y=p32(goal_y), num_goals=p32(num_goals),
        patrol_slot=p32(patrol_slot), patrol_area_slot=p32(patrol_area_slot),
        patrol_start_x=p32(patrol_start_x), patrol_start_y=p32(patrol_start_y),
        max_pushable=max_pushable, pushable_slots=p32(pushable_slots),
        pushable_count=p32(pushable_count),
        wall_step_dx=p32(wall_step_dx), wall_step_dy=p32(wall_step_dy),
        hint_ring_tag=env._hint_ring_tag, goal_frame_tag=env._goal_frame_tag,
    )
    return st, keep


LEVEL_OVERRIDES = {"pixels": 53}
