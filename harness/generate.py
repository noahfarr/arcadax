import dataclasses

import numpy as np

ARC_MAX_KINDS = 16

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


def sample_push(rng, w=9, h=9, boxes=2, pulls=20, wall_density=0.08,
                colors=None):
    from .dsl import WIN_ALL_ON

    colors = colors or [11, 9, 8, 13, 7]
    floor_kind, wall_kind, player_kind, box_kind, goal_kind = 0, 1, 2, 3, 4
    kinds = [Kind(color=int(colors[0])),
             Kind(color=int(colors[1]), on_enter=BLOCK),
             Kind(color=int(colors[2])),
             Kind(color=int(colors[3]), on_enter=PUSH),
             Kind(color=int(colors[4]))]

    floor = np.full((1, h, w), EMPTY, np.int8)
    obj = np.full((1, h, w), EMPTY, np.int8)
    obj[0, 0, :] = wall_kind
    obj[0, -1, :] = wall_kind
    obj[0, :, 0] = wall_kind
    obj[0, :, -1] = wall_kind

    interior = [(y, x) for y in range(1, h - 1) for x in range(1, w - 1)]
    rng.shuffle(interior)
    n_walls = int(len(interior) * wall_density)
    for y, x in interior[:n_walls]:
        obj[0, y, x] = wall_kind
    free = interior[n_walls:]
    if len(free) < boxes + 2:
        return None

    box_cells = [tuple(c) for c in free[:boxes]]
    for y, x in box_cells:
        floor[0, y, x] = goal_kind
        obj[0, y, x] = box_kind
    py, px = free[boxes]

    def is_free(y, x):
        return 0 <= y < h and 0 <= x < w and obj[0, y, x] == EMPTY

    dirs = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    applied = 0
    for _ in range(pulls * 40):
        if applied >= pulls:
            break
        options = [(dy, dx) for dy, dx in dirs
                   if 0 <= py + dy < h and 0 <= px + dx < w
                   and obj[0, py + dy, px + dx] == box_kind
                   and is_free(py - dy, px - dx)]
        if options and rng.random() < 0.85:
            dy, dx = options[int(rng.integers(len(options)))]
            obj[0, py + dy, px + dx] = EMPTY
            obj[0, py, px] = box_kind
            py, px = py - dy, px - dx
            applied += 1
            continue
        cells = np.argwhere(obj[0] == box_kind)
        if len(cells) and rng.random() < 0.7:
            target = cells[int(rng.integers(len(cells)))]
            best = None
            for dy, dx in dirs:
                if not is_free(py + dy, px + dx):
                    continue
                d = abs(py + dy - target[0]) + abs(px + dx - target[1])
                if best is None or d < best[0]:
                    best = (d, dy, dx)
            if best is not None:
                py, px = py + best[1], px + best[2]
                continue
        dy, dx = dirs[int(rng.integers(4))]
        if is_free(py + dy, px + dx):
            py, px = py + dy, px + dx

    if applied == 0:
        return None
    obj[0, py, px] = player_kind
    pitch = int(max(1, min(64 // max(w, h), 8)))
    spec = Spec(kinds=kinds, layouts=obj, floors=floor,
                player_kind=player_kind, win_mode=WIN_ALL_ON, win_a=box_kind,
                win_b=goal_kind, pitch=pitch,
                origin_x=(64 - w * pitch) // 2,
                origin_y=(64 - h * pitch) // 2)
    return Proposal(spec=spec, seed=0, mechanics={"pulls": applied})


LADDER = [
    dict(w=7, h=7, boxes=1, pulls=6, wall_density=0.04),
    dict(w=9, h=9, boxes=1, pulls=25, wall_density=0.06),
    dict(w=9, h=9, boxes=2, pulls=60, wall_density=0.08),
    dict(w=11, h=11, boxes=3, pulls=150, wall_density=0.10),
    dict(w=13, h=13, boxes=4, pulls=250, wall_density=0.10),
    dict(w=15, h=15, boxes=5, pulls=400, wall_density=0.10),
]


def sample_environment(rng, levels=6):
    from .dsl import Spec, WIN_ALL_ON

    colors = [int(c) for c in rng.permutation(PALETTE)[:5]]
    w = max(cfg["w"] for cfg in LADDER[:levels])
    h = max(cfg["h"] for cfg in LADDER[:levels])
    stack_obj, stack_floor, meta = [], [], []
    for i in range(levels):
        cfg = dict(LADDER[i % len(LADDER)])
        p = None
        for _ in range(25):
            cand = sample_push(rng, colors=colors, **cfg)
            if cand is None:
                continue
            if cand.mechanics["pulls"] >= 0.6 * cfg["pulls"]:
                p = cand
                break
            if p is None or cand.mechanics["pulls"] > p.mechanics["pulls"]:
                p = cand
        if p is None:
            return None
        obj = np.full((h, w), EMPTY, np.int8)
        flr = np.full((h, w), EMPTY, np.int8)
        obj[:] = 1
        oy = (h - cfg["h"]) // 2
        ox = (w - cfg["w"]) // 2
        obj[oy:oy + cfg["h"], ox:ox + cfg["w"]] = p.spec.layouts[0]
        flr[oy:oy + cfg["h"], ox:ox + cfg["w"]] = p.spec.floors[0]
        stack_obj.append(obj)
        stack_floor.append(flr)
        meta.append({"pulls": p.mechanics["pulls"], "boxes": cfg["boxes"]})
    proto = p.spec
    pitch = int(max(1, min(64 // max(w, h), 8)))
    spec = Spec(kinds=proto.kinds, layouts=np.stack(stack_obj),
                floors=np.stack(stack_floor), player_kind=proto.player_kind,
                win_mode=WIN_ALL_ON, win_a=proto.win_a, win_b=proto.win_b,
                pitch=pitch, origin_x=(64 - w * pitch) // 2,
                origin_y=(64 - h * pitch) // 2)
    return Proposal(spec=spec, seed=0, mechanics={"levels": meta})


MECHANICS = ("key", "switch", "collect")
HAZARDS = ("patrol", "chase")


def randomise_appearance(rng, spec, protect=()):
    import dataclasses

    pitch = spec.pitch
    kinds = []
    for i, k in enumerate(spec.kinds):
        if i in protect or pitch < 3:
            kinds.append(k)
            continue
        span = int(np.clip(round(pitch * rng.beta(1.1, 2.2)), 1, pitch))
        slack = pitch - span
        kinds.append(dataclasses.replace(
            k, size=span,
            off_x=int(rng.integers(0, slack + 1)),
            off_y=int(rng.integers(0, slack + 1))))
    return dataclasses.replace(spec, kinds=kinds)


def sample_composed(rng, num_mechanics=3, room_w=3, room_h=5, levels=None,
                    hazards=0, scenery=6):
    from .dsl import (IF_NONE_LEFT, ON_CLICK, ON_ENTER, ON_STEP, REMOVE, Rule,
                      Spec, TOGGLE, WIN_REACH)

    rooms = num_mechanics + 1

    swatch = [int(c) for c in rng.permutation(PALETTE)[:int(rng.integers(5, 8))]]
    colors = [swatch[i % len(swatch)] for i in range(2 * ARC_MAX_KINDS)]
    rng.shuffle(colors)
    floor_k, wall_k, player_k, goal_k = 0, 1, 2, 3
    kinds = [Kind(color=int(colors[0])),
             Kind(color=int(colors[1]), on_enter=BLOCK),
             Kind(color=int(colors[2])),
             Kind(color=int(colors[3]))]
    rules = []
    door_kind, trigger_kind, chosen = [], [], []
    for m in range(num_mechanics):
        kind = str(rng.choice(MECHANICS))
        chosen.append(kind)
        d = len(kinds)
        kinds.append(Kind(color=int(colors[4 + 2 * m]), on_enter=BLOCK))
        t = len(kinds)
        kinds.append(Kind(color=int(colors[5 + 2 * m]),
                          on_enter=REMOVE if kind != "switch" else NONE))
        door_kind.append(d)
        trigger_kind.append(t)
        if kind == "key":
            rules.append(Rule(trigger=ON_ENTER, subject=t, effect=TOGGLE,
                              effect_a=d, effect_b=floor_k))
        elif kind == "switch":
            rules.append(Rule(trigger=ON_CLICK, subject=t, effect=TOGGLE,
                              effect_a=d, effect_b=floor_k))
        else:
            rules.append(Rule(trigger=ON_STEP, subject=-1, effect=TOGGLE,
                              predicate=IF_NONE_LEFT, pred_a=t,
                              effect_a=d, effect_b=floor_k))
    from .dsl import CHASE, PATROL

    scenery_kinds = []
    for _ in range(min(3, max(0, scenery))):
        if len(kinds) >= 14:
            break
        scenery_kinds.append(len(kinds))
        kinds.append(Kind(color=int(colors[len(kinds)])))

    hazard_kinds = []
    for hz in range(hazards):
        style = str(rng.choice(HAZARDS))
        if style == "patrol":
            a = len(kinds)
            kinds.append(Kind(color=int(colors[-1 - 2 * hz]), motion=PATROL,
                              motion_a=3, motion_b=a + 1, deadly=1))
            kinds.append(Kind(color=int(colors[-1 - 2 * hz]), motion=PATROL,
                              motion_a=2, motion_b=a, deadly=1))
            hazard_kinds.append(a)
        else:
            a = len(kinds)
            kinds.append(Kind(color=int(colors[-1 - 2 * hz]), motion=CHASE,
                              deadly=1))
            hazard_kinds.append(a)
    if len(kinds) > 16 or len(rules) > 8:
        return None

    import math

    cols = max(1, math.ceil(math.sqrt(rooms)))
    grid_rows = math.ceil(rooms / cols)
    w = cols * room_w + cols + 1
    h = grid_rows * room_h + grid_rows + 1
    if w > 32 or h > 32:
        return None

    def place(i):
        r = i // cols
        c = i % cols if r % 2 == 0 else cols - 1 - (i % cols)
        return r, c

    def room_cells(i):
        r, c = place(i)
        x0 = 1 + c * (room_w + 1)
        y0 = 1 + r * (room_h + 1)
        return [(y, x) for y in range(y0, y0 + room_h)
                for x in range(x0, x0 + room_w)]

    def door_between(i, j):
        ri, ci = place(i)
        rj, cj = place(j)
        if ri == rj:
            x = 1 + min(ci, cj) * (room_w + 1) + room_w
            y = 1 + ri * (room_h + 1) + room_h // 2
        else:
            x = 1 + ci * (room_w + 1) + room_w // 2
            y = 1 + min(ri, rj) * (room_h + 1) + room_h
        return y, x

    stack_obj, stack_flr = [], []
    plan = levels or list(range(1, num_mechanics + 1))
    for used in plan:
        obj = np.full((h, w), wall_k, np.int8)
        flr = np.full((h, w), EMPTY, np.int8)
        for i in range(used + 1):
            for y, x in room_cells(i):
                obj[y, x] = EMPTY
        for i in range(used):
            dy, dx = door_between(i, i + 1)
            obj[dy, dx] = door_kind[i]
        cells = room_cells(0)
        obj[cells[0]] = player_k
        for i in range(used):
            spots = [c for c in room_cells(i) if obj[c] == EMPTY]
            rng.shuffle(spots)
            count = 1 if chosen[i] != "collect" else int(rng.integers(2, 4))
            for j in range(min(count, len(spots))):
                obj[spots[j]] = trigger_kind[i]
        for n, hk in enumerate(hazard_kinds):
            room = min(1 + n, used)
            spots = [c for c in room_cells(room) if obj[c] == EMPTY]
            if len(spots) > 2:
                obj[spots[len(spots) // 2]] = hk
        for _ in range(scenery):
            room = int(rng.integers(0, used + 1))
            spots = [c for c in room_cells(room) if obj[c] == EMPTY]
            if len(spots) > 3 and scenery_kinds:
                pick = spots[int(rng.integers(len(spots)))]
                obj[pick] = scenery_kinds[int(rng.integers(len(scenery_kinds)))]
        last = [c for c in room_cells(used) if obj[c] == EMPTY]
        flr[last[-1]] = goal_k
        stack_obj.append(obj)
        stack_flr.append(flr)

    pitch = int(max(1, min(64 // max(w, h), 8)))
    spec = Spec(kinds=kinds, layouts=np.stack(stack_obj),
                floors=np.stack(stack_flr), player_kind=player_k,
                win_mode=WIN_REACH, win_a=goal_k, pitch=pitch,
                origin_x=(64 - w * pitch) // 2, origin_y=(64 - h * pitch) // 2,
                rules=rules)
    return Proposal(spec=spec, seed=0,
                    mechanics={"kinds": chosen, "hazards": hazard_kinds})


def sample_verified(rng, aux_size, library=None, attempts=12, **kwargs):
    import ctypes
    import dataclasses

    from .validate import necessity

    for _ in range(attempts):
        proposal = sample_composed(rng, **kwargs)
        if proposal is None:
            continue
        spec = proposal.spec
        final = dataclasses.replace(spec, layouts=spec.layouts[-1:],
                                    floors=spec.floors[-1:])
        report, base = necessity(final, aux_size, max_nodes=120_000,
                                 library=library)
        if not base.solvable:
            continue
        gates = [r for r in report if r["kind"] == "rule"]
        actors = [r for r in report if r["kind"] == "actor"]
        if any(r["role"] != "enabling" for r in gates):
            continue
        if actors and all(r["role"] == "decorative" for r in actors):
            continue
        proposal.mechanics["roles"] = report
        proposal.mechanics["shortest"] = base.shortest
        return proposal
    return None
