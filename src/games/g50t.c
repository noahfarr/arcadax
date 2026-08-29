#include "g50t.h"

#include <string.h>

enum { G50T_ACTION1 = 1, G50T_ACTION2 = 2, G50T_ACTION3 = 3, G50T_ACTION4 = 4, G50T_ACTION5 = 5 };
enum { G50T_WIN = 2, G50T_GAME_OVER = 3 };
enum { G50T_GATE_NONE = 0, G50T_GATE_DOOR = 1, G50T_GATE_PORTAL = 2 };
enum { G50T_DOOR_LOCK_SENTINEL = 11, G50T_ECHO_BODY_COLOR = 2, G50T_ECHO_CENTER_COLOR = 5 };

static inline int32_t g50t_fdiv2(int32_t a) {
    return a >= 0 ? a / 2 : -(((-a) + 1) / 2);
}

static inline int32_t g50t_pymod4(int32_t a) {
    int32_t m = a % 4;
    return m < 0 ? m + 4 : m;
}

static inline int32_t g50t_sign(int32_t a) {
    return (a > 0) - (a < 0);
}

static int g50t_pixel_hit(const ArcSprites *s, int32_t slot, int32_t x, int32_t y, int solid) {
    int32_t sx = s->x[slot], sy = s->y[slot], w = s->w[slot], h = s->h[slot];
    int inside = x >= sx && y >= sy && x < sx + w && y < sy + h;
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int32_t py = y - sy; py = py < 0 ? 0 : (py > ph - 1 ? ph - 1 : py);
    int32_t px = x - sx; px = px < 0 ? 0 : (px > pw - 1 ? pw - 1 : px);
    int8_t pixel = arc_sprite_pixels(s, slot)[(size_t)py * pw + px];
    int opaque = solid ? (pixel != -1) : (pixel >= 0);
    return inside && opaque;
}

static void g50t_start_death(ArcSprites *s, int32_t main_slot, const int32_t *death_slots) {
    int32_t piece0 = death_slots[0];
    int32_t offset_x = g50t_fdiv2(s->w[main_slot] - s->w[piece0]);
    int32_t offset_y = g50t_fdiv2(s->h[main_slot] - s->h[piece0]);
    arc_set_position(s, piece0, s->x[main_slot] + offset_x, s->y[main_slot] + offset_y);
    arc_set_interaction(s, piece0, TANGIBLE);
    arc_set_interaction(s, main_slot, REMOVED);
}

static int g50t_in_bounds_and_open(const ArcSprites *s, const G50tStatic *st, int32_t level,
                                   int32_t cx, int32_t cy) {
    if (!g50t_pixel_hit(s, st->bounds_slot[level], cx, cy, 1)) return 0;
    const int32_t *doors = st->door_slot + (size_t)level * G50T_MAX_DOORS;
    for (int32_t i = 0; i < G50T_MAX_DOORS; i++) {
        int32_t slot = doors[i];
        if (slot < 0 || !arc_sprite_visible(s, slot)) continue;
        if (g50t_pixel_hit(s, slot, cx, cy, 1)) return 0;
    }
    return 1;
}

static int32_t g50t_sensor_index_at(const ArcSprites *s, const G50tStatic *st, int32_t level,
                                    int32_t cx, int32_t cy) {
    const int32_t *sensors = st->sensor_slot + (size_t)level * G50T_MAX_SENSORS;
    for (int32_t i = 0; i < G50T_MAX_SENSORS; i++) {
        int32_t slot = sensors[i];
        if (slot < 0) continue;
        if (g50t_pixel_hit(s, slot, cx, cy, 1)) return i;
    }
    return -1;
}

static void g50t_set_door(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                          int32_t door_role, int value, int instant) {
    int32_t slot = st->door_slot[(size_t)level * G50T_MAX_DOORS + door_role];
    int32_t h = s->h[slot], w = s->w[slot];
    int8_t center_px = arc_sprite_pixels(s, slot)[(size_t)(h / 2) * s->atlas->pw + (w / 2)];
    int toggle = center_px == G50T_DOOR_LOCK_SENTINEL;
    int ignore = toggle && !value;
    if (toggle && value && aux->door_open[door_role]) value = 0;
    int changed = !ignore && ((aux->door_open[door_role] != 0) != (value != 0));
    if (!changed) return;

    int32_t dx = st->door_dir_x[(size_t)level * G50T_MAX_DOORS + door_role];
    int32_t dy = st->door_dir_y[(size_t)level * G50T_MAX_DOORS + door_role];
    int32_t sign = value ? 1 : -1;
    int32_t tx = s->x[slot] + dx * sign * G50T_GRID;
    int32_t ty = s->y[slot] + dy * sign * G50T_GRID;
    int32_t delay = instant ? (!value ? 3 : 0) : 0;

    aux->door_open[door_role] = (uint8_t)value;
    aux->door_active[door_role] = 1;
    aux->door_target_x[door_role] = tx;
    aux->door_target_y[door_role] = ty;
    aux->door_delay[door_role] = delay;
    aux->door_instant[door_role] = (uint8_t)instant;
}

