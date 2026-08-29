import types
import sys
import numpy as np
from pathlib import Path
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


EXTRACT_KWARGS = {"extra_slots": 2}


MAX_MOVABLE = 2


MAX_AXIS = 2


MAX_SELECTABLE = MAX_AXIS + MAX_MOVABLE


MAX_UNDO = 320


GRID_SIZE = 21


GRID_H = GRID_SIZE


GRID_W = GRID_SIZE


GHOST_COLOR = 4


BACKGROUND_COLOR = 9


PADDING_COLOR = 5


TARGET_COLOR = 11


EXCLUDED_MOVABLE_COLOR = 5


EXCLUDED_MIRROR_COLOR = 10


SELECTED_COLOR = 0


PLAIN_COLOR = BACKGROUND_COLOR


PRIORITY_TARGET = 4


PRIORITY_EXCLUDED = 3


PRIORITY_SELECTED = 2


PRIORITY_PLAIN = 1


PRIORITY_NONE = -1


ENERGY_COLORS = (11, 12, 15, 8, 14)


ENERGY_TIERS = len(ENERGY_COLORS)


MINIMAP_SCALE = 3


MINIMAP_OFFSET = 1


MOVABLE_TAG = "0006lxjtqggkmi"


VMIRROR_TAG = "0054kgxrvfihgm"


HMIRROR_TAG = "0002nuguepuujf"


AXIS_TAG = "0003uqrdzdofso"


EXCLUDED_TAG = "0056icpryeujyf"


TARGET_TAG = "0001sruqbuvukh"


HINT_TEMPLATE = "0004afwyadxelg"


HINT_ANIMATION_FRAMES = 8


OUT_OF_BOUNDS = 1 << 30


def load_sprite_templates(source: Path) -> dict:
    module = types.ModuleType(f"ar25_templates_{source.stem.replace('-', '_')}")
    module.__file__ = str(source)
    sys.modules[module.__name__] = module
    exec(compile(source.read_text(encoding="utf-8"), str(source), "exec"), module.__dict__)
    return module.sprites


def _locate_source(game_id: str) -> Path:
    matches = sorted(reference_dir().glob(f"{game_id}-*.py"))
    if not matches:
        raise FileNotFoundError(f"no source for {game_id!r} in {reference_dir()}")
    return matches[0]


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    L = data.num_levels
    N = data.num_slots
    grid_w = data.grid_size[:, 0]
    grid_h = data.grid_size[:, 1]
    assert np.all(grid_w == GRID_SIZE) and np.all(grid_h == GRID_SIZE), (
        "ar25 grid size is not the assumed constant 21x21 for every level"
    )
    steps_budget = np.asarray(
        [d.get("StepCounter", 0) for d in data.level_data], np.int32
    )
    assert steps_budget.max() <= MAX_UNDO, "MAX_UNDO must bound the step budget"
    vmirror_slot = np.full((L,), -1, np.int32)
    hmirror_slot = np.full((L,), -1, np.int32)
    movable_slot = np.full((L, MAX_MOVABLE), -1, np.int32)
    axis_slot = np.full((L, MAX_AXIS), -1, np.int32)
    cycle_order = np.full((L, MAX_SELECTABLE), -1, np.int32)
    cycle_count = np.zeros((L,), np.int32)
    initial_selected = np.full((L,), -1, np.int32)
    excluded = np.zeros((L, N), bool)
    movable_role = np.zeros((L, N), bool)
    axis_role = np.zeros((L, N), bool)
    target_grid = np.zeros((L, GRID_H, GRID_W), bool)
    for li in range(L):
        excluded_slots = set(data.slots_with_tag(li, EXCLUDED_TAG))
        for slot in excluded_slots:
            excluded[li, slot] = True
    
        axis_slots = data.slots_with_tag(li, AXIS_TAG)
        movable_slots = data.slots_with_tag(li, MOVABLE_TAG)
        for i, slot in enumerate(axis_slots):
            axis_slot[li, i] = slot
            axis_role[li, slot] = True
        for i, slot in enumerate(movable_slots):
            movable_slot[li, i] = slot
            movable_role[li, slot] = True
        for slot in data.slots_with_tag(li, VMIRROR_TAG):
            vmirror_slot[li] = slot
        for slot in data.slots_with_tag(li, HMIRROR_TAG):
            hmirror_slot[li] = slot
    
        axis_selectable = [s for s in axis_slots if s not in excluded_slots]
        movable_selectable = [s for s in movable_slots if s not in excluded_slots]
        order = axis_selectable + movable_selectable
        for i, slot in enumerate(order):
            cycle_order[li, i] = slot
        cycle_count[li] = len(order)
        if axis_selectable:
            initial_selected[li] = axis_selectable[0]
        elif movable_selectable:
            initial_selected[li] = movable_selectable[0]
    
        for slot in data.slots_with_tag(li, TARGET_TAG):
            tx, ty = int(data.x[li, slot]), int(data.y[li, slot])
            if 0 <= tx < GRID_W and 0 <= ty < GRID_H:
                target_grid[li, ty, tx] = True
    self._grid_w = GRID_W
    self._grid_h = GRID_H
    self._steps_budget = np.asarray(steps_budget)
    self._vmirror_slot = np.asarray(vmirror_slot)
    self._hmirror_slot = np.asarray(hmirror_slot)
    self._movable_slot = np.asarray(movable_slot)
    self._axis_slot = np.asarray(axis_slot)
    self._cycle_order = np.asarray(cycle_order)
    self._cycle_count = np.asarray(cycle_count)
    self._initial_selected = np.asarray(initial_selected)
    self._excluded = np.asarray(excluded)
    self._movable_role = np.asarray(movable_role)
    self._axis_role = np.asarray(axis_role)
    self._trackable_role = np.asarray(movable_role | axis_role)
    self._target_grid = np.asarray(target_grid)
    templates = load_sprite_templates(_locate_source(data.game_id))
    hint_template = templates[HINT_TEMPLATE]
    ph, pw = data.patch_shape
    hint_pixels = np.full((ph, pw), -1, np.int8)
    raw = np.asarray(hint_template.pixels, np.int8)
    hh, hw = raw.shape
    hint_pixels[:hh, :hw] = raw
    self._hint_pixels = np.asarray(hint_pixels)
    self._hint_h = int(hh)
    self._hint_w = int(hw)
    self._hint_layer = int(hint_template.layer)
    self._ghost_slot = self.num_slots - 2
    self._hint_slot = self.num_slots - 1
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
