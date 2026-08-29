#include "ka59.h"

#include <stdlib.h>
#include <string.h>

enum { KA59_PITCH = 3, KA59_MAX_PUSH_DEPTH = 8, KA59_RETRY_THRESHOLD = 5 };
enum { KA59_HIGHLIGHT = 14, KA59_SELECTED_DOT = 0, KA59_DESELECTED_DOT = 4 };
enum { KA59_ACTION1 = 1, KA59_ACTION2 = 2, KA59_ACTION3 = 3, KA59_ACTION4 = 4, KA59_ACTION6 = 6 };
enum { KA59_WIN = 2, KA59_GAME_OVER = 3 };
enum { KA59_CHAIN_CAP = 32, KA59_MAX_PATCH_AREA = 16384 };

static inline size_t ka59_area(const Ka59Static *st) {
    return (size_t)st->ph * (size_t)st->pw;
}

static void ka59_display_to_grid(const ArcCamera *camera, int32_t display_x, int32_t display_y,
                                 int32_t *world_x, int32_t *world_y, int *valid) {
    int32_t scale, x_pad, y_pad;
    arc_scale_and_offset(camera, &scale, &x_pad, &y_pad);
    int32_t dx = display_x - x_pad, dy = display_y - y_pad;
    int32_t grid_x = dx >= 0 ? dx / scale : -1;
    int32_t grid_y = dy >= 0 ? dy / scale : -1;
    *valid = grid_x >= 0 && grid_y >= 0 && grid_x < camera->width && grid_y < camera->height;
    *world_x = grid_x + camera->x;
    *world_y = grid_y + camera->y;
}

static int ka59_push_chain(ArcSprites *s, const Ka59Static *st, int32_t level, int32_t slot,
                           int32_t dx, int32_t dy, int32_t depth) {
    s->x[slot] += dx;
    s->y[slot] += dy;

    const int32_t *border = st->border_slots + (size_t)level * st->max_border;
    int32_t border_n = st->border_count[level];
    for (int32_t k = 0; k < border_n; k++) {
        if (arc_collides_pair(s, slot, border[k], 0)) {
            s->x[slot] -= dx;
            s->y[slot] -= dy;
            return 1;
        }
    }

    if (depth <= 1) return 0;

    const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
    int32_t occ_n = st->occupant_count[level];
    int32_t hit_slots[KA59_CHAIN_CAP];
    int32_t hit_n = 0;
    for (int32_t k = 0; k < occ_n; k++) {
        if (arc_collides_pair(s, slot, occ[k], 0)) hit_slots[hit_n++] = occ[k];
    }

    int any_blocked = 0;
    for (int32_t k = 0; k < hit_n; k++) {
        if (ka59_push_chain(s, st, level, hit_slots[k], dx, dy, depth - 1)) {
            any_blocked = 1;
            break;
        }
    }

    if (any_blocked) {
        s->x[slot] -= dx;
        s->y[slot] -= dy;
        return 1;
    }
    return 0;
}

static int ka59_strip_touches(ArcSprites *s, const Ka59Static *st, int32_t level,
                              int32_t x0, int32_t y0, int32_t width, int32_t height) {
    int32_t ph = st->ph, pw = st->pw;
    size_t area = (size_t)ph * (size_t)pw;
    int32_t slot = st->scratch_slot;

    int32_t save_x = s->x[slot], save_y = s->y[slot];
    int32_t save_h = s->h[slot], save_w = s->w[slot];
    uint8_t save_alive = s->alive[slot];
    int8_t save_interaction = s->interaction[slot], save_blocking = s->blocking[slot];
    int8_t saved_patch[KA59_MAX_PATCH_AREA];
    memcpy(saved_patch, arc_sprite_pixels(s, slot), area);

    int8_t *patch = arc_sprite_pixels_mut(s, slot);
    for (int32_t row = 0; row < ph; row++) {
        int within_row = row < height;
        for (int32_t col = 0; col < pw; col++) {
            int within = within_row && col < width;
            patch[(size_t)row * pw + col] = (int8_t)(within ? 0 : -1);
        }
    }
    s->x[slot] = x0;
    s->y[slot] = y0;
    s->h[slot] = height;
    s->w[slot] = width;
    s->alive[slot] = 1;
    s->interaction[slot] = INVISIBLE;
    s->blocking[slot] = PIXEL_PERFECT;

    const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
    int32_t occ_n = st->occupant_count[level];
    int result = 0;
    for (int32_t k = 0; k < occ_n; k++) {
        if (arc_collides_pair(s, slot, occ[k], 0)) {
            result = 1;
            break;
        }
    }

    s->x[slot] = save_x;
    s->y[slot] = save_y;
    s->h[slot] = save_h;
    s->w[slot] = save_w;
    s->alive[slot] = save_alive;
    s->interaction[slot] = save_interaction;
    s->blocking[slot] = save_blocking;
    memcpy(arc_sprite_pixels_mut(s, slot), saved_patch, area);

    return result;
}

