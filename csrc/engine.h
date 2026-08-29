#ifndef ARCADAX_ENGINE_H
#define ARCADAX_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FRAME_SIZE 64
#define ORDER_BITS 4096

enum { TANGIBLE = 0, INTANGIBLE = 1, INVISIBLE = 2, REMOVED = 3 };
enum { NOT_BLOCKED = 0, BOUNDING_BOX = 1, PIXEL_PERFECT = 2 };

typedef struct {
    const int8_t *pixels;
    int32_t num_slots;
    int32_t num_tags;
    int32_t ph;
    int32_t pw;
} Atlas;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int8_t background;
    int8_t letter_box;
} Camera;

typedef struct {
    int32_t *x;
    int32_t *y;
    int32_t *h;
    int32_t *w;
    int32_t *layer;
    int32_t *order;
    int8_t *interaction;
    int8_t *blocking;
    uint8_t *alive;
    uint8_t *tags;
    int8_t *pixels;
    uint8_t *overridden;
    const Atlas *atlas;
    int32_t *bbox;
} Sprites;

typedef struct {
    int32_t *canvas_keys;
    int8_t *canvas;
    int32_t *sorted;
    int32_t canvas_h;
    int32_t canvas_w;
} RenderScratch;

const int8_t *sprite_pixels(const Sprites *s, int32_t i);
int8_t *sprite_pixels_mut(Sprites *s, int32_t i);
int collides_pair(const Sprites *s, int32_t i, int32_t j, int ignore_mode);
int sprite_visible(const Sprites *s, int32_t i);
int sprite_collidable(const Sprites *s, int32_t i);

void render_scratch_init(RenderScratch *scratch, const Atlas *atlas);
void render_scratch_free(RenderScratch *scratch);
void sprites_recompute_bbox(Sprites *s);

void raw_render(const Sprites *s, const Camera *cam, RenderScratch *scratch,
                int8_t *view);
void render(const Sprites *s, const Camera *cam, RenderScratch *scratch,
            int8_t *frame);
void scale_and_offset(const Camera *cam, int32_t *scale, int32_t *x_offset,
                      int32_t *y_offset);

int32_t get_sprite_at(const Sprites *s, int32_t x, int32_t y, int32_t tag,
                      int ignore_collidable);
int collides(const Sprites *s, int32_t i, int ignore_mode);
int try_move(Sprites *s, int32_t i, int32_t dx, int32_t dy);

void set_position(Sprites *s, int32_t i, int32_t x, int32_t y);
void move_sprite(Sprites *s, int32_t i, int32_t dx, int32_t dy);
void set_interaction(Sprites *s, int32_t i, int8_t mode);
void set_visible(Sprites *s, int32_t i, int visible);
void color_remap(Sprites *s, int32_t i, int has_old, int8_t old, int8_t neu);
void remove_sprite(Sprites *s, int32_t i);
void add_sprite(Sprites *s, int32_t i, int32_t order);

#ifdef __cplusplus
}
#endif

#endif
