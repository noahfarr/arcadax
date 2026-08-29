#include "lf52.h"

#include <stdint.h>
#include <string.h>

enum {
	LF52_MISS_KIND = 0,
	LF52_HEART_MENU_KIND = 1,
	LF52_CANT_MOVE_WIGGLE_KIND = 2,
	LF52_JUMP_KIND = 3,
	LF52_WALL_BUMP_KIND = 4,
	LF52_LEVEL_ONE_DUST_KIND = 5,
	LF52_PEG_REVEAL_KIND = 6,
	LF52_UNREPRODUCIBLE_SHUFFLE_KIND = 7,
	LF52_JUMP_AND_REVEAL_KIND = 8
};
enum { LF52_RED_PEG_COLOR = 1, LF52_BLUE_PEG_COLOR = 2 };
enum { LF52_PITCH = 6, LF52_PEG_SPRITE_SIZE = 6 };
enum { LF52_JUMP_TICK_COUNT = 10 };
enum { LF52_CAPTURE_FADE_TICK_START = 2 };
enum { LF52_CANT_MOVE_WIGGLE_TICK_COUNT = 3 };
enum { LF52_WALL_BUMP_TICK_COUNT = 3 };
enum { LF52_WALL_BUMP_PAN_TICK_COUNT = 7, LF52_WALL_BUMP_PAN_WAIT = 2 };
enum { LF52_LEVEL_ONE_DUST_TICK_COUNT = 16 };
enum {
	LF52_PEG_REVEAL_TICK_COUNT = 27,
	LF52_PEG_REVEAL_WIGGLE_WAIT = 16,
	LF52_WIN_WIGGLE_TICK_COUNT = 26
};
enum { LF52_PEG_REVEAL_IMAGE_SWAP_TICK = 15, LF52_PEG_REVEAL_BUTTON_WAIT = 20 };
enum { LF52_UNREPRODUCIBLE_SHUFFLE_TICK_COUNT = 21 };
enum { LF52_JUMP_TRAIL_GHOST_LAYER = 2, LF52_JUMP_TRAIL_GHOST_TICK_COUNT = 9 };
enum {
	LF52_LEVEL5_DEST_X = 16,
	LF52_LEVEL5_DEST_Y = 2,
	LF52_LEVEL5_RED_CELL_X = 6,
	LF52_LEVEL5_RED_CELL_Y = 6
};
enum {
	LF52_P0 = LF52_STATIC_SLOT_COUNT,
	LF52_H0 = LF52_P0 + LF52_PEG_SLOT_COUNT,
	LF52_RING_SLOT = LF52_H0 + LF52_HEART_SLOT_COUNT,
	LF52_DUST_SLOT = LF52_RING_SLOT + 1,
	LF52_REVEAL_BUTTON_RISING_SLOT = LF52_DUST_SLOT + 1,
	LF52_REVEAL_BUTTON_SETTLED_SLOT = LF52_REVEAL_BUTTON_RISING_SLOT + 1,
	LF52_JUMP_TRAIL_GHOST_SLOT = LF52_REVEAL_BUTTON_SETTLED_SLOT + 1
};

static const int32_t LF52_JUMP_MOVE_OFFSET[LF52_JUMP_TICK_COUNT] = {
	0, 0, 2, 3, 6, 8, 9, 11, 11, 12
};
static const int32_t LF52_JUMP_HOP_OFFSET[LF52_JUMP_TICK_COUNT] = {
	0, 0, -2, -2, -3, -2, -2, -1, -1, 0
};
static const int32_t
	LF52_CANT_MOVE_WIGGLE_OFFSET[LF52_CANT_MOVE_WIGGLE_TICK_COUNT] = { 1,
									   -1,
									   0 };
static const int32_t LF52_WIN_WIGGLE_OFFSET[LF52_WIN_WIGGLE_TICK_COUNT] = {
	0,  0,	0,  0,	0,  0,	0, 0,  0,  0,  -2, -3, -5,
	-5, -6, -6, -5, -2, -1, 0, -2, -3, -4, -4, -1, 0
};
static const int32_t LF52_PEG_REVEAL_WIGGLE_OFFSET[4] = { 2, 1, -1, 0 };
static const int32_t LF52_PEG_REVEAL_BUTTON_RISE_OFFSET[7] = { -2,  -8,	 -11,
							       -10, -13, -13,
							       -14 };
static const int32_t LF52_DIR_X[4] = { 0, 1, 0, -1 };
static const int32_t LF52_DIR_Y[4] = { -1, 0, 1, 0 };

#define LF52_GRID_IDX(level, y, x)                                \
	(((size_t)(level) * LF52_GRID_MAX_HEIGHT + (size_t)(y)) * \
		 LF52_GRID_MAX_WIDTH +                            \
	 (size_t)(x))

static inline int32_t lf52_clampi(int32_t v, int32_t lo, int32_t hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t lf52_mini32(int32_t a, int32_t b)
{
	return a < b ? a : b;
}

static inline float lf52_wall_bump_ease(int32_t k)
{
	double t = (double)(k + 1) / 3.0;
	double u = 1.0 - t;
	return (float)(1.0 - u * u);
}

static inline float lf52_camera_pan_ease(int32_t k)
{
	double t = (double)(k + 1) / 5.0;
	double u = 1.0 - t;
	return (float)(1.0 - u * u);
}

static void lf52_stable_argsort(const int32_t *key, int32_t n, int32_t *order)
{
	for (int32_t i = 0; i < n; i++)
		order[i] = i;
	for (int32_t a = 1; a < n; a++) {
		int32_t v = order[a];
		int32_t kv = key[v];
		int32_t b = a - 1;
		while (b >= 0 && key[order[b]] > kv) {
			order[b + 1] = order[b];
			b--;
		}
		order[b + 1] = v;
	}
}

static int32_t lf52_argmin32(const int32_t *arr, int32_t n)
{
	int32_t best = 0;
	for (int32_t i = 1; i < n; i++)
		if (arr[i] < arr[best])
			best = i;
	return best;
}

static int32_t lf52_wall_bump_tick_position(int32_t start, int32_t delta,
					    int32_t tick, int single_occupant)
{
	int32_t ct = lf52_clampi(tick, 0, LF52_WALL_BUMP_TICK_COUNT - 1);
	float ease = lf52_wall_bump_ease(ct);
	float start_f = (float)start;
	float delta_f = (float)delta;
	int32_t fused = (int32_t)(start_f + delta_f * ease);
	int32_t separate = start + (int32_t)(delta_f * ease);
	return single_occupant ? fused : separate;
}

static int lf52_peg_is_jumpable(const struct lf52_static *st, int32_t level,
				int32_t x, int32_t y, const uint8_t *peg_alive,
				const int32_t *peg_grid_x,
				const int32_t *peg_grid_y,
				const int32_t *wall_tile_grid_x,
				const int32_t *wall_tile_grid_y)
{
	int in_bounds = x >= 0 && x < st->grid_width[level] && y >= 0 &&
			y < st->grid_height[level];
	int has_peg = 0;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++)
		if (peg_alive[i] && peg_grid_x[i] == x && peg_grid_y[i] == y) {
			has_peg = 1;
			break;
		}
	int32_t xc = lf52_clampi(x, 0, LF52_GRID_MAX_WIDTH - 1);
	int32_t yc = lf52_clampi(y, 0, LF52_GRID_MAX_HEIGHT - 1);
	int wall_tile_pin_here = 0;
	int32_t wt_count = st->wall_tile_count[level];
	for (int32_t i = 0; i < wt_count; i++) {
		if (st->wall_tile_has_jumpable_pin
			    [(size_t)level * LF52_WALL_TILE_SLOT_COUNT + i] &&
		    wall_tile_grid_x[i] == x && wall_tile_grid_y[i] == y) {
			wall_tile_pin_here = 1;
			break;
		}
	}
	int has_pin = st->jumpable_pin[LF52_GRID_IDX(level, yc, xc)] ||
		      wall_tile_pin_here;
	return in_bounds && (has_peg || has_pin);
}

