#include "tr87.h"

#include <stdlib.h>
#include <string.h>

enum { TR87_ACTION1 = 1, TR87_ACTION2 = 2, TR87_ACTION3 = 3, TR87_ACTION4 = 4 };
enum { TR87_WIN = 2, TR87_GAME_OVER = 3 };

static const int8_t TR87_PALETTE[8] = {5, 8, 14, 15, 6, 9, 12, 0};

static inline int32_t tr87_area(const Tr87Static *st) { return st->ph * st->pw; }

static inline int32_t tr87_pymod(int32_t a, int32_t b) {
    if (b <= 0) return 0;
    int32_t r = a % b;
    return r < 0 ? r + b : r;
}

static inline int32_t tr87_clampi(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t tr87_rule_left_len(const Tr87Static *st, int32_t level, int32_t r) {
    return st->rule_left_len[(size_t)level * TR87_MAX_RULES + r];
}

static inline const int32_t *tr87_rule_left_slot(const Tr87Static *st, int32_t level, int32_t r) {
    return st->rule_left_slot + ((size_t)level * TR87_MAX_RULES + r) * TR87_MAX_CHAIN;
}

static inline int32_t tr87_rule_right_len(const Tr87Static *st, int32_t level, int32_t r) {
    return st->rule_right_len[(size_t)level * TR87_MAX_RULES + r];
}

static inline const int32_t *tr87_rule_right_slot(const Tr87Static *st, int32_t level, int32_t r) {
    return st->rule_right_slot + ((size_t)level * TR87_MAX_RULES + r) * TR87_MAX_CHAIN;
}

static inline int32_t tr87_identity_code(const Tr87Static *st, int32_t level,
                                         const int32_t *car_digit, int32_t slot) {
    if (slot < 0) return -1;
    int32_t group = st->slot_group[(size_t)level * st->num_slots + slot];
    int32_t digit = car_digit[slot];
    return group * TR87_NUM_DIGITS + (digit - 1);
}

static int tr87_row_matches_pattern(const Tr87Static *st, int32_t level, const int32_t *car_digit,
                                    const int32_t *row_slot, int32_t row_len, int32_t start,
                                    const int32_t *pattern_codes, int32_t pattern_len,
                                    int32_t max_row) {
    if (start + pattern_len > row_len) return 0;
    for (int32_t i = 0; i < pattern_len; i++) {
        int32_t row_idx = tr87_clampi(start + i, 0, max_row - 1);
        if (tr87_identity_code(st, level, car_digit, row_slot[row_idx]) != pattern_codes[i])
            return 0;
    }
    return 1;
}

static void tr87_find_rule_with_left_sequence(const Tr87Static *st, int32_t level,
                                              const int32_t *car_digit,
                                              const int32_t *target_codes, int32_t target_len,
                                              int *out_found, int32_t *out_rule) {
    *out_found = 0;
    *out_rule = 0;
    for (int32_t r = 0; r < TR87_MAX_RULES; r++) {
        int32_t left_len = tr87_rule_left_len(st, level, r);
        if (left_len != target_len) continue;
        const int32_t *left_slots = tr87_rule_left_slot(st, level, r);
        int match = 1;
        for (int32_t i = 0; i < left_len; i++) {
            if (tr87_identity_code(st, level, car_digit, left_slots[i]) != target_codes[i]) {
                match = 0;
                break;
            }
        }
        if (match) {
            *out_found = 1;
            *out_rule = r;
            return;
        }
    }
}

static void tr87_find_rule_with_left_first(const Tr87Static *st, int32_t level,
                                           const int32_t *car_digit, int32_t target_code,
                                           int *out_found, int32_t *out_rule) {
    *out_found = 0;
    *out_rule = 0;
    for (int32_t r = 0; r < TR87_MAX_RULES; r++) {
        int32_t left_len = tr87_rule_left_len(st, level, r);
        if (left_len <= 0) continue;
        int32_t first_slot = tr87_rule_left_slot(st, level, r)[0];
        if (tr87_identity_code(st, level, car_digit, first_slot) == target_code) {
            *out_found = 1;
            *out_rule = r;
            return;
        }
    }
}

typedef struct {
    int resolved;
    int32_t codes[TR87_MAX_RESOLVED];
    int32_t slots[TR87_MAX_RESOLVED];
    int32_t length;
    int32_t extra_slots[TR87_MAX_HIGHLIGHTS];
    int32_t extra_len;
} Tr87RightChain;

static void tr87_resolve_right_chain(const Tr87Static *st, int32_t level, const int32_t *car_digit,
                                     int32_t rule_idx, Tr87RightChain *out) {
    int32_t right_len = tr87_rule_right_len(st, level, rule_idx);
    const int32_t *right_slots = tr87_rule_right_slot(st, level, rule_idx);
    int32_t right_codes[TR87_MAX_CHAIN];
    for (int32_t i = 0; i < right_len; i++)
        right_codes[i] = tr87_identity_code(st, level, car_digit, right_slots[i]);

    for (int32_t i = 0; i < TR87_MAX_RESOLVED; i++) {
        out->codes[i] = -1;
        out->slots[i] = -1;
    }
    for (int32_t i = 0; i < TR87_MAX_HIGHLIGHTS; i++) out->extra_slots[i] = -1;

    int tree_translation = st->tree_translation_level[level];
    int double_translation = st->double_translation_level[level];

    if (tree_translation) {
        int tree_resolved = 1;
        int32_t tree_len = 0, tree_extra_len = 0;
        for (int32_t j = 0; j < right_len; j++) {
            int found_j;
            int32_t rule_j;
            tr87_find_rule_with_left_first(st, level, car_digit, right_codes[j], &found_j, &rule_j);
            if (!found_j) {
                tree_resolved = 0;
                continue;
            }
            int32_t second_right_len = tr87_rule_right_len(st, level, rule_j);
            const int32_t *second_right_slots = tr87_rule_right_slot(st, level, rule_j);
            for (int32_t k = 0; k < second_right_len; k++) {
                int32_t pos = tr87_clampi(tree_len + k, 0, TR87_MAX_RESOLVED - 1);
                out->codes[pos] = tr87_identity_code(st, level, car_digit, second_right_slots[k]);
                out->slots[pos] = second_right_slots[k];
            }
            tree_len += second_right_len;

            int32_t extra_pos0 = tr87_clampi(tree_extra_len, 0, TR87_MAX_HIGHLIGHTS - 1);
            out->extra_slots[extra_pos0] = right_slots[j];
            tree_extra_len += 1;

            int32_t second_left_len = tr87_rule_left_len(st, level, rule_j);
            const int32_t *second_left_slots = tr87_rule_left_slot(st, level, rule_j);
            for (int32_t k = 0; k < second_left_len; k++) {
                int32_t extra_pos = tr87_clampi(tree_extra_len, 0, TR87_MAX_HIGHLIGHTS - 1);
                out->extra_slots[extra_pos] = second_left_slots[k];
                tree_extra_len += 1;
            }
        }
        out->resolved = tree_resolved;
        out->length = tree_len;
        out->extra_len = tree_extra_len;
    } else if (double_translation) {
        int double_found;
        int32_t double_rule;
        tr87_find_rule_with_left_sequence(st, level, car_digit, right_codes, right_len,
                                          &double_found, &double_rule);
        int32_t double_right_len = tr87_rule_right_len(st, level, double_rule);
        const int32_t *double_right_slots = tr87_rule_right_slot(st, level, double_rule);
        for (int32_t k = 0; k < double_right_len; k++) {
            out->codes[k] = tr87_identity_code(st, level, car_digit, double_right_slots[k]);
            out->slots[k] = double_right_slots[k];
        }
        int32_t double_left_len = tr87_rule_left_len(st, level, double_rule);
        const int32_t *double_left_slots = tr87_rule_left_slot(st, level, double_rule);
        for (int32_t k = 0; k < right_len; k++) out->extra_slots[k] = right_slots[k];
        for (int32_t k = 0; k < double_left_len; k++) {
            int32_t pos = tr87_clampi(right_len + k, 0, TR87_MAX_HIGHLIGHTS - 1);
            out->extra_slots[pos] = double_left_slots[k];
        }
        out->resolved = double_found;
        out->length = double_right_len;
        out->extra_len = right_len + double_left_len;
    } else {
        for (int32_t k = 0; k < right_len; k++) {
            out->codes[k] = right_codes[k];
            out->slots[k] = right_slots[k];
        }
        out->resolved = 1;
        out->length = right_len;
        out->extra_len = 0;
    }
}

typedef struct {
    int left_matches_top;
    int resolved;
    int right_matches_bottom;
    int32_t left_len;
    int32_t right_len;
    int32_t color_slots[TR87_MAX_COLOR_TARGETS];
    int32_t color_len;
    int32_t highlight_slots[TR87_MAX_HIGHLIGHTS];
    int32_t highlight_len;
} Tr87RuleTry;

static void tr87_try_rule_at(const Tr87Static *st, int32_t level, const int32_t *car_digit,
                             int32_t top_pos, int32_t bottom_pos, int32_t rule_idx,
                             Tr87RuleTry *out) {
    int32_t left_len = tr87_rule_left_len(st, level, rule_idx);
    const int32_t *left_slots = tr87_rule_left_slot(st, level, rule_idx);
    int32_t left_codes[TR87_MAX_CHAIN];
    for (int32_t i = 0; i < left_len; i++)
        left_codes[i] = tr87_identity_code(st, level, car_digit, left_slots[i]);

    const int32_t *top_slot = st->top_slot + (size_t)level * TR87_MAX_TOP;
    int32_t top_len = st->top_len[level];
    out->left_matches_top = tr87_row_matches_pattern(st, level, car_digit, top_slot, top_len,
                                                      top_pos, left_codes, left_len, TR87_MAX_TOP);

    Tr87RightChain rc;
    tr87_resolve_right_chain(st, level, car_digit, rule_idx, &rc);
    out->resolved = rc.resolved;

    const int32_t *bottom_slot = st->bottom_slot + (size_t)level * TR87_MAX_BOTTOM;
    int32_t bottom_len = st->bottom_len[level];
    out->right_matches_bottom = tr87_row_matches_pattern(st, level, car_digit, bottom_slot,
                                                          bottom_len, bottom_pos, rc.codes,
                                                          rc.length, TR87_MAX_BOTTOM);

    out->left_len = left_len;
    out->right_len = rc.length;

    for (int32_t i = 0; i < TR87_MAX_COLOR_TARGETS; i++) out->color_slots[i] = -1;
    for (int32_t i = 0; i < left_len; i++) {
        int32_t top_idx = tr87_clampi(top_pos + i, 0, TR87_MAX_TOP - 1);
        out->color_slots[i] = top_slot[top_idx];
    }
    for (int32_t i = 0; i < rc.length; i++) {
        int32_t pos = tr87_clampi(left_len + i, 0, TR87_MAX_COLOR_TARGETS - 1);
        int32_t bottom_idx = tr87_clampi(bottom_pos + i, 0, TR87_MAX_BOTTOM - 1);
        out->color_slots[pos] = bottom_slot[bottom_idx];
    }
    out->color_len = left_len + rc.length;

    for (int32_t i = 0; i < TR87_MAX_HIGHLIGHTS; i++) out->highlight_slots[i] = -1;
    for (int32_t i = 0; i < left_len; i++) out->highlight_slots[i] = left_slots[i];
    for (int32_t i = 0; i < rc.length; i++) {
        int32_t pos = tr87_clampi(left_len + i, 0, TR87_MAX_HIGHLIGHTS - 1);
        out->highlight_slots[pos] = rc.slots[i];
    }
    int32_t extra_base = left_len + rc.length;
    for (int32_t i = 0; i < rc.extra_len; i++) {
        int32_t pos = tr87_clampi(extra_base + i, 0, TR87_MAX_HIGHLIGHTS - 1);
        out->highlight_slots[pos] = rc.extra_slots[i];
    }
    out->highlight_len = extra_base + rc.extra_len;
}

typedef struct {
    int found;
    int bottom_mismatch;
    int32_t left_len;
    int32_t right_len;
    int32_t color_slots[TR87_MAX_COLOR_TARGETS];
    int32_t color_len;
    int32_t highlight_slots[TR87_MAX_HIGHLIGHTS];
    int32_t highlight_len;
} Tr87ScanResult;

static void tr87_scan_rules_at_position(const Tr87Static *st, int32_t level,
                                        const int32_t *car_digit, int32_t top_pos,
                                        int32_t bottom_pos, Tr87ScanResult *out) {
    out->found = 0;
    out->bottom_mismatch = 0;
    out->left_len = 0;
    out->right_len = 0;
    out->color_len = 0;
    out->highlight_len = 0;

    int32_t num_rules = tr87_clampi(st->num_rules[level], 0, TR87_MAX_RULES);
    for (int32_t rule_idx = 0; rule_idx < num_rules; rule_idx++) {
        Tr87RuleTry r;
        tr87_try_rule_at(st, level, car_digit, top_pos, bottom_pos, rule_idx, &r);
        int candidate_matched = r.left_matches_top && r.resolved;
        if (candidate_matched && r.right_matches_bottom) {
            out->found = 1;
            out->left_len = r.left_len;
            out->right_len = r.right_len;
            memcpy(out->color_slots, r.color_slots, sizeof out->color_slots);
            out->color_len = r.color_len;
            memcpy(out->highlight_slots, r.highlight_slots, sizeof out->highlight_slots);
            out->highlight_len = r.highlight_len;
            return;
        }
        if (candidate_matched) {
            out->bottom_mismatch = 1;
            return;
        }
    }
}

typedef struct {
    int solved;
    int32_t stage_count;
    int32_t color_slot[TR87_MAX_STAGES][TR87_MAX_COLOR_TARGETS];
    int32_t color_len[TR87_MAX_STAGES];
    int32_t highlight_slot[TR87_MAX_STAGES][TR87_MAX_HIGHLIGHTS];
    int32_t highlight_len[TR87_MAX_STAGES];
} Tr87SolveResult;

static void tr87_puzzle_is_solved(const Tr87Static *st, int32_t level, const int32_t *car_digit,
                                  Tr87SolveResult *out) {
    int32_t top_len = st->top_len[level];
    int32_t top_pos = 0, bottom_pos = 0;
    int failed = 0;
    out->stage_count = 0;
    for (int32_t s = 0; s < TR87_MAX_STAGES; s++) {
        for (int32_t i = 0; i < TR87_MAX_COLOR_TARGETS; i++) out->color_slot[s][i] = -1;
        out->color_len[s] = 0;
        for (int32_t i = 0; i < TR87_MAX_HIGHLIGHTS; i++) out->highlight_slot[s][i] = -1;
        out->highlight_len[s] = 0;
    }

    for (int32_t iter = 0; iter < TR87_MAX_TOP; iter++) {
        if (failed || top_pos >= top_len) break;
        Tr87ScanResult sr;
        tr87_scan_rules_at_position(st, level, car_digit, top_pos, bottom_pos, &sr);
        if (sr.bottom_mismatch || !sr.found) {
            failed = 1;
            break;
        }
        int32_t stage_idx = tr87_clampi(out->stage_count, 0, TR87_MAX_STAGES - 1);
        memcpy(out->color_slot[stage_idx], sr.color_slots, sizeof sr.color_slots);
        out->color_len[stage_idx] = sr.color_len;
        memcpy(out->highlight_slot[stage_idx], sr.highlight_slots, sizeof sr.highlight_slots);
        out->highlight_len[stage_idx] = sr.highlight_len;
        out->stage_count += 1;
        top_pos += sr.left_len;
        bottom_pos += sr.right_len;
    }

    out->solved = !failed && (top_pos >= top_len);
}

static void tr87_selected_unit(const Tr87Static *st, int32_t level, int32_t cursor_index,
                               int32_t slots[TR87_MAX_CHAIN], int32_t *length) {
    if (st->alter_rules_level[level]) {
        int32_t rule = tr87_clampi(cursor_index / 2, 0, TR87_MAX_RULES - 1);
        int is_right_chain = (cursor_index % 2) != 0;
        int32_t chain_len = is_right_chain ? tr87_rule_right_len(st, level, rule)
                                            : tr87_rule_left_len(st, level, rule);
        const int32_t *chain_slots = is_right_chain ? tr87_rule_right_slot(st, level, rule)
                                                     : tr87_rule_left_slot(st, level, rule);
        for (int32_t i = 0; i < TR87_MAX_CHAIN; i++) slots[i] = chain_slots[i];
        *length = chain_len;
    } else {
        int32_t bottom_pos = tr87_clampi(cursor_index, 0, TR87_MAX_BOTTOM - 1);
        slots[0] = st->bottom_slot[(size_t)level * TR87_MAX_BOTTOM + bottom_pos];
        slots[1] = -1;
        slots[2] = -1;
        *length = 1;
    }
}

static int32_t tr87_num_cursor_units(const Tr87Static *st, int32_t level) {
    if (st->alter_rules_level[level]) return 2 * st->num_rules[level];
    return st->bottom_len[level];
}

static void tr87_place_cursor_sprites(Sprites *sprites, const Tr87Static *st, int32_t level,
                                      const Tr87Aux *aux) {
    int32_t slots[TR87_MAX_CHAIN];
    int32_t length;
    tr87_selected_unit(st, level, aux->cursor_index, slots, &length);

    int32_t representative = tr87_clampi(slots[0], 0, st->num_slots - 1);
    int32_t width_id = tr87_clampi(length - 1, 0, 2);
    int32_t cursor_w = width_id == 0 ? 5 : (width_id == 1 ? 12 : 19);
    int32_t top_y = sprites->y[representative] - 4;
    int32_t bottom_y = sprites->y[representative] + sprites->h[representative] - 2 + 4;

    int32_t top = st->cursor_top_slot, bottom = st->cursor_bottom_slot;
    int32_t area = tr87_area(st);
    int32_t rep_x = sprites->x[representative];

    sprites->x[top] = rep_x;
    sprites->x[bottom] = rep_x;
    sprites->y[top] = top_y;
    sprites->y[bottom] = bottom_y;
    sprites->w[top] = cursor_w;
    sprites->w[bottom] = cursor_w;
    memcpy(sprite_pixels_mut(sprites, top), st->cursor_patch_top + (size_t)width_id * area, area);
    memcpy(sprite_pixels_mut(sprites, bottom), st->cursor_patch_bottom + (size_t)width_id * area,
           area);
}

static int32_t tr87_cycled_digit(int32_t digit, int32_t delta) {
    return tr87_pymod(digit - 1 + delta, TR87_NUM_DIGITS) + 1;
}

static void tr87_end_turn(Tr87Aux *aux, int32_t *status, uint8_t *action_complete) {
    if (aux->budget == 0) *status = TR87_GAME_OVER;
    *action_complete = 1;
}

static void tr87_cycle_selected_unit(Sprites *sprites, const Tr87Static *st, int32_t level,
                                     int32_t action_id, Tr87Aux *aux, int32_t *status,
                                     uint8_t *action_complete) {
    int32_t delta = action_id == TR87_ACTION1 ? -1 : 1;
    int32_t slots[TR87_MAX_CHAIN];
    int32_t length;
    tr87_selected_unit(st, level, aux->cursor_index, slots, &length);

    int32_t area = tr87_area(st);
    for (int32_t i = 0; i < length && i < TR87_MAX_CHAIN; i++) {
        int32_t slot = tr87_clampi(slots[i], 0, st->num_slots - 1);
        int32_t new_digit = tr87_cycled_digit(aux->car_digit[slot], delta);
        aux->car_digit[slot] = new_digit;
        const int8_t *patch = st->digit_patch +
                              (((size_t)level * st->num_slots + slot) * TR87_NUM_DIGITS +
                               (new_digit - 1)) *
                                  area;
        memcpy(sprite_pixels_mut(sprites, slot), patch, area);
    }
    aux->budget -= 1;

    Tr87SolveResult sr;
    tr87_puzzle_is_solved(st, level, aux->car_digit, &sr);

    if (sr.solved) {
        sprites->interaction[st->cursor_top_slot] = INVISIBLE;
        sprites->interaction[st->cursor_bottom_slot] = INVISIBLE;
        aux->anim_frame = 0;
        aux->anim_stage_count = sr.stage_count;
        memcpy(aux->anim_color_slot, sr.color_slot, sizeof aux->anim_color_slot);
        memcpy(aux->anim_color_len, sr.color_len, sizeof aux->anim_color_len);
        memcpy(aux->anim_highlight_slot, sr.highlight_slot, sizeof aux->anim_highlight_slot);
        memcpy(aux->anim_highlight_len, sr.highlight_len, sizeof aux->anim_highlight_len);
    } else {
        tr87_end_turn(aux, status, action_complete);
    }
}

static void tr87_move_cursor(Sprites *sprites, const Tr87Static *st, int32_t level,
                             int32_t action_id, Tr87Aux *aux, int32_t *status,
                             uint8_t *action_complete) {
    int32_t delta = action_id == TR87_ACTION3 ? -1 : 1;
    int32_t units = tr87_num_cursor_units(st, level);
    aux->cursor_index = tr87_pymod(aux->cursor_index + delta, units);
    aux->budget -= 1;
    tr87_place_cursor_sprites(sprites, st, level, aux);
    tr87_end_turn(aux, status, action_complete);
}

static void tr87_play(Sprites *sprites, const Tr87Static *st, int32_t level, int32_t action_id,
                      Tr87Aux *aux, int32_t *status, uint8_t *action_complete) {
    if (action_id == TR87_ACTION3 || action_id == TR87_ACTION4) {
        tr87_move_cursor(sprites, st, level, action_id, aux, status, action_complete);
    } else if (action_id == TR87_ACTION1 || action_id == TR87_ACTION2) {
        tr87_cycle_selected_unit(sprites, st, level, action_id, aux, status, action_complete);
    } else {
        tr87_end_turn(aux, status, action_complete);
    }
}

static void tr87_advance_win_animation(Sprites *sprites, const Tr87Static *st, int32_t level,
                                       Tr87Aux *aux, int32_t *score, int32_t *status,
                                       uint8_t *next_level, uint8_t *action_complete) {
    int32_t frame = aux->anim_frame;
    int32_t stage = tr87_clampi(frame / TR87_SUBFRAMES_PER_STAGE, 0, TR87_MAX_STAGES - 1);
    int32_t substep = frame % TR87_SUBFRAMES_PER_STAGE + 1;
    int8_t old_colour = TR87_PALETTE[substep - 1];
    int8_t new_colour = TR87_PALETTE[substep];

    const int32_t *color_slots = aux->anim_color_slot[stage];
    int32_t color_len = aux->anim_color_len[stage];
    for (int32_t i = 0; i < color_len && i < TR87_MAX_COLOR_TARGETS; i++) {
        int32_t slot = tr87_clampi(color_slots[i], 0, st->num_slots - 1);
        color_remap(sprites, slot, 1, old_colour, new_colour);
    }

    if (substep == 1) {
        const int32_t *h_slots = aux->anim_highlight_slot[stage];
        int32_t h_len = aux->anim_highlight_len[stage];
        int32_t area = tr87_area(st);
        for (int32_t i = 0; i < TR87_MAX_HIGHLIGHTS; i++) {
            int32_t pool_slot = st->highlight_base_slot + i;
            int active = i < h_len;
            int32_t target = tr87_clampi(h_slots[i], 0, st->num_slots - 1);
            if (active) {
                sprites->x[pool_slot] = sprites->x[target] - 2;
                sprites->y[pool_slot] = sprites->y[target] - 2;
                sprites->alive[pool_slot] = 1;
                sprites->interaction[pool_slot] = TANGIBLE;
            } else {
                sprites->alive[pool_slot] = 0;
                sprites->interaction[pool_slot] = REMOVED;
            }
            memcpy(sprite_pixels_mut(sprites, pool_slot), st->highlight_patch, area);
        }
    }

    frame += 1;
    aux->anim_frame = frame;
    int32_t total_frames = aux->anim_stage_count * TR87_SUBFRAMES_PER_STAGE;
    if (frame == total_frames) {
        aux->anim_frame = -1;
        int is_last = level == st->num_levels - 1;
        *score += 1;
        *next_level = (uint8_t)!is_last;
        if (is_last) *status = TR87_WIN;
        *action_complete = 1;
    }
}

void tr87_aux_alloc(Tr87Aux *aux, int32_t num_slots) {
    aux->num_slots = num_slots;
    aux->car_digit = calloc((size_t)num_slots, sizeof(int32_t));
}

void tr87_aux_free(Tr87Aux *aux) {
    free(aux->car_digit);
    aux->car_digit = NULL;
}

void tr87_zero_aux(Tr87Aux *aux) {
    for (int32_t i = 0; i < aux->num_slots; i++) aux->car_digit[i] = 1;
    aux->cursor_index = 0;
    aux->budget = 0;
    aux->anim_frame = -1;
    aux->anim_stage_count = 0;
    for (int32_t s = 0; s < TR87_MAX_STAGES; s++) {
        for (int32_t i = 0; i < TR87_MAX_COLOR_TARGETS; i++) aux->anim_color_slot[s][i] = -1;
        aux->anim_color_len[s] = 0;
        for (int32_t i = 0; i < TR87_MAX_HIGHLIGHTS; i++) aux->anim_highlight_slot[s][i] = -1;
        aux->anim_highlight_len[s] = 0;
    }
}

void tr87_on_set_level(Sprites *sprites, const Tr87Static *st, int32_t level, Tr87Aux *aux) {
    int32_t n = st->num_slots;
    int32_t area = tr87_area(st);
    for (int32_t i = 0; i < n; i++) {
        if (!st->is_car_slot[(size_t)level * n + i]) continue;
        int32_t digit = st->initial_digit[(size_t)level * n + i];
        const int8_t *patch = st->digit_patch +
                              (((size_t)level * n + i) * TR87_NUM_DIGITS + (digit - 1)) * area;
        memcpy(sprite_pixels_mut(sprites, i), patch, area);
    }

    int32_t top = st->cursor_top_slot, bottom = st->cursor_bottom_slot;
    sprites->alive[top] = 1;
    sprites->interaction[top] = TANGIBLE;
    sprites->h[top] = 2;
    sprites->alive[bottom] = 1;
    sprites->interaction[bottom] = TANGIBLE;
    sprites->h[bottom] = 2;
    for (int32_t i = 0; i < TR87_MAX_HIGHLIGHTS; i++)
        sprites->layer[st->highlight_base_slot + i] = -2;

    tr87_zero_aux(aux);
    for (int32_t i = 0; i < n; i++) aux->car_digit[i] = st->initial_digit[(size_t)level * n + i];
    aux->budget = st->budget0[level];

    tr87_place_cursor_sprites(sprites, st, level, aux);
}

void tr87_step_once(Sprites *sprites, const Tr87Static *st, int32_t level, int32_t action_id,
                    Tr87Aux *aux, int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    if (aux->anim_frame >= 0) {
        tr87_advance_win_animation(sprites, st, level, aux, score, status, next_level,
                                   action_complete);
    } else {
        tr87_play(sprites, st, level, action_id, aux, status, action_complete);
    }
}

void tr87_render_interface(int8_t *frame, const Tr87Static *st, int32_t level,
                           const Tr87Aux *aux) {
    int32_t budget0 = st->budget0[level];
    int32_t budget = aux->budget < 0 ? 0 : aux->budget;
    int32_t filled = 0;
    if (budget0 > 0) {
        int32_t total = FRAME_SIZE * budget;
        filled = (total + budget0 - 1) / budget0;
        filled = tr87_clampi(filled, 0, FRAME_SIZE);
    }
    int8_t *row = frame + (size_t)(FRAME_SIZE - 1) * FRAME_SIZE;
    for (int32_t c = 0; c < FRAME_SIZE; c++) row[c] = (int8_t)(c < filled ? 1 : 4);
}
