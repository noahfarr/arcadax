#include "r11l.h"

#include <string.h>

enum { R11L_ACTION6 = 6 };
enum { R11L_WIN = 2, R11L_GAME_OVER = 3 };
enum { R11L_MAX_ACTIONS = 60 };
enum { R11L_MAX_HAZARD_HITS = 5 };
enum { R11L_PIECE_IDLE = 3, R11L_PIECE_SELECTED = 0 };
enum { R11L_BUDGET_FILLED = 0, R11L_BUDGET_EMPTY = 5 };
enum {
	R11L_GUIDE_COLOUR = 1,
	R11L_GUIDE_TARGET_A = 5,
	R11L_GUIDE_TARGET_B = 10
};
enum { R11L_LINE_STEPS = 140 };

static inline int32_t r11l_floordiv(int32_t a, int32_t b)
{
	int32_t q = a / b;
	if ((a % b != 0) && ((a < 0) != (b < 0)))
		q -= 1;
	return q;
}

static inline int32_t r11l_clampi(int32_t v, int32_t lo, int32_t hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static int32_t r11l_find_piece(const struct arc_sprites *s,
			       const struct r11l_static *st, int32_t level,
			       int32_t wx, int32_t wy)
{
	const int32_t *order = st->pieces_order + (size_t)level * R11L_NPIECES;
	for (int32_t rank = 0; rank < R11L_NPIECES; rank++) {
		int32_t slot = order[rank];
		if (slot < 0)
			continue;
		if (wx >= s->x[slot] && wx < s->x[slot] + s->w[slot] &&
		    wy >= s->y[slot] && wy < s->y[slot] + s->h[slot])
			return slot;
	}
	return -1;
}

static void r11l_select_piece(struct arc_sprites *s, struct r11l_aux *aux,
			      int32_t slot)
{
	int32_t prev = aux->selected;
	if (prev >= 0)
		arc_color_remap(s, prev, 1, R11L_PIECE_SELECTED,
				R11L_PIECE_IDLE);
	if (slot >= 0)
		arc_color_remap(s, slot, 1, R11L_PIECE_IDLE,
				R11L_PIECE_SELECTED);
	aux->selected = slot;
}

static void r11l_recompute_group(struct arc_sprites *s,
				 const struct r11l_static *st, int32_t level,
				 int32_t group)
{
	int32_t base = level * R11L_G + group;
	int32_t count = st->group_piece_count[base];
	const int32_t *slots =
		st->group_piece_slots + (size_t)base * st->max_group_pieces;
	int32_t sx = 0, sy = 0;
	for (int32_t k = 0; k < count; k++) {
		int32_t i = slots[k];
		sx += s->x[i] + s->w[i] / 2;
		sy += s->y[i] + s->h[i] / 2;
	}
	int32_t divisor = count > 0 ? count : 1;
	int32_t cx = r11l_floordiv(sx, divisor);
	int32_t cy = r11l_floordiv(sy, divisor);

	int32_t target_slot = st->group_target_slot[base];
	if (target_slot < 0)
		return;
	int32_t tx = cx - s->w[target_slot] / 2;
	int32_t ty = cy - s->h[target_slot] / 2;
	arc_set_position(s, target_slot, tx, ty);
}

static void r11l_maybe_merge(struct arc_sprites *s,
			     const struct r11l_static *st, int32_t level,
			     int32_t group)
{
	int32_t base = level * R11L_G + group;
	if (!st->group_is_composite[base])
		return;
	int32_t target_slot = st->group_target_slot[base];
	if (target_slot < 0)
		return;

	int32_t count = st->fragment_count[level];
	const int32_t *frags =
		st->fragment_slots + (size_t)level * st->max_fragments;
	int32_t area = s->atlas->ph * s->atlas->pw;
	for (int32_t k = 0; k < count; k++) {
		int32_t i = frags[k];
		if (!s->alive[i])
			continue;
		if (!arc_collides_pair(s, target_slot, i, 0))
			continue;
		const int8_t *frag_px = arc_sprite_pixels(s, i);
		int8_t *tgt_px = arc_sprite_pixels_mut(s, target_slot);
		for (int32_t p = 0; p < area; p++) {
			int8_t v = frag_px[p];
			if (v != -1 && v != 0)
				tgt_px[p] = v;
		}
		arc_remove_sprite(s, i);
	}
}

static int r11l_hazard_hit(const struct arc_sprites *s,
			   const struct r11l_static *st, int32_t level,
			   int32_t group)
{
	int32_t target_slot = st->group_target_slot[level * R11L_G + group];
	if (target_slot < 0)
		return 0;
	int32_t count = st->hazard_count[level];
	const int32_t *hz = st->hazard_slots + (size_t)level * st->max_hazards;
	for (int32_t k = 0; k < count; k++)
		if (arc_collides_pair(s, target_slot, hz[k], 0))
			return 1;
	return 0;
}

static int r11l_key_matched(const struct arc_sprites *s,
			    const struct r11l_static *st, int32_t level,
			    int32_t k)
{
	int32_t base = level * R11L_K + k;
	int32_t clue_slot = st->key_clue_slot[base];
	int32_t clue_c = clue_slot >= 0 ? clue_slot : 0;
	int32_t target_slot = st->key_target_slot[base];
	int has_direct = target_slot >= 0;
	int32_t tgt_c = has_direct ? target_slot : 0;
	int direct = has_direct && arc_collides_pair(s, clue_c, tgt_c, 0);

	const uint8_t *clue_colours = st->key_colour_set + (size_t)base * 15;
	int comp_match = 0;
	int32_t area = s->atlas->ph * s->atlas->pw;
	for (int32_t c = 0; c < R11L_NCOMP; c++) {
		int32_t comp_slot = st->composite_slot[level * R11L_NCOMP + c];
		int comp_active = comp_slot >= 0;
		if (!comp_active)
			continue;
		int coll = arc_collides_pair(s, clue_c, comp_slot, 0);
		if (!coll)
			continue;
		uint8_t present[15];
		memset(present, 0, sizeof present);
		const int8_t *patch = arc_sprite_pixels(s, comp_slot);
		for (int32_t p = 0; p < area; p++) {
			int8_t v = patch[p];
			if (v >= 1 && v <= 15)
				present[v - 1] = 1;
		}
		int eq = 1;
		for (int32_t cc = 0; cc < 15; cc++)
			if (present[cc] != clue_colours[cc]) {
				eq = 0;
				break;
			}
		comp_match = comp_match || eq;
	}
	return has_direct ? direct : comp_match;
}

static int r11l_win_check(const struct arc_sprites *s,
			  const struct r11l_static *st, int32_t level)
{
	for (int32_t k = 0; k < R11L_K; k++) {
		int32_t base = level * R11L_K + k;
		int active = st->key_clue_slot[base] >= 0;
		int skip = st->key_skip_win[base];
		int need = active && !skip;
		if (need && !r11l_key_matched(s, st, level, k))
			return 0;
	}
	return 1;
}

static int r11l_check_blocked(struct arc_sprites *s,
			      const struct r11l_static *st, int32_t level,
			      int32_t sel, int32_t tx, int32_t ty)
{
	int32_t ox = s->x[sel], oy = s->y[sel];
	arc_set_position(s, sel, tx, ty);
	int blocked = 0;
	int32_t count = st->wall_count[level];
	const int32_t *walls = st->wall_slots + (size_t)level * st->max_walls;
	for (int32_t k = 0; k < count && !blocked; k++)
		if (arc_collides_pair(s, sel, walls[k], 0))
			blocked = 1;
	arc_set_position(s, sel, ox, oy);
	return blocked;
}

static void r11l_handle_click(struct arc_sprites *s,
			      const struct arc_camera *cam,
			      const struct r11l_static *st, int32_t level,
			      int32_t action_id, int32_t action_x,
			      int32_t action_y, struct r11l_aux *aux,
			      uint8_t *action_complete)
{
	int32_t scale, x_pad, y_pad;
	arc_scale_and_offset(cam, &scale, &x_pad, &y_pad);
	int32_t dx = action_x - x_pad, dy = action_y - y_pad;
	int32_t gx = dx >= 0 ? dx / scale : -1;
	int32_t gy = dy >= 0 ? dy / scale : -1;
	int on_board = gx >= 0 && gy >= 0 && gx < cam->width &&
		       gy < cam->height && action_id == R11L_ACTION6;

	if (!on_board) {
		*action_complete = 1;
		return;
	}

	int32_t wx = gx + cam->x, wy = gy + cam->y;
	int32_t hit_slot = r11l_find_piece(s, st, level, wx, wy);

	if (hit_slot >= 0) {
		r11l_select_piece(s, aux, hit_slot);
		*action_complete = 1;
		return;
	}

	int32_t selected = aux->selected;
	if (selected < 0) {
		*action_complete = 1;
		return;
	}

	int32_t tx = wx - s->w[selected] / 2;
	int32_t ty = wy - s->h[selected] / 2;
	if (r11l_check_blocked(s, st, level, selected, tx, ty)) {
		*action_complete = 1;
		return;
	}

	aux->dragging = 1;
	aux->start_x = s->x[selected];
	aux->start_y = s->y[selected];
	aux->target_x = tx;
	aux->target_y = ty;
	aux->tween = 0;
	aux->reverting = 0;
	aux->waiting = 0;
}

static void r11l_update_key(struct arc_sprites *s, const struct r11l_static *st,
			    int32_t level, int32_t k, struct r11l_aux *aux,
			    int32_t *next_order)
{
	int32_t base = level * R11L_K + k;
	int32_t clue_slot = st->key_clue_slot[base];
	int active = clue_slot >= 0;
	int32_t icon_slot = st->icon_base + k;

	int matched = active && r11l_key_matched(s, st, level, k);
	int was_matched = aux->key_was_matched[k];
	int blinking = aux->key_blinking[k];

	int entering = matched && !was_matched && !blinking && active;
	int leaving = !matched && was_matched && active;

	if (leaving) {
		arc_remove_sprite(s, icon_slot);
		aux->key_blinking[k] = 0;
		aux->key_was_matched[k] = 0;
	}
	if (entering) {
		aux->key_blinking[k] = 1;
		aux->key_was_matched[k] = 1;
		aux->key_blink_ticks[k] = 0;
		aux->key_blink_blinks[k] = 0;
	}
	int now_blinking = entering ? 1 : (leaving ? 0 : blinking);
	if (!(now_blinking && active))
		return;

	int32_t t = aux->key_blink_ticks[k] + 1;
	aux->key_blink_ticks[k] = t;
	int want_visible = (t % 2 == 0) && (((t / 2) % 2) == 1);
	int want_hidden = (t % 2 == 0) && !want_visible;

	if (want_visible && !s->alive[icon_slot]) {
		arc_set_position(s, icon_slot, s->x[clue_slot],
				 s->y[clue_slot]);
		arc_add_sprite(s, icon_slot, *next_order);
		*next_order += 1;
	}
	if (want_hidden && s->alive[icon_slot])
		arc_remove_sprite(s, icon_slot);

	if (t % 4 == 0) {
		int32_t b = aux->key_blink_blinks[k] + 1;
		aux->key_blink_blinks[k] = b;
		if (b >= 5) {
			if (!s->alive[icon_slot]) {
				arc_set_position(s, icon_slot, s->x[clue_slot],
						 s->y[clue_slot]);
				arc_add_sprite(s, icon_slot, *next_order);
				*next_order += 1;
			}
			aux->key_blinking[k] = 0;
		}
	}
}

static void r11l_update_blinks(struct arc_sprites *s,
			       const struct r11l_static *st, int32_t level,
			       struct r11l_aux *aux, int32_t *next_order)
{
	for (int32_t k = 0; k < R11L_K; k++)
		r11l_update_key(s, st, level, k, aux, next_order);
}

static int r11l_any_key_blinking(const struct r11l_aux *aux)
{
	for (int32_t k = 0; k < R11L_K; k++)
		if (aux->key_blinking[k])
			return 1;
	return 0;
}

static void r11l_wobble_tick(struct arc_sprites *s,
			     const struct r11l_static *st, struct r11l_aux *aux,
			     int32_t *next_order)
{
	int32_t slot = st->wobble_icon_slot;
	int32_t t = aux->wobble_ticks + 1;
	aux->wobble_ticks = t;
	int want_visible = (t % 2 == 0) && (((t / 2) % 2) == 1);
	int want_hidden = (t % 2 == 0) && !want_visible;

	if (want_visible && !s->alive[slot]) {
		arc_set_position(s, slot, aux->wobble_x, aux->wobble_y);
		arc_add_sprite(s, slot, *next_order);
		*next_order += 1;
	}
	if (want_hidden && s->alive[slot])
		arc_remove_sprite(s, slot);

	if (t % 4 == 0) {
		int32_t b = aux->wobble_blinks + 1;
		aux->wobble_blinks = b;
		if (b >= 5) {
			if (s->alive[slot])
				arc_remove_sprite(s, slot);
			aux->wobbling = 0;
			aux->wobble_blinks = 0;
			aux->wobble_ticks = 0;
			aux->reverting = 1;
		}
	}
}

static void r11l_forward_tween(struct arc_sprites *s,
			       const struct r11l_static *st, int32_t level,
			       struct r11l_aux *aux, int32_t *status,
			       uint8_t *action_complete)
{
	int32_t selected = aux->selected;
	int32_t sel_c = selected >= 0 ? selected : 0;
	int32_t group = st->piece_group[(size_t)level * st->num_slots + sel_c];
	int32_t group_c = group >= 0 ? group : 0;

	arc_set_position(s, selected, aux->target_x, aux->target_y);
	aux->tween = 1;
	r11l_recompute_group(s, st, level, group_c);
	r11l_maybe_merge(s, st, level, group_c);

	if (r11l_hazard_hit(s, st, level, group_c)) {
		int32_t hits = aux->hazard_hits + 1;
		aux->hazard_hits = hits;
		if (hits >= R11L_MAX_HAZARD_HITS) {
			aux->dragging = 0;
			aux->reverting = 0;
			*status = R11L_GAME_OVER;
			*action_complete = 1;
		} else {
			int32_t target_slot =
				st->group_target_slot[level * R11L_G + group_c];
			aux->wobbling = 1;
			aux->wobble_blinks = 0;
			aux->wobble_ticks = 0;
			aux->wobble_x = s->x[target_slot];
			aux->wobble_y = s->y[target_slot];
		}
	} else {
		int win = r11l_win_check(s, st, level);
		aux->won_wait = (uint8_t)win;
		aux->waiting = (uint8_t)!win;
	}
}

static void r11l_reverse_tween(struct arc_sprites *s,
			       const struct r11l_static *st, int32_t level,
			       struct r11l_aux *aux, uint8_t *action_complete)
{
	int32_t selected = aux->selected;
	int32_t sel_c = selected >= 0 ? selected : 0;
	int32_t group = st->piece_group[(size_t)level * st->num_slots + sel_c];
	int32_t group_c = group >= 0 ? group : 0;

	arc_set_position(s, selected, aux->start_x, aux->start_y);
	aux->tween = 0;
	r11l_recompute_group(s, st, level, group_c);
	aux->dragging = 0;
	aux->reverting = 0;
	*action_complete = 1;
}

static void r11l_continue_drag(struct arc_sprites *s,
			       const struct r11l_static *st, int32_t level,
			       struct r11l_aux *aux, int32_t *next_order,
			       int32_t *score, int32_t *status,
			       uint8_t *next_level, uint8_t *action_complete)
{
	r11l_update_blinks(s, st, level, aux, next_order);

	if (aux->wobbling) {
		r11l_wobble_tick(s, st, aux, next_order);
		return;
	}
	if (aux->won_wait) {
		if (!r11l_any_key_blinking(aux)) {
			int is_last = level == st->num_levels - 1;
			*score += 1;
			*next_level = (uint8_t)!is_last;
			if (is_last)
				*status = R11L_WIN;
			aux->dragging = 0;
			aux->won_wait = 0;
			*action_complete = 1;
		}
		return;
	}
	if (aux->waiting) {
		if (!r11l_any_key_blinking(aux)) {
			aux->dragging = 0;
			aux->waiting = 0;
			*action_complete = 1;
		}
		return;
	}
	if (aux->reverting) {
		r11l_reverse_tween(s, st, level, aux, action_complete);
	} else {
		r11l_forward_tween(s, st, level, aux, status, action_complete);
	}
}

void r11l_zero_aux(struct r11l_aux *aux)
{
	aux->dragging = 0;
	aux->selected = -1;
	aux->start_x = 0;
	aux->start_y = 0;
	aux->target_x = 0;
	aux->target_y = 0;
	aux->tween = 0;
	aux->reverting = 0;
	aux->waiting = 0;
	aux->won_wait = 0;
	aux->hazard_hits = 0;
	aux->wobbling = 0;
	aux->wobble_blinks = 0;
	aux->wobble_ticks = 0;
	aux->wobble_x = 0;
	aux->wobble_y = 0;
	for (int32_t k = 0; k < R11L_K; k++) {
		aux->key_blinking[k] = 0;
		aux->key_blink_blinks[k] = 0;
		aux->key_blink_ticks[k] = 0;
		aux->key_was_matched[k] = 0;
	}
}

void r11l_on_set_level(struct arc_sprites *sprites,
		       const struct r11l_static *st, int32_t level,
		       struct r11l_aux *aux, int32_t *next_order)
{
	const int32_t *order = st->pieces_order + (size_t)level * R11L_NPIECES;
	int32_t rank0 = order[0];
	for (int32_t rank = 1; rank < R11L_NPIECES; rank++) {
		int32_t slot = order[rank];
		if (slot < 0)
			continue;
		arc_color_remap(sprites, slot, 1, R11L_PIECE_SELECTED,
				R11L_PIECE_IDLE);
	}
	for (int32_t g = 0; g < R11L_G; g++) {
		int32_t base = level * R11L_G + g;
		if (st->group_piece_count[base] >= 2)
			r11l_recompute_group(sprites, st, level, g);
	}
	r11l_zero_aux(aux);
	aux->selected = rank0;
	*next_order = st->num_slots;
}

void r11l_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct r11l_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct r11l_aux *aux,
		    int32_t *next_order, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete)
{
	if (action_count >= R11L_MAX_ACTIONS) {
		*status = R11L_GAME_OVER;
		*action_complete = 1;
		return;
	}
	if (aux->dragging) {
		r11l_continue_drag(sprites, st, level, aux, next_order, score,
				   status, next_level, action_complete);
	} else {
		r11l_handle_click(sprites, camera, st, level, action_id,
				  action_x, action_y, aux, action_complete);
	}
}

