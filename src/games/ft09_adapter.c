#include "arc/game.h"
#include "ft09.h"

static void adapt_zero_aux(void *aux)
{
	ft09_zero_aux((struct ft09_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	ft09_on_set_level(
		&game->sprites, (const struct ft09_static *)game->statics,
		game->engine.level_index, (struct ft09_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	ft09_step_once(&game->sprites, &game->camera,
		       (const struct ft09_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       (struct ft09_aux *)game->aux, &e->score, &e->status,
		       &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	const struct ft09_aux *aux = (const struct ft09_aux *)game->aux;
	ft09_render_interface(frame, (const struct ft09_static *)game->statics,
			      game->engine.level_index, aux->steps);
}

const struct arc_hooks ft09_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
