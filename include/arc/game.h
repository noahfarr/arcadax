#ifndef ARC_GAME_H
#define ARC_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "arc/engine.h"

enum { NOT_PLAYED = 0, NOT_FINISHED = 1, WIN = 2, GAME_OVER = 3 };
enum {
	ARC_ACTION_RESET = 0,
	ARC_ACTION1 = 1,
	ARC_ACTION2 = 2,
	ARC_ACTION3 = 3,
	ARC_ACTION4 = 4,
	ARC_ACTION5 = 5,
	ARC_ACTION6 = 6
};

struct arc_level_data {
	const int8_t *pixels;
	const int32_t *h;
	const int32_t *w;
	const int32_t *x;
	const int32_t *y;
	const int32_t *layer;
	const int32_t *order;
	const int8_t *interaction;
	const int8_t *blocking;
	const uint8_t *alive;
	const uint8_t *tags;
	const int32_t *grid_size;
	int32_t num_levels;
	int32_t num_slots;
	int32_t num_tags;
	int32_t ph;
	int32_t pw;
	int32_t win_score;
	int8_t background;
	int8_t letter_box;
};

struct arc_engine_state {
	int32_t level_index;
	int32_t score;
	int32_t status;
	int32_t action_id;
	int32_t action_x;
	int32_t action_y;
	int32_t action_count;
	int32_t next_order;
	uint8_t action_complete;
	uint8_t next_level;
	uint8_t full_reset;
};

struct arc_game;

struct arc_hooks {
	void (*zero_aux)(void *aux);
	void (*on_set_level)(struct arc_game *game);
	void (*step_once)(struct arc_game *game);
	void (*render_interface)(struct arc_game *game, int8_t *frame);
};

struct arc_game {
	struct arc_atlas atlas;
	struct arc_sprites sprites;
	struct arc_camera camera;
	struct arc_engine_state engine;
	struct arc_render_scratch *scratch;
	int owns_scratch;
	const struct arc_level_data *levels;
	const struct arc_hooks *hooks;
	void *aux;
	void *statics;
	int32_t max_frames;
	const int32_t *simple_actions;
	int32_t num_simple;
	int32_t has_click;
	int32_t num_actions;
	const int8_t *bbox_atlas;
};

struct arc_game *arc_game_new(const struct arc_level_data *levels,
			      const struct arc_hooks *hooks, void *aux,
			      void *statics, const int32_t *simple_actions,
			      int32_t num_simple, int32_t has_click,
			      int32_t max_frames);
void arc_game_free(struct arc_game *game);
void arc_game_share_scratch(struct arc_game *game,
			    struct arc_render_scratch *scratch);

void arc_game_complete_action(struct arc_game *game);
void arc_game_lose(struct arc_game *game);
void arc_game_next_level(struct arc_game *game);
void arc_game_set_level(struct arc_game *game, int32_t index);

int32_t arc_game_perform_action(struct arc_game *game, int32_t action_id,
				int32_t action_x, int32_t action_y);
int32_t arc_game_perform_action_frames(struct arc_game *game, int32_t action_id,
				       int32_t action_x, int32_t action_y,
				       int8_t *frames, int32_t max_out);
void arc_game_decode_action(const struct arc_game *game, int32_t action,
			    int32_t *action_id, int32_t *x, int32_t *y);
void arc_game_init(struct arc_game *game);
int32_t arc_game_step(struct arc_game *game, int32_t action, int8_t *frame,
		      int32_t *reward, uint8_t *terminated);
void arc_game_frame(struct arc_game *game, int8_t *frame);

size_t arc_game_state_size(const struct arc_game *game, size_t aux_size);
void arc_game_save(const struct arc_game *game, size_t aux_size, void *dst);
void arc_game_load(struct arc_game *game, size_t aux_size, const void *src);
uint64_t arc_game_hash(const struct arc_game *game, size_t aux_size);

#ifdef __cplusplus
}
#endif

#endif
