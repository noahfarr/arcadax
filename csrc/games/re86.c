#include "re86.h"

#include <string.h>

enum { STEP_PX = 3, MIN_RESHAPE_DIM = 6, ACTIVE_MARKER = 0 };
enum { RE86_ACTION1 = 1, RE86_ACTION5 = 5 };
enum { RE86_NOT_FINISHED = 1, RE86_WIN = 2, RE86_GAME_OVER = 3 };

static inline int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t snap3(int32_t z) {
    int32_t r = z % 3;
    if (r < 0) r += 3;
    return r <= 1 ? z - r : z + 3 - r;
}

static int8_t *mut_pixels(Sprites *s, int32_t i) {
    int32_t area = s->atlas->ph * s->atlas->pw;
    int8_t *dst = s->pixels + (size_t)i * area;
    if (!s->overridden[i]) {
        memcpy(dst, s->atlas->pixels + (size_t)i * area, (size_t)area);
        s->overridden[i] = 1;
        s->bbox[i * 4 + 0] = 0;
        s->bbox[i * 4 + 1] = s->atlas->ph;
        s->bbox[i * 4 + 2] = 0;
        s->bbox[i * 4 + 3] = s->atlas->pw;
    }
    return dst;
}

static inline int8_t center_value(const Sprites *s, int32_t slot) {
    int32_t pw = s->atlas->pw;
    return sprite_pixels(s, slot)[(s->h[slot] / 2) * pw + s->w[slot] / 2];
}

static inline void set_center(Sprites *s, int32_t slot, int8_t value) {
    int32_t pw = s->atlas->pw;
    mut_pixels(s, slot)[(s->h[slot] / 2) * pw + s->w[slot] / 2] = value;
}

static int8_t dynamic_own_color(const Sprites *s, int32_t slot) {
    const Atlas *a = s->atlas;
    const int8_t *p = sprite_pixels(s, slot);
    int32_t area = a->ph * a->pw;
    for (int32_t k = 0; k < area; k++)
        if (p[k] != 0 && p[k] != -1) return p[k];
    return p[0];
}

static void first_opaque(const Sprites *s, int32_t slot, int32_t *d_out, int32_t *u_out) {
    const Atlas *a = s->atlas;
    const int8_t *p = sprite_pixels(s, slot);
    int32_t h = s->h[slot], w = s->w[slot], pw = a->pw;
    int32_t d = w - 1;
    for (int32_t c = 0; c < w; c++) {
        int all_opaque = 1;
        for (int32_t r = 0; r < h; r++)
            if (p[r * pw + c] == -1) { all_opaque = 0; break; }
        if (all_opaque) { d = c; break; }
    }
    int32_t u = h - 1;
    for (int32_t r = 0; r < h; r++) {
        int all_opaque = 1;
        for (int32_t c = 0; c < w; c++)
            if (p[r * pw + c] == -1) { all_opaque = 0; break; }
        if (all_opaque) { u = r; break; }
    }
    *d_out = d;
    *u_out = u;
}

static void recompute_natural_center(Sprites *s, const Re86Static *st, int32_t level, int32_t slot) {
    int32_t d, u;
    first_opaque(s, slot, &d, &u);
    int32_t center_row = s->h[slot] / 2, center_col = s->w[slot] / 2;
    int8_t own = dynamic_own_color(s, slot);
    int8_t plain_value = (center_row == u || center_col == d) ? own : (int8_t)-1;
    int32_t idx = level * st->num_slots + slot;
    int8_t value;
    if (st->is_reshape[idx]) value = -1;
    else if (st->is_solid_center[idx]) value = own;
    else value = plain_value;
    int32_t pw = s->atlas->pw;
    mut_pixels(s, slot)[center_row * pw + center_col] = value;
}