static void g50t_teleport(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                          int32_t portal_role) {
    const int32_t *eps = st->portal_endpoints + ((size_t)level * G50T_MAX_PORTALS + portal_role) * 2;
    int32_t e0 = eps[0], e1 = eps[1];
    int valid = e0 >= 0 && e1 >= 0;
    int32_t e0c = valid ? e0 : 0;
    int32_t e1c = valid ? e1 : 0;
    uint8_t m0[G50T_MAX_TRACKED], m1[G50T_MAX_TRACKED];
    memcpy(m0, aux->endpoint_members[e0c], sizeof m0);
    memcpy(m1, aux->endpoint_members[e1c], sizeof m1);
    if (valid) {
        memcpy(aux->endpoint_members[e0c], m1, sizeof m1);
        memcpy(aux->endpoint_members[e1c], m0, sizeof m0);
    }

    int32_t e0_slot = st->endpoint_slot[(size_t)level * G50T_MAX_ENDPOINTS + e0c];
    int32_t e1_slot = st->endpoint_slot[(size_t)level * G50T_MAX_ENDPOINTS + e1c];
    if (e0_slot < 0) e0_slot = 0;
    if (e1_slot < 0) e1_slot = 0;
    int32_t e0x = s->x[e0_slot], e0y = s->y[e0_slot];
    int32_t e1x = s->x[e1_slot], e1y = s->y[e1_slot];

    if (valid) {
        for (int32_t g = 0; g < G50T_PLAYER_GENERATIONS; g++) {
            int32_t slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + g];
            int to0 = m1[g], to1 = m0[g];
            int32_t nx = to0 ? e0x : (to1 ? e1x : s->x[slot]);
            int32_t ny = to0 ? e0y : (to1 ? e1y : s->y[slot]);
            arc_set_position(s, slot, nx, ny);
        }
    }

    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++) {
        int32_t g = G50T_PLAYER_GENERATIONS + ii;
        int32_t slot = st->ice_slot[(size_t)level * G50T_MAX_ICE + ii];
        int32_t s_ = slot >= 0 ? slot : 0;
        int to0 = m1[g], to1 = m0[g];
        int32_t nx = to0 ? e0x : (to1 ? e1x : s->x[s_]);
        int32_t ny = to0 ? e0y : (to1 ? e1y : s->y[s_]);
        arc_set_position(s, s_, nx, ny);
    }
}

static void g50t_set_portal(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                            int32_t portal_role, int value, int instant) {
    if (instant) {
        if (!value) g50t_teleport(s, st, level, aux, portal_role);
        return;
    }
    if (!value) return;
    if (aux->portal_animating[portal_role]) return;
    aux->portal_animating[portal_role] = 1;
    aux->portal_progress[portal_role] = 0;
    aux->portal_delay[portal_role] = 4;
    aux->portal_active[portal_role] = 1;
}

static void g50t_forward_button(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                                int32_t gate_role, int value) {
    if (gate_role < 0) return;
    int changed = (aux->gate_on[gate_role] != 0) != (value != 0);
    if (!changed) return;
    int instant = aux->undoing;
    int32_t delay = instant ? (value ? 0 : 2) : 3;
    aux->gate_on[gate_role] = (uint8_t)value;
    aux->gate_active[gate_role] = 1;
    aux->gate_delay[gate_role] = delay;

    int32_t kind = st->gate_kind[(size_t)level * G50T_MAX_GATES + gate_role];
    int32_t target = st->gate_target[(size_t)level * G50T_MAX_GATES + gate_role];
    if (kind == G50T_GATE_DOOR) g50t_set_door(s, st, level, aux, target, value, instant);
    else if (kind == G50T_GATE_PORTAL) g50t_set_portal(s, st, level, aux, target, value, instant);
}

static void g50t_update_sensor(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                               int32_t tracked_index, int32_t cx, int32_t cy, int value) {
    int32_t idx = g50t_sensor_index_at(s, st, level, cx, cy);
    if (idx < 0) return;
    if (idx < G50T_MAX_BUTTONS) {
        int32_t role = idx;
        uint8_t *members = aux->button_members[role];
        int was_on = 0;
        for (int32_t t = 0; t < G50T_MAX_TRACKED; t++) was_on |= members[t];
        members[tracked_index] = (uint8_t)value;
        int now_on = 0;
        for (int32_t t = 0; t < G50T_MAX_TRACKED; t++) now_on |= members[t];
        if ((was_on != 0) != (now_on != 0)) {
            int32_t gate_role = st->button_gate[(size_t)level * G50T_MAX_BUTTONS + role];
            g50t_forward_button(s, st, level, aux, gate_role, now_on);
        }
    } else {
        int32_t role = idx - G50T_MAX_BUTTONS;
        aux->endpoint_members[role][tracked_index] = (uint8_t)value;
    }
}

static void g50t_register(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                          int32_t tracked_index, int32_t cx, int32_t cy) {
    g50t_update_sensor(s, st, level, aux, tracked_index, cx, cy, 1);
}

static void g50t_deregister(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                            int32_t tracked_index, int32_t cx, int32_t cy) {
    g50t_update_sensor(s, st, level, aux, tracked_index, cx, cy, 0);
}

static int g50t_try_animated_move(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                                  int32_t slot, int32_t dx, int32_t dy, int32_t tracked_index) {
    int32_t x = s->x[slot], y = s->y[slot], w = s->w[slot], h = s->h[slot];
    int32_t nx = x + dx * G50T_GRID, ny = y + dy * G50T_GRID;
    int32_t ncx = nx + w / 2, ncy = ny + h / 2;
    if (!g50t_in_bounds_and_open(s, st, level, ncx, ncy)) return 0;
    g50t_deregister(s, st, level, aux, tracked_index, x + w / 2, y + h / 2);
    return 1;
}

