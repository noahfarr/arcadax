#ifndef ARC_GAMES_AR25_H
#define ARC_GAMES_AR25_H

#include "arc/engine.h"

#define AR25_GRID 21
#define AR25_MAX_UNDO 320

struct ar25_aux {
	int32_t selected_slot;
	uint8_t changed_selection;
	uint8_t won_pending;
	int32_t hint_frame;
	int32_t energy;
	int32_t undo_top;
	int32_t undo_x[AR25_MAX_UNDO][4];
	int32_t undo_y[AR25_MAX_UNDO][4];
};

struct ar25_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ghost_slot;
	int32_t hint_slot;
	int32_t hint_h;
	int32_t hint_w;
	int32_t hint_layer;
	const int8_t *hint_pixels;
	const int32_t *steps_budget;
	const int32_t *vmirror_slot;
	const int32_t *hmirror_slot;
	const int32_t *movable_slot;
	const int32_t *axis_slot;
	const int32_t *cycle_order;
	const int32_t *cycle_count;
	const int32_t *initial_selected;
	const uint8_t *excluded_movable;
	const uint8_t *excluded_vmirror;
	const uint8_t *excluded_hmirror;
	const uint8_t *excluded_axis;
	const uint8_t *target_grid;
};

void ar25_zero_aux(struct ar25_aux *aux);

void ar25_on_set_level(struct arc_sprites *sprites,
		       const struct ar25_static *st, int32_t level,
		       struct ar25_aux *aux);

void ar25_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct ar25_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    struct ar25_aux *aux, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void ar25_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct ar25_static *st, int32_t level,
			   const struct ar25_aux *aux);

#endif
