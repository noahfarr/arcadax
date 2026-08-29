#include "arc/game.h"
#include "re86.h"

static void adapt_zero_aux(void *aux)
{
	re86_zero_aux((struct re86_aux *)aux);
}

static void adapt_on_set_level(struct arc_game *game)
{
	re86_on_set_level((struct re86_aux *)game->aux,
			  (const struct re86_static *)game->statics,
			  game->engine.level_index);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	struct re86_engine eng;
	eng.level_index = e->level_index;
	eng.score = e->score;
	eng.status = e->status;
	eng.camera_width = game->camera.width;
	eng.camera_height = game->camera.height;
	eng.next_level = (int8_t)e->next_level;
	eng.action_complete = (int8_t)e->action_complete;

	re86_step_once(&game->sprites, (struct re86_aux *)game->aux, &eng,
		       (const struct re86_static *)game->statics, e->action_id);

	e->score = eng.score;
	e->status = eng.status;
	e->next_level = (uint8_t)eng.next_level;
	e->action_complete = (uint8_t)eng.action_complete;
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	re86_render_interface(frame, (const struct re86_aux *)game->aux,
			      (const struct re86_static *)game->statics,
			      game->engine.level_index);
}

const struct arc_hooks re86_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
