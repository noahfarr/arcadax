#include "scene.h"

#include <stdlib.h>
#include <string.h>

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void scene_table_alloc(SceneTable *scene, int32_t n) {
    scene->n = n;
    scene->image = malloc(sizeof(int32_t) * (size_t)n);
    scene->x = malloc(sizeof(int32_t) * (size_t)n);
    scene->y = malloc(sizeof(int32_t) * (size_t)n);
    scene->layer = malloc(sizeof(int32_t) * (size_t)n);
    scene->order = malloc(sizeof(int32_t) * (size_t)n);
    scene->frame = malloc((size_t)FRAME_SIZE * FRAME_SIZE);
    scene->dirty = 1;
}

void scene_table_free(SceneTable *scene) {
    free(scene->image);
    free(scene->x);
    free(scene->y);
    free(scene->layer);
    free(scene->order);
    free(scene->frame);
    scene->image = NULL;
    scene->x = NULL;
    scene->y = NULL;
    scene->layer = NULL;
    scene->order = NULL;
    scene->frame = NULL;
}

void scene_table_clear(SceneTable *scene, int8_t background) {
    int32_t n = scene->n;
    for (int32_t i = 0; i < n; i++) {
        scene->image[i] = -1;
        scene->x[i] = 0;
        scene->y[i] = 0;
        scene->layer[i] = 0;
        scene->order[i] = i;
    }
    memset(scene->frame, background, (size_t)FRAME_SIZE * FRAME_SIZE);
    scene->dirty = 1;
}

void scene_touch(SceneTable *scene) { scene->dirty = 1; }

static inline int64_t draw_key(const SceneTable *scene, int32_t i) {
    if (scene->image[i] < 0) return (int64_t)1 << 40;
    return (int64_t)scene->layer[i] * 4096 + scene->order[i];
}

void scene_scratch_init(SceneScratch *scratch, const SceneAtlas *atlas, int32_t n) {
    int32_t ph = atlas->ph, pw = atlas->pw;
    scratch->canvas_h = FRAME_SIZE + 2 * ph;
    scratch->canvas_w = FRAME_SIZE + 2 * pw;
    scratch->canvas = malloc((size_t)scratch->canvas_h * scratch->canvas_w);
    scratch->sorted = malloc(sizeof(int32_t) * (size_t)n);
    scratch->bbox = malloc(sizeof(int32_t) * 4 * (size_t)atlas->size);
    for (int32_t k = 0; k < atlas->size; k++) {
        const int8_t *patch = atlas->pixels + (size_t)k * ph * pw;
        int32_t y0 = ph, y1 = 0, x0 = pw, x1 = 0;
        for (int32_t v = 0; v < ph; v++) {
            const int8_t *row = patch + (size_t)v * pw;
            for (int32_t u = 0; u < pw; u++) {
                if (row[u] >= 0) {
                    if (v < y0) y0 = v;
                    if (v + 1 > y1) y1 = v + 1;
                    if (u < x0) x0 = u;
                    if (u + 1 > x1) x1 = u + 1;
                }
            }
        }
        if (y1 <= y0 || x1 <= x0) {
            y0 = 0; y1 = 0; x0 = 0; x1 = 0;
        }
        scratch->bbox[k * 4 + 0] = y0;
        scratch->bbox[k * 4 + 1] = y1;
        scratch->bbox[k * 4 + 2] = x0;
        scratch->bbox[k * 4 + 3] = x1;
    }
}

void scene_scratch_free(SceneScratch *scratch) {
    free(scratch->canvas);
    free(scratch->sorted);
    free(scratch->bbox);
    scratch->canvas = NULL;
    scratch->sorted = NULL;
    scratch->bbox = NULL;
}

void scene_composite(const SceneTable *scene, const SceneAtlas *atlas,
                     SceneScratch *scratch, int8_t background, int8_t *out) {
    int32_t ph = atlas->ph, pw = atlas->pw;
    int32_t ch = scratch->canvas_h, cw = scratch->canvas_w;
    int8_t *canvas = scratch->canvas;
    memset(canvas, background, (size_t)ch * cw);

    int32_t n = scene->n;
    int32_t *order = scratch->sorted;
    int32_t count = 0;
    for (int32_t i = 0; i < n; i++)
        if (scene->image[i] >= 0) order[count++] = i;

    for (int32_t a = 1; a < count; a++) {
        int32_t v = order[a];
        int64_t key = draw_key(scene, v);
        int32_t b = a - 1;
        while (b >= 0 && draw_key(scene, order[b]) > key) {
            order[b + 1] = order[b];
            b--;
        }
        order[b + 1] = v;
    }

    for (int32_t a = 0; a < count; a++) {
        int32_t i = order[a];
        int32_t img = scene->image[i];
        const int8_t *patch = atlas->pixels + (size_t)img * ph * pw;
        int32_t sy = clamp(scene->y[i] + ph, 0, ch - ph);
        int32_t sx = clamp(scene->x[i] + pw, 0, cw - pw);
        int32_t y0 = scratch->bbox[img * 4 + 0], y1 = scratch->bbox[img * 4 + 1];
        int32_t x0 = scratch->bbox[img * 4 + 2], x1 = scratch->bbox[img * 4 + 3];
        for (int32_t v = y0; v < y1; v++) {
            const int8_t *src = patch + (size_t)v * pw;
            int8_t *dst = canvas + (size_t)(sy + v) * cw + sx;
            for (int32_t u = x0; u < x1; u++) {
                int8_t m = (int8_t)(src[u] >> 7);
                dst[u] = (int8_t)((dst[u] & m) | (src[u] & ~m));
            }
        }
    }

    for (int32_t r = 0; r < FRAME_SIZE; r++)
        memcpy(out + (size_t)r * FRAME_SIZE, canvas + (size_t)(ph + r) * cw + pw,
              FRAME_SIZE);
}

void scene_display(SceneTable *scene, const SceneAtlas *atlas, SceneScratch *scratch,
                   int8_t background) {
    if (scene->dirty) scene_composite(scene, atlas, scratch, background, scene->frame);
    scene->dirty = 0;
}
