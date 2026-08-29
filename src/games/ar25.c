#include "ar25.h"

#include <string.h>

enum { AR25_WIN = 2, AR25_GAME_OVER = 3 };
enum { AR25_ACTION1 = 1, AR25_ACTION2 = 2, AR25_ACTION3 = 3, AR25_ACTION4 = 4,
       AR25_ACTION5 = 5, AR25_ACTION6 = 6, AR25_ACTION7 = 7 };
enum { AR25_GHOST_COLOR = 4 };
enum { AR25_TARGET_COLOR = 11, AR25_EXCLUDED_MOVABLE_COLOR = 5,
       AR25_EXCLUDED_MIRROR_COLOR = 10, AR25_SELECTED_COLOR = 0,
       AR25_PLAIN_COLOR = 9 };
enum { AR25_PRIORITY_TARGET = 4, AR25_PRIORITY_EXCLUDED = 3,
       AR25_PRIORITY_SELECTED = 2, AR25_PRIORITY_PLAIN = 1,
       AR25_PRIORITY_NONE = -1 };
enum { AR25_ENERGY_TIERS = 5 };
enum { AR25_HINT_FRAMES = 8 };
enum { AR25_MINIMAP_SCALE = 3, AR25_MINIMAP_OFFSET = 1 };

static const int8_t AR25_ENERGY_COLORS[AR25_ENERGY_TIERS] = {11, 12, 15, 8, 14};

static void ar25_display_to_grid(const ArcCamera *camera, int32_t display_x,
                                 int32_t display_y, int32_t *world_x,
                                 int32_t *world_y, int *valid) {
    int32_t scale, x_pad, y_pad;
    arc_scale_and_offset(camera, &scale, &x_pad, &y_pad);
    int32_t dx = display_x - x_pad, dy = display_y - y_pad;
    int32_t grid_x = dx >= 0 ? dx / scale : -1;
    int32_t grid_y = dy >= 0 ? dy / scale : -1;
    *valid = grid_x >= 0 && grid_y >= 0 && grid_x < camera->width &&
            grid_y < camera->height;
    *world_x = grid_x + camera->x;
    *world_y = grid_y + camera->y;
}

static int ar25_hit_test(const ArcSprites *s, int32_t slot, int32_t x, int32_t y) {
    if (slot < 0) return 0;
    int32_t sx = s->x[slot], sy = s->y[slot], sw = s->w[slot], sh = s->h[slot];
    if (!(x >= sx && y >= sy && x < sx + sw && y < sy + sh)) return 0;
    int32_t pw = s->atlas->pw;
    int8_t pv = arc_sprite_pixels(s, slot)[(size_t)(y - sy) * pw + (x - sx)];
    return pv != -1;
}

