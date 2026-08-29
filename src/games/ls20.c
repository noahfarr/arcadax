#include "ls20.h"

#include <stdlib.h>
#include <string.h>

enum {
    LS20_CODE_HAZARD = 1,
    LS20_CODE_GOAL = 2,
    LS20_CODE_REFILL = 3,
    LS20_CODE_BTN_SHAPE = 4,
    LS20_CODE_BTN_COLOR = 5,
    LS20_CODE_BTN_ROTATION = 6,
};
enum { LS20_WIN = 2, LS20_GAME_OVER = 3 };
enum { LS20_PUSH_PUSH_PHASE = 8, LS20_DEATH_FRAMES = 5, LS20_FLASH_FRAMES = 5 };
enum { LS20_LIVES_START = 3, LS20_PATROL_CELL = 5 };
enum {
    LS20_COLOR_BLACK = 0,
    LS20_COLOR_FOG = 5,
    LS20_COLOR_STEP_EMPTY = 3,
    LS20_COLOR_STEP_FILLED = 11,
    LS20_COLOR_LIVES_FILLED = 8,
};

static inline int32_t ls20_clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void ls20_set_slot_visible(ArcSprites *sprites, int32_t slot, int visible) {
    if (slot < 0) return;
    arc_set_visible(sprites, slot, visible);
}

static void ls20_set_tag_visible(ArcSprites *sprites, int32_t num_slots, int32_t tag_idx,
                                 int visible) {
    const ArcAtlas *atlas = sprites->atlas;
    for (int32_t i = 0; i < num_slots; i++) {
        if (!sprites->alive[i]) continue;
        if (!sprites->tags[(size_t)i * atlas->num_tags + tag_idx]) continue;
        arc_set_visible(sprites, i, visible);
    }
}

static void ls20_style_loadout(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                               int32_t shape, int32_t color, int32_t rot) {
    int32_t htk = st->htk_slot[level];
    size_t area = (size_t)sprites->atlas->ph * sprites->atlas->pw;
    const int8_t *variant = st->htk_variant + (((size_t)shape * 4 + color) * 4 + rot) * area;
    memcpy(arc_sprite_pixels_mut(sprites, htk), variant, area);
}

static void ls20_apply_hint(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                            Ls20Aux *aux) {
    int any_hint = 0;
    for (int32_t g = 0; g < LS20_MAX_GOALS; g++) {
        int32_t idx = level * LS20_MAX_GOALS + g;
        int matches = aux->shape_idx == st->want_shape[idx] &&
                     aux->color_idx == st->want_color[idx] &&
                     aux->rot_idx == st->want_rot[idx];
        int show = matches && !aux->goal_done[g];
        ls20_set_slot_visible(sprites, st->ring_slot[idx], show);
        any_hint |= show;
    }
    ls20_set_slot_visible(sprites, st->hint_glow_slot[level], any_hint);
    if (any_hint) aux->flash_frame = LS20_FLASH_FRAMES;
}

static int ls20_h_goal(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                       Ls20Aux *aux, int32_t slot) {
    int32_t g = st->goal_index[(size_t)level * st->num_slots + slot];
    if (g < 0) g = 0;
    int32_t idx = level * LS20_MAX_GOALS + g;
    int mismatch = !(aux->shape_idx == st->want_shape[idx] &&
                     aux->color_idx == st->want_color[idx] &&
                     aux->rot_idx == st->want_rot[idx]);
    if (mismatch) {
        arc_color_remap(sprites, st->htk2_slot[level], 0, 0, LS20_COLOR_BLACK);
        aux->flash_frame = LS20_FLASH_FRAMES;
    }
    return mismatch;
}

static void ls20_h_refill(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                          Ls20Aux *aux, int32_t slot) {
    arc_remove_sprite(sprites, slot);
    aux->steps = st->budget[level];
}