static void r11l_draw_budget(int8_t *frame, int32_t action_count)
{
	int32_t remaining = r11l_clampi(R11L_MAX_ACTIONS - action_count, 0,
					R11L_MAX_ACTIONS);
	int32_t total = ARC_FRAME_SIZE * remaining;
	int32_t whole = total / R11L_MAX_ACTIONS;
	int32_t rest = total % R11L_MAX_ACTIONS;
	int round_up = (2 * rest > R11L_MAX_ACTIONS) ||
		       (2 * rest == R11L_MAX_ACTIONS && (whole % 2 == 1));
	int32_t filled = whole + (round_up ? 1 : 0);
	if (filled > ARC_FRAME_SIZE)
		filled = ARC_FRAME_SIZE;
	for (int32_t r = 0; r < ARC_FRAME_SIZE; r++) {
		int8_t v = (ARC_FRAME_SIZE - r <= filled) ? R11L_BUDGET_FILLED :
							    R11L_BUDGET_EMPTY;
		frame[(size_t)r * ARC_FRAME_SIZE] = v;
	}
}

static void r11l_draw_line(int8_t *frame, int32_t x0, int32_t y0, int32_t x1,
			   int32_t y1, int active)
{
	int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
	int32_t dy = y1 > y0 ? y1 - y0 : y0 - y1;
	int32_t sx = x0 < x1 ? 1 : -1;
	int32_t sy = y0 < y1 ? 1 : -1;
	int32_t err = dx - dy;
	int32_t x = x0, y = y0;
	for (int32_t step = 0; step < R11L_LINE_STEPS; step++) {
		int inb = x >= 0 && x < ARC_FRAME_SIZE && y >= 0 &&
			  y < ARC_FRAME_SIZE;
		int32_t yc = r11l_clampi(y, 0, ARC_FRAME_SIZE - 1);
		int32_t xc = r11l_clampi(x, 0, ARC_FRAME_SIZE - 1);
		int8_t cur = frame[(size_t)yc * ARC_FRAME_SIZE + xc];
		int want = active && inb &&
			   (cur == R11L_GUIDE_TARGET_A ||
			    cur == R11L_GUIDE_TARGET_B);
		if (want)
			frame[(size_t)yc * ARC_FRAME_SIZE + xc] =
				R11L_GUIDE_COLOUR;

		if (x == x1 && y == y1)
			break;
		int32_t e2 = err * 2;
		int step_x = e2 > -dy;
		int step_y = e2 < dx;
		err = err - (step_x ? dy : 0) + (step_y ? dx : 0);
		if (step_x)
			x += sx;
		if (step_y)
			y += sy;
	}
}