static void ar25_compute_layers(const ArcSprites *s, const Ar25Static *st,
                                int32_t level, int8_t color_grid[AR25_GRID][AR25_GRID],
                                int8_t owner_grid[AR25_GRID][AR25_GRID],
                                uint8_t true_grid[AR25_GRID][AR25_GRID]) {
    for (int32_t y = 0; y < AR25_GRID; y++) {
        for (int32_t x = 0; x < AR25_GRID; x++) {
            color_grid[y][x] = -1;
            owner_grid[y][x] = -1;
            true_grid[y][x] = 0;
        }
    }

    int32_t vslot = st->vmirror_slot[level];
    int32_t hslot = st->hmirror_slot[level];
    int has_v = vslot >= 0;
    int has_h = hslot >= 0;
    int32_t mx = s->x[has_v ? vslot : 0];
    int32_t my = s->y[has_h ? hslot : 0];
    int32_t pw = s->atlas->pw;

    for (int32_t pi = 0; pi < 2; pi++) {
        int32_t slot = st->movable_slot[level * 2 + pi];
        if (slot < 0) continue;
        const int8_t *pixels = arc_sprite_pixels(s, slot);
        int32_t px = s->x[slot], py = s->y[slot];
        int32_t sh = s->h[slot], sw = s->w[slot];
        for (int32_t li = 0; li < sh; li++) {
            for (int32_t lj = 0; lj < sw; lj++) {
                if (pixels[(size_t)li * pw + lj] < 0) continue;
                int32_t ax = px + lj, ay = py + li;
                if (has_v) {
                    int32_t tx = 2 * mx - ax, ty = ay;
                    if (tx >= 0 && tx < AR25_GRID && ty >= 0 && ty < AR25_GRID) {
                        color_grid[ty][tx] = AR25_GHOST_COLOR;
                        owner_grid[ty][tx] = (int8_t)pi;
                        true_grid[ty][tx] = 0;
                    }
                }
                if (has_h) {
                    int32_t tx = ax, ty = 2 * my - ay;
                    if (tx >= 0 && tx < AR25_GRID && ty >= 0 && ty < AR25_GRID) {
                        color_grid[ty][tx] = AR25_GHOST_COLOR;
                        owner_grid[ty][tx] = (int8_t)pi;
                        true_grid[ty][tx] = 0;
                    }
                }
                if (has_v && has_h) {
                    int32_t tx = 2 * mx - ax, ty = 2 * my - ay;
                    if (tx >= 0 && tx < AR25_GRID && ty >= 0 && ty < AR25_GRID) {
                        color_grid[ty][tx] = AR25_GHOST_COLOR;
                        owner_grid[ty][tx] = (int8_t)pi;
                        true_grid[ty][tx] = 0;
                    }
                }
            }
        }
    }

    for (int32_t pi = 0; pi < 2; pi++) {
        int32_t slot = st->movable_slot[level * 2 + pi];
        if (slot < 0) continue;
        const int8_t *pixels = arc_sprite_pixels(s, slot);
        int32_t px = s->x[slot], py = s->y[slot];
        int32_t sh = s->h[slot], sw = s->w[slot];
        for (int32_t li = 0; li < sh; li++) {
            for (int32_t lj = 0; lj < sw; lj++) {
                int8_t pv = pixels[(size_t)li * pw + lj];
                if (pv < 0) continue;
                int32_t ax = px + lj, ay = py + li;
                if (ax >= 0 && ax < AR25_GRID && ay >= 0 && ay < AR25_GRID) {
                    color_grid[ay][ax] = pv;
                    owner_grid[ay][ax] = (int8_t)pi;
                    true_grid[ay][ax] = 1;
                }
            }
        }
    }
}

static void ar25_refresh_ghost_overlay(ArcSprites *s, const Ar25Static *st, int32_t level) {
    int8_t color_grid[AR25_GRID][AR25_GRID];
    int8_t owner_grid[AR25_GRID][AR25_GRID];
    uint8_t true_grid[AR25_GRID][AR25_GRID];
    ar25_compute_layers(s, st, level, color_grid, owner_grid, true_grid);

    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int8_t *buf = arc_sprite_pixels_mut(s, st->ghost_slot);
    memset(buf, -1, (size_t)ph * pw);
    for (int32_t y = 0; y < AR25_GRID; y++)
        for (int32_t x = 0; x < AR25_GRID; x++)
            buf[(size_t)y * pw + x] = color_grid[y][x];
}

static int ar25_check_win(const ArcSprites *s, const Ar25Static *st, int32_t level) {
    int8_t color_grid[AR25_GRID][AR25_GRID];
    int8_t owner_grid[AR25_GRID][AR25_GRID];
    uint8_t true_grid[AR25_GRID][AR25_GRID];
    ar25_compute_layers(s, st, level, color_grid, owner_grid, true_grid);
    const uint8_t *target = st->target_grid + (size_t)level * AR25_GRID * AR25_GRID;
    for (int32_t y = 0; y < AR25_GRID; y++)
        for (int32_t x = 0; x < AR25_GRID; x++)
            if (target[(size_t)y * AR25_GRID + x] && color_grid[y][x] < 0) return 0;
    return 1;
}

