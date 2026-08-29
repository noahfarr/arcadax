#include "su15.h"

#include <stdlib.h>
#include <string.h>

enum {
    SU15_ACTION6 = 6,
    SU15_ACTION7 = 7,
    SU15_WIN = 2,
    SU15_GAME_OVER = 3,
    SU15_PHASE_IDLE = 0,
    SU15_PHASE_PULLING = 1,
    SU15_PHASE_MISMATCH_FLASH = 2,
    SU15_PHASE_WIN_FLASH = 3,
    SU15_PULL_FRAMES = 4,
    SU15_WOBBLE_FRAMES = 4,
    SU15_SWALLOW_TOTAL_FRAMES = 8,
    SU15_SWALLOW_DYING_FRAMES = 4,
    SU15_EAT_LOCK_FRAMES = 9,
    SU15_CLICK_RADIUS = 8,
    SU15_PULL_STEP = 4,
    SU15_CLICK_Y_MIN = 10,
    SU15_CLICK_Y_MAX = 63,
    SU15_BOARD_SIZE = 64,
    SU15_COLOR_EMPTY = 0,
    SU15_COLOR_NOT_YET = 2,
    SU15_GLOW_MARGIN = 12,
    SU15_MAX_PATCH_AREA = 16384,
    SU15_MAX_CAND = SU15_MAX_BLOBS,
};

static const int32_t su15_wobble_offset[4] = {0, -1, 0, 1};

static int32_t su15_clampi(int32_t v, int32_t lo, int32_t hi) {
    v = v > lo ? v : lo;
    v = v < hi ? v : hi;
    return v;
}

static int32_t su15_floor_div(int32_t a, int32_t b) {
    int32_t q = a / b;
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q -= 1;
    return q;
}

static int su15_overlap(const ArcSprites *s, int32_t a, int32_t b) {
    return s->x[a] < s->x[b] + s->w[b] && s->x[b] < s->x[a] + s->w[a] &&
          s->y[a] < s->y[b] + s->h[b] && s->y[b] < s->y[a] + s->h[a];
}

