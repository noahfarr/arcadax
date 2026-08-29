import numpy as np
from ._util import p32, p8, pu8

DELTA_BOUND, DELTA_SIZE = 90, 181


def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    L, N = d.num_levels, d.num_slots
    ph, pw = d.patch_shape

    blob_slot = const(env._blob_slot)
    fruit_slot = const(env._fruit_slot)
    zone_a_slot = const(env._zone_a_slot)
    zone_b_slot = const(env._zone_b_slot)
    tutorial_slot = const(env._tutorial_slot)
    tier_of_slot = const(env._tier_of_slot)
    first_blob_slot = const(np.argmax(np.asarray(env._tier_of_slot) >= 0, axis=1))
    fruit_kind_of_slot = const(env._fruit_kind_of_slot)

    steps_budget = const(env._steps_budget)
    win_type = const(env._win_type)
    win_value = const(env._win_value)
    win_count = const(env._win_count)

    tier_pixels = const(env._tier_pixels, np.int8)
    tier_h = const(env._tier_h)
    tier_w = const(env._tier_w)
    tier_layer = const(env._tier_layer)
    fruit_pixels = const(env._fruit_pixels, np.int8)
    fruit_h = const(env._fruit_h)
    fruit_w = const(env._fruit_w)
    fruit_layer = const(env._fruit_layer)

    pull_floor_x = const(env._pull_floor_x)
    pull_round_x = const(env._pull_round_x)
    pull_tie_x = const(env._pull_tie_x, np.uint8)
    pull_floor_y = const(env._pull_floor_y)
    pull_round_y = const(env._pull_round_y)
    pull_tie_y = const(env._pull_tie_y, np.uint8)

    swallow_floor_x = const(env._swallow_floor_x)
    swallow_round_x = const(env._swallow_round_x)
    swallow_tie_x = const(env._swallow_tie_x, np.uint8)
    swallow_floor_y = const(env._swallow_floor_y)
    swallow_round_y = const(env._swallow_round_y)
    swallow_tie_y = const(env._swallow_tie_y, np.uint8)

    glow_states = const(env._glow_states, np.int8)

    level_pixels = const(d.pixels, np.int8)

    assert d.patch_shape == (ph, pw)
    assert env._delta_bound == DELTA_BOUND
    assert env._glow_slot == N - 1

    st = dict(
        num_levels=L, num_slots=N, ph=ph, pw=pw, glow_slot=env._glow_slot,
        blob_slot=p32(blob_slot), fruit_slot=p32(fruit_slot),
        zone_a_slot=p32(zone_a_slot), zone_b_slot=p32(zone_b_slot),
        tutorial_slot=p32(tutorial_slot), first_blob_slot=p32(first_blob_slot),
        tier_of_slot=p32(tier_of_slot), fruit_kind_of_slot=p32(fruit_kind_of_slot),
        steps_budget=p32(steps_budget), win_type=p32(win_type), win_value=p32(win_value),
        win_count=p32(win_count),
        tier_pixels=p8(tier_pixels), tier_h=p32(tier_h), tier_w=p32(tier_w), tier_layer=p32(tier_layer),
        fruit_pixels=p8(fruit_pixels), fruit_h=p32(fruit_h), fruit_w=p32(fruit_w),
        fruit_layer=p32(fruit_layer),
        pull_floor_x=p32(pull_floor_x), pull_round_x=p32(pull_round_x), pull_tie_x=pu8(pull_tie_x),
        pull_floor_y=p32(pull_floor_y), pull_round_y=p32(pull_round_y), pull_tie_y=pu8(pull_tie_y),
        swallow_floor_x=p32(swallow_floor_x), swallow_round_x=p32(swallow_round_x),
        swallow_tie_x=pu8(swallow_tie_x),
        swallow_floor_y=p32(swallow_floor_y), swallow_round_y=p32(swallow_round_y),
        swallow_tie_y=pu8(swallow_tie_y),
        glow_states=p8(glow_states),
        level_pixels=p8(level_pixels),
    )
    return st, keep
