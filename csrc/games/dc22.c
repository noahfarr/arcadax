#include "dc22.h"

#include <math.h>
#include <string.h>

enum { DC22_ACTION_RESET = 0, DC22_ACTION1 = 1, DC22_ACTION2 = 2, DC22_ACTION3 = 3, DC22_ACTION4 = 4, DC22_ACTION6 = 6 };
enum { DC22_WIN = 2, DC22_GAME_OVER = 3 };
enum { DC22_STEP_MOVE = 2, DC22_CRANE_PITCH = 4 };
enum { DC22_COLOR_DEATH = 4, DC22_COLOR_HUD_FILLED = 0, DC22_COLOR_HUD_EMPTY = 3, DC22_COLOR_VIGNETTE = 5 };
enum { DC22_DEATH_FRAMES = 14, DC22_DEATH_PENALTY = 20, DC22_SHAKE_FRAMES = 2, DC22_FLICKER_FRAMES = 5 };
enum { DC22_VIGNETTE_START = 3, DC22_VIGNETTE_FULL = 16 };
enum { DC22_DIR_UP = 0, DC22_DIR_DOWN = 1, DC22_DIR_LEFT = 2, DC22_DIR_RIGHT = 3, DC22_DIR_GRAB = 4 };
enum { DC22_ATTACH_NONE = 0, DC22_ATTACH_BRIXTO = 1, DC22_ATTACH_OBJECT = 2 };

static const int32_t DC22_DIR_STEP_X[4] = {0, 0, -1, 1};
static const int32_t DC22_DIR_STEP_Y[4] = {-1, 1, 0, 0};

static inline int32_t dc22_clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t dc22_sort_key_descending(const Sprites *s, int32_t i) {
    return s->layer[i] * ORDER_BITS + (ORDER_BITS - 1 - s->order[i]);
}

static inline int dc22_contains(const Sprites *s, int32_t i, int32_t x, int32_t y) {
    return x >= s->x[i] && y >= s->y[i] && x < s->x[i] + s->w[i] && y < s->y[i] + s->h[i];
}

static inline int8_t dc22_pixel_value(const Sprites *s, int32_t i, int32_t x, int32_t y) {
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int32_t py = dc22_clamp(y - s->y[i], 0, ph - 1);
    int32_t px = dc22_clamp(x - s->x[i], 0, pw - 1);
    return sprite_pixels(s, i)[(size_t)py * pw + px];
}

static int dc22_pixel_overlap_ge0(const Sprites *s, int32_t i, int32_t j) {
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
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
            if (pi[(size_t)v * pw + u] >= 0 && pj[(size_t)vv * pw + uu] >= 0) return 1;
        }
    }
    return 0;
}

static int dc22_collides_ge0_pair(const Sprites *s, int32_t i, int32_t j, int ignore_mode) {
    if (j == i || !s->alive[j] || !s->alive[i]) return 0;
    if (!(s->x[i] < s->x[j] + s->w[j] && s->x[i] + s->w[i] > s->x[j] &&
          s->y[i] < s->y[j] + s->h[j] && s->y[i] + s->h[i] > s->y[j]))
        return 0;
    if (!ignore_mode) {
        if (!sprite_collidable(s, i) || !sprite_collidable(s, j)) return 0;
        if (s->blocking[i] == NOT_BLOCKED || s->blocking[j] == NOT_BLOCKED) return 0;
    }
    return dc22_pixel_overlap_ge0(s, i, j);
}

static int dc22_collides_ge0_any(const Sprites *s, int32_t i, int ignore_mode) {
    int32_t n = s->atlas->num_slots;
    for (int32_t j = 0; j < n; j++) {
        if (j == i) continue;
        if (dc22_collides_ge0_pair(s, i, j, ignore_mode)) return 1;
    }
    return 0;
}

static void dc22_display_to_grid(const Camera *camera, int32_t display_x, int32_t display_y,
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

static int32_t dc22_topmost_visible_opaque(const Sprites *s, const Dc22Static *st, int32_t level,
                                           int32_t x, int32_t y, const int32_t *cand, int32_t count) {
    const uint8_t *is_ignore = st->is_ignore + (size_t)level * st->num_slots;
    int32_t best = -1, best_key = 0;
    int found = 0;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = cand[k];
        if (!s->alive[i] || is_ignore[i]) continue;
        if (s->interaction[i] > INTANGIBLE) continue;
        if (!dc22_contains(s, i, x, y)) continue;
        if (dc22_pixel_value(s, i, x, y) < 0) continue;
        int32_t key = dc22_sort_key_descending(s, i);
        if (!found || key > best_key) {
            found = 1;
            best_key = key;
            best = i;
        }
    }
    return found ? best : -1;
}

