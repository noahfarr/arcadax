#include "sb26.h"

#include <stdlib.h>
#include <string.h>

enum { SB26_ACTION5 = 5, SB26_ACTION6 = 6, SB26_ACTION7 = 7 };
enum { SB26_WIN = 2, SB26_GAME_OVER = 3 };
enum { SB26_PITCH = 6, SB26_SLOT_ORIGIN = 2, SB26_INITIAL_ENERGY = 64, SB26_TWEEN_STEPS = 6 };
enum { SB26_BACKGROUND_COLOR = 4, SB26_Y_THRESHOLD = 53, SB26_HUD_EMPTY = 3, SB26_HUD_FILLED = 2 };
enum { SB26_BLACK = 0, SB26_RED = 8, SB26_GREY = 3, SB26_PINK = 5 };

static int32_t sb26_jidx(int32_t idx, int32_t dim) {
    if (idx < 0) {
        idx += dim;
        return idx < 0 ? 0 : idx;
    }
    return idx >= dim ? dim - 1 : idx;
}

static void sb26_set_visible_slot(Sprites *s, int32_t slot, int visible) {
    if (slot < 0 || slot >= s->atlas->num_slots) return;
    set_visible(s, slot, visible);
}

static uint8_t sb26_tracked(const Sprites *s, const Sb26Static *st, int32_t i) {
    const Atlas *a = s->atlas;
    return s->tags[(size_t)i * a->num_tags + st->item_tag] ||
           s->tags[(size_t)i * a->num_tags + st->spot_tag];
}

static int32_t sb26_tween_lookup(const Sb26Static *st, int32_t k, int32_t a, int32_t b) {
    int32_t w = st->tween_width, off = st->tween_val_min;
    return st->tween_table[((size_t)k * w + (a - off)) * w + (b - off)];
}

static void sb26_lookup(const Sprites *s, const Sb26Static *st, int32_t level,
                        int32_t frame_phys, int32_t slot_idx, int *is_item,
                        int32_t *occupant, int *is_frameref) {
    int32_t cx = s->x[frame_phys] + SB26_SLOT_ORIGIN + slot_idx * SB26_PITCH;
    int32_t cy = s->y[frame_phys] + SB26_SLOT_ORIGIN;
    const Atlas *a = s->atlas;
    int32_t n = a->num_slots;
    int32_t item_slot = 0, spot_slot = 0;
    int found_item = 0, found_spot = 0;
    for (int32_t i = 0; i < n; i++) {
        if (!(s->alive[i] && s->x[i] == cx && s->y[i] == cy)) continue;
        if (!found_item && s->tags[(size_t)i * a->num_tags + st->item_tag]) {
            item_slot = i;
            found_item = 1;
        }
        if (!found_spot && s->tags[(size_t)i * a->num_tags + st->spot_tag]) {
            spot_slot = i;
            found_spot = 1;
        }
    }
    *is_item = found_item;
    *occupant = found_item ? item_slot : spot_slot;
    *is_frameref = found_item && st->is_frameref[(size_t)level * n + item_slot];
}

static void sb26_border_redraw(Sprites *s, int32_t slot, int32_t width, int corner) {
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int32_t h = s->h[slot];
    int8_t *patch = sprite_pixels_mut(s, slot);
    int8_t colour = patch[0];
    for (int32_t row = 0; row < ph; row++) {
        for (int32_t col = 0; col < pw; col++) {
            int left_right = col == 0 || col == width - 1;
            int top_bottom = (row == 0 || row == h - 1) && col < width;
            int border = row < h && (left_right || top_bottom);
            if (corner) {
                int k = 3;
                int erase_sides = row >= k && row <= h - k - 1 && left_right;
                int erase_caps = (row == 0 || row == h - 1) && col >= k && col <= width - k - 1;
                if (erase_sides || erase_caps) border = 0;
            }
            patch[(size_t)row * pw + col] = (int8_t)(border ? colour : -1);
        }
    }
}

static void sb26_rqtmgdegai(Sprites *s, const Sb26Static *st, int32_t slot, int32_t offset,
                            int32_t fx, int32_t fy, int32_t tx, int32_t ty, int32_t k) {
    int32_t x = sb26_tween_lookup(st, k, fx - offset, tx - offset);
    int32_t y = sb26_tween_lookup(st, k, fy - offset, ty - offset);
    set_position(s, slot, x, y);
}

static void sb26_dagvovvbpp(Sprites *s, const Sb26Static *st, int32_t slot, int32_t offset,
                            int32_t fx, int32_t fy, int32_t fw, int32_t tx, int32_t ty,
                            int32_t tw, int32_t k) {
    sb26_rqtmgdegai(s, st, slot, offset, fx, fy, tx, ty, k);
    int32_t width = sb26_tween_lookup(st, k, fw + 2 * offset, tw + 2 * offset);
    int32_t pw = s->atlas->pw;
    int32_t probe_col = width - 1;
    if (probe_col < 0) probe_col = 0;
    if (probe_col >= pw) probe_col = pw - 1;
    int redraw = sprite_pixels(s, slot)[(size_t)1 * pw + probe_col] != 1;
    if (redraw) sb26_border_redraw(s, slot, width, 1);
}

static void sb26_apply_tween_slot(Sprites *s, const Sb26Static *st, int32_t role, int32_t fx,
                                  int32_t fy, int32_t fw, int32_t tx, int32_t ty, int32_t tw,
                                  int32_t k) {
    if (role < 0) return;
    int32_t idx = role;
    if (idx > 2) idx = 2;
    if (idx < 0) idx = 0;
    switch (idx) {
        case 0:
            sb26_dagvovvbpp(s, st, st->mjeqtdqvm, 1, fx, fy, fw, tx, ty, tw, k);
            break;
        case 1:
            sb26_dagvovvbpp(s, st, st->ayaigjtxp, 0, fx, fy, fw, tx, ty, tw, k);
            break;
        default:
            sb26_rqtmgdegai(s, st, st->ohvavdnio, 1, fx, fy, tx, ty, k);
            break;
    }
}