static int su15_within_radius(int32_t bx, int32_t by, int32_t w, int32_t h, int32_t x, int32_t y,
                              int32_t radius) {
    int32_t cx = su15_clampi(x, bx, bx + w - 1);
    int32_t cy = su15_clampi(y, by, by + h - 1);
    int32_t dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

static void su15_clamp_position(int32_t x, int32_t y, int32_t w, int32_t h, int32_t *ox,
                                int32_t *oy) {
    *ox = su15_clampi(x, 0, SU15_BOARD_SIZE - w);
    int32_t yhi = SU15_CLICK_Y_MAX < SU15_BOARD_SIZE - h ? SU15_CLICK_Y_MAX : SU15_BOARD_SIZE - h;
    *oy = su15_clampi(y, SU15_CLICK_Y_MIN, yhi);
}

static int32_t su15_advance_axis(int32_t pos, int32_t floor_c, int32_t round_c, int tie) {
    int32_t base = pos + floor_c;
    int32_t tie_delta = (base % 2 == 0) ? floor_c : floor_c + 1;
    int32_t delta = tie ? tie_delta : round_c;
    return pos + delta;
}

static void su15_display_to_grid(const ArcCamera *camera, int32_t display_x, int32_t display_y,
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

static void su15_spawn_pixels_one(const Su15Static *st, int32_t tier, int32_t fruit_kind,
                                  int8_t *out_pixels, int32_t *out_w, int32_t *out_h,
                                  int32_t *out_layer) {
    int32_t area = st->ph * st->pw;
    if (tier >= 0) {
        int32_t t = su15_clampi(tier, 0, SU15_NUM_TIERS - 1);
        memcpy(out_pixels, st->tier_pixels + (size_t)t * area, (size_t)area);
        *out_w = st->tier_w[t];
        *out_h = st->tier_h[t];
        *out_layer = st->tier_layer[t];
    } else {
        int32_t f = su15_clampi(fruit_kind, 0, SU15_NUM_FRUIT_KINDS - 1);
        memcpy(out_pixels, st->fruit_pixels + (size_t)f * area, (size_t)area);
        *out_w = st->fruit_w[f];
        *out_h = st->fruit_h[f];
        *out_layer = st->fruit_layer[f];
    }
}

static void su15_pull_lookup(const Su15Static *st, int32_t dx, int32_t dy, int32_t *floor_x,
                             int32_t *round_x, int *tie_x, int32_t *floor_y, int32_t *round_y,
                             int *tie_y) {
    int32_t bound = SU15_DELTA_BOUND;
    int32_t ix = su15_clampi(dx, -bound, bound) + bound;
    int32_t iy = su15_clampi(dy, -bound, bound) + bound;
    size_t idx = (size_t)ix * SU15_DELTA_SIZE + (size_t)iy;
    *floor_x = st->pull_floor_x[idx];
    *round_x = st->pull_round_x[idx];
    *tie_x = st->pull_tie_x[idx];
    *floor_y = st->pull_floor_y[idx];
    *round_y = st->pull_round_y[idx];
    *tie_y = st->pull_tie_y[idx];
}

static void su15_clear_glow(ArcSprites *s, const Su15Static *st) {
    int8_t *patch = arc_sprite_pixels_mut(s, st->glow_slot);
    memset(patch, -1, (size_t)(st->ph * st->pw));
}

static void su15_draw_glow(ArcSprites *s, const Su15Static *st, const Su15Aux *aux) {
    int32_t idx = su15_clampi(aux->pull_frame, 0, SU15_GLOW_STATES - 1);
    int8_t *patch = arc_sprite_pixels_mut(s, st->glow_slot);
    memset(patch, -1, (size_t)(st->ph * st->pw));
    const int8_t *pattern = st->glow_states + (size_t)idx * SU15_GLOW_SIZE * SU15_GLOW_SIZE;
    for (int32_t row = 0; row < SU15_GLOW_SIZE; row++)
        memcpy(patch + (size_t)row * st->pw, pattern + (size_t)row * SU15_GLOW_SIZE, SU15_GLOW_SIZE);
    int32_t slot = st->glow_slot;
    s->x[slot] = aux->click_x - SU15_GLOW_MARGIN;
    s->y[slot] = aux->click_y - SU15_GLOW_MARGIN;
    s->w[slot] = SU15_GLOW_SIZE;
    s->h[slot] = SU15_GLOW_SIZE;
}

static void su15_apply_flash_color(ArcSprites *s, const Su15Static *st, const Su15Aux *aux, int lit) {
    int32_t area = st->ph * st->pw;
    int32_t pair[2] = {aux->flash_pair_a, aux->flash_pair_b};
    for (int k = 0; k < 2; k++) {
        int32_t slot = pair[k];
        if (slot < 0) continue;
        int8_t pix[SU15_MAX_PATCH_AREA];
        int32_t w, h, layer;
        su15_spawn_pixels_one(st, aux->tier[slot], aux->fruit_kind[slot], pix, &w, &h, &layer);
        int8_t *dst = arc_sprite_pixels_mut(s, slot);
        if (lit) {
            for (int32_t p = 0; p < area; p++) dst[p] = pix[p] >= 0 ? (int8_t)SU15_COLOR_EMPTY : pix[p];
        } else {
            memcpy(dst, pix, (size_t)area);
        }
    }
}

static void su15_begin_mismatch_flash(ArcSprites *s, const Su15Static *st, Su15Aux *aux, int32_t a,
                                      int32_t b) {
    su15_clear_glow(s, st);
    aux->phase = SU15_PHASE_MISMATCH_FLASH;
    aux->win_flash_frame = 0;
    aux->flash_pair_a = a;
    aux->flash_pair_b = b;
    su15_apply_flash_color(s, st, aux, 1);
}

static void su15_merge_and_flash(ArcSprites *s, const Su15Static *st, Su15Aux *aux,
                                 int32_t *next_order, const int32_t *cand, int32_t m,
                                 int is_blob) {
    if (m < 2) return;
    int32_t *key = is_blob ? aux->tier : aux->fruit_kind;

    int32_t cx[SU15_MAX_CAND], cy[SU15_MAX_CAND];
    for (int32_t k = 0; k < m; k++) {
        int32_t slot = cand[k];
        cx[k] = s->x[slot] + s->w[slot] / 2;
        cy[k] = s->y[slot] + s->h[slot] / 2;
    }

    uint8_t adj[SU15_MAX_CAND][SU15_MAX_CAND];
    for (int32_t k = 0; k < m; k++) {
        for (int32_t j = 0; j < m; j++) {
            if (j == k) { adj[k][j] = 0; continue; }
            adj[k][j] = (uint8_t)(su15_overlap(s, cand[k], cand[j]) && key[cand[k]] == key[cand[j]]);
        }
    }

    int32_t perm[SU15_MAX_CAND];
    for (int32_t k = 0; k < m; k++) perm[k] = k;
    for (int32_t a = 1; a < m; a++) {
        int32_t v = perm[a];
        int32_t okey = s->order[cand[v]];
        int32_t b = a - 1;
        while (b >= 0 && s->order[cand[perm[b]]] > okey) {
            perm[b + 1] = perm[b];
            b--;
        }
        perm[b + 1] = v;
    }

    int32_t fi = -1, fj = -1;
    for (int32_t ri = 0; ri < m && fi < 0; ri++) {
        for (int32_t rj = ri + 1; rj < m; rj++) {
            int32_t a = cand[perm[ri]], b = cand[perm[rj]];
            if (su15_overlap(s, a, b) && key[a] != key[b]) {
                fi = a;
                fj = b;
                break;
            }
        }
    }

    if (fi >= 0) {
        su15_begin_mismatch_flash(s, st, aux, fi, fj);
        return;
    }

    int32_t label[SU15_MAX_CAND], prev[SU15_MAX_CAND];
    for (int32_t k = 0; k < m; k++) label[k] = cand[k];

    for (int32_t iter = 0; iter < m; iter++) {
        memcpy(prev, label, (size_t)m * sizeof(int32_t));
        int changed = 0;
        for (int32_t k = 0; k < m; k++) {
            int32_t best = prev[k];
            for (int32_t j = 0; j < m; j++) {
                if (adj[k][j] && prev[j] < best) best = prev[j];
            }
            if (best != label[k]) {
                label[k] = best;
                changed = 1;
            }
        }
        if (!changed) break;
    }

    int32_t group_count[SU15_MAX_CAND], avg_cx[SU15_MAX_CAND], avg_cy[SU15_MAX_CAND];
    for (int32_t k = 0; k < m; k++) {
        int32_t count = 0, sumx = 0, sumy = 0;
        for (int32_t j = 0; j < m; j++) {
            if (label[j] == label[k]) {
                count++;
                sumx += cx[j];
                sumy += cy[j];
            }
        }
        group_count[k] = count;
        int32_t safe = count > 1 ? count : 1;
        avg_cx[k] = sumx / safe;
        avg_cy[k] = sumy / safe;
    }

    int32_t max_key = is_blob ? SU15_NUM_TIERS - 1 : SU15_NUM_FRUIT_KINDS - 1;
    int any_member = 0;
    int32_t old_next_order = *next_order;

    for (int32_t k = 0; k < m; k++) {
        int32_t slot = cand[k];
        int is_member = group_count[k] >= 2;
        if (is_member) any_member = 1;
        int is_survivor = is_member && cand[k] == label[k];
        int at_max = key[slot] >= max_key;
        int keep_as_new = is_survivor && !at_max;
        int removed = is_member && !keep_as_new;

        if (keep_as_new) {
            int32_t new_key = key[slot] + 1;
            int8_t pix[SU15_MAX_PATCH_AREA];
            int32_t w, h, layer;
            if (is_blob) su15_spawn_pixels_one(st, new_key, -1, pix, &w, &h, &layer);
            else su15_spawn_pixels_one(st, -1, new_key, pix, &w, &h, &layer);
            memcpy(arc_sprite_pixels_mut(s, slot), pix, (size_t)(st->ph * st->pw));
            s->w[slot] = w;
            s->h[slot] = h;
            s->layer[slot] = layer;
            int32_t nx, ny;
            su15_clamp_position(avg_cx[k] - w / 2, avg_cy[k] - h / 2, w, h, &nx, &ny);
            s->x[slot] = nx;
            s->y[slot] = ny;
            s->order[slot] = old_next_order + slot;
            if (is_blob) {
                aux->tier[slot] = new_key;
            } else {
                aux->fruit_kind[slot] = new_key;
                aux->eat_lock[slot] = SU15_PULL_FRAMES;
            }
            aux->pulled[slot] = 0;
            aux->near_click[slot] = 0;
            aux->has_pull_vector[slot] = 0;
        } else if (removed) {
            s->alive[slot] = 0;
            s->interaction[slot] = REMOVED;
            if (is_blob) aux->tier[slot] = -1;
            else aux->fruit_kind[slot] = -1;
        }
    }
    if (any_member) *next_order = old_next_order + st->num_slots;
}

static void su15_check_fruit_merge(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                                   int32_t *next_order) {
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;
    int32_t cand[SU15_MAX_CAND], m = 0;
    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        int32_t slot = fruits[k];
        if (slot >= 0 && s->alive[slot]) cand[m++] = slot;
    }
    if (m >= 2) su15_merge_and_flash(s, st, aux, next_order, cand, m, 0);
}

static void su15_check_blob_merge(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                                  int32_t *next_order) {
    su15_check_fruit_merge(s, st, level, aux, next_order);
    if (aux->phase == SU15_PHASE_MISMATCH_FLASH) return;
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    int32_t cand[SU15_MAX_CAND], m = 0;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t slot = blobs[k];
        if (slot >= 0 && aux->pulled[slot] && !aux->consuming[slot] && s->alive[slot]) cand[m++] = slot;
    }
    if (m >= 2) su15_merge_and_flash(s, st, aux, next_order, cand, m, 1);
}

