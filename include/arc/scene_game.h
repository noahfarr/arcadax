#ifndef ARC_SCENE_GAME_H
#define ARC_SCENE_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arc/scene.h"

enum { SCENE_NOT_PLAYED = 0, SCENE_NOT_FINISHED = 1, SCENE_WIN = 2, SCENE_GAME_OVER = 3 };
enum { SCENE_ACTION_RESET = 0, SCENE_ACTION6 = 6 };

typedef struct {
    int32_t level_index;
    int32_t score;
    int32_t status;
    int32_t action_id;
    int32_t action_x;
    int32_t action_y;
    int32_t action_count;
    uint8_t action_complete;
    uint8_t next_level;
    uint8_t full_reset;
} ArcSceneEngineState;

struct ArcSceneGame;

typedef struct {
    void (*zero_aux)(void *aux);
    void (*build_level)(struct ArcSceneGame *game);
    void (*step_once)(struct ArcSceneGame *game);
    void (*render_interface)(struct ArcSceneGame *game, int8_t *frame);
} ArcSceneHooks;

typedef struct ArcSceneGame {
    ArcSceneAtlas atlas;
    ArcSceneTable scene;
    ArcSceneEngineState engine;
    const ArcSceneHooks *hooks;
    void *aux;
    void *statics;
    int32_t num_levels;
    int8_t background;
    int32_t max_frames;
    const int32_t *simple_actions;
    int32_t num_simple;
    int32_t has_click;
    int32_t num_actions;
    ArcSceneScratch scratch;
} ArcSceneGame;

ArcSceneGame *arc_scene_game_new(const ArcSceneAtlas *atlas, int32_t num_slots, int32_t num_levels,
                          int8_t background, const ArcSceneHooks *hooks, void *aux,
                          void *statics, const int32_t *simple_actions, int32_t num_simple,
                          int32_t has_click, int32_t max_frames);
void arc_scene_game_free(ArcSceneGame *game);

void arc_scene_game_complete_action(ArcSceneGame *game);
void arc_scene_game_lose(ArcSceneGame *game);
void arc_scene_game_next_level(ArcSceneGame *game);
void arc_scene_game_set_level(ArcSceneGame *game, int32_t index);

int32_t arc_scene_game_perform_action(ArcSceneGame *game, int32_t action_id, int32_t action_x,
                                  int32_t action_y);
int32_t arc_scene_game_perform_action_frames(ArcSceneGame *game, int32_t action_id,
                                         int32_t action_x, int32_t action_y,
                                         int8_t *frames, int32_t max_out);
void arc_scene_game_decode_action(const ArcSceneGame *game, int32_t action, int32_t *action_id,
                              int32_t *x, int32_t *y);
void arc_scene_game_init(ArcSceneGame *game);
int32_t arc_scene_game_step(ArcSceneGame *game, int32_t action, int8_t *frame, int32_t *reward,
                        uint8_t *terminated);
void arc_scene_game_frame(ArcSceneGame *game, int8_t *frame);

#ifdef __cplusplus
}
#endif

#endif
