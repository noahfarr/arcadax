#include "lp85.h"

#include <stddef.h>

static const int8_t LP85_FILLED_COLOUR = 5;
static const int8_t LP85_EMPTY_COLOUR = 14;

static int32_t lp85_slot_index(const Lp85Static *st, int32_t level, int32_t slot) {
    return level * st->num_slots + slot;
}

static void lp85_find_ring_sources(const Lp85Static *st, const Sprites *sprites,
                                   int32_t level, int32_t slot, int32_t *sources) {
    int32_t idx = lp85_slot_index(st, level, slot);
    int32_t rl = st->ring_len[idx];
    size_t base = (size_t)idx * st->max_ring;
    const int32_t *from_x = st->ring_from_x + base;
    const int32_t *from_y = st->ring_from_y + base;
    int32_t cand_count = st->num_candidates[level];
    const int32_t *candidates = st->ring_candidates + (size_t)level * st->max_candidates;

    for (int32_t k = 0; k < rl; k++) {
        int32_t found = -1;
        for (int32_t c = 0; c < cand_count; c++) {
            int32_t i = candidates[c];
            if (sprites->alive[i] && sprites->x[i] == from_x[k] && sprites->y[i] == from_y[k]) {
                found = i;
                break;
            }
        }
        sources[k] = found;
    }
}

static void lp85_rotate_ring(const Lp85Static *st, Sprites *sprites, Lp85Scratch *scratch,
                             int32_t level, int32_t slot) {
    int32_t idx = lp85_slot_index(st, level, slot);
    int32_t rl = st->ring_len[idx];
    size_t base = (size_t)idx * st->max_ring;
    const int32_t *to_x = st->ring_to_x + base;
    const int32_t *to_y = st->ring_to_y + base;

    lp85_find_ring_sources(st, sprites, level, slot, scratch->sources);

    for (int32_t k = 0; k < rl; k++) {
        int32_t i = scratch->sources[k];
        if (i < 0) continue;
        sprites->x[i] = to_x[k];
        sprites->y[i] = to_y[k];
    }
}

static int lp85_piece_group_satisfied(const Sprites *sprites, const int32_t *slots,
                                      int32_t count, int32_t goal_tag) {
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!sprites->alive[i]) continue;
        if (get_sprite_at(sprites, sprites->x[i] + 1, sprites->y[i] + 1, goal_tag, 0) < 0)
            return 0;
    }
    return 1;
}

static int lp85_solved(const Lp85Static *st, const Sprites *sprites, int32_t level) {
    const int32_t *piece_a = st->piece_a_slots + (size_t)level * st->max_pieces;
    const int32_t *piece_b = st->piece_b_slots + (size_t)level * st->max_pieces;
    if (!lp85_piece_group_satisfied(sprites, piece_a, st->num_piece_a[level], st->goal_a))
        return 0;
    return lp85_piece_group_satisfied(sprites, piece_b, st->num_piece_b[level], st->goal_b);
}

void lp85_zero_aux(Lp85Aux *aux) { aux->steps = 0; }

void lp85_on_set_level(const Lp85Static *st, Lp85Engine *engine, Lp85Aux *aux) {
    aux->steps = st->budget[engine->level_index];
}

void lp85_step_once(const Lp85Static *st, Sprites *sprites, Lp85Scratch *scratch,
                    Lp85Engine *engine, Lp85Aux *aux) {
    int32_t level = engine->level_index;
    int32_t scale, x_pad, y_pad;
    scale_and_offset(&engine->camera, &scale, &x_pad, &y_pad);

    int32_t dx = engine->action_x - x_pad;
    int32_t dy = engine->action_y - y_pad;
    int32_t local_x = dx >= 0 ? dx / scale : -1;
    int32_t local_y = dy >= 0 ? dy / scale : -1;
    int on_board = local_x >= 0 && local_y >= 0 &&
                   local_x < engine->camera.width && local_y < engine->camera.height;
    int32_t world_x = local_x + engine->camera.x;
    int32_t world_y = local_y + engine->camera.y;
    int is_click = engine->action_id == LP85_ACTION6;

    int32_t nhits = 0;
    if (is_click && on_board) {
        int32_t nb = st->num_buttons[level];
        const int32_t *buttons = st->button_slots + (size_t)level * st->max_buttons;
        for (int32_t k = 0; k < nb; k++) {
            int32_t i = buttons[k];
            if (!sprites->alive[i]) continue;
            if (world_x >= sprites->x[i] && world_y >= sprites->y[i] &&
                world_x < sprites->x[i] + sprites->w[i] &&
                world_y < sprites->y[i] + sprites->h[i])
                scratch->hits[nhits++] = i;
        }
    }

    for (int32_t k = 0; k < nhits; k++)
        lp85_rotate_ring(st, sprites, scratch, level, scratch->hits[k]);

    if (nhits == 0) {
        engine->action_complete = 1;
        return;
    }

    if (lp85_solved(st, sprites, level)) {
        engine->score += 1;
        int is_last = level == st->num_levels - 1;
        engine->next_level = is_last ? 0 : 1;
        if (is_last) engine->status = LP85_WIN;
        engine->action_complete = 1;
        return;
    }

    int32_t steps = aux->steps - 1;
    if (steps < 0) steps = 0;
    aux->steps = steps;
    if (steps == 0) engine->status = LP85_GAME_OVER;
    engine->action_complete = 1;
}

void lp85_render_interface(const Lp85Static *st, const Lp85Engine *engine,
                           const Lp85Aux *aux, int8_t *frame) {
    int32_t budget = st->budget[engine->level_index];
    int32_t used = budget - aux->steps;
    int32_t total = FRAME_SIZE * used;
    int32_t whole = total / budget;
    int32_t rest = total % budget;
    int round_up = (2 * rest > budget) || (2 * rest == budget && (whole % 2 != 0));
    int32_t filled = whole + (round_up ? 1 : 0);
    if (filled < 0) filled = 0;
    if (filled > FRAME_SIZE) filled = FRAME_SIZE;

    for (int32_t r = 0; r < FRAME_SIZE; r++)
        frame[(size_t)r * FRAME_SIZE] = r < filled ? LP85_FILLED_COLOUR : LP85_EMPTY_COLOUR;

    int32_t widget_level = engine->level_index - 1;
    for (int32_t i = 0; i < st->num_levels; i++) {
        int32_t start_x = 10 + i * 5;
        int8_t colour = i <= widget_level ? LP85_EMPTY_COLOUR : LP85_FILLED_COLOUR;
        for (int32_t c = 0; c < 4; c++) {
            int32_t x = start_x + c;
            if (x < FRAME_SIZE) frame[FRAME_SIZE + x] = colour;
        }
    }
}
