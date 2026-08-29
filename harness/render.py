import numpy as np

PALETTE = np.array([
    (0, 0, 0), (0, 116, 217), (255, 65, 54), (46, 204, 64), (255, 220, 0),
    (170, 170, 170), (240, 18, 190), (255, 133, 27), (127, 219, 255),
    (135, 12, 37), (60, 60, 60), (100, 200, 160), (200, 120, 255),
    (90, 90, 200), (30, 160, 90), (250, 250, 250),
], np.uint8)


def to_image(frame, scale: int = 8):
    from PIL import Image

    idx = np.clip(np.asarray(frame, np.int16), 0, 15)
    rgb = PALETTE[idx]
    return Image.fromarray(np.kron(rgb, np.ones((scale, scale, 1), np.uint8)))


def record(spec, path, actions, scale: int = 8, ms: int = 260,
           hold: int = 2200, library=None):
    from .dsl import DslGame

    game = DslGame(spec, library=library, max_frames=64)
    game.init()
    frames = [to_image(game.frame(), scale)]
    for action in actions:
        for f in game.act(*action):
            frames.append(to_image(f, scale))
    game.close()

    durations = [ms] * len(frames)
    durations[0] = 1200
    durations[-1] = hold
    frames[0].save(path, save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, disposal=1)
    return len(frames)
