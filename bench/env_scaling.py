import ctypes
import pathlib
import sys
import time

import numpy as np

sys.path.insert(0, str(pathlib.Path("csrc/games").resolve()))
sys.path.insert(0, str(pathlib.Path("csrc").resolve()))

lib = ctypes.CDLL(str(pathlib.Path("csrc/libm0r0_driver.dylib").resolve()))


class LevelData(ctypes.Structure):
    _fields_ = [(n, ctypes.c_void_p) for n in
                ("pixels", "h", "w", "x", "y", "layer", "order",
                 "interaction", "blocking", "alive", "tags", "grid_size")] + [
        ("num_levels", ctypes.c_int32), ("num_slots", ctypes.c_int32),
        ("num_tags", ctypes.c_int32), ("ph", ctypes.c_int32),
        ("pw", ctypes.c_int32), ("win_score", ctypes.c_int32),
        ("background", ctypes.c_int8), ("letter_box", ctypes.c_int8)]


lib.game_new.restype = ctypes.c_void_p
lib.game_new.argtypes = [ctypes.POINTER(LevelData), ctypes.c_void_p, ctypes.c_void_p,
                         ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32,
                         ctypes.c_int32, ctypes.c_int32]
lib.game_bench_envs.restype = ctypes.c_int64
lib.game_bench_envs.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_int32,
                                ctypes.c_int32, ctypes.c_int32, ctypes.c_uint32]


from harness import differ

library, game = differ.build("m0r0")
lib = library.lib
lib.game_bench_envs.restype = ctypes.c_int64
lib.game_bench_envs.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_int32,
                                ctypes.c_int32, ctypes.c_int32, ctypes.c_uint32]

slots = game.levels.num_slots
ph, pw = game.levels.patch_shape
per_env_bytes = slots * (6 * 4 + 3) + slots * ph * pw
print(f"m0r0: {slots} slots, ~{per_env_bytes/1024:.1f} KB mutable state per env")

THREADS = int(sys.argv[1]) if len(sys.argv) > 1 else 11
print(f"\n{'envs':>6s} {'state MB':>9s} {'steps/s':>13s}")
for num_envs in (11, 44, 176, 704, 2816, 4092):
    pool = [ctypes.c_void_p(game.clone()) for _ in range(num_envs)]
    arr = (ctypes.c_void_p * num_envs)(*pool)
    lib.game_bench_envs(arr, num_envs, THREADS, 500, 1)
    iters = 20000
    t0 = time.perf_counter()
    lib.game_bench_envs(arr, num_envs, THREADS, iters, 4242)
    dt = time.perf_counter() - t0
    print(f"{num_envs:6d} {num_envs*per_env_bytes/1e6:8.1f} {THREADS*iters/dt:10,.0f}/s")