static void sb26_paint_row(Sprites *s, int32_t slot, int32_t row, int8_t colour) {
    int32_t ph = s->atlas->ph, pw = s->atlas->pw;
    int32_t w = s->w[slot];
    int32_t r = row + 1;
    if (r < 0 || r >= ph) return;
    int8_t *patch = sprite_pixels_mut(s, slot);
    for (int32_t c = 1; c < w - 1 && c < pw; c++) patch[(size_t)r * pw + c] = colour;
}

static int32_t sb26_click_hit(const Sprites *s, const Sb26Aux *aux, const Sb26Static *st,
                              int32_t x, int32_t y) {
    const Atlas *a = s->atlas;
    int32_t best = -1, best_key = 0;
    int found = 0;
    for (int32_t i = 0; i < a->num_slots; i++) {
        if (!s->alive[i]) continue;
        if (!(x >= s->x[i] && y >= s->y[i] && x < s->x[i] + s->w[i] && y < s->y[i] + s->h[i]))
            continue;
        if (!sprite_collidable(s, i)) continue;
        if (s->blocking[i] == PIXEL_PERFECT) {
            int32_t py = y - s->y[i], px = x - s->x[i];
            if (py < 0) py = 0;
            if (py >= a->ph) py = a->ph - 1;
            if (px < 0) px = 0;
            if (px >= a->pw) px = a->pw - 1;
            if (sprite_pixels(s, i)[(size_t)py * a->pw + px] == -1) continue;
        }
        if (!s->tags[(size_t)i * a->num_tags + st->click_tag]) continue;
        if (aux->click_off[i]) continue;
        int32_t key = s->layer[i] * ORDER_BITS + (ORDER_BITS - 1 - s->order[i]);
        if (!found || key > best_key) {
            found = 1;
            best_key = key;
            best = i;
        }
    }
    return found ? best : -1;
}

static void sb26_next_level(const Sb26Static *st, int32_t level, int32_t *score,
                            int32_t *status, uint8_t *next_level) {
    int is_last = level == st->num_levels - 1;
    *score += 1;
    *next_level = (uint8_t)!is_last;
    if (is_last) *status = SB26_WIN;
}

static void sb26_push_undo(Sprites *s, const Sb26Static *st, Sb26Aux *aux) {
    int32_t n = st->num_slots;
    int32_t idx = aux->undo_depth;
    if (idx < 0) idx = 0;
    if (idx > SB26_MAX_UNDO - 1) idx = SB26_MAX_UNDO - 1;
    int32_t *ux = aux->undo_x + (size_t)idx * n;
    int32_t *uy = aux->undo_y + (size_t)idx * n;
    uint8_t *uv = aux->undo_vis + (size_t)idx * n;
    for (int32_t i = 0; i < n; i++) {
        if (!sb26_tracked(s, st, i)) continue;
        ux[i] = s->x[i];
        uy[i] = s->y[i];
        uv[i] = (uint8_t)sprite_visible(s, i);
    }
    int32_t nd = aux->undo_depth + 1;
    aux->undo_depth = nd > SB26_MAX_UNDO ? SB26_MAX_UNDO : nd;
}

static void sb26_handle_action7(Sprites *s, const Sb26Static *st, Sb26Aux *aux) {
    if (aux->undo_depth < 2) return;
    color_remap(s, st->mrokwhyjs0, 0, 0, SB26_BACKGROUND_COLOR);
    int32_t new_depth = aux->undo_depth - 1;
    int32_t idx = new_depth - 1;
    if (idx < 0) idx = 0;
    if (idx > SB26_MAX_UNDO - 1) idx = SB26_MAX_UNDO - 1;
    int32_t n = st->num_slots;
    const int32_t *ux = aux->undo_x + (size_t)idx * n;
    const int32_t *uy = aux->undo_y + (size_t)idx * n;
    const uint8_t *uv = aux->undo_vis + (size_t)idx * n;
    for (int32_t i = 0; i < n; i++) {
        if (!sb26_tracked(s, st, i)) continue;
        s->x[i] = ux[i];
        s->y[i] = uy[i];
        set_visible(s, i, uv[i]);
    }
    aux->undo_depth = new_depth;
    aux->selected = -1;
}

static void sb26_hjewbkcejq(Sprites *s, const Sb26Static *st, Sb26Aux *aux, int32_t hit) {
    const Atlas *a = s->atlas;
    int hit_is_item = s->tags[(size_t)hit * a->num_tags + st->item_tag];

    if (aux->selected < 0) {
        if (hit_is_item) {
            color_remap(s, st->mrokwhyjs0, 0, 0, SB26_BLACK);
            set_position(s, st->mrokwhyjs0, s->x[hit], s->y[hit]);
            aux->selected = hit;
        }
        return;
    }

    int32_t selected = aux->selected;
    if (hit_is_item) {
        if (hit == selected) {
            color_remap(s, st->mrokwhyjs0, 0, 0, SB26_BACKGROUND_COLOR);
            aux->selected = -1;
            return;
        }
        int both_tray = s->y[selected] > SB26_Y_THRESHOLD && s->y[hit] > SB26_Y_THRESHOLD;
        if (both_tray) {
            set_position(s, st->mrokwhyjs0, s->x[hit], s->y[hit]);
            aux->selected = hit;
            return;
        }
        int32_t sel_x = s->x[selected], sel_y = s->y[selected];
        int32_t hit_x = s->x[hit], hit_y = s->y[hit];
        set_position(s, selected, hit_x, hit_y);
        set_position(s, hit, sel_x, sel_y);
        set_position(s, st->mrokwhyjs1, hit_x, hit_y);
        color_remap(s, st->mrokwhyjs1, 0, 0, SB26_BLACK);
        aux->selected = -1;
        aux->artsfnufc = 0;
        aux->energy -= 1;
        return;
    }

    int hit_is_spot = s->tags[(size_t)hit * a->num_tags + st->spot_tag];
    if (!hit_is_spot) return;

    int32_t origin_spot = get_sprite_at(s, s->x[selected], s->y[selected], st->spot_tag, 0);
    sb26_set_visible_slot(s, origin_spot, 1);
    sb26_set_visible_slot(s, hit, 0);
    set_position(s, selected, s->x[hit], s->y[hit]);
    set_position(s, st->mrokwhyjs1, s->x[hit], s->y[hit]);
    color_remap(s, st->mrokwhyjs1, 0, 0, SB26_BLACK);
    aux->selected = -1;
    aux->artsfnufc = 0;
    aux->energy -= 1;
}