static void ar25_role_slots(const Ar25Static *st, int32_t level, int32_t roles[4]) {
    roles[0] = st->movable_slot[level * 2 + 0];
    roles[1] = st->movable_slot[level * 2 + 1];
    roles[2] = st->axis_slot[level * 2 + 0];
    roles[3] = st->axis_slot[level * 2 + 1];
}

static void ar25_push_undo(ArcSprites *s, const Ar25Static *st, int32_t level, Ar25Aux *aux) {
    int32_t roles[4];
    ar25_role_slots(st, level, roles);
    int32_t idx = aux->undo_top < AR25_MAX_UNDO - 1 ? aux->undo_top : AR25_MAX_UNDO - 1;
    for (int32_t r = 0; r < 4; r++) {
        if (roles[r] < 0) continue;
        aux->undo_x[idx][r] = s->x[roles[r]];
        aux->undo_y[idx][r] = s->y[roles[r]];
    }
    aux->undo_top = aux->undo_top + 1 < AR25_MAX_UNDO - 1 ? aux->undo_top + 1 : AR25_MAX_UNDO - 1;
}

static void ar25_pop_undo(ArcSprites *s, const Ar25Static *st, int32_t level, Ar25Aux *aux) {
    if (aux->undo_top <= 0) return;
    int32_t top = aux->undo_top - 1;
    int32_t roles[4];
    ar25_role_slots(st, level, roles);
    for (int32_t r = 0; r < 4; r++) {
        if (roles[r] < 0) continue;
        arc_set_position(s, roles[r], aux->undo_x[top][r], aux->undo_y[top][r]);
    }
    aux->undo_top = top;
    ar25_refresh_ghost_overlay(s, st, level);
}

static void ar25_move_selected(ArcSprites *s, const Ar25Static *st, int32_t level,
                               int32_t dx0, int32_t dy0, Ar25Aux *aux,
                               int32_t *status, uint8_t *action_complete) {
    int32_t selected = aux->selected_slot;
    int32_t vslot = st->vmirror_slot[level], hslot = st->hmirror_slot[level];
    int is_vmirror = selected == vslot;
    int is_hmirror = selected == hslot;
    int32_t dy = is_vmirror ? 0 : dy0;
    int32_t dx = is_hmirror ? 0 : dx0;

    int32_t x = s->x[selected], y = s->y[selected];
    int32_t w = s->w[selected], h = s->h[selected];
    int32_t nx = x + dx, ny = y + dy;
    int x_oob = nx < 0 || nx + w > AR25_GRID;
    int y_oob = ny < 0 || ny + h > AR25_GRID;
    int blocked = (x_oob && !is_hmirror) || (y_oob && !is_vmirror);
    if (blocked) {
        *action_complete = 1;
        return;
    }

    int moved = dx != 0 || dy != 0;
    if (moved) ar25_push_undo(s, st, level, aux);
    arc_set_position(s, selected, nx, ny);
    ar25_refresh_ghost_overlay(s, st, level);

    if (ar25_check_win(s, st, level)) {
        aux->won_pending = 1;
        return;
    }

    if (!moved) {
        *action_complete = 1;
        return;
    }

    int32_t energy = aux->energy;
    int32_t decremented = energy > 0 ? energy - 1 : energy;
    int still_alive = decremented > 0;
    aux->energy = decremented;
    if (!still_alive) *status = AR25_GAME_OVER;

    int hint_trigger = level == 1 && aux->hint_frame == 0 && aux->energy < 50 &&
                       !aux->changed_selection;
    if (hint_trigger) {
        aux->hint_frame = 1;
    } else {
        *action_complete = 1;
    }
}

