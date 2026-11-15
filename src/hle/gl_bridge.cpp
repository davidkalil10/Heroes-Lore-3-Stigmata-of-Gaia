#include "gl_bridge.h"

#include "../core/machine.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>
#include <unordered_set>

namespace hl3 {

namespace {

// Guest addresses & patch sites for Heroes Lore 3 (iOS 1.0.1 Mach-O ARMv6).
constexpr u32 kAppManagerPtr = 0x000D7DD4u;
constexpr u32 kGameCtxAddr = 0x000D8270u;

constexpr u32 kUiRootDrawAddr = 0x00074DE0u;
constexpr u32 kUiRootDrawFormAddr = 0x00074E1Cu;
constexpr u32 kUiRootDrawDimmerAddr = 0x00074E54u;
constexpr u32 kUiRootDrawPopupAddr = 0x00074E8Cu;
constexpr u32 kUiRootDrawExitAddr = 0x00074F00u;
constexpr u32 kUiMainDrawRet1 = 0x00051F90u;
constexpr u32 kUiMainDrawRet2 = 0x00052140u;
constexpr u32 kUiGfxFlushAddr = 0x0000C688u;
constexpr u32 kUiRootTouchAddr = 0x0007575Cu;

constexpr u32 kOrthoHPoolAddr = 0x0000CBC0u;
constexpr u32 kOrthoWPoolAddr = 0x0000CBC4u;

constexpr u32 kWidthImmSites[] = {
    0x0000CE04u, 0x0000CE0Cu, 0x0000CE6Cu, 0x0000CE94u,
    0x0001878Cu, 0x00019714u, 0x00019A24u, 0x0003985Cu,
    0x0004F8D0u, 0x00065C9Cu, 0x00065CF0u, 0x00074E10u, 0x00074E68u,
};

constexpr u32 kWidthMovR3Sites[] = {
    0x00039940u, 0x00039A14u, 0x00051CA8u, 0x00051E78u, 0x0006D808u,
};

constexpr u32 kHeightImmSites[] = {
    0x0000CDD8u, 0x0000CE10u, 0x0000CE18u, 0x0000CE70u, 0x0000CE9Cu,
    0x0001971Cu, 0x00039868u, 0x00039930u, 0x00039A04u,
    0x0004F8D8u, 0x00051C94u, 0x00051E68u, 0x00065C70u, 0x00065CC0u,
    0x0006D7F8u, 0x00074E08u,
};

constexpr u32 kHeightMovR3Sites[] = {
    0x00018794u, 0x00019A2Cu,
};

struct CodeFix {
    u32 addr;
    u32 word;
};

constexpr CodeFix kUiDimmerOriginFixes[] = {
    {0x00074E60u, 0xE3A01000u},
    {0x00074E64u, 0xE3A02000u},
    {0x00074E70u, 0xE3A02000u},
    {0x00074E74u, 0xE3A01000u},
};

// OpenGL numeric constants (portable without fixed-function headers).
constexpr u32 kGlDepthBufferBit = 0x00000100u;
constexpr u32 kGlColorBufferBit = 0x00004000u;

constexpr u32 kGlLines = 0x0001u;
constexpr u32 kGlLineLoop = 0x0002u;
constexpr u32 kGlLineStrip = 0x0003u;
constexpr u32 kGlTriangles = 0x0004u;
constexpr u32 kGlTriangleStrip = 0x0005u;
constexpr u32 kGlTriangleFan = 0x0006u;

constexpr u32 kGlZero = 0u;
constexpr u32 kGlOne = 1u;
constexpr u32 kGlAlways = 0x0207u;

constexpr u32 kGlAlphaTest = 0x0BC0u;
constexpr u32 kGlBlend = 0x0BE2u;
constexpr u32 kGlScissorTest = 0x0C11u;
constexpr u32 kGlTexture2D = 0x0DE1u;
constexpr u32 kGlDepthTest = 0x0B71u;
constexpr u32 kGlCullFace = 0x0B44u;
constexpr u32 kGlDither = 0x0BD0u;

constexpr u32 kGlByte = 0x1400u;
constexpr u32 kGlUnsignedByte = 0x1401u;
constexpr u32 kGlShort = 0x1402u;
constexpr u32 kGlUnsignedShort = 0x1403u;
constexpr u32 kGlFloat = 0x1406u;
constexpr u32 kGlFixed = 0x140Cu;

constexpr u32 kGlAlpha = 0x1906u;
constexpr u32 kGlRgb = 0x1907u;
constexpr u32 kGlRgba = 0x1908u;
constexpr u32 kGlLuminance = 0x1909u;
constexpr u32 kGlLuminanceAlpha = 0x190Au;

constexpr u32 kGlUnsignedShort4444 = 0x8033u;
constexpr u32 kGlUnsignedShort5551 = 0x8034u;
constexpr u32 kGlUnsignedShort565 = 0x8363u;

constexpr u32 kGlModelview = 0x1700u;
constexpr u32 kGlProjection = 0x1701u;

constexpr u32 kGlVertexArray = 0x8074u;
constexpr u32 kGlColorArray = 0x8076u;
constexpr u32 kGlTextureCoordArray = 0x8078u;

constexpr u32 kGlNearest = 0x2600u;
constexpr u32 kGlLinear = 0x2601u;
constexpr u32 kGlTextureMagFilter = 0x2800u;
constexpr u32 kGlTextureMinFilter = 0x2801u;
constexpr u32 kGlTextureWrapS = 0x2802u;
constexpr u32 kGlTextureWrapT = 0x2803u;
constexpr u32 kGlClampToEdge = 0x812Fu;

constexpr u32 kGlTextureEnvMode = 0x2200u;
constexpr u32 kGlReplace = 0x1E01u;
constexpr u32 kGlModulate = 0x2100u;

constexpr u32 kGlVersion = 0x1F02u;
constexpr u32 kGlUnpackAlignment = 0x0CF5u;

constexpr u32 kGlTexture0 = 0x84C0u;
constexpr u32 kGlArrayBuffer = 0x8892u;
constexpr u32 kGlStreamDraw = 0x88E0u;
constexpr u32 kGlFragmentShader = 0x8B30u;
constexpr u32 kGlVertexShader = 0x8B31u;
constexpr u32 kGlCompileStatus = 0x8B81u;
constexpr u32 kGlLinkStatus = 0x8B82u;

int elem_size_bytes(u32 gl_type) {
    if (gl_type == kGlFloat || gl_type == kGlFixed) return 4;
    if (gl_type == kGlShort || gl_type == kGlUnsignedShort) return 2;
    return 1;
}

float read_coord(const u8* p, u32 gl_type) {
    switch (gl_type) {
        case kGlFloat: {
            float f = 0.0f;
            std::memcpy(&f, p, 4);
            return f;
        }
        case kGlFixed: {
            i32 v = read_s32_le(p);
            return static_cast<float>(v) / 65536.0f;
        }
        case kGlShort: return static_cast<float>(read_s16_le(p));
        case kGlUnsignedShort: return static_cast<float>(read_u16_le(p));
        case kGlByte: return static_cast<float>(static_cast<int8_t>(p[0]));
        case kGlUnsignedByte: return static_cast<float>(p[0]);
        default: return 0.0f;
    }
}

float read_color_comp(const u8* p, u32 gl_type) {
    switch (gl_type) {
        case kGlUnsignedByte: return static_cast<float>(p[0]) / 255.0f;
        case kGlByte: return std::max(0.0f, static_cast<float>(static_cast<int8_t>(p[0])) / 127.0f);
        case kGlFloat: {
            float f = 1.0f;
            std::memcpy(&f, p, 4);
            return f;
        }
        case kGlFixed: {
            i32 v = read_s32_le(p);
            return static_cast<float>(v) / 65536.0f;
        }
        default: return 1.0f;
    }
}

bool decode_to_rgba8(int w, int h, u32 fmt, u32 tp, const Bytes& src, Bytes& dst) {
    size_t px_count = static_cast<size_t>(w) * static_cast<size_t>(h);
    dst.resize(px_count * 4);

    if (tp == kGlUnsignedShort4444 && fmt == kGlRgba) {
        if (src.size() < px_count * 2) return false;
        for (size_t i = 0; i < px_count; ++i) {
            u16 v = read_u16_le(src.data() + i * 2);
            dst[i * 4 + 0] = static_cast<u8>(((v >> 12) & 0xFu) * 17u);
            dst[i * 4 + 1] = static_cast<u8>(((v >> 8) & 0xFu) * 17u);
            dst[i * 4 + 2] = static_cast<u8>(((v >> 4) & 0xFu) * 17u);
            dst[i * 4 + 3] = static_cast<u8>((v & 0xFu) * 17u);
        }
        return true;
    }
    if (tp == kGlUnsignedShort5551 && fmt == kGlRgba) {
        if (src.size() < px_count * 2) return false;
        for (size_t i = 0; i < px_count; ++i) {
            u16 v = read_u16_le(src.data() + i * 2);
            dst[i * 4 + 0] = static_cast<u8>((((v >> 11) & 0x1Fu) * 255u) / 31u);
            dst[i * 4 + 1] = static_cast<u8>((((v >> 6) & 0x1Fu) * 255u) / 31u);
            dst[i * 4 + 2] = static_cast<u8>((((v >> 1) & 0x1Fu) * 255u) / 31u);
            dst[i * 4 + 3] = (v & 1u) ? 255u : 0u;
        }
        return true;
    }
    if (tp == kGlUnsignedShort565 && fmt == kGlRgb) {
        if (src.size() < px_count * 2) return false;
        for (size_t i = 0; i < px_count; ++i) {
            u16 v = read_u16_le(src.data() + i * 2);
            dst[i * 4 + 0] = static_cast<u8>((((v >> 11) & 0x1Fu) * 255u) / 31u);
            dst[i * 4 + 1] = static_cast<u8>((((v >> 5) & 0x3Fu) * 255u) / 63u);
            dst[i * 4 + 2] = static_cast<u8>(((v & 0x1Fu) * 255u) / 31u);
            dst[i * 4 + 3] = 255u;
        }
        return true;
    }
    if (fmt == kGlRgb) {
        if (src.size() < px_count * 3) return false;
        for (size_t i = 0; i < px_count; ++i) {
            dst[i * 4 + 0] = src[i * 3 + 0];
            dst[i * 4 + 1] = src[i * 3 + 1];
            dst[i * 4 + 2] = src[i * 3 + 2];
            dst[i * 4 + 3] = 255u;
        }
        return true;
    }
    if (fmt == kGlLuminanceAlpha) {
        if (src.size() < px_count * 2) return false;
        for (size_t i = 0; i < px_count; ++i) {
            u8 l = src[i * 2 + 0];
            dst[i * 4 + 0] = l;
            dst[i * 4 + 1] = l;
            dst[i * 4 + 2] = l;
            dst[i * 4 + 3] = src[i * 2 + 1];
        }
        return true;
    }
    if (fmt == kGlLuminance) {
        if (src.size() < px_count) return false;
        for (size_t i = 0; i < px_count; ++i) {
            u8 l = src[i];
            dst[i * 4 + 0] = l;
            dst[i * 4 + 1] = l;
            dst[i * 4 + 2] = l;
            dst[i * 4 + 3] = 255u;
        }
        return true;
    }
    if (fmt == kGlAlpha) {
        if (src.size() < px_count) return false;
        for (size_t i = 0; i < px_count; ++i) {
            dst[i * 4 + 0] = 255u;
            dst[i * 4 + 1] = 255u;
            dst[i * 4 + 2] = 255u;
            dst[i * 4 + 3] = src[i];
        }
        return true;
    }
    if (fmt == kGlRgba && src.size() >= px_count * 4) {
        std::memcpy(dst.data(), src.data(), px_count * 4);
        return true;
    }
    return false;
}

#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

}  // namespace

// -----------------------------------------------------------------------------
// Mat4
// -----------------------------------------------------------------------------
Mat4 Mat4::identity() {
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::ortho(float l, float r, float b, float t, float n, float f) {
    Mat4 o{};
    float rl = (r - l) != 0.0f ? (r - l) : 1.0f;
    float tb = (t - b) != 0.0f ? (t - b) : 1.0f;
    float fn = (f - n) != 0.0f ? (f - n) : 1.0f;
    o.m[0] = 2.0f / rl;
    o.m[5] = 2.0f / tb;
    o.m[10] = -2.0f / fn;
    o.m[12] = -(r + l) / rl;
    o.m[13] = -(t + b) / tb;
    o.m[14] = -(f + n) / fn;
    o.m[15] = 1.0f;
    return o;
}

Mat4 Mat4::mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

// -----------------------------------------------------------------------------
// WidescreenController
// -----------------------------------------------------------------------------
u32 WidescreenController::encode_arm_rot_imm(u32 val) {
    for (u32 rot4 = 0; rot4 < 16; ++rot4) {
        u32 rot = rot4 * 2u;
        u32 imm8 = rot ? ((val << rot) | (val >> (32u - rot))) : val;
        if (imm8 <= 0xFFu) {
            return (rot4 << 8) | imm8;
        }
    }
    return 0;
}

WidescreenController::WidescreenController(Machine& m, GLBridge* gl_bridge)
    : m_(m), gl_bridge_(gl_bridge) {}

void WidescreenController::reset_frame_state() {
    in_ui_main_draw_ = false;
    pending_flush_.active = false;
}

void WidescreenController::install_hooks() {
    if (hooks_installed_) return;
    hooks_installed_ = true;

    m_.wr32(kUiFlushReturnStub, 0xE1A00000u);  // ARM nop return target
    m_.add_code_hook(kUiFlushReturnStub, kUiFlushReturnStub, [this](Machine&, u32, u32) {
        if (!pending_flush_.active) return;
        PendingFlush pf = pending_flush_;
        pending_flush_.active = false;
        m_.set_reg(0, pf.regs[0]);
        m_.set_reg(1, pf.regs[1]);
        m_.set_reg(2, pf.regs[2]);
        m_.set_reg(3, pf.regs[3]);
        m_.set_reg(12, pf.regs[4]);
        m_.set_sp(pf.regs[5]);
        m_.set_lr(pf.regs[6]);
        m_.set_cpsr(pf.regs[7]);
        set_gl_ui_offset(pf.ox, pf.oy);
        m_.set_pc(pf.resume_pc);
    });

    m_.add_code_hook(kUiRootDrawAddr, kUiRootDrawAddr, [this](Machine& m, u32, u32) {
        in_ui_main_draw_ = false;
        if (ui_dx_ == 0 && ui_dy_ == 0) return;
        u32 app = m.rd32(kAppManagerPtr);
        if (!app || m.reg(0) != m.rd32(app + 0x40u)) return;
        u32 state = m.rd32(app + 0x78u);
        if (state == 0 || state == 1 || state == 256) return;
        in_ui_main_draw_ = true;
    });

    auto enter_content = [this](Machine& m, u32 addr, u32) {
        if (in_ui_main_draw_) {
            flush_then_set_offset(addr, m.reg(5), ui_dx_, ui_dy_);
        }
    };
    m_.add_code_hook(kUiRootDrawFormAddr, kUiRootDrawFormAddr, enter_content);
    m_.add_code_hook(kUiRootDrawPopupAddr, kUiRootDrawPopupAddr, enter_content);

    m_.add_code_hook(kUiRootDrawDimmerAddr, kUiRootDrawDimmerAddr, [this](Machine& m, u32 addr, u32) {
        if (in_ui_main_draw_) {
            flush_then_set_offset(addr, m.reg(5), 0, 0);
        }
    });

    m_.add_code_hook(kUiRootDrawExitAddr, kUiRootDrawExitAddr, [this](Machine& m, u32 addr, u32) {
        if (in_ui_main_draw_) {
            in_ui_main_draw_ = false;
            flush_then_set_offset(addr, m.reg(5), 0, 0);
        }
    });

    auto exit_caller = [this](Machine& m, u32 addr, u32) {
        if (in_ui_main_draw_) {
            in_ui_main_draw_ = false;
            u32 gfx = m.rd32(m.reg(5) + 0x14u);
            flush_then_set_offset(addr, gfx, 0, 0);
        }
    };
    m_.add_code_hook(kUiMainDrawRet1, kUiMainDrawRet1, exit_caller);
    m_.add_code_hook(kUiMainDrawRet2, kUiMainDrawRet2, exit_caller);

    m_.add_code_hook(kUiRootTouchAddr, kUiRootTouchAddr, [this](Machine& m, u32, u32) {
        if (ui_dx_ == 0 && ui_dy_ == 0) return;
        u32 app = m.rd32(kAppManagerPtr);
        if (!app || m.reg(0) != m.rd32(app + 0x40u)) return;
        u32 state = m.rd32(app + 0x78u);
        if (state == 0 || state == 1 || state == 256) return;
        i32 r3 = static_cast<i32>(m.reg(3)) - ui_dx_;
        m.set_reg(3, static_cast<u32>(r3));
        u32 sp = m.sp();
        i32 sp0 = static_cast<i32>(m.rd32(sp)) - ui_dy_;
        m.wr32(sp, static_cast<u32>(sp0));
    });
}

void WidescreenController::set_gl_ui_offset(int ox, int oy) {
    if (gl_bridge_) gl_bridge_->set_ui_offset(ox, oy);
}

bool WidescreenController::flush_then_set_offset(u32 resume_pc, u32 gfx, int ox, int oy) {
    if (!gl_bridge_) return false;
    if (gl_bridge_->ui_offset() == std::make_pair(ox, oy)) return false;
    if (gfx < kHeapBase || gfx >= kHeapBase + kHeapSize) {
        set_gl_ui_offset(ox, oy);
        return false;
    }
    pending_flush_.active = true;
    pending_flush_.regs = {
        m_.reg(0), m_.reg(1), m_.reg(2), m_.reg(3),
        m_.reg(12), m_.sp(), m_.lr(), m_.cpsr(),
    };
    pending_flush_.resume_pc = resume_pc;
    pending_flush_.ox = ox;
    pending_flush_.oy = oy;

    m_.set_reg(0, gfx);
    m_.set_sp(pending_flush_.regs[5] & ~7u);
    m_.set_lr(kUiFlushReturnStub);
    m_.set_cpsr(pending_flush_.regs[7] & ~0x20u);
    m_.set_pc(kUiGfxFlushAddr);
    return true;
}

void WidescreenController::write_code32(u32 va, u32 word) {
    if (orig_words_.find(va) == orig_words_.end()) {
        orig_words_[va] = m_.rd32(va);
    }
    m_.wr32(va, word);
    uc_ctl_remove_cache(m_.uc(), va, va + 4);
    m_.invalidate_code_word(va);
}

void WidescreenController::patch_imm(u32 va, u32 val) {
    if (orig_words_.find(va) == orig_words_.end()) {
        orig_words_[va] = m_.rd32(va);
    }
    u32 orig = orig_words_[va];
    u32 enc = encode_arm_rot_imm(val);
    write_code32(va, (orig & 0xFFFFF000u) | enc);
}

void WidescreenController::patch_mov_rd(u32 va, u32 rd, u32 val) {
    if (orig_words_.find(va) == orig_words_.end()) {
        orig_words_[va] = m_.rd32(va);
    }
    u32 orig = orig_words_[va];
    u32 cond = orig & 0xF0000000u;
    u32 enc = encode_arm_rot_imm(val);
    write_code32(va, cond | 0x03A00000u | ((rd & 0xFu) << 12) | enc);
}

void WidescreenController::patch_tooltip_clamp(int patch_w) {
    u32 enc_w = encode_arm_rot_imm(static_cast<u32>(patch_w));
    patch_imm(0x0003B96Cu, static_cast<u32>(patch_w));
    write_code32(0x0003B970u, 0xC2813000u | enc_w);
    write_code32(0x0003B974u, 0xC2433001u);
}

void WidescreenController::apply_layout(const ViewportLayout& layout) {
    install_hooks();
    ui_dx_ = layout.ui_dx;
    ui_dy_ = layout.ui_dy;

    if (!has_last_dims_ || last_patch_w_ != layout.patch_w || last_patch_h_ != layout.patch_h ||
        last_ortho_w_ != layout.ortho_w || last_ortho_h_ != layout.ortho_h) {
        has_last_dims_ = true;
        last_patch_w_ = layout.patch_w;
        last_patch_h_ = layout.patch_h;
        last_ortho_w_ = layout.ortho_w;
        last_ortho_h_ = layout.ortho_h;

        u32 pw = static_cast<u32>(layout.patch_w);
        u32 ph = static_cast<u32>(layout.patch_h);
        if (!dimmer_fixed_) {
            dimmer_fixed_ = true;
            for (const auto& fix : kUiDimmerOriginFixes) {
                write_code32(fix.addr, fix.word);
            }
        }
        for (u32 va : kWidthImmSites) patch_imm(va, pw);
        for (u32 va : kWidthMovR3Sites) patch_mov_rd(va, 3, pw);
        for (u32 va : kHeightImmSites) patch_imm(va, ph);
        for (u32 va : kHeightMovR3Sites) patch_mov_rd(va, 3, ph);
        patch_tooltip_clamp(layout.patch_w);

        u8 fbuf[4];
        write_u32_le(fbuf, f32_to_bits(layout.ortho_h));
        m_.wrbytes(kOrthoHPoolAddr, fbuf, 4);
        write_u32_le(fbuf, f32_to_bits(layout.ortho_w));
        m_.wrbytes(kOrthoWPoolAddr, fbuf, 4);
    }

    sync_live_structs(layout);
    sync_hud_nodes(layout);
}

void WidescreenController::sync_live_structs(const ViewportLayout& layout) {
    u32 ctx = kGameCtxAddr;
    u32 pw = static_cast<u32>(layout.patch_w);
    u32 ph = static_cast<u32>(layout.patch_h);
    u32 cur_w = m_.rd32(ctx + 0x40u);
    u32 cur_h = m_.rd32(ctx + 0x44u);
    if (cur_w == 0 && cur_h == 0) return;
    if (cur_w != pw || cur_h != ph) {
        m_.wr32(ctx + 0x40u, pw);
        m_.wr32(ctx + 0x44u, ph);
    }
    u32 cam_base = ctx + 0x5000u;
    u32 map_ptr = m_.rd32(cam_base + 0xC88u);
    if (map_ptr >= kHeapBase && map_ptr < kHeapBase + kHeapSize) {
        u32 vh_map = std::max<u32>(1u, ph > 33u ? (ph - 33u) : 1u);
        u32 cam_cx = (pw > 24u) ? ((pw - 24u) / 2u) : 0u;
        u32 cam_cy = vh_map / 2u;
        if (m_.rd32(cam_base + 0xE60u) != pw || m_.rd32(cam_base + 0xE64u) != vh_map) {
            m_.wr32(cam_base + 0xE60u, pw);
            m_.wr32(cam_base + 0xE64u, vh_map);
            m_.wr32(cam_base + 0xE78u, cam_cx);
            m_.wr32(cam_base + 0xE7Cu, cam_cy);
        }
    }
}

void WidescreenController::sync_hud_nodes(const ViewportLayout& layout) {
    u32 app = m_.rd32(kAppManagerPtr);
    if (app < kHeapBase || app >= kHeapBase + kHeapSize) return;
    u32 ui_hud = m_.rd32(app + 0x44u);
    if (ui_hud < kHeapBase || ui_hud >= kHeapBase + kHeapSize) return;
    u32 form_ptr = m_.rd32(ui_hud + 0x98u);
    if (form_ptr < kHeapBase || form_ptr >= kHeapBase + kHeapSize) return;
    if (m_.rd16(form_ptr + 0x14u) != 70u) return;

    if (hud_form_ptr_ != form_ptr) {
        hud_form_ptr_ = form_ptr;
        hud_nodes_.clear();
    }
    walk_and_anchor_hud(m_.rd32(form_ptr + 0x38u), layout);
}

void WidescreenController::walk_and_anchor_hud(u32 head_ptr, const ViewportLayout& layout) {
    std::unordered_set<u32> seen;
    std::vector<u32> stack{head_ptr};
    while (!stack.empty()) {
        u32 ptr = stack.back();
        stack.pop_back();
        if (ptr < kHeapBase || ptr >= kHeapBase + kHeapSize || seen.count(ptr)) continue;
        seen.insert(ptr);

        u8 raw_rc[8];
        u8 raw_fr[16];
        if (!m_.rdbytes(ptr + 0x08u, 8, raw_rc) || !m_.rdbytes(ptr + 0x18u, 16, raw_fr)) continue;

        i32 px = read_s16_le(raw_rc + 0);
        i32 py = read_s16_le(raw_rc + 2);
        i32 pw = read_s16_le(raw_rc + 4);
        i32 ph = read_s16_le(raw_rc + 6);
        float fx0 = bits_to_f32(read_u32_le(raw_fr + 0));
        float fx1 = bits_to_f32(read_u32_le(raw_fr + 4));
        float fy0 = bits_to_f32(read_u32_le(raw_fr + 8));
        float fy1 = bits_to_f32(read_u32_le(raw_fr + 12));

        auto it = hud_nodes_.find(ptr);
        if (it == hud_nodes_.end()) {
            float bcx = ((fx1 - fx0) >= 2.0f) ? ((fx0 + fx1) * 0.5f) : (static_cast<float>(px) + pw * 0.5f);
            float bcy = ((fy1 - fy0) >= 2.0f) ? ((fy0 + fy1) * 0.5f) : (static_cast<float>(py) + ph * 0.5f);
            it = hud_nodes_.emplace(ptr, HudNodeState{px, py, bcx, bcy, 0, 0}).first;
        }

        if (pw >= 350 || (fx1 - fx0) >= 350.0f) {
            u32 nxt = m_.rd32(ptr + 0x30u);
            u32 cld = m_.rd32(ptr + 0x38u);
            if (nxt) stack.push_back(nxt);
            if (cld) stack.push_back(cld);
            continue;
        }

        HudNodeState& st = it->second;
        int dx = 0;
        if (st.bcx < 150.0f) dx = 0;
        else if (st.bcx <= 330.0f) dx = layout.hud_dx_center;
        else dx = layout.hud_dx_right;

        int dy = 0;
        if (st.bcy < 100.0f) dy = 0;
        else if (st.bcy < 180.0f) dy = layout.hud_dy_bottom / 2;
        else dy = layout.hud_dy_bottom;

        if (st.cur_dx != 0 && px == st.orig_x && py == st.orig_y) {
            st.cur_dx = 0;
            st.cur_dy = 0;
        }
        int ddx = dx - st.cur_dx;
        int ddy = dy - st.cur_dy;
        if (ddx != 0 || ddy != 0) {
            u8 out_rc[4];
            write_u16_le(out_rc + 0, static_cast<u16>(static_cast<i16>(px + ddx)));
            write_u16_le(out_rc + 2, static_cast<u16>(static_cast<i16>(py + ddy)));
            m_.wrbytes(ptr + 0x08u, out_rc, 4);
            if ((fx1 - fx0) >= 1.0f || (fy1 - fy0) >= 1.0f) {
                u8 out_fr[16];
                write_u32_le(out_fr + 0, f32_to_bits(fx0 + ddx));
                write_u32_le(out_fr + 4, f32_to_bits(fx1 + ddx));
                write_u32_le(out_fr + 8, f32_to_bits(fy0 + ddy));
                write_u32_le(out_fr + 12, f32_to_bits(fy1 + ddy));
                m_.wrbytes(ptr + 0x18u, out_fr, 16);
            }
            st.cur_dx = dx;
            st.cur_dy = dy;
        }

        u32 nxt = m_.rd32(ptr + 0x30u);
        u32 cld = m_.rd32(ptr + 0x38u);
        if (nxt) stack.push_back(nxt);
        if (cld) stack.push_back(cld);
    }
}

// -----------------------------------------------------------------------------
// GLBridge OpenGL function table (works on Desktop GL 2.1+ and OpenGL ES 2.0+)
// -----------------------------------------------------------------------------
struct GLBridge::GlImpl {
    using PfnGenTextures = void(APIENTRY*)(int, u32*);
    using PfnDeleteTextures = void(APIENTRY*)(int, const u32*);
    using PfnBindTexture = void(APIENTRY*)(u32, u32);
    using PfnTexImage2D = void(APIENTRY*)(u32, int, int, int, int, int, u32, u32, const void*);
    using PfnTexSubImage2D = void(APIENTRY*)(u32, int, int, int, int, int, u32, u32, const void*);
    using PfnCopyTexSubImage2D = void(APIENTRY*)(u32, int, int, int, int, int, int, int);
    using PfnTexParameteri = void(APIENTRY*)(u32, u32, int);
    using PfnTexParameterf = void(APIENTRY*)(u32, u32, float);
    using PfnPixelStorei = void(APIENTRY*)(u32, int);
    using PfnEnable = void(APIENTRY*)(u32);
    using PfnDisable = void(APIENTRY*)(u32);
    using PfnBlendFunc = void(APIENTRY*)(u32, u32);
    using PfnClearColor = void(APIENTRY*)(float, float, float, float);
    using PfnClear = void(APIENTRY*)(u32);
    using PfnViewport = void(APIENTRY*)(int, int, int, int);
    using PfnScissor = void(APIENTRY*)(int, int, int, int);
    using PfnDrawArrays = void(APIENTRY*)(u32, int, int);
    using PfnLineWidth = void(APIENTRY*)(float);
    using PfnReadPixels = void(APIENTRY*)(int, int, int, int, u32, u32, void*);
    using PfnGetString = const u8*(APIENTRY*)(u32);

    using PfnActiveTexture = void(APIENTRY*)(u32);
    using PfnCreateShader = u32(APIENTRY*)(u32);
    using PfnShaderSource = void(APIENTRY*)(u32, int, const char* const*, const int*);
    using PfnCompileShader = void(APIENTRY*)(u32);
    using PfnGetShaderiv = void(APIENTRY*)(u32, u32, int*);
    using PfnDeleteShader = void(APIENTRY*)(u32);
    using PfnCreateProgram = u32(APIENTRY*)();
    using PfnAttachShader = void(APIENTRY*)(u32, u32);
    using PfnLinkProgram = void(APIENTRY*)(u32);
    using PfnGetProgramiv = void(APIENTRY*)(u32, u32, int*);
    using PfnUseProgram = void(APIENTRY*)(u32);
    using PfnDeleteProgram = void(APIENTRY*)(u32);
    using PfnBindAttribLocation = void(APIENTRY*)(u32, u32, const char*);
    using PfnGetUniformLocation = int(APIENTRY*)(u32, const char*);
    using PfnUniform1i = void(APIENTRY*)(int, int);
    using PfnUniform1f = void(APIENTRY*)(int, float);
    using PfnUniformMatrix4fv = void(APIENTRY*)(int, int, u8, const float*);
    using PfnEnableVertexAttribArray = void(APIENTRY*)(u32);
    using PfnDisableVertexAttribArray = void(APIENTRY*)(u32);
    using PfnVertexAttribPointer = void(APIENTRY*)(u32, int, u32, u8, int, const void*);
    using PfnGenBuffers = void(APIENTRY*)(int, u32*);
    using PfnDeleteBuffers = void(APIENTRY*)(int, const u32*);
    using PfnBindBuffer = void(APIENTRY*)(u32, u32);
    using PfnBufferData = void(APIENTRY*)(u32, std::ptrdiff_t, const void*, u32);
    using PfnBufferSubData = void(APIENTRY*)(u32, std::ptrdiff_t, std::ptrdiff_t, const void*);

    PfnGenTextures gen_textures = nullptr;
    PfnDeleteTextures delete_textures = nullptr;
    PfnBindTexture bind_texture = nullptr;
    PfnTexImage2D tex_image_2d = nullptr;
    PfnTexSubImage2D tex_sub_image_2d = nullptr;
    PfnCopyTexSubImage2D copy_tex_sub_image_2d = nullptr;
    PfnTexParameteri tex_parameteri = nullptr;
    PfnTexParameterf tex_parameterf = nullptr;
    PfnPixelStorei pixel_storei = nullptr;
    PfnEnable enable = nullptr;
    PfnDisable disable = nullptr;
    PfnBlendFunc blend_func = nullptr;
    PfnClearColor clear_color = nullptr;
    PfnClear clear = nullptr;
    PfnViewport viewport = nullptr;
    PfnScissor scissor = nullptr;
    PfnDrawArrays draw_arrays = nullptr;
    PfnLineWidth line_width = nullptr;
    PfnReadPixels read_pixels = nullptr;
    PfnGetString get_string = nullptr;

    PfnActiveTexture active_texture = nullptr;
    PfnCreateShader create_shader = nullptr;
    PfnShaderSource shader_source = nullptr;
    PfnCompileShader compile_shader = nullptr;
    PfnGetShaderiv get_shaderiv = nullptr;
    PfnDeleteShader delete_shader = nullptr;
    PfnCreateProgram create_program = nullptr;
    PfnAttachShader attach_shader = nullptr;
    PfnLinkProgram link_program = nullptr;
    PfnGetProgramiv get_programiv = nullptr;
    PfnUseProgram use_program = nullptr;
    PfnDeleteProgram delete_program = nullptr;
    PfnBindAttribLocation bind_attrib_location = nullptr;
    PfnGetUniformLocation get_uniform_location = nullptr;
    PfnUniform1i uniform1i = nullptr;
    PfnUniform1f uniform1f = nullptr;
    PfnUniformMatrix4fv uniform_matrix4fv = nullptr;
    PfnEnableVertexAttribArray enable_vertex_attrib_array = nullptr;
    PfnDisableVertexAttribArray disable_vertex_attrib_array = nullptr;
    PfnVertexAttribPointer vertex_attrib_pointer = nullptr;
    PfnGenBuffers gen_buffers = nullptr;
    PfnDeleteBuffers delete_buffers = nullptr;
    PfnBindBuffer bind_buffer = nullptr;
    PfnBufferData buffer_data = nullptr;
    PfnBufferSubData buffer_sub_data = nullptr;

    u32 program = 0;
    u32 vbo = 0;
    size_t vbo_capacity_bytes = 0;
    u32 backdrop_tex = 0;
    int backdrop_w = 0;
    int backdrop_h = 0;
    int u_mvp = -1;
    int u_tex = -1;
    int u_use_tex = -1;
    int u_tex_mode = -1;
    int u_alpha_mode = -1;
    int u_alpha_ref = -1;

    // Cached GL pipeline & batch state to minimize driver overhead on mobile/Switch
    bool state_initialized = false;
    u32 gpu_bound_tex = 0;
    int cached_use_tex = -1;
    int cached_tex_mode = -1;
    int cached_alpha_mode = -99;
    float cached_alpha_ref = -1.0f;
    bool cached_has_mvp = false;
    std::array<float, 16> cached_mvp{};

    bool batch_active = false;
    bool batch_use_tex = false;
    u32 batch_tex_id = 0;
    int batch_tex_mode = 0;
    int batch_alpha_mode = -1;
    float batch_alpha_ref = 0.0f;
    std::array<float, 16> batch_mvp{};

    bool overlay_active = false;
    int overlay_tex_mode = -1;
    Mat4 overlay_proj = Mat4::identity();
    bool overlay_prev_blend = false;
    bool overlay_prev_alpha = false;
    bool overlay_prev_scissor = false;

    template <typename Fn>
    static bool load(Fn& dst, const char* name) {
        dst = reinterpret_cast<Fn>(SDL_GL_GetProcAddress(name));
        return dst != nullptr;
    }

    bool load_all() {
        bool ok = true;
        ok &= load(gen_textures, "glGenTextures");
        ok &= load(delete_textures, "glDeleteTextures");
        ok &= load(bind_texture, "glBindTexture");
        ok &= load(tex_image_2d, "glTexImage2D");
        ok &= load(tex_sub_image_2d, "glTexSubImage2D");
        load(copy_tex_sub_image_2d, "glCopyTexSubImage2D");
        ok &= load(tex_parameteri, "glTexParameteri");
        load(tex_parameterf, "glTexParameterf");
        ok &= load(pixel_storei, "glPixelStorei");
        ok &= load(enable, "glEnable");
        ok &= load(disable, "glDisable");
        ok &= load(blend_func, "glBlendFunc");
        ok &= load(clear_color, "glClearColor");
        ok &= load(clear, "glClear");
        ok &= load(viewport, "glViewport");
        ok &= load(scissor, "glScissor");
        ok &= load(draw_arrays, "glDrawArrays");
        load(line_width, "glLineWidth");
        ok &= load(read_pixels, "glReadPixels");
        ok &= load(get_string, "glGetString");

        load(active_texture, "glActiveTexture");
        ok &= load(create_shader, "glCreateShader");
        ok &= load(shader_source, "glShaderSource");
        ok &= load(compile_shader, "glCompileShader");
        ok &= load(get_shaderiv, "glGetShaderiv");
        ok &= load(delete_shader, "glDeleteShader");
        ok &= load(create_program, "glCreateProgram");
        ok &= load(attach_shader, "glAttachShader");
        ok &= load(link_program, "glLinkProgram");
        ok &= load(get_programiv, "glGetProgramiv");
        ok &= load(use_program, "glUseProgram");
        ok &= load(delete_program, "glDeleteProgram");
        ok &= load(bind_attrib_location, "glBindAttribLocation");
        ok &= load(get_uniform_location, "glGetUniformLocation");
        ok &= load(uniform1i, "glUniform1i");
        ok &= load(uniform1f, "glUniform1f");
        ok &= load(uniform_matrix4fv, "glUniformMatrix4fv");
        ok &= load(enable_vertex_attrib_array, "glEnableVertexAttribArray");
        ok &= load(disable_vertex_attrib_array, "glDisableVertexAttribArray");
        ok &= load(vertex_attrib_pointer, "glVertexAttribPointer");
        ok &= load(gen_buffers, "glGenBuffers");
        ok &= load(delete_buffers, "glDeleteBuffers");
        ok &= load(bind_buffer, "glBindBuffer");
        ok &= load(buffer_data, "glBufferData");
        load(buffer_sub_data, "glBufferSubData");
        return ok;
    }

    u32 compile(u32 type, const char* src) {
        u32 sh = create_shader(type);
        shader_source(sh, 1, &src, nullptr);
        compile_shader(sh);
        int status = 0;
        get_shaderiv(sh, kGlCompileStatus, &status);
        if (!status) {
            delete_shader(sh);
            return 0;
        }
        return sh;
    }
};

GLBridge::GLBridge(Machine* m, int win_w, int win_h, const std::string& aspect_mode)
    : impl_(std::make_unique<GlImpl>()),
      m_(m),
      win_w_(std::max(1, win_w)),
      win_h_(std::max(1, win_h)),
      aspect_name_(normalize_aspect_mode(aspect_mode)) {
    mv_stack_.push_back(Mat4::identity());
    proj_stack_.push_back(Mat4::identity());
    if (m_) {
        widescreen_ = std::make_unique<WidescreenController>(*m_, this);
    }
    update_viewport(win_w_, win_h_);
}

GLBridge::~GLBridge() {
    shutdown_gl();
}

bool GLBridge::init_gl() {
    if (gl_ready_) return true;
    if (!impl_->load_all()) return false;

    const u8* ver_raw = impl_->get_string(kGlVersion);
    std::string ver = ver_raw ? reinterpret_cast<const char*>(ver_raw) : "";
    bool is_gles = (ver.find("OpenGL ES") != std::string::npos);

    const char* vs_src = is_gles ?
        "precision highp float;\n"
        "attribute vec3 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "attribute vec4 a_col;\n"
        "uniform mat4 u_mvp;\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "void main() {\n"
        "  v_uv = a_uv;\n"
        "  v_col = a_col;\n"
        "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "}\n"
        :
        "#version 110\n"
        "attribute vec3 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "attribute vec4 a_col;\n"
        "uniform mat4 u_mvp;\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "void main() {\n"
        "  v_uv = a_uv;\n"
        "  v_col = a_col;\n"
        "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "}\n";

    const char* fs_src = is_gles ?
        "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
        "precision highp float;\n"
        "#else\n"
        "precision mediump float;\n"
        "#endif\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "uniform sampler2D u_tex;\n"
        "uniform int u_use_tex;\n"
        "uniform int u_tex_mode;\n"
        "uniform int u_alpha_mode;\n"
        "uniform float u_alpha_ref;\n"
        "void main() {\n"
        "  vec4 c = v_col;\n"
        "  if (u_use_tex != 0) {\n"
        "    vec4 t = texture2D(u_tex, v_uv);\n"
        "    c = (u_tex_mode == 2) ? vec4(t.rgb * v_col.rgb, v_col.a) : ((u_tex_mode == 1) ? t : (t * v_col));\n"
        "  }\n"
        "  if (u_alpha_mode >= 0) {\n"
        "    if (u_alpha_mode == 0) discard;\n"
        "    else if (u_alpha_mode == 1 && !(c.a < u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 2 && !(c.a == u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 3 && !(c.a <= u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 4 && !(c.a > u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 5 && !(c.a != u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 6 && !(c.a >= u_alpha_ref)) discard;\n"
        "  }\n"
        "  gl_FragColor = c;\n"
        "}\n"
        :
        "#version 110\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "uniform sampler2D u_tex;\n"
        "uniform int u_use_tex;\n"
        "uniform int u_tex_mode;\n"
        "uniform int u_alpha_mode;\n"
        "uniform float u_alpha_ref;\n"
        "void main() {\n"
        "  vec4 c = v_col;\n"
        "  if (u_use_tex != 0) {\n"
        "    vec4 t = texture2D(u_tex, v_uv);\n"
        "    c = (u_tex_mode == 2) ? vec4(t.rgb * v_col.rgb, v_col.a) : ((u_tex_mode == 1) ? t : (t * v_col));\n"
        "  }\n"
        "  if (u_alpha_mode >= 0) {\n"
        "    if (u_alpha_mode == 0) discard;\n"
        "    else if (u_alpha_mode == 1 && !(c.a < u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 2 && !(c.a == u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 3 && !(c.a <= u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 4 && !(c.a > u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 5 && !(c.a != u_alpha_ref)) discard;\n"
        "    else if (u_alpha_mode == 6 && !(c.a >= u_alpha_ref)) discard;\n"
        "  }\n"
        "  gl_FragColor = c;\n"
        "}\n";

    u32 vs = impl_->compile(kGlVertexShader, vs_src);
    u32 fs = impl_->compile(kGlFragmentShader, fs_src);
    if (!vs || !fs) {
        if (vs) impl_->delete_shader(vs);
        if (fs) impl_->delete_shader(fs);
        return false;
    }

    u32 prog = impl_->create_program();
    impl_->attach_shader(prog, vs);
    impl_->attach_shader(prog, fs);
    impl_->bind_attrib_location(prog, 0, "a_pos");
    impl_->bind_attrib_location(prog, 1, "a_uv");
    impl_->bind_attrib_location(prog, 2, "a_col");
    impl_->link_program(prog);
    impl_->delete_shader(vs);
    impl_->delete_shader(fs);

    int linked = 0;
    impl_->get_programiv(prog, kGlLinkStatus, &linked);
    if (!linked) {
        impl_->delete_program(prog);
        return false;
    }

    impl_->program = prog;
    impl_->u_mvp = impl_->get_uniform_location(prog, "u_mvp");
    impl_->u_tex = impl_->get_uniform_location(prog, "u_tex");
    impl_->u_use_tex = impl_->get_uniform_location(prog, "u_use_tex");
    impl_->u_tex_mode = impl_->get_uniform_location(prog, "u_tex_mode");
    impl_->u_alpha_mode = impl_->get_uniform_location(prog, "u_alpha_mode");
    impl_->u_alpha_ref = impl_->get_uniform_location(prog, "u_alpha_ref");
    impl_->gen_buffers(1, &impl_->vbo);
    impl_->vbo_capacity_bytes = 64u * 1024u;
    impl_->bind_buffer(kGlArrayBuffer, impl_->vbo);
    impl_->buffer_data(kGlArrayBuffer, static_cast<std::ptrdiff_t>(impl_->vbo_capacity_bytes), nullptr, kGlStreamDraw);
    impl_->use_program(prog);
    impl_->uniform1i(impl_->u_tex, 0);
    if (impl_->active_texture) impl_->active_texture(kGlTexture0);
    impl_->enable_vertex_attrib_array(0);
    impl_->enable_vertex_attrib_array(1);
    impl_->enable_vertex_attrib_array(2);
    const char* base = nullptr;
    impl_->vertex_attrib_pointer(0, 3, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, x));
    impl_->vertex_attrib_pointer(1, 2, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, u));
    impl_->vertex_attrib_pointer(2, 4, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, r));
    impl_->state_initialized = true;

    gl_ready_ = true;
    return true;
}

