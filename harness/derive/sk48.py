import sys
import numpy as np
from ._const import Action
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


TAG_HEAD = "epdquznwmq"


TAG_BLOCK = "elmjchdqcn"


TAG_RAIL = "irkeobngyh"


TAG_BOUNDARY = "jtteddgeyl"


TAG_WALL = "mkgqjopcjn"


TAG_SEGMENT = "qtjqovumxf"


TAG_CLICK = "sys_click"


PITCH = 6


HEAD_ACTIVE_Y_THRESHOLD = 53


STEP_BUDGET = 196


STACK_DEPTH = 197


MAX_HEADS = 5


MAX_SEGMENTS = 8


MAX_MARKERS = 6


MAX_SHADOWS = 12


MAX_PUSH_DEPTH = 7


PUSH_STACK_CAP = 16


PHASE_ENTER = 0


PHASE_SEG_PROBE = 1


PHASE_SEG_AWAIT = 2


PHASE_BLOCK_CROSS = 3


PHASE_BLOCK_FAR = 4


PHASE_BLOCK_AWAIT_FAR = 5


PHASE_BLOCK_SHADOW = 6


PHASE_SEG_DONE_TRUE = 7


WIN_BLINK_FRAMES = 35


BLINK_PERIOD = 5


BLINK_LOCK_FRAME = 25


BLINK_OFF_COLOR = 0


DESELECT_HEAD_FROM = 0


DESELECT_HEAD_TO = 4


DESELECT_SEG_STEP1_FROM = 2


DESELECT_SEG_STEP1_TO = 3


DESELECT_SEG_STEP2_FROM = 1


DESELECT_SEG_STEP2_TO = 2


SELECT_HEAD_FROM = 4


SELECT_HEAD_TO = 0


SELECT_SEG_STEP1_FROM = 2


SELECT_SEG_STEP1_TO = 1


SELECT_SEG_STEP2_FROM = 3


SELECT_SEG_STEP2_TO = 2


HUD_ROW = 53


HUD_BG = 3


HUD_FILL = 2


DIRECTION_BY_ROTATION = {0: (1, 0), 90: (0, 1), 180: (-1, 0), 270: (0, -1)}


MOVE_DELTA_BY_ACTION = {Action.ACTION1: (0, -1), Action.ACTION2: (0, 1), Action.ACTION3: (-1, 0), Action.ACTION4: (1, 0)}


EXTRA_SLOTS = MAX_HEADS * MAX_SEGMENTS + MAX_MARKERS + MAX_SHADOWS


EXTRACT_KWARGS = {"extra_slots": EXTRA_SLOTS}


def _find_source() -> str:
    matches = sorted(reference_dir().glob("sk48-*.py"))
    if not matches:
        raise RuntimeError(f"sk48 reference source not found under {reference_dir()}")
    return str(matches[0])


