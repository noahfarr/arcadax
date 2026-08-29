#ifndef ARC_GAMES_KA59_H
#define ARC_GAMES_KA59_H

#include "arc/engine.h"

struct ka59_aux {
	int32_t active_box;
	uint8_t *push_active;
	int32_t *recoil_dx;
	int32_t *recoil_dy;
	uint8_t *has_recoil;
	int32_t retry_frame;
	uint8_t *exploding;
	int32_t *fuse_progress;
	uint8_t *fuse_first_cycle;
	int32_t collider_dir;
	int32_t steps;
	int32_t *box_bbox;
};

struct ka59_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ph;
	int32_t pw;
	int32_t max_bomb_h;
	int32_t box_tag;
	int32_t collider_slot;
	int32_t scratch_slot;
	int32_t explosion_base;
	int32_t explosion_slots_total;

	const int32_t *step_budget;
	const int32_t *first_box;

	int32_t max_box;
	const int32_t *box_slots;
	const int32_t *box_count;

	int32_t max_marker;
	const int32_t *marker_slots;
	const int32_t *marker_count;

	int32_t max_hole;
	const int32_t *hole_slots;
	const int32_t *hole_count;

	int32_t max_zone;
	const int32_t *zone_slots;
	const int32_t *zone_count;

	int32_t max_bomb;
	const int32_t *bomb_slots;
	const int32_t *bomb_count;

	int32_t max_wall;
	const int32_t *wall_slots;
	const int32_t *wall_count;

	int32_t max_border;
	const int32_t *border_slots;
	const int32_t *border_count;

	int32_t max_occupant;
	const int32_t *occupant_slots;
	const int32_t *occupant_count;

	const uint8_t *center_dot_mask;
	const uint8_t *box_outline_mask;
	const uint8_t *box_edge_masks;

	const int8_t *explosion_pixels;
	const int32_t *explosion_h;
	const int32_t *explosion_w;
	const int32_t *explosion_recoil_dx;
	const int32_t *explosion_recoil_dy;
	const int32_t *explosion_base_slot;

	const int8_t *fuse_frames;
	const int32_t *fuse_total_bad;
	const uint8_t *fuse_last_row_bad;
};

void ka59_aux_alloc(struct ka59_aux *aux, int32_t num_slots);
void ka59_aux_free(struct ka59_aux *aux);

void ka59_zero_aux(struct ka59_aux *aux, const struct ka59_static *st);

void ka59_on_set_level(struct arc_sprites *sprites,
		       const struct ka59_static *st, int32_t level,
		       struct ka59_aux *aux, int32_t *next_order);

void ka59_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct ka59_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct ka59_aux *aux,
		    int32_t *next_order, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void ka59_render_interface(int8_t *frame, const struct ka59_static *st,
			   int32_t level, const struct ka59_aux *aux);

#endif