void GLBridge::shutdown_gl() {
    if (!gl_ready_) return;
    batch_verts_.clear();
    impl_->batch_active = false;
    impl_->state_initialized = false;
    impl_->vbo_capacity_bytes = 0;
    if (impl_->backdrop_tex) {
        impl_->delete_textures(1, &impl_->backdrop_tex);
        impl_->backdrop_tex = 0;
        impl_->backdrop_w = 0;
        impl_->backdrop_h = 0;
    }
    if (impl_->vbo) {
        impl_->delete_buffers(1, &impl_->vbo);
        impl_->vbo = 0;
    }
    if (impl_->program) {
        impl_->delete_program(impl_->program);
        impl_->program = 0;
    }
    gl_ready_ = false;
}

void GLBridge::set_aspect_mode(const std::string& aspect_name, int widescreen_override) {
    aspect_name_ = normalize_aspect_mode(aspect_name);
    widescreen_override_ = widescreen_override;
    update_viewport(win_w_, win_h_);
}

void GLBridge::set_world_active(bool world_active) {
    if (world_active != world_active_) {
        world_active_ = world_active;
        update_viewport(win_w_, win_h_);
    }
}

void GLBridge::update_viewport(int win_w, int win_h) {
    win_w_ = std::max(1, win_w);
    win_h_ = std::max(1, win_h);
    if (widescreen_override_ == 0) {
        double target = aspect_preset_ratio(aspect_name_);
        RectI vr = fit_aspect_rect(win_w_, win_h_, target);
        layout_ = {};
        layout_.aspect_mode = aspect_name_;
        layout_.world_active = world_active_;
        layout_.win_w = win_w_;
        layout_.win_h = win_h_;
        layout_.ortho_w = static_cast<float>(kBaseW);
        layout_.ortho_h = static_cast<float>(kBaseH);
        layout_.patch_w = kBaseW;
        layout_.patch_h = kBaseH;
        layout_.viewport_rect = vr;
        layout_.true_widescreen = false;
    } else {
        std::string mode = aspect_name_;
        if (widescreen_override_ == 1 && mode == "original") {
            double wa = static_cast<double>(win_w_) / static_cast<double>(win_h_);
            mode = (std::fabs(wa - 1.5) > 0.01) ? "auto" : "original";
        }
        layout_ = compute_viewport_layout(mode, win_w_, win_h_, world_active_);
    }
    game_w_ = layout_.ortho_w;
    game_h_ = layout_.ortho_h;
    viewport_rect_ = layout_.viewport_rect;
    true_widescreen_ = layout_.true_widescreen;
}