static void ls20_h_button(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                          Ls20Aux *aux, int which) {
    int32_t shape = aux->shape_idx, color = aux->color_idx, rot = aux->rot_idx;
    if (which == 0) shape = (shape + 1) % 6;
    else if (which == 1) color = (color + 1) % 4;
    else rot = (rot + 1) % 4;
    ls20_style_loadout(sprites, st, level, shape, color, rot);
    aux->shape_idx = shape;
    aux->color_idx = color;
    aux->rot_idx = rot;
    if (level == 0) ls20_apply_hint(sprites, st, level, aux);
}

static void ls20_process_target(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                                int32_t target_x, int32_t target_y, Ls20Aux *aux,
                                int *out_blocked, int *out_refilled) {
    const int32_t *tag_code = st->tag_code + (size_t)level * st->num_slots;
    int32_t pitch_x = st->pitch_x[level], pitch_y = st->pitch_y[level];
    int32_t count = 0;
    for (int32_t i = 0; i < st->num_slots; i++) {
        if (!sprites->alive[i] || tag_code[i] <= 0) continue;
        int32_t sx = sprites->x[i], sy = sprites->y[i];
        if (sx < target_x || sx >= target_x + pitch_x || sy < target_y ||
            sy >= target_y + pitch_y)
            continue;
        aux->hit_slots[count++] = i;
    }

    for (int32_t a = 1; a < count; a++) {
        int32_t v = aux->hit_slots[a];
        int32_t key = sprites->order[v];
        int32_t b = a - 1;
        while (b >= 0 && sprites->order[aux->hit_slots[b]] > key) {
            aux->hit_slots[b + 1] = aux->hit_slots[b];
            b--;
        }
        aux->hit_slots[b + 1] = v;
    }

    int blocked = 0, refilled = 0;
    for (int32_t k = 0; k < count; k++) {
        int32_t slot = aux->hit_slots[k];
        int32_t code = tag_code[slot];
        if (code == LS20_CODE_HAZARD) {
            blocked = 1;
            break;
        } else if (code == LS20_CODE_GOAL) {
            blocked |= ls20_h_goal(sprites, st, level, aux, slot);
        } else if (code == LS20_CODE_REFILL) {
            ls20_h_refill(sprites, st, level, aux, slot);
            refilled = 1;
        } else if (code == LS20_CODE_BTN_SHAPE) {
            ls20_h_button(sprites, st, level, aux, 0);
        } else if (code == LS20_CODE_BTN_COLOR) {
            ls20_h_button(sprites, st, level, aux, 1);
        } else if (code == LS20_CODE_BTN_ROTATION) {
            ls20_h_button(sprites, st, level, aux, 2);
        }
    }
    *out_blocked = blocked;
    *out_refilled = refilled;
}

static void ls20_step_patrols(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                              Ls20Aux *aux, int32_t prev_x[LS20_MAX_PATROLS],
                              int32_t prev_y[LS20_MAX_PATROLS],
                              int32_t prev_dir[LS20_MAX_PATROLS]) {
    static const int32_t offsets[4] = {0, -1, 1, 2};
    int32_t ph = sprites->atlas->ph, pw = sprites->atlas->pw;
    for (int32_t k = 0; k < LS20_MAX_PATROLS; k++) {
        int32_t idx = level * LS20_MAX_PATROLS + k;
        int32_t pslot = st->patrol_slot[idx];
        int32_t aslot = st->patrol_area_slot[idx];
        prev_dir[k] = aux->patrol_dir[k];
        if (pslot < 0) {
            prev_x[k] = 0;
            prev_y[k] = 0;
            continue;
        }
        int32_t px = sprites->x[pslot], py = sprites->y[pslot];
        prev_x[k] = px;
        prev_y[k] = py;
        int32_t ax = sprites->x[aslot], ay = sprites->y[aslot];
        int32_t aw = sprites->w[aslot], ah = sprites->h[aslot];
        const int8_t *area_pixels = arc_sprite_pixels(sprites, aslot);
        int32_t cur_dir = aux->patrol_dir[k];

        int found = 0;
        int32_t best_dx = 0, best_dy = 0, best_dir = cur_dir;
        for (int oi = 0; oi < 4 && !found; oi++) {
            int32_t cand = ((cur_dir + offsets[oi]) % 4 + 4) % 4;
            int32_t ddx = cand == 1 ? 1 : (cand == 3 ? -1 : 0);
            int32_t ddy = cand == 0 ? 1 : (cand == 2 ? -1 : 0);
            int32_t cx = px + ddx * LS20_PATROL_CELL, cy = py + ddy * LS20_PATROL_CELL;
            if (cx < ax || cx >= ax + aw || cy < ay || cy >= ay + ah) continue;
            int32_t rel_x = ls20_clamp(cx - ax, 0, pw - 1);
            int32_t rel_y = ls20_clamp(cy - ay, 0, ph - 1);
            if (area_pixels[(size_t)rel_y * pw + rel_x] < 0) continue;
            best_dx = ddx * LS20_PATROL_CELL;
            best_dy = ddy * LS20_PATROL_CELL;
            best_dir = cand;
            found = 1;
        }
        if (found) arc_set_position(sprites, pslot, px + best_dx, py + best_dy);
        aux->patrol_dir[k] = best_dir;
    }
}