static void ka59_refresh_borders(ArcSprites *s, const Ka59Static *st, int32_t level,
                                 const Ka59Aux *aux) {
    size_t area = ka59_area(st);
    const int32_t *box = st->box_slots + (size_t)level * st->max_box;
    int32_t box_n = st->box_count[level];
    for (int32_t k = 0; k < box_n; k++) {
        int32_t slot = box[k];
        int8_t *patch = arc_sprite_pixels_mut(s, slot);
        const uint8_t *mask = st->box_outline_mask + ((size_t)level * st->num_slots + slot) * area;
        for (size_t p = 0; p < area; p++)
            if (mask[p]) patch[p] = KA59_HIGHLIGHT;
    }

    int32_t active = aux->active_box;
    int32_t box_x = s->x[active], box_y = s->y[active];
    int32_t box_h = s->h[active], box_w = s->w[active];

    int touched[4];
    touched[0] = ka59_strip_touches(s, st, level, box_x, box_y - 1, box_w, 1);
    touched[1] = ka59_strip_touches(s, st, level, box_x, box_y + box_h, box_w, 1);
    touched[2] = ka59_strip_touches(s, st, level, box_x - 1, box_y, 1, box_h);
    touched[3] = ka59_strip_touches(s, st, level, box_x + box_w, box_y, 1, box_h);

    int8_t *patch = arc_sprite_pixels_mut(s, active);
    const uint8_t *edge = st->box_edge_masks +
                          (((size_t)level * st->num_slots + active) * 4) * area;
    for (size_t p = 0; p < area; p++) {
        int erase = 0;
        for (int d = 0; d < 4; d++) {
            if (touched[d] && edge[(size_t)d * area + p]) {
                erase = 1;
                break;
            }
        }
        if (erase) patch[p] = 0;
    }
}

static int ka59_targets_satisfied(const ArcSprites *s, const int32_t *container, int32_t container_n,
                                  const int32_t *content, int32_t content_n) {
    for (int32_t ci = 0; ci < container_n; ci++) {
        int32_t c = container[ci];
        int32_t tx = s->x[c] + 1, ty = s->y[c] + 1;
        int32_t th = s->h[c] - 2, tw = s->w[c] - 2;
        int satisfied = 0;
        for (int32_t mi = 0; mi < content_n; mi++) {
            int32_t m = content[mi];
            if (!s->alive[m]) continue;
            if (s->x[m] == tx && s->y[m] == ty && s->h[m] == th && s->w[m] == tw) {
                satisfied = 1;
                break;
            }
        }
        if (!satisfied) return 0;
    }
    return 1;
}

static int ka59_is_won(const ArcSprites *s, const Ka59Static *st, int32_t level) {
    const int32_t *holes = st->hole_slots + (size_t)level * st->max_hole;
    int32_t holes_n = st->hole_count[level];
    const int32_t *boxes = st->box_slots + (size_t)level * st->max_box;
    int32_t boxes_n = st->box_count[level];
    if (!ka59_targets_satisfied(s, holes, holes_n, boxes, boxes_n)) return 0;

    const int32_t *zones = st->zone_slots + (size_t)level * st->max_zone;
    int32_t zones_n = st->zone_count[level];
    const int32_t *markers = st->marker_slots + (size_t)level * st->max_marker;
    int32_t markers_n = st->marker_count[level];
    return ka59_targets_satisfied(s, zones, zones_n, markers, markers_n);
}

