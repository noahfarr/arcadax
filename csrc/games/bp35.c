#include "bp35.h"

#include <string.h>
#include <math.h>

static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

static inline int32_t floor_div(int32_t a, int32_t b) {
    int32_t q = a / b;
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q -= 1;
    return q;
}

static inline int32_t facing_img(int facing_right) {
    return facing_right ? BP35_IMG_player_right : BP35_IMG_player_left;
}

typedef struct {
    int32_t track[BP35_MAX_FRAMES];
    int32_t offset;
    int32_t cur;
} Seq;

static void seq_init(Seq *s, int32_t start) {
    s->offset = 0;
    s->cur = start;
    for (int32_t k = 0; k < BP35_MAX_FRAMES; k++) s->track[k] = start;
}

static void seq_move(Seq *s, int32_t dur, int32_t target, const float *table, int combine_delta) {
    if (dur > 0) {
        int32_t start = s->cur;
        int32_t dur_c = clampi(dur, 1, BP35_NMAX);
        for (int32_t k = s->offset; k < BP35_MAX_FRAMES; k++) {
            int32_t idx = k - s->offset;
            int32_t val;
            if (idx >= dur) {
                val = target;
            } else {
                int32_t ic = clampi(idx, 0, dur_c - 1);
                float t = table[dur_c * BP35_NMAX + ic];
                float delta = (float)(target - start);
                if (combine_delta) val = start + (int32_t)truncf(delta * t);
                else val = (int32_t)truncf((float)start + delta * t);
            }
            s->track[k] = val;
        }
        s->cur = target;
    }
    s->offset += dur;
}

static void seq_hold(Seq *s, int32_t dur) {
    if (dur > 0) {
        for (int32_t k = s->offset; k < BP35_MAX_FRAMES; k++) s->track[k] = s->cur;
    }
    s->offset += dur;
}

static void seq_discrete(Seq *s, int32_t dur, const int32_t *values, int32_t d) {
    if (dur > 0) {
        for (int32_t k = s->offset; k < BP35_MAX_FRAMES; k++) {
            int32_t idx = k - s->offset;
            int32_t val = idx < dur ? values[clampi(idx, 0, d - 1)] : values[d - 1];
            s->track[k] = val;
        }
        s->cur = values[d - 1];
    }
    s->offset += dur;
}

static void splice(int32_t *out, const int32_t *a_track, int32_t a_dur, const int32_t *b_track) {
    for (int32_t k = 0; k < BP35_MAX_FRAMES; k++) {
        if (k < a_dur) {
            out[k] = a_track[k];
        } else {
            int32_t idx = clampi(k - a_dur, 0, BP35_MAX_FRAMES - 1);
            out[k] = b_track[idx];
        }
    }
}

static void classify(const uint8_t *wall_level, const uint8_t *spike_level, const uint8_t *jcyh_now,
                     const int8_t *kind, int32_t x, int32_t y, int32_t W, int32_t H, int32_t *k,
                     int *is_wall, int *is_spike, int *is_jcyh) {
    int ok = x >= 0 && x < W && y >= 0 && y < H;
    int32_t xi = clampi(x, 0, BP35_WMAX - 1);
    int32_t yi = clampi(y, 0, BP35_HMAX - 1);
    size_t idx = (size_t)yi * BP35_WMAX + xi;
    *k = ok ? kind[idx] : BP35_NONE;
    *is_wall = ok && wall_level[idx];
    *is_spike = ok && spike_level[idx];
    *is_jcyh = ok && jcyh_now[idx];
}

static inline int walkable(int32_t k, int is_wall, int is_spike, int is_jcyh) {
    return !is_wall && !is_spike && !is_jcyh && (k == BP35_NONE || k == BP35_BOMB_SAFE);
}

static void fall_from(const uint8_t *wall_level, const uint8_t *spike_level, const uint8_t *jcyh_now,
                      const int8_t *kind, int32_t x0, int32_t y0, int32_t dy, int32_t W, int32_t H,
                      int32_t *steps_out, int32_t *land_y_out, int *gem_out, int *spk_out) {
    int active = 1;
    int32_t steps = 0;
    int32_t land_y = y0;
    int32_t cur_y = y0;
    int gem = 0, sp = 0;
    for (int32_t i = 0; i < BP35_HMAX && active; i++) {
        int32_t cand_y = cur_y + dy;
        int32_t k;
        int iw, isk, ij;
        classify(wall_level, spike_level, jcyh_now, kind, x0, cand_y, W, H, &k, &iw, &isk, &ij);
        int is_win = k == BP35_WIN && !iw;
        int walk = walkable(k, iw, isk, ij) && active;
        int stop_win = active && !walk && is_win;
        int stop_spike = active && !walk && isk && !is_win;
        if (walk || stop_win || stop_spike) steps++;
        if (walk || stop_win) land_y = cand_y;
        if (walk) cur_y = cand_y;
        gem |= stop_win;
        sp |= stop_spike;
        active = walk;
    }
    *steps_out = steps;
    *land_y_out = land_y;
    *gem_out = gem;
    *spk_out = sp;
}

