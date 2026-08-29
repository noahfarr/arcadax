import ctypes
import subprocess
import tempfile
from pathlib import Path

from .clib import Library


def verify(library: Library | None = None, cc: str = "cc") -> tuple[int, int, list[str]]:
    library = library or Library()
    root = library.root
    arc = library.arc_prefix
    headers = sorted(library.games_dir.glob("*.h"))
    lines = ["#include <stdio.h>", f'#include "{arc}game.h"']
    if (root / "scene_game.h").exists():
        lines.append(f'#include "{arc}scene_game.h"')
    lines += [f'#include "{h.name}"' for h in headers]
    lines.append("int main(void){")

    structs = library.headers.structs
    for name in sorted(structs):
        lines.append(f'printf("%s %zu\\n","{name}",sizeof({name}));')
        for field, _ in structs[name]._fields_:
            lines.append(
                f'printf("%s.%s %zu\\n","{name}","{field}",'
                f"(size_t)__builtin_offsetof({name},{field}));"
            )
    lines.append("return 0;}")

    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "layout.c"
        binary = Path(tmp) / "layout"
        source.write_text("\n".join(lines))
        subprocess.run(
            [cc, "-I", str(library.include_root), "-I", str(library.games_dir),
             "-o", str(binary), str(source)],
            check=True,
        )
        output = subprocess.run([str(binary)], capture_output=True, text=True, check=True).stdout

    problems = []
    checked = 0
    for line in output.strip().splitlines():
        key, value = line.split()
        value = int(value)
        if "." in key:
            owner, field = key.split(".")
            actual = getattr(structs[owner], field).offset
        else:
            actual = ctypes.sizeof(structs[key])
        checked += 1
        if actual != value:
            problems.append(f"{key}: C says {value}, the harness mirror says {actual}")
    return len(structs), checked, problems