static int ka59_tick_fuses(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux) {
    size_t area = ka59_area(st);
    const int32_t *bombs = st->bomb_slots + (size_t)level * st->max_bomb;
    int32_t bomb_n = st->bomb_count[level];
    int any_new = 0;

    for (int32_t k = 0; k < bomb_n; k++) {
        int32_t b = bombs[k];
        size_t idx = (size_t)level * st->num_slots + b;
        int32_t new_progress = aux->fuse_progress[b] + 1;
        int32_t total_bad = aux->fuse_first_cycle[b] ? st->fuse_total_bad[idx] : s->h[b];
        int last_row_bad = aux->fuse_first_cycle[b] ? (int)st->fuse_last_row_bad[idx] : 1;
        int completes = last_row_bad && (new_progress == total_bad);

        int32_t cycle = aux->fuse_first_cycle[b] ? 0 : 1;
        int32_t p_index = new_progress;
        if (p_index < 0) p_index = 0;
        if (p_index > st->max_bomb_h) p_index = st->max_bomb_h;

        const int8_t *frame_src = st->fuse_frames +
                                  (((idx * 2 + (size_t)cycle) * (size_t)(st->max_bomb_h + 1) +
                                    (size_t)p_index)) *
                                      area;
        int8_t *dst = arc_sprite_pixels_mut(s, b);
        memcpy(dst, frame_src, area);

        aux->fuse_progress[b] = completes ? 0 : new_progress;
        aux->fuse_first_cycle[b] = (uint8_t)(aux->fuse_first_cycle[b] && !completes);
        aux->exploding[b] = (uint8_t)completes;
        any_new |= completes;
    }
    return any_new;
}

static void ka59_tail_common(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                             int32_t *score, int32_t *status, uint8_t *next_level,
                             uint8_t *action_complete) {
    ka59_refresh_borders(s, st, level, aux);
    int won = ka59_is_won(s, st, level);
    int out_of_steps = aux->steps == 0;
    if (won) {
        int is_last = level == st->num_levels - 1;
        *score += 1;
        *next_level = (uint8_t)!is_last;
        if (is_last) *status = KA59_WIN;
    } else if (out_of_steps) {
        *status = KA59_GAME_OVER;
    }
    *action_complete = 1;
}

static void ka59_tail_with_tick(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                                int32_t *score, int32_t *status, uint8_t *next_level,
                                uint8_t *action_complete) {
    int any_new = ka59_tick_fuses(s, st, level, aux);
    if (any_new) {
        aux->retry_frame = 0;
        return;
    }
    ka59_tail_common(s, st, level, aux, score, status, next_level, action_complete);
}

static void ka59_select_box(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                            int32_t new_slot) {
    size_t area = ka59_area(st);
    int32_t old_slot = aux->active_box;

    int8_t *old_patch = arc_sprite_pixels_mut(s, old_slot);
    const uint8_t *old_mask =
        st->center_dot_mask + ((size_t)level * st->num_slots + old_slot) * area;
    for (size_t p = 0; p < area; p++)
        if (old_mask[p]) old_patch[p] = KA59_DESELECTED_DOT;

    int8_t *new_patch = arc_sprite_pixels_mut(s, new_slot);
    const uint8_t *new_mask =
        st->center_dot_mask + ((size_t)level * st->num_slots + new_slot) * area;
    for (size_t p = 0; p < area; p++)
        if (new_mask[p]) new_patch[p] = KA59_SELECTED_DOT;

    aux->active_box = new_slot;
}

static void ka59_handle_click(ArcSprites *s, const ArcCamera *camera, const Ka59Static *st, int32_t level,
                              int32_t action_x, int32_t action_y, Ka59Aux *aux, int32_t *score,
                              int32_t *status, uint8_t *next_level, uint8_t *action_complete) {
    aux->steps = aux->steps > 0 ? aux->steps - 1 : 0;

    int32_t wx, wy;
    int on_board;
    ka59_display_to_grid(camera, action_x, action_y, &wx, &wy, &on_board);
    int32_t hit = arc_get_sprite_at(s, wx, wy, st->box_tag, 0);

    if (on_board && hit >= 0) ka59_select_box(s, st, level, aux, hit);

    ka59_tail_common(s, st, level, aux, score, status, next_level, action_complete);
}

