import sys
import dataclasses
import numpy as np
from ._base import Setup, cache_dir, extract, load_reference_game, source_for
from ._scene import Atlas, capture_atlas, image_key


SOURCE = str(source_for("lf52"))


CACHE = cache_dir() / "lf52_full_atlas"


PITCH = 6


GRID_DIRECTIONS = ((0, -1), (1, 0), (0, 1), (-1, 0))


PEG_SPRITE_SIZE = 6


LEVEL_COUNT = 10


STATIC_SLOT_COUNT = 110


PEG_SLOT_COUNT = 16


HEART_SLOT_COUNT = 4


RING_SLOT = STATIC_SLOT_COUNT + PEG_SLOT_COUNT + HEART_SLOT_COUNT


DUST_SLOT = RING_SLOT + 1


REVEAL_BUTTON_RISING_SLOT = DUST_SLOT + 1


REVEAL_BUTTON_SETTLED_SLOT = REVEAL_BUTTON_RISING_SLOT + 1


JUMP_TRAIL_GHOST_SLOT = REVEAL_BUTTON_SETTLED_SLOT + 1


SLOT_COUNT = JUMP_TRAIL_GHOST_SLOT + 1


JUMP_TRAIL_GHOST_LAYER = 2


JUMP_TRAIL_GHOST_TICK_COUNT = 9


GRID_MAX_WIDTH = 28


GRID_MAX_HEIGHT = 14


WALL_TILE_SLOT_COUNT = 10


PEG_COLOR_NAMES = ("fozwvlovdui", "fozwvlovdui_red", "fozwvlovdui_blue")


RED_PEG_COLOR = 1


BLUE_PEG_COLOR = 2


MISS_KIND = 0


HEART_MENU_KIND = 1


CANT_MOVE_WIGGLE_KIND = 2


JUMP_KIND = 3


WALL_BUMP_KIND = 4


LEVEL_ONE_DUST_KIND = 5


PEG_REVEAL_KIND = 6


UNREPRODUCIBLE_SHUFFLE_KIND = 7


JUMP_AND_REVEAL_KIND = 8


JUMP_TICK_COUNT = 10


JUMP_MOVE_OFFSET = np.asarray([0, 0, 2, 3, 6, 8, 9, 11, 11, 12], np.int32)


JUMP_HOP_OFFSET = np.asarray([0, 0, -2, -2, -3, -2, -2, -1, -1, 0], np.int32)


CAPTURE_FADE_TICK_START = 2


CAPTURE_FADE_TICK_COUNT = 3


CAPTURE_INVISIBLE_FROM_TICK = 5


PULSE_BAKED_TICK_COUNT = 7


CANT_MOVE_WIGGLE_TICK_COUNT = 3


CANT_MOVE_WIGGLE_OFFSET = np.asarray([1, -1, 0], np.int32)


WALL_BUMP_TICK_COUNT = 3


WALL_BUMP_EASE_FRACTION = np.asarray([1 - (1 - (k + 1) / 3) ** 2 for k in range(3)], np.float32)


WALL_BUMP_PAN_TICK_COUNT = 7


WALL_BUMP_PAN_WAIT = 2


LEVEL_ONE_DUST_TICK_COUNT = 16


PEG_REVEAL_TICK_COUNT = 27


PEG_REVEAL_WIGGLE_WAIT = 16


WIN_WIGGLE_TICK_COUNT = 26


WIN_WIGGLE_OFFSET = np.asarray(
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, -3, -5, -5, -6, -6, -5, -2, -1, 0, -2, -3, -4, -4, -1, 0], np.int32
)


PEG_REVEAL_IMAGE_SWAP_TICK = 15


PEG_REVEAL_WIGGLE_OFFSET = np.asarray([2, 1, -1, 0], np.int32)


PEG_REVEAL_BUTTON_WAIT = 20


PEG_REVEAL_BUTTON_RISE_OFFSET = np.asarray([-2, -8, -11, -10, -13, -13, -14], np.int32)


UNREPRODUCIBLE_SHUFFLE_TICK_COUNT = 21


UNDO_HISTORY_DEPTH = 1000


JUMP_LANDING_REVEAL_TRIGGER_SLOT_COUNT = 7


