#include "m0r0.h"

#include <string.h>

enum {
	M0R0_ACTION1 = 1,
	M0R0_ACTION2 = 2,
	M0R0_ACTION3 = 3,
	M0R0_ACTION4 = 4,
	M0R0_ACTION6 = 6
};
enum { M0R0_WIN = 2, M0R0_GAME_OVER = 3 };
enum { M0R0_MAX_ACTIONS = 150 };
enum { M0R0_BLOCK_IDLE = 9, M0R0_BLOCK_SELECTED = 11 };
enum { M0R0_MOVER_IDLE = 10, M0R0_MOVER_BLOCKED = 1 };
enum { M0R0_FLASH_ON = 11, M0R0_FLASH_OFF = 10, M0R0_FLASH_FRAMES = 7 };
enum { M0R0_HUD_COLOUR = 5, M0R0_HUD_EMPTY = 0, M0R0_OVERLAY_COLOUR = 5 };

static const int32_t M0R0_SIGN_X[M0R0_NUM_MOVERS] = { 1, -1, 1, -1 };
static const int32_t M0R0_SIGN_Y[M0R0_NUM_MOVERS] = { 1, 1, -1, -1 };

static inline int32_t m0r0_clamp(int32_t v, int32_t lo, int32_t hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static int m0r0_pixel_overlap(const struct arc_sprites *s, int32_t i, int32_t j)
{
	int32_t ph = s->atlas->ph, pw = s->atlas->pw;
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
			if (pi[v * pw + u] != -1 && pj[vv * pw + uu] != -1)
				return 1;
		}
	}
	return 0;
}

static int m0r0_obstacle_hit(const struct arc_sprites *s, int32_t i, int32_t j)
{
	if (!s->alive[j])
		return 0;
	int32_t xi = s->x[i], yi = s->y[i], wi = s->w[i], hi = s->h[i];
	if (!(xi < s->x[j] + s->w[j] && xi + wi > s->x[j] &&
	      yi < s->y[j] + s->h[j] && yi + hi > s->y[j]))
		return 0;
	if (!arc_sprite_collidable(s, i) || !arc_sprite_collidable(s, j))
		return 0;
	if (s->blocking[i] == NOT_BLOCKED || s->blocking[j] == NOT_BLOCKED)
		return 0;
	if (s->blocking[i] == PIXEL_PERFECT || s->blocking[j] == PIXEL_PERFECT)
		return m0r0_pixel_overlap(s, i, j);
	return 1;
}

static void m0r0_display_to_grid(const struct arc_camera *camera,
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

static void m0r0_push_marker(struct arc_sprites *s,
			     const struct arc_camera *camera,
			     const struct m0r0_static *st, int32_t level,
			     int32_t lane, int32_t dx, int32_t dy)
{
	int32_t slot = st->mover_slot[(size_t)level * M0R0_NUM_MOVERS + lane];
	int32_t tx = s->x[slot] + dx, ty = s->y[slot] + dy;
	if (tx < 0 || tx >= camera->width || ty < 0 || ty >= camera->height)
		return;
	arc_move_sprite(s, slot, dx, dy);
	int32_t count = st->obstacle_count[level];
	const int32_t *obstacles =
		st->obstacle_slots + (size_t)level * st->max_obstacles;
	for (int32_t k = 0; k < count; k++) {
		if (m0r0_obstacle_hit(s, slot, obstacles[k])) {
			arc_move_sprite(s, slot, -dx, -dy);
			return;
		}
	}
}

static void m0r0_update_doors(struct arc_sprites *s,
			      const struct m0r0_static *st, int32_t level,
			      const struct m0r0_aux *aux)
{
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	int32_t mover_x[M0R0_NUM_MOVERS], mover_y[M0R0_NUM_MOVERS];
	uint8_t active[M0R0_NUM_MOVERS];
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		int32_t slot = slots[lane];
		active[lane] = slot >= 0 && !aux->locked[lane];
		mover_x[lane] = slot >= 0 ? s->x[slot] : 0;
		mover_y[lane] = slot >= 0 ? s->y[slot] : 0;
	}
	for (int32_t colour = 0; colour < 3; colour++) {
		int32_t idx = level * 3 + colour;
		int32_t sw_count = st->switch_count[idx];
		int32_t dr_count = st->door_count[idx];
		if (sw_count == 0 || dr_count == 0)
			continue;
		const int32_t *switches =
			st->switch_slots + (size_t)idx * st->max_switches;
		const int32_t *doors =
			st->door_slots + (size_t)idx * st->max_doors;
		int pressed = 0;
		for (int32_t k = 0; k < sw_count && !pressed; k++) {
			int32_t sw = switches[k];
			for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
				if (active[lane] && s->x[sw] == mover_x[lane] &&
				    s->y[sw] == mover_y[lane]) {
					pressed = 1;
					break;
				}
			}
		}
		int8_t mode = pressed ? REMOVED : TANGIBLE;
		for (int32_t k = 0; k < dr_count; k++)
			arc_set_interaction(s, doors[k], mode);
	}
}

