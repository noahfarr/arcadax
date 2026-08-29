#include "arc/vecenv.h"

#include <pthread.h>
#include <stdlib.h>

#define FRAME_BYTES (ARC_FRAME_SIZE * ARC_FRAME_SIZE)

struct ArcVecEnv {
    ArcGame **games;
    int32_t num_envs;
    int32_t num_threads;
    int32_t horizon;
    int32_t *elapsed;
};

typedef struct {
    ArcVecEnv *vec;
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
} WorkerArg;

static void *worker(void *raw) {
    WorkerArg *a = (WorkerArg *)raw;
    ArcVecEnv *vec = a->vec;
    for (int32_t i = a->start; i < a->end; i++) {
        ArcGame *g = vec->games[i];
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
        uint8_t trunc = vec->horizon > 0 && vec->elapsed[i] >= vec->horizon && !term;
        a->reward[i] = (float)reward_i;
        a->terminated[i] = term;
        a->truncated[i] = trunc;
        if (a->level) a->level[i] = g->engine.level_index;
        if (a->score) a->score[i] = g->engine.score;
        if (term || trunc) {
            arc_game_init(g);
            vec->elapsed[i] = 0;
            arc_game_frame(g, frame);
        }
    }
    return NULL;
}

static void run(ArcVecEnv *vec, WorkerArg proto) {
    int32_t nt = vec->num_threads;
    if (nt < 1) nt = 1;
    pthread_t threads[128];
    WorkerArg args[128];
    if (nt > 128) nt = 128;
    int32_t chunk = (vec->num_envs + nt - 1) / nt;
    int32_t launched = 0;
    for (int32_t t = 0; t < nt; t++) {
        int32_t start = t * chunk;
        if (start >= vec->num_envs) break;
        int32_t end = start + chunk;
        if (end > vec->num_envs) end = vec->num_envs;
        args[t] = proto;
        args[t].start = start;
        args[t].end = end;
        pthread_create(&threads[t], NULL, worker, &args[t]);
        launched++;
    }
    for (int32_t t = 0; t < launched; t++) pthread_join(threads[t], NULL);
}

ArcVecEnv *arc_vecenv_new(const ArcLevelData *levels, const ArcHooks *hooks, void *aux_array,
                   size_t aux_stride, void *statics,
                   const int32_t *simple_actions, int32_t num_simple,
                   int32_t has_click, int32_t max_frames, int32_t num_envs,
                   int32_t num_threads) {
    ArcVecEnv *vec = calloc(1, sizeof(ArcVecEnv));
    vec->num_envs = num_envs;
    vec->num_threads = num_threads;
    vec->horizon = 0;
    vec->games = calloc(num_envs, sizeof(ArcGame *));
    vec->elapsed = calloc(num_envs, sizeof(int32_t));
    for (int32_t i = 0; i < num_envs; i++) {
        void *aux = (char *)aux_array + (size_t)i * aux_stride;
        vec->games[i] = arc_game_new(levels, hooks, aux, statics, simple_actions,
                                 num_simple, has_click, max_frames);
    }
    return vec;
}

void arc_vecenv_free(ArcVecEnv *vec) {
    for (int32_t i = 0; i < vec->num_envs; i++) arc_game_free(vec->games[i]);
    free(vec->games);
    free(vec->elapsed);
    free(vec);
}

int32_t arc_vecenv_num_actions(const ArcVecEnv *vec) {
    return vec->games[0]->num_actions;
}

void arc_vecenv_reset(ArcVecEnv *vec, int8_t *obs) {
    WorkerArg proto = {0};
    proto.vec = vec;
    proto.obs = obs;
    proto.reset_only = 1;
    run(vec, proto);
}

void arc_vecenv_step(ArcVecEnv *vec, const int32_t *actions, int8_t *obs, float *reward,
                 uint8_t *terminated, uint8_t *truncated, int32_t *level,
                 int32_t *score) {
    WorkerArg proto = {0};
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