JUMP_LANDING_REVEAL_TRIGGERS: dict[int, list[tuple[int, int, int]]] = {
    0: [(0, 2, -1), (2, 2, -1), (5, 1, -1)],
    1: [(0, 1, -1), (2, 1, -1), (4, 1, -1)],
    2: [(1, 0, -1), (0, 3, -1), (10, 0, -1), (13, 2, -1), (13, 4, -1), (10, 2, 5), (10, 4, 4)],
}


JUMP_LANDING_REVEAL_LEVEL5_DEST = (16, 2)


JUMP_LANDING_REVEAL_LEVEL5_RED_PEG_CELL = (6, 6)


def pulse_scale_values() -> list[float]:
    def ease(t: float) -> float:
        return 2.0 * t * t if t < 0.5 else 1.0 - ((-2 * t + 2) ** 2) / 2

    return [1.0 + 0.4 * ease((k + 1) / PULSE_BAKED_TICK_COUNT) for k in range(PULSE_BAKED_TICK_COUNT)]


def capture_scale_values() -> list[float]:
    return [1.0 - (k + 1) / (CAPTURE_FADE_TICK_COUNT + 1) for k in range(CAPTURE_FADE_TICK_COUNT)]


def load_reference_module():
    load_reference_game(SOURCE)
    return next(m for n, m in sys.modules.items() if n.startswith("arc_reference_lf52"))


@dataclasses.dataclass
class RawLevel:
    offset_x: int
    offset_y: int
    grid_width: int
    grid_height: int
    static_keys: list[str]
    static_x: list[int]
    static_y: list[int]
    static_layer: list[int]
    static_wall_tile_index: list[int]
    static_wall_tile_local_x: list[int]
    static_wall_tile_local_y: list[int]
    peg_grid_x: list[int]
    peg_grid_y: list[int]
    peg_color: list[int]
    landable_single: np.ndarray
    landable_double: np.ndarray
    jumpable_pin: np.ndarray
    wall_present: np.ndarray
    wall_tile_grid_x: list[int]
    wall_tile_grid_y: list[int]
    wall_tile_is_landable_double: list[bool]
    wall_tile_has_jumpable_pin: list[bool]


def renderer_visit_order(root) -> list:
    stack = list(root.rxmjztculbk)
    ordered = []
    while stack:
        node = stack.pop()
        ordered.append(node)
        stack.extend(node.rxmjztculbk)
    return ordered