static void m0r0_unswap(struct arc_sprites *s, const struct m0r0_static *st,
			int32_t level, const uint8_t active[M0R0_NUM_MOVERS],
			const struct m0r0_aux *aux)
{
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	for (int32_t i = 0; i < M0R0_NUM_MOVERS; i++) {
		int32_t si = slots[i];
		if (si < 0)
			continue;
		for (int32_t j = i + 1; j < M0R0_NUM_MOVERS; j++) {
			int32_t sj = slots[j];
			if (sj < 0)
				continue;
			int32_t xi = s->x[si], yi = s->y[si], xj = s->x[sj],
				yj = s->y[sj];
			int adjacent = (aux->prev_x[i] - aux->prev_x[j] == 1 ||
					aux->prev_x[j] - aux->prev_x[i] == 1) &&
				       aux->prev_y[i] == aux->prev_y[j];
			int swapped =
				(xi == aux->prev_x[j] &&
				 yi == aux->prev_y[j]) ||
				(xj == aux->prev_x[i] && yj == aux->prev_y[i]);
			int hit = active[i] && active[j] && aux->has_prev[i] &&
				  aux->has_prev[j] && adjacent && swapped;
			if (!hit)
				continue;
			int32_t mid_x = (xi + xj) / 2, mid_y = (yi + yj) / 2;
			arc_set_position(s, si, mid_x, mid_y);
			arc_set_position(s, sj, mid_x, mid_y);
		}
	}
}

static void m0r0_merge(struct arc_sprites *s, const struct m0r0_static *st,
		       int32_t level, const uint8_t active[M0R0_NUM_MOVERS],
		       struct m0r0_aux *aux)
{
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	int32_t x[M0R0_NUM_MOVERS], y[M0R0_NUM_MOVERS];
	for (int32_t i = 0; i < M0R0_NUM_MOVERS; i++) {
		int32_t slot = slots[i];
		x[i] = slot >= 0 ? s->x[slot] : 0;
		y[i] = slot >= 0 ? s->y[slot] : 0;
	}
	uint8_t together[M0R0_NUM_MOVERS][M0R0_NUM_MOVERS];
	int32_t size[M0R0_NUM_MOVERS];
	for (int32_t i = 0; i < M0R0_NUM_MOVERS; i++) {
		size[i] = 0;
		for (int32_t j = 0; j < M0R0_NUM_MOVERS; j++) {
			int32_t same = active[i] && active[j] && x[i] == x[j] &&
				       y[i] == y[j];
			together[i][j] = (uint8_t)same;
			size[i] += same;
		}
	}
	for (int32_t i = 0; i < M0R0_NUM_MOVERS; i++) {
		int32_t rank = 0;
		for (int32_t j = 0; j < i; j++)
			rank += together[i][j];
		int merged = size[i] >= 2 && rank < 2;
		int crowded = size[i] > 2 && rank >= 2 && aux->has_prev[i];
		if (merged)
			aux->locked[i] = 1;
		int32_t slot = slots[i];
		if (slot < 0)
			continue;
		if (merged)
			arc_set_interaction(s, slot, INTANGIBLE);
		if (crowded)
			arc_set_position(s, slot, aux->prev_x[i],
					 aux->prev_y[i]);
	}
}

