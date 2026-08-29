#include "sk48.h"

#include <stdlib.h>
#include <string.h>

enum { SK48_WIN = 2, SK48_GAME_OVER = 3 };
enum {
	SK48_ACTION1 = 1,
	SK48_ACTION2 = 2,
	SK48_ACTION3 = 3,
	SK48_ACTION4 = 4,
	SK48_ACTION6 = 6,
	SK48_ACTION7 = 7
};
enum {
	SK48_DESELECT_HEAD_FROM = 0,
	SK48_DESELECT_HEAD_TO = 4,
	SK48_DESELECT_SEG1_FROM = 2,
	SK48_DESELECT_SEG1_TO = 3,
	SK48_DESELECT_SEG2_FROM = 1,
	SK48_DESELECT_SEG2_TO = 2,
	SK48_SELECT_HEAD_FROM = 4,
	SK48_SELECT_HEAD_TO = 0,
	SK48_SELECT_SEG1_FROM = 2,
	SK48_SELECT_SEG1_TO = 1,
	SK48_SELECT_SEG2_FROM = 3,
	SK48_SELECT_SEG2_TO = 2,
};
enum { SK48_HUD_ROW = 53, SK48_HUD_BG = 3, SK48_HUD_FILL = 2 };
enum {
	SK48_WIN_BLINK_FRAMES = 35,
	SK48_BLINK_PERIOD = 5,
	SK48_BLINK_LOCK_FRAME = 25,
	SK48_BLINK_OFF_COLOR = 0
};

static void sk48_dir(int32_t rotation, int32_t *dx, int32_t *dy)
{
	switch (rotation) {
	case 0:
		*dx = 1;
		*dy = 0;
		break;
	case 90:
		*dx = 0;
		*dy = 1;
		break;
	case 180:
		*dx = -1;
		*dy = 0;
		break;
	default:
		*dx = 0;
		*dy = -1;
		break;
	}
}

static int32_t sk48_rot_idx(int32_t rotation)
{
	if (rotation == 0)
		return 0;
	if (rotation == 90)
		return 1;
	if (rotation == 180)
		return 2;
	return 3;
}

static void sk48_write_segment(struct arc_sprites *s,
			       const struct sk48_static *st, int32_t slot,
			       int32_t x, int32_t y, int32_t rotation)
{
	int32_t ri = sk48_rot_idx(rotation);
	size_t area = (size_t)st->ph * st->pw;
	const int8_t *tmpl = st->segment_template_pixels + (size_t)ri * area;
	int8_t *dst = arc_sprite_pixels_mut(s, slot);
	memcpy(dst, tmpl, area);
	s->x[slot] = x;
	s->y[slot] = y;
	s->h[slot] = st->segment_template_h[ri];
	s->w[slot] = st->segment_template_w[ri];
	s->alive[slot] = 1;
	s->layer[slot] = (int32_t)(rotation == 0 || rotation == 180);
	s->interaction[slot] = TANGIBLE;
	s->blocking[slot] = PIXEL_PERFECT;
}

static inline int8_t sk48_pixel_at(const struct arc_sprites *s, int32_t i,
				   int32_t x, int32_t y, int32_t pw, int32_t ph)
{
	int32_t py = y - s->y[i];
	py = py < 0 ? 0 : (py >= ph ? ph - 1 : py);
	int32_t px = x - s->x[i];
	px = px < 0 ? 0 : (px >= pw ? pw - 1 : px);
	return arc_sprite_pixels(s, i)[(size_t)py * pw + px];
}

static int32_t sk48_candidate_hit(const struct arc_sprites *s,
				  const int32_t *cand, int32_t n, int32_t x,
				  int32_t y, int32_t pw, int32_t ph)
{
	int32_t best = -1, best_key = 0;
	int found = 0;
	for (int32_t k = 0; k < n; k++) {
		int32_t i = cand[k];
		if (!s->alive[i])
			continue;
		if (!(x >= s->x[i] && y >= s->y[i] && x < s->x[i] + s->w[i] &&
		      y < s->y[i] + s->h[i]))
			continue;
		if (!arc_sprite_collidable(s, i))
			continue;
		if (s->blocking[i] == PIXEL_PERFECT &&
		    sk48_pixel_at(s, i, x, y, pw, ph) == -1)
			continue;
		int32_t key = s->layer[i] * ARC_ORDER_BITS +
			      (ARC_ORDER_BITS - 1 - s->order[i]);
		if (!found || key > best_key) {
			found = 1;
			best_key = key;
			best = i;
		}
	}
	return found ? best : -1;
}

static int sk48_oob_or_wall(const struct arc_sprites *s,
			    const struct sk48_static *st, int32_t level,
			    int32_t x, int32_t y)
{
	if (x < st->boundary_left[level] ||
	    x + SK48_PITCH > st->boundary_right[level] ||
	    y < st->boundary_top[level] ||
	    y + SK48_PITCH > st->boundary_bottom[level])
		return 1;
	const int32_t *cand = st->wall_slots + (size_t)level * st->max_wall;
	int32_t n = st->wall_count[level];
	return sk48_candidate_hit(s, cand, n, x, y, st->pw, st->ph) >= 0;
}

