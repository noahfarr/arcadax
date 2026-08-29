#include "../game.h"
#include "cn04.h"

static void adapt_zero_aux(void *aux) { (void)aux; }

static void adapt_on_set_level(Game *game) {
    cn04_on_set_level(&game->sprites, &game->camera, (const Cn04Static *)game->statics,
                      game->engine.level_index, (Cn04Aux *)game->aux);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    cn04_step_once(&game->sprites, &game->camera, (const Cn04Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   e->action_count, (Cn04Aux *)game->aux, &e->score, &e->status,
                   &e->next_level, &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    cn04_render_interface(frame, (const Cn04Static *)game->statics,
                          game->engine.level_index, game->engine.action_count);
}

const Hooks cn04_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
