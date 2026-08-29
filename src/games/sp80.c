#include "sp80.h"

#include <string.h>

enum {
	SP80_RESET = 0,
	SP80_ACTION1 = 1,
	SP80_ACTION2 = 2,
	SP80_ACTION3 = 3,
	SP80_ACTION4 = 4,
	SP80_ACTION5 = 5,
	SP80_ACTION6 = 6
};
enum { SP80_CHANGE = 0, SP80_SPILL = 1 };
enum { SP80_WIN = 2, SP80_GAME_OVER = 3 };

static const int32_t SP80_ACTION_REMAP[4][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0, 4, 3, 1, 2, 5, 6, 7 },
	{ 0, 2, 1, 4, 3, 5, 6, 7 },
	{ 0, 3, 4, 2, 1, 5, 6, 7 },
};

static inline int sp80_contains(const struct arc_sprites *s, int32_t i,
				int32_t x, int32_t y)
{
	return x >= s->x[i] && y >= s->y[i] && x < s->x[i] + s->w[i] &&
	       y < s->y[i] + s->h[i];
}

static inline int sp80_tag(const struct arc_sprites *s, int32_t i, int32_t tag)
{
	return s->tags[(size_t)i * s->atlas->num_tags + tag];
}

static void sp80_display_to_grid(const struct arc_camera *camera,
				 int32_t display_x, int32_t display_y,
				 int32_t *world_x, int32_t *world_y, int *valid)
{
	int32_t scale, x_pad, y_pad;
	arc_scale_and_offset(camera, &scale, &x_pad, &y_pad);
	int32_t dx = display_x - x_pad, dy = display_y - y_pad;
	int32_t grid_x = dx >= 0 ? dx / scale : -1;
	int32_t grid_y = dy >= 0 ? dy / scale : -1;
	*valid = grid_x >= 0 && grid_y >= 0 && grid_x < camera->width &&
		 grid_y < camera->height;
	*world_x = grid_x + camera->x;
	*world_y = grid_y + camera->y;
}

static void sp80_unrotate_click(int32_t x, int32_t y, int32_t k, int32_t *nx,
				int32_t *ny)
{
	switch (k) {
	case 1:
		*nx = 63 - y;
		*ny = x;
		break;
	case 2:
		*nx = 63 - x;
		*ny = 63 - y;
		break;
	case 3:
		*nx = y;
		*ny = 63 - x;
		break;
	default:
		*nx = x;
		*ny = y;
		break;
	}
}

static void sp80_deselect(struct arc_sprites *s, const struct sp80_static *st,
			  struct sp80_aux *aux)
{
	int32_t sel = aux->selected;
	if (sel >= 0) {
		int8_t colour = sp80_tag(s, sel, st->tuvk_tag) ? 15 : 8;
		int32_t area = s->atlas->ph * s->atlas->pw;
		int8_t *p = arc_sprite_pixels_mut(s, sel);
		for (int32_t k = 0; k < area; k++)
			if (p[k] != -1 && p[k] != 4)
				p[k] = colour;
		s->layer[sel] = 0;
	}
	aux->selected = -1;
}

static void sp80_select(struct arc_sprites *s, const struct sp80_static *st,
			struct sp80_aux *aux, int32_t slot)
{
	sp80_deselect(s, st, aux);
	int32_t area = s->atlas->ph * s->atlas->pw;
	int8_t *p = arc_sprite_pixels_mut(s, slot);
	for (int32_t k = 0; k < area; k++)
		if (p[k] != -1 && p[k] != 4)
			p[k] = 9;
	s->layer[slot] = 1;
	aux->selected = slot;
}