static void jcyh_now_grid(const Bp35Static *st, int32_t level, int32_t shift, uint8_t *out) {
    const uint8_t *ceil_w0_level = st->ceil_w0 + (size_t)level * BP35_HMAX * BP35_WMAX;
    for (int32_t r = 0; r < BP35_HMAX; r++) {
        int32_t src = r + shift;
        int ok = src >= 0 && src < BP35_HMAX;
        int32_t srci = clampi(src, 0, BP35_HMAX - 1);
        for (int32_t x = 0; x < BP35_WMAX; x++)
            out[(size_t)r * BP35_WMAX + x] = ok ? ceil_w0_level[(size_t)srci * BP35_WMAX + x] : 0;
    }
}

static void push_history(Bp35Aux *aux) {
    int32_t ptr = clampi(aux->hist_ptr, 0, BP35_MAX_HIST - 1);
    memcpy(aux->hist_kind[ptr], aux->kind, sizeof aux->kind);
    aux->hist_scalar[ptr][0] = aux->px;
    aux->hist_scalar[ptr][1] = aux->py;
    aux->hist_scalar[ptr][2] = aux->facing_right ? 1 : 0;
    aux->hist_scalar[ptr][3] = aux->gravity_down ? 1 : 0;
    aux->hist_scalar[ptr][4] = aux->ceiling_shift;
    aux->hist_scalar[ptr][5] = aux->step_par;
    aux->hist_scalar[ptr][6] = aux->cam_y;
    int32_t next = aux->hist_ptr + 1;
    aux->hist_ptr = next < BP35_MAX_HIST ? next : BP35_MAX_HIST - 1;
}

static void recipe_static(const Bp35Aux *in, Bp35Aux *out) {
    int32_t px_px = in->px * BP35_PITCH;
    int32_t py_px = in->py * BP35_PITCH;
    int32_t pimg = facing_img(in->facing_right);
    *out = *in;
    out->total_frames = 2;
    for (int32_t k = 0; k < BP35_MAX_FRAMES; k++) {
        out->traj_px[k] = px_px;
        out->traj_py[k] = py_px;
        out->traj_pimg[k] = pimg;
        out->traj_cam[k] = in->cam_y;
        out->traj_ceil_dy[k] = -in->ceiling_shift * BP35_PITCH;
    }
    for (int32_t i = 0; i < 5; i++) {
        out->cell_x[i] = -1;
        out->cell_y[i] = -1;
        for (int32_t k = 0; k < BP35_MAX_FRAMES; k++) out->traj_cell_img[i][k] = -1;
    }
    out->win = 0;
    out->lose = 0;
}

static void recipe_undo(const Bp35Aux *in, Bp35Aux *out) {
    Bp35Aux restored = *in;
    int has = in->hist_ptr > 0;
    if (has) {
        int32_t idx = clampi(in->hist_ptr - 1, 0, BP35_MAX_HIST - 1);
        memcpy(restored.kind, in->hist_kind[idx], sizeof restored.kind);
        restored.px = in->hist_scalar[idx][0];
        restored.py = in->hist_scalar[idx][1];
        restored.facing_right = in->hist_scalar[idx][2] > 0;
        restored.gravity_down = in->hist_scalar[idx][3] > 0;
        restored.ceiling_shift = in->hist_scalar[idx][4];
        restored.step_par = in->hist_scalar[idx][5];
        restored.cam_y = in->hist_scalar[idx][6];
        restored.hist_ptr = in->hist_ptr - 1;
    }
    recipe_static(&restored, out);
}

