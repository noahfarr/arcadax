#ifndef ARC_GAMES_WA30_H
#define ARC_GAMES_WA30_H

#include "arc/engine.h"

struct wa30_aux {
	int32_t *partner;
	int32_t rotation;
	int32_t steps;
};

struct wa30_static {
	int32_t num_levels;
	int32_t num_slots;
	const int32_t *player_slot;
	const int32_t *budget;
	const uint8_t *is_box;
	const uint8_t *is_seeker;
	const uint8_t *is_thief;
	const uint8_t *hole_grid;
	const uint8_t *fsj_grid;
	const uint8_t *zqx_grid;
	const int8_t *player_variants;
};

void wa30_aux_alloc(struct wa30_aux *aux, int32_t num_slots);
void wa30_aux_free(struct wa30_aux *aux);

void wa30_zero_aux(struct wa30_aux *aux, const struct wa30_static *st);

void wa30_on_set_level(const struct wa30_static *st, int32_t level,
		       struct wa30_aux *aux);

void wa30_step_once(struct arc_sprites *sprites, const struct wa30_static *st,
		    int32_t level, int32_t action_id, struct wa30_aux *aux,
		    int32_t *score, int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete);

void wa30_render_interface(int8_t *frame, const struct wa30_static *st,
			   int32_t level, const struct wa30_aux *aux);

#endif
