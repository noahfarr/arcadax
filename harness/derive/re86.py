import sys
import numpy as np
from ._base import GameData, Setup, extract, load_reference_game, source_for


STEP_PX = 3


MIN_RESHAPE_DIM = 6


ACTIVE_MARKER = 0


CURSOR_TAG = "0031cppcuvqlbi"


RESHAPE_TAG = "0036ilsgwuvbxv"


SOLID_CENTER_TAG = "0049ppblgltcfi"


WALL_TAG = "0003dlchiwseii"


FLOOD_TARGET_TAG = "0007dtbisvazhv"


WIN_TARGET_TAG = "0054xnsuqceejm"


_SOURCE_PATH = str(source_for("re86"))


def _build_static_tables(source, data: GameData):
    ref = load_reference_game(source)
    module = sys.modules[type(ref).__module__]
    ph, pw = data.patch_shape

    canvas = module.sprites["0000wbshgbbxxc"].pixels
    canvas_template = np.full((ph, pw), -1, np.int8)
    canvas_template[: canvas.shape[0], : canvas.shape[1]] = canvas

    return {"canvas_template": canvas_template}


DATA_PARAM = 'data'


def derive(data, source=None):
    self = Setup(data)
    tables = _build_static_tables(source or _SOURCE_PATH, data)
    self._canvas_template = np.asarray(tables["canvas_template"])
    tags = np.asarray(data.tags)
    alive = np.asarray(data.alive)
    tag = lambda name: tags[:, :, data.tag_index(name)] & alive
    self._is_cursor = tag(CURSOR_TAG)
    self._is_reshape = tag(RESHAPE_TAG)
    self._is_solid_center = tag(SOLID_CENTER_TAG)
    self._is_wall = tag(WALL_TAG)
    self._is_flood_target = tag(FLOOD_TARGET_TAG)
    self._is_win_target = tag(WIN_TARGET_TAG)
    levels = range(data.num_levels)
    slots = range(data.num_slots)
    cursor_slots = [
        [si for si in slots if data.alive[li, si] and data.tags[li, si, data.tag_index(CURSOR_TAG)]]
        for li in levels
    ]
    max_cursors = max(len(c) for c in cursor_slots)
    self._num_cursors = np.asarray([len(c) for c in cursor_slots], np.int32)
    self._cursor_slot_by_rank = np.asarray(
        [c + [-1] * (max_cursors - len(c)) for c in cursor_slots], np.int32
    )
    cursor_rank = np.zeros((data.num_levels, data.num_slots), np.int32)
    for li in levels:
        for rank, si in enumerate(cursor_slots[li]):
            cursor_rank[li, si] = rank
    self._cursor_rank = np.asarray(cursor_rank)
    self._budget = np.asarray(
        [d.get("StepCounter") or 0 for d in data.level_data], np.int32
    )
    return self


def make_args(source, seed=0):
    return (extract(source),), {'source': source}
