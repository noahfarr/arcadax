#include "arc/game.h"

int32_t arc_harness_score(const struct arc_game *game)
{
	return game->engine.score;
}
int32_t arc_harness_status(const struct arc_game *game)
{
	return game->engine.status;
}
int32_t arc_harness_level_index(const struct arc_game *game)
{
	return game->engine.level_index;
}
