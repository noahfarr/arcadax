#include "s5i5.h"
#include <stdlib.h>
#include <string.h>

#define S5I5_PITCH 3
#define S5I5_CAP_MARKER 3
#define S5I5_BAR_FILLED 3
#define S5I5_BAR_EMPTY 4
#define S5I5_ACTION6 6
#define S5I5_STATE_WIN 2
#define S5I5_STATE_GAME_OVER 3

void s5i5_aux_alloc(struct s5i5_aux *aux, int32_t num_slots, int32_t ph,
		    int32_t pw)
{
	aux->steps = 0;
	aux->pending = 0;
	aux->backup_valid = calloc((size_t)num_slots, 1);
	aux->backup_x = calloc((size_t)num_slots, sizeof(int32_t));
	aux->backup_y = calloc((size_t)num_slots, sizeof(int32_t));
	aux->backup_h = calloc((size_t)num_slots, sizeof(int32_t));
	aux->backup_w = calloc((size_t)num_slots, sizeof(int32_t));
	aux->backup_pixels =
		calloc((size_t)num_slots * (size_t)ph * (size_t)pw, 1);
}

void s5i5_aux_free(struct s5i5_aux *aux)
{
	free(aux->backup_valid);
	free(aux->backup_x);
	free(aux->backup_y);
	free(aux->backup_h);
	free(aux->backup_w);
	free(aux->backup_pixels);
	aux->backup_valid = NULL;
	aux->backup_x = aux->backup_y = aux->backup_h = aux->backup_w = NULL;
	aux->backup_pixels = NULL;
}

void s5i5_zero_aux(struct s5i5_aux *aux, const struct s5i5_static *st)
{
	int32_t n = st->num_slots;
	aux->steps = 0;
	aux->pending = 0;
	memset(aux->backup_valid, 0, (size_t)n);
	memset(aux->backup_x, 0, (size_t)n * sizeof(int32_t));
	memset(aux->backup_y, 0, (size_t)n * sizeof(int32_t));
	memset(aux->backup_h, 0, (size_t)n * sizeof(int32_t));
	memset(aux->backup_w, 0, (size_t)n * sizeof(int32_t));
	memset(aux->backup_pixels, 0,
	       (size_t)n * (size_t)st->ph * (size_t)st->pw);
}

void s5i5_on_set_level(struct s5i5_aux *aux, const struct s5i5_static *st,
		       int32_t level)
{
	s5i5_zero_aux(aux, st);
	aux->steps = st->budget[level];
}

static int8_t *sprite_pixels_mut_(struct arc_sprites *s, int32_t i)
{
	const struct arc_atlas *a = s->atlas;
	int32_t area = a->ph * a->pw;
	int8_t *dst = s->pixels + (size_t)i * (size_t)area;
	if (!s->overridden[i]) {
		memcpy(dst, a->pixels + (size_t)i * (size_t)area, (size_t)area);
		s->overridden[i] = 1;
		s->bbox[i * 4 + 0] = 0;
		s->bbox[i * 4 + 1] = a->ph;
		s->bbox[i * 4 + 2] = 0;
		s->bbox[i * 4 + 3] = a->pw;
	}
	return dst;
}

static int32_t rotation_of(const struct arc_sprites *s, int32_t slot)
{
	const struct arc_atlas *a = s->atlas;
	const int8_t *p = arc_sprite_pixels(s, slot);
	int32_t h = s->h[slot];
	int8_t bottom = p[(size_t)(h - 1) * a->pw + 1];
	int8_t left = p[(size_t)1 * a->pw + 0];
	int8_t top = p[(size_t)0 * a->pw + 1];
	if (bottom == S5I5_CAP_MARKER)
		return 0;
	if (left == S5I5_CAP_MARKER)
		return 90;
	if (top == S5I5_CAP_MARKER)
		return 180;
	return 270;
}

static void rot90(const int8_t *src, int8_t *dst, int32_t ph, int32_t pw,
		  int32_t h, int32_t w)
{
	for (int32_t r = 0; r < ph; r++) {
		int32_t src_col = w - 1 - r;
		if (src_col < 0)
			src_col = 0;
		if (src_col > pw - 1)
			src_col = pw - 1;
		for (int32_t c = 0; c < pw; c++) {
			int32_t src_row = c > ph - 1 ? ph - 1 : c;
			int valid = (r < w) && (c < h);
			dst[(size_t)r * pw + c] =
				valid ? src[(size_t)src_row * pw + src_col] :
					(int8_t)-1;
		}
	}
}

