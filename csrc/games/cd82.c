#include "cd82.h"

#include <stddef.h>

enum { CD82_ACTION1 = 1, CD82_ACTION2 = 2, CD82_ACTION3 = 3, CD82_ACTION4 = 4,
       CD82_ACTION5 = 5, CD82_ACTION6 = 6 };
enum { CD82_WIN = 2, CD82_GAME_OVER = 3 };
enum { CD82_STEP_BUDGET = 100 };
enum { CD82_FILLED = 4, CD82_EMPTY = 5 };
enum { CD82_DRAW_REACH = 7 };

static const int32_t CD82_POS_X[8] = {25, 33, 38, 33, 25, 14, 17, 14};
static const int32_t CD82_POS_Y[8] = {24, 21, 32, 40, 45, 40, 32, 21};
static const int32_t CD82_POS_DX[8] = {0, -1, -1, -1, 0, 1, 1, 1};
static const int32_t CD82_POS_DY[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const uint8_t CD82_POS_HORIZONTAL[8] = {1, 0, 1, 0, 1, 0, 1, 0};
static const int32_t CD82_POS_ROTATION[8] = {180, 0, 270, 90, 0, 180, 90, 270};

static const int32_t CD82_POS_TO_A[8] = {0, 0, 1, 2, 2, 2, 1, 0};
static const int32_t CD82_POS_TO_B[8] = {1, 2, 2, 2, 1, 0, 0, 0};
static const int32_t CD82_GRID_TO_POS[3][3] = {
    {7, 0, 1},
    {6, -1, 2},
    {5, 4, 3},
};

static const uint8_t CD82_ARROW_EXISTS[8] = {1, 0, 1, 0, 1, 0, 1, 0};
static const int32_t CD82_ARROW_X[8] = {29, 33, 48, 33, 29, 14, 11, 14};
static const int32_t CD82_ARROW_Y[8] = {18, 21, 36, 40, 55, 40, 36, 21};
static const int32_t CD82_BOUNCE_DX[8] = {0, 0, -2, 0, 0, 0, 2, 0};
static const int32_t CD82_BOUNCE_DY[8] = {2, 0, 0, 0, -2, 0, 0, 0};

static int cd82_rtjwayrycq(int32_t pos, int32_t r, int32_t c) {
    int32_t rot = CD82_POS_ROTATION[pos];
    if (CD82_POS_HORIZONTAL[pos]) {
        if (rot == 180) return r < 5;
        if (rot == 0) return r >= 5;
        if (rot == 90) return c < 5;
        return c >= 5;
    }
    if (rot == 180) return c <= r;
    if (rot == 90) return c >= 9 - r;
    if (rot == 0) return c >= r;
    return c < 10 - r;
}

static int cd82_coublenfir(int32_t direction, int32_t r, int32_t c) {
    switch (direction) {
        case 0: return r >= 0 && r < 3 && c >= 3 && c < 7;
        case 4: return r >= 7 && r < 10 && c >= 3 && c < 7;
        case 6: return r >= 3 && r < 7 && c >= 0 && c < 3;
        case 2: return r >= 3 && r < 7 && c >= 7 && c < 10;
        default: return 0;
    }
}

static void cd82_rebuild(Sprites *s, const Cd82Static *st, int32_t level,
                         Cd82Aux *aux, int32_t *next_order) {
    int32_t pos = aux->position;
    int8_t color = (int8_t)aux->color;
    int32_t area = st->ph * st->pw;

    const int8_t *btpl = st->basket_pixels + (size_t)pos * area;
    const uint8_t *bis15 = st->basket_is15 + (size_t)pos * area;
    int8_t *bdst = sprite_pixels_mut(s, st->basket_slot);
    for (int32_t k = 0; k < area; k++) bdst[k] = bis15[k] ? color : btpl[k];
    set_position(s, st->basket_slot, CD82_POS_X[pos], CD82_POS_Y[pos]);
    s->h[st->basket_slot] = st->basket_h[pos];
    s->w[st->basket_slot] = st->basket_w[pos];
    s->alive[st->basket_slot] = 1;
    s->order[st->basket_slot] = *next_order;
    set_interaction(s, st->basket_slot, TANGIBLE);
    s->blocking[st->basket_slot] = PIXEL_PERFECT;
    *next_order += 1;

    int arrow_active = CD82_ARROW_EXISTS[pos] && st->level_has_arrow[level];
    const int8_t *atpl = st->arrow_pixels + (size_t)pos * area;
    const uint8_t *ais15 = st->arrow_is15 + (size_t)pos * area;
    int8_t *adst = sprite_pixels_mut(s, st->arrow_slot);
    for (int32_t k = 0; k < area; k++) adst[k] = ais15[k] ? color : atpl[k];
    set_position(s, st->arrow_slot, CD82_ARROW_X[pos], CD82_ARROW_Y[pos]);
    s->h[st->arrow_slot] = st->arrow_h[pos];
    s->w[st->arrow_slot] = st->arrow_w[pos];
    s->alive[st->arrow_slot] = (uint8_t)arrow_active;
    if (arrow_active) {
        s->order[st->arrow_slot] = *next_order;
        *next_order += 1;
    }
    set_interaction(s, st->arrow_slot, TANGIBLE);
    s->blocking[st->arrow_slot] = PIXEL_PERFECT;
}

static void cd82_move_ring(Sprites *s, const Cd82Static *st, int32_t level,
                           Cd82Aux *aux, int32_t *next_order, int32_t direction) {
    int32_t a = CD82_POS_TO_A[aux->position];
    int32_t b = CD82_POS_TO_B[aux->position];
    switch (direction) {
        case 1: a = a > 0 ? a - 1 : 0; break;
        case 2: a = a < 2 ? a + 1 : 2; break;
        case 3: b = b > 0 ? b - 1 : 0; break;
        default: b = b < 2 ? b + 1 : 2; break;
    }
    int32_t new_pos = CD82_GRID_TO_POS[a][b];
    int valid = new_pos >= 0 && new_pos != aux->position;
    if (valid) {
        aux->position = new_pos;
        cd82_rebuild(s, st, level, aux, next_order);
    }
}

static void cd82_paint_canvas(Sprites *s, const Cd82Static *st, int32_t level,
                              int8_t color, int (*mask_fn)(int32_t, int32_t, int32_t),
                              int32_t mask_arg) {
    int32_t slot = st->canvas_slot[level];
    int32_t pw = st->pw;
    int8_t *px = sprite_pixels_mut(s, slot);
    for (int32_t r = 0; r < 10; r++)
        for (int32_t c = 0; c < 10; c++)
            if (mask_fn(mask_arg, r, c)) px[r * pw + c] = color;
}

static void cd82_next_level(const Cd82Static *st, int32_t level, int32_t *score,
                            int32_t *status, uint8_t *next_level) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = (uint8_t)!is_last;
    if (is_last) *status = CD82_WIN;
}