static void recipe_move(const Bp35Aux *in, const Bp35Static *st, int32_t level, int32_t W, int32_t H,
                        const uint8_t *wall_level, const uint8_t *spike_level, const uint8_t *jcyh_now,
                        int32_t dx, Bp35Aux *out) {
    int facing = dx > 0;
    int32_t step_par = in->step_par + 1;

    int32_t tx_raw = in->px + dx;
    int offboard = tx_raw < 0;
    int32_t tx = offboard ? 0 : tx_raw;
    int32_t ty = in->py;

    int32_t k;
    int iw, isk, ij;
    classify(wall_level, spike_level, jcyh_now, in->kind, tx, ty, W, H, &k, &iw, &isk, &ij);
    int is_win_t = !offboard && k == BP35_WIN;
    int walkable_t = !offboard && walkable(k, iw, isk, ij);
    int is_bump = !is_win_t && !walkable_t;

    int32_t gravity_dy = in->gravity_down ? -1 : 1;
    int32_t tprcybckbl = in->gravity_down ? -5 : 5;
    int32_t extra, land_y;
    int gem, spk;
    fall_from(wall_level, spike_level, jcyh_now, in->kind, tx, ty, gravity_dy, W, H, &extra, &land_y, &gem, &spk);
    int32_t land_x = tx;
    int settling = extra == 0;

    int ceil_present = st->terrain_present[(size_t)level * BP35_NUM_TERRAIN + 1];
    int ceil_trigger = ceil_present && (step_par % 2) == 0;
    int32_t ceil_m_y_now = st->terrain_anchor[((size_t)level * BP35_NUM_TERRAIN + 1) * 2 + 1] - in->ceiling_shift;
    int crush = ceil_trigger && in->py == ceil_m_y_now;
    int mylefxfaev_called = is_bump || (walkable_t && settling);

    int32_t facing_img_v = facing_img(facing);
    int32_t ceil_base = -in->ceiling_shift * BP35_PITCH;
    int32_t ceil_dur = ceil_trigger ? 4 : 0;
    Seq ceil_seq;
    seq_init(&ceil_seq, ceil_base);
    seq_move(&ceil_seq, ceil_trigger ? 3 : 0, ceil_base - 6, st->ease_lin, 1);
    seq_hold(&ceil_seq, ceil_trigger ? 1 : 0);

    Seq win_x, win_y;
    seq_init(&win_x, in->px * BP35_PITCH);
    seq_move(&win_x, 5, tx * BP35_PITCH, st->ease_out, 1);
    seq_init(&win_y, in->py * BP35_PITCH);
    seq_move(&win_y, 5, ty * BP35_PITCH, st->ease_out, 1);
    int32_t win_frames = 10;

    Seq stl_full_x, stl_full_y, stl_par_x, stl_par_y;
    seq_init(&stl_full_x, in->px * BP35_PITCH);
    seq_move(&stl_full_x, 5, tx * BP35_PITCH, st->ease_out, 0);
    seq_init(&stl_full_y, in->py * BP35_PITCH);
    seq_move(&stl_full_y, 5, ty * BP35_PITCH, st->ease_out, 0);
    seq_init(&stl_par_x, in->px * BP35_PITCH);
    seq_move(&stl_par_x, 5, tx * BP35_PITCH, st->ease_out, 1);
    seq_init(&stl_par_y, in->py * BP35_PITCH);
    seq_move(&stl_par_y, 5, ty * BP35_PITCH, st->ease_out, 1);
    int32_t stl_frames = ceil_dur > 5 ? ceil_dur : 5;

    int32_t sgn = facing ? 1 : -1;
    int32_t base_x = in->px * BP35_PITCH;
    Seq bmp_seq;
    seq_init(&bmp_seq, base_x);
    seq_move(&bmp_seq, 1, base_x - sgn, st->ease_lin, 0);
    seq_move(&bmp_seq, 1, base_x + sgn, st->ease_lin, 0);
    seq_move(&bmp_seq, 1, base_x, st->ease_lin, 0);
    int32_t bmp_py_const = in->py * BP35_PITCH;
    int32_t bmp_frames = ceil_dur > 3 ? ceil_dur : 3;

    int32_t drop_dur = extra * 3 < 20 ? extra * 3 : 20;
    int32_t death_dur = spk ? 10 : 0;
    int32_t death_x[10], death_y[10];
    int32_t death_img[10] = {
        -1, BP35_IMG_player_right_0, BP35_IMG_player_right_0,
        -1, BP35_IMG_player_right_1, BP35_IMG_player_right_1,
        -1, BP35_IMG_player_right_2, BP35_IMG_player_right_2, -1,
    };
    for (int32_t i = 0; i < 10; i++) {
        death_x[i] = land_x * BP35_PITCH;
        death_y[i] = land_y * BP35_PITCH;
    }

    Seq fall_x, fall_y, fall_pimg;
    seq_init(&fall_x, in->px * BP35_PITCH);
    seq_move(&fall_x, 5, tx * BP35_PITCH, st->ease_out, 0);
    seq_move(&fall_x, drop_dur, land_x * BP35_PITCH, st->ease_lin, 0);
    seq_discrete(&fall_x, death_dur, death_x, 10);

    seq_init(&fall_y, in->py * BP35_PITCH);
    seq_move(&fall_y, 5, ty * BP35_PITCH, st->ease_out, 0);
    seq_move(&fall_y, drop_dur, land_y * BP35_PITCH, st->ease_lin, 0);
    seq_discrete(&fall_y, death_dur, death_y, 10);
    int32_t fall_pdur = fall_y.offset;

    seq_init(&fall_pimg, facing_img_v);
    seq_hold(&fall_pimg, 5 + drop_dur);
    seq_discrete(&fall_pimg, death_dur, death_img, 10);

    int32_t cam_target = land_y * BP35_PITCH - 31 + tprcybckbl;
    int32_t pan_dur_raw = iabs32(floor_div(cam_target - in->cam_y, BP35_PITCH)) * 3;
    int32_t pan_dur = pan_dur_raw < 20 ? pan_dur_raw : 20;
    int32_t wait_dur = gem ? 5 : 0;
    wait_dur = wait_dur > 1 ? wait_dur : 1;

    Seq fall_cam;
    seq_init(&fall_cam, in->cam_y);
    seq_hold(&fall_cam, fall_pdur);
    seq_move(&fall_cam, pan_dur, cam_target, st->ease_lin, 0);
    seq_hold(&fall_cam, wait_dur);
    seq_hold(&fall_cam, 1);
    int32_t fall_frames = fall_cam.offset;

    int is_fall = walkable_t && !settling;

    int32_t new_px = is_bump ? in->px : (is_fall ? land_x : tx);
    int32_t new_py = is_bump ? in->py : (is_fall ? land_y : ty);
    int32_t new_cam = is_fall ? cam_target : in->cam_y;
    int32_t new_ceiling_shift = in->ceiling_shift + (mylefxfaev_called && ceil_trigger ? 1 : 0);
    int32_t total_frames = is_win_t ? win_frames : (is_bump ? bmp_frames : (is_fall ? fall_frames : stl_frames));
    int win_flag = is_win_t || (is_fall && gem);
    int lose_flag = (mylefxfaev_called && crush) || (is_fall && spk);

    *out = *in;
    out->facing_right = (uint8_t)facing;
    out->ceiling_shift = new_ceiling_shift;
    out->step_par = step_par;
    out->px = new_px;
    out->py = new_py;
    out->cam_y = new_cam;
    out->total_frames = total_frames;
    for (int32_t i = 0; i < BP35_MAX_FRAMES; i++) {
        out->traj_px[i] = is_win_t ? win_x.track[i]
                          : is_bump ? bmp_seq.track[i]
                          : is_fall ? fall_x.track[i]
                          : ceil_trigger ? stl_par_x.track[i] : stl_full_x.track[i];
        out->traj_py[i] = is_win_t ? win_y.track[i]
                          : is_bump ? bmp_py_const
                          : is_fall ? fall_y.track[i]
                          : ceil_trigger ? stl_par_y.track[i] : stl_full_y.track[i];
        out->traj_pimg[i] = is_fall ? fall_pimg.track[i] : facing_img_v;
        out->traj_cam[i] = is_fall ? fall_cam.track[i] : in->cam_y;
        out->traj_ceil_dy[i] = mylefxfaev_called ? ceil_seq.track[i] : ceil_base;
    }
    for (int32_t i = 0; i < 5; i++) {
        out->cell_x[i] = -1;
        out->cell_y[i] = -1;
        for (int32_t k2 = 0; k2 < BP35_MAX_FRAMES; k2++) out->traj_cell_img[i][k2] = -1;
    }
    out->win = (uint8_t)win_flag;
    out->lose = (uint8_t)lose_flag;
}

