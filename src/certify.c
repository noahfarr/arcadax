#include <pthread.h>
#include <stdlib.h>

#include "arc/certify.h"

struct worker {
	struct arc_game *game;
	const int32_t *actions;
	int32_t num_actions;
	int32_t trials;
	int32_t horizon;
	int32_t start_level;
	uint32_t seed;
	int64_t wins;
	int64_t steps;
};

static uint32_t next_random(uint32_t *state)
{
	uint32_t x = *state;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

static void *run(void *arg)
{
	struct worker *w = (struct worker *)arg;
	uint32_t state = w->seed ? w->seed : 1u;
	int8_t frame[ARC_FRAME_SIZE * ARC_FRAME_SIZE];

	for (int32_t t = 0; t < w->trials; t++) {
		int32_t start;

		arc_game_init(w->game);
		if (w->start_level > 0)
			arc_game_set_level(w->game, w->start_level);
		start = w->game->engine.level_index;
		for (int32_t i = 0; i < w->horizon; i++) {
			int32_t k = (int32_t)(next_random(&state) %
					      (uint32_t)w->num_actions);
			const int32_t *a = w->actions + (size_t)k * 3;

			arc_game_perform_action_frames(w->game, a[0], a[1],
						       a[2], frame, 1);
			w->steps++;
			if (w->game->engine.status == WIN ||
			    w->game->engine.level_index > start) {
				w->wins++;
				break;
			}
			if (w->game->engine.status == GAME_OVER)
				break;
		}
	}
	return NULL;
}

int64_t arc_certify_random(struct arc_game **games, int32_t num_threads,
			   int32_t trials, int32_t horizon, int32_t start_level,
			   const int32_t *actions, int32_t num_actions,
			   uint32_t seed, int64_t *steps_out)
{
	pthread_t threads[ARC_CERTIFY_MAX_THREADS];
	struct worker workers[ARC_CERTIFY_MAX_THREADS];
	int64_t wins = 0;
	int64_t steps = 0;

	if (num_threads < 1)
		num_threads = 1;
	if (num_threads > ARC_CERTIFY_MAX_THREADS)
		num_threads = ARC_CERTIFY_MAX_THREADS;

	for (int32_t i = 0; i < num_threads; i++) {
		workers[i].game = games[i];
		workers[i].actions = actions;
		workers[i].num_actions = num_actions;
		workers[i].trials = trials / num_threads +
				    (i < trials % num_threads ? 1 : 0);
		workers[i].horizon = horizon;
		workers[i].start_level = start_level;
		workers[i].seed = seed + (uint32_t)i * 2654435761u;
		workers[i].wins = 0;
		workers[i].steps = 0;
	}
	for (int32_t i = 1; i < num_threads; i++)
		pthread_create(&threads[i], NULL, run, &workers[i]);
	run(&workers[0]);
	for (int32_t i = 1; i < num_threads; i++)
		pthread_join(threads[i], NULL);

	for (int32_t i = 0; i < num_threads; i++) {
		wins += workers[i].wins;
		steps += workers[i].steps;
	}
	if (steps_out)
		*steps_out = steps;
	return wins;
}