static void ar25_advance_hint_animation(ArcSprites *s, const Ar25Static *st, int32_t level,
                                        Ar25Aux *aux, uint8_t *action_complete) {
    int32_t frame = aux->hint_frame + 1;
    int32_t piece_slot = st->movable_slot[level * 2 + 0];
    int32_t piece_x = s->x[piece_slot], piece_y = s->y[piece_slot];
    int show = (frame % 2) == 1;
    int32_t hint_x = show ? piece_x : 500;
    arc_set_position(s, st->hint_slot, hint_x, piece_y);
    aux->hint_frame = frame;
    if (frame >= AR25_HINT_FRAMES) {
        aux->hint_frame = -1;
        *action_complete = 1;
    }
}

static void ar25_cycle_selection(const Ar25Static *st, int32_t level, Ar25Aux *aux,
                                 int32_t *status) {
    const int32_t *order = st->cycle_order + (size_t)level * 4;
    int32_t count = st->cycle_count[level];
    int32_t current_index = -1;
    for (int32_t i = 0; i < 4; i++) {
        if (order[i] == aux->selected_slot) {
            current_index = i;
            break;
        }
    }
    int32_t next_index = count > 0 ? (current_index + 1) % count : 0;
    int32_t new_selected = count > 0 ? order[next_index] : aux->selected_slot;
    int changed = new_selected != aux->selected_slot;
    aux->selected_slot = new_selected;
    if (changed) aux->changed_selection = 1;

    int32_t energy = aux->energy;
    int32_t decremented = energy > 0 ? energy - 1 : energy;
    int still_alive = decremented > 0;
    aux->energy = decremented;
    if (!still_alive) *status = AR25_GAME_OVER;
}

static void ar25_click_select(ArcSprites *s, const ArcCamera *camera, const Ar25Static *st,
                              int32_t level, int32_t action_x, int32_t action_y,
                              Ar25Aux *aux) {
    int32_t wx, wy;
    int valid;
    ar25_display_to_grid(camera, action_x, action_y, &wx, &wy, &valid);
    if (!valid) return;

    int32_t movable0 = st->movable_slot[level * 2 + 0];
    int32_t movable1 = st->movable_slot[level * 2 + 1];
    int excluded0 = st->excluded_movable[level * 2 + 0];
    int excluded1 = st->excluded_movable[level * 2 + 1];
    int hit0 = ar25_hit_test(s, movable0, wx, wy) && !excluded0;
    int hit1 = ar25_hit_test(s, movable1, wx, wy) && !excluded1;
    int32_t picked_movable = hit0 ? movable0 : (hit1 ? movable1 : -1);
    int any_movable = hit0 || hit1;

    int32_t vslot = st->vmirror_slot[level], hslot = st->hmirror_slot[level];
    int v_excluded = st->excluded_vmirror[level];
    int h_excluded = st->excluded_hmirror[level];
    int v_hit = ar25_hit_test(s, vslot, wx, wy) && !v_excluded;
    int h_hit = ar25_hit_test(s, hslot, wx, wy) && !h_excluded;
    int both = v_hit && h_hit;

    int32_t selected = aux->selected_slot;
    int32_t default_pick = v_hit ? vslot : hslot;
    int not_among_mirrors = selected != vslot && selected != hslot;
    int32_t toggled = selected == vslot ? hslot : (selected == hslot ? vslot : default_pick);
    int32_t mirror_pick = not_among_mirrors ? default_pick : toggled;
    int32_t picked_mirror = both ? mirror_pick : (v_hit ? vslot : (h_hit ? hslot : -1));
    int any_mirror = v_hit || h_hit;

    int32_t picked = any_movable ? picked_movable : (any_mirror ? picked_mirror : -1);
    int changed = picked >= 0 && picked != selected;
    int32_t new_selected = picked >= 0 ? picked : selected;
    aux->selected_slot = new_selected;
    if (changed) aux->changed_selection = 1;
}

