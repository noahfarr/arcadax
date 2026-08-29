#include "arc/scene_game.h"
#include "bp35.h"

static void adapt_zero_aux(void *aux)
{
	bp35_zero_aux((struct bp35_aux *)aux);
}

static void adapt_build_level(struct arc_scene_game *game)
{
	bp35_build_level(
		&game->scene, (const struct bp35_static *)game->statics,
		game->engine.level_index, game->engine.action_id,
		game->engine.action_x, game->engine.action_y,
		(uint8_t)game->engine.next_level, (struct bp35_aux *)game->aux);
}

static void adapt_step_once(struct arc_scene_game *game)
{
	struct arc_scene_engine_state *e = &game->engine;
	bp35_step_once(&game->scene, (const struct bp35_static *)game->statics,
		       e->level_index, e->action_id, e->action_x, e->action_y,
		       (struct bp35_aux *)game->aux, &e->score, &e->status,
		       &e->next_level, &e->action_complete);
}

static void adapt_render_interface(struct arc_scene_game *game, int8_t *frame)
{
	bp35_render_interface(frame, (const struct bp35_static *)game->statics,
			      game->engine.level_index,
			      (const struct bp35_aux *)game->aux);
}

const struct arc_scene_hooks bp35_hooks = { adapt_zero_aux, adapt_build_level,
					    adapt_step_once,
					    adapt_render_interface };