static int32_t sk48_block_at(const struct arc_sprites *s,
			     const struct sk48_static *st, int32_t level,
			     int32_t x, int32_t y)
{
	const int32_t *cand = st->block_slots + (size_t)level * st->max_block;
	int32_t n = st->block_count[level];
	for (int32_t k = 0; k < n; k++) {
		int32_t i = cand[k];
		if (s->alive[i] && s->x[i] == x && s->y[i] == y)
			return i;
	}
	return -1;
}

static int32_t sk48_segment_at(const struct arc_sprites *s,
			       const struct sk48_static *st, int32_t level,
			       int32_t x, int32_t y, int horizontal,
			       int32_t exclude)
{
	int32_t base = SK48_SEGMENT_BASE(st);
	for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
		int32_t rotation =
			st->head_rotation[level * SK48_MAX_HEADS + hi];
		int seg_horizontal = rotation == 0 || rotation == 180;
		if (seg_horizontal != horizontal)
			continue;
		int32_t hbase = base + hi * SK48_MAX_SEGMENTS;
		for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++) {
			int32_t slot = hbase + k;
			if (slot == exclude || !s->alive[slot])
				continue;
			if (s->x[slot] == x && s->y[slot] == y)
				return slot;
		}
	}
	return -1;
}

static void sk48_add_shadow(int32_t *shadow_blocks, int32_t *shadow_count,
			    int32_t slot)
{
	int32_t idx = *shadow_count < SK48_MAX_SHADOWS - 1 ?
			      *shadow_count :
			      SK48_MAX_SHADOWS - 1;
	shadow_blocks[idx] = slot;
	if (*shadow_count < SK48_MAX_SHADOWS)
		(*shadow_count)++;
}

static int sk48_push_node(struct arc_sprites *s, const struct sk48_static *st,
			  int32_t level, int32_t slot, int32_t came_from,
			  int32_t dir_x, int32_t dir_y, uint8_t *visited,
			  int32_t *shadow_blocks, int32_t *shadow_count)
{
	int32_t base = SK48_SEGMENT_BASE(st);
	int is_seg = slot >= base &&
		     slot < base + SK48_MAX_HEADS * SK48_MAX_SEGMENTS;
	int32_t cur_x = s->x[slot], cur_y = s->y[slot];
	int32_t dx = dir_x * SK48_PITCH, dy = dir_y * SK48_PITCH;

	int32_t seg_dir_x = 0, seg_dir_y = 0;
	if (is_seg) {
		int32_t hi = (slot - base) / SK48_MAX_SEGMENTS;
		sk48_dir(st->head_rotation[level * SK48_MAX_HEADS + hi],
			 &seg_dir_x, &seg_dir_y);
	}

	int edge_blocked =
		sk48_oob_or_wall(s, st, level, cur_x + dx, cur_y + dy);
	int retreating = is_seg && seg_dir_x == -dir_x && seg_dir_y == -dir_y;
	int currently_out = sk48_oob_or_wall(s, st, level, cur_x, cur_y);
	int excused = is_seg && (retreating || currently_out);
	if (edge_blocked && !excused)
		return 0;

	if (is_seg) {
		for (int probe = 0; probe < 2; probe++) {
			int32_t px = cur_x + (probe ? dx : 0);
			int32_t py = cur_y + (probe ? dy : 0);
			int32_t block = sk48_block_at(s, st, level, px, py);
			if (block < 0)
				continue;
			int ok = sk48_push_node(s, st, level, block, slot,
						dir_x, dir_y, visited,
						shadow_blocks, shadow_count);
			if (ok) {
				visited[block] = 1;
				continue;
			}
			int perpendicular = (seg_dir_x == 0) != (dir_x == 0);
			if (perpendicular)
				return 0;
			int32_t bx = s->x[block], by = s->y[block];
			int on_track = sk48_segment_at(s, st, level, bx, by,
						       dir_x != 0, -1) >= 0;
			if (!on_track)
				sk48_add_shadow(shadow_blocks, shadow_count,
						block);
		}
	} else {
		int moving_horizontal = dir_x != 0;
		for (int probe = 0; probe < 2; probe++) {
			int32_t px = cur_x + (probe ? dx : 0);
			int32_t py = cur_y + (probe ? dy : 0);
			int32_t seg =
				sk48_segment_at(s, st, level, px, py,
						!moving_horizontal, came_from);
			if (seg < 0)
				continue;
			int32_t odx, ody;
			int32_t ohi = (seg - base) / SK48_MAX_SEGMENTS;
			sk48_dir(
				st->head_rotation[level * SK48_MAX_HEADS + ohi],
				&odx, &ody);
			int allowed = (odx == 0) == (dir_x == 0);
			if (!allowed)
				return 0;
		}
		int32_t far_x = cur_x + dx, far_y = cur_y + dy;
		int32_t far_block = sk48_block_at(s, st, level, far_x, far_y);
		if (far_block >= 0) {
			int ok = sk48_push_node(s, st, level, far_block, -1,
						dir_x, dir_y, visited,
						shadow_blocks, shadow_count);
			if (!ok)
				return 0;
			visited[far_block] = 1;
		}
		int was_on_track = sk48_segment_at(s, st, level, cur_x, cur_y,
						   moving_horizontal, -1) >= 0;
		int will_on_track =
			sk48_segment_at(s, st, level, cur_x + dx, cur_y + dy,
					moving_horizontal, -1) >= 0;
		if (!was_on_track && will_on_track)
			sk48_add_shadow(shadow_blocks, shadow_count, slot);
	}

	visited[slot] = 1;
	return 1;
}

