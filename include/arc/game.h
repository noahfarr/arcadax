#ifndef ARC_GAME_H
#define ARC_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct {
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
} ArcLevelData;

typedef struct {
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
} ArcEngineState;

struct ArcGame;

typedef struct {
    void (*zero_aux)(void *aux);
    void (*on_set_level)(struct ArcGame *game);
    void (*step_once)(struct ArcGame *game);
    void (*render_interface)(struct ArcGame *game, int8_t *frame);
} ArcHooks;

typedef struct ArcGame {
    ArcAtlas atlas;
    ArcSprites sprites;
    ArcCamera camera;
    ArcEngineState engine;
    ArcRenderScratch scratch;
    const ArcLevelData *levels;
    const ArcHooks *hooks;
    void *aux;
    void *statics;
    int32_t max_frames;
    const int32_t *simple_actions;
    int32_t num_simple;
    int32_t has_click;
    int32_t num_actions;
    const int8_t *bbox_atlas;
} ArcGame;

ArcGame *arc_game_new(const ArcLevelData *levels, const ArcHooks *hooks, void *aux,
               void *statics, const int32_t *simple_actions, int32_t num_simple,
               int32_t has_click, int32_t max_frames);
void arc_game_free(ArcGame *game);

void arc_game_complete_action(ArcGame *game);
void arc_game_lose(ArcGame *game);
void arc_game_next_level(ArcGame *game);
void arc_game_set_level(ArcGame *game, int32_t index);

int32_t arc_game_perform_action(ArcGame *game, int32_t action_id, int32_t action_x,
                            int32_t action_y);
int32_t arc_game_perform_action_frames(ArcGame *game, int32_t action_id, int32_t action_x,
                                   int32_t action_y, int8_t *frames,
                                   int32_t max_out);
void arc_game_decode_action(const ArcGame *game, int32_t action, int32_t *action_id,
                        int32_t *x, int32_t *y);
void arc_game_init(ArcGame *game);
int32_t arc_game_step(ArcGame *game, int32_t action, int8_t *frame, int32_t *reward,
                  uint8_t *terminated);
void arc_game_frame(ArcGame *game, int8_t *frame);

#ifdef __cplusplus
}
#endif

#endif
