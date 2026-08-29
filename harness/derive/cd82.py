import sys
import numpy as np
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


STEP_BUDGET = 100


FILLED, EMPTY = 4, 5


RECOLOR_MARKER = 15


EXTRA_SLOTS = 2


EXTRACT_KWARGS = {"extra_slots": EXTRA_SLOTS}


RING = [
    (True, 25, 24, 180, 0, 1),
    (False, 33, 21, 0, -1, 1),
    (True, 38, 32, 270, -1, 0),
    (False, 33, 40, 90, -1, -1),
    (True, 25, 45, 0, 0, -1),
    (False, 14, 40, 180, 1, -1),
    (True, 17, 32, 90, 1, 0),
    (False, 14, 21, 270, 1, 1),
]


POS_TO_AB = [(0, 1), (0, 2), (1, 2), (2, 2), (2, 1), (2, 0), (1, 0), (0, 0)]


ARROW_INFO = {0: (4, -6, 180), 2: (10, 4, 270), 4: (4, 10, 0), 6: (-6, 4, 90)}


BOUNCE_DELTA = {0: (0, 2), 2: (-2, 0), 4: (0, -2), 6: (2, 0)}


def _find_source() -> str:
    matches = sorted(reference_dir().glob("cd82-*.py"))
    if not matches:
        raise RuntimeError(f"cd82 reference source not found under {reference_dir()}")
    return str(matches[0])


def _build_coublenfir_mask() -> np.ndarray:
    mask = np.zeros((8, 10, 10), bool)
    mask[0, 0:3, 3:7] = True
    mask[4, 7:10, 3:7] = True
    mask[6, 3:7, 0:3] = True
    mask[2, 3:7, 7:10] = True
    return mask


