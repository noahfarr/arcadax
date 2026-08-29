#include "../game.h"
#include "sc25.h"

static void adapt_zero_aux(void *aux) { sc25_zero_aux((Sc25Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    sc25_on_set_level(&game->sprites, (const Sc25Static *)game->statics,
                      game->engine.level_index, (Sc25Aux *)game->aux);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    sc25_step_once(&game->sprites, &game->camera, (const Sc25Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   (Sc25Aux *)game->aux, &e->score, &e->status, &e->next_level,
                   &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    sc25_render_interface(frame, &game->sprites, &game->camera,
                          (const Sc25Static *)game->statics, game->engine.level_index,
                          (const Sc25Aux *)game->aux);
}

const Hooks sc25_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