static void su15_check_blob_merge_from_settle(ArcSprites *s, const Su15Static *st, int32_t level,
                                              Su15Aux *aux, int32_t *next_order) {
    su15_check_fruit_merge(s, st, level, aux, next_order);
}

static int su15_any_blob_in_move_phase(const Su15Static *st, int32_t level, const Su15Aux *aux) {
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t slot = blobs[k];
        if (slot < 0) continue;
        if (!aux->consuming[slot] || aux->consume_dying[slot]) continue;
        int32_t f = aux->consume_frame[slot];
        if (f >= SU15_WOBBLE_FRAMES && f < SU15_SWALLOW_TOTAL_FRAMES) return 1;
    }
    return 0;
}

static void su15_trigger_swallow(ArcSprites *s, Su15Aux *aux, int32_t fruit_slot, int32_t blob_slot) {
    if (aux->consuming[blob_slot]) return;
    int32_t tier = aux->tier[blob_slot];
    int dying = tier <= 0;
    int32_t target_tier = dying ? -1 : tier - 1;
    int32_t bcx = s->x[blob_slot] + s->w[blob_slot] / 2, bcy = s->y[blob_slot] + s->h[blob_slot] / 2;
    int32_t fcx = s->x[fruit_slot] + s->w[fruit_slot] / 2, fcy = s->y[fruit_slot] + s->h[fruit_slot] / 2;
    int32_t dx = bcx - fcx, dy = bcy - fcy;
    if (dx == 0 && dy == 0) { dx = 0; dy = -1; }
    int32_t bx = s->x[blob_slot], by = s->y[blob_slot];
    aux->consuming[blob_slot] = 1;
    aux->consume_frame[blob_slot] = 0;
    aux->consume_start_x[blob_slot] = bx;
    aux->consume_start_y[blob_slot] = by;
    aux->consume_dx[blob_slot] = dx;
    aux->consume_dy[blob_slot] = dy;
    aux->consume_pos_x[blob_slot] = bx;
    aux->consume_pos_y[blob_slot] = by;
    aux->consume_dying[blob_slot] = (uint8_t)dying;
    aux->demoted_tier[blob_slot] = target_tier;
    aux->eat_lock[fruit_slot] = SU15_EAT_LOCK_FRAMES;
}

static void su15_swallow_scan(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux) {
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;

    uint8_t eligible_fruit[SU15_MAX_FRUITS];
    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        int32_t f = fruits[k];
        eligible_fruit[k] =
            (uint8_t)(f >= 0 && s->alive[f] && !aux->near_click[f] && aux->eat_lock[f] <= 0);
    }
    uint8_t eligible_blob[SU15_MAX_BLOBS];
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        eligible_blob[k] = (uint8_t)(b >= 0 && s->alive[b] && !aux->consuming[b]);
    }

    for (int32_t fi = 0; fi < SU15_MAX_FRUITS; fi++) {
        if (!eligible_fruit[fi]) continue;
        int32_t f = fruits[fi];
        for (int32_t bi = 0; bi < SU15_MAX_BLOBS; bi++) {
            if (!eligible_blob[bi]) continue;
            int32_t b = blobs[bi];
            if (aux->consuming[b]) continue;
            if (su15_overlap(s, f, b)) su15_trigger_swallow(s, aux, f, b);
        }
    }
}

static void su15_move_fruits_toward_target(ArcSprites *s, const Su15Static *st, int32_t level,
                                           Su15Aux *aux) {
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    int32_t avail[SU15_MAX_BLOBS], navail = 0;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        if (b >= 0 && s->alive[b] && !aux->consuming[b]) avail[navail++] = b;
    }
    if (navail == 0) return;
    for (int32_t fi = 0; fi < SU15_MAX_FRUITS; fi++) {
        int32_t f = fruits[fi];
        if (f < 0 || !s->alive[f] || aux->near_click[f] || aux->eat_lock[f] > 0) continue;
        int32_t fcx = s->x[f] + s->w[f] / 2, fcy = s->y[f] + s->h[f] / 2;
        int32_t best = avail[0];
        int64_t bestd = -1;
        for (int32_t k = 0; k < navail; k++) {
            int32_t b = avail[k];
            int32_t bcx = s->x[b] + s->w[b] / 2, bcy = s->y[b] + s->h[b] / 2;
            int64_t dx = fcx - bcx, dy = fcy - bcy;
            int64_t d2 = dx * dx + dy * dy;
            if (bestd < 0 || d2 < bestd) { bestd = d2; best = b; }
        }
        int32_t bcx = s->x[best] + s->w[best] / 2, bcy = s->y[best] + s->h[best] / 2;
        int32_t speed = aux->fruit_kind[f] == 2 ? 2 : 1;
        int32_t stepx = bcx > fcx ? speed : (bcx < fcx ? -speed : 0);
        int32_t stepy = bcy > fcy ? speed : (bcy < fcy ? -speed : 0);
        int32_t nx, ny;
        su15_clamp_position(s->x[f] + stepx, s->y[f] + stepy, s->w[f], s->h[f], &nx, &ny);
        s->x[f] = nx;
        s->y[f] = ny;
    }
}

