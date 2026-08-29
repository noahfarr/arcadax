#include "arc/game.h"
#include "cn04.h"

static void adapt_zero_aux(void *aux)
{
	(void)aux;
}

static void adapt_on_set_level(struct arc_game *game)
{
	cn04_on_set_level(&game->sprites, &game->camera,
			  (const struct cn04_static *)game->statics,
			  game->engine.level_index,
			  (struct cn04_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	cn04_step_once(&game->sprites, &game->camera,
		       (const struct cn04_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct cn04_aux *)game->aux, &e->score,
		       &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	cn04_render_interface(frame, (const struct cn04_static *)game->statics,
			      game->engine.level_index,
			      game->engine.action_count);
}

const struct arc_hooks cn04_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
