#ifndef ARC_GAMES_WA30_H
#define ARC_GAMES_WA30_H

#include "arc/engine.h"

typedef struct {
    int32_t *partner;
    int32_t rotation;
    int32_t steps;
} Wa30Aux;

typedef struct {
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
} Wa30Static;

void wa30_aux_alloc(Wa30Aux *aux, int32_t num_slots);
void wa30_aux_free(Wa30Aux *aux);

void wa30_zero_aux(Wa30Aux *aux, const Wa30Static *st);

void wa30_on_set_level(const Wa30Static *st, int32_t level, Wa30Aux *aux);

void wa30_step_once(ArcSprites *sprites, const Wa30Static *st, int32_t level,
                    int32_t action_id, Wa30Aux *aux, int32_t *score,
                    int32_t *status, uint8_t *next_level, uint8_t *action_complete);

void wa30_render_interface(int8_t *frame, const Wa30Static *st, int32_t level,
                           const Wa30Aux *aux);

#endif