static void sk48_color_group(struct arc_sprites *s,
			     const struct sk48_static *st, int32_t level,
			     int32_t head_idx, int8_t head_from, int8_t head_to,
			     int8_t seg1_from, int8_t seg1_to, int8_t seg2_from,
			     int8_t seg2_to)
{
	int32_t base = SK48_SEGMENT_BASE(st);
	int32_t idxs[2];
	idxs[0] = head_idx;
	idxs[1] = head_idx >= 0 ?
			  st->head_partner[level * SK48_MAX_HEADS + head_idx] :
			  -1;
	for (int t = 0; t < 2; t++) {
		int32_t hi = idxs[t];
		if (hi < 0)
			continue;
		int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + hi];
		arc_color_remap(s, head_slot, 1, head_from, head_to);
		int32_t hbase = base + hi * SK48_MAX_SEGMENTS;
		for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++)
			if (s->alive[hbase + k])
				arc_color_remap(s, hbase + k, 1, seg1_from,
						seg1_to);
		for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++)
			if (s->alive[hbase + k])
				arc_color_remap(s, hbase + k, 1, seg2_from,
						seg2_to);
	}
}

static void sk48_select_head(struct arc_sprites *s,
			     const struct sk48_static *st, int32_t level,
			     struct sk48_aux *aux, int32_t new_head,
			     int is_initial)
{
	if (!is_initial) {
		sk48_color_group(s, st, level, aux->selected_head,
				 SK48_DESELECT_HEAD_FROM, SK48_DESELECT_HEAD_TO,
				 SK48_DESELECT_SEG1_FROM, SK48_DESELECT_SEG1_TO,
				 SK48_DESELECT_SEG2_FROM,
				 SK48_DESELECT_SEG2_TO);
	}
	sk48_color_group(s, st, level, new_head, SK48_SELECT_HEAD_FROM,
			 SK48_SELECT_HEAD_TO, SK48_SELECT_SEG1_FROM,
			 SK48_SELECT_SEG1_TO, SK48_SELECT_SEG2_FROM,
			 SK48_SELECT_SEG2_TO);
	aux->selected_head = new_head;
}

static int32_t sk48_segment_count(const struct arc_sprites *s,
				  const struct sk48_static *st,
				  int32_t head_idx)
{
	int32_t base = SK48_SEGMENT_BASE(st) + head_idx * SK48_MAX_SEGMENTS;
	int32_t c = 0;
	for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++)
		if (s->alive[base + k])
			c++;
	return c;
}

static int32_t sk48_free_segment_slot(const struct arc_sprites *s,
				      const struct sk48_static *st,
				      int32_t head_idx)
{
	int32_t base = SK48_SEGMENT_BASE(st) + head_idx * SK48_MAX_SEGMENTS;
	for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++)
		if (!s->alive[base + k])
			return base + k;
	return base;
}

static void sk48_apply_visited_delta(struct arc_sprites *s,
				     const struct sk48_static *st,
				     int32_t level, const uint8_t *visited,
				     int32_t dx, int32_t dy)
{
	int32_t base = SK48_SEGMENT_BASE(st);
	for (int32_t k = 0; k < SK48_MAX_HEADS * SK48_MAX_SEGMENTS; k++) {
		int32_t slot = base + k;
		if (visited[slot]) {
			s->x[slot] += dx;
			s->y[slot] += dy;
		}
	}
	const int32_t *cand = st->block_slots + (size_t)level * st->max_block;
	int32_t n = st->block_count[level];
	for (int32_t k = 0; k < n; k++) {
		int32_t slot = cand[k];
		if (visited[slot]) {
			s->x[slot] += dx;
			s->y[slot] += dy;
		}
	}
	for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
		int32_t hs = st->head_slot[level * SK48_MAX_HEADS + hi];
		if (hs >= 0 && visited[hs]) {
			s->x[hs] += dx;
			s->y[hs] += dy;
		}
	}
}

static void sk48_begin_slide(struct arc_sprites *s,
			     const struct sk48_static *st, int32_t level,
			     struct sk48_aux *aux, const uint8_t *visited,
			     int32_t dir_x, int32_t dir_y,
			     const int32_t *shadow_blocks, int32_t shadow_count,
			     int32_t retract_removed_slot)
{
	int32_t shadow_base = SK48_SHADOW_BASE(st);
	int32_t ri = dir_x != 0 ? 0 : 1;
	size_t area = (size_t)st->ph * st->pw;
	for (int32_t i = 0; i < shadow_count; i++) {
		int32_t block = shadow_blocks[i];
		int32_t bx = s->x[block], by = s->y[block];
		const int8_t *tmpl =
			st->shadow_template_pixels + (size_t)ri * area;
		int8_t color = arc_sprite_pixels(s, block)[1 * st->pw + 1];
		int32_t slot = shadow_base + i;
		int8_t *dst = arc_sprite_pixels_mut(s, slot);
		for (size_t p = 0; p < area; p++)
			dst[p] = tmpl[p] >= 0 ? color : tmpl[p];
		s->x[slot] = bx;
		s->y[slot] = by;
		s->h[slot] = st->shadow_template_h[ri];
		s->w[slot] = st->shadow_template_w[ri];
		s->alive[slot] = 1;
		s->layer[slot] = 3;
		s->interaction[slot] = TANGIBLE;
		s->blocking[slot] = PIXEL_PERFECT;
	}

	int32_t step_x = 3 * dir_x, step_y = 3 * dir_y;
	sk48_apply_visited_delta(s, st, level, visited, step_x, step_y);

	for (int32_t i = 0; i < shadow_count; i++) {
		int32_t block = shadow_blocks[i];
		int32_t slot = shadow_base + i;
		s->x[slot] = s->x[block];
		s->y[slot] = s->y[block];
	}

	aux->sliding = 1;
	memcpy(aux->slide_mask, visited, (size_t)st->num_slots);
	aux->slide_dx = step_x;
	aux->slide_dy = step_y;
	aux->retract_removed_slot = retract_removed_slot;
	aux->shadow_count = shadow_count;
	memcpy(aux->shadow_blocks, shadow_blocks,
	       sizeof(int32_t) * SK48_MAX_SHADOWS);
}

