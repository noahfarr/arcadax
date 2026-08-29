from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
from ._const import Action


class SpriteTable:
    FIELDS = ("pixels", "h", "w", "x", "y", "layer", "order",
              "interaction", "blocking", "tags", "alive")

    def __init__(self, data) -> None:
        for name in self.FIELDS:
            setattr(self, name, getattr(data, name))

    @property
    def n(self) -> int:
        return self.pixels.shape[0]


class Setup:
    def __init__(self, data=None) -> None:
        self.data = data
        if data is None or not hasattr(data, "available_actions"):
            return
        self._levels = SpriteTable(data)
        self._grid_size = data.grid_size
        self.num_levels = data.num_levels
        self.num_slots = data.num_slots
        self.win_score = data.win_score
        simple = sorted(a for a in data.available_actions if a != Action.ACTION6)
        self._simple_actions = simple
        self._has_click = Action.ACTION6 in data.available_actions


def reference_dir():
    return ROOT.parent / "reference"


def source_for(game_id: str):
    found = sorted(reference_dir().glob(f"{game_id}-*.py"))
    if len(found) != 1:
        raise FileNotFoundError(f"expected one {game_id}-*.py, found {len(found)}")
    return found[0]


Array = object


def _game_data():
    from ..reference import Levels

    return Levels


GameData = _game_data()


def _enums():
    from ._const import Blocking, Interaction

    values = {e.name: int(e) for e in Interaction}
    values.update({e.name: int(e) for e in Blocking})
    return values


def load_reference_game(source, seed: int = 0):
    from ..reference import load_game

    return load_game(source, seed=seed)


def extract(source, extra_slots: int = 0, seed: int = 0):
    from ..reference import extract_levels, load_game

    return extract_levels(load_game(source, seed=seed), _enums(), extra_slots)


def _arc_base_game():
    from arcengine import ARCBaseGame

    return ARCBaseGame


ARCBaseGame = _arc_base_game()


def cache_dir():
    import os

    override = os.environ.get("ARC_CACHE")
    return Path(override).expanduser() if override else ROOT.parent / "cache"