static int lf52_peg_can_land(const struct lf52_static *st, int32_t level,
			     int32_t x, int32_t y, const uint8_t *peg_alive,
			     const int32_t *peg_grid_x,
			     const int32_t *peg_grid_y,
			     const int32_t *wall_tile_grid_x,
			     const int32_t *wall_tile_grid_y)
{
	int in_bounds = x >= 0 && x < st->grid_width[level] && y >= 0 &&
			y < st->grid_height[level];
	int32_t xc = lf52_clampi(x, 0, LF52_GRID_MAX_WIDTH - 1);
	int32_t yc = lf52_clampi(y, 0, LF52_GRID_MAX_HEIGHT - 1);
	int wall_tile_landable_here = 0;
	int32_t wt_count = st->wall_tile_count[level];
	for (int32_t i = 0; i < wt_count; i++) {
		if (st->wall_tile_is_landable_double
			    [(size_t)level * LF52_WALL_TILE_SLOT_COUNT + i] &&
		    wall_tile_grid_x[i] == x && wall_tile_grid_y[i] == y) {
			wall_tile_landable_here = 1;
			break;
		}
	}
	int floor_ok = st->landable_single[LF52_GRID_IDX(level, yc, xc)] ||
		       wall_tile_landable_here;
	int occupied = 0;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++)
		if (peg_alive[i] && peg_grid_x[i] == x && peg_grid_y[i] == y) {
			occupied = 1;
			break;
		}
	return in_bounds && floor_ok && !occupied;
}

static void lf52_peg_valid_jump_directions(
	const struct lf52_static *st, int32_t level, int32_t gx, int32_t gy,
	const uint8_t *peg_alive, const int32_t *peg_grid_x,
	const int32_t *peg_grid_y, const int32_t *wall_tile_grid_x,
	const int32_t *wall_tile_grid_y, uint8_t out[4])
{
	for (int32_t d = 0; d < 4; d++) {
		int32_t dx = LF52_DIR_X[d], dy = LF52_DIR_Y[d];
		int mid_ok = lf52_peg_is_jumpable(
			st, level, gx + dx, gy + dy, peg_alive, peg_grid_x,
			peg_grid_y, wall_tile_grid_x, wall_tile_grid_y);
		int dest_ok =
			lf52_peg_can_land(st, level, gx + 2 * dx, gy + 2 * dy,
					  peg_alive, peg_grid_x, peg_grid_y,
					  wall_tile_grid_x, wall_tile_grid_y);
		out[d] = (uint8_t)(mid_ok && dest_ok);
	}
}

static int lf52_jump_lands_on_reveal_trigger(
	const struct lf52_static *st, int32_t level, int32_t dest_gx,
	int32_t dest_gy, int32_t remaining_peg_count, int32_t mover_color,
	const uint8_t *peg_alive, const int32_t *peg_grid_x,
	const int32_t *peg_grid_y)
{
	int table_hit = 0;
	for (int32_t i = 0; i < LF52_TRIGGER_SLOT_COUNT; i++) {
		size_t idx = (size_t)level * LF52_TRIGGER_SLOT_COUNT + i;
		if (!st->jump_landing_reveal_trigger_valid[idx])
			continue;
		if (st->jump_landing_reveal_trigger_x[idx] != dest_gx)
			continue;
		if (st->jump_landing_reveal_trigger_y[idx] != dest_gy)
			continue;
		int32_t max_remaining =
			st->jump_landing_reveal_trigger_max_remaining[idx];
		if (max_remaining < 0 || remaining_peg_count <= max_remaining) {
			table_hit = 1;
			break;
		}
	}
	int red_peg_present = 0;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		if (peg_alive[i] &&
		    st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT + i] ==
			    LF52_RED_PEG_COLOR &&
		    peg_grid_x[i] == LF52_LEVEL5_RED_CELL_X &&
		    peg_grid_y[i] == LF52_LEVEL5_RED_CELL_Y) {
			red_peg_present = 1;
			break;
		}
	}
	int level5_hit = level == 5 && dest_gx == LF52_LEVEL5_DEST_X &&
			 dest_gy == LF52_LEVEL5_DEST_Y && mover_color == 0 &&
			 red_peg_present;
	return table_hit || level5_hit;
}

static int32_t lf52_peg_at_point(const struct lf52_aux *aux, int32_t x,
				 int32_t y)
{
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		int32_t px =
			aux->peg_grid_x[i] * LF52_PITCH + aux->camera_offset_x;
		int32_t py =
			aux->peg_grid_y[i] * LF52_PITCH + aux->camera_offset_y;
		if (aux->peg_alive[i] && x >= px &&
		    x < px + LF52_PEG_SPRITE_SIZE && y >= py &&
		    y < py + LF52_PEG_SPRITE_SIZE)
			return i;
	}
	return -1;
}

static void lf52_selected_peg_screen_position(const struct lf52_aux *aux,
					      int32_t *sx, int32_t *sy)
{
	int32_t s = aux->selected_peg < 0 ? 0 : aux->selected_peg;
	*sx = aux->peg_grid_x[s] * LF52_PITCH + aux->camera_offset_x;
	*sy = aux->peg_grid_y[s] * LF52_PITCH + aux->camera_offset_y;
}

static int32_t lf52_heart_direction_at_point(const struct lf52_aux *aux,
					     int32_t x, int32_t y)
{
	if (aux->selected_peg < 0)
		return -1;
	int32_t px, py;
	lf52_selected_peg_screen_position(aux, &px, &py);
	for (int32_t i = 0; i < 4; i++) {
		int32_t hx = px + LF52_DIR_X[i] * 2 * LF52_PITCH;
		int32_t hy = py + LF52_DIR_Y[i] * 2 * LF52_PITCH;
		if (aux->heart_direction_valid[i] && x >= hx &&
		    x < hx + LF52_PEG_SPRITE_SIZE && y >= hy &&
		    y < hy + LF52_PEG_SPRITE_SIZE)
			return i;
	}
	return -1;
}

