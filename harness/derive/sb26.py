import numpy as np
from ._const import Blocking, Interaction
from ._base import GameData, Setup, extract, load_reference_game


MAX_FRAMES = 4


MAX_CARDS = 12


MAX_STACK = 6


MAX_TRAY = 9


MAX_UNDO = 65


CURSOR_SLOTS = MAX_STACK - 1


PITCH = 6


SLOT_ORIGIN = 2


INITIAL_ENERGY = 64


TWEEN_STEPS = 6


BACKGROUND_COLOR = 4


Y_THRESHOLD = 53


HUD_EMPTY, HUD_FILLED = 3, 2


BLACK, RED, GREY, PINK = 0, 8, 3, 5


TAG_ITEM = "lngftsryyw"


TAG_FRAME = "pkpgflvjel"


TAG_SPOT = "susublrply"


TAG_CLICK = "sys_click"


NAME_FRAME_REF = "vgszefyyyp"


NAME_CARD = "quhhhthrri"


NAME_BG = "uzxwqmkrmk"


NAME_REMOVE_ON_LOAD = "zpwrpmkvsv"


EXTRA_SLOTS = MAX_TRAY + 6 + CURSOR_SLOTS


EXTRACT_KWARGS = {"extra_slots": EXTRA_SLOTS}


MIN_PATCH_H = 15


MIN_PATCH_W = 64


_JKXNCPVKNR = np.array(
    [
        [0, 0, 0, 0, 0, 0, 0, 0],
        [0, -1, -1, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, -1, -1, 0],
        [0, 0, 0, 0, 0, 0, 0, 0],
    ],
    dtype=np.int8,
)


_LNZHVCAGOS = np.array(
    [[0, 0, 0] + [-1] * 42 + [0, 0, 0]]
    + [[0] + [-1] * 46 + [0]] * 2
    + [[-1] * 48] * 6
    + [[0] + [-1] * 46 + [0]] * 2
    + [[0, 0, 0] + [-1] * 42 + [0, 0, 0]],
    dtype=np.int8,
)


_WRQPMMFHUP = np.array(
    [
        [0, 0, 0, 0, 0, 0],
        [0, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, 0],
        [0, -1, -1, -1, -1, 0],
        [0, 0, 0, 0, 0, 0],
    ],
    dtype=np.int8,
)


_UPBHQNVNYX = np.array(
    [
        [3, 3, 3, 3, 3, 3],
        [3, -1, -1, -1, -1, 3],
        [3, -1, -1, -1, -1, 3],
        [3, -1, -1, -1, -1, 3],
        [3, -1, -1, -1, -1, 3],
        [3, 3, 3, 3, 3, 3],
    ],
    dtype=np.int8,
)


_OFLGSLMUKU = np.array(
    [[8] * 64] + [[8] + [-1] * 62 + [8]] * 6 + [[8] * 64],
    dtype=np.int8,
)


def _jwaufhyryn() -> np.ndarray:
    px = np.full((15, 43), -1, np.int8)
    px[0, 28:] = 8
    px[1:7, 28] = 8
    px[1:7, 42] = 8
    px[7, :] = 8
    px[7:, 0] = 8
    px[7:, 42] = 8
    px[14, :] = 8
    return px


_JWAUFHYRYN = _jwaufhyryn()


def _patch_data(data: GameData) -> GameData:
    n_static = data.num_slots - EXTRA_SLOTS
    if n_static < 1:
        raise ValueError(
            f"sb26 needs extra_slots={EXTRA_SLOTS}; got {data.num_slots} slots. "
            "Extract via EXTRACT_KWARGS (delete any stale sb26.npz from the cache dir)."
        )
    ph, pw = data.patch_shape
    new_ph, new_pw = max(ph, MIN_PATCH_H), max(pw, MIN_PATCH_W)
    if (new_ph, new_pw) != (ph, pw):
        pixels = np.full((data.num_levels, data.num_slots, new_ph, new_pw), -1, np.int8)
        pixels[:, :, :ph, :pw] = data.pixels
        data.pixels = pixels

    spot_col = data.tag_index(TAG_SPOT)

    def place(level: int, slot: int, px: np.ndarray, layer: int, tags: tuple[str, ...] = ()) -> None:
        h, w = px.shape
        data.pixels[level, slot] = -1
        data.pixels[level, slot, :h, :w] = px
        data.h[level, slot] = h
        data.w[level, slot] = w
        data.layer[level, slot] = layer
        data.blocking[level, slot] = int(Blocking.PIXEL_PERFECT)


        data.interaction[level, slot] = int(Interaction.INVISIBLE)
        for tag in tags:
            data.tags[level, slot, data.tag_index(tag)] = True
        data.names[level][slot] = ""

    for level in range(data.num_levels):


        spot_source = next(
            i for i in range(data.num_slots) if data.alive[level, i] and data.tags[level, i, spot_col]
        )
        spot_px = data.pixels[level, spot_source, : data.h[level, spot_source], : data.w[level, spot_source]]
        for k in range(MAX_TRAY):
            place(level, n_static + k, spot_px, layer=0, tags=(TAG_SPOT, TAG_CLICK))
        place(level, n_static + 9, _WRQPMMFHUP, layer=1)
        place(level, n_static + 10, _WRQPMMFHUP, layer=0)
        place(level, n_static + 11, _LNZHVCAGOS, layer=10)
        place(level, n_static + 12, _WRQPMMFHUP, layer=10)
        place(level, n_static + 13, _JKXNCPVKNR, layer=10)
        num_cards = sum(1 for name in data.names[level] if name == NAME_CARD)
        banner = _JWAUFHYRYN if num_cards >= 12 else _OFLGSLMUKU
        place(level, n_static + 14, banner, layer=12)
        for k in range(CURSOR_SLOTS):
            place(level, n_static + 15 + k, _UPBHQNVNYX, layer=0)
    return data


