#ifndef ARC_GAMES_TN36_H
#define ARC_GAMES_TN36_H

#include "arc/engine.h"

enum {
	TN36_MAX_COLS = 6,
	TN36_MAX_BITS = 6,
	TN36_MAX_WALLS = 5,
	TN36_MAX_CHECKPOINTS = 4,
	TN36_MAX_HAZARDS = 2,
	TN36_MAX_BUTTONS = 5
};

struct tn36_aux {
	int32_t active_lane;
	int32_t oocupkguhu[2];
	int32_t instr[2][TN36_MAX_COLS];
	int32_t rotation[2];
	int32_t scale[2];
	int8_t pattern[2][4][4];
	uint8_t dead[2];
	int32_t saved_x[2];
	int32_t saved_y[2];
	int32_t saved_rotation[2];
	int32_t saved_scale[2];
	int8_t saved_pattern[2][4][4];
	uint8_t reset_enabled[2];
	uint8_t switch_flash;
	uint8_t final_flash;
	uint8_t win_pending;
	uint8_t lose_pending;
	int32_t scroll_tick;
	int32_t selected_button;
};

struct tn36_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ph;
	int32_t pw;

	const int32_t *robot_slot;
	const int32_t *container_box;
	const int32_t *target_slot;
	const int32_t *grid_box;
	const uint8_t *view_only;
	const int32_t *switch_slot;
	const int32_t *switch_box;
	const int32_t *grsysj_box;

	const int32_t *num_cols;
	const int32_t *col_slot;
	const int32_t *num_bits;
	const int32_t *cell_slot;
	const int32_t *cell_state_pixel;

	const int32_t *num_walls;
	const int32_t *wall_box;
	const int32_t *num_checkpoints;
	const int32_t *checkpoint_slot;
	const int32_t *num_hazards;
	const int32_t *hazard_icon;
	const int32_t *hazard_trig;

	const int32_t *num_buttons;
	const int32_t *button_slot;
	const int32_t *button_box;
	const int32_t *button_pos;
	const int32_t *button_rot;
	const int32_t *button_scale;
	const uint8_t *button_reset;
	const int32_t *button_program;
	const uint8_t *has_programs;

	const uint8_t *robot_mask;
	const int32_t *robot_rotation0;
	const int32_t *robot_scale0;
	const int32_t *target_rotation;
	const int32_t *target_scale;
	const int32_t *robot_color0;
	const int32_t *target_color;
	const int32_t *target_touch_x;
	const int32_t *target_touch_y;
	const int32_t *switch_color0;
	const int8_t *clean_target_pixels;
	const uint8_t *bbox_override;

	const int32_t *scroll_slot;
	const int32_t *scroll_bg_x;
	const int32_t *scroll_w;
};

void tn36_zero_aux(struct tn36_aux *aux);

void tn36_on_set_level(struct arc_sprites *sprites,
		       const struct tn36_static *st, int32_t level,
		       struct tn36_aux *aux);

void tn36_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct tn36_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct tn36_aux *aux,
		    int32_t *next_order, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void tn36_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct tn36_static *st, int32_t level,
			   const struct tn36_aux *aux);

#endif
