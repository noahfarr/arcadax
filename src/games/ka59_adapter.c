#include "arc/game.h"
#include "ka59.h"

static void adapt_zero_aux(void *aux)
{
	(void)aux;
}

static void adapt_on_set_level(struct arc_game *game)
{
	ka59_on_set_level(
		&game->sprites, (const struct ka59_static *)game->statics,
		game->engine.level_index, (struct ka59_aux *)game->aux,
		&game->engine.next_order);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	ka59_step_once(&game->sprites, &game->camera,
		       (const struct ka59_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct ka59_aux *)game->aux,
		       &e->next_order, &e->score, &e->status, &e->next_level,
		       &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	ka59_render_interface(frame, (const struct ka59_static *)game->statics,
			      game->engine.level_index,
			      (const struct ka59_aux *)game->aux);
}

const struct arc_hooks ka59_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