static int dc22_on_valid_floor(const Sprites *s, const Dc22Static *st, int32_t level,
                               int32_t x, int32_t y, int32_t exclude) {
    const uint8_t *is_ignore = st->is_ignore + (size_t)level * st->num_slots;
    const uint8_t *is_crzsjq = st->is_crzsjq + (size_t)level * st->num_slots;
    const uint8_t *is_vcha = st->is_vcha + (size_t)level * st->num_slots;
    int32_t n = s->atlas->num_slots;
    for (int32_t i = 0; i < n; i++) {
        if (!s->alive[i] || i == exclude) continue;
        if (is_ignore[i] || is_crzsjq[i] || is_vcha[i]) continue;
        if (s->interaction[i] != INTANGIBLE) continue;
        if (!dc22_contains(s, i, x, y)) continue;
        if (dc22_pixel_value(s, i, x, y) < 0) continue;
        return 1;
    }
    return 0;
}

static int32_t dc22_find_piyqze_hit(const Sprites *s, const Dc22Static *st, int32_t level,
                                    int32_t x, int32_t y) {
    const uint8_t *is_ignore = st->is_ignore + (size_t)level * st->num_slots;
    const int32_t *cand = st->piyqze_slots + (size_t)level * st->max_piyqze;
    int32_t count = st->piyqze_count[level];
    int32_t best = -1, best_key = 0;
    int found = 0;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = cand[k];
        if (!s->alive[i] || is_ignore[i]) continue;
        if (s->interaction[i] == REMOVED) continue;
        int marked = 0;
        for (int off = 0; off < 4 && !marked; off++) {
            int32_t ddx = off & 1, ddy = (off >> 1) & 1;
            int32_t px = x + ddx, py = y + ddy;
            if (!dc22_contains(s, i, px, py)) continue;
            if (dc22_pixel_value(s, i, px, py) == -2) marked = 1;
        }
        if (!marked) continue;
        int32_t key = dc22_sort_key_descending(s, i);
        if (!found || key > best_key) {
            found = 1;
            best_key = key;
            best = i;
        }
    }
    return found ? best : -1;
}

static void dc22_save_undo(Sprites *s, Dc22Aux *aux) {
    int32_t n = s->atlas->num_slots;
    int32_t area = s->atlas->ph * s->atlas->pw;
    for (int32_t i = 0; i < n; i++)
        memcpy(aux->undo_pixels + (size_t)i * area, sprite_pixels(s, i), (size_t)area);
    memcpy(aux->undo_x, s->x, sizeof(int32_t) * (size_t)n);
    memcpy(aux->undo_y, s->y, sizeof(int32_t) * (size_t)n);
    memcpy(aux->undo_interaction, s->interaction, (size_t)n);
    memcpy(aux->undo_alive, s->alive, (size_t)n);
    aux->undo_valid = 1;
    aux->undo_crane_col = aux->crane_col;
    aux->undo_crane_row = aux->crane_row;
    aux->undo_attach_kind = aux->attach_kind;
    aux->undo_attach_slot = aux->attach_slot;
}

static void dc22_restore_undo(Sprites *s, Dc22Aux *aux) {
    int32_t n = s->atlas->num_slots;
    int32_t area = s->atlas->ph * s->atlas->pw;
    for (int32_t i = 0; i < n; i++) {
        int8_t *dst = sprite_pixels_mut(s, i);
        memcpy(dst, aux->undo_pixels + (size_t)i * area, (size_t)area);
    }
    memcpy(s->x, aux->undo_x, sizeof(int32_t) * (size_t)n);
    memcpy(s->y, aux->undo_y, sizeof(int32_t) * (size_t)n);
    memcpy(s->interaction, aux->undo_interaction, (size_t)n);
    memcpy(s->alive, aux->undo_alive, (size_t)n);
    aux->crane_col = aux->undo_crane_col;
    aux->crane_row = aux->undo_crane_row;
    aux->attach_kind = aux->undo_attach_kind;
    aux->attach_slot = aux->undo_attach_slot;
}

static void dc22_apply_njvd_gating(Sprites *s, const Dc22Static *st, int32_t level) {
    int32_t player = st->player_slot[level];
    const int32_t *njvd = st->njvd_slots + (size_t)level * st->max_njvd;
    int32_t njvd_count = st->njvd_count[level];
    const int32_t *governed = st->governed_slots + (size_t)level * st->max_governed;
    int32_t governed_count = st->governed_count[level];
    const int32_t *code_of = st->code_of + (size_t)level * st->num_slots;

    for (int32_t g = 0; g < governed_count; g++) {
        int32_t gslot = governed[g];
        int32_t gcode = code_of[gslot];
        int active = 0;
        for (int32_t k = 0; k < njvd_count; k++) {
            int32_t j = njvd[k];
            if (code_of[j] != gcode) continue;
            if (dc22_collides_ge0_pair(s, player, j, 1)) {
                active = 1;
                break;
            }
        }
        s->interaction[gslot] = active ? INTANGIBLE : INVISIBLE;
    }
}

enum { DC22_MAX_FAMILY_SNAPSHOT = 256 };