static void ls20_undo_patrols(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                              Ls20Aux *aux, const int32_t prev_x[LS20_MAX_PATROLS],
                              const int32_t prev_y[LS20_MAX_PATROLS],
                              const int32_t prev_dir[LS20_MAX_PATROLS]) {
    for (int32_t k = 0; k < LS20_MAX_PATROLS; k++) {
        int32_t pslot = st->patrol_slot[level * LS20_MAX_PATROLS + k];
        if (pslot >= 0) arc_set_position(sprites, pslot, prev_x[k], prev_y[k]);
        aux->patrol_dir[k] = prev_dir[k];
    }
}

static void ls20_reset_patrols(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                               Ls20Aux *aux) {
    for (int32_t k = 0; k < LS20_MAX_PATROLS; k++) {
        int32_t idx = level * LS20_MAX_PATROLS + k;
        int32_t pslot = st->patrol_slot[idx];
        if (pslot >= 0) arc_set_position(sprites, pslot, st->patrol_start_x[idx], st->patrol_start_y[idx]);
        aux->patrol_dir[k] = 0;
    }
}

static void ls20_maybe_start_push(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                                  Ls20Aux *aux) {
    int32_t player = st->player_slot[level];
    int32_t px = sprites->x[player], py = sprites->y[player];
    int32_t pw = sprites->w[player], ph = sprites->h[player];
    int32_t count = st->pushable_count[level];
    const int32_t *slots = st->pushable_slots + (size_t)level * st->max_pushable;
    int32_t best = -1, best_order = 0;
    for (int32_t k = 0; k < count; k++) {
        int32_t s = slots[k];
        if (!sprites->alive[s]) continue;
        int32_t sx = sprites->x[s], sy = sprites->y[s], sw = sprites->w[s], sh = sprites->h[s];
        if (!(px < sx + sw && px + pw > sx && py < sy + sh && py + ph > sy)) continue;
        if (best < 0 || sprites->order[s] < best_order) {
            best = s;
            best_order = sprites->order[s];
        }
    }
    if (best >= 0) {
        aux->push_slot = best;
        aux->push_frame = 1;
    }
}

static int32_t ls20_restore_removables(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                                       int32_t next_order) {
    int32_t count = st->restorable_count[level];
    const int32_t *slots = st->restorable_slots + (size_t)level * st->max_restorable;
    int32_t rank = 0;
    for (int32_t k = 0; k < count; k++) {
        int32_t slot = slots[k];
        if (sprites->alive[slot]) continue;
        sprites->order[slot] = next_order + rank;
        sprites->alive[slot] = 1;
        rank++;
    }
    return next_order + rank;
}

