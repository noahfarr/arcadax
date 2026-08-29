#include "arc/game.h"
#include "sc25.h"

static void adapt_zero_aux(void *aux)
{
	sc25_zero_aux((struct sc25_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	sc25_on_set_level(
		&game->sprites, (const struct sc25_static *)game->statics,
		game->engine.level_index, (struct sc25_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	sc25_step_once(&game->sprites, &game->camera,
		       (const struct sc25_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       (struct sc25_aux *)game->aux, &e->score, &e->status,
		       &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	sc25_render_interface(frame, &game->sprites, &game->camera,
			      (const struct sc25_static *)game->statics,
			      game->engine.level_index,
			      (const struct sc25_aux *)game->aux);
}

const struct arc_hooks sc25_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
