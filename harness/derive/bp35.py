import sys
import numpy as np
from pathlib import Path
from ._const import Action
from ._base import Setup, cache_dir, extract, load_reference_game, source_for
from ._scene import Atlas, capture_atlas, image_key


SOURCE = str(source_for("bp35"))


GAME_ID = "bp35"


CACHE_DIR = cache_dir()


NUM_LEVELS = 9


WMAX = 11


HMAX = 50


PITCH = 6


NONE, BREAKABLE, SWITCH, BOMB_ARMED, BOMB_SAFE, SPREAD, WIN = range(7)


MAX_FRAMES = 57


MAX_HIST = 136


NMAX = 21


BACKGROUND_COLOR = 10


HUD_LOW_BG, HUD_LOW_FILL = 0, 15


HUD_HIGH_BG, HUD_HIGH_A, HUD_HIGH_B = 0, 7, 15


LINEAR = 0


EASE_OUT = 1


TERRAIN_NAMES = ("xcjjwqfzjfe", "aknlbboysnc", "jcyhkseuorf", "ubhhgljbnpu", "hzusueifitk")


NUM_TERRAIN = len(TERRAIN_NAMES)


IMAGE_NAMES = (
    "qclfkhjnaac", "fijhgcrvsfx", "ucflxtuuxln", "xqapkpdjuet",
    "lrpkmzabbfa", "ebjoowkheai", "gnqvqkdqlpt", "ippnakjmssl",
    "yuuqpmlxorv", "oonshderxef", "txjcfisalqu", "cvkgqlojfnh", "ltorejwifje",
    "etlsaqqtjvn", "wpulgmixnbz", "hihodtibubm", "yxaxjsryovv",
    "fjlzdjxhant",
    "player_right", "player_left",
    "player_right_0", "player_right_1", "player_right_2",
    "player_left_0", "player_left_1",
)


IMG = {name: i for i, name in enumerate(IMAGE_NAMES)}


def _build_easing(module) -> tuple[np.ndarray, np.ndarray]:
    ease_out = module.duykuovraf["skmykirpclw"]
    linear = module.duykuovraf["brsdwwrugbi"]
    out = np.zeros((NMAX + 1, NMAX), np.float32)
    lin = np.zeros((NMAX + 1, NMAX), np.float32)
    for n in range(1, NMAX + 1):
        for k in range(n):
            t0 = (k + 1) / n
            out[n, k] = ease_out.ahrizsjlwz(t0)
            lin[n, k] = linear.ahrizsjlwz(t0)
    return out, lin


def _load_atlas(source: str, cache_dir: Path) -> Atlas:
    path = Path(cache_dir) / f"{GAME_ID}_atlas"
    if path.with_suffix(".npz").exists():
        return Atlas.load(path)
    atlas = capture_atlas(GAME_ID, source)
    Path(cache_dir).mkdir(parents=True, exist_ok=True)
    atlas.save(path)
    return atlas