static void dc22_apply_generic_swap(Sprites *s, const Dc22Static *st, int32_t level,
                                    int32_t code, int32_t exclude) {
    int32_t idx = level * st->num_codes + code;
    const int32_t *members = st->code_member_slots + (size_t)idx * st->max_code_member;
    int32_t count = st->code_member_count[idx];
    const int32_t *next_slot = st->next_slot + (size_t)level * st->num_slots;
    const int32_t *next_interaction = st->next_interaction + (size_t)level * st->num_slots;
    int8_t snapshot[DC22_MAX_FAMILY_SNAPSHOT];
    for (int32_t k = 0; k < count; k++) snapshot[k] = s->interaction[members[k]];
    for (int32_t k = 0; k < count; k++) {
        int32_t i = members[k];
        if (!s->alive[i] || i == exclude) continue;
        if (snapshot[k] > INTANGIBLE) continue;
        int32_t target = next_slot[i];
        if (target < 0) continue;
        s->interaction[i] = REMOVED;
        s->interaction[target] = (int8_t)next_interaction[i];
    }
}

static void dc22_apply_tewfut_color_cycle(Sprites *s, const Dc22Static *st, int32_t level,
                                          int32_t code, int32_t exclude) {
    int32_t idx = level * st->num_codes + code;
    const int32_t *members = st->code_member_slots + (size_t)idx * st->max_code_member;
    int32_t count = st->code_member_count[idx];
    const uint8_t *is_tewfut = st->is_tewfut + (size_t)level * st->num_slots;
    const uint8_t *is_qiukbrokfa = st->is_qiukbrokfa + (size_t)level * st->num_slots;
    const int32_t *next_slot = st->next_slot + (size_t)level * st->num_slots;
    const int32_t *next_interaction = st->next_interaction + (size_t)level * st->num_slots;
    int8_t snapshot[DC22_MAX_FAMILY_SNAPSHOT];
    for (int32_t k = 0; k < count; k++) snapshot[k] = s->interaction[members[k]];
    for (int32_t k = 0; k < count; k++) {
        int32_t i = members[k];
        if (!s->alive[i] || i == exclude) continue;
        if (!is_tewfut[i] || !is_qiukbrokfa[i]) continue;
        if (snapshot[k] > INTANGIBLE) continue;
        int32_t target = next_slot[i];
        if (target < 0) continue;
        s->interaction[i] = REMOVED;
        s->interaction[target] = (int8_t)next_interaction[i];
    }
}

static void dc22_teleport_if_standing(Sprites *s, const Dc22Static *st, int32_t level,
                                      int32_t code, int32_t exclude) {
    int32_t player = st->player_slot[level];
    int32_t idx = level * st->num_codes + code;
    const int32_t *members = st->code_member_slots + (size_t)idx * st->max_code_member;
    int32_t count = st->code_member_count[idx];
    const uint8_t *is_tewfut = st->is_tewfut + (size_t)level * st->num_slots;
    const int32_t *name_id = st->name_id + (size_t)level * st->num_slots;
    const int32_t *teleport_name = st->teleport_name + (size_t)level * st->num_slots;

    int32_t standing = -1;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = members[k];
        if (!s->alive[i] || i == exclude || !is_tewfut[i]) continue;
        if (s->interaction[i] > INTANGIBLE) continue;
        if (s->x[i] == s->x[player] && s->y[i] == s->y[player]) {
            standing = i;
            break;
        }
    }
    if (standing < 0) return;
    int32_t target_name = teleport_name[standing];

    int32_t target = -1;
    for (int32_t k = 0; k < count; k++) {
        int32_t i = members[k];
        if (!s->alive[i] || i == exclude || !is_tewfut[i]) continue;
        if (s->interaction[i] > INTANGIBLE) continue;
        if (i == standing) continue;
        if (name_id[i] == target_name) {
            target = i;
            break;
        }
    }
    if (target >= 0 && target_name >= 0) {
        s->x[player] = s->x[target];
        s->y[player] = s->y[target];
    }
}

static void dc22_reveal_by_piyqze(Sprites *s, const Dc22Static *st, int32_t level, int32_t hit) {
    if (hit < 0) return;
    const int32_t *code_of = st->code_of + (size_t)level * st->num_slots;
    int32_t code = code_of[hit];
    const int32_t *buezna = st->buezna_slots + (size_t)level * st->max_buezna;
    int32_t count = st->buezna_count[level];
    for (int32_t k = 0; k < count; k++) {
        int32_t i = buezna[k];
        if (code_of[i] == code) s->interaction[i] = INTANGIBLE;
    }
    s->alive[hit] = 0;
}

static void dc22_hide_aybe(Sprites *s, const Dc22Static *st, int32_t level) {
    const int32_t *aybe = st->aybe_slots + (size_t)level * st->max_aybe;
    int32_t count = st->aybe_count[level];
    for (int32_t k = 0; k < count; k++) s->x[aybe[k]] = 500;
}

