import types
import sys
import numpy as np
from pathlib import Path
from ._const import FRAME_SIZE
from ._base import ARCBaseGame, GameData, Setup, extract, load_reference_game, reference_dir


EXTRACT_KWARGS = {"extra_slots": 1}


TELEPORT, RESIZE, FIREBALL = 0, 1, 2


NUM_SPELLS = 3


SPELL_NAMES = ("tevyeq", "sieesc_chwjgc", "fibcey")


SPELL_COLOR = np.array([11, 15, 6], np.int8)


SPELL_PATTERNS = np.array(
    [
        [[True, True, False], [False, True, False], [False, False, False]],
        [[False, True, False], [True, False, True], [False, True, False]],
        [[False, True, False], [False, True, False], [False, True, False]],
    ],
    bool,
)


MISS_COLOR = 14


BASE_COLOR = 2


BAR_FILLED, BAR_EMPTY = 0, 14


SHAKE_COLOR = 8


REFUND_STEPS = 10


GRID_ORIGIN = (24, 49)


GRID_PITCH = 5


FACING_ROTATION = np.array([180, 0, 90, 270], np.int32)


FACING_DELTA = np.array([[0, -1], [0, 1], [-1, 0], [1, 0]], np.int32)


FIREBALL_ROTATION = np.array([90, 270, 0, 180], np.int32)


CAST_FRAMES = 8


TELEPORT_FRAMES = 4


MISS_FRAMES = 12


SLIDE_FRAMES = 2


RESIZE_FRAMES = 8


RESIZE_BLOCKED_FRAMES = 12


DEMO_FRAMES_PER_CELL = 5


GROWTH_BLOCK_NAMES = ("crzdcq", "dosorb", "seofsw-dosorb")


FIREBALL_BLOCK_NAMES = ("dosorb", "seofsw-dosorb", "crzdcq", "tagsmh", "seofsw-tagsmh")


def _load_module(source: str | Path):
    source = Path(source)
    module = types.ModuleType(f"arc_sc25_probe_{source.stem.replace('-', '_')}")
    module.__file__ = str(source)
    sys.modules[module.__name__] = module
    exec(
        compile(source.read_text(encoding="utf-8"), str(source), "exec"),
        module.__dict__,
    )
    return module


def _instantiate(game_class):
    import inspect

    if "seed" in inspect.signature(game_class.__init__).parameters:
        return game_class(seed=0)
    return game_class()


def _locate_source(game_id: str) -> Path:
    matches = sorted(reference_dir().glob(f"{game_id}-*.py"))
    if not matches:
        raise FileNotFoundError(f"no source for {game_id!r} in {reference_dir()}")
    return matches[0]


DATA_PARAM = 'data'


