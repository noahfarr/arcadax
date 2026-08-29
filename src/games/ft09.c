#include "ft09.h"

#include <string.h>

enum { FT09_RESET = 0, FT09_ACTION6 = 6 };
enum { FT09_WIN = 2, FT09_GAME_OVER = 3 };
enum { FT09_PITCH = 4, FT09_STAMP_MARKER = 6 };
enum { FT09_BAR_FILLED = 12, FT09_BAR_EMPTY = 11 };
enum { FT09_HINT_FRAMES = 4, FT09_HINT_COLOUR_ODD = 0, FT09_HINT_COLOUR_EVEN = 2 };

static int8_t *ft09_pixels_mut(ArcSprites *s, int32_t i) {
    const ArcAtlas *a = s->atlas;
    int32_t area = a->ph * a->pw;
    int8_t *dst = s->pixels + (size_t)i * area;
    if (!s->overridden[i]) {
        memcpy(dst, a->pixels + (size_t)i * area, (size_t)area);
        s->overridden[i] = 1;
        s->bbox[i * 4 + 0] = 0;
        s->bbox[i * 4 + 1] = a->ph;
        s->bbox[i * 4 + 2] = 0;
        s->bbox[i * 4 + 3] = a->pw;
    }
    return dst;
}

static void ft09_cell_at(const ArcSprites *s, const Ft09Static *st, int32_t x,
                         int32_t y, int32_t *slot, int *is_pattern) {
    int32_t plain = arc_get_sprite_at(s, x, y, st->plain_tag, 0);
    int32_t pattern = arc_get_sprite_at(s, x, y, st->pattern_tag, 0);
    int found_plain = plain >= 0;
    *slot = found_plain ? plain : pattern;
    *is_pattern = !found_plain && pattern >= 0;
}

static void ft09_advance_colour(ArcSprites *s, const Ft09Static *st, int32_t level,
                                int32_t slot) {
    int32_t pw = s->atlas->pw;
    int8_t current = arc_sprite_pixels(s, slot)[pw + 1];
    const int32_t *palette = st->palette + (size_t)level * st->palette_width;
    int32_t size = st->palette_size[level];
    int32_t index = 0;
    for (int32_t k = 0; k < size; k++) {
        if (palette[k] == current) {
            index = k;
            break;
        }
    }
    int8_t neu = (int8_t)palette[(index + 1) % size];
    arc_color_remap(s, slot, 1, current, neu);
}

static void ft09_stamp(ArcSprites *s, const Ft09Static *st, int32_t level,
                       int32_t slot, int is_pattern) {
    int32_t pw = s->atlas->pw;
    const int8_t *patch = arc_sprite_pixels(s, slot);
    int32_t own[3][3];
    for (int32_t row = 0; row < 3; row++)
        for (int32_t col = 0; col < 3; col++)
            own[row][col] = patch[row * pw + col] == FT09_STAMP_MARKER ? 1 : 0;
    own[1][1] = 1;
    const int32_t *brush = st->brush + (size_t)level * 9;
    int32_t origin_x = s->x[slot], origin_y = s->y[slot];
    for (int32_t k = 0; k < 9; k++) {
        int32_t col = k / 3, row = k % 3;
        int32_t use = is_pattern ? own[row][col] : brush[row * 3 + col];
        if (!use) continue;
        int32_t x = origin_x + (col - 1) * FT09_PITCH;
        int32_t y = origin_y + (row - 1) * FT09_PITCH;
        int32_t target;
        int target_is_pattern;
        ft09_cell_at(s, st, x, y, &target, &target_is_pattern);
        if (target >= 0) ft09_advance_colour(s, st, level, target);
    }
}

static int ft09_solved(const ArcSprites *s, const Ft09Static *st, int32_t level) {
    static const int32_t offset_dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32_t offset_dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int32_t pw = s->atlas->pw;
    int32_t count = st->clue_count[level];
    const int32_t *clues = st->clue_slots + (size_t)level * st->max_clues;
    for (int32_t ci = 0; ci < count; ci++) {
        int32_t slot = clues[ci];
        const int8_t *patch = arc_sprite_pixels(s, slot);
        int8_t want = patch[pw + 1];
        for (int32_t o = 0; o < 8; o++) {
            int32_t dx = offset_dx[o], dy = offset_dy[o];
            int equal = patch[(dy + 1) * pw + (dx + 1)] == 0;
            int32_t target;
            int target_is_pattern;
            ft09_cell_at(s, st, s->x[slot] + dx * FT09_PITCH,
                        s->y[slot] + dy * FT09_PITCH, &target, &target_is_pattern);
            if (target < 0) continue;
            int8_t colour = arc_sprite_pixels(s, target)[pw + 1];
            int ok = equal ? colour == want : colour != want;
            if (!ok) return 0;
        }
    }
    return 1;
}

static void ft09_clear_level(const Ft09Static *st, int32_t level, int32_t *score,
                             int32_t *status, uint8_t *next_level,
                             uint8_t *action_complete) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = !is_last;
    if (is_last) *status = FT09_WIN;
    *action_complete = 1;
}

static void ft09_spend_step(Ft09Aux *aux, int32_t *status,
                            uint8_t *action_complete) {
    int32_t steps = aux->steps - 1;
    if (steps < 0) steps = 0;
    aux->steps = steps;
    if (steps <= 0) *status = FT09_GAME_OVER;
    *action_complete = 1;
}

