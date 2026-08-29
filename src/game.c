#include "arc/game.h"

#include <stdlib.h>
#include <string.h>

static void load_level(struct arc_game *game, int32_t index)
{
	const struct arc_level_data *d = game->levels;
	int32_t n = d->num_slots;
	size_t off = (size_t)index * n;
	memcpy(game->sprites.x, d->x + off, sizeof(int32_t) * n);
	memcpy(game->sprites.y, d->y + off, sizeof(int32_t) * n);
	memcpy(game->sprites.h, d->h + off, sizeof(int32_t) * n);
	memcpy(game->sprites.w, d->w + off, sizeof(int32_t) * n);
	memcpy(game->sprites.layer, d->layer + off, sizeof(int32_t) * n);
	memcpy(game->sprites.order, d->order + off, sizeof(int32_t) * n);
	memcpy(game->sprites.interaction, d->interaction + off, n);
	memcpy(game->sprites.blocking, d->blocking + off, n);
	memcpy(game->sprites.alive, d->alive + off, n);
	memcpy(game->sprites.tags, d->tags + off * (size_t)d->num_tags,
	       (size_t)n * d->num_tags);
	memset(game->sprites.overridden, 0, n);
	game->atlas.pixels = d->pixels + off * (size_t)d->ph * d->pw;
	if (game->atlas.pixels != game->bbox_atlas) {
		arc_sprites_recompute_bbox(&game->sprites);
		game->bbox_atlas = game->atlas.pixels;
	}
}

struct arc_game *arc_game_new(const struct arc_level_data *levels,
			      const struct arc_hooks *hooks, void *aux,
			      void *statics, const int32_t *simple_actions,
			      int32_t num_simple, int32_t has_click,
			      int32_t max_frames)
{
	struct arc_game *game = calloc(1, sizeof(struct arc_game));
	int32_t n = levels->num_slots;
	game->levels = levels;
	game->hooks = hooks;
	game->aux = aux;
	game->statics = statics;
	game->simple_actions = simple_actions;
	game->num_simple = num_simple;
	game->has_click = has_click;
	game->max_frames = max_frames;
	game->num_actions =
		num_simple + (has_click ? ARC_FRAME_SIZE * ARC_FRAME_SIZE : 0);

	game->atlas.num_slots = n;
	game->atlas.num_tags = levels->num_tags;
	game->atlas.ph = levels->ph;
	game->atlas.pw = levels->pw;
	game->atlas.pixels = levels->pixels;

	struct arc_sprites *s = &game->sprites;
	s->atlas = &game->atlas;
	s->x = calloc(n, sizeof(int32_t));
	s->y = calloc(n, sizeof(int32_t));
	s->h = calloc(n, sizeof(int32_t));
	s->w = calloc(n, sizeof(int32_t));
	s->layer = calloc(n, sizeof(int32_t));
	s->order = calloc(n, sizeof(int32_t));
	s->interaction = calloc(n, 1);
	s->blocking = calloc(n, 1);
	s->alive = calloc(n, 1);
	s->tags = calloc((size_t)n * levels->num_tags, 1);
	s->pixels = calloc((size_t)n * levels->ph * levels->pw, 1);
	s->overridden = calloc(n, 1);
	s->solid = calloc(n, 1);
	s->bbox = calloc((size_t)n * 4, sizeof(int32_t));
	game->scratch = malloc(sizeof(struct arc_render_scratch));
	arc_render_scratch_init(game->scratch, &game->atlas);
	game->owns_scratch = 1;
	return game;
}

void arc_game_free(struct arc_game *game)
{
	struct arc_sprites *s = &game->sprites;
	free(s->x);
	free(s->y);
	free(s->h);
	free(s->w);
	free(s->layer);
	free(s->order);
	free(s->interaction);
	free(s->blocking);
	free(s->alive);
	free(s->tags);
	free(s->pixels);
	free(s->overridden);
	free(s->solid);
	free(s->bbox);
	if (game->owns_scratch) {
		arc_render_scratch_free(game->scratch);
		free(game->scratch);
	}
	free(game);
}

void arc_game_share_scratch(struct arc_game *game,
			    struct arc_render_scratch *scratch)
{
	if (game->owns_scratch) {
		arc_render_scratch_free(game->scratch);
		free(game->scratch);
	}
	game->scratch = scratch;
	game->owns_scratch = 0;
}

void arc_game_complete_action(struct arc_game *game)
{
	game->engine.action_complete = 1;
}

void arc_game_lose(struct arc_game *game)
{
	game->engine.status = GAME_OVER;
}

void arc_game_next_level(struct arc_game *game)
{
	int is_last = game->engine.level_index == game->levels->num_levels - 1;
	game->engine.score += 1;
	game->engine.next_level = !is_last;
	if (is_last)
		game->engine.status = WIN;
}

