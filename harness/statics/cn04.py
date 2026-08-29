import numpy as np
from ._util import p32, p8, pu8

def build(game):
    keep = []

    def const(arr, dtype):
        a = np.ascontiguousarray(np.asarray(arr), dtype)
        keep.append(a)
        return a

    rotvar = const(game._rotvar, np.int8)
    h_table = const(game._h_table, np.int32)
    w_table = const(game._w_table, np.int32)
    init_rot = const(game._init_rot, np.int32)
    group_size = const(game._group_size, np.int32)
    group_rank = const(game._group_rank, np.int32)
    group_members = const(game._group_members, np.int32)
    init_selected = const(game._init_selected, np.int32)
    fwd_row = const(game._fwd_row, np.int32)
    fwd_col = const(game._fwd_col, np.int32)
    fwd_valid = const(game._fwd_valid, np.uint8)
    bwd_row = const(game._bwd_row, np.int32)
    bwd_col = const(game._bwd_col, np.int32)
    bwd_valid = const(game._bwd_valid, np.uint8)
    budget = const(game._budget, np.int32)
    greymask = const(game._greymask, np.uint8)
    level_bg = const(game._level_bg, np.int8)

    ph, pw = game.data.patch_shape
    st = dict(
        num_levels=game.num_levels, num_slots=game.num_slots, ph=ph, pw=pw,
        max_group=group_members.shape[-1],
        rotvar=p8(rotvar),
        h_table=p32(h_table), w_table=p32(w_table),
        init_rot=p32(init_rot),
        group_size=p32(group_size), group_rank=p32(group_rank), group_members=p32(group_members),
        init_selected=p32(init_selected),
        fwd_row=p32(fwd_row), fwd_col=p32(fwd_col), fwd_valid=pu8(fwd_valid),
        bwd_row=p32(bwd_row), bwd_col=p32(bwd_col), bwd_valid=pu8(bwd_valid),
        budget=p32(budget), greymask=pu8(greymask), level_bg=p8(level_bg),
    )
    return st, keep
