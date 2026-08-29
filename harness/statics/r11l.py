import numpy as np
from ._util import p32, pad_mask, pu8

from ..derive.r11l import G, K


def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    L, N = d.num_levels, d.num_slots

    group_target_slot = const(env._group_target_slot)
    group_is_composite = const(env._group_is_composite, np.uint8)

    gpm = np.asarray(env._group_piece_mask).reshape(L * G, N)
    group_piece_slots, group_piece_count, max_group_pieces = pad_mask(gpm)
    keep += [group_piece_slots, group_piece_count]

    piece_group = const(env._piece_group)
    pieces_order = const(env._pieces_order)
    key_clue_slot = const(env._key_clue_slot)
    key_target_slot = const(env._key_target_slot)
    key_skip_win = const(env._key_skip_win, np.uint8)
    composite_slot = const(env._composite_slot)

    fm = np.asarray(env._fragment_mask)
    fragment_slots, fragment_count, max_fragments = pad_mask(fm)
    keep += [fragment_slots, fragment_count]

    wm = np.asarray(env._wall_mask)
    wall_slots, wall_count, max_walls = pad_mask(wm)
    keep += [wall_slots, wall_count]

    hm = np.asarray(env._hazard_mask)
    hazard_slots, hazard_count, max_hazards = pad_mask(hm)
    keep += [hazard_slots, hazard_count]

    key_colour_set = np.zeros((L, K, 15), np.uint8)
    kcs = np.asarray(env._key_clue_slot)
    pixels = np.asarray(d.pixels)
    for li in range(L):
        for k in range(K):
            slot = int(kcs[li, k])
            if slot < 0:
                continue
            px = pixels[li, slot]
            for c in range(1, 16):
                key_colour_set[li, k, c - 1] = np.any(px == c)
    key_colour_set = const(key_colour_set, np.uint8)

    st = dict(
        num_levels=L, num_slots=N,
        icon_base=int(env._icon_base), wobble_icon_slot=int(env._wobble_icon_slot),
        max_group_pieces=max_group_pieces, max_fragments=max_fragments,
        max_walls=max_walls, max_hazards=max_hazards,
        group_target_slot=p32(group_target_slot), group_is_composite=pu8(group_is_composite),
        group_piece_slots=p32(group_piece_slots), group_piece_count=p32(group_piece_count),
        piece_group=p32(piece_group), pieces_order=p32(pieces_order),
        key_clue_slot=p32(key_clue_slot), key_target_slot=p32(key_target_slot),
        key_skip_win=pu8(key_skip_win), key_colour_set=pu8(key_colour_set),
        composite_slot=p32(composite_slot),
        fragment_slots=p32(fragment_slots), fragment_count=p32(fragment_count),
        wall_slots=p32(wall_slots), wall_count=p32(wall_count),
        hazard_slots=p32(hazard_slots), hazard_count=p32(hazard_count),
    )
    return st, keep
