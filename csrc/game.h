#ifndef ARCADAX_GAME_H
#define ARCADAX_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "engine.h"

enum { NOT_PLAYED = 0, NOT_FINISHED = 1, WIN = 2, GAME_OVER = 3 };
enum {
    ACTION_RESET = 0,
    ACTION1 = 1,
    ACTION2 = 2,
    ACTION3 = 3,
    ACTION4 = 4,
    ACTION5 = 5,
    ACTION6 = 6
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
} LevelData;

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
} EngineState;

struct Game;

typedef struct {
    void (*zero_aux)(void *aux);
    void (*on_set_level)(struct Game *game);
    void (*step_once)(struct Game *game);
    void (*render_interface)(struct Game *game, int8_t *frame);
} Hooks;

typedef struct Game {
    Atlas atlas;
    Sprites sprites;
    Camera camera;
    EngineState engine;
    RenderScratch scratch;
    const LevelData *levels;
    const Hooks *hooks;
    void *aux;
    void *statics;
    int32_t max_frames;
    const int32_t *simple_actions;
    int32_t num_simple;
    int32_t has_click;
    int32_t num_actions;
    const int8_t *bbox_atlas;
} Game;

Game *game_new(const LevelData *levels, const Hooks *hooks, void *aux,
               void *statics, const int32_t *simple_actions, int32_t num_simple,
               int32_t has_click, int32_t max_frames);
void game_free(Game *game);

void game_complete_action(Game *game);
void game_lose(Game *game);
void game_next_level(Game *game);
void game_set_level(Game *game, int32_t index);

int32_t game_perform_action(Game *game, int32_t action_id, int32_t action_x,
                            int32_t action_y);
int32_t game_perform_action_frames(Game *game, int32_t action_id, int32_t action_x,
                                   int32_t action_y, int8_t *frames,
                                   int32_t max_out);
void game_decode_action(const Game *game, int32_t action, int32_t *action_id,
                        int32_t *x, int32_t *y);
void game_init(Game *game);
int32_t game_step(Game *game, int32_t action, int8_t *frame, int32_t *reward,
                  uint8_t *terminated);
void game_frame(Game *game, int8_t *frame);

#ifdef __cplusplus
}
#endif

#endif
