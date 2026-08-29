#include "game.h"

#include <stdint.h>
#include <time.h>

static _Thread_local uint32_t rng_state;

static uint32_t xorshift(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

int64_t game_bench_steps(Game *game, int32_t iters, uint32_t seed) {
    rng_state = seed ? seed : 1;
    int8_t frame[FRAME_SIZE * FRAME_SIZE];
    int32_t reward;
    uint8_t terminated;
    int64_t total = 0;
    game_init(game);
    for (int32_t k = 0; k < iters; k++) {
        int32_t action = (int32_t)(xorshift() % (uint32_t)game->num_actions);
        total += game_step(game, action, frame, &reward, &terminated);
        if (terminated) game_perform_action(game, ACTION_RESET, 0, 0);
    }
    return total;
}

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

void game_bench_split(Game *game, int32_t iters, uint32_t seed, int64_t *frames_out,
                      double *logic_ns_out, double *render_ns_out) {
    rng_state = seed ? seed : 1;
    int8_t frame[FRAME_SIZE * FRAME_SIZE];
    int64_t frames = 0;
    double logic_ns = 0, render_ns = 0;
    game_init(game);
    for (int32_t k = 0; k < iters; k++) {
        int32_t action = (int32_t)(xorshift() % (uint32_t)game->num_actions);
        int32_t action_id, x, y;
        game_decode_action(game, action, &action_id, &x, &y);

        double t0 = now_ns();
        int32_t count = game_perform_action(game, action_id, x, y);
        double t1 = now_ns();
        game_frame(game, frame);
        double t2 = now_ns();

        logic_ns += t1 - t0;
        render_ns += t2 - t1;
        frames += count;

        if (game->engine.status == WIN || game->engine.status == GAME_OVER) {
            t0 = now_ns();
            count = game_perform_action(game, ACTION_RESET, 0, 0);
            t1 = now_ns();
            game_frame(game, frame);
            t2 = now_ns();
            logic_ns += t1 - t0;
            render_ns += t2 - t1;
            frames += count;
        }
    }
    *frames_out = frames;
    *logic_ns_out = logic_ns;
    *render_ns_out = render_ns;
}

#include <pthread.h>

typedef struct {
    Game *game;
    int32_t iters;
    uint32_t seed;
    int64_t result;
} BenchArg;

static void *bench_thread(void *arg) {
    BenchArg *a = (BenchArg *)arg;
    a->result = game_bench_steps(a->game, a->iters, a->seed);
    return NULL;
}

int64_t game_bench_parallel(Game **games, int32_t num_threads, int32_t iters,
                            uint32_t seed) {
    pthread_t threads[128];
    BenchArg args[128];
    if (num_threads > 128) num_threads = 128;
    for (int32_t i = 0; i < num_threads; i++) {
        args[i].game = games[i];
        args[i].iters = iters;
        args[i].seed = seed + (uint32_t)i * 7919u;
        pthread_create(&threads[i], NULL, bench_thread, &args[i]);
    }
    int64_t total = 0;
    for (int32_t i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total += args[i].result;
    }
    return total;
}

typedef struct {
    Game **games;
    int32_t num_games;
    int32_t iters;
    uint32_t seed;
    int64_t result;
} RoundRobinArg;

static void *round_robin_thread(void *arg) {
    RoundRobinArg *a = (RoundRobinArg *)arg;
    rng_state = a->seed ? a->seed : 1;
    int8_t frame[FRAME_SIZE * FRAME_SIZE];
    int32_t reward;
    uint8_t terminated;
    int64_t total = 0;
    for (int32_t i = 0; i < a->num_games; i++) game_init(a->games[i]);
    for (int32_t k = 0; k < a->iters; k++) {
        Game *g = a->games[k % a->num_games];
        int32_t action = (int32_t)(xorshift() % (uint32_t)g->num_actions);
        total += game_step(g, action, frame, &reward, &terminated);
        if (terminated) game_perform_action(g, ACTION_RESET, 0, 0);
    }
    a->result = total;
    return NULL;
}

int64_t game_bench_envs(Game **games, int32_t num_games, int32_t num_threads,
                        int32_t iters_per_thread, uint32_t seed) {
    pthread_t threads[128];
    RoundRobinArg args[128];
    if (num_threads > 128) num_threads = 128;
    int32_t per = num_games / num_threads;
    for (int32_t i = 0; i < num_threads; i++) {
        args[i].games = games + (size_t)i * per;
        args[i].num_games = per;
        args[i].iters = iters_per_thread;
        args[i].seed = seed + (uint32_t)i * 7919u;
        pthread_create(&threads[i], NULL, round_robin_thread, &args[i]);
    }
    int64_t total = 0;
    for (int32_t i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total += args[i].result;
    }
    return total;
}
