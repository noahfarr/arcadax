#ifndef ARC_GAMES_RE86_H
#define ARC_GAMES_RE86_H

#include <stdint.h>

#include "arc/engine.h"

struct re86_aux {
	int32_t steps;
	int32_t flood_target_slot;
	int32_t flood_cursor_slot;
};

struct re86_engine {
	int32_t level_index;
	int32_t score;
	int32_t status;
	int32_t camera_width;
	int32_t camera_height;
	int8_t next_level;
	int8_t action_complete;
};

struct re86_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t max_cursors;
	int32_t max_walls;
	int32_t max_flood_targets;
	const int8_t *is_reshape;
	const int8_t *is_solid_center;
	const int32_t *num_cursors;
	const int32_t *cursor_slot_by_rank;
	const int32_t *cursor_rank;
	const int32_t *num_walls;
	const int32_t *wall_slots;
	const int32_t *num_flood_targets;
	const int32_t *flood_target_slots;
	const int32_t *win_target_slot;
	const int32_t *budget;
	const int8_t *canvas_template;
};

void re86_zero_aux(struct re86_aux *aux);
void re86_on_set_level(struct re86_aux *aux, const struct re86_static *st,
		       int32_t level_index);
void re86_step_once(struct arc_sprites *sprites, struct re86_aux *aux,
		    struct re86_engine *engine, const struct re86_static *st,
		    int32_t action_id);
void re86_render_interface(int8_t *frame, const struct re86_aux *aux,
			   const struct re86_static *st, int32_t level_index);

#endif
