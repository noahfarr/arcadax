#include "../game.h"
#include "ft09.h"

static void adapt_zero_aux(void *aux) { ft09_zero_aux((Ft09Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    ft09_on_set_level(&game->sprites, (const Ft09Static *)game->statics,
                      game->engine.level_index, (Ft09Aux *)game->aux);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    ft09_step_once(&game->sprites, &game->camera, (const Ft09Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   (Ft09Aux *)game->aux, &e->score, &e->status, &e->next_level,
                   &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    const Ft09Aux *aux = (const Ft09Aux *)game->aux;
    ft09_render_interface(frame, (const Ft09Static *)game->statics,
                          game->engine.level_index, aux->steps);
}

const Hooks ft09_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