static void m0r0_next_level(const struct m0r0_static *st, int32_t level,
			    int32_t *score, int32_t *status,
			    uint8_t *next_level)
{
	int is_last = level == st->num_levels - 1;
	*score += 1;
	*next_level = (uint8_t)!is_last;
	if (is_last)
		*status = M0R0_WIN;
}

static void m0r0_settle(struct arc_sprites *s, const struct m0r0_static *st,
			int32_t level, const uint8_t active[M0R0_NUM_MOVERS],
			struct m0r0_aux *aux, int32_t *score, int32_t *status,
			uint8_t *next_level, uint8_t *action_complete)
{
	m0r0_unswap(s, st, level, active, aux);
	m0r0_merge(s, st, level, active, aux);
	m0r0_update_doors(s, st, level, aux);
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	int32_t remaining = 0;
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		int32_t slot = slots[lane];
		if (active[lane] && !aux->locked[lane] && slot >= 0 &&
		    s->interaction[slot] != INTANGIBLE)
			remaining++;
	}
	if (remaining == 0)
		m0r0_next_level(st, level, score, status, next_level);
	*action_complete = 1;
}

static void m0r0_animate_hazard(struct arc_sprites *s,
				const struct m0r0_static *st, int32_t level,
				struct m0r0_aux *aux, uint8_t *action_complete)
{
	int32_t flash = aux->flash;
	int lit = flash % 2 == 0 && flash < 5;
	int8_t colour = lit ? M0R0_FLASH_ON : M0R0_FLASH_OFF;
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		if (!aux->flashing[lane])
			continue;
		int32_t slot = slots[lane];
		if (slot >= 0)
			arc_color_remap(s, slot, 0, 0, colour);
	}
	aux->flash = flash + 1;
	if (aux->flash <= M0R0_FLASH_FRAMES - 1)
		return;

	int32_t count = st->clickable_count[level];
	const int32_t *clickable =
		st->clickable_slots + (size_t)level * st->max_clickable;
	const int32_t *clean_x = st->clean_x + (size_t)level * st->num_slots;
	const int32_t *clean_y = st->clean_y + (size_t)level * st->num_slots;
	for (int32_t k = 0; k < count; k++) {
		int32_t slot = clickable[k];
		arc_set_position(s, slot, clean_x[slot], clean_y[slot]);
	}
	aux->flash = -1;
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++)
		aux->flashing[lane] = 0;
	*action_complete = 1;
}

static void m0r0_click(struct arc_sprites *s, const struct arc_camera *camera,
		       const struct m0r0_static *st, int32_t level,
		       int32_t action_x, int32_t action_y, struct m0r0_aux *aux,
		       uint8_t *action_complete)
{
	int32_t world_x, world_y;
	int on_board;
	m0r0_display_to_grid(camera, action_x, action_y, &world_x, &world_y,
			     &on_board);
	int32_t hit = arc_get_sprite_at(s, world_x, world_y, st->click_tag, 0);

	int picked_block = 0;
	if (on_board && hit >= 0) {
		int32_t count = st->block_count[level];
		const int32_t *blocks =
			st->block_slots + (size_t)level * st->max_blocks;
		for (int32_t k = 0; k < count; k++) {
			if (blocks[k] == hit) {
				picked_block = 1;
				break;
			}
		}
	}

	if (on_board) {
		const int32_t *slots =
			st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
		int8_t colour =
			picked_block ? M0R0_MOVER_BLOCKED : M0R0_MOVER_IDLE;
		for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
			int32_t slot = slots[lane];
			if (slot >= 0 && !aux->locked[lane])
				arc_color_remap(s, slot, 0, 0, colour);
		}
		int had = aux->selected >= 0;
		if (had)
			arc_color_remap(s, aux->selected, 0, 0,
					M0R0_BLOCK_IDLE);
		if (picked_block)
			arc_color_remap(s, hit, 0, 0, M0R0_BLOCK_SELECTED);
		aux->move_markers = (uint8_t)!picked_block;
		aux->selected = picked_block ? hit : -1;
	}
	*action_complete = 1;
}