static void sp80_auto_select(struct arc_sprites *s,
			     const struct sp80_static *st, int32_t level,
			     struct sp80_aux *aux)
{
	int32_t best = -1, best_key = 0;
	const int32_t *lists[2] = {
		st->plzw_slots + (size_t)level * st->max_plzw,
		st->tuvk_slots + (size_t)level * st->max_tuvk,
	};
	const int32_t *counts[2] = { st->plzw_count, st->tuvk_count };
	for (int32_t g = 0; g < 2; g++) {
		int32_t count = counts[g][level];
		const int32_t *list = lists[g];
		for (int32_t k = 0; k < count; k++) {
			int32_t slot = list[k];
			if (!s->alive[slot])
				continue;
			int32_t key = s->x[slot] * s->x[slot] +
				      s->y[slot] * s->y[slot];
			key = key * 100000 +
			      st->pick_priority[(size_t)level * st->num_slots +
						slot];
			if (best < 0 || key < best_key) {
				best = slot;
				best_key = key;
			}
		}
	}
	if (best >= 0)
		sp80_select(s, st, aux, best);
}

static int32_t sp80_pick(const struct arc_sprites *s,
			 const struct sp80_static *st, int32_t level,
			 int32_t wx, int32_t wy, int on_board)
{
	if (!on_board)
		return -1;
	int32_t best = -1, best_pri = 0;
	const int32_t *lists[2] = {
		st->plzw_slots + (size_t)level * st->max_plzw,
		st->tuvk_slots + (size_t)level * st->max_tuvk,
	};
	const int32_t *counts[2] = { st->plzw_count, st->tuvk_count };
	for (int32_t g = 0; g < 2; g++) {
		int32_t count = counts[g][level];
		const int32_t *list = lists[g];
		for (int32_t k = 0; k < count; k++) {
			int32_t slot = list[k];
			if (!s->alive[slot] || !sp80_contains(s, slot, wx, wy))
				continue;
			int32_t pri = st->pick_priority[(size_t)level *
								st->num_slots +
							slot];
			if (best < 0 || pri < best_pri) {
				best = slot;
				best_pri = pri;
			}
		}
	}
	return best;
}

static int sp80_can_move(const struct arc_sprites *s,
			 const struct sp80_static *st, int32_t level,
			 int32_t slot, int32_t new_x, int32_t new_y)
{
	if (new_y < 3)
		return 0;
	int32_t w = s->w[slot], h = s->h[slot];
	const int32_t *repw = st->repw_slots + (size_t)level * st->max_repw;
	int32_t count = st->repw_count[level];
	for (int32_t k = 0; k < count; k++) {
		int32_t j = repw[k];
		if (!s->alive[j])
			continue;
		if (new_x < s->x[j] + s->w[j] + 1 && new_x + w > s->x[j] - 1 &&
		    new_y < s->y[j] + s->h[j] + 1 && new_y + h > s->y[j] - 1)
			return 0;
	}
	return 1;
}

static void sp80_apply_move(struct arc_sprites *s, const struct sp80_static *st,
			    int32_t level, int32_t slot, int32_t dx, int32_t dy)
{
	int32_t new_x = s->x[slot] + dx, new_y = s->y[slot] + dy;
	if (!sp80_can_move(s, st, level, slot, new_x, new_y))
		return;
	int32_t old_x = s->x[slot], old_y = s->y[slot];
	s->x[slot] = new_x;
	s->y[slot] = new_y;
	int blocked = 0, all_sib = 1;
	for (int32_t j = 0; j < st->extra_start; j++) {
		if (!arc_collides_pair(s, slot, j, 0))
			continue;
		blocked = 1;
		if (!(sp80_tag(s, j, st->plzw_tag) ||
		      sp80_tag(s, j, st->tuvk_tag)))
			all_sib = 0;
	}
	if (blocked && !all_sib) {
		s->x[slot] = old_x;
		s->y[slot] = old_y;
	}
}

static int32_t sp80_spawn(struct arc_sprites *s, const struct sp80_static *st,
			  int32_t *next_order, int32_t x, int32_t y)
{
	int32_t candidate = st->extra_start + (*next_order - st->num_slots);
	if (candidate < st->extra_start)
		candidate = st->extra_start;
	if (candidate > st->num_slots - 1)
		candidate = st->num_slots - 1;
	arc_add_sprite(s, candidate, *next_order);
	arc_set_position(s, candidate, x, y);
	(*next_order)++;
	return candidate;
}

