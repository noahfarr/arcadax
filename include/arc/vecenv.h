#ifndef ARC_VECENV_H
#define ARC_VECENV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "arc/game.h"

typedef struct ArcVecEnv ArcVecEnv;

ArcVecEnv *arc_vecenv_new(const ArcLevelData *levels, const ArcHooks *hooks, void *aux_array,
                   size_t aux_stride, void *statics,
                   const int32_t *simple_actions, int32_t num_simple,
                   int32_t has_click, int32_t max_frames, int32_t num_envs,
                   int32_t num_threads);
void arc_vecenv_free(ArcVecEnv *vec);
int32_t arc_vecenv_num_actions(const ArcVecEnv *vec);
void arc_vecenv_reset(ArcVecEnv *vec, int8_t *obs);
void arc_vecenv_step(ArcVecEnv *vec, const int32_t *actions, int8_t *obs, float *reward,
                 uint8_t *terminated, uint8_t *truncated, int32_t *level,
                 int32_t *score);

#ifdef __cplusplus
}
#endif

#endif