void GLBridge::sync_guest() {
    if (widescreen_) widescreen_->apply_layout(layout_);
}

void GLBridge::set_ui_offset(int ox, int oy) {
    if (ui_ox_ == ox && ui_oy_ == oy) return;
    ui_ox_ = ox;
    ui_oy_ = oy;
    if (has_last_ortho_ && !proj_stack_.empty()) {
        proj_stack_.back() = Mat4::ortho(
            last_ortho_[0] - ox, last_ortho_[1] - ox,
            last_ortho_[2] - oy, last_ortho_[3] - oy,
            last_ortho_[4], last_ortho_[5]);
    }
}

std::pair<float, float> GLBridge::window_to_game_coord(int wx, int wy) const {
    const RectI& vr = viewport_rect_;
    float px = static_cast<float>(wx - vr.x);
    float py = static_cast<float>(wy - (win_h_ - (vr.y + vr.h)));
    float gx = std::max(0.0f, std::min(game_w_, (px / std::max(1, vr.w)) * game_w_));
    float gy = std::max(0.0f, std::min(game_h_, (py / std::max(1, vr.h)) * game_h_));
    return {gx, gy};
}

void GLBridge::begin_frame() {
    flush_batch();
    ui_ox_ = 0;
    ui_oy_ = 0;
    has_last_ortho_ = false;
    if (widescreen_) widescreen_->reset_frame_state();
    bound_tex_ = 0;
    enabled_caps_.clear();
    blend_s_ = kGlOne;
    blend_d_ = kGlZero;
    alpha_func_ = kGlAlways;
    alpha_ref_ = 0.0f;
    tex_replace_ = false;
    sync_guest();

    while (proj_stack_.size() > 1) proj_stack_.pop_back();
    while (mv_stack_.size() > 1) mv_stack_.pop_back();
    proj_stack_[0] = Mat4::identity();
    mv_stack_[0] = Mat4::identity();

    if (!gl_ready_) return;
    impl_->gpu_bound_tex = 0;
    impl_->cached_use_tex = -1;
    impl_->cached_tex_mode = -1;
    impl_->cached_alpha_mode = -99;
    impl_->cached_has_mvp = false;
    impl_->state_initialized = false;
    impl_->use_program(impl_->program);
    impl_->disable(kGlDepthTest);
    impl_->disable(kGlCullFace);
    impl_->disable(kGlScissorTest);
    enabled_caps_[kGlTexture2D] = true;
    enabled_caps_[kGlScissorTest] = false;

    if (viewport_rect_ != RectI{0, 0, win_w_, win_h_}) {
        impl_->viewport(0, 0, win_w_, win_h_);
        impl_->clear_color(0.0f, 0.0f, 0.0f, 1.0f);
        impl_->clear(kGlColorBufferBit | kGlDepthBufferBit);
    }
    impl_->viewport(viewport_rect_.x, viewport_rect_.y, viewport_rect_.w, viewport_rect_.h);
}

