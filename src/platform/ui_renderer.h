#pragma once

#include "../core/common.h"

#include <memory>
#include <string>
#include <vector>

namespace hl3 {

struct RgbaColor {
    u8 r = 255;
    u8 g = 255;
    u8 b = 255;
    u8 a = 255;
};

struct UiRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool contains(int px, int py) const {
        return px >= x && py >= y && px < (x + w) && py < (y + h);
    }
};

class RgbaCanvas {
public:
    RgbaCanvas() = default;
    RgbaCanvas(int w, int h, RgbaColor clear = {0, 0, 0, 0});

    void resize(int w, int h, RgbaColor clear = {0, 0, 0, 0});
    int width() const { return w_; }
    int height() const { return h_; }
    const u8* data() const { return px_.data(); }
    u8* data() { return px_.data(); }
    bool empty() const { return w_ <= 0 || h_ <= 0 || px_.empty(); }

    void set_clip(const UiRect& r);
    void clear_clip();

    void fill_rect(int x, int y, int w, int h, RgbaColor c);
    void fill_rounded_rect(int x, int y, int w, int h, int radius, RgbaColor c);
    void fill_rounded_gradient_v(int x, int y, int w, int h, int radius, RgbaColor top, RgbaColor bottom);
    void stroke_rounded_rect(int x, int y, int w, int h, int radius, int stroke_w, RgbaColor c);
    void blit_rgba_scaled(const u8* src, int sw, int sh, int dx, int dy, int dw, int dh);

    // Alpha blends a single grayscale glyph mask.
    void blend_mask(int dst_x, int dst_y, int gw, int gh, const u8* mask, RgbaColor c);

private:
    void blend_px(int x, int y, u8 r, u8 g, u8 b, u8 a);

    int w_ = 0;
    int h_ = 0;
    std::vector<u8> px_;
    bool has_clip_ = false;
    UiRect clip_{};
};

class UiFontSystem {
public:
    UiFontSystem();
    ~UiFontSystem();

    UiFontSystem(const UiFontSystem&) = delete;
    UiFontSystem& operator=(const UiFontSystem&) = delete;

    static UiFontSystem& instance();

    int measure_width(const std::string& utf8, int px_size, bool bold = false);
    int line_height(int px_size, bool bold = false);

    void draw_text(RgbaCanvas& canvas, const std::string& utf8, int x, int y,
                   int px_size, bool bold, RgbaColor color);
    void draw_text_shadow(RgbaCanvas& canvas, const std::string& utf8, int x, int y,
                          int px_size, bool bold, RgbaColor color,
                          RgbaColor shadow = {8, 12, 24, 210}, int sx = 1, int sy = 1);
    std::vector<std::string> wrap_text(const std::string& utf8, int px_size, bool bold, int max_w);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

RgbaCanvas build_osd_canvas(const std::string& badge, const std::string& title,
                          const std::string& subtitle, RgbaColor accent, float ui_scale);

RgbaCanvas build_injected_button_canvas(const std::string& label, const Bytes& ribbon_rgba,
                                      int ribbon_w, int ribbon_h);

}  // namespace hl3

