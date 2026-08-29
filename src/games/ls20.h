#ifndef ARC_GAMES_LS20_H
#define ARC_GAMES_LS20_H

#include "arc/engine.h"

#define LS20_MAX_GOALS 2
#define LS20_MAX_PATROLS 3
#define LS20_PUSH_FRAMES 16

struct ls20_aux {
	int32_t lives;
	int32_t steps;
	int32_t shape_idx;
	int32_t color_idx;
	int32_t rot_idx;
	uint8_t goal_done[LS20_MAX_GOALS];
	int32_t push_slot;
	int32_t push_frame;
	int32_t death_frame;
	int32_t flash_frame;
	int32_t patrol_dir[LS20_MAX_PATROLS];
	int32_t *hit_slots;
};

struct ls20_static {
	int32_t num_levels;
	int32_t num_slots;

	const int32_t *player_slot;
	const int32_t *htk_slot;
	const int32_t *htk2_slot;
	const int32_t *hint_glow_slot;
	int32_t flash_slot;

	const int32_t *pitch_x;
	const int32_t *pitch_y;
	const int32_t *player_start_x;
	const int32_t *player_start_y;

	const int32_t *budget;
	const int32_t *decrement;
	const uint8_t *fog;

	const int32_t *start_shape;
	const int32_t *start_color;
	const int32_t *start_rot;

	const int8_t *htk_variant;

	const int32_t *tag_code;
	const int32_t *goal_index;

	int32_t max_restorable;
	const int32_t *restorable_slots;
	const int32_t *restorable_count;

	const int32_t *goal_slot;
	const int32_t *marker_slot;
	const int32_t *ring_slot;
	const int32_t *frame_slot;
	const uint8_t *goal_is_final;
	const int32_t *want_shape;
	const int32_t *want_color;
	const int32_t *want_rot;
	const int32_t *goal_x;
	const int32_t *goal_y;
	const int32_t *num_goals;

	const int32_t *patrol_slot;
	const int32_t *patrol_area_slot;
	const int32_t *patrol_start_x;
	const int32_t *patrol_start_y;

	int32_t max_pushable;
	const int32_t *pushable_slots;
	const int32_t *pushable_count;
	const int32_t *wall_step_dx;
	const int32_t *wall_step_dy;

	int32_t hint_ring_tag;
	int32_t goal_frame_tag;
};

void ls20_aux_alloc(struct ls20_aux *aux, int32_t num_slots);
void ls20_aux_free(struct ls20_aux *aux);

void ls20_zero_aux(struct ls20_aux *aux);

void ls20_on_set_level(struct arc_sprites *sprites,
		       const struct ls20_static *st, int32_t level,
		       struct ls20_aux *aux, int32_t *next_order);

void ls20_step_once(struct arc_sprites *sprites, const struct ls20_static *st,
		    int32_t level, int32_t action_id, struct ls20_aux *aux,
		    int32_t *next_order, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void ls20_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct ls20_static *st, int32_t level,
			   const struct ls20_aux *aux);

#endif