void GLBridge::present_widescreen_backdrop() {
    flush_batch();
    if (!gl_ready_ || world_active_ || !impl_->copy_tex_sub_image_2d) return;
    if (viewport_rect_.w <= 0 || viewport_rect_.h <= 0 || win_w_ <= 0 || win_h_ <= 0) return;

    RectI target = compute_target_aspect_rect(aspect_name_, win_w_, win_h_);
    int vp_right = viewport_rect_.x + viewport_rect_.w;
    int vp_top_gl = viewport_rect_.y + viewport_rect_.h;
    int tgt_right = target.x + target.w;
    int tgt_top_gl = target.y + target.h;
    bool need_lr = (target.x < viewport_rect_.x - 1) || (tgt_right > vp_right + 1);
    bool need_tb = (target.y < viewport_rect_.y - 1) || (tgt_top_gl > vp_top_gl + 1);
    if (!need_lr && !need_tb) return;

    if (!impl_->backdrop_tex) {
        impl_->gen_textures(1, &impl_->backdrop_tex);
        if (!impl_->backdrop_tex) return;
        impl_->backdrop_w = 0;
        impl_->backdrop_h = 0;
    }

    impl_->bind_texture(kGlTexture2D, impl_->backdrop_tex);
    bound_tex_ = impl_->backdrop_tex;
    impl_->tex_parameteri(kGlTexture2D, kGlTextureMinFilter, static_cast<int>(kGlLinear));
    impl_->tex_parameteri(kGlTexture2D, kGlTextureMagFilter, static_cast<int>(kGlLinear));
    impl_->tex_parameteri(kGlTexture2D, kGlTextureWrapS, static_cast<int>(kGlClampToEdge));
    impl_->tex_parameteri(kGlTexture2D, kGlTextureWrapT, static_cast<int>(kGlClampToEdge));
    impl_->pixel_storei(kGlUnpackAlignment, 1);

    if (impl_->backdrop_w != viewport_rect_.w || impl_->backdrop_h != viewport_rect_.h) {
        impl_->tex_image_2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), viewport_rect_.w, viewport_rect_.h, 0,
                            kGlRgba, kGlUnsignedByte, nullptr);
        impl_->backdrop_w = viewport_rect_.w;
        impl_->backdrop_h = viewport_rect_.h;
    }
    impl_->copy_tex_sub_image_2d(kGlTexture2D, 0, 0, 0, viewport_rect_.x, viewport_rect_.y,
                                 viewport_rect_.w, viewport_rect_.h);

    overlay_begin_window(win_w_, win_h_);
    impl_->overlay_tex_mode = 2;

    auto draw_wing_grad = [&](float x0, float y0, float w, float h,
                              float u_tl, float v_tl, float u_tr, float v_tr,
                              float u_br, float v_br, float u_bl, float v_bl,
                              float r0, float g0, float b0, float r1, float g1, float b1,
                              bool horiz_fade, bool seam_on_left) {
        auto c_at = [&](bool is_right, bool is_bottom) -> std::array<float, 3> {
            bool is_inner = horiz_fade ? (seam_on_left ? !is_right : is_right)
                                         : (seam_on_left ? !is_bottom : is_bottom);
            return is_inner ? std::array<float, 3>{r0, g0, b0} : std::array<float, 3>{r1, g1, b1};
        };
        auto c_tl = c_at(false, false);
        auto c_tr = c_at(true, false);
        auto c_br = c_at(true, true);
        auto c_bl = c_at(false, true);
        const Vertex v[4] = {
            {x0, y0, 0.0f, u_tl, v_tl, c_tl[0], c_tl[1], c_tl[2], 1.0f},
            {x0 + w, y0, 0.0f, u_tr, v_tr, c_tr[0], c_tr[1], c_tr[2], 1.0f},
            {x0 + w, y0 + h, 0.0f, u_br, v_br, c_br[0], c_br[1], c_br[2], 1.0f},
            {x0, y0 + h, 0.0f, u_bl, v_bl, c_bl[0], c_bl[1], c_bl[2], 1.0f},
        };
        draw_vertices(kGlTriangleFan, v, 4, true, impl_->backdrop_tex);
    };

    static constexpr float kTaps[4][2] = {
        {-0.018f, -0.022f}, {0.018f, -0.022f}, {-0.018f, 0.022f}, {0.018f, 0.022f},
    };

    auto draw_frosted_wing = [&](int sx, int sy_top, int sw, int sh,
                                 float u_l, float u_r, float v_t, float v_b,
                                 bool horiz_fade, bool seam_on_low) {
        if (sw <= 0 || sh <= 0) return;
        overlay_set_scissor_topdown(sx, sy_top, sw, sh);

        impl_->disable(kGlBlend);
        draw_wing_grad(float(sx), float(sy_top), float(sw), float(sh),
                       u_l, v_t, u_r, v_t, u_r, v_b, u_l, v_b,
                       0.22f, 0.23f, 0.26f, 0.11f, 0.12f, 0.14f,
                       horiz_fade, seam_on_low);

        impl_->enable(kGlBlend);
        impl_->blend_func(kGlOne, kGlOne);
        for (int t = 0; t < 4; ++t) {
            float du = kTaps[t][0];
            float dv = kTaps[t][1];
            float ul = std::clamp(u_l + du, 0.0f, 1.0f);
            float ur = std::clamp(u_r + du, 0.0f, 1.0f);
            float vt = std::clamp(v_t + dv, 0.0f, 1.0f);
            float vb = std::clamp(v_b + dv, 0.0f, 1.0f);
            draw_wing_grad(float(sx), float(sy_top), float(sw), float(sh),
                           ul, vt, ur, vt, ur, vb, ul, vb,
                           0.095f, 0.098f, 0.112f, 0.045f, 0.048f, 0.058f,
                           horiz_fade, seam_on_low);
        }
        impl_->blend_func(0x0302u, 0x0303u);
    };

    int tgt_top_screen = win_h_ - (target.y + target.h);
    if (viewport_rect_.x > target.x) {
        draw_frosted_wing(target.x, tgt_top_screen, viewport_rect_.x - target.x, target.h,
                          0.20f, 0.008f, 1.0f, 0.0f, true, false);
    }
    if (tgt_right > vp_right) {
        draw_frosted_wing(vp_right, tgt_top_screen, tgt_right - vp_right, target.h,
                          0.992f, 0.80f, 1.0f, 0.0f, true, true);
    }
    int vp_top_screen = win_h_ - (viewport_rect_.y + viewport_rect_.h);
    int tgt_bot_screen = tgt_top_screen + target.h;
    int vp_bot_screen = vp_top_screen + viewport_rect_.h;
    if (vp_top_screen > tgt_top_screen) {
        draw_frosted_wing(target.x, tgt_top_screen, target.w, vp_top_screen - tgt_top_screen,
                          0.0f, 1.0f, 0.80f, 0.992f, false, false);
    }
    if (tgt_bot_screen > vp_bot_screen) {
        draw_frosted_wing(target.x, vp_bot_screen, target.w, tgt_bot_screen - vp_bot_screen,
                          0.0f, 1.0f, 0.008f, 0.20f, false, true);
    }

    impl_->overlay_tex_mode = -1;
    overlay_clear_scissor();
    overlay_end();
    blend_s_ = 0x0302u;
    blend_d_ = 0x0303u;
    bound_tex_ = 0;
    impl_->gpu_bound_tex = 0;
}

