#include "arc/game.h"

int32_t arc_harness_score(const ArcGame *game) { return game->engine.score; }
int32_t arc_harness_status(const ArcGame *game) { return game->engine.status; }
int32_t arc_harness_level_index(const ArcGame *game) { return game->engine.level_index; }
