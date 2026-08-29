import numpy as np
from ._base import GameData, Setup, extract, load_reference_game


EXTRA_SLOTS = 300


FRONTIER_CAP = 32


EXTRACT_KWARGS = {"extra_slots": EXTRA_SLOTS}


CHANGE, SPILL = 0, 1


TAG_LIOLF = "liolfvkveqg"


TAG_PLZW = "plzwjbfyfli"


TAG_REPW = "repwkzbkhxl"


TAG_SOWL = "sowlljgtjvn"


TAG_TUVK = "tuvkdkhdokr"


TAG_WAOE = "waoewejnqzc"


_ACTION_SWAP = {
    1: {1: 4, 2: 3, 3: 1, 4: 2},
    2: {1: 2, 2: 1, 3: 4, 4: 3},
    3: {1: 3, 2: 4, 3: 2, 4: 1},
}


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    self._extra_start = self.num_slots - EXTRA_SLOTS
    N = self.num_slots
    self._liolf = data.tag_index(TAG_LIOLF)
    self._plzw = data.tag_index(TAG_PLZW)
    self._repw = data.tag_index(TAG_REPW)
    self._sowl = data.tag_index(TAG_SOWL)
    self._tuvk = data.tag_index(TAG_TUVK)
    self._waoe = data.tag_index(TAG_WAOE)
    levels = range(data.num_levels)
    self._budget = np.asarray(
        [d.get("steps") or 50 for d in data.level_data], np.int32
    )
    self._rotation = np.asarray(
        [(d.get("dojfslwbg") or 0) // 90 % 4 for d in data.level_data], np.int32
    )
    remap = np.tile(np.arange(8, dtype=np.int32), (4, 1))
    for k, mapping in _ACTION_SWAP.items():
        for src, dst in mapping.items():
            remap[k, src] = dst
    self._action_remap = np.asarray(remap)
    slot_idx = np.arange(N)
    big = N + 10
    priority = np.where(
        data.tags[:, :, self._plzw],
        slot_idx[None, :],
        np.where(data.tags[:, :, self._tuvk], big + slot_idx[None, :], 10**6),
    )
    self._pick_priority = np.asarray(priority, np.int32)
    liolf_counts = [int((data.tags[l, :, self._liolf] & data.alive[l]).sum()) for l in levels]
    lmax = max(liolf_counts) if liolf_counts else 1
    liolf_static = np.full((data.num_levels, max(lmax, 1)), -1, np.int32)
    for l in levels:
        idxs = np.nonzero(data.tags[l, :, self._liolf] & data.alive[l])[0]
        liolf_static[l, : len(idxs)] = idxs
    self._liolf_static = np.asarray(liolf_static)
    sowl_counts = [int((data.tags[l, :, self._sowl] & data.alive[l]).sum()) for l in levels]
    smax = max(sowl_counts) if sowl_counts else 1
    spout_slot = np.full((data.num_levels, max(smax, 1)), -1, np.int32)
    spout_rel = np.zeros((data.num_levels, max(smax, 1), 2), np.int32)
    for l in levels:
        idxs = np.nonzero(data.tags[l, :, self._sowl] & data.alive[l])[0]
        for k, si in enumerate(idxs):
            rows, cols = np.nonzero(data.pixels[l, si] == 4)
            spout_slot[l, k] = si
            spout_rel[l, k] = (cols[0], rows[0])
    self._spout_slot = np.asarray(spout_slot)
    self._spout_rel = np.asarray(spout_rel)
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
