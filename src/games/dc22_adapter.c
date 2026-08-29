#include "arc/game.h"
#include "dc22.h"

static void adapt_zero_aux(void *aux)
{
	(void)aux;
}

static void adapt_on_set_level(struct arc_game *game)
{
	dc22_on_set_level((const struct dc22_static *)game->statics,
			  game->engine.level_index,
			  (struct dc22_aux *)game->aux);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	dc22_step_once(&game->sprites, &game->camera,
		       (const struct dc22_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       (struct dc22_aux *)game->aux, &e->score, &e->status,
		       &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	dc22_render_interface(frame, &game->sprites, &game->camera,
			      (const struct dc22_static *)game->statics,
			      game->engine.level_index,
			      (const struct dc22_aux *)game->aux);
}

const struct arc_hooks dc22_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
