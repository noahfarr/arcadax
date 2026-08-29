import ctypes
import dataclasses
from collections import deque

import numpy as np


@dataclasses.dataclass
class Exploration:
    nodes: int
    edges: int
    solvable: bool
    shortest: int | None
    exhausted: bool
    terminal_depth: int | None = None


def _actions_for(game) -> list[tuple[int, int, int]]:
    spec = getattr(game, "spec", None)
    out = [(a, 0, 0) for a in (1, 2, 3, 4)]
    if spec is not None:
        for cy in range(spec.grid_h):
            for cx in range(spec.grid_w):
                out.append((6, spec.origin_x + cx * spec.pitch + spec.pitch // 2,
                            spec.origin_y + cy * spec.pitch + spec.pitch // 2))
    return out


def explore(game, aux_size: int, max_nodes: int = 50_000,
            actions: list[tuple[int, int, int]] | None = None) -> Exploration:
    sym = game.library.sym
    size = sym.game_state_size(game.handle, aux_size)
    actions = actions or _actions_for(game)

    def snapshot():
        buf = (ctypes.c_uint8 * size)()
        sym.game_save(game.handle, aux_size, ctypes.byref(buf))
        return buf

    def restore(buf):
        sym.game_load(game.handle, aux_size, ctypes.byref(buf))

    game.init()
    start_level = int(sym.harness_level_index(game.handle))
    root = snapshot()
    seen = {sym.game_hash(game.handle, aux_size)}
    frontier = deque([(root, 0)])
    nodes = edges = 0
    shortest = None

    while frontier:
        state, depth = frontier.popleft()
        nodes += 1
        if nodes > max_nodes:
            return Exploration(nodes, edges, shortest is not None, shortest,
                               False)
        for action in actions:
            restore(state)
            game.act(*action)
            edges += 1
            level = int(sym.harness_level_index(game.handle))
            status = game.state
            if status == "WIN" or level > start_level:
                if shortest is None:
                    shortest = depth + 1
                continue
            if status == "GAME_OVER":
                continue
            h = sym.game_hash(game.handle, aux_size)
            if h in seen:
                continue
            seen.add(h)
            frontier.append((snapshot(), depth + 1))
    return Exploration(nodes, edges, shortest is not None, shortest, True)


def random_solve_rate(game, trials: int = 10_000, horizon: int = 200,
                      seed: int = 0, actions=None) -> float:
    rng = np.random.default_rng(seed)
    actions = actions or _actions_for(game)
    sym = game.library.sym
    wins = 0
    for t in range(trials):
        game.init()
        start = int(sym.harness_level_index(game.handle))
        for _ in range(horizon):
            a = actions[int(rng.integers(len(actions)))]
            game.act(*a)
            if game.state == "WIN" or int(
                    sym.harness_level_index(game.handle)) > start:
                wins += 1
                break
            if game.state == "GAME_OVER":
                break
    return wins / trials
