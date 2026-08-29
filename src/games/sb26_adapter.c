#include "arc/game.h"
#include "sb26.h"

static void adapt_zero_aux(void *aux)
{
	sb26_zero_aux((struct sb26_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	sb26_on_set_level(
		&game->sprites, (const struct sb26_static *)game->statics,
		game->engine.level_index, (struct sb26_aux *)game->aux,
		&game->engine.next_order);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	sb26_step_once(&game->sprites, &game->camera, &game->scratch,
		       (const struct sb26_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       e->action_count, (struct sb26_aux *)game->aux,
		       &e->next_order, &e->score, &e->status, &e->next_level,
		       &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	sb26_render_interface(frame, &game->sprites, &game->camera,
			      &game->scratch,
			      (const struct sb26_static *)game->statics,
			      (const struct sb26_aux *)game->aux);
}

const struct arc_hooks sb26_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
