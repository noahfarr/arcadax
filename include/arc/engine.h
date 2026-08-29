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

struct arc_atlas {
	const int8_t *pixels;
	int32_t num_slots;
	int32_t num_tags;
	int32_t ph;
	int32_t pw;
};

struct arc_camera {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
	int8_t background;
	int8_t letter_box;
};

struct arc_sprites {
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
	uint8_t *solid;
	const struct arc_atlas *atlas;
	int32_t *bbox;
};

struct arc_render_scratch {
	int32_t *canvas_keys;
	int8_t *canvas;
	int32_t *sorted;
	int32_t canvas_h;
	int32_t canvas_w;
};

const int8_t *arc_sprite_pixels(const struct arc_sprites *s, int32_t i);
int8_t *arc_sprite_pixels_mut(struct arc_sprites *s, int32_t i);
int arc_collides_pair(const struct arc_sprites *s, int32_t i, int32_t j,
		      int ignore_mode);
int arc_sprite_visible(const struct arc_sprites *s, int32_t i);
int arc_sprite_collidable(const struct arc_sprites *s, int32_t i);

void arc_render_scratch_init_dims(struct arc_render_scratch *scratch,
				  int32_t ph, int32_t pw, int32_t num_slots);
void arc_render_scratch_init(struct arc_render_scratch *scratch,
			     const struct arc_atlas *atlas);
void arc_render_scratch_free(struct arc_render_scratch *scratch);
void arc_sprites_recompute_bbox(struct arc_sprites *s);

void arc_raw_render(const struct arc_sprites *s, const struct arc_camera *cam,
		    struct arc_render_scratch *scratch, int8_t *view);
void arc_render(const struct arc_sprites *s, const struct arc_camera *cam,
		struct arc_render_scratch *scratch, int8_t *frame);
void arc_scale_and_offset(const struct arc_camera *cam, int32_t *scale,
			  int32_t *x_offset, int32_t *y_offset);

int32_t arc_get_sprite_at(const struct arc_sprites *s, int32_t x, int32_t y,
			  int32_t tag, int ignore_collidable);
int arc_collides(const struct arc_sprites *s, int32_t i, int ignore_mode);
int arc_try_move(struct arc_sprites *s, int32_t i, int32_t dx, int32_t dy);

void arc_set_position(struct arc_sprites *s, int32_t i, int32_t x, int32_t y);
void arc_move_sprite(struct arc_sprites *s, int32_t i, int32_t dx, int32_t dy);
void arc_set_interaction(struct arc_sprites *s, int32_t i, int8_t mode);
void arc_set_visible(struct arc_sprites *s, int32_t i, int visible);
void arc_color_remap(struct arc_sprites *s, int32_t i, int has_old, int8_t old,
		     int8_t neu);
void arc_remove_sprite(struct arc_sprites *s, int32_t i);
void arc_add_sprite(struct arc_sprites *s, int32_t i, int32_t order);

#ifdef __cplusplus
}
#endif

#endif
