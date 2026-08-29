#include "sc25.h"

#include <stdlib.h>
#include <string.h>

enum { SC25_ACTION_RESET = 0, SC25_ACTION1 = 1, SC25_ACTION2 = 2, SC25_ACTION3 = 3,
       SC25_ACTION4 = 4, SC25_ACTION6 = 6 };
enum { SC25_WIN = 2, SC25_GAME_OVER = 3 };
enum { SC25_TELEPORT = 0, SC25_RESIZE = 1, SC25_FIREBALL = 2, SC25_NUM_SPELLS = 3 };
enum { SC25_MISS_COLOR = 14, SC25_BASE_COLOR = 2, SC25_BAR_FILLED = 0,
       SC25_BAR_EMPTY = 14, SC25_SHAKE_COLOR = 8, SC25_REFUND_STEPS = 10,
       SC25_FLASH_COLOUR = 11 };
enum { SC25_CAST_FRAMES = 8, SC25_TELEPORT_FRAMES = 4, SC25_MISS_FRAMES = 12,
       SC25_SLIDE_FRAMES = 2, SC25_RESIZE_FRAMES = 8, SC25_RESIZE_BLOCKED_FRAMES = 12,
       SC25_DEMO_FRAMES_PER_CELL = 5 };
enum { SC25_PATCH_CAP = 16384 };