static void ft09_animate_hint(ArcSprites *s, const Ft09Static *st, int32_t level,
                              Ft09Aux *aux, uint8_t *action_complete) {
    int32_t hint = aux->hint - 1;
    int32_t slot = st->hint_slot[level];
    int8_t colour = hint % 2 == 1 ? FT09_HINT_COLOUR_ODD : FT09_HINT_COLOUR_EVEN;
    arc_color_remap(s, slot, 0, 0, colour);
    aux->hint = hint;
    if (hint == 0) *action_complete = 1;
}

static void ft09_miss(const ArcSprites *s, const Ft09Static *st, int32_t level,
                      int32_t world_x, int32_t world_y, Ft09Aux *aux,
                      uint8_t *action_complete) {
    int32_t clue = arc_get_sprite_at(s, world_x, world_y, st->clue_tag, 0);
    int show_hint = level == 0 && st->hint_slot[level] >= 0 && clue < 0;
    if (show_hint) {
        aux->hint = FT09_HINT_FRAMES;
    } else {
        *action_complete = 1;
    }
}

static void ft09_resolve(ArcSprites *s, const Ft09Static *st, int32_t level,
                         int32_t slot, int is_pattern, int hit_cell,
                         Ft09Aux *aux, int32_t *score, int32_t *status,
                         uint8_t *next_level, uint8_t *action_complete) {
    if (hit_cell) ft09_stamp(s, st, level, slot, is_pattern);
    if (ft09_solved(s, st, level)) {
        ft09_clear_level(st, level, score, status, next_level, action_complete);
    } else {
        ft09_spend_step(aux, status, action_complete);
    }
}

static void ft09_play(ArcSprites *s, const ArcCamera *camera, const Ft09Static *st,
                      int32_t level, int32_t action_id, int32_t action_x,
                      int32_t action_y, Ft09Aux *aux, int32_t *score,
                      int32_t *status, uint8_t *next_level,
                      uint8_t *action_complete) {
    int32_t scale, x_offset, y_offset;
    arc_scale_and_offset(camera, &scale, &x_offset, &y_offset);
    int32_t dx = action_x - x_offset, dy = action_y - y_offset;
    int32_t grid_x = dx >= 0 ? dx / scale : -1;
    int32_t grid_y = dy >= 0 ? dy / scale : -1;
    int on_board = grid_x >= 0 && grid_y >= 0 && grid_x < camera->width &&
                   grid_y < camera->height;
    int32_t world_x = grid_x + camera->x, world_y = grid_y + camera->y;
    on_board = on_board && action_id == FT09_ACTION6;

    int32_t slot;
    int is_pattern;
    ft09_cell_at(s, st, world_x, world_y, &slot, &is_pattern);
    int hit_cell = on_board && slot >= 0;

    if (!hit_cell && on_board) {
        ft09_miss(s, st, level, world_x, world_y, aux, action_complete);
    } else {
        ft09_resolve(s, st, level, slot, is_pattern, hit_cell, aux, score, status,
                    next_level, action_complete);
    }
}

void ft09_zero_aux(Ft09Aux *aux) {
    aux->steps = 0;
    aux->hint = 0;
}

void ft09_on_set_level(ArcSprites *sprites, const Ft09Static *st, int32_t level,
                       Ft09Aux *aux) {
    const ArcAtlas *a = sprites->atlas;
    int8_t base = (int8_t)st->palette[(size_t)level * st->palette_width];
    for (int32_t i = 0; i < a->num_slots; i++) {
        int is_plain = sprites->tags[(size_t)i * a->num_tags + st->plain_tag];
        int is_pattern = sprites->tags[(size_t)i * a->num_tags + st->pattern_tag];
        if (!is_plain && !is_pattern) continue;
        if (is_plain) {
            int8_t plain_colour = arc_sprite_pixels(sprites, i)[0];
            arc_color_remap(sprites, i, 1, plain_colour, base);
        }
        if (is_pattern) {
            int8_t *patch = ft09_pixels_mut(sprites, i);
            int32_t pw = a->pw;
            for (int32_t row = 0; row < 3; row++)
                for (int32_t col = 0; col < 3; col++) {
                    int32_t k = row * pw + col;
                    if (patch[k] != FT09_STAMP_MARKER) patch[k] = base;
                }
        }
    }
    aux->steps = st->budget[level];
    aux->hint = 0;
}

void ft09_step_once(ArcSprites *sprites, const ArcCamera *camera, const Ft09Static *st,
                    int32_t level, int32_t action_id, int32_t action_x,
                    int32_t action_y, Ft09Aux *aux, int32_t *score,
                    int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    if (action_id == FT09_RESET) {
        *action_complete = 1;
        return;
    }
    if (aux->hint > 0 && st->hint_slot[level] >= 0) {
        ft09_animate_hint(sprites, st, level, aux, action_complete);
        return;
    }
    ft09_play(sprites, camera, st, level, action_id, action_x, action_y, aux,
             score, status, next_level, action_complete);
}

void ft09_render_interface(int8_t *frame, const Ft09Static *st, int32_t level,
                           int32_t steps) {
    int32_t budget = st->budget[level];
    if (budget == 0) return;
    int32_t total = ARC_FRAME_SIZE * steps;
    int32_t whole = total / budget;
    int32_t rest = total % budget;
    int round_up = 2 * rest > budget || (2 * rest == budget && whole % 2 == 1);
    int32_t filled = whole + (round_up ? 1 : 0);
    int8_t *row = frame + (size_t)(ARC_FRAME_SIZE - 1) * ARC_FRAME_SIZE;
    for (int32_t col = 0; col < ARC_FRAME_SIZE; col++)
        row[col] = col < filled ? FT09_BAR_FILLED : FT09_BAR_EMPTY;
}
