import types
import sys
import dataclasses
import numpy as np
from pathlib import Path
from ._const import Blocking, Interaction
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


GRID = 6


DOOR_LOCK_SENTINEL = 11


ECHO_BODY_COLOR = 2


ECHO_CENTER_COLOR = 5


DEATH_PIECES = 9


CROSSFADE_PIECES = 14


DEATH_TEMPLATES = ("fmwxdssydx", "ikziwpoghy", "ynctegjyif")


CROSSFADE_TEMPLATES = (
    "iaidlbpztj",
    "movmcviiig",
    "dbinpkvkey",
    "caduiijgej",
    "dwckbnxaln",
)


CHECKPOINT_ON_TEMPLATE = "oewlmipzzz"


CHECKPOINT_OFF_TEMPLATE = "opsxquspoc"


TAG_ICE = "vtwcsmdoqp"


TAG_ICE_BOUND = "akfoiqesdk"


TAG_BOUNDS = "rsrdfsruqh"


TAG_PLAYER = "qftsebtxuc"


TAG_CHECKPOINT = "gpkhwmwioo"


TAG_GHOST = "ovhuyqtghw"


TAG_DOOR = "kjrcloicja"


TAG_BUTTON = "medyellngi"


TAG_GATE = "hxztohfdlx"


TAG_PORTAL = "hgglgttaui"


TAG_ENDPOINT = "mpreboxmgc"


TAG_GOAL = "gilbljmfbc"


TAG_TIMER = "ppfvilwwnk"


MAX_CHECKPOINTS = 3


MAX_BUTTONS = 5


MAX_GATES = 5


MAX_DOORS = 5


MAX_PORTALS = 2


MAX_ENDPOINTS = 4


MAX_ICE = 1


MAX_SENSORS = MAX_BUTTONS + MAX_ENDPOINTS


PLAYER_GENERATIONS = 6


MAX_TRACKED = PLAYER_GENERATIONS + MAX_ICE


MAX_MOVES = 130


GATE_NONE, GATE_DOOR, GATE_PORTAL = 0, 1, 2


def _load_template_module(source: str | Path) -> types.ModuleType:
    source = Path(source)
    name = f"g50t_templates_{source.stem.replace('-', '_')}"
    if name in sys.modules:
        return sys.modules[name]
    module = types.ModuleType(name)
    module.__file__ = str(source)
    sys.modules[name] = module
    exec(
        compile(source.read_text(encoding="utf-8"), str(source), "exec"),
        module.__dict__,
    )
    return module


def _esidlbhbhw(data: GameData, level: int, slot: int, x: int, y: int) -> int:
    sx, sy = int(data.x[level, slot]), int(data.y[level, slot])
    w, h = int(data.w[level, slot]), int(data.h[level, slot])
    if x < sx or y < sy or x >= sx + w or y >= sy + h:
        return -1
    return int(data.pixels[level, slot, y - sy, x - sx])


