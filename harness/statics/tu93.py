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
    budget = const(env._budget)
    board_xy = const(env._board_xy)
    walkable = const(env._walkable, np.uint8)
    init_rotation = const(env._init_rotation)
    own_color = const(env._own_color, np.int8)
    is_push = const(env._is_push, np.uint8)
    is_drift = const(env._is_drift, np.uint8)
    is_train = const(env._is_train, np.uint8)

    push_slots, push_count, max_push = pad_mask(np.asarray(env._is_push))
    keep += [push_slots, push_count]
    drift_slots, drift_count, max_drift = pad_mask(np.asarray(env._is_drift))
    keep += [drift_slots, drift_count]
    train_slots, train_count, max_train = pad_mask(np.asarray(env._is_train))
    keep += [train_slots, train_count]
    crate_slots, crate_count, max_crate = pad_mask(np.asarray(env._is_crate))
    keep += [crate_slots, crate_count]
    exit_slots, exit_count, max_exit = pad_mask(np.asarray(env._is_exit))
    keep += [exit_slots, exit_count]

    template5 = const(env._template5_rotvar, np.int8)
    template7 = const(env._template7_rotvar, np.int8)
    base3 = const(env._base3_rotvar, np.int8)
    flag_pos3 = const(env._flag_pos3)
    flag_pos5 = const(env._flag_pos5)
    flag_pos7 = const(env._flag_pos7)

    st = dict(
        num_levels=num_levels, num_slots=num_slots, ph=ph, pw=pw,
        max_push=max_push, max_drift=max_drift, max_train=max_train,
        max_crate=max_crate, max_exit=max_exit,
        player_slot=p32(player_slot), budget=p32(budget), board_xy=p32(board_xy),
        walkable=pu8(walkable), init_rotation=p32(init_rotation), own_color=p8(own_color),
        is_push=pu8(is_push), is_drift=pu8(is_drift), is_train=pu8(is_train),
        push_slots=p32(push_slots), push_count=p32(push_count),
        drift_slots=p32(drift_slots), drift_count=p32(drift_count),
        train_slots=p32(train_slots), train_count=p32(train_count),
        crate_slots=p32(crate_slots), crate_count=p32(crate_count),
        exit_slots=p32(exit_slots), exit_count=p32(exit_count),
        template5=p8(template5), template7=p8(template7), base3=p8(base3),
        flag_pos3=p32(flag_pos3), flag_pos5=p32(flag_pos5), flag_pos7=p32(flag_pos7),
    )
    return st, keep
