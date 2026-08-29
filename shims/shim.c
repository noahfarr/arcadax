#include "arc/engine.h"
#include <stdlib.h>
#include <string.h>

struct arc_world {
	struct arc_atlas atlas;
	struct arc_sprites sprites;
	struct arc_render_scratch scratch;
};

struct arc_world *arc_world_new(int32_t num_slots, int32_t num_tags, int32_t ph,
				int32_t pw, const int8_t *pixels,
				const uint8_t *tags)
{
	struct arc_world *world = calloc(1, sizeof(struct arc_world));
	world->atlas.pixels = pixels;
	world->atlas.num_slots = num_slots;
	world->atlas.num_tags = num_tags;
	world->atlas.ph = ph;
	world->atlas.pw = pw;

	struct arc_sprites *s = &world->sprites;
	s->atlas = &world->atlas;
	s->x = calloc(num_slots, sizeof(int32_t));
	s->y = calloc(num_slots, sizeof(int32_t));
	s->h = calloc(num_slots, sizeof(int32_t));
	s->w = calloc(num_slots, sizeof(int32_t));
	s->layer = calloc(num_slots, sizeof(int32_t));
	s->order = calloc(num_slots, sizeof(int32_t));
	s->interaction = calloc(num_slots, 1);
	s->blocking = calloc(num_slots, 1);
	s->alive = calloc(num_slots, 1);
	s->tags = calloc((size_t)num_slots * num_tags, 1);
	if (tags)
		memcpy(s->tags, tags, (size_t)num_slots * num_tags);
	s->pixels = calloc((size_t)num_slots * ph * pw, 1);
	s->overridden = calloc(num_slots, 1);
	s->bbox = calloc((size_t)num_slots * 4, sizeof(int32_t));
	arc_sprites_recompute_bbox(s);
	arc_render_scratch_init(&world->scratch, &world->atlas);
	return world;
}

void arc_world_free(struct arc_world *world)
{
	struct arc_sprites *s = &world->sprites;
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
	free(s->bbox);
	arc_render_scratch_free(&world->scratch);
	free(world);
}

void arc_world_set(struct arc_world *world, const int32_t *x, const int32_t *y,
		   const int32_t *h, const int32_t *w, const int32_t *layer,
		   const int32_t *order, const int8_t *interaction,
		   const int8_t *blocking, const uint8_t *alive)
{
	struct arc_sprites *s = &world->sprites;
	int32_t n = world->atlas.num_slots;
	memcpy(s->x, x, sizeof(int32_t) * n);
	memcpy(s->y, y, sizeof(int32_t) * n);
	memcpy(s->h, h, sizeof(int32_t) * n);
	memcpy(s->w, w, sizeof(int32_t) * n);
	memcpy(s->layer, layer, sizeof(int32_t) * n);
	memcpy(s->order, order, sizeof(int32_t) * n);
	memcpy(s->interaction, interaction, n);
	memcpy(s->blocking, blocking, n);
	memcpy(s->alive, alive, n);
	memset(s->overridden, 0, n);
	arc_sprites_recompute_bbox(s);
}

void arc_world_render(struct arc_world *world, int32_t cx, int32_t cy,
		      int32_t cw, int32_t ch, int8_t background,
		      int8_t letter_box, int8_t *out)
{
	struct arc_camera cam = { cx, cy, cw, ch, background, letter_box };
	arc_render(&world->sprites, &cam, &world->scratch, out);
}

int32_t arc_world_get_sprite_at(struct arc_world *world, int32_t x, int32_t y,
				int32_t tag, int ignore_collidable)
{
	return arc_get_sprite_at(&world->sprites, x, y, tag, ignore_collidable);
}

int arc_world_collides(struct arc_world *world, int32_t i, int ignore_mode)
{
	return arc_collides(&world->sprites, i, ignore_mode);
}

void arc_world_set_pixels(struct arc_world *world, const int8_t *pixels)
{
	struct arc_sprites *s = &world->sprites;
	const struct arc_atlas *a = &world->atlas;
	size_t area = (size_t)a->ph * a->pw;
	for (int32_t i = 0; i < a->num_slots; i++) {
		const int8_t *src = pixels + (size_t)i * area;
		if (memcmp(src, a->pixels + (size_t)i * area, area) != 0) {
			memcpy(s->pixels + (size_t)i * area, src, area);
			s->overridden[i] = 1;
		} else {
			s->overridden[i] = 0;
		}
	}
	arc_sprites_recompute_bbox(s);
}

double arc_world_bench_render(struct arc_world *world, int32_t cx, int32_t cy,
			      int32_t cw, int32_t ch, int8_t background,
			      int8_t letter_box, int32_t iters)
{
	struct arc_camera cam = { cx, cy, cw, ch, background, letter_box };
	int8_t frame[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	volatile int8_t sink = 0;
	for (int32_t k = 0; k < iters; k++) {
		arc_render(&world->sprites, &cam, &world->scratch, frame);
		sink ^= frame[k % (ARC_FRAME_SIZE * ARC_FRAME_SIZE)];
	}
	return (double)sink;
}