void ar25_zero_aux(Ar25Aux *aux) {
    aux->selected_slot = -1;
    aux->changed_selection = 0;
    aux->won_pending = 0;
    aux->hint_frame = 0;
    aux->energy = 0;
    aux->undo_top = 0;
    memset(aux->undo_x, 0, sizeof aux->undo_x);
    memset(aux->undo_y, 0, sizeof aux->undo_y);
}

void ar25_on_set_level(ArcSprites *sprites, const Ar25Static *st, int32_t level, Ar25Aux *aux) {
    ar25_zero_aux(aux);
    aux->selected_slot = st->initial_selected[level];
    aux->energy = st->steps_budget[level];

    int32_t ph = sprites->atlas->ph, pw = sprites->atlas->pw;
    int32_t area = ph * pw;
    int32_t ghost = st->ghost_slot, hint = st->hint_slot;

    int8_t *gp = arc_sprite_pixels_mut(sprites, ghost);
    memset(gp, -1, (size_t)area);
    sprites->w[ghost] = AR25_GRID;
    sprites->h[ghost] = AR25_GRID;
    sprites->x[ghost] = 0;
    sprites->y[ghost] = 0;
    sprites->layer[ghost] = 0;
    arc_add_sprite(sprites, ghost, st->num_slots);
    arc_set_visible(sprites, ghost, 1);

    int8_t *hp = arc_sprite_pixels_mut(sprites, hint);
    memcpy(hp, st->hint_pixels, (size_t)area);
    sprites->w[hint] = st->hint_w;
    sprites->h[hint] = st->hint_h;
    sprites->layer[hint] = st->hint_layer;
    sprites->x[hint] = 500;
    sprites->y[hint] = 0;
    arc_add_sprite(sprites, hint, st->num_slots + 1);
    arc_set_visible(sprites, hint, 1);

    ar25_refresh_ghost_overlay(sprites, st, level);
}

void ar25_step_once(ArcSprites *sprites, const ArcCamera *camera, const Ar25Static *st,
                    int32_t level, int32_t action_id, int32_t action_x,
                    int32_t action_y, Ar25Aux *aux, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete) {
    if (aux->hint_frame > 0) {
        ar25_advance_hint_animation(sprites, st, level, aux, action_complete);
        return;
    }

    if (aux->won_pending) {
        int is_last = level == st->num_levels - 1;
        *score += 1;
        *next_level = (uint8_t)!is_last;
        if (is_last) *status = AR25_WIN;
        *action_complete = 1;
        return;
    }

    int is_movement = aux->selected_slot >= 0 && action_id >= AR25_ACTION1 &&
                      action_id <= AR25_ACTION4;

    if (action_id == AR25_ACTION7) {
        ar25_pop_undo(sprites, st, level, aux);
        *action_complete = 1;
    } else if (is_movement) {
        int32_t dx = action_id == AR25_ACTION3 ? -1 : (action_id == AR25_ACTION4 ? 1 : 0);
        int32_t dy = action_id == AR25_ACTION1 ? -1 : (action_id == AR25_ACTION2 ? 1 : 0);
        ar25_move_selected(sprites, st, level, dx, dy, aux, status, action_complete);
    } else if (action_id == AR25_ACTION5) {
        ar25_cycle_selection(st, level, aux, status);
        *action_complete = 1;
    } else if (action_id == AR25_ACTION6) {
        ar25_click_select(sprites, camera, st, level, action_x, action_y, aux);
        *action_complete = 1;
    } else {
        *action_complete = 1;
    }
}