def _tween_value_bounds(data: GameData) -> tuple[int, int]:
    x, y, w = data.x.astype(np.int64), data.y.astype(np.int64), data.w.astype(np.int64)
    candidates = np.concatenate(
        [(x - 1).ravel(), x.ravel(), (y - 1).ravel(), y.ravel(), w.ravel(), (w + 2).ravel()]
    )
    return int(candidates.min()), int(candidates.max())


def _build_tween_table(val_min: int, val_max: int) -> np.ndarray:
    width = val_max - val_min + 1
    table = np.zeros((TWEEN_STEPS + 1, width, width), np.int32)
    for k in range(TWEEN_STEPS + 1):
        t = k / TWEEN_STEPS
        eased = 1 - (1 - t) * (1 - t)
        for ai in range(width):
            a = ai + val_min
            for bi in range(width):
                b = bi + val_min
                table[k, ai, bi] = round(a + eased * (b - a))
    return table


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    data = _patch_data(data)
    n_static = data.num_slots - EXTRA_SLOTS
    self.TRAY_GHOST_BASE = n_static
    self.MROKWHYJS0 = n_static + 9
    self.MROKWHYJS1 = n_static + 10
    self.MJEQTDQVM = n_static + 11
    self.AYAIGJTXP = n_static + 12
    self.OHVAVDNIO = n_static + 13
    self.OYVBXWYUG = n_static + 14
    self.CURSOR_BASE = n_static + 15
    self._item_tag = data.tag_index(TAG_ITEM)
    self._frame_tag = data.tag_index(TAG_FRAME)
    self._spot_tag = data.tag_index(TAG_SPOT)
    self._click_tag = data.tag_index(TAG_CLICK)
    L, N = data.num_levels, data.num_slots
    names = data.names
    qaagahahj = np.full((L, MAX_FRAMES), -1, np.int32)
    frame_children = np.zeros((L, N), np.int32)
    card_slots = np.full((L, MAX_CARDS), -1, np.int32)
    num_cards = np.zeros(L, np.int32)
    card_mask = np.zeros((L, N), bool)
    is_frameref = np.zeros((L, N), bool)
    tray_item_slot = np.full((L, MAX_TRAY), -1, np.int32)
    n_tray = np.zeros(L, np.int32)
    bg_mask = np.zeros((L, N), bool)
    zpwrpmkvsv_slot = np.full(L, -1, np.int32)
    for level in range(L):
        frame_idx = [
            i
            for i in range(N)
            if data.alive[level, i] and data.tags[level, i, self._frame_tag]
        ]
        frame_idx.sort(key=lambda i: (data.y[level, i], data.x[level, i]))
        for rank, i in enumerate(frame_idx[:MAX_FRAMES]):
            qaagahahj[level, rank] = i
            frame_children[level, i] = int(names[level][i][-1])
    
        card_idx = [i for i in range(N) if data.alive[level, i] and names[level][i] == NAME_CARD]
        card_idx.sort(key=lambda i: (data.y[level, i], data.x[level, i]))
        num_cards[level] = len(card_idx)
        for rank, i in enumerate(card_idx[:MAX_CARDS]):
            card_slots[level, rank] = i
            card_mask[level, i] = True
    
        item_idx = [
            i for i in range(N) if data.alive[level, i] and data.tags[level, i, self._item_tag]
        ]
        for i in item_idx:
            if names[level][i] == NAME_FRAME_REF:
                is_frameref[level, i] = True
        tray_idx = [i for i in item_idx if data.y[level, i] > Y_THRESHOLD]
        n_tray[level] = len(tray_idx)
        for rank, i in enumerate(tray_idx[:MAX_TRAY]):
            tray_item_slot[level, rank] = i
    
        for i in range(N):
            if data.alive[level, i] and names[level][i] == NAME_BG:
                bg_mask[level, i] = True
            if data.alive[level, i] and names[level][i] == NAME_REMOVE_ON_LOAD:
                zpwrpmkvsv_slot[level] = i
    self._qaagahahj = np.asarray(qaagahahj)
    self._frame_children = np.asarray(frame_children)
    self._card_slots = np.asarray(card_slots)
    self._num_cards = np.asarray(num_cards)
    self._card_mask = np.asarray(card_mask)
    self._is_frameref = np.asarray(is_frameref)
    self._tray_item_slot = np.asarray(tray_item_slot)
    self._n_tray = np.asarray(n_tray)
    self._bg_mask = np.asarray(bg_mask)
    self._zpwrpmkvsv_slot = np.asarray(zpwrpmkvsv_slot)
    allow_fixed = np.zeros(N, bool)
    for slot in (self.MROKWHYJS0, self.MROKWHYJS1, self.MJEQTDQVM, self.AYAIGJTXP, self.OHVAVDNIO):
        allow_fixed[slot] = True
    self._allow_fixed = np.asarray(allow_fixed)
    tween_val_min, tween_val_max = _tween_value_bounds(data)
    self._tween_val_min = tween_val_min
    self._tween_table = np.asarray(_build_tween_table(tween_val_min, tween_val_max))
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
