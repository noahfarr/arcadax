#include "arc/game.h"
#include "r11l.h"

static void adapt_zero_aux(void *aux) { r11l_zero_aux((R11lAux *)aux); }

static void adapt_on_set_level(ArcGame *game) {
    r11l_on_set_level(&game->sprites, (const R11lStatic *)game->statics,
                      game->engine.level_index, (R11lAux *)game->aux,
                      &game->engine.next_order);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    r11l_step_once(&game->sprites, &game->camera, (const R11lStatic *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   e->action_count, (R11lAux *)game->aux, &e->next_order, &e->score,
                   &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    r11l_render_interface(frame, &game->sprites, &game->camera,
                          (const R11lStatic *)game->statics, game->engine.level_index,
                          (const R11lAux *)game->aux, game->engine.action_count);
}

const ArcHooks r11l_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
