import dataclasses

import numpy as np

from .dsl import (BECOME, BLOCK, EMPTY, LOSE, NONE, PUSH, REMOVE, TOGGLE,
                  WIN_ALL_ON, WIN_NONE_LEFT, WIN_REACH, Kind, Spec)

PALETTE = [1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
ENTER_EFFECTS = [NONE, BLOCK, REMOVE, PUSH, LOSE]
CLICK_EFFECTS = [NONE, REMOVE, BECOME, TOGGLE]


@dataclasses.dataclass
class Proposal:
    spec: Spec
    seed: int
    mechanics: dict


def _layout(rng, w, h, kinds_present, wall, player, wall_density):
    floor = np.zeros((1, h, w), np.int8)
    obj = np.full((1, h, w), EMPTY, np.int8)
    obj[0, 0, :] = wall
    obj[0, -1, :] = wall
    obj[0, :, 0] = wall
    obj[0, :, -1] = wall
    free = [(y, x) for y in range(1, h - 1) for x in range(1, w - 1)]
    rng.shuffle(free)
    n_walls = int(len(free) * wall_density)
    for y, x in free[:n_walls]:
        obj[0, y, x] = wall
    cells = free[n_walls:]
    return floor, obj, cells


def sample(rng, seed: int) -> Proposal | None:
    w = int(rng.integers(6, 9))
    h = int(rng.integers(6, 9))
    colors = list(rng.permutation(PALETTE))

    floor_kind, wall_kind, player_kind = 0, 1, 2
    kinds = [Kind(color=int(colors[0])),
             Kind(color=int(colors[1]), on_enter=BLOCK),
             Kind(color=int(colors[2]))]
    extra = int(rng.integers(1, 4))
    mechanics = {}
    for i in range(extra):
        on_enter = int(rng.choice(ENTER_EFFECTS))
        on_click = int(rng.choice(CLICK_EFFECTS)) if rng.random() < 0.4 else NONE
        k = Kind(color=int(colors[3 + i]), on_enter=on_enter, on_click=on_click)
        if on_click in (BECOME,):
            k.click_a = floor_kind
        if on_click == TOGGLE:
            k.click_a, k.click_b = wall_kind, floor_kind
        kinds.append(k)
        mechanics[3 + i] = (on_enter, on_click)

    pushable = [i for i, k in enumerate(kinds) if k.on_enter == PUSH]
    removable = [i for i, k in enumerate(kinds) if k.on_enter == REMOVE]
    goal_kind = len(kinds)
    kinds.append(Kind(color=int(colors[3 + extra])))

    if pushable:
        win_mode, win_a, win_b = WIN_ALL_ON, pushable[0], goal_kind
    elif removable:
        win_mode, win_a, win_b = WIN_NONE_LEFT, removable[0], -1
    else:
        win_mode, win_a, win_b = WIN_REACH, goal_kind, -1
    if len(kinds) > 16:
        return None

    floor, obj, cells = _layout(rng, w, h, kinds, wall_kind, player_kind,
                                float(rng.uniform(0.0, 0.18)))
    if len(cells) < 6:
        return None

    at = 0
    py, px = cells[at]
    obj[0, py, px] = player_kind
    at += 1

    if win_mode == WIN_ALL_ON:
        n = int(rng.integers(1, 3))
        for _ in range(n):
            if at + 1 >= len(cells):
                return None
            by, bx = cells[at]
            obj[0, by, bx] = win_a
            at += 1
            gy, gx = cells[at]
            floor[0, gy, gx] = goal_kind
            at += 1
    elif win_mode == WIN_NONE_LEFT:
        for _ in range(int(rng.integers(1, 4))):
            if at >= len(cells):
                return None
            y, x = cells[at]
            obj[0, y, x] = win_a
            at += 1
    else:
        if at >= len(cells):
            return None
        y, x = cells[at]
        floor[0, y, x] = goal_kind
        at += 1

    for i, k in enumerate(kinds):
        if i in (floor_kind, wall_kind, player_kind, goal_kind):
            continue
        if k.on_enter in (PUSH, REMOVE) and i == win_a:
            continue
        for _ in range(int(rng.integers(0, 3))):
            if at >= len(cells):
                break
            y, x = cells[at]
            obj[0, y, x] = i
            at += 1

    spec = Spec(kinds=kinds, layouts=obj, floors=floor,
                player_kind=player_kind, win_mode=win_mode, win_a=win_a,
                win_b=win_b, pitch=int(min(64 // max(w, h), 8)),
                origin_x=1, origin_y=1)
    return Proposal(spec=spec, seed=seed, mechanics=mechanics)