static void cd82_check_win(Sprites *s, const Cd82Static *st, int32_t level,
                           int32_t *score, int32_t *status, uint8_t *next_level) {
    int32_t canvas_slot = st->canvas_slot[level];
    int32_t answer_slot = st->answer_slot[level];
    const int8_t *canvas = sprite_pixels(s, canvas_slot);
    const int8_t *answer = sprite_pixels(s, answer_slot);
    int32_t pw = st->pw;
    int equal = 1;
    for (int32_t r = 0; r < 10 && equal; r++) {
        for (int32_t c = 0; c < 10; c++) {
            if (r == c || r == 9 - c) continue;
            if (canvas[r * pw + c] != answer[r * pw + c]) {
                equal = 0;
                break;
            }
        }
    }
    if (equal) cd82_next_level(st, level, score, status, next_level);
}

static void cd82_start_draw(Cd82Aux *aux) {
    aux->drawing = 1;
    aux->draw_forward = 1;
    aux->draw_counter = 0;
    aux->draw_painted = 0;
}

static void cd82_continue_draw(Sprites *s, const Cd82Static *st, int32_t level,
                               Cd82Aux *aux, int32_t *score, int32_t *status,
                               uint8_t *next_level, uint8_t *action_complete) {
    int32_t pos = aux->position;
    int32_t dx = CD82_POS_DX[pos], dy = CD82_POS_DY[pos];
    if (aux->draw_forward) {
        move_sprite(s, st->basket_slot, dx, dy);
        aux->draw_counter += 1;
        int reached = aux->draw_counter >= CD82_DRAW_REACH;
        if (reached && !aux->draw_painted) {
            cd82_paint_canvas(s, st, level, (int8_t)aux->color, cd82_rtjwayrycq, pos);
            aux->draw_painted = 1;
        }
        if (reached) aux->draw_forward = 0;
    } else {
        move_sprite(s, st->basket_slot, -dx, -dy);
        aux->draw_counter -= 1;
        if (aux->draw_counter <= 0) {
            set_position(s, st->basket_slot, CD82_POS_X[pos], CD82_POS_Y[pos]);
            aux->drawing = 0;
            cd82_check_win(s, st, level, score, status, next_level);
            *action_complete = 1;
        }
    }
}

