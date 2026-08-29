#ifndef ARCADAX_GAMES_S5I5_H
#define ARCADAX_GAMES_S5I5_H

#include "../engine.h"

typedef struct {
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
} S5i5Static;

typedef struct {
    int32_t steps;
    uint8_t pending;
    uint8_t *backup_valid;
    int32_t *backup_x;
    int32_t *backup_y;
    int32_t *backup_h;
    int32_t *backup_w;
    int8_t *backup_pixels;
} S5i5Aux;

typedef struct {
    int32_t level_index;
    int32_t action_id;
    int32_t action_x;
    int32_t action_y;
    int32_t score;
    int32_t status;
    uint8_t action_complete;
    uint8_t next_level;
} S5i5Engine;

void s5i5_aux_alloc(S5i5Aux *aux, int32_t num_slots, int32_t ph, int32_t pw);
void s5i5_aux_free(S5i5Aux *aux);

void s5i5_zero_aux(S5i5Aux *aux, const S5i5Static *st);
void s5i5_on_set_level(S5i5Aux *aux, const S5i5Static *st, int32_t level);
void s5i5_step_once(Sprites *sprites, const Camera *camera, S5i5Engine *engine,
                    S5i5Aux *aux, const S5i5Static *st);
void s5i5_render_interface(int8_t *frame, const S5i5Aux *aux, const S5i5Static *st,
                           int32_t level);

#endif
