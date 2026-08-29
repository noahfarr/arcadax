import numpy as np
from ._util import p32

from ..derive.ft09 import TAG_CLUE


def build(env, d):
    levels = range(env.num_levels)
    budget = np.ascontiguousarray(env._budget, np.int32)
    palette = np.ascontiguousarray(env._palette, np.int32)
    palette_size = np.ascontiguousarray(env._palette_size, np.int32)
    brush = np.ascontiguousarray(env._brush.reshape(env.num_levels, 9), np.int32)
    hint_slot = np.ascontiguousarray(env._hint_slot, np.int32)

    clue_lists = [d.slots_with_tag(li, TAG_CLUE) for li in levels]
    max_clues = max((len(c) for c in clue_lists), default=0)
    clue_slots = np.full((env.num_levels, max(max_clues, 1)), -1, np.int32)
    clue_count = np.zeros(env.num_levels, np.int32)
    for li, slots in enumerate(clue_lists):
        clue_count[li] = len(slots)
        clue_slots[li, : len(slots)] = slots

    keepalive = [budget, palette, palette_size, brush, hint_slot, clue_slots, clue_count]
    static = dict(
        num_levels=env.num_levels, plain_tag=int(env._plain), pattern_tag=int(env._pattern),
        clue_tag=int(env._clue), palette_width=palette.shape[1], max_clues=clue_slots.shape[1],
        budget=p32(budget), palette=p32(palette), palette_size=p32(palette_size),
        brush=p32(brush), hint_slot=p32(hint_slot), clue_slots=p32(clue_slots),
        clue_count=p32(clue_count),
    )
    return static, keepalive