static void hollow_rect(int8_t *out, int32_t pw_stride, int32_t h, int32_t w, int8_t color) {
    for (int32_t r = 0; r < FRAME_SIZE; r++) {
        for (int32_t c = 0; c < FRAME_SIZE; c++) {
            int inside = r < h && c < w;
            int8_t v = -1;
            if (inside) {
                int border = r == 0 || r == h - 1 || c == 0 || c == w - 1;
                v = border ? color : (int8_t)-1;
            }
            out[r * pw_stride + c] = v;
        }
    }
    out[(h / 2) * pw_stride + w / 2] = ACTIVE_MARKER;
}

static inline void undo_move(Sprites *s, int32_t cursor, int32_t dx, int32_t dy) {
    s->x[cursor] -= dx;
    s->y[cursor] -= dy;
}

static void reshape_effect(Sprites *s, int32_t cursor, int32_t dx, int32_t dy) {
    int32_t h = s->h[cursor], w = s->w[cursor];
    int8_t color = dynamic_own_color(s, cursor);
    int32_t pw = s->atlas->pw;
    if (dx != 0) {
        if (w > MIN_RESHAPE_DIM) {
            int32_t new_h = h + 3, new_w = w - 3;
            hollow_rect(mut_pixels(s, cursor), pw, new_h, new_w, color);
            int32_t y_offset = h / 2 - (h + 3) / 2;
            s->x[cursor] = s->x[cursor] + (dx < 0 ? STEP_PX : 0);
            s->y[cursor] = snap3(s->y[cursor] + y_offset);
            s->h[cursor] = new_h;
            s->w[cursor] = new_w;
        } else {
            undo_move(s, cursor, dx, dy);
        }
    } else {
        if (h > MIN_RESHAPE_DIM) {
            int32_t new_h = h - 3, new_w = w + 3;
            hollow_rect(mut_pixels(s, cursor), pw, new_h, new_w, color);
            int32_t x_offset = w / 2 - (w + 3) / 2;
            s->y[cursor] = s->y[cursor] + (dy < 0 ? STEP_PX : 0);
            s->x[cursor] = snap3(s->x[cursor] + x_offset);
            s->h[cursor] = new_h;
            s->w[cursor] = new_w;
        } else {
            undo_move(s, cursor, dx, dy);
        }
    }
}

static void redraw_column(Sprites *s, int32_t cursor, int32_t d, int32_t delta, int32_t u, int8_t color) {
    const Atlas *a = s->atlas;
    int32_t ph = a->ph, pw = a->pw;
    int8_t *p = mut_pixels(s, cursor);
    int32_t h = s->h[cursor], w = s->w[cursor];
    for (int32_t r = 0; r < ph; r++) p[r * pw + d] = -1;
    int32_t dc = d + delta;
    if (dc >= 0 && dc < pw)
        for (int32_t r = 0; r < h && r < ph; r++) p[r * pw + dc] = color;
    p[u * pw + d] = color;
    p[(h / 2) * pw + w / 2] = ACTIVE_MARKER;
}

static void redraw_row(Sprites *s, int32_t cursor, int32_t u, int32_t delta, int32_t d, int8_t color) {
    const Atlas *a = s->atlas;
    int32_t ph = a->ph, pw = a->pw;
    int8_t *p = mut_pixels(s, cursor);
    int32_t h = s->h[cursor], w = s->w[cursor];
    for (int32_t c = 0; c < pw; c++) p[u * pw + c] = -1;
    int32_t ur = u + delta;
    if (ur >= 0 && ur < ph)
        for (int32_t c = 0; c < w && c < pw; c++) p[ur * pw + c] = color;
    p[u * pw + d] = color;
    p[(h / 2) * pw + w / 2] = ACTIVE_MARKER;
}

