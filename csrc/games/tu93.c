#include "tu93.h"

#include <string.h>

static inline int32_t tu93_clip(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t tu93_mod(int32_t a, int32_t b) {
    int32_t r = a % b;
    return r < 0 ? r + b : r;
}

static inline int32_t tu93_ridx(int32_t rotation) {
    int32_t r = (rotation / 90) % 4;
    return r < 0 ? r + 4 : r;
}

static void tu93_move1(int32_t rotation, int32_t *dx, int32_t *dy) {
    *dx = rotation == 90 ? 1 : (rotation == 270 ? -1 : 0);
    *dy = rotation == 0 ? -1 : (rotation == 180 ? 1 : 0);
}

static int tu93_behind(int32_t x, int32_t y, int32_t rotation, int32_t tx, int32_t ty,
                       int32_t dist) {
    if (rotation == 90) return x == tx - dist && y == ty;
    if (rotation == 270) return x == tx + dist && y == ty;
    if (rotation == 180) return x == tx && y == ty - dist;
    if (rotation == 0) return x == tx && y == ty + dist;
    return 0;
}

static void tu93_flag_rc(const Tu93Static *st, int32_t ridx, int32_t h, int32_t *row,
                         int32_t *col) {
    const int32_t *p = h == 5 ? st->flag_pos5 : (h == 7 ? st->flag_pos7 : st->flag_pos3);
    *row = p[ridx * 2 + 0];
    *col = p[ridx * 2 + 1];
}

static int tu93_attached(const Sprites *s, const Tu93Static *st, const Tu93Aux *aux,
                         int32_t i) {
    int32_t ridx = tu93_ridx(aux->rotation[i]);
    int32_t row, col;
    tu93_flag_rc(st, ridx, s->h[i], &row, &col);
    return sprite_pixels(s, i)[(size_t)row * st->pw + col] == TU93_ATTACHED_COLOR;
}

static void tu93_set_attached_one(Sprites *s, const Tu93Static *st, const Tu93Aux *aux,
                                  int32_t i) {
    int32_t ridx = tu93_ridx(aux->rotation[i]);
    int32_t row, col;
    tu93_flag_rc(st, ridx, s->h[i], &row, &col);
    int8_t *dst = sprite_pixels_mut(s, i);
    dst[(size_t)row * st->pw + col] = TU93_ATTACHED_COLOR;
}

static void tu93_apply_rotation_one(Sprites *s, const Tu93Static *st, Tu93Aux *aux,
                                    int32_t level, int32_t i, int32_t new_rotation) {
    int was_attached = tu93_attached(s, st, aux, i);
    aux->rotation[i] = new_rotation;
    int32_t ridx = tu93_ridx(new_rotation);
    int32_t h = s->h[i];
    int32_t area = st->ph * st->pw;
    int8_t *dst = sprite_pixels_mut(s, i);
    if (h == 5 || h == 7) {
        const int8_t *tmpl = (h == 5 ? st->template5 : st->template7) + (size_t)ridx * area;
        int8_t colour = st->own_color[(size_t)level * st->num_slots + i];
        for (int32_t k = 0; k < area; k++) dst[k] = tmpl[k] >= 0 ? colour : tmpl[k];
    } else {
        const int8_t *base = st->base3 +
                             (((size_t)level * st->num_slots + i) * 4 + ridx) * area;
        memcpy(dst, base, (size_t)area);
    }
    int32_t row, col;
    tu93_flag_rc(st, ridx, h, &row, &col);
    if (was_attached) dst[(size_t)row * st->pw + col] = TU93_ATTACHED_COLOR;
}

static int tu93_consume_one(Sprites *s, const Tu93Static *st, const Tu93Aux *aux,
                            int32_t level, int32_t i, int mask) {
    int32_t h = s->h[i];
    int grow5 = mask && h == 3;
    int grow7 = mask && h == 5;
    int remove = mask && h != 3 && h != 5;
    if (grow5 || grow7) {
        int32_t ridx = tu93_ridx(aux->rotation[i]);
        int32_t area = st->ph * st->pw;
        const int8_t *tmpl = (grow5 ? st->template5 : st->template7) + (size_t)ridx * area;
        int8_t colour = st->own_color[(size_t)level * st->num_slots + i];
        int8_t *dst = sprite_pixels_mut(s, i);
        for (int32_t k = 0; k < area; k++) dst[k] = tmpl[k] >= 0 ? colour : tmpl[k];
        s->h[i] = grow5 ? 5 : 7;
        s->w[i] = grow5 ? 5 : 7;
        s->x[i] -= 1;
        s->y[i] -= 1;
    }
    if (remove) s->alive[i] = 0;
    return mask ? remove : 1;
}

static void tu93_push_pickup(Sprites *s, const Tu93Static *st, Tu93Aux *aux, int32_t level,
                             int32_t player) {
    int32_t px = s->x[player], py = s->y[player];
    int32_t count = st->push_count[level];
    const int32_t *slots = st->push_slots + (size_t)level * st->max_push;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!s->alive[i]) continue;
        int32_t rot = aux->rotation[i];
        if (!tu93_behind(s->x[i], s->y[i], rot, px, py, TU93_CELL_PX)) continue;
        tu93_set_attached_one(s, st, aux, i);
        int32_t dx, dy;
        tu93_move1(rot, &dx, &dy);
        s->x[i] += dx;
        s->y[i] += dy;
    }
}

