<div align="center">

# ⚡ arc.c

**The ARC-AGI-3 interactive environments, in C.**

A dependency-free C implementation of the [ARC Prize](https://arcprize.org/arc-agi/3) reasoning environments: 25 games, two rendering backends, a batched multi-threaded vector environment, and an optional XLA custom call so JAX can drive it without leaving the compiled program.

Environments are held to **frame-exact** parity with the official
[ARC-AGI Toolkit](https://github.com/arcprize/ARC-AGI), enforced by differential
tests that replay both implementations side by side against the *reference*,
not against a sibling port.

[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![No dependencies](https://img.shields.io/badge/dependencies-none-brightgreen.svg)](#quickstart)
[![Parity](https://img.shields.io/badge/parity-23%2F25%20frame--exact-yellow.svg)](#status)

</div>

---

## Status

All 25 public ARC-AGI-3 environments are implemented. 23 of 25 are verified
frame-exact against the upstream Python reference across every level, 3 seeds
and 120 actions each. The two scene-backend games (`bp35`, `lf52`) are
currently verified against the JAX implementation only.

`lf52` cannot be fully verified against the reference: it calls
`np.random.shuffle` on numpy's unseeded global `RandomState`, so that region is
not reproducible between processes in any language.

## Quickstart

**Requirements:** a C11 compiler.

```bash
git clone git@github.com:noahfarr/arc.c.git
cd arc.c
make -j
```

That builds `libarc`. `make ffi` adds the XLA custom call (needs jax installed
for its headers), `make pgo` does a profile-guided build.

The differential harnesses live on the
[`jax-environments`](https://github.com/noahfarr/arc.c/tree/jax-environments)
branch, alongside the JAX implementation they compare against.
