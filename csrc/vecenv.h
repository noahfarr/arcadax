#ifndef ARCADAX_VECENV_H
#define ARCADAX_VECENV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "game.h"

typedef struct VecEnv VecEnv;

VecEnv *vecenv_new(const LevelData *levels, const Hooks *hooks, void *aux_array,
                   size_t aux_stride, void *statics,
                   const int32_t *simple_actions, int32_t num_simple,
                   int32_t has_click, int32_t max_frames, int32_t num_envs,
                   int32_t num_threads);
void vecenv_free(VecEnv *vec);
int32_t vecenv_num_actions(const VecEnv *vec);
void vecenv_reset(VecEnv *vec, int8_t *obs);
void vecenv_step(VecEnv *vec, const int32_t *actions, int8_t *obs, float *reward,
                 uint8_t *terminated, uint8_t *truncated, int32_t *level,
                 int32_t *score);

#ifdef __cplusplus
}
#endif

#endif
