import sys
import numpy as np
from ._const import FRAME_SIZE
from ._base import GameData, Setup, extract, load_reference_game, source_for


STEP_PX = 3


CELL_PX = 6


TRAIN_RANGE_PX = 12


ATTACHED_COLOR = 11


HUD_FILLED = 6


BOARD_TAG = "0005uvnhiglpvh"


EXIT_TAG = "0015msvpvzxhqf"


PLAYER_TAG = "0017unajnymcki"


PUSH_TAG = "0001haidilggfh"


DRIFT_TAG = "0020npxxteirsg"


TRAIN_TAG = "0023otenflmryc"


MAX_QUEUE = 8


_SOURCE_PATH = str(source_for("tu93"))


_ROTATIONS = (0, 90, 180, 270)


def _rotated(base: np.ndarray, degrees: int) -> np.ndarray:
    k = int((-degrees % 360) / 90)
    return np.rot90(base, k=k) if k else base.copy()


def _flag_rc(h: int, w: int, degrees: int) -> tuple[int, int]:
    idx = np.arange(h * w).reshape(h, w)
    rotated = _rotated(idx, degrees)
    row, col = np.argwhere(rotated == 1)[0]
    return int(row), int(col)


def _build_static_tables(source, data: GameData):
    ref = load_reference_game(source)
    module = sys.modules[type(ref).__module__]
    ph, pw = data.patch_shape
    num_levels, num_slots = data.num_levels, data.num_slots

    def pad(px: np.ndarray) -> np.ndarray:
        out = np.full((ph, pw), -1, np.int8)
        out[: px.shape[0], : px.shape[1]] = px
        return out

    template5_rotvar = np.stack([pad(_rotated(module.sprites["0002ebnnauydmr"].pixels, d)) for d in _ROTATIONS])
    template7_rotvar = np.stack([pad(_rotated(module.sprites["0003zgknydacap"].pixels, d)) for d in _ROTATIONS])
    flag_pos3 = np.array([_flag_rc(3, 3, d) for d in _ROTATIONS], np.int32)
    flag_pos5 = np.array([_flag_rc(5, 5, d) for d in _ROTATIONS], np.int32)
    flag_pos7 = np.array([_flag_rc(7, 7, d) for d in _ROTATIONS], np.int32)

    init_rotation = np.zeros((num_levels, num_slots), np.int32)
    board_xy = np.zeros((num_levels, 2), np.int32)
    walkable = np.zeros((num_levels, FRAME_SIZE, FRAME_SIZE), bool)
    base3_rotvar = np.full((num_levels, num_slots, 4, ph, pw), -1, np.int8)

    for li, level in enumerate(ref._clean_levels):
        by_name = {s.name: s for s in level.get_sprites()}
        for si in range(num_slots):
            name = data.names[li][si]
            if not name:
                continue
            sprite = by_name[name]
            init_rotation[li, si] = int(sprite.rotation)
            if sprite.pixels.shape == (3, 3):
                for ridx, degrees in enumerate(_ROTATIONS):
                    base3_rotvar[li, si, ridx] = pad(_rotated(sprite.pixels, degrees))
        board = level.get_sprites_by_tag(BOARD_TAG)[0]
        board_xy[li] = (board.x, board.y)
        ys, xs = np.where(board.pixels == 2)
        walkable[li, board.y + ys, board.x + xs] = True

    return {
        "template5_rotvar": template5_rotvar,
        "template7_rotvar": template7_rotvar,
        "base3_rotvar": base3_rotvar,
        "flag_pos3": flag_pos3,
        "flag_pos5": flag_pos5,
        "flag_pos7": flag_pos7,
        "init_rotation": init_rotation,
        "board_xy": board_xy,
        "walkable": walkable,
    }


DATA_PARAM = 'data'


def derive(data, source=None):
    self = Setup(data)
    tables = _build_static_tables(source or _SOURCE_PATH, data)
    self._template5_rotvar = np.asarray(tables["template5_rotvar"])
    self._template7_rotvar = np.asarray(tables["template7_rotvar"])
    self._base3_rotvar = np.asarray(tables["base3_rotvar"])
    self._flag_pos3 = np.asarray(tables["flag_pos3"])
    self._flag_pos5 = np.asarray(tables["flag_pos5"])
    self._flag_pos7 = np.asarray(tables["flag_pos7"])
    self._init_rotation = np.asarray(tables["init_rotation"])
    self._board_xy = np.asarray(tables["board_xy"])
    self._walkable = np.asarray(tables["walkable"])
    tags = np.asarray(data.tags)
    alive = np.asarray(data.alive)
    tag = lambda name: tags[:, :, data.tag_index(name)] & alive
    self._is_board = tag(BOARD_TAG)
    self._is_exit = tag(EXIT_TAG)
    self._is_player = tag(PLAYER_TAG)
    self._is_push = tag(PUSH_TAG)
    self._is_drift = tag(DRIFT_TAG)
    self._is_train = tag(TRAIN_TAG)
    self._is_crate = self._is_push | self._is_drift | self._is_train
    levels = range(data.num_levels)
    self._player_slot = np.asarray(
        [data.slots_with_tag(li, PLAYER_TAG)[0] for li in levels], np.int32
    )
    self._own_color = np.asarray(data.pixels[:, :, 0, 0])
    self._budget = np.asarray(
        [d.get("StepCounter") or 0 for d in data.level_data], np.int32
    )
    return self


def make_args(source, seed=0):
    return (extract(source),), {'source': source}