std::vector<u32> GLBridge::gen_textures(int n) {
    if (n <= 0) return {};
    std::vector<u32> ids(static_cast<size_t>(n), 0);
    if (gl_ready_) {
        impl_->gen_textures(n, ids.data());
    } else {
        static u32 s_fake = 100;
        for (int i = 0; i < n; ++i) ids[i] = s_fake++;
    }
    return ids;
}

void GLBridge::bind_texture(u32 target, u32 tex_id) {
    if (target == kGlTexture2D && bound_tex_ == tex_id) return;
    if (target != kGlTexture2D) {
        flush_batch();
        if (gl_ready_) impl_->bind_texture(target, tex_id);
        return;
    }
    bound_tex_ = tex_id;
    auto it = tex_params_.find(tex_id);
    if (it == tex_params_.end()) {
        flush_batch();
        if (gl_ready_ && impl_->gpu_bound_tex != tex_id) {
            impl_->bind_texture(kGlTexture2D, tex_id);
            impl_->gpu_bound_tex = tex_id;
        }
        auto& tp = tex_params_[tex_id];
        auto ensure = [&](u32 pname, int param) {
            if (gl_ready_) impl_->tex_parameteri(target, pname, param);
            tp[pname] = param;
        };
        ensure(kGlTextureWrapS, static_cast<int>(kGlClampToEdge));
        ensure(kGlTextureWrapT, static_cast<int>(kGlClampToEdge));
        ensure(kGlTextureMinFilter, static_cast<int>(kGlNearest));
        ensure(kGlTextureMagFilter, static_cast<int>(kGlNearest));
    }
}