static void lf52_push_undo_checkpoint(struct lf52_aux *aux)
{
	int32_t depth =
		lf52_clampi(aux->undo_depth, 0, LF52_UNDO_HISTORY_DEPTH - 1);
	memcpy(aux->undo_peg_grid_x[depth], aux->peg_grid_x,
	       sizeof(aux->peg_grid_x));
	memcpy(aux->undo_peg_grid_y[depth], aux->peg_grid_y,
	       sizeof(aux->peg_grid_y));
	memcpy(aux->undo_peg_alive[depth], aux->peg_alive,
	       sizeof(aux->peg_alive));
	memcpy(aux->undo_wall_tile_grid_x[depth], aux->wall_tile_grid_x,
	       sizeof(aux->wall_tile_grid_x));
	memcpy(aux->undo_wall_tile_grid_y[depth], aux->wall_tile_grid_y,
	       sizeof(aux->wall_tile_grid_y));
	aux->undo_selected_peg[depth] = aux->selected_peg;
	memcpy(aux->undo_heart_direction_valid[depth],
	       aux->heart_direction_valid, sizeof(aux->heart_direction_valid));
	aux->undo_camera_offset_x[depth] = aux->camera_offset_x;
	aux->undo_camera_offset_y[depth] = aux->camera_offset_y;
	aux->undo_pegs_show_revealed_image[depth] =
		aux->pegs_show_revealed_image;
	aux->undo_reveal_button_visible[depth] = aux->reveal_button_visible;
	aux->undo_stalemate_action_count[depth] = aux->stalemate_action_count;
	aux->undo_depth =
		lf52_mini32(aux->undo_depth + 1, LF52_UNDO_HISTORY_DEPTH - 1);
}

static void lf52_pop_undo_checkpoint(struct lf52_aux *aux)
{
	if (aux->undo_depth <= 0)
		return;
	int32_t depth = lf52_clampi(aux->undo_depth - 1, 0,
				    LF52_UNDO_HISTORY_DEPTH - 1);
	memcpy(aux->peg_grid_x, aux->undo_peg_grid_x[depth],
	       sizeof(aux->peg_grid_x));
	memcpy(aux->peg_grid_y, aux->undo_peg_grid_y[depth],
	       sizeof(aux->peg_grid_y));
	memcpy(aux->peg_alive, aux->undo_peg_alive[depth],
	       sizeof(aux->peg_alive));
	memcpy(aux->wall_tile_grid_x, aux->undo_wall_tile_grid_x[depth],
	       sizeof(aux->wall_tile_grid_x));
	memcpy(aux->wall_tile_grid_y, aux->undo_wall_tile_grid_y[depth],
	       sizeof(aux->wall_tile_grid_y));
	aux->selected_peg = aux->undo_selected_peg[depth];
	memcpy(aux->heart_direction_valid,
	       aux->undo_heart_direction_valid[depth],
	       sizeof(aux->heart_direction_valid));
	aux->camera_offset_x = aux->undo_camera_offset_x[depth];
	aux->camera_offset_y = aux->undo_camera_offset_y[depth];
	aux->pegs_show_revealed_image =
		aux->undo_pegs_show_revealed_image[depth];
	aux->reveal_button_visible = aux->undo_reveal_button_visible[depth];
	aux->stalemate_action_count = aux->undo_stalemate_action_count[depth];
	aux->undo_depth = aux->undo_depth - 1;
}