static void cd82_start_bounce(Cd82Aux *aux) {
    aux->bouncing = 1;
    aux->bounce_forward = 1;
    aux->bounce_counter = 0;
    aux->bounce_painted = 0;
    aux->bounce_direction = aux->position;
}

static void cd82_continue_bounce(Sprites *s, const Cd82Static *st, int32_t level,
                                 Cd82Aux *aux, int32_t *score, int32_t *status,
                                 uint8_t *next_level, uint8_t *action_complete) {
    int32_t dir = aux->bounce_direction;
    int32_t dx = CD82_BOUNCE_DX[dir], dy = CD82_BOUNCE_DY[dir];
    if (aux->bounce_forward) {
        move_sprite(s, st->arrow_slot, dx, dy);
        aux->bounce_counter += 1;
        int reached = aux->bounce_counter >= CD82_DRAW_REACH;
        if (reached && !aux->bounce_painted) {
            cd82_paint_canvas(s, st, level, (int8_t)aux->color, cd82_coublenfir, dir);
            aux->bounce_painted = 1;
        }
        if (reached) aux->bounce_forward = 0;
    } else {
        move_sprite(s, st->arrow_slot, -dx, -dy);
        aux->bounce_counter -= 1;
        if (aux->bounce_counter <= 0) {
            set_position(s, st->arrow_slot, CD82_ARROW_X[dir], CD82_ARROW_Y[dir]);
            aux->bouncing = 0;
            cd82_check_win(s, st, level, score, status, next_level);
            *action_complete = 1;
        }
    }
}

static void cd82_handle_click(Sprites *s, const Camera *camera, const Cd82Static *st,
                              int32_t level, int32_t action_x, int32_t action_y,
                              Cd82Aux *aux, int32_t *next_order, uint8_t *action_complete) {
    int32_t scale, x_pad, y_pad;
    scale_and_offset(camera, &scale, &x_pad, &y_pad);
    int32_t dx = action_x - x_pad, dy = action_y - y_pad;
    int32_t gx = dx >= 0 ? dx / scale : -1;
    int32_t gy = dy >= 0 ? dy / scale : -1;
    int on_board = gx >= 0 && gy >= 0 && gx < camera->width && gy < camera->height;
    int32_t wx = gx + camera->x, wy = gy + camera->y;
    int32_t hit = get_sprite_at(s, wx, wy, -1, 1);
    hit = on_board ? hit : -1;
    int32_t hit_clip = hit < 0 ? 0 : hit;
    int is_arrow = hit >= 0 && hit == st->arrow_slot && s->alive[st->arrow_slot];
    int is_palette = hit >= 0 && st->palette_mask[(size_t)level * st->num_slots + hit_clip];

    if (is_arrow) {
        cd82_start_bounce(aux);
    } else if (is_palette) {
        int32_t color = sprite_pixels(s, hit_clip)[2 * st->pw + 2];
        int32_t marker_slot = st->marker_slot[level];
        int32_t hx = s->x[hit_clip], hy = s->y[hit_clip];
        set_position(s, marker_slot, hx, hy + 5);
        aux->color = color;
        cd82_rebuild(s, st, level, aux, next_order);
    }
    if (!aux->bouncing) *action_complete = 1;
}

