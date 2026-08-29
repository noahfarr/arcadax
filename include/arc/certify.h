#ifndef ARC_CERTIFY_H
#define ARC_CERTIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arc/game.h"

#define ARC_CERTIFY_MAX_THREADS 64

int64_t arc_certify_random(struct arc_game **games, int32_t num_threads,
			   int32_t trials, int32_t horizon, int32_t start_level,
			   const int32_t *actions, int32_t num_actions,
			   uint32_t seed, int64_t *steps_out);

#ifdef __cplusplus
}
#endif

#endif