static void su15_advance_swallow_animations(ArcSprites *s, const Su15Static *st, int32_t level,
                                            Su15Aux *aux) {
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t i = blobs[k];
        if (i < 0 || !aux->consuming[i]) continue;
        int dying = aux->consume_dying[i];
        int32_t total = dying ? SU15_SWALLOW_DYING_FRAMES : SU15_SWALLOW_TOTAL_FRAMES;
        int32_t frame = aux->consume_frame[i];
        int in_wobble = frame < SU15_WOBBLE_FRAMES;
        int promote_now = !dying && frame == SU15_WOBBLE_FRAMES;

        if (promote_now) {
            aux->tier[i] = aux->demoted_tier[i];
            int8_t pix[SU15_MAX_PATCH_AREA];
            int32_t w, h, layer;
            su15_spawn_pixels_one(st, aux->tier[i], aux->fruit_kind[i], pix, &w, &h, &layer);
            memcpy(arc_sprite_pixels_mut(s, i), pix, (size_t)(st->ph * st->pw));
            s->w[i] = w;
            s->h[i] = h;
        }

        int32_t floor_x, round_x, floor_y, round_y;
        int tie_x, tie_y;
        int32_t bound = SU15_DELTA_BOUND;
        int32_t ix = su15_clampi(aux->consume_dx[i], -bound, bound) + bound;
        int32_t iy = su15_clampi(aux->consume_dy[i], -bound, bound) + bound;
        int32_t mult_idx = su15_clampi(frame - SU15_WOBBLE_FRAMES, 0, 3);
        size_t tbl = ((size_t)mult_idx * SU15_DELTA_SIZE + (size_t)ix) * SU15_DELTA_SIZE + (size_t)iy;
        floor_x = st->swallow_floor_x[tbl];
        round_x = st->swallow_round_x[tbl];
        tie_x = st->swallow_tie_x[tbl];
        floor_y = st->swallow_floor_y[tbl];
        round_y = st->swallow_round_y[tbl];
        tie_y = st->swallow_tie_y[tbl];

        int32_t moved_x = su15_advance_axis(aux->consume_start_x[i], floor_x, round_x, tie_x);
        int32_t moved_y = su15_advance_axis(aux->consume_start_y[i], floor_y, round_y, tie_y);
        if (!in_wobble) {
            aux->consume_pos_x[i] = moved_x;
            aux->consume_pos_y[i] = moved_y;
        }

        int32_t raw_x = in_wobble ? aux->consume_start_x[i] : aux->consume_pos_x[i];
        int32_t raw_y =
            in_wobble ? aux->consume_start_y[i] + su15_wobble_offset[frame % 4] : aux->consume_pos_y[i];
        int32_t nx, ny;
        su15_clamp_position(raw_x, raw_y, s->w[i], s->h[i], &nx, &ny);
        s->x[i] = nx;
        s->y[i] = ny;

        int32_t next_frame = frame + 1;
        int finished = next_frame >= total;
        aux->consume_frame[i] = next_frame;
        if (finished) {
            if (dying) {
                s->alive[i] = 0;
                s->interaction[i] = REMOVED;
                aux->tier[i] = -1;
            }
            aux->consuming[i] = 0;
        }
    }
}

static void su15_settle_fruits(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                               int32_t *next_order) {
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;

    int any_consuming = 0;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        if (b >= 0 && aux->consuming[b]) { any_consuming = 1; break; }
    }
    if (any_consuming) su15_advance_swallow_animations(s, st, level, aux);

    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        int32_t f = fruits[k];
        if (f >= 0 && aux->eat_lock[f] > 0) aux->eat_lock[f]--;
    }

    int any_fruit_exists = 0;
    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        if (fruits[k] >= 0) { any_fruit_exists = 1; break; }
    }
    int any_blob_alive = 0;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        if (b >= 0 && s->alive[b]) { any_blob_alive = 1; break; }
    }
    int frozen = su15_any_blob_in_move_phase(st, level, aux);
    int gate = aux->pull_frame < SU15_PULL_FRAMES && any_fruit_exists && any_blob_alive && !frozen;
    if (gate) {
        su15_move_fruits_toward_target(s, st, level, aux);
        su15_swallow_scan(s, st, level, aux);
        su15_check_blob_merge_from_settle(s, st, level, aux, next_order);
    }
}

static void su15_pull_move_one(ArcSprites *s, const Su15Static *st, const Su15Aux *aux, int32_t i) {
    int32_t cx = s->x[i] + s->w[i] / 2, cy = s->y[i] + s->h[i] / 2;
    int32_t ddx = su15_clampi(aux->click_x - cx, -SU15_PULL_STEP, SU15_PULL_STEP);
    int32_t ddy = su15_clampi(aux->click_y - cy, -SU15_PULL_STEP, SU15_PULL_STEP);
    int32_t greedy_x = s->x[i] + ddx, greedy_y = s->y[i] + ddy;

    int32_t fx, rx, fy, ry;
    int tx, ty;
    su15_pull_lookup(st, aux->pull_dx[i], aux->pull_dy[i], &fx, &rx, &tx, &fy, &ry, &ty);
    int32_t vec_x = su15_advance_axis(s->x[i], fx, rx, tx);
    int32_t vec_y = su15_advance_axis(s->y[i], fy, ry, ty);

    int32_t raw_x = aux->has_pull_vector[i] ? vec_x : greedy_x;
    int32_t raw_y = aux->has_pull_vector[i] ? vec_y : greedy_y;
    int32_t nx, ny;
    su15_clamp_position(raw_x, raw_y, s->w[i], s->h[i], &nx, &ny);
    s->x[i] = nx;
    s->y[i] = ny;
}

static void su15_pull_step(ArcSprites *s, const Su15Static *st, int32_t level, const Su15Aux *aux) {
    int frozen = su15_any_blob_in_move_phase(st, level, aux);
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;

    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t i = blobs[k];
        if (i < 0) continue;
        if (aux->pulled[i] && !aux->consuming[i]) su15_pull_move_one(s, st, aux, i);
    }
    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        int32_t i = fruits[k];
        if (i < 0) continue;
        if (aux->near_click[i] && !frozen) su15_pull_move_one(s, st, aux, i);
    }
}

static int su15_pkrdtzfrth(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                           int32_t *next_order) {
    su15_pull_step(s, st, level, aux);
    su15_settle_fruits(s, st, level, aux, next_order);

    if (aux->phase == SU15_PHASE_MISMATCH_FLASH) {
        su15_clear_glow(s, st);
        return 1;
    }

    int32_t next_frame = aux->pull_frame + 1;
    aux->pull_frame = next_frame;
    if (next_frame < SU15_PULL_FRAMES) {
        su15_draw_glow(s, st, aux);
        return 1;
    }

    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    int any_consuming = 0;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        if (b >= 0 && aux->consuming[b]) { any_consuming = 1; break; }
    }
    su15_clear_glow(s, st);
    return any_consuming;
}

