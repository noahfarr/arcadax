#ifndef ARC_GAMES_SP80_H
#define ARC_GAMES_SP80_H

#include "arc/engine.h"

#define SP80_FRONTIER_CAP 32

typedef struct {
    int32_t mode;
    int32_t selected;
    int32_t steps;
    int32_t fail_count;
    uint8_t growing;
    uint8_t draining;
    int32_t flash;
    uint8_t *filled;
    uint8_t *touched;
    int32_t frontier_slot[SP80_FRONTIER_CAP];
    int32_t frontier_dx[SP80_FRONTIER_CAP];
    int32_t frontier_dy[SP80_FRONTIER_CAP];
    uint8_t frontier_active[SP80_FRONTIER_CAP];
} Sp80Aux;

typedef struct {
    int32_t num_levels;
    int32_t num_slots;
    int32_t extra_start;
    int32_t liolf_tag;
    int32_t plzw_tag;
    int32_t repw_tag;
    int32_t tuvk_tag;
    int32_t waoe_tag;
    const int32_t *budget;
    const int32_t *rotation;
    const int32_t *pick_priority;
    int32_t max_plzw;
    const int32_t *plzw_slots;
    const int32_t *plzw_count;
    int32_t max_tuvk;
    const int32_t *tuvk_slots;
    const int32_t *tuvk_count;
    int32_t max_repw;
    const int32_t *repw_slots;
    const int32_t *repw_count;
    int32_t max_waoe;
    const int32_t *waoe_slots;
    const int32_t *waoe_count;
    int32_t max_liolf_seed;
    const int32_t *liolf_seed_slots;
    const int32_t *liolf_seed_count;
    int32_t max_spout;
    const int32_t *spout_slot;
    const int32_t *spout_dx;
    const int32_t *spout_dy;
    const int32_t *spout_count;
} Sp80Static;

void sp80_zero_aux(Sp80Aux *aux, const Sp80Static *st);

void sp80_on_set_level(ArcSprites *sprites, const Sp80Static *st, int32_t level,
                       Sp80Aux *aux);

void sp80_step_once(ArcSprites *sprites, const ArcCamera *camera, const Sp80Static *st,
                    int32_t level, int32_t action_id, int32_t action_x,
                    int32_t action_y, int32_t action_count, Sp80Aux *aux,
                    int32_t *next_order, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete);

void sp80_render_interface(int8_t *frame, const ArcSprites *sprites,
                           const ArcCamera *camera, const Sp80Static *st,
                           int32_t level, const Sp80Aux *aux);

#endif
