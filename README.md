# arc.c

A C implementation of the ARC-AGI-3 interactive environments.

All 25 games, a shared sprite engine and a scene engine, a batched
multi-threaded vector environment, and an optional XLA custom call for use from
JAX.

## Build

    cd csrc && make          # libarc, no dependencies beyond a C compiler
    make ffi                 # adds the XLA custom call, needs jax installed
    make pgo                 # profile-guided build
    make -j32                # parallel

## Correctness

The 23 sprite-backend games are verified frame-exact against the upstream
Python reference implementation (`arcengine`), across every level, three seeds
and 120 actions per level, comparing every intermediate frame of every action
along with score, status and level. The two scene-backend games (`bp35`,
`lf52`) are currently verified against the JAX implementation only.

`lf52` cannot be fully verified against the reference: the reference calls
`np.random.shuffle` on numpy's unseeded global RandomState, so its shuffle
region is not reproducible between processes.

The differential harnesses live on the `jax-environments` branch, alongside the
JAX implementation they compare against.

## Design

Sprite pixel data is static and shared across environments through an atlas;
only genuinely mutable fields are stored per environment. The largest game
holds 4096 environments in 72MB.

Rendering bounds each sprite's blit to the bounding box of its non-transparent
pixels, widened whenever a game takes a raw mutable pointer to that sprite's
patch.

## Throughput

Measured on an 11-core laptop, single game, 4092 environments: 630k environment
steps per second. Per-game single-core throughput ranges from 14k to 477k steps
per second.
