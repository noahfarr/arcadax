#include "arc/scene.h"

#include <stdlib.h>
#include <string.h>

struct arc_scene_world {
	struct arc_scene_atlas atlas;
	struct arc_scene_table scene;
	struct arc_scene_scratch scratch;
};

struct arc_scene_world *arc_scene_world_new(int32_t n, int32_t atlas_size,
					    int32_t ph, int32_t pw,
					    const int8_t *atlas_pixels)
{
	struct arc_scene_world *world =
		calloc(1, sizeof(struct arc_scene_world));
	world->atlas.pixels = atlas_pixels;
	world->atlas.size = atlas_size;
	world->atlas.ph = ph;
	world->atlas.pw = pw;
	arc_scene_table_alloc(&world->scene, n);
	arc_scene_scratch_init(&world->scratch, &world->atlas, n);
	return world;
}

void arc_scene_world_free(struct arc_scene_world *world)
{
	arc_scene_table_free(&world->scene);
	arc_scene_scratch_free(&world->scratch);
	free(world);
}

void arc_scene_world_set(struct arc_scene_world *world, const int32_t *image,
			 const int32_t *x, const int32_t *y,
			 const int32_t *layer, const int32_t *order)
{
	int32_t n = world->scene.n;
	memcpy(world->scene.image, image, sizeof(int32_t) * (size_t)n);
	memcpy(world->scene.x, x, sizeof(int32_t) * (size_t)n);
	memcpy(world->scene.y, y, sizeof(int32_t) * (size_t)n);
	memcpy(world->scene.layer, layer, sizeof(int32_t) * (size_t)n);
	memcpy(world->scene.order, order, sizeof(int32_t) * (size_t)n);
}

void arc_scene_world_composite(struct arc_scene_world *world, int8_t background,
			       int8_t *out)
{
	arc_scene_composite(&world->scene, &world->atlas, &world->scratch,
			    background, out);
}

void arc_scene_world_set_frame(struct arc_scene_world *world,
			       const int8_t *frame, uint8_t dirty)
{
	memcpy(world->scene.frame, frame,
	       (size_t)ARC_FRAME_SIZE * ARC_FRAME_SIZE);
	world->scene.dirty = dirty;
}

void arc_scene_world_display(struct arc_scene_world *world, int8_t background,
			     int8_t *out, uint8_t *dirty_out)
{
	arc_scene_display(&world->scene, &world->atlas, &world->scratch,
			  background);
	memcpy(out, world->scene.frame,
	       (size_t)ARC_FRAME_SIZE * ARC_FRAME_SIZE);
	*dirty_out = world->scene.dirty;
}
