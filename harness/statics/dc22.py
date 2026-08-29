import numpy as np
from ._util import p32, p8, pad_mask, pu8

FRAME_SIZE = 64
NUM_DIRS = 5


def build(env, d):
    d = env.data
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    L, N = d.num_levels, d.num_slots
    num_codes = env._num_codes
    ph, pw = d.patch_shape

    budget = const(env._budget)
    player_slot = const(env._player_slot)
    goal_slot = const(env._goal_slot)
    crane_slot = const(env._crane_slot)
    crane_is_brixto = const(env._crane_is_brixto, np.uint8)
    crane_origin_x = const(env._crane_origin_x)
    crane_origin_y = const(env._crane_origin_y)
    crane_offset_x = const(env._crane_offset_x)
    crane_offset_y = const(env._crane_offset_y)
    grawwq_object_slot = const(env._grawwq_object_slot)
    dir_slot = const(env._dir_slot)
    assert dir_slot.shape == (L, NUM_DIRS)

    vcha_map = const(env._vcha_map, np.uint8)
    assert vcha_map.shape == (L, FRAME_SIZE, FRAME_SIZE)

    is_ignore = const(env._is_ignore, np.uint8)
    is_crzsjq = const(env._is_crzsjq, np.uint8)
    is_vcha = const(env._is_vcha, np.uint8)
    is_tewfut = const(env._is_tewfut, np.uint8)
    is_qiukbrokfa = const(env._is_qiukbrokfa, np.uint8)
    is_iophjflwsn = const(env._is_iophjflwsn, np.uint8)

    code_of = const(env._code_of)
    next_slot = const(env._next_slot)
    next_interaction = const(env._next_interaction)
    name_id = const(env._name_id)
    teleport_name = const(env._teleport_name)

    crane_hold_pixels = const(env._crane_hold_pixels, np.int8)
    assert crane_hold_pixels.shape == (L, ph, pw)

    is_buezna = np.asarray(env._is_buezna, bool)
    is_sys_click = np.asarray(env._is_sys_click, bool)
    is_piyqze = np.asarray(env._is_piyqze, bool)
    is_aybe = np.asarray(env._is_aybe, bool)
    is_njvd = np.asarray(env._is_njvd, bool)
    has_njvd_governance = np.asarray(env._has_njvd_governance, bool)
    is_brixto_candidate = np.asarray(env._is_brixto_candidate, bool)
    code_member = np.asarray(env._code_member, bool)
    assert code_member.shape == (L, num_codes, N)

    buezna_slots, buezna_count, max_buezna = pad_mask(is_buezna)
    keep += [buezna_slots, buezna_count]
    sys_click_slots, sys_click_count, max_sys_click = pad_mask(is_sys_click)
    keep += [sys_click_slots, sys_click_count]
    piyqze_slots, piyqze_count, max_piyqze = pad_mask(is_piyqze)
    keep += [piyqze_slots, piyqze_count]
    aybe_slots, aybe_count, max_aybe = pad_mask(is_aybe)
    keep += [aybe_slots, aybe_count]
    njvd_slots, njvd_count, max_njvd = pad_mask(is_njvd)
    keep += [njvd_slots, njvd_count]
    governed_slots, governed_count, max_governed = pad_mask(has_njvd_governance & is_buezna)
    keep += [governed_slots, governed_count]
    brixto_slots, brixto_count, max_brixto = pad_mask(is_brixto_candidate)
    keep += [brixto_slots, brixto_count]
    code_member_slots, code_member_count, max_code_member = pad_mask(
        code_member.reshape(L * num_codes, N)
    )
    keep += [code_member_slots, code_member_count]
    # dc22_apply_generic_swap / dc22_apply_tewfut_color_cycle snapshot eligibility into a
    # fixed DC22_MAX_FAMILY_SNAPSHOT-sized stack buffer (see dc22.c) to match JAX's
    # semantics of freezing the eligibility mask before mutating; keep that bound sound.
    assert max_code_member <= 256, (
        f"code_member family width {max_code_member} exceeds DC22_MAX_FAMILY_SNAPSHOT in dc22.c"
    )

    st = dict(
        num_levels=L, num_slots=N, num_codes=num_codes, patch_h=ph, patch_w=pw,
        budget=p32(budget), player_slot=p32(player_slot), goal_slot=p32(goal_slot),
        crane_slot=p32(crane_slot), crane_is_brixto=pu8(crane_is_brixto),
        crane_origin_x=p32(crane_origin_x), crane_origin_y=p32(crane_origin_y),
        crane_offset_x=p32(crane_offset_x), crane_offset_y=p32(crane_offset_y),
        grawwq_object_slot=p32(grawwq_object_slot), dir_slot=p32(dir_slot),
        vcha_map=pu8(vcha_map),
        is_ignore=pu8(is_ignore), is_crzsjq=pu8(is_crzsjq), is_vcha=pu8(is_vcha),
        is_tewfut=pu8(is_tewfut), is_qiukbrokfa=pu8(is_qiukbrokfa), is_iophjflwsn=pu8(is_iophjflwsn),
        code_of=p32(code_of), next_slot=p32(next_slot), next_interaction=p32(next_interaction),
        name_id=p32(name_id), teleport_name=p32(teleport_name),
        crane_hold_pixels=p8(crane_hold_pixels),
        max_buezna=max_buezna, buezna_slots=p32(buezna_slots), buezna_count=p32(buezna_count),
        max_sys_click=max_sys_click, sys_click_slots=p32(sys_click_slots), sys_click_count=p32(sys_click_count),
        max_piyqze=max_piyqze, piyqze_slots=p32(piyqze_slots), piyqze_count=p32(piyqze_count),
        max_aybe=max_aybe, aybe_slots=p32(aybe_slots), aybe_count=p32(aybe_count),
        max_njvd=max_njvd, njvd_slots=p32(njvd_slots), njvd_count=p32(njvd_count),
        max_governed=max_governed, governed_slots=p32(governed_slots), governed_count=p32(governed_count),
        max_brixto=max_brixto, brixto_slots=p32(brixto_slots), brixto_count=p32(brixto_count),
        max_code_member=max_code_member, code_member_slots=p32(code_member_slots),
        code_member_count=p32(code_member_count),
    )
    return st, keep


LEVEL_OVERRIDES = {"interaction": 13, "blocking": 6, "synthetic_tags": 1}