static void lf52_render_action_tick(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	const struct lf52_static *st =
		(const struct lf52_static *)game->statics;
	int32_t level = game->engine.level_index;
	struct arc_scene_table *scene = &game->scene;
	int32_t tick = aux->action_tick;
	int32_t kind = aux->action_kind;

	for (int32_t i = 0; i < LF52_STATIC_SLOT_COUNT; i++) {
		size_t sidx = (size_t)level * LF52_STATIC_SLOT_COUNT + i;
		int32_t attached = st->static_wall_tile_index[sidx];
		int has_wall_tile = attached >= 0;
		int32_t wt = has_wall_tile ? attached : 0;
		int wall_tile_bumping = kind == LF52_WALL_BUMP_KIND &&
					has_wall_tile &&
					aux->bump_wall_tile_moving[wt];
		int32_t local_x = st->static_wall_tile_local_x[sidx];
		int32_t local_y = st->static_wall_tile_local_y[sidx];

		int32_t static_rest_x, static_rest_y;
		if (has_wall_tile) {
			static_rest_x = aux->wall_tile_grid_x[wt] * LF52_PITCH +
					local_x + aux->camera_offset_x;
			static_rest_y = aux->wall_tile_grid_y[wt] * LF52_PITCH +
					local_y + aux->camera_offset_y;
		} else {
			static_rest_x =
				st->static_x[sidx] + aux->camera_offset_x;
			static_rest_y =
				st->static_y[sidx] + aux->camera_offset_y;
		}

		if (wall_tile_bumping) {
			int32_t static_bump_start_x =
				aux->bump_wall_tile_start_x[wt] + local_x;
			int32_t static_bump_start_y =
				aux->bump_wall_tile_start_y[wt] + local_y;
			scene->x[i] = lf52_wall_bump_tick_position(
				static_bump_start_x,
				aux->bump_direction_x * LF52_PITCH, tick,
				aux->bump_single_occupant);
			scene->y[i] = lf52_wall_bump_tick_position(
				static_bump_start_y,
				aux->bump_direction_y * LF52_PITCH, tick,
				aux->bump_single_occupant);
		} else {
			scene->x[i] = static_rest_x;
			scene->y[i] = static_rest_y;
		}
	}

	int32_t rest_x[LF52_PEG_SLOT_COUNT], rest_y[LF52_PEG_SLOT_COUNT];
	int32_t base_image[LF52_PEG_SLOT_COUNT];
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		rest_x[i] =
			aux->peg_grid_x[i] * LF52_PITCH + aux->camera_offset_x;
		rest_y[i] =
			aux->peg_grid_y[i] * LF52_PITCH + aux->camera_offset_y;
		int32_t color =
			st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT + i];
		int32_t revealed = aux->pegs_show_revealed_image ?
					   st->revealed_peg_image :
					   st->peg_base_image[color];
		base_image[i] = aux->peg_alive[i] ? revealed : -1;
	}

	int is_jump_or_jump_reveal =
		kind == LF52_JUMP_KIND || kind == LF52_JUMP_AND_REVEAL_KIND;
	int is_jump_phase_kind =
		kind == LF52_JUMP_KIND || (kind == LF52_JUMP_AND_REVEAL_KIND &&
					   tick < LF52_JUMP_TICK_COUNT);

	int32_t jump_tick = lf52_clampi(tick, 0, LF52_JUMP_TICK_COUNT - 1);
	int32_t jump_move =
		aux->jump_direction_x * LF52_JUMP_MOVE_OFFSET[jump_tick];
	int32_t jump_hop = aux->jump_direction_y == 0 ?
				   LF52_JUMP_HOP_OFFSET[jump_tick] :
				   0;
	int32_t jump_x = aux->jump_mover_start_x + jump_move;
	int32_t jump_y =
		aux->jump_mover_start_y +
		aux->jump_direction_y * LF52_JUMP_MOVE_OFFSET[jump_tick] +
		jump_hop;

	int32_t wiggle_x_offset = LF52_CANT_MOVE_WIGGLE_OFFSET[lf52_clampi(
		tick, 0, LF52_CANT_MOVE_WIGGLE_TICK_COUNT - 1)];
	int32_t reveal_wiggle_index =
		lf52_clampi(tick - LF52_PEG_REVEAL_WIGGLE_WAIT, 0, 3);
	int32_t reveal_x_offset =
		tick < LF52_PEG_REVEAL_WIGGLE_WAIT ?
			0 :
			LF52_PEG_REVEAL_WIGGLE_OFFSET[reveal_wiggle_index];

	int32_t peg_x[LF52_PEG_SLOT_COUNT], peg_y[LF52_PEG_SLOT_COUNT];
	uint8_t is_mover[LF52_PEG_SLOT_COUNT], is_captured[LF52_PEG_SLOT_COUNT],
		is_revealing[LF52_PEG_SLOT_COUNT];
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		int mover = is_jump_phase_kind && i == aux->jump_mover_peg;
		int captured = is_jump_or_jump_reveal &&
			       aux->jump_captured_peg >= 0 &&
			       i == aux->jump_captured_peg;
		int wiggling = kind == LF52_CANT_MOVE_WIGGLE_KIND &&
			       i == aux->jump_mover_peg;
		int bumping =
			kind == LF52_WALL_BUMP_KIND && aux->bump_moving_peg[i];
		int revealing = (kind == LF52_PEG_REVEAL_KIND ||
				 kind == LF52_JUMP_AND_REVEAL_KIND) &&
				aux->peg_alive[i];
		is_mover[i] = (uint8_t)mover;
		is_captured[i] = (uint8_t)captured;
		is_revealing[i] = (uint8_t)revealing;

		int32_t bump_x = lf52_wall_bump_tick_position(
			aux->bump_peg_start_x[i],
			aux->bump_direction_x * LF52_PITCH, tick,
			aux->bump_single_occupant);
		int32_t bump_y = lf52_wall_bump_tick_position(
			aux->bump_peg_start_y[i],
			aux->bump_direction_y * LF52_PITCH, tick,
			aux->bump_single_occupant);
		int32_t reveal_x = rest_x[i] + reveal_x_offset;
		int32_t wiggle_x = rest_x[i] + wiggle_x_offset;

		int32_t px =
			mover ? jump_x :
				(wiggling ?
					 wiggle_x :
					 (bumping ? bump_x :
						    (revealing ? reveal_x :
								 rest_x[i])));
		int32_t py = mover ? jump_y : (bumping ? bump_y : rest_y[i]);

		int win_wiggling = kind == LF52_JUMP_KIND &&
				   aux->jump_triggers_win && aux->peg_alive[i];
		if (win_wiggling)
			py += LF52_WIN_WIGGLE_OFFSET[lf52_clampi(
				tick, 0, LF52_WIN_WIGGLE_TICK_COUNT - 1)];

		peg_x[i] = px;
		peg_y[i] = py;
	}

	int32_t vertical_jump = aux->jump_direction_y != 0;
	int32_t pulse_tick = lf52_clampi(tick, 0, LF52_PULSE_TICK_COUNT - 1);
	int32_t mover_slot_clipped =
		aux->jump_mover_peg < 0 ? 0 : aux->jump_mover_peg;
	int32_t mover_color =
		st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT +
			      mover_slot_clipped];
	int32_t mover_base = aux->pegs_show_revealed_image ?
				     st->revealed_peg_image :
				     st->peg_base_image[mover_color];
	int32_t pulse_image_val =
		tick < LF52_PULSE_TICK_COUNT ?
			st->peg_pulse_image[(size_t)mover_color *
						    LF52_PULSE_TICK_COUNT +
					    pulse_tick] :
			mover_base;
	int32_t mover_image = vertical_jump ? pulse_image_val : mover_base;

	int32_t captured_slot_clipped =
		aux->jump_captured_peg < 0 ? 0 : aux->jump_captured_peg;
	int32_t captured_color =
		st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT +
			      captured_slot_clipped];
	int32_t fade_local = tick - LF52_CAPTURE_FADE_TICK_START;
	int fading =
		fade_local >= 0 && fade_local < LF52_CAPTURE_FADE_TICK_COUNT;
	int32_t captured_base = aux->pegs_show_revealed_image ?
					st->revealed_peg_image :
					st->peg_base_image[captured_color];
	int32_t fade_tick =
		lf52_clampi(fade_local, 0, LF52_CAPTURE_FADE_TICK_COUNT - 1);
	int32_t captured_fade_image =
		aux->pegs_show_revealed_image ?
			st->revealed_peg_capture_fade_image[fade_tick] :
			st->peg_capture_fade_image
				[(size_t)captured_color *
					 LF52_CAPTURE_FADE_TICK_COUNT +
				 fade_tick];
	int32_t captured_image =
		fading ? captured_fade_image :
			 (tick < LF52_CAPTURE_FADE_TICK_START ? captured_base :
								-1);

	int is_reveal_action = kind == LF52_PEG_REVEAL_KIND ||
			       kind == LF52_JUMP_AND_REVEAL_KIND;

	int32_t image[LF52_PEG_SLOT_COUNT];
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		int32_t color =
			st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT + i];
		int32_t revealing_at_tick =
			(aux->pegs_show_revealed_image ||
			 tick >= LF52_PEG_REVEAL_IMAGE_SWAP_TICK) ?
				st->revealed_peg_image :
				st->peg_base_image[color];
		int32_t reveal_image =
			aux->peg_alive[i] ? revealing_at_tick : -1;
		image[i] = is_mover[i] ?
				   mover_image :
				   (is_captured[i] ?
					    captured_image :
					    (is_revealing[i] ? reveal_image :
							       base_image[i]));
	}

	int trail_ghost_visible = is_jump_or_jump_reveal &&
				  aux->jump_shows_trail_ghost &&
				  tick < LF52_JUMP_TRAIL_GHOST_TICK_COUNT;
	int32_t trail_ghost_settled_image = st->peg_base_image[mover_color];
	int32_t trail_ghost_pulse_image =
		tick < LF52_PULSE_TICK_COUNT ?
			st->peg_pulse_image[(size_t)mover_color *
						    LF52_PULSE_TICK_COUNT +
					    pulse_tick] :
			trail_ghost_settled_image;
	int32_t trail_ghost_image = vertical_jump ? trail_ghost_pulse_image :
						    trail_ghost_settled_image;
	scene->image[LF52_JUMP_TRAIL_GHOST_SLOT] =
		trail_ghost_visible ? trail_ghost_image : -1;
	scene->x[LF52_JUMP_TRAIL_GHOST_SLOT] = jump_x;
	scene->y[LF52_JUMP_TRAIL_GHOST_SLOT] = jump_y;

	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		scene->image[LF52_P0 + i] = image[i];
		scene->x[LF52_P0 + i] = peg_x[i];
		scene->y[LF52_P0 + i] = peg_y[i];
		scene->order[LF52_P0 + i] =
			is_mover[i] ? LF52_H0 : (LF52_P0 + i);
	}

	int32_t selected_slot = aux->selected_peg < 0 ? 0 : aux->selected_peg;
	int32_t sel_x = peg_x[selected_slot], sel_y = peg_y[selected_slot];
	int32_t heart_variant = aux->heart_blink_tick == 0 ?
					st->heart_image[0] :
					st->heart_image[1];
	for (int32_t i = 0; i < 4; i++) {
		int visible =
			aux->selected_peg >= 0 && aux->heart_direction_valid[i];
		scene->image[LF52_H0 + i] = visible ? heart_variant : -1;
		scene->x[LF52_H0 + i] = sel_x + LF52_DIR_X[i] * 2 * LF52_PITCH;
		scene->y[LF52_H0 + i] = sel_y + LF52_DIR_Y[i] * 2 * LF52_PITCH;
	}

	int ring_visible = aux->selected_peg >= 0;
	scene->image[LF52_RING_SLOT] = ring_visible ? st->ring_image : -1;
	scene->x[LF52_RING_SLOT] = sel_x;
	scene->y[LF52_RING_SLOT] = sel_y;

	int at_grid_1_2 = aux->selected_peg >= 0 &&
			  aux->peg_grid_x[selected_slot] == 1 &&
			  aux->peg_grid_y[selected_slot] == 2;
	int32_t dust_x =
		aux->reveal_button_visible ? 3 : (at_grid_1_2 ? 27 : 15);
	int32_t dust_y = aux->reveal_button_visible ? 52 : 16;
	int32_t dust_stage = lf52_clampi(tick / 3, 0, 4);
	int dust_visible = kind == LF52_LEVEL_ONE_DUST_KIND &&
			   tick < LF52_LEVEL_ONE_DUST_TICK_COUNT - 1;
	int32_t dust_variant =
		dust_stage % 2 == 0 ? st->dust_image[0] : st->dust_image[1];
	scene->image[LF52_DUST_SLOT] = dust_visible ? dust_variant : -1;
	scene->x[LF52_DUST_SLOT] = dust_x;
	scene->y[LF52_DUST_SLOT] = dust_y;

	int32_t button_rise_index =
		lf52_clampi(tick - LF52_PEG_REVEAL_BUTTON_WAIT, 0, 6);
	int32_t button_y_during_reveal =
		tick < LF52_PEG_REVEAL_BUTTON_WAIT ?
			65 :
			65 + LF52_PEG_REVEAL_BUTTON_RISE_OFFSET
					[button_rise_index];
	int rising_button_visible =
		is_reveal_action && tick >= LF52_PEG_REVEAL_BUTTON_WAIT;
	int button_visible =
		aux->reveal_button_visible || rising_button_visible;
	int settled_button_visible =
		is_reveal_action ?
			aux->reveal_button_was_visible_before_this_action :
			aux->reveal_button_visible;
	scene->image[LF52_REVEAL_BUTTON_SETTLED_SLOT] =
		settled_button_visible ? st->reveal_button_image : -1;
	scene->x[LF52_REVEAL_BUTTON_SETTLED_SLOT] = 2;
	scene->y[LF52_REVEAL_BUTTON_SETTLED_SLOT] = 51;
	scene->image[LF52_REVEAL_BUTTON_RISING_SLOT] =
		rising_button_visible ? st->reveal_button_image : -1;
	scene->x[LF52_REVEAL_BUTTON_RISING_SLOT] = 2;
	scene->y[LF52_REVEAL_BUTTON_RISING_SLOT] = button_y_during_reveal;

	int32_t camera_pan_index = lf52_clampi(
		tick - LF52_WALL_BUMP_PAN_WAIT, 0,
		LF52_WALL_BUMP_PAN_TICK_COUNT - LF52_WALL_BUMP_PAN_WAIT - 1);
	int applying_pan = kind == LF52_WALL_BUMP_KIND &&
			   aux->bump_pans_camera &&
			   tick >= LF52_WALL_BUMP_PAN_WAIT;
	float pan_fraction = lf52_camera_pan_ease(camera_pan_index);
	int32_t pan_offset_x =
		(int32_t)((float)aux->bump_camera_delta_x * pan_fraction);
	int32_t pan_offset_y =
		(int32_t)((float)aux->bump_camera_delta_y * pan_fraction);
	int32_t new_camera_x = applying_pan ?
				       aux->bump_camera_start_x + pan_offset_x :
				       aux->camera_offset_x;
	int32_t new_camera_y = applying_pan ?
				       aux->bump_camera_start_y + pan_offset_y :
				       aux->camera_offset_y;

	int pegs_now_revealed =
		aux->pegs_show_revealed_image ||
		(is_reveal_action && tick >= LF52_PEG_REVEAL_IMAGE_SWAP_TICK);
	int reveal_button_now_visible = button_visible;

	arc_scene_touch(scene);
	aux->camera_offset_x = new_camera_x;
	aux->camera_offset_y = new_camera_y;
	aux->pegs_show_revealed_image = (uint8_t)pegs_now_revealed;
	aux->reveal_button_visible = (uint8_t)reveal_button_now_visible;
	aux->heart_blink_tick = lf52_mini32(aux->heart_blink_tick + 1, 1);
}

