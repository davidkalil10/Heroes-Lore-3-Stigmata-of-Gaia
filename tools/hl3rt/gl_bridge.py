"""Desktop OpenGL ES 1.1 bridge for Heroes Lore 3 with True Widescreen and Viewport Management.

Forwards guest GLES 1.1 calls to host OpenGL with:
  * Aspect-fit with letterbox/pillarbox (3:2, 16:9, 16:10, 20:9, 4:3, free window)
  * True Widescreen horizontal expansion (glOrthof)
  * Pixel-perfect scissor box and viewport scaling
  * Calibrated window-to-game coordinate transformation
"""

from __future__ import annotations

import struct
from OpenGL.GL import *

ASPECT_RATIOS = {
    "3:2": 480.0 / 320.0,  # 1.50 (Original)
    "16:9": 16.0 / 9.0,  # 1.777...
    "16:10": 16.0 / 10.0,  # 1.60
    "20:9": 20.0 / 9.0,  # 2.222...
    "4:3": 4.0 / 3.0,  # 1.333...
}


class GLBridge:
    def __init__(self, m, win_w: int = 960, win_h: int = 640):
        self.m = m
        self.enabled = True
        self.win_w = win_w
        self.win_h = win_h
        self.game_w = 480.0
        self.game_h = 320.0
        self.aspect_name = "3:2"
        self.true_widescreen = False
        self.viewport_rect = (0, 0, win_w, win_h)

        self.v_size, self.v_type, self.v_stride, self.v_ptr = 2, GL_FLOAT, 0, 0
        self.t_size, self.t_type, self.t_stride, self.t_ptr = 2, GL_FLOAT, 0, 0
        self.c_size, self.c_type, self.c_stride, self.c_ptr = 4, GL_UNSIGNED_BYTE, 0, 0
        self.client_state_vert = False
        self.client_state_tex = False
        self.client_state_col = False
        self.textures: set[int] = set()
        self.cur_matrix_mode = GL_MODELVIEW

        self.update_viewport(win_w, win_h)

    # -------------------------------------------------------- Viewport manager
    def set_aspect_mode(self, aspect_name: str, true_widescreen: bool = False):
        if aspect_name in ASPECT_RATIOS:
            self.aspect_name = aspect_name
        self.true_widescreen = true_widescreen
        self.update_viewport(self.win_w, self.win_h)

    def update_viewport(self, win_w: int, win_h: int):
        self.win_w = max(1, win_w)
        self.win_h = max(1, win_h)

        if self.true_widescreen:
            # Expand horizontal ortho coordinate space to match window aspect ratio
            aspect = float(self.win_w) / float(self.win_h)
            self.game_w = round(320.0 * aspect, 1)
            self.game_h = 320.0
            self.viewport_rect = (0, 0, self.win_w, self.win_h)
        else:
            # Pillarbox / Letterbox to maintain aspect ratio
            target_aspect = ASPECT_RATIOS.get(self.aspect_name, 1.5)
            win_aspect = float(self.win_w) / float(self.win_h)
            self.game_w = 480.0
            self.game_h = 320.0

            if win_aspect > target_aspect:
                # Window is wider than target -> pillarbox (left/right bars)
                vh = self.win_h
                vw = int(vh * target_aspect)
                vx = (self.win_w - vw) // 2
                vy = 0
            else:
                # Window is taller than target -> letterbox (top/bottom bars)
                vw = self.win_w
                vh = int(vw / target_aspect)
                vx = 0
                vy = (self.win_h - vh) // 2
            self.viewport_rect = (vx, vy, vw, vh)

    def window_to_game_coord(self, wx: int, wy: int) -> tuple[float, float]:
        vx, vy, vw, vh = self.viewport_rect
        # wx is in window pixels from left
        # wy is in window pixels from top
        px = wx - vx
        # Convert top-down Y (pygame) into top-down inside the viewport rect
        # OpenGL Y origin is at bottom, but window origin is at top
        py = wy - (self.win_h - (vy + vh))
        gx = max(0.0, min(float(self.game_w), (px / max(1, vw)) * self.game_w))
        gy = max(0.0, min(float(self.game_h), (py / max(1, vh)) * self.game_h))
        return gx, gy

    def begin_frame(self):
        """Clears letterbox/pillarbox borders to solid black before drawing the frame."""
        if not self.true_widescreen and self.viewport_rect != (0, 0, self.win_w, self.win_h):
            glViewport(0, 0, self.win_w, self.win_h)
            glClearColor(0.0, 0.0, 0.0, 1.0)
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        vx, vy, vw, vh = self.viewport_rect
        glViewport(vx, vy, vw, vh)
        try:
            glMatrixMode(GL_PROJECTION)
            while glGetIntegerv(GL_PROJECTION_STACK_DEPTH) > 1:
                glPopMatrix()
            glMatrixMode(GL_MODELVIEW)
            while glGetIntegerv(GL_MODELVIEW_STACK_DEPTH) > 1:
                glPopMatrix()
            if self.cur_matrix_mode:
                glMatrixMode(self.cur_matrix_mode)
        except Exception:
            pass

    # ------------------------------------------------------------- OpenGL calls
    def type_bytes(self, gl_type: int) -> int:
        if gl_type in (GL_FLOAT, 0x140C):  # GL_FLOAT, GL_FIXED
            return 4
        if gl_type in (GL_SHORT, GL_UNSIGNED_SHORT):
            return 2
        return 1

    def gen_textures(self, n: int) -> list[int]:
        ids = glGenTextures(n)
        if hasattr(ids, "__iter__"):
            ids = [int(x) for x in ids]
        else:
            ids = [int(ids)]
        self.textures.update(ids)
        return ids

    def bind_texture(self, target: int, tex_id: int):
        glBindTexture(target, tex_id)
        try:
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST)
        except Exception:
            pass

    def tex_image_2d(
        self,
        target: int,
        level: int,
        ifmt: int,
        w: int,
        h: int,
        border: int,
        fmt: int,
        tp: int,
        pix_ptr: int,
    ):
        if pix_ptr == 0:
            glTexImage2D(target, level, ifmt, w, h, border, fmt, tp, None)
            return
        bpp = 4
        if fmt == GL_RGB:
            bpp = 3
        elif fmt == GL_LUMINANCE_ALPHA:
            bpp = 2
        elif fmt in (GL_LUMINANCE, GL_ALPHA):
            bpp = 1
        nbytes = w * h * bpp
        data = self.m.rdbytes(pix_ptr, nbytes)
        glTexImage2D(target, level, ifmt, w, h, border, fmt, tp, data)
        try:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST)
        except Exception:
            pass

    def tex_parameter_i(self, target: int, pname: int, param: int):
        # Force GL_NEAREST to prevent texture atlas bleeding and quad grid seams on scaled resolutions
        if pname in (GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER):
            param = GL_NEAREST
        glTexParameteri(target, pname, param)

    def tex_parameter_f(self, target: int, pname: int, param: float):
        glTexParameterf(target, pname, param)

    def tex_env_i(self, target: int, pname: int, param: int):
        glTexEnvi(target, pname, param)

    def matrix_mode(self, mode: int):
        self.cur_matrix_mode = mode
        glMatrixMode(mode)

    def load_identity(self):
        glLoadIdentity()

    def push_matrix(self):
        try:
            depth = glGetIntegerv(GL_MODELVIEW_STACK_DEPTH if self.cur_matrix_mode == GL_MODELVIEW else GL_PROJECTION_STACK_DEPTH)
            max_depth = 16 if self.cur_matrix_mode == GL_MODELVIEW else 2
            if depth < max_depth:
                glPushMatrix()
        except Exception:
            pass

    def pop_matrix(self):
        try:
            depth = glGetIntegerv(GL_MODELVIEW_STACK_DEPTH if self.cur_matrix_mode == GL_MODELVIEW else GL_PROJECTION_STACK_DEPTH)
            if depth > 1:
                glPopMatrix()
        except Exception:
            pass

    def ortho_f(self, l: float, r: float, b: float, t: float, n: float, f: float):
        # In True Widescreen mode, expand the 480px ortho width to the full canvas width
        if self.true_widescreen and abs(l - 0.0) < 0.1 and abs(r - 480.0) < 0.1:
            r = float(self.game_w)
        glOrtho(l, r, b, t, n, f)

    def enable(self, cap: int):
        glEnable(cap)

    def disable(self, cap: int):
        glDisable(cap)

    def enable_client_state(self, cap: int):
        if cap == GL_VERTEX_ARRAY:
            self.client_state_vert = True
        elif cap == GL_TEXTURE_COORD_ARRAY:
            self.client_state_tex = True
        elif cap == GL_COLOR_ARRAY:
            self.client_state_col = True
        glEnableClientState(cap)

    def disable_client_state(self, cap: int):
        if cap == GL_VERTEX_ARRAY:
            self.client_state_vert = False
        elif cap == GL_TEXTURE_COORD_ARRAY:
            self.client_state_tex = False
        elif cap == GL_COLOR_ARRAY:
            self.client_state_col = False
        glDisableClientState(cap)

    def blend_func(self, sfactor: int, dfactor: int):
        glBlendFunc(sfactor, dfactor)

    def alpha_func(self, func: int, ref: float):
        glAlphaFunc(func, ref)

    def clear_color(self, r: float, g: float, b: float, a: float):
        glClearColor(r, g, b, a)

    def clear(self, mask: int):
        glClear(mask)

    def scissor(self, x: int, y: int, w: int, h: int):
        vx, vy, vw, vh = self.viewport_rect
        # Scale guest scissor box into host viewport space (converting top-down Y to bottom-up GL)
        sx = vx + int((x / self.game_w) * vw)
        sy = vy + int(((self.game_h - (y + h)) / self.game_h) * vh)
        sw = max(1, int((w / self.game_w) * vw))
        sh = max(1, int((h / self.game_h) * vh))
        glScissor(sx, sy, sw, sh)

    def viewport(self, x: int, y: int, w: int, h: int):
        vx, vy, vw, vh = self.viewport_rect
        glViewport(vx, vy, vw, vh)

    def vertex_pointer(self, sz: int, tp: int, strd: int, ptr: int):
        self.v_size, self.v_type, self.v_stride, self.v_ptr = sz, tp, strd, ptr

    def tex_coord_pointer(self, sz: int, tp: int, strd: int, ptr: int):
        self.t_size, self.t_type, self.t_stride, self.t_ptr = sz, tp, strd, ptr

    def color_pointer(self, sz: int, tp: int, strd: int, ptr: int):
        self.c_size, self.c_type, self.c_stride, self.c_ptr = sz, tp, strd, ptr

    def draw_arrays(self, mode: int, first: int, count: int):
        if count <= 0:
            return
        if self.client_state_vert and self.v_ptr != 0:
            strd = self.v_stride if self.v_stride > 0 else self.v_size * self.type_bytes(self.v_type)
            nbytes = (first + count) * strd
            vdata = self.m.rdbytes(self.v_ptr, nbytes)
            glVertexPointer(self.v_size, self.v_type, self.v_stride, vdata)

        if self.client_state_tex and self.t_ptr != 0:
            strd = self.t_stride if self.t_stride > 0 else self.t_size * self.type_bytes(self.t_type)
            nbytes = (first + count) * strd
            tdata = self.m.rdbytes(self.t_ptr, nbytes)
            glTexCoordPointer(self.t_size, self.t_type, self.t_stride, tdata)

        if self.client_state_col and self.c_ptr != 0:
            strd = self.c_stride if self.c_stride > 0 else self.c_size * self.type_bytes(self.c_type)
            nbytes = (first + count) * strd
            cdata = self.m.rdbytes(self.c_ptr, nbytes)
            glColorPointer(self.c_size, self.c_type, self.c_stride, cdata)

        glDrawArrays(mode, first, count)
