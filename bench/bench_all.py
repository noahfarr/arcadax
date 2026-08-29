import ctypes
import pathlib
import sys
import time

import numpy as np

sys.path.insert(0, str(pathlib.Path("csrc").resolve()))
sys.path.insert(0, str(pathlib.Path("csrc/games").resolve()))

from harness import differ

GAMES = ["ar25", "cd82", "cn04", "dc22", "ft09", "g50t", "ka59", "lp85", "ls20",
         "m0r0", "r11l", "re86", "s5i5", "sb26", "sc25", "sk48", "sp80", "su15",
         "tn36", "tr87", "tu93", "vc33", "wa30"]

ITERS = int(sys.argv[1]) if len(sys.argv) > 1 else 20000

from harness.clib import Library

library = Library()
lib = library.lib
lib.game_bench_split.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32,
                                 ctypes.POINTER(ctypes.c_int64),
                                 ctypes.POINTER(ctypes.c_double),
                                 ctypes.POINTER(ctypes.c_double)]
lib.game_bench_steps.restype = ctypes.c_int64
lib.game_bench_steps.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32]

rows = []
alive = []
for gid in GAMES:
    try:
        _, game = differ.build(gid, library)
        alive.append(game)
        slots = game.levels.num_slots
        ph, pw = game.levels.patch_shape
        handle = game.handle

        lib.game_bench_steps(handle, 500, 1)
        frames = ctypes.c_int64()
        logic = ctypes.c_double()
        render = ctypes.c_double()
        t0 = time.perf_counter()
        lib.game_bench_split(handle, ITERS, 4242, ctypes.byref(frames),
                             ctypes.byref(logic), ctypes.byref(render))
        wall = time.perf_counter() - t0
        sps = ITERS / wall
        total_ns = logic.value + render.value
        share = 100.0 * render.value / total_ns if total_ns > 0 else 0.0
        rows.append((gid, slots, ph * pw, sps, logic.value / ITERS,
                     render.value / ITERS, share, frames.value / ITERS))
    except Exception as exc:
        rows.append((gid, -1, -1, 0.0, 0.0, 0.0, 0.0, 0.0))
        print(f"{gid}: FAILED {str(exc)[:60]}", file=sys.stderr)

rows.sort(key=lambda r: r[3])
print(f"{'game':6s} {'slots':>5s} {'patch':>7s} {'steps/s':>11s} "
      f"{'logic ns':>9s} {'render ns':>10s} {'render%':>8s} {'frames':>7s}")
print("-" * 74)
for gid, slots, patch, sps, lns, rns, share, fr in rows:
    if slots < 0:
        print(f"{gid:6s} {'FAILED':>5s}")
        continue
    print(f"{gid:6s} {slots:5d} {patch:7d} {sps:11,.0f} {lns:9.0f} {rns:10.0f} "
          f"{share:7.1f}% {fr:7.2f}")

ok = [r for r in rows if r[1] >= 0]
if ok:
    total = sum(1.0 / r[3] for r in ok if r[3] > 0)
    print("-" * 74)
    print(f"slowest: {ok[0][0]} at {ok[0][3]:,.0f}/s   fastest: {ok[-1][0]} at {ok[-1][3]:,.0f}/s")
    print(f"mean render share: {sum(r[6] for r in ok) / len(ok):.1f}%")