static void g50t_queue_gen_move(G50tAux *aux, int32_t gen, int32_t tx, int32_t ty, int32_t delay,
                                int instant) {
    aux->target_x[gen] = tx;
    aux->target_y[gen] = ty;
    aux->delay[gen] = delay;
    if (instant) aux->instant[gen] = 1;
    aux->active[gen] = 1;
}

static void g50t_queue_ice_move(G50tAux *aux, int32_t ii, int32_t tx, int32_t ty, int32_t delay,
                                int instant) {
    aux->ice_target_x[ii] = tx;
    aux->ice_target_y[ii] = ty;
    aux->ice_delay[ii] = delay;
    if (instant) aux->ice_instant[ii] = 1;
    aux->ice_active[ii] = 1;
}

static int g50t_move_gen(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                         int32_t gen, int32_t dx, int32_t dy) {
    int32_t slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + gen];
    if (!g50t_try_animated_move(s, st, level, aux, slot, dx, dy, gen)) return 0;
    int32_t nx = s->x[slot] + dx * G50T_GRID;
    int32_t ny = s->y[slot] + dy * G50T_GRID;
    g50t_queue_gen_move(aux, gen, nx, ny, 0, 0);
    return 1;
}

static int g50t_move_ice(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                         int32_t ii, int32_t dx, int32_t dy) {
    int32_t slot = st->ice_slot[(size_t)level * G50T_MAX_ICE + ii];
    if (slot < 0) return 0;
    int32_t tracked = G50T_PLAYER_GENERATIONS + ii;
    if (!g50t_try_animated_move(s, st, level, aux, slot, dx, dy, tracked)) return 0;
    int32_t nx = s->x[slot] + dx * G50T_GRID;
    int32_t ny = s->y[slot] + dy * G50T_GRID;
    g50t_queue_ice_move(aux, ii, nx, ny, 0, 0);
    return 1;
}

static void g50t_ice_next_dir(int32_t direction, int32_t *dx, int32_t *dy) {
    static const int32_t deltas[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    int32_t d = g50t_pymod4(direction);
    *dx = deltas[d][0];
    *dy = deltas[d][1];
}

static int g50t_ice_in_boundary(const ArcSprites *s, const G50tStatic *st, int32_t level,
                                int32_t ii, int32_t x, int32_t y) {
    int32_t bound_slot = st->ice_bound_slot[(size_t)level * G50T_MAX_ICE + ii];
    if (bound_slot < 0) return 0;
    return g50t_pixel_hit(s, bound_slot, x, y, 0);
}

static void g50t_ice_slide_and_record(ArcSprites *s, const G50tStatic *st, int32_t level,
                                      G50tAux *aux, int32_t ii) {
    int32_t direction0 = aux->ice_dir[ii];
    int32_t candidates[4] = {
        direction0,
        g50t_pymod4(direction0 - 1),
        g50t_pymod4(direction0 + 1),
        g50t_pymod4(direction0 + 2),
    };
    int32_t slot = st->ice_slot[(size_t)level * G50T_MAX_ICE + ii];
    int moved = 0;
    int32_t final_dir = direction0, mdx = 0, mdy = 0;
    for (int32_t k = 0; k < 4 && !moved; k++) {
        int32_t d = candidates[k];
        int32_t dx, dy;
        g50t_ice_next_dir(d, &dx, &dy);
        int32_t x = s->x[slot] + dx * G50T_GRID;
        int32_t y = s->y[slot] + dy * G50T_GRID;
        if (!g50t_ice_in_boundary(s, st, level, ii, x, y)) continue;
        if (!g50t_move_ice(s, st, level, aux, ii, dx, dy)) continue;
        moved = 1;
        final_dir = d;
        mdx = dx;
        mdy = dy;
    }
    aux->ice_dir[ii] = final_dir;
    if (moved) {
        int32_t rec = aux->ice_move_count[ii];
        aux->ice_history[ii][rec][0] = (int8_t)mdx;
        aux->ice_history[ii][rec][1] = (int8_t)mdy;
        aux->ice_move_count[ii] = rec + 1;
    }
}

static void g50t_do_move(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                         int32_t dx, int32_t dy) {
    int32_t gen = aux->gen;
    if (!g50t_move_gen(s, st, level, aux, gen, dx, dy)) return;

    int32_t step_idx = aux->move_count;
    aux->history[gen][step_idx][0] = (int8_t)dx;
    aux->history[gen][step_idx][1] = (int8_t)dy;

    for (int32_t g = 0; g < G50T_PLAYER_GENERATIONS; g++) {
        if (!aux->echo[g]) continue;
        if (step_idx >= aux->history_len[g]) continue;
        int32_t gdx = aux->history[g][step_idx][0];
        int32_t gdy = aux->history[g][step_idx][1];
        int32_t rank = 0;
        for (int32_t j = 0; j < g; j++) rank += aux->echo[j] ? 1 : 0;
        if (g50t_move_gen(s, st, level, aux, g, gdx, gdy)) aux->delay[g] = rank + 2;
    }

    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++) {
        if (st->ice_slot[(size_t)level * G50T_MAX_ICE + ii] < 0) continue;
        if (aux->ice_dead[ii]) continue;
        g50t_ice_slide_and_record(s, st, level, aux, ii);
    }

    aux->move_count = step_idx + 1;
}

