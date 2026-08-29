import numpy as np
from ._base import GameData, Setup, extract, load_reference_game, reference_dir


EXTRACT_KWARGS = {"extra_slots": 2}


MAX_COLS = 6


MAX_BITS = 6


MAX_WALLS = 5


MAX_CHECKPOINTS = 4


MAX_HAZARDS = 2


MAX_BUTTONS = 5


PITCH = 4


COLOR_UNSELECTED = 2


COLOR_ON = 5


COLOR_OFF = 1


COLOR_SELECTED = 9


COLOR_FLASH = 14


SWITCH_HIGHLIGHT = 10


_GHOST_PATTERN = np.array(
    [[12, 13, 12, 13], [13, 12, 13, 12], [12, 13, 12, 13], [13, -1, -1, 12]], np.int8
)


_MOVE = {1: (-PITCH, 0), 2: (PITCH, 0), 3: (0, PITCH), 10: (2 * PITCH, 0), 11: (2 * PITCH, 0),
         12: (-2 * PITCH, 0), 13: (-2 * PITCH, 0), 33: (0, -PITCH), 34: (-PITCH, 0)}


_ROTATE = {5: 90, 6: -90, 7: 180, 16: 270}


_SCALE = {8: 1, 9: -1}


_RECOLOR = {14: 9, 15: 8, 63: 15}


def _tables() -> tuple[np.ndarray, ...]:
    dx = np.zeros(64, np.int32)
    dy = np.zeros(64, np.int32)
    is_move = np.zeros(64, bool)
    for code, (mx, my) in _MOVE.items():
        dx[code], dy[code], is_move[code] = mx, my, True
    drot = np.zeros(64, np.int32)
    is_rotate = np.zeros(64, bool)
    for code, d in _ROTATE.items():
        drot[code], is_rotate[code] = d, True
    dscale = np.zeros(64, np.int32)
    is_scale = np.zeros(64, bool)
    for code, d in _SCALE.items():
        dscale[code], is_scale[code] = d, True
    recolor = np.zeros(64, np.int32)
    is_recolor = np.zeros(64, bool)
    for code, c in _RECOLOR.items():
        recolor[code], is_recolor[code] = c, True
    return dx, dy, is_move, drot, is_rotate, dscale, is_scale, recolor, is_recolor


def _contains(cx, cy, cw, ch, x, y):
    return (x >= cx) & (y >= cy) & (x < cx + cw) & (y < ch + cy)


DATA_PARAM = 'data'


