#ifndef ARCADAX_GAMES_BP35_H
#define ARCADAX_GAMES_BP35_H

#include "../scene.h"

#define BP35_NUM_LEVELS 9
#define BP35_WMAX 11
#define BP35_HMAX 50
#define BP35_PITCH 6
#define BP35_MAX_FRAMES 57
#define BP35_MAX_HIST 136
#define BP35_NMAX 21
#define BP35_NUM_TERRAIN 5
#define BP35_NUM_IMAGES 25
#define BP35_CELL_SLOTS (BP35_HMAX * BP35_WMAX)
#define BP35_PLAYER_SLOT BP35_CELL_SLOTS
#define BP35_TERRAIN_SLOT0 (BP35_PLAYER_SLOT + 1)
#define BP35_ANIM_SLOT0 (BP35_TERRAIN_SLOT0 + BP35_NUM_TERRAIN)
#define BP35_NUM_SLOTS (BP35_ANIM_SLOT0 + 5)

enum {
    BP35_NONE = 0,
    BP35_BREAKABLE = 1,
    BP35_SWITCH = 2,
    BP35_BOMB_ARMED = 3,
    BP35_BOMB_SAFE = 4,
    BP35_SPREAD = 5,
    BP35_WIN = 6,
};

enum {
    BP35_IMG_qclfkhjnaac = 0,
    BP35_IMG_fijhgcrvsfx = 1,
    BP35_IMG_ucflxtuuxln = 2,
    BP35_IMG_xqapkpdjuet = 3,
    BP35_IMG_lrpkmzabbfa = 4,
    BP35_IMG_ebjoowkheai = 5,
    BP35_IMG_gnqvqkdqlpt = 6,
    BP35_IMG_ippnakjmssl = 7,
    BP35_IMG_yuuqpmlxorv = 8,
    BP35_IMG_oonshderxef = 9,
    BP35_IMG_txjcfisalqu = 10,
    BP35_IMG_cvkgqlojfnh = 11,
    BP35_IMG_ltorejwifje = 12,
    BP35_IMG_etlsaqqtjvn = 13,
    BP35_IMG_wpulgmixnbz = 14,
    BP35_IMG_hihodtibubm = 15,
    BP35_IMG_yxaxjsryovv = 16,
    BP35_IMG_fjlzdjxhant = 17,
    BP35_IMG_player_right = 18,
    BP35_IMG_player_left = 19,
    BP35_IMG_player_right_0 = 20,
    BP35_IMG_player_right_1 = 21,
    BP35_IMG_player_right_2 = 22,
    BP35_IMG_player_left_0 = 23,
    BP35_IMG_player_left_1 = 24,
};

typedef struct {
    int8_t kind[BP35_HMAX * BP35_WMAX];
    uint8_t facing_right;
    uint8_t gravity_down;
    int32_t ceiling_shift;
    int32_t step_par;
    int32_t action_ct;
    int32_t px;
    int32_t py;
    int32_t cam_y;

    int32_t frame_idx;
    int32_t total_frames;

    int32_t traj_px[BP35_MAX_FRAMES];
    int32_t traj_py[BP35_MAX_FRAMES];
    int32_t traj_pimg[BP35_MAX_FRAMES];
    int32_t traj_cam[BP35_MAX_FRAMES];
    int32_t traj_ceil_dy[BP35_MAX_FRAMES];

    int32_t cell_x[5];
    int32_t cell_y[5];
    int32_t traj_cell_img[5][BP35_MAX_FRAMES];

    uint8_t win;
    uint8_t lose;

    int8_t hist_kind[BP35_MAX_HIST][BP35_HMAX * BP35_WMAX];
    int32_t hist_scalar[BP35_MAX_HIST][7];
    int32_t hist_ptr;
} Bp35Aux;

typedef struct {
    const int8_t *kind0;
    const uint8_t *wall;
    const uint8_t *spike;
    const uint8_t *ceil_w0;
    const int32_t *level_w;
    const int32_t *level_h;
    const int32_t *player_start;
    const uint8_t *terrain_present;
    const int32_t *terrain_anchor;
    const int32_t *terrain_atlas;
    const int32_t *terrain_layer;
    const int32_t *img_atlas;
    const int32_t *img_layer;
    const float *ease_out;
    const float *ease_lin;
} Bp35Static;

void bp35_zero_aux(Bp35Aux *aux);

void bp35_build_level(SceneTable *scene, const Bp35Static *st, int32_t level,
                      int32_t action_id, int32_t action_x, int32_t action_y,
                      uint8_t next_level, Bp35Aux *aux);

void bp35_step_once(SceneTable *scene, const Bp35Static *st, int32_t level,
                    int32_t action_id, int32_t action_x, int32_t action_y,
                    Bp35Aux *aux, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete);

void bp35_render_interface(int8_t *frame, const Bp35Static *st, int32_t level,
                           const Bp35Aux *aux);

#endif