static void sp80_append(int32_t *fs, int32_t *fdx, int32_t *fdy, uint8_t *fa,
			int32_t *write_ptr, int32_t slot, int32_t dx,
			int32_t dy)
{
	int32_t pos = *write_ptr < SP80_FRONTIER_CAP - 1 ?
			      *write_ptr :
			      SP80_FRONTIER_CAP - 1;
	fs[pos] = slot;
	fdx[pos] = dx;
	fdy[pos] = dy;
	fa[pos] = 1;
	(*write_ptr)++;
}

static void sp80_spread(struct arc_sprites *s, const struct sp80_static *st,
			int32_t *next_order, int32_t px, int32_t py, int32_t ox,
			int32_t oy, int32_t dx, int32_t dy, int32_t *fs,
			int32_t *fdx, int32_t *fdy, uint8_t *fa,
			int32_t *write_ptr)
{
	int32_t tx = px + ox, ty = py + oy;
	if (arc_get_sprite_at(s, tx, ty, -1, 0) >= 0)
		return;
	int32_t new_slot = sp80_spawn(s, st, next_order, tx, ty);
	sp80_append(fs, fdx, fdy, fa, write_ptr, new_slot, dx, dy);
}

static void sp80_enter_spill(struct arc_sprites *s,
			     const struct sp80_static *st, int32_t level,
			     struct sp80_aux *aux, int32_t *next_order)
{
	for (int32_t i = st->extra_start; i < st->num_slots; i++)
		s->alive[i] = 0;
	*next_order = st->num_slots;

	for (int32_t i = 0; i < SP80_FRONTIER_CAP; i++) {
		aux->frontier_slot[i] = -1;
		aux->frontier_dx[i] = 0;
		aux->frontier_dy[i] = 0;
		aux->frontier_active[i] = 0;
	}
	int32_t write_ptr = 0;

	const int32_t *liolf =
		st->liolf_seed_slots + (size_t)level * st->max_liolf_seed;
	int32_t liolf_count = st->liolf_seed_count[level];
	for (int32_t i = 0; i < liolf_count; i++)
		sp80_append(aux->frontier_slot, aux->frontier_dx,
			    aux->frontier_dy, aux->frontier_active, &write_ptr,
			    liolf[i], 0, 1);

	const int32_t *spout_slot =
		st->spout_slot + (size_t)level * st->max_spout;
	const int32_t *spout_dx = st->spout_dx + (size_t)level * st->max_spout;
	const int32_t *spout_dy = st->spout_dy + (size_t)level * st->max_spout;
	int32_t spout_count = st->spout_count[level];
	for (int32_t i = 0; i < spout_count; i++) {
		int32_t slot = spout_slot[i];
		int32_t qx = s->x[slot] + spout_dx[i];
		int32_t qy = s->y[slot] + spout_dy[i] + 1;
		if (arc_get_sprite_at(s, qx, qy, -1, 0) < 0) {
			int32_t new_slot =
				sp80_spawn(s, st, next_order, qx, qy);
			sp80_append(aux->frontier_slot, aux->frontier_dx,
				    aux->frontier_dy, aux->frontier_active,
				    &write_ptr, new_slot, 0, 1);
		}
	}

	aux->mode = SP80_SPILL;
	aux->growing = 0;
	aux->draining = 0;
	aux->flash = 0;
	memset(aux->filled, 0, (size_t)st->num_slots);
	memset(aux->touched, 0, (size_t)st->num_slots);
}

static void sp80_step_flow(struct arc_sprites *s, const struct sp80_static *st,
			   struct sp80_aux *aux, int32_t *next_order)
{
	int32_t new_fs[SP80_FRONTIER_CAP], new_fdx[SP80_FRONTIER_CAP],
		new_fdy[SP80_FRONTIER_CAP];
	uint8_t new_fa[SP80_FRONTIER_CAP];
	for (int32_t i = 0; i < SP80_FRONTIER_CAP; i++) {
		new_fs[i] = -1;
		new_fdx[i] = 0;
		new_fdy[i] = 0;
		new_fa[i] = 0;
	}
	int32_t write_ptr = 0;
	int growing = aux->growing;

