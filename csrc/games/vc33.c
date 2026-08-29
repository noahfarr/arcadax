#include "vc33.h"

#include <stddef.h>

enum { VC33_ACTION6 = 6 };
enum { VC33_WIN = 2, VC33_GAME_OVER = 3 };
enum { VC33_COUPLER_ACTIVE_COLOR = 12, VC33_COUPLER_INACTIVE_COLOR = 1 };
enum { VC33_BUDGET_FILLED_COLOR = 7, VC33_BUDGET_EMPTY_COLOR = 4 };
enum { VC33_SIDE_LEFT = 0, VC33_SIDE_RIGHT = 1 };

static inline size_t vc33_ln(const Vc33Static *st, int32_t level, int32_t slot) {
    return (size_t)level * (size_t)st->num_slots + (size_t)slot;
}

static inline size_t vc33_lnn(const Vc33Static *st, int32_t level, int32_t a, int32_t b) {
    return (vc33_ln(st, level, a)) * (size_t)st->num_slots + (size_t)b;
}

static inline size_t vc33_lc(int32_t level, int32_t k) {
    return (size_t)level * (size_t)VC33_MAX_COUPLERS + (size_t)k;
}

static inline int vc33_horiz(const Vc33Static *st, int32_t level) {
    return st->grav_x[level] != 0;
}

static inline int vc33_positive(const Vc33Static *st, int32_t level) {
    return st->grav_x[level] > 0 || st->grav_y[level] > 0;
}

static inline int32_t vc33_signed(const Vc33Static *st, int32_t level) {
    return vc33_horiz(st, level) ? st->grav_x[level] : st->grav_y[level];
}

static inline int32_t vc33_along(const Sprites *s, const Vc33Static *st, int32_t level, int32_t idx) {
    return vc33_horiz(st, level) ? s->x[idx] : s->y[idx];
}

static inline int32_t vc33_along_size(const Sprites *s, const Vc33Static *st, int32_t level, int32_t idx) {
    return vc33_horiz(st, level) ? s->w[idx] : s->h[idx];
}

static inline int32_t vc33_perp(const Sprites *s, const Vc33Static *st, int32_t level, int32_t idx) {
    return vc33_horiz(st, level) ? s->y[idx] : s->x[idx];
}

static inline int32_t vc33_perp_size(const Sprites *s, const Vc33Static *st, int32_t level, int32_t idx) {
    return vc33_horiz(st, level) ? s->h[idx] : s->w[idx];
}

static inline int32_t vc33_front(const Sprites *s, const Vc33Static *st, int32_t level, int32_t idx) {
    int32_t a = vc33_along(s, st, level, idx);
    int32_t sz = vc33_along_size(s, st, level, idx);
    return vc33_positive(st, level) ? a + sz : a;
}

static inline int32_t vc33_back(const Sprites *s, const Vc33Static *st, int32_t level, int32_t idx) {
    int32_t a = vc33_along(s, st, level, idx);
    int32_t sz = vc33_along_size(s, st, level, idx);
    return vc33_positive(st, level) ? a : a + sz;
}

static int vc33_attached(const Sprites *s, const Vc33Static *st, int32_t level,
                         int32_t sensor_idx, int32_t pipe_idx) {
    int32_t perp_s = vc33_perp(s, st, level, sensor_idx);
    int32_t front_s = vc33_front(s, st, level, sensor_idx);
    int32_t perp_p = vc33_perp(s, st, level, pipe_idx);
    int32_t perp_size_p = vc33_perp_size(s, st, level, pipe_idx);
    int32_t back_p = vc33_back(s, st, level, pipe_idx);
    return perp_s >= perp_p && perp_s < perp_p + perp_size_p && front_s == back_p;
}

static int32_t vc33_first_pipe(const Sprites *s, const Vc33Static *st, int32_t level,
                               int32_t coupler_idx, int side) {
    int32_t front_c = vc33_front(s, st, level, coupler_idx);
    int32_t perp_c = vc33_perp(s, st, level, coupler_idx);
    int32_t perp_size_c = vc33_perp_size(s, st, level, coupler_idx);
    int32_t count = st->pipe_count[level];
    const int32_t *slots = st->pipe_slots + (size_t)level * st->max_pipes;
    for (int32_t k = 0; k < count; k++) {
        int32_t p = slots[k];
        if (vc33_back(s, st, level, p) != front_c) continue;
        int32_t perp_p = vc33_perp(s, st, level, p);
        int32_t perp_size_p = vc33_perp_size(s, st, level, p);
        int match = side == VC33_SIDE_LEFT ? (perp_p + perp_size_p == perp_c)
                                           : (perp_p == perp_c + perp_size_c);
        if (match) return p;
    }
    return -1;
}

