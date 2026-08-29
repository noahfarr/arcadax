import numpy as np
from ._util import p32, pu8

from ..derive.lf52 import CAPTURE_FADE_TICK_COUNT, GRID_MAX_HEIGHT, GRID_MAX_WIDTH, JUMP_LANDING_REVEAL_TRIGGER_SLOT_COUNT, LEVEL_COUNT, PEG_SLOT_COUNT, PULSE_BAKED_TICK_COUNT, STATIC_SLOT_COUNT, WALL_TILE_SLOT_COUNT


def build(env, d):
    keep = []

    def i32(a):
        arr = np.ascontiguousarray(np.asarray(a), np.int32)
        keep.append(arr)
        return arr

    def u8(a):
        arr = np.ascontiguousarray(np.asarray(a), np.uint8)
        keep.append(arr)
        return arr

    static_image = i32(env.static_image)
    static_x = i32(env.static_x)
    static_y = i32(env.static_y)
    static_layer = i32(env.static_layer)
    static_wall_tile_index = i32(env.static_wall_tile_index)
    static_wall_tile_local_x = i32(env.static_wall_tile_local_x)
    static_wall_tile_local_y = i32(env.static_wall_tile_local_y)
    wall_tile_grid_x_initial = i32(env.wall_tile_grid_x_initial)
    wall_tile_grid_y_initial = i32(env.wall_tile_grid_y_initial)
    wall_tile_is_landable_double = u8(env.wall_tile_is_landable_double)
    wall_tile_has_jumpable_pin = u8(env.wall_tile_has_jumpable_pin)
    wall_tile_count = i32(env.wall_tile_count)
    peg_grid_x_initial = i32(env.peg_grid_x_initial)
    peg_grid_y_initial = i32(env.peg_grid_y_initial)
    peg_color = i32(env.peg_color)
    peg_alive_initial = u8(env.peg_alive_initial)
    grid_width = i32(env.grid_width)
    grid_height = i32(env.grid_height)
    offset_x = i32(env.offset_x)
    offset_y = i32(env.offset_y)
    landable_single = u8(env.landable_single)
    landable_double = u8(env.landable_double)
    jumpable_pin = u8(env.jumpable_pin)
    wall_present = u8(env.wall_present)
    peg_base_image = i32(env.peg_base_image)
    peg_pulse_image = i32(env.peg_pulse_image)
    peg_capture_fade_image = i32(env.peg_capture_fade_image)
    revealed_peg_capture_fade_image = i32(env.revealed_peg_capture_fade_image)
    heart_image = i32(env.heart_image)
    dust_image = i32(env.dust_image)
    win_blue_offset = i32(env.win_blue_offset)
    win_target_peg_count = i32(env.win_target_peg_count)
    stalemate_action_budget = i32(env.stalemate_action_budget)
    trigger_x = i32(env.jump_landing_reveal_trigger_x)
    trigger_y = i32(env.jump_landing_reveal_trigger_y)
    trigger_max_remaining = i32(env.jump_landing_reveal_trigger_max_remaining)
    trigger_valid = u8(env.jump_landing_reveal_trigger_valid)

    assert static_image.shape == (LEVEL_COUNT, STATIC_SLOT_COUNT)
    assert peg_grid_x_initial.shape == (LEVEL_COUNT, PEG_SLOT_COUNT)
    assert wall_tile_grid_x_initial.shape == (LEVEL_COUNT, WALL_TILE_SLOT_COUNT)
    assert landable_single.shape == (LEVEL_COUNT, GRID_MAX_HEIGHT, GRID_MAX_WIDTH)
    assert peg_pulse_image.shape == (3, PULSE_BAKED_TICK_COUNT)
    assert peg_capture_fade_image.shape == (3, CAPTURE_FADE_TICK_COUNT)
    assert trigger_x.shape == (LEVEL_COUNT, JUMP_LANDING_REVEAL_TRIGGER_SLOT_COUNT)

    st = dict(
        static_image=p32(static_image), static_x=p32(static_x), static_y=p32(static_y),
        static_layer=p32(static_layer), static_wall_tile_index=p32(static_wall_tile_index),
        static_wall_tile_local_x=p32(static_wall_tile_local_x),
        static_wall_tile_local_y=p32(static_wall_tile_local_y),
        wall_tile_grid_x_initial=p32(wall_tile_grid_x_initial),
        wall_tile_grid_y_initial=p32(wall_tile_grid_y_initial),
        wall_tile_is_landable_double=pu8(wall_tile_is_landable_double),
        wall_tile_has_jumpable_pin=pu8(wall_tile_has_jumpable_pin),
        wall_tile_count=p32(wall_tile_count),
        peg_grid_x_initial=p32(peg_grid_x_initial), peg_grid_y_initial=p32(peg_grid_y_initial),
        peg_color=p32(peg_color), peg_alive_initial=pu8(peg_alive_initial),
        grid_width=p32(grid_width), grid_height=p32(grid_height), offset_x=p32(offset_x), offset_y=p32(offset_y),
        landable_single=pu8(landable_single), landable_double=pu8(landable_double),
        jumpable_pin=pu8(jumpable_pin), wall_present=pu8(wall_present),
        peg_base_image=p32(peg_base_image), peg_pulse_image=p32(peg_pulse_image),
        peg_capture_fade_image=p32(peg_capture_fade_image),
        revealed_peg_capture_fade_image=p32(revealed_peg_capture_fade_image),
        heart_image=p32(heart_image), ring_image=int(env.ring_image),
        dust_image=p32(dust_image), revealed_peg_image=int(env.revealed_peg_image),
        reveal_button_image=int(env.reveal_button_image),
        win_blue_offset=p32(win_blue_offset), win_target_peg_count=p32(win_target_peg_count),
        stalemate_action_budget=p32(stalemate_action_budget),
        jump_landing_reveal_trigger_x=p32(trigger_x), jump_landing_reveal_trigger_y=p32(trigger_y),
        jump_landing_reveal_trigger_max_remaining=p32(trigger_max_remaining),
        jump_landing_reveal_trigger_valid=pu8(trigger_valid),
    )
    return st, keep
