import math
import types
import sys
import numpy as np
from pathlib import Path
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


EXTRACT_KWARGS = {"extra_slots": 1}


MAX_BLOBS = 9


MAX_FRUITS = 4


MAX_ZONES = 4


MAX_WIN_TERMS = 3


MAX_UNDO = 64


NUM_TIERS = 9


NUM_FRUIT_KINDS = 3


GLOW_MARGIN = 12


GLOW_SIZE = 2 * GLOW_MARGIN + 1


PULL_SPEED = 4 * 0.85


SWALLOW_SPEED = 10.0 / 4.0


PULL_FRAMES = 4


WOBBLE_FRAMES = 4


SWALLOW_TOTAL_FRAMES = WOBBLE_FRAMES + 4


SWALLOW_DYING_FRAMES = WOBBLE_FRAMES


WOBBLE_OFFSET = (0, -1, 0, 1)


EAT_LOCK_FRAMES = WOBBLE_FRAMES + 4 + 1


CLICK_RADIUS = 8


PULL_STEP = 4


CLICK_Y_MIN = 10


CLICK_Y_MAX = 63


BOARD_SIZE = 64


COLOR_EMPTY = 0


COLOR_NOT_YET = 2


PHASE_IDLE = 0


PHASE_PULLING = 1


PHASE_MISMATCH_FLASH = 2


PHASE_WIN_FLASH = 3


FRUIT_TAGS = ("ybnveypak", "ybnveypak2", "ybnveypak3")


BLOB_TAG = "zmlxwcvwb"


ZONE_A_TAG = "rgjznrcin"


ZONE_B_TAG = "xkstxyqbs"


TUTORIAL_TAG = "ooutlqdaq"


GLOW_TEMPLATE = "0029obpugmgzgr"


def load_sprite_templates(source: Path) -> dict:
    module = types.ModuleType(f"su15_templates_{source.stem.replace('-', '_')}")
    module.__file__ = str(source)
    sys.modules[module.__name__] = module
    exec(compile(source.read_text(encoding="utf-8"), str(source), "exec"), module.__dict__)
    return module.sprites


def bake_axis_delta(unit: float, speed: float) -> tuple[int, int, bool]:
    import math

    c = unit * speed
    floor_c = math.floor(c)
    frac = c - floor_c
    is_tie = frac == 0.5
    return floor_c, round(c), is_tie


def build_axis_table(bound: int, speed: float) -> tuple[np.ndarray, ...]:
    size = 2 * bound + 1
    floor_x = np.zeros((size, size), np.int32)
    round_x = np.zeros((size, size), np.int32)
    tie_x = np.zeros((size, size), bool)
    floor_y = np.zeros((size, size), np.int32)
    round_y = np.zeros((size, size), np.int32)
    tie_y = np.zeros((size, size), bool)
    for dx in range(-bound, bound + 1):
        for dy in range(-bound, bound + 1):
            if dx == 0 and dy == 0:
                continue
            r = (dx * dx + dy * dy) ** 0.5
            ux, uy = dx / r, dy / r
            fx, rx, tx = bake_axis_delta(ux, speed)
            fy, ry, ty = bake_axis_delta(uy, speed)
            floor_x[dx + bound, dy + bound] = fx
            round_x[dx + bound, dy + bound] = rx
            tie_x[dx + bound, dy + bound] = tx
            floor_y[dx + bound, dy + bound] = fy
            round_y[dx + bound, dy + bound] = ry
            tie_y[dx + bound, dy + bound] = ty
    return floor_x, round_x, tie_x, floor_y, round_y, tie_y


def build_delta_tables(bound: int) -> dict:
    tables = {"pull": build_axis_table(bound, PULL_SPEED)}
    for mult in range(1, 5):
        tables[f"swallow{mult}"] = build_axis_table(bound, SWALLOW_SPEED * mult)
    return tables