def _center(data: GameData, level: int, slot: int) -> tuple[int, int]:
    return (
        int(data.x[level, slot] + data.w[level, slot] // 2),
        int(data.y[level, slot] + data.h[level, slot] // 2),
    )


def _overlap(data: GameData, level: int, a: int, b: int) -> bool:
    ax, ay = int(data.x[level, a]), int(data.y[level, a])
    aw, ah = int(data.w[level, a]), int(data.h[level, a])
    bx, by = int(data.x[level, b]), int(data.y[level, b])
    bw, bh = int(data.w[level, b]), int(data.h[level, b])
    return ax < bx + bw and ax + aw > bx and ay < by + bh and ay + ah > by


def _slots(data: GameData, level: int, tag: str) -> list[int]:
    ti = data.tag_index(tag)
    return [
        i
        for i in range(data.num_slots)
        if data.alive[level, i] and data.tags[level, i, ti]
    ]


@dataclasses.dataclass
class _Wiring:
    checkpoint_slot: np.ndarray
    checkpoint_count: np.ndarray
    player_slot: np.ndarray
    goal_slot: np.ndarray
    bounds_slot: np.ndarray
    timer_slot: np.ndarray
    ghost_slot: np.ndarray
    button_slot: np.ndarray
    gate_slot: np.ndarray
    door_slot: np.ndarray
    portal_slot: np.ndarray
    endpoint_slot: np.ndarray
    ice_slot: np.ndarray
    ice_bound_role: np.ndarray
    sensor_slot: np.ndarray
    button_gate: np.ndarray
    gate_kind: np.ndarray
    gate_target: np.ndarray
    portal_endpoints: np.ndarray
    door_dir_x: np.ndarray
    door_dir_y: np.ndarray


_ROTATION_TO_OPEN_DIR = {0: (0, 1), 90: (-1, 0), 180: (0, -1), 270: (1, 0)}


def _compute_wiring(data: GameData, module: types.ModuleType | None = None) -> _Wiring:
    L = data.num_levels
    w = _Wiring(
        checkpoint_slot=np.full((L, MAX_CHECKPOINTS), -1, np.int32),
        checkpoint_count=np.zeros(L, np.int32),
        player_slot=np.zeros(L, np.int32),
        goal_slot=np.zeros(L, np.int32),
        bounds_slot=np.zeros(L, np.int32),
        timer_slot=np.zeros(L, np.int32),
        ghost_slot=np.zeros(L, np.int32),
        button_slot=np.full((L, MAX_BUTTONS), -1, np.int32),
        gate_slot=np.full((L, MAX_GATES), -1, np.int32),
        door_slot=np.full((L, MAX_DOORS), -1, np.int32),
        portal_slot=np.full((L, MAX_PORTALS), -1, np.int32),
        endpoint_slot=np.full((L, MAX_ENDPOINTS), -1, np.int32),
        ice_slot=np.full((L, MAX_ICE), -1, np.int32),
        ice_bound_role=np.full((L, MAX_ICE), -1, np.int32),
        sensor_slot=np.full((L, MAX_SENSORS), -1, np.int32),
        button_gate=np.full((L, MAX_BUTTONS), -1, np.int32),
        gate_kind=np.full((L, MAX_GATES), GATE_NONE, np.int32),
        gate_target=np.full((L, MAX_GATES), -1, np.int32),
        portal_endpoints=np.full((L, MAX_PORTALS, 2), -1, np.int32),
        door_dir_x=np.zeros((L, MAX_DOORS), np.int32),
        door_dir_y=np.zeros((L, MAX_DOORS), np.int32),
    )
    for li in range(L):
        checkpoints = sorted(
            _slots(data, li, TAG_CHECKPOINT), key=lambda i: int(data.x[li, i])
        )
        w.checkpoint_count[li] = len(checkpoints)
        for k, s in enumerate(checkpoints[:MAX_CHECKPOINTS]):
            w.checkpoint_slot[li, k] = s

        w.player_slot[li] = _slots(data, li, TAG_PLAYER)[0]
        w.goal_slot[li] = _slots(data, li, TAG_GOAL)[0]
        w.bounds_slot[li] = _slots(data, li, TAG_BOUNDS)[0]
        w.timer_slot[li] = _slots(data, li, TAG_TIMER)[0]
        w.ghost_slot[li] = _slots(data, li, TAG_GHOST)[0]

        buttons = _slots(data, li, TAG_BUTTON)
        gates = _slots(data, li, TAG_GATE)
        doors = _slots(data, li, TAG_DOOR)
        portals = _slots(data, li, TAG_PORTAL)
        endpoints = _slots(data, li, TAG_ENDPOINT)
        ice = _slots(data, li, TAG_ICE)
        ice_bounds = _slots(data, li, TAG_ICE_BOUND)

        for i, s in enumerate(buttons[:MAX_BUTTONS]):
            w.button_slot[li, i] = s
        for i, s in enumerate(gates[:MAX_GATES]):
            w.gate_slot[li, i] = s
        for i, s in enumerate(doors[:MAX_DOORS]):
            w.door_slot[li, i] = s
            if module is not None:
                rotation = int(module.levels[li].get_sprites()[s].rotation)
                dx, dy = _ROTATION_TO_OPEN_DIR[rotation]
                w.door_dir_x[li, i] = dx
                w.door_dir_y[li, i] = dy
        for i, s in enumerate(portals[:MAX_PORTALS]):
            w.portal_slot[li, i] = s
        for i, s in enumerate(endpoints[:MAX_ENDPOINTS]):
            w.endpoint_slot[li, i] = s
        for i, s in enumerate(ice[:MAX_ICE]):
            w.ice_slot[li, i] = s

        gate_role = {s: r for r, s in enumerate(gates[:MAX_GATES])}
        door_role = {s: r for r, s in enumerate(doors[:MAX_DOORS])}
        portal_role = {s: r for r, s in enumerate(portals[:MAX_PORTALS])}
        endpoint_role = {s: r for r, s in enumerate(endpoints[:MAX_ENDPOINTS])}

        for bi, b in enumerate(buttons[:MAX_BUTTONS]):
            cx, cy = _center(data, li, b)
            match = None
            for g in gates:
                if _overlap(data, li, b, g) and _esidlbhbhw(data, li, g, cx, cy) != -1:
                    match = g
            if match is not None:
                w.button_gate[li, bi] = gate_role[match]

        for d in doors:
            cx, cy = _center(data, li, d)
            for g in gates:
                if _overlap(data, li, d, g) and _esidlbhbhw(data, li, g, cx, cy) != -1:
                    gr = gate_role[g]
                    w.gate_kind[li, gr] = GATE_DOOR
                    w.gate_target[li, gr] = door_role[d]

        portal_eps: dict[int, list[int]] = {p: [] for p in portals}
        for e in endpoints:
            cx, cy = _center(data, li, e)
            for p in portals:
                if len(portal_eps[p]) >= 2:
                    continue
                if _overlap(data, li, e, p) and _esidlbhbhw(data, li, p, cx, cy) != -1:
                    portal_eps[p].append(e)
        for p, eps in portal_eps.items():
            pr = portal_role[p]
            for k, e in enumerate(eps):
                w.portal_endpoints[li, pr, k] = endpoint_role[e]

        for p, eps in portal_eps.items():
            if len(eps) != 2:
                continue
            e0x, e0y = _center(data, li, eps[0])
            e1x, e1y = _center(data, li, eps[1])
            for g in gates:
                if not _overlap(data, li, eps[0], g) or not _overlap(
                    data, li, eps[1], g
                ):
                    continue
                if (
                    _esidlbhbhw(data, li, g, e0x, e0y) == -1
                    or _esidlbhbhw(data, li, g, e1x, e1y) == -1
                ):
                    continue
                gr = gate_role[g]
                w.gate_kind[li, gr] = GATE_PORTAL
                w.gate_target[li, gr] = portal_role[p]

        for ii, i in enumerate(ice[:MAX_ICE]):
            ix, iy = int(data.x[li, i]), int(data.y[li, i])
            match = None
            for b in ice_bounds:
                bx, by = int(data.x[li, b]), int(data.y[li, b])
                bw, bh = int(data.w[li, b]), int(data.h[li, b])
                if (
                    ix >= bx
                    and iy >= by
                    and ix < bx + bw
                    and iy < by + bh
                    and _esidlbhbhw(data, li, b, ix, iy) >= 0
                ):
                    match = b
            if match is not None:
                w.ice_bound_role[li, ii] = match

        sensors = buttons[:MAX_BUTTONS] + endpoints[:MAX_ENDPOINTS]
        for i, s in enumerate(sensors[:MAX_SENSORS]):
            w.sensor_slot[li, i] = s

    return w


@dataclasses.dataclass
class _Layout:
    base: int
    checkpoint_on: list[int]
    checkpoint_off: list[int]
    player_death: list[int]
    ice_death: list[list[int]]
    portal_crossfade: list[list[int]]
    gen_body: list[int]
    total_extra: int


def _make_layout(base: int) -> _Layout:
    off = base
    checkpoint_on = [off + 2 * i for i in range(MAX_CHECKPOINTS)]
    checkpoint_off = [off + 2 * i + 1 for i in range(MAX_CHECKPOINTS)]
    off += 2 * MAX_CHECKPOINTS
    player_death = list(range(off, off + DEATH_PIECES))
    off += DEATH_PIECES
    ice_death = []
    for _ in range(MAX_ICE):
        ice_death.append(list(range(off, off + DEATH_PIECES)))
        off += DEATH_PIECES
    portal_crossfade = []
    for _ in range(MAX_PORTALS):
        portal_crossfade.append(list(range(off, off + CROSSFADE_PIECES)))
        off += CROSSFADE_PIECES
    gen_body = list(range(off, off + (PLAYER_GENERATIONS - 1)))
    off += PLAYER_GENERATIONS - 1
    return _Layout(
        base=base,
        checkpoint_on=checkpoint_on,
        checkpoint_off=checkpoint_off,
        player_death=player_death,
        ice_death=ice_death,
        portal_crossfade=portal_crossfade,
        gen_body=gen_body,
        total_extra=off - base,
    )


def _place(
    data: GameData,
    level: int,
    slot: int,
    *,
    pixels: np.ndarray,
    x: int,
    y: int,
    layer: int,
    blocking: int,
) -> None:
    ph, pw = pixels.shape
    data.pixels[level, slot, :, :] = -1
    data.pixels[level, slot, :ph, :pw] = pixels
    data.h[level, slot] = ph
    data.w[level, slot] = pw
    data.x[level, slot] = x
    data.y[level, slot] = y
    data.layer[level, slot] = layer
    data.interaction[level, slot] = int(Interaction.REMOVED)
    data.blocking[level, slot] = blocking
    data.alive[level, slot] = True


def _populate_extras(
    data: GameData, module: types.ModuleType, wiring: _Wiring, layout: _Layout
) -> None:
    templates = module.sprites
    on_px = templates[CHECKPOINT_ON_TEMPLATE].render()
    off_px = templates[CHECKPOINT_OFF_TEMPLATE].render()
    death_px = [templates[n].render() for n in DEATH_TEMPLATES]
    cross_px = [templates[n].render() for n in CROSSFADE_TEMPLATES]
    death_layer = 5
    cross_layer = int(templates[CROSSFADE_TEMPLATES[0]].layer)

    for li in range(data.num_levels):
        for k in range(int(wiring.checkpoint_count[li])):
            pip = int(wiring.checkpoint_slot[li, k])
            x, y = int(data.x[li, pip]), int(data.y[li, pip])
            _place(
                data,
                li,
                layout.checkpoint_on[k],
                pixels=on_px,
                x=x,
                y=y,
                layer=0,
                blocking=int(Blocking.PIXEL_PERFECT),
            )
            _place(
                data,
                li,
                layout.checkpoint_off[k],
                pixels=off_px,
                x=x,
                y=y,
                layer=0,
                blocking=int(Blocking.PIXEL_PERFECT),
            )

        player_slot = int(wiring.player_slot[li])
        player_color = int(data.pixels[li, player_slot, 1, 1])
        player_bl = int(data.blocking[li, player_slot])
        for piece in range(DEATH_PIECES):
            template = death_px[piece // 3]
            px = np.where(template >= 0, player_color, template).astype(np.int8)
            _place(
                data,
                li,
                layout.player_death[piece],
                pixels=px,
                x=0,
                y=0,
                layer=death_layer,
                blocking=player_bl,
            )

        for gen in range(PLAYER_GENERATIONS - 1):
            body = data.pixels[li, player_slot].copy()
            _place(
                data,
                li,
                layout.gen_body[gen],
                pixels=body[
                    : int(data.h[li, player_slot]), : int(data.w[li, player_slot])
                ],
                x=0,
                y=0,
                layer=int(data.layer[li, player_slot]),
                blocking=player_bl,
            )

        for ii in range(MAX_ICE):
            ice_slot = int(wiring.ice_slot[li, ii])
            if ice_slot < 0:
                continue
            ice_color = int(data.pixels[li, ice_slot, 1, 1])
            ice_bl = int(data.blocking[li, ice_slot])
            for piece in range(DEATH_PIECES):
                template = death_px[piece // 3]
                px = np.where(template >= 0, ice_color, template).astype(np.int8)
                _place(
                    data,
                    li,
                    layout.ice_death[ii][piece],
                    pixels=px,
                    x=0,
                    y=0,
                    layer=death_layer,
                    blocking=ice_bl,
                )

        for pi in range(MAX_PORTALS):
            portal_slot = int(wiring.portal_slot[li, pi])
            if portal_slot < 0:
                continue
            x, y = int(data.x[li, portal_slot]), int(data.y[li, portal_slot])
            order = [0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 4, 4]
            for piece in range(CROSSFADE_PIECES):
                px = cross_px[order[piece]]
                _place(
                    data,
                    li,
                    layout.portal_crossfade[pi][piece],
                    pixels=px,
                    x=x,
                    y=y,
                    layer=cross_layer,
                    blocking=int(Blocking.PIXEL_PERFECT),
                )


def _build_data(source: str | Path, seed: int = 0) -> tuple[GameData, "_Wiring"]:
    base_data = extract(source, seed=seed)
    layout = _make_layout(base_data.num_slots)
    data = extract(source, extra_slots=layout.total_extra, seed=seed)
    module = _load_template_module(source)
    wiring = _compute_wiring(data, module=module)
    _populate_extras(data, module, wiring, layout)
    return data, wiring


def _locate_source(game_id: str) -> Path:
    matches = sorted(reference_dir().glob(f"{game_id}-*.py"))
    if not matches:
        raise FileNotFoundError(f"no source for {game_id!r} in {reference_dir()}")
    return matches[0]


DATA_PARAM = 'data'


def derive(data, _wiring=None):
    self = Setup(data)
    if _wiring is None:
        data, _wiring = _build_data(_locate_source(data.game_id))
    w = _wiring
    i32 = lambda a: np.asarray(a, np.int32)
    self._checkpoint_slot = i32(w.checkpoint_slot)
    self._checkpoint_count = i32(w.checkpoint_count)
    self._player_slot = i32(w.player_slot)
    self._goal_slot = i32(w.goal_slot)
    self._bounds_slot = i32(w.bounds_slot)
    self._timer_slot = i32(w.timer_slot)
    self._ghost_slot = i32(w.ghost_slot)
    self._button_slot = i32(w.button_slot)
    self._gate_slot = i32(w.gate_slot)
    self._door_slot = i32(w.door_slot)
    self._portal_slot = i32(w.portal_slot)
    self._endpoint_slot = i32(w.endpoint_slot)
    self._ice_slot = i32(w.ice_slot)
    self._ice_bound_role = i32(w.ice_bound_role)
    self._sensor_slot = i32(w.sensor_slot)
    self._button_gate = i32(w.button_gate)
    self._gate_kind = i32(w.gate_kind)
    self._gate_target = i32(w.gate_target)
    self._portal_endpoints = i32(w.portal_endpoints)
    self._door_dir_x = i32(w.door_dir_x)
    self._door_dir_y = i32(w.door_dir_y)
    base = data.num_slots - _make_layout(0).total_extra
    layout = _make_layout(base)
    self._layout = layout
    gen_slot = np.zeros((data.num_levels, PLAYER_GENERATIONS), np.int32)
    gen_slot[:, 0] = w.player_slot
    for g in range(1, PLAYER_GENERATIONS):
        gen_slot[:, g] = layout.gen_body[g - 1]
    self._gen_slot_table = np.asarray(gen_slot, np.int32)
    self._player_death_slot = np.asarray(layout.player_death, np.int32)
    self._ice_death_slot = (
        np.asarray(layout.ice_death, np.int32)
        if MAX_ICE
        else np.zeros((0, DEATH_PIECES), np.int32)
    )
    self._portal_crossfade_slot = np.asarray(layout.portal_crossfade, np.int32)
    self._checkpoint_on_slot = np.asarray(layout.checkpoint_on, np.int32)
    self._checkpoint_off_slot = np.asarray(layout.checkpoint_off, np.int32)
    self._player_color = np.asarray(
        data.pixels[np.arange(data.num_levels), w.player_slot, 1, 1], np.int8
    )
    self._player_pixels = np.asarray(
        data.pixels[np.arange(data.num_levels), w.player_slot], np.int8
    )
    ice_color = np.zeros((data.num_levels, MAX_ICE), np.int8)
    for li in range(data.num_levels):
        for ii in range(MAX_ICE):
            s = int(w.ice_slot[li, ii])
            if s >= 0:
                ice_color[li, ii] = data.pixels[li, s, 1, 1]
    self._ice_color = np.asarray(ice_color)
    self._spawn_x = np.asarray(
        data.x[np.arange(data.num_levels), w.player_slot], np.int32
    )
    self._spawn_y = np.asarray(
        data.y[np.arange(data.num_levels), w.player_slot], np.int32
    )
    return self


def make_args(source, seed=0):
    data, wiring = _build_data(source)
    return (data,), {'_wiring': wiring}
