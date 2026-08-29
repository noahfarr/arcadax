#include "tn36.h"

#include <string.h>

enum { TN36_ACTION6 = 6 };
enum { TN36_WIN = 2, TN36_GAME_OVER = 3 };
enum { TN36_PITCH = 4 };
enum {
    TN36_COLOR_UNSELECTED = 2,
    TN36_COLOR_ON = 5,
    TN36_COLOR_OFF = 1,
    TN36_COLOR_SELECTED = 9,
    TN36_COLOR_FLASH = 14,
    TN36_SWITCH_HIGHLIGHT = 10
};

static const int8_t TN36_GHOST_PATTERN[4][4] = {
    {12, 13, 12, 13},
    {13, 12, 13, 12},
    {12, 13, 12, 13},
    {13, -1, -1, 12},
};

static const int32_t TN36_DX[64] = {[1] = -4, [2] = 4, [10] = 8, [11] = 8, [12] = -8, [13] = -8, [34] = -4};
static const int32_t TN36_DY[64] = {[3] = 4, [33] = -4};
static const uint8_t TN36_IS_MOVE[64] = {[1] = 1, [2] = 1, [3] = 1, [10] = 1, [11] = 1,
                                         [12] = 1, [13] = 1, [33] = 1, [34] = 1};
static const int32_t TN36_DROT[64] = {[5] = 90, [6] = -90, [7] = 180, [16] = 270};
static const uint8_t TN36_IS_ROTATE[64] = {[5] = 1, [6] = 1, [7] = 1, [16] = 1};
static const int32_t TN36_DSCALE[64] = {[8] = 1, [9] = -1};
static const uint8_t TN36_IS_SCALE[64] = {[8] = 1, [9] = 1};
static const int32_t TN36_RECOLOR[64] = {[14] = 9, [15] = 8, [63] = 15};
static const uint8_t TN36_IS_RECOLOR[64] = {[14] = 1, [15] = 1, [63] = 1};

static inline int32_t tn36_floordiv(int32_t a, int32_t b) {
    int32_t q = a / b, r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q--;
    return q;
}

