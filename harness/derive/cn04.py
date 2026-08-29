import numpy as np
from collections import defaultdict
from ._base import GameData, Setup, extract, load_reference_game, source_for


_SOURCE_PATH = str(source_for("cn04"))


_ROTATIONS = (0, 90, 180, 270)


_CONNECTOR_A = 8


_CONNECTOR_B = 13


_MATED = 3


_ZEROED = 0


_GREY = 4


def _rotated(base: np.ndarray, degrees: int) -> np.ndarray:
    k = int((-degrees % 360) / 90)
    return np.rot90(base, k=k) if k else base.copy()


def _build_static_tables(source, data: GameData):
    ref = load_reference_game(source)
    num_levels, num_slots = data.num_levels, data.num_slots
    ph, pw = data.patch_shape

    rotvar = np.full((num_levels, num_slots, 4, ph, pw), -1, np.int8)
    h_table = np.zeros((num_levels, num_slots, 4), np.int32)
    w_table = np.zeros((num_levels, num_slots, 4), np.int32)
    init_rot = np.zeros((num_levels, num_slots), np.int32)

    fwd_row = np.zeros((num_levels, num_slots, 4, ph, pw), np.int32)
    fwd_col = np.zeros((num_levels, num_slots, 4, ph, pw), np.int32)
    fwd_valid = np.zeros((num_levels, num_slots, 4, ph, pw), bool)
    bwd_row = np.zeros((num_levels, num_slots, 4, ph, pw), np.int32)
    bwd_col = np.zeros((num_levels, num_slots, 4, ph, pw), np.int32)
    bwd_valid = np.zeros((num_levels, num_slots, 4, ph, pw), bool)

    group_size = np.ones((num_levels, num_slots), np.int32)
    group_rank = np.zeros((num_levels, num_slots), np.int32)
    max_group = 1
    groups_by_level = []
    for li in range(num_levels):
        groups: dict[tuple[int, int], list[int]] = defaultdict(list)
        for si in range(num_slots):
            if data.alive[li, si]:
                groups[(int(data.x[li, si]), int(data.y[li, si]))].append(si)
        groups_by_level.append(groups)
        for members in groups.values():
            max_group = max(max_group, len(members))

    group_members = np.full((num_levels, num_slots, max_group), -1, np.int32)
    init_selected = np.full(num_levels, -1, np.int32)

    for li, level in enumerate(ref._clean_levels):
        by_name = {s.name: s for s in level.get_sprites()}

        for members in groups_by_level[li].values():
            ordered = sorted(members, key=lambda si: int(data.layer[li, si]))
            for rank, si in enumerate(ordered):
                group_size[li, si] = len(ordered)
                group_rank[li, si] = rank
                for k, m in enumerate(ordered):
                    group_members[li, si, k] = m

        candidates = [
            si
            for si in range(num_slots)
            if data.alive[li, si] and data.interaction[li, si] <= 1
        ]
        if candidates:
            init_selected[li] = min(
                candidates, key=lambda si: int(data.x[li, si]) ** 2 + int(data.y[li, si]) ** 2
            )

        for si in range(num_slots):
            name = data.names[li][si]
            if not name:
                continue
            sprite = by_name[name]
            base = sprite.pixels
            rotatable = group_size[li, si] == 1
            states = _ROTATIONS if rotatable else (sprite.rotation,)
            init_idx = _ROTATIONS.index(sprite.rotation)
            init_rot[li, si] = init_idx
            for degrees in states:
                ridx = _ROTATIONS.index(degrees)
                variant = _rotated(base, degrees)
                vh, vw = variant.shape
                rotvar[li, si, ridx, :vh, :vw] = variant
                h_table[li, si, ridx] = vh
                w_table[li, si, ridx] = vw

            h_init, w_init = int(h_table[li, si, init_idx]), int(w_table[li, si, init_idx])
            idx_init = np.arange(h_init * w_init).reshape(h_init, w_init)
            for degrees in states:
                ridx = _ROTATIONS.index(degrees)
                k_delta = (init_idx - ridx) % 4
                rotated_idx = np.rot90(idx_init, k=k_delta) if k_delta else idx_init.copy()
                hk, wk = rotated_idx.shape
                row0, col0 = rotated_idx // w_init, rotated_idx % w_init
                fwd_row[li, si, ridx, :hk, :wk] = row0
                fwd_col[li, si, ridx, :hk, :wk] = col0
                fwd_valid[li, si, ridx, :hk, :wk] = True
                rr, cc = np.indices((hk, wk))
                bwd_row[li, si, ridx][row0, col0] = rr
                bwd_col[li, si, ridx][row0, col0] = cc
                bwd_valid[li, si, ridx][row0, col0] = True

    return {
        "rotvar": rotvar,
        "h_table": h_table,
        "w_table": w_table,
        "init_rot": init_rot,
        "group_size": group_size,
        "group_rank": group_rank,
        "group_members": group_members,
        "init_selected": init_selected,
        "fwd_row": fwd_row,
        "fwd_col": fwd_col,
        "fwd_valid": fwd_valid,
        "bwd_row": bwd_row,
        "bwd_col": bwd_col,
        "bwd_valid": bwd_valid,
    }


DATA_PARAM = 'data'


def derive(data, source=None):
    self = Setup(data)
    tables = _build_static_tables(source or _SOURCE_PATH, data)
    self._rotvar = np.asarray(tables["rotvar"])
    self._h_table = np.asarray(tables["h_table"])
    self._w_table = np.asarray(tables["w_table"])
    self._init_rot = np.asarray(tables["init_rot"])
    self._group_size = np.asarray(tables["group_size"])
    self._group_rank = np.asarray(tables["group_rank"])
    self._group_members = np.asarray(tables["group_members"])
    self._init_selected = np.asarray(tables["init_selected"])
    self._fwd_row = np.asarray(tables["fwd_row"])
    self._fwd_col = np.asarray(tables["fwd_col"])
    self._fwd_valid = np.asarray(tables["fwd_valid"])
    self._bwd_row = np.asarray(tables["bwd_row"])
    self._bwd_col = np.asarray(tables["bwd_col"])
    self._bwd_valid = np.asarray(tables["bwd_valid"])
    self._budget = np.asarray([d.get("MaxSteps") or 150 for d in data.level_data], np.int32)
    self._greymask = np.asarray(
        [bool(d.get("GreyMasking") or False) for d in data.level_data]
    )
    self._level_bg = np.asarray(
        [d.get("BackgroundColour", data.background) for d in data.level_data], np.int8
    )
    return self


def make_args(source, seed=0):
    return (extract(source),), {'source': source}
