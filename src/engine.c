#include "arc/engine.h"

#include <stdlib.h>
#include <string.h>

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t patch_area(const struct arc_atlas *a)
{
	return a->ph * a->pw;
}

const int8_t *arc_sprite_pixels(const struct arc_sprites *s, int32_t i)
{
	int32_t area = patch_area(s->atlas);
	if (s->overridden[i])
		return s->pixels + (size_t)i * area;
	return s->atlas->pixels + (size_t)i * area;
}

int8_t *arc_sprite_pixels_mut(struct arc_sprites *s, int32_t i)
{
	int32_t area = patch_area(s->atlas);
	int8_t *dst = s->pixels + (size_t)i * area;
	if (!s->overridden[i]) {
		memcpy(dst, s->atlas->pixels + (size_t)i * area, (size_t)area);
		s->overridden[i] = 1;
		s->bbox[i * 4 + 0] = 0;
		s->bbox[i * 4 + 1] = s->atlas->ph;
		s->bbox[i * 4 + 2] = 0;
		s->bbox[i * 4 + 3] = s->atlas->pw;
	}
	return dst;
}

void arc_sprites_recompute_bbox(struct arc_sprites *s)
{
	const struct arc_atlas *a = s->atlas;
	int32_t ph = a->ph, pw = a->pw;
	for (int32_t i = 0; i < a->num_slots; i++) {
		const int8_t *patch = arc_sprite_pixels(s, i);
		int32_t y0 = ph, y1 = 0, x0 = pw, x1 = 0;
		for (int32_t v = 0; v < ph; v++) {
			const int8_t *row = patch + (size_t)v * pw;
			for (int32_t u = 0; u < pw; u++) {
				if (row[u] >= 0) {
					if (v < y0)
						y0 = v;
					if (v + 1 > y1)
						y1 = v + 1;
					if (u < x0)
						x0 = u;
					if (u + 1 > x1)
						x1 = u + 1;
				}
			}
		}
		if (y1 <= y0 || x1 <= x0) {
			y0 = 0;
			y1 = 0;
			x0 = 0;
			x1 = 0;
		}
		s->bbox[i * 4 + 0] = y0;
		s->bbox[i * 4 + 1] = y1;
		s->bbox[i * 4 + 2] = x0;
		s->bbox[i * 4 + 3] = x1;
	}
}

int arc_sprite_visible(const struct arc_sprites *s, int32_t i)
{
	return s->alive[i] && s->interaction[i] <= INTANGIBLE;
}

int arc_sprite_collidable(const struct arc_sprites *s, int32_t i)
{
	return s->alive[i] && (s->interaction[i] % 2) == 0;
}

void arc_render_scratch_init(struct arc_render_scratch *scratch,
			     const struct arc_atlas *atlas)
{
	scratch->canvas_h = ARC_FRAME_SIZE + 2 * atlas->ph;
	scratch->canvas_w = ARC_FRAME_SIZE + 2 * atlas->pw;
	scratch->canvas = malloc((size_t)scratch->canvas_h * scratch->canvas_w);
	scratch->canvas_keys = NULL;
	scratch->sorted = malloc(sizeof(int32_t) * (size_t)atlas->num_slots);
}

void arc_render_scratch_free(struct arc_render_scratch *scratch)
{
	free(scratch->canvas);
	free(scratch->sorted);
	scratch->canvas = NULL;
	scratch->sorted = NULL;
}

static inline int32_t sort_key_ascending(const struct arc_sprites *s, int32_t i)
{
	return s->layer[i] * ARC_ORDER_BITS + s->order[i];
}

static inline int32_t sort_key_descending(const struct arc_sprites *s,
					  int32_t i)
{
	return s->layer[i] * ARC_ORDER_BITS +
	       (ARC_ORDER_BITS - 1 - s->order[i]);
}

void arc_raw_render(const struct arc_sprites *s, const struct arc_camera *cam,
		    struct arc_render_scratch *scratch, int8_t *view)
{
	const struct arc_atlas *atlas = s->atlas;
	int32_t n = atlas->num_slots, ph = atlas->ph, pw = atlas->pw;
	int32_t ch = scratch->canvas_h, cw = scratch->canvas_w;
	int8_t *canvas = scratch->canvas;
	memset(canvas, cam->background, (size_t)ch * cw);

	int32_t *order = scratch->sorted;
	int32_t count = 0;
	for (int32_t i = 0; i < n; i++)
		if (arc_sprite_visible(s, i))
			order[count++] = i;

