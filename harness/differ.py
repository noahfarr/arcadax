import dataclasses
from pathlib import Path

import numpy as np

from . import config as configuration
from . import policy, statics
from .clib import Game, Library
from .reference import MAX_FRAMES, Runner, extract_levels, load_game

ROOT = Path(__file__).resolve().parent.parent


def source_for(game_id: str, directory: Path | None = None) -> Path:
    directory = directory or ROOT / "reference"
    found = sorted(directory.glob(f"{game_id}-*.py"))
    if len(found) != 1:
        raise FileNotFoundError(f"expected one {game_id}-*.py in {directory}, found {len(found)}")
    return found[0]


STRICT_TABLES = ("h", "w", "x", "y", "layer", "order", "alive")
CONFIGURABLE_TABLES = ("pixels", "interaction", "blocking")


def _live_mask(shape, official_alive: np.ndarray, num_levels: int, num_slots: int):
    live = np.zeros((num_levels, num_slots), bool)
    live[:, : official_alive.shape[1]] = official_alive.astype(bool)
    return np.broadcast_to(live.reshape(live.shape + (1,) * (len(shape) - 2)), shape), live


def reconcile(game_id: str, levels, official_alive: np.ndarray, data, declared: dict):
    if levels.num_slots != int(data.num_slots):
        raise AssertionError(
            f"{game_id}: extracted {levels.num_slots} slots but the static configuration "
            f"was built for {data.num_slots}"
        )
    if not np.array_equal(np.asarray(levels.grid_size), np.asarray(data.grid_size)):
        raise AssertionError(f"{game_id}: grid_size disagrees with the static configuration")

    observed: dict[str, object] = {}
    replaced = {}

    for name in STRICT_TABLES + CONFIGURABLE_TABLES:
        mine = np.asarray(getattr(levels, name))
        theirs = np.asarray(getattr(data, name)).astype(mine.dtype)
        if mine.shape != theirs.shape:
            if name != "pixels" or mine.shape[:2] != theirs.shape[:2]:
                raise AssertionError(f"{game_id}: {name} shape {mine.shape} vs {theirs.shape}")
            window = theirs[:, :, : mine.shape[2], : mine.shape[3]]
            mask, _ = _live_mask(mine.shape, official_alive, levels.num_levels, levels.num_slots)
            if not np.array_equal(mine[mask], window[mask]):
                raise AssertionError(
                    f"{game_id}: the official {name} are not preserved inside the enlarged patch"
                )
            observed["patch_shape"] = theirs.shape[2:]
            replaced[name] = np.ascontiguousarray(theirs)
            continue

        mask, live = _live_mask(mine.shape, official_alive, levels.num_levels, levels.num_slots)
        count = int((mask & (mine != theirs)).sum())
        if name in STRICT_TABLES and count:
            raise AssertionError(
                f"{game_id}: {name} of sprites in the official source disagrees with the "
                f"static configuration in {count} places; this table is never port-configured"
            )
        if count:
            observed[name] = count
        np.copyto(mine, theirs)

    synthetic = len(getattr(data, "tag_names", levels.tag_names)) - len(levels.tag_names)
    if synthetic:
        observed["synthetic_tags"] = synthetic

    def normalise(d):
        return {k: list(v) if isinstance(v, (tuple, list)) else v for k, v in d.items()}

    if normalise(observed) != normalise(declared):
        raise AssertionError(
            f"{game_id}: level-data overrides changed. declared={declared or '{}'} "
            f"observed={observed or '{}'}. Update LEVEL_OVERRIDES in harness/statics/{game_id}.py "
            f"only after confirming the new override is intended."
        )

    replaced["tags"] = np.ascontiguousarray(np.asarray(data.tags), np.uint8)
    return dataclasses.replace(
        levels, tag_names=list(getattr(data, "tag_names", levels.tag_names)), **replaced
    ), observed


def level_count(game_id: str, library) -> int:
    from .reference import extract_levels, load_game

    game = load_game(source_for(game_id), seed=0)
    return extract_levels(game, library.headers.constants).num_levels