	for (int32_t i = 0; i < SP80_FRONTIER_CAP; i++) {
		if (!aux->frontier_active[i])
			continue;
		int32_t slot_i = aux->frontier_slot[i];
		int32_t dx = aux->frontier_dx[i], dy = aux->frontier_dy[i];
		int32_t px = s->x[slot_i], py = s->y[slot_i];

		int vertical = dy != 0;
		int32_t obz = vertical ? -1 : 0;
		int32_t unv = vertical ? 1 : 0;
		int32_t vww = vertical ? 0 : -1;
		int32_t kgh = vertical ? 0 : 1;

		int32_t tx = px + dx, ty = py + dy;
		int32_t target = arc_get_sprite_at(s, tx, ty, -1, 0);
		int has_target = target >= 0;
		int t_liolf = has_target && sp80_tag(s, target, st->liolf_tag);
		int t_plzw = has_target && sp80_tag(s, target, st->plzw_tag);
		int t_repw = has_target && sp80_tag(s, target, st->repw_tag);
		int t_tuvk = has_target && sp80_tag(s, target, st->tuvk_tag);
		int t_waoe = has_target && sp80_tag(s, target, st->waoe_tag);

		if (!has_target) {
			int32_t new_slot =
				sp80_spawn(s, st, next_order, tx, ty);
			sp80_append(new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr, new_slot, dx, dy);
		}

		if (t_liolf)
			sp80_append(new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr, target, dx, dy);

		if (t_plzw) {
			sp80_spread(s, st, next_order, px, py, obz, vww, dx, dy,
				    new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr);
			sp80_spread(s, st, next_order, px, py, unv, kgh, dx, dy,
				    new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr);
		}

		int32_t owy = arc_get_sprite_at(s, px + obz, py + vww, -1, 0);
		int32_t fak = arc_get_sprite_at(s, px + unv, py + kgh, -1, 0);
		int flanked = (owy == target) && (fak == target);
		int cond_repw_fill = t_repw && flanked;
		int cond_repw_spread = t_repw && !flanked;

		if (cond_repw_fill) {
			arc_color_remap(s, target, 0, 0, 13);
			aux->filled[target] = 1;
		}
		if (cond_repw_spread) {
			sp80_spread(s, st, next_order, px, py, obz, vww, dx, dy,
				    new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr);
			sp80_spread(s, st, next_order, px, py, unv, kgh, dx, dy,
				    new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr);
		}

		int cond_a = t_tuvk && (owy == target) && (fak < 0);
		int cond_b = t_tuvk && (fak == target) && (owy < 0);
		int cond_else = t_tuvk && !cond_b;

		if (cond_a) {
			int32_t ndx = dy, ndy = -dx;
			int32_t new_slot = sp80_spawn(s, st, next_order,
						      px + ndx, py + ndy);
			sp80_append(new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr, new_slot, ndx, ndy);
		}
		if (cond_b) {
			int32_t ndx = -dy, ndy = dx;
			int32_t new_slot = sp80_spawn(s, st, next_order,
						      px + ndx, py + ndy);
			sp80_append(new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr, new_slot, ndx, ndy);
		}
		if (cond_else) {
			sp80_spread(s, st, next_order, px, py, obz, vww, dx, dy,
				    new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr);
			sp80_spread(s, st, next_order, px, py, unv, kgh, dx, dy,
				    new_fs, new_fdx, new_fdy, new_fa,
				    &write_ptr);
		}

		if (t_waoe) {
			arc_color_remap(s, target, 0, 0, 14);
			aux->touched[target] = 1;
			growing = 1;
		}
	}

	memcpy(aux->frontier_slot, new_fs, sizeof new_fs);
	memcpy(aux->frontier_dx, new_fdx, sizeof new_fdx);
	memcpy(aux->frontier_dy, new_fdy, sizeof new_fdy);
	memcpy(aux->frontier_active, new_fa, sizeof new_fa);
	aux->growing = (uint8_t)growing;
	int frontier_empty = write_ptr == 0;
	aux->draining = (uint8_t)frontier_empty;
	if (frontier_empty)
		aux->flash = 0;
}

