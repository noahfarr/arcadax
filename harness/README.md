# harness

Differential test of the C port against the **official ARC-AGI-3 Python**, and
nothing else. It runs without jax.

```bash
uv run python -m harness              # every game, every level
uv run python -m harness --no-jax     # the same, with jax imports blocked
uv run python -m harness sc25 -n 400  # one game, more actions
uv run python -m harness --layout     # mirrors vs the compiler
```

## Why only the official Python

A port validated against another port cannot find a fault the two share. That
is not hypothetical here: `sp80` passed its own suite for 1,369 frames because
the suite compared C against the JAX port, which had the same bug, and `sc25`
was later reported as a C defect when arbitration against the reference showed
the C port was right and the JAX port was wrong. The reference settles both
questions, so it is the only thing this harness compares against.

## What is checked

Every action compares, in order: each frame pixel by pixel, the number of
frames, the score, the game state and the level index. Runs start at each level
in turn, so coverage does not depend on a random policy happening to finish
level 0.

## Where the data comes from

| Piece | Source |
| --- | --- |
| Behaviour to match | the official game, driven through `arcengine` |
| Sprite geometry, pixels, layer, order, alive | extracted from the official source |
| Static tables and the port's level overrides | `harness/derive/<game>.py`, from the same sources |
| ctypes struct layouts | parsed from the C headers |

Nothing in that table needs jax, and nothing imports `arcadax`. `--no-jax`
installs an import blocker that turns any attempt to load either into an error,
so the claim is enforced rather than asserted; that is how the sweep is run
here.

The static tables each game hands to C are derived in `harness/derive/`, in
plain numpy, from the official level data. They were originally computed by
the JAX port's constructors, but that code never used jax for anything beyond
spelling its array constructors `jnp.asarray` and `jnp.int32` — no tracing, no
grad, no vmap, no scan — so it ports across unchanged. All 23 games produce
tables bit-identical to what the JAX constructors produced.

Level data the port overrides must be **declared** as `LEVEL_OVERRIDES` in the
game's statics module, and the observed overrides must match exactly. Twenty of
the twenty-three games declare none: their level data is byte-identical to the
official extraction.

## Not covered

`bp35` and `lf52` run on the scene backend, which takes an image atlas rather
than sprite tables and needs a separate construction path. They are still
tested only by `csrc/games/{bp35,lf52}_difftest.py`, which compare against the
JAX port. That is the weaker bar this harness exists to replace, and those two
games keep it until the scene path is wired in here.

## Known failure

`g50t` level 5 emits seven frames where the reference emits eight; the missing
one is a duplicate of the last. It reproduces on three of four seeds and is not
yet fixed.