@dataclasses.dataclass
class Divergence:
    step: int
    action: tuple[int, int, int]
    kind: str
    detail: str

    def __str__(self) -> str:
        aid, x, y = self.action
        where = f"action {self.step} (id={aid}" + (f" x={x} y={y}" if aid == 6 else "") + ")"
        return f"{where}: {self.kind} {self.detail}"


@dataclasses.dataclass
class Result:
    game_id: str
    seed: int
    divergences: list[Divergence]
    actions_checked: int
    frames_checked: int
    levels_seen: set[int]
    final_state: str
    score: int
    config: object = None
    source: str = "frozen"

    @property
    def ok(self) -> bool:
        return not self.divergences


def _compare_frames(want: list[np.ndarray], got: list[np.ndarray]) -> tuple[str, str] | None:
    for i, (a, b) in enumerate(zip(want, got)):
        if not np.array_equal(a, b):
            r, c = (int(v) for v in np.argwhere(a != b)[0])
            return "frame", f"#{i} differs at ({r},{c}): reference={int(a[r, c])} c={int(b[r, c])}"
    if len(want) != len(got):
        return "frame count", f"reference={len(want)} c={len(got)}"
    return None


def run(game_id: str, count: int = 120, seed: int = 0, library: Library | None = None,
        start_level: int | None = None, stop_early: bool = True,
        prefer: str = "frozen") -> Result:
    library = library or Library()
    source = source_for(game_id)

    reference = Runner(load_game(source, seed=seed))
    fresh = load_game(source, seed=seed)
    enums = library.headers.constants
    struct_type = library.headers.struct(f"{game_id.capitalize()}Static")
    cfg = configuration.load(game_id, struct_type)

    base = extract_levels(fresh, enums)
    extra = max(0, int(cfg.data.num_slots) - base.num_slots)
    levels = extract_levels(fresh, enums, extra)
    levels, observed = reconcile(game_id, levels, base.alive, cfg.data, cfg.overrides)

    game = Game(library, game_id, levels, cfg, MAX_FRAMES)

    divergences: list[Divergence] = []
    seen: set[int] = set()
    checked = frames = 0

    plan = policy.actions(levels, count, seed, level=start_level)

    def step(index: int, action: tuple[int, int, int]) -> bool:
        nonlocal checked, frames
        aid, x, y = action
        want = reference.act(aid, x, y)
        got = game.act(aid, x, y)
        bad = _compare_frames(want, got)
        if bad:
            divergences.append(Divergence(index, action, bad[0], bad[1]))
            return False
        for label, a, b in (
            ("score", reference.score, game.score),
            ("state", reference.state, game.state),
            ("level", reference.level_index, game.level_index),
        ):
            if a != b:
                divergences.append(Divergence(index, action, label, f"reference={a} c={b}"))
                return False
        checked += 1
        frames += len(got)
        seen.add(reference.level_index)
        return True

    game.init()
    ok = step(0, (0, 0, 0))

    if ok and start_level is not None and start_level != reference.level_index:
        reference.set_level(start_level)
        game.set_level(start_level)

    for index, action in enumerate(plan, start=1):
        if not ok and stop_early:
            break
        ok = step(index, action)

    result = Result(game_id, seed, divergences, checked, frames, seen,
                    reference.state, reference.score, observed, cfg.source)
    game.close()
    return result


def build(game_id: str, library: Library | None = None, seed: int = 0):
    library = library or Library()
    fresh = load_game(source_for(game_id), seed=seed)
    enums = library.headers.constants
    struct_type = library.headers.struct(f"{game_id.capitalize()}Static")
    cfg = configuration.load(game_id, struct_type)
    base = extract_levels(fresh, enums)
    levels = extract_levels(fresh, enums, max(0, int(cfg.data.num_slots) - base.num_slots))
    levels, _ = reconcile(game_id, levels, base.alive, cfg.data, cfg.overrides)
    return library, Game(library, game_id, levels, cfg, MAX_FRAMES)
