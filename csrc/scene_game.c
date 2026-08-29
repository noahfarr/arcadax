#include "scene_game.h"

#include <stdlib.h>
#include <string.h>

SceneGame *scene_game_new(const SceneAtlas *atlas, int32_t num_slots, int32_t num_levels,
                          int8_t background, const SceneHooks *hooks, void *aux,
                          void *statics, const int32_t *simple_actions, int32_t num_simple,
                          int32_t has_click, int32_t max_frames) {
    SceneGame *game = calloc(1, sizeof(SceneGame));
    game->atlas = *atlas;
    game->hooks = hooks;
    game->aux = aux;
    game->statics = statics;
    game->num_levels = num_levels;
    game->background = background;
    game->max_frames = max_frames;
    game->simple_actions = simple_actions;
    game->num_simple = num_simple;
    game->has_click = has_click;
    game->num_actions = num_simple + (has_click ? FRAME_SIZE * FRAME_SIZE : 0);
    scene_table_alloc(&game->scene, num_slots);
    scene_scratch_init(&game->scratch, &game->atlas, num_slots);
    return game;
}

void scene_game_free(SceneGame *game) {
    scene_table_free(&game->scene);
    scene_scratch_free(&game->scratch);
    free(game);
}

void scene_game_complete_action(SceneGame *game) { game->engine.action_complete = 1; }

void scene_game_lose(SceneGame *game) { game->engine.status = SCENE_GAME_OVER; }

void scene_game_next_level(SceneGame *game) {
    int is_last = game->engine.level_index == game->num_levels - 1;
    game->engine.score += 1;
    game->engine.next_level = !is_last;
    if (is_last) game->engine.status = SCENE_WIN;
}

void scene_game_set_level(SceneGame *game, int32_t index) {
    scene_table_clear(&game->scene, game->background);
    game->engine.level_index = index;
    game->engine.action_count = 0;
    if (game->hooks->build_level) game->hooks->build_level(game);
}

static void scene_full_reset(SceneGame *game) {
    game->engine.score = 0;
    game->engine.full_reset = 1;
    scene_game_set_level(game, 0);
    game->engine.status = SCENE_NOT_FINISHED;
}

static void scene_level_reset(SceneGame *game) {
    scene_game_set_level(game, game->engine.level_index);
    game->engine.status = SCENE_NOT_FINISHED;
}

static void scene_handle_reset(SceneGame *game) {
    if (game->engine.action_count == 0 || game->engine.status == SCENE_WIN)
        scene_full_reset(game);
    else
        scene_level_reset(game);
}

static void scene_advance_level(SceneGame *game) {
    scene_game_set_level(game, game->engine.level_index + 1);
    game->engine.next_level = 0;
}

void scene_game_frame(SceneGame *game, int8_t *frame) {
    memcpy(frame, game->scene.frame, (size_t)FRAME_SIZE * FRAME_SIZE);
    if (game->hooks->render_interface) game->hooks->render_interface(game, frame);
}

static int32_t scene_perform(SceneGame *game, int32_t action_id, int32_t action_x,
                             int32_t action_y, int8_t *frames, int32_t max_out) {
    SceneEngineState *e = &game->engine;
    e->full_reset = 0;
    int is_reset = action_id == SCENE_ACTION_RESET;
    int terminal = e->status == SCENE_WIN || e->status == SCENE_GAME_OVER;
    if (is_reset) scene_handle_reset(game);
    if (!is_reset && terminal) return 0;

    e->status = SCENE_NOT_FINISHED;
    e->action_id = action_id;
    e->action_x = action_x;
    e->action_y = action_y;
    e->action_complete = 0;
    if (!is_reset) e->action_count += 1;

    int32_t count = 0;
    while (!(!e->next_level && e->action_complete) && count < game->max_frames) {
        if (e->next_level)
            scene_advance_level(game);
        else
            game->hooks->step_once(game);
        scene_display(&game->scene, &game->atlas, &game->scratch, game->background);
        if (frames && count < max_out)
            scene_game_frame(game, frames + (size_t)count * FRAME_SIZE * FRAME_SIZE);
        count++;
    }
    return count;
}

int32_t scene_game_perform_action(SceneGame *game, int32_t action_id, int32_t action_x,
                                  int32_t action_y) {
    return scene_perform(game, action_id, action_x, action_y, NULL, 0);
}

int32_t scene_game_perform_action_frames(SceneGame *game, int32_t action_id,
                                         int32_t action_x, int32_t action_y,
                                         int8_t *frames, int32_t max_out) {
    return scene_perform(game, action_id, action_x, action_y, frames, max_out);
}

void scene_game_decode_action(const SceneGame *game, int32_t action, int32_t *action_id,
                              int32_t *x, int32_t *y) {
    if (!game->has_click) {
        *action_id = game->simple_actions[action];
        *x = 0;
        *y = 0;
        return;
    }
    int32_t click = action - game->num_simple;
    if (click >= 0) {
        *action_id = SCENE_ACTION6;
        *x = click % FRAME_SIZE;
        *y = click / FRAME_SIZE;
        return;
    }
    *action_id = game->num_simple ? game->simple_actions[action] : 0;
    *x = 0;
    *y = 0;
}

void scene_game_init(SceneGame *game) {
    memset(&game->engine, 0, sizeof(SceneEngineState));
    game->engine.status = SCENE_NOT_PLAYED;
    game->engine.action_id = SCENE_ACTION_RESET;
    if (game->hooks->zero_aux) game->hooks->zero_aux(game->aux);
    scene_table_clear(&game->scene, game->background);
    scene_game_perform_action(game, SCENE_ACTION_RESET, 0, 0);
}

int32_t scene_game_step(SceneGame *game, int32_t action, int8_t *frame, int32_t *reward,
                        uint8_t *terminated) {
    int32_t action_id, x, y;
    scene_game_decode_action(game, action, &action_id, &x, &y);
    int32_t before = game->engine.score;
    int32_t count = scene_perform(game, action_id, x, y, NULL, 0);
    *reward = game->engine.score - before;
    *terminated = game->engine.status == SCENE_WIN || game->engine.status == SCENE_GAME_OVER;
    if (frame) scene_game_frame(game, frame);
    return count;
}
