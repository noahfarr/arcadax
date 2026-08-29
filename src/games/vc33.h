#ifndef ARC_GAMES_VC33_H
#define ARC_GAMES_VC33_H

#include "arc/engine.h"

#define VC33_MAX_COUPLERS 3
#define VC33_MAX_QUEUE 6

struct vc33_aux {
	int32_t steps;
	int32_t queue_slot[VC33_MAX_QUEUE];
	int32_t queue_tx[VC33_MAX_QUEUE];
	int32_t queue_ty[VC33_MAX_QUEUE];
	int32_t queue_len;
	int32_t queue_idx;
};

struct vc33_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t icon_base;

	const uint8_t *lever_mask;
	const uint8_t *coupler_mask;

	const int32_t *grav_x;
	const int32_t *grav_y;
	const int32_t *budget;
	const int32_t *sensor_adjust;

	const int32_t *lever_src;
	const int32_t *lever_dst;

	const uint8_t *pipe_uses_floor;
	const int32_t *pipe_floor_max_front;
	const int32_t *pipe_wall_extreme;
	const uint8_t *wall_touch_mask;

	const int32_t *marker_along;
	const int32_t *marker_wall;
	const uint8_t *sensor_marker_color;

	const int32_t *level_coupler_slot;
	const int32_t *icon_dx;
	const int32_t *icon_dy;

	int32_t max_pipes;
	const int32_t *pipe_slots;
	const int32_t *pipe_count;

	int32_t max_sensors;
	const int32_t *sensor_slots;
	const int32_t *sensor_count;

	int32_t max_markers;
	const int32_t *marker_slots;
	const int32_t *marker_count;
};

void vc33_zero_aux(struct vc33_aux *aux);

void vc33_on_set_level(const struct vc33_static *st, int32_t level,
		       struct vc33_aux *aux);

void vc33_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct vc33_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    struct vc33_aux *aux, int32_t *next_order, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete);

void vc33_render_interface(int8_t *frame, const struct vc33_static *st,
			   int32_t level, const struct vc33_aux *aux);

#endif
