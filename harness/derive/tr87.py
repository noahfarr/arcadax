import sys
import numpy as np
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


TAG_CAR = "nxkictbbvzt"


CAR_PITCH = 7


NUM_DIGITS = 7


WIN_ANIMATION_PALETTE = (5, 8, 14, 15, 6, 9, 12, 0)


SUBFRAMES_PER_STAGE = len(WIN_ANIMATION_PALETTE) - 1


MAX_TOP = 8


MAX_BOTTOM = 7


MAX_RULES = 8


MAX_CHAIN = 3


MAX_RESOLVED = 6


MAX_COLOR_TARGETS = 8


MAX_HIGHLIGHTS = 10


MAX_STAGES = MAX_TOP


EXTRACT_KWARGS = {"extra_slots": 2 + MAX_HIGHLIGHTS}


GROUP_LETTERS = "ABC"


def _puzzle_tables(data: GameData) -> dict[str, np.ndarray]:
    source = sorted(reference_dir().glob(f"{data.game_id}-*.py"))[0]
    reference = load_reference_game(source)
    module = sys.modules[type(reference).__module__]
    sprite_catalog = module.sprites

    num_levels, num_slots = data.num_levels, data.num_slots
    ph, pw = data.patch_shape

    top_len = np.zeros(num_levels, np.int32)
    top_slot = np.full((num_levels, MAX_TOP), -1, np.int32)
    bottom_len = np.zeros(num_levels, np.int32)
    bottom_slot = np.full((num_levels, MAX_BOTTOM), -1, np.int32)
    num_rules = np.zeros(num_levels, np.int32)
    rule_left_len = np.zeros((num_levels, MAX_RULES), np.int32)
    rule_left_slot = np.full((num_levels, MAX_RULES, MAX_CHAIN), -1, np.int32)
    rule_right_len = np.zeros((num_levels, MAX_RULES), np.int32)
    rule_right_slot = np.full((num_levels, MAX_RULES, MAX_CHAIN), -1, np.int32)
    alter_rules_level = np.zeros(num_levels, bool)
    tree_translation_level = np.zeros(num_levels, bool)
    double_translation_level = np.zeros(num_levels, bool)
    budget0 = np.zeros(num_levels, np.int32)
    slot_group = np.full((num_levels, num_slots), -1, np.int32)
    initial_digit = np.ones((num_levels, num_slots), np.int32)
    is_car_slot = np.zeros((num_levels, num_slots), bool)
    digit_patch = np.full((num_levels, num_slots, NUM_DIGITS, ph, pw), -1, np.int8)

    for li in range(num_levels):
        clean_car_slot_by_position = {
            (s.x, s.y): i
            for i, s in enumerate(reference._clean_levels[li].get_sprites())
            if TAG_CAR in s.tags
        }

        reference.set_level(li)

        def slot_of(car) -> int:
            return clean_car_slot_by_position[(car.x, car.y)]

        for car in reference.current_level.get_sprites():
            if TAG_CAR not in car.tags:
                continue
            slot = slot_of(car)
            group = GROUP_LETTERS.index(car.name[11])
            digit = int(car.name[12:])
            slot_group[li, slot] = group
            initial_digit[li, slot] = digit
            is_car_slot[li, slot] = True
            for d in range(NUM_DIGITS):
                name = f"{TAG_CAR}{GROUP_LETTERS[group]}{d + 1}"
                patch = sprite_catalog[name].clone().set_rotation(car.rotation).render()
                digit_patch[li, slot, d, : patch.shape[0], : patch.shape[1]] = patch

        top_len[li] = len(reference.zvojhrjxxm)
        for i, car in enumerate(reference.zvojhrjxxm):
            top_slot[li, i] = slot_of(car)
        bottom_len[li] = len(reference.ztgmtnnufb)
        for i, car in enumerate(reference.ztgmtnnufb):
            bottom_slot[li, i] = slot_of(car)

        num_rules[li] = len(reference.cifzvbcuwqe)
        for r, (left, right) in enumerate(reference.cifzvbcuwqe):
            rule_left_len[li, r] = len(left)
            for k, car in enumerate(left):
                rule_left_slot[li, r, k] = slot_of(car)
            rule_right_len[li, r] = len(right)
            for k, car in enumerate(right):
                rule_right_slot[li, r, k] = slot_of(car)

        alter_rules_level[li] = bool(reference.current_level.get_data("alter_rules"))
        tree_translation_level[li] = bool(reference.current_level.get_data("tree_translation"))
        double_translation_level[li] = bool(reference.current_level.get_data("double_translation"))
        budget0[li] = reference.vfpimnmtnta

    cursor_patch_top = np.full((3, ph, pw), -1, np.int8)
    cursor_patch_bottom = np.full((3, ph, pw), -1, np.int8)
    for width_id in range(3):
        base = sprite_catalog[f"qvtymdcqear{width_id + 1}"]
        top_patch = base.clone().render()
        bottom_patch = base.clone().set_rotation(180).render()
        cursor_patch_top[width_id, : top_patch.shape[0], : top_patch.shape[1]] = top_patch
        cursor_patch_bottom[width_id, : bottom_patch.shape[0], : bottom_patch.shape[1]] = (
            bottom_patch
        )

    highlight_patch = np.full((ph, pw), -1, np.int8)
    hp = sprite_catalog["nxkictbbvztedxeenecwqa"].render()
    highlight_patch[: hp.shape[0], : hp.shape[1]] = hp

    return dict(
        top_len=top_len,
        top_slot=top_slot,
        bottom_len=bottom_len,
        bottom_slot=bottom_slot,
        num_rules=num_rules,
        rule_left_len=rule_left_len,
        rule_left_slot=rule_left_slot,
        rule_right_len=rule_right_len,
        rule_right_slot=rule_right_slot,
        alter_rules_level=alter_rules_level,
        tree_translation_level=tree_translation_level,
        double_translation_level=double_translation_level,
        budget0=budget0,
        slot_group=slot_group,
        initial_digit=initial_digit,
        is_car_slot=is_car_slot,
        digit_patch=digit_patch,
        cursor_patch_top=cursor_patch_top,
        cursor_patch_bottom=cursor_patch_bottom,
        highlight_patch=highlight_patch,
    )


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    tables = _puzzle_tables(data)
    self._top_len = np.asarray(tables["top_len"])
    self._top_slot = np.asarray(tables["top_slot"])
    self._bottom_len = np.asarray(tables["bottom_len"])
    self._bottom_slot = np.asarray(tables["bottom_slot"])
    self._num_rules = np.asarray(tables["num_rules"])
    self._rule_left_len = np.asarray(tables["rule_left_len"])
    self._rule_left_slot = np.asarray(tables["rule_left_slot"])
    self._rule_right_len = np.asarray(tables["rule_right_len"])
    self._rule_right_slot = np.asarray(tables["rule_right_slot"])
    self._alter_rules_level = np.asarray(tables["alter_rules_level"])
    self._tree_translation_level = np.asarray(tables["tree_translation_level"])
    self._double_translation_level = np.asarray(tables["double_translation_level"])
    self._budget0 = np.asarray(tables["budget0"])
    self._slot_group = np.asarray(tables["slot_group"])
    self._initial_digit = np.asarray(tables["initial_digit"])
    self._is_car_slot = np.asarray(tables["is_car_slot"])
    self._digit_patch = np.asarray(tables["digit_patch"])
    self._cursor_patch_top = np.asarray(tables["cursor_patch_top"])
    self._cursor_patch_bottom = np.asarray(tables["cursor_patch_bottom"])
    self._highlight_patch = np.asarray(tables["highlight_patch"])
    base = data.num_slots - MAX_HIGHLIGHTS - 2
    self._cursor_top_slot = np.int32(base)
    self._cursor_bottom_slot = np.int32(base + 1)
    self._highlight_base_slot = base + 2
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
