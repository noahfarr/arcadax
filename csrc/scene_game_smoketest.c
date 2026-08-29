#include "scene_game.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int32_t calls;
    int32_t total_calls;
    int32_t complete_after;
    int32_t win_after_complete;
    int32_t lose_after_complete;
} StubAux;

static void stub_zero_aux(void *aux) {
    StubAux *a = (StubAux *)aux;
    a->calls = 0;
    a->total_calls = 0;
    a->complete_after = 1;
    a->win_after_complete = 0;
    a->lose_after_complete = 0;
}

static void stub_build_level(SceneGame *game) { (void)game; }

static void stub_step_once(SceneGame *game) {
    StubAux *aux = (StubAux *)game->aux;
    aux->calls += 1;
    aux->total_calls += 1;
    if (aux->calls >= aux->complete_after) {
        if (aux->win_after_complete) scene_game_next_level(game);
        if (aux->lose_after_complete) scene_game_lose(game);
        scene_game_complete_action(game);
        aux->calls = 0;
    }
}

static void stub_render_interface(SceneGame *game, int8_t *frame) {
    (void)game;
    (void)frame;
}

static const SceneHooks stub_hooks = {stub_zero_aux, stub_build_level, stub_step_once,
                                      stub_render_interface};

int main(void) {
    SceneAtlas atlas = {0};
    int8_t dummy_pixel = -1;
    atlas.pixels = &dummy_pixel;
    atlas.size = 1;
    atlas.ph = 1;
    atlas.pw = 1;

    StubAux aux;
    int32_t simple_actions[] = {1, 2, 3, 4, 5};
    SceneGame *game = scene_game_new(&atlas, 4, 3, 10, &stub_hooks, &aux, NULL,
                                     simple_actions, 5, 1, 50);
    scene_game_init(game);

    assert(game->engine.level_index == 0);
    assert(game->engine.status == SCENE_NOT_FINISHED);
    assert(game->engine.score == 0);
    assert(game->engine.action_count == 0);

    aux.complete_after = 1;
    aux.win_after_complete = 0;
    aux.lose_after_complete = 0;
    int32_t count = scene_game_perform_action(game, 1, 0, 0);
    assert(count == 1);
    assert(game->engine.action_count == 1);
    assert(game->engine.level_index == 0);

    aux.win_after_complete = 1;
    count = scene_game_perform_action(game, 1, 0, 0);
    assert(count == 2);
    assert(game->engine.level_index == 1);
    assert(game->engine.score == 1);
    assert(game->engine.status == SCENE_NOT_FINISHED);
    aux.win_after_complete = 0;

    aux.lose_after_complete = 1;
    int32_t total_before = aux.total_calls;
    count = scene_game_perform_action(game, 1, 0, 0);
    assert(count == 1);
    assert(game->engine.status == SCENE_GAME_OVER);
    aux.lose_after_complete = 0;

    count = scene_game_perform_action(game, 1, 0, 0);
    assert(count == 0);
    assert(aux.total_calls == total_before + 1);
    assert(game->engine.status == SCENE_GAME_OVER);

    int32_t level_before_reset = game->engine.level_index;
    int32_t score_before_reset = game->engine.score;
    scene_game_perform_action(game, 0, 0, 0);
    assert(game->engine.level_index == level_before_reset);
    assert(game->engine.score == score_before_reset);
    assert(game->engine.status == SCENE_NOT_FINISHED);
    assert(game->engine.action_count == 0);

    aux.win_after_complete = 1;
    scene_game_perform_action(game, 1, 0, 0);
    assert(game->engine.level_index == 2);
    assert(game->engine.status == SCENE_NOT_FINISHED);
    count = scene_game_perform_action(game, 1, 0, 0);
    assert(count == 1);
    assert(game->engine.status == SCENE_WIN);
    assert(game->engine.score == 3);
    aux.win_after_complete = 0;

    scene_game_perform_action(game, 0, 0, 0);
    assert(game->engine.level_index == 0);
    assert(game->engine.score == 0);
    assert(game->engine.status == SCENE_NOT_FINISHED);

    aux.complete_after = 1000;
    count = scene_game_perform_action(game, 1, 0, 0);
    assert(count == game->max_frames);
    assert(game->engine.action_complete == 0);

    scene_game_free(game);
    printf("scene_game smoke test: PASS\n");
    return 0;
}
