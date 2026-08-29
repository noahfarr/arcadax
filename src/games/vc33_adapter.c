#include "arc/game.h"
#include "vc33.h"

static void adapt_zero_aux(void *aux)
{
	vc33_zero_aux((struct vc33_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	vc33_on_set_level((const struct vc33_static *)game->statics,
			  game->engine.level_index,
			  (struct vc33_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	vc33_step_once(&game->sprites, &game->camera,
		       (const struct vc33_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       (struct vc33_aux *)game->aux, &e->next_order, &e->score,
		       &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	vc33_render_interface(frame, (const struct vc33_static *)game->statics,
			      game->engine.level_index,
			      (const struct vc33_aux *)game->aux);
}

const struct arc_hooks vc33_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
