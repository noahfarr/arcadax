#include "../game.h"
#include "g50t.h"

static void adapt_zero_aux(void *aux) { g50t_zero_aux((G50tAux *)aux); }

static void adapt_on_set_level(Game *game) {
    g50t_on_set_level(&game->sprites, (const G50tStatic *)game->statics,
                      game->engine.level_index, (G50tAux *)game->aux);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    g50t_step_once(&game->sprites, (const G50tStatic *)game->statics, e->level_index,
                   e->action_id, e->action_x, e->action_y, e->action_count, (G50tAux *)game->aux,
                   &e->next_order, &e->score, &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    g50t_render_interface(frame, &game->sprites, &game->camera, (const G50tStatic *)game->statics,
                          game->engine.level_index, (const G50tAux *)game->aux);
}

const Hooks g50t_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