static int ka59_probe_move(ArcSprites *s, const Ka59Static *st, int32_t level, int32_t slot,
                           int32_t dx, int32_t dy, Ka59Aux *aux) {
    s->x[slot] += dx;
    s->y[slot] += dy;

    int hard_hit = 0;
    const int32_t *wall = st->wall_slots + (size_t)level * st->max_wall;
    int32_t wall_n = st->wall_count[level];
    for (int32_t k = 0; k < wall_n && !hard_hit; k++)
        if (arc_collides_pair(s, slot, wall[k], 0)) hard_hit = 1;
    const int32_t *border = st->border_slots + (size_t)level * st->max_border;
    int32_t border_n = st->border_count[level];
    for (int32_t k = 0; k < border_n && !hard_hit; k++)
        if (arc_collides_pair(s, slot, border[k], 0)) hard_hit = 1;

    const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
    int32_t occ_n = st->occupant_count[level];
    int any_pushable = 0;
    for (int32_t k = 0; k < occ_n; k++) {
        int32_t o = occ[k];
        int hit = !hard_hit && arc_collides_pair(s, slot, o, 0);
        aux->push_active[o] = (uint8_t)hit;
        any_pushable |= hit;
    }

    if (hard_hit || any_pushable) {
        s->x[slot] -= dx;
        s->y[slot] -= dy;
    }
    return any_pushable;
}

static void ka59_handle_move(ArcSprites *s, const Ka59Static *st, int32_t level, int32_t dir_idx,
                             Ka59Aux *aux, int32_t *next_order, int32_t *score, int32_t *status,
                             uint8_t *next_level, uint8_t *action_complete) {
    aux->steps = aux->steps > 0 ? aux->steps - 1 : 0;

    int32_t dx = 0, dy = 0;
    switch (dir_idx) {
        case 0: dy = -KA59_PITCH; break;
        case 1: dy = KA59_PITCH; break;
        case 2: dx = -KA59_PITCH; break;
        default: dx = KA59_PITCH; break;
    }

    int32_t active = aux->active_box;
    int any_pushable = ka59_probe_move(s, st, level, active, dx, dy, aux);

    if (any_pushable) {
        size_t area = ka59_area(st);
        const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
        int32_t occ_n = st->occupant_count[level];
        for (int32_t k = 0; k < occ_n; k++) {
            int32_t o = occ[k];
            if (!aux->push_active[o]) continue;
            aux->recoil_dx[o] = dx;
            aux->recoil_dy[o] = dy;
            aux->has_recoil[o] = 1;
        }

        int8_t *patch = arc_sprite_pixels_mut(s, active);
        const uint8_t *edge = st->box_edge_masks +
                              ((((size_t)level * st->num_slots + active) * 4) + (size_t)dir_idx) *
                                  area;
        for (size_t p = 0; p < area; p++)
            if (edge[p]) patch[p] = KA59_HIGHLIGHT;

        int32_t box_x = s->x[active], box_y = s->y[active];
        int32_t box_h = s->h[active], box_w = s->w[active];
        int32_t ch = box_h + 2, cw = box_w + 2;
        int32_t ph = st->ph, pw = st->pw;

        int8_t *cpatch = arc_sprite_pixels_mut(s, st->collider_slot);
        for (int32_t row = 0; row < ph; row++) {
            for (int32_t col = 0; col < pw; col++) {
                int strip;
                switch (dir_idx) {
                    case 0: strip = row == 0 && col >= 1 && col < 1 + box_w; break;
                    case 1: strip = row == ch - 1 && col >= 1 && col < 1 + box_w; break;
                    case 2: strip = col == 0 && row >= 1 && row < 1 + box_h; break;
                    default: strip = col == cw - 1 && row >= 1 && row < 1 + box_h; break;
                }
                cpatch[(size_t)row * pw + col] = (int8_t)(strip ? 0 : -1);
            }
        }
        s->x[st->collider_slot] = box_x - 1;
        s->y[st->collider_slot] = box_y - 1;
        s->h[st->collider_slot] = ch;
        s->w[st->collider_slot] = cw;
        s->alive[st->collider_slot] = 1;
        s->order[st->collider_slot] = *next_order;
        s->interaction[st->collider_slot] = TANGIBLE;
        s->blocking[st->collider_slot] = PIXEL_PERFECT;
        (*next_order)++;

        aux->retry_frame = 0;
        aux->collider_dir = dir_idx;
    } else {
        ka59_tail_with_tick(s, st, level, aux, score, status, next_level, action_complete);
    }
}

