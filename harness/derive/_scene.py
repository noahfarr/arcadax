import dataclasses
import hashlib
import sys
from pathlib import Path

import numpy as np


@dataclasses.dataclass
class Atlas:
    pixels: np.ndarray
    h: np.ndarray
    w: np.ndarray
    keys: list[str]

    @property
    def size(self) -> int:
        return self.pixels.shape[0]

    @property
    def patch_shape(self) -> tuple[int, int]:
        return self.pixels.shape[1], self.pixels.shape[2]

    def save(self, path: str | Path) -> None:
        path = Path(path)
        np.savez_compressed(
            path.with_suffix(".npz"), pixels=self.pixels, h=self.h, w=self.w
        )
        path.with_suffix(".keys").write_text("\n".join(self.keys), encoding="utf-8")

    @classmethod
    def load(cls, path: str | Path) -> "Atlas":
        path = Path(path)
        arrays = np.load(path.with_suffix(".npz"))
        keys = path.with_suffix(".keys").read_text(encoding="utf-8").splitlines()
        return cls(arrays["pixels"], arrays["h"], arrays["w"], keys)

    @classmethod
    def from_images(cls, images: dict[str, np.ndarray]) -> "Atlas":
        keys = sorted(images)
        patch_h = max(images[k].shape[0] for k in keys)
        patch_w = max(images[k].shape[1] for k in keys)
        pixels = np.full((len(keys), patch_h, patch_w), -1, np.int8)
        h = np.zeros(len(keys), np.int32)
        w = np.zeros(len(keys), np.int32)
        for index, key in enumerate(keys):
            image = images[key]
            pixels[index, : image.shape[0], : image.shape[1]] = image
            h[index], w[index] = image.shape
        return cls(pixels, h, w, keys)


def image_key(image: np.ndarray) -> str:
    array = np.ascontiguousarray(image, dtype=np.int8)
    return hashlib.sha1(
        array.tobytes() + str(array.shape).encode()
    ).hexdigest()[:16]


ENGINES = {
    "lf52": {
        "engine": "rjfmjxejeiq",
        "render": "vclswpkbjs",
        "flatten": "luszojnlqu",
        "image": "sxwqiwdisg",
        "dirty": "wuomahqexpl",
    },
    "bp35": {
        "engine": "yodvybvftxa",
        "render": "srlqyenmue",
        "flatten": "upiapwkxxz",
        "image": "ieikpxxuml",
        "dirty": "hlfdukibtwe",
    },
}


def capture_atlas(game_id: str, source: str | Path, actions_per_level: int = 200) -> Atlas:
    from arcengine import ActionInput, GameAction

    from ..reference import load_game as load_reference_game

    game = load_reference_game(source)
    module = next(m for n, m in sys.modules.items() if n.startswith(f"arc_reference_{game_id}"))
    spec = ENGINES[game_id]
    engine = getattr(module, spec["engine"])

    images: dict[str, np.ndarray] = {}
    original = getattr(engine, spec["render"])

    def wrapped(self):
        for _, _, image in getattr(self, spec["flatten"])():
            array = getattr(image, spec["image"])()
            if array.size:
                images[image_key(array)] = np.asarray(array, np.int8)
        return original(self)

    setattr(engine, spec["render"], wrapped)
    try:
        game.perform_action(ActionInput(id=GameAction.RESET), raw=True)
        rng = np.random.default_rng(0)
        for level in range(len(game._levels)):
            game.perform_action(ActionInput(id=GameAction.RESET), raw=True)
            game.set_level(level)
            for _ in range(actions_per_level):
                action = int(rng.choice(game._available_actions))
                data = (
                    {"x": int(rng.integers(FRAME_SIZE)), "y": int(rng.integers(FRAME_SIZE))}
                    if action == GameAction.ACTION6.value
                    else {}
                )
                game.perform_action(
                    ActionInput(id=GameAction.from_id(action), data=data), raw=True
                )
                if game._state.value in ("WIN", "GAME_OVER"):
                    game.perform_action(ActionInput(id=GameAction.RESET), raw=True)
                    game.set_level(level)
    finally:
        setattr(engine, spec["render"], original)

    return Atlas.from_images(images)