def derive(data, source=None):
    self = Setup(data)
    if source is None:
        source = _locate_source(data.game_id)
    L, N = data.num_levels, data.num_slots
    names = data.names
    def find(level, name):
        for i in range(N):
            if data.alive[level, i] and names[level][i] == name:
                return i
        return -1
    def find_all(level, name):
        return [
            i for i in range(N) if data.alive[level, i] and names[level][i] == name
        ]
    allowed = np.zeros((L, NUM_SPELLS), bool)
    budget = np.zeros(L, np.int32)
    auto_hint = np.full(L, -1, np.int32)
    for li, ld in enumerate(data.level_data):
        budget[li] = ld.get("slfh") or 0
        spells = ld.get("efvw") or []
        if isinstance(spells, str):
            spells = [spells]
        for s in spells:
            allowed[li, SPELL_NAMES.index(s)] = True
        if li < 3 and spells and spells[0] in SPELL_NAMES:
            auto_hint[li] = SPELL_NAMES.index(spells[0])
    self._allowed = np.asarray(allowed)
    self._budget = np.asarray(budget)
    self._auto_hint = np.asarray(auto_hint)
    demo_slot = np.full((NUM_SPELLS, 3, 3), -1, np.int32)
    demo_count = np.zeros(NUM_SPELLS, np.int32)
    for sp in range(NUM_SPELLS):
        k = 0
        for r in range(3):
            for c in range(3):
                if SPELL_PATTERNS[sp, r, c]:
                    demo_slot[sp, r, c] = k
                    k += 1
        demo_count[sp] = k
    self._demo_slot = np.asarray(demo_slot)
    self._demo_count = np.asarray(demo_count)
    self._pattern = np.asarray(SPELL_PATTERNS)
    self._spell_color = np.asarray(SPELL_COLOR)
    self._exydhv = np.asarray([find(li, "exydhv") for li in range(L)], np.int32)
    self._action_ui = np.asarray(
        [find(li, "action-ui") for li in range(L)], np.int32
    )
    self._pluyoo = np.asarray([find(li, "pluyoo") for li in range(L)], np.int32)
    self._tagsmh = np.asarray([find(li, "tagsmh") for li in range(L)], np.int32)
    self._seofsw_tagsmh = np.asarray(
        [find(li, "seofsw-tagsmh") for li in range(L)], np.int32
    )
    self._dosorb = np.asarray([find(li, "dosorb") for li in range(L)], np.int32)
    self._seofsw_dosorb = np.asarray(
        [find(li, "seofsw-dosorb") for li in range(L)], np.int32
    )
    self._acyylh_ovl = np.asarray(
        [find(li, "acyylh-tevyeq-inkpfx") for li in range(L)], np.int32
    )
    self._smzaik_ovl = np.asarray(
        [find(li, "smzaik-tevyeq-inkpfx") for li in range(L)], np.int32
    )
    self._fireball_slot = np.int32(N - 1)
    blockers = [find_all(li, "enjehv-pahtoz") for li in range(L)]
    max_blockers = max((len(b) for b in blockers), default=0)
    blocker_slots = np.full((L, max(max_blockers, 1)), -1, np.int32)
    for li, b in enumerate(blockers):
        blocker_slots[li, : len(b)] = b
    self._blockers = np.asarray(blocker_slots)
    tp_lists = [
        [find_all(li, "smzaik-tevyeq-tagsmh"), find_all(li, "tevyeq-tagsmh")]
        for li in range(L)
    ]
    max_tp = max((len(lst) for pair in tp_lists for lst in pair), default=0)
    max_tp = max(max_tp, 1)
    tp_slots = np.full((L, 2, max_tp), -1, np.int32)
    tp_counts = np.zeros((L, 2), np.int32)
    for li in range(L):
        for k in range(2):
            lst = tp_lists[li][k]
            tp_slots[li, k, : len(lst)] = lst
            tp_counts[li, k] = len(lst)
    self._tp_slots = np.asarray(tp_slots)
    self._tp_counts = np.asarray(tp_counts)
    grid_slot = np.full((L, 3, 3), -1, np.int32)
    for li in range(L):
        for i in range(N):
            if data.alive[li, i] and names[li][i] == "clzbxlm-sptivk-slsrhr":
                x, y = int(data.x[li, i]), int(data.y[li, i])
                r = (y - GRID_ORIGIN[1]) // GRID_PITCH
                c = (x - GRID_ORIGIN[0]) // GRID_PITCH
                grid_slot[li, r, c] = i
    self._grid_slot = np.asarray(grid_slot)
    kind = np.full((L, N), 3, np.int32)
    cell_r = np.full((L, N), -1, np.int32)
    cell_c = np.full((L, N), -1, np.int32)
    spell_idx = np.full((L, N), -1, np.int32)
    for li in range(L):
        glyph_pos = []
        for i in range(N):
            if not data.alive[li, i]:
                continue
            nm = names[li][i]
            if nm.startswith("sptivk-") and nm not in (
                "sptivk-ui",
                "sptivk-caxiiu",
            ):
                glyph_pos.append(
                    (
                        int(data.x[li, i]),
                        int(data.y[li, i]),
                        SPELL_NAMES.index(nm[len("sptivk-") :]),
                    )
                )
        for i in range(N):
            if not data.alive[li, i]:
                continue
            nm = names[li][i]
            if nm == "clzbxlm-sptivk-slsrhr":
                x, y = int(data.x[li, i]), int(data.y[li, i])
                kind[li, i] = 0
                cell_r[li, i] = (y - GRID_ORIGIN[1]) // GRID_PITCH
                cell_c[li, i] = (x - GRID_ORIGIN[0]) // GRID_PITCH
            elif "clcbko" in nm.lower():
                x, y, w, h = (
                    int(data.x[li, i]),
                    int(data.y[li, i]),
                    int(data.w[li, i]),
                    int(data.h[li, i]),
                )
                best = None
                for r in range(3):
                    for c in range(3):
                        gs = grid_slot[li, r, c]
                        if gs < 0:
                            continue
                        gx, gy = int(data.x[li, gs]), int(data.y[li, gs])
                        if x <= gx + 1 < x + w and y <= gy + 1 < y + h:
                            best = (r, c)
                            break
                kind[li, i] = 1
                if best:
                    cell_r[li, i], cell_c[li, i] = best
            elif nm == "sptivk-caxiiu":
                x, y = int(data.x[li, i]), int(data.y[li, i])
                match = None
                for gx, gy, sidx in glyph_pos:
                    if abs(gx - x) <= 5 and abs(gy - y) <= 5:
                        match = sidx
                        break
                kind[li, i] = 2
                spell_idx[li, i] = match if match is not None else -1
            elif nm.startswith("sptivk-") and nm != "sptivk-ui":
                kind[li, i] = 2
                spell_idx[li, i] = SPELL_NAMES.index(nm[len("sptivk-") :])
    self._click_kind = np.asarray(kind)
    self._click_r = np.asarray(cell_r)
    self._click_c = np.asarray(cell_c)
    self._click_spell = np.asarray(spell_idx)
    ph, pw = data.patch_shape
    bar_mask = np.zeros((L, ph), bool)
    for li in range(L):
        slot = int(self._action_ui[li])
        if slot >= 0:
            bar_mask[li] = data.pixels[li, slot, :, 0] >= 0
    self._bar_mask = np.asarray(bar_mask)
    self._bar_count = np.asarray(bar_mask.sum(axis=1).astype(np.int32))
    gsize = data.grid_size
    grow_block = np.zeros((L, FRAME_SIZE, FRAME_SIZE), bool)
    fb_hit_slot = np.full((L, FRAME_SIZE, FRAME_SIZE), -1, np.int32)
    for li in range(L):
        w, h = int(gsize[li, 0]), int(gsize[li, 1])
        for i in range(N):
            if not data.alive[li, i]:
                continue
            nm = names[li][i]
            is_duvwsv = nm.startswith("duvwsv-")
            in_grow = is_duvwsv or nm in GROWTH_BLOCK_NAMES
            in_fb = is_duvwsv or nm in FIREBALL_BLOCK_NAMES
            if not (in_grow or in_fb):
                continue
            x0, y0 = int(data.x[li, i]), int(data.y[li, i])
            ph_i, pw_i = int(data.h[li, i]), int(data.w[li, i])
            for dy in range(ph_i):
                yy = y0 + dy
                if yy < 0 or yy >= FRAME_SIZE:
                    continue
                for dx in range(pw_i):
                    xx = x0 + dx
                    if xx < 0 or xx >= FRAME_SIZE:
                        continue
                    opaque = data.pixels[li, i, dy, dx] != -1
                    if not opaque and is_duvwsv:
                        continue
                    if in_grow and (is_duvwsv and not opaque):
                        pass
                    if in_grow:
                        if (is_duvwsv and opaque) or (
                            not is_duvwsv and nm in GROWTH_BLOCK_NAMES
                        ):
                            grow_block[li, yy, xx] = True
                    if in_fb and fb_hit_slot[li, yy, xx] < 0:
                        if (is_duvwsv and opaque) or (
                            not is_duvwsv and nm in FIREBALL_BLOCK_NAMES
                        ):
                            fb_hit_slot[li, yy, xx] = i
    self._grow_block = np.asarray(grow_block)
    self._fb_hit_slot = np.asarray(fb_hit_slot)
    fb_kind = np.full((L, N), 3, np.int32)
    for li in range(L):
        t = int(self._tagsmh[li])
        st = int(self._seofsw_tagsmh[li])
        if t >= 0:
            fb_kind[li, t] = 1
        if st >= 0:
            fb_kind[li, st] = 2
    self._fb_slot_kind = np.asarray(fb_kind)
    module = _load_module(source)
    ref = None
    from arcengine import ARCBaseGame
    for value in vars(module).values():
        if (
            isinstance(value, type)
            and issubclass(value, ARCBaseGame)
            and value is not ARCBaseGame
        ):
            ref = _instantiate(value)
            break
    player_base = np.zeros((L, 2, 2), np.int8)
    player_rotation0 = np.zeros(L, np.int32)
    for li, lvl in enumerate(ref._clean_levels):
        p = lvl.get_sprites_by_name("pluyoo")[0]
        player_base[li] = p.pixels.astype(np.int8)
        player_rotation0[li] = p.rotation
    self._player_base0 = np.asarray(player_base)
    self._player_rotation0 = np.asarray(player_rotation0)
    bases = {
        name: module.sprites[name].pixels.astype(np.int8)
        for name in ("fibcey", "fibcey-2")
    }
    base_h = max(b.shape[0] for b in bases.values())
    base_w = max(b.shape[1] for b in bases.values())
    fb_base = np.full((2, base_h, base_w), -1, np.int8)
    fb_base_shape = np.zeros((2, 2), np.int32)
    for scale_idx, name in enumerate(("fibcey-2", "fibcey")):
        b = bases[name]
        fb_base[scale_idx, : b.shape[0], : b.shape[1]] = b
        fb_base_shape[scale_idx] = b.shape
    self._fb_base = np.asarray(fb_base)
    self._fb_base_shape = np.asarray(fb_base_shape)
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS), source,), {}