static int ka59_retry_push_frame(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux) {
    const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
    int32_t occ_n = st->occupant_count[level];
    int still_scanning = 1;

    for (int32_t k = 0; k < occ_n; k++) {
        int32_t i = occ[k];
        if (!aux->push_active[i]) continue;

        int wall_hit = 0;
        const int32_t *wall = st->wall_slots + (size_t)level * st->max_wall;
        int32_t wall_n = st->wall_count[level];
        for (int32_t w = 0; w < wall_n && !wall_hit; w++)
            if (arc_collides_pair(s, i, wall[w], 0)) wall_hit = 1;

        int settle_now = (aux->retry_frame >= KA59_RETRY_THRESHOLD) && !wall_hit;
        int result;
        if (settle_now) {
            result = 1;
        } else {
            result = ka59_push_chain(s, st, level, i, aux->recoil_dx[i], aux->recoil_dy[i],
                                     KA59_MAX_PUSH_DEPTH);
        }

        if (!result) {
            still_scanning = 0;
            break;
        }
    }
    return still_scanning;
}

static void ka59_push_retry_dispatch(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                                     int32_t *score, int32_t *status, uint8_t *next_level,
                                     uint8_t *action_complete) {
    int finished = ka59_retry_push_frame(s, st, level, aux);
    if (finished) {
        s->alive[st->collider_slot] = 0;
        for (int32_t i = 0; i < st->num_slots; i++) aux->push_active[i] = 0;
        aux->collider_dir = -1;
        ka59_tail_with_tick(s, st, level, aux, score, status, next_level, action_complete);
    } else {
        aux->retry_frame += 1;
        ka59_refresh_borders(s, st, level, aux);
    }
}

static void ka59_spawn_explosion_frame(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                                       int32_t *next_order) {
    size_t area = ka59_area(st);
    int32_t frame = aux->retry_frame;
    int32_t k = frame + 1;

    const int32_t *bombs = st->bomb_slots + (size_t)level * st->max_bomb;
    int32_t bomb_n = st->bomb_count[level];
    const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
    int32_t occ_n = st->occupant_count[level];

    for (int32_t bi = 0; bi < bomb_n; bi++) {
        int32_t b = bombs[bi];
        if (!aux->exploding[b]) continue;

        size_t idx = (size_t)level * st->num_slots + b;
        int32_t ex_slot = st->explosion_base_slot[idx] + frame;
        int32_t bx = s->x[b] - KA59_PITCH * k;
        int32_t by = s->y[b] - KA59_PITCH * k;
        const int8_t *pix = st->explosion_pixels + (idx * 3 + (size_t)frame) * area;
        int32_t eh = st->explosion_h[idx * 3 + (size_t)frame];
        int32_t ew = st->explosion_w[idx * 3 + (size_t)frame];

        int8_t *dst = arc_sprite_pixels_mut(s, ex_slot);
        memcpy(dst, pix, area);
        s->x[ex_slot] = bx;
        s->y[ex_slot] = by;
        s->h[ex_slot] = eh;
        s->w[ex_slot] = ew;
        s->alive[ex_slot] = 1;
        s->order[ex_slot] = *next_order;
        s->interaction[ex_slot] = TANGIBLE;
        s->blocking[ex_slot] = PIXEL_PERFECT;
        (*next_order)++;

        int32_t dx = st->explosion_recoil_dx[idx], dy = st->explosion_recoil_dy[idx];
        for (int32_t oi = 0; oi < occ_n; oi++) {
            int32_t o = occ[oi];
            if (!arc_collides_pair(s, ex_slot, o, 0)) continue;
            ka59_push_chain(s, st, level, o, dx, dy, KA59_MAX_PUSH_DEPTH);
            aux->recoil_dx[o] = dx;
            aux->recoil_dy[o] = dy;
            aux->has_recoil[o] = 1;
        }
    }

    aux->retry_frame = frame + 1;
    ka59_refresh_borders(s, st, level, aux);
}

