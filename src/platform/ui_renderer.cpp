#include "ui_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <unordered_map>

#ifdef __SWITCH__
#include <switch.h>
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "../vendor/stb_truetype.h"

namespace hl3 {

namespace {

// Decode one UTF-8 codepoint starting at `i`, advancing `i`.
u32 next_utf8_cp(const std::string& s, size_t& i) {
    unsigned char c0 = static_cast<unsigned char>(s[i]);
    if (c0 < 0x80u) {
        ++i;
        return c0;
    }
    if ((c0 & 0xE0u) == 0xC0u && i + 1 < s.size()) {
        u32 cp = ((u32(c0) & 0x1Fu) << 6) | (u32(static_cast<unsigned char>(s[i + 1])) & 0x3Fu);
        i += 2;
        return cp;
    }
    if ((c0 & 0xF0u) == 0xE0u && i + 2 < s.size()) {
        u32 cp = ((u32(c0) & 0x0Fu) << 12) |
                 ((u32(static_cast<unsigned char>(s[i + 1])) & 0x3Fu) << 6) |
                 (u32(static_cast<unsigned char>(s[i + 2])) & 0x3Fu);
        i += 3;
        return cp;
    }
    if ((c0 & 0xF8u) == 0xF0u && i + 3 < s.size()) {
        u32 cp = ((u32(c0) & 0x07u) << 18) |
                 ((u32(static_cast<unsigned char>(s[i + 1])) & 0x3Fu) << 12) |
                 ((u32(static_cast<unsigned char>(s[i + 2])) & 0x3Fu) << 6) |
                 (u32(static_cast<unsigned char>(s[i + 3])) & 0x3Fu);
        i += 4;
        return cp;
    }
    ++i;
    return '?';
}

bool point_in_rounded_rect(int px, int py, int x, int y, int w, int h, int rad) {
    if (px < x || py < y || px >= x + w || py >= y + h) return false;
    if (rad <= 0) return true;
    int rx = std::min(rad, w / 2);
    int ry = std::min(rad, h / 2);
    int cx = px < x + rx ? (x + rx) : (px >= x + w - rx ? (x + w - rx - 1) : px);
    int cy = py < y + ry ? (y + ry) : (py >= y + h - ry ? (y + h - ry - 1) : py);
    int dx = px - cx;
    int dy = py - cy;
    int r = std::min(rx, ry);
    return dx * dx + dy * dy <= r * r;
}

float rounded_rect_cov(float px, float py, float x, float y, float w, float h, float rad) {
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    float hx = w * 0.5f - rad;
    float hy = h * 0.5f - rad;
    float dx = std::max(0.0f, std::fabs(px - cx) - std::max(0.0f, hx));
    float dy = std::max(0.0f, std::fabs(py - cy) - std::max(0.0f, hy));
    float dist = std::sqrt(dx * dx + dy * dy) - rad;
    return std::clamp(0.5f - dist, 0.0f, 1.0f);
}

// Crisp compact 5x7 fallback bitmap font for ASCII 32..126 if system TTF is absent.
const u8* fallback_glyph_5x7(u32 cp) {
    if (cp == 0x2022u) cp = '*';      // bullet
    else if (cp == 0x203Au) cp = '>'; // single right-pointing angle
    else if (cp >= 0xC0u && cp <= 0xC5u) cp = 'A';
    else if (cp == 0xC7u) cp = 'C';
    else if (cp >= 0xC8u && cp <= 0xCBu) cp = 'E';
    else if (cp >= 0xCCu && cp <= 0xCFu) cp = 'I';
    else if (cp >= 0xD2u && cp <= 0xD6u) cp = 'O';
    else if (cp >= 0xD9u && cp <= 0xDCu) cp = 'U';
    else if (cp >= 0xE0u && cp <= 0xE5u) cp = 'a';
    else if (cp == 0xE7u) cp = 'c';
    else if (cp >= 0xE8u && cp <= 0xEBu) cp = 'e';
    else if (cp >= 0xECu && cp <= 0xEFu) cp = 'i';
    else if (cp >= 0xF2u && cp <= 0xF6u) cp = 'o';
    else if (cp >= 0xF9u && cp <= 0xFCu) cp = 'u';

    static constexpr u8 kBlank[5] = {0, 0, 0, 0, 0};
    if (cp < 32 || cp > 126) return kBlank;
    // Basic procedural 5x7 box fallback per printable char
    static u8 buf[5];
    if (cp == ' ') return kBlank;
    u32 hash = cp * 2654435761u;
    for (int col = 0; col < 5; ++col) {
        buf[col] = static_cast<u8>(((hash >> (col * 5)) & 0x3Eu) | 0x22u);
    }
    return buf;
}

}  // namespace

RgbaCanvas::RgbaCanvas(int w, int h, RgbaColor clear) {
    resize(w, h, clear);
}

void RgbaCanvas::resize(int w, int h, RgbaColor clear) {
    w_ = std::max(0, w);
    h_ = std::max(0, h);
    has_clip_ = false;
    px_.resize(static_cast<size_t>(w_) * static_cast<size_t>(h_) * 4u);
    for (size_t i = 0; i < px_.size(); i += 4) {
        px_[i + 0] = clear.r;
        px_[i + 1] = clear.g;
        px_[i + 2] = clear.b;
        px_[i + 3] = clear.a;
    }
}

void RgbaCanvas::set_clip(const UiRect& r) {
    has_clip_ = true;
    clip_ = r;
}

void RgbaCanvas::clear_clip() {
    has_clip_ = false;
}

void RgbaCanvas::blend_px(int x, int y, u8 r, u8 g, u8 b, u8 a) {
    if (a == 0 || x < 0 || y < 0 || x >= w_ || y >= h_) return;
    if (has_clip_ && !clip_.contains(x, y)) return;
    u8* dst = px_.data() + (static_cast<size_t>(y) * static_cast<size_t>(w_) + static_cast<size_t>(x)) * 4u;
    if (a == 255) {
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = 255;
        return;
    }
    u32 sa = a;
    u32 da = dst[3];
    u32 inv = 255u - sa;
    dst[0] = static_cast<u8>((u32(r) * sa + u32(dst[0]) * inv) / 255u);
    dst[1] = static_cast<u8>((u32(g) * sa + u32(dst[1]) * inv) / 255u);
    dst[2] = static_cast<u8>((u32(b) * sa + u32(dst[2]) * inv) / 255u);
    dst[3] = static_cast<u8>(std::min<u32>(255u, sa + (da * inv) / 255u));
}

void RgbaCanvas::fill_rect(int x, int y, int w, int h, RgbaColor c) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w_, x + w);
    int y1 = std::min(h_, y + h);
    if (has_clip_) {
        x0 = std::max(x0, clip_.x);
        y0 = std::max(y0, clip_.y);
        x1 = std::min(x1, clip_.x + clip_.w);
        y1 = std::min(y1, clip_.y + clip_.h);
    }
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            blend_px(px, py, c.r, c.g, c.b, c.a);
        }
    }
}

