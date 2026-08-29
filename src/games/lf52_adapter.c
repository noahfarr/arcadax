#include "lf52.h"

static void adapt_zero_aux(void *aux) { lf52_zero_aux((Lf52Aux *)aux); }

static void adapt_build_level(ArcSceneGame *game) { lf52_build_level(game); }

static void adapt_step_once(ArcSceneGame *game) { lf52_step_once(game); }

static void adapt_render_interface(ArcSceneGame *game, int8_t *frame) { lf52_render_interface(game, frame); }

const ArcSceneHooks lf52_hooks = {adapt_zero_aux, adapt_build_level, adapt_step_once, adapt_render_interface};