def _build_rtjwayrycq_mask(is_horizontal: bool, rotation: int) -> np.ndarray:
    mask = np.zeros((10, 10), bool)
    if is_horizontal:
        if rotation == 180:
            mask[0:5, :] = True
        elif rotation == 0:
            mask[5:10, :] = True
        elif rotation == 90:
            mask[:, 0:5] = True
        elif rotation == 270:
            mask[:, 5:10] = True
    else:
        for i in range(10):
            if rotation == 180:
                mask[i, 0 : i + 1] = True
            elif rotation == 90:
                mask[i, 9 - i : 10] = True
            elif rotation == 0:
                mask[i, i:10] = True
            elif rotation == 270:
                mask[i, 0 : 10 - i] = True
    return mask


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    L, N = data.num_levels, data.num_slots
    PH, PW = data.patch_shape
    original = N - EXTRA_SLOTS
    self._basket_slot = original
    self._arrow_slot = original + 1
    names = data.names
    alive = data.alive
    def name_mask(pred):
        return np.asarray(
            [[alive[li, si] and pred(names[li][si]) for si in range(N)] for li in range(L)]
        )
    palette_mask = name_mask(lambda n: n == "pqkenviek")
    canvas_mask = name_mask(lambda n: n == "xytrjjbyib")
    marker_mask = name_mask(lambda n: n == "ydiwkzjkgl")
    answer_mask = name_mask(lambda n: n.startswith("eoqnvkspoa-"))
    clean_removal = name_mask(lambda n: n.startswith("oaoosfneq-") or n == "ctwspzkygu")
    self._palette_mask = np.asarray(palette_mask)
    self._clean_removal_mask = np.asarray(clean_removal)
    def unique_slot(mask):
        out = np.full(L, -1, np.int32)
        for li in range(L):
            hits = np.flatnonzero(mask[li])
            if len(hits):
                out[li] = hits[0]
        return out
    self._canvas_slot = np.asarray(unique_slot(canvas_mask))
    self._answer_slot = np.asarray(unique_slot(answer_mask))
    self._marker_slot = np.asarray(unique_slot(marker_mask))
    has_arrow = np.array(
        [any(names[li][si] == "ctwspzkygu" for si in range(N) if alive[li, si]) for li in range(L)]
    )
    self._level_has_arrow = np.asarray(has_arrow)
    pos_x = np.array([r[1] for r in RING], np.int32)
    pos_y = np.array([r[2] for r in RING], np.int32)
    pos_rotation = np.array([r[3] for r in RING], np.int32)
    pos_dx = np.array([r[4] for r in RING], np.int32)
    pos_dy = np.array([r[5] for r in RING], np.int32)
    pos_is_horizontal = np.array([r[0] for r in RING], bool)
    self._pos_x = np.asarray(pos_x)
    self._pos_y = np.asarray(pos_y)
    self._pos_dx = np.asarray(pos_dx)
    self._pos_dy = np.asarray(pos_dy)
    self._pos_ab = np.asarray(np.array(POS_TO_AB, np.int32))
    grid_to_pos = np.full((3, 3), -1, np.int32)
    for pos, (a, b) in enumerate(POS_TO_AB):
        grid_to_pos[a, b] = pos
    self._grid_to_pos = np.asarray(grid_to_pos)
    arrow_exists = np.zeros(8, bool)
    arrow_offset_x = np.zeros(8, np.int32)
    arrow_offset_y = np.zeros(8, np.int32)
    arrow_rotation = np.zeros(8, np.int32)
    bounce_dx = np.zeros(8, np.int32)
    bounce_dy = np.zeros(8, np.int32)
    for pos in range(8):
        if pos in ARROW_INFO:
            arrow_exists[pos] = True
            ox, oy, rot = ARROW_INFO[pos]
            arrow_offset_x[pos] = ox
            arrow_offset_y[pos] = oy
            arrow_rotation[pos] = rot
        if pos in BOUNCE_DELTA:
            bdx, bdy = BOUNCE_DELTA[pos]
            bounce_dx[pos] = bdx
            bounce_dy[pos] = bdy
    self._arrow_exists = np.asarray(arrow_exists)
    self._arrow_x = np.asarray(pos_x + arrow_offset_x)
    self._arrow_y = np.asarray(pos_y + arrow_offset_y)
    self._bounce_dx = np.asarray(bounce_dx)
    self._bounce_dy = np.asarray(bounce_dy)
    ref = load_reference_game(_find_source())
    module = sys.modules[type(ref).__module__]
    templates = module.sprites
    basket_pixels = np.full((8, PH, PW), -1, np.int8)
    basket_is15 = np.zeros((8, PH, PW), bool)
    basket_h = np.zeros(8, np.int32)
    basket_w = np.zeros(8, np.int32)
    arrow_pixels = np.full((8, PH, PW), -1, np.int8)
    arrow_is15 = np.zeros((8, PH, PW), bool)
    arrow_h = np.zeros(8, np.int32)
    arrow_w = np.zeros(8, np.int32)
    def render_with_mask(template_name, rotation):
        tmpl = templates[template_name]
        base = tmpl.clone()
        base.set_rotation(rotation)
        base_render = np.asarray(base.render())
        marker = tmpl.clone()
        raw = np.asarray(marker.pixels)
        marker.pixels = np.where(raw == RECOLOR_MARKER, np.int8(1), np.int8(-1))
        marker.set_rotation(rotation)
        marker_render = np.asarray(marker.render())
        return base_render, marker_render == 1
    for pos in range(8):
        shape = "oaoosfneq-oaanwen" if pos_is_horizontal[pos] else "oaoosfneq-laopvne"
        px, is15 = render_with_mask(shape, int(pos_rotation[pos]))
        h, w = px.shape
        basket_pixels[pos, :h, :w] = px
        basket_is15[pos, :h, :w] = is15
        basket_h[pos], basket_w[pos] = h, w
    
        if arrow_exists[pos]:
            px, is15 = render_with_mask("ctwspzkygu", int(arrow_rotation[pos]))
            h, w = px.shape
            arrow_pixels[pos, :h, :w] = px
            arrow_is15[pos, :h, :w] = is15
            arrow_h[pos], arrow_w[pos] = h, w
    self._basket_pixels = np.asarray(basket_pixels)
    self._basket_is15 = np.asarray(basket_is15)
    self._basket_h = np.asarray(basket_h)
    self._basket_w = np.asarray(basket_w)
    self._arrow_pixels = np.asarray(arrow_pixels)
    self._arrow_is15 = np.asarray(arrow_is15)
    self._arrow_h = np.asarray(arrow_h)
    self._arrow_w = np.asarray(arrow_w)
    self._coublenfir_mask = np.asarray(_build_coublenfir_mask())
    self._rtjwayrycq_mask = np.asarray(
        np.stack([_build_rtjwayrycq_mask(pos_is_horizontal[p], int(pos_rotation[p])) for p in range(8)])
    )
    rows = np.arange(10)[:, None]
    cols = np.arange(10)[None, :]
    self._win_mask = np.asarray((rows != cols) & (rows != 9 - cols))
    return self


def make_args(source, seed=0):
    return (extract(source, extra_slots=EXTRA_SLOTS),), {}
