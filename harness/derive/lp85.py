import sys
import numpy as np
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


TAG_PIECE_A = "bghvgbtwcb"


TAG_PIECE_B = "fdgmtkfrxl"


TAG_GOAL_A = "goal"


TAG_GOAL_B = "goal-o"


CELL_PITCH = 3


FILLED_COLOUR = 5


EMPTY_COLOUR = 14


def _ring_tables(
    data: GameData,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    source = sorted(reference_dir().glob(f"{data.game_id}-*.py"))[0]
    reference = load_reference_game(source)
    module = sys.modules[type(reference).__module__]
    ring_pairs_for = module.chmfaflqhy
    ring_config = module.qfvvosdkqr(module.izutyjcpih)

    num_levels, num_slots = data.num_levels, data.num_slots
    button_cols = [i for i, name in enumerate(data.tag_names) if name.startswith("button_")]

    per_slot: dict[tuple[int, int], list[tuple[int, int, int, int]]] = {}
    max_ring = 0
    for li in range(num_levels):
        level_name = data.level_data[li]["level_name"]
        for si in range(num_slots):
            if not data.alive[li, si]:
                continue
            hits = [c for c in button_cols if data.tags[li, si, c]]
            if not hits:
                continue
            assert len(hits) == 1, "a slot should carry at most one button tag"
            tag = data.tag_names[hits[0]]
            _, map_name, direction = tag.split("_")
            pairs = ring_pairs_for(level_name, map_name, direction == "R", ring_config)
            per_slot[li, si] = [
                (frm.x * CELL_PITCH, frm.y * CELL_PITCH, to.x * CELL_PITCH, to.y * CELL_PITCH)
                for frm, to in pairs
            ]
            max_ring = max(max_ring, len(pairs))

    max_ring = max(max_ring, 1)
    shape = (num_levels, num_slots)
    is_button = np.zeros(shape, bool)
    ring_len = np.zeros(shape, np.int32)
    from_x = np.zeros((*shape, max_ring), np.int32)
    from_y = np.zeros((*shape, max_ring), np.int32)
    to_x = np.zeros((*shape, max_ring), np.int32)
    to_y = np.zeros((*shape, max_ring), np.int32)
    for (li, si), pairs in per_slot.items():
        is_button[li, si] = True
        ring_len[li, si] = len(pairs)
        for k, (fx, fy, tx, ty) in enumerate(pairs):
            from_x[li, si, k] = fx
            from_y[li, si, k] = fy
            to_x[li, si, k] = tx
            to_y[li, si, k] = ty
    return is_button, ring_len, from_x, from_y, to_x, to_y


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    is_button, ring_len, from_x, from_y, to_x, to_y = _ring_tables(data)
    self._is_button = np.asarray(is_button)
    self._ring_len = np.asarray(ring_len)
    self._ring_from_x = np.asarray(from_x)
    self._ring_from_y = np.asarray(from_y)
    self._ring_to_x = np.asarray(to_x)
    self._ring_to_y = np.asarray(to_y)
    self._max_ring = from_x.shape[-1]
    self._budget = np.asarray([d["StepCounter"] for d in data.level_data], np.int32)
    self._piece_a = data.tag_index(TAG_PIECE_A)
    self._piece_b = data.tag_index(TAG_PIECE_B)
    self._goal_a = data.tag_index(TAG_GOAL_A)
    self._goal_b = data.tag_index(TAG_GOAL_B)
    return self


def make_args(source, seed=0):
    return (extract(source),), {}
