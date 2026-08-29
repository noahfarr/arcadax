import types
import sys
import numpy as np
from pathlib import Path
from ._const import BLOCKING_FROM_NAME, Blocking, FRAME_SIZE, INTERACTION_FROM_NAME, Interaction
from ._base import ARCBaseGame, GameData, Setup, extract, load_reference_game, reference_dir


STEP_MOVE = 2


CRANE_PITCH = 4


COLOR_DEATH = 4


COLOR_HUD_FILLED = 0


COLOR_HUD_EMPTY = 3


COLOR_VIGNETTE = 5


DEATH_FRAMES = 14


DEATH_PENALTY = 20


SHAKE_FRAMES = 2


FLICKER_FRAMES = 5


VIGNETTE_START = 3


VIGNETTE_FULL = 16


DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_GRAB, NUM_DIRS = 0, 1, 2, 3, 4, 5


DIR_STEP = np.asarray([[0, 1], [0, -1], [-1, 0], [1, 0], [0, 0]], np.int32)


ATTACH_NONE, ATTACH_BRIXTO, ATTACH_OBJECT = 0, 1, 2


TAG_IGNORE = "ignore"


TAG_TOVEMC = "tovemc"


TAG_OMVZ = "omvz"


TAG_INZEJTIBLE = "inzejtible"


TAG_BUEZNA = "buezna"


TAG_SYS_CLICK = "sys_click"


TAG_TEWFUT = "tewfut"


TAG_NJVD_ROLO = "njvd-rolo"


TAG_PIYQZE = "piyqze"


TAG_JFVA = "jfva"


TAG_GOKNOI = "goknoi"


TAG_CRZSJQ = "crzsjq"


TAG_GRAWWQ_OBJECT = "grawwq-object"


TAG_VCHA = "vcha"


TAG_AYBE = "aybe"


TAG_TEWFUT_COLOR_BUEZNA = "tewfut-color-buezna"


TAG_TEWFUT_COLOR_CYCLE = "tewfut-color-cycle"


_NEG = np.int32(-(2**30))


def _load_reference_module(source: str | Path, seed: int = 0):
    from arcengine import ARCBaseGame

    source = Path(source)
    module = types.ModuleType(f"arcadax_ref_{source.stem.replace('-', '_')}_dc22")
    module.__file__ = str(source)
    sys.modules[module.__name__] = module
    exec(compile(source.read_text(encoding="utf-8"), str(source), "exec"), module.__dict__)

    candidates = [
        value
        for value in vars(module).values()
        if isinstance(value, type) and issubclass(value, ARCBaseGame) and value is not ARCBaseGame
    ]
    if len(candidates) != 1:
        raise RuntimeError(f"expected exactly one game class in {source}, got {candidates}")
    try:
        game = candidates[0](seed=seed)
    except TypeError:
        game = candidates[0]()
    return module, game