static void shift_window_effect(Sprites *s, int32_t cursor, int32_t wall, int32_t dx, int32_t dy) {
    int8_t color = dynamic_own_color(s, cursor);
    int32_t d, u;
    first_opaque(s, cursor, &d, &u);
    int32_t x = s->x[cursor], y = s->y[cursor], w = s->w[cursor], h = s->h[cursor];
    int32_t wx = s->x[wall], wy = s->y[wall], ww = s->w[wall], wh = s->h[wall];
    int col_in_wall = x + d >= wx && x + d < wx + ww;
    int row_in_wall = y + u >= wy && y + u < wy + wh;

    int primary = dy != 0 ? row_in_wall : col_in_wall;
    int secondary = dy != 0 ? col_in_wall : row_in_wall;
    int case_id = (primary && secondary) ? 0 : (primary ? 1 : (secondary ? 2 : 3));
    if (case_id == 3) return;
    if (case_id == 0) { undo_move(s, cursor, dx, dy); return; }

    if (dx != 0) {
        int32_t shrink = dx > 0 ? -3 : 3;
        int32_t grow = dx > 0 ? 3 : -3;
        int room_shrink = dx > 0 ? d > 0 : d < w - 2;
        int room_grow = dx > 0 ? d < w - 2 : d > 0;
        if (case_id == 1) {
            if (room_shrink) redraw_column(s, cursor, d, shrink, u, color);
            else undo_move(s, cursor, dx, dy);
        } else {
            undo_move(s, cursor, dx, dy);
            if (room_grow) redraw_column(s, cursor, d, grow, u, color);
        }
    } else {
        int32_t shrink = dy > 0 ? -3 : 3;
        int32_t grow = dy > 0 ? 3 : -3;
        int room_shrink = dy > 0 ? u > 0 : u < h - 2;
        int room_grow = dy > 0 ? u < h - 2 : u > 0;
        if (case_id == 1) {
            if (room_shrink) redraw_row(s, cursor, u, shrink, d, color);
            else undo_move(s, cursor, dx, dy);
        } else {
            undo_move(s, cursor, dx, dy);
            if (room_grow) redraw_row(s, cursor, u, grow, d, color);
        }
    }
}

static int pixel_overlap_pair(const Sprites *s, int32_t i, int32_t j) {
    const Atlas *a = s->atlas;
    int32_t ph = a->ph, pw = a->pw;
    int32_t dy = s->y[j] - s->y[i];
    int32_t dx = s->x[j] - s->x[i];
    const int8_t *pi = sprite_pixels(s, i);
    const int8_t *pj = sprite_pixels(s, j);
    for (int32_t v = 0; v < ph; v++) {
        int32_t vv = v - dy;
        if (vv < 0 || vv >= ph) continue;
        for (int32_t u = 0; u < pw; u++) {
            int32_t uu = u - dx;
            if (uu < 0 || uu >= pw) continue;
            if (pi[v * pw + u] != -1 && pj[vv * pw + uu] != -1) return 1;
        }
    }
    return 0;
}

static int re86_collides_pair(const Sprites *s, int32_t i, int32_t j) {
    if (!s->alive[j]) return 0;
    int32_t xi = s->x[i], yi = s->y[i], wi = s->w[i], hi = s->h[i];
    int32_t xj = s->x[j], yj = s->y[j], wj = s->w[j], hj = s->h[j];
    if (!(xi < xj + wj && xi + wi > xj && yi < yj + hj && yi + hi > yj)) return 0;
    if (!sprite_collidable(s, i) || !sprite_collidable(s, j)) return 0;
    if (s->blocking[i] == NOT_BLOCKED || s->blocking[j] == NOT_BLOCKED) return 0;
    int pixel_perfect = s->blocking[i] == PIXEL_PERFECT || s->blocking[j] == PIXEL_PERFECT;
    return !pixel_perfect || pixel_overlap_pair(s, i, j);
}

