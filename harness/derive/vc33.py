import sys
import numpy as np
from ._const import Blocking, Interaction
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


TAG_LEVER = "0022jvmlspyigc"


TAG_PIPE = "0043nzrtobajqi"


TAG_SENSOR = "0016uciqlhjlom"


TAG_MARKER = "0010gnulkywfpz"


TAG_WALL = "0025yfyiswdvoh"


TAG_COUPLER = "0004sttgkofqwb"


COUPLER_ACTIVE_COLOR = 12


COUPLER_INACTIVE_COLOR = 1


BUDGET_FILLED_COLOR = 7


BUDGET_EMPTY_COLOR = 4


MAX_COUPLERS = 3


MAX_QUEUE = 6


MAX_FRAMES = 256


EXTRACT_KWARGS = {"extra_slots": MAX_COUPLERS}


def _slot_lookup(data: GameData, level: int) -> dict[tuple[str, int, int], int]:
    lookup: dict[tuple[str, int, int], int] = {}
    for slot in range(data.num_slots):
        if not data.alive[level, slot]:
            continue
        key = (data.names[level][slot], int(data.x[level, slot]), int(data.y[level, slot]))
        lookup[key] = slot
    return lookup


def _slot_of(lookup: dict[tuple[str, int, int], int], sprite) -> int:
    return lookup[(sprite.name, int(sprite.x), int(sprite.y))]


