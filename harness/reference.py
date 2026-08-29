import dataclasses
import sys
import types
from pathlib import Path

import numpy as np
from arcengine import ARCBaseGame, ActionInput, GameAction
from arcengine.base_game import MAX_FRAME_PER_ACTION

CLICK_ACTION = GameAction.ACTION6.value


def load_game(source: str | Path, seed: int = 0) -> ARCBaseGame:
    source = Path(source)
    name = f"arc_reference_{source.stem.replace('-', '_')}"
    module = types.ModuleType(name)
    module.__file__ = str(source)
    sys.modules[name] = module
    exec(compile(source.read_text(encoding="utf-8"), str(source), "exec"), module.__dict__)

    found = [
        value
        for value in vars(module).values()
        if isinstance(value, type) and issubclass(value, ARCBaseGame) and value is not ARCBaseGame
    ]
    if len(found) != 1:
        raise RuntimeError(f"expected one ARCBaseGame subclass in {source}, found {len(found)}")
    try:
        return found[0](seed=seed)
    except TypeError:
        return found[0]()


@dataclasses.dataclass
class Levels:
    game_id: str
    tag_names: list[str]
    pixels: np.ndarray
    h: np.ndarray
    w: np.ndarray
    x: np.ndarray
    y: np.ndarray
    layer: np.ndarray
    order: np.ndarray
    interaction: np.ndarray
    blocking: np.ndarray
    alive: np.ndarray
    tags: np.ndarray
    grid_size: np.ndarray
    names: list[list[str]]
    level_data: list[dict]
    background: int
    letter_box: int
    win_score: int
    available_actions: list[int]

    @property
    def num_levels(self) -> int:
        return self.pixels.shape[0]

    @property
    def num_slots(self) -> int:
        return self.pixels.shape[1]

    @property
    def patch_shape(self) -> tuple[int, int]:
        return self.pixels.shape[2], self.pixels.shape[3]

    def slot_of(self, level: int, name: str) -> int:
        matches = [i for i, n in enumerate(self.names[level]) if n == name]
        if len(matches) != 1:
            raise KeyError(f"{name!r} matches {len(matches)} slots on level {level}")
        return matches[0]

    def tag_index(self, tag: str) -> int:
        return self.tag_names.index(tag)

    def slots_with_tag(self, level: int, tag: str) -> list[int]:
        idx = self.tag_index(tag)
        return [i for i in range(self.num_slots)
                if self.alive[level, i] and self.tags[level, i, idx]]

    @property
    def simple_actions(self) -> list[int]:
        return [a for a in self.available_actions if a != CLICK_ACTION]

    @property
    def has_click(self) -> bool:
        return CLICK_ACTION in self.available_actions


def extract_levels(game: ARCBaseGame, enums: dict[str, int], extra_slots: int = 0) -> Levels:
    levels = [level.clone() for level in game._clean_levels]
    rendered = [[(s, s.render()) for s in level.get_sprites()] for level in levels]

    num_levels = len(levels)
    num_slots = max(len(r) for r in rendered) + extra_slots
    ph = max((px.shape[0] for r in rendered for _, px in r), default=1)
    pw = max((px.shape[1] for r in rendered for _, px in r), default=1)
    tag_names = sorted({t for level in levels for s in level.get_sprites() for t in s.tags})

    shape = (num_levels, num_slots)
    out = Levels(
        game_id=game.game_id,
        tag_names=tag_names,
        pixels=np.full((*shape, ph, pw), -1, np.int8),
        h=np.zeros(shape, np.int32),
        w=np.zeros(shape, np.int32),
        x=np.zeros(shape, np.int32),
        y=np.zeros(shape, np.int32),
        layer=np.zeros(shape, np.int32),
        order=np.tile(np.arange(num_slots, dtype=np.int32), (num_levels, 1)),
        interaction=np.full(shape, enums["REMOVED"], np.int32),
        blocking=np.full(shape, enums["NOT_BLOCKED"], np.int32),
        alive=np.zeros(shape, bool),
        tags=np.zeros((*shape, len(tag_names)), bool),
        grid_size=np.zeros((num_levels, 2), np.int32),
        names=[[""] * num_slots for _ in range(num_levels)],
        level_data=[dict(level._data) for level in levels],
        background=int(game.camera.background),
        letter_box=int(game.camera.letter_box),
        win_score=int(game.win_score),
        available_actions=list(game._available_actions),
    )

    for li, level in enumerate(levels):
        out.grid_size[li] = level.grid_size or (game.camera.width, game.camera.height)
        for si, (sprite, px) in enumerate(rendered[li]):
            out.pixels[li, si, : px.shape[0], : px.shape[1]] = px
            out.h[li, si] = px.shape[0]
            out.w[li, si] = px.shape[1]
            out.x[li, si] = sprite.x
            out.y[li, si] = sprite.y
            out.layer[li, si] = sprite.layer
            out.interaction[li, si] = enums[sprite.interaction.name]
            out.blocking[li, si] = enums[sprite.blocking.name]
            out.alive[li, si] = True
            out.names[li][si] = sprite.name
            for tag in sprite.tags:
                out.tags[li, si, tag_names.index(tag)] = True
    return out


class Runner:
    def __init__(self, game: ARCBaseGame) -> None:
        self.game = game

    def reset(self) -> list[np.ndarray]:
        return self.act(GameAction.RESET.value, 0, 0)

    def act(self, action_id: int, x: int, y: int) -> list[np.ndarray]:
        data = {"x": x, "y": y} if action_id == CLICK_ACTION else {}
        out = self.game.perform_action(
            ActionInput(id=GameAction.from_id(action_id), data=data), raw=True
        )
        return [np.asarray(f, np.int8) for f in out.frame]

    def set_level(self, index: int) -> None:
        self.game.set_level(index)

    @property
    def score(self) -> int:
        return int(self.game._score)

    @property
    def state(self) -> str:
        return self.game._state.value

    @property
    def level_index(self) -> int:
        return int(self.game.level_index)


MAX_FRAMES = MAX_FRAME_PER_ACTION
