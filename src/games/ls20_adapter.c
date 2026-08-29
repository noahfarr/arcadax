#include "arc/game.h"
#include "ls20.h"

static void adapt_zero_aux(void *aux) { ls20_zero_aux((Ls20Aux *)aux); }

static void adapt_on_set_level(ArcGame *game) {
    ls20_on_set_level(&game->sprites, (const Ls20Static *)game->statics,
                      game->engine.level_index, (Ls20Aux *)game->aux,
                      &game->engine.next_order);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    ls20_step_once(&game->sprites, (const Ls20Static *)game->statics, e->level_index,
                   e->action_id, (Ls20Aux *)game->aux, &e->next_order, &e->score,
                   &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    ls20_render_interface(frame, &game->sprites, (const Ls20Static *)game->statics,
                          game->engine.level_index, (const Ls20Aux *)game->aux);
}

const ArcHooks ls20_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