static void ls20_start_death(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                             Ls20Aux *aux, int32_t *next_order, int32_t *status,
                             uint8_t *action_complete) {
    int32_t lives = aux->lives - 1;
    if (lives == 0) {
        aux->lives = lives;
        *status = LS20_GAME_OVER;
        *action_complete = 1;
        return;
    }

    ls20_set_slot_visible(sprites, st->flash_slot, 1);
    ls20_set_slot_visible(sprites, st->htk_slot[level], 0);
    int32_t player = st->player_slot[level];
    arc_set_position(sprites, player, st->player_start_x[level], st->player_start_y[level]);
    *next_order = ls20_restore_removables(sprites, st, level, *next_order);
    ls20_set_tag_visible(sprites, st->num_slots, st->hint_ring_tag, 0);
    ls20_set_tag_visible(sprites, st->num_slots, st->goal_frame_tag, 1);
    ls20_set_slot_visible(sprites, st->hint_glow_slot[level], 0);
    ls20_reset_patrols(sprites, st, level, aux);
    int32_t shape0 = st->start_shape[level], color0 = st->start_color[level],
           rot0 = st->start_rot[level];
    ls20_style_loadout(sprites, st, level, shape0, color0, rot0);

    ls20_zero_aux(aux);
    aux->lives = lives;
    aux->steps = st->budget[level];
    aux->shape_idx = shape0;
    aux->color_idx = color0;
    aux->rot_idx = rot0;
    for (int32_t g = 0; g < LS20_MAX_GOALS; g++) aux->goal_done[g] = g >= st->num_goals[level];
    aux->death_frame = LS20_DEATH_FRAMES;
}

static void ls20_advance_death(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                               Ls20Aux *aux, uint8_t *action_complete) {
    int32_t frame = aux->death_frame - 1;
    aux->death_frame = frame;
    if (frame > 0) return;
    ls20_set_slot_visible(sprites, st->flash_slot, 0);
    ls20_set_slot_visible(sprites, st->htk_slot[level], 1);
    *action_complete = 1;
}

static void ls20_advance_flash(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                               Ls20Aux *aux, uint8_t *action_complete) {
    int32_t frame = aux->flash_frame - 1;
    aux->flash_frame = frame;
    if (frame > 0) return;
    arc_color_remap(sprites, st->htk2_slot[level], 0, 0, LS20_COLOR_FOG);
    ls20_set_slot_visible(sprites, st->hint_glow_slot[level], 0);
    ls20_set_tag_visible(sprites, st->num_slots, st->hint_ring_tag, 0);
    *action_complete = 1;
}

static int ls20_check_goals(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                            Ls20Aux *aux) {
    int32_t player = st->player_slot[level];
    int32_t px = sprites->x[player], py = sprites->y[player];
    for (int32_t g = 0; g < LS20_MAX_GOALS; g++) {
        if (aux->goal_done[g]) continue;
        int32_t idx = level * LS20_MAX_GOALS + g;
        int matches = aux->shape_idx == st->want_shape[idx] &&
                     aux->color_idx == st->want_color[idx] &&
                     aux->rot_idx == st->want_rot[idx];
        int newly = matches && px == st->goal_x[idx] && py == st->goal_y[idx];
        if (!newly) continue;
        sprites->alive[st->goal_slot[idx]] = 0;
        sprites->alive[st->marker_slot[idx]] = 0;
        if (st->goal_is_final[idx]) {
            ls20_set_slot_visible(sprites, st->frame_slot[idx], 0);
            ls20_set_slot_visible(sprites, st->ring_slot[idx], 0);
            ls20_set_slot_visible(sprites, st->hint_glow_slot[level], 0);
        }
        aux->goal_done[g] = 1;
    }
    for (int32_t g = 0; g < LS20_MAX_GOALS; g++)
        if (!aux->goal_done[g]) return 0;
    return 1;
}

static void ls20_next_level(const Ls20Static *st, int32_t level, int32_t *score,
                            int32_t *status, uint8_t *next_level) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = (uint8_t)!is_last;
    if (is_last) *status = LS20_WIN;
}

