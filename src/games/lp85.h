#ifndef ARC_GAMES_LP85_H
#define ARC_GAMES_LP85_H

#include "arc/engine.h"

enum { LP85_NOT_PLAYED = 0, LP85_NOT_FINISHED = 1, LP85_WIN = 2, LP85_GAME_OVER = 3 };
enum { LP85_ACTION6 = 6 };

typedef struct {
    int32_t num_levels;
    int32_t num_slots;
    int32_t max_ring;
    int32_t max_buttons;
    int32_t max_candidates;
    int32_t max_pieces;
    int32_t piece_a;
    int32_t piece_b;
    int32_t goal_a;
    int32_t goal_b;
    const uint8_t *is_button;
    const int32_t *ring_len;
    const int32_t *ring_from_x;
    const int32_t *ring_from_y;
    const int32_t *ring_to_x;
    const int32_t *ring_to_y;
    const int32_t *budget;
    const int32_t *button_slots;
    const int32_t *num_buttons;
    const int32_t *ring_candidates;
    const int32_t *num_candidates;
    const int32_t *piece_a_slots;
    const int32_t *num_piece_a;
    const int32_t *piece_b_slots;
    const int32_t *num_piece_b;
} Lp85Static;

typedef struct {
    int32_t *sources;
    int32_t *hits;
} Lp85Scratch;

typedef struct {
    int32_t steps;
} Lp85Aux;

typedef struct {
    ArcCamera camera;
    int32_t level_index;
    int32_t score;
    int32_t status;
    int32_t action_id;
    int32_t action_x;
    int32_t action_y;
    uint8_t action_complete;
    uint8_t next_level;
} Lp85Engine;

void lp85_zero_aux(Lp85Aux *aux);
void lp85_on_set_level(const Lp85Static *st, Lp85Engine *engine, Lp85Aux *aux);
void lp85_step_once(const Lp85Static *st, ArcSprites *sprites, Lp85Scratch *scratch,
                    Lp85Engine *engine, Lp85Aux *aux);
void lp85_render_interface(const Lp85Static *st, const Lp85Engine *engine,
                           const Lp85Aux *aux, int8_t *frame);

#endif
