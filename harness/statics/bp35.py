import numpy as np
from ._util import p32, p8, pf, pu8

def build(env, d):
    keep = []

    def const(a, dtype):
        arr = np.ascontiguousarray(np.asarray(a), dtype)
        keep.append(arr)
        return arr

    kind0 = const(env._kind0, np.int8)
    wall = const(env._wall, np.uint8)
    spike = const(env._spike, np.uint8)
    ceil_w0 = const(env._ceil_w0, np.uint8)
    level_w = const(env._level_w, np.int32)
    level_h = const(env._level_h, np.int32)
    player_start = const(env._player_start, np.int32)
    terrain_present = const(env._terrain_present, np.uint8)
    terrain_anchor = const(env._terrain_anchor, np.int32)
    terrain_atlas = const(env._terrain_atlas, np.int32)
    terrain_layer = const(env._terrain_layer, np.int32)
    img_atlas = const(env._img_atlas, np.int32)
    img_layer = const(env._img_layer, np.int32)
    ease_out = const(env._ease_out, np.float32)
    ease_lin = const(env._ease_lin, np.float32)

    st = dict(
        kind0=p8(kind0), wall=pu8(wall), spike=pu8(spike), ceil_w0=pu8(ceil_w0),
        level_w=p32(level_w), level_h=p32(level_h), player_start=p32(player_start),
        terrain_present=pu8(terrain_present), terrain_anchor=p32(terrain_anchor),
        terrain_atlas=p32(terrain_atlas), terrain_layer=p32(terrain_layer),
        img_atlas=p32(img_atlas), img_layer=p32(img_layer),
        ease_out=pf(ease_out), ease_lin=pf(ease_lin),
    )
    return st, keep
