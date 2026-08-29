#include "../game.h"
#include "tu93.h"

static void adapt_zero_aux(void *aux) { tu93_zero_aux((Tu93Aux *)aux); }

static void adapt_on_set_level(Game *game) {
    tu93_on_set_level(&game->sprites, (const Tu93Static *)game->statics,
                      game->engine.level_index, (Tu93Aux *)game->aux);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    tu93_step_once(&game->sprites, &game->camera, (const Tu93Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   (Tu93Aux *)game->aux, &e->score, &e->status, &e->next_level,
                   &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    tu93_render_interface(frame, &game->sprites, &game->camera,
                          (const Tu93Static *)game->statics, game->engine.level_index,
                          (const Tu93Aux *)game->aux);
}

const Hooks tu93_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
