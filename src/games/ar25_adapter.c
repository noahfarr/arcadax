#include "arc/game.h"
#include "ar25.h"

static void adapt_zero_aux(void *aux) { ar25_zero_aux((Ar25Aux *)aux); }

static void adapt_on_set_level(ArcGame *game) {
    ar25_on_set_level(&game->sprites, (const Ar25Static *)game->statics,
                      game->engine.level_index, (Ar25Aux *)game->aux);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    ar25_step_once(&game->sprites, &game->camera, (const Ar25Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   (Ar25Aux *)game->aux, &e->score, &e->status, &e->next_level,
                   &e->action_complete);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    ar25_render_interface(frame, &game->sprites, (const Ar25Static *)game->statics,
                          game->engine.level_index, (const Ar25Aux *)game->aux);
}

const ArcHooks ar25_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