static void sb26_handle_click(Sprites *s, const Sb26Static *st, Sb26Aux *aux, int32_t action_x,
                              int32_t action_y) {
    int32_t hit = sb26_click_hit(s, aux, st, action_x, action_y);
    if (hit >= 0) sb26_hjewbkcejq(s, st, aux, hit);
}

static void sb26_handle_default(Sprites *s, const Sb26Static *st, Sb26Aux *aux, int32_t action_id,
                                int32_t action_x, int32_t action_y, uint8_t *action_complete) {
    int is7 = action_id == SB26_ACTION7;
    int is6 = action_id == SB26_ACTION6;
    if (is7) sb26_handle_action7(s, st, aux);
    int32_t pre_artsfnufc = aux->artsfnufc;
    if (is6) sb26_handle_click(s, st, aux, action_x, action_y);
    int just_placed = is6 && pre_artsfnufc < 0 && aux->artsfnufc == 0;
    if (just_placed) {
        sb26_push_undo(s, st, aux);
        return;
    }
    *action_complete = 1;
}

static void sb26_rfdjlhefnd(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux) {
    int32_t n = st->num_slots;
    int32_t root = st->qaagahahj[(size_t)level * SB26_MAX_FRAMES + 0];

    for (int32_t i = 0; i < SB26_MAX_STACK; i++) {
        aux->stack_frame[i] = 0;
        aux->stack_slot[i] = 0;
    }
    aux->stack_frame[0] = root;

    sb26_set_visible_slot(s, st->ayaigjtxp, 1);
    sb26_set_visible_slot(s, st->ohvavdnio, 1);
    aux->fading[st->ayaigjtxp] = 1;
    aux->fade_target[st->ayaigjtxp] = SB26_BLACK;
    aux->fading[st->ohvavdnio] = 1;
    aux->fade_target[st->ohvavdnio] = SB26_BLACK;

    int is_item0, is_fr0;
    int32_t child0;
    sb26_lookup(s, st, level, root, 0, &is_item0, &child0, &is_fr0);
    int32_t cx = s->x[child0], cy = s->y[child0], cw = s->w[child0];
    sb26_dagvovvbpp(s, st, st->ayaigjtxp, 0, cx, cy, cw, cx, cy, cw, 0);

    int32_t card0 = st->card_slots[(size_t)level * SB26_MAX_CARDS + 0];
    set_position(s, st->ohvavdnio, s->x[card0] - 1, s->y[card0] - 1);

    for (int32_t i = 0; i < n; i++) {
        int allow = st->allow_fixed[i] || st->card_mask[(size_t)level * n + i];
        int hide = s->alive[i] && sprite_visible(s, i) && !allow;
        aux->peek_hidden[i] = (uint8_t)hide;
        if (hide) sb26_set_visible_slot(s, i, 0);
    }

    aux->stack_depth = 1;
    aux->cursor_count = 0;
    aux->pmygakdvy = 0;
    aux->ppsxsxiod = 0;
    aux->peek_active = 1;
    aux->modqnpqfi = 15;
}

static void sb26_handle_action5(Sprites *s, const Camera *camera, RenderScratch *scratch,
                                const Sb26Static *st, int32_t level, Sb26Aux *aux) {
    int8_t save_mjeq = s->interaction[st->mjeqtdqvm];
    int8_t save_m0 = s->interaction[st->mrokwhyjs0];
    sb26_set_visible_slot(s, st->mjeqtdqvm, 0);
    sb26_set_visible_slot(s, st->mrokwhyjs0, 0);
    render(s, camera, scratch, aux->snapshot);
    s->interaction[st->mjeqtdqvm] = save_mjeq;
    s->interaction[st->mrokwhyjs0] = save_m0;

    int has_sel = aux->selected >= 0;
    if (has_sel) {
        aux->fading[st->mrokwhyjs0] = 1;
        aux->fade_target[st->mrokwhyjs0] = SB26_BACKGROUND_COLOR;
        aux->selected = -1;
    }
    aux->energy -= 1;
    sb26_rfdjlhefnd(s, st, level, aux);
}