static void g50t_pop_move(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux) {
    int32_t step_idx = aux->move_count - 1;
    int32_t gen = aux->gen;
    int32_t dx = aux->history[gen][step_idx][0];
    int32_t dy = aux->history[gen][step_idx][1];

    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++) {
        int32_t ice_slot = st->ice_slot[(size_t)level * G50T_MAX_ICE + ii];
        if (ice_slot < 0) continue;
        int32_t rec = aux->ice_move_count[ii];
        if (step_idx >= rec) continue;
        arc_set_interaction(s, ice_slot, TANGIBLE);
        int32_t r = rec - 1;
        int32_t gdx = -aux->ice_history[ii][r][0];
        int32_t gdy = -aux->ice_history[ii][r][1];
        int ok = g50t_move_ice(s, st, level, aux, ii, gdx, gdy);
        aux->ice_move_count[ii] = r;
        aux->ice_instant[ii] = 1;
        if (ok) aux->ice_delay[ii] = 3;
        aux->ice_dead[ii] = 0;
    }

    for (int32_t g = 0; g < G50T_PLAYER_GENERATIONS; g++) {
        if (!aux->echo[g]) continue;
        if (step_idx >= aux->history_len[g]) continue;
        if (g == gen) continue;
        int32_t gdx = -aux->history[g][step_idx][0];
        int32_t gdy = -aux->history[g][step_idx][1];
        if (g50t_move_gen(s, st, level, aux, g, gdx, gdy)) {
            aux->instant[g] = 1;
            aux->delay[g] = 3;
        }
    }

    int ok = g50t_move_gen(s, st, level, aux, gen, -dx, -dy);
    aux->instant[gen] = 1;
    if (ok) aux->delay[gen] = 3;
    aux->move_count = step_idx;
}

static void g50t_do_undo(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux) {
    if (aux->move_count <= 0) return;
    aux->commit_pending = 1;
    aux->pending_history_len = aux->move_count;
    g50t_pop_move(s, st, level, aux);
    aux->undoing = aux->move_count > 0;
}

static int g50t_any_active(const G50tAux *aux) {
    for (int32_t g = 0; g < G50T_PLAYER_GENERATIONS; g++) if (aux->active[g]) return 1;
    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++) if (aux->ice_active[ii]) return 1;
    for (int32_t i = 0; i < G50T_MAX_DOORS; i++) if (aux->door_active[i]) return 1;
    for (int32_t i = 0; i < G50T_MAX_GATES; i++) if (aux->gate_active[i]) return 1;
    for (int32_t i = 0; i < G50T_MAX_PORTALS; i++) if (aux->portal_active[i]) return 1;
    return aux->ghost_active != 0;
}

static void g50t_tick_mover(int32_t x, int32_t y, int32_t target_x, int32_t target_y,
                            int32_t delay, int instant, int32_t *out_delay, int32_t *out_x,
                            int32_t *out_y, int *out_instant, int *out_busy) {
    if (delay > 0) {
        *out_delay = delay - 1;
        *out_x = x;
        *out_y = y;
        *out_instant = instant;
        *out_busy = 1;
        return;
    }
    if (instant) {
        *out_delay = delay;
        *out_x = target_x;
        *out_y = target_y;
        *out_instant = 0;
        *out_busy = 0;
        return;
    }
    int32_t dx = g50t_sign(target_x - x);
    int32_t dy = g50t_sign(target_y - y);
    int32_t nx = x + dx, ny = y + dy;
    *out_delay = delay;
    *out_x = nx;
    *out_y = ny;
    *out_instant = instant;
    *out_busy = (target_x != nx) || (target_y != ny);
}

static void g50t_tick_gen(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                          int32_t g) {
    if (!aux->active[g]) return;
    int32_t slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + g];
    if (aux->dying[g]) {
        int32_t step = aux->death_step[g];
        int32_t cur = st->player_death_slot[step];
        arc_set_interaction(s, cur, REMOVED);
        int32_t next_step = step + 1;
        int finished = next_step == G50T_DEATH_PIECES;
        if (!finished) {
            int32_t clipped = next_step > G50T_DEATH_PIECES - 1 ? G50T_DEATH_PIECES - 1 : next_step;
            int32_t nxt = st->player_death_slot[clipped];
            int32_t x = aux->target_x[g] + g50t_fdiv2(s->w[slot] - s->w[cur]);
            int32_t y = aux->target_y[g] + g50t_fdiv2(s->h[slot] - s->h[cur]);
            arc_set_position(s, nxt, x, y);
            arc_set_interaction(s, nxt, TANGIBLE);
        }
        aux->dying[g] = (uint8_t)!finished;
        if (finished) aux->dead[g] = 1;
        aux->death_step[g] = next_step;
        return;
    }
    int32_t new_delay, new_x, new_y;
    int new_instant, busy;
    g50t_tick_mover(s->x[slot], s->y[slot], aux->target_x[g], aux->target_y[g], aux->delay[g],
                    aux->instant[g], &new_delay, &new_x, &new_y, &new_instant, &busy);
    arc_set_position(s, slot, new_x, new_y);
    aux->delay[g] = new_delay;
    aux->instant[g] = (uint8_t)new_instant;
    aux->active[g] = (uint8_t)busy;
}

