#include "arc/vecenv.h"

#include <pthread.h>
#include <stdlib.h>

#define FRAME_BYTES (ARC_FRAME_SIZE * ARC_FRAME_SIZE)

struct worker_arg {
	struct arc_vec_env *vec;
	int32_t start;
	int32_t end;
	const int32_t *actions;
	int8_t *obs;
	float *reward;
	uint8_t *terminated;
	uint8_t *truncated;
	int32_t *level;
	int32_t *score;
	int reset_only;
};

struct slot {
	struct arc_vec_env *vec;
	int32_t start;
	int32_t end;
	struct arc_render_scratch scratch;
};

struct arc_vec_env {
	struct arc_game **games;
	int32_t num_envs;
	int32_t num_threads;
	int32_t horizon;
	int32_t *elapsed;
	pthread_t *threads;
	struct slot *slots;
	pthread_mutex_t lock;
	pthread_cond_t start_cv;
	pthread_cond_t done_cv;
	struct worker_arg proto;
	int32_t generation;
	int32_t pending;
	int stop;
};

static void work(const struct worker_arg *a)
{
	struct arc_vec_env *vec = a->vec;
	for (int32_t i = a->start; i < a->end; i++) {
		struct arc_game *g = vec->games[i];
		int8_t *frame = a->obs + (size_t)i * FRAME_BYTES;
		if (a->reset_only) {
			arc_game_init(g);
			vec->elapsed[i] = 0;
			arc_game_frame(g, frame);
			continue;
		}
		int32_t reward_i;
		uint8_t term;
		arc_game_step(g, a->actions[i], frame, &reward_i, &term);
		vec->elapsed[i] += 1;
		uint8_t trunc = vec->horizon > 0 &&
				vec->elapsed[i] >= vec->horizon && !term;
		a->reward[i] = (float)reward_i;
		a->terminated[i] = term;
		a->truncated[i] = trunc;
		if (a->level)
			a->level[i] = g->engine.level_index;
		if (a->score)
			a->score[i] = g->engine.score;
		if (term || trunc) {
			arc_game_init(g);
			vec->elapsed[i] = 0;
			arc_game_frame(g, frame);
		}
	}
}

static void *attend(void *raw)
{
	struct slot *slot = (struct slot *)raw;
	struct arc_vec_env *vec = slot->vec;
	int32_t seen = 0;
	for (;;) {
		pthread_mutex_lock(&vec->lock);
		while (vec->generation == seen && !vec->stop)
			pthread_cond_wait(&vec->start_cv, &vec->lock);
		if (vec->stop) {
			pthread_mutex_unlock(&vec->lock);
			return NULL;
		}
		seen = vec->generation;
		struct worker_arg a = vec->proto;
		pthread_mutex_unlock(&vec->lock);

		a.start = slot->start;
		a.end = slot->end;
		work(&a);

		pthread_mutex_lock(&vec->lock);
		if (--vec->pending == 0)
			pthread_cond_signal(&vec->done_cv);
		pthread_mutex_unlock(&vec->lock);
	}
}

static void run(struct arc_vec_env *vec, struct worker_arg proto)
{
	proto.vec = vec;
	if (vec->num_threads <= 1) {
		proto.start = 0;
		proto.end = vec->num_envs;
		work(&proto);
		return;
	}

	pthread_mutex_lock(&vec->lock);
	vec->proto = proto;
	vec->pending = vec->num_threads - 1;
	vec->generation++;
	pthread_cond_broadcast(&vec->start_cv);
	pthread_mutex_unlock(&vec->lock);

	struct slot *mine = &vec->slots[vec->num_threads - 1];
	proto.start = mine->start;
	proto.end = mine->end;
	work(&proto);

	pthread_mutex_lock(&vec->lock);
	while (vec->pending > 0)
		pthread_cond_wait(&vec->done_cv, &vec->lock);
	pthread_mutex_unlock(&vec->lock);
}

