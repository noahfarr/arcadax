#include "arc/game.h"
#include "sp80.h"

static void adapt_zero_aux(void *aux) { (void)aux; }

static void adapt_on_set_level(ArcGame *game) {
    sp80_on_set_level(&game->sprites, (const Sp80Static *)game->statics,
                      game->engine.level_index, (Sp80Aux *)game->aux);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    sp80_step_once(&game->sprites, &game->camera, (const Sp80Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   e->action_count, (Sp80Aux *)game->aux, &e->next_order, &e->score,
                   &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    sp80_render_interface(frame, &game->sprites, &game->camera,
                          (const Sp80Static *)game->statics, game->engine.level_index,
                          (const Sp80Aux *)game->aux);
}

const ArcHooks sp80_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
