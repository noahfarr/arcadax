import argparse
import sys
import time

from . import differ
from .clib import Library

GAMES = [
    "ar25", "cd82", "cn04", "dc22", "ft09", "g50t", "ka59", "lp85", "ls20", "m0r0",
    "r11l", "re86", "s5i5", "sb26", "sc25", "sk48", "sp80", "su15", "tn36", "tr87",
    "tu93", "vc33", "wa30",
]


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="harness",
        description="Differential test of the C port against the official ARC-AGI-3 Python.",
    )
    parser.add_argument("games", nargs="*")
    parser.add_argument("-n", "--actions", type=int, default=120)
    parser.add_argument("-s", "--seeds", type=int, default=1)
    parser.add_argument("--levels", default="all")
    parser.add_argument("--library", default=None)
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("--no-jax", action="store_true",
                        help="fail if anything tries to import jax or arcadax")
    parser.add_argument("--layout", action="store_true",
                        help="verify the ctypes mirrors against the C headers and exit")
    args = parser.parse_args()

    if args.no_jax:
        from . import isolation

        isolation.forbid("jax", "jaxlib", "arcadax")

    library = Library(path=args.library)

    if args.layout:
        from . import layout

        structs, checked, problems = layout.verify(library)
        for problem in problems:
            print(problem)
        print(f"{structs} structs, {checked} sizes and field offsets checked against the compiler")
        print("PASS" if not problems else f"FAIL ({len(problems)} mismatches)")
        return 1 if problems else 0
    targets = args.games or GAMES
    failures: list[str] = []
    actions = frames = runs = 0
    start = time.perf_counter()

    print(f"library   {library.path}")
    print(f"reference arcengine, official sources in reference/")
    print(f"policy    {args.actions} actions x {args.seeds} seed(s) per level")
    print("config    derived from the official sources" +
          ("  (jax and arcadax imports blocked)" if args.no_jax else ""))
    print()

    for game_id in targets:
        levels = differ.level_count(game_id, library)
        chosen = range(levels) if args.levels == "all" else [int(v) for v in args.levels.split(",")]
        covered, bad, overrides = 0, 0, {}
        for level in chosen:
            for seed in range(args.seeds):
                result = differ.run(game_id, args.actions, seed, library, start_level=level)
                actions += result.actions_checked
                frames += result.frames_checked
                overrides = result.config or overrides
                runs += 1
                if result.ok:
                    covered += 1
                else:
                    bad += 1
                    failures.append(f"{game_id} level {level} seed {seed}")
                    print(f"{game_id:6} level {level} seed {seed}: {result.divergences[0]}")
                    if args.verbose:
                        for d in result.divergences[1:4]:
                            print(f"{'':13}{d}")
        note = f"  port-configured: {overrides}" if overrides else ""
        print(f"{game_id:6} {'ok ' if not bad else 'BAD'} {levels} levels, "
              f"{covered}/{covered + bad} runs clean{note}")

    elapsed = time.perf_counter() - start
    print(f"\n{len(targets)} games, {runs} runs, {actions} actions and {frames} frames "
          f"verified against the reference in {elapsed:.1f}s")
    if failures:
        print(f"FAIL: {len(failures)} run(s) diverged")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
