#include <string.h>

#include "arc/dsl.h"

static const int32_t DX[4] = { 0, 0, -1, 1 };
static const int32_t DY[4] = { -1, 1, 0, 0 };

static const struct arc_dsl_spec *spec_of(const struct arc_game *game)
{
	return (const struct arc_dsl_spec *)game->statics;
}

static int32_t idx(const struct arc_dsl_spec *s, int32_t x, int32_t y)
{
	return y * s->grid_w + x;
}

static int inside(const struct arc_dsl_spec *s, int32_t x, int32_t y)
{
	return x >= 0 && y >= 0 && x < s->grid_w && y < s->grid_h;
}

void arc_dsl_zero_aux(void *aux)
{
	memset(aux, 0, sizeof(struct arc_dsl_aux));
}

int32_t arc_dsl_num_actions(const struct arc_dsl_spec *spec)
{
	return spec->grid_w * spec->grid_h;
}

static void load_level(struct arc_game *game)
{
	const struct arc_dsl_spec *s = spec_of(game);
	struct arc_dsl_aux *aux = (struct arc_dsl_aux *)game->aux;
	int32_t level = game->engine.level_index;
	int32_t n = s->grid_w * s->grid_h;
	const int8_t *src = s->layout + (size_t)level * n;

	memcpy(aux->grid, src, (size_t)n);
	memcpy(aux->floor, s->floor + (size_t)level * n, (size_t)n);
	aux->player_x = -1;
	aux->player_y = -1;
	for (int32_t y = 0; y < s->grid_h; y++) {
		for (int32_t x = 0; x < s->grid_w; x++) {
			if (aux->grid[idx(s, x, y)] == (int8_t)s->player_kind) {
				aux->player_x = x;
				aux->player_y = y;
			}
		}
	}
	aux->settled = 0;
}

static int won(const struct arc_game *game)
{
	const struct arc_dsl_spec *s = spec_of(game);
	const struct arc_dsl_aux *aux = (const struct arc_dsl_aux *)game->aux;
	int32_t n = s->grid_w * s->grid_h;

	if (s->win_mode == ARC_DSL_WIN_NONE_LEFT) {
		for (int32_t i = 0; i < n; i++)
			if (aux->grid[i] == (int8_t)s->win_a)
				return 0;
		return 1;
	}
	if (s->win_mode == ARC_DSL_WIN_ALL_ON) {
		for (int32_t i = 0; i < n; i++)
			if (aux->floor[i] == (int8_t)s->win_b &&
			    aux->grid[i] != (int8_t)s->win_a)
				return 0;
		return 1;
	}
	if (s->win_mode == ARC_DSL_WIN_REACH)
		return aux->floor[idx(s, aux->player_x, aux->player_y)] ==
		       (int8_t)s->win_a;
	return 0;
}

static void apply(struct arc_game *game, int32_t cx, int32_t cy, uint8_t effect,
		  int8_t a, int8_t b, int *blocked)
{
	const struct arc_dsl_spec *s = spec_of(game);
	struct arc_dsl_aux *aux = (struct arc_dsl_aux *)game->aux;
	int32_t n = s->grid_w * s->grid_h;

	switch (effect) {
	case ARC_DSL_BLOCK:
		*blocked = 1;
		break;
	case ARC_DSL_REMOVE:
		aux->grid[idx(s, cx, cy)] = ARC_DSL_EMPTY;
		break;
	case ARC_DSL_BECOME:
		aux->grid[idx(s, cx, cy)] = a;
		break;
	case ARC_DSL_TOGGLE:
		for (int32_t i = 0; i < n; i++) {
			if (aux->grid[i] == a)
				aux->grid[i] = b;
			else if (aux->grid[i] == b)
				aux->grid[i] = a;
		}
		break;
	case ARC_DSL_WIN:
		arc_game_next_level(game);
		break;
	case ARC_DSL_LOSE:
		arc_game_lose(game);
		break;
	default:
		break;
	}
}

