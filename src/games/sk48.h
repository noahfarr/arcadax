#ifndef ARC_GAMES_SK48_H
#define ARC_GAMES_SK48_H

#include "arc/engine.h"

enum {
	SK48_MAX_HEADS = 5,
	SK48_MAX_SEGMENTS = 8,
	SK48_MAX_MARKERS = 6,
	SK48_MAX_SHADOWS = 12,
	SK48_STACK_DEPTH = 197,
	SK48_STEP_BUDGET = 196,
	SK48_PITCH = 6,
};

#define SK48_EXTRA_SLOTS                                         \
	(SK48_MAX_HEADS * SK48_MAX_SEGMENTS + SK48_MAX_MARKERS + \
	 SK48_MAX_SHADOWS)
#define SK48_SEGMENT_BASE(st) ((st)->num_slots - SK48_EXTRA_SLOTS)
#define SK48_MARKER_BASE(st) \
	(SK48_SEGMENT_BASE(st) + SK48_MAX_HEADS * SK48_MAX_SEGMENTS)
#define SK48_SHADOW_BASE(st) (SK48_MARKER_BASE(st) + SK48_MAX_MARKERS)

struct sk48_aux {
	int32_t num_slots;

	int32_t selected_head;
	uint8_t sliding;
	uint8_t *slide_mask;
	int32_t slide_dx;
	int32_t slide_dy;
	int32_t retract_removed_slot;
	uint8_t shadow_wait;
	int32_t shadow_count;
	int32_t shadow_blocks[SK48_MAX_SHADOWS];
	uint8_t blink_active;
	int32_t blink_frame;
	uint8_t *blink_mask;
	int8_t *blink_original;
	int32_t steps;
	int32_t stack_top;
	int32_t stack_head_x[SK48_STACK_DEPTH][SK48_MAX_HEADS];
	int32_t stack_head_y[SK48_STACK_DEPTH][SK48_MAX_HEADS];
	int32_t stack_seg_count[SK48_STACK_DEPTH][SK48_MAX_HEADS];
	int32_t *stack_block_x;
	int32_t *stack_block_y;

	uint8_t *push_visited;
};

struct sk48_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ph;
	int32_t pw;
	int32_t tag_rail;
	int32_t tag_click;

	const int32_t *boundary_left;
	const int32_t *boundary_top;
	const int32_t *boundary_right;
	const int32_t *boundary_bottom;

	const int32_t *head_slot;
	const int32_t *head_rotation;
	const int32_t *head_partner;
	const int32_t *head_initial_segments;

	const int32_t *target_color;
	const int32_t *marker_count;
	const int32_t *marker_slot;
	const int32_t *marker_x;
	const int32_t *marker_y;

	int32_t max_block;
	const int32_t *block_slots;
	const int32_t *block_count;

	int32_t max_wall;
	const int32_t *wall_slots;
	const int32_t *wall_count;

	int32_t max_removal;
	const int32_t *removal_slots;
	const int32_t *removal_count;

	const int8_t *segment_template_pixels;
	const int32_t *segment_template_h;
	const int32_t *segment_template_w;

	const int8_t *marker_pixels;
	int32_t marker_h;
	int32_t marker_w;

	const int8_t *shadow_template_pixels;
	const int32_t *shadow_template_h;
	const int32_t *shadow_template_w;
};

void sk48_aux_alloc(struct sk48_aux *aux, int32_t num_slots);
void sk48_aux_free(struct sk48_aux *aux);

void sk48_zero_aux(struct sk48_aux *aux);

void sk48_on_set_level(struct arc_sprites *sprites,
		       const struct sk48_static *st, int32_t level,
		       struct sk48_aux *aux);

void sk48_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct sk48_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct sk48_aux *aux, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete);

void sk48_render_interface(int8_t *frame, const struct sk48_aux *aux);

#endif
