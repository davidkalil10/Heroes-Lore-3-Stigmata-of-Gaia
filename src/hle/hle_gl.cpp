#include "../core/common.h"
#include "../core/machine.h"
#include "../core/objc.h"
#include "../core/runtime.h"
#include "gl_bridge.h"

#include <any>
#include <string>
#include <vector>

namespace hl3 {

namespace {

constexpr u32 kGlFramebufferCompleteOes = 0x8CD5u;
constexpr u32 kGlRenderbufferWidthOes = 0x8D42u;
constexpr u32 kGlRenderbufferHeightOes = 0x8D43u;
constexpr u32 kGlMaxTextureSize = 0x0D33u;
constexpr u32 kGlMaxTextureUnits = 0x84E2u;
constexpr u32 kGlViewport = 0x0BA2u;

struct GlIdState {
    u32 tex = 1;
    u32 buf = 1;
    u32 fb = 1;
};

GlIdState& get_gl_ids(Runtime& rt) {
    auto it = rt.kv_state.find("gl_ids");
    if (it == rt.kv_state.end()) {
        it = rt.kv_state.emplace("gl_ids", GlIdState{}).first;
    }
    return *std::any_cast<GlIdState>(&it->second);
}

u32 alloc_gl_string(Runtime& rt, u32 key, const std::string& val) {
    auto it = rt.kv_state.find("gl_strings");
    if (it == rt.kv_state.end()) {
        it = rt.kv_state.emplace("gl_strings", std::unordered_map<u32, u32>{}).first;
    }
    auto& tbl = *std::any_cast<std::unordered_map<u32, u32>>(&it->second);
    if (auto sit = tbl.find(key); sit != tbl.end()) return sit->second;
    u32 addr = rt.machine().new_cstr(val);
    tbl[key] = addr;
    return addr;
}

}  // namespace

void register_hle_gl(Runtime& rt, HleRegistry& reg) {
    reg.add_func("_glGenTextures", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        if (auto* gl = rt.gl_bridge()) {
            auto ids = gl->gen_textures(static_cast<int>(n));
            for (size_t i = 0; i < ids.size(); ++i) {
                m.wr32(ptr + 4u * static_cast<u32>(i), ids[i]);
            }
        } else {
            auto& ids = get_gl_ids(rt);
            for (u32 i = 0; i < n; ++i) {
                m.wr32(ptr + 4u * i, ids.tex + i);
            }
            ids.tex += n;
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glBindTexture", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->bind_texture(m.reg(0), m.reg(1));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glTexImage2D", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            u32 target = m.reg(0);
            int level = static_cast<int>(m.reg(1));
            u32 ifmt = m.reg(2);
            int w = static_cast<int>(m.reg(3));
            u32 sp = m.sp();
            int h = static_cast<int>(m.rd32(sp + 0));
            int border = static_cast<int>(m.rd32(sp + 4));
            u32 fmt = m.rd32(sp + 8);
            u32 tp = m.rd32(sp + 12);
            u32 pix = m.rd32(sp + 16);
            gl->tex_image_2d(target, level, ifmt, w, h, border, fmt, tp, pix);
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glTexParameteri", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->tex_parameter_i(m.reg(0), m.reg(1), static_cast<i32>(m.reg(2)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glTexParameterf", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->tex_parameter_f(m.reg(0), m.reg(1), bits_to_f32(m.reg(2)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glTexEnvx|_glTexEnvi", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->tex_env_i(m.reg(0), m.reg(1), static_cast<i32>(m.reg(2)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glPixelStorei", [](Machine&) { return TrapResult::val(0u); });

    reg.add_func("_glMatrixMode", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->matrix_mode(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_glLoadIdentity", [&rt](Machine&) {
        if (auto* gl = rt.gl_bridge()) gl->load_identity();
        return TrapResult::val(0u);
    });

    reg.add_func("_glPushMatrix", [&rt](Machine&) {
        if (auto* gl = rt.gl_bridge()) gl->push_matrix();
        return TrapResult::val(0u);
    });

    reg.add_func("_glPopMatrix", [&rt](Machine&) {
        if (auto* gl = rt.gl_bridge()) gl->pop_matrix();
        return TrapResult::val(0u);
    });

    reg.add_func("_glOrthof", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            float l = bits_to_f32(m.reg(0));
            float r = bits_to_f32(m.reg(1));
            float b = bits_to_f32(m.reg(2));
            float t = bits_to_f32(m.reg(3));
            float n = bits_to_f32(m.rd32(m.sp() + 0));
            float f = bits_to_f32(m.rd32(m.sp() + 4));
            gl->ortho_f(l, r, b, t, n, f);
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glEnable", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->enable(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_glDisable", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->disable(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_glEnableClientState", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->enable_client_state(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_glDisableClientState", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->disable_client_state(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_glBlendFunc", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->blend_func(m.reg(0), m.reg(1));
        return TrapResult::val(0u);
    });

    reg.add_func("_glAlphaFunc", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->alpha_func(m.reg(0), bits_to_f32(m.reg(1)));
        return TrapResult::val(0u);
    });

    reg.add_func("_glClearColor", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->clear_color(bits_to_f32(m.reg(0)), bits_to_f32(m.reg(1)),
                            bits_to_f32(m.reg(2)), bits_to_f32(m.reg(3)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glClear", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) gl->clear(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_glScissor", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->scissor(static_cast<i32>(m.reg(0)), static_cast<i32>(m.reg(1)),
                        static_cast<i32>(m.reg(2)), static_cast<i32>(m.reg(3)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glViewport", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->viewport(static_cast<i32>(m.reg(0)), static_cast<i32>(m.reg(1)),
                         static_cast<i32>(m.reg(2)), static_cast<i32>(m.reg(3)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glVertexPointer", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->vertex_pointer(static_cast<i32>(m.reg(0)), m.reg(1),
                               static_cast<i32>(m.reg(2)), m.reg(3));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glTexCoordPointer", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->tex_coord_pointer(static_cast<i32>(m.reg(0)), m.reg(1),
                                  static_cast<i32>(m.reg(2)), m.reg(3));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glColorPointer", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->color_pointer(static_cast<i32>(m.reg(0)), m.reg(1),
                              static_cast<i32>(m.reg(2)), m.reg(3));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glDrawArrays", [&rt](Machine& m) {
        if (auto* gl = rt.gl_bridge()) {
            gl->draw_arrays(m.reg(0), static_cast<i32>(m.reg(1)), static_cast<i32>(m.reg(2)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glDrawElements|_glGetError|_glIsEnabled", [](Machine&) {
        return TrapResult::val(0u);
    });

    reg.add_func("_glCheckFramebufferStatusOES", [](Machine&) {
        return TrapResult::val(kGlFramebufferCompleteOes);
    });

    reg.add_func("_glGetRenderbufferParameterivOES", [](Machine& m) {
        u32 pname = m.reg(1);
        u32 ptr = m.reg(2);
        if (ptr) m.wr32(ptr, (pname == kGlRenderbufferWidthOes) ? 480u : 320u);
        return TrapResult::val(0u);
    });

    reg.add_func("_glGetIntegerv", [](Machine& m) {
        u32 pname = m.reg(0);
        u32 ptr = m.reg(1);
        if (!ptr) return TrapResult::val(0u);
        if (pname == kGlMaxTextureSize) {
            m.wr32(ptr, 1024u);
        } else if (pname == kGlMaxTextureUnits) {
            m.wr32(ptr, 2u);
        } else if (pname == kGlViewport) {
            m.wr32(ptr + 0, 0u);
            m.wr32(ptr + 4, 0u);
            m.wr32(ptr + 8, 480u);
            m.wr32(ptr + 12, 320u);
        } else {
            m.wr32(ptr, 0u);
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glGetString", [&rt](Machine& m) {
        u32 name = m.reg(0);
        std::string val;
        if (name == 0x1F00u) val = "Apple Inc.";
        else if (name == 0x1F01u) val = "Apple Software Renderer";
        else if (name == 0x1F02u) val = "OpenGL ES-CM 1.1";
        else if (name == 0x1F03u) val = "GL_OES_framebuffer_object";
        return TrapResult::val(alloc_gl_string(rt, name, val));
    });

    reg.add_func("_glGenBuffers", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        auto& ids = get_gl_ids(rt);
        for (u32 i = 0; i < n; ++i) m.wr32(ptr + 4u * i, ids.buf + i);
        ids.buf += n;
        return TrapResult::val(0u);
    });

    reg.add_func("_glGenFramebuffersOES|_glGenRenderbuffersOES", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        auto& ids = get_gl_ids(rt);
        for (u32 i = 0; i < n; ++i) m.wr32(ptr + 4u * i, ids.fb + i);
        ids.fb += n;
        return TrapResult::val(0u);
    });

    reg.add_func("_glBindFramebufferOES|_glBindRenderbufferOES|_glRenderbufferStorageOES|_glFramebufferRenderbufferOES",
                 [](Machine&) { return TrapResult::val(0u); });

    reg.add_func("_glDeleteTextures", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        if (auto* gl = rt.gl_bridge(); gl && ptr && n > 0) {
            std::vector<u32> ids(n);
            for (u32 i = 0; i < n; ++i) ids[i] = m.rd32(ptr + 4u * i);
            gl->delete_textures(ids);
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_glDeleteBuffers|_glDeleteFramebuffersOES|_glDeleteRenderbuffersOES|_glFlush|_glFinish",
                 [](Machine&) { return TrapResult::val(0u); });
}

}  // namespace hl3