static void recipe_click(const Bp35Aux *in, const Bp35Static *st, int32_t W, int32_t H,
                         const uint8_t *wall_level, const uint8_t *spike_level, const uint8_t *jcyh_now,
                         int32_t screen_x, int32_t screen_y, Bp35Aux *out) {
    int32_t kx = floor_div(screen_x, BP35_PITCH);
    int32_t ky = floor_div(screen_y + in->cam_y, BP35_PITCH);
    int in_bounds = kx >= 0 && kx < W && ky >= 0 && ky < H;
    int32_t kxi = clampi(kx, 0, BP35_WMAX - 1);
    int32_t kyi = clampi(ky, 0, BP35_HMAX - 1);
    int32_t kind_at = in_bounds ? in->kind[(size_t)kyi * BP35_WMAX + kxi] : BP35_NONE;

    int is_breakable = in_bounds && kind_at == BP35_BREAKABLE;
    int is_switch = in_bounds && kind_at == BP35_SWITCH;
    int is_bomb_arm = in_bounds && kind_at == BP35_BOMB_ARMED;
    int is_bomb_safe = in_bounds && kind_at == BP35_BOMB_SAFE;
    int is_spread = in_bounds && kind_at == BP35_SPREAD;
    int is_active = is_breakable || is_switch || is_bomb_arm || is_bomb_safe || is_spread;

    int32_t step_par = in->step_par + 1;

    int8_t kind_after[BP35_HMAX * BP35_WMAX];
    memcpy(kind_after, in->kind, sizeof kind_after);
    int32_t existing = in->kind[(size_t)kyi * BP35_WMAX + kxi];
    int32_t new_here = (is_breakable || is_switch || is_spread)
                       ? BP35_NONE
                       : (is_bomb_arm ? BP35_BOMB_SAFE : (is_bomb_safe ? BP35_BOMB_ARMED : existing));
    kind_after[(size_t)kyi * BP35_WMAX + kxi] = (int8_t)new_here;

    static const int32_t ndx[4] = {-1, 1, 0, 0};
    static const int32_t ndy[4] = {0, 0, -1, 1};
    int32_t n_x[4], n_y[4];
    int n_valid[4];
    for (int i = 0; i < 4; i++) {
        int32_t nx = kx + ndx[i], ny = ky + ndy[i];
        int32_t nk;
        int nw, nsk, nj;
        classify(wall_level, spike_level, jcyh_now, in->kind, nx, ny, W, H, &nk, &nw, &nsk, &nj);
        int ok = nx >= 0 && nx < W && ny >= 0 && ny < H;
        int empty = ok && !nw && !nsk && !nj && nk == BP35_NONE;
        int valid = is_spread && empty;
        n_x[i] = nx;
        n_y[i] = ny;
        n_valid[i] = valid;
        int32_t nxi = clampi(nx, 0, BP35_WMAX - 1), nyi = clampi(ny, 0, BP35_HMAX - 1);
        if (valid) kind_after[(size_t)nyi * BP35_WMAX + nxi] = BP35_SPREAD;
    }

    int qssroarxob = is_switch;
    int gravity_now = is_switch ? !in->gravity_down : in->gravity_down;
    int32_t dy = gravity_now ? -1 : 1;
    int32_t ey = in->py + dy;
    int proceed = qssroarxob || (kx == in->px && ky == ey);

    int32_t k2;
    int iw2, isk2, ij2;
    classify(wall_level, spike_level, jcyh_now, in->kind, in->px, ey, W, H, &k2, &iw2, &isk2, &ij2);
    int is_solid_indiv = k2 == BP35_SWITCH || k2 == BP35_BOMB_ARMED || k2 == BP35_BREAKABLE || k2 == BP35_SPREAD;
    int blocked = iw2 || (qssroarxob && is_solid_indiv);

    int32_t extra, land_y;
    int gem, spk;
    fall_from(wall_level, spike_level, jcyh_now, in->kind, in->px, ey, dy, W, H, &extra, &land_y, &gem, &spk);
    int32_t land_x = in->px;
    int32_t total_extra = extra + 1;
    int32_t tprcybckbl = gravity_now ? -5 : 5;
    int32_t drop_dur = 3 * total_extra < 20 ? 3 * total_extra : 20;
    int32_t death_dur = spk ? 3 : 0;
    int32_t death_img[3] = {BP35_IMG_player_left_0, BP35_IMG_player_left_1, BP35_IMG_player_left_1};
    int32_t cam_target = land_y * BP35_PITCH - 31 + tprcybckbl;
    int32_t pan_dur_raw = iabs32(floor_div(cam_target - in->cam_y, BP35_PITCH)) * 3;
    int32_t pan_dur = pan_dur_raw < 20 ? pan_dur_raw : 20;
    int32_t wait_dur = gem ? 5 : 0;
    int32_t facing_img_v = facing_img(in->facing_right);

    int32_t land_xs[3] = {land_x * BP35_PITCH, land_x * BP35_PITCH, land_x * BP35_PITCH};
    int32_t land_ys[3] = {land_y * BP35_PITCH, land_y * BP35_PITCH, land_y * BP35_PITCH};

    Seq real_x, real_y, real_pimg, real_cam;
    seq_init(&real_x, in->px * BP35_PITCH);
    seq_move(&real_x, drop_dur, land_x * BP35_PITCH, st->ease_lin, 0);
    seq_discrete(&real_x, death_dur, land_xs, 3);

    seq_init(&real_y, in->py * BP35_PITCH);
    seq_move(&real_y, drop_dur, land_y * BP35_PITCH, st->ease_lin, 0);
    seq_discrete(&real_y, death_dur, land_ys, 3);

    seq_init(&real_pimg, facing_img_v);
    seq_hold(&real_pimg, drop_dur);
    seq_discrete(&real_pimg, death_dur, death_img, 3);

    seq_init(&real_cam, in->cam_y);
    seq_hold(&real_cam, drop_dur + death_dur);
    seq_move(&real_cam, pan_dur, cam_target, st->ease_lin, 0);
    seq_hold(&real_cam, wait_dur);
    int32_t real_dur = real_cam.offset;

    int32_t recenter_target = in->py * BP35_PITCH - 31 + tprcybckbl;
    Seq blocked_cam;
    seq_init(&blocked_cam, in->cam_y);
    seq_move(&blocked_cam, 6, recenter_target, st->ease_lin, 0);
    int32_t blocked_dur = blocked_cam.offset;

    int32_t blocked_px_c = in->px * BP35_PITCH;
    int32_t blocked_py_c = in->py * BP35_PITCH;
    int32_t blocked_pimg_c = facing_img_v;

    int32_t noop_px_c = in->px * BP35_PITCH;
    int32_t noop_py_c = in->py * BP35_PITCH;
    int32_t noop_pimg_c = facing_img_v;
    int32_t noop_cam_c = in->cam_y;
    int32_t noop_dur = 1;

    int proceed_real = proceed && !blocked;
    int32_t sel_dur = !proceed ? noop_dur : (blocked ? blocked_dur : real_dur);
    int sel_win = proceed_real && gem;
    int sel_lose = proceed_real && spk;
    int32_t sel_new_px = proceed_real ? land_x : in->px;
    int32_t sel_new_py = proceed_real ? land_y : in->py;
    int32_t sel_new_cam = proceed ? (blocked ? recenter_target : cam_target) : in->cam_y;

    int32_t cell0_track[BP35_MAX_FRAMES];
    {
        Seq s;
        if (is_breakable) {
            int32_t vals[4] = {BP35_IMG_fijhgcrvsfx, BP35_IMG_ucflxtuuxln, BP35_IMG_xqapkpdjuet, -1};
            seq_init(&s, -1);
            seq_discrete(&s, 4, vals, 4);
        } else if (is_switch) {
            int32_t vals[4] = {BP35_IMG_ebjoowkheai, BP35_IMG_gnqvqkdqlpt, BP35_IMG_ippnakjmssl, -1};
            seq_init(&s, -1);
            seq_discrete(&s, 4, vals, 4);
        } else if (is_bomb_arm) {
            int32_t vals[4] = {BP35_IMG_txjcfisalqu, BP35_IMG_cvkgqlojfnh, BP35_IMG_ltorejwifje, BP35_IMG_oonshderxef};
            seq_init(&s, -1);
            seq_discrete(&s, 4, vals, 4);
            seq_hold(&s, 1);
        } else if (is_bomb_safe) {
            int32_t vals[4] = {BP35_IMG_ltorejwifje, BP35_IMG_cvkgqlojfnh, BP35_IMG_txjcfisalqu, BP35_IMG_yuuqpmlxorv};
            seq_init(&s, -1);
            seq_discrete(&s, 4, vals, 4);
            seq_hold(&s, 1);
        } else if (is_spread) {
            int32_t vals[4] = {BP35_IMG_wpulgmixnbz, BP35_IMG_hihodtibubm, BP35_IMG_yxaxjsryovv, -1};
            seq_init(&s, -1);
            seq_discrete(&s, 4, vals, 4);
        } else {
            seq_init(&s, -1);
        }
        memcpy(cell0_track, s.track, sizeof cell0_track);
    }

    int32_t neighbour_track[BP35_MAX_FRAMES];
    {
        Seq s;
        seq_init(&s, BP35_IMG_yxaxjsryovv);
        seq_hold(&s, 4);
        int32_t v1[1] = {BP35_IMG_hihodtibubm};
        seq_discrete(&s, 1, v1, 1);
        seq_hold(&s, sel_dur);
        int32_t v2[1] = {BP35_IMG_etlsaqqtjvn};
        seq_discrete(&s, 1, v2, 1);
        if (is_spread) memcpy(neighbour_track, s.track, sizeof neighbour_track);
        else for (int32_t i = 0; i < BP35_MAX_FRAMES; i++) neighbour_track[i] = -1;
    }

    int32_t swap_total = (is_spread || is_bomb_arm || is_bomb_safe) ? 5 : 4;
    int32_t total_frames_active = swap_total + sel_dur + (is_spread ? 1 : 0);

    int32_t player_hold_x[BP35_MAX_FRAMES], player_hold_y[BP35_MAX_FRAMES];
    int32_t player_hold_img[BP35_MAX_FRAMES], player_hold_cam[BP35_MAX_FRAMES];
    int32_t sel_px[BP35_MAX_FRAMES], sel_py[BP35_MAX_FRAMES];
    int32_t sel_pimg[BP35_MAX_FRAMES], sel_cam[BP35_MAX_FRAMES];
    for (int32_t i = 0; i < BP35_MAX_FRAMES; i++) {
        player_hold_x[i] = in->px * BP35_PITCH;
        player_hold_y[i] = in->py * BP35_PITCH;
        player_hold_img[i] = facing_img_v;
        player_hold_cam[i] = in->cam_y;
        sel_px[i] = !proceed ? noop_px_c : (blocked ? blocked_px_c : real_x.track[i]);
        sel_py[i] = !proceed ? noop_py_c : (blocked ? blocked_py_c : real_y.track[i]);
        sel_pimg[i] = !proceed ? noop_pimg_c : (blocked ? blocked_pimg_c : real_pimg.track[i]);
        sel_cam[i] = !proceed ? noop_cam_c : (blocked ? blocked_cam.track[i] : real_cam.track[i]);
    }

    int32_t active_px[BP35_MAX_FRAMES], active_py[BP35_MAX_FRAMES];
    int32_t active_pimg[BP35_MAX_FRAMES], active_cam[BP35_MAX_FRAMES];
    splice(active_px, player_hold_x, swap_total, sel_px);
    splice(active_py, player_hold_y, swap_total, sel_py);
    splice(active_pimg, player_hold_img, swap_total, sel_pimg);
    splice(active_cam, player_hold_cam, swap_total, sel_cam);

    int32_t active_cell_x[5], active_cell_y[5];
    active_cell_x[0] = is_active ? kx : -1;
    active_cell_y[0] = is_active ? ky : -1;
    for (int i = 0; i < 4; i++) {
        active_cell_x[1 + i] = n_valid[i] ? n_x[i] : -1;
        active_cell_y[1 + i] = n_valid[i] ? n_y[i] : -1;
    }

    Bp35Aux statics;
    recipe_static(in, &statics);

    int select = is_active;

    *out = *in;
    if (select) memcpy(out->kind, kind_after, sizeof out->kind);
    out->gravity_down = (uint8_t)(is_switch ? gravity_now : in->gravity_down);
    out->step_par = step_par;
    out->px = select ? sel_new_px : statics.px;
    out->py = select ? sel_new_py : statics.py;
    out->cam_y = select ? sel_new_cam : statics.cam_y;
    out->total_frames = select ? total_frames_active : statics.total_frames;
    if (select) {
        memcpy(out->traj_px, active_px, sizeof active_px);
        memcpy(out->traj_py, active_py, sizeof active_py);
        memcpy(out->traj_pimg, active_pimg, sizeof active_pimg);
        memcpy(out->traj_cam, active_cam, sizeof active_cam);
    } else {
        memcpy(out->traj_px, statics.traj_px, sizeof statics.traj_px);
        memcpy(out->traj_py, statics.traj_py, sizeof statics.traj_py);
        memcpy(out->traj_pimg, statics.traj_pimg, sizeof statics.traj_pimg);
        memcpy(out->traj_cam, statics.traj_cam, sizeof statics.traj_cam);
    }
    memcpy(out->traj_ceil_dy, statics.traj_ceil_dy, sizeof statics.traj_ceil_dy);
    if (select) {
        memcpy(out->cell_x, active_cell_x, sizeof active_cell_x);
        memcpy(out->cell_y, active_cell_y, sizeof active_cell_y);
        for (int i = 0; i < 5; i++) {
            const int32_t *src = i == 0 ? cell0_track : neighbour_track;
            memcpy(out->traj_cell_img[i], src, sizeof cell0_track);
        }
    } else {
        memcpy(out->cell_x, statics.cell_x, sizeof statics.cell_x);
        memcpy(out->cell_y, statics.cell_y, sizeof statics.cell_y);
        memcpy(out->traj_cell_img, statics.traj_cell_img, sizeof statics.traj_cell_img);
    }
    out->win = (uint8_t)(select && sel_win);
    out->lose = (uint8_t)(select && sel_lose);
}

