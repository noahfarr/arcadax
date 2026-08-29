#ifndef ARCADAX_GAMES_M0R0_H
#define ARCADAX_GAMES_M0R0_H

#include "../engine.h"

#define M0R0_NUM_MOVERS 4

typedef struct {
    uint8_t locked[M0R0_NUM_MOVERS];
    int32_t prev_x[M0R0_NUM_MOVERS];
    int32_t prev_y[M0R0_NUM_MOVERS];
    uint8_t has_prev[M0R0_NUM_MOVERS];
    int32_t selected;
    uint8_t move_markers;
    int32_t flash;
    uint8_t flashing[M0R0_NUM_MOVERS];
    int32_t steps;
} M0r0Aux;

typedef struct {
    int32_t num_levels;
    int32_t num_slots;
    int32_t click_tag;
    int32_t max_blocks;
    int32_t max_clickable;
    int32_t max_obstacles;
    int32_t max_switches;
    int32_t max_doors;
    const int32_t *mover_slot;
    const int32_t *block_slots;
    const int32_t *block_count;
    const int32_t *clickable_slots;
    const int32_t *clickable_count;
    const int32_t *obstacle_slots;
    const int32_t *obstacle_count;
    const int32_t *switch_slots;
    const int32_t *switch_count;
    const int32_t *door_slots;
    const int32_t *door_count;
    const uint8_t *hazard_map;
    const int32_t *background;
    const int32_t *clean_x;
    const int32_t *clean_y;
} M0r0Static;

void m0r0_zero_aux(M0r0Aux *aux);

void m0r0_on_set_level(Sprites *sprites, const M0r0Static *st, int32_t level,
                       M0r0Aux *aux);

void m0r0_step_once(Sprites *sprites, const Camera *camera, const M0r0Static *st,
                    int32_t level, int32_t action_id, int32_t action_x,
                    int32_t action_y, int32_t action_count, M0r0Aux *aux,
                    int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete);

void m0r0_render_interface(int8_t *frame, const Sprites *sprites,
                           const Camera *camera, const M0r0Static *st,
                           int32_t level, const M0r0Aux *aux);

#endif