static int32_t target_check(Sprites *s, Re86Aux *aux, const Re86Static *st, int32_t level, int32_t cursor) {
    int32_t n = st->num_flood_targets[level];
    const int32_t *targets = st->flood_target_slots + (size_t)level * st->max_flood_targets;
    int8_t own = dynamic_own_color(s, cursor);
    int32_t pw = s->atlas->pw;
    int32_t target = -1;
    for (int32_t k = 0; k < n; k++) {
        int32_t j = targets[k];
        if (!s->alive[j]) continue;
        if (!re86_collides_pair(s, cursor, j)) continue;
        int8_t fill = sprite_pixels(s, j)[pw + 1];
        if (own == fill) continue;
        target = j;
        break;
    }
    if (target < 0) {
        set_center(s, cursor, ACTIVE_MARKER);
        return -1;
    }

    int32_t ph = s->atlas->ph;
    int32_t tx = s->x[target], ty = s->y[target], tw = s->w[target], th = s->h[target];
    int32_t cx = s->x[cursor], cy = s->y[cursor];
    int8_t fill = sprite_pixels(s, target)[pw + 1];
    int8_t *cp = mut_pixels(s, cursor);
    int32_t r0 = clamp32(ty - cy, 0, ph), r1 = clamp32(ty + th - cy, 0, ph);
    int32_t c0 = clamp32(tx - cx, 0, pw), c1 = clamp32(tx + tw - cx, 0, pw);
    for (int32_t r = r0; r < r1; r++) {
        int8_t *row = cp + (size_t)r * pw;
        for (int32_t c = c0; c < c1; c++)
            if (row[c] != -1) row[c] = fill;
    }
    aux->flood_target_slot = target;
    aux->flood_cursor_slot = cursor;
    return target;
}

static int32_t move_and_collide(Sprites *s, Re86Aux *aux, const Re86Static *st, int32_t level,
                                int32_t cursor, int32_t dx, int32_t dy,
                                int32_t camera_width, int32_t camera_height) {
    int32_t old_x = s->x[cursor], old_y = s->y[cursor];
    int32_t new_x = old_x + dx, new_y = old_y + dy;
    int32_t h = s->h[cursor], w = s->w[cursor];
    int32_t cx = new_x + w / 2, cy = new_y + h / 2;
    if (!(cx >= 0 && cy >= 0 && cx < camera_width && cy < camera_height)) return -1;

    s->x[cursor] = new_x;
    s->y[cursor] = new_y;

    int32_t nwalls = st->num_walls[level];
    const int32_t *walls = st->wall_slots + (size_t)level * st->max_walls;
    for (int32_t k = 0; k < nwalls; k++) {
        int32_t i = walls[k];
        if (!s->alive[i]) continue;
        if (!re86_collides_pair(s, cursor, i)) continue;
        if (st->is_reshape[level * st->num_slots + cursor]) reshape_effect(s, cursor, dx, dy);
        else shift_window_effect(s, cursor, i, dx, dy);
    }

    recompute_natural_center(s, st, level, cursor);
    return target_check(s, aux, st, level, cursor);
}

static int32_t find_active(const Sprites *s, const Re86Static *st, int32_t level) {
    int32_t nc = st->num_cursors[level];
    const int32_t *cursors = st->cursor_slot_by_rank + (size_t)level * st->max_cursors;
    for (int32_t k = 0; k < nc; k++) {
        int32_t slot = cursors[k];
        if (s->alive[slot] && center_value(s, slot) == ACTIVE_MARKER) return slot;
    }
    return -1;
}

static inline void spend_step(Re86Aux *aux) {
    aux->steps = aux->steps > 0 ? aux->steps - 1 : 0;
}