static void dispatch(const Bp35Static *st, int32_t level, int32_t action_id, int32_t action_x,
                     int32_t action_y, Bp35Aux *aux) {
    int32_t W = st->level_w[level], H = st->level_h[level];
    const uint8_t *wall_level = st->wall + (size_t)level * BP35_HMAX * BP35_WMAX;
    const uint8_t *spike_level = st->spike + (size_t)level * BP35_HMAX * BP35_WMAX;

    int is_move = action_id == 3 || action_id == 4;
    int is_click = action_id == 6;
    int is_undo = action_id == 7;

    if (is_move || is_click) push_history(aux);

    uint8_t jcyh_now[BP35_HMAX * BP35_WMAX];
    jcyh_now_grid(st, level, aux->ceiling_shift, jcyh_now);

    Bp35Aux snapshot = *aux;
    int32_t dx = action_id == 4 ? 1 : -1;

    if (is_move) {
        recipe_move(&snapshot, st, level, W, H, wall_level, spike_level, jcyh_now, dx, aux);
    } else if (is_click) {
        recipe_click(&snapshot, st, W, H, wall_level, spike_level, jcyh_now, action_x, action_y, aux);
    } else if (is_undo) {
        recipe_undo(&snapshot, aux);
    } else {
        recipe_static(&snapshot, aux);
    }

    int32_t action_ct = snapshot.action_ct + (action_id == 0 ? 0 : 1);
    int32_t threshold = level + 1 <= 6 ? 64 : 128;
    aux->action_ct = action_ct;
    aux->lose = (uint8_t)(aux->lose || action_ct >= threshold);
    aux->frame_idx = 0;
}