struct arc_vec_env *arc_vecenv_new(const struct arc_level_data *levels,
				   const struct arc_hooks *hooks,
				   void *aux_array, size_t aux_stride,
				   void *statics, const int32_t *simple_actions,
				   int32_t num_simple, int32_t has_click,
				   int32_t max_frames, int32_t num_envs,
				   int32_t num_threads)
{
	struct arc_vec_env *vec = calloc(1, sizeof(struct arc_vec_env));
	vec->num_envs = num_envs;
	vec->horizon = 0;
	vec->games = calloc(num_envs, sizeof(struct arc_game *));
	vec->elapsed = calloc(num_envs, sizeof(int32_t));
	for (int32_t i = 0; i < num_envs; i++) {
		void *aux = (char *)aux_array + (size_t)i * aux_stride;
		vec->games[i] = arc_game_new(levels, hooks, aux, statics,
					     simple_actions, num_simple,
					     has_click, max_frames);
	}

	int32_t nt = num_threads < 1 ? 1 : num_threads;
	int32_t chunk = (num_envs + nt - 1) / nt;
	if (chunk < 1)
		chunk = 1;
	vec->slots = calloc(nt, sizeof(struct slot));
	int32_t used = 0;
	for (int32_t t = 0; t < nt; t++) {
		int32_t start = t * chunk;
		if (start >= num_envs)
			break;
		int32_t end = start + chunk;
		if (end > num_envs)
			end = num_envs;
		vec->slots[used].vec = vec;
		vec->slots[used].start = start;
		vec->slots[used].end = end;
		used++;
	}
	vec->num_threads = used;

	for (int32_t t = 0; t < used; t++) {
		arc_render_scratch_init(&vec->slots[t].scratch,
					&vec->games[0]->atlas);
		for (int32_t i = vec->slots[t].start; i < vec->slots[t].end; i++)
			arc_game_share_scratch(vec->games[i],
					       &vec->slots[t].scratch);
	}

	if (used > 1) {
		pthread_mutex_init(&vec->lock, NULL);
		pthread_cond_init(&vec->start_cv, NULL);
		pthread_cond_init(&vec->done_cv, NULL);
		vec->threads = calloc(used - 1, sizeof(pthread_t));
		for (int32_t t = 0; t < used - 1; t++)
			pthread_create(&vec->threads[t], NULL, attend,
				       &vec->slots[t]);
	}
	return vec;
}

void arc_vecenv_free(struct arc_vec_env *vec)
{
	if (vec->num_threads > 1) {
		pthread_mutex_lock(&vec->lock);
		vec->stop = 1;
		pthread_cond_broadcast(&vec->start_cv);
		pthread_mutex_unlock(&vec->lock);
		for (int32_t t = 0; t < vec->num_threads - 1; t++)
			pthread_join(vec->threads[t], NULL);
		free(vec->threads);
		pthread_cond_destroy(&vec->done_cv);
		pthread_cond_destroy(&vec->start_cv);
		pthread_mutex_destroy(&vec->lock);
	}
	for (int32_t i = 0; i < vec->num_envs; i++)
		arc_game_free(vec->games[i]);
	for (int32_t t = 0; t < vec->num_threads; t++)
		arc_render_scratch_free(&vec->slots[t].scratch);
	free(vec->slots);
	free(vec->games);
	free(vec->elapsed);
	free(vec);
}

int32_t arc_vecenv_num_actions(const struct arc_vec_env *vec)
{
	return vec->games[0]->num_actions;
}

void arc_vecenv_reset(struct arc_vec_env *vec, int8_t *obs)
{
	struct worker_arg proto = { 0 };
	proto.vec = vec;
	proto.obs = obs;
	proto.reset_only = 1;
	run(vec, proto);
}

void arc_vecenv_step(struct arc_vec_env *vec, const int32_t *actions,
		     int8_t *obs, float *reward, uint8_t *terminated,
		     uint8_t *truncated, int32_t *level, int32_t *score)
{
	struct worker_arg proto = { 0 };
	proto.vec = vec;
	proto.actions = actions;
	proto.obs = obs;
	proto.reward = reward;
	proto.terminated = terminated;
	proto.truncated = truncated;
	proto.level = level;
	proto.score = score;
	run(vec, proto);
}
