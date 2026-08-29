import ctypes
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent / "games"))

from harness import differ

ROOT = pathlib.Path(__file__).resolve().parent
SO = "dylib" if sys.platform == "darwin" else "so"


WARMUP_ITERS = 2000
PROFILE_ITERS = 200000


def run(game_id):
    lib, game = differ.build(game_id)
    lib.game_bench_steps.restype = ctypes.c_int64
    lib.game_bench_steps.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32]
    lib.game_bench_steps(game, WARMUP_ITERS, 1)
    lib.game_bench_steps(game, PROFILE_ITERS, 12345)
    if aux_free is not None:
        aux_free()


def main():
    games = sys.argv[1:] or ["tu93", "ls20", "m0r0"]
    for game_id in games:
        run(game_id)
        print(f"pgo_workload: {game_id} done", file=sys.stderr)


if __name__ == "__main__":
    main()
