#include "engine.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Atlas atlas;
    Sprites sprites;
    RenderScratch scratch;
} World;

World *world_new(int32_t num_slots, int32_t num_tags, int32_t ph, int32_t pw,
                 const int8_t *pixels, const uint8_t *tags) {
    World *world = calloc(1, sizeof(World));
    world->atlas.pixels = pixels;
    world->atlas.num_slots = num_slots;
    world->atlas.num_tags = num_tags;
    world->atlas.ph = ph;
    world->atlas.pw = pw;

    Sprites *s = &world->sprites;
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
    if (tags) memcpy(s->tags, tags, (size_t)num_slots * num_tags);
    s->pixels = calloc((size_t)num_slots * ph * pw, 1);
    s->overridden = calloc(num_slots, 1);
    s->bbox = calloc((size_t)num_slots * 4, sizeof(int32_t));
    sprites_recompute_bbox(s);
    render_scratch_init(&world->scratch, &world->atlas);
    return world;
}

void world_free(World *world) {
    Sprites *s = &world->sprites;
    free(s->x); free(s->y); free(s->h); free(s->w);
    free(s->layer); free(s->order);
    free(s->interaction); free(s->blocking); free(s->alive); free(s->tags);
    free(s->pixels); free(s->overridden); free(s->bbox);
    render_scratch_free(&world->scratch);
    free(world);
}

void world_set(World *world, const int32_t *x, const int32_t *y,
               const int32_t *h, const int32_t *w,
               const int32_t *layer, const int32_t *order,
               const int8_t *interaction, const int8_t *blocking,
               const uint8_t *alive) {
    Sprites *s = &world->sprites;
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
    sprites_recompute_bbox(s);
}

void world_render(World *world, int32_t cx, int32_t cy, int32_t cw, int32_t ch,
                  int8_t background, int8_t letter_box, int8_t *out) {
    Camera cam = {cx, cy, cw, ch, background, letter_box};
    render(&world->sprites, &cam, &world->scratch, out);
}

int32_t world_get_sprite_at(World *world, int32_t x, int32_t y, int32_t tag,
                            int ignore_collidable) {
    return get_sprite_at(&world->sprites, x, y, tag, ignore_collidable);
}

int world_collides(World *world, int32_t i, int ignore_mode) {
    return collides(&world->sprites, i, ignore_mode);
}

void world_set_pixels(World *world, const int8_t *pixels) {
    Sprites *s = &world->sprites;
    const Atlas *a = &world->atlas;
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
    sprites_recompute_bbox(s);
}

double world_bench_render(World *world, int32_t cx, int32_t cy, int32_t cw,
                          int32_t ch, int8_t background, int8_t letter_box,
                          int32_t iters) {
    Camera cam = {cx, cy, cw, ch, background, letter_box};
    int8_t frame[FRAME_SIZE * FRAME_SIZE];
    volatile int8_t sink = 0;
    for (int32_t k = 0; k < iters; k++) {
        render(&world->sprites, &cam, &world->scratch, frame);
        sink ^= frame[k % (FRAME_SIZE * FRAME_SIZE)];
    }
    return (double)sink;
}