static void sk48_finish_move(struct arc_sprites *s,
			     const struct sk48_static *st, int32_t level,
			     struct sk48_aux *aux, int32_t *score,
			     int32_t *status, uint8_t *next_level,
			     uint8_t *action_complete);

static void sk48_continue_slide(struct arc_sprites *s,
				const struct sk48_static *st, int32_t level,
				struct sk48_aux *aux, int32_t *score,
				int32_t *status, uint8_t *next_level,
				uint8_t *action_complete)
{
	sk48_apply_visited_delta(s, st, level, aux->slide_mask, aux->slide_dx,
				 aux->slide_dy);
	int32_t shadow_base = SK48_SHADOW_BASE(st);
	for (int32_t i = 0; i < aux->shadow_count; i++) {
		int32_t block = aux->shadow_blocks[i];
		int32_t slot = shadow_base + i;
		s->x[slot] = s->x[block];
		s->y[slot] = s->y[block];
	}
	if (aux->retract_removed_slot >= 0)
		s->alive[aux->retract_removed_slot] = 0;
	aux->sliding = 0;
	aux->retract_removed_slot = -1;
	if (aux->shadow_count > 0) {
		aux->shadow_wait = 1;
		return;
	}
	sk48_finish_move(s, st, level, aux, score, status, next_level,
			 action_complete);
}

static void sk48_continue_shadow_wait(struct arc_sprites *s,
				      const struct sk48_static *st,
				      int32_t level, struct sk48_aux *aux,
				      int32_t *score, int32_t *status,
				      uint8_t *next_level,
				      uint8_t *action_complete)
{
	int32_t shadow_base = SK48_SHADOW_BASE(st);
	for (int32_t i = 0; i < aux->shadow_count; i++)
		s->alive[shadow_base + i] = 0;
	aux->shadow_wait = 0;
	aux->shadow_count = 0;
	sk48_finish_move(s, st, level, aux, score, status, next_level,
			 action_complete);
}

static void sk48_compacted_block_colors(const struct arc_sprites *s,
					const struct sk48_static *st,
					int32_t level, int32_t head_idx,
					int32_t *out_colors, int32_t *out_count)
{
	int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + head_idx];
	int32_t hx = s->x[head_slot], hy = s->y[head_slot];
	int32_t dx, dy;
	sk48_dir(st->head_rotation[level * SK48_MAX_HEADS + head_idx], &dx,
		 &dy);
	int32_t seg_count = sk48_segment_count(s, st, head_idx);
	int32_t count = 0;
	for (int32_t k = 0; k < seg_count && k < SK48_MAX_SEGMENTS; k++) {
		int32_t x = hx + k * dx * SK48_PITCH,
			y = hy + k * dy * SK48_PITCH;
		int32_t block = sk48_block_at(s, st, level, x, y);
		if (block >= 0)
			out_colors[count++] =
				arc_sprite_pixels(s, block)[1 * st->pw + 1];
	}
	for (int32_t k = count; k < SK48_MAX_SEGMENTS; k++)
		out_colors[k] = -1;
	*out_count = count;
}

static int sk48_update_markers(struct arc_sprites *s,
			       const struct sk48_static *st, int32_t level)
{
	int won = 1;
	for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
		int32_t partner_idx =
			st->head_partner[level * SK48_MAX_HEADS + hi];
		if (partner_idx < 0)
			continue;
		int32_t active_colors[SK48_MAX_SEGMENTS];
		int32_t active_count;
		sk48_compacted_block_colors(s, st, level, hi, active_colors,
					    &active_count);
		int32_t mcount = st->marker_count[level * SK48_MAX_HEADS + hi];
		for (int32_t i = 0; i < SK48_MAX_SEGMENTS - 1; i++) {
			size_t idx = (size_t)(level * SK48_MAX_HEADS + hi) *
					     (SK48_MAX_SEGMENTS - 1) +
				     i;
			int32_t marker_slot = st->marker_slot[idx];
			int in_range = i < mcount;
			int matches = i < active_count &&
				      active_colors[i] == st->target_color[idx];
			if (in_range)
				won = won && matches;
			if (in_range && marker_slot >= 0)
				s->interaction[marker_slot] =
					(int8_t)(matches ? TANGIBLE :
							   INVISIBLE);
		}
	}
	return won;
}

