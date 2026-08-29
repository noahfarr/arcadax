import ctypes

import numpy as np

ALLOC: dict[str, tuple[str, ...]] = {
    "cn04": ("num_slots", "ph", "pw"),
    "ka59": ("num_slots",),
    "ls20": ("num_slots",),
    "s5i5": ("num_slots", "ph", "pw"),
    "sb26": ("num_slots",),
    "sk48": ("num_slots",),
    "su15": ("num_slots",),
    "tr87": ("num_slots",),
    "wa30": ("num_slots",),
}

BUFFERS: dict[str, dict[str, tuple]] = {
    "dc22": {
        "undo_pixels": (lambda d: d["num_slots"] * d["ph"] * d["pw"], np.int8, -1),
        "undo_x": (lambda d: d["num_slots"], np.int32, 0),
        "undo_y": (lambda d: d["num_slots"], np.int32, 0),
        "undo_interaction": (lambda d: d["num_slots"], np.int8, 3),
        "undo_alive": (lambda d: d["num_slots"], np.uint8, 0),
    },
    "sp80": {
        "filled": (lambda d: d["num_slots"], np.uint8, 0),
        "touched": (lambda d: d["num_slots"], np.uint8, 0),
    },
}


def allocate(game_id: str, aux: ctypes.Structure, dims: dict[str, int]) -> list[np.ndarray]:
    keep = []
    for name, (size, dtype, fill) in BUFFERS.get(game_id, {}).items():
        array = np.full(size(dims) if callable(size) else size, fill, dtype)
        pointer = ctypes.POINTER(np.ctypeslib.as_ctypes_type(np.dtype(dtype)))
        setattr(aux, name, array.ctypes.data_as(pointer))
        keep.append(array)
    return keep