def _augmented_extract(source: str | Path, seed: int = 0) -> tuple[GameData, dict]:
    module, game = _load_reference_module(source, seed=seed)
    palette = module.sprites

    per_level: list[list] = []
    for i in range(len(game._clean_levels)):
        game.set_level(i)
        per_level.append(list(game.current_level.get_sprites()))

    num_levels = len(per_level)
    num_slots = max(len(lvl) for lvl in per_level)
    rendered = [[(sp, sp.render()) for sp in lvl] for lvl in per_level]
    patch_h = max(px.shape[0] for lvl in rendered for _, px in lvl)
    patch_w = max(px.shape[1] for lvl in rendered for _, px in lvl)
    tag_names = sorted({t for lvl in per_level for sp in lvl for t in sp.tags})

    shape = (num_levels, num_slots)
    data = GameData(
        game_id=game.game_id,
        tag_names=tag_names,
        pixels=np.full((*shape, patch_h, patch_w), -1, np.int8),
        h=np.zeros(shape, np.int32),
        w=np.zeros(shape, np.int32),
        x=np.zeros(shape, np.int32),
        y=np.zeros(shape, np.int32),
        layer=np.zeros(shape, np.int32),
        order=np.tile(np.arange(num_slots, dtype=np.int32), (num_levels, 1)),
        interaction=np.full(shape, int(Interaction.REMOVED), np.int32),
        blocking=np.full(shape, int(Blocking.NOT_BLOCKED), np.int32),
        tags=np.zeros((*shape, len(tag_names)), bool),
        alive=np.zeros(shape, bool),
        names=[[""] * num_slots for _ in range(num_levels)],
        grid_size=np.zeros((num_levels, 2), np.int32),
        level_data=[dict(lvl._data) for lvl in game._clean_levels],
        background=int(game.camera.background),
        letter_box=int(game.camera.letter_box),
        available_actions=list(game._available_actions),
        win_score=int(game.win_score),
    )

    first_code: list[list[str | None]] = [[None] * num_slots for _ in range(num_levels)]
    for li, lvl in enumerate(per_level):
        for si, sprite in enumerate(lvl):
            first_code[li][si] = next((t for t in sprite.tags if len(t) == 1), None)

    for li, lvl in enumerate(per_level):
        size = game._clean_levels[li].grid_size or (game.camera.width, game.camera.height)
        data.grid_size[li] = size
        for si, (sprite, px) in enumerate(rendered[li]):
            data.pixels[li, si, : px.shape[0], : px.shape[1]] = px
            data.h[li, si] = px.shape[0]
            data.w[li, si] = px.shape[1]
            data.x[li, si] = sprite.x
            data.y[li, si] = sprite.y
            data.layer[li, si] = sprite.layer
            data.interaction[li, si] = INTERACTION_FROM_NAME[sprite.interaction.name]
            blocking = BLOCKING_FROM_NAME[sprite.blocking.name]
            if TAG_IGNORE in sprite.tags:
                blocking = int(Blocking.NOT_BLOCKED)
            data.blocking[li, si] = blocking
            data.alive[li, si] = True
            data.names[li][si] = sprite.name
            for tag in sprite.tags:
                data.tags[li, si, tag_names.index(tag)] = True

    crane_hold_pixels: dict[int, np.ndarray] = {}
    for li, lvl in enumerate(per_level):
        cranes = [sp for sp in lvl if TAG_CRZSJQ in sp.tags]
        if not cranes:
            continue
        prefix = cranes[0].name.rsplit("-", 1)[0]
        template = palette[f"{prefix}-2"].render()
        buf = np.full((patch_h, patch_w), -1, np.int8)
        buf[: template.shape[0], : template.shape[1]] = template
        crane_hold_pixels[li] = buf

    usmccgsno_per_level: list[dict[str, int]] = []
    clean_counts: list[int] = []
    for i in range(num_levels):
        game.set_level(i)
        usmccgsno_per_level.append(dict(game.usmccgsno))
        clean_counts.append(len(game._clean_levels[i].get_sprites()))

    return data, {
        "crane_hold_pixels": crane_hold_pixels,
        "usmccgsno": usmccgsno_per_level,
        "clean_counts": clean_counts,
        "first_code": first_code,
    }


