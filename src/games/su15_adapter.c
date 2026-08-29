#include "arc/game.h"
#include "su15.h"

static void adapt_zero_aux(void *aux)
{
	su15_zero_aux((struct su15_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	su15_on_set_level(
		&game->sprites, (const struct su15_static *)game->statics,
		game->engine.level_index, (struct su15_aux *)game->aux,
		&game->engine.next_order);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	su15_step_once(&game->sprites, &game->camera,
		       (const struct su15_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       (struct su15_aux *)game->aux, &e->next_order, &e->score,
		       &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	su15_render_interface(frame, (const struct su15_static *)game->statics,
			      game->engine.level_index,
			      (const struct su15_aux *)game->aux);
}

const struct arc_hooks su15_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
