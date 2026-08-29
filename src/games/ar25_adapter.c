#include "arc/game.h"
#include "ar25.h"

static void adapt_zero_aux(void *aux)
{
	ar25_zero_aux((struct ar25_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	ar25_on_set_level(
		&game->sprites, (const struct ar25_static *)game->statics,
		game->engine.level_index, (struct ar25_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	ar25_step_once(&game->sprites, &game->camera,
		       (const struct ar25_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       (struct ar25_aux *)game->aux, &e->score, &e->status,
		       &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	ar25_render_interface(frame, &game->sprites,
			      (const struct ar25_static *)game->statics,
			      game->engine.level_index,
			      (const struct ar25_aux *)game->aux);
}

const struct arc_hooks ar25_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
