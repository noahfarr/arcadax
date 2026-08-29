import numpy as np
from ._util import p32, pad_mask, pu8

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    num_levels, num_slots = d.num_levels, d.num_slots

    mover_slot = const(env._mover_slot)

    block_slots, block_count, max_blocks = pad_mask(np.asarray(env._blocks))
    keep += [block_slots, block_count]

    clickable_slots, clickable_count, max_clickable = pad_mask(np.asarray(env._clickable))
    keep += [clickable_slots, clickable_count]

    obstacles = np.asarray(env._walls) | np.asarray(env._blocking_tags) | np.asarray(env._any_door)
    obstacle_slots, obstacle_count, max_obstacles = pad_mask(obstacles)
    keep += [obstacle_slots, obstacle_count]

    doors = np.asarray(env._doors).reshape(num_levels * 3, num_slots)
    door_slots, door_count, max_doors = pad_mask(doors)
    keep += [door_slots, door_count]

    switches = np.asarray(env._switches).reshape(num_levels * 3, num_slots)
    switch_slots, switch_count, max_switches = pad_mask(switches)
    keep += [switch_slots, switch_count]

    hazard_map = const(env._hazard_map, np.uint8)
    background = const(env._background)
    clean_x = const(env._clean_x)
    clean_y = const(env._clean_y)

    st = dict(
        num_levels=num_levels, num_slots=num_slots, click_tag=d.tag_index("sys_click"),
        max_blocks=max_blocks, max_clickable=max_clickable, max_obstacles=max_obstacles,
        max_switches=max_switches, max_doors=max_doors,
        mover_slot=p32(mover_slot),
        block_slots=p32(block_slots), block_count=p32(block_count),
        clickable_slots=p32(clickable_slots), clickable_count=p32(clickable_count),
        obstacle_slots=p32(obstacle_slots), obstacle_count=p32(obstacle_count),
        switch_slots=p32(switch_slots), switch_count=p32(switch_count),
        door_slots=p32(door_slots), door_count=p32(door_count),
        hazard_map=pu8(hazard_map),
        background=p32(background),
        clean_x=p32(clean_x), clean_y=p32(clean_y),
    )
    return st, keep
