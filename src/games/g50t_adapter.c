#include "arc/game.h"
#include "g50t.h"

static void adapt_zero_aux(void *aux)
{
	g50t_zero_aux((struct g50t_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	g50t_on_set_level(
		&game->sprites, (const struct g50t_static *)game->statics,
		game->engine.level_index, (struct g50t_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	g50t_step_once(
		&game->sprites, (const struct g50t_static *)game->statics,
		e->level_index, e->action_id, e->action_x, e->action_y,
		e->action_count, (struct g50t_aux *)game->aux, &e->next_order,
		&e->score, &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	g50t_render_interface(frame, &game->sprites, &game->camera,
			      (const struct g50t_static *)game->statics,
			      game->engine.level_index,
			      (const struct g50t_aux *)game->aux);
}

const struct arc_hooks g50t_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
