#ifndef ARC_SCENE_H
#define ARC_SCENE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "arc/engine.h"

struct arc_scene_atlas {
	const int8_t *pixels;
	int32_t size;
	int32_t ph;
	int32_t pw;
};

struct arc_scene_table {
	int32_t *image;
	int32_t *x;
	int32_t *y;
	int32_t *layer;
	int32_t *order;
	int8_t *frame;
	uint8_t dirty;
	int32_t n;
};

struct arc_scene_scratch {
	int8_t *canvas;
	int32_t *sorted;
	int32_t canvas_h;
	int32_t canvas_w;
	int32_t *bbox;
};

void arc_scene_table_alloc(struct arc_scene_table *scene, int32_t n);
void arc_scene_table_free(struct arc_scene_table *scene);
void arc_scene_table_clear(struct arc_scene_table *scene, int8_t background);
void arc_scene_touch(struct arc_scene_table *scene);

void arc_scene_scratch_init(struct arc_scene_scratch *scratch,
			    const struct arc_scene_atlas *atlas, int32_t n);
void arc_scene_scratch_free(struct arc_scene_scratch *scratch);

void arc_scene_composite(const struct arc_scene_table *scene,
			 const struct arc_scene_atlas *atlas,
			 struct arc_scene_scratch *scratch, int8_t background,
			 int8_t *out);
void arc_scene_display(struct arc_scene_table *scene,
		       const struct arc_scene_atlas *atlas,
		       struct arc_scene_scratch *scratch, int8_t background);

#ifdef __cplusplus
}
#endif

#endif