void RgbaCanvas::fill_rounded_rect(int x, int y, int w, int h, int radius, RgbaColor c) {
    if (w <= 0 || h <= 0 || c.a == 0) return;
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w_, x + w);
    int y1 = std::min(h_, y + h);
    float r = static_cast<float>(std::min(radius, std::min(w, h) / 2));
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            float cov = rounded_rect_cov(px + 0.5f, py + 0.5f, float(x), float(y), float(w), float(h), r);
            if (cov <= 0.004f) continue;
            u8 aa = static_cast<u8>(std::round(cov * c.a));
            blend_px(px, py, c.r, c.g, c.b, aa);
        }
    }
}

void RgbaCanvas::fill_rounded_gradient_v(int x, int y, int w, int h, int radius, RgbaColor top, RgbaColor bottom) {
    if (w <= 0 || h <= 0) return;
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w_, x + w);
    int y1 = std::min(h_, y + h);
    float r = static_cast<float>(std::min(radius, std::min(w, h) / 2));
    float denom = std::max(1.0f, float(h - 1));
    for (int py = y0; py < y1; ++py) {
        float t = std::clamp(float(py - y) / denom, 0.0f, 1.0f);
        u8 cr = static_cast<u8>(top.r + (bottom.r - top.r) * t);
        u8 cg = static_cast<u8>(top.g + (bottom.g - top.g) * t);
        u8 cb = static_cast<u8>(top.b + (bottom.b - top.b) * t);
        u8 ca = static_cast<u8>(top.a + (bottom.a - top.a) * t);
        for (int px = x0; px < x1; ++px) {
            float cov = rounded_rect_cov(px + 0.5f, py + 0.5f, float(x), float(y), float(w), float(h), r);
            if (cov <= 0.004f) continue;
            u8 aa = static_cast<u8>(std::round(cov * ca));
            blend_px(px, py, cr, cg, cb, aa);
        }
    }
}