static void compose(SceneTable *scene, const Bp35Static *st, int32_t level, const int8_t *kind,
                    int32_t px, int32_t py, int32_t pimgidx, int32_t cam_y, int32_t ceil_dy,
                    const int32_t *cell_x, const int32_t *cell_y, const int32_t *cellimgidx) {
    static const int is_ceiling_slot[BP35_NUM_TERRAIN] = {0, 1, 1, 0, 0};
    static const int32_t kind_idle_imgidx[7] = {
        -1, BP35_IMG_qclfkhjnaac, BP35_IMG_lrpkmzabbfa, BP35_IMG_yuuqpmlxorv,
        BP35_IMG_oonshderxef, BP35_IMG_etlsaqqtjvn, BP35_IMG_fjlzdjxhant,
    };

    uint8_t active_mask[BP35_HMAX * BP35_WMAX];
    memset(active_mask, 0, sizeof active_mask);
    for (int i = 0; i < 5; i++) {
        if (cell_x[i] < 0) continue;
        int32_t yi = clampi(cell_y[i], 0, BP35_HMAX - 1);
        int32_t xi = clampi(cell_x[i], 0, BP35_WMAX - 1);
        active_mask[(size_t)yi * BP35_WMAX + xi] = 1;
    }

    for (int32_t y = 0; y < BP35_HMAX; y++) {
        for (int32_t x = 0; x < BP35_WMAX; x++) {
            size_t idx = (size_t)y * BP35_WMAX + x;
            int32_t imgidx = active_mask[idx] ? -1 : kind_idle_imgidx[kind[idx]];
            scene->image[idx] = imgidx >= 0 ? st->img_atlas[imgidx] : -1;
            scene->x[idx] = x * BP35_PITCH;
            scene->y[idx] = y * BP35_PITCH - cam_y;
            scene->layer[idx] = imgidx >= 0 ? st->img_layer[imgidx] : 0;
        }
    }

    scene->image[BP35_PLAYER_SLOT] = pimgidx >= 0 ? st->img_atlas[pimgidx] : -1;
    scene->x[BP35_PLAYER_SLOT] = px;
    scene->y[BP35_PLAYER_SLOT] = py - cam_y;
    scene->layer[BP35_PLAYER_SLOT] = pimgidx >= 0 ? st->img_layer[pimgidx] : 0;

    for (int i = 0; i < BP35_NUM_TERRAIN; i++) {
        int32_t idx = level * BP35_NUM_TERRAIN + i;
        int present = st->terrain_present[idx];
        int32_t anchor_x = st->terrain_anchor[idx * 2 + 0];
        int32_t anchor_y = st->terrain_anchor[idx * 2 + 1];
        int32_t ceil_offset = is_ceiling_slot[i] ? ceil_dy : 0;
        scene->image[BP35_TERRAIN_SLOT0 + i] = present ? st->terrain_atlas[idx] : -1;
        scene->x[BP35_TERRAIN_SLOT0 + i] = anchor_x * BP35_PITCH;
        scene->y[BP35_TERRAIN_SLOT0 + i] = anchor_y * BP35_PITCH - cam_y + ceil_offset;
        scene->layer[BP35_TERRAIN_SLOT0 + i] = st->terrain_layer[idx];
    }

    for (int i = 0; i < 5; i++) {
        int32_t ci = cellimgidx[i];
        scene->image[BP35_ANIM_SLOT0 + i] = ci >= 0 ? st->img_atlas[ci] : -1;
        scene->x[BP35_ANIM_SLOT0 + i] = cell_x[i] * BP35_PITCH;
        scene->y[BP35_ANIM_SLOT0 + i] = cell_y[i] * BP35_PITCH - cam_y;
        scene->layer[BP35_ANIM_SLOT0 + i] = ci >= 0 ? st->img_layer[ci] : 0;
    }

    for (int32_t i = 0; i < BP35_NUM_SLOTS; i++) scene->order[i] = i;
    scene->dirty = 1;
}

