#include "arc/game.h"
#include "vc33.h"

static void adapt_zero_aux(void *aux) { vc33_zero_aux((Vc33Aux *)aux); }

static void adapt_on_set_level(ArcGame *game) {
    vc33_on_set_level((const Vc33Static *)game->statics, game->engine.level_index,
                      (Vc33Aux *)game->aux);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    vc33_step_once(&game->sprites, &game->camera, (const Vc33Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   (Vc33Aux *)game->aux, &e->next_order, &e->score, &e->status,
                   &e->next_level, &e->action_complete);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    vc33_render_interface(frame, (const Vc33Static *)game->statics,
                          game->engine.level_index, (const Vc33Aux *)game->aux);
}

const ArcHooks vc33_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
