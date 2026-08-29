import ctypes
import dataclasses

import numpy as np

from .clib import Library
from .reference import Levels

MAX_KINDS = 16
NONE, BLOCK, REMOVE, PUSH, BECOME, TOGGLE, WIN, LOSE = range(8)
WIN_NONE_LEFT, WIN_ALL_ON, WIN_REACH = range(3)
ON_ENTER, ON_CLICK, ON_STEP = range(3)
ALWAYS, IF_COUNT_LE, IF_NONE_LEFT, IF_ADJACENT = range(4)
MAX_RULES = 8


@dataclasses.dataclass
class Rule:
    trigger: int
    subject: int
    effect: int
    predicate: int = ALWAYS
    pred_a: int = -1
    pred_b: int = -1
    effect_a: int = -1
    effect_b: int = -1
    enabled: int = 1
EMPTY = -1


STATIC, CHASE, FLEE, PATROL = range(4)


@dataclasses.dataclass
class Kind:
    color: int
    motion: int = STATIC
    motion_a: int = 0
    motion_b: int = -1
    deadly: int = 0
    gravity: int = 0
    on_enter: int = NONE
    enter_a: int = -1
    enter_b: int = -1
    on_click: int = NONE
    click_a: int = -1
    click_b: int = -1


@dataclasses.dataclass
class Spec:
    kinds: list[Kind]
    layouts: np.ndarray
    floors: np.ndarray
    player_kind: int
    win_mode: int
    win_a: int = -1
    win_b: int = -1
    pitch: int = 4
    origin_x: int = 0
    origin_y: int = 0
    background: int = 0
    rules: list = dataclasses.field(default_factory=list)

    @property
    def num_levels(self) -> int:
        return self.layouts.shape[0]

    @property
    def grid_h(self) -> int:
        return self.layouts.shape[1]

    @property
    def grid_w(self) -> int:
        return self.layouts.shape[2]


class DslGame:
    def __init__(self, spec: Spec, library: Library | None = None,
                 max_frames: int = 8) -> None:
        self.library = library or Library()
        self.spec = spec
        self._keep: list = []
        h = self.library.headers

        kind_t = h.struct("arc_dsl_kind")
        spec_t = h.struct("arc_dsl_spec")
        aux_t = h.struct("arc_dsl_aux")

        layout = np.ascontiguousarray(spec.layouts, np.int8)
        floor = np.ascontiguousarray(spec.floors, np.int8)
        self._keep += [layout, floor]

        native = spec_t()
        native.num_kinds = len(spec.kinds)
        native.num_levels = spec.num_levels
        native.grid_w = spec.grid_w
        native.grid_h = spec.grid_h
        native.pitch = spec.pitch
        native.origin_x = spec.origin_x
        native.origin_y = spec.origin_y
        native.player_kind = spec.player_kind
        native.win_mode = spec.win_mode
        native.win_a = spec.win_a
        native.win_b = spec.win_b
        native.background = spec.background
        rule_t = h.struct("arc_dsl_rule")
        native.num_rules = len(spec.rules)
        for i, r in enumerate(spec.rules):
            native.rules[i] = rule_t(r.trigger, r.subject, r.predicate,
                                     r.pred_a, r.pred_b, r.effect, r.effect_a,
                                     r.effect_b, r.enabled)
        for i, k in enumerate(spec.kinds):
            native.kinds[i] = kind_t(k.color, k.motion, k.motion_a,
                                     k.motion_b, k.deadly, k.gravity,
                                     k.on_enter,
                                     k.enter_a, k.enter_b, k.on_click,
                                     k.click_a, k.click_b)
        native.layout = layout.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))
        native.floor = floor.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))
        self._keep.append(native)

        aux = aux_t()
        self._keep.append(aux)

        self.levels = self._blank_levels()
        level_data = self._level_data()

        simple = np.ascontiguousarray([1, 2, 3, 4], np.int32)
        self._keep.append(simple)
        hooks = ctypes.c_void_p.in_dll(self.library.lib, "arc_dsl_hooks")
        self.handle = self.library.sym.game_new(
            ctypes.byref(level_data), ctypes.addressof(hooks),
            ctypes.byref(aux), ctypes.byref(native),
            simple.ctypes.data_as(ctypes.c_void_p), 4, 1, max_frames,
        )
        self.max_frames = max_frames
        self.frames = np.zeros((max_frames, 64, 64), np.int8)
        self._ptr = self.frames.ctypes.data_as(ctypes.c_void_p)

    def _blank_levels(self) -> Levels:
        n, s = self.spec.num_levels, 1
        return Levels(
            game_id="dsl", tag_names=[],
            pixels=np.full((n, s, 1, 1), -1, np.int8),
            h=np.zeros((n, s), np.int32), w=np.zeros((n, s), np.int32),
            x=np.zeros((n, s), np.int32), y=np.zeros((n, s), np.int32),
            layer=np.zeros((n, s), np.int32),
            order=np.zeros((n, s), np.int32),
            interaction=np.full((n, s), 3, np.int32),
            blocking=np.zeros((n, s), np.int32),
            alive=np.zeros((n, s), bool),
            tags=np.zeros((n, s, 0), bool),
            grid_size=np.tile(np.array([64, 64], np.int32), (n, 1)),
            names=[[""] for _ in range(n)], level_data=[{} for _ in range(n)],
            background=self.spec.background, letter_box=0,
            win_score=n, available_actions=[1, 2, 3, 4, 6],
        )

    def _level_data(self):
        from .clib import LEVEL_DTYPES, LEVEL_FIELDS, as_pointer
        from . import statics

        t = self.library.headers.struct("LevelData")
        declared = dict(t._fields_)
        values = {}
        for name in LEVEL_FIELDS:
            arr = np.ascontiguousarray(getattr(self.levels, name),
                                       LEVEL_DTYPES.get(name, np.int32))
            self._keep.append(arr)
            values[name] = as_pointer(arr, declared[name], name)
        values.update(num_levels=self.spec.num_levels, num_slots=1, num_tags=0,
                      ph=1, pw=1, win_score=self.spec.num_levels,
                      background=self.spec.background, letter_box=0)
        data, owned = statics.pack(t, values)
        self._keep += owned + [data]
        return data

    def init(self):
        self.library.sym.game_init(self.handle)

    def act(self, action_id, x=0, y=0):
        n = self.library.sym.game_perform_action_frames(
            self.handle, int(action_id), int(x), int(y), self._ptr,
            self.max_frames)
        return [self.frames[i].copy() for i in range(min(n, self.max_frames))]

    @property
    def score(self):
        return int(self.library.sym.harness_score(self.handle))

    @property
    def state(self):
        return self.library.status_names()[
            int(self.library.sym.harness_status(self.handle))]

    def close(self):
        if self.handle is not None:
            self.library.sym.game_free(self.handle)
            self.handle = None
