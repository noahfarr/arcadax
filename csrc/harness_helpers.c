#include "game.h"

int32_t harness_score(const Game *game) { return game->engine.score; }
int32_t harness_status(const Game *game) { return game->engine.status; }
int32_t harness_level_index(const Game *game) { return game->engine.level_index; }