static void g50t_tick_ice(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                          int32_t ii) {
    if (!aux->ice_active[ii]) return;
    int32_t slot = st->ice_slot[(size_t)level * G50T_MAX_ICE + ii];
    if (slot < 0) slot = 0;
    if (aux->ice_dying[ii]) {
        int32_t step = aux->ice_death_step[ii];
        const int32_t *death_slot = st->ice_death_slot + (size_t)ii * G50T_DEATH_PIECES;
        int32_t cur = death_slot[step];
        arc_set_interaction(s, cur, REMOVED);
        int32_t next_step = step + 1;
        int finished = next_step == G50T_DEATH_PIECES;
        if (!finished) {
            int32_t clipped = next_step > G50T_DEATH_PIECES - 1 ? G50T_DEATH_PIECES - 1 : next_step;
            int32_t nxt = death_slot[clipped];
            int32_t x = aux->ice_target_x[ii] + g50t_fdiv2(s->w[slot] - s->w[cur]);
            int32_t y = aux->ice_target_y[ii] + g50t_fdiv2(s->h[slot] - s->h[cur]);
            arc_set_position(s, nxt, x, y);
            arc_set_interaction(s, nxt, TANGIBLE);
        }
        aux->ice_dying[ii] = (uint8_t)!finished;
        if (finished) aux->ice_dead[ii] = 1;
        aux->ice_death_step[ii] = next_step;
        return;
    }
    int32_t new_delay, new_x, new_y;
    int new_instant, busy;
    g50t_tick_mover(s->x[slot], s->y[slot], aux->ice_target_x[ii], aux->ice_target_y[ii],
                    aux->ice_delay[ii], aux->ice_instant[ii], &new_delay, &new_x, &new_y,
                    &new_instant, &busy);
    int was_active = aux->ice_active[ii];
    arc_set_position(s, slot, new_x, new_y);
    aux->ice_delay[ii] = new_delay;
    aux->ice_instant[ii] = (uint8_t)new_instant;
    aux->ice_active[ii] = (uint8_t)busy;

    if (!(was_active && !busy)) return;
    int32_t gen = aux->gen;
    int32_t pslot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + gen];
    int32_t icx = s->x[slot] + s->w[slot] / 2, icy = s->y[slot] + s->h[slot] / 2;
    int32_t ax = s->x[slot], ay = s->y[slot], aw = s->w[slot], ah = s->h[slot];
    int32_t bx = s->x[pslot], by = s->y[pslot], bw = s->w[pslot], bh = s->h[pslot];
    int bbox = ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
    int crush = bbox && g50t_pixel_hit(s, pslot, icx, icy, 0) && !aux->dead[gen] && !aux->dying[gen];
    if (crush) {
        g50t_start_death(s, pslot, st->player_death_slot);
        aux->dying[gen] = 1;
        aux->death_step[gen] = 0;
        aux->active[gen] = 1;
    }
}

static void g50t_tick_door(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                           int32_t i) {
    int32_t slot = st->door_slot[(size_t)level * G50T_MAX_DOORS + i];
    int valid = slot >= 0;
    int32_t s_ = valid ? slot : 0;
    int active = aux->door_active[i] && valid;
    int32_t new_delay, new_x, new_y;
    int new_instant, busy;
    g50t_tick_mover(s->x[s_], s->y[s_], aux->door_target_x[i], aux->door_target_y[i],
                    aux->door_delay[i], aux->door_instant[i], &new_delay, &new_x, &new_y,
                    &new_instant, &busy);
    if (active) arc_set_position(s, s_, new_x, new_y);

    int just_settled = active && !busy;
    int32_t gen = aux->gen;
    int32_t pslot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + gen];
    int32_t px = s->x[pslot] + s->w[pslot] / 2, py = s->y[pslot] + s->h[pslot] / 2;
    int32_t ax = s->x[s_], ay = s->y[s_], aw = s->w[s_], ah = s->h[s_];
    int32_t bx = s->x[pslot], by = s->y[pslot], bw = s->w[pslot], bh = s->h[pslot];
    int bbox = ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
    int closing = !aux->door_open[i];
    int crush = just_settled && closing && bbox && g50t_pixel_hit(s, s_, px, py, 1) &&
               !aux->dead[gen] && !aux->dying[gen];
    if (crush) g50t_start_death(s, pslot, st->player_death_slot);

    if (active) {
        aux->door_delay[i] = new_delay;
        aux->door_instant[i] = (uint8_t)new_instant;
        aux->door_active[i] = (uint8_t)busy;
    }
    if (crush) {
        aux->dying[gen] = 1;
        aux->death_step[gen] = 0;
        aux->active[gen] = 1;
    }

    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++) {
        int32_t ice_slot = st->ice_slot[(size_t)level * G50T_MAX_ICE + ii];
        if (ice_slot < 0) continue;
        int collide = arc_collides_pair(s, ice_slot, s_, 0);
        int32_t icx = s->x[ice_slot] + s->w[ice_slot] / 2, icy = s->y[ice_slot] + s->h[ice_slot] / 2;
        int icrush = just_settled && closing && collide && g50t_pixel_hit(s, s_, icx, icy, 1) &&
                    !aux->ice_dead[ii] && !aux->ice_dying[ii];
        if (icrush) {
            g50t_start_death(s, ice_slot, st->ice_death_slot + (size_t)ii * G50T_DEATH_PIECES);
            aux->ice_dying[ii] = 1;
            aux->ice_death_step[ii] = 0;
            aux->ice_active[ii] = 1;
        }
    }
}