def _tag_mask(data: GameData, tag: str) -> np.ndarray:
    return data.alive & data.tags[:, :, data.tag_index(tag)]


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    L, N = data.num_levels, data.num_slots
    PH, PW = data.patch_shape
    original = N - EXTRA_SLOTS
    self._segment_base = original
    self._marker_base = original + MAX_HEADS * MAX_SEGMENTS
    self._shadow_base = self._marker_base + MAX_MARKERS
    block_mask = _tag_mask(data, TAG_BLOCK)
    boundary_mask = _tag_mask(data, TAG_BOUNDARY)
    clean_segment_mask = _tag_mask(data, TAG_SEGMENT)
    self._block_mask = np.asarray(block_mask)
    self._clean_removal_mask = np.asarray(clean_segment_mask)
    boundary_slot = np.full(L, -1, np.int32)
    for li in range(L):
        hits = np.flatnonzero(boundary_mask[li])
        boundary_slot[li] = hits[0]
    self._boundary_left = np.asarray(data.x[np.arange(L), boundary_slot])
    self._boundary_top = np.asarray(data.y[np.arange(L), boundary_slot])
    self._boundary_right = np.asarray(
        data.x[np.arange(L), boundary_slot] + data.w[np.arange(L), boundary_slot]
    )
    self._boundary_bottom = np.asarray(
        data.y[np.arange(L), boundary_slot] + data.h[np.arange(L), boundary_slot]
    )
    ref = load_reference_game(_find_source())
    module = sys.modules[type(ref).__module__]
    templates = module.sprites
    head_slot = np.full((L, MAX_HEADS), -1, np.int32)
    head_rotation = np.zeros((L, MAX_HEADS), np.int32)
    head_active = np.zeros((L, MAX_HEADS), bool)
    head_partner = np.full((L, MAX_HEADS), -1, np.int32)
    head_initial_segments = np.zeros((L, MAX_HEADS), np.int32)
    target_color = np.full((L, MAX_HEADS, MAX_SEGMENTS - 1), -1, np.int32)
    marker_count = np.zeros((L, MAX_HEADS), np.int32)
    marker_slot = np.full((L, MAX_HEADS, MAX_SEGMENTS - 1), -1, np.int32)
    marker_x = np.zeros((L, MAX_HEADS, MAX_SEGMENTS - 1), np.int32)
    marker_y = np.zeros((L, MAX_HEADS, MAX_SEGMENTS - 1), np.int32)
    dir_x_by_rotation = {r: v[0] for r, v in DIRECTION_BY_ROTATION.items()}
    dir_y_by_rotation = {r: v[1] for r, v in DIRECTION_BY_ROTATION.items()}
    def find_segment(x, y, horizontal, segments):
        for seg in segments:
            if seg.x == x and seg.y == y and horizontal == (seg.rotation in (0, 180)):
                return seg
        return None
    for li, lvl in enumerate(ref._clean_levels):
        sprites_in_order = lvl.get_sprites()
        heads = [s for s in sprites_in_order if TAG_HEAD in s.tags]
        blocks = [s for s in sprites_in_order if TAG_BLOCK in s.tags]
        all_segments = [s for s in sprites_in_order if TAG_SEGMENT in s.tags]
    
        for hi, head in enumerate(heads):
            head_slot[li, hi] = sprites_in_order.index(head)
            head_rotation[li, hi] = head.rotation
            head_active[li, hi] = head.y < HEAD_ACTIVE_Y_THRESHOLD
    
        for hi, head in enumerate(heads):
            if not head_active[li, hi]:
                continue
            partner = next(
                (b for b in heads if b is not head and b.pixels[2, 2] == head.pixels[2, 2]), None
            )
            if partner is not None:
                head_partner[li, hi] = heads.index(partner)
    
        head_segments = {}
        for hi, head in enumerate(heads):
            dx, dy = dir_x_by_rotation[head.rotation], dir_y_by_rotation[head.rotation]
            horizontal = head.rotation in (0, 180)
            segs = []
            cur = find_segment(head.x, head.y, horizontal, all_segments)
            while cur is not None:
                segs.append(cur)
                cur = find_segment(cur.x + dx * PITCH, cur.y + dy * PITCH, horizontal, all_segments)
            head_segments[hi] = segs
            head_initial_segments[li, hi] = len(segs)
    
        next_marker_slot = 0
        for hi, head in enumerate(heads):
            partner_idx = int(head_partner[li, hi])
            if partner_idx < 0:
                continue
            tray_segments = head_segments[partner_idx]
            count = max(0, len(tray_segments) - 1)
            marker_count[li, hi] = count
            compacted_colors = []
            for seg in tray_segments:
                match = next((b for b in blocks if b.x == seg.x and b.y == seg.y), None)
                if match is not None:
                    compacted_colors.append(int(match.pixels[1, 1]))
            for i in range(count):
                target_color[li, hi, i] = compacted_colors[i] if i < len(compacted_colors) else -1
                marked = tray_segments[i + 1]
                marker_slot[li, hi, i] = self._marker_base + next_marker_slot
                marker_x[li, hi, i] = marked.x + 1
                marker_y[li, hi, i] = marked.y + 1
                next_marker_slot += 1
        assert next_marker_slot <= MAX_MARKERS
    self._head_slot = np.asarray(head_slot)
    self._head_rotation = np.asarray(head_rotation)
    self._head_active = np.asarray(head_active)
    self._head_partner = np.asarray(head_partner)
    self._head_initial_segments = np.asarray(head_initial_segments)
    self._target_color = np.asarray(target_color)
    self._marker_count = np.asarray(marker_count)
    self._marker_slot = np.asarray(marker_slot)
    self._marker_x = np.asarray(marker_x)
    self._marker_y = np.asarray(marker_y)
    head_direction_x = np.zeros((L, MAX_HEADS), np.int32)
    head_direction_y = np.zeros((L, MAX_HEADS), np.int32)
    for r, (dx, dy) in DIRECTION_BY_ROTATION.items():
        sel = head_rotation == r
        head_direction_x[sel] = dx
        head_direction_y[sel] = dy
    self._head_direction_x = np.asarray(head_direction_x)
    self._head_direction_y = np.asarray(head_direction_y)
    is_segment = np.zeros(N, bool)
    segment_head = np.full(N, -1, np.int32)
    segment_local = np.full(N, -1, np.int32)
    for hi in range(MAX_HEADS):
        for k in range(MAX_SEGMENTS):
            slot = self._segment_base + hi * MAX_SEGMENTS + k
            is_segment[slot] = True
            segment_head[slot] = hi
            segment_local[slot] = k
    self._is_segment = np.asarray(is_segment)
    self._segment_head = np.asarray(segment_head)
    segment_rotation_by_slot = np.zeros((L, N), np.int32)
    for li in range(L):
        for hi in range(MAX_HEADS):
            if head_slot[li, hi] < 0:
                continue
            base = self._segment_base + hi * MAX_SEGMENTS
            segment_rotation_by_slot[li, base : base + MAX_SEGMENTS] = head_rotation[li, hi]
    self._segment_rotation = np.asarray(segment_rotation_by_slot)
    segment_dir_x = np.zeros((L, N), np.int32)
    segment_dir_y = np.zeros((L, N), np.int32)
    for r, (dx, dy) in DIRECTION_BY_ROTATION.items():
        sel = segment_rotation_by_slot == r
        segment_dir_x[sel] = dx
        segment_dir_y[sel] = dy
    self._segment_dir_x = np.asarray(segment_dir_x)
    self._segment_dir_y = np.asarray(segment_dir_y)
    self._segment_horizontal = np.asarray(np.isin(segment_rotation_by_slot, [0, 180]))
    template = templates[TAG_SEGMENT]
    segment_pixels_by_rotation = {}
    for rot in (0, 90, 180, 270):
        clone = template.clone()
        clone.set_rotation(rot)
        px = np.asarray(clone.render())
        buf = np.full((PH, PW), -1, np.int8)
        buf[: px.shape[0], : px.shape[1]] = px
        segment_pixels_by_rotation[rot] = (buf, px.shape[0], px.shape[1])
    self._segment_template_pixels = np.asarray(
        np.stack([segment_pixels_by_rotation[r][0] for r in (0, 90, 180, 270)])
    )
    self._segment_template_h = np.asarray([segment_pixels_by_rotation[r][1] for r in (0, 90, 180, 270)])
    self._segment_template_w = np.asarray([segment_pixels_by_rotation[r][2] for r in (0, 90, 180, 270)])
    marker_template = templates["kevthtkmzm"]
    marker_px = np.asarray(marker_template.render())
    marker_buf = np.full((PH, PW), -1, np.int8)
    marker_buf[: marker_px.shape[0], : marker_px.shape[1]] = marker_px
    self._marker_pixels = np.asarray(marker_buf)
    self._marker_h = int(marker_px.shape[0])
    self._marker_w = int(marker_px.shape[1])
    shadow_template = templates["pkzxknabii"]
    shadow_pixels_by_rotation = {}
    for rot in (0, 90):
        clone = shadow_template.clone()
        clone.set_rotation(rot)
        px = np.asarray(clone.render())
        buf = np.full((PH, PW), -1, np.int8)
        buf[: px.shape[0], : px.shape[1]] = px
        shadow_pixels_by_rotation[rot] = (buf, px.shape[0], px.shape[1])
    self._shadow_template_pixels = np.asarray(
        np.stack([shadow_pixels_by_rotation[0][0], shadow_pixels_by_rotation[90][0]])
    )
    self._shadow_template_h = np.asarray([shadow_pixels_by_rotation[0][1], shadow_pixels_by_rotation[90][1]])
    self._shadow_template_w = np.asarray([shadow_pixels_by_rotation[0][2], shadow_pixels_by_rotation[90][2]])
    return self


def make_args(source, seed=0):
    return (extract(source, extra_slots=EXTRA_SLOTS),), {}