static void sk48_push_undo_snapshot(struct arc_sprites *s,
				    const struct sk48_static *st, int32_t level,
				    struct sk48_aux *aux)
{
	int32_t top = aux->stack_top;
	if (top >= SK48_STACK_DEPTH)
		return;
	for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
		int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + hi];
		int32_t idx = head_slot >= 0 ? head_slot : 0;
		aux->stack_head_x[top][hi] = s->x[idx];
		aux->stack_head_y[top][hi] = s->y[idx];
		aux->stack_seg_count[top][hi] = sk48_segment_count(s, st, hi);
	}
	memcpy(aux->stack_block_x + (size_t)top * st->num_slots, s->x,
	       sizeof(int32_t) * (size_t)st->num_slots);
	memcpy(aux->stack_block_y + (size_t)top * st->num_slots, s->y,
	       sizeof(int32_t) * (size_t)st->num_slots);
	aux->stack_top = top + 1;
}

static void sk48_finish_move(struct arc_sprites *s,
			     const struct sk48_static *st, int32_t level,
			     struct sk48_aux *aux, int32_t *score,
			     int32_t *status, uint8_t *next_level,
			     uint8_t *action_complete)
{
	(void)score;
	(void)next_level;
	int won = sk48_update_markers(s, st, level);
	if (won) {
		uint8_t *mask = aux->blink_mask;
		memset(mask, 0, (size_t)st->num_slots);
		for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
			int32_t head_slot =
				st->head_slot[level * SK48_MAX_HEADS + hi];
			if (head_slot < 0)
				continue;
			int32_t hx = s->x[head_slot], hy = s->y[head_slot];
			int32_t dx, dy;
			sk48_dir(st->head_rotation[level * SK48_MAX_HEADS + hi],
				 &dx, &dy);
			int32_t count = sk48_segment_count(s, st, hi);
			for (int32_t k = 0; k < count && k < SK48_MAX_SEGMENTS;
			     k++) {
				int32_t x = hx + k * dx * SK48_PITCH,
					y = hy + k * dy * SK48_PITCH;
				int32_t block =
					sk48_block_at(s, st, level, x, y);
				if (block >= 0)
					mask[block] = 1;
			}
		}
		for (int32_t i = 0; i < st->num_slots; i++)
			aux->blink_original[i] =
				arc_sprite_pixels(s, i)[1 * st->pw + 1];
		aux->blink_active = 1;
		aux->blink_frame = 0;
		return;
	}

	sk48_push_undo_snapshot(s, st, level, aux);

	*action_complete = 1;
	if (aux->steps == 0)
		*status = SK48_GAME_OVER;
}

static void sk48_continue_blink(struct arc_sprites *s,
				const struct sk48_static *st, int32_t level,
				struct sk48_aux *aux, int32_t *score,
				int32_t *status, uint8_t *next_level,
				uint8_t *action_complete)
{
	int32_t frame = aux->blink_frame;
	int lit = (frame / SK48_BLINK_PERIOD) % 2 == 0 ||
		  frame >= SK48_BLINK_LOCK_FRAME;
	const int32_t *cand = st->block_slots + (size_t)level * st->max_block;
	int32_t n = st->block_count[level];
	for (int32_t k = 0; k < n; k++) {
		int32_t i = cand[k];
		if (!aux->blink_mask[i])
			continue;
		int8_t c = lit ? (int8_t)SK48_BLINK_OFF_COLOR :
				 aux->blink_original[i];
		arc_color_remap(s, i, 0, 0, c);
	}
	aux->blink_frame = frame + 1;
	if (frame + 1 >= SK48_WIN_BLINK_FRAMES) {
		*action_complete = 1;
		int is_last = level == st->num_levels - 1;
		*score += 1;
		*next_level = (uint8_t)!is_last;
		if (is_last)
			*status = SK48_WIN;
	}
}

static void sk48_handle_extend(struct arc_sprites *s,
			       const struct sk48_static *st, int32_t level,
			       int32_t head_idx, int32_t dir_x, int32_t dir_y,
			       struct sk48_aux *aux)
{
	int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + head_idx];
	int32_t count = sk48_segment_count(s, st, head_idx);
	int32_t head_x = s->x[head_slot], head_y = s->y[head_slot];
	int32_t last_x = head_x + (count - 1) * dir_x * SK48_PITCH;
	int32_t last_y = head_y + (count - 1) * dir_y * SK48_PITCH;
	if (sk48_oob_or_wall(s, st, level, last_x + dir_x * SK48_PITCH,
			     last_y + dir_y * SK48_PITCH))
		return;

	uint8_t *visited = aux->push_visited;
	memset(visited, 0, (size_t)st->num_slots);
	int32_t shadow_blocks[SK48_MAX_SHADOWS];
	for (int32_t i = 0; i < SK48_MAX_SHADOWS; i++)
		shadow_blocks[i] = -1;
	int32_t shadow_count = 0;

	int horizontal = dir_y == 0;
	for (int32_t k = 0; k < count; k++) {
		int32_t x = head_x + k * dir_x * SK48_PITCH,
			y = head_y + k * dir_y * SK48_PITCH;
		int32_t seg =
			sk48_segment_at(s, st, level, x, y, horizontal, -1);
		if (seg >= 0)
			sk48_push_node(s, st, level, seg, -1, dir_x, dir_y,
				       visited, shadow_blocks, &shadow_count);
	}

	int32_t new_slot = sk48_free_segment_slot(s, st, head_idx);
	int32_t rotation = st->head_rotation[level * SK48_MAX_HEADS + head_idx];
	sk48_write_segment(s, st, new_slot, head_x, head_y, rotation);
	arc_color_remap(s, new_slot, 1, SK48_SELECT_SEG1_FROM,
			SK48_SELECT_SEG1_TO);
	arc_color_remap(s, new_slot, 1, SK48_SELECT_SEG2_FROM,
			SK48_SELECT_SEG2_TO);

	sk48_begin_slide(s, st, level, aux, visited, dir_x, dir_y,
			 shadow_blocks, shadow_count, -1);
}