static void g50t_tick_portal(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                             int32_t i) {
    if (!aux->portal_active[i]) return;
    if (aux->portal_delay[i] > 0) {
        aux->portal_delay[i] -= 1;
        return;
    }
    int32_t progress = aux->portal_progress[i];
    const int32_t *slots = st->portal_crossfade_slot + (size_t)i * G50T_CROSSFADE_PIECES;
    if (progress > 0) {
        int32_t clipped = progress - 1;
        clipped = clipped > G50T_CROSSFADE_PIECES - 1 ? G50T_CROSSFADE_PIECES - 1 : clipped;
        arc_set_interaction(s, slots[clipped], REMOVED);
    }
    int more = progress < G50T_CROSSFADE_PIECES;
    if (more) {
        int32_t clipped = progress > G50T_CROSSFADE_PIECES - 1 ? G50T_CROSSFADE_PIECES - 1 : progress;
        arc_set_interaction(s, slots[clipped], TANGIBLE);
    }
    aux->portal_progress[i] = progress + 1;
    if (!more) {
        g50t_teleport(s, st, level, aux, i);
        aux->portal_animating[i] = 0;
        aux->portal_active[i] = 0;
    }
}

static void g50t_tick_ghost(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux) {
    if (!aux->ghost_active) return;
    int32_t slot = st->ghost_slot[level];
    int32_t x = s->x[slot], y = s->y[slot];
    int32_t dx = g50t_sign(aux->ghost_target_x - x);
    int32_t dy = g50t_sign(aux->ghost_target_y - y);
    int32_t nx = x + dx, ny = y + dy;
    arc_set_position(s, slot, nx, ny);
    aux->ghost_active = (uint8_t)((aux->ghost_target_x != nx) || (aux->ghost_target_y != ny));
}

static void g50t_tick_all(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux) {
    for (int32_t g = 0; g < G50T_PLAYER_GENERATIONS; g++) g50t_tick_gen(s, st, level, aux, g);
    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++) g50t_tick_ice(s, st, level, aux, ii);
    for (int32_t i = 0; i < G50T_MAX_DOORS; i++) g50t_tick_door(s, st, level, aux, i);
    for (int32_t i = 0; i < G50T_MAX_GATES; i++) {
        if (!aux->gate_active[i]) continue;
        int32_t nd = aux->gate_delay[i] - 1;
        int still = nd > 0;
        aux->gate_delay[i] = nd < 0 ? 0 : nd;
        aux->gate_active[i] = (uint8_t)still;
    }
    for (int32_t i = 0; i < G50T_MAX_PORTALS; i++) g50t_tick_portal(s, st, level, aux, i);
    g50t_tick_ghost(s, st, level, aux);
}

static void g50t_register_presence(ArcSprites *s, const G50tStatic *st, int32_t level,
                                   G50tAux *aux) {
    int32_t gen = aux->gen;
    int32_t pslot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + gen];
    g50t_register(s, st, level, aux, gen, s->x[pslot] + s->w[pslot] / 2,
                 s->y[pslot] + s->h[pslot] / 2);

    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++) {
        int32_t slot = st->ice_slot[(size_t)level * G50T_MAX_ICE + ii];
        if (slot < 0) continue;
        g50t_register(s, st, level, aux, G50T_PLAYER_GENERATIONS + ii, s->x[slot] + s->w[slot] / 2,
                     s->y[slot] + s->h[slot] / 2);
    }

    for (int32_t g = 0; g < G50T_PLAYER_GENERATIONS; g++) {
        if (!aux->echo[g]) continue;
        int32_t slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + g];
        g50t_register(s, st, level, aux, g, s->x[slot] + s->w[slot] / 2, s->y[slot] + s->h[slot] / 2);
    }
}

static void g50t_checkpoint_swap(ArcSprites *s, const G50tStatic *st, G50tAux *aux) {
    int32_t idx = aux->checkpoint_idx;
    if (idx == 0) {
        for (int32_t k = 0; k < G50T_MAX_CHECKPOINTS; k++)
            arc_set_interaction(s, st->checkpoint_on_slot[k], REMOVED);
        for (int32_t k = 0; k < G50T_MAX_CHECKPOINTS; k++)
            arc_set_interaction(s, st->checkpoint_off_slot[k], REMOVED);
    } else {
        int32_t prev = idx - 1;
        prev = prev < 0 ? 0 : (prev > G50T_MAX_CHECKPOINTS - 1 ? G50T_MAX_CHECKPOINTS - 1 : prev);
        arc_set_interaction(s, st->checkpoint_on_slot[prev], REMOVED);
        arc_set_interaction(s, st->checkpoint_off_slot[prev], TANGIBLE);
    }
    int32_t ci = idx < 0 ? 0 : (idx > G50T_MAX_CHECKPOINTS - 1 ? G50T_MAX_CHECKPOINTS - 1 : idx);
    arc_set_interaction(s, st->checkpoint_on_slot[ci], TANGIBLE);
    aux->swap_pending = 0;
}