static int vc33_coupler_active(const Sprites *s, const Vc33Static *st, int32_t level, int32_t coupler_idx) {
    return vc33_first_pipe(s, st, level, coupler_idx, VC33_SIDE_LEFT) >= 0 &&
           vc33_first_pipe(s, st, level, coupler_idx, VC33_SIDE_RIGHT) >= 0;
}

static int32_t vc33_growth_cap(const Sprites *s, const Vc33Static *st, int32_t level, int32_t pipe_idx) {
    int has_sensor = 0;
    int32_t count = st->sensor_count[level];
    const int32_t *slots = st->sensor_slots + (size_t)level * st->max_sensors;
    for (int32_t k = 0; k < count && !has_sensor; k++) {
        if (vc33_attached(s, st, level, slots[k], pipe_idx)) has_sensor = 1;
    }
    size_t idx = vc33_ln(st, level, pipe_idx);
    int32_t floor_val = st->pipe_floor_max_front[idx] - (has_sensor ? st->sensor_adjust[level] : 0);
    int32_t wall_val = st->pipe_wall_extreme[idx];
    return st->pipe_uses_floor[idx] ? floor_val : wall_val;
}

static void vc33_write_pipe(Sprites *s, int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h) {
    s->x[idx] = x;
    s->y[idx] = y;
    s->w[idx] = w;
    s->h[idx] = h;
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int8_t *patch = sprite_pixels_mut(s, idx);
    for (int32_t r = 0; r < ph; r++) {
        for (int32_t c = 0; c < pw; c++) {
            patch[(size_t)r * pw + c] = (r < h && c < w) ? (int8_t)0 : (int8_t)-1;
        }
    }
}

static void vc33_refresh_couplers(Sprites *s, const Vc33Static *st, int32_t level, int32_t *next_order) {
    for (int32_t k = 0; k < VC33_MAX_COUPLERS; k++) remove_sprite(s, st->icon_base + k);
    for (int32_t k = 0; k < VC33_MAX_COUPLERS; k++) {
        int32_t coupler_slot = st->level_coupler_slot[vc33_lc(level, k)];
        if (coupler_slot < 0) continue;
        int active = vc33_coupler_active(s, st, level, coupler_slot);
        int8_t new_color = active ? VC33_COUPLER_ACTIVE_COLOR : VC33_COUPLER_INACTIVE_COLOR;
        color_remap(s, coupler_slot, 0, 0, new_color);
        if (active) {
            int32_t icon_slot = st->icon_base + k;
            int32_t icon_x = s->x[coupler_slot] + st->icon_dx[vc33_lc(level, k)];
            int32_t icon_y = s->y[coupler_slot] + st->icon_dy[vc33_lc(level, k)];
            set_position(s, icon_slot, icon_x, icon_y);
            set_interaction(s, icon_slot, TANGIBLE);
            add_sprite(s, icon_slot, *next_order);
            (*next_order)++;
        }
    }
}

static void vc33_resize(Sprites *s, const Vc33Static *st, int32_t level, int32_t lever_idx,
                        int32_t *next_order) {
    int32_t src = st->lever_src[vc33_ln(st, level, lever_idx)];
    int32_t dst = st->lever_dst[vc33_ln(st, level, lever_idx)];
    if (src < 0 || dst < 0) return;

    int32_t src_size = vc33_along_size(s, st, level, src);
    if (src_size <= 0) return;

    int32_t dst_back = vc33_back(s, st, level, dst);
    int32_t cap = vc33_growth_cap(s, st, level, dst);
    int positive = vc33_positive(st, level);
    int guard2 = positive ? (dst_back > cap) : (dst_back < cap);
    if (!guard2) return;

    int32_t gx = st->grav_x[level], gy = st->grav_y[level], g = vc33_signed(st, level);
    int32_t scount = st->sensor_count[level];
    const int32_t *sslots = st->sensor_slots + (size_t)level * st->max_sensors;
    for (int32_t k = 0; k < scount; k++) {
        int32_t sidx = sslots[k];
        int a_src = vc33_attached(s, st, level, sidx, src);
        int a_dst = vc33_attached(s, st, level, sidx, dst);
        int32_t x = s->x[sidx], y = s->y[sidx];
        if (a_src) { x += gx; y += gy; }
        if (a_dst) { x -= gx; y -= gy; }
        s->x[sidx] = x;
        s->y[sidx] = y;
    }

    int horiz = vc33_horiz(st, level);
    int32_t a_src = vc33_along(s, st, level, src), s_src = vc33_along_size(s, st, level, src);
    int32_t new_a_src = a_src + (g > 0 ? g : 0), new_s_src = s_src - (g < 0 ? -g : g);
    vc33_write_pipe(s, src, horiz ? new_a_src : s->x[src], horiz ? s->y[src] : new_a_src,
                    horiz ? new_s_src : s->w[src], horiz ? s->h[src] : new_s_src);

    int32_t a_dst = vc33_along(s, st, level, dst), s_dst = vc33_along_size(s, st, level, dst);
    int32_t new_a_dst = a_dst - (g > 0 ? g : 0), new_s_dst = s_dst + (g < 0 ? -g : g);
    vc33_write_pipe(s, dst, horiz ? new_a_dst : s->x[dst], horiz ? s->y[dst] : new_a_dst,
                    horiz ? new_s_dst : s->w[dst], horiz ? s->h[dst] : new_s_dst);

    vc33_refresh_couplers(s, st, level, next_order);
}