static int su15_win_check_point(const ArcSprites *s, const Su15Static *st, int32_t level,
                                const Su15Aux *aux) {
    const int32_t *zones = st->zone_b_slot + (size_t)level * SU15_MAX_ZONES;
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;

    int32_t tier_counts[SU15_NUM_TIERS] = {0};
    int32_t kind_counts[SU15_NUM_FRUIT_KINDS] = {0};

    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        if (b < 0 || !s->alive[b]) continue;
        int32_t cx = s->x[b] + s->w[b] / 2, cy = s->y[b] + s->h[b] / 2;
        int in_zone = 0;
        for (int32_t z = 0; z < SU15_MAX_ZONES; z++) {
            int32_t zi = zones[z];
            if (zi < 0) continue;
            if (cx >= s->x[zi] && cx < s->x[zi] + s->w[zi] && cy >= s->y[zi] && cy < s->y[zi] + s->h[zi]) {
                in_zone = 1;
                break;
            }
        }
        if (!in_zone) continue;
        int32_t t = aux->tier[b];
        if (t >= 0 && t < SU15_NUM_TIERS) tier_counts[t]++;
    }
    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        int32_t f = fruits[k];
        if (f < 0 || !s->alive[f]) continue;
        int32_t cx = s->x[f] + s->w[f] / 2, cy = s->y[f] + s->h[f] / 2;
        int in_zone = 0;
        for (int32_t z = 0; z < SU15_MAX_ZONES; z++) {
            int32_t zi = zones[z];
            if (zi < 0) continue;
            if (cx >= s->x[zi] && cx < s->x[zi] + s->w[zi] && cy >= s->y[zi] && cy < s->y[zi] + s->h[zi]) {
                in_zone = 1;
                break;
            }
        }
        if (!in_zone) continue;
        int32_t kd = aux->fruit_kind[f];
        if (kd >= 0 && kd < SU15_NUM_FRUIT_KINDS) kind_counts[kd]++;
    }

    const int32_t *wtype = st->win_type + (size_t)level * SU15_MAX_WIN_TERMS;
    const int32_t *wvalue = st->win_value + (size_t)level * SU15_MAX_WIN_TERMS;
    const int32_t *wcount = st->win_count + (size_t)level * SU15_MAX_WIN_TERMS;
    for (int32_t k = 0; k < SU15_MAX_WIN_TERMS; k++) {
        if (wcount[k] < 0) continue;
        int32_t actual = wtype[k] == 0 ? tier_counts[wvalue[k]] : kind_counts[wvalue[k]];
        if (actual != wcount[k]) return 0;
    }
    return 1;
}

static int su15_win_check_weighted(const ArcSprites *s, const Su15Static *st, int32_t level,
                                   const Su15Aux *aux) {
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    int32_t actual_sum = 0;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        if (b < 0 || !s->alive[b]) continue;
        int32_t t = aux->tier[b];
        int32_t sh = t > 0 ? t : 0;
        actual_sum += (int32_t)1 << sh;
    }
    const int32_t *wtype = st->win_type + (size_t)level * SU15_MAX_WIN_TERMS;
    const int32_t *wvalue = st->win_value + (size_t)level * SU15_MAX_WIN_TERMS;
    const int32_t *wcount = st->win_count + (size_t)level * SU15_MAX_WIN_TERMS;
    int32_t needed = 0;
    for (int32_t k = 0; k < SU15_MAX_WIN_TERMS; k++) {
        if (wcount[k] >= 0 && wtype[k] == 0) needed += ((int32_t)1 << wvalue[k]) * wcount[k];
    }
    return actual_sum >= needed;
}

static void su15_paint_zone_pieces(ArcSprites *s, const Su15Static *st, int32_t level, int32_t color) {
    const int32_t *za = st->zone_a_slot + (size_t)level * SU15_MAX_ZONES;
    const int32_t *zb = st->zone_b_slot + (size_t)level * SU15_MAX_ZONES;
    for (int32_t k = 0; k < SU15_MAX_ZONES; k++) {
        int32_t z = za[k];
        if (z >= 0) arc_color_remap(s, z, 0, 0, (int8_t)color);
    }
    for (int32_t k = 0; k < SU15_MAX_ZONES; k++) {
        int32_t z = zb[k];
        if (z >= 0) arc_color_remap(s, z, 0, 0, (int8_t)color);
    }
}

static void su15_restore_zone_pixels(ArcSprites *s, const Su15Static *st, int32_t level) {
    int32_t area = st->ph * st->pw;
    const int32_t *za = st->zone_a_slot + (size_t)level * SU15_MAX_ZONES;
    const int32_t *zb = st->zone_b_slot + (size_t)level * SU15_MAX_ZONES;
    for (int32_t k = 0; k < SU15_MAX_ZONES; k++) {
        int32_t z = za[k];
        if (z < 0) continue;
        memcpy(arc_sprite_pixels_mut(s, z), st->level_pixels + ((size_t)level * st->num_slots + z) * area,
              (size_t)area);
    }
    for (int32_t k = 0; k < SU15_MAX_ZONES; k++) {
        int32_t z = zb[k];
        if (z < 0) continue;
        memcpy(arc_sprite_pixels_mut(s, z), st->level_pixels + ((size_t)level * st->num_slots + z) * area,
              (size_t)area);
    }
}

static void su15_push_undo(ArcSprites *s, const Su15Static *st, Su15Aux *aux) {
    int32_t n = st->num_slots;
    int32_t idx = su15_clampi(aux->undo_top, 0, SU15_MAX_UNDO - 1);
    int32_t *ut = aux->undo_tier + (size_t)idx * n;
    int32_t *uf = aux->undo_fruit_kind + (size_t)idx * n;
    int32_t *ux = aux->undo_x + (size_t)idx * n;
    int32_t *uy = aux->undo_y + (size_t)idx * n;
    uint8_t *ua = aux->undo_alive + (size_t)idx * n;
    int32_t *uo = aux->undo_order + (size_t)idx * n;
    for (int32_t i = 0; i < n; i++) {
        int is_piece = aux->tier[i] >= 0 || aux->fruit_kind[i] >= 0;
        int alive_mask = s->alive[i] && is_piece;
        ut[i] = alive_mask ? aux->tier[i] : -1;
        uf[i] = alive_mask ? aux->fruit_kind[i] : -1;
        ux[i] = s->x[i];
        uy[i] = s->y[i];
        ua[i] = (uint8_t)alive_mask;
        uo[i] = s->order[i];
    }
    int32_t top = aux->undo_top + 1;
    aux->undo_top = top > SU15_MAX_UNDO - 1 ? SU15_MAX_UNDO - 1 : top;
}

