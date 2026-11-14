#include "bundle.h"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#elif defined(__ANDROID__)
#include <SDL_system.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace hl3 {

namespace {

std::filesystem::path executable_dir() {
#ifdef _WIN32
    wchar_t buf[32768];
    DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    if (n > 0 && n < std::size(buf)) {
        return std::filesystem::path(std::wstring(buf, n)).parent_path();
    }
#elif defined(__linux__) && !defined(__ANDROID__)
    char buf[4096];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0 && n < static_cast<ssize_t>(sizeof(buf))) {
        buf[n] = '\0';
        return std::filesystem::u8path(buf).parent_path();
    }
#endif
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

bool is_dir_writable(const std::filesystem::path& dir) {
    if (dir.empty()) return false;
#ifdef _WIN32
    std::wstring wp = dir.wstring();
    return ::_waccess(wp.c_str(), 2) == 0;
#else
    return ::access(dir.u8string().c_str(), W_OK | X_OK) == 0;
#endif
}

std::string normalize_rel(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    if (s.rfind(kAppPrefix, 0) == 0) {
        s = s.substr(std::strlen(kAppPrefix));
    }
    while (!s.empty() && s[0] == '/') {
        s.erase(s.begin());
    }
    return s;
}

#ifdef __ANDROID__
bool read_android_sdl_rw(const std::string& raw_rel, Bytes& out) {
    std::string clean = raw_rel;
    std::replace(clean.begin(), clean.end(), '\\', '/');
    while (clean.rfind("./", 0) == 0) clean = clean.substr(2);
    while (!clean.empty() && clean[0] == '/') clean.erase(clean.begin());

    std::vector<std::string> candidates;
    candidates.push_back(clean);
    if (clean.rfind("assets/", 0) == 0) {
        candidates.push_back(clean.substr(7));
    }

    for (const auto& cand : candidates) {
        if (cand.empty()) continue;
        SDL_RWops* rw = SDL_RWFromFile(cand.c_str(), "rb");
        if (!rw) continue;
        Sint64 sz = SDL_RWsize(rw);
        if (sz > 0 && sz <= 128LL * 1024LL * 1024LL) {
            out.resize(static_cast<size_t>(sz));
            size_t got = SDL_RWread(rw, out.data(), 1, out.size());
            SDL_RWclose(rw);
            if (got == out.size()) return true;
        } else {
            SDL_RWclose(rw);
        }
    }
    out.clear();
    return false;
}

bool has_android_sdl_rw(const std::string& raw_rel) {
    std::string clean = raw_rel;
    std::replace(clean.begin(), clean.end(), '\\', '/');
    while (clean.rfind("./", 0) == 0) clean = clean.substr(2);
    while (!clean.empty() && clean[0] == '/') clean.erase(clean.begin());

    SDL_RWops* rw = SDL_RWFromFile(clean.c_str(), "rb");
    if (rw) {
        SDL_RWclose(rw);
        return true;
    }
    if (clean.rfind("assets/", 0) == 0) {
        rw = SDL_RWFromFile(clean.substr(7).c_str(), "rb");
        if (rw) {
            SDL_RWclose(rw);
            return true;
        }
    }
    return false;
}
#endif

}  // namespace

std::filesystem::path find_project_or_exe_root() {
#if defined(__SWITCH__)
    return std::filesystem::path("romfs:/");
#elif defined(__ANDROID__)
    return std::filesystem::path{};
#else
    std::error_code ec;
    std::vector<std::filesystem::path> seeds;
    seeds.push_back(executable_dir());
    seeds.push_back(std::filesystem::current_path(ec));

    for (const auto& seed : seeds) {
        std::filesystem::path cur = seed;
        for (int depth = 0; depth < 6 && !cur.empty(); ++depth) {
            if (std::filesystem::exists(cur / "assets" / "game.ipa", ec) ||
                std::filesystem::exists(cur / "project.json", ec)) {
                return cur;
            }
            if (!cur.has_parent_path() || cur.parent_path() == cur) break;
            cur = cur.parent_path();
        }
    }
    return executable_dir();
#endif
}