static void ar25_render_energy_bar(int8_t *frame, const Ar25Static *st, int32_t level,
                                   const Ar25Aux *aux) {
    int32_t budget = st->steps_budget[level];
    if (budget <= 0) return;
    int32_t current = aux->energy;

    int32_t tiers = (budget + ARC_FRAME_SIZE - 1) / ARC_FRAME_SIZE;
    if (tiers < 1) tiers = 1;
    if (tiers > AR25_ENERGY_TIERS) tiers = AR25_ENERGY_TIERS;
    int32_t max_span = ARC_FRAME_SIZE * tiers;
    int32_t spent = budget - current;
    if (spent < 0) spent = 0;
    if (spent > max_span) spent = max_span;
    int32_t raw_tier = spent / ARC_FRAME_SIZE;
    int32_t within = spent - raw_tier * ARC_FRAME_SIZE;
    int clamp_needed = raw_tier >= tiers;
    int32_t tier_index = clamp_needed ? tiers - 1 : raw_tier;
    if (clamp_needed) within = ARC_FRAME_SIZE;

    int8_t color = AR25_ENERGY_COLORS[tier_index];
    int is_last_tier = tier_index == tiers - 1;
    int32_t next_idx = tier_index + 1 > AR25_ENERGY_TIERS - 1 ? AR25_ENERGY_TIERS - 1 : tier_index + 1;
    int8_t next_color = AR25_ENERGY_COLORS[next_idx];

    int8_t col[ARC_FRAME_SIZE];
    for (int32_t row = 0; row < ARC_FRAME_SIZE; row++) col[row] = frame[(size_t)row * ARC_FRAME_SIZE + ARC_FRAME_SIZE - 1];
    for (int32_t row = 0; row < ARC_FRAME_SIZE; row++)
        if (row >= within) col[row] = color;
    if (!is_last_tier && within > 0)
        for (int32_t row = 0; row < within; row++) col[row] = next_color;
    for (int32_t row = 0; row < ARC_FRAME_SIZE; row++) frame[(size_t)row * ARC_FRAME_SIZE + ARC_FRAME_SIZE - 1] = col[row];
}

static void ar25_paint_axis_slot(int8_t grid[AR25_GRID][AR25_GRID],
                                 int8_t priority[AR25_GRID][AR25_GRID],
                                 const int8_t owner_grid[AR25_GRID][AR25_GRID],
                                 const uint8_t true_grid[AR25_GRID][AR25_GRID],
                                 const ArcSprites *s, const Ar25Static *st, int32_t level,
                                 int32_t slot, int32_t axis_index, int32_t selected_slot) {
    if (slot < 0) return;
    int32_t sx = s->x[slot], sy = s->y[slot], sh = s->h[slot], sw = s->w[slot];
    int32_t pw = s->atlas->pw;
    const int8_t *pixels = arc_sprite_pixels(s, slot);
    const uint8_t *target = st->target_grid + (size_t)level * AR25_GRID * AR25_GRID;
    int excluded = st->excluded_axis[level * 2 + axis_index];
    int selected = selected_slot == slot;
    int8_t plain_color = selected ? AR25_SELECTED_COLOR : AR25_PLAIN_COLOR;
    int8_t plain_priority = selected ? AR25_PRIORITY_SELECTED : AR25_PRIORITY_PLAIN;

    for (int32_t li = 0; li < sh; li++) {
        for (int32_t lj = 0; lj < sw; lj++) {
            if (pixels[(size_t)li * pw + lj] < 0) continue;
            int32_t ax = sx + lj, ay = sy + li;
            if (ax < 0 || ax >= AR25_GRID || ay < 0 || ay >= AR25_GRID) continue;

            int is_target = target[(size_t)ay * AR25_GRID + ax];
            int8_t color = is_target ? AR25_TARGET_COLOR
                                     : (excluded ? AR25_EXCLUDED_MIRROR_COLOR : plain_color);
            int8_t cell_priority = is_target ? AR25_PRIORITY_TARGET
                                             : (excluded ? AR25_PRIORITY_EXCLUDED : plain_priority);

            int8_t owner_at = owner_grid[ay][ax];
            uint8_t true_at = true_grid[ay][ax];
            int skip = !is_target && color == AR25_PLAIN_COLOR && owner_at >= 0 && !true_at;
            if (skip) continue;

            int8_t current_priority = priority[ay][ax];
            int win = current_priority == AR25_PRIORITY_NONE || cell_priority > current_priority;
            if (win) {
                grid[ay][ax] = color;
                priority[ay][ax] = cell_priority;
            }
        }
    }
}