static int win_check(const Sprites *s, const Re86Static *st, int32_t level) {
    const Atlas *a = s->atlas;
    int32_t ph = a->ph, pw = a->pw;
    int32_t canvas_h = FRAME_SIZE + 2 * ph, canvas_w = FRAME_SIZE + 2 * pw;
    int8_t canvas[3 * FRAME_SIZE * 3 * FRAME_SIZE];
    memset(canvas, -1, (size_t)canvas_h * canvas_w);
    for (int32_t r = 0; r < FRAME_SIZE; r++)
        memcpy(canvas + (size_t)(ph + r) * canvas_w + pw,
              st->canvas_template + (size_t)r * FRAME_SIZE, FRAME_SIZE);

    int32_t ncur = st->num_cursors[level];
    const int32_t *cursors = st->cursor_slot_by_rank + (size_t)level * st->max_cursors;
    for (int32_t k = 0; k < ncur; k++) {
        int32_t i = cursors[k];
        if (!s->alive[i]) continue;
        const int8_t *patch = sprite_pixels(s, i);
        int32_t sy = clamp32(s->y[i] + ph, 0, canvas_h - ph);
        int32_t sx = clamp32(s->x[i] + pw, 0, canvas_w - pw);
        for (int32_t v = 0; v < ph; v++) {
            const int8_t *src = patch + (size_t)v * pw;
            int8_t *dst = canvas + (size_t)(sy + v) * canvas_w + sx;
            for (int32_t u = 0; u < pw; u++)
                if (src[u] != -1) dst[u] = src[u];
        }
    }

    int32_t target_slot = st->win_target_slot[level];
    const int8_t *target_pixels = sprite_pixels(s, target_slot);
    for (int32_t r = 0; r < FRAME_SIZE; r++) {
        const int8_t *trow = target_pixels + (size_t)r * pw;
        const int8_t *crow = canvas + (size_t)(ph + r) * canvas_w + pw;
        for (int32_t c = 0; c < FRAME_SIZE; c++) {
            int8_t t = trow[c];
            if (t != -1 && t != 4 && t != crow[c]) return 0;
        }
    }
    return 1;
}

static void tail(Sprites *s, Re86Aux *aux, Re86Engine *engine, const Re86Static *st) {
    int32_t level = engine->level_index;
    int won = win_check(s, st, level);
    int lost = aux->steps <= 0;
    if (won) {
        engine->score += 1;
        engine->next_level = 1;
        if (level == st->num_levels - 1) engine->status = RE86_WIN;
    } else if (lost) {
        engine->status = RE86_GAME_OVER;
    }
    engine->action_complete = 1;
}

static void directional(Sprites *s, Re86Aux *aux, Re86Engine *engine, const Re86Static *st,
                        int32_t dx, int32_t dy) {
    spend_step(aux);
    int32_t level = engine->level_index;
    int32_t cursor = find_active(s, st, level);
    if (cursor < 0) { tail(s, aux, engine, st); return; }
    int32_t touched = move_and_collide(s, aux, st, level, cursor, dx, dy,
                                       engine->camera_width, engine->camera_height);
    if (touched >= 0) return;
    tail(s, aux, engine, st);
}

static void cycle_step(Sprites *s, Re86Aux *aux, Re86Engine *engine, const Re86Static *st) {
    spend_step(aux);
    int32_t level = engine->level_index;
    int32_t active = find_active(s, st, level);
    if (active >= 0) {
        int32_t rank = st->cursor_rank[level * st->num_slots + active];
        int32_t nc = st->num_cursors[level];
        int32_t next_rank = rank < nc - 1 ? rank + 1 : 0;
        const int32_t *cursors = st->cursor_slot_by_rank + (size_t)level * st->max_cursors;
        int32_t next_slot = cursors[next_rank];
        for (int32_t k = 0; k < nc; k++)
            if (s->alive[cursors[k]]) recompute_natural_center(s, st, level, cursors[k]);
        set_center(s, next_slot, ACTIVE_MARKER);
    }
    tail(s, aux, engine, st);
}

