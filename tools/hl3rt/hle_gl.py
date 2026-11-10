"""OpenGL ES 1.1 HLE.

S03 provides object-name generation, query answers and call counters for headless mode.
S04 provides full forwarders to host OpenGL via `rt.gl_bridge` (GLBridge).
"""

from __future__ import annotations

import struct
from .objc import func

GL_FRAMEBUFFER_COMPLETE_OES = 0x8CD5
GL_RENDERBUFFER_WIDTH_OES = 0x8D42
GL_RENDERBUFFER_HEIGHT_OES = 0x8D43
GL_MAX_TEXTURE_SIZE = 0x0D33
GL_MAX_TEXTURE_UNITS = 0x84E2
GL_VIEWPORT = 0x0BA2

SCREEN_W = 480
SCREEN_H = 320


def _f32(m, reg_val):
    return struct.unpack("<f", struct.pack("<I", reg_val))[0]


@func("_glGenTextures")
def _gl_gen_textures(rt):
    m = rt.m
    n, ptr = m.reg(0), m.reg(1)
    if getattr(rt, "gl_bridge", None):
        ids = rt.gl_bridge.gen_textures(n)
        for i, tid in enumerate(ids):
            m.wr32(ptr + 4 * i, tid)
    else:
        ctr = rt.state.setdefault("gl_ids", {})
        base = ctr.get("tex", 1)
        for i in range(n):
            m.wr32(ptr + 4 * i, base + i)
        ctr["tex"] = base + n
    return 0