static void dc22_crane_anchor(const Dc22Static *st, int32_t level, int32_t col, int32_t row,
                              int32_t *ax, int32_t *ay) {
    int32_t wx = st->crane_origin_x[level] + col * DC22_CRANE_PITCH;
    int32_t wy = st->crane_origin_y[level] - row * DC22_CRANE_PITCH;
    *ax = wx + st->crane_offset_x[level];
    *ay = wy + st->crane_offset_y[level];
}

static int dc22_crane_move_valid(const Dc22Static *st, int32_t level, const Dc22Aux *aux,
                                 int32_t direction, int32_t new_col, int32_t new_row) {
    int32_t ax, ay;
    dc22_crane_anchor(st, level, new_col, new_row, &ax, &ay);
    int in_bounds = ax >= 0 && ax < FRAME_SIZE && ay >= 0 && ay < FRAME_SIZE;
    int32_t axc = dc22_clamp(ax, 0, FRAME_SIZE - 1), ayc = dc22_clamp(ay, 0, FRAME_SIZE - 1);
    int in_track = in_bounds && st->vcha_map[(size_t)level * FRAME_SIZE * FRAME_SIZE + (size_t)ayc * FRAME_SIZE + axc];

    int32_t col = aux->crane_col, row = aux->crane_row;
    int grid_ok = 0;
    if (direction == DC22_DIR_UP) grid_ok = (col == 0) && (row < 3);
    else if (direction == DC22_DIR_DOWN) grid_ok = (col == 0) && (row > 0);
    else if (direction == DC22_DIR_LEFT) grid_ok = ((row == 3) || (row == 0)) && (col > 0);
    else if (direction == DC22_DIR_RIGHT) grid_ok = ((row == 3) || (row == 0)) && (col < 3);

    return st->crane_is_brixto[level] ? in_track : grid_ok;
}

static void dc22_reposition_attachment(Sprites *s, const Dc22Aux *aux, int32_t anchor_x, int32_t anchor_y) {
    if (aux->attach_kind == DC22_ATTACH_NONE) return;
    int32_t slot = aux->attach_slot < 0 ? 0 : aux->attach_slot;
    s->x[slot] = anchor_x - s->w[slot] / 2;
    s->y[slot] = anchor_y - s->h[slot] / 2;
}

static void dc22_move_crane_to(Sprites *s, const Dc22Static *st, int32_t level, Dc22Aux *aux,
                               int32_t col, int32_t row) {
    int32_t ax, ay;
    dc22_crane_anchor(st, level, col, row, &ax, &ay);
    int32_t slot = st->crane_slot[level];
    s->x[slot] = ax - st->crane_offset_x[level];
    s->y[slot] = ay - st->crane_offset_y[level];
    aux->crane_col = col;
    aux->crane_row = row;
    dc22_reposition_attachment(s, aux, ax, ay);
}

static void dc22_start_shake(Sprites *s, const Dc22Static *st, int32_t level, Dc22Aux *aux,
                             int32_t dx, int32_t dy) {
    int32_t slot = st->crane_slot[level];
    aux->shake_active = 1;
    aux->shake_frame = 0;
    aux->shake_dx = dx;
    aux->shake_dy = dy;
    aux->shake_origin_x = s->x[slot];
    aux->shake_origin_y = s->y[slot];
}

static int dc22_bridge_move_blocked(Sprites *s, const Dc22Static *st, int32_t level,
                                    int32_t slot, int32_t new_x, int32_t new_y) {
    int32_t ox = s->x[slot], oy = s->y[slot];
    s->x[slot] = new_x;
    s->y[slot] = new_y;
    int blocked = 0;
    const int32_t *cand = st->brixto_slots + (size_t)level * st->max_brixto;
    int32_t count = st->brixto_count[level];
    for (int32_t k = 0; k < count; k++) {
        int32_t j = cand[k];
        if (j == slot) continue;
        if (s->interaction[j] == REMOVED) continue;
        if (dc22_collides_ge0_pair(s, slot, j, 1)) {
            blocked = 1;
            break;
        }
    }
    s->x[slot] = ox;
    s->y[slot] = oy;
    return blocked;
}

static void dc22_attach(Sprites *s, const Dc22Static *st, int32_t level, Dc22Aux *aux,
                        int32_t kind, int32_t slot, int32_t anchor_x, int32_t anchor_y) {
    aux->attach_kind = kind;
    aux->attach_slot = slot;
    int32_t area = s->atlas->ph * s->atlas->pw;
    int8_t *dst = sprite_pixels_mut(s, st->crane_slot[level]);
    const int8_t *src = st->crane_hold_pixels + (size_t)level * area;
    memcpy(dst, src, (size_t)area);
    dc22_reposition_attachment(s, aux, anchor_x, anchor_y);
}

