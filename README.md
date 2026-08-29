<div align="center">

# ⚡ arc.c

**The ARC-AGI-3 interactive environments, in C.**

A dependency-free C implementation of the [ARC Prize](https://arcprize.org/arc-agi/3) reasoning environments: 25 games, two rendering backends, a batched multi-threaded vector environment, and an optional XLA custom call so JAX can drive it without leaving the compiled program.

Environments are held to **frame-exact** parity with the official
[ARC-AGI Toolkit](https://github.com/arcprize/ARC-AGI) — enforced by differential
tests that replay both implementations side by side against the *reference*, not
against a sibling port. See [Status](#status) for where each game stands.

[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![No dependencies](https://img.shields.io/badge/dependencies-none-brightgreen.svg)](#quickstart)
[![Parity](https://img.shields.io/badge/parity-23%2F25%20frame--exact-yellow.svg)](#status)

</div>

---

## Why arc.c

- **Frame-exact parity against the reference is the contract.** Not against another port. Every intermediate frame of every action is compared, along with score, status and level, so agents and evaluations transfer without caveats. [Status](#status) reports what passes and what does not.
- **Static data stays static.** Sprite pixels live once in an atlas shared across environments; only genuinely mutable fields are duplicated per environment. The largest game holds 4096 environments in **72 MB** — the equivalent JAX layout needs over 20 GB and does not fit on a 24 GB accelerator.
- **Rendering is bounded, not brute-forced.** Each blit is clipped to the bounding box of a sprite's non-transparent pixels, widened whenever a game takes a raw mutable pointer into that patch. On the heaviest game that is a **12.3×** speedup; sprites there are 64×64 patches that are 1.6% opaque.
- **Real branching.** The same game logic under `vmap` executes every branch of every `cond` and `switch`. In C only the taken branch runs, and a loop can stop when it is done.
- **Batteries for RL.** A pthread-parallel vector environment steps thousands of games per call, and an XLA typed FFI target lets JAX call it inside `jit` with no Python round-trip.
- **No hand-transcribed game data.** Level data is extracted from `arcengine` and handed in; only game *logic* is ported.

## Status

All **25** public ARC-AGI-3 environments are implemented. **23 of 25** are
verified frame-exact against the upstream Python reference. The two
scene-backend games (`bp35`, `lf52`) are currently verified against the JAX
implementation only — the reference harness does not yet cover that backend.

The acceptance bar for the 23 is the same one the JAX port uses: every level,
3 seeds, 120 actions drawn from the game's full action space, comparing every
intermediate frame of every action plus score, status and level — directly
against `arcengine`.

The history is worth knowing before trusting any number here:

| Bar | Verdict |
| --- | --- |
| Per-game suites, each written beside its own port | 25/25 — but validated against the JAX port, not the reference |
| Against the reference, 120 actions, 3 seeds | 22/23 — `sp80` diverged |
| After fixing `sp80` | 23/23 |

The `sp80` bug is the reason the first row is struck through. It passed its own
suite at 1,369 frames because that suite compared against the JAX port, which
shared no such bug — and because its action distribution never reached the
state that triggered it. Validating a port against another port cannot find a
fault the two do not share; only the reference can.

One documented exception: in `lf52`, one click region drives a transition built
with `np.random.shuffle` on numpy's **unseeded global** `RandomState`. The
reference does not reproduce itself there — identical action sequences in
separate processes yield different frames — so no deterministic implementation
can match it, in any language. That region is excluded by name.

## Quickstart

**Requirements:** a C11 compiler. That is all.

```bash
git clone git@github.com:noahfarr/arc.c.git
cd arc.c/csrc
make -j
```

That builds `libarc`. To add the XLA custom call for use from JAX:

```bash
make ffi          # requires jax installed, for its FFI headers
make pgo          # profile-guided build, ~13-15% faster
```

## Use it as a library

```c
#include "game.h"

Game *game = game_new(&levels, &hooks, aux, statics,
                      simple_actions, num_simple, has_click, max_frames);

int8_t frame[FRAME_SIZE * FRAME_SIZE];
int32_t reward;
uint8_t terminated;

game_init(game);
game_step(game, action, frame, &reward, &terminated);
```

`frame` is 64×64 palette indices, `reward` is levels completed this step. Level
data is supplied by the caller, so the library carries no game assets.

For batched use:

```c
VecEnv *vec = vecenv_new(&levels, &hooks, aux_array, aux_stride, statics,
                         simple_actions, num_simple, has_click, max_frames,
                         num_envs, num_threads);
vecenv_step(vec, actions, obs, reward, terminated, truncated, level, score);
```

One call steps every environment across a thread pool, resetting terminated
environments in place.

## How it works

| Concern | JAX port | arc.c |
| --- | --- | --- |
| Sprite pixels | Per-environment array, duplicated 4096× | Shared immutable atlas, copy-on-write per slot |
| Blit | Every pixel of every patch | Bounded to the patch's opaque bounding box |
| Branching | Every branch of every `cond` executes | Only the taken branch |
| Loop bounds | Static trip count, worst case for the batch | Stops when the work is done |
| Parallelism | `vmap` across one accelerator | pthreads across cores |
| Reset | Computed for all, selected away | A branch |

Per-action multi-frame animations are preserved: `step_once` is one reference
`step()` call, and the loop runs until the game completes the action.
`game_perform_action_frames` captures every intermediate frame; `game_step`
materialises only the last, which is all an observation needs.

## Throughput

Single core, one environment, per game:

| | steps/s | | steps/s |
| --- | --- | --- | --- |
| `lp85` | 477,356 | `ls20` | 165,162 |
| `ft09` | 294,267 | `dc22` | 156,858 |
| `cn04` | 285,631 | `tn36` | 149,470 |
| `tu93` | 271,252 | `sb26` | 134,109 |
| `cd82` | 263,158 | `m0r0` | 99,389 |
| `su15` | 229,254 | `ka59` | 86,243 |
| `g50t` | 212,640 | `sc25` | 13,809 |

Batched, 4092 environments across 11 cores: **630k steps/s**. The JAX
implementation of the same game reaches 593k on an RTX A5000.

`sc25` is the outlier and the current bottleneck: 94% of its time is game
logic, not rendering.

## Testing

The differential harnesses live on the [`jax-environments`](https://github.com/noahfarr/arc.c/tree/jax-environments)
branch, alongside the JAX implementation they compare against.

```bash
cd csrc && make test          # every game against the reference
make ACTIONS=200 test         # deeper sweep
```

- **`ref_difftest.py`** — the acceptance bar. Drives C and `arcengine` from
  identical action sequences and compares every frame of every action.
- **`difftest.py`** — engine primitives against the JAX ops on real evolving
  game states: rendering, `get_sprite_at`.
- **`<game>_difftest.py`** — per-game, comparing every aux and sprite field
  each internal frame.
- **`bench_all.py`** — per-game throughput with the logic/render split.