static inline int32_t tn36_pymod(int32_t a, int32_t b) {
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

static inline int32_t tn36_li(int32_t level, int32_t lane) { return level * 2 + lane; }

static inline int32_t tn36_robot_slot(const Tn36Static *st, int32_t level, int32_t lane) {
    return st->robot_slot[tn36_li(level, lane)];
}

static inline const int32_t *tn36_box(const int32_t *base, int32_t level, int32_t lane) {
    return base + (size_t)tn36_li(level, lane) * 4;
}

static void tn36_display_to_grid(const Camera *camera, int32_t display_x, int32_t display_y,
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

static int tn36_box_hit(const int32_t *box, int32_t x, int32_t y) {
    return x >= box[0] && y >= box[1] && x < box[0] + box[2] && y < box[1] + box[3];
}

static void tn36_synth(int8_t *canvas, int32_t ph, int32_t pw, const int8_t pattern[4][4],
                       int32_t rotation_deg, int32_t scale) {
    int32_t safe_scale = scale > 1 ? scale : 1;
    int32_t k = tn36_pymod(4 - tn36_floordiv(rotation_deg, 90), 4);
    for (int32_t i = 0; i < ph; i++) {
        int32_t bi = i / safe_scale;
        int in_row = bi < 4;
        int32_t cbi = bi > 3 ? 3 : bi;
        for (int32_t j = 0; j < pw; j++) {
            int32_t bj = j / safe_scale;
            int in_range = in_row && bj < 4;
            int32_t cbj = bj > 3 ? 3 : bj;
            int8_t val;
            switch (k) {
                case 1: val = pattern[cbj][3 - cbi]; break;
                case 2: val = pattern[3 - cbi][3 - cbj]; break;
                case 3: val = pattern[3 - cbj][cbi]; break;
                default: val = pattern[cbi][cbj]; break;
            }
            canvas[(size_t)i * pw + j] = in_range ? val : -1;
        }
    }
}

static void tn36_rerender_robot(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                                const Tn36Aux *aux) {
    int32_t slot = tn36_robot_slot(st, level, lane);
    int8_t *canvas = sprite_pixels_mut(s, slot);
    tn36_synth(canvas, s->atlas->ph, s->atlas->pw, aux->pattern[lane], aux->rotation[lane], aux->scale[lane]);
    int32_t size = 4 * aux->scale[lane];
    s->w[slot] = size;
    s->h[slot] = size;
}

static int tn36_target_match(const Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                             const Tn36Aux *aux) {
    int32_t target_slot = st->target_slot[tn36_li(level, lane)];
    if (target_slot < 0) return 0;
    int32_t robot_slot = tn36_robot_slot(st, level, lane);
    if (s->x[robot_slot] != st->target_touch_x[tn36_li(level, lane)]) return 0;
    if (s->y[robot_slot] != st->target_touch_y[tn36_li(level, lane)]) return 0;
    if (aux->rotation[lane] != st->target_rotation[tn36_li(level, lane)]) return 0;
    if (aux->scale[lane] != st->target_scale[tn36_li(level, lane)]) return 0;
    if (aux->pattern[lane][1][1] != (int8_t)st->target_color[tn36_li(level, lane)]) return 0;
    return 1;
}

static void tn36_checkpoint_scan(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                                 Tn36Aux *aux, int save) {
    int32_t robot_slot = tn36_robot_slot(st, level, lane);
    int32_t rx = s->x[robot_slot], ry = s->y[robot_slot];
    int32_t rs = 4 * aux->scale[lane];
    int32_t n = st->num_checkpoints[tn36_li(level, lane)];
    const int32_t *slots = st->checkpoint_slot + (size_t)tn36_li(level, lane) * TN36_MAX_CHECKPOINTS;
    for (int32_t k = 0; k < n; k++) {
        int32_t slot = slots[k];
        if (slot < 0) continue;
        int32_t cx = s->x[slot], cy = s->y[slot], cw = s->w[slot], ch = s->h[slot];
        int overlap = rx < cx + cw && rx + rs > cx && ry < cy + ch && ry + rs > cy;
        int match = overlap && cw == rs;
        set_visible(s, slot, !match);
        if (match) {
            if (save) {
                aux->saved_x[lane] = rx;
                aux->saved_y[lane] = ry;
                aux->saved_rotation[lane] = aux->rotation[lane];
                aux->saved_scale[lane] = aux->scale[lane];
                memcpy(aux->saved_pattern[lane], aux->pattern[lane], sizeof aux->pattern[lane]);
            }
            return;
        }
    }
}

static void tn36_toggle_hazards_on_failure(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane) {
    int32_t n = st->num_hazards[tn36_li(level, lane)];
    const int32_t *trig = st->hazard_trig + (size_t)tn36_li(level, lane) * TN36_MAX_HAZARDS;
    for (int32_t k = 0; k < n; k++) {
        int32_t slot = trig[k];
        if (slot < 0) continue;
        set_visible(s, slot, !sprite_visible(s, slot));
    }
}

static void tn36_kill(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane, Tn36Aux *aux,
                      int32_t *next_order) {
    int32_t robot_slot = tn36_robot_slot(st, level, lane);
    int32_t ghost_slot = st->num_slots - 2 + lane;
    set_visible(s, robot_slot, 0);
    int8_t *canvas = sprite_pixels_mut(s, ghost_slot);
    tn36_synth(canvas, s->atlas->ph, s->atlas->pw, TN36_GHOST_PATTERN, aux->rotation[lane], aux->scale[lane]);
    int32_t size = 4 * aux->scale[lane];
    s->w[ghost_slot] = size;
    s->h[ghost_slot] = size;
    s->x[ghost_slot] = s->x[robot_slot];
    s->y[ghost_slot] = s->y[robot_slot];
    s->layer[ghost_slot] = s->layer[robot_slot];
    add_sprite(s, ghost_slot, *next_order);
    (*next_order)++;
    set_visible(s, ghost_slot, 1);
    aux->dead[lane] = 1;
}

static void tn36_toggle_hazards_blink(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                                      Tn36Aux *aux, int32_t *next_order) {
    int32_t n = st->num_hazards[tn36_li(level, lane)];
    const int32_t *trig = st->hazard_trig + (size_t)tn36_li(level, lane) * TN36_MAX_HAZARDS;
    int32_t robot_slot = tn36_robot_slot(st, level, lane);
    int32_t rx = s->x[robot_slot], ry = s->y[robot_slot];
    int32_t rsize = 4 * aux->scale[lane];
    for (int32_t k = 0; k < n; k++) {
        int32_t slot = trig[k];
        if (slot < 0) continue;
        int new_vis = !sprite_visible(s, slot);
        set_visible(s, slot, new_vis);
        int32_t tx = s->x[slot], ty = s->y[slot], tw = s->w[slot], th = s->h[slot];
        int overlap = rx < tx + tw && rx + rsize > tx && ry < ty + th && ry + rsize > ty;
        if (new_vis && overlap) tn36_kill(s, st, level, lane, aux, next_order);
    }
}

static int tn36_hits_walls(const int32_t *box, int32_t n, int32_t x, int32_t y, int32_t size) {
    for (int32_t k = 0; k < n; k++) {
        const int32_t *b = box + (size_t)k * 4;
        if (x < b[0] + b[2] && x + size > b[0] && y < b[1] + b[3] && y + size > b[1]) return 1;
    }
    return 0;
}

static int tn36_hazard_hit(const Sprites *s, const Tn36Static *st, int32_t level, int32_t lane, int32_t x,
                           int32_t y, int32_t size) {
    int32_t n = st->num_hazards[tn36_li(level, lane)];
    const int32_t *icon = st->hazard_icon + (size_t)tn36_li(level, lane) * TN36_MAX_HAZARDS;
    const int32_t *trig = st->hazard_trig + (size_t)tn36_li(level, lane) * TN36_MAX_HAZARDS;
    for (int32_t k = 0; k < n; k++) {
        int32_t ti = trig[k], ii = icon[k];
        int trig_visible = sprite_visible(s, ti);
        int32_t bx = trig_visible ? s->x[ti] : s->x[ii];
        int32_t by = trig_visible ? s->y[ti] : s->y[ii];
        int32_t bw = trig_visible ? s->w[ti] : s->w[ii];
        int32_t bh = trig_visible ? s->h[ti] : s->h[ii];
        if (x < bx + bw && x + size > bx && y < by + bh && y + size > by) return 1;
    }
    return 0;
}

static void tn36_try_move_or_scale(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                                   int32_t code, Tn36Aux *aux, int32_t *next_order) {
    int32_t robot_slot = tn36_robot_slot(st, level, lane);
    int32_t old_x = s->x[robot_slot], old_y = s->y[robot_slot];
    int32_t old_scale = aux->scale[lane];
    int32_t dx = TN36_DX[code], dy = TN36_DY[code], dscale = TN36_DSCALE[code];
    int32_t new_x = old_x + dx, new_y = old_y + dy;
    int32_t new_scale = old_scale + dscale;
    int32_t new_size = 4 * new_scale;

    const int32_t *walls = st->wall_box + (size_t)tn36_li(level, lane) * TN36_MAX_WALLS * 4;
    int32_t n_walls = st->num_walls[tn36_li(level, lane)];
    int blocked = tn36_hits_walls(walls, n_walls, new_x, new_y, new_size);

    int32_t final_x = blocked ? old_x : new_x;
    int32_t final_y = blocked ? old_y : new_y;
    int32_t final_scale = blocked ? old_scale : new_scale;
    int32_t final_size = 4 * final_scale;

    aux->scale[lane] = final_scale;
    s->x[robot_slot] = final_x;
    s->y[robot_slot] = final_y;
    tn36_rerender_robot(s, st, level, lane, aux);

    if (tn36_hazard_hit(s, st, level, lane, final_x, final_y, final_size))
        tn36_kill(s, st, level, lane, aux, next_order);

    tn36_checkpoint_scan(s, st, level, lane, aux, 0);
}

static void tn36_execute(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane, int32_t code,
                         Tn36Aux *aux, int32_t *next_order) {
    int32_t idx = code < 0 ? 0 : (code > 63 ? 63 : code);
    if (TN36_IS_MOVE[idx] || TN36_IS_SCALE[idx]) {
        if (!aux->dead[lane]) tn36_try_move_or_scale(s, st, level, lane, idx, aux, next_order);
    } else if (TN36_IS_ROTATE[idx]) {
        aux->rotation[lane] = tn36_pymod(aux->rotation[lane] + TN36_DROT[idx], 360);
        tn36_rerender_robot(s, st, level, lane, aux);
    } else if (TN36_IS_RECOLOR[idx]) {
        int8_t newcolor = (int8_t)TN36_RECOLOR[idx];
        const uint8_t *mask = st->robot_mask + (size_t)tn36_li(level, lane) * 16;
        for (int32_t r = 0; r < 4; r++)
            for (int32_t c = 0; c < 4; c++) aux->pattern[lane][r][c] = mask[r * 4 + c] ? newcolor : -1;
        tn36_rerender_robot(s, st, level, lane, aux);
    }
}

static void tn36_begin_success_flash(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                                     Tn36Aux *aux) {
    int8_t flash_pattern[4][4];
    const uint8_t *mask = st->robot_mask + (size_t)tn36_li(level, lane) * 16;
    for (int32_t r = 0; r < 4; r++)
        for (int32_t c = 0; c < 4; c++) flash_pattern[r][c] = mask[r * 4 + c] ? TN36_COLOR_FLASH : -1;
    int32_t robot_slot = tn36_robot_slot(st, level, lane);
    int8_t *canvas = sprite_pixels_mut(s, robot_slot);
    tn36_synth(canvas, s->atlas->ph, s->atlas->pw, flash_pattern, aux->rotation[lane], aux->scale[lane]);

    int32_t target_slot = st->target_slot[tn36_li(level, lane)];
    if (target_slot >= 0) {
        int32_t ph = s->atlas->ph, pw = s->atlas->pw;
        const int8_t *clean = st->clean_target_pixels + (size_t)tn36_li(level, lane) * ph * pw;
        int8_t *patch = sprite_pixels_mut(s, target_slot);
        for (int32_t p = 0; p < ph * pw; p++) patch[p] = clean[p] >= 0 ? TN36_COLOR_FLASH : -1;
    }
    aux->final_flash = 1;
}

static void tn36_end_success(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane, Tn36Aux *aux) {
    tn36_rerender_robot(s, st, level, lane, aux);
    int32_t target_slot = st->target_slot[tn36_li(level, lane)];
    if (target_slot >= 0) {
        int32_t ph = s->atlas->ph, pw = s->atlas->pw;
        const int8_t *clean = st->clean_target_pixels + (size_t)tn36_li(level, lane) * ph * pw;
        int8_t *patch = sprite_pixels_mut(s, target_slot);
        memcpy(patch, clean, (size_t)ph * pw);
    }
    aux->final_flash = 0;
    aux->active_lane = -1;
    aux->win_pending = 1;
}

static void tn36_revert_to_checkpoint(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                                      Tn36Aux *aux) {
    int32_t robot_slot = tn36_robot_slot(st, level, lane);
    s->x[robot_slot] = aux->saved_x[lane];
    s->y[robot_slot] = aux->saved_y[lane];
    aux->rotation[lane] = aux->saved_rotation[lane];
    aux->scale[lane] = aux->saved_scale[lane];
    memcpy(aux->pattern[lane], aux->saved_pattern[lane], sizeof aux->pattern[lane]);
    aux->dead[lane] = 0;
    set_visible(s, robot_slot, 1);
    tn36_rerender_robot(s, st, level, lane, aux);
    tn36_checkpoint_scan(s, st, level, lane, aux, 0);
}

static void tn36_finish_or_complete(Sprites *s, const Tn36Static *st, int32_t level, Tn36Aux *aux,
                                    int32_t *score, int32_t *status, uint8_t *next_level,
                                    uint8_t *action_complete) {
    (void)score;
    (void)status;
    (void)next_level;
    if (tn36_target_match(s, st, level, 1, aux)) {
        aux->win_pending = 1;
        return;
    }
    int32_t slot = st->scroll_slot[level];
    int scroll_ok = st->scroll_bg_x[level] < s->x[slot] + st->scroll_w[level];
    if (!scroll_ok) {
        aux->lose_pending = 1;
        return;
    }
    *action_complete = 1;
}

static void tn36_advance_lane(Sprites *s, const Tn36Static *st, int32_t level, Tn36Aux *aux,
                              int32_t *next_order, int32_t *score, int32_t *status, uint8_t *next_level,
                              uint8_t *action_complete) {
    int32_t lane = aux->active_lane;

    if (aux->switch_flash) {
        int32_t slot = st->switch_slot[tn36_li(level, lane)];
        int8_t old = (int8_t)st->switch_color0[tn36_li(level, lane)];
        color_remap(s, slot, 1, TN36_SWITCH_HIGHLIGHT, old);
        aux->switch_flash = 0;
    }

    int32_t ghost_slot = st->num_slots - 2 + lane;
    if (aux->dead[lane]) remove_sprite(s, ghost_slot);

    if (aux->final_flash) {
        tn36_end_success(s, st, level, lane, aux);
        return;
    }

    int32_t i = aux->oocupkguhu[lane];
    int32_t n = st->num_cols[tn36_li(level, lane)];
    const int32_t *col_slot = st->col_slot + (size_t)tn36_li(level, lane) * TN36_MAX_COLS;

    if (i > 0) {
        int32_t prev_slot = col_slot[i - 1];
        if (prev_slot >= 0) set_visible(s, prev_slot, 0);
    }

    if (i >= n) {
        tn36_checkpoint_scan(s, st, level, lane, aux, 1);
        if (tn36_target_match(s, st, level, lane, aux)) {
            tn36_begin_success_flash(s, st, level, lane, aux);
        } else {
            tn36_toggle_hazards_on_failure(s, st, level, lane);
            if (aux->reset_enabled[lane]) tn36_revert_to_checkpoint(s, st, level, lane, aux);
            aux->active_lane = -1;
            tn36_finish_or_complete(s, st, level, aux, score, status, next_level, action_complete);
        }
    } else {
        int32_t slot = col_slot[i];
        if (slot >= 0) set_visible(s, slot, 1);
        int32_t code = aux->instr[lane][i];
        tn36_execute(s, st, level, lane, code, aux, next_order);
        int blink = i < n - 1 && i % 3 == 2;
        if (blink) tn36_toggle_hazards_blink(s, st, level, lane, aux, next_order);
        aux->oocupkguhu[lane] = i + 1;
    }
}

static void tn36_set_grid(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane,
                          const int32_t *program) {
    int32_t n_cols = st->num_cols[tn36_li(level, lane)];
    int32_t n_bits = st->num_bits[tn36_li(level, lane)];
    const int32_t *cell_slot =
        st->cell_slot + (size_t)tn36_li(level, lane) * TN36_MAX_COLS * TN36_MAX_BITS;
    for (int32_t c = 0; c < n_cols; c++) {
        for (int32_t b = 0; b < n_bits; b++) {
            int32_t slot = cell_slot[c * TN36_MAX_BITS + b];
            if (slot < 0) continue;
            int bit_on = ((program[c] >> b) & 1) == 1;
            color_remap(s, slot, 0, 0, bit_on ? TN36_COLOR_ON : TN36_COLOR_OFF);
        }
    }
}

static void tn36_queue_lane(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane, Tn36Aux *aux) {
    int32_t n_bits = st->num_bits[tn36_li(level, lane)];
    int32_t pw = s->atlas->pw;
    const int32_t *cell_slot =
        st->cell_slot + (size_t)tn36_li(level, lane) * TN36_MAX_COLS * TN36_MAX_BITS;
    const int32_t *cell_state =
        st->cell_state_pixel + (size_t)tn36_li(level, lane) * TN36_MAX_COLS * TN36_MAX_BITS * 2;
    for (int32_t c = 0; c < TN36_MAX_COLS; c++) {
        int32_t acc = 0;
        for (int32_t b = 0; b < n_bits; b++) {
            int32_t slot = cell_slot[c * TN36_MAX_BITS + b];
            if (slot < 0) continue;
            int32_t pr = cell_state[(c * TN36_MAX_BITS + b) * 2 + 0];
            int32_t pc = cell_state[(c * TN36_MAX_BITS + b) * 2 + 1];
            const int8_t *patch = sprite_pixels(s, slot);
            if (patch[pr * pw + pc] == TN36_COLOR_ON) acc |= (int32_t)1 << b;
        }
        aux->instr[lane][c] = acc;
    }
    aux->oocupkguhu[lane] = 0;
    aux->active_lane = lane;
    tn36_revert_to_checkpoint(s, st, level, lane, aux);
}

static void tn36_load_waypoint(Sprites *s, const Tn36Static *st, int32_t level, int32_t idx,
                               Tn36Aux *aux) {
    const int32_t *pos = st->button_pos + (size_t)level * TN36_MAX_BUTTONS * 2 + (size_t)idx * 2;
    const int32_t *gb = tn36_box(st->grsysj_box, level, 0);
    int32_t new_x = gb[0] + (gb[2] / TN36_PITCH / 2) * TN36_PITCH + pos[0] * TN36_PITCH;
    int32_t new_y = gb[1] + (gb[3] / TN36_PITCH / 2) * TN36_PITCH + pos[1] * TN36_PITCH;
    int32_t rotation = st->button_rot[(size_t)level * TN36_MAX_BUTTONS + idx];
    int32_t scale = st->button_scale[(size_t)level * TN36_MAX_BUTTONS + idx];
    uint8_t reset = st->button_reset[(size_t)level * TN36_MAX_BUTTONS + idx];

    int32_t robot_slot = tn36_robot_slot(st, level, 0);
    s->x[robot_slot] = new_x;
    s->y[robot_slot] = new_y;
    aux->rotation[0] = rotation;
    aux->scale[0] = scale;
    aux->saved_x[0] = new_x;
    aux->saved_y[0] = new_y;
    aux->saved_rotation[0] = rotation;
    aux->saved_scale[0] = scale;
    aux->reset_enabled[0] = reset;
    tn36_rerender_robot(s, st, level, 0, aux);

    const int32_t *program = st->button_program + ((size_t)level * TN36_MAX_BUTTONS + idx) * TN36_MAX_COLS;
    tn36_set_grid(s, st, level, 0, program);
}

static int32_t tn36_button_hit(const Tn36Static *st, int32_t level, int32_t x, int32_t y) {
    int32_t n = st->num_buttons[level];
    const int32_t *box = st->button_box + (size_t)level * TN36_MAX_BUTTONS * 4;
    for (int32_t k = 0; k < n; k++)
        if (tn36_box_hit(box + (size_t)k * 4, x, y)) return k;
    return -1;
}

static void tn36_select_button(Sprites *s, const Tn36Static *st, int32_t level, int32_t idx,
                               Tn36Aux *aux) {
    int32_t n = st->num_buttons[level];
    const int32_t *slots = st->button_slot + (size_t)level * TN36_MAX_BUTTONS;
    for (int32_t k = 0; k < n; k++) {
        int32_t slot = slots[k];
        if (slot < 0) continue;
        int is_sel = k == idx;
        int8_t old = is_sel ? TN36_COLOR_UNSELECTED : TN36_COLOR_SELECTED;
        int8_t neu = is_sel ? TN36_COLOR_SELECTED : TN36_COLOR_UNSELECTED;
        color_remap(s, slot, 1, old, neu);
    }
    tn36_load_waypoint(s, st, level, idx, aux);
    tn36_queue_lane(s, st, level, 0, aux);
}

static void tn36_toggle_cell(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane, int32_t x,
                             int32_t y) {
    int32_t n_cols = st->num_cols[tn36_li(level, lane)];
    int32_t n_bits = st->num_bits[tn36_li(level, lane)];
    int32_t pw = s->atlas->pw;
    const int32_t *cell_slot =
        st->cell_slot + (size_t)tn36_li(level, lane) * TN36_MAX_COLS * TN36_MAX_BITS;
    const int32_t *cell_state =
        st->cell_state_pixel + (size_t)tn36_li(level, lane) * TN36_MAX_COLS * TN36_MAX_BITS * 2;
    for (int32_t c = 0; c < n_cols; c++) {
        for (int32_t b = 0; b < n_bits; b++) {
            int32_t slot = cell_slot[c * TN36_MAX_BITS + b];
            if (slot < 0) continue;
            if (x < s->x[slot] || y < s->y[slot] || x >= s->x[slot] + s->w[slot] ||
                y >= s->y[slot] + s->h[slot])
                continue;
            int32_t pr = cell_state[(c * TN36_MAX_BITS + b) * 2 + 0];
            int32_t pc = cell_state[(c * TN36_MAX_BITS + b) * 2 + 1];
            const int8_t *patch = sprite_pixels(s, slot);
            int cur_on = patch[pr * pw + pc] == TN36_COLOR_ON;
            color_remap(s, slot, 0, 0, cur_on ? TN36_COLOR_OFF : TN36_COLOR_ON);
        }
    }
}

static int tn36_lane_click(Sprites *s, const Tn36Static *st, int32_t level, int32_t lane, int32_t x,
                           int32_t y, Tn36Aux *aux) {
    const int32_t *cb = tn36_box(st->container_box, level, lane);
    if (!tn36_box_hit(cb, x, y)) return 0;

    const int32_t *gb = tn36_box(st->grid_box, level, lane);
    int view_only = st->view_only[tn36_li(level, lane)];
    if (!view_only && tn36_box_hit(gb, x, y)) {
        tn36_toggle_cell(s, st, level, lane, x, y);
        return 1;
    }

    int32_t switch_slot = st->switch_slot[tn36_li(level, lane)];
    if (switch_slot >= 0) {
        const int32_t *sb = tn36_box(st->switch_box, level, lane);
        if (tn36_box_hit(sb, x, y)) {
            int8_t old = (int8_t)st->switch_color0[tn36_li(level, lane)];
            color_remap(s, switch_slot, 1, old, TN36_SWITCH_HIGHLIGHT);
            aux->switch_flash = 1;
            tn36_queue_lane(s, st, level, lane, aux);
            return 1;
        }
    }
    return 0;
}

static void tn36_route_click(Sprites *s, const Tn36Static *st, int32_t level, int32_t x, int32_t y,
                             Tn36Aux *aux) {
    int32_t idx = tn36_button_hit(st, level, x, y);
    if (idx >= 0) {
        tn36_select_button(s, st, level, idx, aux);
        return;
    }
    if (tn36_lane_click(s, st, level, 0, x, y, aux)) return;
    tn36_lane_click(s, st, level, 1, x, y, aux);
}

static void tn36_scroll_tick(Sprites *s, const Tn36Static *st, int32_t level, Tn36Aux *aux) {
    int32_t tick = aux->scroll_tick + 1;
    int should_move = level >= 5 ? tick % 2 == 0 : 1;
    if (should_move) s->x[st->scroll_slot[level]] -= 1;
    aux->scroll_tick = tick;
}

static void tn36_handle_click(Sprites *s, const Camera *camera, const Tn36Static *st, int32_t level,
                              int32_t action_x, int32_t action_y, Tn36Aux *aux, int32_t *score,
                              int32_t *status, uint8_t *next_level, uint8_t *action_complete) {
    int32_t wx, wy;
    int on_board;
    tn36_display_to_grid(camera, action_x, action_y, &wx, &wy, &on_board);
    int32_t x = on_board ? wx : 0, y = on_board ? wy : 0;
    tn36_route_click(s, st, level, x, y, aux);
    tn36_scroll_tick(s, st, level, aux);
    if (aux->active_lane >= 0) return;
    tn36_finish_or_complete(s, st, level, aux, score, status, next_level, action_complete);
}

static void tn36_next_level(const Tn36Static *st, int32_t level, int32_t *score, int32_t *status,
                            uint8_t *next_level) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = (uint8_t)!is_last;
    if (is_last) *status = TN36_WIN;
}

void tn36_zero_aux(Tn36Aux *aux) {
    aux->active_lane = -1;
    for (int32_t lane = 0; lane < 2; lane++) {
        aux->oocupkguhu[lane] = 0;
        for (int32_t c = 0; c < TN36_MAX_COLS; c++) aux->instr[lane][c] = 0;
        aux->rotation[lane] = 0;
        aux->scale[lane] = 1;
        for (int32_t r = 0; r < 4; r++)
            for (int32_t c = 0; c < 4; c++) {
                aux->pattern[lane][r][c] = -1;
                aux->saved_pattern[lane][r][c] = -1;
            }
        aux->dead[lane] = 0;
        aux->saved_x[lane] = 0;
        aux->saved_y[lane] = 0;
        aux->saved_rotation[lane] = 0;
        aux->saved_scale[lane] = 1;
        aux->reset_enabled[lane] = 1;
    }
    aux->switch_flash = 0;
    aux->final_flash = 0;
    aux->win_pending = 0;
    aux->lose_pending = 0;
    aux->scroll_tick = 0;
    aux->selected_button = -1;
}

void tn36_on_set_level(Sprites *sprites, const Tn36Static *st, int32_t level, Tn36Aux *aux) {
    tn36_zero_aux(aux);
    for (int32_t lane = 0; lane < 2; lane++) {
        aux->rotation[lane] = st->robot_rotation0[tn36_li(level, lane)];
        aux->scale[lane] = st->robot_scale0[tn36_li(level, lane)];
        int8_t color = (int8_t)st->robot_color0[tn36_li(level, lane)];
        const uint8_t *mask = st->robot_mask + (size_t)tn36_li(level, lane) * 16;
        for (int32_t r = 0; r < 4; r++)
            for (int32_t c = 0; c < 4; c++) aux->pattern[lane][r][c] = mask[r * 4 + c] ? color : -1;
    }
    for (int32_t lane = 0; lane < 2; lane++) {
        int32_t robot_slot = tn36_robot_slot(st, level, lane);
        aux->saved_x[lane] = sprites->x[robot_slot];
        aux->saved_y[lane] = sprites->y[robot_slot];
        aux->saved_rotation[lane] = aux->rotation[lane];
        aux->saved_scale[lane] = aux->scale[lane];
        memcpy(aux->saved_pattern[lane], aux->pattern[lane], sizeof aux->pattern[lane]);
    }

    const uint8_t *mask = st->bbox_override + (size_t)level * st->num_slots;
    for (int32_t i = 0; i < st->num_slots; i++)
        if (mask[i]) sprites->blocking[i] = BOUNDING_BOX;

    tn36_rerender_robot(sprites, st, level, 0, aux);
    tn36_rerender_robot(sprites, st, level, 1, aux);

    if (st->has_programs[level]) tn36_load_waypoint(sprites, st, level, 0, aux);
}

void tn36_step_once(Sprites *sprites, const Camera *camera, const Tn36Static *st, int32_t level,
                    int32_t action_id, int32_t action_x, int32_t action_y, int32_t action_count,
                    Tn36Aux *aux, int32_t *next_order, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete) {
    (void)action_count;

    if (aux->win_pending) {
        tn36_next_level(st, level, score, status, next_level);
        *action_complete = 1;
        return;
    }
    if (aux->lose_pending) {
        *status = TN36_GAME_OVER;
        *action_complete = 1;
        return;
    }
    if (aux->active_lane >= 0) {
        tn36_advance_lane(sprites, st, level, aux, next_order, score, status, next_level, action_complete);
        return;
    }
    if (action_id == TN36_ACTION6) {
        tn36_handle_click(sprites, camera, st, level, action_x, action_y, aux, score, status, next_level,
                          action_complete);
    } else {
        *action_complete = 1;
    }
}

void tn36_render_interface(int8_t *frame, const Sprites *sprites, const Camera *camera,
                           const Tn36Static *st, int32_t level, const Tn36Aux *aux) {
    (void)frame;
    (void)sprites;
    (void)camera;
    (void)st;
    (void)level;
    (void)aux;
}
