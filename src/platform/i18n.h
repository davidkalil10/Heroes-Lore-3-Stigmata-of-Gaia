#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace hl3 {

inline constexpr const char* kAboutAuthor = "David Kalil Braga";
inline constexpr const char* kAboutYear = "2026";
inline constexpr const char* kAssetWindowsX64 = "heroes_lore_3_windows_x64.zip";
inline constexpr const char* kAssetLinuxX64 = "heroes_lore_3_linux_x86_64.AppImage";
inline constexpr const char* kAssetAndroidApk = "heroes_lore_3_android_universal.apk";
inline constexpr const char* kAssetSwitchNro = "heroes_lore_3.nro";

std::string get_target_asset_name();
std::string get_port_tag(std::string_view version = "");
std::string about_credit_prefix(std::string_view lang, std::string_view version);

std::string set_active_i18n_language(std::string_view lang);
const std::string& get_active_i18n_language();

std::string tr(std::string_view key, std::string_view lang = {});
std::string tr_fmt(std::string_view key,
                   std::initializer_list<std::pair<std::string_view, std::string_view>> vars,
                   std::string_view lang = {});

}  // namespace hl3

