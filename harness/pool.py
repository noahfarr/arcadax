import ctypes

from . import aux as auxdecl
from . import differ
from .clib import Library


class Spec(ctypes.Structure):
    _fields_ = [
        ("levels", ctypes.c_void_p),
        ("hooks", ctypes.c_void_p),
        ("aux_array", ctypes.c_void_p),
        ("aux_stride", ctypes.c_size_t),
        ("statics", ctypes.c_void_p),
        ("simple_actions", ctypes.c_void_p),
        ("num_simple", ctypes.c_int32),
        ("has_click", ctypes.c_int32),
        ("max_frames", ctypes.c_int32),
    ]


def signatures(lib):
    lib.arc_vecenv_new_pool.restype = ctypes.c_void_p
    lib.arc_vecenv_new_pool.argtypes = [
        ctypes.POINTER(Spec), ctypes.c_int32, ctypes.c_int32, ctypes.c_int32,
        ctypes.c_uint64,
    ]
    lib.arc_vecenv_free.argtypes = [ctypes.c_void_p]
    lib.arc_vecenv_reset.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.arc_vecenv_step.argtypes = [ctypes.c_void_p] * 8
    lib.arc_vecenv_num_actions.restype = ctypes.c_int32
    lib.arc_vecenv_num_actions.argtypes = [ctypes.c_void_p]
    lib.arc_vecenv_tasks.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.arc_vecenv_action_counts.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    return lib


class Pool:

    def __init__(self, games, num_envs: int = 64, num_threads: int = 16,
                 seed: int = 0, library: Library | None = None):
        if isinstance(games, str):
            games = [games]
        games = list(games)
        if not games:
            raise ValueError("a pool needs at least one game")
        self.games = games
        self.num_envs = num_envs
        self.library = library or Library()
        self.lib = signatures(self.library.lib)
        self._keep = []

        specs = (Spec * len(games))()
        for index, game in enumerate(games):
            _, proto = differ.build(game, self.library)
            self._keep.append(proto)
            kind = type(proto._aux)
            auxes = (kind * num_envs)()
            self._keep.append(auxes)
            allocate = auxdecl.ALLOC.get(game)
            for slot in range(num_envs):
                self._keep += auxdecl.allocate(game, auxes[slot],
                                               dict(proto._dims))
                if allocate:
                    call = getattr(self.lib, f"{game}_aux_alloc")
                    call.argtypes = ([ctypes.c_void_p] +
                                     [ctypes.c_int32] * len(allocate))
                    call(ctypes.byref(auxes[slot]),
                         *[proto._dims[name] for name in allocate])
            specs[index] = Spec(
                levels=ctypes.addressof(proto._level_data_struct),
                hooks=ctypes.addressof(proto._hooks),
                aux_array=ctypes.addressof(auxes),
                aux_stride=ctypes.sizeof(kind),
                statics=ctypes.addressof(proto._static),
                simple_actions=proto._simple.ctypes.data,
                num_simple=len(proto._simple),
                has_click=int(proto.levels.has_click),
                max_frames=proto.max_frames,
            )
        self._keep.append(specs)
        self.handle = self.lib.arc_vecenv_new_pool(
            specs, len(games), num_envs, num_threads, seed)
        self.num_actions = int(self.lib.arc_vecenv_num_actions(self.handle))

    def tasks(self, out):
        self.lib.arc_vecenv_tasks(self.handle, out.ctypes.data)

    def action_counts(self, out):
        self.lib.arc_vecenv_action_counts(self.handle, out.ctypes.data)

    def close(self):
        if self.handle is not None:
            self.lib.arc_vecenv_free(ctypes.c_void_p(self.handle))
            self.handle = None


def make(games, **kwargs) -> Pool:
    return Pool(games, **kwargs)
