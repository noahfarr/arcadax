#include "arc/game.h"
#include "m0r0.h"

static void adapt_zero_aux(void *aux)
{
	m0r0_zero_aux((struct m0r0_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	m0r0_on_set_level(
		&game->sprites, (const struct m0r0_static *)game->statics,
		game->engine.level_index, (struct m0r0_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	m0r0_step_once(&game->sprites, &game->camera,
		       (const struct m0r0_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct m0r0_aux *)game->aux, &e->score,
		       &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	m0r0_render_interface(frame, &game->sprites, &game->camera,
			      (const struct m0r0_static *)game->statics,
			      game->engine.level_index,
			      (const struct m0r0_aux *)game->aux);
}

const struct arc_hooks m0r0_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
