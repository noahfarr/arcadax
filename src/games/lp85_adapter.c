#include "arc/game.h"
#include "lp85.h"

#include <stdlib.h>

static int32_t *scratch_sources;
static int32_t *scratch_hits;
static const Lp85Static *scratch_for;

static void adapt_zero_aux(void *aux) { lp85_zero_aux((Lp85Aux *)aux); }

static Lp85Engine engine_from_game(const ArcGame *game) {
    Lp85Engine e;
    e.camera = game->camera;
    e.level_index = game->engine.level_index;
    e.score = game->engine.score;
    e.status = game->engine.status;
    e.action_id = game->engine.action_id;
    e.action_x = game->engine.action_x;
    e.action_y = game->engine.action_y;
    e.action_complete = game->engine.action_complete;
    e.next_level = game->engine.next_level;
    return e;
}

static void engine_to_game(ArcGame *game, const Lp85Engine *e) {
    game->camera = e->camera;
    game->engine.level_index = e->level_index;
    game->engine.score = e->score;
    game->engine.status = e->status;
    game->engine.action_id = e->action_id;
    game->engine.action_x = e->action_x;
    game->engine.action_y = e->action_y;
    game->engine.action_complete = e->action_complete;
    game->engine.next_level = e->next_level;
}

static void ensure_scratch(const Lp85Static *st) {
    if (scratch_for == st) return;
    free(scratch_sources);
    free(scratch_hits);
    scratch_sources = calloc((size_t)st->max_ring, sizeof(int32_t));
    scratch_hits = calloc((size_t)st->max_buttons, sizeof(int32_t));
    scratch_for = st;
}

static void adapt_on_set_level(ArcGame *game) {
    const Lp85Static *st = (const Lp85Static *)game->statics;
    ensure_scratch(st);
    Lp85Engine e = engine_from_game(game);
    lp85_on_set_level(st, &e, (Lp85Aux *)game->aux);
    engine_to_game(game, &e);
}

static void adapt_step_once(ArcGame *game) {
    const Lp85Static *st = (const Lp85Static *)game->statics;
    ensure_scratch(st);
    Lp85Scratch scratch = {scratch_sources, scratch_hits};
    Lp85Engine e = engine_from_game(game);
    lp85_step_once(st, &game->sprites, &scratch, &e, (Lp85Aux *)game->aux);
    engine_to_game(game, &e);
}

static void adapt_render_interface(ArcGame *game, int8_t *frame) {
    const Lp85Static *st = (const Lp85Static *)game->statics;
    Lp85Engine e = engine_from_game(game);
    lp85_render_interface(st, &e, (const Lp85Aux *)game->aux, frame);
}

const ArcHooks lp85_hooks = {adapt_zero_aux, adapt_on_set_level, adapt_step_once,
                          adapt_render_interface};
