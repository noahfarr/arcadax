#ifndef ARC_GAMES_FT09_H
#define ARC_GAMES_FT09_H

#include "arc/engine.h"

struct ft09_aux {
	int32_t steps;
	int32_t hint;
};

struct ft09_static {
	int32_t num_levels;
	int32_t plain_tag;
	int32_t pattern_tag;
	int32_t clue_tag;
	int32_t palette_width;
	int32_t max_clues;
	const int32_t *budget;
	const int32_t *palette;
	const int32_t *palette_size;
	const int32_t *brush;
	const int32_t *hint_slot;
	const int32_t *clue_slots;
	const int32_t *clue_count;
};

void ft09_zero_aux(struct ft09_aux *aux);

void ft09_on_set_level(struct arc_sprites *sprites,
		       const struct ft09_static *st, int32_t level,
		       struct ft09_aux *aux);

void ft09_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct ft09_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    struct ft09_aux *aux, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void ft09_render_interface(int8_t *frame, const struct ft09_static *st,
			   int32_t level, int32_t steps);

#endif