static void sp80_flash_frame(struct arc_sprites *s,
			     const struct sp80_static *st, int32_t level,
			     struct sp80_aux *aux)
{
	int32_t flash = aux->flash;
	int8_t goal_colour = (int8_t)((flash % 2 == 1) ? 14 : 1);
	for (int32_t i = 0; i < st->extra_start; i++)
		if (aux->touched[i])
			arc_color_remap(s, i, 0, 0, goal_colour);

	if (flash < 5) {
		int8_t obstacle_colour = (int8_t)((flash % 2 == 0) ? 0 : 11);
		const int32_t *repw =
			st->repw_slots + (size_t)level * st->max_repw;
		int32_t count = st->repw_count[level];
		for (int32_t k = 0; k < count; k++) {
			int32_t slot = repw[k];
			if (s->alive[slot] && !aux->filled[slot])
				arc_color_remap(s, slot, 0, 0, obstacle_colour);
		}
	}
	aux->flash = flash + 1;
}

static void sp80_finish_failed_spill(struct arc_sprites *s,
				     const struct sp80_static *st,
				     int32_t level, struct sp80_aux *aux,
				     int32_t *status, uint8_t *action_complete)
{
	for (int32_t i = st->extra_start; i < st->num_slots; i++)
		s->alive[i] = 0;

	const int32_t *repw = st->repw_slots + (size_t)level * st->max_repw;
	int32_t repw_count = st->repw_count[level];
	for (int32_t k = 0; k < repw_count; k++)
		arc_color_remap(s, repw[k], 0, 0, 11);

	const int32_t *waoe = st->waoe_slots + (size_t)level * st->max_waoe;
	int32_t waoe_count = st->waoe_count[level];
	for (int32_t k = 0; k < waoe_count; k++)
		arc_color_remap(s, waoe[k], 0, 0, 1);

	aux->mode = SP80_CHANGE;
	aux->selected = -1;
	aux->growing = 0;
	aux->draining = 0;
	aux->flash = 0;
	memset(aux->filled, 0, (size_t)st->num_slots);
	memset(aux->touched, 0, (size_t)st->num_slots);
	aux->fail_count += 1;

	sp80_auto_select(s, st, level, aux);
	if (aux->steps <= 0)
		*status = SP80_GAME_OVER;
	*action_complete = 1;
}

static void sp80_step_drain_fail(struct arc_sprites *s,
				 const struct sp80_static *st, int32_t level,
				 struct sp80_aux *aux, int32_t *status,
				 uint8_t *action_complete)
{
	if (aux->flash < 6)
		sp80_flash_frame(s, st, level, aux);
	else
		sp80_finish_failed_spill(s, st, level, aux, status,
					 action_complete);
}

static void sp80_step_drain_win(const struct sp80_static *st, int32_t level,
				int32_t *score, int32_t *status,
				uint8_t *next_level, uint8_t *action_complete)
{
	int is_last = level == st->num_levels - 1;
	*score += 1;
	*next_level = (uint8_t)!is_last;
	if (is_last)
		*status = SP80_WIN;
	*action_complete = 1;
}

static void sp80_step_drain(struct arc_sprites *s, const struct sp80_static *st,
			    int32_t level, struct sp80_aux *aux, int32_t *score,
			    int32_t *status, uint8_t *next_level,
			    uint8_t *action_complete)
{
	const int32_t *repw = st->repw_slots + (size_t)level * st->max_repw;
	int32_t count = st->repw_count[level];
	int all_filled = 1;
	for (int32_t k = 0; k < count; k++) {
		int32_t slot = repw[k];
		if (s->alive[slot] && !aux->filled[slot]) {
			all_filled = 0;
			break;
		}
	}
	int failed = aux->growing || !all_filled;
	if (failed)
		sp80_step_drain_fail(s, st, level, aux, status,
				     action_complete);
	else
		sp80_step_drain_win(st, level, score, status, next_level,
				    action_complete);
}