static void vc33_sensor_target(const Sprites *s, const Vc33Static *st, int32_t level, int32_t i,
                               int32_t other_perp, int32_t other_perp_size, int32_t *tx, int32_t *ty) {
    int32_t sp_size = vc33_perp_size(s, st, level, i);
    int32_t new_perp = other_perp + other_perp_size / 2 - sp_size / 2;
    int horiz = vc33_horiz(st, level);
    *tx = horiz ? s->x[i] : new_perp;
    *ty = horiz ? new_perp : s->y[i];
}

static void vc33_add_pass(const Sprites *s, const Vc33Static *st, int32_t level, int32_t pipe_c,
                          int32_t other_perp, int32_t other_perp_size,
                          int32_t *slot_arr, int32_t *tx_arr, int32_t *ty_arr, int32_t *ptr) {
    int32_t count = st->sensor_count[level];
    const int32_t *slots = st->sensor_slots + (size_t)level * st->max_sensors;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = slots[k];
        if (!vc33_attached(s, st, level, i, pipe_c)) continue;
        int32_t tx, ty;
        vc33_sensor_target(s, st, level, i, other_perp, other_perp_size, &tx, &ty);
        int32_t p = *ptr;
        if (p < 0) p = 0;
        if (p > VC33_MAX_QUEUE - 1) p = VC33_MAX_QUEUE - 1;
        slot_arr[p] = i;
        tx_arr[p] = tx;
        ty_arr[p] = ty;
        (*ptr)++;
    }
}

static void vc33_build_queue(const Sprites *s, const Vc33Static *st, int32_t level, int32_t coupler_idx,
                             int32_t *slot_arr, int32_t *tx_arr, int32_t *ty_arr, int32_t *queue_len) {
    int32_t left_pipe = vc33_first_pipe(s, st, level, coupler_idx, VC33_SIDE_LEFT);
    int32_t right_pipe = vc33_first_pipe(s, st, level, coupler_idx, VC33_SIDE_RIGHT);
    int32_t left_pipe_c = left_pipe < 0 ? 0 : left_pipe;
    int32_t right_pipe_c = right_pipe < 0 ? 0 : right_pipe;

    int32_t rp_perp = vc33_perp(s, st, level, right_pipe_c);
    int32_t rp_perp_size = vc33_perp_size(s, st, level, right_pipe_c);
    int32_t lp_perp = vc33_perp(s, st, level, left_pipe_c);
    int32_t lp_perp_size = vc33_perp_size(s, st, level, left_pipe_c);

    int32_t cx0 = s->x[coupler_idx], cy0 = s->y[coupler_idx];
    int32_t cw = s->w[coupler_idx], ch = s->h[coupler_idx];
    int32_t gx = st->grav_x[level];
    int32_t first_dx = gx > 0 ? -cw : (gx < 0 ? cw : 0);
    int32_t first_dy = gx == 0 ? -ch : 0;

    for (int32_t k = 0; k < VC33_MAX_QUEUE; k++) {
        slot_arr[k] = -1;
        tx_arr[k] = 0;
        ty_arr[k] = 0;
    }
    slot_arr[0] = coupler_idx;
    tx_arr[0] = cx0 + first_dx;
    ty_arr[0] = cy0 + first_dy;

    int32_t ptr = 1;
    vc33_add_pass(s, st, level, left_pipe_c, rp_perp, rp_perp_size, slot_arr, tx_arr, ty_arr, &ptr);
    vc33_add_pass(s, st, level, right_pipe_c, lp_perp, lp_perp_size, slot_arr, tx_arr, ty_arr, &ptr);

    int32_t p = ptr;
    if (p < 0) p = 0;
    if (p > VC33_MAX_QUEUE - 1) p = VC33_MAX_QUEUE - 1;
    slot_arr[p] = coupler_idx;
    tx_arr[p] = cx0;
    ty_arr[p] = cy0;
    *queue_len = ptr + 1;
}

