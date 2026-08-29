#include "arc/game.h"
#include "tn36.h"

static void adapt_zero_aux(void *aux)
{
	tn36_zero_aux((struct tn36_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	tn36_on_set_level(
		&game->sprites, (const struct tn36_static *)game->statics,
		game->engine.level_index, (struct tn36_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	tn36_step_once(&game->sprites, &game->camera,
		       (const struct tn36_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct tn36_aux *)game->aux,
		       &e->next_order, &e->score, &e->status, &e->next_level,
		       &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	tn36_render_interface(frame, &game->sprites, &game->camera,
			      (const struct tn36_static *)game->statics,
			      game->engine.level_index,
			      (const struct tn36_aux *)game->aux);
}

const struct arc_hooks tn36_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
