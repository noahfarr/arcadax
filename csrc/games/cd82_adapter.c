#include "../game.h"
#include "cd82.h"

static void adapt_zero_aux(void *aux) { cd82_zero_aux((Cd82Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    cd82_on_set_level(&game->sprites, (const Cd82Static *)game->statics,
                      game->engine.level_index, (Cd82Aux *)game->aux,
                      &game->engine.next_order);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    cd82_step_once(&game->sprites, &game->camera, (const Cd82Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   e->action_count, (Cd82Aux *)game->aux, &e->next_order, &e->score,
                   &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    cd82_render_interface(frame, game->engine.action_count);
}

const Hooks cd82_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