	for (int32_t a = 1; a < count; a++) {
		int32_t v = order[a];
		int32_t key = sort_key_ascending(s, v);
		int32_t b = a - 1;
		while (b >= 0 && sort_key_ascending(s, order[b]) > key) {
			order[b + 1] = order[b];
			b--;
		}
		order[b + 1] = v;
	}

	for (int32_t a = 0; a < count; a++) {
		int32_t i = order[a];
		const int8_t *patch = arc_sprite_pixels(s, i);
		int32_t sy = clamp(s->y[i] - cam->y + ph, 0, ch - ph);
		int32_t sx = clamp(s->x[i] - cam->x + pw, 0, cw - pw);
		int32_t y0 = s->bbox[i * 4 + 0], y1 = s->bbox[i * 4 + 1];
		int32_t x0 = s->bbox[i * 4 + 2], x1 = s->bbox[i * 4 + 3];
		for (int32_t v = y0; v < y1; v++) {
			const int8_t *src = patch + (size_t)v * pw;
			int8_t *dst = canvas + (size_t)(sy + v) * cw + sx;
			for (int32_t u = x0; u < x1; u++) {
				int8_t m = (int8_t)(src[u] >> 7);
				dst[u] = (int8_t)((dst[u] & m) | (src[u] & ~m));
			}
		}
	}

	for (int32_t r = 0; r < ARC_FRAME_SIZE; r++)
		memcpy(view + (size_t)r * ARC_FRAME_SIZE,
		       canvas + (size_t)(ph + r) * cw + pw, ARC_FRAME_SIZE);
}

void arc_scale_and_offset(const struct arc_camera *cam, int32_t *scale,
			  int32_t *x_offset, int32_t *y_offset)
{
	int32_t sx = ARC_FRAME_SIZE / cam->width;
	int32_t sy = ARC_FRAME_SIZE / cam->height;
	int32_t sc = sx < sy ? sx : sy;
	*scale = sc;
	*x_offset = (ARC_FRAME_SIZE - cam->width * sc) / 2;
	*y_offset = (ARC_FRAME_SIZE - cam->height * sc) / 2;
}

void arc_render(const struct arc_sprites *s, const struct arc_camera *cam,
		struct arc_render_scratch *scratch, int8_t *frame)
{
	int8_t view[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	arc_raw_render(s, cam, scratch, view);

	int32_t scale, x_offset, y_offset;
	arc_scale_and_offset(cam, &scale, &x_offset, &y_offset);

	for (int32_t r = 0; r < ARC_FRAME_SIZE; r++) {
		int32_t gy = (r - y_offset) / scale;
		int inside_row = (r >= y_offset) && (gy < cam->height);
		for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
			int32_t gx = (c - x_offset) / scale;
			int inside = inside_row && (c >= x_offset) &&
				     (gx < cam->width);
			frame[(size_t)r * ARC_FRAME_SIZE + c] =
				inside ?
					view[(size_t)clamp(gy, 0,
							   ARC_FRAME_SIZE - 1) *
						     ARC_FRAME_SIZE +
					     clamp(gx, 0, ARC_FRAME_SIZE - 1)] :
					cam->letter_box;
		}
	}
}

static inline int contains(const struct arc_sprites *s, int32_t i, int32_t x,
			   int32_t y)
{
	return x >= s->x[i] && y >= s->y[i] && x < s->x[i] + s->w[i] &&
	       y < s->y[i] + s->h[i];
}

static inline int8_t pixel_at(const struct arc_sprites *s, int32_t i, int32_t x,
			      int32_t y)
{
	const struct arc_atlas *a = s->atlas;
	int32_t py = clamp(y - s->y[i], 0, a->ph - 1);
	int32_t px = clamp(x - s->x[i], 0, a->pw - 1);
	return arc_sprite_pixels(s, i)[(size_t)py * a->pw + px];
}

int32_t arc_get_sprite_at(const struct arc_sprites *s, int32_t x, int32_t y,
			  int32_t tag, int ignore_collidable)
{
	const struct arc_atlas *a = s->atlas;
	int32_t best = -1, best_key = 0;
	int found = 0;
	for (int32_t i = 0; i < a->num_slots; i++) {
		if (!s->alive[i] || !contains(s, i, x, y))
			continue;
		if (!ignore_collidable && !arc_sprite_collidable(s, i))
			continue;
		if (s->blocking[i] == PIXEL_PERFECT &&
		    pixel_at(s, i, x, y) == -1)
			continue;
		if (tag >= 0 && !s->tags[(size_t)i * a->num_tags + tag])
			continue;
		int32_t key = sort_key_descending(s, i);
		if (!found || key > best_key) {
			found = 1;
			best_key = key;
			best = i;
		}
	}
	return found ? best : -1;
}