def extract_levels(module) -> list[RawLevel]:
    engine_cls = module.equnaohchtj
    color_of = {name: i for i, name in enumerate(PEG_COLOR_NAMES)}
    levels = []
    for level in range(LEVEL_COUNT):
        engine = engine_cls()
        engine.whtqurkphir = level + 1
        engine.qjwmwkhrml()
        grid = engine.hncnfaqaddg
        offset_x, offset_y = grid.mepgityjcj()
        grid_width, grid_height = grid.grid_size

        wall_tiles = grid.whdmasyorl("hupkpseyuim2")
        wall_tile_index_by_id = {id(cell): index for index, cell in enumerate(wall_tiles)}
        wall_tile_grid_x = [int(cell.chahdtpdoz[0]) for cell in wall_tiles]
        wall_tile_grid_y = [int(cell.chahdtpdoz[1]) for cell in wall_tiles]
        wall_tile_index_by_cell = {
            (wall_tile_grid_x[index], wall_tile_grid_y[index]): index for index in range(len(wall_tiles))
        }

        static_keys, static_x, static_y, static_layer = [], [], [], []
        static_wall_tile_index = []
        static_wall_tile_local_x, static_wall_tile_local_y = [], []
        peg_grid_x, peg_grid_y, peg_color = [], [], []
        for node in renderer_visit_order(grid):
            name = getattr(node, "yrxvacxlgrf", "")
            if name.startswith("fozwvlovdui"):
                gx, gy = node.chahdtpdoz
                peg_grid_x.append(int(gx))
                peg_grid_y.append(int(gy))
                peg_color.append(color_of[name])
                continue
            shape = node.ultqqtpbdxi or node.fjhfwhfazwo
            image = shape.sxwqiwdisg() if shape is not None else None
            if image is None or image.size == 0:
                continue
            world_x, world_y = node.mepgityjcj()
            static_keys.append(image_key(image))
            static_x.append(int(world_x) - int(offset_x))
            static_y.append(int(world_y) - int(offset_y))
            static_layer.append(int(node.bifnvdxmkdu))
            parent = node.qoifrofmiu
            attached_index = wall_tile_index_by_id.get(id(node))
            if attached_index is None and parent is not None:
                attached_index = wall_tile_index_by_id.get(id(parent))
            if attached_index is None and "kraubslpehi" not in name:
                cell = getattr(node, "chahdtpdoz", None)
                if cell is not None:
                    attached_index = wall_tile_index_by_cell.get((int(cell[0]), int(cell[1])))
            static_wall_tile_index.append(attached_index if attached_index is not None else -1)
            if attached_index is not None:
                static_wall_tile_local_x.append(static_x[-1] - wall_tile_grid_x[attached_index] * PITCH)
                static_wall_tile_local_y.append(static_y[-1] - wall_tile_grid_y[attached_index] * PITCH)
            else:
                static_wall_tile_local_x.append(0)
                static_wall_tile_local_y.append(0)

        landable_single = np.zeros((grid_height, grid_width), bool)
        landable_double = np.zeros((grid_height, grid_width), bool)
        jumpable_pin = np.zeros((grid_height, grid_width), bool)
        wall_present = np.zeros((grid_height, grid_width), bool)
        for y in range(grid_height):
            for x in range(grid_width):
                names = [n.yrxvacxlgrf for n in grid.ijpoqzvnjt(x, y) if not n.yrxvacxlgrf.startswith("fozwvlovdui")]
                if any("dgxfozncuiz" in n for n in names):
                    jumpable_pin[y, x] = True
                if len(names) == 1 and "hupkpseyuim" in names[0]:
                    landable_single[y, x] = True
                if len(names) == 2 and "hupkpseyuim2" in names:
                    landable_double[y, x] = True
                if any("kraubslpehi" in n for n in names):
                    wall_present[y, x] = True

        wall_tile_is_landable_double = [
            bool(landable_double[wall_tile_grid_y[index], wall_tile_grid_x[index]])
            for index in range(len(wall_tiles))
        ]
        wall_tile_has_jumpable_pin = [
            bool(jumpable_pin[wall_tile_grid_y[index], wall_tile_grid_x[index]])
            for index in range(len(wall_tiles))
        ]
        for index, carries_pin in enumerate(wall_tile_has_jumpable_pin):
            if carries_pin:
                jumpable_pin[wall_tile_grid_y[index], wall_tile_grid_x[index]] = False

        levels.append(
            RawLevel(
                offset_x=int(offset_x), offset_y=int(offset_y),
                grid_width=int(grid_width), grid_height=int(grid_height),
                static_keys=static_keys, static_x=static_x, static_y=static_y, static_layer=static_layer,
                static_wall_tile_index=static_wall_tile_index,
                static_wall_tile_local_x=static_wall_tile_local_x, static_wall_tile_local_y=static_wall_tile_local_y,
                peg_grid_x=peg_grid_x, peg_grid_y=peg_grid_y, peg_color=peg_color,
                landable_single=landable_single, landable_double=landable_double,
                jumpable_pin=jumpable_pin, wall_present=wall_present,
                wall_tile_grid_x=wall_tile_grid_x, wall_tile_grid_y=wall_tile_grid_y,
                wall_tile_is_landable_double=wall_tile_is_landable_double,
                wall_tile_has_jumpable_pin=wall_tile_has_jumpable_pin,
            )
        )
    return levels