static void sk48_handle_retract(struct arc_sprites *s,
				const struct sk48_static *st, int32_t level,
				int32_t head_idx, int32_t dir_x, int32_t dir_y,
				struct sk48_aux *aux)
{
	int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + head_idx];
	int32_t count = sk48_segment_count(s, st, head_idx);
	if (count == 1)
		return;
	int32_t head_x = s->x[head_slot], head_y = s->y[head_slot];
	int32_t base_dir_x, base_dir_y;
	sk48_dir(st->head_rotation[level * SK48_MAX_HEADS + head_idx],
		 &base_dir_x, &base_dir_y);
	int horizontal = base_dir_y == 0;
	int32_t removed_slot =
		sk48_segment_at(s, st, level, head_x, head_y, horizontal, -1);

	uint8_t *visited = aux->push_visited;
	memset(visited, 0, (size_t)st->num_slots);
	int32_t shadow_blocks[SK48_MAX_SHADOWS];
	for (int32_t i = 0; i < SK48_MAX_SHADOWS; i++)
		shadow_blocks[i] = -1;
	int32_t shadow_count = 0;

	for (int32_t k = 1; k < count; k++) {
		int32_t x = head_x + k * base_dir_x * SK48_PITCH,
			y = head_y + k * base_dir_y * SK48_PITCH;
		int32_t seg =
			sk48_segment_at(s, st, level, x, y, horizontal, -1);
		if (seg >= 0)
			sk48_push_node(s, st, level, seg, -1, dir_x, dir_y,
				       visited, shadow_blocks, &shadow_count);
	}

	sk48_begin_slide(s, st, level, aux, visited, dir_x, dir_y,
			 shadow_blocks, shadow_count, removed_slot);
}

static void sk48_handle_orthogonal(struct arc_sprites *s,
				   const struct sk48_static *st, int32_t level,
				   int32_t head_idx, int32_t dir_x,
				   int32_t dir_y, struct sk48_aux *aux)
{
	int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + head_idx];
	int32_t head_x = s->x[head_slot], head_y = s->y[head_slot];
	int32_t dx = dir_x * SK48_PITCH, dy = dir_y * SK48_PITCH;
	int32_t rail_x = head_x + 2 + dx / 2, rail_y = head_y + 2 + dy / 2;
	if (arc_get_sprite_at(s, rail_x, rail_y, st->tag_rail, 0) < 0)
		return;

	int32_t count = sk48_segment_count(s, st, head_idx);
	int32_t base_dir_x, base_dir_y;
	sk48_dir(st->head_rotation[level * SK48_MAX_HEADS + head_idx],
		 &base_dir_x, &base_dir_y);
	int horizontal = base_dir_y == 0;

	uint8_t *visited = aux->push_visited;
	memset(visited, 0, (size_t)st->num_slots);
	int32_t shadow_blocks[SK48_MAX_SHADOWS];
	for (int32_t i = 0; i < SK48_MAX_SHADOWS; i++)
		shadow_blocks[i] = -1;
	int32_t shadow_count = 0;

	int ok = 1;
	for (int32_t k = 0; k < count; k++) {
		if (!ok)
			break;
		int32_t x = head_x + k * base_dir_x * SK48_PITCH,
			y = head_y + k * base_dir_y * SK48_PITCH;
		int32_t seg =
			sk48_segment_at(s, st, level, x, y, horizontal, -1);
		if (seg < 0)
			continue;
		if (!sk48_push_node(s, st, level, seg, -1, dir_x, dir_y,
				    visited, shadow_blocks, &shadow_count))
			ok = 0;
	}
	if (!ok)
		return;

	visited[head_slot] = 1;
	sk48_begin_slide(s, st, level, aux, visited, dir_x, dir_y,
			 shadow_blocks, shadow_count, -1);
}

static void sk48_handle_move(struct arc_sprites *s,
			     const struct sk48_static *st, int32_t level,
			     int32_t action_id, struct sk48_aux *aux)
{
	aux->steps = aux->steps > 0 ? aux->steps - 1 : 0;
	int32_t move_x = 0, move_y = 0;
	switch (action_id) {
	case SK48_ACTION1:
		move_y = -1;
		break;
	case SK48_ACTION2:
		move_y = 1;
		break;
	case SK48_ACTION3:
		move_x = -1;
		break;
	case SK48_ACTION4:
		move_x = 1;
		break;
	default:
		break;
	}
	int32_t head_idx = aux->selected_head;
	int32_t base_dir_x, base_dir_y;
	sk48_dir(st->head_rotation[level * SK48_MAX_HEADS + head_idx],
		 &base_dir_x, &base_dir_y);
	int is_extend = move_x == base_dir_x && move_y == base_dir_y;
	int is_retract = move_x == -base_dir_x && move_y == -base_dir_y;
	if (is_extend)
		sk48_handle_extend(s, st, level, head_idx, move_x, move_y, aux);
	else if (is_retract)
		sk48_handle_retract(s, st, level, head_idx, move_x, move_y,
				    aux);
	else
		sk48_handle_orthogonal(s, st, level, head_idx, move_x, move_y,
				       aux);
}

