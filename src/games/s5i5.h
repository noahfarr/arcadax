#ifndef ARC_GAMES_S5I5_H
#define ARC_GAMES_S5I5_H

#include "arc/engine.h"

struct s5i5_static {
	int32_t num_levels;
	int32_t num_slots;
	int32_t ph;
	int32_t pw;
	int32_t handle_tag;
	int32_t slider_tag;
	const int32_t *pipe_color;
	const int32_t *handle_color;
	const int32_t *parent_slot;
	const int32_t *budget;
	const int32_t *pipe_offset;
	const int32_t *pipe_flat;
	const int32_t *desc_offset;
	const int32_t *desc_flat;
	const int32_t *slider_pipe_offset;
	const int32_t *slider_pipe_flat;
	const int32_t *target_offset;
	const int32_t *target_flat;
	const int32_t *conn_offset;
	const int32_t *conn_flat;
};

struct s5i5_aux {
	int32_t steps;
	uint8_t pending;
	uint8_t *backup_valid;
	int32_t *backup_x;
	int32_t *backup_y;
	int32_t *backup_h;
	int32_t *backup_w;
	int8_t *backup_pixels;
};

struct s5i5_engine {
	int32_t level_index;
	int32_t action_id;
	int32_t action_x;
	int32_t action_y;
	int32_t score;
	int32_t status;
	uint8_t action_complete;
	uint8_t next_level;
};

void s5i5_aux_alloc(struct s5i5_aux *aux, int32_t num_slots, int32_t ph,
		    int32_t pw);
void s5i5_aux_free(struct s5i5_aux *aux);

void s5i5_zero_aux(struct s5i5_aux *aux, const struct s5i5_static *st);
void s5i5_on_set_level(struct s5i5_aux *aux, const struct s5i5_static *st,
		       int32_t level);
void s5i5_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera, struct s5i5_engine *engine,
		    struct s5i5_aux *aux, const struct s5i5_static *st);
void s5i5_render_interface(int8_t *frame, const struct s5i5_aux *aux,
			   const struct s5i5_static *st, int32_t level);

#endif