static void sb26_advance_fades(Sprites *s, Sb26Aux *aux) {
    int32_t n = s->atlas->num_slots;
    int32_t area = s->atlas->ph * s->atlas->pw;
    for (int32_t i = 0; i < n; i++) {
        if (!aux->fading[i]) continue;
        int8_t *patch = sprite_pixels_mut(s, i);
        int32_t cur = patch[0];
        int32_t diff = aux->fade_target[i] - cur;
        int32_t step = cur + (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
        for (int32_t p = 0; p < area; p++)
            if (patch[p] >= 0) patch[p] = (int8_t)step;
        if (step == aux->fade_target[i]) aux->fading[i] = 0;
    }
}

static void sb26_tick_japgbruyb(Sprites *s, const Sb26Static *st, Sb26Aux *aux) {
    int32_t step = aux->japgbruyb;
    int vis = (step / 3) % 2 == 0;
    sb26_set_visible_slot(s, st->oyvbxwyug, vis);
    int32_t step_new = step + 1;
    int done = step_new >= 18;
    aux->japgbruyb = done ? -1 : step_new;
    if (done) aux->bbiavyren = 0;
}

static void sb26_tick_lmvwmlqtw(const Sb26Static *st, int32_t level, Sb26Aux *aux, int32_t *score,
                                int32_t *status, uint8_t *next_level, uint8_t *action_complete) {
    int32_t step = aux->lmvwmlqtw + 1;
    aux->lmvwmlqtw = step;
    if (step > 15) {
        aux->lmvwmlqtw = -1;
        *action_complete = 1;
        sb26_next_level(st, level, score, status, next_level);
    }
}

static void sb26_tick_xjxrqgaqw(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux) {
    int32_t step = aux->xjxrqgaqw;
    int32_t row = step % 4;
    int32_t depth = aux->stack_depth;
    int32_t top_frame = aux->stack_frame[depth - 1];
    int32_t top_slot = aux->stack_slot[depth - 1];
    int is_item, is_fr;
    int32_t target_slot;
    sb26_lookup(s, st, level, top_frame, top_slot, &is_item, &target_slot, &is_fr);
    int32_t card_slot = st->card_slots[(size_t)level * SB26_MAX_CARDS + aux->pmygakdvy];
    int8_t colour = step < 4 ? SB26_BLACK : sprite_pixels(s, card_slot)[0];

    sb26_paint_row(s, card_slot, row, colour);
    sb26_paint_row(s, target_slot, row, colour);

    int32_t step_new = step + 1;
    int done = step_new == 8;
    aux->xjxrqgaqw = done ? -1 : step_new;
    if (done) aux->modqnpqfi = 5;
}

static void sb26_tick_bbiavyren(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux,
                                int32_t *status, uint8_t *action_complete) {
    int32_t step = aux->bbiavyren;
    int32_t row = step < 3 ? step : 3;
    int32_t n = st->num_slots;
    for (int32_t i = 0; i < n; i++) {
        if (!st->card_mask[(size_t)level * n + i]) continue;
        sb26_paint_row(s, i, row, -1);
    }

    int is_bg = sprite_pixels(s, st->mjeqtdqvm)[0] == SB26_BACKGROUND_COLOR;
    if (is_bg) {
        int32_t root = st->qaagahahj[(size_t)level * SB26_MAX_FRAMES + 0];
        int32_t rx = s->x[root], ry = s->y[root], rw = s->w[root];
        sb26_dagvovvbpp(s, st, st->mjeqtdqvm, 1, rx, ry, rw, rx, ry, rw, 0);
        color_remap(s, st->mjeqtdqvm, 0, 0, SB26_BACKGROUND_COLOR - 1);
        aux->fading[st->mjeqtdqvm] = 1;
        aux->fade_target[st->mjeqtdqvm] = SB26_BLACK;
    }

    int32_t step_new = step + 1;
    aux->bbiavyren = step_new;

    if (step_new >= 8) {
        for (int32_t k = 0; k < SB26_CURSOR_SLOTS; k++)
            if (k < aux->cursor_count) s->alive[st->cursor_base + k] = 0;
        sb26_set_visible_slot(s, st->ayaigjtxp, 0);
        for (int32_t i = 0; i < n; i++)
            if (aux->peek_hidden[i]) sb26_set_visible_slot(s, i, 1);
        aux->bbiavyren = -1;
        aux->cursor_count = 0;
        aux->peek_active = 0;
        for (int32_t i = 0; i < n; i++) aux->peek_hidden[i] = 0;
        *action_complete = 1;
        if (aux->energy == 0) *status = SB26_GAME_OVER;
    }
}

static void sb26_tick_ftyhvmeft(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux) {
    int32_t step = aux->ftyhvmeft;
    int32_t sbg = (step / 3) % 2;
    int8_t colour = sbg == 0 ? SB26_RED : SB26_BLACK;
    color_remap(s, st->ayaigjtxp, 0, 0, colour);
    color_remap(s, st->ohvavdnio, 0, 0, colour);
    int32_t step_new = step + 1;
    aux->ftyhvmeft = step_new;

    if (step_new >= 18) {
        int32_t depth = aux->stack_depth;
        int32_t top_frame = aux->stack_frame[depth - 1];
        int32_t root = st->qaagahahj[(size_t)level * SB26_MAX_FRAMES + 0];
        int skip = top_frame == root;

        if (!skip) {
            aux->fading[st->mjeqtdqvm] = 1;
            aux->fade_target[st->mjeqtdqvm] = SB26_BACKGROUND_COLOR;
        }
        aux->fading[st->ayaigjtxp] = 1;
        aux->fade_target[st->ayaigjtxp] = SB26_BACKGROUND_COLOR;
        for (int32_t k = 0; k < SB26_CURSOR_SLOTS; k++) {
            if (k >= aux->cursor_count) continue;
            aux->fading[st->cursor_base + k] = 1;
            aux->fade_target[st->cursor_base + k] = SB26_BACKGROUND_COLOR;
        }
        aux->fading[st->ohvavdnio] = 1;
        aux->fade_target[st->ohvavdnio] = SB26_PINK;

        aux->ftyhvmeft = -1;
        aux->bbiavyren = 0;
    }
}

static void sb26_tick_artsfnufc(Sprites *s, const Sb26Static *st, Sb26Aux *aux, int32_t *status,
                                uint8_t *action_complete) {
    int32_t colour = aux->artsfnufc - 1;
    if (colour < 0) colour = 0;
    color_remap(s, st->mrokwhyjs0, 0, 0, (int8_t)colour);
    color_remap(s, st->mrokwhyjs1, 0, 0, (int8_t)colour);
    aux->artsfnufc += 1;
    int done = sprite_pixels(s, st->mrokwhyjs0)[0] == SB26_BACKGROUND_COLOR;
    if (done) {
        aux->artsfnufc = -1;
        *action_complete = 1;
        if (aux->energy == 0) *status = SB26_GAME_OVER;
    }
}

static void sb26_tick_tween(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux) {
    int32_t k = aux->jlcrtmkes + 1;
    sb26_apply_tween_slot(s, st, aux->tw_role_a, aux->tw_fx_a, aux->tw_fy_a, aux->tw_fw_a,
                          aux->tw_tx_a, aux->tw_ty_a, aux->tw_tw_a, k);
    sb26_apply_tween_slot(s, st, aux->tw_role_b, aux->tw_fx_b, aux->tw_fy_b, aux->tw_fw_b,
                          aux->tw_tx_b, aux->tw_ty_b, aux->tw_tw_b, k);
    aux->jlcrtmkes = k;

    if (k == SB26_TWEEN_STEPS) {
        int32_t depth = aux->stack_depth;
        int32_t top_frame = aux->stack_frame[depth - 1];
        int32_t top_slot = aux->stack_slot[depth - 1];
        int is_item, is_fr;
        int32_t occ;
        sb26_lookup(s, st, level, top_frame, top_slot, &is_item, &occ, &is_fr);
        int modq_cond = top_slot == 0 || is_fr || aux->ppsxsxiod;
        aux->modqnpqfi = modq_cond ? 10 : 5;
        aux->jlcrtmkes = -1;
        aux->tw_role_a = -1;
        aux->tw_role_b = -1;
    }
}

static void sb26_sibihgzarf(Sprites *s, const Sb26Static *st, Sb26Aux *aux) {
    color_remap(s, st->ayaigjtxp, 0, 0, SB26_RED);
    color_remap(s, st->ohvavdnio, 0, 0, SB26_RED);
    aux->ftyhvmeft = 0;
}

static void sb26_pop_cursor(Sprites *s, const Sb26Static *st, Sb26Aux *aux) {
    int32_t idx = aux->cursor_count - 1;
    if (idx >= 0 && idx < SB26_CURSOR_SLOTS) s->alive[st->cursor_base + idx] = 0;
    aux->cursor_count = idx;
    aux->ppsxsxiod = 0;
}

static void sb26_pop_to_parent(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux,
                               int32_t target_slot) {
    int32_t depth = aux->stack_depth;
    int32_t top_frame = aux->stack_frame[depth - 1];
    int32_t new_depth = depth - 1;
    int32_t parent_frame = aux->stack_frame[new_depth - 1];
    int32_t parent_slot = aux->stack_slot[new_depth - 1];
    int is_item, is_fr;
    int32_t parent_target;
    sb26_lookup(s, st, level, parent_frame, parent_slot, &is_item, &parent_target, &is_fr);

    int32_t from_x = s->x[top_frame], from_y = s->y[top_frame], from_w = s->w[top_frame];
    int32_t to_x = s->x[parent_frame], to_y = s->y[parent_frame], to_w = s->w[parent_frame];
    int32_t old_x = s->x[target_slot], old_y = s->y[target_slot], old_w = s->w[target_slot];
    int32_t new_x = s->x[parent_target], new_y = s->y[parent_target], new_w = s->w[parent_target];

    if (aux->cursor_count > 1) {
        int32_t gp_slot = st->cursor_base + aux->cursor_count - 2;
        aux->fading[gp_slot] = 1;
        aux->fade_target[gp_slot] = SB26_GREY;
    }

    aux->stack_depth = new_depth;
    aux->ppsxsxiod = 1;
    aux->jlcrtmkes = 0;
    aux->tw_role_a = 0;
    aux->tw_fx_a = from_x;
    aux->tw_fy_a = from_y;
    aux->tw_fw_a = from_w;
    aux->tw_tx_a = to_x;
    aux->tw_ty_a = to_y;
    aux->tw_tw_a = to_w;
    aux->tw_role_b = 1;
    aux->tw_fx_b = old_x;
    aux->tw_fy_b = old_y;
    aux->tw_fw_b = old_w;
    aux->tw_tx_b = new_x;
    aux->tw_ty_b = new_y;
    aux->tw_tw_b = new_w;
}

static void sb26_exhausted(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux) {
    aux->fading[st->mjeqtdqvm] = 1;
    aux->fading[st->ayaigjtxp] = 1;
    aux->fading[st->ohvavdnio] = 1;
    aux->fade_target[st->mjeqtdqvm] = SB26_BACKGROUND_COLOR;
    aux->fade_target[st->ayaigjtxp] = SB26_BACKGROUND_COLOR;
    aux->fade_target[st->ohvavdnio] = SB26_PINK;

    int32_t n = st->num_slots;
    int32_t ci = sb26_jidx(aux->pmygakdvy + 1, SB26_MAX_CARDS);
    int32_t new_card = st->card_slots[(size_t)level * SB26_MAX_CARDS + ci];
    int32_t rd = sb26_jidx(new_card, n);
    int32_t pos_x = s->x[rd] - 1;
    int32_t pos_y = s->y[rd] - 1;

    int32_t num_cards = st->num_cards[level];
    int narrow = num_cards < SB26_MAX_CARDS;
    int32_t width = 7 * (num_cards - aux->pmygakdvy - 1) + 1;
    if (narrow) sb26_border_redraw(s, st->oyvbxwyug, width, 0);
    int32_t pos_x2 = narrow ? pos_x : pos_x - 28;
    set_position(s, st->oyvbxwyug, pos_x2, pos_y);
    sb26_set_visible_slot(s, st->oyvbxwyug, 1);

    aux->japgbruyb = 0;
}

static void sb26_push_frame(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux,
                            int32_t *next_order, int32_t target_slot) {
    int32_t depth = aux->stack_depth;
    int32_t top_frame = aux->stack_frame[depth - 1];

    if (aux->cursor_count > 0) {
        int32_t prev = aux->cursor_count - 1;
        if (prev < 0) prev = 0;
        int32_t prev_slot = st->cursor_base + prev;
        aux->fading[prev_slot] = 1;
        aux->fade_target[prev_slot] = SB26_BACKGROUND_COLOR;
    }

    int32_t new_cursor = st->cursor_base + aux->cursor_count;
    set_position(s, new_cursor, s->x[target_slot], s->y[target_slot]);
    color_remap(s, new_cursor, 0, 0, SB26_GREY);
    add_sprite(s, new_cursor, *next_order);
    sb26_set_visible_slot(s, new_cursor, 1);

    int32_t pw = s->atlas->pw;
    int8_t wanted_colour = sprite_pixels(s, target_slot)[(size_t)1 * pw + 1];
    int32_t new_rank = 0;
    for (int32_t k = 0; k < SB26_MAX_FRAMES; k++) {
        int32_t fs = st->qaagahahj[(size_t)level * SB26_MAX_FRAMES + k];
        int32_t clipped = fs < 0 ? 0 : fs;
        int8_t fc = sprite_pixels(s, clipped)[0];
        if (fs >= 0 && fc == wanted_colour) {
            new_rank = k;
            break;
        }
    }
    int32_t new_frame = st->qaagahahj[(size_t)level * SB26_MAX_FRAMES + new_rank];

    int32_t from_x = s->x[top_frame], from_y = s->y[top_frame], from_w = s->w[top_frame];
    int32_t to_x = s->x[new_frame], to_y = s->y[new_frame], to_w = s->w[new_frame];
    int32_t old_x = s->x[target_slot], old_y = s->y[target_slot], old_w = s->w[target_slot];
    int is_item, is_fr;
    int32_t first_child;
    sb26_lookup(s, st, level, new_frame, 0, &is_item, &first_child, &is_fr);
    int32_t new_x = s->x[first_child], new_y = s->y[first_child], new_w = s->w[first_child];

    aux->stack_frame[depth] = new_frame;
    aux->stack_slot[depth] = 0;
    aux->stack_depth = depth + 1;
    aux->cursor_count += 1;
    aux->jlcrtmkes = 0;
    aux->tw_role_a = 0;
    aux->tw_fx_a = from_x;
    aux->tw_fy_a = from_y;
    aux->tw_fw_a = from_w;
    aux->tw_tx_a = to_x;
    aux->tw_ty_a = to_y;
    aux->tw_tw_a = to_w;
    aux->tw_role_b = 1;
    aux->tw_fx_b = old_x;
    aux->tw_fy_b = old_y;
    aux->tw_fw_b = old_w;
    aux->tw_tx_b = new_x;
    aux->tw_ty_b = new_y;
    aux->tw_tw_b = new_w;
    *next_order += 1;
}

static void sb26_check(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux,
                       int32_t *next_order) {
    int32_t depth = aux->stack_depth;
    int32_t top_frame = aux->stack_frame[depth - 1];
    int32_t top_slot = aux->stack_slot[depth - 1];
    int is_item, is_frameref;
    int32_t target_slot;
    sb26_lookup(s, st, level, top_frame, top_slot, &is_item, &target_slot, &is_frameref);
    int is_plain = is_item && !is_frameref;

    int32_t pw = s->atlas->pw;
    int32_t card_slot = st->card_slots[(size_t)level * SB26_MAX_CARDS + aux->pmygakdvy];
    int confirmed = sprite_pixels(s, card_slot)[(size_t)1 * pw + 1] != -1;

    if (!confirmed && is_plain) {
        int match = sprite_pixels(s, target_slot)[(size_t)1 * pw + 1] ==
                    sprite_pixels(s, card_slot)[0];
        if (match)
            aux->xjxrqgaqw = 0;
        else
            sb26_sibihgzarf(s, st, aux);
        return;
    }

    int win = (aux->pmygakdvy == st->num_cards[level] - 1) && confirmed;
    if (win) {
        int32_t n = st->num_slots;
        aux->fading[st->mjeqtdqvm] = 1;
        aux->fading[st->ayaigjtxp] = 1;
        aux->fade_target[st->mjeqtdqvm] = SB26_BACKGROUND_COLOR;
        aux->fade_target[st->ayaigjtxp] = SB26_BACKGROUND_COLOR;
        for (int32_t i = 0; i < n; i++) {
            if (!st->bg_mask[(size_t)level * n + i]) continue;
            aux->fading[i] = 1;
            aux->fade_target[i] = SB26_BLACK;
        }
        for (int32_t i = 0; i < n; i++)
            if (aux->peek_hidden[i]) sb26_set_visible_slot(s, i, 1);
        aux->lmvwmlqtw = 0;
        aux->peek_active = 0;
        return;
    }

    int advance_cond = aux->ppsxsxiod || is_plain;
    if (advance_cond) {
        if (aux->ppsxsxiod) sb26_pop_cursor(s, st, aux);
        depth = aux->stack_depth;
        top_frame = aux->stack_frame[depth - 1];
        top_slot = aux->stack_slot[depth - 1];
        int32_t new_slot = top_slot + 1;
        int32_t nslots = st->frame_children[(size_t)level * st->num_slots + top_frame];
        if (new_slot < nslots) {
            int is_item2, is_fr2;
            int32_t new_child;
            sb26_lookup(s, st, level, top_frame, new_slot, &is_item2, &new_child, &is_fr2);
            int32_t from_x = s->x[target_slot], from_y = s->y[target_slot], from_w = s->w[target_slot];
            int32_t to_x = s->x[new_child], to_y = s->y[new_child], to_w = s->w[new_child];
            int32_t new_pmy = aux->pmygakdvy + 1;
            int32_t new_card = st->card_slots[(size_t)level * SB26_MAX_CARDS + new_pmy];
            int32_t oc_x = s->x[card_slot], oc_y = s->y[card_slot];
            int32_t nc_x = s->x[new_card], nc_y = s->y[new_card];
            aux->stack_slot[depth - 1] = new_slot;
            aux->pmygakdvy = new_pmy;
            aux->jlcrtmkes = 0;
            aux->tw_role_a = 1;
            aux->tw_fx_a = from_x;
            aux->tw_fy_a = from_y;
            aux->tw_fw_a = from_w;
            aux->tw_tx_a = to_x;
            aux->tw_ty_a = to_y;
            aux->tw_tw_a = to_w;
            aux->tw_role_b = 2;
            aux->tw_fx_b = oc_x;
            aux->tw_fy_b = oc_y;
            aux->tw_fw_b = 0;
            aux->tw_tx_b = nc_x;
            aux->tw_ty_b = nc_y;
            aux->tw_tw_b = 0;
        } else if (aux->stack_depth > 1) {
            sb26_pop_to_parent(s, st, level, aux, target_slot);
        } else {
            sb26_exhausted(s, st, level, aux);
        }
        return;
    }

    if (is_item && is_frameref) {
        int32_t d2 = aux->stack_depth;
        int earlier = 0;
        for (int32_t j = 0; j < d2 - 1; j++) {
            if (aux->stack_frame[j] == top_frame && aux->stack_slot[j] == 0) {
                earlier = 1;
                break;
            }
        }
        int prev_slot_zero = d2 >= 2 ? aux->stack_slot[d2 - 2] == 0 : 0;
        int guard = top_slot == 0 && earlier && prev_slot_zero && d2 >= 2;
        if (guard)
            sb26_sibihgzarf(s, st, aux);
        else
            sb26_push_frame(s, st, level, aux, next_order, target_slot);
    } else {
        sb26_sibihgzarf(s, st, aux);
    }
}

static void sb26_tick_modqnpqfi(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux,
                                int32_t *next_order) {
    int32_t m = aux->modqnpqfi - 1;
    aux->modqnpqfi = m;
    if (m == 0) sb26_check(s, st, level, aux, next_order);
}

void sb26_aux_alloc(Sb26Aux *aux, int32_t num_slots) {
    aux->num_slots = num_slots;
    aux->fading = calloc((size_t)num_slots, sizeof(uint8_t));
    aux->fade_target = calloc((size_t)num_slots, sizeof(int32_t));
    aux->undo_x = calloc((size_t)SB26_MAX_UNDO * num_slots, sizeof(int32_t));
    aux->undo_y = calloc((size_t)SB26_MAX_UNDO * num_slots, sizeof(int32_t));
    aux->undo_vis = calloc((size_t)SB26_MAX_UNDO * num_slots, sizeof(uint8_t));
    aux->peek_hidden = calloc((size_t)num_slots, sizeof(uint8_t));
    aux->click_off = calloc((size_t)num_slots, sizeof(uint8_t));
}

void sb26_aux_free(Sb26Aux *aux) {
    free(aux->fading);
    free(aux->fade_target);
    free(aux->undo_x);
    free(aux->undo_y);
    free(aux->undo_vis);
    free(aux->peek_hidden);
    free(aux->click_off);
    aux->fading = NULL;
    aux->fade_target = NULL;
    aux->undo_x = NULL;
    aux->undo_y = NULL;
    aux->undo_vis = NULL;
    aux->peek_hidden = NULL;
    aux->click_off = NULL;
}

void sb26_zero_aux(Sb26Aux *aux) {
    int32_t n = aux->num_slots;
    aux->energy = 0;
    aux->selected = -1;
    for (int32_t i = 0; i < SB26_MAX_STACK; i++) {
        aux->stack_frame[i] = 0;
        aux->stack_slot[i] = 0;
    }
    aux->stack_depth = 1;
    aux->cursor_count = 0;
    aux->pmygakdvy = 0;
    aux->ppsxsxiod = 0;
    for (int32_t i = 0; i < n; i++) {
        aux->fading[i] = 0;
        aux->fade_target[i] = 0;
        aux->click_off[i] = 0;
        aux->peek_hidden[i] = 0;
    }
    memset(aux->undo_x, 0, sizeof(int32_t) * (size_t)SB26_MAX_UNDO * n);
    memset(aux->undo_y, 0, sizeof(int32_t) * (size_t)SB26_MAX_UNDO * n);
    memset(aux->undo_vis, 0, sizeof(uint8_t) * (size_t)SB26_MAX_UNDO * n);
    aux->undo_depth = 0;
    aux->japgbruyb = -1;
    aux->lmvwmlqtw = -1;
    aux->xjxrqgaqw = -1;
    aux->bbiavyren = -1;
    aux->ftyhvmeft = -1;
    aux->artsfnufc = -1;
    aux->modqnpqfi = 0;
    aux->jlcrtmkes = -1;
    aux->tw_role_a = -1;
    aux->tw_fx_a = aux->tw_fy_a = aux->tw_fw_a = aux->tw_tx_a = aux->tw_ty_a = aux->tw_tw_a = 0;
    aux->tw_role_b = -1;
    aux->tw_fx_b = aux->tw_fy_b = aux->tw_fw_b = aux->tw_tx_b = aux->tw_ty_b = aux->tw_tw_b = 0;
    aux->peek_active = 0;
    memset(aux->snapshot, 0, sizeof aux->snapshot);
}

void sb26_on_set_level(Sprites *s, const Sb26Static *st, int32_t level, Sb26Aux *aux,
                       int32_t *next_order) {
    sb26_zero_aux(aux);
    aux->energy = SB26_INITIAL_ENERGY;

    int32_t n = st->num_slots;
    int32_t zp = st->zpwrpmkvsv_slot[level];
    if (zp >= 0 && zp < n) s->alive[zp] = 0;

    const Atlas *a = s->atlas;
    for (int32_t i = 0; i < n; i++) {
        int is_item = s->tags[(size_t)i * a->num_tags + st->item_tag];
        aux->click_off[i] = (uint8_t)(is_item && s->y[i] <= SB26_Y_THRESHOLD);
    }

    for (int32_t k = 0; k < SB26_MAX_TRAY; k++) {
        int32_t ghost = st->tray_ghost_base + k;
        int valid = k < st->n_tray[level];
        if (valid) {
            int32_t src = st->tray_item_slot[(size_t)level * SB26_MAX_TRAY + k];
            s->x[ghost] = s->x[src];
            s->y[ghost] = s->y[src];
        }
        s->alive[ghost] = (uint8_t)valid;
        s->order[ghost] = *next_order + k;
    }
    for (int32_t k = 0; k < SB26_MAX_TRAY; k++)
        if (k < st->n_tray[level]) sb26_set_visible_slot(s, st->tray_ghost_base + k, 0);
    *next_order += st->n_tray[level];

    set_position(s, st->mrokwhyjs0, -10, -10);
    color_remap(s, st->mrokwhyjs0, 0, 0, SB26_BACKGROUND_COLOR);
    add_sprite(s, st->mrokwhyjs0, *next_order);
    sb26_set_visible_slot(s, st->mrokwhyjs0, 1);
    (*next_order)++;

    set_position(s, st->mrokwhyjs1, -10, -10);
    color_remap(s, st->mrokwhyjs1, 0, 0, SB26_BACKGROUND_COLOR);
    add_sprite(s, st->mrokwhyjs1, *next_order);
    sb26_set_visible_slot(s, st->mrokwhyjs1, 1);
    (*next_order)++;

    int32_t root = st->qaagahahj[(size_t)level * SB26_MAX_FRAMES + 0];
    int32_t rx = s->x[root], ry = s->y[root], rw = s->w[root];
    sb26_dagvovvbpp(s, st, st->mjeqtdqvm, 1, rx, ry, rw, rx, ry, rw, 0);
    add_sprite(s, st->mjeqtdqvm, *next_order);
    sb26_set_visible_slot(s, st->mjeqtdqvm, 1);
    (*next_order)++;

    color_remap(s, st->ayaigjtxp, 0, 0, SB26_BACKGROUND_COLOR);
    add_sprite(s, st->ayaigjtxp, *next_order);
    sb26_set_visible_slot(s, st->ayaigjtxp, 0);
    (*next_order)++;

    color_remap(s, st->ohvavdnio, 0, 0, SB26_PINK);
    set_position(s, st->ohvavdnio, -100, -100);
    add_sprite(s, st->ohvavdnio, *next_order);
    sb26_set_visible_slot(s, st->ohvavdnio, 1);
    (*next_order)++;

    add_sprite(s, st->oyvbxwyug, *next_order);
    sb26_set_visible_slot(s, st->oyvbxwyug, 0);
    (*next_order)++;

    sb26_push_undo(s, st, aux);
}

void sb26_step_once(Sprites *s, const Camera *camera, RenderScratch *scratch,
                    const Sb26Static *st, int32_t level, int32_t action_id, int32_t action_x,
                    int32_t action_y, int32_t action_count, Sb26Aux *aux, int32_t *next_order,
                    int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete) {
    (void)action_count;
    int valid = action_id == SB26_ACTION5 || action_id == SB26_ACTION6 ||
               action_id == SB26_ACTION7;
    if (!valid) {
        *action_complete = 1;
        return;
    }

    sb26_advance_fades(s, aux);

    if (aux->japgbruyb >= 0) {
        sb26_tick_japgbruyb(s, st, aux);
        return;
    }
    if (aux->lmvwmlqtw >= 0) {
        sb26_tick_lmvwmlqtw(st, level, aux, score, status, next_level, action_complete);
        return;
    }
    if (aux->xjxrqgaqw >= 0) {
        sb26_tick_xjxrqgaqw(s, st, level, aux);
        return;
    }
    if (aux->bbiavyren >= 0) {
        sb26_tick_bbiavyren(s, st, level, aux, status, action_complete);
        return;
    }
    if (aux->ftyhvmeft >= 0) {
        sb26_tick_ftyhvmeft(s, st, level, aux);
        return;
    }
    if (aux->jlcrtmkes >= 0) {
        sb26_tick_tween(s, st, level, aux);
        return;
    }
    if (aux->modqnpqfi > 0) {
        sb26_tick_modqnpqfi(s, st, level, aux, next_order);
        return;
    }
    if (aux->artsfnufc >= 0) {
        sb26_tick_artsfnufc(s, st, aux, status, action_complete);
        return;
    }
    if (action_id == SB26_ACTION5) {
        sb26_handle_action5(s, camera, scratch, st, level, aux);
        return;
    }
    sb26_handle_default(s, st, aux, action_id, action_x, action_y, action_complete);
}

void sb26_render_interface(int8_t *frame, const Sprites *s, const Camera *camera,
                           RenderScratch *scratch, const Sb26Static *st, const Sb26Aux *aux) {
    (void)st;
    Camera sentinel = *camera;
    sentinel.background = -1;
    sentinel.letter_box = -1;
    int8_t coverage[FRAME_SIZE * FRAME_SIZE];
    render(s, &sentinel, scratch, coverage);
    if (aux->peek_active) {
        for (int32_t i = 0; i < FRAME_SIZE * FRAME_SIZE; i++)
            if (coverage[i] == -1) frame[i] = aux->snapshot[i];
    }

    int32_t filled = (FRAME_SIZE * aux->energy + SB26_INITIAL_ENERGY - 1) / SB26_INITIAL_ENERGY;
    int8_t *row = frame + (size_t)SB26_Y_THRESHOLD * FRAME_SIZE;
    for (int32_t c = 0; c < FRAME_SIZE; c++)
        row[c] = (int8_t)(c < filled ? SB26_HUD_FILLED : SB26_HUD_EMPTY);
}