static void tu93_drift_advance(Sprites *s, const Tu93Static *st, const Tu93Aux *aux,
                               int32_t level) {
    int32_t count = st->drift_count[level];
    const int32_t *slots = st->drift_slots + (size_t)level * st->max_drift;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!s->alive[i]) continue;
        int32_t dx, dy;
        tu93_move1(aux->rotation[i], &dx, &dy);
        s->x[i] += dx;
        s->y[i] += dy;
    }
}

static void tu93_train_advance(Sprites *s, const Tu93Static *st, const Tu93Aux *aux,
                               int32_t level) {
    int32_t count = st->train_count[level];
    const int32_t *slots = st->train_slots + (size_t)level * st->max_train;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!s->alive[i]) continue;
        if (!tu93_attached(s, st, aux, i)) continue;
        int32_t dx, dy;
        tu93_move1(aux->rotation[i], &dx, &dy);
        s->x[i] += dx;
        s->y[i] += dy;
    }
}

static void tu93_drift_bounce(Sprites *s, const Tu93Static *st, Tu93Aux *aux,
                              int32_t level) {
    int32_t count = st->drift_count[level];
    const int32_t *slots = st->drift_slots + (size_t)level * st->max_drift;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!s->alive[i]) continue;
        int32_t rot = aux->rotation[i];
        int32_t cdx, cdy;
        tu93_move1(rot, &cdx, &cdy);
        int32_t cx = tu93_clip(s->x[i] + cdx * TU93_STEP_PX, 0, FRAME_SIZE - 1);
        int32_t cy = tu93_clip(s->y[i] + cdy * TU93_STEP_PX, 0, FRAME_SIZE - 1);
        int walkable = st->walkable[(size_t)level * FRAME_SIZE * FRAME_SIZE +
                                    (size_t)cy * FRAME_SIZE + cx];
        if (walkable) continue;
        int32_t flipped = tu93_mod(rot + 180, 360);
        tu93_apply_rotation_one(s, st, aux, level, i, flipped);
    }
}

static void tu93_train_pickup_and_dequeue(Sprites *s, const Tu93Static *st, Tu93Aux *aux,
                                          int32_t level, int32_t player) {
    int32_t px = s->x[player], py = s->y[player];
    int32_t count = st->train_count[level];
    const int32_t *slots = st->train_slots + (size_t)level * st->max_train;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!s->alive[i]) continue;
        int already_attached = tu93_attached(s, st, aux, i);
        int32_t rot = aux->rotation[i];
        int hit = !already_attached && tu93_behind(s->x[i], s->y[i], rot, px, py,
                                                    TU93_TRAIN_RANGE_PX);
        if (hit) {
            tu93_set_attached_one(s, st, aux, i);
            aux->queue[i][0] = rot;
            aux->queue[i][1] = rot;
            aux->queue_len[i] = 2;
        }
        int attached_now = tu93_attached(s, st, aux, i);
        int can_pop = attached_now && aux->queue_len[i] > 0;
        if (can_pop) {
            int32_t popped = aux->queue[i][0];
            for (int32_t m = 0; m < TU93_MAX_QUEUE - 1; m++)
                aux->queue[i][m] = aux->queue[i][m + 1];
            aux->queue[i][TU93_MAX_QUEUE - 1] = 0;
            aux->queue_len[i] -= 1;
            tu93_apply_rotation_one(s, st, aux, level, i, popped);
        }
    }
}

static void tu93_finish(Tu93Aux *aux, int32_t *status, uint8_t *action_complete) {
    if (aux->steps <= 0) *status = TU93_GAME_OVER;
    *action_complete = 1;
}

