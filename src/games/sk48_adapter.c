#include "arc/game.h"
#include "sk48.h"

static void adapt_zero_aux(void *aux)
{
	sk48_zero_aux((struct sk48_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	sk48_on_set_level(
		&game->sprites, (const struct sk48_static *)game->statics,
		game->engine.level_index, (struct sk48_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	sk48_step_once(&game->sprites, &game->camera,
		       (const struct sk48_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct sk48_aux *)game->aux, &e->score,
		       &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	sk48_render_interface(frame, (const struct sk48_aux *)game->aux);
}

const struct arc_hooks sk48_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