def _build_static_tables(data: GameData) -> tuple[GameData, dict[str, np.ndarray]]:
    source = sorted(reference_dir().glob(f"{data.game_id}-*.py"))[0]
    reference = load_reference_game(source)
    module = sys.modules[type(reference).__module__]
    raw_sprites = module.sprites

    L, N = data.num_levels, data.num_slots
    icon_base = N - MAX_COUPLERS

    grav_x = np.zeros(L, np.int32)
    grav_y = np.zeros(L, np.int32)
    budget = np.zeros(L, np.int32)
    sensor_adjust = np.zeros(L, np.int32)

    lever_src = np.full((L, N), -1, np.int32)
    lever_dst = np.full((L, N), -1, np.int32)

    pipe_uses_floor = np.zeros((L, N), bool)
    pipe_floor_max_front = np.zeros((L, N), np.int32)
    pipe_wall_extreme = np.zeros((L, N), np.int32)
    wall_touch_mask = np.zeros((L, N, N), bool)

    marker_along = np.zeros((L, N), np.int32)
    marker_wall = np.full((L, N), -1, np.int32)
    sensor_marker_color = np.zeros((L, N, N), bool)

    level_coupler_slot = np.full((L, MAX_COUPLERS), -1, np.int32)
    icon_dx = np.zeros((L, MAX_COUPLERS), np.int32)
    icon_dy = np.zeros((L, MAX_COUPLERS), np.int32)

    for level in range(L):
        reference.set_level(level)
        game = reference
        grav = game.dwwmpxqsza
        grav_x[level], grav_y[level] = int(grav[0]), int(grav[1])
        budget[level] = int(game.current_level.get_data("StepCounter") or 0)
        sensor_adjust[level] = 6 if grav[0] == -3 else 4

        lookup = _slot_lookup(data, level)

        for lever, (src, dst) in game.wrcxjliglr.items():
            li_slot = _slot_of(lookup, lever)
            lever_src[level, li_slot] = _slot_of(lookup, src)
            lever_dst[level, li_slot] = _slot_of(lookup, dst)

        for pipe in game.current_level.get_sprites_by_tag(TAG_PIPE):
            pidx = _slot_of(lookup, pipe)
            floors = game.kectayqmfn(pipe)
            if floors:
                pipe_uses_floor[level, pidx] = True
                pipe_floor_max_front[level, pidx] = max(game.zfcrfmorna(f) for f in floors)
            walls = game.rcbyiqlbza(pipe)
            if walls:
                extreme = max if game.qhmwbtpcsk() else min
                pipe_wall_extreme[level, pidx] = extreme(game.hpakcxndwy(w) for w in walls)
                for wall in walls:
                    wall_touch_mask[level, pidx, _slot_of(lookup, wall)] = True

        markers = game.current_level.get_sprites_by_tag(TAG_MARKER)
        walls_all = game.current_level.get_sprites_by_tag(TAG_WALL)
        for marker in markers:
            midx = _slot_of(lookup, marker)
            marker_along[level, midx] = game.xitrlzpbgu(marker)
            touching = [w for w in walls_all if w.collides_with(marker)]
            if touching:
                marker_wall[level, midx] = _slot_of(lookup, touching[0])

        for sensor in game.current_level.get_sprites_by_tag(TAG_SENSOR):
            sidx = _slot_of(lookup, sensor)
            corner = int(sensor.pixels[-1, -1])
            for marker in markers:
                if corner in marker.pixels:
                    sensor_marker_color[level, sidx, _slot_of(lookup, marker)] = True

        couplers = game.current_level.get_sprites_by_tag(TAG_COUPLER)
        if len(couplers) > MAX_COUPLERS:
            raise ValueError(f"level {level} has {len(couplers)} couplers > MAX_COUPLERS={MAX_COUPLERS}")
        for k, coupler in enumerate(couplers):
            cidx = _slot_of(lookup, coupler)
            level_coupler_slot[level, k] = cidx
            icon_name = "0006mfbmvylbss" if coupler.pixels.shape[1] == 3 else "0008rybtprnbie"
            icon = raw_sprites[icon_name].clone()
            icon.set_rotation(coupler.rotation)
            patch = icon.render()
            slot = icon_base + k
            data.pixels[level, slot] = -1
            data.pixels[level, slot, : patch.shape[0], : patch.shape[1]] = patch
            data.h[level, slot] = patch.shape[0]
            data.w[level, slot] = patch.shape[1]
            data.layer[level, slot] = 0
            data.interaction[level, slot] = int(Interaction.TANGIBLE)
            data.blocking[level, slot] = int(Blocking.PIXEL_PERFECT)
            if coupler.rotation in (0, 180):
                icon_dx[level, k], icon_dy[level, k] = -2, 0
            else:
                icon_dx[level, k], icon_dy[level, k] = 0, -2

    tables = dict(
        grav_x=grav_x,
        grav_y=grav_y,
        budget=budget,
        sensor_adjust=sensor_adjust,
        lever_src=lever_src,
        lever_dst=lever_dst,
        pipe_uses_floor=pipe_uses_floor,
        pipe_floor_max_front=pipe_floor_max_front,
        pipe_wall_extreme=pipe_wall_extreme,
        wall_touch_mask=wall_touch_mask,
        marker_along=marker_along,
        marker_wall=marker_wall,
        sensor_marker_color=sensor_marker_color,
        level_coupler_slot=level_coupler_slot,
        icon_dx=icon_dx,
        icon_dy=icon_dy,
    )
    return data, tables


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    data, t = _build_static_tables(data)
    self._icon_base = self.num_slots - MAX_COUPLERS
    self._lever_mask = np.asarray(data.tags[:, :, data.tag_index(TAG_LEVER)])
    self._pipe_mask = np.asarray(data.tags[:, :, data.tag_index(TAG_PIPE)])
    self._sensor_mask = np.asarray(data.tags[:, :, data.tag_index(TAG_SENSOR)])
    self._marker_mask = np.asarray(data.tags[:, :, data.tag_index(TAG_MARKER)])
    self._coupler_mask = np.asarray(data.tags[:, :, data.tag_index(TAG_COUPLER)])
    self._grav_x = np.asarray(t["grav_x"])
    self._grav_y = np.asarray(t["grav_y"])
    self._grav_horizontal = self._grav_x != 0
    self._grav_positive = (self._grav_x > 0) | (self._grav_y > 0)
    self._grav_signed = np.where(self._grav_horizontal, self._grav_x, self._grav_y)
    self._budget = np.asarray(t["budget"])
    self._sensor_adjust = np.asarray(t["sensor_adjust"])
    self._lever_src = np.asarray(t["lever_src"])
    self._lever_dst = np.asarray(t["lever_dst"])
    self._pipe_uses_floor = np.asarray(t["pipe_uses_floor"])
    self._pipe_floor_max_front = np.asarray(t["pipe_floor_max_front"])
    self._pipe_wall_extreme = np.asarray(t["pipe_wall_extreme"])
    self._wall_touch_mask = np.asarray(t["wall_touch_mask"])
    self._marker_along = np.asarray(t["marker_along"])
    self._marker_wall = np.asarray(t["marker_wall"])
    self._sensor_marker_color = np.asarray(t["sensor_marker_color"])
    self._level_coupler_slot = np.asarray(t["level_coupler_slot"])
    self._icon_dx = np.asarray(t["icon_dx"])
    self._icon_dy = np.asarray(t["icon_dy"])
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
