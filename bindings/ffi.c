#include <stdint.h>

#include "arc/vecenv.h"

void arc_ffi_step(void *out, const void **in) {
    int64_t handle = *(const int64_t *)in[0];
    const int32_t *actions = (const int32_t *)in[1];
    void **outputs = (void **)out;
    arc_vecenv_step((ArcVecEnv *)(intptr_t)handle, actions, (int8_t *)outputs[0],
                (float *)outputs[1], (uint8_t *)outputs[2],
                (uint8_t *)outputs[3], (int32_t *)outputs[4],
                (int32_t *)outputs[5]);
}

void arc_ffi_reset(void *out, const void **in) {
    int64_t handle = *(const int64_t *)in[0];
    void **outputs = (void **)out;
    arc_vecenv_reset((ArcVecEnv *)(intptr_t)handle, (int8_t *)outputs[0]);
}
