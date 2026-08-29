import numpy as np
from ._util import p32, p8, pad_mask, pu8

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    L, N = d.num_levels, d.num_slots
    ph, pw = d.patch_shape

    step_budget = const(env._step_budget)
    first_box = const(env._first_box)

    box_slots, box_count, max_box = pad_mask(np.asarray(env._box_mask))
    keep += [box_slots, box_count]
    marker_slots, marker_count, max_marker = pad_mask(np.asarray(env._marker_mask))
    keep += [marker_slots, marker_count]
    hole_slots, hole_count, max_hole = pad_mask(np.asarray(env._hole_mask))
    keep += [hole_slots, hole_count]
    zone_slots, zone_count, max_zone = pad_mask(np.asarray(env._zone_mask))
    keep += [zone_slots, zone_count]
    bomb_slots, bomb_count, max_bomb = pad_mask(np.asarray(env._bomb_mask))
    keep += [bomb_slots, bomb_count]
    wall_slots, wall_count, max_wall = pad_mask(np.asarray(env._wall_mask))
    keep += [wall_slots, wall_count]
    border_slots, border_count, max_border = pad_mask(np.asarray(env._border_mask))
    keep += [border_slots, border_count]
    occupant_slots, occupant_count, max_occupant = pad_mask(np.asarray(env._occupant_mask))
    keep += [occupant_slots, occupant_count]

    center_dot_mask = const(env._center_dot_mask, np.uint8)
    box_outline_mask = const(env._box_outline_mask, np.uint8)
    box_edge_masks = const(env._box_edge_masks, np.uint8)

    explosion_pixels = const(env._explosion_pixels, np.int8)
    explosion_h = const(env._explosion_h)
    explosion_w = const(env._explosion_w)
    explosion_recoil_dx = const(env._explosion_recoil_dx)
    explosion_recoil_dy = const(env._explosion_recoil_dy)
    explosion_base_slot = const(env._explosion_base_slot)

    fuse_frames = const(env._fuse_frames, np.int8)
    fuse_total_bad = const(env._fuse_total_bad)
    fuse_last_row_bad = const(env._fuse_last_row_bad, np.uint8)

    st = dict(
        num_levels=L, num_slots=N, ph=ph, pw=pw,
        max_bomb_h=env._max_bomb_h, box_tag=d.tag_index("0022vrxelxosfy"),
        collider_slot=env._collider_slot, scratch_slot=env._scratch_slot,
        explosion_base=0, explosion_slots_total=0,

        step_budget=p32(step_budget), first_box=p32(first_box),

        max_box=max_box, box_slots=p32(box_slots), box_count=p32(box_count),
        max_marker=max_marker, marker_slots=p32(marker_slots), marker_count=p32(marker_count),
        max_hole=max_hole, hole_slots=p32(hole_slots), hole_count=p32(hole_count),
        max_zone=max_zone, zone_slots=p32(zone_slots), zone_count=p32(zone_count),
        max_bomb=max_bomb, bomb_slots=p32(bomb_slots), bomb_count=p32(bomb_count),
        max_wall=max_wall, wall_slots=p32(wall_slots), wall_count=p32(wall_count),
        max_border=max_border, border_slots=p32(border_slots), border_count=p32(border_count),
        max_occupant=max_occupant, occupant_slots=p32(occupant_slots),
        occupant_count=p32(occupant_count),

        center_dot_mask=pu8(center_dot_mask), box_outline_mask=pu8(box_outline_mask),
        box_edge_masks=pu8(box_edge_masks),

        explosion_pixels=p8(explosion_pixels), explosion_h=p32(explosion_h),
        explosion_w=p32(explosion_w),
        explosion_recoil_dx=p32(explosion_recoil_dx), explosion_recoil_dy=p32(explosion_recoil_dy),
        explosion_base_slot=p32(explosion_base_slot),

        fuse_frames=p8(fuse_frames), fuse_total_bad=p32(fuse_total_bad),
        fuse_last_row_bad=pu8(fuse_last_row_bad),
    )

    from ..derive.ka59 import (
        COLLIDER_SLOT_OFFSET, EXPLOSION_BASE_OFFSET, EXTRA_SLOTS, MAX_BOMBS, SCRATCH_SLOT_OFFSET,
    )

    original = N - EXTRA_SLOTS
    assert env._collider_slot == original + COLLIDER_SLOT_OFFSET
    assert env._scratch_slot == original + SCRATCH_SLOT_OFFSET
    st["explosion_base"] = original + EXPLOSION_BASE_OFFSET
    st["explosion_slots_total"] = 3 * MAX_BOMBS

    return st, keep