static void m0r0_move_block(struct arc_sprites *s,
			    const struct arc_camera *camera,
			    const struct m0r0_static *st, int32_t level,
			    int32_t dx, int32_t dy, struct m0r0_aux *aux,
			    uint8_t *action_complete)
{
	int32_t slot = aux->selected;
	int32_t tx = s->x[slot] + dx, ty = s->y[slot] + dy;
	if (tx >= 0 && tx < camera->width && ty >= 0 && ty < camera->height) {
		arc_try_move(s, slot, dx, dy);
		m0r0_update_doors(s, st, level, aux);
	}
	*action_complete = 1;
}

static void m0r0_move_markers(struct arc_sprites *s,
			      const struct arc_camera *camera,
			      const struct m0r0_static *st, int32_t level,
			      int32_t dx, int32_t dy, struct m0r0_aux *aux,
			      int32_t *score, int32_t *status,
			      uint8_t *next_level, uint8_t *action_complete)
{
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	uint8_t active[M0R0_NUM_MOVERS];
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		int32_t slot = slots[lane];
		active[lane] = slot >= 0 && !aux->locked[lane];
		if (active[lane]) {
			aux->prev_x[lane] = s->x[slot];
			aux->prev_y[lane] = s->y[slot];
		}
		aux->has_prev[lane] =
			(uint8_t)(aux->has_prev[lane] || active[lane]);
	}

	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		if (!active[lane])
			continue;
		int32_t sdx = M0R0_SIGN_X[lane] * dx,
			sdy = M0R0_SIGN_Y[lane] * dy;
		if (sdx != 0 || sdy != 0)
			m0r0_push_marker(s, camera, st, level, lane, sdx, sdy);
	}

	uint8_t on_hazard[M0R0_NUM_MOVERS];
	int any_hazard = 0;
	const uint8_t *hmap = st->hazard_map +
			      (size_t)level * ARC_FRAME_SIZE * ARC_FRAME_SIZE;
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		on_hazard[lane] = 0;
		if (!active[lane])
			continue;
		int32_t slot = slots[lane];
		int32_t yy = m0r0_clamp(s->y[slot], 0, ARC_FRAME_SIZE - 1);
		int32_t xx = m0r0_clamp(s->x[slot], 0, ARC_FRAME_SIZE - 1);
		on_hazard[lane] = hmap[(size_t)yy * ARC_FRAME_SIZE + xx];
		any_hazard |= on_hazard[lane];
	}

	if (any_hazard) {
		for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++)
			aux->flashing[lane] = on_hazard[lane];
		aux->flash = 0;
		return;
	}
	m0r0_settle(s, st, level, active, aux, score, status, next_level,
		    action_complete);
}

static void m0r0_press_key(struct arc_sprites *s,
			   const struct arc_camera *camera,
			   const struct m0r0_static *st, int32_t level,
			   int32_t action_id, struct m0r0_aux *aux,
			   int32_t *score, int32_t *status, uint8_t *next_level,
			   uint8_t *action_complete)
{
	int32_t dx = 0, dy = 0;
	switch (action_id) {
	case M0R0_ACTION1:
		dy = -1;
		break;
	case M0R0_ACTION2:
		dy = 1;
		break;
	case M0R0_ACTION3:
		dx = -1;
		break;
	case M0R0_ACTION4:
		dx = 1;
		break;
	default:
		break;
	}
	int driving_block = aux->selected >= 0 && !aux->move_markers;
	if (driving_block) {
		m0r0_move_block(s, camera, st, level, dx, dy, aux,
				action_complete);
	} else if (aux->move_markers) {
		m0r0_move_markers(s, camera, st, level, dx, dy, aux, score,
				  status, next_level, action_complete);
	} else {
		*action_complete = 1;
	}
}