static void lf52_advance_action(struct arc_scene_game *game)
{
	lf52_render_action_tick(game);
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	int32_t tick = aux->action_tick + 1;
	int done = tick >= aux->action_tick_count;
	aux->action_tick = tick;
	aux->action_in_progress = (uint8_t)!done;
	if (done) {
		int is_jump_kind =
			aux->action_kind == LF52_JUMP_KIND ||
			aux->action_kind == LF52_JUMP_AND_REVEAL_KIND;
		int triggers_win = is_jump_kind && aux->jump_triggers_win;
		const struct lf52_static *st =
			(const struct lf52_static *)game->statics;
		int32_t level = game->engine.level_index;
		int exceeds_stalemate_budget =
			aux->stalemate_action_count >=
			st->stalemate_action_budget[level];
		if (triggers_win)
			arc_scene_game_next_level(game);
		if (!triggers_win && exceeds_stalemate_budget)
			arc_scene_game_lose(game);
		arc_scene_game_complete_action(game);
	}
}

static void lf52_start_miss(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	aux->selected_peg = -1;
	memset(aux->heart_direction_valid, 0,
	       sizeof(aux->heart_direction_valid));
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count = 2;
	aux->action_kind = LF52_MISS_KIND;
	aux->skip_hud_sweep = 0;
	lf52_advance_action(game);
}

static void lf52_start_undo(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	lf52_pop_undo_checkpoint(aux);
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count = 2;
	aux->action_kind = LF52_MISS_KIND;
	aux->skip_hud_sweep = 0;
	lf52_advance_action(game);
}

static void lf52_start_select(struct arc_scene_game *game, int32_t slot)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	const struct lf52_static *st =
		(const struct lf52_static *)game->statics;
	int32_t level = game->engine.level_index;
	int32_t gx = aux->peg_grid_x[slot], gy = aux->peg_grid_y[slot];
	uint8_t valid[4];
	lf52_peg_valid_jump_directions(st, level, gx, gy, aux->peg_alive,
				       aux->peg_grid_x, aux->peg_grid_y,
				       aux->wall_tile_grid_x,
				       aux->wall_tile_grid_y, valid);
	int has_move = valid[0] || valid[1] || valid[2] || valid[3];
	aux->selected_peg = has_move ? slot : -1;
	for (int32_t i = 0; i < 4; i++)
		aux->heart_direction_valid[i] = has_move ? valid[i] : 0;
	aux->heart_blink_tick = 0;
	aux->jump_mover_peg = slot;
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count =
		has_move ? 2 : LF52_CANT_MOVE_WIGGLE_TICK_COUNT;
	aux->action_kind =
		has_move ? LF52_HEART_MENU_KIND : LF52_CANT_MOVE_WIGGLE_KIND;
	aux->skip_hud_sweep = 0;
	lf52_advance_action(game);
}

