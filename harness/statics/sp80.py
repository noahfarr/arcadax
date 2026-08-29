import numpy as np
from ._util import p32, pad_mask

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    num_levels, num_slots = d.num_levels, d.num_slots

    budget = const(env._budget)
    rotation = const(env._rotation)
    pick_priority = const(env._pick_priority)

    tags = np.asarray(d.tags)
    alive = np.asarray(d.alive)

    plzw_slots, plzw_count, max_plzw = pad_mask(tags[:, :, env._plzw])
    repw_slots, repw_count, max_repw = pad_mask(tags[:, :, env._repw])
    waoe_slots, waoe_count, max_waoe = pad_mask(tags[:, :, env._waoe])
    tuvk_slots, tuvk_count, max_tuvk = pad_mask(tags[:, :, env._tuvk])
    liolf_seed_slots, liolf_seed_count, max_liolf_seed = pad_mask(tags[:, :, env._liolf] & alive)
    keep += [
        plzw_slots, plzw_count, repw_slots, repw_count, waoe_slots, waoe_count,
        tuvk_slots, tuvk_count, liolf_seed_slots, liolf_seed_count,
    ]

    spout_slot_arr = const(env._spout_slot)
    spout_rel = np.asarray(env._spout_rel)
    spout_dx = const(spout_rel[:, :, 0])
    spout_dy = const(spout_rel[:, :, 1])
    sowl_mask = tags[:, :, env._sowl] & alive
    spout_count = const(sowl_mask.sum(1))
    max_spout = spout_slot_arr.shape[1]

    st = dict(
        num_levels=num_levels, num_slots=num_slots, extra_start=int(env._extra_start),
        liolf_tag=int(env._liolf), plzw_tag=int(env._plzw), repw_tag=int(env._repw),
        tuvk_tag=int(env._tuvk), waoe_tag=int(env._waoe),
        budget=p32(budget), rotation=p32(rotation), pick_priority=p32(pick_priority),
        max_plzw=max_plzw, plzw_slots=p32(plzw_slots), plzw_count=p32(plzw_count),
        max_tuvk=max_tuvk, tuvk_slots=p32(tuvk_slots), tuvk_count=p32(tuvk_count),
        max_repw=max_repw, repw_slots=p32(repw_slots), repw_count=p32(repw_count),
        max_waoe=max_waoe, waoe_slots=p32(waoe_slots), waoe_count=p32(waoe_count),
        max_liolf_seed=max_liolf_seed, liolf_seed_slots=p32(liolf_seed_slots),
        liolf_seed_count=p32(liolf_seed_count),
        max_spout=max_spout, spout_slot=p32(spout_slot_arr), spout_dx=p32(spout_dx),
        spout_dy=p32(spout_dy), spout_count=p32(spout_count),
    )
    return st, keep
