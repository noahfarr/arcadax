import numpy as np
from ._util import p32, pu8

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    L, N = d.num_levels, d.num_slots

    qaagahahj = const(env._qaagahahj)
    frame_children = const(env._frame_children)
    card_slots = const(env._card_slots)
    num_cards = const(env._num_cards)
    card_mask = const(env._card_mask, np.uint8)
    is_frameref = const(env._is_frameref, np.uint8)
    tray_item_slot = const(env._tray_item_slot)
    n_tray = const(env._n_tray)
    bg_mask = const(env._bg_mask, np.uint8)
    zpwrpmkvsv_slot = const(env._zpwrpmkvsv_slot)
    allow_fixed = const(env._allow_fixed, np.uint8)
    tween_table = const(env._tween_table)
    tween_width = tween_table.shape[1]

    st = dict(
        num_levels=L, num_slots=N,
        item_tag=env._item_tag, frame_tag=env._frame_tag, spot_tag=env._spot_tag,
        click_tag=env._click_tag,
        tray_ghost_base=env.TRAY_GHOST_BASE, mrokwhyjs0=env.MROKWHYJS0,
        mrokwhyjs1=env.MROKWHYJS1, mjeqtdqvm=env.MJEQTDQVM, ayaigjtxp=env.AYAIGJTXP,
        ohvavdnio=env.OHVAVDNIO, oyvbxwyug=env.OYVBXWYUG, cursor_base=env.CURSOR_BASE,
        qaagahahj=p32(qaagahahj), frame_children=p32(frame_children),
        card_slots=p32(card_slots), num_cards=p32(num_cards),
        card_mask=pu8(card_mask), is_frameref=pu8(is_frameref),
        tray_item_slot=p32(tray_item_slot), n_tray=p32(n_tray),
        bg_mask=pu8(bg_mask), zpwrpmkvsv_slot=p32(zpwrpmkvsv_slot),
        allow_fixed=pu8(allow_fixed),
        tween_val_min=env._tween_val_min, tween_width=tween_width,
        tween_table=p32(tween_table),
    )
    return st, keep


LEVEL_OVERRIDES = {"patch_shape": (15, 64)}
