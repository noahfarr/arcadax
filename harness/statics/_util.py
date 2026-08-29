import ctypes

import numpy as np

P8 = ctypes.POINTER(ctypes.c_int8)
PU8 = ctypes.POINTER(ctypes.c_uint8)
P32 = ctypes.POINTER(ctypes.c_int32)
PF = ctypes.POINTER(ctypes.c_float)


class Buffer:
    __slots__ = ("array", "ctype")

    def __init__(self, array, ctype) -> None:
        self.array = np.ascontiguousarray(array)
        self.ctype = ctype

    def pointer(self):
        return self.array.ctypes.data_as(ctypes.POINTER(self.ctype))


def p8(a):
    return Buffer(a, ctypes.c_int8)


def pu8(a):
    return Buffer(a, ctypes.c_uint8)


def p32(a):
    return Buffer(a, ctypes.c_int32)


def pf(a):
    return Buffer(a, ctypes.c_float)


def ptr(a, ctype):
    return Buffer(a, ctype)


def pad_mask(mask):
    n = mask.shape[0]
    counts = mask.sum(1).astype(np.int32)
    width = max(int(counts.max()) if n else 0, 1)
    out = np.full((n, width), -1, np.int32)
    for i in range(n):
        idx = np.nonzero(mask[i])[0]
        out[i, : len(idx)] = idx
    return np.ascontiguousarray(out), np.ascontiguousarray(counts), width
