#include "../game.h"
#include "su15.h"

static void adapt_zero_aux(void *aux) { su15_zero_aux((Su15Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    su15_on_set_level(&game->sprites, (const Su15Static *)game->statics,
                      game->engine.level_index, (Su15Aux *)game->aux, &game->engine.next_order);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    su15_step_once(&game->sprites, &game->camera, (const Su15Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y, (Su15Aux *)game->aux,
                   &e->next_order, &e->score, &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    su15_render_interface(frame, (const Su15Static *)game->statics, game->engine.level_index,
                          (const Su15Aux *)game->aux);
}

const Hooks su15_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
