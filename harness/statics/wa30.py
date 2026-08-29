import numpy as np
from ._util import p32, p8, pu8

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    player_slot = const(env._player_slot)
    budget = const(env._budget)
    is_box = const(env._is_box, np.uint8)
    is_seeker = const(env._is_seeker, np.uint8)
    is_thief = const(env._is_thief, np.uint8)
    hole_grid = const(env._hole_grid, np.uint8)
    fsj_grid = const(env._fsj_grid, np.uint8)
    zqx_grid = const(env._zqx_grid, np.uint8)
    player_variants = const(env._player_variants, np.int8)

    st = dict(
        num_levels=d.num_levels, num_slots=d.num_slots,
        player_slot=p32(player_slot), budget=p32(budget),
        is_box=pu8(is_box), is_seeker=pu8(is_seeker), is_thief=pu8(is_thief),
        hole_grid=pu8(hole_grid), fsj_grid=pu8(fsj_grid), zqx_grid=pu8(zqx_grid),
        player_variants=p8(player_variants),
    )
    return st, keep
