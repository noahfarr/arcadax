#include "arc/game.h"
#include "ls20.h"

static void adapt_zero_aux(void *aux)
{
	ls20_zero_aux((struct ls20_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	ls20_on_set_level(
		&game->sprites, (const struct ls20_static *)game->statics,
		game->engine.level_index, (struct ls20_aux *)game->aux,
		&game->engine.next_order);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	ls20_step_once(&game->sprites,
		       (const struct ls20_static *)game->statics,
		       e->level_index, e->action_id,
		       (struct ls20_aux *)game->aux, &e->next_order, &e->score,
		       &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	ls20_render_interface(frame, &game->sprites,
			      (const struct ls20_static *)game->statics,
			      game->engine.level_index,
			      (const struct ls20_aux *)game->aux);
}

const struct arc_hooks ls20_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
