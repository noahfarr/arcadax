import numpy as np
from ._const import FRAME_SIZE
from ._base import GameData, Setup, extract, load_reference_game


PITCH = 4


GRID_N = FRAME_SIZE // PITCH


NUM_CELLS = GRID_N * GRID_N


TAG_PLAYER = "wbmdvjhthc"


TAG_BOX = "geezpjgiyd"


TAG_SEEKER = "kdweefinfi"


TAG_THIEF = "ysysltqlke"


TAG_HOLE = "bnzklblgdk"


TAG_WIN_TILE = "fsjjayjoeg"


TAG_THIEF_TILE = "zqxwgacnue"


BOX_HELD_BY_PLAYER = 0


BOX_FACED = 3


BOX_HELD = 5


BOX_IDLE = 4


THIEF_FACED = 11


THIEF_IDLE = 15


ROT_UP, ROT_RIGHT, ROT_DOWN, ROT_LEFT = 0, 90, 180, 270


_BFS_DIRS = ((-1, 0), (1, 0), (0, -1), (0, 1))


def _rot90_variants(patch: np.ndarray) -> np.ndarray:
    variants = np.zeros((4, *patch.shape), patch.dtype)
    for idx, degrees in enumerate((0, 90, 180, 270)):
        k = int((-degrees % 360) / 90)
        variants[idx] = np.rot90(patch, k=k)
    return variants


def _origin_grid(data: GameData, tag: str) -> np.ndarray:
    grid = np.zeros((data.num_levels, GRID_N, GRID_N), bool)
    col = data.tag_index(tag)
    for li in range(data.num_levels):
        for si in range(data.num_slots):
            if data.alive[li, si] and data.tags[li, si, col]:
                cx, cy = int(data.x[li, si]) // PITCH, int(data.y[li, si]) // PITCH
                if 0 <= cx < GRID_N and 0 <= cy < GRID_N:
                    grid[li, cy, cx] = True
    return grid


def _footprint_grid(data: GameData, tag: str) -> np.ndarray:
    grid = np.zeros((data.num_levels, GRID_N, GRID_N), bool)
    col = data.tag_index(tag)
    for li in range(data.num_levels):
        for si in range(data.num_slots):
            if data.alive[li, si] and data.tags[li, si, col]:
                x0, y0 = int(data.x[li, si]), int(data.y[li, si])
                w, h = int(data.w[li, si]), int(data.h[li, si])
                for yy in range(y0, y0 + h):
                    for xx in range(x0, x0 + w):
                        cx, cy = xx // PITCH, yy // PITCH
                        if 0 <= cx < GRID_N and 0 <= cy < GRID_N:
                            grid[li, cy, cx] = True
    return grid


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    self._is_box = np.asarray(data.tags[:, :, data.tag_index(TAG_BOX)])
    self._is_seeker = np.asarray(data.tags[:, :, data.tag_index(TAG_SEEKER)])
    self._is_thief = np.asarray(data.tags[:, :, data.tag_index(TAG_THIEF)])
    self._player_slot = np.asarray(
        [data.slots_with_tag(li, TAG_PLAYER)[0] for li in range(data.num_levels)], np.int32
    )
    self._budget = np.asarray([d["StepCounter"] for d in data.level_data], np.int32)
    self._hole_grid = np.asarray(_origin_grid(data, TAG_HOLE))
    self._fsj_grid = np.asarray(_footprint_grid(data, TAG_WIN_TILE))
    self._zqx_grid = np.asarray(_footprint_grid(data, TAG_THIEF_TILE))
    variants = np.zeros((data.num_levels, 4, PITCH, PITCH), data.pixels.dtype)
    for li in range(data.num_levels):
        slot = int(self._player_slot[li])
        variants[li] = _rot90_variants(data.pixels[li, slot, :PITCH, :PITCH])
    self._player_variants = np.asarray(variants)
    ph, pw = data.patch_shape
    rows = np.arange(ph)[:, None]
    cols = np.arange(pw)[None, :]
    border = (rows < PITCH) & (cols < PITCH) & ((rows == 0) | (rows == PITCH - 1) | (cols == 0) | (cols == PITCH - 1))
    self._border = np.asarray(border)
    return self


def make_args(source, seed=0):
    return (extract(source),), {}
