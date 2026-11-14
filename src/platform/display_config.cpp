#include "display_config.h"

#include "../core/bundle.h"
#include "../core/common.h"
#include "../core/json_mini.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace hl3 {

namespace {

std::string to_lower_trim(std::string s) {
    s = trim_ascii(s);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}  // namespace

bool is_aspect_preset(const std::string& mode) {
    return mode == "original" || mode == "3:2" || mode == "16:9" || mode == "16:10" ||
           mode == "20:9" || mode == "4:3";
}

double aspect_preset_ratio(const std::string& mode) {
    if (mode == "original" || mode == "3:2") return 480.0 / 320.0;
    if (mode == "16:9") return 16.0 / 9.0;
    if (mode == "16:10") return 16.0 / 10.0;
    if (mode == "20:9") return 20.0 / 9.0;
    if (mode == "4:3") return 4.0 / 3.0;
    return 480.0 / 320.0;
}

std::string normalize_aspect_mode(const std::string& mode) {
    if (mode.empty()) return kDefaultAspectMode;
    std::string key = to_lower_trim(mode);
    if (key == "3:2" || key == "classic" || key == "orig" || key == "original") return "original";
    if (key == "auto" || key == "fit" || key == "fill") return "auto";
    if (is_aspect_preset(key)) return key;
    return kDefaultAspectMode;
}

int normalize_fps(int fps) {
    if (fps <= 0) return kDefaultFpsTarget;
    return (fps >= 45) ? 60 : 30;
}

std::string normalize_language(const std::string& lang) {
    if (lang.empty()) return kDefaultLanguage;
    std::string key = to_lower_trim(lang);
    std::replace(key.begin(), key.end(), '_', '-');
    if (key.rfind("pt", 0) == 0 || key.find("portugu") != std::string::npos) return "pt-BR";
    if (key.rfind("en", 0) == 0 || key.find("english") != std::string::npos) return "en";
    return kDefaultLanguage;
}

int ceil_mult4(double val, int minimum, int maximum) {
    int c = static_cast<int>(std::ceil(val - 1e-6));
    c = std::max(minimum, std::min(maximum, c));
    int m = ((c + 3) / 4) * 4;
    return std::min(maximum, m);
}

RectI fit_aspect_rect(int win_w, int win_h, double target_aspect) {
    double win_aspect = static_cast<double>(win_w) / static_cast<double>(std::max(1, win_h));
    if (std::fabs(win_aspect - target_aspect) < 0.002) {
        return {0, 0, win_w, win_h};
    }
    int vx = 0, vy = 0, vw = 1, vh = 1;
    if (win_aspect > target_aspect) {
        vh = win_h;
        vw = std::max(1, static_cast<int>(std::llround(vh * target_aspect)));
        vx = (win_w - vw) / 2;
        vy = 0;
    } else {
        vw = win_w;
        vh = std::max(1, static_cast<int>(std::llround(vw / target_aspect)));
        vx = 0;
        vy = (win_h - vh) / 2;
    }
    return {vx, vy, vw, vh};
}

ViewportLayout compute_viewport_layout(const std::string& aspect_mode, int win_w, int win_h, bool world_active) {
    int w = std::max(1, win_w);
    int h = std::max(1, win_h);
    std::string mode = normalize_aspect_mode(aspect_mode);
    double win_aspect = static_cast<double>(w) / static_cast<double>(h);

    double ortho_w = static_cast<double>(kBaseW);
    double ortho_h = static_cast<double>(kBaseH);
    int patch_w = kBaseW;
    int patch_h = kBaseH;
    RectI vr{0, 0, w, h};

    if (!world_active || mode == "original") {
        double target_aspect = aspect_preset_ratio("original");
        vr = fit_aspect_rect(w, h, target_aspect);
    } else if (mode == "auto") {
        double max_aspect = static_cast<double>(kMaxPatchDim) / static_cast<double>(kBaseH);
        double min_aspect = static_cast<double>(kBaseW) / static_cast<double>(kMaxPatchDim);
        double clamped = std::max(min_aspect, std::min(max_aspect, win_aspect));
        if (clamped >= 1.5) {
            ortho_w = std::max(static_cast<double>(kBaseW), static_cast<double>(kBaseH) * clamped);
            ortho_h = static_cast<double>(kBaseH);
        } else {
            ortho_w = static_cast<double>(kBaseW);
            ortho_h = std::max(static_cast<double>(kBaseH), static_cast<double>(kBaseW) / clamped);
        }
        ortho_w = std::min(static_cast<double>(kMaxPatchDim), ortho_w);
        ortho_h = std::min(static_cast<double>(kMaxPatchDim), ortho_h);
        patch_w = ceil_mult4(ortho_w, kBaseW);
        patch_h = ceil_mult4(ortho_h, kBaseH);
        if (std::fabs(win_aspect - clamped) < 1e-4) {
            vr = {0, 0, w, h};
        } else {
            vr = fit_aspect_rect(w, h, clamped);
        }
    } else {
        double target_aspect = aspect_preset_ratio(mode);
        if (target_aspect >= 1.5) {
            ortho_w = std::max(static_cast<double>(kBaseW), static_cast<double>(kBaseH) * target_aspect);
            ortho_h = static_cast<double>(kBaseH);
        } else {
            ortho_w = static_cast<double>(kBaseW);
            ortho_h = std::max(static_cast<double>(kBaseH), static_cast<double>(kBaseW) / target_aspect);
        }
        ortho_w = std::min(static_cast<double>(kMaxPatchDim), ortho_w);
        ortho_h = std::min(static_cast<double>(kMaxPatchDim), ortho_h);
        patch_w = ceil_mult4(ortho_w, kBaseW);
        patch_h = ceil_mult4(ortho_h, kBaseH);
        vr = fit_aspect_rect(w, h, target_aspect);
    }

    bool widescreen = (patch_w > kBaseW) || (patch_h > kBaseH);
    double extra_w = std::max(0.0, ortho_w - static_cast<double>(kBaseW));
    double extra_h = std::max(0.0, ortho_h - static_cast<double>(kBaseH));
    int dx_half = world_active ? static_cast<int>(std::llround(extra_w * 0.5)) : 0;
    int dy_half = world_active ? static_cast<int>(std::llround(extra_h * 0.5)) : 0;

    ViewportLayout out;
    out.aspect_mode = mode;
    out.world_active = world_active;
    out.win_w = w;
    out.win_h = h;
    out.ortho_w = static_cast<float>(ortho_w);
    out.ortho_h = static_cast<float>(ortho_h);
    out.patch_w = patch_w;
    out.patch_h = patch_h;
    out.viewport_rect = vr;
    out.true_widescreen = widescreen;
    out.ui_dx = dx_half;
    out.ui_dy = dy_half;
    out.hud_dx_center = static_cast<int>(std::llround(extra_w * 0.5));
    out.hud_dx_right = static_cast<int>(std::llround(extra_w));
    out.hud_dy_bottom = static_cast<int>(std::llround(extra_h));
    return out;
}

std::filesystem::path default_display_config_path() {
    return find_writable_data_root() / "config" / "display.json";
}

DisplayConfig load_display_config(const std::filesystem::path& path) {
    std::filesystem::path cfg_path = path.empty() ? default_display_config_path() : path;
    DisplayConfig out;
    Bytes raw;
    if (!read_file_bytes(cfg_path, raw)) return out;
    JsonValue doc;
    if (!JsonValue::parse(std::string(raw.begin(), raw.end()), doc) || !doc.is_object()) return out;
    if (const auto* v = doc.get("aspect_mode"); v && v->is_string()) {
        out.aspect_mode = normalize_aspect_mode(v->str_val);
    }
    if (const auto* v = doc.get("target_fps"); v) {
        if (v->is_number()) out.target_fps = normalize_fps(static_cast<int>(v->num_val));
        else if (v->is_string()) out.target_fps = normalize_fps(std::atoi(v->str_val.c_str()));
    }
    if (const auto* v = doc.get("language"); v && v->is_string()) {
        out.language = normalize_language(v->str_val);
    }
    return out;
}

std::filesystem::path save_display_config(const DisplayConfig& cfg, const std::filesystem::path& path) {
    std::filesystem::path cfg_path = path.empty() ? default_display_config_path() : path;
    JsonValue doc = JsonValue::make_object();
    doc.obj_val["aspect_mode"] = JsonValue::make_string(normalize_aspect_mode(cfg.aspect_mode));
    doc.obj_val["target_fps"] = JsonValue::make_number(static_cast<double>(normalize_fps(cfg.target_fps)));
    doc.obj_val["language"] = JsonValue::make_string(normalize_language(cfg.language));
    std::string text = doc.dump(2) + "\n";
    write_file_bytes(cfg_path, text.data(), text.size());
    return cfg_path;
}

}  // namespace hl3