void bp35_zero_aux(Bp35Aux *aux) {
    memset(aux, 0, sizeof *aux);
    aux->facing_right = 1;
    aux->gravity_down = 1;
    aux->total_frames = 1;
    for (int32_t i = 0; i < BP35_MAX_FRAMES; i++) aux->traj_pimg[i] = -1;
    for (int32_t i = 0; i < 5; i++) {
        aux->cell_x[i] = -1;
        aux->cell_y[i] = -1;
        for (int32_t k = 0; k < BP35_MAX_FRAMES; k++) aux->traj_cell_img[i][k] = -1;
    }
}

void bp35_build_level(SceneTable *scene, const Bp35Static *st, int32_t level, int32_t action_id,
                      int32_t action_x, int32_t action_y, uint8_t next_level, Bp35Aux *aux) {
    int32_t px = st->player_start[level * 2 + 0];
    int32_t py = st->player_start[level * 2 + 1];
    int32_t cam_y = py * BP35_PITCH - 31 - 5;

    Bp35Aux fresh;
    bp35_zero_aux(&fresh);
    memcpy(fresh.kind, st->kind0 + (size_t)level * BP35_HMAX * BP35_WMAX, sizeof fresh.kind);
    fresh.px = px;
    fresh.py = py;
    fresh.cam_y = cam_y;

    int32_t neg5[5] = {-1, -1, -1, -1, -1};
    int32_t f_image[BP35_NUM_SLOTS], f_x[BP35_NUM_SLOTS], f_y[BP35_NUM_SLOTS];
    int32_t f_layer[BP35_NUM_SLOTS], f_order[BP35_NUM_SLOTS];
    SceneTable fresh_scene = {f_image, f_x, f_y, f_layer, f_order, NULL, 0, BP35_NUM_SLOTS};
    compose(&fresh_scene, st, level, fresh.kind, px * BP35_PITCH, py * BP35_PITCH,
           facing_img(fresh.facing_right), cam_y, 0, neg5, neg5, neg5);

    Bp35Aux dispatched = fresh;
    dispatch(st, level, action_id, action_x, action_y, &dispatched);
    int32_t t = clampi(dispatched.total_frames - 1, 0, BP35_MAX_FRAMES - 1);

    int32_t cellimg_t[5];
    for (int i = 0; i < 5; i++) cellimg_t[i] = dispatched.traj_cell_img[i][t];
    int32_t e_image[BP35_NUM_SLOTS], e_x[BP35_NUM_SLOTS], e_y[BP35_NUM_SLOTS];
    int32_t e_layer[BP35_NUM_SLOTS], e_order[BP35_NUM_SLOTS];
    SceneTable echo_scene = {e_image, e_x, e_y, e_layer, e_order, NULL, 0, BP35_NUM_SLOTS};
    compose(&echo_scene, st, level, dispatched.kind, dispatched.traj_px[t], dispatched.traj_py[t],
           dispatched.traj_pimg[t], dispatched.traj_cam[t], dispatched.traj_ceil_dy[t],
           dispatched.cell_x, dispatched.cell_y, cellimg_t);

    const SceneTable *chosen = next_level ? &echo_scene : &fresh_scene;
    memcpy(scene->image, chosen->image, sizeof(int32_t) * BP35_NUM_SLOTS);
    memcpy(scene->x, chosen->x, sizeof(int32_t) * BP35_NUM_SLOTS);
    memcpy(scene->y, chosen->y, sizeof(int32_t) * BP35_NUM_SLOTS);
    memcpy(scene->layer, chosen->layer, sizeof(int32_t) * BP35_NUM_SLOTS);
    memcpy(scene->order, chosen->order, sizeof(int32_t) * BP35_NUM_SLOTS);
    scene->dirty = 1;

    *aux = fresh;
}

