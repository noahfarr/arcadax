import sys
import numpy as np
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


TAG_MARKER = "0001uqqokjrptk"


TAG_BOMB = "0003umnkyodpjp"


TAG_HOLE = "0010xzmuziohuf"


TAG_WALL = "0015qniapgwsvb"


TAG_BOX = "0022vrxelxosfy"


TAG_ZONE = "0027jbgxilrocf"


TAG_BORDER = "0029ifoxxfvvvs"


PITCH = 3


RETRY_THRESHOLD = 5


HIGHLIGHT = 14


SELECTED_DOT = 0


DESELECTED_DOT = 4


FUSE_FILLED = 12


BOMB_SPENT = 13


MAX_BOMBS = 5


MAX_PUSH_DEPTH = 8


COLLIDER_SLOT_OFFSET = 0


SCRATCH_SLOT_OFFSET = 1


EXPLOSION_BASE_OFFSET = 2


EXTRA_SLOTS = EXPLOSION_BASE_OFFSET + 3 * MAX_BOMBS


MOVE_DELTAS = np.array([[0, -PITCH], [0, PITCH], [-PITCH, 0], [PITCH, 0]], np.int32)


def _tag_mask(data: GameData, tag: str) -> np.ndarray:
    return data.alive & data.tags[:, :, data.tag_index(tag)]


def _find_source() -> str:
    matches = sorted(reference_dir().glob("ka59-*.py"))
    if not matches:
        raise RuntimeError(f"ka59 reference source not found under {reference_dir()}")
    return str(matches[0])