static void lf52_start_jump(struct arc_scene_game *game,
			    int32_t direction_index)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	const struct lf52_static *st =
		(const struct lf52_static *)game->statics;
	int32_t level = game->engine.level_index;
	int32_t mover = aux->selected_peg;
	int32_t dx = LF52_DIR_X[direction_index],
		dy = LF52_DIR_Y[direction_index];
	int32_t from_gx = aux->peg_grid_x[mover],
		from_gy = aux->peg_grid_y[mover];
	int32_t mid_gx = from_gx + dx, mid_gy = from_gy + dy;
	int32_t dest_gx = from_gx + 2 * dx, dest_gy = from_gy + 2 * dy;

	int32_t mid_index = -1;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		if (aux->peg_alive[i] && aux->peg_grid_x[i] == mid_gx &&
		    aux->peg_grid_y[i] == mid_gy) {
			mid_index = i;
			break;
		}
	}
	int mid_found = mid_index >= 0;
	int32_t mover_color =
		st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT + mover];
	int32_t mid_color =
		mid_found ?
			st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT +
				      mid_index] :
			st->peg_color[(size_t)level * LF52_PEG_SLOT_COUNT + 0];
	int captures = mid_found && mid_color == mover_color &&
		       mid_color != LF52_BLUE_PEG_COLOR;
	int32_t captured_slot = captures ? mid_index : -1;
	int shows_trail_ghost = mid_found && mid_color != mover_color &&
				mid_color != LF52_BLUE_PEG_COLOR;

	int32_t peg_grid_x_new[LF52_PEG_SLOT_COUNT],
		peg_grid_y_new[LF52_PEG_SLOT_COUNT];
	uint8_t peg_alive_new[LF52_PEG_SLOT_COUNT];
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		peg_grid_x_new[i] = i == mover ? dest_gx : aux->peg_grid_x[i];
		peg_grid_y_new[i] = i == mover ? dest_gy : aux->peg_grid_y[i];
		peg_alive_new[i] = (uint8_t)(aux->peg_alive[i] &&
					     !(captures && i == mid_index));
	}

	int32_t alive_count = 0;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++)
		alive_count += peg_alive_new[i] ? 1 : 0;
	int32_t remaining_peg_count = alive_count - st->win_blue_offset[level];
	int triggers_win =
		remaining_peg_count == st->win_target_peg_count[level];
	int triggers_reveal = lf52_jump_lands_on_reveal_trigger(
		st, level, dest_gx, dest_gy, remaining_peg_count, mover_color,
		peg_alive_new, peg_grid_x_new, peg_grid_y_new);

	memcpy(aux->peg_grid_x, peg_grid_x_new, sizeof(peg_grid_x_new));
	memcpy(aux->peg_grid_y, peg_grid_y_new, sizeof(peg_grid_y_new));
	memcpy(aux->peg_alive, peg_alive_new, sizeof(peg_alive_new));
	aux->selected_peg = -1;
	memset(aux->heart_direction_valid, 0,
	       sizeof(aux->heart_direction_valid));
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count =
		triggers_reveal ? LF52_PEG_REVEAL_TICK_COUNT :
				  (triggers_win ? LF52_WIN_WIGGLE_TICK_COUNT :
						  LF52_JUMP_TICK_COUNT);
	aux->action_kind =
		triggers_reveal ? LF52_JUMP_AND_REVEAL_KIND : LF52_JUMP_KIND;
	aux->jump_mover_peg = mover;
	aux->jump_captured_peg = captured_slot;
	aux->jump_mover_start_x = from_gx * LF52_PITCH + aux->camera_offset_x;
	aux->jump_mover_start_y = from_gy * LF52_PITCH + aux->camera_offset_y;
	aux->jump_direction_x = dx;
	aux->jump_direction_y = dy;
	aux->jump_triggers_win = (uint8_t)triggers_win;
	aux->jump_shows_trail_ghost = (uint8_t)shows_trail_ghost;
	aux->reveal_button_was_visible_before_this_action =
		aux->reveal_button_visible;
	aux->skip_hud_sweep = 0;
	lf52_advance_action(game);
}

static void lf52_start_level_one_dust(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count = LF52_LEVEL_ONE_DUST_TICK_COUNT;
	aux->action_kind = LF52_LEVEL_ONE_DUST_KIND;
	aux->skip_hud_sweep = 0;
	lf52_advance_action(game);
}

static void lf52_start_peg_reveal(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	aux->reveal_button_was_visible_before_this_action =
		aux->reveal_button_visible;
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count = LF52_PEG_REVEAL_TICK_COUNT;
	aux->action_kind = LF52_PEG_REVEAL_KIND;
	aux->skip_hud_sweep = 0;
	lf52_advance_action(game);
}

static void lf52_start_unreproducible_shuffle(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	const struct lf52_static *st =
		(const struct lf52_static *)game->statics;
	int32_t level = game->engine.level_index;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		size_t idx = (size_t)level * LF52_PEG_SLOT_COUNT + i;
		aux->peg_grid_x[i] = st->peg_grid_x_initial[idx];
		aux->peg_grid_y[i] = st->peg_grid_y_initial[idx];
		aux->peg_alive[i] = st->peg_alive_initial[idx];
	}
	for (int32_t i = 0; i < LF52_WALL_TILE_SLOT_COUNT; i++) {
		size_t idx = (size_t)level * LF52_WALL_TILE_SLOT_COUNT + i;
		aux->wall_tile_grid_x[i] = st->wall_tile_grid_x_initial[idx];
		aux->wall_tile_grid_y[i] = st->wall_tile_grid_y_initial[idx];
	}
	aux->selected_peg = -1;
	memset(aux->heart_direction_valid, 0,
	       sizeof(aux->heart_direction_valid));
	aux->camera_offset_x = st->offset_x[level];
	aux->camera_offset_y = st->offset_y[level];
	aux->pegs_show_revealed_image = 0;
	aux->reveal_button_visible = 0;
	aux->reveal_button_was_visible_before_this_action = 0;
	aux->undo_depth = 0;
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count = LF52_UNREPRODUCIBLE_SHUFFLE_TICK_COUNT;
	aux->action_kind = LF52_UNREPRODUCIBLE_SHUFFLE_KIND;
	aux->skip_hud_sweep = 0;
	lf52_advance_action(game);
}