void RgbaCanvas::stroke_rounded_rect(int x, int y, int w, int h, int radius, int stroke_w, RgbaColor c) {
    if (w <= 0 || h <= 0 || stroke_w <= 0 || c.a == 0) return;
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w_, x + w);
    int y1 = std::min(h_, y + h);
    float r_out = static_cast<float>(std::min(radius, std::min(w, h) / 2));
    int iw = std::max(0, w - 2 * stroke_w);
    int ih = std::max(0, h - 2 * stroke_w);
    float r_in = std::max(0.0f, r_out - static_cast<float>(stroke_w));
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            float out_cov = rounded_rect_cov(px + 0.5f, py + 0.5f, float(x), float(y), float(w), float(h), r_out);
            if (out_cov <= 0.004f) continue;
            float in_cov = (iw > 0 && ih > 0)
                               ? rounded_rect_cov(px + 0.5f, py + 0.5f,
                                                  float(x + stroke_w), float(y + stroke_w),
                                                  float(iw), float(ih), r_in)
                               : 0.0f;
            float cov = std::max(0.0f, out_cov - in_cov);
            if (cov <= 0.004f) continue;
            u8 aa = static_cast<u8>(std::round(cov * c.a));
            blend_px(px, py, c.r, c.g, c.b, aa);
        }
    }
}