static const int8_t SC25_SPELL_COLOR[SC25_NUM_SPELLS] = {11, 15, 6};
static const uint8_t SC25_PATTERN[SC25_NUM_SPELLS][3][3] = {
    {{1, 1, 0}, {0, 1, 0}, {0, 0, 0}},
    {{0, 1, 0}, {1, 0, 1}, {0, 1, 0}},
    {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}},
};
static const int32_t SC25_FACING_ROTATION[4] = {180, 0, 90, 270};
static const int32_t SC25_FACING_DELTA[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
static const int32_t SC25_FIREBALL_ROTATION[4] = {90, 270, 0, 180};

static inline int32_t sc25_min(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t sc25_max(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t sc25_clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t sc25_rotk(int32_t rot) {
    int32_t m = (-rot) % 360;
    if (m < 0) m += 360;
    return m / 90;
}

static void sc25_recolor_all(ArcSprites *s, int32_t slot, int8_t colour) {
    int32_t area = s->atlas->ph * s->atlas->pw;
    int8_t *p = arc_sprite_pixels_mut(s, slot);
    for (int32_t i = 0; i < area; i++)
        if (p[i] >= 0) p[i] = colour;
}

static void sc25_recolor_all_if(ArcSprites *s, int32_t slot, int8_t colour) {
    if (colour >= 0) sc25_recolor_all(s, slot, colour);
}

static void sc25_recolor_bounded(ArcSprites *s, int32_t slot, int8_t colour,
                                 const int32_t *box) {
    int32_t pw = s->atlas->pw;
    int8_t *p = arc_sprite_pixels_mut(s, slot);
    int32_t y0 = box[0], y1 = box[1], x0 = box[2], x1 = box[3];
    for (int32_t v = y0; v < y1; v++) {
        int8_t *row = p + (size_t)v * pw;
        for (int32_t u = x0; u < x1; u++)
            if (row[u] >= 0) row[u] = colour;
    }
}

static void sc25_compute_grid_bbox(Sc25Aux *aux, const ArcSprites *s, const Sc25Static *st,
                                   int32_t level) {
    const int32_t *grid = st->grid_slot + (size_t)level * 9;
    const ArcAtlas *a = s->atlas;
    int32_t ph = a->ph, pw = a->pw;
    for (int32_t i = 0; i < 9; i++) {
        int32_t slot = grid[i];
        int32_t y0 = ph, y1 = 0, x0 = pw, x1 = 0;
        if (slot >= 0) {
            const int8_t *patch = a->pixels + (size_t)slot * ph * pw;
            for (int32_t v = 0; v < ph; v++) {
                const int8_t *row = patch + (size_t)v * pw;
                for (int32_t u = 0; u < pw; u++) {
                    if (row[u] >= 0) {
                        if (v < y0) y0 = v;
                        if (v + 1 > y1) y1 = v + 1;
                        if (u < x0) x0 = u;
                        if (u + 1 > x1) x1 = u + 1;
                    }
                }
            }
        }
        if (y1 <= y0 || x1 <= x0) {
            y0 = 0; y1 = 0; x0 = 0; x1 = 0;
        }
        aux->grid_bbox[i][0] = y0;
        aux->grid_bbox[i][1] = y1;
        aux->grid_bbox[i][2] = x0;
        aux->grid_bbox[i][3] = x1;
    }
}

static void sc25_recolor_grid_cells(ArcSprites *s, const Sc25Static *st, int32_t level,
                                    int8_t colour, const Sc25Aux *aux) {
    const int32_t *grid = st->grid_slot + (size_t)level * 9;
    for (int32_t i = 0; i < 9; i++) {
        int32_t slot = grid[i];
        if (slot >= 0) sc25_recolor_bounded(s, slot, colour, aux->grid_bbox[i]);
    }
}

static void sc25_write_grid(ArcSprites *s, const Sc25Static *st, int32_t level,
                            const int8_t colours[3][3], const Sc25Aux *aux) {
    const int32_t *grid = st->grid_slot + (size_t)level * 9;
    for (int32_t r = 0; r < 3; r++)
        for (int32_t c = 0; c < 3; c++) {
            int32_t slot = grid[r * 3 + c];
            if (slot >= 0) sc25_recolor_bounded(s, slot, colours[r][c], aux->grid_bbox[r * 3 + c]);
        }
}

static int32_t sc25_matched_spell(const Sc25Aux *aux, const Sc25Static *st, int32_t level) {
    for (int32_t sp = 0; sp < SC25_NUM_SPELLS; sp++) {
        int match = 1;
        for (int32_t r = 0; r < 3 && match; r++)
            for (int32_t c = 0; c < 3; c++)
                if ((int)aux->drawn[r][c] != (int)SC25_PATTERN[sp][r][c]) { match = 0; break; }
        if (match) return st->allowed[level * SC25_NUM_SPELLS + sp] ? sp : -1;
    }
    return -1;
}

static void sc25_cell_colour(const Sc25Aux *aux, int32_t matched, int8_t out[3][3]) {
    int32_t hint = aux->hint;
    int8_t drawn_colour = matched >= 0 ? SC25_SPELL_COLOR[matched] : (int8_t)SC25_MISS_COLOR;
    for (int32_t r = 0; r < 3; r++) {
        for (int32_t c = 0; c < 3; c++) {
            if (aux->drawn[r][c]) { out[r][c] = drawn_colour; continue; }
            int hp = hint >= 0 && SC25_PATTERN[hint][r][c];
            out[r][c] = hp ? (int8_t)0 : (int8_t)SC25_BASE_COLOR;
        }
    }
}

static void sc25_recolour_grid(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux) {
    int32_t matched = sc25_matched_spell(aux, st, level);
    int8_t colours[3][3];
    sc25_cell_colour(aux, matched, colours);
    sc25_write_grid(s, st, level, colours, aux);
    if (matched >= 0 && !aux->cast_active) {
        aux->cast_active = 1;
        aux->cast_frame = 0;
        aux->cast_spell = matched;
    }
}

static void sc25_fill_bar(ArcSprites *s, const Sc25Static *st, int32_t level, const Sc25Aux *aux) {
    int32_t slot = st->action_ui[level];
    if (slot < 0) return;
    int32_t base_total = st->bar_count[level] / 2;
    int32_t budget = st->budget[level];
    int32_t filled_base = budget > 0 ? (aux->steps * base_total) / budget : 0;
    int32_t filled = filled_base * 2;
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    const uint8_t *mask = st->bar_mask + (size_t)level * st->bar_ph;
    int8_t *p = arc_sprite_pixels_mut(s, slot);
    for (int32_t r = 0; r < ph; r++) {
        if (r >= st->bar_ph || !mask[r]) continue;
        int8_t colour = (int8_t)(r < filled ? SC25_BAR_FILLED : SC25_BAR_EMPTY);
        for (int32_t c = 0; c < pw; c++) p[r * pw + c] = colour;
    }
}

static void sc25_apply_overlay_visibility(ArcSprites *s, const Sc25Static *st, int32_t level,
                                          const Sc25Aux *aux) {
    int32_t a_slot = st->acyylh_ovl[level], s_slot = st->smzaik_ovl[level];
    if (a_slot >= 0) arc_set_visible(s, a_slot, aux->scale2);
    if (s_slot >= 0) arc_set_visible(s, s_slot, !aux->scale2);
}

static void sc25_reposition_overlay(ArcSprites *s, const Sc25Static *st, int32_t level,
                                    const Sc25Aux *aux) {
    int32_t a_slot = st->acyylh_ovl[level];
    if (a_slot < 0) return;
    int32_t count = st->tp_counts[level * 2 + 1];
    if (count <= 0) return;
    int32_t idx = sc25_min(aux->tp_idx[1], count - 1);
    int32_t target_slot = st->tp_slots[(size_t)(level * 2 + 1) * st->max_tp + idx];
    int32_t tx = s->x[target_slot] - 1, ty = s->y[target_slot] - 1;
    arc_set_position(s, a_slot, tx, ty);
}

static void sc25_spend_step(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                            int32_t *status, uint8_t *action_complete) {
    aux->steps += 1;
    *action_complete = 1;
    sc25_fill_bar(s, st, level, aux);
    int32_t budget = st->budget[level];
    if (budget > 0 && aux->steps > budget) *status = SC25_GAME_OVER;
}

static void sc25_reset_drawn(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux) {
    memset(aux->drawn, 0, sizeof aux->drawn);
    sc25_recolour_grid(s, st, level, aux);
}

static void sc25_finish_effect(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                               int32_t *status, uint8_t *action_complete) {
    sc25_reset_drawn(s, st, level, aux);
    sc25_spend_step(s, st, level, aux, status, action_complete);
}

static void sc25_rot90_2x2(int8_t out[2][2], const int8_t in[2][2], int32_t k) {
    switch (k & 3) {
        case 0:
            out[0][0] = in[0][0]; out[0][1] = in[0][1];
            out[1][0] = in[1][0]; out[1][1] = in[1][1];
            break;
        case 1:
            out[0][0] = in[0][1]; out[0][1] = in[1][1];
            out[1][0] = in[0][0]; out[1][1] = in[1][0];
            break;
        case 2:
            out[0][0] = in[1][1]; out[0][1] = in[1][0];
            out[1][0] = in[0][1]; out[1][1] = in[0][0];
            break;
        default:
            out[0][0] = in[1][0]; out[0][1] = in[0][0];
            out[1][0] = in[1][1]; out[1][1] = in[0][1];
            break;
    }
}

static void sc25_player_render(const Sc25Aux *aux, const Sc25Static *st, int32_t level,
                               int32_t ph, int32_t pw, int8_t *canvas, int32_t *out_h,
                               int32_t *out_w) {
    int32_t rot = aux->facing_set ? SC25_FACING_ROTATION[aux->facing] : st->player_rotation0[level];
    int32_t k = sc25_rotk(rot);
    int8_t rotated[2][2];
    sc25_rot90_2x2(rotated, aux->player_base, k);

    int8_t painted[4][4];
    if (aux->scale2) {
        for (int32_t i = 0; i < 4; i++)
            for (int32_t j = 0; j < 4; j++)
                painted[i][j] = rotated[i / 2][j / 2];
    } else {
        for (int32_t i = 0; i < 4; i++)
            for (int32_t j = 0; j < 4; j++)
                painted[i][j] = (i < 2 && j < 2) ? rotated[i][j] : (int8_t)-1;
    }

    memset(canvas, -1, (size_t)ph * pw);
    int32_t lh = sc25_min(4, ph), lw = sc25_min(4, pw);
    for (int32_t i = 0; i < lh; i++)
        for (int32_t j = 0; j < lw; j++)
            canvas[i * pw + j] = painted[i][j];

    *out_h = aux->scale2 ? 4 : 2;
    *out_w = *out_h;
}

static void sc25_apply_player_sprite(ArcSprites *s, const Sc25Static *st, int32_t level,
                                     Sc25Aux *aux) {
    int32_t slot = st->pluyoo[level];
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int8_t canvas[SC25_PATCH_CAP];
    int32_t h, w;
    sc25_player_render(aux, st, level, ph, pw, canvas, &h, &w);
    memcpy(arc_sprite_pixels_mut(s, slot), canvas, (size_t)ph * pw);
    s->h[slot] = h;
    s->w[slot] = w;
}

void sc25_zero_aux(Sc25Aux *aux) {
    memset(aux, 0, sizeof *aux);
    aux->hint = -1;
    aux->scale2 = 1;
    aux->facing = 1;
    aux->tp_target_slot = -1;
    aux->demo_spell = -1;
    aux->cast_spell = -1;
}

void sc25_on_set_level(ArcSprites *sprites, const Sc25Static *st, int32_t level, Sc25Aux *aux) {
    int32_t old_hint = aux->hint;
    int32_t old_facing = aux->facing;
    sc25_zero_aux(aux);
    sc25_compute_grid_bbox(aux, sprites, st, level);
    sc25_recolor_grid_cells(sprites, st, level, (int8_t)SC25_BASE_COLOR, aux);
    memcpy(aux->player_base, st->player_base0 + (size_t)level * 4, 4);
    aux->hint = old_hint;
    aux->facing = old_facing;
    aux->steps = 0;
    aux->demo0_pending = (uint8_t)(level == 0);

    sc25_fill_bar(sprites, st, level, aux);
    int32_t auto_hint = st->auto_hint[level];
    if (auto_hint >= 0) aux->hint = auto_hint;
    sc25_recolour_grid(sprites, st, level, aux);
    sc25_reposition_overlay(sprites, st, level, aux);
    sc25_apply_overlay_visibility(sprites, st, level, aux);
    sc25_apply_player_sprite(sprites, st, level, aux);
}

static void sc25_next_level(const Sc25Static *st, int32_t level, int32_t *score,
                            int32_t *status, uint8_t *next_level) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = (uint8_t)!is_last;
    if (is_last) *status = SC25_WIN;
}

static void sc25_step_demo0(Sc25Aux *aux) {
    int32_t hint = aux->hint;
    aux->demo0_pending = 0;
    aux->demo_active = (uint8_t)(hint >= 0);
    aux->demo_frame = 0;
    aux->demo_spell = hint;
}

static void sc25_step_slide(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                            int32_t *score, int32_t *status, uint8_t *next_level,
                            uint8_t *action_complete) {
    int32_t slot = st->pluyoo[level];
    if (aux->slide_frame > 0) {
        arc_move_sprite(s, slot, aux->slide_dx, aux->slide_dy);
        aux->slide_frame -= 1;
    } else {
        aux->slide_active = 0;
        sc25_next_level(st, level, score, status, next_level);
        *action_complete = 1;
    }
}

static void sc25_step_miss(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                           uint8_t *action_complete) {
    int32_t frame = aux->miss_frame;
    int32_t cyc = frame % 6;
    int8_t colours[3][3];
    if (cyc < 3) {
        for (int32_t r = 0; r < 3; r++)
            for (int32_t c = 0; c < 3; c++) colours[r][c] = (int8_t)SC25_SHAKE_COLOR;
    } else {
        int32_t matched = sc25_matched_spell(aux, st, level);
        sc25_cell_colour(aux, matched, colours);
    }
    sc25_write_grid(s, st, level, colours, aux);
    frame += 1;
    aux->miss_frame = frame;
    if (frame >= SC25_MISS_FRAMES) {
        aux->miss_active = 0;
        sc25_recolour_grid(s, st, level, aux);
        *action_complete = 1;
    }
}

static void sc25_step_demo(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                           uint8_t *action_complete) {
    int32_t frame = aux->demo_frame;
    int32_t spell = aux->demo_spell;
    int32_t active_idx = frame / SC25_DEMO_FRAMES_PER_CELL;
    int32_t count = 0;
    int8_t colours[3][3];
    for (int32_t r = 0; r < 3; r++) {
        for (int32_t c = 0; c < 3; c++) {
            int t = spell >= 0 && SC25_PATTERN[spell][r][c];
            colours[r][c] = (t && count == active_idx) ? (int8_t)14 : (int8_t)-1;
            if (t) count += 1;
        }
    }
    int32_t total = count * SC25_DEMO_FRAMES_PER_CELL;
    const int32_t *grid = st->grid_slot + (size_t)level * 9;
    for (int32_t r = 0; r < 3; r++)
        for (int32_t c = 0; c < 3; c++) {
            int32_t slot = grid[r * 3 + c];
            if (slot >= 0) sc25_recolor_all_if(s, slot, colours[r][c]);
        }
    frame += 1;
    aux->demo_frame = frame;
    if (frame > total) {
        aux->demo_active = 0;
        sc25_recolour_grid(s, st, level, aux);
        *action_complete = 1;
    }
}

static void sc25_start_teleport(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                                int32_t *status, uint8_t *action_complete) {
    int32_t list_idx = aux->scale2 ? 1 : 0;
    int32_t count = st->tp_counts[level * 2 + list_idx];
    int32_t idx = aux->tp_idx[list_idx];
    int32_t idx_valid = sc25_min(idx, sc25_max(count - 1, 0));
    int32_t target_slot = st->tp_slots[(size_t)(level * 2 + list_idx) * st->max_tp + idx_valid];
    int has_target = count > 0;
    int32_t next_idx = has_target ? (idx + 1) % count : idx;
    if (has_target) aux->tp_idx[list_idx] = next_idx;

    int32_t safe_slot = target_slot < 0 ? 0 : target_slot;
    int32_t tx = s->x[safe_slot], ty = s->y[safe_slot];

    aux->tp_active = (uint8_t)has_target;
    aux->tp_frame = 0;
    aux->tp_target_x = tx;
    aux->tp_target_y = ty;
    aux->tp_target_slot = has_target ? target_slot : -1;
    memcpy(aux->tp_player_saved, aux->player_base, sizeof aux->tp_player_saved);

    sc25_reset_drawn(s, st, level, aux);
    if (!has_target) sc25_spend_step(s, st, level, aux, status, action_complete);
}

static void sc25_step_teleport(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                               int32_t *status, uint8_t *action_complete) {
    int32_t target_slot = aux->tp_target_slot;
    int32_t frame_next = aux->tp_frame + 1;
    aux->tp_frame = frame_next;

    if (frame_next < SC25_TELEPORT_FRAMES) {
        for (int32_t i = 0; i < 2; i++)
            for (int32_t j = 0; j < 2; j++)
                if (aux->player_base[i][j] >= 0) aux->player_base[i][j] = SC25_FLASH_COLOUR;
        sc25_recolor_all(s, target_slot, SC25_FLASH_COLOUR);
    } else if (frame_next == SC25_TELEPORT_FRAMES) {
        int32_t area = s->atlas->ph * s->atlas->pw;
        const int8_t *clean = s->atlas->pixels + (size_t)target_slot * area;
        memcpy(arc_sprite_pixels_mut(s, target_slot), clean, (size_t)area);
        memcpy(aux->player_base, aux->tp_player_saved, sizeof aux->player_base);
        int32_t pslot = st->pluyoo[level];
        s->x[pslot] = aux->tp_target_x;
        s->y[pslot] = aux->tp_target_y;
    } else {
        aux->tp_active = 0;
        sc25_reposition_overlay(s, st, level, aux);
        sc25_spend_step(s, st, level, aux, status, action_complete);
    }
}

static void sc25_blocked_offset(const Sc25Static *st, int32_t level, int32_t px, int32_t py,
                                int32_t *out_dx, int32_t *out_dy, int *out_ok) {
    static const int32_t OFF[9][2] = {
        {-2, -2}, {-2, -1}, {-2, 0}, {-1, -2}, {-1, -1}, {-1, 0}, {0, -2}, {0, -1}, {0, 0},
    };
    const uint8_t *block = st->grow_block + (size_t)level * ARC_FRAME_SIZE * ARC_FRAME_SIZE;
    for (int32_t k = 0; k < 9; k++) {
        int32_t ox = OFF[k][0], oy = OFF[k][1];
        int32_t nx = px + ox, ny = py + oy;
        if (nx < 0 || ny < 0 || nx + 4 > ARC_FRAME_SIZE || ny + 4 > ARC_FRAME_SIZE) continue;
        int hit = 0;
        for (int32_t dy = 0; dy < 4 && !hit; dy++)
            for (int32_t dx = 0; dx < 4; dx++)
                if (block[(size_t)(ny + dy) * ARC_FRAME_SIZE + (nx + dx)]) { hit = 1; break; }
        if (!hit) { *out_dx = ox; *out_dy = oy; *out_ok = 1; return; }
    }
    *out_dx = 0;
    *out_dy = 0;
    *out_ok = 0;
}

static void sc25_start_resize(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux) {
    int32_t target2 = !aux->scale2;
    int32_t slot = st->pluyoo[level];
    int32_t px = s->x[slot], py = s->y[slot];
    int32_t ox, oy, ok;
    sc25_blocked_offset(st, level, px, py, &ox, &oy, &ok);
    int growing = target2;
    int blocked = growing && !ok;
    int32_t off_x = (growing && ok) ? ox : 0;
    int32_t off_y = (growing && ok) ? oy : 0;
    if (off_x != 0 || off_y != 0) {
        s->x[slot] += off_x;
        s->y[slot] += off_y;
    }

    aux->resize_active = 1;
    aux->resize_frame = 0;
    aux->resize_target2 = (uint8_t)target2;
    aux->resize_blocked = (uint8_t)blocked;
    aux->resize_offset_x = off_x;
    aux->resize_offset_y = off_y;

    sc25_reset_drawn(s, st, level, aux);
}

static void sc25_collect_blockers(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux) {
    int32_t slot = st->pluyoo[level];
    int32_t px = s->x[slot], py = s->y[slot];
    int32_t scale = aux->scale2 ? 2 : 1;
    int32_t pw = 2 * scale, ph = 2 * scale;
    int32_t count = st->blocker_count[level];
    const int32_t *blockers = st->blockers + (size_t)level * st->max_blockers;
    for (int32_t k = 0; k < count; k++) {
        int32_t bslot = blockers[k];
        if (!s->alive[bslot]) continue;
        int32_t bx = s->x[bslot], by = s->y[bslot], bw = s->w[bslot], bh = s->h[bslot];
        int overlap = (px < bx + bw) && (px + pw > bx) && (py < by + bh) && (py + ph > by);
        if (overlap) {
            arc_remove_sprite(s, bslot);
            aux->steps = sc25_max(aux->steps - SC25_REFUND_STEPS, 0);
            sc25_fill_bar(s, st, level, aux);
        }
    }
}

static void sc25_step_resize(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                             int32_t *status, uint8_t *action_complete) {
    int32_t frame = aux->resize_frame;
    int blocked = aux->resize_blocked;
    int32_t tg = frame / 2;

    if (blocked) {
        int cleanup = (tg % 4 == 1) || (tg % 4 == 3);
        if (cleanup) {
            for (int32_t i = 0; i < 2; i++)
                for (int32_t j = 0; j < 2; j++) {
                    int8_t v = aux->player_base[i][j];
                    if (v == 9 || v == 10) aux->player_base[i][j] = 4;
                }
        } else {
            aux->player_base[0][0] = 10; aux->player_base[0][1] = 10;
            aux->player_base[1][0] = 9; aux->player_base[1][1] = 9;
        }
    } else {
        int cur_is_target = (tg % 2) == 1;
        aux->scale2 = (uint8_t)(cur_is_target ? aux->resize_target2 : !aux->resize_target2);
    }

    frame += 1;
    aux->resize_frame = frame;
    int32_t max_frames = blocked ? SC25_RESIZE_BLOCKED_FRAMES : SC25_RESIZE_FRAMES;

    if (frame >= max_frames) {
        if (blocked) {
            aux->player_base[0][0] = 10; aux->player_base[0][1] = 10;
            aux->player_base[1][0] = 9; aux->player_base[1][1] = 9;
        } else {
            aux->scale2 = aux->resize_target2;
            sc25_collect_blockers(s, st, level, aux);
            sc25_apply_overlay_visibility(s, st, level, aux);
        }
        aux->resize_active = 0;
        sc25_spend_step(s, st, level, aux, status, action_complete);
    }
}

static void sc25_rotate_pad(int8_t *out, int32_t ph, int32_t pw, const int8_t *base,
                            int32_t base_h, int32_t base_w, int32_t consumed, int32_t k,
                            int32_t true_h, int32_t true_w) {
    for (int32_t row = 0; row < ph; row++) {
        for (int32_t col = 0; col < pw; col++) {
            int32_t valid, sr, sc;
            switch (k & 3) {
                case 0: valid = row < true_h && col < true_w; sr = row; sc = col; break;
                case 1: valid = row < true_w && col < true_h; sr = col; sc = true_w - 1 - row; break;
                case 2: valid = row < true_h && col < true_w; sr = true_h - 1 - row; sc = true_w - 1 - col; break;
                default: valid = row < true_w && col < true_h; sr = true_h - 1 - col; sc = row; break;
            }
            int32_t br = sc25_clamp(sr, 0, base_h - 1);
            int32_t bc = sc25_clamp(sc + consumed, 0, base_w - 1);
            out[row * pw + col] = valid ? base[br * base_w + bc] : (int8_t)-1;
        }
    }
}

static void sc25_fb_canvas(int8_t *out, int32_t ph, int32_t pw, const Sc25Static *st,
                           const Sc25Aux *aux, int8_t colour, int mirror_lr, int mirror_ud) {
    int32_t base_h = st->fb_base_shape[aux->fb_scale_idx * 2 + 0];
    int32_t base_w = st->fb_base_shape[aux->fb_scale_idx * 2 + 1];
    int32_t remaining_w = base_w - aux->fb_consumed;
    const int8_t *base = st->fb_base + (size_t)aux->fb_scale_idx * st->fb_base_h * st->fb_base_w;

    int8_t canvas[SC25_PATCH_CAP];
    sc25_rotate_pad(canvas, ph, pw, base, base_h, base_w, aux->fb_consumed, aux->fb_rot_k,
                    base_h, remaining_w);

    int32_t odd = aux->fb_rot_k % 2;
    int32_t true_h = odd ? remaining_w : base_h;
    int32_t true_w = odd ? base_h : remaining_w;

    int8_t tmp[SC25_PATCH_CAP];
    if (mirror_lr) {
        for (int32_t row = 0; row < ph; row++)
            for (int32_t col = 0; col < pw; col++) {
                int32_t fc = col < true_w ? true_w - 1 - col : col;
                fc = sc25_clamp(fc, 0, pw - 1);
                tmp[row * pw + col] = canvas[row * pw + fc];
            }
    } else {
        memcpy(tmp, canvas, (size_t)ph * pw);
    }

    if (mirror_ud) {
        for (int32_t row = 0; row < ph; row++) {
            int32_t fr = row < true_h ? true_h - 1 - row : row;
            fr = sc25_clamp(fr, 0, ph - 1);
            memcpy(out + row * pw, tmp + fr * pw, (size_t)pw);
        }
    } else {
        memcpy(out, tmp, (size_t)ph * pw);
    }

    int32_t area = ph * pw;
    for (int32_t i = 0; i < area; i++) out[i] = out[i] >= 0 ? colour : out[i];
}

static void sc25_start_fireball(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux) {
    int32_t facing = aux->facing;
    int32_t dx = SC25_FACING_DELTA[facing][0], dy = SC25_FACING_DELTA[facing][1];
    int32_t slot = st->pluyoo[level];
    int32_t px = s->x[slot], py = s->y[slot];
    int32_t scale = aux->scale2 ? 2 : 1;
    int32_t step = 2 * scale;
    int32_t off_x = dx > 0 ? step - 1 : 0;
    int32_t off_y = dy > 0 ? step - 1 : 0;
    int32_t origin_x = px + off_x, origin_y = py + off_y;

    const int32_t *hit_grid = st->fb_hit_slot + (size_t)level * ARC_FRAME_SIZE * ARC_FRAME_SIZE;
    int32_t max_distance = 63;
    int32_t hit_slot = -1;
    for (int32_t ii = 1; ii < 64; ii++) {
        int32_t xs = origin_x + dx * ii, ys = origin_y + dy * ii;
        if (xs < 0 || xs >= ARC_FRAME_SIZE || ys < 0 || ys >= ARC_FRAME_SIZE) {
            max_distance = ii - 1;
            break;
        }
        int32_t hv = hit_grid[(size_t)ys * ARC_FRAME_SIZE + xs];
        if (hv >= 0) {
            max_distance = ii - 1;
            hit_slot = hv;
            break;
        }
    }
    max_distance = sc25_max(max_distance - ((dx > 0 || dy > 0) ? 2 : 0), 0);

    int32_t scale1_idx = aux->scale2 ? 1 : 0;
    int32_t hit_kind = hit_slot >= 0 ? st->fb_slot_kind[level * st->num_slots + hit_slot] : 0;

    int32_t rot = SC25_FIREBALL_ROTATION[facing];
    int32_t rot_k = sc25_rotk(rot);

    int32_t fslot = st->fireball_slot;
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int32_t base_true_h = st->fb_base_shape[scale1_idx * 2 + 0];
    int32_t base_true_w = st->fb_base_shape[scale1_idx * 2 + 1];
    const int8_t *base = st->fb_base + (size_t)scale1_idx * st->fb_base_h * st->fb_base_w;
    int8_t canvas[SC25_PATCH_CAP];
    sc25_rotate_pad(canvas, ph, pw, base, base_true_h, base_true_w, 0, rot_k, base_true_h,
                    base_true_w);
    memcpy(arc_sprite_pixels_mut(s, fslot), canvas, (size_t)ph * pw);
    s->alive[fslot] = 1;
    s->interaction[fslot] = INTANGIBLE;
    s->x[fslot] = px;
    s->y[fslot] = py;
    s->layer[fslot] = 2;

    aux->fb_active = 1;
    aux->fb_frame = 0;
    aux->fb_dx = dx;
    aux->fb_dy = dy;
    aux->fb_max_dist = max_distance;
    aux->fb_cur_dist = 0;
    aux->fb_shrinking = 0;
    aux->fb_consumed = 0;
    aux->fb_hit_kind = hit_kind;
    aux->fb_scale_idx = scale1_idx;
    aux->fb_rot_k = rot_k;
}

static void sc25_step_fireball(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                               int32_t *status, uint8_t *action_complete) {
    int32_t fslot = st->fireball_slot;
    int32_t frame = aux->fb_frame + 1;
    aux->fb_frame = frame;

    int32_t true_w = st->fb_base_shape[aux->fb_scale_idx * 2 + 1];
    int32_t width = true_w - aux->fb_consumed;
    int will_slice = aux->fb_shrinking && (width > 3);
    if (will_slice) aux->fb_consumed += 3;

    int mirror_lr = (aux->fb_dx == 0) && (frame % 2 == 0);
    int mirror_ud = (aux->fb_dx != 0) && (frame % 2 == 0);
    int8_t colour = (int8_t)((frame % 4) < 2 ? 7 : 6);

    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int8_t canvas[SC25_PATCH_CAP];
    sc25_fb_canvas(canvas, ph, pw, st, aux, colour, mirror_lr, mirror_ud);
    memcpy(arc_sprite_pixels_mut(s, fslot), canvas, (size_t)ph * pw);

    if (!aux->fb_shrinking) {
        s->x[fslot] += aux->fb_dx * 2;
        s->y[fslot] += aux->fb_dy * 2;
        int32_t cur = aux->fb_cur_dist + 2;
        aux->fb_cur_dist = cur;
        aux->fb_shrinking = (uint8_t)(cur >= aux->fb_max_dist);
    } else if (width > 3) {
        int32_t w_new = sc25_max(width - 3, 0);
        int need_reposition = (w_new <= 3) && (aux->fb_dx > 0 || aux->fb_dy > 0);
        if (need_reposition) {
            s->x[fslot] += aux->fb_dx * 3;
            s->y[fslot] += aux->fb_dy * 3;
        }
    } else {
        int32_t hit_kind = aux->fb_hit_kind;
        if (hit_kind == 1) {
            arc_remove_sprite(s, st->tagsmh[level]);
            int32_t g = st->dosorb[level];
            if (g >= 0) arc_remove_sprite(s, g);
        } else if (hit_kind == 2) {
            arc_remove_sprite(s, st->seofsw_tagsmh[level]);
            int32_t g = st->seofsw_dosorb[level];
            if (g >= 0) arc_remove_sprite(s, g);
        }
        arc_remove_sprite(s, fslot);
        aux->fb_active = 0;
        sc25_finish_effect(s, st, level, aux, status, action_complete);
    }
}

static void sc25_step_cast(ArcSprites *s, const Sc25Static *st, int32_t level, Sc25Aux *aux,
                           int32_t *status, uint8_t *action_complete) {
    int32_t frame = aux->cast_frame;
    int32_t spell = aux->cast_spell;
    int8_t colour = SC25_SPELL_COLOR[spell];
    int forward = (frame / 2) % 2 == 0;
    int8_t remapped[2][2];
    for (int32_t i = 0; i < 2; i++)
        for (int32_t j = 0; j < 2; j++) {
            int8_t v = aux->player_base[i][j];
            remapped[i][j] = forward ? (v == 9 ? colour : v) : (v == colour ? (int8_t)9 : v);
        }
    memcpy(aux->player_base, remapped, sizeof remapped);
    frame += 1;
    aux->cast_frame = frame;
    if (frame >= SC25_CAST_FRAMES) {
        int32_t sp = aux->cast_spell;
        aux->cast_active = 0;
        aux->hint = -1;
        if (sp == SC25_TELEPORT) sc25_start_teleport(s, st, level, aux, status, action_complete);
        else if (sp == SC25_RESIZE) sc25_start_resize(s, st, level, aux);
        else if (sp == SC25_FIREBALL) sc25_start_fireball(s, st, level, aux);
    }
}

static void sc25_display_to_grid(const ArcCamera *camera, int32_t display_x, int32_t display_y,
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

static void sc25_grid_cell(ArcSprites *s, const Sc25Static *st, int32_t level, int32_t hit,
                           Sc25Aux *aux, int32_t *status, uint8_t *action_complete) {
    int32_t r = st->click_r[level * st->num_slots + hit];
    int32_t c = st->click_c[level * st->num_slots + hit];
    if (r < 0) {
        *action_complete = 1;
        return;
    }
    aux->drawn[r][c] = !aux->drawn[r][c];
    sc25_recolour_grid(s, st, level, aux);
    aux->steps += 1;
    sc25_fill_bar(s, st, level, aux);
    int32_t budget = st->budget[level];
    int over = budget > 0 && aux->steps > budget;
    if (over) {
        *status = SC25_GAME_OVER;
        *action_complete = 1;
    } else if (!aux->cast_active) {
        *action_complete = 1;
    }
}

static void sc25_spell_select(ArcSprites *s, const Sc25Static *st, int32_t level, int32_t hit,
                              Sc25Aux *aux, uint8_t *action_complete) {
    int32_t spell = st->click_spell[level * st->num_slots + hit];
    if (spell < 0) {
        *action_complete = 1;
        return;
    }
    if (aux->hint == spell) {
        aux->demo_active = 1;
        aux->demo_frame = 0;
        aux->demo_spell = spell;
    } else {
        aux->hint = spell;
        sc25_recolour_grid(s, st, level, aux);
        *action_complete = 1;
    }
}

static void sc25_step_click(ArcSprites *s, const ArcCamera *camera, const Sc25Static *st, int32_t level,
                            int32_t action_x, int32_t action_y, Sc25Aux *aux, int32_t *status,
                            uint8_t *action_complete) {
    int32_t gx, gy, on_board;
    sc25_display_to_grid(camera, action_x, action_y, &gx, &gy, &on_board);
    int32_t hit = arc_get_sprite_at(s, gx, gy, -1, 0);
    int hit_valid = on_board && hit >= 0;
    int32_t kind = hit_valid ? st->click_kind[level * st->num_slots + hit] : 3;

    if (kind == 0 || kind == 1) sc25_grid_cell(s, st, level, hit, aux, status, action_complete);
    else if (kind == 2) sc25_spell_select(s, st, level, hit, aux, action_complete);
    else {
        aux->miss_active = 1;
        aux->miss_frame = 0;
    }
}

static void sc25_step_move(ArcSprites *s, const Sc25Static *st, int32_t level, int32_t action_id,
                           Sc25Aux *aux, int32_t *status, uint8_t *action_complete) {
    int32_t facing = aux->facing;
    switch (action_id) {
        case SC25_ACTION1: facing = 0; break;
        case SC25_ACTION2: facing = 1; break;
        case SC25_ACTION3: facing = 2; break;
        case SC25_ACTION4: facing = 3; break;
        default: break;
    }
    int32_t step = aux->scale2 ? 4 : 2;
    int32_t mdx = SC25_FACING_DELTA[facing][0] * step, mdy = SC25_FACING_DELTA[facing][1] * step;

    aux->facing = facing;
    aux->facing_set = 1;
    sc25_apply_player_sprite(s, st, level, aux);

    int32_t slot = st->pluyoo[level];
    int blocked = arc_try_move(s, slot, mdx, mdy);
    int still_blocked = blocked;
    if (blocked && aux->scale2 && (abs(mdx) + abs(mdy) == 4))
        still_blocked = arc_try_move(s, slot, mdx / 2, mdy / 2);

    sc25_collect_blockers(s, st, level, aux);

    int32_t exy = st->exydhv[level];
    int32_t slot_x = s->x[slot], slot_y = s->y[slot];
    int32_t pw_ = s->w[slot], ph_ = s->h[slot];
    int moved = !still_blocked;
    int overlap_exit = 0;
    int32_t sdx = 0, sdy = 0;
    if (moved && exy >= 0) {
        int32_t ex = s->x[exy], ey = s->y[exy], ew = s->w[exy], eh = s->h[exy];
        overlap_exit = (slot_x < ex + ew) && (slot_x + pw_ > ex) && (slot_y < ey + eh) &&
                      (slot_y + ph_ > ey);
        int32_t pcx = slot_x + pw_ / 2, pcy = slot_y + ph_ / 2;
        int32_t ecx = ex + ew / 2, ecy = ey + eh / 2;
        int32_t ddx = ecx - pcx, ddy = ecy - pcy;
        int use_x = abs(ddx) > abs(ddy);
        if (use_x) sdx = ddx > 0 ? step / 2 : -(step / 2);
        else sdy = ddy > 0 ? step / 2 : -(step / 2);
    }

    if (overlap_exit) {
        aux->slide_active = 1;
        aux->slide_dx = sdx;
        aux->slide_dy = sdy;
        aux->slide_frame = SC25_SLIDE_FRAMES;
    } else {
        sc25_spend_step(s, st, level, aux, status, action_complete);
    }
}

void sc25_step_once(ArcSprites *sprites, const ArcCamera *camera, const Sc25Static *st, int32_t level,
                    int32_t action_id, int32_t action_x, int32_t action_y, Sc25Aux *aux,
                    int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    int not_reset = action_id != SC25_ACTION_RESET;

    if (aux->demo0_pending && not_reset) {
        sc25_step_demo0(aux);
    } else if (aux->slide_active) {
        sc25_step_slide(sprites, st, level, aux, score, status, next_level, action_complete);
    } else if (aux->miss_active) {
        sc25_step_miss(sprites, st, level, aux, action_complete);
    } else if (aux->demo_active) {
        sc25_step_demo(sprites, st, level, aux, action_complete);
    } else if (aux->cast_active) {
        sc25_step_cast(sprites, st, level, aux, status, action_complete);
    } else if (aux->tp_active) {
        sc25_step_teleport(sprites, st, level, aux, status, action_complete);
    } else if (aux->resize_active) {
        sc25_step_resize(sprites, st, level, aux, status, action_complete);
    } else if (aux->fb_active) {
        sc25_step_fireball(sprites, st, level, aux, status, action_complete);
    } else if (action_id == SC25_ACTION6) {
        sc25_step_click(sprites, camera, st, level, action_x, action_y, aux, status,
                        action_complete);
    } else if (action_id >= SC25_ACTION1 && action_id <= SC25_ACTION4) {
        sc25_step_move(sprites, st, level, action_id, aux, status, action_complete);
    } else {
        *action_complete = 1;
    }
    sc25_apply_player_sprite(sprites, st, level, aux);
}

void sc25_render_interface(int8_t *frame, const ArcSprites *sprites, const ArcCamera *camera,
                           const Sc25Static *st, int32_t level, const Sc25Aux *aux) {
    (void)frame;
    (void)sprites;
    (void)camera;
    (void)st;
    (void)level;
    (void)aux;
}
