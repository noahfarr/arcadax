import ctypes
import importlib
import inspect

import numpy as np

from ._util import Buffer

_MODULES: dict[str, object] = {}


def module(game_id: str):
    if game_id not in _MODULES:
        _MODULES[game_id] = importlib.import_module(f"harness.statics.{game_id}")
    return _MODULES[game_id]



def pack(struct_type: type[ctypes.Structure], fields: dict) -> tuple[ctypes.Structure, list]:
    declared = dict(struct_type._fields_)
    if set(declared) != set(fields):
        raise ValueError(
            f"{struct_type.__name__} does not match the C header: "
            f"missing={sorted(set(declared) - set(fields))} "
            f"unexpected={sorted(set(fields) - set(declared))}"
        )
    keep, values = [], {}
    for name, value in fields.items():
        wanted = declared[name]
        if isinstance(value, Buffer):
            element = getattr(wanted, "_type_", None)
            if element is None:
                raise TypeError(f"{struct_type.__name__}.{name} is not a pointer in the header")
            if np.dtype(element) != value.array.dtype:
                raise TypeError(
                    f"{struct_type.__name__}.{name}: the header declares {element.__name__} "
                    f"but the table is {value.array.dtype}"
                )
            keep.append(value.array)
            values[name] = value.array.ctypes.data_as(wanted)
        else:
            values[name] = value
    return struct_type(**values), keep


def build(game_id: str, struct_type: type[ctypes.Structure], env, data):
    builder = module(game_id).build
    arity = len(inspect.signature(builder).parameters)
    result = builder(env, data) if arity >= 2 else builder(env)
    fields, keep = result[0], []
    for item in result[1:]:
        keep.extend(item) if isinstance(item, list) else keep.append(item)
    static, owned = pack(struct_type, fields)
    return static, keep + owned


def aux_buffers(game_id: str, aux: ctypes.Structure, dims: dict[str, int]) -> list[np.ndarray]:
    spec = getattr(module(game_id), "AUX_BUFFERS", None)
    if not spec:
        return []
    keep = []
    for name, (size, dtype, fill) in spec.items():
        count = size(dims) if callable(size) else size
        array = np.full(count, fill, dtype)
        pointer = ctypes.POINTER(np.ctypeslib.as_ctypes_type(np.dtype(dtype)))
        setattr(aux, name, array.ctypes.data_as(pointer))
        keep.append(array)
    return keep


def aux_alloc_args(game_id: str) -> tuple[str, ...] | None:
    return getattr(module(game_id), "AUX_ALLOC", None)