static void su15_pop_undo(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                          int32_t *next_order) {
    if (aux->undo_top <= 0) return;
    int32_t n = st->num_slots;
    int32_t top = aux->undo_top - 1;
    const int32_t *ut = aux->undo_tier + (size_t)top * n;
    const int32_t *uf = aux->undo_fruit_kind + (size_t)top * n;
    const int32_t *ux = aux->undo_x + (size_t)top * n;
    const int32_t *uy = aux->undo_y + (size_t)top * n;
    const uint8_t *ua = aux->undo_alive + (size_t)top * n;
    const int32_t *uo = aux->undo_order + (size_t)top * n;

    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;

    int32_t bslot[SU15_MAX_BLOBS], nb = 0;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t sl = blobs[k];
        if (sl >= 0) bslot[nb++] = sl;
    }
    int32_t fslot[SU15_MAX_FRUITS], nf = 0;
    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        int32_t sl = fruits[k];
        if (sl >= 0) fslot[nf++] = sl;
    }

    int32_t num_blobs_alive = 0;
    for (int32_t k = 0; k < nb; k++) if (ua[bslot[k]]) num_blobs_alive++;
    int32_t num_fruits_alive = 0;
    for (int32_t k = 0; k < nf; k++) if (ua[fslot[k]]) num_fruits_alive++;

    int32_t old_next_order = *next_order;

    for (int32_t k = 0; k < nb; k++) {
        int32_t slot = bslot[k];
        int32_t rank;
        if (ua[slot]) {
            int32_t r = 0;
            for (int32_t m = 0; m < nb; m++)
                if (ua[bslot[m]] && uo[bslot[m]] < uo[slot]) r++;
            rank = r;
        } else {
            int32_t before = 0;
            for (int32_t m = 0; m < nb; m++)
                if (ua[bslot[m]] && bslot[m] < slot) before++;
            rank = num_blobs_alive + slot - before;
        }
        int8_t pix[SU15_MAX_PATCH_AREA];
        int32_t w, h, layer;
        su15_spawn_pixels_one(st, ut[slot], uf[slot], pix, &w, &h, &layer);
        memcpy(arc_sprite_pixels_mut(s, slot), pix, (size_t)(st->ph * st->pw));
        s->w[slot] = w;
        s->h[slot] = h;
        s->layer[slot] = layer;
        s->x[slot] = ux[slot];
        s->y[slot] = uy[slot];
        s->order[slot] = old_next_order + rank;
        s->alive[slot] = ua[slot];
        s->interaction[slot] = ua[slot] ? TANGIBLE : REMOVED;
        aux->tier[slot] = ut[slot];
        aux->fruit_kind[slot] = uf[slot];
    }
    for (int32_t k = 0; k < nf; k++) {
        int32_t slot = fslot[k];
        int32_t rank;
        if (ua[slot]) {
            int32_t r = 0;
            for (int32_t m = 0; m < nf; m++)
                if (ua[fslot[m]] && uo[fslot[m]] < uo[slot]) r++;
            rank = r;
        } else {
            int32_t before = 0;
            for (int32_t m = 0; m < nf; m++)
                if (ua[fslot[m]] && fslot[m] < slot) before++;
            rank = num_fruits_alive + slot - before;
        }
        int8_t pix[SU15_MAX_PATCH_AREA];
        int32_t w, h, layer;
        su15_spawn_pixels_one(st, ut[slot], uf[slot], pix, &w, &h, &layer);
        memcpy(arc_sprite_pixels_mut(s, slot), pix, (size_t)(st->ph * st->pw));
        s->w[slot] = w;
        s->h[slot] = h;
        s->layer[slot] = layer;
        s->x[slot] = ux[slot];
        s->y[slot] = uy[slot];
        s->order[slot] = old_next_order + num_blobs_alive + rank;
        s->alive[slot] = ua[slot];
        s->interaction[slot] = ua[slot] ? TANGIBLE : REMOVED;
        aux->tier[slot] = ut[slot];
        aux->fruit_kind[slot] = uf[slot];
    }
    *next_order = old_next_order + n;
    aux->undo_top = top;

    int should_clear = aux->not_yet_flash && su15_win_check_weighted(s, st, level, aux);
    if (should_clear) {
        su15_restore_zone_pixels(s, st, level);
        aux->not_yet_flash = 0;
    }
}

static void su15_handle_tutorial_hide_if_moved(ArcSprites *s, const Su15Static *st, int32_t level,
                                               Su15Aux *aux) {
    if (level != 0) return;
    int32_t first_blob = st->first_blob_slot[level];
    int moved = s->x[first_blob] != 3 || s->y[first_blob] != 58;
    int32_t tslot = st->tutorial_slot[level];
    int should_hide = moved && tslot >= 0 && !aux->tutorial_hidden;
    if (should_hide) {
        s->x[tslot] = 500;
        aux->tutorial_hidden = 1;
    }
}

static void su15_hide_tutorial_arrow_if_clicked(ArcSprites *s, const Su15Static *st, int32_t level,
                                                int32_t wx, int32_t wy) {
    if (level != 0) return;
    int32_t slot = st->tutorial_slot[level];
    if (slot < 0) return;
    int hit = wx >= s->x[slot] && wx < s->x[slot] + s->w[slot] && wy >= s->y[slot] &&
             wy < s->y[slot] + s->h[slot];
    if (hit) s->x[slot] = 500;
}

static void su15_begin_pull(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux, int32_t gx,
                            int32_t gy) {
    int in_range = gy > SU15_CLICK_Y_MIN - 1 && gy < SU15_CLICK_Y_MAX;
    if (!in_range) return;
    int32_t n = st->num_slots;

    memset(aux->pulled, 0, (size_t)n);
    memset(aux->pull_dx, 0, (size_t)n * sizeof(int32_t));
    memset(aux->pull_dy, 0, (size_t)n * sizeof(int32_t));
    memset(aux->has_pull_vector, 0, (size_t)n);
    memset(aux->near_click, 0, (size_t)n);
    memset(aux->consuming, 0, (size_t)n);
    memset(aux->consume_frame, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_start_x, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_start_y, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_dx, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_dy, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_pos_x, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_pos_y, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_dying, 0, (size_t)n);
    memset(aux->demoted, 0, (size_t)n);
    for (int32_t i = 0; i < n; i++) aux->demoted_tier[i] = -1;
    memset(aux->eat_lock, 0, (size_t)n * sizeof(int32_t));

    memcpy(aux->pull_anchor_x, s->x, (size_t)n * sizeof(int32_t));
    memcpy(aux->pull_anchor_y, s->y, (size_t)n * sizeof(int32_t));

    aux->win_flash_frame = 0;
    aux->flash_pair_a = -1;
    aux->flash_pair_b = -1;

    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    const int32_t *fruits = st->fruit_slot + (size_t)level * SU15_MAX_FRUITS;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t i = blobs[k];
        if (i < 0 || !s->alive[i]) continue;
        int near = su15_within_radius(s->x[i], s->y[i], s->w[i], s->h[i], gx, gy, SU15_CLICK_RADIUS);
        aux->pulled[i] = (uint8_t)near;
    }
    for (int32_t k = 0; k < SU15_MAX_FRUITS; k++) {
        int32_t i = fruits[k];
        if (i < 0 || !s->alive[i]) continue;
        int near = su15_within_radius(s->x[i], s->y[i], s->w[i], s->h[i], gx, gy, SU15_CLICK_RADIUS);
        aux->near_click[i] = (uint8_t)near;
        int has_vec = near && aux->fruit_kind[i] == 0;
        aux->has_pull_vector[i] = (uint8_t)has_vec;
        if (has_vec) {
            int32_t cx = s->x[i] + s->w[i] / 2, cy = s->y[i] + s->h[i] / 2;
            aux->pull_dx[i] = gx - cx;
            aux->pull_dy[i] = gy - cy;
        }
    }

    aux->click_x = gx;
    aux->click_y = gy;
    aux->pull_frame = 0;
    aux->pull_confirmed = 0;
    aux->phase = SU15_PHASE_PULLING;

    su15_draw_glow(s, st, aux);
}