static void rotate_one(struct arc_sprites *s, int32_t ph, int32_t pw,
		       int32_t anchor_x, int32_t anchor_y, int32_t k)
{
	int32_t hk = s->h[k], wk = s->w[k];
	int32_t xk = s->x[k], yk = s->y[k];
	int32_t dx = anchor_x - xk;
	int32_t dy = anchor_y - yk;
	int32_t new_x = anchor_x - dy;
	int32_t new_y = anchor_y + dx - (wk - S5I5_PITCH);
	int8_t rotated[ph * pw > 0 ? (size_t)ph * pw : 1];
	rot90(arc_sprite_pixels(s, k), rotated, ph, pw, hk, wk);
	arc_set_position(s, k, new_x, new_y);
	s->h[k] = wk;
	s->w[k] = hk;
	int8_t *dst = sprite_pixels_mut_(s, k);
	memcpy(dst, rotated, (size_t)ph * (size_t)pw);
}

static void rotate_closure(struct arc_sprites *s, const struct s5i5_static *st,
			   int32_t level, int32_t root)
{
	const struct arc_atlas *a = s->atlas;
	int32_t ph = a->ph, pw = a->pw;
	int32_t h0 = s->h[root], w0 = s->w[root];
	int32_t x0 = s->x[root], y0 = s->y[root];
	int32_t rot = rotation_of(s, root);
	int32_t anchor_x = rot == 270 ? x0 + w0 - S5I5_PITCH : x0;
	int32_t anchor_y = rot == 0 ? y0 + h0 - S5I5_PITCH : y0;

	rotate_one(s, ph, pw, anchor_x, anchor_y, root);

	int32_t base = level * st->num_slots + root;
	int32_t off = st->desc_offset[base],
		cnt = st->desc_offset[base + 1] - off;
	for (int32_t k = 0; k < cnt; k++)
		rotate_one(s, ph, pw, anchor_x, anchor_y,
			   st->desc_flat[off + k]);
}

static void resize_one(struct arc_sprites *s, const struct s5i5_static *st,
		       int32_t level, int32_t root, int32_t new_len)
{
	const struct arc_atlas *a = s->atlas;
	int32_t ph = a->ph, pw = a->pw;
	int32_t h0 = s->h[root], w0 = s->w[root];
	int32_t colour = st->pipe_color[level * st->num_slots + root];
	int32_t rot = rotation_of(s, root);
	int is0 = rot == 0, is90 = rot == 90, is180 = rot == 180;
	int32_t new_h = (is0 || is180) ? new_len : S5I5_PITCH;
	int32_t new_w = (is0 || is180) ? S5I5_PITCH : new_len;

	int8_t patch[ph * pw > 0 ? (size_t)ph * pw : 1];
	for (int32_t r = 0; r < ph; r++) {
		for (int32_t c = 0; c < pw; c++) {
			int body = (r < new_h) && (c < new_w);
			int marker;
			if (is0)
				marker = (r == new_len - 1) && (c < S5I5_PITCH);
			else if (is90)
				marker = (c == 0) && (r < S5I5_PITCH);
			else if (is180)
				marker = (r == 0) && (c < S5I5_PITCH);
			else
				marker = (c == new_len - 1) && (r < S5I5_PITCH);
			patch[(size_t)r * pw + c] =
				marker ? (int8_t)S5I5_CAP_MARKER :
				body   ? (int8_t)colour :
					 (int8_t)-1;
		}
	}

	int32_t dx0 = is90 ? new_len - w0 : (rot == 270 ? -(new_len - w0) : 0);
	int32_t dy0 = is0 ? -(new_len - h0) : (is180 ? new_len - h0 : 0);
	int32_t root_dx = rot == 270 ? dx0 : 0;
	int32_t root_dy = rot == 0 ? dy0 : 0;

	arc_move_sprite(s, root, root_dx, root_dy);
	s->h[root] = new_h;
	s->w[root] = new_w;
	int8_t *dst = sprite_pixels_mut_(s, root);
	memcpy(dst, patch, (size_t)ph * (size_t)pw);

	int32_t base = level * st->num_slots + root;
	int32_t off = st->desc_offset[base],
		cnt = st->desc_offset[base + 1] - off;
	for (int32_t k = 0; k < cnt; k++)
		arc_move_sprite(s, st->desc_flat[off + k], dx0, dy0);
}

