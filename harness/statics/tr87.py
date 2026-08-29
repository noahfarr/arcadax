import numpy as np
from ._util import p32, p8, pu8

def build(env, d):
    keep = []

    def const(a, dtype):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    top_len = const(env._top_len, np.int32)
    top_slot = const(env._top_slot, np.int32)
    bottom_len = const(env._bottom_len, np.int32)
    bottom_slot = const(env._bottom_slot, np.int32)
    num_rules = const(env._num_rules, np.int32)
    rule_left_len = const(env._rule_left_len, np.int32)
    rule_left_slot = const(env._rule_left_slot, np.int32)
    rule_right_len = const(env._rule_right_len, np.int32)
    rule_right_slot = const(env._rule_right_slot, np.int32)
    alter_rules_level = const(env._alter_rules_level, np.uint8)
    tree_translation_level = const(env._tree_translation_level, np.uint8)
    double_translation_level = const(env._double_translation_level, np.uint8)
    budget0 = const(env._budget0, np.int32)
    slot_group = const(env._slot_group, np.int32)
    initial_digit = const(env._initial_digit, np.int32)
    is_car_slot = const(env._is_car_slot, np.uint8)
    digit_patch = const(env._digit_patch, np.int8)
    cursor_patch_top = const(env._cursor_patch_top, np.int8)
    cursor_patch_bottom = const(env._cursor_patch_bottom, np.int8)
    highlight_patch = const(env._highlight_patch, np.int8)

    ph, pw = d.patch_shape
    st = dict(
        num_levels=d.num_levels, num_slots=d.num_slots, ph=ph, pw=pw,
        cursor_top_slot=int(env._cursor_top_slot),
        cursor_bottom_slot=int(env._cursor_bottom_slot),
        highlight_base_slot=int(env._highlight_base_slot),
        top_len=p32(top_len), top_slot=p32(top_slot),
        bottom_len=p32(bottom_len), bottom_slot=p32(bottom_slot),
        num_rules=p32(num_rules),
        rule_left_len=p32(rule_left_len), rule_left_slot=p32(rule_left_slot),
        rule_right_len=p32(rule_right_len), rule_right_slot=p32(rule_right_slot),
        alter_rules_level=pu8(alter_rules_level),
        tree_translation_level=pu8(tree_translation_level),
        double_translation_level=pu8(double_translation_level),
        budget0=p32(budget0),
        slot_group=p32(slot_group), initial_digit=p32(initial_digit), is_car_slot=pu8(is_car_slot),
        digit_patch=p8(digit_patch),
        cursor_patch_top=p8(cursor_patch_top), cursor_patch_bottom=p8(cursor_patch_bottom),
        highlight_patch=p8(highlight_patch),
    )
    return st, keep