static void su15_advance_pulling(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                                 int32_t *next_order, int32_t *status, uint8_t *action_complete) {
    int still_running = su15_pkrdtzfrth(s, st, level, aux, next_order);

    if (!aux->pull_confirmed && aux->pull_frame >= SU15_PULL_FRAMES) {
        su15_check_blob_merge(s, st, level, aux, next_order);
        aux->pull_confirmed = 1;
    }

    if (aux->phase == SU15_PHASE_MISMATCH_FLASH) return;

    if (su15_win_check_point(s, st, level, aux)) {
        su15_clear_glow(s, st);
        aux->phase = SU15_PHASE_WIN_FLASH;
        aux->win_flash_frame = 0;
        su15_paint_zone_pieces(s, st, level, SU15_COLOR_EMPTY);
        return;
    }

    if (still_running) return;

    aux->phase = SU15_PHASE_IDLE;
    int not_yet = !su15_win_check_weighted(s, st, level, aux);
    if (not_yet) {
        su15_paint_zone_pieces(s, st, level, SU15_COLOR_NOT_YET);
        aux->not_yet_flash = 1;
    }
    su15_handle_tutorial_hide_if_moved(s, st, level, aux);

    if (su15_win_check_point(s, st, level, aux)) {
        aux->phase = SU15_PHASE_WIN_FLASH;
        aux->win_flash_frame = 0;
        su15_paint_zone_pieces(s, st, level, SU15_COLOR_EMPTY);
        return;
    }

    int no_blobs = 1;
    const int32_t *blobs = st->blob_slot + (size_t)level * SU15_MAX_BLOBS;
    for (int32_t k = 0; k < SU15_MAX_BLOBS; k++) {
        int32_t b = blobs[k];
        if (b >= 0 && s->alive[b]) { no_blobs = 0; break; }
    }
    int32_t steps = aux->steps_remaining;
    int32_t new_steps = steps - 1 > 0 ? steps - 1 : 0;
    int out_of_steps = steps <= 1;
    aux->steps_remaining = new_steps;
    int should_lose = no_blobs || out_of_steps;
    if (should_lose) *status = SU15_GAME_OVER;
    *action_complete = 1;
}

static void su15_advance_win_flash(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                                   int32_t *score, int32_t *status, uint8_t *next_level,
                                   uint8_t *action_complete) {
    su15_paint_zone_pieces(s, st, level, SU15_COLOR_EMPTY);
    int32_t frame = aux->win_flash_frame + 1;
    aux->win_flash_frame = frame;
    if (frame > 10) {
        int is_last = level == st->num_levels - 1;
        *score += 1;
        *next_level = (uint8_t)!is_last;
        if (is_last) *status = SU15_WIN;
        *action_complete = 1;
    }
}

static void su15_end_mismatch_flash(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                                    int32_t *next_order, int32_t *status, uint8_t *action_complete) {
    su15_apply_flash_color(s, st, aux, 0);
    aux->phase = SU15_PHASE_IDLE;
    aux->flash_pair_a = -1;
    aux->flash_pair_b = -1;

    int32_t cost = 2 + aux->step_penalty * 2;
    int32_t steps = aux->steps_remaining;
    int32_t remaining = steps > 0 ? steps - cost : steps;
    int32_t penalty = steps > 0 ? aux->step_penalty + 1 : aux->step_penalty;
    int still_has_steps = remaining > 0;
    aux->steps_remaining = remaining;
    aux->step_penalty = penalty;

    if (still_has_steps) {
        su15_pop_undo(s, st, level, aux, next_order);
    } else {
        *status = SU15_GAME_OVER;
    }
    *action_complete = 1;
}

static void su15_advance_mismatch_flash(ArcSprites *s, const Su15Static *st, int32_t level, Su15Aux *aux,
                                        int32_t *next_order, int32_t *status,
                                        uint8_t *action_complete) {
    int32_t frame = aux->win_flash_frame;
    int lit = (frame / 2) % 2 == 0;
    su15_apply_flash_color(s, st, aux, lit);
    int32_t next_frame = frame + 1;
    aux->win_flash_frame = next_frame;
    if (next_frame >= 16) su15_end_mismatch_flash(s, st, level, aux, next_order, status, action_complete);
}

static void su15_handle_idle_action(ArcSprites *s, const ArcCamera *camera, const Su15Static *st,
                                    int32_t level, int32_t action_id, int32_t action_x,
                                    int32_t action_y, Su15Aux *aux, int32_t *next_order,
                                    uint8_t *action_complete) {
    if (action_id == SU15_ACTION6) {
        int32_t wx, wy;
        int valid;
        su15_display_to_grid(camera, action_x, action_y, &wx, &wy, &valid);
        if (valid) {
            su15_push_undo(s, st, aux);
            su15_hide_tutorial_arrow_if_clicked(s, st, level, wx, wy);
            su15_begin_pull(s, st, level, aux, wx, wy);
        }
        if (aux->phase != SU15_PHASE_PULLING) *action_complete = 1;
    } else if (action_id == SU15_ACTION7) {
        su15_pop_undo(s, st, level, aux, next_order);
        *action_complete = 1;
    } else {
        *action_complete = 1;
    }
}

void su15_aux_alloc(Su15Aux *aux, int32_t num_slots) {
    aux->num_slots = num_slots;
    aux->tier = calloc((size_t)num_slots, sizeof(int32_t));
    aux->fruit_kind = calloc((size_t)num_slots, sizeof(int32_t));
    aux->pulled = calloc((size_t)num_slots, 1);
    aux->pull_dx = calloc((size_t)num_slots, sizeof(int32_t));
    aux->pull_dy = calloc((size_t)num_slots, sizeof(int32_t));
    aux->has_pull_vector = calloc((size_t)num_slots, 1);
    aux->pull_anchor_x = calloc((size_t)num_slots, sizeof(int32_t));
    aux->pull_anchor_y = calloc((size_t)num_slots, sizeof(int32_t));
    aux->near_click = calloc((size_t)num_slots, 1);
    aux->consuming = calloc((size_t)num_slots, 1);
    aux->consume_frame = calloc((size_t)num_slots, sizeof(int32_t));
    aux->consume_start_x = calloc((size_t)num_slots, sizeof(int32_t));
    aux->consume_start_y = calloc((size_t)num_slots, sizeof(int32_t));
    aux->consume_dx = calloc((size_t)num_slots, sizeof(int32_t));
    aux->consume_dy = calloc((size_t)num_slots, sizeof(int32_t));
    aux->consume_pos_x = calloc((size_t)num_slots, sizeof(int32_t));
    aux->consume_pos_y = calloc((size_t)num_slots, sizeof(int32_t));
    aux->consume_dying = calloc((size_t)num_slots, 1);
    aux->demoted = calloc((size_t)num_slots, 1);
    aux->demoted_tier = calloc((size_t)num_slots, sizeof(int32_t));
    aux->eat_lock = calloc((size_t)num_slots, sizeof(int32_t));
    size_t undo_n = (size_t)SU15_MAX_UNDO * (size_t)num_slots;
    aux->undo_tier = calloc(undo_n, sizeof(int32_t));
    aux->undo_fruit_kind = calloc(undo_n, sizeof(int32_t));
    aux->undo_x = calloc(undo_n, sizeof(int32_t));
    aux->undo_y = calloc(undo_n, sizeof(int32_t));
    aux->undo_alive = calloc(undo_n, 1);
    aux->undo_order = calloc(undo_n, sizeof(int32_t));
}

