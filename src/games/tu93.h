#ifndef ARC_GAMES_TU93_H
#define ARC_GAMES_TU93_H

#include "arc/engine.h"

#define TU93_NUM_SLOTS 11
#define TU93_MAX_QUEUE 8

enum { TU93_WIN = 2, TU93_GAME_OVER = 3 };
enum { TU93_ACTION1 = 1, TU93_ACTION2 = 2, TU93_ACTION3 = 3, TU93_ACTION4 = 4 };
enum { TU93_STEP_PX = 3, TU93_CELL_PX = 6, TU93_TRAIN_RANGE_PX = 12 };
enum { TU93_ATTACHED_COLOR = 11, TU93_HUD_FILLED = 6 };

struct tu93_aux {
	int32_t phase;
	int32_t rotation[TU93_NUM_SLOTS];
	int32_t queue[TU93_NUM_SLOTS][TU93_MAX_QUEUE];
	int32_t queue_len[TU93_NUM_SLOTS];
	int32_t steps;
};

struct tu93_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ph;
	int32_t pw;
	int32_t max_push;
	int32_t max_drift;
	int32_t max_train;
	int32_t max_crate;
	int32_t max_exit;
	const int32_t *player_slot;
	const int32_t *budget;
	const int32_t *board_xy;
	const uint8_t *walkable;
	const int32_t *init_rotation;
	const int8_t *own_color;
	const uint8_t *is_push;
	const uint8_t *is_drift;
	const uint8_t *is_train;
	const int32_t *push_slots;
	const int32_t *push_count;
	const int32_t *drift_slots;
	const int32_t *drift_count;
	const int32_t *train_slots;
	const int32_t *train_count;
	const int32_t *crate_slots;
	const int32_t *crate_count;
	const int32_t *exit_slots;
	const int32_t *exit_count;
	const int8_t *template5;
	const int8_t *template7;
	const int8_t *base3;
	const int32_t *flag_pos3;
	const int32_t *flag_pos5;
	const int32_t *flag_pos7;
};

void tu93_zero_aux(struct tu93_aux *aux);

void tu93_on_set_level(struct arc_sprites *sprites,
		       const struct tu93_static *st, int32_t level,
		       struct tu93_aux *aux);

void tu93_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct tu93_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    struct tu93_aux *aux, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void tu93_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct tu93_static *st, int32_t level,
			   const struct tu93_aux *aux);

#endif