def build_atlas_and_tables() -> tuple[Atlas, list[RawLevel], dict]:
    module = load_reference_module()
    levels = extract_levels(module)

    images: dict[str, np.ndarray] = {}

    def register(pixels: np.ndarray) -> str:
        pixels = np.asarray(pixels, np.int8)
        key = image_key(pixels)
        images[key] = pixels
        return key

    for name in PEG_COLOR_NAMES:
        register(module.lebqfosqjk[name].sxwqiwdisg())
    for key in (
        "lgbyiaitpdi", "lgbyiaitpdi_0", "csrvckunbev",
        "bhdfjlqapap_0", "bhdfjlqapap_1",
        "jotnhmftwdg_fozwvlovdui", "cwyrzsciwms",
    ):
        register(module.lebqfosqjk[key].sxwqiwdisg())

    pulse_values = pulse_scale_values()
    capture_values = capture_scale_values()
    scaled_keys: dict[tuple[int, int], str] = {}
    for color_index, name in enumerate(PEG_COLOR_NAMES):
        base = np.asarray(module.lebqfosqjk[name].sxwqiwdisg(), np.int8)
        for tick, value in enumerate(pulse_values):
            scaled_keys[(color_index, tick)] = register(module.daeoulgzfx(base, value, value))
        for tick, value in enumerate(capture_values):
            scaled_keys[(color_index, PULSE_BAKED_TICK_COUNT + tick)] = register(module.daeoulgzfx(base, value, value))

    revealed_base = np.asarray(module.lebqfosqjk["jotnhmftwdg_fozwvlovdui"].sxwqiwdisg(), np.int8)
    revealed_capture_fade_keys: dict[int, str] = {}
    for tick, value in enumerate(capture_values):
        revealed_capture_fade_keys[tick] = register(module.daeoulgzfx(revealed_base, value, value))

    engine_cls = module.equnaohchtj
    for level_index in range(LEVEL_COUNT):
        engine = engine_cls()
        engine.whtqurkphir = level_index + 1
        engine.qjwmwkhrml()
        for node in renderer_visit_order(engine.hncnfaqaddg):
            name = getattr(node, "yrxvacxlgrf", "")
            if name.startswith("fozwvlovdui") or node.ultqqtpbdxi is None:
                continue
            pixels = node.ultqqtpbdxi.sxwqiwdisg()
            if pixels.size:
                register(pixels)

    captured = capture_atlas("lf52", SOURCE, actions_per_level=40)
    for index, key in enumerate(captured.keys):
        if key not in images:
            h, w = int(captured.h[index]), int(captured.w[index])
            images[key] = captured.pixels[index, :h, :w]

    atlas = Atlas.from_images(images)
    ids = {key: index for index, key in enumerate(atlas.keys)}

    peg_base_image = np.zeros(3, np.int32)
    peg_pulse_image = np.zeros((3, PULSE_BAKED_TICK_COUNT), np.int32)
    peg_capture_fade_image = np.zeros((3, CAPTURE_FADE_TICK_COUNT), np.int32)
    for color_index, name in enumerate(PEG_COLOR_NAMES):
        base = np.asarray(module.lebqfosqjk[name].sxwqiwdisg(), np.int8)
        peg_base_image[color_index] = ids[image_key(base)]
        for tick in range(PULSE_BAKED_TICK_COUNT):
            peg_pulse_image[color_index, tick] = ids[scaled_keys[(color_index, tick)]]
        for tick in range(CAPTURE_FADE_TICK_COUNT):
            peg_capture_fade_image[color_index, tick] = ids[scaled_keys[(color_index, PULSE_BAKED_TICK_COUNT + tick)]]

    revealed_peg_capture_fade_image = np.zeros(CAPTURE_FADE_TICK_COUNT, np.int32)
    for tick in range(CAPTURE_FADE_TICK_COUNT):
        revealed_peg_capture_fade_image[tick] = ids[revealed_capture_fade_keys[tick]]

    heart_image = np.asarray(
        [ids[image_key(np.asarray(module.lebqfosqjk[k].sxwqiwdisg(), np.int8))] for k in ("lgbyiaitpdi_0", "lgbyiaitpdi")],
        np.int32,
    )
    ring_image = np.int32(ids[image_key(np.asarray(module.lebqfosqjk["csrvckunbev"].sxwqiwdisg(), np.int8))])
    dust_image = np.asarray(
        [ids[image_key(np.asarray(module.lebqfosqjk[k].sxwqiwdisg(), np.int8))] for k in ("bhdfjlqapap_0", "bhdfjlqapap_1")],
        np.int32,
    )
    revealed_peg_image = np.int32(
        ids[image_key(np.asarray(module.lebqfosqjk["jotnhmftwdg_fozwvlovdui"].sxwqiwdisg(), np.int8))]
    )
    reveal_button_image = np.int32(
        ids[image_key(np.asarray(module.lebqfosqjk["cwyrzsciwms"].sxwqiwdisg(), np.int8))]
    )
    dust_shape = module.lebqfosqjk["bhdfjlqapap_1"].sxwqiwdisg().shape
    reveal_button_shape = module.lebqfosqjk["cwyrzsciwms"].sxwqiwdisg().shape

    tables = dict(
        peg_base_image=peg_base_image, peg_pulse_image=peg_pulse_image,
        peg_capture_fade_image=peg_capture_fade_image, heart_image=heart_image, ring_image=ring_image,
        dust_image=dust_image, revealed_peg_image=revealed_peg_image, reveal_button_image=reveal_button_image,
        revealed_peg_capture_fade_image=revealed_peg_capture_fade_image,
        dust_height=np.int32(dust_shape[0]), dust_width=np.int32(dust_shape[1]),
        reveal_button_height=np.int32(reveal_button_shape[0]), reveal_button_width=np.int32(reveal_button_shape[1]),
        ids=ids,
    )
    return atlas, levels, tables