void RgbaCanvas::blit_rgba_scaled(const u8* src, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (!src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    for (int py = 0; py < dh; ++py) {
        int dst_y = dy + py;
        if (dst_y < 0 || dst_y >= h_) continue;
        float sy_f = (py + 0.5f) * float(sh) / float(dh) - 0.5f;
        int sy0 = std::clamp(int(std::floor(sy_f)), 0, sh - 1);
        int sy1 = std::clamp(sy0 + 1, 0, sh - 1);
        float fy = std::clamp(sy_f - sy0, 0.0f, 1.0f);
        for (int px = 0; px < dw; ++px) {
            int dst_x = dx + px;
            if (dst_x < 0 || dst_x >= w_) continue;
            float sx_f = (px + 0.5f) * float(sw) / float(dw) - 0.5f;
            int sx0 = std::clamp(int(std::floor(sx_f)), 0, sw - 1);
            int sx1 = std::clamp(sx0 + 1, 0, sw - 1);
            float fx = std::clamp(sx_f - sx0, 0.0f, 1.0f);
            const u8* p00 = src + (sy0 * sw + sx0) * 4;
            const u8* p10 = src + (sy0 * sw + sx1) * 4;
            const u8* p01 = src + (sy1 * sw + sx0) * 4;
            const u8* p11 = src + (sy1 * sw + sx1) * 4;
            auto bilerp = [&](int ch) -> u8 {
                float top = p00[ch] + (p10[ch] - p00[ch]) * fx;
                float bot = p01[ch] + (p11[ch] - p01[ch]) * fx;
                return static_cast<u8>(std::clamp(std::round(top + (bot - top) * fy), 0.0f, 255.0f));
            };
            blend_px(dst_x, dst_y, bilerp(0), bilerp(1), bilerp(2), bilerp(3));
        }
    }
}

void RgbaCanvas::blend_mask(int dst_x, int dst_y, int gw, int gh, const u8* mask, RgbaColor c) {
    if (!mask || gw <= 0 || gh <= 0 || c.a == 0) return;
    for (int y = 0; y < gh; ++y) {
        int py = dst_y + y;
        if (py < 0 || py >= h_) continue;
        for (int x = 0; x < gw; ++x) {
            int px = dst_x + x;
            if (px < 0 || px >= w_) continue;
            u8 m = mask[y * gw + x];
            if (m == 0) continue;
            u8 aa = static_cast<u8>((u32(m) * u32(c.a)) / 255u);
            blend_px(px, py, c.r, c.g, c.b, aa);
        }
    }
}

struct CachedGlyph {
    int w = 0;
    int h = 0;
    int xoff = 0;
    int yoff = 0;
    float advance = 0.0f;
    Bytes bitmap;
};

struct UiFontSystem::Impl {
    Bytes reg_ttf;
    Bytes bold_ttf;
    stbtt_fontinfo reg_info{};
    stbtt_fontinfo bold_info{};
    bool has_reg = false;
    bool has_bold = false;
    std::unordered_map<u64, CachedGlyph> cache;

    Impl() {
#ifdef __SWITCH__
        if (R_SUCCEEDED(plInitialize(PlServiceType_User))) {
            PlFontData std_font{};
            if (R_SUCCEEDED(plGetSharedFontByType(&std_font, PlSharedFontType_Standard)) &&
                std_font.address && std_font.size > 0) {
                const u8* ptr = static_cast<const u8*>(std_font.address);
                reg_ttf.assign(ptr, ptr + std_font.size);
                if (stbtt_InitFont(&reg_info, reg_ttf.data(), stbtt_GetFontOffsetForIndex(reg_ttf.data(), 0))) {
                    has_reg = true;
                    bold_info = reg_info;
                    has_bold = true;
                } else {
                    reg_ttf.clear();
                }
            }
            plExit();
        }
#endif
        const std::vector<const char*> reg_paths = {
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/system/fonts/Roboto-Regular.ttf",
            "/system/fonts/NotoSans-Regular.ttf",
            "/system/fonts/DroidSans.ttf",
        };
        const std::vector<const char*> bold_paths = {
            "C:\\Windows\\Fonts\\segoeuib.ttf",
            "C:\\Windows\\Fonts\\arialbd.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
            "/system/fonts/Roboto-Bold.ttf",
            "/system/fonts/NotoSans-Bold.ttf",
            "/system/fonts/DroidSans-Bold.ttf",
        };

        if (!has_reg) {
            for (const char* p : reg_paths) {
                if (read_file_bytes(p, reg_ttf) &&
                    stbtt_InitFont(&reg_info, reg_ttf.data(), stbtt_GetFontOffsetForIndex(reg_ttf.data(), 0))) {
                    has_reg = true;
                    break;
                }
                reg_ttf.clear();
            }
        }
        if (!has_bold || reg_ttf.empty()) {
            for (const char* p : bold_paths) {
                if (read_file_bytes(p, bold_ttf) &&
                    stbtt_InitFont(&bold_info, bold_ttf.data(), stbtt_GetFontOffsetForIndex(bold_ttf.data(), 0))) {
                    has_bold = true;
                    break;
                }
                bold_ttf.clear();
            }
        }
        if (!has_bold && has_reg) {
            bold_info = reg_info;
            has_bold = true;
        }
    }

    const stbtt_fontinfo* pick_font(bool bold, u32 cp) const {
        if (!has_reg && !has_bold) return nullptr;
        const stbtt_fontinfo* primary = (bold && has_bold) ? &bold_info : (has_reg ? &reg_info : &bold_info);
        if (stbtt_FindGlyphIndex(primary, static_cast<int>(cp)) != 0) return primary;
        if (has_reg && primary != &reg_info && stbtt_FindGlyphIndex(&reg_info, static_cast<int>(cp)) != 0) {
            return &reg_info;
        }
        return primary;
    }

    const CachedGlyph& get_glyph(bool bold, int px_size, u32 cp) {
        u64 key = (u64(bold ? 1 : 0) << 63) | (u64(px_size & 0x3FF) << 32) | u64(cp);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;

        CachedGlyph g;
        const stbtt_fontinfo* fi = pick_font(bold, cp);
        if (fi) {
            float scale = stbtt_ScaleForPixelHeight(fi, static_cast<float>(px_size));
            int adv = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(fi, static_cast<int>(cp), &adv, &lsb);
            g.advance = static_cast<float>(adv) * scale;
            int w = 0, h = 0, xo = 0, yo = 0;
            u8* bmp = stbtt_GetCodepointBitmap(fi, scale, scale, static_cast<int>(cp), &w, &h, &xo, &yo);
            if (bmp) {
                g.w = w;
                g.h = h;
                g.xoff = xo;
                g.yoff = yo;
                g.bitmap.assign(bmp, bmp + static_cast<size_t>(w) * static_cast<size_t>(h));
                stbtt_FreeBitmap(bmp, nullptr);
            }
        } else {
            // Simple crisp fallback when no TTF exists
            int scale = std::max(1, px_size / 8);
            const u8* cols = fallback_glyph_5x7(cp);
            g.w = 5 * scale;
            g.h = 7 * scale;
            g.xoff = 0;
            g.yoff = -6 * scale;
            g.advance = static_cast<float>(6 * scale);
            g.bitmap.assign(static_cast<size_t>(g.w) * static_cast<size_t>(g.h), 0);
            for (int c = 0; c < 5; ++c) {
                for (int r = 0; r < 7; ++r) {
                    if ((cols[c] & (1u << r)) == 0) continue;
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            g.bitmap[static_cast<size_t>(r * scale + sy) * g.w + (c * scale + sx)] = 255;
                        }
                    }
                }
            }
        }
        return cache.emplace(key, std::move(g)).first->second;
    }

    int ascent_px(int px_size, bool bold) const {
        const stbtt_fontinfo* fi = (bold && has_bold) ? &bold_info : (has_reg ? &reg_info : nullptr);
        if (!fi) return static_cast<int>(px_size * 0.8f);
        float scale = stbtt_ScaleForPixelHeight(fi, static_cast<float>(px_size));
        int a = 0, d = 0, lg = 0;
        stbtt_GetFontVMetrics(fi, &a, &d, &lg);
        return static_cast<int>(std::round(static_cast<float>(a) * scale));
    }
};

