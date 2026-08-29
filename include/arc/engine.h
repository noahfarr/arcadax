#ifndef ARC_ENGINE_H
#define ARC_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ARC_FRAME_SIZE 64
#define ARC_ORDER_BITS 4096

enum { TANGIBLE = 0, INTANGIBLE = 1, INVISIBLE = 2, REMOVED = 3 };
enum { NOT_BLOCKED = 0, BOUNDING_BOX = 1, PIXEL_PERFECT = 2 };

typedef struct {
    const int8_t *pixels;
    int32_t num_slots;
    int32_t num_tags;
    int32_t ph;
    int32_t pw;
} ArcAtlas;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int8_t background;
    int8_t letter_box;
} ArcCamera;

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
    const ArcAtlas *atlas;
    int32_t *bbox;
} ArcSprites;

typedef struct {
    int32_t *canvas_keys;
    int8_t *canvas;
    int32_t *sorted;
    int32_t canvas_h;
    int32_t canvas_w;
} ArcRenderScratch;

const int8_t *arc_sprite_pixels(const ArcSprites *s, int32_t i);
int8_t *arc_sprite_pixels_mut(ArcSprites *s, int32_t i);
int arc_collides_pair(const ArcSprites *s, int32_t i, int32_t j, int ignore_mode);
int arc_sprite_visible(const ArcSprites *s, int32_t i);
int arc_sprite_collidable(const ArcSprites *s, int32_t i);

void arc_render_scratch_init(ArcRenderScratch *scratch, const ArcAtlas *atlas);
void arc_render_scratch_free(ArcRenderScratch *scratch);
void arc_sprites_recompute_bbox(ArcSprites *s);

void arc_raw_render(const ArcSprites *s, const ArcCamera *cam, ArcRenderScratch *scratch,
                int8_t *view);
void arc_render(const ArcSprites *s, const ArcCamera *cam, ArcRenderScratch *scratch,
            int8_t *frame);
void arc_scale_and_offset(const ArcCamera *cam, int32_t *scale, int32_t *x_offset,
                      int32_t *y_offset);

int32_t arc_get_sprite_at(const ArcSprites *s, int32_t x, int32_t y, int32_t tag,
                      int ignore_collidable);
int arc_collides(const ArcSprites *s, int32_t i, int ignore_mode);
int arc_try_move(ArcSprites *s, int32_t i, int32_t dx, int32_t dy);

void arc_set_position(ArcSprites *s, int32_t i, int32_t x, int32_t y);
void arc_move_sprite(ArcSprites *s, int32_t i, int32_t dx, int32_t dy);
void arc_set_interaction(ArcSprites *s, int32_t i, int8_t mode);
void arc_set_visible(ArcSprites *s, int32_t i, int visible);
void arc_color_remap(ArcSprites *s, int32_t i, int has_old, int8_t old, int8_t neu);
void arc_remove_sprite(ArcSprites *s, int32_t i);
void arc_add_sprite(ArcSprites *s, int32_t i, int32_t order);

#ifdef __cplusplus
}
#endif

#endif