static void sk48_handle_click(struct arc_sprites *s,
			      const struct arc_camera *camera,
			      const struct sk48_static *st, int32_t level,
			      int32_t action_x, int32_t action_y,
			      struct sk48_aux *aux, uint8_t *action_complete)
{
	int32_t scale, x_pad, y_pad;
	arc_scale_and_offset(camera, &scale, &x_pad, &y_pad);
	int32_t ddx = action_x - x_pad, ddy = action_y - y_pad;
	int32_t gx = ddx >= 0 ? ddx / scale : -1;
	int32_t gy = ddy >= 0 ? ddy / scale : -1;
	int on_board =
		gx >= 0 && gy >= 0 && gx < camera->width && gy < camera->height;
	int32_t wx = gx + camera->x, wy = gy + camera->y;
	int32_t hit =
		on_board ? arc_get_sprite_at(s, wx, wy, st->tag_click, 0) : -1;

	int32_t found = -1;
	for (int32_t hi = 0; hi < SK48_MAX_HEADS && found < 0; hi++) {
		int32_t partner_idx =
			st->head_partner[level * SK48_MAX_HEADS + hi];
		if (partner_idx < 0)
			continue;
		int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + hi];
		int32_t partner_slot =
			st->head_slot[level * SK48_MAX_HEADS + partner_idx];
		if (hit >= 0 && (hit == head_slot || hit == partner_slot) &&
		    hi != aux->selected_head)
			found = hi;
	}
	if (found >= 0)
		sk48_select_head(s, st, level, aux, found, 0);
	*action_complete = 1;
}

static void sk48_pop_undo_snapshot(struct arc_sprites *s,
				   const struct sk48_static *st, int32_t level,
				   struct sk48_aux *aux)
{
	if (aux->stack_top < 2)
		return;
	int32_t new_top = aux->stack_top - 1;
	int32_t row = new_top - 1;
	int32_t base = SK48_SEGMENT_BASE(st);

	const int32_t *cand = st->block_slots + (size_t)level * st->max_block;
	int32_t n = st->block_count[level];
	for (int32_t k = 0; k < n; k++) {
		int32_t i = cand[k];
		s->x[i] = aux->stack_block_x[(size_t)row * st->num_slots + i];
		s->y[i] = aux->stack_block_y[(size_t)row * st->num_slots + i];
	}

	for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
		int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + hi];
		if (head_slot < 0)
			continue;
		int32_t new_x = aux->stack_head_x[row][hi];
		int32_t new_y = aux->stack_head_y[row][hi];
		int32_t count = aux->stack_seg_count[row][hi];
		int32_t rotation =
			st->head_rotation[level * SK48_MAX_HEADS + hi];
		int32_t dx, dy;
		sk48_dir(rotation, &dx, &dy);
		s->x[head_slot] = new_x;
		s->y[head_slot] = new_y;
		int32_t hbase = base + hi * SK48_MAX_SEGMENTS;
		for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++)
			s->alive[hbase + k] = 0;
		for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++) {
			if (k < count)
				sk48_write_segment(s, st, hbase + k,
						   new_x + k * dx * SK48_PITCH,
						   new_y + k * dy * SK48_PITCH,
						   rotation);
		}
	}

	aux->stack_top = new_top;
	int32_t selected = aux->selected_head;
	sk48_select_head(s, st, level, aux, selected, 1);
	sk48_update_markers(s, st, level);
}

static void sk48_dispatch_action(struct arc_sprites *s,
				 const struct arc_camera *camera,
				 const struct sk48_static *st, int32_t level,
				 int32_t action_id, int32_t action_x,
				 int32_t action_y, struct sk48_aux *aux,
				 uint8_t *action_complete)
{
	if (action_id >= SK48_ACTION1 && action_id <= SK48_ACTION4) {
		sk48_handle_move(s, st, level, action_id, aux);
		if (!aux->sliding)
			*action_complete = 1;
		return;
	}
	if (action_id == SK48_ACTION6) {
		sk48_handle_click(s, camera, st, level, action_x, action_y, aux,
				  action_complete);
		return;
	}
	if (action_id == SK48_ACTION7) {
		sk48_pop_undo_snapshot(s, st, level, aux);
		*action_complete = 1;
		return;
	}
	*action_complete = 1;
}

void sk48_aux_alloc(struct sk48_aux *aux, int32_t num_slots)
{
	aux->num_slots = num_slots;
	aux->slide_mask = calloc((size_t)num_slots, 1);
	aux->blink_mask = calloc((size_t)num_slots, 1);
	aux->blink_original = calloc((size_t)num_slots, 1);
	aux->stack_block_x = calloc(
		(size_t)SK48_STACK_DEPTH * (size_t)num_slots, sizeof(int32_t));
	aux->stack_block_y = calloc(
		(size_t)SK48_STACK_DEPTH * (size_t)num_slots, sizeof(int32_t));
	aux->push_visited = calloc((size_t)num_slots, 1);
}

void sk48_aux_free(struct sk48_aux *aux)
{
	free(aux->slide_mask);
	free(aux->blink_mask);
	free(aux->blink_original);
	free(aux->stack_block_x);
	free(aux->stack_block_y);
	free(aux->push_visited);
	aux->slide_mask = NULL;
	aux->blink_mask = NULL;
	aux->blink_original = NULL;
	aux->stack_block_x = NULL;
	aux->stack_block_y = NULL;
	aux->push_visited = NULL;
}