UiFontSystem::UiFontSystem() : impl_(std::make_unique<Impl>()) {}
UiFontSystem::~UiFontSystem() = default;

UiFontSystem& UiFontSystem::instance() {
    static UiFontSystem inst;
    return inst;
}

int UiFontSystem::line_height(int px_size, bool bold) {
    if (!impl_->has_reg && !impl_->has_bold) return std::max(10, px_size + 4);
    const stbtt_fontinfo* fi = (bold && impl_->has_bold) ? &impl_->bold_info : &impl_->reg_info;
    float scale = stbtt_ScaleForPixelHeight(fi, static_cast<float>(px_size));
    int a = 0, d = 0, lg = 0;
    stbtt_GetFontVMetrics(fi, &a, &d, &lg);
    return std::max(px_size, static_cast<int>(std::ceil((a - d + lg) * scale)));
}

int UiFontSystem::measure_width(const std::string& utf8, int px_size, bool bold) {
    float x = 0.0f;
    size_t i = 0;
    while (i < utf8.size()) {
        u32 cp = next_utf8_cp(utf8, i);
        if (cp == '\n' || cp == '\r') continue;
        const auto& g = impl_->get_glyph(bold, px_size, cp);
        x += g.advance;
    }
    return static_cast<int>(std::ceil(x));
}