void GLBridge::delete_textures(const std::vector<u32>& tex_ids) {
    flush_batch();
    for (u32 id : tex_ids) {
        tex_params_.erase(id);
        if (bound_tex_ == id) bound_tex_ = 0;
        if (impl_->gpu_bound_tex == id) impl_->gpu_bound_tex = 0;
    }
    if (gl_ready_ && !tex_ids.empty()) {
        impl_->delete_textures(static_cast<int>(tex_ids.size()), tex_ids.data());
    }
}

void GLBridge::tex_image_2d(u32 target, int level, u32, int w, int h, int border, u32 fmt, u32 tp, u32 pix_ptr) {
    if (!gl_ready_) return;
    flush_batch();
    if (target == kGlTexture2D && impl_->gpu_bound_tex != bound_tex_) {
        impl_->bind_texture(kGlTexture2D, bound_tex_);
        impl_->gpu_bound_tex = bound_tex_;
    }
    if (pix_ptr == 0 || !m_) {
        impl_->tex_image_2d(target, level, static_cast<int>(kGlRgba), w, h, border, kGlRgba, kGlUnsignedByte, nullptr);
        return;
    }
    int bpp = 4;
    if (tp == kGlUnsignedShort4444 || tp == kGlUnsignedShort5551 || tp == kGlUnsignedShort565) bpp = 2;
    else if (fmt == kGlRgb) bpp = 3;
    else if (fmt == kGlLuminanceAlpha) bpp = 2;
    else if (fmt == kGlLuminance || fmt == kGlAlpha) bpp = 1;

    size_t nbytes = static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h)) * static_cast<size_t>(bpp);
    scratch_raw_.resize(nbytes);
    if (nbytes > 0 && !m_->rdbytes(pix_ptr, nbytes, scratch_raw_.data())) return;

    Bytes rgba;
    impl_->pixel_storei(kGlUnpackAlignment, 1);
    if (fmt == kGlRgba && tp == kGlUnsignedByte) {
        impl_->tex_image_2d(target, level, static_cast<int>(kGlRgba), w, h, border, kGlRgba, kGlUnsignedByte,
                            scratch_raw_.data());
    } else if (decode_to_rgba8(w, h, fmt, tp, scratch_raw_, rgba)) {
        impl_->tex_image_2d(target, level, static_cast<int>(kGlRgba), w, h, border, kGlRgba, kGlUnsignedByte,
                            rgba.data());
    } else {
        impl_->tex_image_2d(target, level, static_cast<int>(fmt), w, h, border, fmt, tp, scratch_raw_.data());
    }
    tex_parameter_i(target, kGlTextureMinFilter, static_cast<int>(kGlNearest));
    tex_parameter_i(target, kGlTextureMagFilter, static_cast<int>(kGlNearest));
}

void GLBridge::tex_parameter_i(u32 target, u32 pname, int param) {
    if (pname == kGlTextureMinFilter || pname == kGlTextureMagFilter) {
        param = static_cast<int>(kGlNearest);
    }
    if (target == kGlTexture2D && bound_tex_ != 0) {
        auto& tp = tex_params_[bound_tex_];
        auto it = tp.find(pname);
        if (it != tp.end() && it->second == param) return;
        tp[pname] = param;
    }
    flush_batch();
    if (gl_ready_) {
        if (target == kGlTexture2D && impl_->gpu_bound_tex != bound_tex_) {
            impl_->bind_texture(kGlTexture2D, bound_tex_);
            impl_->gpu_bound_tex = bound_tex_;
        }
        impl_->tex_parameteri(target, pname, param);
    }
}

void GLBridge::tex_parameter_f(u32 target, u32 pname, float param) {
    if (pname == kGlTextureMinFilter || pname == kGlTextureMagFilter ||
        pname == kGlTextureWrapS || pname == kGlTextureWrapT) {
        tex_parameter_i(target, pname, static_cast<int>(param));
        return;
    }
    if (gl_ready_ && impl_->tex_parameterf) impl_->tex_parameterf(target, pname, param);
}

void GLBridge::tex_env_i(u32, u32 pname, int param) {
    if (pname == kGlTextureEnvMode) {
        tex_replace_ = (static_cast<u32>(param) == kGlReplace);
    }
}

void GLBridge::matrix_mode(u32 mode) {
    cur_matrix_mode_ = mode;
}

void GLBridge::load_identity() {
    if (cur_matrix_mode_ == kGlProjection) {
        has_last_ortho_ = false;
        proj_stack_.back() = Mat4::identity();
    } else {
        mv_stack_.back() = Mat4::identity();
    }
}

void GLBridge::push_matrix() {
    if (cur_matrix_mode_ == kGlProjection) {
        if (proj_stack_.size() < 2) proj_stack_.push_back(proj_stack_.back());
    } else {
        if (mv_stack_.size() < 16) mv_stack_.push_back(mv_stack_.back());
    }
}

void GLBridge::pop_matrix() {
    if (cur_matrix_mode_ == kGlProjection) {
        if (proj_stack_.size() > 1) proj_stack_.pop_back();
    } else {
        if (mv_stack_.size() > 1) mv_stack_.pop_back();
    }
}

void GLBridge::ortho_f(float l, float r, float b, float t, float n, float f) {
    if (true_widescreen_ && std::fabs(l - 0.0f) < 0.1f && r >= 479.0f) r = game_w_;
    if (true_widescreen_ && std::fabs(t - 0.0f) < 0.1f && b >= 319.0f) b = game_h_;
    has_last_ortho_ = true;
    last_ortho_[0] = l;
    last_ortho_[1] = r;
    last_ortho_[2] = b;
    last_ortho_[3] = t;
    last_ortho_[4] = n;
    last_ortho_[5] = f;
    proj_stack_.back() = Mat4::ortho(l - ui_ox_, r - ui_ox_, b - ui_oy_, t - ui_oy_, n, f);
}

void GLBridge::enable(u32 cap) {
    auto it = enabled_caps_.find(cap);
    if (it != enabled_caps_.end() && it->second) return;
    enabled_caps_[cap] = true;
    if (!gl_ready_) return;
    if (cap == kGlBlend || cap == kGlScissorTest || cap == kGlDepthTest || cap == kGlCullFace || cap == kGlDither) {
        flush_batch();
        impl_->enable(cap);
    }
}

void GLBridge::disable(u32 cap) {
    auto it = enabled_caps_.find(cap);
    if (it != enabled_caps_.end() && !it->second) return;
    enabled_caps_[cap] = false;
    if (!gl_ready_) return;
    if (cap == kGlBlend || cap == kGlScissorTest || cap == kGlDepthTest || cap == kGlCullFace || cap == kGlDither) {
        flush_batch();
        impl_->disable(cap);
    }
}

void GLBridge::enable_client_state(u32 cap) {
    if (cap == kGlVertexArray) client_vert_ = true;
    else if (cap == kGlTextureCoordArray) client_tex_ = true;
    else if (cap == kGlColorArray) client_col_ = true;
}

void GLBridge::disable_client_state(u32 cap) {
    if (cap == kGlVertexArray) client_vert_ = false;
    else if (cap == kGlTextureCoordArray) client_tex_ = false;
    else if (cap == kGlColorArray) client_col_ = false;
}

void GLBridge::blend_func(u32 sfactor, u32 dfactor) {
    if (blend_s_ == sfactor && blend_d_ == dfactor) return;
    blend_s_ = sfactor;
    blend_d_ = dfactor;
    flush_batch();
    if (gl_ready_) impl_->blend_func(sfactor, dfactor);
}

void GLBridge::alpha_func(u32 func, float ref) {
    alpha_func_ = func;
    alpha_ref_ = ref;
}

void GLBridge::clear_color(float r, float g, float b, float a) {
    if (gl_ready_) impl_->clear_color(r, g, b, a);
}

void GLBridge::clear(u32 mask) {
    flush_batch();
    if (gl_ready_) impl_->clear(mask);
}

void GLBridge::scissor(int x, int y, int w, int h) {
    if (!gl_ready_) return;
    flush_batch();
    x += ui_ox_;
    y -= ui_oy_;
    const RectI& vr = viewport_rect_;
    int sx0 = vr.x + static_cast<int>((static_cast<double>(x) / game_w_) * vr.w);
    int sy0 = vr.y + static_cast<int>((static_cast<double>(y) / game_h_) * vr.h);
    int sx1 = vr.x + static_cast<int>(((static_cast<double>(x + w) / game_w_) * vr.w) + 0.5);
    int sy1 = vr.y + static_cast<int>(((static_cast<double>(y + h) / game_h_) * vr.h) + 0.5);
    int sx = std::max(vr.x, std::min(vr.x + vr.w, sx0));
    int sy = std::max(vr.y, std::min(vr.y + vr.h, sy0));
    int sw = std::max(1, std::min(vr.x + vr.w, sx1) - sx);
    int sh = std::max(1, std::min(vr.y + vr.h, sy1) - sy);
    impl_->scissor(sx, sy, sw, sh);
}

void GLBridge::viewport(int, int, int, int) {
    flush_batch();
    if (gl_ready_) impl_->viewport(viewport_rect_.x, viewport_rect_.y, viewport_rect_.w, viewport_rect_.h);
}

