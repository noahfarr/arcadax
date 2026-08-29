#include "cn04.h"

#include <stdlib.h>
#include <string.h>

enum {
	CN04_CONNECTOR_A = 8,
	CN04_CONNECTOR_B = 13,
	CN04_MATED = 3,
	CN04_ZEROED = 0,
	CN04_GREY = 4
};
enum {
	CN04_ACTION1 = 1,
	CN04_ACTION2 = 2,
	CN04_ACTION3 = 3,
	CN04_ACTION4 = 4,
	CN04_ACTION5 = 5,
	CN04_ACTION6 = 6
};
enum { CN04_WIN = 2, CN04_GAME_OVER = 3 };

static inline int32_t cn04_clamp(int32_t v, int32_t lo, int32_t hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

void cn04_aux_alloc(struct cn04_aux *aux, int32_t num_slots, int32_t ph,
		    int32_t pw)
{
	aux->rotation_index = calloc((size_t)num_slots, sizeof(int32_t));
	aux->snapshot = calloc((size_t)num_slots * (size_t)ph * (size_t)pw, 1);
	aux->selected = -1;
	aux->ping_pong = 1;
	aux->pending_win = 0;
}

void cn04_aux_free(struct cn04_aux *aux)
{
	free(aux->rotation_index);
	free(aux->snapshot);
	aux->rotation_index = NULL;
	aux->snapshot = NULL;
}

void cn04_zero_aux(struct cn04_aux *aux, const struct cn04_static *st)
{
	int32_t n = st->num_slots;
	int32_t area = st->ph * st->pw;
	memset(aux->rotation_index, 0, (size_t)n * sizeof(int32_t));
	memset(aux->snapshot, 0, (size_t)n * (size_t)area);
	aux->selected = -1;
	aux->ping_pong = 1;
	aux->pending_win = 0;
}

static inline size_t cn04_rot_off(const struct cn04_static *st, int32_t level,
				  int32_t slot, int32_t ridx)
{
	return (((size_t)level * (size_t)st->num_slots + (size_t)slot) * 4 +
		(size_t)ridx) *
	       (size_t)st->ph * (size_t)st->pw;
}

static inline int32_t cn04_hw_off(const struct cn04_static *st, int32_t level,
				  int32_t slot, int32_t ridx)
{
	return (level * st->num_slots + slot) * 4 + ridx;
}

static inline const int8_t *cn04_pattern(const struct cn04_static *st,
					 int32_t level, int32_t slot,
					 int32_t ridx)
{
	return st->rotvar + cn04_rot_off(st, level, slot, ridx);
}

static void cn04_apply_table(const int8_t *src, const int32_t *row,
			     const int32_t *col, const uint8_t *valid,
			     int32_t ph, int32_t pw, int8_t *dst)
{
	int32_t area = ph * pw;
	for (int32_t k = 0; k < area; k++)
		dst[k] = valid[k] ? src[row[k] * pw + col[k]] : (int8_t)-1;
}

static void cn04_to_rotation(const struct cn04_static *st, int32_t level,
			     int32_t slot, int32_t ridx,
			     const int8_t *base_patch, int8_t *out)
{
	size_t off = cn04_rot_off(st, level, slot, ridx);
	cn04_apply_table(base_patch, st->fwd_row + off, st->fwd_col + off,
			 st->fwd_valid + off, st->ph, st->pw, out);
}

static void cn04_to_base(const struct cn04_static *st, int32_t level,
			 int32_t slot, int32_t ridx, const int8_t *patch,
			 int8_t *out)
{
	size_t off = cn04_rot_off(st, level, slot, ridx);
	cn04_apply_table(patch, st->bwd_row + off, st->bwd_col + off,
			 st->bwd_valid + off, st->ph, st->pw, out);
}

static void cn04_restore_pixels(const int8_t *snapshot, int greymask,
				int32_t area, int8_t *out)
{
	for (int32_t k = 0; k < area; k++) {
		int8_t v = snapshot[k];
		int8_t r = (v == CN04_CONNECTOR_B) ? CN04_CONNECTOR_A : v;
		if (greymask && r >= 0 && r != CN04_CONNECTOR_A)
			r = CN04_GREY;
		out[k] = r;
	}
}

static void cn04_build_grids(const struct arc_sprites *s,
			     const struct cn04_static *st,
			     const struct cn04_aux *aux, int32_t level,
			     uint8_t *grid_a, uint8_t *grid_b)
{
	memset(grid_a, 0, (size_t)ARC_FRAME_SIZE * ARC_FRAME_SIZE);
	memset(grid_b, 0, (size_t)ARC_FRAME_SIZE * ARC_FRAME_SIZE);
	int32_t n = st->num_slots, ph = st->ph, pw = st->pw;
	for (int32_t i = 0; i < n; i++) {
		if (!arc_sprite_visible(s, i))
			continue;
		const int8_t *patt =
			cn04_pattern(st, level, i, aux->rotation_index[i]);
		int32_t xi = s->x[i], yi = s->y[i];
		for (int32_t r = 0; r < ph; r++) {
			int32_t wy = cn04_clamp(yi + r, 0, ARC_FRAME_SIZE - 1);
			for (int32_t c = 0; c < pw; c++) {
				int8_t v = patt[r * pw + c];
				if (v != CN04_CONNECTOR_A &&
				    v != CN04_CONNECTOR_B)
					continue;
				int32_t wx = cn04_clamp(xi + c, 0,
							ARC_FRAME_SIZE - 1);
				uint8_t *cell =
					(v == CN04_CONNECTOR_A ? grid_a :
								 grid_b) +
					(size_t)wy * ARC_FRAME_SIZE + wx;
				if (*cell < 255)
					(*cell)++;
			}
		}
	}
}

static void cn04_full_refresh(struct arc_sprites *s,
			      const struct cn04_static *st,
			      const struct cn04_aux *aux, int32_t level)
{
	int32_t n = st->num_slots, ph = st->ph, pw = st->pw;
	uint8_t grid_a[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	uint8_t grid_b[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	cn04_build_grids(s, st, aux, level, grid_a, grid_b);
	int greymask = st->greymask[level];
	int32_t selected = aux->selected;
	for (int32_t i = 0; i < n; i++) {
		if (!arc_sprite_visible(s, i))
			continue;
		const int8_t *patt =
			cn04_pattern(st, level, i, aux->rotation_index[i]);
		int8_t *dst = arc_sprite_pixels_mut(s, i);
		int is_sel = (i == selected);
		int32_t xi = s->x[i], yi = s->y[i];
		for (int32_t r = 0; r < ph; r++) {
			int32_t wy = cn04_clamp(yi + r, 0, ARC_FRAME_SIZE - 1);
			for (int32_t c = 0; c < pw; c++) {
				int8_t v = patt[r * pw + c];
				int8_t p = (v == CN04_CONNECTOR_B) ?
						   CN04_CONNECTOR_A :
						   v;
				if (is_sel && !greymask && p >= 0 &&
				    p != CN04_CONNECTOR_A)
					p = CN04_ZEROED;
				if (!is_sel && greymask && p >= 0)
					p = CN04_GREY;
				int connector = (v == CN04_CONNECTOR_A) ||
						(v == CN04_CONNECTOR_B);
				if (connector) {
					int32_t wx = cn04_clamp(
						xi + c, 0, ARC_FRAME_SIZE - 1);
					int32_t cnt =
						(int32_t)grid_a
							[(size_t)wy *
								 ARC_FRAME_SIZE +
							 wx] +
						(int32_t)grid_b
							[(size_t)wy *
								 ARC_FRAME_SIZE +
							 wx];
					if (cnt == 2 && p == CN04_CONNECTOR_A)
						p = CN04_MATED;
				}
				dst[r * pw + c] = p;
			}
		}
	}
}

static int cn04_check_win(const struct arc_sprites *s,
			  const struct cn04_static *st,
			  const struct cn04_aux *aux, int32_t level)
{
	int32_t n = st->num_slots, ph = st->ph, pw = st->pw;
	uint8_t grid_a[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	uint8_t grid_b[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	cn04_build_grids(s, st, aux, level, grid_a, grid_b);
	for (int32_t i = 0; i < n; i++) {
		if (!arc_sprite_visible(s, i))
			continue;
		const int8_t *patt =
			cn04_pattern(st, level, i, aux->rotation_index[i]);
		int32_t xi = s->x[i], yi = s->y[i];
		for (int32_t r = 0; r < ph; r++) {
			int32_t wy = cn04_clamp(yi + r, 0, ARC_FRAME_SIZE - 1);
			for (int32_t c = 0; c < pw; c++) {
				int8_t v = patt[r * pw + c];
				if (v != CN04_CONNECTOR_A &&
				    v != CN04_CONNECTOR_B)
					continue;
				int32_t wx = cn04_clamp(xi + c, 0,
							ARC_FRAME_SIZE - 1);
				uint8_t cnt =
					(v == CN04_CONNECTOR_A) ?
						grid_a[(size_t)wy *
							       ARC_FRAME_SIZE +
						       wx] :
						grid_b[(size_t)wy *
							       ARC_FRAME_SIZE +
						       wx];
				if (cnt != 2)
					return 0;
			}
		}
	}
	return 1;
}

static void cn04_deselect(struct arc_sprites *s, const struct cn04_static *st,
			  struct cn04_aux *aux, int32_t level)
{
	int32_t sel = aux->selected;
	int32_t area = st->ph * st->pw;
	int greymask = st->greymask[level];
	int8_t restored_base[area > 0 ? area : 1];
	int8_t restored[area > 0 ? area : 1];
	cn04_restore_pixels(aux->snapshot + (size_t)sel * area, greymask, area,
			    restored_base);
	cn04_to_rotation(st, level, sel, aux->rotation_index[sel],
			 restored_base, restored);
	int8_t *dst = arc_sprite_pixels_mut(s, sel);
	memcpy(dst, restored, (size_t)area);
	s->layer[sel] = 0;
	aux->selected = -1;
}

static void cn04_select(struct arc_sprites *s, const struct cn04_static *st,
			struct cn04_aux *aux, int32_t level, int32_t target)
{
	if (aux->selected >= 0)
		cn04_deselect(s, st, aux, level);
	int32_t area = st->ph * st->pw;
	int8_t base_snapshot[area > 0 ? area : 1];
	cn04_to_base(st, level, target, aux->rotation_index[target],
		     arc_sprite_pixels(s, target), base_snapshot);
	memcpy(aux->snapshot + (size_t)target * area, base_snapshot,
	       (size_t)area);
	s->layer[target] = 4;
	aux->selected = target;
	cn04_full_refresh(s, st, aux, level);
}

static void cn04_cycle_group(struct arc_sprites *s,
			     const struct cn04_static *st, struct cn04_aux *aux,
			     int32_t level)
{
	int32_t old = aux->selected;
	arc_set_visible(s, old, 0);
	int32_t old_x = s->x[old], old_y = s->y[old];
	int32_t n = st->num_slots;
	int32_t base = level * n + old;
	int32_t size = st->group_size[base];
	int32_t rank = st->group_rank[base];
	const int32_t *members =
		st->group_members + (size_t)base * (size_t)st->max_group;

	int32_t fwd_next = rank + 1;
	int fwd_oob = fwd_next >= size;
	int32_t bwd_next = rank - 1;
	int bwd_oob = bwd_next < 0;
	int32_t rank_fwd_path =
		fwd_oob ? (bwd_oob ? rank : bwd_next) : fwd_next;
	int dir_fwd_path = fwd_oob ? (bwd_oob ? 1 : 0) : 1;
	int32_t rank_bwd_path =
		bwd_oob ? (fwd_oob ? rank : fwd_next) : bwd_next;
	int dir_bwd_path = bwd_oob ? (fwd_oob ? 0 : 1) : 0;
	int32_t new_rank = aux->ping_pong ? rank_fwd_path : rank_bwd_path;
	int new_dir = aux->ping_pong ? dir_fwd_path : dir_bwd_path;
	int32_t new_slot = members[new_rank];

	int32_t area = st->ph * st->pw;
	int greymask = st->greymask[level];
	int8_t restored_base[area > 0 ? area : 1];
	int8_t restored[area > 0 ? area : 1];
	cn04_restore_pixels(aux->snapshot + (size_t)old * area, greymask, area,
			    restored_base);
	cn04_to_rotation(st, level, old, aux->rotation_index[old],
			 restored_base, restored);
	int8_t *dst_old = arc_sprite_pixels_mut(s, old);
	memcpy(dst_old, restored, (size_t)area);
	s->layer[old] = 0;

	int8_t new_snapshot[area > 0 ? area : 1];
	cn04_to_base(st, level, new_slot, aux->rotation_index[new_slot],
		     arc_sprite_pixels(s, new_slot), new_snapshot);
	s->layer[new_slot] = 4;
	s->x[new_slot] = old_x;
	s->y[new_slot] = old_y;
	arc_set_visible(s, new_slot, 1);

	aux->selected = new_slot;
	aux->ping_pong = (uint8_t)new_dir;
	memcpy(aux->snapshot + (size_t)new_slot * area, new_snapshot,
	       (size_t)area);

	cn04_full_refresh(s, st, aux, level);
}

static void cn04_rotate_selected(struct arc_sprites *s,
				 const struct arc_camera *camera,
				 const struct cn04_static *st,
				 struct cn04_aux *aux, int32_t level)
{
	int32_t sel = aux->selected;
	int32_t new_idx = (aux->rotation_index[sel] + 1) % 4;
	int32_t hw_off = cn04_hw_off(st, level, sel, new_idx);
	int32_t new_h = st->h_table[hw_off];
	int32_t new_w = st->w_table[hw_off];
	int32_t max_x = camera->width - new_w;
	if (max_x < 0)
		max_x = 0;
	int32_t max_y = camera->height - new_h;
	if (max_y < 0)
		max_y = 0;
	int32_t new_x = cn04_clamp(s->x[sel], 0, max_x);
	int32_t new_y = cn04_clamp(s->y[sel], 0, max_y);
	s->h[sel] = new_h;
	s->w[sel] = new_w;
	s->x[sel] = new_x;
	s->y[sel] = new_y;
	aux->rotation_index[sel] = new_idx;
	cn04_full_refresh(s, st, aux, level);
}

static void cn04_display_to_grid(const struct arc_camera *camera,
				 int32_t display_x, int32_t display_y,
				 int32_t *world_x, int32_t *world_y, int *valid)
{
	int32_t scale, x_pad, y_pad;
	arc_scale_and_offset(camera, &scale, &x_pad, &y_pad);
	int32_t dx = display_x - x_pad, dy = display_y - y_pad;
	int32_t gx = dx >= 0 ? dx / scale : -1;
	int32_t gy = dy >= 0 ? dy / scale : -1;
	*valid =
		gx >= 0 && gy >= 0 && gx < camera->width && gy < camera->height;
	*world_x = gx + camera->x;
	*world_y = gy + camera->y;
}

static inline int cn04_contains(const struct arc_sprites *s, int32_t i,
				int32_t x, int32_t y)
{
	return x >= s->x[i] && y >= s->y[i] && x < s->x[i] + s->w[i] &&
	       y < s->y[i] + s->h[i];
}

static void cn04_handle_click(struct arc_sprites *s,
			      const struct arc_camera *camera,
			      const struct cn04_static *st, int32_t level,
			      int32_t action_x, int32_t action_y,
			      struct cn04_aux *aux)
{
	int32_t world_x, world_y;
	int on_board;
	cn04_display_to_grid(camera, action_x, action_y, &world_x, &world_y,
			     &on_board);
	if (!on_board)
		return;

	int32_t n = st->num_slots, ph = st->ph, pw = st->pw;
	int32_t target = -1;
	for (int32_t i = 0; i < n; i++) {
		if (!arc_sprite_visible(s, i))
			continue;
		if (!cn04_contains(s, i, world_x, world_y))
			continue;
		int32_t ly = cn04_clamp(world_y - s->y[i], 0, ph - 1);
		int32_t lx = cn04_clamp(world_x - s->x[i], 0, pw - 1);
		const int8_t *patt =
			cn04_pattern(st, level, i, aux->rotation_index[i]);
		if (patt[ly * pw + lx] < 0)
			continue;
		target = i;
		break;
	}
	if (target < 0)
		return;

	int32_t selected = aux->selected;
	if (selected >= 0 && target == selected) {
		int32_t ly = cn04_clamp(world_y - s->y[selected], 0, ph - 1);
		int32_t lx = cn04_clamp(world_x - s->x[selected], 0, pw - 1);
		int8_t disp_val = arc_sprite_pixels(s, selected)[ly * pw + lx];
		int32_t gsize = st->group_size[level * n + selected];
		if (gsize > 1 && disp_val == CN04_ZEROED)
			cn04_cycle_group(s, st, aux, level);
		else
			cn04_deselect(s, st, aux, level);
	} else {
		cn04_select(s, st, aux, level, target);
	}
}

static void cn04_handle_action5(struct arc_sprites *s,
				const struct arc_camera *camera,
				const struct cn04_static *st, int32_t level,
				struct cn04_aux *aux, uint8_t *action_complete)
{
	int32_t sel = aux->selected;
	if (sel < 0) {
		*action_complete = 1;
		return;
	}
	int32_t gsize = st->group_size[level * st->num_slots + sel];
	if (gsize > 1)
		cn04_cycle_group(s, st, aux, level);
	else
		cn04_rotate_selected(s, camera, st, aux, level);
	if (cn04_check_win(s, st, aux, level))
		aux->pending_win = 1;
	else
		*action_complete = 1;
}

static void cn04_handle_move(struct arc_sprites *s,
			     const struct arc_camera *camera,
			     const struct cn04_static *st, int32_t level,
			     int32_t action_id, struct cn04_aux *aux,
			     uint8_t *action_complete)
{
	int32_t sel = aux->selected;
	if (sel < 0) {
		*action_complete = 1;
		return;
	}
	int32_t dx = (action_id == CN04_ACTION3) ?
			     -1 :
			     (action_id == CN04_ACTION4 ? 1 : 0);
	int32_t dy = (action_id == CN04_ACTION1) ?
			     -1 :
			     (action_id == CN04_ACTION2 ? 1 : 0);
	int32_t new_x = s->x[sel] + dx, new_y = s->y[sel] + dy;
	int in_bounds = new_x >= 0 && new_y >= 0 &&
			new_x + s->w[sel] <= camera->width &&
			new_y + s->h[sel] <= camera->height;
	if (in_bounds) {
		s->x[sel] = new_x;
		s->y[sel] = new_y;
	}
	cn04_full_refresh(s, st, aux, level);
	if (cn04_check_win(s, st, aux, level))
		aux->pending_win = 1;
	else
		*action_complete = 1;
}

static void cn04_play(struct arc_sprites *s, const struct arc_camera *camera,
		      const struct cn04_static *st, int32_t level,
		      int32_t action_id, int32_t action_x, int32_t action_y,
		      struct cn04_aux *aux, uint8_t *action_complete)
{
	if (action_id == CN04_ACTION6) {
		cn04_handle_click(s, camera, st, level, action_x, action_y,
				  aux);
		*action_complete = 1;
	} else if (action_id == CN04_ACTION5) {
		cn04_handle_action5(s, camera, st, level, aux, action_complete);
	} else if (action_id >= CN04_ACTION1 && action_id <= CN04_ACTION4) {
		cn04_handle_move(s, camera, st, level, action_id, aux,
				 action_complete);
	} else {
		*action_complete = 1;
	}
}

static void cn04_next_level(const struct cn04_static *st, int32_t level,
			    int32_t *score, int32_t *status,
			    uint8_t *next_level)
{
	int is_last = level == st->num_levels - 1;
	*score += 1;
	*next_level = (uint8_t)(!is_last);
	if (is_last)
		*status = CN04_WIN;
}

void cn04_on_set_level(struct arc_sprites *sprites, struct arc_camera *camera,
		       const struct cn04_static *st, int32_t level,
		       struct cn04_aux *aux)
{
	int32_t n = st->num_slots, ph = st->ph, pw = st->pw, area = ph * pw;
	camera->background = st->level_bg[level];
	camera->letter_box = st->level_bg[level];

	for (int32_t i = 0; i < n; i++)
		sprites->layer[i] = 0;

	int greymask = st->greymask[level];
	for (int32_t i = 0; i < n; i++) {
		int32_t ridx = st->init_rot[level * n + i];
		aux->rotation_index[i] = ridx;
		if (!sprites->alive[i]) {
			memset(aux->snapshot + (size_t)i * area, 0,
			       (size_t)area);
			continue;
		}
		const int8_t *patt = cn04_pattern(st, level, i, ridx);
		int8_t *dst = arc_sprite_pixels_mut(sprites, i);
		for (int32_t k = 0; k < area; k++) {
			int8_t v = patt[k];
			int8_t d0 =
				(v == CN04_CONNECTOR_B) ? CN04_CONNECTOR_A : v;
			int8_t d = (greymask && d0 >= 0) ? CN04_GREY : d0;
			dst[k] = d;
			aux->snapshot[(size_t)i * area + k] = d;
		}
	}

	aux->selected = -1;
	aux->ping_pong = 1;
	aux->pending_win = 0;

	int32_t init_sel = st->init_selected[level];
	if (init_sel >= 0)
		cn04_select(sprites, st, aux, level, init_sel);
	else
		cn04_full_refresh(sprites, st, aux, level);
}

void cn04_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct cn04_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct cn04_aux *aux, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete)
{
	if (aux->pending_win) {
		aux->pending_win = 0;
		cn04_next_level(st, level, score, status, next_level);
		*action_complete = 1;
		return;
	}

	int32_t budget = st->budget[level];
	if (action_count >= budget) {
		*status = CN04_GAME_OVER;
		*action_complete = 1;
		return;
	}
	cn04_play(sprites, camera, st, level, action_id, action_x, action_y,
		  aux, action_complete);
}

void cn04_render_interface(int8_t *frame, const struct cn04_static *st,
			   int32_t level, int32_t action_count)
{
	int32_t budget = st->budget[level];
	if (budget == 0)
		return;
	int32_t diff = budget - action_count;
	int32_t steps = diff < 0 ? 0 : diff;

	int32_t width = 32;
	int32_t x_offset = (ARC_FRAME_SIZE - width) / 2;
	int32_t total = width * steps;
	int32_t whole = total / budget;
	int32_t rest = total % budget;
	int round_up =
		(2 * rest > budget) || (2 * rest == budget && (whole % 2) == 1);
	int32_t filled = whole + (round_up ? 1 : 0);
	if (filled > width)
		filled = width;
	int32_t empty = width - filled;
	for (int32_t c = 0; c < width; c++)
		frame[x_offset + c] =
			(int8_t)(c < empty ? CN04_ZEROED : CN04_GREY);
}