def _locate_source(game_id: str) -> Path:
    matches = sorted(reference_dir().glob(f"{game_id}-*.py"))
    if not matches:
        raise FileNotFoundError(f"no source for {game_id!r} in {reference_dir()}")
    return matches[0]


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    source = _locate_source(data.game_id)
    data, extra = _augmented_extract(source)
    L, N = data.num_levels, data.num_slots
    names = data.names
    alive = data.alive
    tags = data.tags
    def tag_mask(name: str) -> np.ndarray:
        if name not in data.tag_names:
            return np.zeros((L, N), bool)
        return tags[:, :, data.tag_index(name)] & alive
    self._is_jfva = np.asarray(tag_mask(TAG_JFVA))
    self._is_goknoi = np.asarray(tag_mask(TAG_GOKNOI))
    self._is_crzsjq = np.asarray(tag_mask(TAG_CRZSJQ))
    self._is_grawwq_object = np.asarray(tag_mask(TAG_GRAWWQ_OBJECT))
    self._is_buezna = np.asarray(tag_mask(TAG_BUEZNA))
    self._is_sys_click = np.asarray(tag_mask(TAG_SYS_CLICK))
    self._is_tewfut = np.asarray(tag_mask(TAG_TEWFUT))
    self._is_njvd = np.asarray(tag_mask(TAG_NJVD_ROLO))
    self._is_piyqze = np.asarray(tag_mask(TAG_PIYQZE))
    self._is_vcha = np.asarray(tag_mask(TAG_VCHA))
    self._is_aybe = np.asarray(tag_mask(TAG_AYBE))
    self._is_ignore = np.asarray(tag_mask(TAG_IGNORE))
    self._is_iophjflwsn = np.asarray(tag_mask(TAG_TEWFUT_COLOR_BUEZNA))
    self._is_qiukbrokfa = np.asarray(tag_mask(TAG_TEWFUT_COLOR_CYCLE))
    is_omvz = tag_mask(TAG_OMVZ)
    is_inzejtible = tag_mask(TAG_INZEJTIBLE)
    is_buezna_np = tag_mask(TAG_BUEZNA)
    player_slot = np.full(L, -1, np.int32)
    goal_slot = np.full(L, -1, np.int32)
    for li in range(L):
        js = np.flatnonzero(tag_mask(TAG_JFVA)[li])
        if len(js):
            player_slot[li] = js[0]
        gs = np.flatnonzero(tag_mask(TAG_GOKNOI)[li])
        if len(gs):
            goal_slot[li] = gs[0]
    self._player_slot = np.asarray(player_slot)
    self._goal_slot = np.asarray(goal_slot)
    self._budget = np.asarray(
        [int(d.get("StepCounter", 0)) for d in data.level_data], np.int32
    )
    single_char_tags = [t for t in data.tag_names if len(t) == 1]
    code_index = {t: ci for ci, t in enumerate(single_char_tags)}
    code_of = np.full((L, N), -1, np.int32)
    first_code = extra["first_code"]
    for li in range(L):
        for si in range(N):
            code = first_code[li][si]
            if code is not None:
                code_of[li, si] = code_index[code]
    self._code_of = np.asarray(code_of)
    num_codes = max(len(single_char_tags), 1)
    code_member = np.zeros((L, num_codes, N), bool)
    for ci, t in enumerate(single_char_tags):
        code_member[:, ci, :] = tag_mask(t)
    self._code_member = np.asarray(code_member)
    usmccgsno = extra["usmccgsno"]
    clean_counts = extra["clean_counts"]
    families: list[list[dict[int, int]]] = [[] for _ in range(L)]
    next_slot = np.full((L, N), -1, np.int32)
    next_interaction = np.full((L, N), int(Interaction.REMOVED), np.int32)
    for li in range(L):
        cursor = clean_counts[li]
        for si in range(clean_counts[li]):
            name = names[li][si]
            if not name or not name[-1].isdigit() or not tag_mask(TAG_TOVEMC)[li, si]:
                continue
            prefix = name[:-1]
            size = usmccgsno[li].get(prefix, 1)
            if size < 2:
                continue
            owner_frame = int(name[-1])
            members = {owner_frame: si}
            for _ in range(size - 1):
                members[int(names[li][cursor][-1])] = cursor
                cursor += 1
            families[li].append(members)
            for frame, slot in members.items():
                target = members.get(frame % size + 1)
                if target is None:
                    continue
                next_slot[li, slot] = target
                if is_omvz[li, target]:
                    next_interaction[li, slot] = int(Interaction.INTANGIBLE)
                elif is_inzejtible[li, target]:
                    next_interaction[li, slot] = int(Interaction.INVISIBLE)
                elif is_buezna_np[li, target]:
                    next_interaction[li, slot] = int(Interaction.INTANGIBLE)
                else:
                    next_interaction[li, slot] = int(Interaction.TANGIBLE)
        level_slot_count = int(alive[li].sum())
        assert cursor == level_slot_count, (
            f"level {li}: family reconstruction consumed {cursor} of {level_slot_count} slots"
        )
    self._next_slot = np.asarray(next_slot)
    self._next_interaction = np.asarray(next_interaction)
    name_id = np.full((L, N), -1, np.int32)
    teleport_name = np.full((L, N), -1, np.int32)
    for li in range(L):
        name_to_id: dict[str, int] = {}
        for si in range(N):
            if not alive[li, si]:
                continue
            name_to_id.setdefault(names[li][si], len(name_to_id))
            name_id[li, si] = name_to_id[names[li][si]]
        for si in range(N):
            if not alive[li, si] or not tag_mask(TAG_TEWFUT)[li, si]:
                continue
            name = names[li][si]
            if not name or not name[-1].isdigit():
                continue
            prefix, frame = name[:-1], int(name[-1])
            sibling_names = [n for n in name_to_id if n[:-1] == prefix and n[-1].isdigit()]
            size = len(sibling_names)
            if size < 2:
                continue
            target_name = f"{prefix}{frame % size + 1}"
            if target_name in name_to_id:
                teleport_name[li, si] = name_to_id[target_name]
    self._name_id = np.asarray(name_id)
    self._teleport_name = np.asarray(teleport_name)
    njvd_by_code = np.zeros((L, num_codes), bool)
    for li in range(L):
        for si in range(N):
            if alive[li, si] and tag_mask(TAG_NJVD_ROLO)[li, si] and code_of[li, si] >= 0:
                njvd_by_code[li, code_of[li, si]] = True
    self._njvd_by_code = np.asarray(njvd_by_code)
    has_gov = np.zeros((L, N), bool)
    for li in range(L):
        has_gov[li] = np.where(code_of[li] >= 0, njvd_by_code[li][np.clip(code_of[li], 0, num_codes - 1)], False)
    self._has_njvd_governance = np.asarray(has_gov)
    crane_slot = np.full(L, -1, np.int32)
    crane_is_brixto = np.zeros(L, bool)
    crane_origin_x = np.zeros(L, np.int32)
    crane_origin_y = np.zeros(L, np.int32)
    crane_offset_x = np.zeros(L, np.int32)
    crane_offset_y = np.zeros(L, np.int32)
    grawwq_object_slot = np.full(L, -1, np.int32)
    dir_slot = np.full((L, NUM_DIRS), -1, np.int32)
    dir_tags = ["up", "dowlja", "lersnf", "riidpd", "grawwq"]
    crane_hold = np.full((L, data.patch_shape[0], data.patch_shape[1]), -1, np.int8)
    for li in range(L):
        cs = np.flatnonzero(tag_mask(TAG_CRZSJQ)[li])
        if not len(cs):
            continue
        crane_slot[li] = cs[0]
        name = names[li][cs[0]]
        prefix = name.rsplit("-", 1)[0]
        crane_is_brixto[li] = prefix == "brixtocrzsjq"
        crane_origin_x[li] = data.x[li, cs[0]]
        crane_origin_y[li] = data.y[li, cs[0]]
        if crane_is_brixto[li]:
            crane_offset_x[li] = int(data.w[li, cs[0]]) // 2
            crane_offset_y[li] = int(data.h[li, cs[0]]) // 2
        else:
            crane_offset_x[li] = 9
            crane_offset_y[li] = 4
        go = np.flatnonzero(tag_mask(TAG_GRAWWQ_OBJECT)[li])
        if len(go):
            grawwq_object_slot[li] = go[0]
        for d, tag in enumerate(dir_tags):
            ds = np.flatnonzero(tag_mask(tag)[li] & tag_mask(TAG_SYS_CLICK)[li])
            if len(ds):
                dir_slot[li, d] = ds[0]
        if li in extra["crane_hold_pixels"]:
            crane_hold[li] = extra["crane_hold_pixels"][li]
    self._crane_slot = np.asarray(crane_slot)
    self._crane_present = np.asarray(crane_slot >= 0)
    self._crane_is_brixto = np.asarray(crane_is_brixto)
    self._crane_origin_x = np.asarray(crane_origin_x)
    self._crane_origin_y = np.asarray(crane_origin_y)
    self._crane_offset_x = np.asarray(crane_offset_x)
    self._crane_offset_y = np.asarray(crane_offset_y)
    self._grawwq_object_slot = np.asarray(grawwq_object_slot)
    self._dir_slot = np.asarray(dir_slot)
    self._crane_hold_pixels = np.asarray(crane_hold)
    is_brixto_candidate = np.zeros((L, N), bool)
    for li in range(L):
        for members in families[li]:
            if len(members) != 2:
                continue
            owner = min(members.values())
            name = names[li][owner]
            if name[:-1].startswith("brixto"):
                for slot in members.values():
                    is_brixto_candidate[li, slot] = True
    self._is_brixto_candidate = np.asarray(is_brixto_candidate)
    vcha_map = np.zeros((L, FRAME_SIZE, FRAME_SIZE), bool)
    for li in range(L):
        for si in np.flatnonzero(tag_mask(TAG_VCHA)[li]):
            h, w = int(data.h[li, si]), int(data.w[li, si])
            px = data.pixels[li, si, :h, :w]
            ys, xs = np.nonzero(px >= 0)
            x0, y0 = int(data.x[li, si]), int(data.y[li, si])
            vcha_map[li, y0 + ys, x0 + xs] = True
    self._vcha_map = np.asarray(vcha_map)
    self._num_codes = num_codes
    return self


def make_args(source, seed=0):
    data, _ = _augmented_extract(source)
    return (data,), {}
