import enum

FRAME_SIZE = 64
TRANSPARENT = -1


class Interaction(enum.IntEnum):
    TANGIBLE = 0
    INTANGIBLE = 1
    INVISIBLE = 2
    REMOVED = 3


class Blocking(enum.IntEnum):
    NOT_BLOCKED = 0
    BOUNDING_BOX = 1
    PIXEL_PERFECT = 2


class GameState(enum.IntEnum):
    NOT_PLAYED = 0
    NOT_FINISHED = 1
    WIN = 2
    GAME_OVER = 3


class Action(enum.IntEnum):
    RESET = 0
    ACTION1 = 1
    ACTION2 = 2
    ACTION3 = 3
    ACTION4 = 4
    ACTION5 = 5
    ACTION6 = 6
    ACTION7 = 7


INTERACTION_FROM_NAME = {e.name: e for e in Interaction}
BLOCKING_FROM_NAME = {e.name: e for e in Blocking}
