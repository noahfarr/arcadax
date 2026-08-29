import numpy as np
from ._util import p32, p8, pu8

def build(env, d):
    keep = []

    def i32(name):
        arr = np.ascontiguousarray(np.asarray(getattr(env, name)), np.int32)
        keep.append(arr)
        return arr

    def u8(name):
        arr = np.ascontiguousarray(np.asarray(getattr(env, name)), np.uint8)
        keep.append(arr)
        return arr

    def i8(name):
        arr = np.ascontiguousarray(np.asarray(getattr(env, name)), np.int8)
        keep.append(arr)
        return arr

    robot_slot = i32("_robot_slot")
    container_box = i32("_container_box")
    target_slot = i32("_target_slot")
    grid_box = i32("_grid_box")
    view_only = u8("_view_only")
    switch_slot = i32("_switch_slot")
    switch_box = i32("_switch_box")
    grsysj_box = i32("_grsysj_box")

    num_cols = i32("_num_cols")
    col_slot = i32("_col_slot")
    num_bits = i32("_num_bits")
    cell_slot = i32("_cell_slot")
    cell_state_pixel = i32("_cell_state_pixel")

    num_walls = i32("_num_walls")
    wall_box = i32("_wall_box")
    num_checkpoints = i32("_num_checkpoints")
    checkpoint_slot = i32("_checkpoint_slot")
    num_hazards = i32("_num_hazards")
    hazard_icon = i32("_hazard_icon")
    hazard_trig = i32("_hazard_trig")

    num_buttons = i32("_num_buttons")
    button_slot = i32("_button_slot")
    button_box = i32("_button_box")
    button_pos = i32("_button_pos")
    button_rot = i32("_button_rot")
    button_scale = i32("_button_scale")
    button_reset = u8("_button_reset")
    button_program = i32("_button_program")
    has_programs = u8("_has_programs")

    robot_mask = u8("_robot_mask")
    robot_rotation0 = i32("_robot_rotation0")
    robot_scale0 = i32("_robot_scale0")
    target_rotation = i32("_target_rotation")
    target_scale = i32("_target_scale")
    robot_color0 = i32("_robot_color0")
    target_color = i32("_target_color")
    target_touch_x = i32("_target_touch_x")
    target_touch_y = i32("_target_touch_y")
    switch_color0 = i32("_switch_color0")
    clean_target_pixels = i8("_clean_target_pixels")
    bbox_override = u8("_bbox_override")

    scroll_slot = i32("_scroll_slot")
    scroll_bg_x = i32("_scroll_bg_x")
    scroll_w = i32("_scroll_w")

    st = dict(
        num_levels=d.num_levels, num_slots=d.num_slots, ph=d.patch_shape[0], pw=d.patch_shape[1],
        robot_slot=p32(robot_slot), container_box=p32(container_box), target_slot=p32(target_slot),
        grid_box=p32(grid_box), view_only=pu8(view_only), switch_slot=p32(switch_slot),
        switch_box=p32(switch_box), grsysj_box=p32(grsysj_box),
        num_cols=p32(num_cols), col_slot=p32(col_slot), num_bits=p32(num_bits),
        cell_slot=p32(cell_slot), cell_state_pixel=p32(cell_state_pixel),
        num_walls=p32(num_walls), wall_box=p32(wall_box),
        num_checkpoints=p32(num_checkpoints), checkpoint_slot=p32(checkpoint_slot),
        num_hazards=p32(num_hazards), hazard_icon=p32(hazard_icon), hazard_trig=p32(hazard_trig),
        num_buttons=p32(num_buttons), button_slot=p32(button_slot), button_box=p32(button_box),
        button_pos=p32(button_pos), button_rot=p32(button_rot), button_scale=p32(button_scale),
        button_reset=pu8(button_reset), button_program=p32(button_program), has_programs=pu8(has_programs),
        robot_mask=pu8(robot_mask), robot_rotation0=p32(robot_rotation0), robot_scale0=p32(robot_scale0),
        target_rotation=p32(target_rotation), target_scale=p32(target_scale), robot_color0=p32(robot_color0),
        target_color=p32(target_color), target_touch_x=p32(target_touch_x), target_touch_y=p32(target_touch_y),
        switch_color0=p32(switch_color0), clean_target_pixels=p8(clean_target_pixels),
        bbox_override=pu8(bbox_override),
        scroll_slot=p32(scroll_slot), scroll_bg_x=p32(scroll_bg_x), scroll_w=p32(scroll_w),
    )
    return st, keep