static int vc33_win_check(const Sprites *s, const Vc33Static *st, int32_t level) {
    int32_t scount = st->sensor_count[level];
    const int32_t *sslots = st->sensor_slots + (size_t)level * st->max_sensors;
    int32_t pcount = st->pipe_count[level];
    const int32_t *pslots = st->pipe_slots + (size_t)level * st->max_pipes;
    int32_t mcount = st->marker_count[level];
    const int32_t *mslots = st->marker_slots + (size_t)level * st->max_markers;

    for (int32_t si = 0; si < scount; si++) {
        int32_t sensor = sslots[si];
        int32_t matched_pipe = -1;
        for (int32_t pi = 0; pi < pcount; pi++) {
            int32_t p = pslots[pi];
            if (vc33_attached(s, st, level, sensor, p)) {
                matched_pipe = p;
                break;
            }
        }
        int32_t mp = matched_pipe < 0 ? 0 : matched_pipe;
        int32_t sensor_along = vc33_along(s, st, level, sensor);

        int ok = 0;
        for (int32_t mi = 0; mi < mcount && !ok; mi++) {
            int32_t marker = mslots[mi];
            size_t midx = vc33_ln(st, level, marker);
            int along_ok = st->marker_along[midx] == sensor_along;
            int color_ok = st->sensor_marker_color[vc33_lnn(st, level, sensor, marker)];
            int32_t wall_col = st->marker_wall[midx];
            if (wall_col < 0) wall_col = 0;
            int wall_ok = st->wall_touch_mask[vc33_lnn(st, level, mp, wall_col)];
            if (along_ok && color_ok && wall_ok) ok = 1;
        }
        if (!ok) return 0;
    }
    return 1;
}

static void vc33_resolve(const Sprites *s, const Vc33Static *st, int32_t level, const Vc33Aux *aux,
                         int32_t *score, int32_t *status, uint8_t *next_level, uint8_t *action_complete) {
    int won = vc33_win_check(s, st, level);
    if (won) {
        int is_last = level == st->num_levels - 1;
        *score += 1;
        *next_level = (uint8_t)!is_last;
        if (is_last) *status = VC33_WIN;
    } else if (aux->steps == 0) {
        *status = VC33_GAME_OVER;
    }
    *action_complete = 1;
}

static void vc33_display_to_grid(const Camera *camera, int32_t display_x, int32_t display_y,
                                 int32_t *world_x, int32_t *world_y, int *valid) {
    int32_t scale, x_pad, y_pad;
    scale_and_offset(camera, &scale, &x_pad, &y_pad);
    int32_t dx = display_x - x_pad, dy = display_y - y_pad;
    int32_t grid_x = dx >= 0 ? dx / scale : -1;
    int32_t grid_y = dy >= 0 ? dy / scale : -1;
    *valid = grid_x >= 0 && grid_y >= 0 && grid_x < camera->width && grid_y < camera->height;
    *world_x = grid_x + camera->x;
    *world_y = grid_y + camera->y;
}

