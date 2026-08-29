#include "scene.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    SceneAtlas atlas;
    SceneTable scene;
    SceneScratch scratch;
} SceneWorld;

SceneWorld *scene_world_new(int32_t n, int32_t atlas_size, int32_t ph, int32_t pw,
                            const int8_t *atlas_pixels) {
    SceneWorld *world = calloc(1, sizeof(SceneWorld));
    world->atlas.pixels = atlas_pixels;
    world->atlas.size = atlas_size;
    world->atlas.ph = ph;
    world->atlas.pw = pw;
    scene_table_alloc(&world->scene, n);
    scene_scratch_init(&world->scratch, &world->atlas, n);
    return world;
}

void scene_world_free(SceneWorld *world) {
    scene_table_free(&world->scene);
    scene_scratch_free(&world->scratch);
    free(world);
}

void scene_world_set(SceneWorld *world, const int32_t *image, const int32_t *x,
                     const int32_t *y, const int32_t *layer, const int32_t *order) {
    int32_t n = world->scene.n;
    memcpy(world->scene.image, image, sizeof(int32_t) * (size_t)n);
    memcpy(world->scene.x, x, sizeof(int32_t) * (size_t)n);
    memcpy(world->scene.y, y, sizeof(int32_t) * (size_t)n);
    memcpy(world->scene.layer, layer, sizeof(int32_t) * (size_t)n);
    memcpy(world->scene.order, order, sizeof(int32_t) * (size_t)n);
}

void scene_world_composite(SceneWorld *world, int8_t background, int8_t *out) {
    scene_composite(&world->scene, &world->atlas, &world->scratch, background, out);
}

void scene_world_set_frame(SceneWorld *world, const int8_t *frame, uint8_t dirty) {
    memcpy(world->scene.frame, frame, (size_t)FRAME_SIZE * FRAME_SIZE);
    world->scene.dirty = dirty;
}

void scene_world_display(SceneWorld *world, int8_t background, int8_t *out,
                         uint8_t *dirty_out) {
    scene_display(&world->scene, &world->atlas, &world->scratch, background);
    memcpy(out, world->scene.frame, (size_t)FRAME_SIZE * FRAME_SIZE);
    *dirty_out = world->scene.dirty;
}