static void tu93_do_move(Sprites *s, const Tu93Static *st, Tu93Aux *aux, int32_t level,
                         int32_t player, int32_t px, int32_t py, int32_t rotation) {
    int32_t count = st->train_count[level];
    const int32_t *slots = st->train_slots + (size_t)level * st->max_train;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!tu93_attached(s, st, aux, i)) continue;
        int32_t pos = tu93_clip(aux->queue_len[i], 0, TU93_MAX_QUEUE - 1);
        aux->queue[i][pos] = rotation;
        aux->queue_len[i] = aux->queue_len[i] + 1 < TU93_MAX_QUEUE ? aux->queue_len[i] + 1
                                                                    : TU93_MAX_QUEUE;
    }
    int32_t dx, dy;
    tu93_move1(rotation, &dx, &dy);
    s->x[player] = px + dx;
    s->y[player] = py + dy;
    aux->phase = 1;
    tu93_apply_rotation_one(s, st, aux, level, player, rotation);
}

static void tu93_phase0(Sprites *s, const Tu93Static *st, int32_t level, int32_t action_id,
                        Tu93Aux *aux, int32_t *status, uint8_t *action_complete) {
    int32_t player = st->player_slot[level];
    int32_t px = s->x[player], py = s->y[player];
    int32_t rotation = action_id == TU93_ACTION1
                           ? 0
                           : (action_id == TU93_ACTION2
                                  ? 180
                                  : (action_id == TU93_ACTION3
                                         ? 270
                                         : (action_id == TU93_ACTION4 ? 90 : -1)));
    int valid_action = rotation >= 0;
    if (valid_action) aux->steps = aux->steps > 1 ? aux->steps - 1 : 0;

    int32_t cdx, cdy;
    tu93_move1(rotation, &cdx, &cdy);
    int32_t check_x = tu93_clip(px + cdx * TU93_STEP_PX, 0, FRAME_SIZE - 1);
    int32_t check_y = tu93_clip(py + cdy * TU93_STEP_PX, 0, FRAME_SIZE - 1);
    int walkable = st->walkable[(size_t)level * FRAME_SIZE * FRAME_SIZE +
                                (size_t)check_y * FRAME_SIZE + check_x];
    int can_move = valid_action && walkable;

    if (can_move) {
        tu93_do_move(s, st, aux, level, player, px, py, rotation);
    } else {
        tu93_finish(aux, status, action_complete);
    }
}

static void tu93_phase1(Sprites *s, const Tu93Static *st, int32_t level, Tu93Aux *aux) {
    int32_t player = st->player_slot[level];
    int32_t board_x = st->board_xy[level * 2 + 0], board_y = st->board_xy[level * 2 + 1];
    int32_t px = s->x[player], py = s->y[player];
    int aligned = tu93_mod(py - board_y, TU93_CELL_PX) == 0 &&
                 tu93_mod(px - board_x, TU93_CELL_PX) == 0;
    if (!aligned) {
        int32_t dx, dy;
        tu93_move1(aux->rotation[player], &dx, &dy);
        s->x[player] += dx;
        s->y[player] += dy;
        return;
    }

    int32_t player_cx = s->x[player] + s->w[player] / 2;
    int32_t player_cy = s->y[player] + s->h[player] / 2;
    int32_t count = st->crate_count[level];
    const int32_t *cslots = st->crate_slots + (size_t)level * st->max_crate;
    int all_done = 1;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = cslots[k];
        int32_t cx = s->x[i] + s->w[i] / 2, cy = s->y[i] + s->h[i] / 2;
        int overlap = s->alive[i] && cx == player_cx && cy == player_cy;
        int resolved = tu93_consume_one(s, st, aux, level, i, overlap);
        if (!resolved) all_done = 0;
    }
    if (!all_done) return;

    tu93_push_pickup(s, st, aux, level, player);
    tu93_drift_advance(s, st, aux, level);
    tu93_train_advance(s, st, aux, level);
    aux->phase = 2;
}

