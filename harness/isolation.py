import sys


class Forbidden(ImportError):
    pass


class Blocker:
    def __init__(self, names: tuple[str, ...]) -> None:
        self.names = names

    def find_module(self, fullname, path=None):
        return self.find_spec(fullname, path)

    def find_spec(self, fullname, path=None, target=None):
        root = fullname.split(".")[0]
        if root in self.names:
            raise Forbidden(
                f"'{fullname}' is not allowed here: the harness is meant to run without it"
            )
        return None


def forbid(*names: str) -> None:
    for name in names:
        for loaded in [m for m in sys.modules if m == name or m.startswith(name + ".")]:
            del sys.modules[loaded]
    sys.meta_path.insert(0, Blocker(names))
