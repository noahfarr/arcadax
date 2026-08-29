import ctypes
import re
from pathlib import Path

SCALARS: dict[str, type] = {
    "int8_t": ctypes.c_int8,
    "uint8_t": ctypes.c_uint8,
    "int16_t": ctypes.c_int16,
    "uint16_t": ctypes.c_uint16,
    "int32_t": ctypes.c_int32,
    "uint32_t": ctypes.c_uint32,
    "int64_t": ctypes.c_int64,
    "uint64_t": ctypes.c_uint64,
    "size_t": ctypes.c_size_t,
    "float": ctypes.c_float,
    "double": ctypes.c_double,
    "char": ctypes.c_char,
    "int": ctypes.c_int,
}

_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
_DEFINE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s+([^\n]+)$", re.M)
_ENUM = re.compile(r"\benum\s*\{(.*?)\}\s*;", re.S)
_TYPEDEF_STRUCT = re.compile(r"\btypedef\s+struct\s*\{(.*?)\}\s*([A-Za-z_]\w*)\s*;", re.S)
_TAG_STRUCT = re.compile(r"\bstruct\s+([A-Za-z_]\w*)\s*\{(.*?)\n\}\s*;", re.S)
_FUNC_PTR = re.compile(r"^[\w\s\*]+\(\s*\*\s*(\w+)\s*\)\s*\(")
_DECL = re.compile(r"^(?:const\s+)?(?:struct\s+)?(\w+)\s+(.*)$", re.S)
_ITEM = re.compile(r"^(\**)\s*(\w+)\s*((?:\[[^\]]+\]\s*)*)$")
_DIM = re.compile(r"\[([^\]]+)\]")


class CHeaders:
    def __init__(self) -> None:
        self.constants: dict[str, int] = {}
        self.structs: dict[str, type[ctypes.Structure]] = {}
        self.tags: set[str] = set()

    def load(self, *paths: str | Path) -> "CHeaders":
        for path in paths:
            self._ingest(Path(path).read_text(encoding="utf-8"))
        return self

    def _ingest(self, text: str) -> None:
        text = _COMMENT.sub(" ", text)
        self._defines(text)
        self._enums(text)
        self._structs(text)

    def _defines(self, text: str) -> None:
        for name, body in _DEFINE.findall(text):
            if "(" in name:
                continue
            value = self._value(body.strip())
            if value is not None:
                self.constants[name] = value

    def _enums(self, text: str) -> None:
        for body in _ENUM.findall(text):
            nxt = 0
            for item in body.split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    name, _, expr = item.partition("=")
                    value = self._value(expr.strip())
                    if value is None:
                        continue
                    nxt = value
                else:
                    name = item
                self.constants[name.strip()] = nxt
                nxt += 1

    def _structs(self, text: str) -> None:
        for name, body in _TAG_STRUCT.findall(text):
            if name in self.structs:
                continue
            self.tags.add(name)
            self.structs[name] = type(
                name, (ctypes.Structure,), {"_fields_": self._fields(name, body)}
            )
        for body, name in _TYPEDEF_STRUCT.findall(text):
            if name in self.structs:
                continue
            self.structs[name] = type(
                name, (ctypes.Structure,), {"_fields_": self._fields(name, body)}
            )

    def _fields(self, owner: str, body: str) -> list[tuple[str, type]]:
        out: list[tuple[str, type]] = []
        for raw in body.split(";"):
            decl = " ".join(raw.split())
            if not decl:
                continue
            fp = _FUNC_PTR.match(decl)
            if fp:
                out.append((fp.group(1), ctypes.c_void_p))
                continue
            head = _DECL.match(decl)
            if not head:
                raise ValueError(f"{owner}: cannot parse declaration {decl!r}")
            base, rest = head.groups()
            for piece in self._split(rest):
                item = _ITEM.match(piece.strip())
                if not item:
                    raise ValueError(f"{owner}: cannot parse declarator {piece!r} in {decl!r}")
                stars, field, dims = item.groups()
                kind = self._resolve(base, len(stars))
                if kind is None:
                    raise ValueError(f"{owner}: unknown type {base!r} for field {field!r}")
                for dim in reversed(_DIM.findall(dims)):
                    size = self._value(dim)
                    if size is None:
                        raise ValueError(f"{owner}.{field}: cannot evaluate array size {dim!r}")
                    kind = kind * size
                out.append((field, kind))
        if not out:
            raise ValueError(f"{owner}: no fields parsed")
        return out

    @staticmethod
    def _split(rest: str) -> list[str]:
        parts, depth, current = [], 0, ""
        for ch in rest:
            if ch == "[":
                depth += 1
            elif ch == "]":
                depth -= 1
            if ch == "," and depth == 0:
                parts.append(current)
                current = ""
            else:
                current += ch
        parts.append(current)
        return [p for p in parts if p.strip()]

    def _resolve(self, base: str, depth: int) -> type | None:
        if base == "void":
            return ctypes.c_void_p if depth else None
        kind = SCALARS.get(base) or self.structs.get(base)
        if kind is None:
            return ctypes.c_void_p if depth else None
        for _ in range(depth):
            kind = ctypes.POINTER(kind)
        return kind

    def _value(self, expr: str) -> int | None:
        expr = expr.strip().rstrip("uUlL")
        if not expr or not re.fullmatch(r"[\w\s+\-*/()]+", expr):
            return None
        try:
            return int(eval(expr, {"__builtins__": {}}, dict(self.constants)))
        except Exception:
            return None

    def struct(self, name: str) -> type[ctypes.Structure]:
        snake = re.sub(r"_+", "_", re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower())
        for candidate in (name, f"Arc{name}", snake, f"arc_{snake}"):
            if candidate in self.structs:
                return self.structs[candidate]
        raise KeyError(f"no struct matching {name!r} in the parsed headers")