@func("_glBindTexture")
def _gl_bind_texture(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.bind_texture(rt.m.reg(0), rt.m.reg(1))
    return 0


@func("_glTexImage2D")
def _gl_tex_image_2d(rt):
    m = rt.m
    target, level, ifmt, w, h = [m.reg(i) for i in range(4)] + [m.rd32(m.sp)]
    border, fmt, tp, pix = [m.rd32(m.sp + 4 * i) for i in range(1, 5)]
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.tex_image_2d(target, level, ifmt, w, h, border, fmt, tp, pix)
    return 0


@func("_glTexParameteri")
def _gl_tex_param_i(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.tex_parameter_i(rt.m.reg(0), rt.m.reg(1), rt.m.reg(2))
    return 0


@func("_glTexParameterf")
def _gl_tex_param_f(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.tex_parameter_f(rt.m.reg(0), rt.m.reg(1), _f32(rt.m, rt.m.reg(2)))
    return 0


@func("_glTexEnvx|_glTexEnvi")
def _gl_tex_env_i(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.tex_env_i(rt.m.reg(0), rt.m.reg(1), rt.m.reg(2))
    return 0


@func("_glPixelStorei")
def _gl_pixel_store_i(rt):
    return 0


@func("_glMatrixMode")
def _gl_matrix_mode(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.matrix_mode(rt.m.reg(0))
    return 0


@func("_glLoadIdentity")
def _gl_load_identity(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.load_identity()
    return 0


@func("_glPushMatrix")
def _gl_push_matrix(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.push_matrix()
    return 0


@func("_glPopMatrix")
def _gl_pop_matrix(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.pop_matrix()
    return 0


@func("_glOrthof")
def _gl_orthof(rt):
    m = rt.m
    l, r, b, t = [_f32(m, m.reg(i)) for i in range(4)]
    n, f = [_f32(m, m.rd32(m.sp + 4 * i)) for i in range(2)]
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.ortho_f(l, r, b, t, n, f)
    return 0


@func("_glEnable")
def _gl_enable(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.enable(rt.m.reg(0))
    return 0


@func("_glDisable")
def _gl_disable(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.disable(rt.m.reg(0))
    return 0


@func("_glEnableClientState")
def _gl_enable_client_state(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.enable_client_state(rt.m.reg(0))
    return 0


@func("_glDisableClientState")
def _gl_disable_client_state(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.disable_client_state(rt.m.reg(0))
    return 0


@func("_glBlendFunc")
def _gl_blend_func(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.blend_func(rt.m.reg(0), rt.m.reg(1))
    return 0


@func("_glAlphaFunc")
def _gl_alpha_func(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.alpha_func(rt.m.reg(0), _f32(rt.m, rt.m.reg(1)))
    return 0


@func("_glClearColor")
def _gl_clear_color(rt):
    m = rt.m
    r, g, b, a = [_f32(m, m.reg(i)) for i in range(4)]
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.clear_color(r, g, b, a)
    return 0


@func("_glClear")
def _gl_clear(rt):
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.clear(rt.m.reg(0))
    return 0


@func("_glScissor")
def _gl_scissor(rt):
    m = rt.m
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.scissor(m.reg(0), m.reg(1), m.reg(2), m.reg(3))
    return 0


@func("_glViewport")
def _gl_viewport(rt):
    m = rt.m
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.viewport(m.reg(0), m.reg(1), m.reg(2), m.reg(3))
    return 0


@func("_glVertexPointer")
def _gl_vertex_pointer(rt):
    m = rt.m
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.vertex_pointer(m.reg(0), m.reg(1), m.reg(2), m.reg(3))
    return 0


@func("_glTexCoordPointer")
def _gl_tex_coord_pointer(rt):
    m = rt.m
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.tex_coord_pointer(m.reg(0), m.reg(1), m.reg(2), m.reg(3))
    return 0


@func("_glColorPointer")
def _gl_color_pointer(rt):
    m = rt.m
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.color_pointer(m.reg(0), m.reg(1), m.reg(2), m.reg(3))
    return 0


@func("_glDrawArrays")
def _gl_draw_arrays(rt):
    m = rt.m
    if getattr(rt, "gl_bridge", None):
        rt.gl_bridge.draw_arrays(m.reg(0), m.reg(1), m.reg(2))
    return 0


@func("_glDrawElements")
def _gl_draw_elements(rt):
    return 0


@func("_glGetError")
def _gl_get_error(rt):
    return 0


@func("_glCheckFramebufferStatusOES")
def _gl_fb_status(rt):
    return GL_FRAMEBUFFER_COMPLETE_OES


@func("_glGetRenderbufferParameterivOES")
def _gl_rb_param(rt):
    m = rt.m
    pname, ptr = m.reg(1), m.reg(2)
    m.wr32(ptr, SCREEN_W if pname == GL_RENDERBUFFER_WIDTH_OES else SCREEN_H)
    return 0


@func("_glGetIntegerv")
def _gl_get_integerv(rt):
    m = rt.m
    pname, ptr = m.reg(0), m.reg(1)
    if pname == GL_MAX_TEXTURE_SIZE:
        m.wr32(ptr, 1024)
    elif pname == GL_MAX_TEXTURE_UNITS:
        m.wr32(ptr, 2)
    elif pname == GL_VIEWPORT:
        for i, v in enumerate((0, 0, SCREEN_W, SCREEN_H)):
            m.wr32(ptr + 4 * i, v)
    else:
        m.wr32(ptr, 0)
    return 0


@func("_glGetString")
def _gl_get_string(rt):
    name = rt.m.reg(0)
    table = {
        0x1F00: "Apple Inc.",
        0x1F01: "Apple Software Renderer",
        0x1F02: "OpenGL ES-CM 1.1",
        0x1F03: "GL_OES_framebuffer_object",
    }
    cache = rt.state.setdefault("gl_strings", {})
    if name not in cache:
        cache[name] = rt.m.new_cstr(table.get(name, ""))
    return cache[name]


@func("_glIsEnabled")
def _gl_is_enabled(rt):
    return 0


@func("_glGenBuffers")
def _gl_gen_buffers(rt):
    m = rt.m
    n, ptr = m.reg(0), m.reg(1)
    ctr = rt.state.setdefault("gl_ids", {})
    base = ctr.get("buf", 1)
    for i in range(n):
        m.wr32(ptr + 4 * i, base + i)
    ctr["buf"] = base + n
    return 0


@func("_glGenFramebuffersOES|_glGenRenderbuffersOES")
def _gl_gen_fb_rb(rt):
    m = rt.m
    n, ptr = m.reg(0), m.reg(1)
    ctr = rt.state.setdefault("gl_ids", {})
    base = ctr.get("fb", 1)
    for i in range(n):
        m.wr32(ptr + 4 * i, base + i)
    ctr["fb"] = base + n
    return 0


@func("_glBindFramebufferOES|_glBindRenderbufferOES|_glRenderbufferStorageOES|_glFramebufferRenderbufferOES")
def _gl_fb_ops(rt):
    return 0


@func("_glDeleteTextures|_glDeleteBuffers|_glDeleteFramebuffersOES|_glDeleteRenderbuffersOES|_glFlush|_glFinish")
def _gl_noops(rt):
    return 0
