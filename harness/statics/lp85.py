import ctypes
import numpy as np
from ._util import ptr

def build(env, d):
    data = env.data
    L, N = data.num_levels, data.num_slots
    is_button = np.ascontiguousarray(np.asarray(env._is_button, np.uint8))
    ring_len = np.ascontiguousarray(np.asarray(env._ring_len, np.int32))
    ring_from_x = np.ascontiguousarray(np.asarray(env._ring_from_x, np.int32))
    ring_from_y = np.ascontiguousarray(np.asarray(env._ring_from_y, np.int32))
    ring_to_x = np.ascontiguousarray(np.asarray(env._ring_to_x, np.int32))
    ring_to_y = np.ascontiguousarray(np.asarray(env._ring_to_y, np.int32))
    budget = np.ascontiguousarray(np.asarray(env._budget, np.int32))
    max_ring = int(env._max_ring)

    alive = np.asarray(env._levels.alive)
    xs = np.asarray(env._levels.x)
    ys = np.asarray(env._levels.y)
    tags = np.asarray(env._levels.tags)

    button_lists = []
    for li in range(L):
        button_lists.append([si for si in range(N) if is_button[li, si]])
    max_buttons = max(1, max(len(b) for b in button_lists))
    button_slots = np.full((L, max_buttons), -1, np.int32)
    num_buttons = np.zeros(L, np.int32)
    for li, lst in enumerate(button_lists):
        num_buttons[li] = len(lst)
        for k, si in enumerate(lst):
            button_slots[li, k] = si

    cand_lists = []
    for li in range(L):
        cells = set()
        for si in button_lists[li]:
            rl = int(ring_len[li, si])
            for k in range(rl):
                cells.add((int(ring_from_x[li, si, k]), int(ring_from_y[li, si, k])))
                cells.add((int(ring_to_x[li, si, k]), int(ring_to_y[li, si, k])))
        lst = [si for si in range(N) if alive[li, si] and (int(xs[li, si]), int(ys[li, si])) in cells]
        cand_lists.append(lst)
    max_candidates = max(1, max(len(c) for c in cand_lists))
    ring_candidates = np.full((L, max_candidates), -1, np.int32)
    num_candidates = np.zeros(L, np.int32)
    for li, lst in enumerate(cand_lists):
        num_candidates[li] = len(lst)
        for k, si in enumerate(lst):
            ring_candidates[li, k] = si

    piece_a_lists = [[si for si in range(N) if alive[li, si] and tags[li, si, env._piece_a]] for li in range(L)]
    piece_b_lists = [[si for si in range(N) if alive[li, si] and tags[li, si, env._piece_b]] for li in range(L)]
    max_pieces = max(1, max(len(p) for p in piece_a_lists + piece_b_lists))
    piece_a_slots = np.full((L, max_pieces), -1, np.int32)
    num_piece_a = np.zeros(L, np.int32)
    piece_b_slots = np.full((L, max_pieces), -1, np.int32)
    num_piece_b = np.zeros(L, np.int32)
    for li in range(L):
        num_piece_a[li] = len(piece_a_lists[li])
        for k, si in enumerate(piece_a_lists[li]):
            piece_a_slots[li, k] = si
        num_piece_b[li] = len(piece_b_lists[li])
        for k, si in enumerate(piece_b_lists[li]):
            piece_b_slots[li, k] = si

    keepalive = dict(
        is_button=is_button, ring_len=ring_len, ring_from_x=ring_from_x,
        ring_from_y=ring_from_y, ring_to_x=ring_to_x, ring_to_y=ring_to_y,
        budget=budget, button_slots=button_slots, num_buttons=num_buttons,
        ring_candidates=ring_candidates, num_candidates=num_candidates,
        piece_a_slots=piece_a_slots, num_piece_a=num_piece_a,
        piece_b_slots=piece_b_slots, num_piece_b=num_piece_b,
    )

    st = dict(
        num_levels=L, num_slots=N, max_ring=max_ring, max_buttons=max_buttons,
        max_candidates=max_candidates, max_pieces=max_pieces,
        piece_a=int(env._piece_a), piece_b=int(env._piece_b),
        goal_a=int(env._goal_a), goal_b=int(env._goal_b),
        is_button=ptr(is_button, ctypes.c_uint8),
        ring_len=ptr(ring_len, ctypes.c_int32),
        ring_from_x=ptr(ring_from_x, ctypes.c_int32),
        ring_from_y=ptr(ring_from_y, ctypes.c_int32),
        ring_to_x=ptr(ring_to_x, ctypes.c_int32),
        ring_to_y=ptr(ring_to_y, ctypes.c_int32),
        budget=ptr(budget, ctypes.c_int32),
        button_slots=ptr(button_slots, ctypes.c_int32),
        num_buttons=ptr(num_buttons, ctypes.c_int32),
        ring_candidates=ptr(ring_candidates, ctypes.c_int32),
        num_candidates=ptr(num_candidates, ctypes.c_int32),
        piece_a_slots=ptr(piece_a_slots, ctypes.c_int32),
        num_piece_a=ptr(num_piece_a, ctypes.c_int32),
        piece_b_slots=ptr(piece_b_slots, ctypes.c_int32),
        num_piece_b=ptr(num_piece_b, ctypes.c_int32),
    )
    return st, keepalive, button_lists, cand_lists, piece_a_lists, piece_b_lists