static void ls20_advance_push(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                              Ls20Aux *aux, uint8_t *action_complete) {
    int32_t slot = aux->push_slot, k = aux->push_frame;
    size_t idx = ((size_t)level * st->num_slots + slot) * LS20_PUSH_FRAMES + (k - 1);
    int32_t dx = st->wall_step_dx[idx], dy = st->wall_step_dy[idx];
    arc_move_sprite(sprites, slot, dx, dy);
    int32_t player = st->player_slot[level];
    if (k <= LS20_PUSH_PUSH_PHASE) arc_move_sprite(sprites, player, dx, dy);

    if (k == LS20_PUSH_FRAMES) {
        aux->push_slot = -1;
        aux->push_frame = 0;
        int32_t px = sprites->x[player], py = sprites->y[player];
        int blocked, refilled;
        ls20_process_target(sprites, st, level, px, py, aux, &blocked, &refilled);
        *action_complete = 1;
    } else {
        aux->push_frame = k + 1;
    }
}

static void ls20_play_move(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                           int32_t action_id, Ls20Aux *aux, int32_t *next_order,
                           int32_t *score, int32_t *status, uint8_t *next_level,
                           uint8_t *action_complete) {
    int32_t dx = action_id == 3 ? -1 : (action_id == 4 ? 1 : 0);
    int32_t dy = action_id == 1 ? -1 : (action_id == 2 ? 1 : 0);

    int32_t prev_x[LS20_MAX_PATROLS], prev_y[LS20_MAX_PATROLS], prev_dir[LS20_MAX_PATROLS];
    ls20_step_patrols(sprites, st, level, aux, prev_x, prev_y, prev_dir);

    int32_t player = st->player_slot[level];
    int32_t px = sprites->x[player], py = sprites->y[player];
    int32_t target_x = px + dx * st->pitch_x[level];
    int32_t target_y = py + dy * st->pitch_y[level];

    int blocked, refilled;
    ls20_process_target(sprites, st, level, target_x, target_y, aux, &blocked, &refilled);

    if (blocked) ls20_undo_patrols(sprites, st, level, aux, prev_x, prev_y, prev_dir);
    else arc_set_position(sprites, player, target_x, target_y);

    if (aux->flash_frame > 0) return;

    int32_t steps = aux->steps;
    int32_t new_steps = refilled ? steps : steps - st->decrement[level];
    aux->steps = new_steps;
    int out_of_steps = !refilled && new_steps < 0;

    if (!out_of_steps) ls20_maybe_start_push(sprites, st, level, aux);
    if (aux->push_frame > 0) return;

    int all_done = ls20_check_goals(sprites, st, level, aux);
    if (all_done) {
        ls20_next_level(st, level, score, status, next_level);
        *action_complete = 1;
    } else if (out_of_steps) {
        ls20_start_death(sprites, st, level, aux, next_order, status, action_complete);
    } else {
        *action_complete = 1;
    }
}

static void ls20_play(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                      int32_t action_id, Ls20Aux *aux, int32_t *next_order,
                      int32_t *score, int32_t *status, uint8_t *next_level,
                      uint8_t *action_complete) {
    if (action_id >= 1 && action_id <= 4) {
        ls20_play_move(sprites, st, level, action_id, aux, next_order, score, status,
                       next_level, action_complete);
    } else {
        *action_complete = 1;
    }
}

void ls20_aux_alloc(Ls20Aux *aux, int32_t num_slots) {
    aux->hit_slots = malloc(sizeof(int32_t) * (size_t)num_slots);
}

void ls20_aux_free(Ls20Aux *aux) {
    free(aux->hit_slots);
    aux->hit_slots = NULL;
}

