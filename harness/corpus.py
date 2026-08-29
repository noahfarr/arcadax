import json
from pathlib import Path

import numpy as np

from .dsl import Kind, Spec
from .generate import sample_environment
from .validate import certify


def save(spec: Spec, labels: dict, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        path, layouts=spec.layouts, floors=spec.floors,
        kinds=np.array([[k.color, k.motion, k.motion_a, k.motion_b, k.deadly,
                         k.gravity, k.on_enter, k.enter_a, k.enter_b,
                         k.on_click, k.click_a, k.click_b]
                        for k in spec.kinds], np.int32),
        params=np.array([spec.player_kind, spec.win_mode, spec.win_a,
                         spec.win_b, spec.pitch, spec.origin_x, spec.origin_y,
                         spec.background], np.int32),
        labels=np.frombuffer(json.dumps(labels).encode("utf-8"), np.uint8))


def load(path: Path) -> tuple[Spec, dict]:
    with np.load(path) as z:
        k = z["kinds"]
        p = z["params"]
        spec = Spec(
            kinds=[Kind(*[int(v) for v in row]) for row in k],
            layouts=z["layouts"], floors=z["floors"], player_kind=int(p[0]),
            win_mode=int(p[1]), win_a=int(p[2]), win_b=int(p[3]),
            pitch=int(p[4]), origin_x=int(p[5]), origin_y=int(p[6]),
            background=int(p[7]))
        labels = json.loads(bytes(z["labels"]).decode("utf-8"))
    return spec, labels


def build(count: int, out: Path, seed: int = 0, trials: int = 20_000,
          horizon: int = 400, threads: int = 8, verbose: bool = True) -> list:
    rng = np.random.default_rng(seed)
    out = Path(out)
    manifest = []
    made = 0
    attempts = 0
    while made < count and attempts < count * 6:
        attempts += 1
        proposal = sample_environment(rng)
        if proposal is None:
            continue
        spec = proposal.spec
        rates = []
        for level in range(spec.num_levels):
            rate, _ = certify(spec, trials=trials, horizon=horizon,
                              threads=threads, seed=made * 97 + level + 1,
                              start_level=level)
            rates.append(rate)
        pulls = [m["pulls"] for m in proposal.mechanics["levels"]]
        if rates[0] < 1e-3:
            continue
        labels = {"pulls": pulls, "random_rate": rates,
                  "grid": [spec.grid_w, spec.grid_h],
                  "levels": spec.num_levels}
        name = f"env_{made:04d}"
        save(spec, labels, out / f"{name}.npz")
        manifest.append({"name": name, **labels})
        made += 1
        if verbose:
            print(f"  {name}: pulls={pulls} rates="
                  f"{['%.5f' % r for r in rates]}")
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1))
    return manifest
