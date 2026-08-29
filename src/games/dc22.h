#ifndef ARC_GAMES_DC22_H
#define ARC_GAMES_DC22_H

#include "arc/engine.h"

#define DC22_NUM_DIRS 5

struct dc22_aux {
	int32_t steps;
	int32_t crane_col;
	int32_t crane_row;
	int32_t attach_kind;
	int32_t attach_slot;
	uint8_t death_active;
	int32_t death_frame;
	uint8_t shake_active;
	int32_t shake_frame;
	int32_t shake_dx;
	int32_t shake_dy;
	int32_t shake_origin_x;
	int32_t shake_origin_y;
	uint8_t flicker_active;
	int32_t flicker_frame;
	uint8_t undo_valid;
	int8_t *undo_pixels;
	int32_t *undo_x;
	int32_t *undo_y;
	int8_t *undo_interaction;
	uint8_t *undo_alive;
	int32_t undo_crane_col;
	int32_t undo_crane_row;
	int32_t undo_attach_kind;
	int32_t undo_attach_slot;
};

struct dc22_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t num_codes;
	int32_t patch_h;
	int32_t patch_w;

	const int32_t *budget;
	const int32_t *player_slot;
	const int32_t *goal_slot;
	const int32_t *crane_slot;
	const uint8_t *crane_is_brixto;
	const int32_t *crane_origin_x;
	const int32_t *crane_origin_y;
	const int32_t *crane_offset_x;
	const int32_t *crane_offset_y;
	const int32_t *grawwq_object_slot;
	const int32_t *dir_slot;

	const uint8_t *vcha_map;

	const uint8_t *is_ignore;
	const uint8_t *is_crzsjq;
	const uint8_t *is_vcha;
	const uint8_t *is_tewfut;
	const uint8_t *is_qiukbrokfa;
	const uint8_t *is_iophjflwsn;

	const int32_t *code_of;
	const int32_t *next_slot;
	const int32_t *next_interaction;
	const int32_t *name_id;
	const int32_t *teleport_name;

	const int8_t *crane_hold_pixels;

	int32_t max_buezna;
	const int32_t *buezna_slots;
	const int32_t *buezna_count;

	int32_t max_sys_click;
	const int32_t *sys_click_slots;
	const int32_t *sys_click_count;

	int32_t max_piyqze;
	const int32_t *piyqze_slots;
	const int32_t *piyqze_count;

	int32_t max_aybe;
	const int32_t *aybe_slots;
	const int32_t *aybe_count;

	int32_t max_njvd;
	const int32_t *njvd_slots;
	const int32_t *njvd_count;

	int32_t max_governed;
	const int32_t *governed_slots;
	const int32_t *governed_count;

	int32_t max_brixto;
	const int32_t *brixto_slots;
	const int32_t *brixto_count;

	int32_t max_code_member;
	const int32_t *code_member_slots;
	const int32_t *code_member_count;
};

void dc22_zero_aux(struct dc22_aux *aux, const struct dc22_static *st);

void dc22_on_set_level(const struct dc22_static *st, int32_t level,
		       struct dc22_aux *aux);

void dc22_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct dc22_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    struct dc22_aux *aux, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void dc22_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct dc22_static *st, int32_t level,
			   const struct dc22_aux *aux);

#endif