static void ka59_finalize_explosion(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                                    int32_t *score, int32_t *status, uint8_t *next_level,
                                    uint8_t *action_complete) {
    size_t area = ka59_area(st);

    for (int32_t i = 0; i < st->explosion_slots_total; i++)
        s->alive[st->explosion_base + i] = 0;

    const int32_t *bombs = st->bomb_slots + (size_t)level * st->max_bomb;
    int32_t bomb_n = st->bomb_count[level];
    for (int32_t bi = 0; bi < bomb_n; bi++) {
        int32_t b = bombs[bi];
        if (!aux->exploding[b]) continue;
        size_t idx = (size_t)level * st->num_slots + b;
        const int8_t *spent =
            st->fuse_frames + ((idx * 2 + 1) * (size_t)(st->max_bomb_h + 1) + 0) * area;
        int8_t *dst = arc_sprite_pixels_mut(s, b);
        memcpy(dst, spent, area);
    }

    const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
    int32_t occ_n = st->occupant_count[level];
    int any_retry = 0;
    for (int32_t oi = 0; oi < occ_n; oi++) {
        int32_t o = occ[oi];
        if (!aux->has_recoil[o]) continue;

        int wall_hit = 0;
        const int32_t *wall = st->wall_slots + (size_t)level * st->max_wall;
        int32_t wall_n = st->wall_count[level];
        for (int32_t w = 0; w < wall_n && !wall_hit; w++)
            if (arc_collides_pair(s, o, wall[w], 0)) wall_hit = 1;
        if (!wall_hit) continue;

        int blocked = ka59_push_chain(s, st, level, o, aux->recoil_dx[o], aux->recoil_dy[o],
                                      KA59_MAX_PUSH_DEPTH);
        if (!blocked) any_retry = 1;
    }

    if (any_retry) return;

    for (int32_t bi = 0; bi < bomb_n; bi++) aux->exploding[bombs[bi]] = 0;
    ka59_tail_common(s, st, level, aux, score, status, next_level, action_complete);
}

static void ka59_explosion_dispatch(ArcSprites *s, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                                    int32_t *next_order, int32_t *score, int32_t *status,
                                    uint8_t *next_level, uint8_t *action_complete) {
    if (aux->retry_frame < 3) {
        ka59_spawn_explosion_frame(s, st, level, aux, next_order);
    } else {
        ka59_finalize_explosion(s, st, level, aux, score, status, next_level, action_complete);
    }
}

static void ka59_dispatch_action(ArcSprites *s, const ArcCamera *camera, const Ka59Static *st,
                                 int32_t level, int32_t action_id, int32_t action_x,
                                 int32_t action_y, Ka59Aux *aux, int32_t *next_order,
                                 int32_t *score, int32_t *status, uint8_t *next_level,
                                 uint8_t *action_complete) {
    switch (action_id) {
        case KA59_ACTION1:
            ka59_handle_move(s, st, level, 0, aux, next_order, score, status, next_level,
                             action_complete);
            break;
        case KA59_ACTION2:
            ka59_handle_move(s, st, level, 1, aux, next_order, score, status, next_level,
                             action_complete);
            break;
        case KA59_ACTION3:
            ka59_handle_move(s, st, level, 2, aux, next_order, score, status, next_level,
                             action_complete);
            break;
        case KA59_ACTION4:
            ka59_handle_move(s, st, level, 3, aux, next_order, score, status, next_level,
                             action_complete);
            break;
        case KA59_ACTION6:
            ka59_handle_click(s, camera, st, level, action_x, action_y, aux, score, status,
                              next_level, action_complete);
            break;
        default:
            ka59_tail_common(s, st, level, aux, score, status, next_level, action_complete);
            break;
    }
}

void ka59_aux_alloc(Ka59Aux *aux, int32_t num_slots) {
    aux->push_active = calloc((size_t)num_slots, sizeof(uint8_t));
    aux->recoil_dx = calloc((size_t)num_slots, sizeof(int32_t));
    aux->recoil_dy = calloc((size_t)num_slots, sizeof(int32_t));
    aux->has_recoil = calloc((size_t)num_slots, sizeof(uint8_t));
    aux->exploding = calloc((size_t)num_slots, sizeof(uint8_t));
    aux->fuse_progress = calloc((size_t)num_slots, sizeof(int32_t));
    aux->fuse_first_cycle = calloc((size_t)num_slots, sizeof(uint8_t));
    aux->active_box = 0;
    aux->retry_frame = 0;
    aux->collider_dir = -1;
    aux->steps = 0;
}