std::filesystem::path find_writable_data_root() {
    if (const char* env_dir = std::getenv("HL3_DATA_DIR"); env_dir && env_dir[0] != '\0') {
        return std::filesystem::u8path(env_dir);
    }
#if defined(__SWITCH__)
    return std::filesystem::path("sdmc:/switch/heroes_lore_3");
#elif defined(__ANDROID__)
    if (const char* internal = SDL_AndroidGetInternalStoragePath(); internal && internal[0] != '\0') {
        return std::filesystem::u8path(internal);
    }
    if (char* pref = SDL_GetPrefPath("DavidKalilBraga", "HeroesLore3")) {
        std::filesystem::path p = std::filesystem::u8path(pref);
        SDL_free(pref);
        return p;
    }
    return std::filesystem::path(".");
#elif defined(_WIN32)
    std::filesystem::path bundle_root = find_project_or_exe_root();
    if (is_dir_writable(bundle_root)) {
        return bundle_root;
    }
    if (const char* local_app = std::getenv("LOCALAPPDATA"); local_app && local_app[0] != '\0') {
        return std::filesystem::u8path(local_app) / "DavidKalilBraga" / "HeroesLore3";
    }
    return bundle_root;
#else
    std::filesystem::path bundle_root = find_project_or_exe_root();
    bool in_appimage = (std::getenv("APPIMAGE") != nullptr) || (std::getenv("APPDIR") != nullptr);
    if (!in_appimage && is_dir_writable(bundle_root)) {
        return bundle_root;
    }
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && xdg[0] != '\0') {
        return std::filesystem::u8path(xdg) / "DavidKalilBraga" / "HeroesLore3";
    }
    if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
        return std::filesystem::u8path(home) / ".local" / "share" / "DavidKalilBraga" / "HeroesLore3";
    }
    return bundle_root;
#endif
}

bool read_bundle_asset_file(const std::filesystem::path& path, Bytes& out) {
    if (!path.empty() && read_file_bytes(path, out)) {
        return true;
    }
#ifdef __ANDROID__
    if (read_android_sdl_rw(path.generic_u8string(), out)) {
        return true;
    }
#endif
    return false;
}

bool Bundle::open(const std::filesystem::path& root_or_ipa, std::string* err) {
    std::error_code ec;
    Bytes ipa_bytes;
    std::filesystem::path chosen_root;

    auto set_err = [&](const std::string& msg) {
        if (err) *err = msg;
        return false;
    };

    if (!root_or_ipa.empty() && std::filesystem::is_regular_file(root_or_ipa, ec)) {
        if (!read_file_bytes(root_or_ipa, ipa_bytes)) {
            return set_err("Failed to read IPA: " + root_or_ipa.u8string());
        }
        auto p = root_or_ipa.parent_path();
        chosen_root = (p.filename() == "assets") ? p.parent_path() : p;
    } else {
        chosen_root = root_or_ipa.empty() ? find_project_or_exe_root() : root_or_ipa;
        std::filesystem::path ipa_path = chosen_root.empty()
                                             ? std::filesystem::path("assets") / "game.ipa"
                                             : (chosen_root / "assets" / "game.ipa");
        if (read_bundle_asset_file(ipa_path, ipa_bytes)) {
            // Loaded from disk or Android APK assets.
        } else {
            // Fallback to original APK in project root if game.ipa isn't unpacked yet.
            std::filesystem::path apk_path =
                chosen_root / "Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk";
            ZipArchive apk;
            if (!apk.open_from_file(apk_path) || !apk.read_entry("assets/game.ipa", ipa_bytes)) {
                return set_err("Could not locate assets/game.ipa or original APK under " + chosen_root.u8string());
            }
        }
    }

    if (sha256_hex(ipa_bytes) != kExpectedGameIpaSha256) {
        return set_err("assets/game.ipa SHA-256 mismatch");
    }
    if (!ipa_.open_from_memory(std::move(ipa_bytes))) {
        return set_err("Invalid ZIP structure in assets/game.ipa");
    }

    std::string exe_entry = std::string(kAppPrefix) + "Heroes Lore";
    if (!ipa_.read_entry(exe_entry, macho_)) {
        return set_err("Missing Mach-O executable in game.ipa");
    }
    std::string actual_sha = sha256_hex(macho_);
    if (actual_sha != kExpectedExecutableSha256) {
        return set_err("Mach-O executable SHA-256 mismatch: got " + actual_sha +
                       " size=" + std::to_string(macho_.size()));
    }

    root_dir_ = chosen_root;
    assets_dir_ = chosen_root.empty() ? std::filesystem::path("assets") : (chosen_root / "assets");
    cache_.clear();
    return true;
}

