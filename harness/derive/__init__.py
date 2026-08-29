import importlib

_CACHE: dict[str, object] = {}


def derive(game_id: str, data):
    if game_id not in _CACHE:
        _CACHE[game_id] = importlib.import_module(f"harness.derive.{game_id}")
    return _CACHE[game_id].derive(data)