void ka59_aux_free(Ka59Aux *aux) {
    free(aux->push_active);
    free(aux->recoil_dx);
    free(aux->recoil_dy);
    free(aux->has_recoil);
    free(aux->exploding);
    free(aux->fuse_progress);
    free(aux->fuse_first_cycle);
    aux->push_active = NULL;
    aux->recoil_dx = NULL;
    aux->recoil_dy = NULL;
    aux->has_recoil = NULL;
    aux->exploding = NULL;
    aux->fuse_progress = NULL;
    aux->fuse_first_cycle = NULL;
}

void ka59_zero_aux(Ka59Aux *aux, const Ka59Static *st) {
    for (int32_t i = 0; i < st->num_slots; i++) {
        aux->push_active[i] = 0;
        aux->recoil_dx[i] = 0;
        aux->recoil_dy[i] = 0;
        aux->has_recoil[i] = 0;
        aux->exploding[i] = 0;
        aux->fuse_progress[i] = 0;
        aux->fuse_first_cycle[i] = 1;
    }
    aux->active_box = 0;
    aux->retry_frame = 0;
    aux->collider_dir = -1;
    aux->steps = 0;
}

void ka59_on_set_level(ArcSprites *sprites, const Ka59Static *st, int32_t level, Ka59Aux *aux,
                       int32_t *next_order) {
    *next_order = st->num_slots;
    ka59_zero_aux(aux, st);

    int32_t active = st->first_box[level];
    size_t area = ka59_area(st);
    int8_t *patch = arc_sprite_pixels_mut(sprites, active);
    const uint8_t *mask = st->center_dot_mask + ((size_t)level * st->num_slots + active) * area;
    for (size_t p = 0; p < area; p++)
        if (mask[p]) patch[p] = KA59_SELECTED_DOT;

    aux->active_box = active;
    aux->steps = st->step_budget[level];

    ka59_refresh_borders(sprites, st, level, aux);
}

void ka59_step_once(ArcSprites *sprites, const ArcCamera *camera, const Ka59Static *st, int32_t level,
                    int32_t action_id, int32_t action_x, int32_t action_y, int32_t action_count,
                    Ka59Aux *aux, int32_t *next_order, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete) {
    (void)action_count;

    const int32_t *occ = st->occupant_slots + (size_t)level * st->max_occupant;
    int32_t occ_n = st->occupant_count[level];
    int any_push = 0;
    for (int32_t k = 0; k < occ_n && !any_push; k++)
        if (aux->push_active[occ[k]]) any_push = 1;

    if (any_push) {
        ka59_push_retry_dispatch(sprites, st, level, aux, score, status, next_level,
                                 action_complete);
        return;
    }

    const int32_t *bombs = st->bomb_slots + (size_t)level * st->max_bomb;
    int32_t bomb_n = st->bomb_count[level];
    int any_exploding = 0;
    for (int32_t k = 0; k < bomb_n && !any_exploding; k++)
        if (aux->exploding[bombs[k]]) any_exploding = 1;

    if (any_exploding) {
        ka59_explosion_dispatch(sprites, st, level, aux, next_order, score, status, next_level,
                                action_complete);
        return;
    }

    ka59_dispatch_action(sprites, camera, st, level, action_id, action_x, action_y, aux,
                         next_order, score, status, next_level, action_complete);
}

void ka59_render_interface(int8_t *frame, const Ka59Static *st, int32_t level,
                           const Ka59Aux *aux) {
    int32_t budget = st->step_budget[level];
    if (budget == 0) return;

    int32_t total = ARC_FRAME_SIZE * aux->steps;
    int32_t whole = total / budget, rest = total % budget;
    int round_up = (2 * rest > budget) || (2 * rest == budget && (whole % 2 == 1));
    int32_t filled = whole + (round_up ? 1 : 0);

    int8_t *row = frame + (size_t)(ARC_FRAME_SIZE - 1) * ARC_FRAME_SIZE;
    for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
        row[c] = (int8_t)(c < filled ? 4 : 0);
}
