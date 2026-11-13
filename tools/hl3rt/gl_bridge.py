"""Desktop OpenGL ES 1.1 bridge for Heroes Lore 3 with True Widescreen and Viewport Management.

Forwards guest GLES 1.1 calls to host OpenGL with:
  * Aspect-fit with letterbox/pillarbox (3:2, 16:9, 16:10, 20:9, 4:3, free window)
  * True Widescreen horizontal expansion (glOrthof)
  * Pixel-perfect scissor box and viewport scaling
  * Calibrated window-to-game coordinate transformation
"""

from __future__ import annotations

import ctypes
import struct
import OpenGL
OpenGL.ERROR_CHECKING = False
from OpenGL.GL import *
from OpenGL.raw.GL.VERSION import GL_1_1 as _gl11

from .display import (
    ASPECT_PRESET_RATIOS as ASPECT_RATIOS,
    DEFAULT_ASPECT_MODE,
    ViewportLayout,
    _fit_aspect_rect,
    compute_viewport_layout,
    normalize_aspect_mode,
)
from .widescreen import WidescreenController


class GLBridge:
    def __init__(self, m, win_w: int = 960, win_h: int = 640, aspect_mode: str = DEFAULT_ASPECT_MODE):
        self.m = m
        self.enabled = True
        self.win_w = win_w
        self.win_h = win_h
        self.game_w = 480.0
        self.game_h = 320.0
        self.aspect_name = normalize_aspect_mode(aspect_mode)
        self.true_widescreen = self.aspect_name in ("16:9", "16:10", "20:9", "4:3", "auto")
        self._widescreen_override: bool | None = None
        self.world_active = True
        self.viewport_rect = (0, 0, win_w, win_h)
        self.layout: ViewportLayout = compute_viewport_layout(self.aspect_name, win_w, win_h, True)
        self.widescreen = WidescreenController(m, gl_bridge=self) if m is not None else None
        self._ui_offset: tuple[int, int] = (0, 0)
        self._last_ortho: tuple[float, float, float, float, float, float] | None = None

        self.v_size, self.v_type, self.v_stride, self.v_ptr = 2, GL_FLOAT, 0, 0
        self.t_size, self.t_type, self.t_stride, self.t_ptr = 2, GL_FLOAT, 0, 0
        self.c_size, self.c_type, self.c_stride, self.c_ptr = 4, GL_UNSIGNED_BYTE, 0, 0
        self.client_state_vert = False
        self.client_state_tex = False
        self.client_state_col = False
        self.textures: set[int] = set()
        self.cur_matrix_mode = GL_MODELVIEW

        # Per-frame / per-texture state caches to avoid redundant PyOpenGL FFI calls
        self._bound_tex: int | None = None
        self._tex_params: dict[int, dict[int, int]] = {}
        self._enabled_caps: dict[int, bool] = {}
        self._client_state_known: set[int] = set()
        self._blend_func: tuple[int, int] | None = None
        self._alpha_func: tuple[int, float] | None = None
        self._mv_depth: int = 1
        self._proj_depth: int = 1
        self._v_buf_cap = 65536
        self._v_buf = (ctypes.c_char * self._v_buf_cap)()
        self._v_buf_ptr = ctypes.cast(self._v_buf, ctypes.c_void_p)
        self._t_buf_cap = 65536
        self._t_buf = (ctypes.c_char * self._t_buf_cap)()
        self._t_buf_ptr = ctypes.cast(self._t_buf, ctypes.c_void_p)
        self._c_buf_cap = 65536
        self._c_buf = (ctypes.c_char * self._c_buf_cap)()
        self._c_buf_ptr = ctypes.cast(self._c_buf, ctypes.c_void_p)

        self.update_viewport(win_w, win_h)

    # -------------------------------------------------------- Viewport manager
    def set_aspect_mode(self, aspect_name: str, true_widescreen: bool | None = None):
        self.aspect_name = normalize_aspect_mode(aspect_name)
        self._widescreen_override = true_widescreen
        self.update_viewport(self.win_w, self.win_h)

    def set_world_active(self, world_active: bool):
        world_active = bool(world_active)
        if world_active != self.world_active:
            self.world_active = world_active
            self.update_viewport(self.win_w, self.win_h)

    def update_viewport(self, win_w: int, win_h: int):
        self.win_w = max(1, win_w)
        self.win_h = max(1, win_h)
        if self._widescreen_override is False:
            target = ASPECT_RATIOS.get(self.aspect_name, 1.5)
            vx, vy, vw, vh = _fit_aspect_rect(self.win_w, self.win_h, target)
            self.layout = ViewportLayout(
                aspect_mode=self.aspect_name,
                world_active=self.world_active,
                win_w=self.win_w,
                win_h=self.win_h,
                ortho_w=480.0,
                ortho_h=320.0,
                patch_w=480,
                patch_h=320,
                viewport_rect=(vx, vy, vw, vh),
                true_widescreen=False,
                ui_dx=0,
                ui_dy=0,
                hud_dx_center=0,
                hud_dx_right=0,
                hud_dy_bottom=0,
            )
        else:
            mode = self.aspect_name
            if self._widescreen_override is True and mode == "original":
                layout_win_aspect = float(self.win_w) / float(self.win_h)
                mode = "auto" if abs(layout_win_aspect - 1.5) > 0.01 else "original"
            self.layout = compute_viewport_layout(mode, self.win_w, self.win_h, world_active=self.world_active)

        self.game_w = float(self.layout.ortho_w)
        self.game_h = float(self.layout.ortho_h)
        self.viewport_rect = self.layout.viewport_rect
        self.true_widescreen = self.layout.true_widescreen

    def sync_guest(self):
        if self.widescreen is not None:
            self.widescreen.apply_layout(self.layout)

    def set_ui_offset(self, ox: int, oy: int):
        nxt = (int(ox), int(oy))
        if nxt == self._ui_offset:
            return
        self._ui_offset = nxt
        if self._last_ortho is not None:
            l, r, b, t, n, f = self._last_ortho
            glMatrixMode(GL_PROJECTION)
            glLoadIdentity()
            glOrtho(l - nxt[0], r - nxt[0], b - nxt[1], t - nxt[1], n, f)
            glMatrixMode(self.cur_matrix_mode)

    def window_to_game_coord(self, wx: int, wy: int) -> tuple[float, float]:
        vx, vy, vw, vh = self.viewport_rect
        px = wx - vx
        # Convert top-down Y (pygame) into top-down inside the viewport rect
        py = wy - (self.win_h - (vy + vh))
        gx = max(0.0, min(float(self.game_w), (px / max(1, vw)) * self.game_w))
        gy = max(0.0, min(float(self.game_h), (py / max(1, vh)) * self.game_h))
        return gx, gy

    def begin_frame(self):
        """Clears letterbox/pillarbox borders and resets transient per-frame GL caches."""
        self._ui_offset = (0, 0)
        self._last_ortho = None
        if self.widescreen is not None:
            self.widescreen._in_ui_main_draw = False
            self.widescreen._pending_flush = None
        self._bound_tex = None
        self._enabled_caps.clear()
        self._client_state_known.clear()
        self._blend_func = None
        self._alpha_func = None
        self.sync_guest()
        if self.viewport_rect != (0, 0, self.win_w, self.win_h):
            glViewport(0, 0, self.win_w, self.win_h)
            glClearColor(0.0, 0.0, 0.0, 1.0)
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        vx, vy, vw, vh = self.viewport_rect
        glViewport(vx, vy, vw, vh)
        try:
            glEnable(GL_TEXTURE_2D)
            self._enabled_caps[GL_TEXTURE_2D] = True
            glColor4f(1.0, 1.0, 1.0, 1.0)
            if self._proj_depth > 1:
                glMatrixMode(GL_PROJECTION)
                while self._proj_depth > 1:
                    glPopMatrix()
                    self._proj_depth -= 1
            if self._mv_depth > 1:
                glMatrixMode(GL_MODELVIEW)
                while self._mv_depth > 1:
                    glPopMatrix()
                    self._mv_depth -= 1
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
        if target == GL_TEXTURE_2D and self._bound_tex == tex_id:
            return
        glBindTexture(target, tex_id)
        if target == GL_TEXTURE_2D:
            self._bound_tex = tex_id
            tp = self._tex_params.get(tex_id)
            if tp is None:
                tp = {}
                self._tex_params[tex_id] = tp
            for pname, param in (
                (GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE),
                (GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE),
                (GL_TEXTURE_MIN_FILTER, GL_NEAREST),
                (GL_TEXTURE_MAG_FILTER, GL_NEAREST),
            ):
                if tp.get(pname) != param:
                    glTexParameteri(target, pname, param)
                    tp[pname] = param

    def delete_textures(self, tex_ids: list[int]):
        for tid in tex_ids:
            self._tex_params.pop(tid, None)
            self.textures.discard(tid)
            if self._bound_tex == tid:
                self._bound_tex = None
        if tex_ids:
            try:
                glDeleteTextures(tex_ids)
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
            self.tex_parameter_i(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
            self.tex_parameter_i(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST)
        except Exception:
            pass

    def tex_parameter_i(self, target: int, pname: int, param: int):
        # Force GL_NEAREST to prevent texture atlas bleeding and quad grid seams on scaled resolutions
        if pname in (GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER):
            param = GL_NEAREST
        if target == GL_TEXTURE_2D and self._bound_tex is not None:
            tp = self._tex_params.setdefault(self._bound_tex, {})
            if tp.get(pname) == param:
                return
            tp[pname] = param
        glTexParameteri(target, pname, param)

    def tex_parameter_f(self, target: int, pname: int, param: float):
        glTexParameterf(target, pname, param)

    def tex_env_i(self, target: int, pname: int, param: int):
        glTexEnvi(target, pname, param)

    def matrix_mode(self, mode: int):
        self.cur_matrix_mode = mode
        glMatrixMode(mode)

    def load_identity(self):
        if self.cur_matrix_mode == GL_PROJECTION:
            self._last_ortho = None
        glLoadIdentity()

    def push_matrix(self):
        if self.cur_matrix_mode == GL_PROJECTION:
            if self._proj_depth < 2:
                glPushMatrix()
                self._proj_depth += 1
        else:
            if self._mv_depth < 16:
                glPushMatrix()
                self._mv_depth += 1

    def pop_matrix(self):
        if self.cur_matrix_mode == GL_PROJECTION:
            if self._proj_depth > 1:
                glPopMatrix()
                self._proj_depth -= 1
        else:
            if self._mv_depth > 1:
                glPopMatrix()
                self._mv_depth -= 1

    def ortho_f(self, l: float, r: float, b: float, t: float, n: float, f: float):
        # Expand guest ortho bounds to match exact square-pixel viewport width/height
        if self.true_widescreen and abs(l - 0.0) < 0.1 and r >= 479.0:
            r = float(self.game_w)
        if self.true_widescreen and abs(t - 0.0) < 0.1 and b >= 319.0:
            b = float(self.game_h)
        self._last_ortho = (l, r, b, t, n, f)
        ox, oy = self._ui_offset
        glOrtho(l - ox, r - ox, b - oy, t - oy, n, f)

    def enable(self, cap: int):
        if self._enabled_caps.get(cap) is True:
            return
        self._enabled_caps[cap] = True
        glEnable(cap)

    def disable(self, cap: int):
        if self._enabled_caps.get(cap) is False:
            return
        self._enabled_caps[cap] = False
        glDisable(cap)

    def enable_client_state(self, cap: int):
        already = cap in self._client_state_known
        if cap == GL_VERTEX_ARRAY:
            if already and self.client_state_vert:
                return
            self.client_state_vert = True
        elif cap == GL_TEXTURE_COORD_ARRAY:
            if already and self.client_state_tex:
                return
            self.client_state_tex = True
        elif cap == GL_COLOR_ARRAY:
            if already and self.client_state_col:
                return
            self.client_state_col = True
        self._client_state_known.add(cap)
        glEnableClientState(cap)

    def disable_client_state(self, cap: int):
        already = cap in self._client_state_known
        if cap == GL_VERTEX_ARRAY:
            if already and not self.client_state_vert:
                return
            self.client_state_vert = False
        elif cap == GL_TEXTURE_COORD_ARRAY:
            if already and not self.client_state_tex:
                return
            self.client_state_tex = False
        elif cap == GL_COLOR_ARRAY:
            if already and not self.client_state_col:
                return
            self.client_state_col = False
        self._client_state_known.add(cap)
        glDisableClientState(cap)

    def blend_func(self, sfactor: int, dfactor: int):
        state = (sfactor, dfactor)
        if self._blend_func == state:
            return
        self._blend_func = state
        glBlendFunc(sfactor, dfactor)

    def alpha_func(self, func: int, ref: float):
        state = (func, ref)
        if self._alpha_func == state:
            return
        self._alpha_func = state
        glAlphaFunc(func, ref)

    def clear_color(self, r: float, g: float, b: float, a: float):
        glClearColor(r, g, b, a)

    def clear(self, mask: int):
        glClear(mask)

    def scissor(self, x: int, y: int, w: int, h: int):
        ox, oy = self._ui_offset
        x = x + ox
        y = y - oy
        vx, vy, vw, vh = self.viewport_rect
        # Scale guest scissor box directly into host viewport space (clamped cleanly to viewport bounds)
        sx0 = vx + int((x / self.game_w) * vw)
        sy0 = vy + int((y / self.game_h) * vh)
        sx1 = vx + int(((x + w) / self.game_w) * vw + 0.5)
        sy1 = vy + int(((y + h) / self.game_h) * vh + 0.5)
        sx = max(vx, min(vx + vw, sx0))
        sy = max(vy, min(vy + vh, sy0))
        sw = max(1, min(vx + vw, sx1) - sx)
        sh = max(1, min(vy + vh, sy1) - sy)
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
        m = self.m
        if self.client_state_vert and self.v_ptr != 0:
            elem_bytes = self.v_size * (4 if self.v_type in (GL_FLOAT, 0x140C) else (2 if self.v_type in (GL_SHORT, GL_UNSIGNED_SHORT) else 1))
            strd = self.v_stride if self.v_stride > 0 else elem_bytes
            nbytes = (count - 1) * strd + elem_bytes
            if nbytes > self._v_buf_cap:
                self._v_buf_cap = max(nbytes, self._v_buf_cap * 2)
                self._v_buf = (ctypes.c_char * self._v_buf_cap)()
                self._v_buf_ptr = ctypes.cast(self._v_buf, ctypes.c_void_p)
            m.rdinto(self.v_ptr + first * strd, self._v_buf, nbytes)
            _gl11.glVertexPointer(self.v_size, self.v_type, self.v_stride, self._v_buf_ptr)

        if self.client_state_tex and self.t_ptr != 0:
            elem_bytes = self.t_size * (4 if self.t_type in (GL_FLOAT, 0x140C) else (2 if self.t_type in (GL_SHORT, GL_UNSIGNED_SHORT) else 1))
            strd = self.t_stride if self.t_stride > 0 else elem_bytes
            nbytes = (count - 1) * strd + elem_bytes
            if nbytes > self._t_buf_cap:
                self._t_buf_cap = max(nbytes, self._t_buf_cap * 2)
                self._t_buf = (ctypes.c_char * self._t_buf_cap)()
                self._t_buf_ptr = ctypes.cast(self._t_buf, ctypes.c_void_p)
            m.rdinto(self.t_ptr + first * strd, self._t_buf, nbytes)
            _gl11.glTexCoordPointer(self.t_size, self.t_type, self.t_stride, self._t_buf_ptr)

        if self.client_state_col and self.c_ptr != 0:
            elem_bytes = self.c_size * (4 if self.c_type in (GL_FLOAT, 0x140C) else (2 if self.c_type in (GL_SHORT, GL_UNSIGNED_SHORT) else 1))
            strd = self.c_stride if self.c_stride > 0 else elem_bytes
            nbytes = (count - 1) * strd + elem_bytes
            if nbytes > self._c_buf_cap:
                self._c_buf_cap = max(nbytes, self._c_buf_cap * 2)
                self._c_buf = (ctypes.c_char * self._c_buf_cap)()
                self._c_buf_ptr = ctypes.cast(self._c_buf, ctypes.c_void_p)
            m.rdinto(self.c_ptr + first * strd, self._c_buf, nbytes)
            _gl11.glColorPointer(self.c_size, self.c_type, self.c_stride, self._c_buf_ptr)

        _gl11.glDrawArrays(mode, 0, count)