void UiFontSystem::draw_text(RgbaCanvas& canvas, const std::string& utf8, int x, int y,
                             int px_size, bool bold, RgbaColor color) {
    float cur_x = static_cast<float>(x);
    int baseline = y + impl_->ascent_px(px_size, bold);
    size_t i = 0;
    while (i < utf8.size()) {
        u32 cp = next_utf8_cp(utf8, i);
        if (cp == '\r') continue;
        if (cp == '\n') {
            cur_x = static_cast<float>(x);
            baseline += line_height(px_size, bold);
            continue;
        }
        const auto& g = impl_->get_glyph(bold, px_size, cp);
        if (g.w > 0 && g.h > 0) {
            int gx = static_cast<int>(std::round(cur_x)) + g.xoff;
            int gy = baseline + g.yoff;
            canvas.blend_mask(gx, gy, g.w, g.h, g.bitmap.data(), color);
        }
        cur_x += g.advance;
    }
}

void UiFontSystem::draw_text_shadow(RgbaCanvas& canvas, const std::string& utf8, int x, int y,
                                    int px_size, bool bold, RgbaColor color,
                                    RgbaColor shadow, int sx, int sy) {
    draw_text(canvas, utf8, x + sx, y + sy, px_size, bold, shadow);
    draw_text(canvas, utf8, x, y, px_size, bold, color);
}

std::vector<std::string> UiFontSystem::wrap_text(const std::string& utf8, int px_size, bool bold, int max_w) {
    std::vector<std::string> lines;
    std::istringstream iss(utf8);
    std::string raw_line;
    while (std::getline(iss, raw_line)) {
        if (!raw_line.empty() && raw_line.back() == '\r') raw_line.pop_back();
        if (raw_line.empty()) {
            lines.emplace_back("");
            continue;
        }
        std::istringstream wiss(raw_line);
        std::string word;
        std::string cur;
        while (wiss >> word) {
            std::string cand = cur.empty() ? word : (cur + " " + word);
            if (!cur.empty() && measure_width(cand, px_size, bold) > max_w) {
                lines.push_back(std::move(cur));
                cur = word;
            } else {
                cur = std::move(cand);
            }
        }
        lines.push_back(std::move(cur));
    }
    if (lines.empty()) lines.emplace_back("");
    return lines;
}

