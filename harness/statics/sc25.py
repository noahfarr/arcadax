import numpy as np
from ._util import p32, p8, pu8

def build(env, d):
    keep = []

    def a32(a):
        out = np.ascontiguousarray(np.asarray(a), np.int32)
        keep.append(out)
        return out

    def au8(a):
        out = np.ascontiguousarray(np.asarray(a), np.uint8)
        keep.append(out)
        return out

    def a8(a):
        out = np.ascontiguousarray(np.asarray(a), np.int8)
        keep.append(out)
        return out

    allowed = au8(env._allowed)
    budget = a32(env._budget)
    auto_hint = a32(env._auto_hint)
    exydhv = a32(env._exydhv)
    action_ui = a32(env._action_ui)
    pluyoo = a32(env._pluyoo)
    tagsmh = a32(env._tagsmh)
    seofsw_tagsmh = a32(env._seofsw_tagsmh)
    dosorb = a32(env._dosorb)
    seofsw_dosorb = a32(env._seofsw_dosorb)
    acyylh_ovl = a32(env._acyylh_ovl)
    smzaik_ovl = a32(env._smzaik_ovl)

    blockers_np = np.asarray(env._blockers)
    blocker_count = a32((blockers_np >= 0).sum(axis=1))
    blockers = a32(blockers_np)
    max_blockers = blockers_np.shape[1]

    tp_slots_np = np.asarray(env._tp_slots)
    tp_slots = a32(tp_slots_np)
    tp_counts = a32(env._tp_counts)
    max_tp = tp_slots_np.shape[2]

    grid_slot = a32(env._grid_slot)

    click_kind = a32(env._click_kind)
    click_r = a32(env._click_r)
    click_c = a32(env._click_c)
    click_spell = a32(env._click_spell)

    bar_mask_np = np.asarray(env._bar_mask)
    bar_mask = au8(bar_mask_np)
    bar_count = a32(env._bar_count)
    bar_ph = bar_mask_np.shape[1]

    grow_block = au8(env._grow_block)
    fb_hit_slot = a32(env._fb_hit_slot)
    fb_slot_kind = a32(env._fb_slot_kind)

    player_base0 = a8(env._player_base0)
    player_rotation0 = a32(env._player_rotation0)

    fb_base_np = np.asarray(env._fb_base)
    fb_base = a8(fb_base_np)
    fb_base_shape = a32(env._fb_base_shape)
    fb_base_h, fb_base_w = fb_base_np.shape[1], fb_base_np.shape[2]

    st = dict(
        num_levels=d.num_levels, num_slots=d.num_slots, fireball_slot=int(env._fireball_slot),
        max_blockers=max_blockers, max_tp=max_tp, bar_ph=bar_ph,
        fb_base_h=fb_base_h, fb_base_w=fb_base_w,
        allowed=pu8(allowed), budget=p32(budget), auto_hint=p32(auto_hint),
        exydhv=p32(exydhv), action_ui=p32(action_ui), pluyoo=p32(pluyoo),
        tagsmh=p32(tagsmh), seofsw_tagsmh=p32(seofsw_tagsmh),
        dosorb=p32(dosorb), seofsw_dosorb=p32(seofsw_dosorb),
        acyylh_ovl=p32(acyylh_ovl), smzaik_ovl=p32(smzaik_ovl),
        blockers=p32(blockers), blocker_count=p32(blocker_count),
        tp_slots=p32(tp_slots), tp_counts=p32(tp_counts),
        grid_slot=p32(grid_slot),
        click_kind=p32(click_kind), click_r=p32(click_r), click_c=p32(click_c),
        click_spell=p32(click_spell),
        bar_mask=pu8(bar_mask), bar_count=p32(bar_count),
        grow_block=pu8(grow_block), fb_hit_slot=p32(fb_hit_slot), fb_slot_kind=p32(fb_slot_kind),
        player_base0=p8(player_base0), player_rotation0=p32(player_rotation0),
        fb_base=p8(fb_base), fb_base_shape=p32(fb_base_shape),
    )
    return st, keep