static void backup_one(struct arc_sprites *s, struct s5i5_aux *aux, int32_t k)
{
	const struct arc_atlas *a = s->atlas;
	int32_t area = a->ph * a->pw;
	aux->backup_valid[k] = 1;
	aux->backup_x[k] = s->x[k];
	aux->backup_y[k] = s->y[k];
	aux->backup_h[k] = s->h[k];
	aux->backup_w[k] = s->w[k];
	memcpy(aux->backup_pixels + (size_t)k * (size_t)area,
	       arc_sprite_pixels(s, k), (size_t)area);
}

static void backup_closure(struct arc_sprites *s, struct s5i5_aux *aux,
			   const struct s5i5_static *st, int32_t level,
			   int32_t root)
{
	backup_one(s, aux, root);
	int32_t base = level * st->num_slots + root;
	int32_t off = st->desc_offset[base],
		cnt = st->desc_offset[base + 1] - off;
	for (int32_t k = 0; k < cnt; k++)
		backup_one(s, aux, st->desc_flat[off + k]);
}

static void do_revert(struct arc_sprites *s, struct s5i5_aux *aux)
{
	int32_t n = s->atlas->num_slots;
	int32_t area = s->atlas->ph * s->atlas->pw;
	for (int32_t k = 0; k < n; k++) {
		if (!aux->backup_valid[k])
			continue;
		arc_set_position(s, k, aux->backup_x[k], aux->backup_y[k]);
		s->h[k] = aux->backup_h[k];
		s->w[k] = aux->backup_w[k];
		int8_t *dst = sprite_pixels_mut_(s, k);
		memcpy(dst, aux->backup_pixels + (size_t)k * (size_t)area,
		       (size_t)area);
	}
	aux->pending = 0;
	memset(aux->backup_valid, 0, (size_t)n);
}

static int pixel_overlap_(const struct arc_sprites *s, int32_t i, int32_t j)
{
	const struct arc_atlas *a = s->atlas;
	int32_t ph = a->ph, pw = a->pw;
	int32_t dy = s->y[j] - s->y[i];
	int32_t dx = s->x[j] - s->x[i];
	const int8_t *pi = arc_sprite_pixels(s, i);
	const int8_t *pj = arc_sprite_pixels(s, j);
	for (int32_t v = 0; v < ph; v++) {
		int32_t vv = v - dy;
		if (vv < 0 || vv >= ph)
			continue;
		for (int32_t u = 0; u < pw; u++) {
			int32_t uu = u - dx;
			if (uu < 0 || uu >= pw)
				continue;
			if (pi[(size_t)v * pw + u] != -1 &&
			    pj[(size_t)vv * pw + uu] != -1)
				return 1;
		}
	}
	return 0;
}

static int pair_collides(const struct arc_sprites *s, int32_t i, int32_t j)
{
	if (!s->alive[j])
		return 0;
	if (!(s->x[i] < s->x[j] + s->w[j] && s->x[i] + s->w[i] > s->x[j] &&
	      s->y[i] < s->y[j] + s->h[j] && s->y[i] + s->h[i] > s->y[j]))
		return 0;
	if (!arc_sprite_collidable(s, i) || !arc_sprite_collidable(s, j))
		return 0;
	if (s->blocking[i] == NOT_BLOCKED || s->blocking[j] == NOT_BLOCKED)
		return 0;
	int pp = s->blocking[i] == PIXEL_PERFECT ||
		 s->blocking[j] == PIXEL_PERFECT;
	if (pp && !pixel_overlap_(s, i, j))
		return 0;
	return 1;
}