static void dc22_resolve_crane_click(Sprites *s, const Dc22Static *st, int32_t level, Dc22Aux *aux,
                                     int32_t direction, int *blocked, int *is_directional_block,
                                     int32_t *shake_dx, int32_t *shake_dy) {
    int32_t col = aux->crane_col, row = aux->crane_row;
    int32_t new_col = col, new_row = row;
    if (direction == DC22_DIR_LEFT) new_col = col - 1;
    else if (direction == DC22_DIR_RIGHT) new_col = col + 1;
    if (direction == DC22_DIR_UP) new_row = row + 1;
    else if (direction == DC22_DIR_DOWN) new_row = row - 1;
    int is_directional = direction <= DC22_DIR_RIGHT;
    *blocked = 0;
    *is_directional_block = 0;
    *shake_dx = 0;
    *shake_dy = 0;

    if (is_directional) {
        int move_ok = dc22_crane_move_valid(st, level, aux, direction, new_col, new_row);
        if (move_ok) {
            int32_t ax, ay;
            dc22_crane_anchor(st, level, new_col, new_row, &ax, &ay);
            if (aux->attach_kind == DC22_ATTACH_BRIXTO) {
                int32_t slot = aux->attach_slot < 0 ? 0 : aux->attach_slot;
                int32_t nx = ax - s->w[slot] / 2;
                int32_t ny = ay - s->h[slot] / 2;
                if (dc22_bridge_move_blocked(s, st, level, slot, nx, ny)) {
                    *blocked = 1;
                    *is_directional_block = 1;
                    *shake_dx = DC22_DIR_STEP_X[direction];
                    *shake_dy = DC22_DIR_STEP_Y[direction];
                } else {
                    dc22_move_crane_to(s, st, level, aux, new_col, new_row);
                }
            } else {
                dc22_move_crane_to(s, st, level, aux, new_col, new_row);
            }
        } else {
            *blocked = 1;
            *is_directional_block = 1;
            *shake_dx = DC22_DIR_STEP_X[direction];
            *shake_dy = DC22_DIR_STEP_Y[direction];
        }
        return;
    }

    int32_t ax, ay;
    dc22_crane_anchor(st, level, col, row, &ax, &ay);

    int32_t bridge_slot = -1;
    const int32_t *bcand = st->brixto_slots + (size_t)level * st->max_brixto;
    int32_t bcount = st->brixto_count[level];
    for (int32_t k = 0; k < bcount; k++) {
        int32_t i = bcand[k];
        if (s->interaction[i] == REMOVED) continue;
        int32_t cx = s->x[i] + s->w[i] / 2;
        int32_t cy = s->y[i] + s->h[i] / 2;
        if (cx == ax && cy == ay) {
            bridge_slot = i;
            break;
        }
    }
    int found_bridge = st->crane_is_brixto[level] && bridge_slot >= 0;

    int32_t obj_slot = st->grawwq_object_slot[level];
    int obj_present = obj_slot >= 0;
    int found_object = 0;
    if (obj_present) {
        int32_t ocx = s->x[obj_slot] + s->w[obj_slot] / 2;
        int32_t ocy = s->y[obj_slot] + s->h[obj_slot] / 2;
        found_object = (ocx == ax && ocy == ay);
    }

    int already = aux->attach_kind != DC22_ATTACH_NONE;
    if (already) {
        return;
    } else if (found_bridge) {
        dc22_attach(s, st, level, aux, DC22_ATTACH_BRIXTO, bridge_slot, ax, ay);
    } else if (found_object) {
        dc22_attach(s, st, level, aux, DC22_ATTACH_OBJECT, obj_slot, ax, ay);
    } else {
        aux->flicker_active = 1;
        aux->flicker_frame = 0;
        *blocked = 1;
    }
}

static void dc22_next_level(const Dc22Static *st, int32_t level, int32_t *score,
                            int32_t *status, uint8_t *next_level) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = (uint8_t)!is_last;
    if (is_last) *status = DC22_WIN;
}

static int dc22_spend_step(Dc22Aux *aux) {
    aux->steps -= 1;
    return aux->steps > 0;
}

static inline void dc22_complete(uint8_t *action_complete) { *action_complete = 1; }

static inline void dc22_lose_now(int32_t *status, uint8_t *action_complete) {
    *status = DC22_GAME_OVER;
    *action_complete = 1;
}

static void dc22_noop(const Dc22Static *st, int32_t level, Dc22Aux *aux, Sprites *s,
                      int32_t *status, uint8_t *action_complete) {
    int has_budget = dc22_spend_step(aux);
    if (has_budget) {
        dc22_apply_njvd_gating(s, st, level);
        dc22_complete(action_complete);
    } else {
        dc22_lose_now(status, action_complete);
    }
}