static void g50t_commit_checkpoint(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                                   int32_t *next_order) {
    int32_t gen = aux->gen;
    int32_t old_slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + gen];
    int32_t cp_idx = aux->checkpoint_idx + 1;
    int32_t count = st->checkpoint_count[level];
    int is_last = cp_idx == count;
    int32_t spawn_x = st->spawn_x[level], spawn_y = st->spawn_y[level];

    arc_set_position(s, old_slot, spawn_x, spawn_y);

    if (is_last) {
        for (int32_t g = 0; g < G50T_PLAYER_GENERATIONS; g++) {
            if (!aux->echo[g]) continue;
            int32_t slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + g];
            arc_set_interaction(s, slot, REMOVED);
        }
        memset(aux->echo, 0, sizeof aux->echo);
    } else {
        int32_t new_gen = (gen + 1) % G50T_PLAYER_GENERATIONS;
        int32_t new_slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + new_gen];

        int32_t h = s->h[old_slot], w = s->w[old_slot];
        int32_t area = s->atlas->ph * s->atlas->pw;
        int8_t *old_px = arc_sprite_pixels_mut(s, old_slot);
        for (int32_t k = 0; k < area; k++)
            if (old_px[k] >= 0) old_px[k] = G50T_ECHO_BODY_COLOR;
        old_px[(size_t)(h / 2) * s->atlas->pw + (w / 2)] = G50T_ECHO_CENTER_COLOR;

        int8_t *new_px = arc_sprite_pixels_mut(s, new_slot);
        const int8_t *fresh = st->player_pixels + (size_t)level * area;
        memcpy(new_px, fresh, (size_t)area);

        arc_set_position(s, new_slot, spawn_x, spawn_y);
        arc_add_sprite(s, new_slot, *next_order);
        arc_set_interaction(s, new_slot, TANGIBLE);
        *next_order += 1;

        aux->echo[gen] = 1;
        aux->history_len[gen] = aux->pending_history_len;
        aux->gen = new_gen;
        aux->move_count = 0;
        aux->target_x[new_gen] = spawn_x;
        aux->target_y[new_gen] = spawn_y;
    }

    int32_t cp_idx_final = is_last ? 0 : cp_idx;
    aux->checkpoint_idx = cp_idx_final;
    aux->commit_pending = 0;

    int32_t target_slot = st->checkpoint_slot[(size_t)level * G50T_MAX_CHECKPOINTS + cp_idx_final];
    aux->ghost_active = 1;
    aux->ghost_target_x = s->x[target_slot];
    aux->ghost_target_y = s->y[target_slot];
    aux->swap_pending = 1;
}

static void g50t_advance_checkpoint_or_undo(ArcSprites *s, const G50tStatic *st, int32_t level,
                                            G50tAux *aux, int32_t *next_order) {
    if (aux->undoing) {
        g50t_pop_move(s, st, level, aux);
        aux->undoing = aux->move_count > 0;
    } else {
        g50t_commit_checkpoint(s, st, level, aux, next_order);
    }
}

static void g50t_do_resolve(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                            int32_t *next_order) {
    if (aux->swap_pending) {
        g50t_checkpoint_swap(s, st, aux);
        return;
    }
    g50t_register_presence(s, st, level, aux);
    if (g50t_any_active(aux)) return;
    if (aux->commit_pending) g50t_advance_checkpoint_or_undo(s, st, level, aux, next_order);
}

static int g50t_is_lost(const ArcSprites *s, const G50tStatic *st, int32_t level,
                        const G50tAux *aux) {
    int32_t timer_slot = st->timer_slot[level];
    int32_t bar_x = s->x[timer_slot], bar_w = s->w[timer_slot];
    int timer_out = -bar_x > bar_w;
    return timer_out || aux->dead[aux->gen];
}

static int g50t_is_won(const ArcSprites *s, const G50tStatic *st, int32_t level,
                       const G50tAux *aux) {
    int32_t goal_slot = st->goal_slot[level];
    int32_t player_slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + aux->gen];
    return (s->x[goal_slot] + 1 == s->x[player_slot]) && (s->y[goal_slot] + 1 == s->y[player_slot]);
}

static void g50t_complete(G50tAux *aux, uint8_t *action_complete) {
    aux->dispatched = 0;
    *action_complete = 1;
}

static void g50t_next_level(const G50tStatic *st, int32_t level, int32_t *score, int32_t *status,
                            uint8_t *next_level) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = (uint8_t)!is_last;
    if (is_last) *status = G50T_WIN;
}

static void g50t_maybe_finish(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                              int32_t *score, int32_t *status, uint8_t *next_level,
                              uint8_t *action_complete) {
    if (g50t_any_active(aux)) return;
    if (g50t_is_lost(s, st, level, aux)) {
        aux->lost_pending = 1;
        return;
    }
    if (g50t_is_won(s, st, level, aux)) {
        aux->won_pending = 1;
        return;
    }
    g50t_complete(aux, action_complete);
    (void)score;
    (void)status;
    (void)next_level;
}

static void g50t_finish_win(const G50tStatic *st, int32_t level, G50tAux *aux, int32_t *score,
                            int32_t *status, uint8_t *next_level, uint8_t *action_complete) {
    aux->won_pending = 0;
    g50t_next_level(st, level, score, status, next_level);
    g50t_complete(aux, action_complete);
}

