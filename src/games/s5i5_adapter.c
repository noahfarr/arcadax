#include "arc/game.h"
#include "s5i5.h"

static void adapt_zero_aux(void *aux)
{
	(void)aux;
}

static void adapt_on_set_level(struct arc_game *game)
{
	s5i5_on_set_level((struct s5i5_aux *)game->aux,
			  (const struct s5i5_static *)game->statics,
			  game->engine.level_index);
}

static void adapt_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	struct s5i5_engine eng;
	eng.level_index = e->level_index;
	eng.action_id = e->action_id;
	eng.action_x = e->action_x;
	eng.action_y = e->action_y;
	eng.score = e->score;
	eng.status = e->status;
	eng.action_complete = e->action_complete;
	eng.next_level = e->next_level;

	s5i5_step_once(&game->sprites, &game->camera, &eng,
		       (struct s5i5_aux *)game->aux,
		       (const struct s5i5_static *)game->statics);

	e->score = eng.score;
	e->status = eng.status;
	e->action_complete = eng.action_complete;
	e->next_level = eng.next_level;
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	s5i5_render_interface(frame, (const struct s5i5_aux *)game->aux,
			      (const struct s5i5_static *)game->statics,
			      game->engine.level_index);
}

const struct arc_hooks s5i5_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
