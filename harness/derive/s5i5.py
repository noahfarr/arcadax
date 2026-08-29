import numpy as np
from ._base import Array, GameData, Setup, extract, load_reference_game, reference_dir


TAG_PIPE = "0001qwdmnlybkb"


TAG_CONNECTOR = "0064ocqkuqacti"


TAG_SLIDER = "0066ghlkyvdbgg"


TAG_TARGET = "0087vvmblxkzdi"


TAG_HANDLE = "0089rvqdprjwpz"


PITCH = 3


CAP_MARKER = 3


BAR_FILLED, BAR_EMPTY = 3, 4


def _family_tables(
    data: GameData,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    source = sorted(reference_dir().glob(f"{data.game_id}-*.py"))[0]
    reference = load_reference_game(source)
    num_levels, num_slots = data.num_levels, data.num_slots

    descendants = np.zeros((num_levels, num_slots, num_slots), bool)
    parent_slot = np.full((num_levels, num_slots), -1, np.int32)
    slider_pipe = np.zeros((num_levels, num_slots, num_slots), bool)

    for li in range(num_levels):
        reference.set_level(li)
        sprite_list = reference.current_level.get_sprites()
        slot_of = {id(s): i for i, s in enumerate(sprite_list)}

        direct: dict[int, list[int]] = {}
        for key_obj, values in reference.uricqfoplr.items():
            k = slot_of.get(id(key_obj))
            if k is None:
                continue
            direct[k] = [slot_of[id(v)] for v in values if id(v) in slot_of]

        for root_slot, children in direct.items():
            seen: set[int] = set()
            frontier = list(children)
            while frontier:
                nxt = frontier.pop()
                if nxt in seen:
                    continue
                seen.add(nxt)
                frontier.extend(direct.get(nxt, []))
            for s in seen:
                descendants[li, root_slot, s] = True

        for pipe_idx, pipe_obj in enumerate(sprite_list):
            for key_obj, values in reference.uricqfoplr.items():
                if pipe_obj in values:
                    parent_slot[li, pipe_idx] = slot_of.get(id(key_obj), -1)
                    break

        for slider_obj, pipe_objs in reference.pigtralzpb.items():
            s_idx = slot_of.get(id(slider_obj))
            if s_idx is None:
                continue
            for p in pipe_objs:
                p_idx = slot_of.get(id(p))
                if p_idx is not None:
                    slider_pipe[li, s_idx, p_idx] = True

    return descendants, parent_slot, slider_pipe


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    n = self.num_slots
    tags = np.asarray(data.tags)
    alive = np.asarray(data.alive)
    def tag_mask(name: str) -> Array:
        return tags[:, :, data.tag_index(name)] & alive
    self._is_pipe = tag_mask(TAG_PIPE)
    self._is_connector = tag_mask(TAG_CONNECTOR)
    self._is_slider = tag_mask(TAG_SLIDER)
    self._is_target = tag_mask(TAG_TARGET)
    self._is_handle = tag_mask(TAG_HANDLE)
    self._handle_tag = data.tag_index(TAG_HANDLE)
    self._slider_tag = data.tag_index(TAG_SLIDER)
    self._pipe_color = np.asarray(data.pixels[:, :, 1, 1], np.int32)
    handle_color = np.zeros((data.num_levels, n), np.int32)
    for li in range(data.num_levels):
        for si in range(n):
            half = int(data.h[li, si]) // 2
            handle_color[li, si] = data.pixels[li, si, half, half]
    self._handle_color = np.asarray(handle_color)
    descendants, parent_slot, slider_pipe = _family_tables(data)
    self._descendants = np.asarray(descendants)
    self._parent_slot = np.asarray(parent_slot)
    self._slider_pipe = np.asarray(slider_pipe)
    self._budget = np.asarray([d["StepCounter"] for d in data.level_data], np.int32)
    return self


def make_args(source, seed=0):
    return (extract(source),), {}