void su15_aux_free(Su15Aux *aux) {
    free(aux->tier);
    free(aux->fruit_kind);
    free(aux->pulled);
    free(aux->pull_dx);
    free(aux->pull_dy);
    free(aux->has_pull_vector);
    free(aux->pull_anchor_x);
    free(aux->pull_anchor_y);
    free(aux->near_click);
    free(aux->consuming);
    free(aux->consume_frame);
    free(aux->consume_start_x);
    free(aux->consume_start_y);
    free(aux->consume_dx);
    free(aux->consume_dy);
    free(aux->consume_pos_x);
    free(aux->consume_pos_y);
    free(aux->consume_dying);
    free(aux->demoted);
    free(aux->demoted_tier);
    free(aux->eat_lock);
    free(aux->undo_tier);
    free(aux->undo_fruit_kind);
    free(aux->undo_x);
    free(aux->undo_y);
    free(aux->undo_alive);
    free(aux->undo_order);
    memset(aux, 0, sizeof(*aux));
}

void su15_zero_aux(Su15Aux *aux) {
    int32_t n = aux->num_slots;
    memset(aux->pulled, 0, (size_t)n);
    memset(aux->pull_dx, 0, (size_t)n * sizeof(int32_t));
    memset(aux->pull_dy, 0, (size_t)n * sizeof(int32_t));
    memset(aux->has_pull_vector, 0, (size_t)n);
    memset(aux->pull_anchor_x, 0, (size_t)n * sizeof(int32_t));
    memset(aux->pull_anchor_y, 0, (size_t)n * sizeof(int32_t));
    memset(aux->near_click, 0, (size_t)n);
    memset(aux->consuming, 0, (size_t)n);
    memset(aux->consume_frame, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_start_x, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_start_y, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_dx, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_dy, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_pos_x, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_pos_y, 0, (size_t)n * sizeof(int32_t));
    memset(aux->consume_dying, 0, (size_t)n);
    memset(aux->demoted, 0, (size_t)n);
    memset(aux->eat_lock, 0, (size_t)n * sizeof(int32_t));

    for (int32_t i = 0; i < n; i++) {
        aux->tier[i] = -1;
        aux->fruit_kind[i] = -1;
        aux->demoted_tier[i] = -1;
    }

    aux->phase = SU15_PHASE_IDLE;
    aux->pull_frame = 0;
    aux->pull_confirmed = 0;
    aux->click_x = 0;
    aux->click_y = 0;
    aux->win_flash_frame = 0;
    aux->not_yet_flash = 0;
    aux->flash_pair_a = -1;
    aux->flash_pair_b = -1;
    aux->tutorial_hidden = 0;
    aux->steps_remaining = 0;
    aux->step_penalty = 0;
    aux->undo_top = 0;

    size_t undo_n = (size_t)SU15_MAX_UNDO * (size_t)n;
    memset(aux->undo_tier, 0, undo_n * sizeof(int32_t));
    memset(aux->undo_fruit_kind, 0, undo_n * sizeof(int32_t));
    memset(aux->undo_x, 0, undo_n * sizeof(int32_t));
    memset(aux->undo_y, 0, undo_n * sizeof(int32_t));
    memset(aux->undo_alive, 0, undo_n);
    memset(aux->undo_order, 0, undo_n * sizeof(int32_t));
}

void su15_on_set_level(ArcSprites *sprites, const Su15Static *st, int32_t level, Su15Aux *aux,
                       int32_t *next_order) {
    su15_zero_aux(aux);
    int32_t n = st->num_slots;
    const int32_t *tos = st->tier_of_slot + (size_t)level * n;
    const int32_t *fos = st->fruit_kind_of_slot + (size_t)level * n;
    memcpy(aux->tier, tos, (size_t)n * sizeof(int32_t));
    memcpy(aux->fruit_kind, fos, (size_t)n * sizeof(int32_t));
    aux->steps_remaining = st->steps_budget[level];

    int32_t slot = st->glow_slot;
    int8_t *patch = arc_sprite_pixels_mut(sprites, slot);
    memset(patch, -1, (size_t)(st->ph * st->pw));
    sprites->w[slot] = SU15_GLOW_SIZE;
    sprites->h[slot] = SU15_GLOW_SIZE;
    sprites->x[slot] = 0;
    sprites->y[slot] = 0;
    arc_add_sprite(sprites, slot, *next_order);
    arc_set_visible(sprites, slot, 1);
    *next_order += 1;

    su15_push_undo(sprites, st, aux);
}

void su15_step_once(ArcSprites *sprites, const ArcCamera *camera, const Su15Static *st, int32_t level,
                    int32_t action_id, int32_t action_x, int32_t action_y, Su15Aux *aux,
                    int32_t *next_order, int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    int32_t phase = aux->phase;
    if (phase == SU15_PHASE_IDLE) {
        su15_handle_idle_action(sprites, camera, st, level, action_id, action_x, action_y, aux,
                                next_order, action_complete);
    } else if (phase == SU15_PHASE_WIN_FLASH) {
        su15_advance_win_flash(sprites, st, level, aux, score, status, next_level, action_complete);
    } else if (phase == SU15_PHASE_MISMATCH_FLASH) {
        su15_advance_mismatch_flash(sprites, st, level, aux, next_order, status, action_complete);
    } else {
        su15_advance_pulling(sprites, st, level, aux, next_order, status, action_complete);
    }
}

void su15_render_interface(int8_t *frame, const Su15Static *st, int32_t level, const Su15Aux *aux) {
    int32_t budget = st->steps_budget[level];
    if (budget <= 0) return;
    int32_t steps = aux->steps_remaining;
    int32_t a = -steps * ARC_FRAME_SIZE;
    int32_t filled = -su15_floor_div(a, budget);
    int32_t end;
    if (filled >= 0) {
        end = filled < ARC_FRAME_SIZE ? filled : ARC_FRAME_SIZE;
    } else {
        int32_t v = ARC_FRAME_SIZE + filled;
        end = v > 0 ? v : 0;
    }
    int8_t *row = frame + (size_t)(ARC_FRAME_SIZE - 1) * ARC_FRAME_SIZE;
    for (int32_t c = 0; c < end; c++) row[c] = (int8_t)SU15_COLOR_EMPTY;
}