EXTRACT_KWARGS = {"extra_slots": EXTRA_SLOTS}


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    L, N = data.num_levels, data.num_slots
    PH, PW = data.patch_shape
    original = N - EXTRA_SLOTS
    self._collider_slot = original + COLLIDER_SLOT_OFFSET
    self._scratch_slot = original + SCRATCH_SLOT_OFFSET
    explosion_base = original + EXPLOSION_BASE_OFFSET
    box_mask = _tag_mask(data, TAG_BOX)
    hole_mask = _tag_mask(data, TAG_HOLE)
    marker_mask = _tag_mask(data, TAG_MARKER)
    zone_mask = _tag_mask(data, TAG_ZONE)
    bomb_mask = _tag_mask(data, TAG_BOMB)
    wall_mask = _tag_mask(data, TAG_WALL)
    border_mask = _tag_mask(data, TAG_BORDER)
    occupant_mask = box_mask | marker_mask | bomb_mask
    self._box_mask = np.asarray(box_mask)
    self._hole_mask = np.asarray(hole_mask)
    self._marker_mask = np.asarray(marker_mask)
    self._zone_mask = np.asarray(zone_mask)
    self._bomb_mask = np.asarray(bomb_mask)
    self._wall_mask = np.asarray(wall_mask)
    self._border_mask = np.asarray(border_mask)
    self._occupant_mask = np.asarray(occupant_mask)
    is_explosion_slot = np.zeros(N, bool)
    is_explosion_slot[explosion_base : explosion_base + 3 * MAX_BOMBS] = True
    self._is_explosion_slot = np.asarray(is_explosion_slot)
    self._step_budget = np.asarray(
        [d["StepCounter"] for d in data.level_data], np.int32
    )
    first_box = np.full(L, -1, np.int32)
    for li in range(L):
        hits = np.flatnonzero(box_mask[li])
        if len(hits):
            first_box[li] = hits[0]
    self._first_box = np.asarray(first_box)
    center_dot = np.zeros((L, N, PH, PW), bool)
    for li in range(L):
        for si in range(N):
            if not box_mask[li, si]:
                continue
            h, w = int(data.h[li, si]), int(data.w[li, si])
            if h == 3 and w == 3:
                center_dot[li, si, 1, 1] = True
            elif h == 6 and w == 3:
                center_dot[li, si, 2:4, 1] = True
            elif h == 3 and w == 6:
                center_dot[li, si, 1, 2:4] = True
            else:
                center_dot[li, si, 2:4, 2:4] = True
    self._center_dot_mask = np.asarray(center_dot)
    outline = np.zeros((L, N, PH, PW), bool)
    edges = np.zeros((L, N, 4, PH, PW), bool)
    rows = np.arange(PH)[:, None]
    cols = np.arange(PW)[None, :]
    for li in range(L):
        for si in range(N):
            if not box_mask[li, si]:
                continue
            h, w = int(data.h[li, si]), int(data.w[li, si])
            top = (rows == 0) & (cols < w)
            bottom = (rows == h - 1) & (cols < w)
            left = (cols == 0) & (rows < h)
            right = (cols == w - 1) & (rows < h)
            edges[li, si, 0] = top
            edges[li, si, 1] = bottom
            edges[li, si, 2] = left
            edges[li, si, 3] = right
            outline[li, si] = top | bottom | left | right
    self._box_outline_mask = np.asarray(outline)
    self._box_edge_masks = np.asarray(edges)
    ref = load_reference_game(_find_source())
    module = sys.modules[type(ref).__module__]
    templates = module.sprites
    max_h = int(data.h[bomb_mask].max()) if bomb_mask.any() else 1
    self._max_bomb_h = max_h
    rotation = np.zeros((L, N), np.int32)
    explosion_pixels = np.full((L, N, 3, PH, PW), -1, np.int8)
    explosion_h = np.zeros((L, N, 3), np.int32)
    explosion_w = np.zeros((L, N, 3), np.int32)
    is_bad = np.zeros((L, N, max_h), bool)
    bad_rank = np.zeros((L, N, max_h), np.int32)
    total_bad = np.zeros((L, N), np.int32)
    last_bad = np.zeros((L, N), bool)
    fuse_frames = np.full((L, N, 2, max_h + 1, PH, PW), -1, np.int8)
    for li, lvl in enumerate(ref._clean_levels):
        for si, sp in enumerate(lvl.get_sprites()):
            rotation[li, si] = sp.rotation
            if not bomb_mask[li, si]:
                continue
            width_class = int(sp.width)
            for k in range(3):
                tmpl = templates[f"explode-{width_class}-{k + 1}"]
                rotated = tmpl.clone().set_rotation((int(sp.rotation) + 180) % 360)
                px = np.asarray(rotated.render())
                h, w = px.shape
                explosion_pixels[li, si, k, :h, :w] = px
                explosion_h[li, si, k] = h
                explosion_w[li, si, k] = w
    
            h = int(sp.height)
            raw = np.asarray(sp.pixels, dtype=np.int8).copy()
            bad = raw[:, 0] != FUSE_FILLED
            rank = np.cumsum(bad)
            is_bad[li, si, :h] = bad
            bad_rank[li, si, :h] = rank
            total_bad[li, si] = rank[-1]
            last_bad[li, si] = bad[-1]
            bases = [raw, np.full_like(raw, BOMB_SPENT)]
            for cycle, base in enumerate(bases):
                for p in range(h + 1):
                    if cycle == 0:
                        converted = bad & (rank <= p)
                    else:
                        converted = np.arange(h) < p
                    frame = base.copy()
                    frame[converted, :] = FUSE_FILLED
                    clone = sp.clone()
                    clone.pixels = frame
                    rendered = np.asarray(clone.render())
                    rh, rw = rendered.shape
                    fuse_frames[li, si, cycle, p, :rh, :rw] = rendered
    self._explosion_pixels = np.asarray(explosion_pixels)
    self._explosion_h = np.asarray(explosion_h)
    self._explosion_w = np.asarray(explosion_w)
    self._fuse_frames = np.asarray(fuse_frames)
    recoil_dx = np.zeros((L, N), np.int32)
    recoil_dy = np.zeros((L, N), np.int32)
    recoil_dy[rotation == 0] = PITCH
    recoil_dx[rotation == 90] = -PITCH
    recoil_dy[rotation == 180] = -PITCH
    recoil_dx[rotation == 270] = PITCH
    self._explosion_recoil_dx = np.asarray(recoil_dx)
    self._explosion_recoil_dy = np.asarray(recoil_dy)
    bomb_rank = np.full((L, N), -1, np.int32)
    for li in range(L):
        bombs = np.flatnonzero(bomb_mask[li])
        for rank, si in enumerate(bombs):
            bomb_rank[li, si] = rank
    self._explosion_base_slot = np.asarray(
        np.where(bomb_mask, explosion_base + 3 * np.maximum(bomb_rank, 0), 0)
    )
    self._fuse_total_bad = np.asarray(total_bad)
    self._fuse_last_row_bad = np.asarray(last_bad)
    return self


def make_args(source, seed=0):
    return (extract(source, extra_slots=EXTRA_SLOTS),), {}