static void m0r0_play(struct arc_sprites *s, const struct arc_camera *camera,
		      const struct m0r0_static *st, int32_t level,
		      int32_t action_id, int32_t action_x, int32_t action_y,
		      int32_t action_count, struct m0r0_aux *aux,
		      int32_t *score, int32_t *status, uint8_t *next_level,
		      uint8_t *action_complete)
{
	int32_t steps = m0r0_clamp(M0R0_MAX_ACTIONS - action_count, 0,
				   M0R0_MAX_ACTIONS);
	aux->steps = steps;
	if (action_count > M0R0_MAX_ACTIONS) {
		*status = M0R0_GAME_OVER;
		*action_complete = 1;
		return;
	}
	if (action_id == M0R0_ACTION6) {
		m0r0_click(s, camera, st, level, action_x, action_y, aux,
			   action_complete);
	} else {
		m0r0_press_key(s, camera, st, level, action_id, aux, score,
			       status, next_level, action_complete);
	}
}

void m0r0_zero_aux(struct m0r0_aux *aux)
{
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		aux->locked[lane] = 0;
		aux->prev_x[lane] = 0;
		aux->prev_y[lane] = 0;
		aux->has_prev[lane] = 0;
		aux->flashing[lane] = 0;
	}
	aux->selected = -1;
	aux->move_markers = 1;
	aux->flash = -1;
	aux->steps = M0R0_MAX_ACTIONS;
}

void m0r0_on_set_level(struct arc_sprites *sprites,
		       const struct m0r0_static *st, int32_t level,
		       struct m0r0_aux *aux)
{
	int32_t count = st->block_count[level];
	const int32_t *blocks =
		st->block_slots + (size_t)level * st->max_blocks;
	for (int32_t k = 0; k < count; k++)
		arc_color_remap(sprites, blocks[k], 0, 0, M0R0_BLOCK_IDLE);
	m0r0_zero_aux(aux);
}

void m0r0_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    const struct m0r0_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct m0r0_aux *aux, int32_t *score,
		    int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete)
{
	if (aux->flash >= 0) {
		m0r0_animate_hazard(sprites, st, level, aux, action_complete);
	} else {
		m0r0_play(sprites, camera, st, level, action_id, action_x,
			  action_y, action_count, aux, score, status,
			  next_level, action_complete);
	}
}

static void m0r0_draw_background(int8_t *frame, const struct arc_sprites *s,
				 const struct m0r0_static *st, int32_t level,
				 const struct m0r0_aux *aux)
{
	int32_t c0 = st->background[(size_t)level * 2 + 0];
	int32_t c1 = st->background[(size_t)level * 2 + 1];
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	int32_t loose = 0;
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		int32_t slot = slots[lane];
		if (slot >= 0 && !aux->locked[lane] &&
		    s->interaction[slot] != REMOVED)
			loose++;
	}
	int quad_mode = loose == M0R0_NUM_MOVERS;
	int32_t half = ARC_FRAME_SIZE / 2;
	for (int32_t r = 0; r < ARC_FRAME_SIZE; r++) {
		for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
			int8_t *px = frame + (size_t)r * ARC_FRAME_SIZE + c;
			if (*px != 0)
				continue;
			int use_first =
				quad_mode ? (r < half) == (c < half) : c < half;
			*px = (int8_t)(use_first ? c0 : c1);
		}
	}
}