static void lf52_start_wall_bump_action(struct arc_scene_game *game,
					int32_t direction_index)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	const struct lf52_static *st =
		(const struct lf52_static *)game->statics;
	int32_t level = game->engine.level_index;
	int32_t dx = LF52_DIR_X[direction_index],
		dy = LF52_DIR_Y[direction_index];
	int32_t wt_count = st->wall_tile_count[level];

	int32_t wall_push_priority[LF52_WALL_TILE_SLOT_COUNT];
	for (int32_t i = 0; i < LF52_WALL_TILE_SLOT_COUNT; i++) {
		int active = i < wt_count;
		int32_t key;
		if (dx != 0)
			key = dx > 0 ? -aux->wall_tile_grid_x[i] :
				       aux->wall_tile_grid_x[i];
		else
			key = dy > 0 ? -aux->wall_tile_grid_y[i] :
				       aux->wall_tile_grid_y[i];
		wall_push_priority[i] = active ? key : INT32_MAX;
	}
	int32_t wall_push_order[LF52_WALL_TILE_SLOT_COUNT];
	lf52_stable_argsort(wall_push_priority, LF52_WALL_TILE_SLOT_COUNT,
			    wall_push_order);

	int32_t cur_x[LF52_WALL_TILE_SLOT_COUNT],
		cur_y[LF52_WALL_TILE_SLOT_COUNT];
	uint8_t blocked[LF52_WALL_TILE_SLOT_COUNT];
	memcpy(cur_x, aux->wall_tile_grid_x, sizeof(cur_x));
	memcpy(cur_y, aux->wall_tile_grid_y, sizeof(cur_y));
	memset(blocked, 0, sizeof(blocked));

	for (int32_t k = 0; k < LF52_WALL_TILE_SLOT_COUNT; k++) {
		int32_t idx = wall_push_order[k];
		int32_t gx = cur_x[idx], gy = cur_y[idx];
		int32_t front_x = gx + dx, front_y = gy + dy;
		int front_occupied = 0;
		for (int32_t j = 0; j < wt_count; j++) {
			if (j == idx)
				continue;
			if (cur_x[j] == front_x && cur_y[j] == front_y) {
				front_occupied = 1;
				break;
			}
		}
		int active = idx < wt_count;
		int32_t xc = lf52_clampi(front_x, 0, LF52_GRID_MAX_WIDTH - 1);
		int32_t yc = lf52_clampi(front_y, 0, LF52_GRID_MAX_HEIGHT - 1);
		int in_bounds = front_x >= 0 && front_x < LF52_GRID_MAX_WIDTH &&
				front_y >= 0 && front_y < LF52_GRID_MAX_HEIGHT;
		int wall_here = in_bounds &&
				st->wall_present[LF52_GRID_IDX(level, yc, xc)];
		int is_blocked = active && wall_here && !front_occupied;
		cur_x[idx] = is_blocked ? front_x : gx;
		cur_y[idx] = is_blocked ? front_y : gy;
		blocked[idx] = (uint8_t)is_blocked;
	}

	uint8_t peg_blocked[LF52_PEG_SLOT_COUNT];
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		int on_tile = 0;
		for (int32_t j = 0; j < LF52_WALL_TILE_SLOT_COUNT; j++) {
			if (blocked[j] &&
			    aux->wall_tile_grid_x[j] == aux->peg_grid_x[i] &&
			    aux->wall_tile_grid_y[j] == aux->peg_grid_y[i]) {
				on_tile = 1;
				break;
			}
		}
		peg_blocked[i] = (uint8_t)(on_tile && aux->peg_alive[i]);
	}

	int32_t blocked_tile_count = 0;
	for (int32_t i = 0; i < LF52_WALL_TILE_SLOT_COUNT; i++)
		blocked_tile_count += blocked[i] ? 1 : 0;
	int any_cell_blocked = blocked_tile_count > 0;
	int blocked_tile_carries_pin = 0;
	for (int32_t i = 0; i < LF52_WALL_TILE_SLOT_COUNT; i++) {
		if (blocked[i] &&
		    st->wall_tile_has_jumpable_pin
			    [(size_t)level * LF52_WALL_TILE_SLOT_COUNT + i]) {
			blocked_tile_carries_pin = 1;
			break;
		}
	}
	int32_t peg_blocked_count = 0;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++)
		peg_blocked_count += peg_blocked[i] ? 1 : 0;
	int single_occupant = blocked_tile_count == 1 &&
			      peg_blocked_count == 0 &&
			      !blocked_tile_carries_pin;

	int32_t priority_key[LF52_PEG_SLOT_COUNT];
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		int32_t key;
		if (dx != 0)
			key = dx > 0 ? -aux->peg_grid_x[i] : aux->peg_grid_x[i];
		else
			key = dy > 0 ? -aux->peg_grid_y[i] : aux->peg_grid_y[i];
		priority_key[i] = peg_blocked[i] ? key : INT32_MAX;
	}
	int32_t leader = lf52_argmin32(priority_key, LF52_PEG_SLOT_COUNT);
	int has_leader = peg_blocked_count > 0;

	int32_t leader_gy = aux->peg_grid_y[leader];
	int level3_high = level == 3 && leader_gy >= 11;
	int32_t pan_dx;
	if (level == 2)
		pan_dx = -8 * dx;
	else if (level3_high)
		pan_dx = 0;
	else if (level == 3 || level == 4 || level == 5 || level == 8)
		pan_dx = -6 * dx;
	else
		pan_dx = 0;
	int32_t pan_dy;
	if (level == 7 || level == 8)
		pan_dy = -6 * dy;
	else if (level == 3 && !level3_high)
		pan_dy = -6 * dy;
	else
		pan_dy = 0;
	int pan_wanted = has_leader && (pan_dx != 0 || pan_dy != 0);
	int pan_cancelled =
		level != 4 && aux->camera_offset_x >= 5 && pan_dx > 0;
	int pan_active = pan_wanted && !pan_cancelled;

	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		aux->bump_moving_peg[i] = peg_blocked[i];
		aux->bump_peg_start_x[i] =
			aux->peg_grid_x[i] * LF52_PITCH + aux->camera_offset_x;
		aux->bump_peg_start_y[i] =
			aux->peg_grid_y[i] * LF52_PITCH + aux->camera_offset_y;
	}
	aux->bump_direction_index = direction_index;
	aux->bump_direction_x = dx;
	aux->bump_direction_y = dy;
	aux->bump_single_occupant = (uint8_t)single_occupant;
	aux->bump_pans_camera = (uint8_t)pan_active;
	aux->bump_camera_start_x = aux->camera_offset_x;
	aux->bump_camera_start_y = aux->camera_offset_y;
	aux->bump_camera_delta_x = pan_dx;
	aux->bump_camera_delta_y = pan_dy;
	for (int32_t i = 0; i < LF52_WALL_TILE_SLOT_COUNT; i++) {
		aux->bump_wall_tile_moving[i] = blocked[i];
		aux->bump_wall_tile_start_x[i] =
			aux->wall_tile_grid_x[i] * LF52_PITCH +
			aux->camera_offset_x;
		aux->bump_wall_tile_start_y[i] =
			aux->wall_tile_grid_y[i] * LF52_PITCH +
			aux->camera_offset_y;
	}
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		aux->peg_grid_x[i] =
			aux->peg_grid_x[i] + (peg_blocked[i] ? dx : 0);
		aux->peg_grid_y[i] =
			aux->peg_grid_y[i] + (peg_blocked[i] ? dy : 0);
	}
	memcpy(aux->wall_tile_grid_x, cur_x, sizeof(cur_x));
	memcpy(aux->wall_tile_grid_y, cur_y, sizeof(cur_y));
	if (any_cell_blocked) {
		aux->selected_peg = -1;
		memset(aux->heart_direction_valid, 0,
		       sizeof(aux->heart_direction_valid));
	}
	aux->action_in_progress = 1;
	aux->action_tick = 0;
	aux->action_tick_count =
		pan_active ? LF52_WALL_BUMP_PAN_TICK_COUNT :
			     (any_cell_blocked ? LF52_WALL_BUMP_TICK_COUNT : 2);
	aux->action_kind = LF52_WALL_BUMP_KIND;
	aux->skip_hud_sweep = 0;
	aux->stalemate_action_count += 1;
	lf52_advance_action(game);
}

