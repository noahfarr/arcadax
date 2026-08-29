#include "../scene_game.h"
#include "bp35.h"

static void adapt_zero_aux(void *aux) { bp35_zero_aux((Bp35Aux *)aux); }

static void adapt_build_level(SceneGame *game) {
    bp35_build_level(&game->scene, (const Bp35Static *)game->statics, game->engine.level_index,
                     game->engine.action_id, game->engine.action_x, game->engine.action_y,
                     (uint8_t)game->engine.next_level, (Bp35Aux *)game->aux);
}

static void adapt_step_once(SceneGame *game) {
    SceneEngineState *e = &game->engine;
    bp35_step_once(&game->scene, (const Bp35Static *)game->statics, e->level_index, e->action_id,
                   e->action_x, e->action_y, (Bp35Aux *)game->aux, &e->score, &e->status,
                   &e->next_level, &e->action_complete);
}

static void adapt_render_interface(SceneGame *game, int8_t *frame) {
    bp35_render_interface(frame, (const Bp35Static *)game->statics, game->engine.level_index,
                          (const Bp35Aux *)game->aux);
}

const SceneHooks bp35_hooks = {adapt_zero_aux, adapt_build_level, adapt_step_once,
                               adapt_render_interface};
