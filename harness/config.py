import ctypes

import numpy as np

from .statics._util import Buffer

DATA_TABLES = ("pixels", "h", "w", "x", "y", "layer", "order", "interaction",
               "blocking", "alive", "tags", "grid_size")


class PortTables:
    def __init__(self, arrays: dict, meta: dict) -> None:
        self._arrays = arrays
        self.num_slots = meta["num_slots"]
        self.num_levels = meta["num_levels"]
        self.tag_names = meta["tag_names"]

    def __getattr__(self, name: str):
        if name in DATA_TABLES:
            return self._arrays[f"data.{name}"]
        raise AttributeError(name)


class Configuration:
    def __init__(self, game_id: str, data, static_fields: dict, overrides: dict,
                 source: str) -> None:
        self.game_id = game_id
        self.data = data
        self.static_fields = static_fields
        self.overrides = overrides
        self.source = source




def load(game_id: str, struct_type) -> Configuration:
    import importlib
    import inspect

    from . import statics
    from .differ import source_for

    module = importlib.import_module(f"harness.derive.{game_id}")
    args, kwargs = module.make_args(str(source_for(game_id)))
    setup = module.derive(*args, **kwargs)
    data = setup.data

    builder = statics.module(game_id).build
    arity = len(inspect.signature(builder).parameters)
    result = builder(setup, data) if arity >= 2 else builder(setup)
    overrides = getattr(statics.module(game_id), "LEVEL_OVERRIDES", {})
    return Configuration(game_id, data, result[0], overrides, "derived")