def _build_static(source: str, atlas: Atlas) -> dict[str, np.ndarray]:
    import sys

    from ..extract import load_reference_game
    from ..scene import image_key

    game = load_reference_game(source)
    module = next(m for n, m in sys.modules.items() if n.startswith(f"arcadax_ref_{GAME_ID}"))
    ids = {k: i for i, k in enumerate(atlas.keys)}

    tjdtolkmxo = module.tjdtolkmxo
    ymmwcccrhb = module.ymmwcccrhb
    board_cls = module.klmsuijofik

    kind0 = np.zeros((NUM_LEVELS, HMAX, WMAX), np.int8)
    wall = np.zeros((NUM_LEVELS, HMAX, WMAX), bool)
    spike = np.zeros((NUM_LEVELS, HMAX, WMAX), bool)
    ceil_m0 = np.zeros((NUM_LEVELS, HMAX, WMAX), bool)
    ceil_w0 = np.zeros((NUM_LEVELS, HMAX, WMAX), bool)
    level_w = np.zeros(NUM_LEVELS, np.int32)
    level_h = np.zeros(NUM_LEVELS, np.int32)
    player_start = np.zeros((NUM_LEVELS, 2), np.int32)
    terrain_present = np.zeros((NUM_LEVELS, NUM_TERRAIN), bool)
    terrain_anchor = np.zeros((NUM_LEVELS, NUM_TERRAIN, 2), np.int32)
    terrain_atlas = np.full((NUM_LEVELS, NUM_TERRAIN), -1, np.int32)
    terrain_layer = np.zeros((NUM_LEVELS, NUM_TERRAIN), np.int32)

    name_to_kind = {
        "qclfkhjnaac": BREAKABLE,
        "lrpkmzabbfa": SWITCH,
        "yuuqpmlxorv": BOMB_ARMED,
        "oonshderxef": BOMB_SAFE,
        "etlsaqqtjvn": SPREAD,
        "fjlzdjxhant": WIN,
    }
    spike_names = {"ubhhgljbnpu", "hzusueifitk"}

    for li in range(NUM_LEVELS):
        g = tjdtolkmxo[f"grid{li + 1}"]
        board = board_cls(g, ymmwcccrhb, name=f"level{li + 1}")
        w, h = board.grid_size
        if w > WMAX or h > HMAX:
            raise ValueError(f"level {li} is {w}x{h}, exceeds bound ({WMAX},{HMAX})")
        level_w[li], level_h[li] = w, h
        for y in range(h):
            for x in range(w):
                for p in board.jhzcxkveiw(x, y):
                    if p.name == "player_right":
                        player_start[li] = (x, y)
                    elif p.name in name_to_kind:
                        kind0[li, y, x] = name_to_kind[p.name]
                    elif p.name == "xcjjwqfzjfe":
                        wall[li, y, x] = True
                    elif p.name in spike_names:
                        spike[li, y, x] = True
                    elif p.name == "aknlbboysnc":
                        ceil_m0[li, y, x] = True
                    elif p.name == "jcyhkseuorf":
                        ceil_w0[li, y, x] = True
                    else:
                        raise ValueError(f"unexpected tile {p.name!r} in level {li}")
        for ti, name in enumerate(TERRAIN_NAMES):
            pieces = board.wwkbcxznzg(name)
            if not pieces:
                continue
            piece = pieces[0]
            img = piece.gimrsagplbc.ieikpxxuml()
            terrain_present[li, ti] = True
            terrain_anchor[li, ti] = piece.qumspquyus
            terrain_atlas[li, ti] = ids[image_key(img)]
            terrain_layer[li, ti] = piece.gimrsagplbc.layer

    img_atlas = np.full(len(IMAGE_NAMES), -1, np.int32)
    img_layer = np.zeros(len(IMAGE_NAMES), np.int32)
    for name, i in IMG.items():
        arr = ymmwcccrhb[name].ieikpxxuml()
        img_atlas[i] = ids[image_key(arr)]
        img_layer[i] = ymmwcccrhb[name].layer

    ease_out, ease_lin = _build_easing(module)

    return dict(
        kind0=kind0, wall=wall, spike=spike, ceil_m0=ceil_m0, ceil_w0=ceil_w0,
        level_w=level_w, level_h=level_h, player_start=player_start,
        terrain_present=terrain_present, terrain_anchor=terrain_anchor,
        terrain_atlas=terrain_atlas, terrain_layer=terrain_layer,
        img_atlas=img_atlas, img_layer=img_layer,
        ease_out=ease_out, ease_lin=ease_lin,
    )


def _load_static(source: str, cache_dir: Path = CACHE_DIR) -> dict[str, np.ndarray]:
    path = Path(cache_dir) / f"{GAME_ID}_levels.npz"
    if path.exists():
        with np.load(path) as data:
            return {k: data[k] for k in data.files}
    atlas = _load_atlas(source, cache_dir)
    data = _build_static(source, atlas)
    Path(cache_dir).mkdir(parents=True, exist_ok=True)
    np.savez_compressed(path, **data)
    return data


CELL_SLOTS = HMAX * WMAX


PLAYER_SLOT = CELL_SLOTS


TERRAIN_SLOT0 = PLAYER_SLOT + 1


ANIM_SLOT0 = TERRAIN_SLOT0 + NUM_TERRAIN


NUM_SLOTS = ANIM_SLOT0 + 5


DATA_PARAM = 'source'


def derive(source=SOURCE, cache_dir=CACHE_DIR):
    self = Setup(source)
    atlas = _load_atlas(source, cache_dir)
    data = _load_static(source, cache_dir)
    self._kind0 = np.asarray(data["kind0"])
    self._wall = np.asarray(data["wall"])
    self._spike = np.asarray(data["spike"])
    self._ceil_w0 = np.asarray(data["ceil_w0"])
    self._level_w = np.asarray(data["level_w"])
    self._level_h = np.asarray(data["level_h"])
    self._player_start = np.asarray(data["player_start"])
    self._terrain_present = np.asarray(data["terrain_present"])
    self._terrain_anchor = np.asarray(data["terrain_anchor"])
    self._terrain_atlas = np.asarray(data["terrain_atlas"])
    self._terrain_layer = np.asarray(data["terrain_layer"])
    self._img_atlas = np.asarray(data["img_atlas"])
    self._img_layer = np.asarray(data["img_layer"])
    self._ease_out = np.asarray(data["ease_out"])
    self._ease_lin = np.asarray(data["ease_lin"])
    self._frame_axis = np.arange(MAX_FRAMES, dtype=np.int32)
    idle = [-1, IMG["qclfkhjnaac"], IMG["lrpkmzabbfa"], IMG["yuuqpmlxorv"],
            IMG["oonshderxef"], IMG["etlsaqqtjvn"], IMG["fjlzdjxhant"]]
    self._kind_idle_imgidx = np.asarray(idle, np.int32)
    self._is_ceiling_slot = np.asarray([False, True, True, False, False])
    return self


def make_args(source, seed=0):
    return (source,), {}
