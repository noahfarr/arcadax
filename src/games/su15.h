#ifndef ARC_GAMES_SU15_H
#define ARC_GAMES_SU15_H

#include "arc/engine.h"

#define SU15_MAX_BLOBS 9
#define SU15_MAX_FRUITS 4
#define SU15_MAX_ZONES 4
#define SU15_MAX_WIN_TERMS 3
#define SU15_MAX_UNDO 64
#define SU15_NUM_TIERS 9
#define SU15_NUM_FRUIT_KINDS 3
#define SU15_GLOW_SIZE 25
#define SU15_GLOW_STATES 5
#define SU15_DELTA_BOUND 90
#define SU15_DELTA_SIZE 181

struct su15_aux {
	int32_t num_slots;

	int32_t phase;
	int32_t pull_frame;
	uint8_t pull_confirmed;
	int32_t click_x;
	int32_t click_y;
	int32_t win_flash_frame;
	uint8_t not_yet_flash;
	int32_t flash_pair_a;
	int32_t flash_pair_b;
	uint8_t tutorial_hidden;
	int32_t steps_remaining;
	int32_t step_penalty;
	int32_t undo_top;

	int32_t *tier;
	int32_t *fruit_kind;
	uint8_t *pulled;
	int32_t *pull_dx;
	int32_t *pull_dy;
	uint8_t *has_pull_vector;
	int32_t *pull_anchor_x;
	int32_t *pull_anchor_y;
	uint8_t *near_click;
	uint8_t *consuming;
	int32_t *consume_frame;
	int32_t *consume_start_x;
	int32_t *consume_start_y;
	int32_t *consume_dx;
	int32_t *consume_dy;
	int32_t *consume_pos_x;
	int32_t *consume_pos_y;
	uint8_t *consume_dying;
	uint8_t *demoted;
	int32_t *demoted_tier;
	int32_t *eat_lock;

	int32_t *undo_tier;
	int32_t *undo_fruit_kind;
	int32_t *undo_x;
	int32_t *undo_y;
	uint8_t *undo_alive;
	int32_t *undo_order;
};

struct su15_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ph;
	int32_t pw;
	int32_t glow_slot;

	const int32_t *blob_slot;
	const int32_t *fruit_slot;
	const int32_t *zone_a_slot;
	const int32_t *zone_b_slot;
	const int32_t *tutorial_slot;
	const int32_t *first_blob_slot;

	const int32_t *tier_of_slot;
	const int32_t *fruit_kind_of_slot;

	const int32_t *steps_budget;
	const int32_t *win_type;
	const int32_t *win_value;
	const int32_t *win_count;

	const int8_t *tier_pixels;
	const int32_t *tier_h;
	const int32_t *tier_w;
	const int32_t *tier_layer;
	const int8_t *fruit_pixels;
	const int32_t *fruit_h;
	const int32_t *fruit_w;
	const int32_t *fruit_layer;

	const int32_t *pull_floor_x;
	const int32_t *pull_round_x;
	const uint8_t *pull_tie_x;
	const int32_t *pull_floor_y;
	const int32_t *pull_round_y;
	const uint8_t *pull_tie_y;

	const int32_t *swallow_floor_x;
	const int32_t *swallow_round_x;
	const uint8_t *swallow_tie_x;
	const int32_t *swallow_floor_y;
	const int32_t *swallow_round_y;
	const uint8_t *swallow_tie_y;

	const int8_t *glow_states;

	const int8_t *level_pixels;
};

void su15_aux_alloc(struct su15_aux *aux, int32_t num_slots);
void su15_aux_free(struct su15_aux *aux);

void su15_zero_aux(struct su15_aux *aux);

void su15_on_set_level(struct arc_sprites *sprites,
		       const struct su15_static *st, int32_t level,
		       struct su15_aux *aux, int32_t *next_order);

void su15_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct su15_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    struct su15_aux *aux, int32_t *next_order, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete);

void su15_render_interface(int8_t *frame, const struct su15_static *st,
			   int32_t level, const struct su15_aux *aux);

#endif
