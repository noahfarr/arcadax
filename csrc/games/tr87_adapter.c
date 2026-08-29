#include "../game.h"
#include "tr87.h"

static void adapt_zero_aux(void *aux) { tr87_zero_aux((Tr87Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    tr87_on_set_level(&game->sprites, (const Tr87Static *)game->statics,
                      game->engine.level_index, (Tr87Aux *)game->aux);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    tr87_step_once(&game->sprites, (const Tr87Static *)game->statics, e->level_index,
                   e->action_id, (Tr87Aux *)game->aux, &e->score, &e->status, &e->next_level,
                   &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    tr87_render_interface(frame, (const Tr87Static *)game->statics, game->engine.level_index,
                          (const Tr87Aux *)game->aux);
}

const Hooks tr87_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