void GLBridge::vertex_pointer(int sz, u32 tp, int stride, u32 ptr) {
    v_size_ = sz;
    v_type_ = tp;
    v_stride_ = stride;
    v_ptr_ = ptr;
}

void GLBridge::tex_coord_pointer(int sz, u32 tp, int stride, u32 ptr) {
    t_size_ = sz;
    t_type_ = tp;
    t_stride_ = stride;
    t_ptr_ = ptr;
}

void GLBridge::color_pointer(int sz, u32 tp, int stride, u32 ptr) {
    c_size_ = sz;
    c_type_ = tp;
    c_stride_ = stride;
    c_ptr_ = ptr;
}

void GLBridge::draw_arrays(u32 mode, int first, int count) {
    if (!gl_ready_ || count <= 0 || !m_ || !client_vert_ || v_ptr_ == 0) return;

    scratch_verts_.resize(static_cast<size_t>(count));

    auto get_src_ptr = [&](u32 addr, size_t nbytes) -> const u8* {
        if (const u8* direct = m_->guest_rptr(addr, nbytes)) return direct;
        scratch_raw_.resize(nbytes);
        if (!m_->rdbytes(addr, nbytes, scratch_raw_.data())) return nullptr;
        return scratch_raw_.data();
    };

    // Read vertices
    {
        int eb = elem_size_bytes(v_type_);
        int elem_bytes = std::max(1, v_size_) * eb;
        int strd = (v_stride_ > 0) ? v_stride_ : elem_bytes;
        size_t nbytes = static_cast<size_t>(count - 1) * strd + elem_bytes;
        const u8* src = get_src_ptr(v_ptr_ + static_cast<u32>(first * strd), nbytes);
        if (!src) return;
        for (int i = 0; i < count; ++i) {
            const u8* vp = src + i * strd;
            auto& dst = scratch_verts_[i];
            dst.x = read_coord(vp, v_type_);
            dst.y = (v_size_ >= 2) ? read_coord(vp + eb, v_type_) : 0.0f;
            dst.z = (v_size_ >= 3) ? read_coord(vp + 2 * eb, v_type_) : 0.0f;
        }
    }

    // Read texcoords
    if (client_tex_ && t_ptr_ != 0) {
        int eb = elem_size_bytes(t_type_);
        int elem_bytes = std::max(1, t_size_) * eb;
        int strd = (t_stride_ > 0) ? t_stride_ : elem_bytes;
        size_t nbytes = static_cast<size_t>(count - 1) * strd + elem_bytes;
        if (const u8* src = get_src_ptr(t_ptr_ + static_cast<u32>(first * strd), nbytes)) {
            for (int i = 0; i < count; ++i) {
                const u8* tp = src + i * strd;
                scratch_verts_[i].u = read_coord(tp, t_type_);
                scratch_verts_[i].v = (t_size_ >= 2) ? read_coord(tp + eb, t_type_) : 0.0f;
            }
        }
    } else {
        for (int i = 0; i < count; ++i) {
            scratch_verts_[i].u = 0.0f;
            scratch_verts_[i].v = 0.0f;
        }
    }

    // Read colors
    if (client_col_ && c_ptr_ != 0) {
        int eb = elem_size_bytes(c_type_);
        int elem_bytes = std::max(1, c_size_) * eb;
        int strd = (c_stride_ > 0) ? c_stride_ : elem_bytes;
        size_t nbytes = static_cast<size_t>(count - 1) * strd + elem_bytes;
        if (const u8* src = get_src_ptr(c_ptr_ + static_cast<u32>(first * strd), nbytes)) {
            for (int i = 0; i < count; ++i) {
                const u8* cp = src + i * strd;
                scratch_verts_[i].r = read_color_comp(cp, c_type_);
                scratch_verts_[i].g = (c_size_ >= 2) ? read_color_comp(cp + eb, c_type_) : 1.0f;
                scratch_verts_[i].b = (c_size_ >= 3) ? read_color_comp(cp + 2 * eb, c_type_) : 1.0f;
                scratch_verts_[i].a = (c_size_ >= 4) ? read_color_comp(cp + 3 * eb, c_type_) : 1.0f;
            }
        }
    } else {
        for (int i = 0; i < count; ++i) {
            scratch_verts_[i].r = 1.0f;
            scratch_verts_[i].g = 1.0f;
            scratch_verts_[i].b = 1.0f;
            scratch_verts_[i].a = 1.0f;
        }
    }

    bool use_tex = true;
    if (auto it = enabled_caps_.find(kGlTexture2D); it != enabled_caps_.end()) use_tex = it->second;
    draw_vertices(mode, scratch_verts_.data(), static_cast<size_t>(count), use_tex, bound_tex_);
}

Mat4 GLBridge::current_mvp() const {
    return Mat4::mul(proj_stack_.back(), mv_stack_.back());
}

int GLBridge::compute_alpha_mode() const {
    bool alpha_on = false;
    if (auto it = enabled_caps_.find(kGlAlphaTest); it != enabled_caps_.end()) alpha_on = it->second;
    if (alpha_on && alpha_func_ >= 0x0200u && alpha_func_ <= 0x0207u) {
        return static_cast<int>(alpha_func_ - 0x0200u);
    }
    return -1;
}

void GLBridge::flush_batch() {
    if (!gl_ready_ || batch_verts_.empty()) {
        batch_verts_.clear();
        if (impl_) impl_->batch_active = false;
        return;
    }
    flush_batch_internal();
}

void GLBridge::flush_batch_internal() {
    if (!impl_->state_initialized) {
        impl_->use_program(impl_->program);
        impl_->uniform1i(impl_->u_tex, 0);
        if (impl_->active_texture) impl_->active_texture(kGlTexture0);
        impl_->bind_buffer(kGlArrayBuffer, impl_->vbo);
        impl_->enable_vertex_attrib_array(0);
        impl_->enable_vertex_attrib_array(1);
        impl_->enable_vertex_attrib_array(2);
        const char* base = nullptr;
        impl_->vertex_attrib_pointer(0, 3, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, x));
        impl_->vertex_attrib_pointer(1, 2, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, u));
        impl_->vertex_attrib_pointer(2, 4, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, r));
        impl_->state_initialized = true;
        impl_->cached_has_mvp = false;
        impl_->cached_use_tex = -1;
        impl_->cached_tex_mode = -1;
        impl_->cached_alpha_mode = -99;
    }

    if (!impl_->cached_has_mvp ||
        std::memcmp(impl_->cached_mvp.data(), impl_->batch_mvp.data(), sizeof(impl_->batch_mvp)) != 0) {
        impl_->uniform_matrix4fv(impl_->u_mvp, 1, 0, impl_->batch_mvp.data());
        impl_->cached_mvp = impl_->batch_mvp;
        impl_->cached_has_mvp = true;
    }
    int use_tex_i = impl_->batch_use_tex ? 1 : 0;
    if (impl_->cached_use_tex != use_tex_i) {
        impl_->uniform1i(impl_->u_use_tex, use_tex_i);
        impl_->cached_use_tex = use_tex_i;
    }
    if (impl_->cached_tex_mode != impl_->batch_tex_mode) {
        impl_->uniform1i(impl_->u_tex_mode, impl_->batch_tex_mode);
        impl_->cached_tex_mode = impl_->batch_tex_mode;
    }
    if (impl_->cached_alpha_mode != impl_->batch_alpha_mode) {
        impl_->uniform1i(impl_->u_alpha_mode, impl_->batch_alpha_mode);
        impl_->cached_alpha_mode = impl_->batch_alpha_mode;
        if (impl_->batch_alpha_mode >= 0) {
            impl_->uniform1f(impl_->u_alpha_ref, impl_->batch_alpha_ref);
            impl_->cached_alpha_ref = impl_->batch_alpha_ref;
        }
    } else if (impl_->batch_alpha_mode >= 0 && impl_->cached_alpha_ref != impl_->batch_alpha_ref) {
        impl_->uniform1f(impl_->u_alpha_ref, impl_->batch_alpha_ref);
        impl_->cached_alpha_ref = impl_->batch_alpha_ref;
    }

    if (impl_->batch_use_tex && impl_->batch_tex_id != 0 && impl_->gpu_bound_tex != impl_->batch_tex_id) {
        impl_->bind_texture(kGlTexture2D, impl_->batch_tex_id);
        impl_->gpu_bound_tex = impl_->batch_tex_id;
    }

    size_t need_bytes = batch_verts_.size() * sizeof(Vertex);
    if (!impl_->buffer_sub_data) {
        impl_->buffer_data(kGlArrayBuffer, static_cast<std::ptrdiff_t>(need_bytes), batch_verts_.data(), kGlStreamDraw);
    } else {
        if (need_bytes > impl_->vbo_capacity_bytes) {
            impl_->vbo_capacity_bytes =
                std::max(impl_->vbo_capacity_bytes * 2u, (need_bytes + 65535u) & ~size_t(65535u));
            impl_->buffer_data(kGlArrayBuffer, static_cast<std::ptrdiff_t>(impl_->vbo_capacity_bytes), nullptr,
                               kGlStreamDraw);
        }
        impl_->buffer_sub_data(kGlArrayBuffer, 0, static_cast<std::ptrdiff_t>(need_bytes), batch_verts_.data());
    }

    impl_->draw_arrays(kGlTriangles, 0, static_cast<int>(batch_verts_.size()));
    batch_verts_.clear();
    impl_->batch_active = false;
}

