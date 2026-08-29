#include "../game.h"
#include "sb26.h"

static void adapt_zero_aux(void *aux) { sb26_zero_aux((Sb26Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    sb26_on_set_level(&game->sprites, (const Sb26Static *)game->statics,
                      game->engine.level_index, (Sb26Aux *)game->aux,
                      &game->engine.next_order);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    sb26_step_once(&game->sprites, &game->camera, &game->scratch,
                   (const Sb26Static *)game->statics, e->level_index, e->action_id,
                   e->action_x, e->action_y, e->action_count, (Sb26Aux *)game->aux,
                   &e->next_order, &e->score, &e->status, &e->next_level,
                   &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    sb26_render_interface(frame, &game->sprites, &game->camera, &game->scratch,
                          (const Sb26Static *)game->statics, (const Sb26Aux *)game->aux);
}

const Hooks sb26_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