static void dc22_handle_move(Sprites *s, const Dc22Static *st, int32_t level, int32_t dx,
                             int32_t dy, Dc22Aux *aux, int32_t *score, int32_t *status,
                             uint8_t *next_level, uint8_t *action_complete) {
    dc22_save_undo(s, aux);
    int32_t player = st->player_slot[level];
    int32_t before_x = s->x[player], before_y = s->y[player];

    s->x[player] += dx;
    s->y[player] += dy;
    int blocked = dc22_collides_ge0_any(s, player, 0);
    if (blocked) {
        s->x[player] -= dx;
        s->y[player] -= dy;
    }

    int ok = dc22_on_valid_floor(s, st, level, s->x[player], s->y[player], player);
    if (!blocked && !ok) {
        s->x[player] = before_x;
        s->y[player] = before_y;
    }

    int32_t piyqze_hit = dc22_find_piyqze_hit(s, st, level, s->x[player], s->y[player]);
    dc22_reveal_by_piyqze(s, st, level, piyqze_hit);
    dc22_apply_njvd_gating(s, st, level);

    int32_t goal = st->goal_slot[level];
    int won = s->x[player] == s->x[goal] && s->y[player] == s->y[goal];
    if (won) {
        dc22_next_level(st, level, score, status, next_level);
        dc22_complete(action_complete);
    } else {
        int has_budget = dc22_spend_step(aux);
        if (has_budget) dc22_complete(action_complete);
        else dc22_lose_now(status, action_complete);
    }
}

static void dc22_handle_click(Sprites *s, const Camera *camera, const Dc22Static *st,
                              int32_t level, int32_t action_x, int32_t action_y, Dc22Aux *aux,
                              int32_t *status, uint8_t *action_complete) {
    dc22_save_undo(s, aux);

    int32_t wx, wy;
    int on_board;
    dc22_display_to_grid(camera, action_x, action_y, &wx, &wy, &on_board);

    int ok = 1;
    int handled = 0;
    int hit_buezna_flag = 0;

    if (on_board) {
        int32_t buezna_hit = dc22_topmost_visible_opaque(
            s, st, level, wx, wy, st->buezna_slots + (size_t)level * st->max_buezna, st->buezna_count[level]);
        int hit_buezna = buezna_hit >= 0;
        if (hit_buezna) {
            const int32_t *code_of = st->code_of + (size_t)level * st->num_slots;
            int32_t code = code_of[buezna_hit];
            dc22_hide_aybe(s, st, level);
            const uint8_t *is_iophjflwsn = st->is_iophjflwsn + (size_t)level * st->num_slots;
            int is_color_cycle = is_iophjflwsn[buezna_hit];
            if (is_color_cycle) {
                dc22_apply_tewfut_color_cycle(s, st, level, code, buezna_hit);
            } else {
                dc22_apply_generic_swap(s, st, level, code, buezna_hit);
                dc22_teleport_if_standing(s, st, level, code, buezna_hit);
            }
        }

        int32_t crane_slot = st->crane_slot[level];
        int crane_present = crane_slot >= 0;
        int32_t click_hit = -1;
        if (crane_present) {
            click_hit = dc22_topmost_visible_opaque(
                s, st, level, wx, wy, st->sys_click_slots + (size_t)level * st->max_sys_click,
                st->sys_click_count[level]);
        }
        int32_t direction = -1;
        if (click_hit >= 0) {
            const int32_t *dir_slot = st->dir_slot + (size_t)level * DC22_NUM_DIRS;
            for (int32_t d = 0; d < DC22_NUM_DIRS; d++)
                if (dir_slot[d] == click_hit) direction = d;
        }
        int hit_click = direction >= 0;

        int blocked = 0, is_directional_block = 0;
        int32_t shake_dx = 0, shake_dy = 0;
        if (hit_click) {
            dc22_resolve_crane_click(s, st, level, aux, direction, &blocked, &is_directional_block,
                                     &shake_dx, &shake_dy);
        }

        int32_t player = st->player_slot[level];
        ok = dc22_on_valid_floor(s, st, level, s->x[player], s->y[player], player);

        if (!ok) {
            aux->death_active = 1;
            aux->death_frame = 0;
        } else if (blocked && is_directional_block) {
            int has_budget = dc22_spend_step(aux);
            if (has_budget) dc22_start_shake(s, st, level, aux, shake_dx, shake_dy);
            else dc22_lose_now(status, action_complete);
        }
        handled = blocked;
        hit_buezna_flag = hit_buezna;
    }

    if (ok && !handled) {
        int has_budget = dc22_spend_step(aux);
        if (!has_budget) {
            dc22_lose_now(status, action_complete);
            return;
        }
        if (hit_buezna_flag) {
            int has_budget2 = dc22_spend_step(aux);
            if (!has_budget2) {
                dc22_lose_now(status, action_complete);
                return;
            }
        }
        dc22_apply_njvd_gating(s, st, level);
        dc22_complete(action_complete);
    }
}