def build_glow_states() -> np.ndarray:
    kacsjmxae = 8
    gdamdvokm = 4
    step = float(kacsjmxae) / float(gdamdvokm - 1)
    radii = [float(kacsjmxae)]
    r = float(kacsjmxae)
    for _ in range(gdamdvokm):
        r = max(0.0, r - step)
        radii.append(r)
    gy, gx = np.meshgrid(
        np.arange(GLOW_SIZE, dtype=np.float32), np.arange(GLOW_SIZE, dtype=np.float32), indexing="ij"
    )
    dxg = gx - float(GLOW_MARGIN)
    dyg = gy - float(GLOW_MARGIN)
    dist2 = dxg * dxg + dyg * dyg
    states = np.full((len(radii), GLOW_SIZE, GLOW_SIZE), -1, np.int8)
    for k, radius in enumerate(radii):
        if radius <= 0.0:
            continue
        upper = radius + 0.5
        lower = max(0.0, radius - 0.5)
        mask = (dist2 <= upper * upper) & (dist2 >= lower * lower)
        states[k][mask] = COLOR_EMPTY
    return states


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    L = data.num_levels
    N = data.num_slots
    tag = lambda name: data.tags[:, :, data.tag_index(name)] & data.alive
    blob_mask = tag(BLOB_TAG)
    fruit_mask = np.zeros((L, N), bool)
    for name in FRUIT_TAGS:
        if name in data.tag_names:
            fruit_mask |= tag(name)
    zone_a_mask = tag(ZONE_A_TAG)
    zone_b_mask = tag(ZONE_B_TAG)
    tutorial_mask = tag(TUTORIAL_TAG) if TUTORIAL_TAG in data.tag_names else np.zeros((L, N), bool)
    tier_of_slot = np.full((L, N), -1, np.int32)
    for tier in range(NUM_TIERS):
        name = str(tier)
        if name in data.tag_names:
            col = data.tag_index(name)
            hit = data.tags[:, :, col] & blob_mask
            tier_of_slot[hit] = tier
    fruit_kind_of_slot = np.full((L, N), -1, np.int32)
    for kind, name in enumerate(FRUIT_TAGS):
        if name in data.tag_names:
            col = data.tag_index(name)
            hit = data.tags[:, :, col] & fruit_mask
            fruit_kind_of_slot[hit] = kind
    blob_slot = np.full((L, MAX_BLOBS), -1, np.int32)
    fruit_slot = np.full((L, MAX_FRUITS), -1, np.int32)
    zone_a_slot = np.full((L, MAX_ZONES), -1, np.int32)
    zone_b_slot = np.full((L, MAX_ZONES), -1, np.int32)
    tutorial_slot = np.full((L,), -1, np.int32)
    for li in range(L):
        bs = [si for si in range(N) if blob_mask[li, si]]
        fs = [si for si in range(N) if fruit_mask[li, si]]
        zas = [si for si in range(N) if zone_a_mask[li, si]]
        zbs = [si for si in range(N) if zone_b_mask[li, si]]
        tus = [si for si in range(N) if tutorial_mask[li, si]]
        blob_slot[li, : len(bs)] = bs
        fruit_slot[li, : len(fs)] = fs
        zone_a_slot[li, : len(zas)] = zas
        zone_b_slot[li, : len(zbs)] = zbs
        if tus:
            tutorial_slot[li] = tus[0]
    steps_budget = np.zeros((L,), np.int32)
    win_type = np.zeros((L, MAX_WIN_TERMS), np.int32)
    win_value = np.zeros((L, MAX_WIN_TERMS), np.int32)
    win_count = np.full((L, MAX_WIN_TERMS), -1, np.int32)
    template_name_to_kind = {"0030xjmmfvfpqm": 0, "0031xcwudgivus": 1, "0032qekmtelwqi": 2}
    for li in range(L):
        steps_budget[li] = data.level_data[li].get("steps", 0)
        raw = data.level_data[li].get("xkstxyqbs")
        terms = []
        if raw is not None:
            if isinstance(raw[0], (list, tuple)):
                terms = list(raw)
            else:
                terms = [raw]
        for k, (key, count) in enumerate(terms[:MAX_WIN_TERMS]):
            if isinstance(key, str) and key in template_name_to_kind:
                win_type[li, k] = 1
                win_value[li, k] = template_name_to_kind[key]
            else:
                win_type[li, k] = 0
                win_value[li, k] = int(key)
            win_count[li, k] = int(count)
    source = sorted(reference_dir().glob(f"{data.game_id}-*.py"))[0]
    templates = load_sprite_templates(source)
    ph, pw = data.patch_shape
    tier_pixels = np.full((NUM_TIERS, ph, pw), -1, np.int8)
    tier_h = np.zeros(NUM_TIERS, np.int32)
    tier_w = np.zeros(NUM_TIERS, np.int32)
    tier_layer = np.zeros(NUM_TIERS, np.int32)
    for tier in range(NUM_TIERS):
        s = templates[str(tier)]
        h, w = s.pixels.shape
        tier_pixels[tier, :h, :w] = s.pixels
        tier_h[tier], tier_w[tier], tier_layer[tier] = h, w, s.layer
    fruit_pixels = np.full((NUM_FRUIT_KINDS, ph, pw), -1, np.int8)
    fruit_h = np.zeros(NUM_FRUIT_KINDS, np.int32)
    fruit_w = np.zeros(NUM_FRUIT_KINDS, np.int32)
    fruit_layer = np.zeros(NUM_FRUIT_KINDS, np.int32)
    for kind, name in enumerate(FRUIT_TAGS):
        s = templates[name]
        h, w = s.pixels.shape
        fruit_pixels[kind, :h, :w] = s.pixels
        fruit_h[kind], fruit_w[kind], fruit_layer[kind] = h, w, s.layer
    delta_bound = 90
    delta_tables = build_delta_tables(delta_bound)
    glow_states = build_glow_states()
    j = np.asarray
    self._blob_slot = j(blob_slot)
    self._fruit_slot = j(fruit_slot)
    self._zone_a_slot = j(zone_a_slot)
    self._zone_b_slot = j(zone_b_slot)
    self._tutorial_slot = j(tutorial_slot)
    self._tier_of_slot = j(tier_of_slot)
    self._fruit_kind_of_slot = j(fruit_kind_of_slot)
    self._steps_budget = j(steps_budget)
    self._win_type = j(win_type)
    self._win_value = j(win_value)
    self._win_count = j(win_count)
    self._tier_pixels = j(tier_pixels)
    self._tier_h = j(tier_h)
    self._tier_w = j(tier_w)
    self._tier_layer = j(tier_layer)
    self._fruit_pixels = j(fruit_pixels)
    self._fruit_h = j(fruit_h)
    self._fruit_w = j(fruit_w)
    self._fruit_layer = j(fruit_layer)
    self._delta_bound = delta_bound
    self._pull_floor_x = j(delta_tables["pull"][0])
    self._pull_round_x = j(delta_tables["pull"][1])
    self._pull_tie_x = j(delta_tables["pull"][2])
    self._pull_floor_y = j(delta_tables["pull"][3])
    self._pull_round_y = j(delta_tables["pull"][4])
    self._pull_tie_y = j(delta_tables["pull"][5])
    self._swallow_floor_x = j(np.stack([delta_tables[f"swallow{m}"][0] for m in range(1, 5)]))
    self._swallow_round_x = j(np.stack([delta_tables[f"swallow{m}"][1] for m in range(1, 5)]))
    self._swallow_tie_x = j(np.stack([delta_tables[f"swallow{m}"][2] for m in range(1, 5)]))
    self._swallow_floor_y = j(np.stack([delta_tables[f"swallow{m}"][3] for m in range(1, 5)]))
    self._swallow_round_y = j(np.stack([delta_tables[f"swallow{m}"][4] for m in range(1, 5)]))
    self._swallow_tie_y = j(np.stack([delta_tables[f"swallow{m}"][5] for m in range(1, 5)]))
    self._glow_states = j(glow_states)
    self._glow_slot = self.num_slots - 1
    self._patch_shape = data.patch_shape
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
