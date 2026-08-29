#include "arc/game.h"
#include "lp85.h"

#include <stdlib.h>

static _Thread_local int32_t *scratch_sources;
static _Thread_local int32_t *scratch_hits;
static _Thread_local const struct lp85_static *scratch_for;

static void adapt_zero_aux(void *aux)
{
	lp85_zero_aux((struct lp85_aux *)aux);
}

static struct lp85_engine engine_from_game(const struct arc_game *game)
{
	struct lp85_engine e;
	e.camera = game->camera;
	e.level_index = game->engine.level_index;
	e.score = game->engine.score;
	e.status = game->engine.status;
	e.action_id = game->engine.action_id;
	e.action_x = game->engine.action_x;
	e.action_y = game->engine.action_y;
	e.action_complete = game->engine.action_complete;
	e.next_level = game->engine.next_level;
	return e;
}

static void engine_to_game(struct arc_game *game, const struct lp85_engine *e)
{
	game->camera = e->camera;
	game->engine.level_index = e->level_index;
	game->engine.score = e->score;
	game->engine.status = e->status;
	game->engine.action_id = e->action_id;
	game->engine.action_x = e->action_x;
	game->engine.action_y = e->action_y;
	game->engine.action_complete = e->action_complete;
	game->engine.next_level = e->next_level;
}

static void ensure_scratch(const struct lp85_static *st)
{
	if (scratch_for == st)
		return;
	free(scratch_sources);
	free(scratch_hits);
	scratch_sources = calloc((size_t)st->max_ring, sizeof(int32_t));
	scratch_hits = calloc((size_t)st->max_buttons, sizeof(int32_t));
	scratch_for = st;
}

static void adapt_on_set_level(struct arc_game *game)
{
	const struct lp85_static *st =
		(const struct lp85_static *)game->statics;
	ensure_scratch(st);
	struct lp85_engine e = engine_from_game(game);
	lp85_on_set_level(st, &e, (struct lp85_aux *)game->aux);
	engine_to_game(game, &e);
}

static void adapt_step_once(struct arc_game *game)
{
	const struct lp85_static *st =
		(const struct lp85_static *)game->statics;
	ensure_scratch(st);
	struct lp85_scratch scratch = { scratch_sources, scratch_hits };
	struct lp85_engine e = engine_from_game(game);
	lp85_step_once(st, &game->sprites, &scratch, &e,
		       (struct lp85_aux *)game->aux);
	engine_to_game(game, &e);
}

static void adapt_render_interface(struct arc_game *game, int8_t *frame)
{
	const struct lp85_static *st =
		(const struct lp85_static *)game->statics;
	struct lp85_engine e = engine_from_game(game);
	lp85_render_interface(st, &e, (const struct lp85_aux *)game->aux,
			      frame);
}

const struct arc_hooks lp85_hooks = { adapt_zero_aux, adapt_on_set_level,
				      adapt_step_once, adapt_render_interface };
