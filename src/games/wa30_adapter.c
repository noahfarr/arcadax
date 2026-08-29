#include "arc/game.h"
#include "wa30.h"

static void adapt_zero_aux(void *aux) { (void)aux; }

static void adapt_on_set_level(ArcGame *game) {
    wa30_on_set_level((const Wa30Static *)game->statics, game->engine.level_index,
                      (Wa30Aux *)game->aux);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    wa30_step_once(&game->sprites, (const Wa30Static *)game->statics, e->level_index,
                   e->action_id, (Wa30Aux *)game->aux, &e->score, &e->status,
                   &e->next_level, &e->action_complete);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    wa30_render_interface(frame, (const Wa30Static *)game->statics,
                          game->engine.level_index, (const Wa30Aux *)game->aux);
}

const ArcHooks wa30_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