static void m0r0_draw_overlays(int8_t *frame, const struct arc_sprites *s,
			       const struct arc_camera *camera,
			       const struct m0r0_static *st, int32_t level)
{
	int32_t scale, x_offset, y_offset;
	arc_scale_and_offset(camera, &scale, &x_offset, &y_offset);

	size_t live = (size_t)m0r0_clamp(camera->height, 0, ARC_FRAME_SIZE) *
		      ARC_FRAME_SIZE;

	uint8_t block_map[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	memset(block_map, 0, live);
	int32_t bcount = st->block_count[level];
	const int32_t *blocks =
		st->block_slots + (size_t)level * st->max_blocks;
	for (int32_t k = 0; k < bcount; k++) {
		int32_t slot = blocks[k];
		if (!arc_sprite_visible(s, slot))
			continue;
		int32_t yy = m0r0_clamp(s->y[slot], 0, ARC_FRAME_SIZE - 1);
		int32_t xx = m0r0_clamp(s->x[slot], 0, ARC_FRAME_SIZE - 1);
		block_map[(size_t)yy * ARC_FRAME_SIZE + xx] = 1;
	}

	uint8_t covered[ARC_FRAME_SIZE * ARC_FRAME_SIZE];
	memset(covered, 0, live);
	const int32_t *slots = st->mover_slot + (size_t)level * M0R0_NUM_MOVERS;
	for (int32_t lane = 0; lane < M0R0_NUM_MOVERS; lane++) {
		int32_t slot = slots[lane];
		if (slot < 0 || !arc_sprite_collidable(s, slot))
			continue;
		int32_t yy = m0r0_clamp(s->y[slot], 0, ARC_FRAME_SIZE - 1);
		int32_t xx = m0r0_clamp(s->x[slot], 0, ARC_FRAME_SIZE - 1);
		covered[(size_t)yy * ARC_FRAME_SIZE + xx] = 1;
	}

	const uint8_t *hmap = st->hazard_map +
			      (size_t)level * ARC_FRAME_SIZE * ARC_FRAME_SIZE;
	int32_t cxmap[ARC_FRAME_SIZE];
	uint8_t insidex[ARC_FRAME_SIZE], borderx[ARC_FRAME_SIZE];
	for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
		int32_t gx = (c - x_offset) / scale;
		int32_t cx = m0r0_clamp(gx, 0, ARC_FRAME_SIZE - 1);
		int32_t left = cx * scale + x_offset;
		cxmap[c] = cx;
		insidex[c] = c >= x_offset && gx < camera->width;
		borderx[c] = c == left || c == left + scale - 1;
	}

	for (int32_t r = 0; r < ARC_FRAME_SIZE; r++) {
		int32_t gy = (r - y_offset) / scale;
		if (r < y_offset || gy >= camera->height)
			continue;
		int32_t cy = m0r0_clamp(gy, 0, ARC_FRAME_SIZE - 1);
		int32_t top = cy * scale + y_offset;
		int row_border = r == top || r == top + scale - 1;
		const uint8_t *brow = block_map + (size_t)cy * ARC_FRAME_SIZE;
		const uint8_t *hrow = hmap + (size_t)cy * ARC_FRAME_SIZE;
		const uint8_t *crow = covered + (size_t)cy * ARC_FRAME_SIZE;
		int8_t *frow = frame + (size_t)r * ARC_FRAME_SIZE;
		for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
			if (!insidex[c])
				continue;
			int32_t cx = cxmap[c];
			int outline = brow[cx] && (row_border || borderx[c]);
			int hatch = hrow[cx] && !crow[cx] && ((r + c) % 2 == 1);
			if (outline || hatch)
				frow[c] = M0R0_OVERLAY_COLOUR;
		}
	}
}

static void m0r0_draw_budget(int8_t *frame, const struct m0r0_aux *aux)
{
	int32_t total = ARC_FRAME_SIZE * aux->steps;
	int32_t whole = total / M0R0_MAX_ACTIONS,
		rest = total % M0R0_MAX_ACTIONS;
	int round_up = 2 * rest > M0R0_MAX_ACTIONS ||
		       (2 * rest == M0R0_MAX_ACTIONS && whole % 2 == 1);
	int32_t filled = whole + (round_up ? 1 : 0);
	if (filled > ARC_FRAME_SIZE)
		filled = ARC_FRAME_SIZE;
	int8_t *top = frame;
	int8_t *bottom = frame + (size_t)(ARC_FRAME_SIZE - 1) * ARC_FRAME_SIZE;
	for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
		top[c] =
			(int8_t)(c < filled ? M0R0_HUD_COLOUR : M0R0_HUD_EMPTY);
		bottom[c] = (int8_t)(ARC_FRAME_SIZE - 1 - c < filled ?
					     M0R0_HUD_COLOUR :
					     M0R0_HUD_EMPTY);
	}
}

void m0r0_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   const struct m0r0_static *st, int32_t level,
			   const struct m0r0_aux *aux)
{
	m0r0_draw_background(frame, sprites, st, level, aux);
	m0r0_draw_overlays(frame, sprites, camera, st, level);
	m0r0_draw_budget(frame, aux);
}
