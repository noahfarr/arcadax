#ifndef ARC_GAMES_LP85_H
#define ARC_GAMES_LP85_H

#include "arc/engine.h"

enum {
	LP85_NOT_PLAYED = 0,
	LP85_NOT_FINISHED = 1,
	LP85_WIN = 2,
	LP85_GAME_OVER = 3
};
enum { LP85_ACTION6 = 6 };

struct lp85_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t max_ring;
	int32_t max_buttons;
	int32_t max_candidates;
	int32_t max_pieces;
	int32_t piece_a;
	int32_t piece_b;
	int32_t goal_a;
	int32_t goal_b;
	const uint8_t *is_button;
	const int32_t *ring_len;
	const int32_t *ring_from_x;
	const int32_t *ring_from_y;
	const int32_t *ring_to_x;
	const int32_t *ring_to_y;
	const int32_t *budget;
	const int32_t *button_slots;
	const int32_t *num_buttons;
	const int32_t *ring_candidates;
	const int32_t *num_candidates;
	const int32_t *piece_a_slots;
	const int32_t *num_piece_a;
	const int32_t *piece_b_slots;
	const int32_t *num_piece_b;
};

struct lp85_scratch {
	int32_t *sources;
	int32_t *hits;
};

struct lp85_aux {
	int32_t steps;
};

struct lp85_engine {
	struct arc_camera camera;
	int32_t level_index;
	int32_t score;
	int32_t status;
	int32_t action_id;
	int32_t action_x;
	int32_t action_y;
	uint8_t action_complete;
	uint8_t next_level;
};

void lp85_zero_aux(struct lp85_aux *aux);
void lp85_on_set_level(const struct lp85_static *st, struct lp85_engine *engine,
		       struct lp85_aux *aux);
void lp85_step_once(const struct lp85_static *st, struct arc_sprites *sprites,
		    struct lp85_scratch *scratch, struct lp85_engine *engine,
		    struct lp85_aux *aux);
void lp85_render_interface(const struct lp85_static *st,
			   const struct lp85_engine *engine,
			   const struct lp85_aux *aux, int8_t *frame);

#endif
