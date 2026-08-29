import numpy as np
from ._util import p32, pad_mask, pu8

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    num_levels, num_slots = d.num_levels, d.num_slots

    lever_mask = const(env._lever_mask, np.uint8)
    coupler_mask = const(env._coupler_mask, np.uint8)

    grav_x = const(env._grav_x)
    grav_y = const(env._grav_y)
    budget = const(env._budget)
    sensor_adjust = const(env._sensor_adjust)

    lever_src = const(env._lever_src)
    lever_dst = const(env._lever_dst)

    pipe_uses_floor = const(env._pipe_uses_floor, np.uint8)
    pipe_floor_max_front = const(env._pipe_floor_max_front)
    pipe_wall_extreme = const(env._pipe_wall_extreme)
    wall_touch_mask = const(env._wall_touch_mask, np.uint8)

    marker_along = const(env._marker_along)
    marker_wall = const(env._marker_wall)
    sensor_marker_color = const(env._sensor_marker_color, np.uint8)

    level_coupler_slot = const(env._level_coupler_slot)
    icon_dx = const(env._icon_dx)
    icon_dy = const(env._icon_dy)

    pipe_slots, pipe_count, max_pipes = pad_mask(np.asarray(env._pipe_mask))
    keep += [pipe_slots, pipe_count]

    sensor_slots, sensor_count, max_sensors = pad_mask(np.asarray(env._sensor_mask))
    keep += [sensor_slots, sensor_count]

    marker_slots, marker_count, max_markers = pad_mask(np.asarray(env._marker_mask))
    keep += [marker_slots, marker_count]

    st = dict(
        num_levels=num_levels, num_slots=num_slots, icon_base=int(env._icon_base),
        lever_mask=pu8(lever_mask), coupler_mask=pu8(coupler_mask),
        grav_x=p32(grav_x), grav_y=p32(grav_y), budget=p32(budget), sensor_adjust=p32(sensor_adjust),
        lever_src=p32(lever_src), lever_dst=p32(lever_dst),
        pipe_uses_floor=pu8(pipe_uses_floor), pipe_floor_max_front=p32(pipe_floor_max_front),
        pipe_wall_extreme=p32(pipe_wall_extreme), wall_touch_mask=pu8(wall_touch_mask),
        marker_along=p32(marker_along), marker_wall=p32(marker_wall),
        sensor_marker_color=pu8(sensor_marker_color),
        level_coupler_slot=p32(level_coupler_slot), icon_dx=p32(icon_dx), icon_dy=p32(icon_dy),
        max_pipes=max_pipes, pipe_slots=p32(pipe_slots), pipe_count=p32(pipe_count),
        max_sensors=max_sensors, sensor_slots=p32(sensor_slots), sensor_count=p32(sensor_count),
        max_markers=max_markers, marker_slots=p32(marker_slots), marker_count=p32(marker_count),
    )
    return st, keep