static void dc22_play(Sprites *s, const Camera *camera, const Dc22Static *st, int32_t level,
                      int32_t action_id, int32_t action_x, int32_t action_y, Dc22Aux *aux,
                      int32_t *score, int32_t *status, uint8_t *next_level,
                      uint8_t *action_complete) {
    int32_t dx = 0, dy = 0;
    if (action_id == DC22_ACTION3) dx = -DC22_STEP_MOVE;
    else if (action_id == DC22_ACTION4) dx = DC22_STEP_MOVE;
    if (action_id == DC22_ACTION1) dy = -DC22_STEP_MOVE;
    else if (action_id == DC22_ACTION2) dy = DC22_STEP_MOVE;
    int is_click = action_id == DC22_ACTION6;
    int is_move = !is_click && (dx != 0 || dy != 0);

    if (is_click) {
        dc22_handle_click(s, camera, st, level, action_x, action_y, aux, status, action_complete);
    } else if (is_move) {
        dc22_handle_move(s, st, level, dx, dy, aux, score, status, next_level, action_complete);
    } else {
        dc22_noop(st, level, aux, s, status, action_complete);
    }
}

static void dc22_animate_death(Sprites *s, const Dc22Static *st, int32_t level, Dc22Aux *aux,
                               int32_t *status, uint8_t *action_complete) {
    int32_t frame = aux->death_frame + 1;
    int32_t player = st->player_slot[level];
    int32_t filled = frame / 3;

    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int32_t real_h = s->h[player], real_w = s->w[player];
    int8_t *patch = sprite_pixels_mut(s, player);
    for (int32_t r = 0; r < ph; r++) {
        for (int32_t c = 0; c < pw; c++) {
            int in_bounds = r < real_h && c < real_w;
            int32_t flat_index = r * real_w + c;
            if (in_bounds && flat_index <= filled) patch[(size_t)r * pw + c] = (int8_t)DC22_COLOR_DEATH;
        }
    }
    aux->death_frame = frame;

    if (frame == DC22_DEATH_FRAMES) {
        int32_t steps_after = aux->steps - DC22_DEATH_PENALTY;
        if (steps_after > 0) {
            dc22_restore_undo(s, aux);
            dc22_apply_njvd_gating(s, st, level);
            aux->steps = steps_after;
            aux->death_active = 0;
            aux->death_frame = -1;
            dc22_complete(action_complete);
        } else {
            aux->death_active = 0;
            aux->death_frame = -1;
            *status = DC22_GAME_OVER;
            dc22_complete(action_complete);
        }
    }
}

static void dc22_animate_shake(Sprites *s, const Dc22Static *st, int32_t level, Dc22Aux *aux,
                               uint8_t *action_complete) {
    int32_t slot = st->crane_slot[level];
    int32_t x = (aux->shake_frame == 0) ? aux->shake_origin_x + aux->shake_dx : aux->shake_origin_x;
    int32_t y = (aux->shake_frame == 0) ? aux->shake_origin_y + aux->shake_dy : aux->shake_origin_y;
    s->x[slot] = x;
    s->y[slot] = y;
    int32_t anchor_x = x + st->crane_offset_x[level];
    int32_t anchor_y = y + st->crane_offset_y[level];
    dc22_reposition_attachment(s, aux, anchor_x, anchor_y);

    int32_t frame = aux->shake_frame + 1;
    aux->shake_frame = frame;
    if (frame >= DC22_SHAKE_FRAMES) {
        aux->shake_active = 0;
        aux->shake_frame = 0;
        dc22_apply_njvd_gating(s, st, level);
        dc22_complete(action_complete);
    }
}

static void dc22_animate_flicker(Sprites *s, const Dc22Static *st, int32_t level, Dc22Aux *aux,
                                 uint8_t *action_complete) {
    int32_t slot = st->crane_slot[level];
    int32_t area = s->atlas->ph * s->atlas->pw;
    int variant2 = (aux->flicker_frame % 2) == 1;
    const int8_t *src = variant2 ? (st->crane_hold_pixels + (size_t)level * area)
                                  : (s->atlas->pixels + (size_t)slot * area);
    memcpy(sprite_pixels_mut(s, slot), src, (size_t)area);

    int32_t frame = aux->flicker_frame + 1;
    aux->flicker_frame = frame;

    if (frame == DC22_FLICKER_FRAMES) {
        memcpy(sprite_pixels_mut(s, slot), s->atlas->pixels + (size_t)slot * area, (size_t)area);
        aux->flicker_active = 0;
        aux->flicker_frame = 0;
        dc22_apply_njvd_gating(s, st, level);
        dc22_complete(action_complete);
    }
}

