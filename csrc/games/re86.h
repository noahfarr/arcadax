#ifndef ARCADAX_GAMES_RE86_H
#define ARCADAX_GAMES_RE86_H

#include <stdint.h>

#include "../engine.h"

typedef struct {
    int32_t steps;
    int32_t flood_target_slot;
    int32_t flood_cursor_slot;
} Re86Aux;

typedef struct {
    int32_t level_index;
    int32_t score;
    int32_t status;
    int32_t camera_width;
    int32_t camera_height;
    int8_t next_level;
    int8_t action_complete;
} Re86Engine;

typedef struct {
    int32_t num_levels;
    int32_t num_slots;
    int32_t max_cursors;
    int32_t max_walls;
    int32_t max_flood_targets;
    const int8_t *is_reshape;
    const int8_t *is_solid_center;
    const int32_t *num_cursors;
    const int32_t *cursor_slot_by_rank;
    const int32_t *cursor_rank;
    const int32_t *num_walls;
    const int32_t *wall_slots;
    const int32_t *num_flood_targets;
    const int32_t *flood_target_slots;
    const int32_t *win_target_slot;
    const int32_t *budget;
    const int8_t *canvas_template;
} Re86Static;

void re86_zero_aux(Re86Aux *aux);
void re86_on_set_level(Re86Aux *aux, const Re86Static *st, int32_t level_index);
void re86_step_once(Sprites *sprites, Re86Aux *aux, Re86Engine *engine,
                    const Re86Static *st, int32_t action_id);
void re86_render_interface(int8_t *frame, const Re86Aux *aux,
                           const Re86Static *st, int32_t level_index);

#endif
