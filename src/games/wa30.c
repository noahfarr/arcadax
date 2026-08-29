#include "wa30.h"

#include <stdlib.h>
#include <string.h>

enum { WA30_PITCH = 4, WA30_GRID_N = 16, WA30_NUM_CELLS = 256 };
enum {
	WA30_ACTION_RESET = 0,
	WA30_ACTION1 = 1,
	WA30_ACTION2 = 2,
	WA30_ACTION3 = 3,
	WA30_ACTION4 = 4
};
enum {
	WA30_ROT_UP = 0,
	WA30_ROT_RIGHT = 90,
	WA30_ROT_DOWN = 180,
	WA30_ROT_LEFT = 270
};
enum {
	WA30_BOX_HELD_BY_PLAYER = 0,
	WA30_BOX_FACED = 3,
	WA30_BOX_IDLE = 4,
	WA30_BOX_HELD = 5
};
enum { WA30_THIEF_FACED = 11, WA30_THIEF_IDLE = 15 };
enum { WA30_STATE_WIN = 2, WA30_STATE_GAME_OVER = 3 };

static inline int32_t wa30_clamp(int32_t v, int32_t lo, int32_t hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t wa30_floordiv(int32_t a, int32_t b)
{
	int32_t q = a / b, r = a % b;
	return (r != 0 && ((r < 0) != (b < 0))) ? q - 1 : q;
}

static void wa30_blocked_grid(const struct arc_sprites *s, int32_t num_slots,
			      uint8_t blocked[WA30_NUM_CELLS])
{
	memset(blocked, 0, WA30_NUM_CELLS);
	for (int32_t i = 0; i < num_slots; i++) {
		if (!arc_sprite_collidable(s, i))
			continue;
		int32_t cy = wa30_clamp(wa30_floordiv(s->y[i], WA30_PITCH), 0,
					WA30_GRID_N - 1);
		int32_t cx = wa30_clamp(wa30_floordiv(s->x[i], WA30_PITCH), 0,
					WA30_GRID_N - 1);
		blocked[cy * WA30_GRID_N + cx] = 1;
	}
}

static void wa30_cell_state(int32_t x, int32_t y,
			    const uint8_t blocked[WA30_NUM_CELLS],
			    const uint8_t hole[WA30_NUM_CELLS], int *is_blocked,
			    int *is_hole)
{
	int in_bounds =
		x >= 0 && x < ARC_FRAME_SIZE && y >= 0 && y < ARC_FRAME_SIZE;
	int32_t cy =
		wa30_clamp(wa30_floordiv(y, WA30_PITCH), 0, WA30_GRID_N - 1);
	int32_t cx =
		wa30_clamp(wa30_floordiv(x, WA30_PITCH), 0, WA30_GRID_N - 1);
	*is_blocked = in_bounds ? blocked[cy * WA30_GRID_N + cx] : 1;
	*is_hole = in_bounds ? hole[cy * WA30_GRID_N + cx] : 0;
}

static void wa30_bfs_first_step(const uint8_t passable[WA30_NUM_CELLS],
				const uint8_t goal[WA30_NUM_CELLS],
				int32_t source_cy, int32_t source_cx,
				int *valid, int32_t *out_cy, int32_t *out_cx)
{
	static const int32_t DX[4] = { -1, 1, 0, 0 };
	static const int32_t DY[4] = { 0, 0, -1, 1 };
	int32_t sc_y = wa30_clamp(source_cy, 0, WA30_GRID_N - 1);
	int32_t sc_x = wa30_clamp(source_cx, 0, WA30_GRID_N - 1);
	int32_t source_id = sc_y * WA30_GRID_N + sc_x;

	int32_t queue[WA30_NUM_CELLS];
	uint8_t visited[WA30_NUM_CELLS];
	int32_t parent[WA30_NUM_CELLS];
	memset(visited, 0, sizeof visited);
	for (int32_t i = 0; i < WA30_NUM_CELLS; i++)
		parent[i] = -1;

	queue[0] = source_id;
	visited[source_id] = 1;
	int32_t head = 0, tail = 1;
	int found = 0;
	int32_t found_id = -1;

	while (head < tail) {
		int32_t current = queue[head++];
		if (goal[current]) {
			found = 1;
			found_id = current;
			break;
		}
		int32_t cy = current / WA30_GRID_N, cx = current % WA30_GRID_N;
		for (int d = 0; d < 4; d++) {
			int32_t ncy = cy + DY[d], ncx = cx + DX[d];
			if (ncy < 0 || ncy >= WA30_GRID_N || ncx < 0 ||
			    ncx >= WA30_GRID_N)
				continue;
			int32_t nid = ncy * WA30_GRID_N + ncx;
			if (!passable[nid] || visited[nid])
				continue;
			visited[nid] = 1;
			parent[nid] = current;
			queue[tail++] = nid;
		}
	}

	if (!found || found_id == source_id) {
		*valid = 0;
		*out_cy = 0;
		*out_cx = 0;
		return;
	}
	int32_t cur = found_id;
	while (parent[cur] != source_id)
		cur = parent[cur];
	*valid = 1;
	*out_cy = cur / WA30_GRID_N;
	*out_cx = cur % WA30_GRID_N;
}

static void wa30_bfs_drag(const struct arc_sprites *s,
			  const struct wa30_static *st, int32_t level,
			  const uint8_t target_grid[WA30_NUM_CELLS],
			  int32_t actor_slot, int32_t box_slot, int *valid,
			  int32_t *out_cy, int32_t *out_cx)
{
	uint8_t blocked[WA30_NUM_CELLS];
	wa30_blocked_grid(s, st->num_slots, blocked);
	const uint8_t *hole = st->hole_grid + (size_t)level * WA30_NUM_CELLS;

	int32_t actor_cy = wa30_floordiv(s->y[actor_slot], WA30_PITCH);
	int32_t actor_cx = wa30_floordiv(s->x[actor_slot], WA30_PITCH);
	int32_t box_cy = wa30_floordiv(s->y[box_slot], WA30_PITCH);
	int32_t box_cx = wa30_floordiv(s->x[box_slot], WA30_PITCH);
	int32_t doff_y = box_cy - actor_cy, doff_x = box_cx - actor_cx;

	uint8_t passable[WA30_NUM_CELLS];
	uint8_t goal[WA30_NUM_CELLS];
	for (int32_t cy = 0; cy < WA30_GRID_N; cy++) {
		for (int32_t cx = 0; cx < WA30_GRID_N; cx++) {
			int32_t idx = cy * WA30_GRID_N + cx;
			int v_eq_box = cy == box_cy && cx == box_cx;
			int target_eq_actor = (cy + doff_y) == actor_cy &&
					      (cx + doff_x) == actor_cx;
			int32_t t_cy = cy + doff_y, t_cx = cx + doff_x;
			int in_range = t_cy >= 0 && t_cy < WA30_GRID_N &&
				       t_cx >= 0 && t_cx < WA30_GRID_N;
			int32_t t_cy_c = wa30_clamp(t_cy, 0, WA30_GRID_N - 1);
			int32_t t_cx_c = wa30_clamp(t_cx, 0, WA30_GRID_N - 1);
			int blocked_shifted =
				in_range ?
					blocked[t_cy_c * WA30_GRID_N + t_cx_c] :
					1;
			int goal_shifted =
				in_range ? target_grid[t_cy_c * WA30_GRID_N +
						       t_cx_c] :
					   0;
			passable[idx] =
				(uint8_t)(((!blocked[idx]) || v_eq_box) &&
					  !hole[idx] &&
					  ((!blocked_shifted) ||
					   target_eq_actor));
			goal[idx] = (uint8_t)goal_shifted;
		}
	}
	wa30_bfs_first_step(passable, goal, actor_cy, actor_cx, valid, out_cy,
			    out_cx);
}

static void wa30_move_actor(struct arc_sprites *s, const struct wa30_static *st,
			    int32_t level, struct wa30_aux *aux,
			    int32_t actor_slot, int32_t dx, int32_t dy)
{
	int32_t ax = s->x[actor_slot], ay = s->y[actor_slot];
	int32_t tx = ax + dx, ty = ay + dy;
	int32_t box_slot = aux->partner[actor_slot];

	uint8_t blocked[WA30_NUM_CELLS];
	wa30_blocked_grid(s, st->num_slots, blocked);
	const uint8_t *hole = st->hole_grid + (size_t)level * WA30_NUM_CELLS;

	if (box_slot < 0) {
		int is_blocked, is_hole;
		wa30_cell_state(tx, ty, blocked, hole, &is_blocked, &is_hole);
		if (!is_blocked && !is_hole)
			arc_set_position(s, actor_slot, tx, ty);
		return;
	}

	int32_t bx = s->x[box_slot], by = s->y[box_slot];
	int32_t box_dx = bx - ax, box_dy = by - ay;
	int32_t tbx = tx + box_dx, tby = ty + box_dy;

	int v_blocked, v_hole;
	wa30_cell_state(tx, ty, blocked, hole, &v_blocked, &v_hole);
	int v_ok = (!v_blocked || (tx == bx && ty == by)) && !v_hole;

	int t_blocked, t_hole_unused;
	wa30_cell_state(tbx, tby, blocked, hole, &t_blocked, &t_hole_unused);
	int t_ok = !t_blocked || (tbx == ax && tby == ay);

	if (v_ok && t_ok) {
		arc_set_position(s, actor_slot, tx, ty);
		arc_set_position(s, box_slot, tbx, tby);
	}
}

static int32_t wa30_rotation_for(int32_t dx, int32_t dy)
{
	if (dy < 0)
		return WA30_ROT_UP;
	if (dx > 0)
		return WA30_ROT_RIGHT;
	if (dy > 0)
		return WA30_ROT_DOWN;
	return WA30_ROT_LEFT;
}

static void wa30_apply_player_variant(struct arc_sprites *s,
				      const struct wa30_static *st,
				      int32_t level, int32_t player,
				      int32_t rotation)
{
	int32_t rot_idx = rotation / 90;
	int32_t pw = s->atlas->pw;
	const int8_t *variant =
		st->player_variants + (((size_t)level * 4 + (size_t)rot_idx) *
				       WA30_PITCH * WA30_PITCH);
	int8_t *patch = arc_sprite_pixels_mut(s, player);
	for (int32_t row = 0; row < WA30_PITCH; row++)
		for (int32_t col = 0; col < WA30_PITCH; col++)
			patch[row * pw + col] = variant[row * WA30_PITCH + col];
}

static void wa30_move_player(struct arc_sprites *s,
			     const struct wa30_static *st, int32_t level,
			     struct wa30_aux *aux, int32_t dx, int32_t dy)
{
	int32_t player = st->player_slot[level];
	int holding = aux->partner[player] >= 0;
	int32_t rotation = holding ? aux->rotation : wa30_rotation_for(dx, dy);
	aux->rotation = rotation;
	wa30_apply_player_variant(s, st, level, player, rotation);
	wa30_move_actor(s, st, level, aux, player, dx, dy);
}

static void wa30_pair(struct wa30_aux *aux, int32_t actor_slot,
		      int32_t box_slot)
{
	int32_t old_actor = aux->partner[box_slot];
	if (old_actor >= 0)
		aux->partner[old_actor] = -1;
	aux->partner[box_slot] = actor_slot;
	aux->partner[actor_slot] = box_slot;
}

static void wa30_unpair(struct wa30_aux *aux, int32_t actor_slot)
{
	int32_t other = aux->partner[actor_slot];
	if (other >= 0) {
		aux->partner[other] = -1;
		aux->partner[actor_slot] = -1;
	}
}

static void wa30_front_cell(const struct arc_sprites *s,
			    const struct wa30_static *st, int32_t level,
			    const struct wa30_aux *aux, int32_t *fx,
			    int32_t *fy)
{
	int32_t player = st->player_slot[level];
	int32_t rotation = aux->rotation;
	int32_t fdx = rotation == WA30_ROT_RIGHT ?
			      WA30_PITCH :
			      (rotation == WA30_ROT_LEFT ? -WA30_PITCH : 0);
	int32_t fdy = rotation == WA30_ROT_DOWN ?
			      WA30_PITCH :
			      (rotation == WA30_ROT_UP ? -WA30_PITCH : 0);
	*fx = s->x[player] + fdx;
	*fy = s->y[player] + fdy;
}

static void wa30_action5(struct arc_sprites *s, const struct wa30_static *st,
			 int32_t level, struct wa30_aux *aux)
{
	int32_t player = st->player_slot[level];
	if (aux->partner[player] >= 0) {
		wa30_unpair(aux, player);
		return;
	}

	int32_t fx, fy;
	wa30_front_cell(s, st, level, aux, &fx, &fy);
	const uint8_t *is_box = st->is_box + (size_t)level * st->num_slots;
	const uint8_t *is_thief = st->is_thief + (size_t)level * st->num_slots;

	int32_t box_slot = -1;
	for (int32_t j = 0; j < st->num_slots; j++) {
		if (is_box[j] && s->alive[j] && s->x[j] == fx &&
		    s->y[j] == fy) {
			box_slot = j;
			break;
		}
	}
	if (box_slot >= 0)
		wa30_pair(aux, player, box_slot);

	int32_t thief_slot = -1;
	for (int32_t j = 0; j < st->num_slots; j++) {
		if (is_thief[j] && s->alive[j] && s->x[j] == fx &&
		    s->y[j] == fy) {
			thief_slot = j;
			break;
		}
	}
	if (thief_slot >= 0) {
		wa30_unpair(aux, thief_slot);
		arc_remove_sprite(s, thief_slot);
	}
}

static int wa30_box_eligible(const struct wa30_static *st, int32_t level,
			     const struct wa30_aux *aux, int32_t slot,
			     int is_seeker)
{
	int32_t partner = aux->partner[slot];
	int has_partner = partner >= 0;
	if (is_seeker)
		return !has_partner;
	if (!has_partner)
		return 1;
	int32_t idx = wa30_clamp(partner, 0, st->num_slots - 1);
	int partner_is_thief =
		st->is_thief[(size_t)level * st->num_slots + idx];
	return !partner_is_thief;
}

static void wa30_adjacency_goal_grid(const struct arc_sprites *s,
				     const struct wa30_static *st,
				     int32_t level, const struct wa30_aux *aux,
				     int is_seeker,
				     const uint8_t target_grid[WA30_NUM_CELLS],
				     uint8_t goal[WA30_NUM_CELLS])
{
	static const int32_t DX[4] = { -1, 1, 0, 0 };
	static const int32_t DY[4] = { 0, 0, -1, 1 };
	memset(goal, 0, WA30_NUM_CELLS);
	const uint8_t *is_box = st->is_box + (size_t)level * st->num_slots;
	for (int32_t j = 0; j < st->num_slots; j++) {
		if (!(is_box[j] && s->alive[j]))
			continue;
		if (!wa30_box_eligible(st, level, aux, j, is_seeker))
			continue;
		int32_t cy = wa30_clamp(wa30_floordiv(s->y[j], WA30_PITCH), 0,
					WA30_GRID_N - 1);
		int32_t cx = wa30_clamp(wa30_floordiv(s->x[j], WA30_PITCH), 0,
					WA30_GRID_N - 1);
		if (target_grid[cy * WA30_GRID_N + cx])
			continue;
		for (int d = 0; d < 4; d++) {
			int32_t ny = cy + DY[d], nx = cx + DX[d];
			if (ny < 0 || ny >= WA30_GRID_N || nx < 0 ||
			    nx >= WA30_GRID_N)
				continue;
			goal[ny * WA30_GRID_N + nx] = 1;
		}
	}
}

static int wa30_process_one_actor(struct arc_sprites *s,
				  const struct wa30_static *st, int32_t level,
				  struct wa30_aux *aux, int32_t actor_slot,
				  int is_seeker,
				  const uint8_t target_grid[WA30_NUM_CELLS])
{
	int32_t box_slot = aux->partner[actor_slot];
	if (box_slot >= 0) {
		int32_t bcy =
			wa30_clamp(wa30_floordiv(s->y[box_slot], WA30_PITCH), 0,
				   WA30_GRID_N - 1);
		int32_t bcx =
			wa30_clamp(wa30_floordiv(s->x[box_slot], WA30_PITCH), 0,
				   WA30_GRID_N - 1);
		if (target_grid[bcy * WA30_GRID_N + bcx]) {
			wa30_unpair(aux, actor_slot);
		} else {
			int valid;
			int32_t step_cy, step_cx;
			wa30_bfs_drag(s, st, level, target_grid, actor_slot,
				      box_slot, &valid, &step_cy, &step_cx);
			if (valid) {
				int32_t cur_x = s->x[actor_slot],
					cur_y = s->y[actor_slot];
				int32_t dx = step_cx * WA30_PITCH - cur_x,
					dy = step_cy * WA30_PITCH - cur_y;
				wa30_move_actor(s, st, level, aux, actor_slot,
						dx, dy);
			}
		}
		return 0;
	}

	const uint8_t *is_box = st->is_box + (size_t)level * st->num_slots;
	int32_t acy = wa30_floordiv(s->y[actor_slot], WA30_PITCH);
	int32_t acx = wa30_floordiv(s->x[actor_slot], WA30_PITCH);
	int32_t first_box = -1;
	for (int32_t j = 0; j < st->num_slots; j++) {
		if (!(is_box[j] && s->alive[j]))
			continue;
		int32_t bcy = wa30_floordiv(s->y[j], WA30_PITCH);
		int32_t bcx = wa30_floordiv(s->x[j], WA30_PITCH);
		if (abs(bcy - acy) + abs(bcx - acx) != 1)
			continue;
		int32_t tcy = wa30_clamp(bcy, 0, WA30_GRID_N - 1);
		int32_t tcx = wa30_clamp(bcx, 0, WA30_GRID_N - 1);
		if (target_grid[tcy * WA30_GRID_N + tcx])
			continue;
		if (!wa30_box_eligible(st, level, aux, j, is_seeker))
			continue;
		first_box = j;
		break;
	}
	if (first_box >= 0) {
		wa30_pair(aux, actor_slot, first_box);
		return 1;
	}

	uint8_t blocked[WA30_NUM_CELLS];
	wa30_blocked_grid(s, st->num_slots, blocked);
	const uint8_t *hole = st->hole_grid + (size_t)level * WA30_NUM_CELLS;
	uint8_t passable[WA30_NUM_CELLS];
	for (int32_t k = 0; k < WA30_NUM_CELLS; k++)
		passable[k] = (uint8_t)(!blocked[k] && !hole[k]);
	uint8_t goal[WA30_NUM_CELLS];
	wa30_adjacency_goal_grid(s, st, level, aux, is_seeker, target_grid,
				 goal);

	int valid;
	int32_t step_cy, step_cx;
	wa30_bfs_first_step(passable, goal, acy, acx, &valid, &step_cy,
			    &step_cx);
	if (valid) {
		int32_t cur_x = s->x[actor_slot], cur_y = s->y[actor_slot];
		int32_t dx = step_cx * WA30_PITCH - cur_x,
			dy = step_cy * WA30_PITCH - cur_y;
		wa30_move_actor(s, st, level, aux, actor_slot, dx, dy);
	}
	return 0;
}

static void wa30_actor_phase(struct arc_sprites *s,
			     const struct wa30_static *st, int32_t level,
			     struct wa30_aux *aux, int is_seeker)
{
	const uint8_t *tag_mask = (is_seeker ? st->is_seeker : st->is_thief) +
				  (size_t)level * st->num_slots;
	const uint8_t *target_grid = (is_seeker ? st->fsj_grid : st->zqx_grid) +
				     (size_t)level * WA30_NUM_CELLS;
	for (int32_t i = 0; i < st->num_slots; i++) {
		if (!(tag_mask[i] && s->alive[i]))
			continue;
		if (wa30_process_one_actor(s, st, level, aux, i, is_seeker,
					   target_grid))
			break;
	}
}

static void wa30_paint_border(struct arc_sprites *s, int32_t i, int8_t colour)
{
	int32_t pw = s->atlas->pw;
	int8_t *patch = arc_sprite_pixels_mut(s, i);
	for (int32_t row = 0; row < WA30_PITCH; row++) {
		for (int32_t col = 0; col < WA30_PITCH; col++) {
			if (row == 0 || row == WA30_PITCH - 1 || col == 0 ||
			    col == WA30_PITCH - 1)
				patch[row * pw + col] = colour;
		}
	}
}

static void wa30_recolour(struct arc_sprites *s, const struct wa30_static *st,
			  int32_t level, const struct wa30_aux *aux)
{
	int32_t player = st->player_slot[level];
	int32_t fx, fy;
	wa30_front_cell(s, st, level, aux, &fx, &fy);
	const uint8_t *is_box = st->is_box + (size_t)level * st->num_slots;
	const uint8_t *is_thief = st->is_thief + (size_t)level * st->num_slots;

	for (int32_t i = 0; i < st->num_slots; i++) {
		int box = is_box[i], thief = is_thief[i];
		if (!(box || thief) || !s->alive[i])
			continue;
		int in_front = s->x[i] == fx && s->y[i] == fy;
		int8_t colour;
		if (box) {
			int32_t partner = aux->partner[i];
			int has_partner = partner >= 0;
			int held_by_player = has_partner && partner == player;
			if (held_by_player)
				colour = WA30_BOX_HELD_BY_PLAYER;
			else if (has_partner && in_front)
				colour = WA30_BOX_FACED;
			else if (has_partner)
				colour = WA30_BOX_HELD;
			else if (in_front)
				colour = WA30_BOX_FACED;
			else
				colour = WA30_BOX_IDLE;
		} else {
			colour = in_front ? WA30_THIEF_FACED : WA30_THIEF_IDLE;
		}
		wa30_paint_border(s, i, colour);
	}
}

static void wa30_run_ai(struct arc_sprites *s, const struct wa30_static *st,
			int32_t level, struct wa30_aux *aux)
{
	wa30_actor_phase(s, st, level, aux, 1);
	wa30_actor_phase(s, st, level, aux, 0);
	wa30_recolour(s, st, level, aux);
}

static void wa30_play(struct arc_sprites *s, const struct wa30_static *st,
		      int32_t level, int32_t action_id, struct wa30_aux *aux)
{
	aux->steps = aux->steps > 0 ? aux->steps - 1 : 0;
	if (action_id >= WA30_ACTION1 && action_id <= WA30_ACTION4) {
		int32_t dx = 0, dy = 0;
		if (action_id == WA30_ACTION3)
			dx = -WA30_PITCH;
		else if (action_id == WA30_ACTION4)
			dx = WA30_PITCH;
		if (action_id == WA30_ACTION1)
			dy = -WA30_PITCH;
		else if (action_id == WA30_ACTION2)
			dy = WA30_PITCH;
		wa30_move_player(s, st, level, aux, dx, dy);
	} else {
		wa30_action5(s, st, level, aux);
	}
	wa30_run_ai(s, st, level, aux);
}

static int wa30_check_win(const struct arc_sprites *s,
			  const struct wa30_static *st, int32_t level,
			  const struct wa30_aux *aux)
{
	const uint8_t *is_box = st->is_box + (size_t)level * st->num_slots;
	const uint8_t *fsj = st->fsj_grid + (size_t)level * WA30_NUM_CELLS;
	for (int32_t i = 0; i < st->num_slots; i++) {
		if (!(is_box[i] && s->alive[i]))
			continue;
		int32_t cy = wa30_clamp(wa30_floordiv(s->y[i], WA30_PITCH), 0,
					WA30_GRID_N - 1);
		int32_t cx = wa30_clamp(wa30_floordiv(s->x[i], WA30_PITCH), 0,
					WA30_GRID_N - 1);
		int on_target = fsj[cy * WA30_GRID_N + cx];
		int unpaired = aux->partner[i] < 0;
		if (!(on_target && unpaired))
			return 0;
	}
	return 1;
}

static void wa30_next_level(const struct wa30_static *st, int32_t level,
			    int32_t *score, int32_t *status,
			    uint8_t *next_level)
{
	int is_last = level == st->num_levels - 1;
	*score += 1;
	*next_level = (uint8_t)!is_last;
	if (is_last)
		*status = WA30_STATE_WIN;
}

void wa30_aux_alloc(struct wa30_aux *aux, int32_t num_slots)
{
	aux->partner = calloc((size_t)num_slots, sizeof(int32_t));
	aux->rotation = WA30_ROT_UP;
	aux->steps = 0;
}

void wa30_aux_free(struct wa30_aux *aux)
{
	free(aux->partner);
	aux->partner = NULL;
}

void wa30_zero_aux(struct wa30_aux *aux, const struct wa30_static *st)
{
	for (int32_t i = 0; i < st->num_slots; i++)
		aux->partner[i] = -1;
	aux->rotation = WA30_ROT_UP;
	aux->steps = 0;
}

void wa30_on_set_level(const struct wa30_static *st, int32_t level,
		       struct wa30_aux *aux)
{
	for (int32_t i = 0; i < st->num_slots; i++)
		aux->partner[i] = -1;
	aux->rotation = WA30_ROT_UP;
	aux->steps = st->budget[level];
}

void wa30_step_once(struct arc_sprites *sprites, const struct wa30_static *st,
		    int32_t level, int32_t action_id, struct wa30_aux *aux,
		    int32_t *score, int32_t *status, uint8_t *next_level,
		    uint8_t *action_complete)
{
	if (action_id != WA30_ACTION_RESET)
		wa30_play(sprites, st, level, action_id, aux);

	if (wa30_check_win(sprites, st, level, aux)) {
		wa30_next_level(st, level, score, status, next_level);
	} else if (aux->steps <= 0) {
		*status = WA30_STATE_GAME_OVER;
	}
	*action_complete = 1;
}

void wa30_render_interface(int8_t *frame, const struct wa30_static *st,
			   int32_t level, const struct wa30_aux *aux)
{
	int32_t budget = st->budget[level];
	if (budget == 0)
		return;
	int32_t safe_budget = budget > 0 ? budget : 1;
	int32_t total = ARC_FRAME_SIZE * aux->steps;
	int32_t whole = total / safe_budget, rest = total % safe_budget;
	int round_up = (2 * rest > safe_budget) ||
		       (2 * rest == safe_budget && (whole % 2 == 1));
	int32_t filled = whole + (round_up ? 1 : 0);
	int8_t *row = frame + (size_t)(ARC_FRAME_SIZE - 1) * ARC_FRAME_SIZE;
	for (int32_t c = 0; c < ARC_FRAME_SIZE; c++)
		row[c] = (int8_t)(c < filled ? 7 : 4);
}
