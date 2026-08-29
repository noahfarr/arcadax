#include "arc/game.h"
#include "sp80.h"

static void adapt_zero_aux(void *aux)
{
	(void)aux;
}

static void adapt_on_set_level(struct arc_game *game)
{
	sp80_on_set_level(
		&game->sprites, (const struct sp80_static *)game->statics,
		game->engine.level_index, (struct sp80_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	sp80_step_once(&game->sprites, &game->camera,
		       (const struct sp80_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct sp80_aux *)game->aux,
		       &e->next_order, &e->score, &e->status, &e->next_level,
		       &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	sp80_render_interface(frame, &game->sprites, &game->camera,
			      (const struct sp80_static *)game->statics,
			      game->engine.level_index,
			      (const struct sp80_aux *)game->aux);
}

const struct arc_hooks sp80_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
