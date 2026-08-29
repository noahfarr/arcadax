import numpy as np

from .reference import CLICK_ACTION, Levels


def click_targets(levels: Levels, level: int | None = None) -> np.ndarray:
    points: set[tuple[int, int]] = set()
    indices = range(levels.num_levels) if level is None else [level]
    for li in indices:
        for si in range(levels.num_slots):
            if not levels.alive[li, si]:
                continue
            w, h = int(levels.w[li, si]), int(levels.h[li, si])
            if w <= 0 or h <= 0:
                continue
            x0, y0 = max(0, int(levels.x[li, si])), max(0, int(levels.y[li, si]))
            for x in range(x0, min(64, x0 + w)):
                for y in range(y0, min(64, y0 + h)):
                    points.add((x, y))
    return np.array(sorted(points), np.int32) if points else np.zeros((0, 2), np.int32)


def actions(levels: Levels, count: int, seed: int, click_bias: float = 0.85,
            level: int | None = None) -> list[tuple[int, int, int]]:
    rng = np.random.default_rng(seed)
    kinds = levels.simple_actions + ([CLICK_ACTION] if levels.has_click else [])
    targets = click_targets(levels, level) if levels.has_click else None
    out = []
    for _ in range(count):
        kind = int(rng.choice(kinds))
        if kind != CLICK_ACTION:
            out.append((kind, 0, 0))
        elif targets is not None and len(targets) and rng.random() < click_bias:
            x, y = targets[int(rng.integers(len(targets)))]
            out.append((CLICK_ACTION, int(x), int(y)))
        else:
            out.append((CLICK_ACTION, int(rng.integers(64)), int(rng.integers(64))))
    return out