static int any_pipe_collision(const struct arc_sprites *s,
			      const struct s5i5_static *st, int32_t level)
{
	int32_t off = st->pipe_offset[level],
		cnt = st->pipe_offset[level + 1] - off;
	for (int32_t a = 0; a < cnt; a++) {
		int32_t i = st->pipe_flat[off + a];
		for (int32_t b = 0; b < cnt; b++) {
			int32_t j = st->pipe_flat[off + b];
			if (i == j)
				continue;
			if (pair_collides(s, i, j))
				return 1;
		}
	}
	return 0;
}

static int check_win(const struct arc_sprites *s, const struct s5i5_static *st,
		     int32_t level)
{
	int32_t toff = st->target_offset[level],
		tcnt = st->target_offset[level + 1] - toff;
	int32_t coff = st->conn_offset[level],
		ccnt = st->conn_offset[level + 1] - coff;
	for (int32_t a = 0; a < tcnt; a++) {
		int32_t t = st->target_flat[toff + a];
		if (!s->alive[t])
			continue;
		int ok = 0;
		for (int32_t b = 0; b < ccnt; b++) {
			int32_t c = st->conn_flat[coff + b];
			if (!s->alive[c])
				continue;
			if (s->x[c] == s->x[t] && s->y[c] == s->y[t]) {
				ok = 1;
				break;
			}
		}
		if (!ok)
			return 0;
	}
	return 1;
}

static void engine_complete(struct s5i5_engine *e)
{
	e->action_complete = 1;
}

static void engine_next_level(struct s5i5_engine *e, int32_t num_levels)
{
	e->score += 1;
	int is_last = e->level_index == num_levels - 1;
	e->next_level = is_last ? 0 : 1;
	if (is_last)
		e->status = S5I5_STATE_WIN;
}

static void engine_lose(struct s5i5_engine *e)
{
	e->status = S5I5_STATE_GAME_OVER;
}

static void finish(struct arc_sprites *s, struct s5i5_engine *e,
		   struct s5i5_aux *aux, const struct s5i5_static *st)
{
	if (check_win(s, st, e->level_index)) {
		engine_next_level(e, st->num_levels);
	} else if (aux->steps == 0) {
		engine_lose(e);
	}
	engine_complete(e);
}

static void resolve_move(struct arc_sprites *s, struct s5i5_engine *e,
			 struct s5i5_aux *aux, const struct s5i5_static *st,
			 int32_t level)
{
	if (any_pipe_collision(s, st, level)) {
		aux->pending = 1;
	} else {
		finish(s, e, aux, st);
	}
}

static void process_root_rotation(struct arc_sprites *s,
				  const struct s5i5_static *st, int32_t level,
				  int32_t root)
{
	int32_t parent = st->parent_slot[level * st->num_slots + root];
	if (parent >= 0) {
		int32_t rot_parent = rotation_of(s, parent);
		int32_t rot_self = rotation_of(s, root);
		int32_t diff = rot_self - 90 - rot_parent;
		if (diff < 0)
			diff = -diff;
		if (diff == 180)
			rotate_closure(s, st, level, root);
	}
	rotate_closure(s, st, level, root);
}

static void process_rotate(struct arc_sprites *s, struct s5i5_aux *aux,
			   const struct s5i5_static *st, int32_t level,
			   int32_t handle)
{
	int32_t clicked_color =
		st->handle_color[level * st->num_slots + handle];
	memset(aux->backup_valid, 0, (size_t)st->num_slots);
	int32_t lbase = level * st->num_slots;
	int32_t off = st->pipe_offset[level],
		cnt = st->pipe_offset[level + 1] - off;
	for (int32_t idx = 0; idx < cnt; idx++) {
		int32_t root = st->pipe_flat[off + idx];
		if (st->pipe_color[lbase + root] != clicked_color)
			continue;
		backup_closure(s, aux, st, level, root);
		process_root_rotation(s, st, level, root);
	}
}

static void display_to_grid(const struct arc_camera *cam, int32_t display_x,
			    int32_t display_y, int32_t *grid_x, int32_t *grid_y,
			    int *valid)
{
	int32_t scale, x_pad, y_pad;
	arc_scale_and_offset(cam, &scale, &x_pad, &y_pad);
	int32_t dx = display_x - x_pad;
	int32_t dy = display_y - y_pad;
	int32_t gx = dx >= 0 ? dx / scale : -1;
	int32_t gy = dy >= 0 ? dy / scale : -1;
	*valid = gx >= 0 && gy >= 0 && gx < cam->width && gy < cam->height;
	*grid_x = gx + cam->x;
	*grid_y = gy + cam->y;
}