void sk48_zero_aux(struct sk48_aux *aux)
{
	int32_t num_slots = aux->num_slots;
	aux->selected_head = 0;
	aux->sliding = 0;
	memset(aux->slide_mask, 0, (size_t)num_slots);
	aux->slide_dx = 0;
	aux->slide_dy = 0;
	aux->retract_removed_slot = -1;
	aux->shadow_wait = 0;
	aux->shadow_count = 0;
	for (int32_t i = 0; i < SK48_MAX_SHADOWS; i++)
		aux->shadow_blocks[i] = -1;
	aux->blink_active = 0;
	aux->blink_frame = 0;
	memset(aux->blink_mask, 0, (size_t)num_slots);
	memset(aux->blink_original, 0, (size_t)num_slots);
	aux->steps = SK48_STEP_BUDGET;
	aux->stack_top = 0;
	memset(aux->stack_head_x, 0, sizeof aux->stack_head_x);
	memset(aux->stack_head_y, 0, sizeof aux->stack_head_y);
	memset(aux->stack_seg_count, 0, sizeof aux->stack_seg_count);
	memset(aux->stack_block_x, 0,
	       sizeof(int32_t) * (size_t)SK48_STACK_DEPTH * (size_t)num_slots);
	memset(aux->stack_block_y, 0,
	       sizeof(int32_t) * (size_t)SK48_STACK_DEPTH * (size_t)num_slots);
}

void sk48_on_set_level(struct arc_sprites *s, const struct sk48_static *st,
		       int32_t level, struct sk48_aux *aux)
{
	const int32_t *rcand =
		st->removal_slots + (size_t)level * st->max_removal;
	int32_t rn = st->removal_count[level];
	for (int32_t k = 0; k < rn; k++)
		s->alive[rcand[k]] = 0;

	int32_t base = SK48_SEGMENT_BASE(st);
	for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
		int32_t head_slot = st->head_slot[level * SK48_MAX_HEADS + hi];
		if (head_slot < 0)
			continue;
		int32_t rotation =
			st->head_rotation[level * SK48_MAX_HEADS + hi];
		int32_t dx, dy;
		sk48_dir(rotation, &dx, &dy);
		int32_t count =
			st->head_initial_segments[level * SK48_MAX_HEADS + hi];
		int32_t hx = s->x[head_slot], hy = s->y[head_slot];
		int32_t hbase = base + hi * SK48_MAX_SEGMENTS;
		for (int32_t k = 0; k < SK48_MAX_SEGMENTS; k++) {
			if (k < count)
				sk48_write_segment(s, st, hbase + k,
						   hx + k * dx * SK48_PITCH,
						   hy + k * dy * SK48_PITCH,
						   rotation);
		}
	}

	for (int32_t hi = 0; hi < SK48_MAX_HEADS; hi++) {
		for (int32_t i = 0; i < SK48_MAX_SEGMENTS - 1; i++) {
			size_t idx = (size_t)(level * SK48_MAX_HEADS + hi) *
					     (SK48_MAX_SEGMENTS - 1) +
				     i;
			int32_t slot = st->marker_slot[idx];
			if (slot < 0)
				continue;
			int32_t x = st->marker_x[idx], y = st->marker_y[idx];
			size_t area = (size_t)st->ph * st->pw;
			int8_t *dst = arc_sprite_pixels_mut(s, slot);
			memcpy(dst, st->marker_pixels, area);
			s->x[slot] = x;
			s->y[slot] = y;
			s->h[slot] = st->marker_h;
			s->w[slot] = st->marker_w;
			s->alive[slot] = 1;
			s->layer[slot] = 2;
			s->interaction[slot] = INVISIBLE;
			s->blocking[slot] = PIXEL_PERFECT;
		}
	}

	sk48_zero_aux(aux);
	sk48_select_head(s, st, level, aux, 0, 1);
	sk48_update_markers(s, st, level);
	sk48_push_undo_snapshot(s, st, level, aux);
}

void sk48_step_once(struct arc_sprites *s, const struct arc_camera *camera,
		    const struct sk48_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct sk48_aux *aux, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete)
{
	(void)action_count;
	if (aux->blink_active) {
		sk48_continue_blink(s, st, level, aux, score, status,
				    next_level, action_complete);
		return;
	}
	if (aux->sliding) {
		sk48_continue_slide(s, st, level, aux, score, status,
				    next_level, action_complete);
		return;
	}
	if (aux->shadow_wait) {
		sk48_continue_shadow_wait(s, st, level, aux, score, status,
					  next_level, action_complete);
		return;
	}
	sk48_dispatch_action(s, camera, st, level, action_id, action_x,
			     action_y, aux, action_complete);
}

void sk48_render_interface(int8_t *frame, const struct sk48_aux *aux)
{
	int32_t filled = (ARC_FRAME_SIZE * aux->steps + SK48_STEP_BUDGET - 1) /
			 SK48_STEP_BUDGET;
	if (filled < 0)
		filled = 0;
	if (filled > ARC_FRAME_SIZE)
		filled = ARC_FRAME_SIZE;
	int8_t *row = frame + (size_t)SK48_HUD_ROW * ARC_FRAME_SIZE;
	for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
		row[c] = (int8_t)(c < filled ? SK48_HUD_FILL : SK48_HUD_BG);
}
