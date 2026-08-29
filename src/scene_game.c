#include "arc/scene_game.h"

#include <stdlib.h>
#include <string.h>

ArcSceneGame *arc_scene_game_new(const ArcSceneAtlas *atlas, int32_t num_slots, int32_t num_levels,
                          int8_t background, const ArcSceneHooks *hooks, void *aux,
                          void *statics, const int32_t *simple_actions, int32_t num_simple,
                          int32_t has_click, int32_t max_frames) {
    ArcSceneGame *game = calloc(1, sizeof(ArcSceneGame));
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
    game->num_actions = num_simple + (has_click ? ARC_FRAME_SIZE * ARC_FRAME_SIZE : 0);
    arc_scene_table_alloc(&game->scene, num_slots);
    arc_scene_scratch_init(&game->scratch, &game->atlas, num_slots);
    return game;
}

void arc_scene_game_free(ArcSceneGame *game) {
    arc_scene_table_free(&game->scene);
    arc_scene_scratch_free(&game->scratch);
    free(game);
}

void arc_scene_game_complete_action(ArcSceneGame *game) { game->engine.action_complete = 1; }

void arc_scene_game_lose(ArcSceneGame *game) { game->engine.status = SCENE_GAME_OVER; }

void arc_scene_game_next_level(ArcSceneGame *game) {
    int is_last = game->engine.level_index == game->num_levels - 1;
    game->engine.score += 1;
    game->engine.next_level = !is_last;
    if (is_last) game->engine.status = SCENE_WIN;
}

void arc_scene_game_set_level(ArcSceneGame *game, int32_t index) {
    arc_scene_table_clear(&game->scene, game->background);
    game->engine.level_index = index;
    game->engine.action_count = 0;
    if (game->hooks->build_level) game->hooks->build_level(game);
}

static void scene_full_reset(ArcSceneGame *game) {
    game->engine.score = 0;
    game->engine.full_reset = 1;
    arc_scene_game_set_level(game, 0);
    game->engine.status = SCENE_NOT_FINISHED;
}

static void scene_level_reset(ArcSceneGame *game) {
    arc_scene_game_set_level(game, game->engine.level_index);
    game->engine.status = SCENE_NOT_FINISHED;
}

static void scene_handle_reset(ArcSceneGame *game) {
    if (game->engine.action_count == 0 || game->engine.status == SCENE_WIN)
        scene_full_reset(game);
    else
        scene_level_reset(game);
}

static void scene_advance_level(ArcSceneGame *game) {
    arc_scene_game_set_level(game, game->engine.level_index + 1);
    game->engine.next_level = 0;
}

void arc_scene_game_frame(ArcSceneGame *game, int8_t *frame) {
    memcpy(frame, game->scene.frame, (size_t)ARC_FRAME_SIZE * ARC_FRAME_SIZE);
    if (game->hooks->render_interface) game->hooks->render_interface(game, frame);
}

static int32_t scene_perform(ArcSceneGame *game, int32_t action_id, int32_t action_x,
                             int32_t action_y, int8_t *frames, int32_t max_out) {
    ArcSceneEngineState *e = &game->engine;
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
        arc_scene_display(&game->scene, &game->atlas, &game->scratch, game->background);
        if (frames && count < max_out)
            arc_scene_game_frame(game, frames + (size_t)count * ARC_FRAME_SIZE * ARC_FRAME_SIZE);
        count++;
    }
    return count;
}

int32_t arc_scene_game_perform_action(ArcSceneGame *game, int32_t action_id, int32_t action_x,
                                  int32_t action_y) {
    return scene_perform(game, action_id, action_x, action_y, NULL, 0);
}

int32_t arc_scene_game_perform_action_frames(ArcSceneGame *game, int32_t action_id,
                                         int32_t action_x, int32_t action_y,
                                         int8_t *frames, int32_t max_out) {
    return scene_perform(game, action_id, action_x, action_y, frames, max_out);
}

void arc_scene_game_decode_action(const ArcSceneGame *game, int32_t action, int32_t *action_id,
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
        *x = click % ARC_FRAME_SIZE;
        *y = click / ARC_FRAME_SIZE;
        return;
    }
    *action_id = game->num_simple ? game->simple_actions[action] : 0;
    *x = 0;
    *y = 0;
}

void arc_scene_game_init(ArcSceneGame *game) {
    memset(&game->engine, 0, sizeof(ArcSceneEngineState));
    game->engine.status = SCENE_NOT_PLAYED;
    game->engine.action_id = SCENE_ACTION_RESET;
    if (game->hooks->zero_aux) game->hooks->zero_aux(game->aux);
    arc_scene_table_clear(&game->scene, game->background);
    arc_scene_game_perform_action(game, SCENE_ACTION_RESET, 0, 0);
}

int32_t arc_scene_game_step(ArcSceneGame *game, int32_t action, int8_t *frame, int32_t *reward,
                        uint8_t *terminated) {
    int32_t action_id, x, y;
    arc_scene_game_decode_action(game, action, &action_id, &x, &y);
    int32_t before = game->engine.score;
    int32_t count = scene_perform(game, action_id, x, y, NULL, 0);
    *reward = game->engine.score - before;
    *terminated = game->engine.status == SCENE_WIN || game->engine.status == SCENE_GAME_OVER;
    if (frame) arc_scene_game_frame(game, frame);
    return count;
}