static void sp80_step_spill(struct arc_sprites *s, const struct sp80_static *st,
			    int32_t level, struct sp80_aux *aux,
			    int32_t *next_order, int32_t *score,
			    int32_t *status, uint8_t *next_level,
			    uint8_t *action_complete)
{
	if (aux->draining)
		sp80_step_drain(s, st, level, aux, score, status, next_level,
				action_complete);
	else
		sp80_step_flow(s, st, aux, next_order);
}

static void sp80_step_change(struct arc_sprites *s,
			     const struct arc_camera *camera,
			     const struct sp80_static *st, int32_t level,
			     int32_t action_id, int32_t action_x,
			     int32_t action_y, struct sp80_aux *aux,
			     int32_t *next_order, int32_t *score,
			     int32_t *status, uint8_t *action_complete)
{
	(void)score;
	int32_t k = st->rotation[level];
	int32_t remapped_id = SP80_ACTION_REMAP[k][action_id];
	int is_click_raw = k != 0 && action_id == SP80_ACTION6;
	int32_t click_x = action_x, click_y = action_y;
	if (is_click_raw)
		sp80_unrotate_click(action_x, action_y, k, &click_x, &click_y);

	int not_reset = action_id != SP80_RESET;
	int32_t steps = aux->steps - (not_reset ? 1 : 0);
	if (steps < 0)
		steps = 0;
	int just_lost = not_reset && steps <= 0;
	aux->steps = steps;
	if (just_lost)
		*status = SP80_GAME_OVER;
	*action_complete = just_lost ? 1 : 0;

	int32_t wx, wy;
	int on_board;
	sp80_display_to_grid(camera, click_x, click_y, &wx, &wy, &on_board);
	int32_t hit_slot = sp80_pick(s, st, level, wx, wy, on_board);
	int gate_a = remapped_id == SP80_ACTION6 && hit_slot >= 0;

	int32_t dx = 0, dy = 0;
	if (remapped_id == SP80_ACTION3)
		dx = -1;
	else if (remapped_id == SP80_ACTION4)
		dx = 1;
	if (remapped_id == SP80_ACTION1)
		dy = -1;
	else if (remapped_id == SP80_ACTION2)
		dy = 1;
	int gate_b = aux->selected >= 0 && (dx != 0 || dy != 0);

	int is_action5 = remapped_id == SP80_ACTION5;
	int gate_c = is_action5 && aux->fail_count >= 4;
	int gate_d = is_action5 && aux->fail_count < 4;

	if (gate_a) {
		sp80_select(s, st, aux, hit_slot);
		*action_complete = 1;
	} else if (gate_b) {
		sp80_apply_move(s, st, level, aux->selected, dx, dy);
		*action_complete = 1;
	} else if (gate_c) {
		*status = SP80_GAME_OVER;
		*action_complete = 1;
	} else if (gate_d) {
		sp80_deselect(s, st, aux);
		sp80_enter_spill(s, st, level, aux, next_order);
	} else {
		*action_complete = 1;
	}
}

void sp80_zero_aux(struct sp80_aux *aux, const struct sp80_static *st)
{
	aux->mode = SP80_CHANGE;
	aux->selected = -1;
	aux->steps = 0;
	aux->fail_count = 0;
	aux->growing = 0;
	aux->draining = 0;
	aux->flash = 0;
	memset(aux->filled, 0, (size_t)st->num_slots);
	memset(aux->touched, 0, (size_t)st->num_slots);
	for (int32_t i = 0; i < SP80_FRONTIER_CAP; i++) {
		aux->frontier_slot[i] = -1;
		aux->frontier_dx[i] = 0;
		aux->frontier_dy[i] = 0;
		aux->frontier_active[i] = 0;
	}
}