static void try_move(struct arc_game *game, int32_t dir)
{
	const struct arc_dsl_spec *s = spec_of(game);
	struct arc_dsl_aux *aux = (struct arc_dsl_aux *)game->aux;
	int32_t nx = aux->player_x + DX[dir];
	int32_t ny = aux->player_y + DY[dir];
	int blocked = 0;
	int8_t target;

	if (!inside(s, nx, ny))
		return;
	target = aux->grid[idx(s, nx, ny)];

	if (target != ARC_DSL_EMPTY) {
		const struct arc_dsl_kind *k = &s->kinds[target];

		if (k->on_enter == ARC_DSL_PUSH) {
			int32_t px = nx + DX[dir];
			int32_t py = ny + DY[dir];
			int8_t beyond;

			if (!inside(s, px, py))
				return;
			beyond = aux->grid[idx(s, px, py)];
			if (beyond != ARC_DSL_EMPTY)
				return;
			aux->grid[idx(s, px, py)] = target;
			aux->grid[idx(s, nx, ny)] = ARC_DSL_EMPTY;
		} else {
			apply(game, nx, ny, k->on_enter, k->enter_a, k->enter_b,
			      &blocked);
			if (blocked)
				return;
		}
	}

	aux->grid[idx(s, aux->player_x, aux->player_y)] = ARC_DSL_EMPTY;
	aux->player_x = nx;
	aux->player_y = ny;
	aux->grid[idx(s, nx, ny)] = (int8_t)s->player_kind;
}

static void do_click(struct arc_game *game, int32_t ax, int32_t ay)
{
	const struct arc_dsl_spec *s = spec_of(game);
	struct arc_dsl_aux *aux = (struct arc_dsl_aux *)game->aux;
	int32_t cx = (ax - s->origin_x) / s->pitch;
	int32_t cy = (ay - s->origin_y) / s->pitch;
	int blocked = 0;
	int8_t target;

	if (ax < s->origin_x || ay < s->origin_y || !inside(s, cx, cy))
		return;
	target = aux->grid[idx(s, cx, cy)];
	if (target == ARC_DSL_EMPTY)
		return;
	apply(game, cx, cy, s->kinds[target].on_click, s->kinds[target].click_a,
	      s->kinds[target].click_b, &blocked);
}

static void dsl_on_set_level(struct arc_game *game)
{
	load_level(game);
}

static void dsl_step_once(struct arc_game *game)
{
	struct arc_engine_state *e = &game->engine;
	int32_t id = e->action_id;

	if (id >= ARC_ACTION1 && id <= ARC_ACTION4)
		try_move(game, id - ARC_ACTION1);
	else if (id == ARC_ACTION6)
		do_click(game, e->action_x, e->action_y);

	if (e->status == NOT_FINISHED && won(game))
		arc_game_next_level(game);
	arc_game_complete_action(game);
}

static void dsl_render_interface(struct arc_game *game, int8_t *frame)
{
	const struct arc_dsl_spec *s = spec_of(game);
	const struct arc_dsl_aux *aux = (const struct arc_dsl_aux *)game->aux;

	for (int32_t y = 0; y < s->grid_h; y++) {
		for (int32_t x = 0; x < s->grid_w; x++) {
			int8_t kind = aux->grid[idx(s, x, y)];
			int8_t color;
			int32_t x0, y0;

			if (kind == ARC_DSL_EMPTY)
				kind = aux->floor[idx(s, x, y)];
			if (kind == ARC_DSL_EMPTY)
				continue;
			color = s->kinds[kind].color;
			x0 = s->origin_x + x * s->pitch;
			y0 = s->origin_y + y * s->pitch;
			for (int32_t v = 0; v < s->pitch; v++) {
				int32_t row = y0 + v;

				if (row < 0 || row >= ARC_FRAME_SIZE)
					continue;
				for (int32_t u = 0; u < s->pitch; u++) {
					int32_t col = x0 + u;

					if (col < 0 || col >= ARC_FRAME_SIZE)
						continue;
					frame[row * ARC_FRAME_SIZE + col] =
						color;
				}
			}
		}
	}
}

const struct arc_hooks arc_dsl_hooks = { arc_dsl_zero_aux, dsl_on_set_level,
					 dsl_step_once, dsl_render_interface };