static void tu93_phase2(Sprites *s, const Tu93Static *st, int32_t level, Tu93Aux *aux,
                        int32_t *score, int32_t *status, uint8_t *next_level,
                        uint8_t *action_complete) {
    int32_t player = st->player_slot[level];
    int32_t board_x = st->board_xy[level * 2 + 0], board_y = st->board_xy[level * 2 + 1];
    int32_t count = st->crate_count[level];
    const int32_t *cslots = st->crate_slots + (size_t)level * st->max_crate;

    int32_t pending_n = 0;
    int any_need_move = 0;
    int32_t player_cx = s->x[player] + s->w[player] / 2;
    int32_t player_cy = s->y[player] + s->h[player] / 2;

    for (int32_t k = 0; k < count; k++) {
        int32_t i = cslots[k];
        if (!s->alive[i]) continue;
        int is_push_i = st->is_push[(size_t)level * st->num_slots + i];
        int is_drift_i = st->is_drift[(size_t)level * st->num_slots + i];
        int is_train_i = st->is_train[(size_t)level * st->num_slots + i];
        int attached_i = (is_push_i || is_train_i) ? tu93_attached(s, st, aux, i) : 0;
        int movable = ((is_push_i || is_train_i) && attached_i) || is_drift_i;
        if (!movable) continue;
        int aligned_i = tu93_mod(s->y[i] - board_y, TU93_CELL_PX) == 0 &&
                       tu93_mod(s->x[i] - board_x, TU93_CELL_PX) == 0;
        if (!aligned_i) {
            int32_t dx, dy;
            tu93_move1(aux->rotation[i], &dx, &dy);
            s->x[i] += dx;
            s->y[i] += dy;
            any_need_move = 1;
            continue;
        }
        int32_t cx = s->x[i] + s->w[i] / 2, cy = s->y[i] + s->h[i] / 2;
        if (s->alive[player] && cx == player_cx && cy == player_cy) pending_n++;
    }

    for (int32_t k = 0; k < pending_n; k++) {
        if (!s->alive[player]) break;
        tu93_consume_one(s, st, aux, level, player, 1);
    }

    int player_alive = s->alive[player];
    int still_pending = (pending_n > 0 && player_alive) || any_need_move;
    if (still_pending) return;

    tu93_drift_bounce(s, st, aux, level);
    tu93_train_pickup_and_dequeue(s, st, aux, level, player);

    player_alive = s->alive[player];
    int at_exit = 0;
    int32_t ecount = st->exit_count[level];
    const int32_t *eslots = st->exit_slots + (size_t)level * st->max_exit;
    for (int32_t k = 0; k < ecount; k++) {
        int32_t i = eslots[k];
        if (s->alive[i] && s->x[i] == s->x[player] && s->y[i] == s->y[player]) {
            at_exit = 1;
            break;
        }
    }
    int win = player_alive && at_exit;
    int lose_cond = !player_alive || aux->steps <= 0;
    if (win) {
        int is_last = level == st->num_levels - 1;
        *score += 1;
        *next_level = (uint8_t)(!is_last);
        if (is_last) *status = TU93_WIN;
    } else if (lose_cond) {
        *status = TU93_GAME_OVER;
    }
    aux->phase = 0;
    *action_complete = 1;
}

void tu93_zero_aux(Tu93Aux *aux) {
    aux->phase = 0;
    for (int32_t i = 0; i < TU93_NUM_SLOTS; i++) {
        aux->rotation[i] = 0;
        aux->queue_len[i] = 0;
        for (int32_t j = 0; j < TU93_MAX_QUEUE; j++) aux->queue[i][j] = 0;
    }
    aux->steps = 0;
}

void tu93_on_set_level(Sprites *sprites, const Tu93Static *st, int32_t level,
                       Tu93Aux *aux) {
    aux->phase = 0;
    for (int32_t i = 0; i < st->num_slots; i++) {
        aux->rotation[i] = st->init_rotation[(size_t)level * st->num_slots + i];
        aux->queue_len[i] = 0;
        for (int32_t j = 0; j < TU93_MAX_QUEUE; j++) aux->queue[i][j] = 0;
        sprites->layer[i] = 0;
    }
    aux->steps = st->budget[level];
}

void tu93_step_once(Sprites *sprites, const Camera *camera, const Tu93Static *st,
                    int32_t level, int32_t action_id, int32_t action_x,
                    int32_t action_y, Tu93Aux *aux, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete) {
    (void)camera;
    (void)action_x;
    (void)action_y;
    int32_t phase = tu93_clip(aux->phase, 0, 2);
    if (phase == 0) {
        tu93_phase0(sprites, st, level, action_id, aux, status, action_complete);
    } else if (phase == 1) {
        tu93_phase1(sprites, st, level, aux);
    } else {
        tu93_phase2(sprites, st, level, aux, score, status, next_level, action_complete);
    }
}

void tu93_render_interface(int8_t *frame, const Sprites *sprites, const Camera *camera,
                           const Tu93Static *st, int32_t level, const Tu93Aux *aux) {
    (void)sprites;
    (void)camera;
    int32_t budget = st->budget[level];
    if (budget == 0) return;
    int32_t steps = tu93_clip(aux->steps, 0, budget);
    int32_t total = FRAME_SIZE * steps;
    int32_t whole = total / budget;
    int32_t rest = total % budget;
    int round_up = 2 * rest > budget || (2 * rest == budget && whole % 2 == 1);
    int32_t filled = whole + (round_up ? 1 : 0);
    if (filled > FRAME_SIZE) filled = FRAME_SIZE;
    int8_t *row = frame + (size_t)(FRAME_SIZE - 1) * FRAME_SIZE;
    for (int32_t c = 0; c < FRAME_SIZE; c++)
        row[c] = (int8_t)(c < filled ? TU93_HUD_FILLED : 0);
}
