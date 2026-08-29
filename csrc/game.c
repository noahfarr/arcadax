#include "game.h"

#include <stdlib.h>
#include <string.h>

static void load_level(Game *game, int32_t index) {
    const LevelData *d = game->levels;
    int32_t n = d->num_slots;
    size_t off = (size_t)index * n;
    memcpy(game->sprites.x, d->x + off, sizeof(int32_t) * n);
    memcpy(game->sprites.y, d->y + off, sizeof(int32_t) * n);
    memcpy(game->sprites.h, d->h + off, sizeof(int32_t) * n);
    memcpy(game->sprites.w, d->w + off, sizeof(int32_t) * n);
    memcpy(game->sprites.layer, d->layer + off, sizeof(int32_t) * n);
    memcpy(game->sprites.order, d->order + off, sizeof(int32_t) * n);
    memcpy(game->sprites.interaction, d->interaction + off, n);
    memcpy(game->sprites.blocking, d->blocking + off, n);
    memcpy(game->sprites.alive, d->alive + off, n);
    memcpy(game->sprites.tags, d->tags + off * (size_t)d->num_tags,
           (size_t)n * d->num_tags);
    memset(game->sprites.overridden, 0, n);
    game->atlas.pixels = d->pixels + off * (size_t)d->ph * d->pw;
    if (game->atlas.pixels != game->bbox_atlas) {
        sprites_recompute_bbox(&game->sprites);
        game->bbox_atlas = game->atlas.pixels;
    }
}

Game *game_new(const LevelData *levels, const Hooks *hooks, void *aux,
               void *statics, const int32_t *simple_actions, int32_t num_simple,
               int32_t has_click, int32_t max_frames) {
    Game *game = calloc(1, sizeof(Game));
    int32_t n = levels->num_slots;
    game->levels = levels;
    game->hooks = hooks;
    game->aux = aux;
    game->statics = statics;
    game->simple_actions = simple_actions;
    game->num_simple = num_simple;
    game->has_click = has_click;
    game->max_frames = max_frames;
    game->num_actions = num_simple + (has_click ? FRAME_SIZE * FRAME_SIZE : 0);

    game->atlas.num_slots = n;
    game->atlas.num_tags = levels->num_tags;
    game->atlas.ph = levels->ph;
    game->atlas.pw = levels->pw;
    game->atlas.pixels = levels->pixels;

    Sprites *s = &game->sprites;
    s->atlas = &game->atlas;
    s->x = calloc(n, sizeof(int32_t));
    s->y = calloc(n, sizeof(int32_t));
    s->h = calloc(n, sizeof(int32_t));
    s->w = calloc(n, sizeof(int32_t));
    s->layer = calloc(n, sizeof(int32_t));
    s->order = calloc(n, sizeof(int32_t));
    s->interaction = calloc(n, 1);
    s->blocking = calloc(n, 1);
    s->alive = calloc(n, 1);
    s->tags = calloc((size_t)n * levels->num_tags, 1);
    s->pixels = calloc((size_t)n * levels->ph * levels->pw, 1);
    s->overridden = calloc(n, 1);
    s->bbox = calloc((size_t)n * 4, sizeof(int32_t));
    render_scratch_init(&game->scratch, &game->atlas);
    return game;
}

void game_free(Game *game) {
    Sprites *s = &game->sprites;
    free(s->x); free(s->y); free(s->h); free(s->w);
    free(s->layer); free(s->order); free(s->interaction);
    free(s->blocking); free(s->alive); free(s->tags);
    free(s->pixels); free(s->overridden); free(s->bbox);
    render_scratch_free(&game->scratch);
    free(game);
}

void game_complete_action(Game *game) { game->engine.action_complete = 1; }

void game_lose(Game *game) { game->engine.status = GAME_OVER; }

void game_next_level(Game *game) {
    int is_last = game->engine.level_index == game->levels->num_levels - 1;
    game->engine.score += 1;
    game->engine.next_level = !is_last;
    if (is_last) game->engine.status = WIN;
}

void game_set_level(Game *game, int32_t index) {
    const LevelData *d = game->levels;
    load_level(game, index);
    game->camera.width = d->grid_size[index * 2];
    game->camera.height = d->grid_size[index * 2 + 1];
    game->engine.level_index = index;
    game->engine.action_count = 0;
    game->engine.next_order = d->num_slots;
    if (game->hooks->on_set_level) game->hooks->on_set_level(game);
}