def pack_level_tables(levels: list[RawLevel], tables: dict) -> dict[str, np.ndarray]:
    ids = tables["ids"]

    static_image = np.full((LEVEL_COUNT, STATIC_SLOT_COUNT), -1, np.int32)
    static_x = np.zeros((LEVEL_COUNT, STATIC_SLOT_COUNT), np.int32)
    static_y = np.zeros((LEVEL_COUNT, STATIC_SLOT_COUNT), np.int32)
    static_layer = np.zeros((LEVEL_COUNT, STATIC_SLOT_COUNT), np.int32)
    static_wall_tile_index = np.full((LEVEL_COUNT, STATIC_SLOT_COUNT), -1, np.int32)
    static_wall_tile_local_x = np.zeros((LEVEL_COUNT, STATIC_SLOT_COUNT), np.int32)
    static_wall_tile_local_y = np.zeros((LEVEL_COUNT, STATIC_SLOT_COUNT), np.int32)
    wall_tile_grid_x_initial = np.zeros((LEVEL_COUNT, WALL_TILE_SLOT_COUNT), np.int32)
    wall_tile_grid_y_initial = np.zeros((LEVEL_COUNT, WALL_TILE_SLOT_COUNT), np.int32)
    wall_tile_is_landable_double = np.zeros((LEVEL_COUNT, WALL_TILE_SLOT_COUNT), bool)
    wall_tile_has_jumpable_pin = np.zeros((LEVEL_COUNT, WALL_TILE_SLOT_COUNT), bool)
    wall_tile_count = np.zeros(LEVEL_COUNT, np.int32)
    peg_grid_x = np.zeros((LEVEL_COUNT, PEG_SLOT_COUNT), np.int32)
    peg_grid_y = np.zeros((LEVEL_COUNT, PEG_SLOT_COUNT), np.int32)
    peg_color = np.zeros((LEVEL_COUNT, PEG_SLOT_COUNT), np.int32)
    peg_alive_initial = np.zeros((LEVEL_COUNT, PEG_SLOT_COUNT), bool)
    grid_width = np.zeros(LEVEL_COUNT, np.int32)
    grid_height = np.zeros(LEVEL_COUNT, np.int32)
    offset_x = np.zeros(LEVEL_COUNT, np.int32)
    offset_y = np.zeros(LEVEL_COUNT, np.int32)
    landable_single = np.zeros((LEVEL_COUNT, GRID_MAX_HEIGHT, GRID_MAX_WIDTH), bool)
    landable_double = np.zeros((LEVEL_COUNT, GRID_MAX_HEIGHT, GRID_MAX_WIDTH), bool)
    jumpable_pin = np.zeros((LEVEL_COUNT, GRID_MAX_HEIGHT, GRID_MAX_WIDTH), bool)
    wall_present = np.zeros((LEVEL_COUNT, GRID_MAX_HEIGHT, GRID_MAX_WIDTH), bool)

    for li, level in enumerate(levels):
        assert len(level.static_keys) <= STATIC_SLOT_COUNT, (li, len(level.static_keys))
        assert len(level.peg_grid_x) <= PEG_SLOT_COUNT, (li, len(level.peg_grid_x))
        assert len(level.wall_tile_grid_x) <= WALL_TILE_SLOT_COUNT, (li, len(level.wall_tile_grid_x))
        for si, key in enumerate(level.static_keys):
            static_image[li, si] = ids[key]
            static_x[li, si] = level.static_x[si]
            static_y[li, si] = level.static_y[si]
            static_layer[li, si] = level.static_layer[si]
            static_wall_tile_index[li, si] = level.static_wall_tile_index[si]
            static_wall_tile_local_x[li, si] = level.static_wall_tile_local_x[si]
            static_wall_tile_local_y[li, si] = level.static_wall_tile_local_y[si]
        for si in range(len(level.peg_grid_x)):
            peg_grid_x[li, si] = level.peg_grid_x[si]
            peg_grid_y[li, si] = level.peg_grid_y[si]
            peg_color[li, si] = level.peg_color[si]
            peg_alive_initial[li, si] = True
        for si in range(len(level.wall_tile_grid_x)):
            wall_tile_grid_x_initial[li, si] = level.wall_tile_grid_x[si]
            wall_tile_grid_y_initial[li, si] = level.wall_tile_grid_y[si]
            wall_tile_is_landable_double[li, si] = level.wall_tile_is_landable_double[si]
            wall_tile_has_jumpable_pin[li, si] = level.wall_tile_has_jumpable_pin[si]
        wall_tile_count[li] = len(level.wall_tile_grid_x)
        grid_width[li], grid_height[li] = level.grid_width, level.grid_height
        offset_x[li], offset_y[li] = level.offset_x, level.offset_y
        landable_single[li, : level.grid_height, : level.grid_width] = level.landable_single
        landable_double[li, : level.grid_height, : level.grid_width] = level.landable_double
        jumpable_pin[li, : level.grid_height, : level.grid_width] = level.jumpable_pin
        wall_present[li, : level.grid_height, : level.grid_width] = level.wall_present

    return dict(
        static_image=static_image, static_x=static_x, static_y=static_y, static_layer=static_layer,
        static_wall_tile_index=static_wall_tile_index,
        static_wall_tile_local_x=static_wall_tile_local_x, static_wall_tile_local_y=static_wall_tile_local_y,
        wall_tile_grid_x_initial=wall_tile_grid_x_initial, wall_tile_grid_y_initial=wall_tile_grid_y_initial,
        wall_tile_is_landable_double=wall_tile_is_landable_double,
        wall_tile_has_jumpable_pin=wall_tile_has_jumpable_pin,
        wall_tile_count=wall_tile_count,
        peg_grid_x=peg_grid_x, peg_grid_y=peg_grid_y, peg_color=peg_color, peg_alive_initial=peg_alive_initial,
        grid_width=grid_width, grid_height=grid_height, offset_x=offset_x, offset_y=offset_y,
        landable_single=landable_single, landable_double=landable_double, jumpable_pin=jumpable_pin,
        wall_present=wall_present,
        peg_base_image=tables["peg_base_image"], peg_pulse_image=tables["peg_pulse_image"],
        peg_capture_fade_image=tables["peg_capture_fade_image"], heart_image=tables["heart_image"],
        ring_image=np.asarray(tables["ring_image"]), dust_image=tables["dust_image"],
        revealed_peg_image=np.asarray(tables["revealed_peg_image"]),
        revealed_peg_capture_fade_image=tables["revealed_peg_capture_fade_image"],
        reveal_button_image=np.asarray(tables["reveal_button_image"]),
        dust_height=np.asarray(tables["dust_height"]), dust_width=np.asarray(tables["dust_width"]),
        reveal_button_height=np.asarray(tables["reveal_button_height"]),
        reveal_button_width=np.asarray(tables["reveal_button_width"]),
    )


