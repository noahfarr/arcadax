import ctypes
import numpy as np

from ._util import Buffer

def csr2(mask):
    offsets = [0]
    flat = []
    for li in range(mask.shape[0]):
        idx = np.nonzero(mask[li])[0]
        flat.extend(int(i) for i in idx)
        offsets.append(len(flat))
    return np.asarray(offsets, np.int32), np.asarray(flat if flat else [0], np.int32)
def csr3(mask):
    L, S, _ = mask.shape
    offsets = [0]
    flat = []
    for li in range(L):
        for ri in range(S):
            idx = np.nonzero(mask[li, ri])[0]
            flat.extend(int(i) for i in idx)
            offsets.append(len(flat))
    return np.asarray(offsets, np.int32), np.asarray(flat if flat else [0], np.int32)


def build(game):
    n = game.num_slots
    keep = {}

    def p32(a):
        arr = np.ascontiguousarray(np.asarray(a, np.int32))
        keep[id(arr)] = arr
        return Buffer(arr, ctypes.c_int32), arr

    is_pipe = np.asarray(game._is_pipe, bool)
    is_connector = np.asarray(game._is_connector, bool)
    is_target = np.asarray(game._is_target, bool)
    descendants = np.asarray(game._descendants, bool)
    slider_pipe = np.asarray(game._slider_pipe, bool)

    pipe_offset, pipe_flat = csr2(is_pipe)
    conn_offset, conn_flat = csr2(is_connector)
    target_offset, target_flat = csr2(is_target)
    desc_offset, desc_flat = csr3(descendants)
    slider_pipe_offset, slider_pipe_flat = csr3(slider_pipe)

    st = dict()
    st["num_levels"] = game.num_levels
    st["num_slots"] = n
    st["ph"], st["pw"] = game.data.patch_shape
    st["handle_tag"] = int(game._handle_tag)
    st["slider_tag"] = int(game._slider_tag)

    pc_ptr, pc_arr = p32(game._pipe_color)
    hc_ptr, hc_arr = p32(game._handle_color)
    ps_ptr, ps_arr = p32(game._parent_slot)
    bd_ptr, bd_arr = p32(game._budget)
    po_ptr, po_arr = p32(pipe_offset)
    pf_ptr, pf_arr = p32(pipe_flat)
    do_ptr, do_arr = p32(desc_offset)
    df_ptr, df_arr = p32(desc_flat)
    spo_ptr, spo_arr = p32(slider_pipe_offset)
    spf_ptr, spf_arr = p32(slider_pipe_flat)
    to_ptr, to_arr = p32(target_offset)
    tf_ptr, tf_arr = p32(target_flat)
    co_ptr, co_arr = p32(conn_offset)
    cf_ptr, cf_arr = p32(conn_flat)

    st["pipe_color"], st["handle_color"], st["parent_slot"], st["budget"] = pc_ptr, hc_ptr, ps_ptr, bd_ptr
    st["pipe_offset"], st["pipe_flat"] = po_ptr, pf_ptr
    st["desc_offset"], st["desc_flat"] = do_ptr, df_ptr
    st["slider_pipe_offset"], st["slider_pipe_flat"] = spo_ptr, spf_ptr
    st["target_offset"], st["target_flat"] = to_ptr, tf_ptr
    st["conn_offset"], st["conn_flat"] = co_ptr, cf_ptr

    keep_list = [pc_arr, hc_arr, ps_arr, bd_arr, po_arr, pf_arr, do_arr, df_arr,
                spo_arr, spf_arr, to_arr, tf_arr, co_arr, cf_arr]
    return st, keep_list