static void ar25_render_minimap(int8_t *frame, const ArcSprites *s, const Ar25Static *st,
                                int32_t level, const Ar25Aux *aux) {
    int8_t color_grid[AR25_GRID][AR25_GRID];
    int8_t owner_grid[AR25_GRID][AR25_GRID];
    uint8_t true_grid[AR25_GRID][AR25_GRID];
    ar25_compute_layers(s, st, level, color_grid, owner_grid, true_grid);
    const uint8_t *target = st->target_grid + (size_t)level * AR25_GRID * AR25_GRID;

    int32_t movable0 = st->movable_slot[level * 2 + 0];
    int32_t movable1 = st->movable_slot[level * 2 + 1];
    int excl0 = st->excluded_movable[level * 2 + 0];
    int excl1 = st->excluded_movable[level * 2 + 1];
    int sel0 = aux->selected_slot == movable0;
    int sel1 = aux->selected_slot == movable1;

    int8_t grid[AR25_GRID][AR25_GRID];
    int8_t priority[AR25_GRID][AR25_GRID];
    for (int32_t y = 0; y < AR25_GRID; y++) {
        for (int32_t x = 0; x < AR25_GRID; x++) {
            int8_t ow = owner_grid[y][x];
            int is_movable_true = ow >= 0 && true_grid[y][x];
            int owner_is0 = ow == 0;
            int selected_here = owner_is0 ? sel0 : sel1;
            int excluded_here = owner_is0 ? excl0 : excl1;
            int8_t movable_color = excluded_here ? AR25_EXCLUDED_MOVABLE_COLOR
                                                 : (selected_here ? AR25_SELECTED_COLOR : AR25_PLAIN_COLOR);
            int8_t movable_priority = excluded_here ? AR25_PRIORITY_EXCLUDED
                                                    : (selected_here ? AR25_PRIORITY_SELECTED : AR25_PRIORITY_PLAIN);
            int is_target = target[(size_t)y * AR25_GRID + x];
            if (is_target) {
                grid[y][x] = AR25_TARGET_COLOR;
                priority[y][x] = AR25_PRIORITY_TARGET;
            } else if (is_movable_true) {
                grid[y][x] = movable_color;
                priority[y][x] = movable_priority;
            } else {
                grid[y][x] = -1;
                priority[y][x] = AR25_PRIORITY_NONE;
            }
        }
    }

    for (int32_t axis_index = 0; axis_index < 2; axis_index++) {
        int32_t slot = st->axis_slot[level * 2 + axis_index];
        ar25_paint_axis_slot(grid, priority, owner_grid, true_grid, s, st, level, slot,
                             axis_index, aux->selected_slot);
    }

    for (int32_t y = 0; y < AR25_GRID; y++) {
        for (int32_t x = 0; x < AR25_GRID; x++) {
            if (grid[y][x] < 0) continue;
            int32_t sy = y * AR25_MINIMAP_SCALE + AR25_MINIMAP_OFFSET;
            int32_t sx = x * AR25_MINIMAP_SCALE + AR25_MINIMAP_OFFSET;
            frame[(size_t)sy * ARC_FRAME_SIZE + sx] = grid[y][x];
        }
    }
}

void ar25_render_interface(int8_t *frame, const ArcSprites *sprites,
                           const Ar25Static *st, int32_t level, const Ar25Aux *aux) {
    ar25_render_energy_bar(frame, st, level, aux);
    ar25_render_minimap(frame, sprites, st, level, aux);
}