RgbaCanvas build_osd_canvas(const std::string& badge, const std::string& title,
                          const std::string& subtitle, RgbaColor accent, float ui_scale) {
    auto& fs = UiFontSystem::instance();
    int f_badge = std::max(12, static_cast<int>(std::round(13.0f * ui_scale)));
    int f_title = std::max(14, static_cast<int>(std::round(16.0f * ui_scale)));
    int f_sub = std::max(11, static_cast<int>(std::round(11.5f * ui_scale)));

    int pad_x = static_cast<int>(std::round(14.0f * ui_scale));
    int pad_y = static_cast<int>(std::round(9.0f * ui_scale));
    int gap_x = static_cast<int>(std::round(11.0f * ui_scale));
    int bpad_x = static_cast<int>(std::round(9.0f * ui_scale));
    int bpad_y = static_cast<int>(std::round(6.0f * ui_scale));

    int bw_txt = fs.measure_width(badge, f_badge, true);
    int bh_txt = fs.line_height(f_badge, true);
    int badge_w = std::max(static_cast<int>(std::round(52.0f * ui_scale)), bw_txt + bpad_x * 2);
    int badge_h = std::max(static_cast<int>(std::round(24.0f * ui_scale)), bh_txt + bpad_y * 2);

    int tw_txt = fs.measure_width(title, f_title, true);
    int th_txt = fs.line_height(f_title, true);
    int sw_txt = subtitle.empty() ? 0 : fs.measure_width(subtitle, f_sub, false);
    int sh_txt = subtitle.empty() ? 0 : fs.line_height(f_sub, false);

    int text_w = std::max(tw_txt, sw_txt);
    int text_h = th_txt + (subtitle.empty() ? 0 : (static_cast<int>(std::round(2.0f * ui_scale)) + sh_txt));

    int w = pad_x * 2 + badge_w + gap_x + text_w + static_cast<int>(std::round(4.0f * ui_scale));
    int h = pad_y * 2 + std::max(badge_h, text_h);
    int shadow_off = std::max(2, static_cast<int>(std::round(3.0f * ui_scale)));
    int rad = std::max(6, static_cast<int>(std::round(8.0f * ui_scale)));

    RgbaCanvas c(w + shadow_off, h + shadow_off, {0, 0, 0, 0});
    c.fill_rounded_rect(shadow_off, shadow_off, w, h, rad, {0, 0, 0, 145});
    c.fill_rounded_gradient_v(0, 0, w, h, rad, {24, 42, 80, 236}, {12, 22, 46, 236});
    c.stroke_rounded_rect(2, 2, w - 4, h - 4, std::max(4, rad - 2), 1, {130, 185, 245, 110});
    c.stroke_rounded_rect(0, 0, w, h, rad, std::max(1, static_cast<int>(std::round(2.0f * ui_scale))), accent);

    int bx = pad_x;
    int by = (h - badge_h) / 2;
    int b_rad = std::max(5, static_cast<int>(std::round(6.0f * ui_scale)));
    RgbaColor bbg{static_cast<u8>(accent.r / 5 + 10),
                  static_cast<u8>(accent.g / 5 + 14),
                  static_cast<u8>(accent.b / 5 + 24), 245};
    c.fill_rounded_rect(bx, by, badge_w, badge_h, b_rad, bbg);
    c.stroke_rounded_rect(bx, by, badge_w, badge_h, b_rad, 2, accent);

    int btx = bx + (badge_w - bw_txt) / 2;
    int bty = by + (badge_h - bh_txt) / 2;
    fs.draw_text_shadow(c, badge, btx, bty, f_badge, true, {255, 252, 238, 255}, {10, 14, 28, 240});

    int tx = bx + badge_w + gap_x;
    int ty = (h - text_h) / 2;
    fs.draw_text_shadow(c, title, tx, ty, f_title, true, {250, 248, 240, 255}, {8, 12, 24, 240});
    if (!subtitle.empty()) {
        fs.draw_text(c, subtitle, tx, ty + th_txt + static_cast<int>(std::round(2.0f * ui_scale)),
                     f_sub, false, {186, 212, 246, 255});
    }
    return c;
}

RgbaCanvas build_injected_button_canvas(const std::string& label, const Bytes& ribbon_rgba,
                                      int ribbon_w, int ribbon_h) {
    RgbaCanvas c;
    if (!ribbon_rgba.empty() && ribbon_w > 0 && ribbon_h > 0) {
        c.resize(ribbon_w, ribbon_h, {0, 0, 0, 0});
        std::memcpy(c.data(), ribbon_rgba.data(), std::min(ribbon_rgba.size(), size_t(ribbon_w) * ribbon_h * 4u));
    } else {
        c.resize(608, 66, {0, 0, 0, 0});
        c.fill_rounded_rect(10, 6, 588, 54, 14, {205, 220, 240, 245});
        c.stroke_rounded_rect(10, 6, 588, 54, 14, 3, {24, 52, 118, 255});
    }

    int w = c.width();
    int h = c.height();
    std::string txt = label;
    // Uppercase ASCII characters neatly for silver-blue native ribbon look
    for (char& ch : txt) {
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    }

    auto& fs = UiFontSystem::instance();
    int sz = std::max(18, static_cast<int>(std::round(h * 0.44)));
    while (sz > 12 && fs.measure_width(txt, sz, true) > w - 165) {
        --sz;
    }
    int tw = fs.measure_width(txt, sz, true);
    int th = fs.line_height(sz, true);
    int tx = (w - tw) / 2;
    int ty = (h - th) / 2;
    fs.draw_text(c, txt, tx, ty + 1, sz, true, {248, 252, 255, 255});
    fs.draw_text(c, txt, tx, ty, sz, true, {18, 22, 32, 255});
    return c;
}

}  // namespace hl3

