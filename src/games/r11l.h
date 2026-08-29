#ifndef ARC_GAMES_R11L_H
#define ARC_GAMES_R11L_H

#include "arc/engine.h"

#define R11L_G 3
#define R11L_K 4
#define R11L_NCOMP 2
#define R11L_NPIECES 7

typedef struct {
    uint8_t dragging;
    int32_t selected;
    int32_t start_x;
    int32_t start_y;
    int32_t target_x;
    int32_t target_y;
    int32_t tween;
    uint8_t reverting;
    uint8_t waiting;
    uint8_t won_wait;
    int32_t hazard_hits;
    uint8_t wobbling;
    int32_t wobble_blinks;
    int32_t wobble_ticks;
    int32_t wobble_x;
    int32_t wobble_y;
    uint8_t key_blinking[R11L_K];
    int32_t key_blink_blinks[R11L_K];
    int32_t key_blink_ticks[R11L_K];
    uint8_t key_was_matched[R11L_K];
} R11lAux;

typedef struct {
    int32_t num_levels;
    int32_t num_slots;
    int32_t icon_base;
    int32_t wobble_icon_slot;
    int32_t max_group_pieces;
    int32_t max_fragments;
    int32_t max_walls;
    int32_t max_hazards;
    const int32_t *group_target_slot;
    const uint8_t *group_is_composite;
    const int32_t *group_piece_slots;
    const int32_t *group_piece_count;
    const int32_t *piece_group;
    const int32_t *pieces_order;
    const int32_t *key_clue_slot;
    const int32_t *key_target_slot;
    const uint8_t *key_skip_win;
    const uint8_t *key_colour_set;
    const int32_t *composite_slot;
    const int32_t *fragment_slots;
    const int32_t *fragment_count;
    const int32_t *wall_slots;
    const int32_t *wall_count;
    const int32_t *hazard_slots;
    const int32_t *hazard_count;
} R11lStatic;

void r11l_zero_aux(R11lAux *aux);

void r11l_on_set_level(ArcSprites *sprites, const R11lStatic *st, int32_t level, R11lAux *aux,
                       int32_t *next_order);

void r11l_step_once(ArcSprites *sprites, const ArcCamera *camera, const R11lStatic *st,
                    int32_t level, int32_t action_id, int32_t action_x, int32_t action_y,
                    int32_t action_count, R11lAux *aux, int32_t *next_order,
                    int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete);

void r11l_render_interface(int8_t *frame, const ArcSprites *sprites, const ArcCamera *camera,
                           const R11lStatic *st, int32_t level, const R11lAux *aux,
                           int32_t action_count);

#endif