static void vc33_handle_action(Sprites *s, const Camera *camera, const Vc33Static *st, int32_t level,
                               int32_t action_id, int32_t action_x, int32_t action_y, Vc33Aux *aux,
                               int32_t *next_order, int32_t *score, int32_t *status,
                               uint8_t *next_level, uint8_t *action_complete) {
    if (action_id == VC33_ACTION6) {
        aux->steps = aux->steps > 0 ? aux->steps - 1 : 0;

        int32_t wx, wy;
        int on_board;
        vc33_display_to_grid(camera, action_x, action_y, &wx, &wy, &on_board);
        int32_t hit = on_board ? get_sprite_at(s, wx, wy, -1, 0) : -1;
        int32_t hc = hit < 0 ? 0 : hit;
        int is_lever = hit >= 0 && st->lever_mask[vc33_ln(st, level, hc)];
        int is_coupler = hit >= 0 && st->coupler_mask[vc33_ln(st, level, hc)];

        if (is_lever) {
            vc33_resize(s, st, level, hc, next_order);
        } else if (is_coupler) {
            if (vc33_coupler_active(s, st, level, hc)) {
                int32_t slot_arr[VC33_MAX_QUEUE], tx_arr[VC33_MAX_QUEUE], ty_arr[VC33_MAX_QUEUE], qlen;
                vc33_build_queue(s, st, level, hc, slot_arr, tx_arr, ty_arr, &qlen);
                for (int32_t k = 0; k < VC33_MAX_COUPLERS; k++) {
                    int32_t slot = st->icon_base + k;
                    if (s->alive[slot]) set_visible(s, slot, 0);
                }
                for (int32_t k = 0; k < VC33_MAX_QUEUE; k++) {
                    aux->queue_slot[k] = slot_arr[k];
                    aux->queue_tx[k] = tx_arr[k];
                    aux->queue_ty[k] = ty_arr[k];
                }
                aux->queue_len = qlen;
                aux->queue_idx = 0;
            }
        }
    }

    if (aux->queue_len > 0) return;
    vc33_resolve(s, st, level, aux, score, status, next_level, action_complete);
}

static void vc33_continue_queue(Sprites *s, const Vc33Static *st, int32_t level, Vc33Aux *aux,
                                int32_t *next_order, int32_t *score, int32_t *status,
                                uint8_t *next_level, uint8_t *action_complete) {
    int32_t idx = aux->queue_idx;
    int32_t slot = aux->queue_slot[idx];
    int32_t tx = aux->queue_tx[idx], ty = aux->queue_ty[idx];
    int32_t cx = s->x[slot], cy = s->y[slot];
    int32_t dx = tx - cx, dy = ty - cy;

    if (dx == 0 && dy == 0) {
        int32_t new_idx = idx + 1;
        if (new_idx >= aux->queue_len) {
            aux->queue_idx = 0;
            aux->queue_len = 0;
            vc33_refresh_couplers(s, st, level, next_order);
            vc33_resolve(s, st, level, aux, score, status, next_level, action_complete);
        } else {
            aux->queue_idx = new_idx;
        }
    } else {
        int32_t adx = dx < 0 ? -dx : dx;
        int32_t ady = dy < 0 ? -dy : dy;
        int use_dx = adx >= ady;
        int32_t nx = cx + (use_dx ? (dx > 0 ? 1 : (dx < 0 ? -1 : 0)) : 0);
        int32_t ny = cy + (use_dx ? 0 : (dy > 0 ? 1 : (dy < 0 ? -1 : 0)));
        set_position(s, slot, nx, ny);
    }
}

void vc33_zero_aux(Vc33Aux *aux) {
    aux->steps = 0;
    for (int32_t k = 0; k < VC33_MAX_QUEUE; k++) {
        aux->queue_slot[k] = -1;
        aux->queue_tx[k] = 0;
        aux->queue_ty[k] = 0;
    }
    aux->queue_len = 0;
    aux->queue_idx = 0;
}

void vc33_on_set_level(const Vc33Static *st, int32_t level, Vc33Aux *aux) {
    vc33_zero_aux(aux);
    aux->steps = st->budget[level];
}

void vc33_step_once(Sprites *sprites, const Camera *camera, const Vc33Static *st, int32_t level,
                    int32_t action_id, int32_t action_x, int32_t action_y, Vc33Aux *aux,
                    int32_t *next_order, int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    if (aux->queue_len > 0) {
        vc33_continue_queue(sprites, st, level, aux, next_order, score, status, next_level,
                            action_complete);
    } else {
        vc33_handle_action(sprites, camera, st, level, action_id, action_x, action_y, aux,
                           next_order, score, status, next_level, action_complete);
    }
}

void vc33_render_interface(int8_t *frame, const Vc33Static *st, int32_t level, const Vc33Aux *aux) {
    int32_t budget = st->budget[level];
    if (budget == 0) return;

    int32_t steps = aux->steps;
    int32_t total = FRAME_SIZE * steps;
    int32_t whole = total / budget, rest = total % budget;
    int round_up = 2 * rest > budget || (2 * rest == budget && whole % 2 == 1);
    int32_t filled = whole + (round_up ? 1 : 0);
    if (filled < 0) filled = 0;
    if (filled > FRAME_SIZE) filled = FRAME_SIZE;

    for (int32_t c = 0; c < FRAME_SIZE; c++) {
        frame[c] = (int8_t)(c < filled ? VC33_BUDGET_FILLED_COLOR : VC33_BUDGET_EMPTY_COLOR);
    }
}
