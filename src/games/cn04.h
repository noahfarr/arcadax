#ifndef ARC_GAMES_CN04_H
#define ARC_GAMES_CN04_H

#include "arc/engine.h"

struct cn04_aux {
	int32_t *rotation_index;
	int8_t *snapshot;
	int32_t selected;
	uint8_t ping_pong;
	uint8_t pending_win;
};

struct cn04_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ph;
	int32_t pw;
	int32_t max_group;
	const int8_t *rotvar;
	const int32_t *h_table;
	const int32_t *w_table;
	const int32_t *init_rot;
	const int32_t *group_size;
	const int32_t *group_rank;
	const int32_t *group_members;
	const int32_t *init_selected;
	const int32_t *fwd_row;
	const int32_t *fwd_col;
	const uint8_t *fwd_valid;
	const int32_t *bwd_row;
	const int32_t *bwd_col;
	const uint8_t *bwd_valid;
	const int32_t *budget;
	const uint8_t *greymask;
	const int8_t *level_bg;
};

void cn04_aux_alloc(struct cn04_aux *aux, int32_t num_slots, int32_t ph,
		    int32_t pw);
void cn04_aux_free(struct cn04_aux *aux);

void cn04_zero_aux(struct cn04_aux *aux, const struct cn04_static *st);

void cn04_on_set_level(struct arc_sprites *sprites, struct arc_camera *camera,
		       const struct cn04_static *st, int32_t level,
		       struct cn04_aux *aux);

void cn04_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct cn04_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct cn04_aux *aux, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete);

void cn04_render_interface(int8_t *frame, const struct cn04_static *st,
			   int32_t level, int32_t action_count);

#endif
