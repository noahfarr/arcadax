#ifndef ARC_GAMES_G50T_H
#define ARC_GAMES_G50T_H

#include "arc/engine.h"

#define G50T_GRID 6
#define G50T_MAX_CHECKPOINTS 3
#define G50T_MAX_BUTTONS 5
#define G50T_MAX_GATES 5
#define G50T_MAX_DOORS 5
#define G50T_MAX_PORTALS 2
#define G50T_MAX_ENDPOINTS 4
#define G50T_MAX_ICE 1
#define G50T_MAX_SENSORS (G50T_MAX_BUTTONS + G50T_MAX_ENDPOINTS)
#define G50T_PLAYER_GENERATIONS 6
#define G50T_MAX_TRACKED (G50T_PLAYER_GENERATIONS + G50T_MAX_ICE)
#define G50T_MAX_MOVES 130
#define G50T_DEATH_PIECES 9
#define G50T_CROSSFADE_PIECES 14

struct g50t_aux {
	uint8_t dispatched;
	uint8_t won_pending;
	uint8_t lost_pending;

	int32_t gen;
	uint8_t echo[G50T_PLAYER_GENERATIONS];
	uint8_t active[G50T_PLAYER_GENERATIONS];
	int32_t target_x[G50T_PLAYER_GENERATIONS];
	int32_t target_y[G50T_PLAYER_GENERATIONS];
	int32_t delay[G50T_PLAYER_GENERATIONS];
	uint8_t instant[G50T_PLAYER_GENERATIONS];
	uint8_t dying[G50T_PLAYER_GENERATIONS];
	int32_t death_step[G50T_PLAYER_GENERATIONS];
	uint8_t dead[G50T_PLAYER_GENERATIONS];
	int8_t history[G50T_PLAYER_GENERATIONS][G50T_MAX_MOVES][2];
	int32_t history_len[G50T_PLAYER_GENERATIONS];
	int32_t move_count;
	int32_t pending_history_len;

	uint8_t ice_active[G50T_MAX_ICE];
	int32_t ice_target_x[G50T_MAX_ICE];
	int32_t ice_target_y[G50T_MAX_ICE];
	int32_t ice_delay[G50T_MAX_ICE];
	uint8_t ice_instant[G50T_MAX_ICE];
	uint8_t ice_dying[G50T_MAX_ICE];
	int32_t ice_death_step[G50T_MAX_ICE];
	uint8_t ice_dead[G50T_MAX_ICE];
	int32_t ice_dir[G50T_MAX_ICE];
	int8_t ice_history[G50T_MAX_ICE][G50T_MAX_MOVES][2];
	int32_t ice_move_count[G50T_MAX_ICE];

	uint8_t door_open[G50T_MAX_DOORS];
	uint8_t door_active[G50T_MAX_DOORS];
	int32_t door_target_x[G50T_MAX_DOORS];
	int32_t door_target_y[G50T_MAX_DOORS];
	int32_t door_delay[G50T_MAX_DOORS];
	uint8_t door_instant[G50T_MAX_DOORS];

	uint8_t gate_on[G50T_MAX_GATES];
	uint8_t gate_active[G50T_MAX_GATES];
	int32_t gate_delay[G50T_MAX_GATES];

	uint8_t portal_active[G50T_MAX_PORTALS];
	int32_t portal_delay[G50T_MAX_PORTALS];
	uint8_t portal_animating[G50T_MAX_PORTALS];
	int32_t portal_progress[G50T_MAX_PORTALS];

	uint8_t button_members[G50T_MAX_BUTTONS][G50T_MAX_TRACKED];
	uint8_t endpoint_members[G50T_MAX_ENDPOINTS][G50T_MAX_TRACKED];

	int32_t checkpoint_idx;
	uint8_t swap_pending;
	uint8_t commit_pending;
	uint8_t undoing;

	uint8_t ghost_active;
	int32_t ghost_target_x;
	int32_t ghost_target_y;

	int32_t action_count;
};

struct g50t_static {
	int32_t num_levels;
	int32_t num_slots;

	const int32_t *checkpoint_slot;
	const int32_t *checkpoint_count;
	const int32_t *goal_slot;
	const int32_t *bounds_slot;
	const int32_t *timer_slot;
	const int32_t *ghost_slot;
	const int32_t *door_slot;
	const int32_t *endpoint_slot;
	const int32_t *ice_slot;
	const int32_t *ice_bound_slot;
	const int32_t *sensor_slot;
	const int32_t *button_gate;
	const int32_t *gate_kind;
	const int32_t *gate_target;
	const int32_t *portal_endpoints;
	const int32_t *door_dir_x;
	const int32_t *door_dir_y;

	const int32_t *gen_slot;
	const int32_t *player_death_slot;
	const int32_t *ice_death_slot;
	const int32_t *portal_crossfade_slot;
	const int32_t *checkpoint_on_slot;
	const int32_t *checkpoint_off_slot;

	const int32_t *spawn_x;
	const int32_t *spawn_y;
	const int8_t *player_pixels;
};

void g50t_zero_aux(struct g50t_aux *aux);

void g50t_on_set_level(struct arc_sprites *sprites,
		       const struct g50t_static *st, int32_t level,
		       struct g50t_aux *aux);

void g50t_step_once(struct arc_sprites *sprites, const struct g50t_static *st,
		    int32_t level, int32_t action_id, int32_t action_x,
		    int32_t action_y, int32_t action_count,
		    struct g50t_aux *aux, int32_t *next_order, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete);

void g50t_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct g50t_static *st, int32_t level,
			   const struct g50t_aux *aux);

#endif
