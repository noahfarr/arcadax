#ifndef ARC_SCENE_H
#define ARC_SCENE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "arc/engine.h"

typedef struct {
    const int8_t *pixels;
    int32_t size;
    int32_t ph;
    int32_t pw;
} ArcSceneAtlas;

typedef struct {
    int32_t *image;
    int32_t *x;
    int32_t *y;
    int32_t *layer;
    int32_t *order;
    int8_t *frame;
    uint8_t dirty;
    int32_t n;
} ArcSceneTable;

typedef struct {
    int8_t *canvas;
    int32_t *sorted;
    int32_t canvas_h;
    int32_t canvas_w;
    int32_t *bbox;
} ArcSceneScratch;

void arc_scene_table_alloc(ArcSceneTable *scene, int32_t n);
void arc_scene_table_free(ArcSceneTable *scene);
void arc_scene_table_clear(ArcSceneTable *scene, int8_t background);
void arc_scene_touch(ArcSceneTable *scene);

void arc_scene_scratch_init(ArcSceneScratch *scratch, const ArcSceneAtlas *atlas, int32_t n);
void arc_scene_scratch_free(ArcSceneScratch *scratch);

void arc_scene_composite(const ArcSceneTable *scene, const ArcSceneAtlas *atlas,
                     ArcSceneScratch *scratch, int8_t background, int8_t *out);
void arc_scene_display(ArcSceneTable *scene, const ArcSceneAtlas *atlas, ArcSceneScratch *scratch,
                   int8_t background);

#ifdef __cplusplus
}
#endif

#endif