static void initial_camera(Game *game) {
    const LevelData *d = game->levels;
    game->camera.x = 0;
    game->camera.y = 0;
    game->camera.width = d->grid_size[0];
    game->camera.height = d->grid_size[1];
    game->camera.background = d->background;
    game->camera.letter_box = d->letter_box;
}

static void full_reset(Game *game) {
    game->engine.score = 0;
    game->engine.full_reset = 1;
    initial_camera(game);
    game_set_level(game, 0);
    game->engine.status = NOT_FINISHED;
}

static void level_reset(Game *game) {
    game_set_level(game, game->engine.level_index);
    game->engine.status = NOT_FINISHED;
}

static void handle_reset(Game *game) {
    if (game->engine.action_count == 0 || game->engine.status == WIN)
        full_reset(game);
    else
        level_reset(game);
}

static void advance_level(Game *game) {
    game_set_level(game, game->engine.level_index + 1);
    game->engine.next_level = 0;
}

static int32_t perform(Game *game, int32_t action_id, int32_t action_x,
                       int32_t action_y, int8_t *frames, int32_t max_out) {
    EngineState *e = &game->engine;
    e->full_reset = 0;
    int is_reset = action_id == ACTION_RESET;
    int terminal = e->status == WIN || e->status == GAME_OVER;
    if (is_reset) handle_reset(game);
    if (!is_reset && terminal) return 0;

    e->status = NOT_FINISHED;
    e->action_id = action_id;
    e->action_x = action_x;
    e->action_y = action_y;
    e->action_complete = 0;
    if (!is_reset) e->action_count += 1;

    int32_t count = 0;
    while (!(!e->next_level && e->action_complete) && count < game->max_frames) {
        if (e->next_level)
            advance_level(game);
        else
            game->hooks->step_once(game);
        if (frames && count < max_out)
            game_frame(game, frames + (size_t)count * FRAME_SIZE * FRAME_SIZE);
        count++;
    }
    return count;
}

int32_t game_perform_action(Game *game, int32_t action_id, int32_t action_x,
                            int32_t action_y) {
    return perform(game, action_id, action_x, action_y, NULL, 0);
}

int32_t game_perform_action_frames(Game *game, int32_t action_id, int32_t action_x,
                                   int32_t action_y, int8_t *frames,
                                   int32_t max_out) {
    return perform(game, action_id, action_x, action_y, frames, max_out);
}

void game_decode_action(const Game *game, int32_t action, int32_t *action_id,
                        int32_t *x, int32_t *y) {
    if (!game->has_click) {
        *action_id = game->simple_actions[action];
        *x = 0;
        *y = 0;
        return;
    }
    int32_t click = action - game->num_simple;
    if (click >= 0) {
        *action_id = ACTION6;
        *x = click % FRAME_SIZE;
        *y = click / FRAME_SIZE;
        return;
    }
    *action_id = game->num_simple ? game->simple_actions[action] : 0;
    *x = 0;
    *y = 0;
}

void game_frame(Game *game, int8_t *frame) {
    render(&game->sprites, &game->camera, &game->scratch, frame);
    if (game->hooks->render_interface) game->hooks->render_interface(game, frame);
}

void game_init(Game *game) {
    memset(&game->engine, 0, sizeof(EngineState));
    game->engine.status = NOT_PLAYED;
    game->engine.action_id = ACTION_RESET;
    if (game->hooks->zero_aux) game->hooks->zero_aux(game->aux);
    initial_camera(game);
    game_set_level(game, 0);
    game_perform_action(game, ACTION_RESET, 0, 0);
}

int32_t game_step(Game *game, int32_t action, int8_t *frame, int32_t *reward,
                  uint8_t *terminated) {
    int32_t action_id, x, y;
    game_decode_action(game, action, &action_id, &x, &y);
    int32_t before = game->engine.score;
    int32_t count = perform(game, action_id, x, y, NULL, 0);
    *reward = game->engine.score - before;
    *terminated = game->engine.status == WIN || game->engine.status == GAME_OVER;
    if (frame) game_frame(game, frame);
    return count;
}