DATA_PARAM = 'atlas'


def derive(atlas, tables):
    self = Setup(atlas)
    t = {k: np.asarray(v) for k, v in tables.items()}
    self.static_image = t["static_image"]
    self.static_x = t["static_x"]
    self.static_y = t["static_y"]
    self.static_layer = t["static_layer"]
    self.static_wall_tile_index = t["static_wall_tile_index"]
    self.static_wall_tile_local_x = t["static_wall_tile_local_x"]
    self.static_wall_tile_local_y = t["static_wall_tile_local_y"]
    self.wall_tile_grid_x_initial = t["wall_tile_grid_x_initial"]
    self.wall_tile_grid_y_initial = t["wall_tile_grid_y_initial"]
    self.wall_tile_is_landable_double = t["wall_tile_is_landable_double"]
    self.wall_tile_has_jumpable_pin = t["wall_tile_has_jumpable_pin"]
    self.wall_tile_count = t["wall_tile_count"]
    self.peg_grid_x_initial = t["peg_grid_x"]
    self.peg_grid_y_initial = t["peg_grid_y"]
    self.peg_color = t["peg_color"]
    self.peg_alive_initial = t["peg_alive_initial"]
    self.grid_width = t["grid_width"]
    self.grid_height = t["grid_height"]
    self.offset_x = t["offset_x"]
    self.offset_y = t["offset_y"]
    self.landable_single = t["landable_single"]
    self.landable_double = t["landable_double"]
    self.jumpable_pin = t["jumpable_pin"]
    self.wall_present = t["wall_present"]
    self.peg_base_image = t["peg_base_image"]
    self.peg_pulse_image = t["peg_pulse_image"]
    self.peg_capture_fade_image = t["peg_capture_fade_image"]
    self.revealed_peg_capture_fade_image = t["revealed_peg_capture_fade_image"]
    self.heart_image = t["heart_image"]
    self.ring_image = t["ring_image"]
    self.dust_image = t["dust_image"]
    self.revealed_peg_image = t["revealed_peg_image"]
    self.reveal_button_image = t["reveal_button_image"]
    self.dust_height = t["dust_height"]
    self.dust_width = t["dust_width"]
    self.reveal_button_height = t["reveal_button_height"]
    self.reveal_button_width = t["reveal_button_width"]
    self.directions = np.asarray(GRID_DIRECTIONS, np.int32)
    levels = np.arange(LEVEL_COUNT)
    blue_peg_count = np.sum((self.peg_color == BLUE_PEG_COLOR) & self.peg_alive_initial, axis=1)
    self.win_blue_offset = np.where(levels >= 7, blue_peg_count, 0)
    self.win_target_peg_count = np.where((levels == 5) | (levels == 6), 2, 1)
    self.stalemate_action_budget = np.where(levels == 0, 64, np.where(levels >= 5, 640, 320))
    trigger_x = np.zeros((LEVEL_COUNT, JUMP_LANDING_REVEAL_TRIGGER_SLOT_COUNT), np.int32)
    trigger_y = np.zeros((LEVEL_COUNT, JUMP_LANDING_REVEAL_TRIGGER_SLOT_COUNT), np.int32)
    trigger_max_remaining = np.full((LEVEL_COUNT, JUMP_LANDING_REVEAL_TRIGGER_SLOT_COUNT), -1, np.int32)
    trigger_valid = np.zeros((LEVEL_COUNT, JUMP_LANDING_REVEAL_TRIGGER_SLOT_COUNT), bool)
    for li, entries in JUMP_LANDING_REVEAL_TRIGGERS.items():
        for si, (tx, ty, max_remaining) in enumerate(entries):
            trigger_x[li, si] = tx
            trigger_y[li, si] = ty
            trigger_max_remaining[li, si] = max_remaining
            trigger_valid[li, si] = True
    self.jump_landing_reveal_trigger_x = np.asarray(trigger_x)
    self.jump_landing_reveal_trigger_y = np.asarray(trigger_y)
    self.jump_landing_reveal_trigger_max_remaining = np.asarray(trigger_max_remaining)
    self.jump_landing_reveal_trigger_valid = np.asarray(trigger_valid)
    return self


def make_args(source, seed=0):
    del source
    data_path = CACHE.with_suffix(".data.npz")
    if CACHE.with_suffix(".npz").exists() and data_path.exists():
        atlas = Atlas.load(CACHE)
        packed = dict(np.load(data_path))
    else:
        atlas, levels, tables = build_atlas_and_tables()
        packed = pack_level_tables(levels, tables)
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        atlas.save(CACHE)
        np.savez_compressed(data_path, **packed)
    return (atlas, packed,), {}