void ls20_zero_aux(Ls20Aux *aux) {
    aux->lives = LS20_LIVES_START;
    aux->steps = 0;
    aux->shape_idx = 0;
    aux->color_idx = 0;
    aux->rot_idx = 0;
    for (int32_t g = 0; g < LS20_MAX_GOALS; g++) aux->goal_done[g] = 1;
    aux->push_slot = -1;
    aux->push_frame = 0;
    aux->death_frame = 0;
    aux->flash_frame = 0;
    for (int32_t k = 0; k < LS20_MAX_PATROLS; k++) aux->patrol_dir[k] = 0;
}

void ls20_on_set_level(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                       Ls20Aux *aux, int32_t *next_order) {
    *next_order = st->num_slots;
    int32_t shape0 = st->start_shape[level], color0 = st->start_color[level],
           rot0 = st->start_rot[level];
    ls20_style_loadout(sprites, st, level, shape0, color0, rot0);
    ls20_zero_aux(aux);
    aux->steps = st->budget[level];
    aux->shape_idx = shape0;
    aux->color_idx = color0;
    aux->rot_idx = rot0;
    for (int32_t g = 0; g < LS20_MAX_GOALS; g++) aux->goal_done[g] = g >= st->num_goals[level];
}

void ls20_step_once(ArcSprites *sprites, const Ls20Static *st, int32_t level,
                    int32_t action_id, Ls20Aux *aux, int32_t *next_order,
                    int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    if (aux->push_frame > 0) {
        ls20_advance_push(sprites, st, level, aux, action_complete);
    } else if (aux->death_frame > 0) {
        ls20_advance_death(sprites, st, level, aux, action_complete);
    } else if (aux->flash_frame > 0) {
        ls20_advance_flash(sprites, st, level, aux, action_complete);
    } else {
        ls20_play(sprites, st, level, action_id, aux, next_order, score, status,
                 next_level, action_complete);
    }
}

void ls20_render_interface(int8_t *frame, const ArcSprites *sprites, const Ls20Static *st,
                           int32_t level, const Ls20Aux *aux) {
    int32_t budget = st->budget[level];
    if (budget == 0 || aux->death_frame > 0) return;

    if (st->fog[level]) {
        int32_t player = st->player_slot[level];
        int32_t px = sprites->x[player], py = sprites->y[player];
        for (int32_t r = 0; r < ARC_FRAME_SIZE; r++) {
            for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
                int32_t a = 2 * r - 2 * py - 3, b = 2 * c - 2 * px - 3;
                if (a * a + b * b > 1600) frame[(size_t)r * ARC_FRAME_SIZE + c] = LS20_COLOR_FOG;
            }
        }
        int32_t htk = st->htk_slot[level];
        if (arc_sprite_visible(sprites, htk)) {
            const int8_t *patch = arc_sprite_pixels(sprites, htk);
            int32_t pw = sprites->atlas->pw;
            for (int32_t v = 0; v < 6; v++) {
                for (int32_t u = 0; u < 6; u++) {
                    int8_t pv = patch[(size_t)v * pw + u];
                    if (pv >= 0) frame[(size_t)(55 + v) * ARC_FRAME_SIZE + (3 + u)] = pv;
                }
            }
        }
    }

    for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
        int32_t i = c - 13;
        if (i < 0 || i >= budget) continue;
        int filled = (budget - i - 1) < aux->steps;
        int8_t colour = filled ? LS20_COLOR_STEP_FILLED : LS20_COLOR_STEP_EMPTY;
        frame[61 * ARC_FRAME_SIZE + c] = colour;
        frame[62 * ARC_FRAME_SIZE + c] = colour;
    }

    for (int32_t b = 0; b < 3; b++) {
        int32_t x0 = 56 + 3 * b;
        int8_t colour = aux->lives > b ? LS20_COLOR_LIVES_FILLED : LS20_COLOR_STEP_EMPTY;
        frame[61 * ARC_FRAME_SIZE + x0] = colour;
        frame[61 * ARC_FRAME_SIZE + x0 + 1] = colour;
        frame[62 * ARC_FRAME_SIZE + x0] = colour;
        frame[62 * ARC_FRAME_SIZE + x0 + 1] = colour;
    }
}