void sp80_on_set_level(struct arc_sprites *sprites,
		       const struct sp80_static *st, int32_t level,
		       struct sp80_aux *aux)
{
	int32_t area = sprites->atlas->ph * sprites->atlas->pw;
	int32_t num_tags = sprites->atlas->num_tags;
	for (int32_t i = st->extra_start; i < st->num_slots; i++)
		sprites->tags[(size_t)i * num_tags + st->liolf_tag] = 1;

	const int32_t *plzw = st->plzw_slots + (size_t)level * st->max_plzw;
	int32_t plzw_count = st->plzw_count[level];
	for (int32_t k = 0; k < plzw_count; k++) {
		int8_t *p = arc_sprite_pixels_mut(sprites, plzw[k]);
		for (int32_t u = 0; u < area; u++)
			if (p[u] != -1 && p[u] != 4)
				p[u] = 8;
	}

	const int32_t *repw = st->repw_slots + (size_t)level * st->max_repw;
	int32_t repw_count = st->repw_count[level];
	for (int32_t k = 0; k < repw_count; k++)
		arc_color_remap(sprites, repw[k], 0, 0, 11);

	const int32_t *waoe = st->waoe_slots + (size_t)level * st->max_waoe;
	int32_t waoe_count = st->waoe_count[level];
	for (int32_t k = 0; k < waoe_count; k++)
		arc_color_remap(sprites, waoe[k], 0, 0, 1);

	for (int32_t i = st->extra_start; i < st->num_slots; i++) {
		int8_t *p = arc_sprite_pixels_mut(sprites, i);
		memset(p, -1, (size_t)area);
		p[0] = 6;
		sprites->h[i] = 1;
		sprites->w[i] = 1;
		sprites->blocking[i] = PIXEL_PERFECT;
		sprites->interaction[i] = TANGIBLE;
		sprites->alive[i] = 0;
	}

	sp80_zero_aux(aux, st);
	aux->steps = st->budget[level];
	sp80_auto_select(sprites, st, level, aux);
}

void sp80_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct sp80_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct sp80_aux *aux,
		    int32_t *next_order, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete)
{
	(void)action_count;
	if (aux->mode == SP80_SPILL) {
		sp80_step_spill(sprites, st, level, aux, next_order, score,
				status, next_level, action_complete);
	} else {
		sp80_step_change(sprites, camera, st, level, action_id,
				 action_x, action_y, aux, next_order, score,
				 status, action_complete);
	}
}

void sp80_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct sp80_static *st, int32_t level,
			   const struct sp80_aux *aux)
{
	(void)sprites;
	(void)camera;
	int32_t budget = st->budget[level];
	if (budget != 0) {
		int32_t steps = aux->steps;
		int32_t total = ARC_FRAME_SIZE * steps;
		int32_t whole = total / budget, rest = total % budget;
		int round_up = (2 * rest > budget) ||
			       (2 * rest == budget && (whole % 2 == 1));
		int32_t filled = whole + (round_up ? 1 : 0);
		for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
			frame[c] = (int8_t)(c < filled ? 14 : 0);
	}

	int32_t k = st->rotation[level];
	if (k == 0)
		return;
	int8_t tmp[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	if (k == 1) {
		for (int32_t r = 0; r < ARC_FRAME_SIZE; r++)
			for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
				tmp[r * ARC_FRAME_SIZE + c] =
					frame[c * ARC_FRAME_SIZE +
					      (ARC_FRAME_SIZE - 1 - r)];
	} else if (k == 2) {
		for (int32_t r = 0; r < ARC_FRAME_SIZE; r++)
			for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
				tmp[r * ARC_FRAME_SIZE + c] =
					frame[(ARC_FRAME_SIZE - 1 - r) *
						      ARC_FRAME_SIZE +
					      (ARC_FRAME_SIZE - 1 - c)];
	} else {
		for (int32_t r = 0; r < ARC_FRAME_SIZE; r++)
			for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
				tmp[r * ARC_FRAME_SIZE + c] =
					frame[(ARC_FRAME_SIZE - 1 - c) *
						      ARC_FRAME_SIZE +
					      r];
	}
	memcpy(frame, tmp, sizeof tmp);
}
