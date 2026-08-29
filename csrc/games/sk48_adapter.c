#include "../game.h"
#include "sk48.h"

static void adapt_zero_aux(void *aux) { sk48_zero_aux((Sk48Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    sk48_on_set_level(&game->sprites, (const Sk48Static *)game->statics,
                      game->engine.level_index, (Sk48Aux *)game->aux);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    sk48_step_once(&game->sprites, &game->camera, (const Sk48Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y, e->action_count,
                   (Sk48Aux *)game->aux, &e->score, &e->status, &e->next_level,
                   &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    sk48_render_interface(frame, (const Sk48Aux *)game->aux);
}

const Hooks sk48_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