static void r11l_draw_guides(int8_t *frame, const struct arc_sprites *s,
			     const struct arc_camera *cam,
			     const struct r11l_static *st, int32_t level)
{
	int32_t scale, x_off, y_off;
	arc_scale_and_offset(cam, &scale, &x_off, &y_off);
	const int32_t *order = st->pieces_order + (size_t)level * R11L_NPIECES;

	for (int32_t rank = 0; rank < R11L_NPIECES; rank++) {
		int32_t i = order[rank];
		if (i < 0)
			continue;
		int32_t group =
			st->piece_group[(size_t)level * st->num_slots + i];
		int32_t group_c = group >= 0 ? group : 0;
		int32_t target_slot =
			st->group_target_slot[level * R11L_G + group_c];
		int active = target_slot >= 0;
		int32_t tgt_c = active ? target_slot : 0;

		int32_t px = (s->x[i] + s->w[i] / 2 - cam->x) * scale + x_off;
		int32_t py = (s->y[i] + s->h[i] / 2 - cam->y) * scale + y_off;
		int32_t tx = (s->x[tgt_c] + s->w[tgt_c] / 2 - cam->x) * scale +
			     x_off;
		int32_t ty = (s->y[tgt_c] + s->h[tgt_c] / 2 - cam->y) * scale +
			     y_off;
		r11l_draw_line(frame, px, py, tx, ty, active);
	}
}

void r11l_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct r11l_static *st, int32_t level,
			   const struct r11l_aux *aux, int32_t action_count)
{
	(void)aux;
	r11l_draw_budget(frame, action_count);
	r11l_draw_guides(frame, sprites, camera, st, level);
}
