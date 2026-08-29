import sys
import numpy as np
from ._const import Blocking, Interaction
from ._base import GameData, Setup, extract, load_reference_game, source_for


EXTRACT_KWARGS = {"extra_slots": 1}


TAG_PLAYER = "sfqyzhzkij"


TAG_LOADOUT = "wgmbtyhvbc"


TAG_LOADOUT_FRAME = "eqatonpohu"


TAG_HINT_GLOW = "ghizzeqtoh"


TAG_GOAL = "rjlbuycveu"


TAG_GOAL_MARKER = "kvynsvxbpi"


TAG_GOAL_FRAME = "vjotnebuqo"


TAG_GOAL_FRAME_FINAL = "vfkkzdgxzx"


TAG_HINT_RING = "hoswmpiqkw"


TAG_HAZARD = "ihdgageizm"


TAG_REFILL = "npxgalaybz"


TAG_BTN_SHAPE = "ttfwljgohq"


TAG_BTN_COLOR = "soyhouuebz"


TAG_BTN_ROTATION = "rhsxkxzdjz"


TAG_PUSHABLE = "gbvqrjtaqo"


TAG_PATROL_AREA = "xfmluydglp"


CODE_HAZARD, CODE_GOAL, CODE_REFILL, CODE_BTN_SHAPE, CODE_BTN_COLOR, CODE_BTN_ROTATION = range(1, 7)


MAX_GOALS = 2


MAX_PATROLS = 3


PUSH_FRAMES = 16


PUSH_PUSH_PHASE = 8


DEATH_FRAMES = 5


FLASH_FRAMES = 5


LIVES_START = 3


PATROL_CELL = 5


COLOR_BLACK = 0


COLOR_FOG = 5


COLOR_STEP_EMPTY = 3


COLOR_STEP_FILLED = 11


COLOR_LIVES_FILLED = 8


COLOR_DEATH_FLASH = 11


def _loadout_variants(reference) -> np.ndarray:
    import sys

    module = sys.modules[type(reference).__module__]
    base_colour = module.epeqflmtfc
    reference.set_level(0)
    base = reference.htkmubhry.clone()
    h, w = base.render().shape
    out = np.full((6, 4, 4, h, w), -1, np.int8)
    for s, shape_sprite in enumerate(reference.ijessuuig):
        for c, colour in enumerate(reference.tnkekoeuk):
            for r, rotation in enumerate(reference.dhksvilbb):
                sp = base.clone()
                sp.pixels = shape_sprite.pixels.copy()
                sp.color_remap(base_colour, colour)
                sp.set_rotation(rotation)
                out[s, c, r] = sp.render()
    return out


def _bake_goal_markers(data: GameData, reference) -> None:
    for li in range(data.num_levels):
        reference.set_level(li)
        markers = reference.current_level.get_sprites_by_tag(TAG_GOAL_MARKER)
        slots = [i for i in range(data.num_slots) if data.alive[li, i] and data.tags[li, i, data.tag_index(TAG_GOAL_MARKER)]]
        assert len(markers) == len(slots)
        for marker, slot in zip(markers, slots):
            px = marker.render()
            data.pixels[li, slot] = -1
            data.pixels[li, slot, : px.shape[0], : px.shape[1]] = px


class _DummyPlayer:
    def __init__(self, x: int, y: int) -> None:
        self.x = x
        self.y = y

    def move(self, dx: int, dy: int) -> None:
        self.x += dx
        self.y += dy


def _push_tables(data: GameData, reference) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    shape = (data.num_levels, data.num_slots)
    is_pushable = np.zeros(shape, bool)
    step_dx = np.zeros((*shape, PUSH_FRAMES), np.int32)
    step_dy = np.zeros((*shape, PUSH_FRAMES), np.int32)

    for li in range(data.num_levels):
        reference.set_level(li)
        walls = reference.hasivfwip
        slots = [i for i in range(data.num_slots) if data.alive[li, i] and data.tags[li, i, data.tag_index(TAG_PUSHABLE)]]
        by_pos = {(data.names[li][i], int(data.x[li, i]), int(data.y[li, i])): i for i in slots}
        for wall in walls:
            slot = by_pos[(wall.sprite.name, wall.sprite.x, wall.sprite.y)]
            distance = wall.ullzqnksoj(None)
            if distance <= 0:
                continue
            target_x = wall.start_x + wall.dx * wall.width * distance
            target_y = wall.start_y + wall.dy * wall.height * distance
            wall.aqxtoxeino = _DummyPlayer(wall.start_x, wall.start_y)
            wall.is_pushing = True
            wall.target_x = target_x
            wall.target_y = target_y
            wall.monctmxmpp(target_x, target_y, wall.qeekhxkoad)
            prev_x, prev_y = wall.sprite.x, wall.sprite.y
            for k in range(PUSH_FRAMES):
                wall.wgxrzqzazj()
                step_dx[li, slot, k] = wall.sprite.x - prev_x
                step_dy[li, slot, k] = wall.sprite.y - prev_y
                prev_x, prev_y = wall.sprite.x, wall.sprite.y
            assert wall.sprite.x == wall.start_x and wall.sprite.y == wall.start_y
            is_pushable[li, slot] = True
    return is_pushable, step_dx, step_dy


