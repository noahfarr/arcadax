#ifndef ARC_GAMES_SC25_H
#define ARC_GAMES_SC25_H

#include "arc/engine.h"

struct sc25_aux {
	uint8_t drawn[3][3];
	int32_t hint;
	uint8_t scale2;
	int8_t player_base[2][2];
	int32_t facing;
	uint8_t facing_set;
	int32_t steps;
	uint8_t demo0_pending;
	int32_t tp_idx[2];

	uint8_t slide_active;
	int32_t slide_dx;
	int32_t slide_dy;
	int32_t slide_frame;

	uint8_t miss_active;
	int32_t miss_frame;

	uint8_t demo_active;
	int32_t demo_frame;
	int32_t demo_spell;

	uint8_t cast_active;
	int32_t cast_frame;
	int32_t cast_spell;

	uint8_t tp_active;
	int32_t tp_frame;
	int32_t tp_target_x;
	int32_t tp_target_y;
	int32_t tp_target_slot;
	int8_t tp_player_saved[2][2];

	uint8_t resize_active;
	int32_t resize_frame;
	uint8_t resize_target2;
	uint8_t resize_blocked;
	int32_t resize_offset_x;
	int32_t resize_offset_y;

	uint8_t fb_active;
	int32_t fb_frame;
	int32_t fb_dx;
	int32_t fb_dy;
	int32_t fb_max_dist;
	int32_t fb_cur_dist;
	uint8_t fb_shrinking;
	int32_t fb_consumed;
	int32_t fb_hit_kind;
	int32_t fb_scale_idx;
	int32_t fb_rot_k;

	int32_t grid_bbox[9][4];
};

struct sc25_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t fireball_slot;
	int32_t max_blockers;
	int32_t max_tp;
	int32_t bar_ph;
	int32_t fb_base_h;
	int32_t fb_base_w;

	const uint8_t *allowed;
	const int32_t *budget;
	const int32_t *auto_hint;

	const int32_t *exydhv;
	const int32_t *action_ui;
	const int32_t *pluyoo;
	const int32_t *tagsmh;
	const int32_t *seofsw_tagsmh;
	const int32_t *dosorb;
	const int32_t *seofsw_dosorb;
	const int32_t *acyylh_ovl;
	const int32_t *smzaik_ovl;

	const int32_t *blockers;
	const int32_t *blocker_count;

	const int32_t *tp_slots;
	const int32_t *tp_counts;

	const int32_t *grid_slot;

	const int32_t *click_kind;
	const int32_t *click_r;
	const int32_t *click_c;
	const int32_t *click_spell;

	const uint8_t *bar_mask;
	const int32_t *bar_count;

	const uint8_t *grow_block;
	const int32_t *fb_hit_slot;
	const int32_t *fb_slot_kind;

	const int8_t *player_base0;
	const int32_t *player_rotation0;

	const int8_t *fb_base;
	const int32_t *fb_base_shape;
};

void sc25_zero_aux(struct sc25_aux *aux);

void sc25_on_set_level(struct arc_sprites *sprites,
		       const struct sc25_static *st, int32_t level,
		       struct sc25_aux *aux);

void sc25_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct sc25_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    struct sc25_aux *aux, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void sc25_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct sc25_static *st, int32_t level,
			   const struct sc25_aux *aux);

#endif