void arc_game_set_level(struct arc_game *game, int32_t index)
{
	const struct arc_level_data *d = game->levels;
	load_level(game, index);
	game->camera.width = d->grid_size[index * 2];
	game->camera.height = d->grid_size[index * 2 + 1];
	game->engine.level_index = index;
	game->engine.action_count = 0;
	game->engine.next_order = d->num_slots;
	if (game->hooks->on_set_level)
		game->hooks->on_set_level(game);
}

static void initial_camera(struct arc_game *game)
{
	const struct arc_level_data *d = game->levels;
	game->camera.x = 0;
	game->camera.y = 0;
	game->camera.width = d->grid_size[0];
	game->camera.height = d->grid_size[1];
	game->camera.background = d->background;
	game->camera.letter_box = d->letter_box;
}

static void full_reset(struct arc_game *game)
{
	game->engine.score = 0;
	game->engine.full_reset = 1;
	initial_camera(game);
	arc_game_set_level(game, 0);
	game->engine.status = NOT_FINISHED;
}

static void level_reset(struct arc_game *game)
{
	arc_game_set_level(game, game->engine.level_index);
	game->engine.status = NOT_FINISHED;
}

static void handle_reset(struct arc_game *game)
{
	if (game->engine.action_count == 0 || game->engine.status == WIN)
		full_reset(game);
	else
		level_reset(game);
}

static void advance_level(struct arc_game *game)
{
	arc_game_set_level(game, game->engine.level_index + 1);
	game->engine.next_level = 0;
}

static int32_t perform(struct arc_game *game, int32_t action_id,
		       int32_t action_x, int32_t action_y, int8_t *frames,
		       int32_t max_out)
{
	struct arc_engine_state *e = &game->engine;
	e->full_reset = 0;
	int is_reset = action_id == ARC_ACTION_RESET;
	int terminal = e->status == WIN || e->status == GAME_OVER;
	if (is_reset)
		handle_reset(game);
	if (!is_reset && terminal)
		return 0;

	e->status = NOT_FINISHED;
	e->action_id = action_id;
	e->action_x = action_x;
	e->action_y = action_y;
	e->action_complete = 0;
	if (!is_reset)
		e->action_count += 1;

	int32_t count = 0;
	while (!(!e->next_level && e->action_complete) &&
	       count < game->max_frames) {
		if (e->next_level)
			advance_level(game);
		else
			game->hooks->step_once(game);
		if (frames && count < max_out)
			arc_game_frame(game, frames + (size_t)count *
							      ARC_FRAME_SIZE *
							      ARC_FRAME_SIZE);
		count++;
	}
	return count;
}

int32_t arc_game_perform_action(struct arc_game *game, int32_t action_id,
				int32_t action_x, int32_t action_y)
{
	return perform(game, action_id, action_x, action_y, NULL, 0);
}

int32_t arc_game_perform_action_frames(struct arc_game *game, int32_t action_id,
				       int32_t action_x, int32_t action_y,
				       int8_t *frames, int32_t max_out)
{
	return perform(game, action_id, action_x, action_y, frames, max_out);
}

void arc_game_decode_action(const struct arc_game *game, int32_t action,
			    int32_t *action_id, int32_t *x, int32_t *y)
{
	if (action < 0)
		action = 0;
	if (action >= game->num_actions)
		action = game->num_actions - 1;
	if (!game->has_click) {
		*action_id = game->simple_actions[action];
		*x = 0;
		*y = 0;
		return;
	}
	int32_t click = action - game->num_simple;
	if (click >= 0) {
		*action_id = ARC_ACTION6;
		*x = click % ARC_FRAME_SIZE;
		*y = click / ARC_FRAME_SIZE;
		return;
	}
	*action_id = game->num_simple ? game->simple_actions[action] : 0;
	*x = 0;
	*y = 0;
}

void arc_game_frame(struct arc_game *game, int8_t *frame)
{
	arc_render(&game->sprites, &game->camera, game->scratch, frame);
	if (game->hooks->render_interface)
		game->hooks->render_interface(game, frame);
}

void arc_game_init(struct arc_game *game)
{
	memset(&game->engine, 0, sizeof(struct arc_engine_state));
	game->engine.status = NOT_PLAYED;
	game->engine.action_id = ARC_ACTION_RESET;
	if (game->hooks->zero_aux)
		game->hooks->zero_aux(game->aux);
	initial_camera(game);
	arc_game_set_level(game, 0);
	arc_game_perform_action(game, ARC_ACTION_RESET, 0, 0);
}

int32_t arc_game_step(struct arc_game *game, int32_t action, int8_t *frame,
		      int32_t *reward, uint8_t *terminated)
{
	int32_t action_id, x, y;
	arc_game_decode_action(game, action, &action_id, &x, &y);
	int32_t before = game->engine.score;
	int32_t count = perform(game, action_id, x, y, NULL, 0);
	*reward = game->engine.score - before;
	*terminated =
		game->engine.status == WIN || game->engine.status == GAME_OVER;
	if (frame)
		arc_game_frame(game, frame);
	return count;
}

