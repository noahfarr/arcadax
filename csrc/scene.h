#ifndef ARCADAX_SCENE_H
#define ARCADAX_SCENE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "engine.h"

typedef struct {
    const int8_t *pixels;
    int32_t size;
    int32_t ph;
    int32_t pw;
} SceneAtlas;

typedef struct {
    int32_t *image;
    int32_t *x;
    int32_t *y;
    int32_t *layer;
    int32_t *order;
    int8_t *frame;
    uint8_t dirty;
    int32_t n;
} SceneTable;

typedef struct {
    int8_t *canvas;
    int32_t *sorted;
    int32_t canvas_h;
    int32_t canvas_w;
    int32_t *bbox;
} SceneScratch;

void scene_table_alloc(SceneTable *scene, int32_t n);
void scene_table_free(SceneTable *scene);
void scene_table_clear(SceneTable *scene, int8_t background);
void scene_touch(SceneTable *scene);

void scene_scratch_init(SceneScratch *scratch, const SceneAtlas *atlas, int32_t n);
void scene_scratch_free(SceneScratch *scratch);

void scene_composite(const SceneTable *scene, const SceneAtlas *atlas,
                     SceneScratch *scratch, int8_t background, int8_t *out);
void scene_display(SceneTable *scene, const SceneAtlas *atlas, SceneScratch *scratch,
                   int8_t background);

#ifdef __cplusplus
}
#endif

#endif
