import numpy as np
from ._const import Action, FRAME_SIZE
from ._base import Array, GameData, Setup, extract, load_reference_game


MOVER_NAMES = (
    "pikgci-toljda-leklkn",
    "pikgci-toljda-rivmdg",
    "pikgci-boweok-leklkn",
    "pikgci-boweok-rivmdg",
)


MOVER_SIGNS = np.array([[1, 1], [-1, 1], [1, -1], [-1, -1]], np.int32)


NUM_MOVERS = len(MOVER_NAMES)


DOOR_COLOURS = ("grwjuk", "orfrpe", "puvdux")


DOOR_NAME = "gayktr-{}"


SWITCH_NAME = "unobxw-{}"


BLOCK_NAME = "mosdlc"


TAG_MOVER = "fucr"


TAG_HAZARD = "spswjz"


TAG_WALL = "wahtyt"


TAG_BLOCK = "xbso"


TAG_CLICK = "sys_click"


MAX_ACTIONS = 150


BLOCK_IDLE, BLOCK_SELECTED = 9, 11


MOVER_IDLE, MOVER_BLOCKED = 10, 1


FLASH_ON, FLASH_OFF = 11, 10


FLASH_FRAMES = 7


HUD_COLOUR, HUD_EMPTY = 5, 0


OVERLAY_COLOUR = 5


MOVE_DELTAS = {Action.ACTION1: (0, -1), Action.ACTION2: (0, 1), Action.ACTION3: (-1, 0), Action.ACTION4: (1, 0)}


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    levels = range(data.num_levels)
    slots = range(data.num_slots)
    names = data.names
    def name_mask(predicate) -> Array:
        return np.asarray(
            [[predicate(names[li][si]) for si in slots] for li in levels]
        )
    self._mover_slot = np.asarray(
        [
            [
                next((si for si in slots if names[li][si] == mover), -1)
                for mover in MOVER_NAMES
            ]
            for li in levels
        ],
        np.int32,
    )
    self._mover_sign = np.asarray(MOVER_SIGNS)
    self._blocks = name_mask(lambda n: n == BLOCK_NAME)
    self._doors = np.stack(
        [name_mask(lambda n, c=c: n == DOOR_NAME.format(c)) for c in DOOR_COLOURS], 1
    )
    self._switches = np.stack(
        [name_mask(lambda n, c=c: n == SWITCH_NAME.format(c)) for c in DOOR_COLOURS], 1
    )
    self._any_door = np.any(self._doors, axis=1)
    tags = np.asarray(data.tags)
    alive = np.asarray(data.alive)
    tag = lambda name: tags[:, :, data.tag_index(name)] & alive
    self._walls = tag(TAG_WALL)
    self._hazards = tag(TAG_HAZARD)
    self._blocking_tags = tag(TAG_BLOCK)
    self._clickable = tag(TAG_CLICK)
    hazard_map = np.zeros((data.num_levels, FRAME_SIZE, FRAME_SIZE), bool)
    for li in levels:
        for si in data.slots_with_tag(li, TAG_HAZARD):
            hazard_map[li, data.y[li, si], data.x[li, si]] = True
    self._hazard_map = np.asarray(hazard_map)
    self._background = np.asarray(
        [d["psqw"][:2] for d in data.level_data], np.int32
    )
    self._clean_x = np.asarray(data.x)
    self._clean_y = np.asarray(data.y)
    return self


def make_args(source, seed=0):
    return (extract(source),), {}