void GLBridge::draw_vertices(u32 mode, const Vertex* verts, size_t count, bool use_tex, u32 tex_id) {
    if (!gl_ready_ || count == 0) return;

    bool actual_use_tex = (use_tex && tex_id != 0);
    u32 actual_tex_id = actual_use_tex ? tex_id : 0u;
    int tex_mode = (impl_->overlay_active && impl_->overlay_tex_mode >= 0) ? impl_->overlay_tex_mode
                                                                          : (tex_replace_ ? 1 : 0);
    int amode = impl_->overlay_active ? -1 : compute_alpha_mode();
    float aref = alpha_ref_;
    Mat4 mvp = impl_->overlay_active ? impl_->overlay_proj : current_mvp();

    size_t tri_verts = 0;
    if (mode == kGlTriangles && count >= 3 && (count % 3 == 0)) {
        tri_verts = count;
    } else if ((mode == kGlTriangleFan || mode == kGlTriangleStrip) && count >= 3) {
        tri_verts = (count - 2) * 3;
    }

    if (tri_verts > 0) {
        bool same_state = impl_->batch_active &&
                          impl_->batch_use_tex == actual_use_tex &&
                          (!actual_use_tex || impl_->batch_tex_id == actual_tex_id) &&
                          impl_->batch_tex_mode == tex_mode &&
                          impl_->batch_alpha_mode == amode &&
                          (amode < 0 || impl_->batch_alpha_ref == aref) &&
                          std::memcmp(impl_->batch_mvp.data(), mvp.m.data(), sizeof(mvp.m)) == 0;
        if (impl_->batch_active && (!same_state || (batch_verts_.size() + tri_verts) > 4096u)) {
            flush_batch_internal();
        }
        if (!impl_->batch_active) {
            impl_->batch_active = true;
            impl_->batch_use_tex = actual_use_tex;
            impl_->batch_tex_id = actual_tex_id;
            impl_->batch_tex_mode = tex_mode;
            impl_->batch_alpha_mode = amode;
            impl_->batch_alpha_ref = aref;
            impl_->batch_mvp = mvp.m;
        }
        if (mode == kGlTriangles) {
            batch_verts_.insert(batch_verts_.end(), verts, verts + count);
        } else if (mode == kGlTriangleFan) {
            for (size_t i = 1; i + 1 < count; ++i) {
                batch_verts_.push_back(verts[0]);
                batch_verts_.push_back(verts[i]);
                batch_verts_.push_back(verts[i + 1]);
            }
        } else {  // kGlTriangleStrip
            for (size_t i = 0; i + 2 < count; ++i) {
                if ((i & 1u) == 0u) {
                    batch_verts_.push_back(verts[i]);
                    batch_verts_.push_back(verts[i + 1]);
                    batch_verts_.push_back(verts[i + 2]);
                } else {
                    batch_verts_.push_back(verts[i + 1]);
                    batch_verts_.push_back(verts[i]);
                    batch_verts_.push_back(verts[i + 2]);
                }
            }
        }
        return;
    }

    // Immediate path for non-triangle primitives (e.g. GL_LINE_LOOP overlay outlines)
    flush_batch();
    if (!impl_->state_initialized) {
        impl_->use_program(impl_->program);
        impl_->uniform1i(impl_->u_tex, 0);
        if (impl_->active_texture) impl_->active_texture(kGlTexture0);
        impl_->bind_buffer(kGlArrayBuffer, impl_->vbo);
        impl_->enable_vertex_attrib_array(0);
        impl_->enable_vertex_attrib_array(1);
        impl_->enable_vertex_attrib_array(2);
        const char* base = nullptr;
        impl_->vertex_attrib_pointer(0, 3, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, x));
        impl_->vertex_attrib_pointer(1, 2, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, u));
        impl_->vertex_attrib_pointer(2, 4, kGlFloat, 0, sizeof(Vertex), base + offsetof(Vertex, r));
        impl_->state_initialized = true;
        impl_->cached_has_mvp = false;
        impl_->cached_use_tex = -1;
        impl_->cached_tex_mode = -1;
        impl_->cached_alpha_mode = -99;
    }
    impl_->uniform_matrix4fv(impl_->u_mvp, 1, 0, mvp.m.data());
    impl_->cached_mvp = mvp.m;
    impl_->cached_has_mvp = true;
    impl_->uniform1i(impl_->u_use_tex, actual_use_tex ? 1 : 0);
    impl_->cached_use_tex = actual_use_tex ? 1 : 0;
    impl_->uniform1i(impl_->u_tex_mode, tex_mode);
    impl_->cached_tex_mode = tex_mode;
    impl_->uniform1i(impl_->u_alpha_mode, amode);
    impl_->cached_alpha_mode = amode;
    if (amode >= 0) {
        impl_->uniform1f(impl_->u_alpha_ref, aref);
        impl_->cached_alpha_ref = aref;
    }
    if (actual_use_tex && impl_->gpu_bound_tex != actual_tex_id) {
        impl_->bind_texture(kGlTexture2D, actual_tex_id);
        impl_->gpu_bound_tex = actual_tex_id;
    }
    size_t need_bytes = count * sizeof(Vertex);
    if (!impl_->buffer_sub_data) {
        impl_->buffer_data(kGlArrayBuffer, static_cast<std::ptrdiff_t>(need_bytes), verts, kGlStreamDraw);
    } else {
        if (need_bytes > impl_->vbo_capacity_bytes) {
            impl_->vbo_capacity_bytes =
                std::max(impl_->vbo_capacity_bytes * 2u, (need_bytes + 65535u) & ~size_t(65535u));
            impl_->buffer_data(kGlArrayBuffer, static_cast<std::ptrdiff_t>(impl_->vbo_capacity_bytes), nullptr,
                               kGlStreamDraw);
        }
        impl_->buffer_sub_data(kGlArrayBuffer, 0, static_cast<std::ptrdiff_t>(need_bytes), verts);
    }
    impl_->draw_arrays(mode, 0, static_cast<int>(count));
}

// -----------------------------------------------------------------------------
// Host Overlay Helpers
// -----------------------------------------------------------------------------
u32 GLBridge::create_rgba_texture(int w, int h, const u8* rgba, bool linear_filter) {
    if (!gl_ready_ || w <= 0 || h <= 0 || !rgba) return 0;
    flush_batch();
    u32 tex = 0;
    impl_->gen_textures(1, &tex);
    if (!tex) return 0;
    impl_->bind_texture(kGlTexture2D, tex);
    bound_tex_ = tex;
    impl_->gpu_bound_tex = tex;
    int filt = static_cast<int>(linear_filter ? kGlLinear : kGlNearest);
    impl_->tex_parameteri(kGlTexture2D, kGlTextureMinFilter, filt);
    impl_->tex_parameteri(kGlTexture2D, kGlTextureMagFilter, filt);
    impl_->tex_parameteri(kGlTexture2D, kGlTextureWrapS, static_cast<int>(kGlClampToEdge));
    impl_->tex_parameteri(kGlTexture2D, kGlTextureWrapT, static_cast<int>(kGlClampToEdge));
    impl_->pixel_storei(kGlUnpackAlignment, 1);
    impl_->tex_image_2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), w, h, 0, kGlRgba, kGlUnsignedByte, rgba);
    return tex;
}

void GLBridge::update_rgba_texture(u32 tex_id, int w, int h, const u8* rgba) {
    if (!gl_ready_ || !tex_id || w <= 0 || h <= 0 || !rgba) return;
    flush_batch();
    impl_->bind_texture(kGlTexture2D, tex_id);
    bound_tex_ = tex_id;
    impl_->gpu_bound_tex = tex_id;
    impl_->pixel_storei(kGlUnpackAlignment, 1);
    impl_->tex_image_2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), w, h, 0, kGlRgba, kGlUnsignedByte, rgba);
}

void GLBridge::destroy_texture(u32 tex_id) {
    if (!gl_ready_ || !tex_id) return;
    flush_batch();
    if (bound_tex_ == tex_id) bound_tex_ = 0;
    if (impl_->gpu_bound_tex == tex_id) impl_->gpu_bound_tex = 0;
    tex_params_.erase(tex_id);
    impl_->delete_textures(1, &tex_id);
}

void GLBridge::overlay_begin_game(float ortho_w, float ortho_h, bool in_viewport) {
    if (!gl_ready_) return;
    flush_batch();
    impl_->overlay_active = true;
    if (in_viewport) {
        impl_->viewport(viewport_rect_.x, viewport_rect_.y, viewport_rect_.w, viewport_rect_.h);
    } else {
        impl_->viewport(0, 0, win_w_, win_h_);
    }
    impl_->overlay_proj = Mat4::ortho(0.0f, ortho_w, ortho_h, 0.0f, -1.0f, 1.0f);
    impl_->overlay_prev_blend = enabled_caps_[kGlBlend];
    impl_->overlay_prev_scissor = enabled_caps_[kGlScissorTest];
    impl_->disable(kGlScissorTest);
    impl_->enable(kGlBlend);
    impl_->blend_func(0x0302u, 0x0303u);  // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
    blend_s_ = 0x0302u;
    blend_d_ = 0x0303u;
}

void GLBridge::overlay_begin_window(int win_w, int win_h) {
    if (!gl_ready_) return;
    flush_batch();
    impl_->overlay_active = true;
    impl_->viewport(0, 0, win_w, win_h);
    impl_->overlay_proj = Mat4::ortho(0.0f, static_cast<float>(win_w), static_cast<float>(win_h), 0.0f, -1.0f, 1.0f);
    impl_->overlay_prev_blend = enabled_caps_[kGlBlend];
    impl_->overlay_prev_scissor = enabled_caps_[kGlScissorTest];
    impl_->disable(kGlScissorTest);
    impl_->enable(kGlBlend);
    impl_->blend_func(0x0302u, 0x0303u);
    blend_s_ = 0x0302u;
    blend_d_ = 0x0303u;
}

void GLBridge::overlay_end() {
    if (!gl_ready_) return;
    flush_batch();
    impl_->overlay_active = false;
    impl_->disable(kGlScissorTest);
    enabled_caps_[kGlScissorTest] = impl_->overlay_prev_scissor;
    if (impl_->overlay_prev_scissor) impl_->enable(kGlScissorTest);
    if (!impl_->overlay_prev_blend) impl_->disable(kGlBlend);
    enabled_caps_[kGlBlend] = impl_->overlay_prev_blend;
}

void GLBridge::overlay_set_scissor_topdown(int x, int y, int w, int h) {
    if (!gl_ready_) return;
    flush_batch();
    int sx = std::max(0, x);
    int sy = std::max(0, win_h_ - (y + h));
    int sw = std::max(1, std::min(win_w_ - sx, w));
    int sh = std::max(1, std::min(win_h_ - sy, h));
    impl_->enable(kGlScissorTest);
    impl_->scissor(sx, sy, sw, sh);
}

void GLBridge::overlay_clear_scissor() {
    flush_batch();
    if (gl_ready_) impl_->disable(kGlScissorTest);
}

void GLBridge::overlay_fill_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    const Vertex v[4] = {
        {x, y, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x + w, y, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x + w, y + h, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x, y + h, 0.0f, 0.0f, 0.0f, r, g, b, a},
    };
    draw_vertices(kGlTriangleFan, v, 4, false, 0);
}

void GLBridge::overlay_outline_rect(float x, float y, float w, float h, float line_w, float r, float g, float b, float a) {
    if (gl_ready_ && impl_->line_width) impl_->line_width(line_w);
    const Vertex v[4] = {
        {x, y, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x + w, y, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x + w, y + h, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x, y + h, 0.0f, 0.0f, 0.0f, r, g, b, a},
    };
    draw_vertices(kGlLineLoop, v, 4, false, 0);
    if (gl_ready_ && impl_->line_width) impl_->line_width(1.0f);
}

void GLBridge::overlay_triangle(float x0, float y0, float x1, float y1, float x2, float y2,
                                float r, float g, float b, float a) {
    const Vertex v[3] = {
        {x0, y0, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x1, y1, 0.0f, 0.0f, 0.0f, r, g, b, a},
        {x2, y2, 0.0f, 0.0f, 0.0f, r, g, b, a},
    };
    draw_vertices(kGlTriangles, v, 3, false, 0);
}

void GLBridge::overlay_textured_rect(u32 tex_id, float x, float y, float w, float h,
                                     float u0, float v0, float u1, float v1,
                                     float r, float g, float b, float a) {
    const Vertex v[4] = {
        {x, y, 0.0f, u0, v0, r, g, b, a},
        {x + w, y, 0.0f, u1, v0, r, g, b, a},
        {x + w, y + h, 0.0f, u1, v1, r, g, b, a},
        {x, y + h, 0.0f, u0, v1, r, g, b, a},
    };
    draw_vertices(kGlTriangleFan, v, 4, true, tex_id);
}

bool GLBridge::read_framebuffer_rgba(int& out_w, int& out_h, Bytes& out_rgba) const {
    if (!gl_ready_) return false;
    const_cast<GLBridge*>(this)->flush_batch();
    out_w = win_w_;
    out_h = win_h_;
    out_rgba.resize(static_cast<size_t>(out_w) * static_cast<size_t>(out_h) * 4);
    impl_->pixel_storei(kGlUnpackAlignment, 1);
    impl_->read_pixels(0, 0, out_w, out_h, kGlRgba, kGlUnsignedByte, out_rgba.data());
    return true;
}

}  // namespace hl3
