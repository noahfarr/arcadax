import numpy as np
from ._base import GameData, Setup, extract, load_reference_game


PITCH = 4


STAMP_MARKER = 6


BAR_FILLED, BAR_EMPTY = 12, 11


HINT_COLOURS = (0, 2)


HINT_FRAMES = 4


TAG_PLAIN = "Hkx"


TAG_PATTERN = "NTi"


TAG_CLUE = "bsT"


TAG_HINT = "Ycb"


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    self._plain = data.tag_index(TAG_PLAIN)
    self._pattern = data.tag_index(TAG_PATTERN)
    self._clue = data.tag_index(TAG_CLUE)
    levels = range(data.num_levels)
    self._budget = np.asarray(
        [d["kCv"] for d in data.level_data], np.int32
    )
    palettes = [d.get("cwU") or [9, 8] for d in data.level_data]
    width = max(len(p) for p in palettes)
    self._palette = np.asarray(
        [p + [-1] * (width - len(p)) for p in palettes], np.int32
    )
    self._palette_size = np.asarray([len(p) for p in palettes], np.int32)
    self._brush = np.asarray(
        [d.get("elp") or [[0, 0, 0], [0, 1, 0], [0, 0, 0]] for d in data.level_data],
        np.int32,
    )
    self._hint_slot = np.asarray(
        [
            (slots[0] if (slots := data.slots_with_tag(li, TAG_HINT)) else -1)
            for li in levels
        ],
        np.int32,
    )
    return self


def make_args(source, seed=0):
    return (extract(source),), {}