static void lf52_dispatch_click(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	int32_t level = game->engine.level_index;
	int32_t x = game->engine.action_x, y = game->engine.action_y;
	int in_shuffle_zone =
		aux->reveal_button_visible && x < 16 && y > ARC_FRAME_SIZE - 16;
	int32_t direction_index = lf52_heart_direction_at_point(aux, x, y);
	int32_t peg_hit = lf52_peg_at_point(aux, x, y);
	int is_jump = direction_index >= 0;
	int is_select = !is_jump && peg_hit >= 0;
	int32_t alive_count = 0;
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++)
		alive_count += aux->peg_alive[i] ? 1 : 0;
	int is_level_one_dust =
		!is_jump && !is_select && level == 0 &&
		(aux->reveal_button_visible || alive_count == 5);

	aux->stalemate_action_count += in_shuffle_zone ? 0 : 1;

	if (in_shuffle_zone)
		lf52_start_unreproducible_shuffle(game);
	else if (is_jump)
		lf52_start_jump(game, direction_index);
	else if (is_select)
		lf52_start_select(game, peg_hit);
	else if (is_level_one_dust)
		lf52_start_level_one_dust(game);
	else
		lf52_start_miss(game);
}

static void lf52_dispatch_action(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	int32_t action_id = game->engine.action_id;
	int pushes_undo = action_id != SCENE_ACTION_RESET && action_id != 7;
	if (pushes_undo)
		lf52_push_undo_checkpoint(aux);
	int32_t routed = lf52_clampi(action_id, 0, 7);
	switch (routed) {
	case 0:
		lf52_start_miss(game);
		break;
	case 1:
		lf52_start_wall_bump_action(game, 0);
		break;
	case 2:
		lf52_start_wall_bump_action(game, 2);
		break;
	case 3:
		lf52_start_wall_bump_action(game, 3);
		break;
	case 4:
		lf52_start_wall_bump_action(game, 1);
		break;
	case 5:
		lf52_start_peg_reveal(game);
		break;
	case 6:
		lf52_dispatch_click(game);
		break;
	case 7:
		lf52_start_undo(game);
		break;
	default:
		break;
	}
}

void lf52_zero_aux(struct lf52_aux *aux)
{
	memset(aux, 0, sizeof(*aux));
	aux->selected_peg = -1;
	aux->jump_mover_peg = -1;
	aux->jump_captured_peg = -1;
	aux->skip_hud_sweep = 1;
	memset(aux->undo_selected_peg, 0xFF, sizeof(aux->undo_selected_peg));
	aux->undo_depth = 0;
}

void lf52_build_level(struct arc_scene_game *game)
{
	const struct lf52_static *st =
		(const struct lf52_static *)game->statics;
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	int32_t level = game->engine.level_index;
	struct arc_scene_table *scene = &game->scene;

	for (int32_t i = 0; i < LF52_STATIC_SLOT_COUNT; i++) {
		size_t idx = (size_t)level * LF52_STATIC_SLOT_COUNT + i;
		scene->image[i] = st->static_image[idx];
		scene->x[i] = st->static_x[idx] + st->offset_x[level];
		scene->y[i] = st->static_y[idx] + st->offset_y[level];
		scene->layer[i] = st->static_layer[idx];
	}

	lf52_zero_aux(aux);
	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		size_t idx = (size_t)level * LF52_PEG_SLOT_COUNT + i;
		aux->peg_grid_x[i] = st->peg_grid_x_initial[idx];
		aux->peg_grid_y[i] = st->peg_grid_y_initial[idx];
		aux->peg_alive[i] = st->peg_alive_initial[idx];
	}
	for (int32_t i = 0; i < LF52_WALL_TILE_SLOT_COUNT; i++) {
		size_t idx = (size_t)level * LF52_WALL_TILE_SLOT_COUNT + i;
		aux->wall_tile_grid_x[i] = st->wall_tile_grid_x_initial[idx];
		aux->wall_tile_grid_y[i] = st->wall_tile_grid_y_initial[idx];
	}
	aux->camera_offset_x = st->offset_x[level];
	aux->camera_offset_y = st->offset_y[level];

	for (int32_t i = 0; i < LF52_PEG_SLOT_COUNT; i++) {
		size_t idx = (size_t)level * LF52_PEG_SLOT_COUNT + i;
		int32_t color = st->peg_color[idx];
		scene->image[LF52_P0 + i] =
			aux->peg_alive[i] ? st->peg_base_image[color] : -1;
		scene->x[LF52_P0 + i] =
			aux->peg_grid_x[i] * LF52_PITCH + st->offset_x[level];
		scene->y[LF52_P0 + i] =
			aux->peg_grid_y[i] * LF52_PITCH + st->offset_y[level];
		scene->layer[LF52_P0 + i] = 1;
	}

	for (int32_t i = 0; i < LF52_HEART_SLOT_COUNT; i++) {
		scene->image[LF52_H0 + i] = -1;
		scene->layer[LF52_H0 + i] = 1;
	}
	scene->image[LF52_RING_SLOT] = -1;
	scene->layer[LF52_RING_SLOT] = 1;
	scene->image[LF52_DUST_SLOT] = -1;
	scene->layer[LF52_DUST_SLOT] = 11;
	scene->image[LF52_REVEAL_BUTTON_SETTLED_SLOT] = -1;
	scene->layer[LF52_REVEAL_BUTTON_SETTLED_SLOT] = 10;
	scene->image[LF52_REVEAL_BUTTON_RISING_SLOT] = -1;
	scene->layer[LF52_REVEAL_BUTTON_RISING_SLOT] = 10;
	scene->image[LF52_JUMP_TRAIL_GHOST_SLOT] = -1;
	scene->layer[LF52_JUMP_TRAIL_GHOST_SLOT] = LF52_JUMP_TRAIL_GHOST_LAYER;

	arc_scene_touch(scene);
}

void lf52_step_once(struct arc_scene_game *game)
{
	struct lf52_aux *aux = (struct lf52_aux *)game->aux;
	if (aux->action_in_progress)
		lf52_advance_action(game);
	else
		lf52_dispatch_action(game);
}

void lf52_render_interface(struct arc_scene_game *game, int8_t *frame)
{
	static const int8_t palette[13] = { 0, 1, 2, 3, 4, 5, 5,
					    0, 1, 2, 3, 4, 5 };
	int32_t count =
		lf52_clampi(game->engine.action_count, 0, ARC_FRAME_SIZE * 5);
	int32_t bucket = lf52_clampi(count / ARC_FRAME_SIZE, 0, 8);
	int32_t remainder = count % ARC_FRAME_SIZE;
	for (int32_t c = 0; c < ARC_FRAME_SIZE; c++) {
		int8_t sweep =
			c < remainder ? palette[bucket + 1] : palette[bucket];
		int8_t tail = c >= remainder ? palette[11] : frame[c];
		int8_t value;
		if (count >= ARC_FRAME_SIZE * 5)
			value = frame[c];
		else if (count >= ARC_FRAME_SIZE * 4)
			value = tail;
		else
			value = sweep;
		frame[c] = value;
	}
}