static void flood_spread(Sprites *s, const Re86Aux *aux) {
    int32_t cursor = aux->flood_cursor_slot, target = aux->flood_target_slot;
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int8_t fill = sprite_pixels(s, target)[pw + 1];
    int8_t *p = mut_pixels(s, cursor);
    int8_t matched[FRAME_SIZE * FRAME_SIZE];
    int32_t area = ph * pw;
    for (int32_t k = 0; k < area; k++) matched[k] = p[k] == fill;
    int8_t next[FRAME_SIZE * FRAME_SIZE];
    memcpy(next, p, (size_t)area);
    for (int32_t r = 0; r < ph; r++) {
        for (int32_t c = 0; c < pw; c++) {
            if (p[r * pw + c] == -1) continue;
            int dilated = 0;
            for (int32_t dr = -1; dr <= 1 && !dilated; dr++) {
                int32_t rr = r + dr;
                if (rr < 0 || rr >= ph) continue;
                for (int32_t dc = -1; dc <= 1; dc++) {
                    int32_t cc = c + dc;
                    if (cc < 0 || cc >= pw) continue;
                    if (matched[rr * pw + cc]) { dilated = 1; break; }
                }
            }
            if (dilated) next[r * pw + c] = fill;
        }
    }
    memcpy(p, next, (size_t)area);
}

static void flood_step(Sprites *s, Re86Aux *aux, Re86Engine *engine, const Re86Static *st) {
    int32_t cursor = aux->flood_cursor_slot, target = aux->flood_target_slot;
    int32_t pw = s->atlas->pw, area = s->atlas->ph * pw;
    int8_t fill = sprite_pixels(s, target)[pw + 1];
    const int8_t *p = sprite_pixels(s, cursor);
    int unresolved = 0;
    for (int32_t k = 0; k < area; k++)
        if (p[k] != -1 && p[k] != fill) { unresolved = 1; break; }

    if (unresolved) {
        flood_spread(s, aux);
        return;
    }
    set_center(s, cursor, ACTIVE_MARKER);
    aux->flood_target_slot = -1;
    aux->flood_cursor_slot = -1;
    tail(s, aux, engine, st);
}

void re86_zero_aux(Re86Aux *aux) {
    aux->steps = 0;
    aux->flood_target_slot = -1;
    aux->flood_cursor_slot = -1;
}

void re86_on_set_level(Re86Aux *aux, const Re86Static *st, int32_t level_index) {
    aux->steps = st->budget[level_index];
    aux->flood_target_slot = -1;
    aux->flood_cursor_slot = -1;
}

void re86_step_once(Sprites *sprites, Re86Aux *aux, Re86Engine *engine,
                    const Re86Static *st, int32_t action_id) {
    engine->action_complete = 0;

    if (aux->flood_cursor_slot >= 0) {
        flood_step(sprites, aux, engine, st);
        return;
    }
    if (action_id < RE86_ACTION1 || action_id > RE86_ACTION5) {
        tail(sprites, aux, engine, st);
        return;
    }
    switch (action_id - RE86_ACTION1) {
        case 0: directional(sprites, aux, engine, st, 0, -STEP_PX); break;
        case 1: directional(sprites, aux, engine, st, 0, STEP_PX); break;
        case 2: directional(sprites, aux, engine, st, -STEP_PX, 0); break;
        case 3: directional(sprites, aux, engine, st, STEP_PX, 0); break;
        default: cycle_step(sprites, aux, engine, st); break;
    }
}

void re86_render_interface(int8_t *frame, const Re86Aux *aux, const Re86Static *st, int32_t level_index) {
    int32_t budget = st->budget[level_index];
    if (budget == 0) return;
    int32_t steps = clamp32(aux->steps, 0, budget);
    int32_t total = FRAME_SIZE * steps;
    int32_t whole = total / budget;
    int32_t rest = total % budget;
    int32_t filled = whole;
    if (2 * rest > budget || (2 * rest == budget && whole % 2 == 1)) filled += 1;
    if (filled > FRAME_SIZE) filled = FRAME_SIZE;
    int8_t *row = frame + (size_t)(FRAME_SIZE - 1) * FRAME_SIZE;
    for (int32_t c = 0; c < FRAME_SIZE; c++) row[c] = c < filled ? 15 : 1;
}
