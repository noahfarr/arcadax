#include "arc/game.h"
#include "s5i5.h"

static void adapt_zero_aux(void *aux) { (void)aux; }

static void adapt_on_set_level(ArcGame *game) {
    s5i5_on_set_level((S5i5Aux *)game->aux, (const S5i5Static *)game->statics,
                      game->engine.level_index);
}

static void adapt_step_once(ArcGame *game) {
    ArcEngineState *e = &game->engine;
    S5i5Engine eng;
    eng.level_index = e->level_index;
    eng.action_id = e->action_id;
    eng.action_x = e->action_x;
    eng.action_y = e->action_y;
    eng.score = e->score;
    eng.status = e->status;
    eng.action_complete = e->action_complete;
    eng.next_level = e->next_level;

    s5i5_step_once(&game->sprites, &game->camera, &eng, (S5i5Aux *)game->aux,
                   (const S5i5Static *)game->statics);

    e->score = eng.score;
    e->status = eng.status;
    e->action_complete = eng.action_complete;
    e->next_level = eng.next_level;
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    s5i5_render_interface(frame, (const S5i5Aux *)game->aux,
                          (const S5i5Static *)game->statics, game->engine.level_index);
}

const ArcHooks s5i5_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