void Bundle::set_language(std::string lang) {
    std::string low = lang;
    for (char& c : low) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        else if (c == '_') c = '-';
    }
    lang = (low.rfind("pt", 0) == 0) ? "pt-br" : "en";
    if (lang != language_) {
        language_ = std::move(lang);
        cache_.clear();
    }
}

std::string Bundle::resolve_override_rel(const std::string& rel_path) const {
    std::string norm = normalize_rel(rel_path);
    if (norm.rfind("newres/fonts/", 0) == 0) {
        std::string tail = norm.substr(std::strlen("newres/fonts/"));
        if (tail == "font_mainmenu.rff" || tail == "font_mainmenu.pvr" ||
            tail == "font_menu_medium.rff" || tail == "font_menu_medium.pvr" ||
            tail == "font_menu_small.rff" || tail == "font_menu_small.pvr") {
            return "fonts/" + tail;
        }
    }
    if (language_ == "pt-br") {
        if (norm == "newres/texts/t_en") return "texts/t_pt-br";
        if (norm == "newres/texts/hdr") return "texts/hdr_pt-br";
        if (norm == "dat/InGame_txt_2") return "texts/InGame_txt_2_pt-br";
    }
    return {};
}

bool Bundle::has_asset(const std::string& rel_path) const {
    std::string norm = normalize_rel(rel_path);
    std::string ov = resolve_override_rel(norm);
    if (!ov.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(assets_dir_ / std::filesystem::u8path(ov), ec)) {
            return true;
        }
#ifdef __ANDROID__
        if (has_android_sdl_rw(ov)) return true;
#endif
    }
    return ipa_.has_entry(std::string(kAppPrefix) + norm);
}

bool Bundle::has_asset_or_dir(const std::string& rel_path) const {
    if (has_asset(rel_path)) return true;
    std::string norm = normalize_rel(rel_path);
    while (!norm.empty() && norm.back() == '/') norm.pop_back();
    std::string prefix = std::string(kAppPrefix) + norm + "/";
    for (const auto& e : ipa_.entries()) {
        if (e.name.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

bool Bundle::read_asset(const std::string& rel_path, Bytes& out) const {
    std::string norm = normalize_rel(rel_path);
    std::string cache_key = language_ + "|" + norm;
    if (auto it = cache_.find(cache_key); it != cache_.end()) {
        out = it->second;
        return true;
    }

    std::string ov = resolve_override_rel(norm);
    if (!ov.empty()) {
        std::filesystem::path ov_path = assets_dir_ / std::filesystem::u8path(ov);
        if (read_bundle_asset_file(ov_path, out)) {
            cache_[cache_key] = out;
            return true;
        }
    }

    if (ipa_.read_entry(std::string(kAppPrefix) + norm, out)) {
        cache_[cache_key] = out;
        return true;
    }
    out.clear();
    return false;
}

}  // namespace hl3
