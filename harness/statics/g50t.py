import numpy as np
from ._util import p32, p8

def build(env, d):
    keep = []

    def const(a, dtype=np.int32):
        arr = np.ascontiguousarray(np.asarray(a, dtype))
        keep.append(arr)
        return arr

    st = dict(
        num_levels=d.num_levels, num_slots=d.num_slots,
        checkpoint_slot=p32(const(env._checkpoint_slot)),
        checkpoint_count=p32(const(env._checkpoint_count)),
        goal_slot=p32(const(env._goal_slot)),
        bounds_slot=p32(const(env._bounds_slot)),
        timer_slot=p32(const(env._timer_slot)),
        ghost_slot=p32(const(env._ghost_slot)),
        door_slot=p32(const(env._door_slot)),
        endpoint_slot=p32(const(env._endpoint_slot)),
        ice_slot=p32(const(env._ice_slot)),
        ice_bound_slot=p32(const(env._ice_bound_role)),
        sensor_slot=p32(const(env._sensor_slot)),
        button_gate=p32(const(env._button_gate)),
        gate_kind=p32(const(env._gate_kind)),
        gate_target=p32(const(env._gate_target)),
        portal_endpoints=p32(const(env._portal_endpoints)),
        door_dir_x=p32(const(env._door_dir_x)),
        door_dir_y=p32(const(env._door_dir_y)),
        gen_slot=p32(const(env._gen_slot_table)),
        player_death_slot=p32(const(env._player_death_slot)),
        ice_death_slot=p32(const(env._ice_death_slot)),
        portal_crossfade_slot=p32(const(env._portal_crossfade_slot)),
        checkpoint_on_slot=p32(const(env._checkpoint_on_slot)),
        checkpoint_off_slot=p32(const(env._checkpoint_off_slot)),
        spawn_x=p32(const(env._spawn_x)),
        spawn_y=p32(const(env._spawn_y)),
        player_pixels=p8(const(env._player_pixels, np.int8)),
    )
    return st, keep
