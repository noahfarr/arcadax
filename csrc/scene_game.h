#ifndef ARCADAX_SCENE_GAME_H
#define ARCADAX_SCENE_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "scene.h"

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
} SceneEngineState;

struct SceneGame;

typedef struct {
    void (*zero_aux)(void *aux);
    void (*build_level)(struct SceneGame *game);
    void (*step_once)(struct SceneGame *game);
    void (*render_interface)(struct SceneGame *game, int8_t *frame);
} SceneHooks;

typedef struct SceneGame {
    SceneAtlas atlas;
    SceneTable scene;
    SceneEngineState engine;
    const SceneHooks *hooks;
    void *aux;
    void *statics;
    int32_t num_levels;
    int8_t background;
    int32_t max_frames;
    const int32_t *simple_actions;
    int32_t num_simple;
    int32_t has_click;
    int32_t num_actions;
    SceneScratch scratch;
} SceneGame;

SceneGame *scene_game_new(const SceneAtlas *atlas, int32_t num_slots, int32_t num_levels,
                          int8_t background, const SceneHooks *hooks, void *aux,
                          void *statics, const int32_t *simple_actions, int32_t num_simple,
                          int32_t has_click, int32_t max_frames);
void scene_game_free(SceneGame *game);

void scene_game_complete_action(SceneGame *game);
void scene_game_lose(SceneGame *game);
void scene_game_next_level(SceneGame *game);
void scene_game_set_level(SceneGame *game, int32_t index);

int32_t scene_game_perform_action(SceneGame *game, int32_t action_id, int32_t action_x,
                                  int32_t action_y);
int32_t scene_game_perform_action_frames(SceneGame *game, int32_t action_id,
                                         int32_t action_x, int32_t action_y,
                                         int8_t *frames, int32_t max_out);
void scene_game_decode_action(const SceneGame *game, int32_t action, int32_t *action_id,
                              int32_t *x, int32_t *y);
void scene_game_init(SceneGame *game);
int32_t scene_game_step(SceneGame *game, int32_t action, int8_t *frame, int32_t *reward,
                        uint8_t *terminated);
void scene_game_frame(SceneGame *game, int8_t *frame);

#ifdef __cplusplus
}
#endif

#endif
