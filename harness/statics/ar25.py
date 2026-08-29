import numpy as np
from ._util import p32, p8, pu8

def build(env, d):
    keep = []

    def const(a, dtype):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    L = env.num_levels

    steps_budget = const(env._steps_budget, np.int32)
    vmirror_slot = const(env._vmirror_slot, np.int32)
    hmirror_slot = const(env._hmirror_slot, np.int32)
    movable_slot = const(env._movable_slot, np.int32)
    axis_slot = const(env._axis_slot, np.int32)
    cycle_order = const(env._cycle_order, np.int32)
    cycle_count = const(env._cycle_count, np.int32)
    initial_selected = const(env._initial_selected, np.int32)
    target_grid = const(env._target_grid, np.uint8)
    hint_pixels = const(env._hint_pixels, np.int8)

    excl = np.asarray(env._excluded)
    mov = np.asarray(env._movable_slot)
    axs = np.asarray(env._axis_slot)
    vm = np.asarray(env._vmirror_slot)
    hm = np.asarray(env._hmirror_slot)

    excluded_movable = np.zeros((L, 2), np.uint8)
    excluded_axis = np.zeros((L, 2), np.uint8)
    excluded_vmirror = np.zeros((L,), np.uint8)
    excluded_hmirror = np.zeros((L,), np.uint8)
    for li in range(L):
        for k in range(2):
            s = mov[li, k]
            excluded_movable[li, k] = excl[li, max(s, 0)] if s >= 0 else 0
            s2 = axs[li, k]
            excluded_axis[li, k] = excl[li, max(s2, 0)] if s2 >= 0 else 0
        excluded_vmirror[li] = excl[li, max(int(vm[li]), 0)] if vm[li] >= 0 else 0
        excluded_hmirror[li] = excl[li, max(int(hm[li]), 0)] if hm[li] >= 0 else 0
    keep += [excluded_movable, excluded_axis, excluded_vmirror, excluded_hmirror]

    st = dict(
        num_levels=L, num_slots=env.num_slots,
        ghost_slot=env._ghost_slot, hint_slot=env._hint_slot,
        hint_h=env._hint_h, hint_w=env._hint_w, hint_layer=env._hint_layer,
        hint_pixels=p8(hint_pixels),
        steps_budget=p32(steps_budget),
        vmirror_slot=p32(vmirror_slot), hmirror_slot=p32(hmirror_slot),
        movable_slot=p32(movable_slot), axis_slot=p32(axis_slot),
        cycle_order=p32(cycle_order), cycle_count=p32(cycle_count),
        initial_selected=p32(initial_selected),
        excluded_movable=pu8(excluded_movable), excluded_vmirror=pu8(excluded_vmirror),
        excluded_hmirror=pu8(excluded_hmirror), excluded_axis=pu8(excluded_axis),
        target_grid=pu8(target_grid),
    )
    return st, keep
