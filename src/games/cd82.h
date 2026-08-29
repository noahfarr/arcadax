#ifndef ARC_GAMES_CD82_H
#define ARC_GAMES_CD82_H

#include "arc/engine.h"

struct cd82_aux {
	int32_t position;
	int32_t color;
	uint8_t drawing;
	uint8_t draw_forward;
	int32_t draw_counter;
	uint8_t draw_painted;
	uint8_t bouncing;
	uint8_t bounce_forward;
	int32_t bounce_counter;
	uint8_t bounce_painted;
	int32_t bounce_direction;
};

struct cd82_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t basket_slot;
	int32_t arrow_slot;
	int32_t ph;
	int32_t pw;
	int32_t max_removal;
	const int32_t *removal_slots;
	const int32_t *removal_count;
	const uint8_t *palette_mask;
	const int32_t *canvas_slot;
	const int32_t *answer_slot;
	const int32_t *marker_slot;
	const uint8_t *level_has_arrow;
	const int8_t *basket_pixels;
	const uint8_t *basket_is15;
	const int32_t *basket_h;
	const int32_t *basket_w;
	const int8_t *arrow_pixels;
	const uint8_t *arrow_is15;
	const int32_t *arrow_h;
	const int32_t *arrow_w;
};

void cd82_zero_aux(struct cd82_aux *aux);

void cd82_on_set_level(struct arc_sprites *sprites,
		       const struct cd82_static *st, int32_t level,
		       struct cd82_aux *aux, int32_t *next_order);

void cd82_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct cd82_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct cd82_aux *aux,
		    int32_t *next_order, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void cd82_render_interface(int8_t *frame, int32_t action_count);

#endif