static void process_resize(struct arc_sprites *s, struct s5i5_aux *aux,
			   const struct s5i5_static *st,
			   const struct arc_camera *cam, int32_t level,
			   int32_t slider, int32_t action_x, int32_t action_y)
{
	int32_t world_x, world_y, valid;
	display_to_grid(cam, action_x, action_y, &world_x, &world_y, &valid);
	int32_t sw = s->w[slider], sh = s->h[slider];
	int horizontal = sw > sh;
	int32_t mid = horizontal ? sw / 2 : sh / 2;
	int32_t offset =
		horizontal ? world_x - s->x[slider] : world_y - s->y[slider];
	memset(aux->backup_valid, 0, (size_t)st->num_slots);

	int32_t base = level * st->num_slots + slider;
	int32_t off = st->slider_pipe_offset[base],
		cnt = st->slider_pipe_offset[base + 1] - off;
	for (int32_t k = 0; k < cnt; k++) {
		int32_t j = st->slider_pipe_flat[off + k];
		backup_closure(s, aux, st, level, j);
		int32_t h = s->h[j], w = s->w[j];
		int32_t index = h > w ? h / S5I5_PITCH : w / S5I5_PITCH;
		int grow = offset > mid;
		int shrink = offset < mid && index > 1;
		int32_t new_len = grow	 ? (index + 1) * S5I5_PITCH :
				  shrink ? (index - 1) * S5I5_PITCH :
					   index * S5I5_PITCH;
		if (grow || shrink)
			resize_one(s, st, level, j, new_len);
	}
}

static void do_click(struct arc_sprites *s, struct s5i5_engine *e,
		     struct s5i5_aux *aux, const struct s5i5_static *st,
		     const struct arc_camera *cam)
{
	aux->steps = aux->steps > 0 ? aux->steps - 1 : 0;
	int32_t world_x, world_y, on_board;
	display_to_grid(cam, e->action_x, e->action_y, &world_x, &world_y,
			&on_board);
	if (!on_board) {
		finish(s, e, aux, st);
		return;
	}
	int32_t level = e->level_index;
	int32_t handle =
		arc_get_sprite_at(s, world_x, world_y, st->handle_tag, 0);
	if (handle >= 0) {
		process_rotate(s, aux, st, level, handle);
		resolve_move(s, e, aux, st, level);
		return;
	}
	int32_t slider =
		arc_get_sprite_at(s, world_x, world_y, st->slider_tag, 0);
	if (slider >= 0) {
		process_resize(s, aux, st, cam, level, slider, e->action_x,
			       e->action_y);
		resolve_move(s, e, aux, st, level);
		return;
	}
	finish(s, e, aux, st);
}

void s5i5_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera, struct s5i5_engine *engine,
		    struct s5i5_aux *aux, const struct s5i5_static *st)
{
	if (aux->pending) {
		do_revert(sprites, aux);
		finish(sprites, engine, aux, st);
		return;
	}
	if (engine->action_id == S5I5_ACTION6) {
		do_click(sprites, engine, aux, st, camera);
	} else {
		finish(sprites, engine, aux, st);
	}
}

void s5i5_render_interface(int8_t *frame, const struct s5i5_aux *aux,
			   const struct s5i5_static *st, int32_t level)
{
	int32_t budget = st->budget[level];
	if (budget == 0)
		return;
	int32_t total = ARC_FRAME_SIZE * aux->steps;
	int32_t whole = total / budget;
	int32_t rest = total % budget;
	int32_t filled = whole;
	if (2 * rest > budget)
		filled += 1;
	else if (2 * rest == budget && (whole % 2) == 1)
		filled += 1;
	int8_t *row = frame + (size_t)(ARC_FRAME_SIZE - 1) * ARC_FRAME_SIZE;
	for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
		row[c] = c < filled ? (int8_t)S5I5_BAR_FILLED :
				      (int8_t)S5I5_BAR_EMPTY;
}