static void cd82_dispatch(Sprites *s, const Camera *camera, const Cd82Static *st,
                          int32_t level, int32_t action_id, int32_t action_x,
                          int32_t action_y, Cd82Aux *aux, int32_t *next_order,
                          uint8_t *action_complete) {
    switch (action_id) {
        case CD82_ACTION1:
            cd82_move_ring(s, st, level, aux, next_order, 1);
            *action_complete = 1;
            break;
        case CD82_ACTION2:
            cd82_move_ring(s, st, level, aux, next_order, 2);
            *action_complete = 1;
            break;
        case CD82_ACTION3:
            cd82_move_ring(s, st, level, aux, next_order, 3);
            *action_complete = 1;
            break;
        case CD82_ACTION4:
            cd82_move_ring(s, st, level, aux, next_order, 4);
            *action_complete = 1;
            break;
        case CD82_ACTION5:
            cd82_start_draw(aux);
            break;
        case CD82_ACTION6:
            cd82_handle_click(s, camera, st, level, action_x, action_y, aux, next_order,
                              action_complete);
            break;
        default:
            *action_complete = 1;
            break;
    }
}

void cd82_zero_aux(Cd82Aux *aux) {
    aux->position = 0;
    aux->color = 15;
    aux->drawing = 0;
    aux->draw_forward = 1;
    aux->draw_counter = 0;
    aux->draw_painted = 0;
    aux->bouncing = 0;
    aux->bounce_forward = 1;
    aux->bounce_counter = 0;
    aux->bounce_painted = 0;
    aux->bounce_direction = 0;
}

void cd82_on_set_level(Sprites *sprites, const Cd82Static *st, int32_t level,
                       Cd82Aux *aux, int32_t *next_order) {
    int32_t count = st->removal_count[level];
    const int32_t *slots = st->removal_slots + (size_t)level * st->max_removal;
    for (int32_t k = 0; k < count; k++) sprites->alive[slots[k]] = 0;
    cd82_zero_aux(aux);
    *next_order = st->num_slots;
    cd82_rebuild(sprites, st, level, aux, next_order);
}

void cd82_step_once(Sprites *sprites, const Camera *camera, const Cd82Static *st,
                    int32_t level, int32_t action_id, int32_t action_x,
                    int32_t action_y, int32_t action_count, Cd82Aux *aux,
                    int32_t *next_order, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete) {
    if (action_count >= CD82_STEP_BUDGET) {
        *status = CD82_GAME_OVER;
        *action_complete = 1;
        return;
    }
    if (aux->drawing) {
        cd82_continue_draw(sprites, st, level, aux, score, status, next_level, action_complete);
    } else if (aux->bouncing) {
        cd82_continue_bounce(sprites, st, level, aux, score, status, next_level, action_complete);
    } else {
        cd82_dispatch(sprites, camera, st, level, action_id, action_x, action_y, aux,
                      next_order, action_complete);
    }
}

void cd82_render_interface(int8_t *frame, int32_t action_count) {
    int32_t steps = CD82_STEP_BUDGET - action_count;
    if (steps < 0) steps = 0;
    if (steps > CD82_STEP_BUDGET) steps = CD82_STEP_BUDGET;
    int32_t total = FRAME_SIZE * steps;
    int32_t whole = total / CD82_STEP_BUDGET, rest = total % CD82_STEP_BUDGET;
    int round_up = 2 * rest > CD82_STEP_BUDGET ||
                  (2 * rest == CD82_STEP_BUDGET && whole % 2 == 1);
    int32_t filled = whole + (round_up ? 1 : 0);
    if (filled > FRAME_SIZE) filled = FRAME_SIZE;
    int8_t *row = frame + (size_t)(FRAME_SIZE - 1) * FRAME_SIZE;
    for (int32_t c = 0; c < FRAME_SIZE; c++)
        row[c] = (int8_t)(c < filled ? CD82_FILLED : CD82_EMPTY);
}
