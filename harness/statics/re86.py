import numpy as np
from ._util import p32, p8

def build(env, d):
    data = env.data
    L, S = data.num_levels, data.num_slots
    is_reshape = np.ascontiguousarray(np.asarray(env._is_reshape, np.int8))
    is_solid_center = np.ascontiguousarray(np.asarray(env._is_solid_center, np.int8))
    is_wall = np.asarray(env._is_wall, bool)
    is_flood_target = np.asarray(env._is_flood_target, bool)
    is_win_target = np.asarray(env._is_win_target, bool)
    cursor_slot_by_rank = np.ascontiguousarray(np.asarray(env._cursor_slot_by_rank, np.int32))
    num_cursors = np.ascontiguousarray(np.asarray(env._num_cursors, np.int32))
    cursor_rank = np.ascontiguousarray(np.asarray(env._cursor_rank, np.int32))
    budget = np.ascontiguousarray(np.asarray(env._budget, np.int32))
    canvas_template = np.ascontiguousarray(np.asarray(env._canvas_template, np.int8))

    wall_lists = [[i for i in range(S) if is_wall[level, i]] for level in range(L)]
    max_walls = max((len(w) for w in wall_lists), default=0) or 1
    wall_slots = np.full((L, max_walls), -1, np.int32)
    num_walls = np.zeros(L, np.int32)
    for level in range(L):
        num_walls[level] = len(wall_lists[level])
        for k, i in enumerate(wall_lists[level]):
            wall_slots[level, k] = i

    ft_lists = [[i for i in range(S) if is_flood_target[level, i]] for level in range(L)]
    max_ft = max((len(f) for f in ft_lists), default=0) or 1
    flood_target_slots = np.full((L, max_ft), -1, np.int32)
    num_flood_targets = np.zeros(L, np.int32)
    for level in range(L):
        num_flood_targets[level] = len(ft_lists[level])
        for k, i in enumerate(ft_lists[level]):
            flood_target_slots[level, k] = i

    win_target_slot = np.array([int(np.argmax(is_win_target[level])) for level in range(L)], np.int32)
    max_cursors = cursor_slot_by_rank.shape[1]

    st = dict(
        num_levels=L, num_slots=S, max_cursors=max_cursors, max_walls=max_walls,
        max_flood_targets=max_ft,
        is_reshape=p8(is_reshape), is_solid_center=p8(is_solid_center),
        num_cursors=p32(num_cursors), cursor_slot_by_rank=p32(cursor_slot_by_rank),
        cursor_rank=p32(cursor_rank), num_walls=p32(num_walls), wall_slots=p32(wall_slots),
        num_flood_targets=p32(num_flood_targets), flood_target_slots=p32(flood_target_slots),
        win_target_slot=p32(win_target_slot), budget=p32(budget),
        canvas_template=p8(canvas_template),
    )
    keepalive = (is_reshape, is_solid_center, num_cursors, cursor_slot_by_rank, cursor_rank,
                num_walls, wall_slots, num_flood_targets, flood_target_slots, win_target_slot,
                budget, canvas_template)
    return st, keepalive