void dc22_zero_aux(Dc22Aux *aux, const Dc22Static *st) {
    aux->steps = 0;
    aux->crane_col = 0;
    aux->crane_row = 0;
    aux->attach_kind = DC22_ATTACH_NONE;
    aux->attach_slot = -1;
    aux->death_active = 0;
    aux->death_frame = -1;
    aux->shake_active = 0;
    aux->shake_frame = 0;
    aux->shake_dx = 0;
    aux->shake_dy = 0;
    aux->shake_origin_x = 0;
    aux->shake_origin_y = 0;
    aux->flicker_active = 0;
    aux->flicker_frame = 0;
    aux->undo_valid = 0;

    int32_t n = st->num_slots;
    int32_t area = st->patch_h * st->patch_w;
    memset(aux->undo_pixels, -1, (size_t)n * (size_t)area);
    memset(aux->undo_x, 0, sizeof(int32_t) * (size_t)n);
    memset(aux->undo_y, 0, sizeof(int32_t) * (size_t)n);
    memset(aux->undo_interaction, REMOVED, (size_t)n);
    memset(aux->undo_alive, 0, (size_t)n);
    aux->undo_crane_col = 0;
    aux->undo_crane_row = 0;
    aux->undo_attach_kind = DC22_ATTACH_NONE;
    aux->undo_attach_slot = -1;
}

void dc22_on_set_level(const Dc22Static *st, int32_t level, Dc22Aux *aux) {
    dc22_zero_aux(aux, st);
    aux->steps = st->budget[level];
}

void dc22_step_once(Sprites *sprites, const Camera *camera, const Dc22Static *st, int32_t level,
                    int32_t action_id, int32_t action_x, int32_t action_y, Dc22Aux *aux,
                    int32_t *score, int32_t *status, uint8_t *next_level, uint8_t *action_complete) {
    if (action_id == DC22_ACTION_RESET) {
        dc22_complete(action_complete);
        return;
    }
    dc22_apply_njvd_gating(sprites, st, level);
    if (aux->death_active) {
        dc22_animate_death(sprites, st, level, aux, status, action_complete);
    } else if (aux->shake_active) {
        dc22_animate_shake(sprites, st, level, aux, action_complete);
    } else if (aux->flicker_active) {
        dc22_animate_flicker(sprites, st, level, aux, action_complete);
    } else {
        dc22_play(sprites, camera, st, level, action_id, action_x, action_y, aux, score, status,
                 next_level, action_complete);
    }
}

static void dc22_draw_budget(int8_t *frame, const Dc22Static *st, int32_t level, const Dc22Aux *aux) {
    int32_t budget = st->budget[level];
    if (budget == 0) return;
    int32_t hi = budget < 0 ? 0 : budget;
    int32_t steps = dc22_clamp(aux->steps, 0, hi);
    int32_t done = budget - steps;
    int8_t *row = frame + (size_t)(FRAME_SIZE - 1) * FRAME_SIZE;
    for (int32_t c = 0; c < FRAME_SIZE; c++) {
        int filled = (c * budget) >= (FRAME_SIZE * done);
        row[c] = (int8_t)(filled ? DC22_COLOR_HUD_FILLED : DC22_COLOR_HUD_EMPTY);
    }
}

static void dc22_draw_death_vignette(int8_t *frame, const Sprites *sprites, const Camera *camera,
                                     const Dc22Static *st, int32_t level, const Dc22Aux *aux) {
    if (!aux->death_active) return;
    int32_t n = aux->death_frame + DC22_VIGNETTE_START;
    int full = n >= DC22_VIGNETTE_FULL;
    int32_t player = st->player_slot[level];

    int32_t scale = FRAME_SIZE / camera->width;
    int32_t sy = FRAME_SIZE / camera->height;
    if (sy < scale) scale = sy;
    int32_t x_offset = (FRAME_SIZE - camera->width * scale) / 2;
    int32_t y_offset = (FRAME_SIZE - camera->height * scale) / 2;
    int32_t rel_x = sprites->x[player] - camera->x;
    int32_t rel_y = sprites->y[player] - camera->y;

    if (full) {
        memset(frame, DC22_COLOR_VIGNETTE, (size_t)FRAME_SIZE * FRAME_SIZE);
        return;
    }

    float cx = (float)(x_offset + rel_x * scale) + (float)scale / 2.0f;
    float cy = (float)(y_offset + rel_y * scale) + (float)scale / 2.0f;
    int32_t diff = DC22_VIGNETTE_FULL - n;
    float radius = (100.0f * (float)diff) / (float)DC22_VIGNETTE_FULL;

    for (int32_t r = 0; r < FRAME_SIZE; r++) {
        for (int32_t c = 0; c < FRAME_SIZE; c++) {
            float ddx = (float)c - cx;
            float ddy = (float)r - cy;
            float dist = sqrtf(ddx * ddx + ddy * ddy);
            if (dist > radius) frame[(size_t)r * FRAME_SIZE + c] = (int8_t)DC22_COLOR_VIGNETTE;
        }
    }
}

void dc22_render_interface(int8_t *frame, const Sprites *sprites, const Camera *camera,
                           const Dc22Static *st, int32_t level, const Dc22Aux *aux) {
    dc22_draw_budget(frame, st, level, aux);
    dc22_draw_death_vignette(frame, sprites, camera, st, level, aux);
}