static void g50t_finish_lose(G50tAux *aux, int32_t *status, uint8_t *action_complete) {
    aux->lost_pending = 0;
    *status = G50T_GAME_OVER;
    g50t_complete(aux, action_complete);
}

static void g50t_dispatch(ArcSprites *s, const G50tStatic *st, int32_t level, int32_t action_id,
                          G50tAux *aux, int32_t *score, int32_t *status, uint8_t *next_level,
                          uint8_t *action_complete) {
    int32_t dx = 0, dy = 0;
    switch (action_id) {
        case G50T_ACTION1: dy = -1; break;
        case G50T_ACTION2: dy = 1; break;
        case G50T_ACTION3: dx = -1; break;
        case G50T_ACTION4: dx = 1; break;
        default: break;
    }
    if (action_id >= G50T_ACTION1 && action_id <= G50T_ACTION4)
        g50t_do_move(s, st, level, aux, dx, dy);
    if (action_id == G50T_ACTION5) g50t_do_undo(s, st, level, aux);

    if (action_id != 0) {
        int32_t count = aux->action_count + 1;
        int32_t timer_slot = st->timer_slot[level];
        if (count % 2 == 0) s->x[timer_slot] -= 1;
        aux->action_count = count;
    }
    aux->dispatched = 1;
    g50t_maybe_finish(s, st, level, aux, score, status, next_level, action_complete);
}

static void g50t_tick_then_resolve(ArcSprites *s, const G50tStatic *st, int32_t level, G50tAux *aux,
                                   int32_t *next_order, int32_t *score, int32_t *status,
                                   uint8_t *next_level, uint8_t *action_complete) {
    g50t_tick_all(s, st, level, aux);
    if (g50t_any_active(aux)) return;
    g50t_do_resolve(s, st, level, aux, next_order);
    g50t_maybe_finish(s, st, level, aux, score, status, next_level, action_complete);
}

void g50t_zero_aux(G50tAux *aux) {
    memset(aux, 0, sizeof(*aux));
}

void g50t_on_set_level(ArcSprites *sprites, const G50tStatic *st, int32_t level, G50tAux *aux) {
    g50t_zero_aux(aux);
    int32_t player_slot = st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + 0];
    int32_t spawn_x = sprites->x[player_slot], spawn_y = sprites->y[player_slot];

    for (int32_t g = 1; g < G50T_PLAYER_GENERATIONS; g++)
        arc_set_interaction(sprites, st->gen_slot[(size_t)level * G50T_PLAYER_GENERATIONS + g], REMOVED);
    for (int32_t k = 0; k < G50T_DEATH_PIECES; k++)
        arc_set_interaction(sprites, st->player_death_slot[k], REMOVED);
    for (int32_t ii = 0; ii < G50T_MAX_ICE; ii++)
        for (int32_t k = 0; k < G50T_DEATH_PIECES; k++)
            arc_set_interaction(sprites, st->ice_death_slot[(size_t)ii * G50T_DEATH_PIECES + k], REMOVED);
    for (int32_t pi = 0; pi < G50T_MAX_PORTALS; pi++)
        for (int32_t k = 0; k < G50T_CROSSFADE_PIECES; k++)
            arc_set_interaction(sprites, st->portal_crossfade_slot[(size_t)pi * G50T_CROSSFADE_PIECES + k],
                            REMOVED);
    for (int32_t k = 0; k < G50T_MAX_CHECKPOINTS; k++)
        arc_set_interaction(sprites, st->checkpoint_on_slot[k], REMOVED);
    for (int32_t k = 0; k < G50T_MAX_CHECKPOINTS; k++)
        arc_set_interaction(sprites, st->checkpoint_off_slot[k], REMOVED);

    arc_set_interaction(sprites, player_slot, TANGIBLE);
    if (st->checkpoint_count[level] > 0) arc_set_interaction(sprites, st->checkpoint_on_slot[0], TANGIBLE);

    aux->target_x[0] = spawn_x;
    aux->target_y[0] = spawn_y;

    for (int32_t i = 0; i < G50T_MAX_DOORS; i++) {
        int32_t slot = st->door_slot[(size_t)level * G50T_MAX_DOORS + i];
        int32_t idx = slot >= 0 ? slot : st->num_slots - 1;
        aux->door_target_x[i] = sprites->x[idx];
        aux->door_target_y[i] = sprites->y[idx];
    }
}

void g50t_step_once(ArcSprites *sprites, const G50tStatic *st, int32_t level, int32_t action_id,
                    int32_t action_x, int32_t action_y, int32_t action_count, G50tAux *aux,
                    int32_t *next_order, int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    (void)action_x;
    (void)action_y;
    (void)action_count;
    if (aux->won_pending) {
        g50t_finish_win(st, level, aux, score, status, next_level, action_complete);
        return;
    }
    if (aux->lost_pending) {
        g50t_finish_lose(aux, status, action_complete);
        return;
    }
    if (aux->dispatched)
        g50t_tick_then_resolve(sprites, st, level, aux, next_order, score, status, next_level,
                               action_complete);
    else
        g50t_dispatch(sprites, st, level, action_id, aux, score, status, next_level, action_complete);
}

void g50t_render_interface(int8_t *frame, const ArcSprites *sprites, const ArcCamera *camera,
                           const G50tStatic *st, int32_t level, const G50tAux *aux) {
    (void)frame;
    (void)sprites;
    (void)camera;
    (void)st;
    (void)level;
    (void)aux;
}
