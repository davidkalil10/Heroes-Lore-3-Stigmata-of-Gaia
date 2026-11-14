#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace hl3 {

inline constexpr int kBaseW = 480;
inline constexpr int kBaseH = 320;
inline constexpr int kMaxPatchDim = 1020;
inline constexpr const char* kDefaultAspectMode = "auto";
inline constexpr int kDefaultFpsTarget = 30;
inline constexpr const char* kDefaultLanguage = "en";

inline constexpr std::array<const char*, 3> kUiAspectCycle = {"auto", "16:9", "original"};

struct RectI {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool operator==(const RectI& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
    bool operator!=(const RectI& o) const { return !(*this == o); }
};

struct ViewportLayout {
    std::string aspect_mode = kDefaultAspectMode;
    bool world_active = true;
    int win_w = 960;
    int win_h = 640;
    float ortho_w = 480.0f;
    float ortho_h = 320.0f;
    int patch_w = 480;
    int patch_h = 320;
    RectI viewport_rect{0, 0, 960, 640};
    bool true_widescreen = false;
    int ui_dx = 0;
    int ui_dy = 0;
    int hud_dx_center = 0;
    int hud_dx_right = 0;
    int hud_dy_bottom = 0;
};

struct DisplayConfig {
    std::string aspect_mode = kDefaultAspectMode;
    int target_fps = kDefaultFpsTarget;
    std::string language = kDefaultLanguage;
};

double aspect_preset_ratio(const std::string& mode);
bool is_aspect_preset(const std::string& mode);
std::string normalize_aspect_mode(const std::string& mode);
int normalize_fps(int fps);
std::string normalize_language(const std::string& lang);
int ceil_mult4(double val, int minimum, int maximum = kMaxPatchDim);
RectI fit_aspect_rect(int win_w, int win_h, double target_aspect);
ViewportLayout compute_viewport_layout(const std::string& aspect_mode, int win_w, int win_h, bool world_active = true);

std::filesystem::path default_display_config_path();
DisplayConfig load_display_config(const std::filesystem::path& path = {});
std::filesystem::path save_display_config(const DisplayConfig& cfg, const std::filesystem::path& path = {});

}  // namespace hl3

