#include "arc/game.h"
#include "wa30.h"

static void adapt_zero_aux(void *aux)
{
	(void)aux;
}

static void adapt_on_set_level(struct arc_game *game)
{
	wa30_on_set_level((const struct wa30_static *)game->statics,
			  game->engine.level_index,
			  (struct wa30_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	wa30_step_once(
		&game->sprites, (const struct wa30_static *)game->statics,
		e->level_index, e->action_id, (struct wa30_aux *)game->aux,
		&e->score, &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	wa30_render_interface(frame, (const struct wa30_static *)game->statics,
			      game->engine.level_index,
			      (const struct wa30_aux *)game->aux);
}

const struct arc_hooks wa30_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
