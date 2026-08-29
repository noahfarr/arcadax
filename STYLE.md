# Style

The C follows the [Linux kernel coding
style](https://www.kernel.org/doc/html/latest/process/coding-style.html). That
document is the authority; this file only records how it is enforced here and
where this codebase necessarily differs.

## Enforced mechanically

`.clang-format` encodes the mechanical rules: tabs indented eight columns, an
80 column limit, the opening brace of a function on its own line and everything
else's on the same line, `} else {`, a space after `if`/`for`/`while`/`switch`
but not after `sizeof`, and `*` bound to the name rather than the type.

```bash
make format        # reformat
make check-format  # fail if anything is out of style
```

## Not enforceable by a formatter

- **No typedefs for structs.** The kernel is explicit that `typedef struct
  {...} foo_t;` hides what a type is. Structures are declared as `struct
  arc_sprites` and spelled that way at every use.
- **Names are lowercase with underscores.** No CamelCase and no Hungarian
  notation. Public symbols carry an `arc_` prefix because a shared library has
  one flat namespace; per-game symbols carry their game id, `ka59_step_once`.
  Macros and enum constants are capitals.
- **Local names are short.** `i`, `n`, `tmp` in a tight loop; descriptive names
  for anything with a wider scope.
- **Functions do one thing** and stay short enough to read at once.
- **`goto` is the right tool** for unwinding on error, with labels that say
  what they do.

## Deliberate differences

This is userspace, so the fixed-width types are the C99 ones from `<stdint.h>`
(`int32_t`, `uint8_t`) rather than the kernel's `s32` and `u8`, and none of the
kernel's headers, allocators or locking primitives apply.

The kernel prefers no braces around a single statement. That one is left to
judgement rather than enforced, because clang-format's support for rewriting it
is still experimental and a wrong rewrite is a silent change in control flow.