void bp35_step_once(SceneTable *scene, const Bp35Static *st, int32_t level, int32_t action_id,
                    int32_t action_x, int32_t action_y, Bp35Aux *aux, int32_t *score, int32_t *status,
                    uint8_t *next_level, uint8_t *action_complete) {
    if (aux->frame_idx == 0) dispatch(st, level, action_id, action_x, action_y, aux);

    int32_t f = aux->frame_idx;
    int32_t cellimg_f[5];
    for (int i = 0; i < 5; i++) cellimg_f[i] = aux->traj_cell_img[i][f];
    compose(scene, st, level, aux->kind, aux->traj_px[f], aux->traj_py[f], aux->traj_pimg[f],
           aux->traj_cam[f], aux->traj_ceil_dy[f], aux->cell_x, aux->cell_y, cellimg_f);

    int done = f + 1 >= aux->total_frames;
    aux->frame_idx = done ? 0 : f + 1;

    *next_level = 0;
    *action_complete = 0;
    if (done) {
        if (aux->win) {
            int is_last = level == BP35_NUM_LEVELS - 1;
            *score += 1;
            *next_level = (uint8_t)!is_last;
            if (is_last) *status = 2;
        }
        if (aux->lose) *status = 3;
        *action_complete = 1;
    }
}

void bp35_render_interface(int8_t *frame, const Bp35Static *st, int32_t level, const Bp35Aux *aux) {
    (void)st;
    int32_t level1 = level + 1;
    int32_t ct = aux->action_ct;
    if (aux->total_frames == 1) return;
    for (int32_t c = 0; c < FRAME_SIZE; c++) {
        int32_t val;
        if (level1 <= 6) {
            val = c < ct ? 15 : 0;
        } else {
            int32_t min_ct64 = ct < 64 ? ct : 64;
            int32_t max_ct64 = ct - 64 > 0 ? ct - 64 : 0;
            val = c < min_ct64 ? 7 : 0;
            if (c < max_ct64) val = 15;
        }
        frame[(FRAME_SIZE - 1) * FRAME_SIZE + c] = (int8_t)val;
    }
}