static int pixel_overlap(const struct arc_sprites *s, int32_t i, int32_t j)
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

int arc_collides_pair(const struct arc_sprites *s, int32_t i, int32_t j,
		      int ignore_mode)
{
	const struct arc_atlas *a = s->atlas;
	(void)a;
	if (j == i || !s->alive[j] || !s->alive[i])
		return 0;
	if (!(s->x[i] < s->x[j] + s->w[j] && s->x[i] + s->w[i] > s->x[j] &&
	      s->y[i] < s->y[j] + s->h[j] && s->y[i] + s->h[i] > s->y[j]))
		return 0;
	if (!ignore_mode) {
		if (!arc_sprite_collidable(s, j) ||
		    !arc_sprite_collidable(s, i))
			return 0;
		if (s->blocking[j] == NOT_BLOCKED ||
		    s->blocking[i] == NOT_BLOCKED)
			return 0;
	}
	if ((s->blocking[j] == PIXEL_PERFECT ||
	     s->blocking[i] == PIXEL_PERFECT) &&
	    !pixel_overlap(s, i, j))
		return 0;
	return 1;
}

int arc_collides(const struct arc_sprites *s, int32_t i, int ignore_mode)
{
	const struct arc_atlas *a = s->atlas;
	int32_t xi = s->x[i], yi = s->y[i], wi = s->w[i], hi = s->h[i];
	for (int32_t j = 0; j < a->num_slots; j++) {
		if (j == i || !s->alive[j])
			continue;
		if (!(xi < s->x[j] + s->w[j] && xi + wi > s->x[j] &&
		      yi < s->y[j] + s->h[j] && yi + hi > s->y[j]))
			continue;
		if (!ignore_mode) {
			if (!arc_sprite_collidable(s, j) ||
			    !arc_sprite_collidable(s, i))
				continue;
			if (s->blocking[j] == NOT_BLOCKED ||
			    s->blocking[i] == NOT_BLOCKED)
				continue;
		}
		int pp = s->blocking[j] == PIXEL_PERFECT ||
			 s->blocking[i] == PIXEL_PERFECT;
		if (pp && !pixel_overlap(s, i, j))
			continue;
		return 1;
	}
	return 0;
}

void arc_set_position(struct arc_sprites *s, int32_t i, int32_t x, int32_t y)
{
	s->x[i] = x;
	s->y[i] = y;
}

void arc_move_sprite(struct arc_sprites *s, int32_t i, int32_t dx, int32_t dy)
{
	s->x[i] += dx;
	s->y[i] += dy;
}

int arc_try_move(struct arc_sprites *s, int32_t i, int32_t dx, int32_t dy)
{
	int32_t ox = s->x[i], oy = s->y[i];
	s->x[i] = ox + dx;
	s->y[i] = oy + dy;
	if (arc_collides(s, i, 0)) {
		s->x[i] = ox;
		s->y[i] = oy;
		return 1;
	}
	return 0;
}

void arc_set_interaction(struct arc_sprites *s, int32_t i, int8_t mode)
{
	s->interaction[i] = mode;
}

void arc_set_visible(struct arc_sprites *s, int32_t i, int visible)
{
	int collidable = arc_sprite_collidable(s, i);
	s->interaction[i] = visible ? (collidable ? TANGIBLE : INTANGIBLE) :
				      (collidable ? INVISIBLE : REMOVED);
}

void arc_color_remap(struct arc_sprites *s, int32_t i, int has_old, int8_t old,
		     int8_t neu)
{
	int32_t area = patch_area(s->atlas);
	int8_t *patch = arc_sprite_pixels_mut(s, i);
	for (int32_t k = 0; k < area; k++) {
		int hit = has_old ? (patch[k] == old) : (patch[k] >= 0);
		if (hit)
			patch[k] = neu;
	}
}

void arc_remove_sprite(struct arc_sprites *s, int32_t i)
{
	s->alive[i] = 0;
}

void arc_add_sprite(struct arc_sprites *s, int32_t i, int32_t order)
{
	s->alive[i] = 1;
	s->order[i] = order;
}