def _patrol_tables(
    data: GameData, reference
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    shape = (data.num_levels, MAX_PATROLS)
    patrol_slot = np.full(shape, -1, np.int32)
    area_slot = np.full(shape, -1, np.int32)
    start_x = np.zeros(shape, np.int32)
    start_y = np.zeros(shape, np.int32)

    for li in range(data.num_levels):
        reference.set_level(li)
        patrols = reference.wsoslqeku
        assert len(patrols) <= MAX_PATROLS
        slots = list(range(data.num_slots))
        for k, patrol in enumerate(patrols):
            psprite, asprite = patrol._sprite, patrol.bfdcztirdu
            pslot = next(
                i for i in slots if data.alive[li, i] and data.names[li][i] == psprite.name and data.x[li, i] == psprite.x and data.y[li, i] == psprite.y
            )
            aslot = next(
                i for i in slots if data.alive[li, i] and data.names[li][i] == asprite.name and data.x[li, i] == asprite.x and data.y[li, i] == asprite.y
            )
            patrol_slot[li, k] = pslot
            area_slot[li, k] = aslot
            start_x[li, k] = psprite.x
            start_y[li, k] = psprite.y
    return patrol_slot, area_slot, start_x, start_y


def _goal_tables(data: GameData, reference):
    shape = (data.num_levels, MAX_GOALS)
    goal_slot = np.full(shape, -1, np.int32)
    marker_slot = np.full(shape, -1, np.int32)
    ring_slot = np.full(shape, -1, np.int32)
    frame_slot = np.full(shape, -1, np.int32)
    is_final = np.zeros(shape, bool)
    want_shape = np.zeros(shape, np.int32)
    want_color = np.zeros(shape, np.int32)
    want_rot = np.zeros(shape, np.int32)
    goal_x = np.full(shape, -1000, np.int32)
    goal_y = np.full(shape, -1000, np.int32)
    num_goals = np.zeros(data.num_levels, np.int32)

    ring_tag = data.tag_index(TAG_HINT_RING)
    frame_tag = data.tag_index(TAG_GOAL_FRAME)
    final_tag = data.tag_index(TAG_GOAL_FRAME_FINAL)

    for li in range(data.num_levels):
        goal_slots = data.slots_with_tag(li, TAG_GOAL)
        marker_slots = data.slots_with_tag(li, TAG_GOAL_MARKER)
        assert len(goal_slots) == len(marker_slots) <= MAX_GOALS
        num_goals[li] = len(goal_slots)

        gcolor = data.level_data[li]["GoalColor"]
        grot = data.level_data[li]["GoalRotation"]
        gshape = data.level_data[li][TAG_GOAL_MARKER]
        gcolor = gcolor if isinstance(gcolor, list) else [gcolor]
        grot = grot if isinstance(grot, list) else [grot]
        gshape = gshape if isinstance(gshape, list) else [gshape]

        for g, (gs, ms) in enumerate(zip(goal_slots, marker_slots)):
            goal_slot[li, g] = gs
            marker_slot[li, g] = ms
            goal_x[li, g] = data.x[li, gs]
            goal_y[li, g] = data.y[li, gs]
            want_shape[li, g] = gshape[g]
            want_color[li, g] = reference.tnkekoeuk.index(gcolor[g])
            want_rot[li, g] = reference.dhksvilbb.index(grot[g])
            tx, ty = int(data.x[li, gs]) - 1, int(data.y[li, gs]) - 1
            for slot in range(data.num_slots):
                if not data.alive[li, slot] or data.x[li, slot] != tx or data.y[li, slot] != ty:
                    continue
                if data.tags[li, slot, ring_tag]:
                    ring_slot[li, g] = slot
                if data.tags[li, slot, frame_tag]:
                    frame_slot[li, g] = slot
                    is_final[li, g] = bool(data.tags[li, slot, final_tag])
    return goal_slot, marker_slot, ring_slot, frame_slot, is_final, want_shape, want_color, want_rot, goal_x, goal_y, num_goals


DATA_PARAM = 'data'


def derive(data, source=str(source_for('ls20'))):
    self = Setup(data)
    reference = load_reference_game(source)
    _bake_goal_markers(data, reference)
    variants = _loadout_variants(reference)
    is_pushable, step_dx, step_dy = _push_tables(data, reference)
    patrol_slot, area_slot, patrol_start_x, patrol_start_y = _patrol_tables(data, reference)
    (
        goal_slot,
        marker_slot,
        ring_slot,
        frame_slot,
        is_final,
        want_shape,
        want_color,
        want_rot,
        goal_x,
        goal_y,
        num_goals,
    ) = _goal_tables(data, reference)
    flash_slot = data.num_slots - 1
    data.pixels[:, flash_slot] = -1
    data.pixels[:, flash_slot, :, :] = COLOR_DEATH_FLASH
    data.h[:, flash_slot] = data.patch_shape[0]
    data.w[:, flash_slot] = data.patch_shape[1]
    data.x[:, flash_slot] = 0
    data.y[:, flash_slot] = 0
    data.layer[:, flash_slot] = 3
    data.alive[:, flash_slot] = True
    data.blocking[:, flash_slot] = int(Blocking.PIXEL_PERFECT)
    data.interaction[:, flash_slot] = int(Interaction.INVISIBLE)
    levels = range(data.num_levels)
    self._htk_variant = np.asarray(variants)
    self._is_pushable = np.asarray(is_pushable)
    self._wall_step_dx = np.asarray(step_dx)
    self._wall_step_dy = np.asarray(step_dy)
    self._patrol_slot = np.asarray(patrol_slot)
    self._patrol_area_slot = np.asarray(area_slot)
    self._patrol_start_x = np.asarray(patrol_start_x)
    self._patrol_start_y = np.asarray(patrol_start_y)
    self._goal_slot = np.asarray(goal_slot)
    self._marker_slot = np.asarray(marker_slot)
    self._ring_slot = np.asarray(ring_slot)
    self._frame_slot = np.asarray(frame_slot)
    self._goal_is_final = np.asarray(is_final)
    self._want_shape = np.asarray(want_shape)
    self._want_color = np.asarray(want_color)
    self._want_rot = np.asarray(want_rot)
    self._goal_x = np.asarray(goal_x)
    self._goal_y = np.asarray(goal_y)
    self._num_goals = np.asarray(num_goals)
    goal_index = np.full((data.num_levels, data.num_slots), -1, np.int32)
    for li in levels:
        for g in range(MAX_GOALS):
            s = goal_slot[li, g]
            if s >= 0:
                goal_index[li, s] = g
    self._goal_index = np.asarray(goal_index)
    tag_code = np.zeros((data.num_levels, data.num_slots), np.int32)
    for name, code in (
        (TAG_HAZARD, CODE_HAZARD),
        (TAG_GOAL, CODE_GOAL),
        (TAG_REFILL, CODE_REFILL),
        (TAG_BTN_SHAPE, CODE_BTN_SHAPE),
        (TAG_BTN_COLOR, CODE_BTN_COLOR),
        (TAG_BTN_ROTATION, CODE_BTN_ROTATION),
    ):
        col = data.tag_index(name)
        tag_code[data.tags[:, :, col]] = code
    self._tag_code = np.asarray(tag_code)
    restorable = np.zeros((data.num_levels, data.num_slots), bool)
    for name in (TAG_REFILL, TAG_GOAL, TAG_GOAL_MARKER):
        restorable |= data.tags[:, :, data.tag_index(name)]
    self._restorable = np.asarray(restorable)
    self._player_slot = np.asarray([data.slots_with_tag(li, TAG_PLAYER)[0] for li in levels], np.int32)
    self._htk_slot = np.asarray([data.slots_with_tag(li, TAG_LOADOUT)[0] for li in levels], np.int32)
    self._htk2_slot = np.asarray([data.slots_with_tag(li, TAG_LOADOUT_FRAME)[0] for li in levels], np.int32)
    self._hint_glow_slot = np.asarray([data.slots_with_tag(li, TAG_HINT_GLOW)[0] for li in levels], np.int32)
    self._hint_ring_tag = data.tag_index(TAG_HINT_RING)
    self._goal_frame_tag = data.tag_index(TAG_GOAL_FRAME)
    self._pitch_x = np.asarray(data.w)[np.arange(data.num_levels), self._player_slot]
    self._pitch_y = np.asarray(data.h)[np.arange(data.num_levels), self._player_slot]
    self._player_start_x = np.asarray(data.x)[np.arange(data.num_levels), self._player_slot]
    self._player_start_y = np.asarray(data.y)[np.arange(data.num_levels), self._player_slot]
    self._budget = np.asarray([d["StepCounter"] for d in data.level_data], np.int32)
    self._decrement = np.asarray([d.get("StepsDecrement", 2) for d in data.level_data], np.int32)
    self._fog = np.asarray([bool(d.get("Fog")) for d in data.level_data])
    self._start_shape = np.asarray([d["StartShape"] for d in data.level_data], np.int32)
    self._start_color = np.asarray(
        [reference.tnkekoeuk.index(d["StartColor"]) for d in data.level_data], np.int32
    )
    self._start_rot = np.asarray(
        [reference.dhksvilbb.index(d["StartRotation"]) for d in data.level_data], np.int32
    )
    self._flash_slot = np.int32(flash_slot)
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {'source': source}
