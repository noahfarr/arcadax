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

    removal_slots, removal_count, max_removal = pad_mask(np.asarray(env._clean_removal_mask))
    keep += [removal_slots, removal_count]

    palette_mask = const(env._palette_mask, np.uint8)
    canvas_slot = const(env._canvas_slot)
    answer_slot = const(env._answer_slot)
    marker_slot = const(env._marker_slot)
    level_has_arrow = const(env._level_has_arrow, np.uint8)

    basket_pixels = const(env._basket_pixels, np.int8)
    basket_is15 = const(env._basket_is15, np.uint8)
    basket_h = const(env._basket_h)
    basket_w = const(env._basket_w)
    arrow_pixels = const(env._arrow_pixels, np.int8)
    arrow_is15 = const(env._arrow_is15, np.uint8)
    arrow_h = const(env._arrow_h)
    arrow_w = const(env._arrow_w)

    st = dict(
        num_levels=num_levels, num_slots=num_slots,
        basket_slot=int(env._basket_slot), arrow_slot=int(env._arrow_slot),
        ph=ph, pw=pw,
        max_removal=max_removal,
        removal_slots=p32(removal_slots), removal_count=p32(removal_count),
        palette_mask=pu8(palette_mask),
        canvas_slot=p32(canvas_slot), answer_slot=p32(answer_slot), marker_slot=p32(marker_slot),
        level_has_arrow=pu8(level_has_arrow),
        basket_pixels=p8(basket_pixels), basket_is15=pu8(basket_is15),
        basket_h=p32(basket_h), basket_w=p32(basket_w),
        arrow_pixels=p8(arrow_pixels), arrow_is15=pu8(arrow_is15),
        arrow_h=p32(arrow_h), arrow_w=p32(arrow_w),
    )
    return st, keep
