import numpy as np
from ._const import Blocking, Interaction
from ._base import GameData, Setup, extract, load_reference_game


G = 3


K = 4


NCOMP = 2


NPIECES = 7


MAX_ACTIONS = 60


MAX_HAZARD_HITS = 5


WOBBLE_TICKS = 20


PIECE_IDLE_COLOUR, PIECE_SELECTED_COLOUR = 3, 0


BUDGET_FILLED, BUDGET_EMPTY = 0, 5


GUIDE_COLOUR = 1


GUIDE_TARGET_A, GUIDE_TARGET_B = 5, 10


LINE_STEPS = 140


TAG_CLICK = "sys_click"


_WOBBLE_ICON_PX = np.array(
    [
        [-1, 0, 0, 0, -1],
        [0, 0, 0, 0, 0],
        [0, 0, -1, 0, 0],
        [0, 0, 0, 0, 0],
        [-1, 0, 0, 0, -1],
    ],
    dtype=np.int8,
)


_BLINK_ICON_PX = np.array(
    [
        [-1, -1, 0, -1, 0, -1, -1],
        [-1, 0, -1, -1, -1, 0, -1],
        [0, -1, -1, -1, -1, -1, 0],
        [-1, -1, -1, -1, -1, -1, -1],
        [0, -1, -1, -1, -1, -1, 0],
        [-1, 0, -1, -1, -1, 0, -1],
        [-1, -1, 0, -1, 0, -1, -1],
    ],
    dtype=np.int8,
)


_WOBBLE_LAYER = 100


_BLINK_LAYER = 0


EXTRACT_KWARGS = {"extra_slots": K + 1}


def _inject_icon_templates(data: GameData) -> GameData:
    icon_base = data.num_slots - (K + 1)
    wobble_slot = data.num_slots - 1

    def place(slot: int, px: np.ndarray, layer: int) -> None:
        h, w = px.shape
        data.pixels[:, slot] = -1
        data.pixels[:, slot, :h, :w] = px
        data.h[:, slot] = h
        data.w[:, slot] = w
        data.layer[:, slot] = layer
        data.interaction[:, slot] = int(Interaction.TANGIBLE)
        data.blocking[:, slot] = int(Blocking.PIXEL_PERFECT)

    for k in range(K):
        place(icon_base + k, _BLINK_ICON_PX, _BLINK_LAYER)
    place(wobble_slot, _WOBBLE_ICON_PX, _WOBBLE_LAYER)
    return data


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    data = _inject_icon_templates(data)
    self._icon_base = self.num_slots - (K + 1)
    self._wobble_icon_slot = self.num_slots - 1
    self._max_actions = MAX_ACTIONS
    L, N = data.num_levels, data.num_slots
    group_target_slot = np.full((L, G), -1, np.int32)
    group_is_composite = np.zeros((L, G), bool)
    group_piece_mask = np.zeros((L, G, N), bool)
    group_piece_count = np.zeros((L, G), np.int32)
    piece_group = np.full((L, N), -1, np.int32)
    pieces_order = np.full((L, NPIECES), -1, np.int32)
    key_clue_slot = np.full((L, K), -1, np.int32)
    key_target_slot = np.full((L, K), -1, np.int32)
    key_skip_win = np.zeros((L, K), bool)
    composite_slot = np.full((L, NCOMP), -1, np.int32)
    fragment_mask = np.zeros((L, N), bool)
    wall_mask = np.zeros((L, N), bool)
    hazard_mask = np.zeros((L, N), bool)
    clue_colour_set = np.zeros((L, N, 15), bool)
    for li in range(L):
        names = data.names[li]
        target_slot_of: dict[str, int] = {}
        for si, n in enumerate(names):
            if n.startswith("roefwu-") and not n.startswith("roefwulewcui-"):
                target_slot_of[n[len("roefwu-") :]] = si
    
        group_keys = sorted(target_slot_of)
        if len(group_keys) > G:
            raise ValueError(f"level {li} has {len(group_keys)} groups > G={G}")
        for gi, key in enumerate(group_keys):
            slot = target_slot_of[key]
            group_target_slot[li, gi] = slot
            group_is_composite[li, gi] = key.startswith("whkxtx")
            pieces = [si for si, n in enumerate(names) if n == f"roefwulewcui-{key}"]
            for si in pieces:
                group_piece_mask[li, gi, si] = True
                piece_group[li, si] = gi
            group_piece_count[li, gi] = max(len(pieces), 1)
    
        comp = sorted(si for si, n in enumerate(names) if n.startswith("roefwu-whkxtx"))
        if len(comp) > NCOMP:
            raise ValueError(f"level {li} has {len(comp)} composites > NCOMP={NCOMP}")
        for ci, si in enumerate(comp):
            composite_slot[li, ci] = si
    
        fragment_mask[li] = [n.startswith("puukul-") for n in names]
        wall_mask[li] = [n.startswith("wakneh-") for n in names]
        hazard_mask[li] = [n.startswith("defgjl") for n in names]
    
        flkdtg_keys = sorted({n[len("flkdtg-") :] for n in names if n.startswith("flkdtg-")})
        if len(flkdtg_keys) > K:
            raise ValueError(f"level {li} has {len(flkdtg_keys)} clue keys > K={K}")
        for ki, key in enumerate(flkdtg_keys):
            slot = min(si for si, n in enumerate(names) if n == f"flkdtg-{key}")
            key_clue_slot[li, ki] = slot
            key_skip_win[li, ki] = "dirwzt" in key
            key_target_slot[li, ki] = target_slot_of.get(key, -1)
            px = data.pixels[li, slot]
            for c in range(1, 16):
                clue_colour_set[li, slot, c - 1] = bool(np.any(px == c))
    
        piece_slots = [si for si, n in enumerate(names) if n.startswith("roefwulewcui-")]
        if len(piece_slots) > NPIECES:
            raise ValueError(f"level {li} has {len(piece_slots)} pieces > NPIECES={NPIECES}")
        piece_slots.sort(key=lambda si: (float(data.x[li, si]) ** 2 + float(data.y[li, si]) ** 2) ** 0.5)
        for pi, si in enumerate(piece_slots):
            pieces_order[li, pi] = si
    self._group_target_slot = np.asarray(group_target_slot)
    self._group_is_composite = np.asarray(group_is_composite)
    self._group_piece_mask = np.asarray(group_piece_mask)
    self._group_piece_count = np.asarray(group_piece_count)
    self._piece_group = np.asarray(piece_group)
    self._pieces_order = np.asarray(pieces_order)
    self._key_clue_slot = np.asarray(key_clue_slot)
    self._key_target_slot = np.asarray(key_target_slot)
    self._key_skip_win = np.asarray(key_skip_win)
    self._composite_slot = np.asarray(composite_slot)
    self._fragment_mask = np.asarray(fragment_mask)
    self._wall_mask = np.asarray(wall_mask)
    self._hazard_mask = np.asarray(hazard_mask)
    self._clue_colour_set = np.asarray(clue_colour_set)
    tag_click = data.tag_index(TAG_CLICK)
    self._piece_mask = np.asarray(data.tags[:, :, tag_click] & data.alive)
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
