import numpy as np
from ._util import p32, p8, pad_mask

from ..derive.sk48 import TAG_WALL, TAG_RAIL


def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    L, N = d.num_levels, d.num_slots
    ph, pw = d.patch_shape

    boundary_left = const(env._boundary_left)
    boundary_top = const(env._boundary_top)
    boundary_right = const(env._boundary_right)
    boundary_bottom = const(env._boundary_bottom)

    head_slot = const(env._head_slot)
    head_rotation = const(env._head_rotation)
    head_partner = const(env._head_partner)
    head_initial_segments = const(env._head_initial_segments)

    target_color = const(env._target_color)
    marker_count = const(env._marker_count)
    marker_slot = const(env._marker_slot)
    marker_x = const(env._marker_x)
    marker_y = const(env._marker_y)

    block_slots, block_count, max_block = pad_mask(np.asarray(env._block_mask))
    keep += [block_slots, block_count]

    wall_mask = np.asarray(d.alive) & np.asarray(d.tags[:, :, d.tag_index(TAG_WALL)])
    wall_slots, wall_count, max_wall = pad_mask(wall_mask)
    keep += [wall_slots, wall_count]

    removal_slots, removal_count, max_removal = pad_mask(np.asarray(env._clean_removal_mask))
    keep += [removal_slots, removal_count]

    segment_template_pixels = const(env._segment_template_pixels, np.int8)
    segment_template_h = const(env._segment_template_h)
    segment_template_w = const(env._segment_template_w)

    marker_pixels = const(env._marker_pixels, np.int8)
    shadow_template_pixels = const(env._shadow_template_pixels, np.int8)
    shadow_template_h = const(env._shadow_template_h)
    shadow_template_w = const(env._shadow_template_w)

    st = dict(
        num_levels=L, num_slots=N, ph=ph, pw=pw,
        tag_rail=d.tag_index(TAG_RAIL), tag_click=d.tag_index("sys_click"),
        boundary_left=p32(boundary_left), boundary_top=p32(boundary_top),
        boundary_right=p32(boundary_right), boundary_bottom=p32(boundary_bottom),
        head_slot=p32(head_slot), head_rotation=p32(head_rotation),
        head_partner=p32(head_partner), head_initial_segments=p32(head_initial_segments),
        target_color=p32(target_color), marker_count=p32(marker_count),
        marker_slot=p32(marker_slot), marker_x=p32(marker_x), marker_y=p32(marker_y),
        max_block=max_block, block_slots=p32(block_slots), block_count=p32(block_count),
        max_wall=max_wall, wall_slots=p32(wall_slots), wall_count=p32(wall_count),
        max_removal=max_removal, removal_slots=p32(removal_slots), removal_count=p32(removal_count),
        segment_template_pixels=p8(segment_template_pixels),
        segment_template_h=p32(segment_template_h), segment_template_w=p32(segment_template_w),
        marker_pixels=p8(marker_pixels), marker_h=int(env._marker_h), marker_w=int(env._marker_w),
        shadow_template_pixels=p8(shadow_template_pixels),
        shadow_template_h=p32(shadow_template_h), shadow_template_w=p32(shadow_template_w),
    )
    return st, keep