def derive(data):
    self = Setup(data)
    L = data.num_levels
    tag = lambda name: data.tags[:, :, data.tag_index(name)] & data.alive
    container = tag("plljmx")
    robot_slot = np.full((L, 2), -1, np.int32)
    container_box = np.zeros((L, 2, 4), np.int32)
    target_slot = np.full((L, 2), -1, np.int32)
    grid_slot = np.full((L, 2), -1, np.int32)
    grid_box = np.zeros((L, 2, 4), np.int32)
    view_only = np.zeros((L, 2), bool)
    switch_slot = np.full((L, 2), -1, np.int32)
    switch_box = np.zeros((L, 2, 4), np.int32)
    grsysj_box = np.zeros((L, 2, 4), np.int32)
    num_cols = np.zeros((L, 2), np.int32)
    col_slot = np.full((L, 2, MAX_COLS), -1, np.int32)
    num_bits = np.zeros((L, 2), np.int32)
    cell_slot = np.full((L, 2, MAX_COLS, MAX_BITS), -1, np.int32)
    cell_state_pixel = np.zeros((L, 2, MAX_COLS, MAX_BITS, 2), np.int32)
    num_walls = np.zeros((L, 2), np.int32)
    wall_box = np.zeros((L, 2, MAX_WALLS, 4), np.int32)
    num_checkpoints = np.zeros((L, 2), np.int32)
    checkpoint_slot = np.full((L, 2, MAX_CHECKPOINTS), -1, np.int32)
    num_hazards = np.zeros((L, 2), np.int32)
    hazard_icon = np.full((L, 2, MAX_HAZARDS), -1, np.int32)
    hazard_trig = np.full((L, 2, MAX_HAZARDS), -1, np.int32)
    for li in range(L):
        idxs = np.where(container[li])[0]
        idxs = idxs[np.argsort(data.x[li, idxs], kind="stable")]
        for lane, ci in enumerate(idxs):
            cx, cy, cw, ch = (data.x[li, ci], data.y[li, ci], data.w[li, ci], data.h[li, ci])
            container_box[li, lane] = [cx, cy, cw, ch]
            inbox = _contains(cx, cy, cw, ch, data.x[li], data.y[li]) & data.alive[li]
    
            rs = np.where(inbox & tag("bltjrl")[li])[0]
            robot_slot[li, lane] = rs[0]
    
            ts = np.where(inbox & tag("taptxx")[li])[0]
            if len(ts):
                target_slot[li, lane] = ts[0]
    
            gs = np.where(inbox & tag("takfnb")[li])[0]
            if len(gs):
                g = gs[0]
                grid_slot[li, lane] = g
                grid_box[li, lane] = [data.x[li, g], data.y[li, g], data.w[li, g], data.h[li, g]]
                view_only[li, lane] = bool(data.tags[li, g, data.tag_index("reooao")])
    
            ss = np.where(inbox & tag("sucqgk")[li])[0]
            if len(ss):
                s = ss[0]
                switch_slot[li, lane] = s
                switch_box[li, lane] = [data.x[li, s], data.y[li, s], data.w[li, s], data.h[li, s]]
    
            gys = np.where(inbox & tag("grsysj")[li])[0]
            if len(gys):
                g = gys[0]
                grsysj_box[li, lane] = [data.x[li, g], data.y[li, g], data.w[li, g], data.h[li, g]]
    
            cols = np.where(inbox & tag("inwola")[li])[0]
            cols = cols[np.argsort(data.x[li, cols], kind="stable")]
            num_cols[li, lane] = len(cols)
            for ci2, c in enumerate(cols):
                col_slot[li, lane, ci2] = c
                cellbox = _contains(data.x[li, c], data.y[li, c], data.w[li, c], data.h[li, c],
                                     data.x[li], data.y[li])
                cells = np.where(cellbox & tag("Maidxz")[li])[0]
                cells = cells[np.argsort(data.y[li, cells], kind="stable")]
                if ci2 == 0:
                    num_bits[li, lane] = len(cells)
                for bi, cell in enumerate(cells):
                    cell_slot[li, lane, ci2, bi] = cell
                    opaque = np.argwhere(data.pixels[li, cell] >= 0)
                    cell_state_pixel[li, lane, ci2, bi] = opaque[0]
    
            walls = np.where(inbox & tag("wauzms")[li])[0]
            num_walls[li, lane] = len(walls)
            for wi, w in enumerate(walls):
                wall_box[li, lane, wi] = [data.x[li, w], data.y[li, w], data.w[li, w], data.h[li, w]]
    
            chks = np.where(inbox & tag("chrccc")[li])[0]
            num_checkpoints[li, lane] = len(chks)
            for cki, ck in enumerate(chks):
                checkpoint_slot[li, lane, cki] = ck
    
            icons_global = np.where(tag("laycmuofkkgm")[li])[0]
            trigs = np.where(inbox & tag("laycmuommm")[li])[0]
            hz = 0
            for tr in trigs:
                tx, ty, tw, th = data.x[li, tr], data.y[li, tr], data.w[li, tr], data.h[li, tr]
                match = None
                for ic in icons_global:
                    ix, iy, iw, ih = data.x[li, ic], data.y[li, ic], data.w[li, ic], data.h[li, ic]
                    if tx < ix + iw and tx + tw > ix and ty < iy + ih and ty + th > iy:
                        match = ic
                        break
                if match is not None:
                    hazard_icon[li, lane, hz] = match
                    hazard_trig[li, lane, hz] = tr
                    hz += 1
            num_hazards[li, lane] = hz
    button_slot = np.full((L, MAX_BUTTONS), -1, np.int32)
    num_buttons = np.zeros((L,), np.int32)
    button_box = np.zeros((L, MAX_BUTTONS, 4), np.int32)
    button_pos = np.zeros((L, MAX_BUTTONS, 2), np.int32)
    button_rot = np.zeros((L, MAX_BUTTONS), np.int32)
    button_scale = np.ones((L, MAX_BUTTONS), np.int32)
    button_program = np.zeros((L, MAX_BUTTONS, MAX_COLS), np.int32)
    button_reset = np.ones((L, MAX_BUTTONS), bool)
    has_programs = np.zeros((L,), bool)
    for li in range(L):
        btns = np.where(tag("tozzsf")[li])[0]
        btns = btns[np.argsort(data.x[li, btns], kind="stable")]
        num_buttons[li] = len(btns)
        for bi, b in enumerate(btns):
            button_slot[li, bi] = b
            button_box[li, bi] = [data.x[li, b], data.y[li, b], data.w[li, b], data.h[li, b]]
        d = data.level_data[li]
        progs = d.get("Programs") or []
        positions = d.get("Positions") or []
        rotations = d.get("Rotations") or []
        scales = d.get("scvkkws") or []
        resets = d.get("Reset") or []
        has_programs[li] = bool(progs)
        for i in range(len(progs)):
            button_pos[li, i] = positions[i]
            button_rot[li, i] = rotations[i]
            button_scale[li, i] = scales[i]
            if i < len(resets):
                button_reset[li, i] = resets[i]
            for bit, v in enumerate(progs[i]):
                button_program[li, i, bit] = v
    source = sorted(reference_dir().glob(f"{data.game_id}-*.py"))[0]
    ref = load_reference_game(source)
    levels = [lvl.clone() for lvl in ref._clean_levels]
    robot_mask = np.zeros((L, 2, 4, 4), bool)
    robot_rotation0 = np.zeros((L, 2), np.int32)
    robot_scale0 = np.ones((L, 2), np.int32)
    target_rotation = np.zeros((L, 2), np.int32)
    target_scale = np.ones((L, 2), np.int32)
    for li, lvl in enumerate(levels):
        sprites = lvl.get_sprites()
        containers = sorted([s for s in sprites if "plljmx" in s.tags], key=lambda s: s.x)
        for lane, c in enumerate(containers):
            robot = next(
                s for s in sprites if "bltjrl" in s.tags
                and c.x <= s.x < c.x + c.width and c.y <= s.y < c.y + c.height
            )
            robot_mask[li, lane] = robot.pixels >= 0
            robot_rotation0[li, lane] = robot.rotation
            robot_scale0[li, lane] = robot.scale
            target = next(
                (s for s in sprites if "taptxx" in s.tags
                 and c.x <= s.x < c.x + c.width and c.y <= s.y < c.y + c.height),
                None,
            )
            if target is not None:
                target_rotation[li, lane] = target.rotation
                target_scale[li, lane] = target.scale
    j = np.asarray
    self._robot_slot = j(robot_slot)
    self._container_box = j(container_box)
    self._target_slot = j(target_slot)
    self._grid_slot = j(grid_slot)
    self._grid_box = j(grid_box)
    self._view_only = j(view_only)
    self._switch_slot = j(switch_slot)
    self._switch_box = j(switch_box)
    self._grsysj_box = j(grsysj_box)
    self._num_cols = j(num_cols)
    self._col_slot = j(col_slot)
    self._num_bits = j(num_bits)
    self._cell_slot = j(cell_slot)
    self._cell_state_pixel = j(cell_state_pixel)
    self._num_walls = j(num_walls)
    self._wall_box = j(wall_box)
    self._num_checkpoints = j(num_checkpoints)
    self._checkpoint_slot = j(checkpoint_slot)
    self._num_hazards = j(num_hazards)
    self._hazard_icon = j(hazard_icon)
    self._hazard_trig = j(hazard_trig)
    self._num_buttons = j(num_buttons)
    self._button_slot = j(button_slot)
    self._button_box = j(button_box)
    self._button_pos = j(button_pos)
    self._button_rot = j(button_rot)
    self._button_scale = j(button_scale)
    self._button_reset = j(button_reset)
    self._button_program = j(button_program)
    self._has_programs = j(has_programs)
    self._robot_mask = j(robot_mask)
    self._robot_rotation0 = j(robot_rotation0)
    self._robot_scale0 = j(robot_scale0)
    self._target_rotation = j(target_rotation)
    self._target_scale = j(target_scale)
    self._ghost_pattern = j(_GHOST_PATTERN)
    self._ghost_slot = np.asarray([self.num_slots - 2, self.num_slots - 1], np.int32)
    robot_color0 = data.pixels[np.arange(L)[:, None], robot_slot, 1, 1].astype(np.int32)
    self._robot_color0 = j(robot_color0)
    target_color = np.where(
        target_slot >= 0,
        data.pixels[np.arange(L)[:, None], np.clip(target_slot, 0, None), 1, 0].astype(np.int32),
        -999,
    )
    self._target_color = j(target_color)
    target_touch_x = np.where(
        target_slot >= 0,
        data.x[np.arange(L)[:, None], np.clip(target_slot, 0, None)] + target_scale,
        -999,
    )
    target_touch_y = np.where(
        target_slot >= 0,
        data.y[np.arange(L)[:, None], np.clip(target_slot, 0, None)] + target_scale,
        -999,
    )
    self._target_touch_x = j(target_touch_x)
    self._target_touch_y = j(target_touch_y)
    switch_color0 = np.zeros((L, 2), np.int32)
    for li in range(L):
        for lane in range(2):
            s = switch_slot[li, lane]
            if s >= 0:
                w, h = data.w[li, s], data.h[li, s]
                switch_color0[li, lane] = data.pixels[li, s, h // 2, w // 2]
    self._switch_color0 = j(switch_color0)
    ph, pw = data.patch_shape
    clean_target_pixels = np.full((L, 2, ph, pw), -1, np.int8)
    for li in range(L):
        for lane in range(2):
            s = target_slot[li, lane]
            if s >= 0:
                clean_target_pixels[li, lane] = data.pixels[li, s]
    self._clean_target_pixels = j(clean_target_pixels)
    override = np.zeros((L, data.num_slots), bool)
    for name in ("bltjrl", "chrccc", "wauzms", "laycmuofkkgm", "laycmuommm"):
        override |= tag(name)
    self._bbox_override = j(override)
    self._patch_shape = data.patch_shape
    scroll_slot = np.array(
        [np.where(tag("sthpyh")[li])[0][0] for li in range(L)], np.int32
    )
    bg_slot = np.array(
        [np.where(tag("sthpyhbaatdv")[li])[0][0] for li in range(L)], np.int32
    )
    self._scroll_slot = j(scroll_slot)
    self._scroll_bg_x = j(data.x[np.arange(L), bg_slot])
    self._scroll_w = j(data.w[np.arange(L), scroll_slot])
    (self._dx, self._dy, self._is_move, self._drot, self._is_rotate,
     self._dscale, self._is_scale, self._recolor, self._is_recolor) = (j(a) for a in _tables())
    return self


def make_args(source, seed=0):
    return (extract(source, **EXTRACT_KWARGS),), {}