struct arc_state_layout {
	size_t engine;
	size_t slots;
	size_t tags;
	size_t pixels;
	size_t aux;
};

static struct arc_state_layout state_layout(const struct arc_game *game,
					    size_t aux_size)
{
	struct arc_state_layout l;
	int32_t n = game->atlas.num_slots;

	l.engine = sizeof(struct arc_engine_state);
	l.slots = (size_t)n * (6 * sizeof(int32_t) + 2 * sizeof(int8_t) +
			       2 * sizeof(uint8_t) + 4 * sizeof(int32_t));
	l.tags = (size_t)n * (size_t)game->atlas.num_tags;
	l.pixels = (size_t)n * (size_t)game->atlas.ph * (size_t)game->atlas.pw;
	l.aux = aux_size;
	return l;
}

size_t arc_game_state_size(const struct arc_game *game, size_t aux_size)
{
	struct arc_state_layout l = state_layout(game, aux_size);

	return l.engine + l.slots + l.tags + l.pixels + l.aux;
}

static size_t copy_block(void *dst, const void *src, size_t n, int save)
{
	if (save)
		memcpy(dst, src, n);
	else
		memcpy((void *)src, dst, n);
	return n;
}

static void walk_state(struct arc_game *game, size_t aux_size, void *buffer,
		       int save)
{
	const struct arc_sprites *s = &game->sprites;
	int32_t n = game->atlas.num_slots;
	uint8_t *p = (uint8_t *)buffer;

	p += copy_block(p, &game->engine, sizeof(game->engine), save);
	p += copy_block(p, s->x, (size_t)n * sizeof(int32_t), save);
	p += copy_block(p, s->y, (size_t)n * sizeof(int32_t), save);
	p += copy_block(p, s->h, (size_t)n * sizeof(int32_t), save);
	p += copy_block(p, s->w, (size_t)n * sizeof(int32_t), save);
	p += copy_block(p, s->layer, (size_t)n * sizeof(int32_t), save);
	p += copy_block(p, s->order, (size_t)n * sizeof(int32_t), save);
	p += copy_block(p, s->interaction, (size_t)n, save);
	p += copy_block(p, s->blocking, (size_t)n, save);
	p += copy_block(p, s->alive, (size_t)n, save);
	p += copy_block(p, s->overridden, (size_t)n, save);
	p += copy_block(p, s->bbox, (size_t)n * 4 * sizeof(int32_t), save);
	p += copy_block(p, s->tags, (size_t)n * (size_t)game->atlas.num_tags,
			save);
	p += copy_block(p, s->pixels,
			(size_t)n * (size_t)game->atlas.ph *
				(size_t)game->atlas.pw,
			save);
	if (aux_size && game->aux)
		copy_block(p, game->aux, aux_size, save);
}

void arc_game_save(const struct arc_game *game, size_t aux_size, void *dst)
{
	walk_state((struct arc_game *)game, aux_size, dst, 1);
}

void arc_game_load(struct arc_game *game, size_t aux_size, const void *src)
{
	walk_state(game, aux_size, (void *)src, 0);
}

static uint64_t mix(uint64_t h, const void *data, size_t n)
{
	const uint8_t *p = (const uint8_t *)data;

	for (size_t i = 0; i < n; i++) {
		h ^= p[i];
		h *= 1099511628211ULL;
	}
	return h;
}

uint64_t arc_game_hash(const struct arc_game *game, size_t aux_size)
{
	const struct arc_sprites *s = &game->sprites;
	const struct arc_engine_state *e = &game->engine;
	int32_t n = game->atlas.num_slots;
	uint64_t h = 1469598103934665603ULL;

	h = mix(h, &e->level_index, sizeof(e->level_index));
	h = mix(h, &e->score, sizeof(e->score));
	h = mix(h, &e->status, sizeof(e->status));
	h = mix(h, s->x, (size_t)n * sizeof(int32_t));
	h = mix(h, s->y, (size_t)n * sizeof(int32_t));
	h = mix(h, s->h, (size_t)n * sizeof(int32_t));
	h = mix(h, s->w, (size_t)n * sizeof(int32_t));
	h = mix(h, s->layer, (size_t)n * sizeof(int32_t));
	h = mix(h, s->interaction, (size_t)n);
	h = mix(h, s->blocking, (size_t)n);
	h = mix(h, s->alive, (size_t)n);
	h = mix(h, s->tags, (size_t)n * (size_t)game->atlas.num_tags);
	h = mix(h, s->pixels,
		(size_t)n * (size_t)game->atlas.ph * (size_t)game->atlas.pw);
	if (aux_size && game->aux)
		h = mix(h, game->aux, aux_size);
	return h;
}
