#include "arc/game.h"
#include "tr87.h"

static void adapt_zero_aux(void *aux)
{
	tr87_zero_aux((struct tr87_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	tr87_on_set_level(
		&game->sprites, (const struct tr87_static *)game->statics,
		game->engine.level_index, (struct tr87_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	tr87_step_once(
		&game->sprites, (const struct tr87_static *)game->statics,
		e->level_index, e->action_id, (struct tr87_aux *)game->aux,
		&e->score, &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	tr87_render_interface(frame, (const struct tr87_static *)game->statics,
			      game->engine.level_index,
			      (const struct tr87_aux *)game->aux);
}

const struct arc_hooks tr87_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
