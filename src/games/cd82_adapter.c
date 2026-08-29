#include "arc/game.h"
#include "cd82.h"

static void adapt_zero_aux(void *aux)
{
	cd82_zero_aux((struct cd82_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	cd82_on_set_level(
		&game->sprites, (const struct cd82_static *)game->statics,
		game->engine.level_index, (struct cd82_aux *)game->aux,
		&game->engine.next_order);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	cd82_step_once(&game->sprites, &game->camera,
		       (const struct cd82_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct cd82_aux *)game->aux,
		       &e->next_order, &e->score, &e->status, &e->next_level,
		       &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	cd82_render_interface(frame, game->engine.action_count);
}

const struct arc_hooks cd82_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
