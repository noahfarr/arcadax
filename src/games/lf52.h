#ifndef ARC_GAMES_LF52_H
#define ARC_GAMES_LF52_H

#include "arc/scene_game.h"

#define LF52_LEVEL_COUNT 10
#define LF52_STATIC_SLOT_COUNT 110
#define LF52_PEG_SLOT_COUNT 16
#define LF52_HEART_SLOT_COUNT 4
#define LF52_WALL_TILE_SLOT_COUNT 10
#define LF52_GRID_MAX_WIDTH 28
#define LF52_GRID_MAX_HEIGHT 14
#define LF52_UNDO_HISTORY_DEPTH 1000
#define LF52_PULSE_TICK_COUNT 7
#define LF52_CAPTURE_FADE_TICK_COUNT 3
#define LF52_TRIGGER_SLOT_COUNT 7

typedef struct {
    int32_t peg_grid_x[LF52_PEG_SLOT_COUNT];
    int32_t peg_grid_y[LF52_PEG_SLOT_COUNT];
    uint8_t peg_alive[LF52_PEG_SLOT_COUNT];
    int32_t wall_tile_grid_x[LF52_WALL_TILE_SLOT_COUNT];
    int32_t wall_tile_grid_y[LF52_WALL_TILE_SLOT_COUNT];
    int32_t selected_peg;
    uint8_t heart_direction_valid[4];
    int32_t heart_blink_tick;
    int32_t camera_offset_x;
    int32_t camera_offset_y;
    uint8_t pegs_show_revealed_image;
    uint8_t reveal_button_visible;
    uint8_t reveal_button_was_visible_before_this_action;
    uint8_t action_in_progress;
    int32_t action_tick;
    int32_t action_tick_count;
    int32_t action_kind;
    int32_t jump_mover_peg;
    int32_t jump_captured_peg;
    int32_t jump_mover_start_x;
    int32_t jump_mover_start_y;
    int32_t jump_direction_x;
    int32_t jump_direction_y;
    uint8_t jump_triggers_win;
    uint8_t jump_shows_trail_ghost;
    uint8_t bump_moving_peg[LF52_PEG_SLOT_COUNT];
    int32_t bump_peg_start_x[LF52_PEG_SLOT_COUNT];
    int32_t bump_peg_start_y[LF52_PEG_SLOT_COUNT];
    uint8_t bump_wall_tile_moving[LF52_WALL_TILE_SLOT_COUNT];
    int32_t bump_wall_tile_start_x[LF52_WALL_TILE_SLOT_COUNT];
    int32_t bump_wall_tile_start_y[LF52_WALL_TILE_SLOT_COUNT];
    int32_t bump_direction_index;
    int32_t bump_direction_x;
    int32_t bump_direction_y;
    uint8_t bump_single_occupant;
    uint8_t bump_pans_camera;
    int32_t bump_camera_start_x;
    int32_t bump_camera_start_y;
    int32_t bump_camera_delta_x;
    int32_t bump_camera_delta_y;
    uint8_t skip_hud_sweep;
    int32_t stalemate_action_count;
    int32_t undo_peg_grid_x[LF52_UNDO_HISTORY_DEPTH][LF52_PEG_SLOT_COUNT];
    int32_t undo_peg_grid_y[LF52_UNDO_HISTORY_DEPTH][LF52_PEG_SLOT_COUNT];
    uint8_t undo_peg_alive[LF52_UNDO_HISTORY_DEPTH][LF52_PEG_SLOT_COUNT];
    int32_t undo_wall_tile_grid_x[LF52_UNDO_HISTORY_DEPTH][LF52_WALL_TILE_SLOT_COUNT];
    int32_t undo_wall_tile_grid_y[LF52_UNDO_HISTORY_DEPTH][LF52_WALL_TILE_SLOT_COUNT];
    int32_t undo_selected_peg[LF52_UNDO_HISTORY_DEPTH];
    uint8_t undo_heart_direction_valid[LF52_UNDO_HISTORY_DEPTH][4];
    int32_t undo_camera_offset_x[LF52_UNDO_HISTORY_DEPTH];
    int32_t undo_camera_offset_y[LF52_UNDO_HISTORY_DEPTH];
    uint8_t undo_pegs_show_revealed_image[LF52_UNDO_HISTORY_DEPTH];
    uint8_t undo_reveal_button_visible[LF52_UNDO_HISTORY_DEPTH];
    int32_t undo_stalemate_action_count[LF52_UNDO_HISTORY_DEPTH];
    int32_t undo_depth;
} Lf52Aux;

typedef struct {
    const int32_t *static_image;
    const int32_t *static_x;
    const int32_t *static_y;
    const int32_t *static_layer;
    const int32_t *static_wall_tile_index;
    const int32_t *static_wall_tile_local_x;
    const int32_t *static_wall_tile_local_y;
    const int32_t *wall_tile_grid_x_initial;
    const int32_t *wall_tile_grid_y_initial;
    const uint8_t *wall_tile_is_landable_double;
    const uint8_t *wall_tile_has_jumpable_pin;
    const int32_t *wall_tile_count;
    const int32_t *peg_grid_x_initial;
    const int32_t *peg_grid_y_initial;
    const int32_t *peg_color;
    const uint8_t *peg_alive_initial;
    const int32_t *grid_width;
    const int32_t *grid_height;
    const int32_t *offset_x;
    const int32_t *offset_y;
    const uint8_t *landable_single;
    const uint8_t *landable_double;
    const uint8_t *jumpable_pin;
    const uint8_t *wall_present;
    const int32_t *peg_base_image;
    const int32_t *peg_pulse_image;
    const int32_t *peg_capture_fade_image;
    const int32_t *revealed_peg_capture_fade_image;
    const int32_t *heart_image;
    int32_t ring_image;
    const int32_t *dust_image;
    int32_t revealed_peg_image;
    int32_t reveal_button_image;
    const int32_t *win_blue_offset;
    const int32_t *win_target_peg_count;
    const int32_t *stalemate_action_budget;
    const int32_t *jump_landing_reveal_trigger_x;
    const int32_t *jump_landing_reveal_trigger_y;
    const int32_t *jump_landing_reveal_trigger_max_remaining;
    const uint8_t *jump_landing_reveal_trigger_valid;
} Lf52Static;

void lf52_zero_aux(Lf52Aux *aux);
void lf52_build_level(ArcSceneGame *game);
void lf52_step_once(ArcSceneGame *game);
void lf52_render_interface(ArcSceneGame *game, int8_t *frame);

#endif
