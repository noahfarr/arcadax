#include "../game.h"
#include "ka59.h"

static void adapt_zero_aux(void *aux) { (void)aux; }

static void adapt_on_set_level(Game *game) {
    ka59_on_set_level(&game->sprites, (const Ka59Static *)game->statics,
                      game->engine.level_index, (Ka59Aux *)game->aux,
                      &game->engine.next_order);
}

static void adapt_step_once(Game *game) {
    EngineState *e = &game->engine;
    ka59_step_once(&game->sprites, &game->camera, (const Ka59Static *)game->statics,
                   e->level_index, e->action_id, e->action_x, e->action_y,
                   e->action_count, (Ka59Aux *)game->aux, &e->next_order, &e->score,
                   &e->status, &e->next_level, &e->action_complete);
}

static void adapt_render_interface(Game *game, int8_t *frame) {
    ka59_render_interface(frame, (const Ka59Static *)game->statics,
                          game->engine.level_index, (const Ka59Aux *)game->aux);
}

const Hooks ka59_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
