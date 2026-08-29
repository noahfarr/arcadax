#include "arc/game.h"
#include "tn36.h"

static void adapt_zero_aux(void *aux) { tn36_zero_aux((Tn36Aux *)aux); }

static void adapt_on_set_level(ArcGame *game) {
    tn36_on_set_level(&game->sprites, (const Tn36Static *)game->statics,
                      game->engine.level_index, (Tn36Aux *)game->aux);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    tn36_step_once(&game->sprites, &game->camera,
                   (const Tn36Static *)game->statics, e->level_index,
                   e->action_id, e->action_x, e->action_y, e->action_count,
                   (Tn36Aux *)game->aux, &e->next_order, &e->score, &e->status,
                   &e->next_level, &e->action_complete);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    tn36_render_interface(frame, &game->sprites, &game->camera,
                          (const Tn36Static *)game->statics,
                          game->engine.level_index, (const Tn36Aux *)game->aux);
}

const ArcHooks tn36_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
