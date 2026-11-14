#include "updater.h"
#include "../core/bundle.h"
#include "../core/json_mini.h"
#include "../core/zip_archive.h"
#include "i18n.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace hl3 {

namespace {

bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string find_sha256_in_line(const std::string& line) {
    size_t run = 0;
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); ++i) {
        char c = (i < line.size()) ? line[i] : '\0';
        bool hex = (i < line.size()) && is_hex_char(c);
        if (hex) {
            if (run == 0) start = i;
            ++run;
        } else {
            if (run == 64) {
                bool left_ok = (start == 0) || (!std::isalnum(static_cast<unsigned char>(line[start - 1])) && line[start - 1] != '_');
                bool right_ok = (i == line.size()) || (!std::isalnum(static_cast<unsigned char>(c)) && c != '_');
                if (left_ok && right_ok) return to_lower(line.substr(start, 64));
            }
            run = 0;
        }
    }
    return {};
}

std::string extract_sha256_from_asset_or_body(const JsonValue& asset, const std::string& body,
                                              const std::string& asset_name) {
    std::string digest = trim_ascii(asset.get_string("digest"));
    std::string low_d = to_lower(digest);
    if (low_d.rfind("sha256:", 0) == 0) {
        std::string cand = trim_ascii(digest.substr(7));
        if (cand.size() == 64 && std::all_of(cand.begin(), cand.end(), is_hex_char)) {
            return to_lower(cand);
        }
    }
    std::istringstream iss(body);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find(asset_name) != std::string::npos || to_lower(line).find("sha256") != std::string::npos) {
            std::string found = find_sha256_in_line(line);
            if (!found.empty()) return found;
        }
    }
    return {};
}

std::string ps_escape_single_quotes(const std::filesystem::path& p) {
    std::string s = p.string();
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\'') out += "''";
        else out.push_back(c);
    }
    return out;
}

std::string now_local_timestamp_str() {
    std::time_t t = std::time(nullptr);
    std::tm tm_loc{};
#ifdef _WIN32
    localtime_s(&tm_loc, &t);
#else
    localtime_r(&t, &tm_loc);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm_loc.tm_year + 1900, tm_loc.tm_mon + 1, tm_loc.tm_mday,
                  tm_loc.tm_hour, tm_loc.tm_min, tm_loc.tm_sec);
    return std::string(buf);
}

std::filesystem::path find_extracted_exe_root(const std::filesystem::path& dest_dir) {
    std::error_code ec;
    if (std::filesystem::exists(dest_dir / "HeroesLore3.exe", ec)) return dest_dir;
    for (const auto& entry : std::filesystem::directory_iterator(dest_dir, ec)) {
        if (entry.is_directory(ec) && std::filesystem::exists(entry.path() / "HeroesLore3.exe", ec)) {
            return entry.path();
        }
    }
    return dest_dir;
}

}  // namespace

std::filesystem::path default_services_config_path() {
    return find_writable_data_root() / "config" / "services.json";
}

std::filesystem::path default_updates_dir() {
    return find_writable_data_root() / "data" / "updates";
}

bool load_services_startup_check(const std::filesystem::path& path) {
    std::filesystem::path cfg = path.empty() ? default_services_config_path() : path;
    Bytes raw;
    if (!read_file_bytes(cfg, raw)) return kDefaultStartupUpdateCheck;
    JsonValue doc;
    if (!JsonValue::parse(std::string(raw.begin(), raw.end()), doc) || !doc.is_object()) {
        return kDefaultStartupUpdateCheck;
    }
    return doc.get_bool("check_updates_at_startup", kDefaultStartupUpdateCheck);
}

std::filesystem::path save_services_startup_check(bool enabled, const std::filesystem::path& path) {
    std::filesystem::path cfg = path.empty() ? default_services_config_path() : path;
    std::error_code ec;
    if (cfg.has_parent_path()) std::filesystem::create_directories(cfg.parent_path(), ec);
    JsonValue::Object obj;
    obj["check_updates_at_startup"] = JsonValue::make_bool(enabled);
    std::string out = JsonValue::make_object(std::move(obj)).dump(2) + "\n";
    auto tmp = cfg;
    tmp += ".tmp";
    if (write_file_bytes(tmp, out.data(), out.size())) {
        std::filesystem::rename(tmp, cfg, ec);
    }
    return cfg;
}

std::optional<std::array<int, 3>> parse_semver(const std::string& version_or_tag) {
    std::string s = trim_ascii(version_or_tag);
    if (s.empty()) return std::nullopt;
    if (s[0] == 'v' || s[0] == 'V') s.erase(s.begin());
    if (s.empty()) return std::nullopt;

    std::array<int, 3> parts{0, 0, 0};
    size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        size_t dot = s.find('.', start);
        if (i < 2 && dot == std::string::npos) return std::nullopt;
        if (i == 2 && dot != std::string::npos) return std::nullopt;
        std::string_view seg = (dot == std::string::npos)
                                   ? std::string_view(s).substr(start)
                                   : std::string_view(s).substr(start, dot - start);
        if (seg.empty()) return std::nullopt;
        for (char c : seg) {
            if (c < '0' || c > '9') return std::nullopt;
        }
        parts[i] = std::atoi(std::string(seg).c_str());
        start = dot + 1;
    }
    return parts;
}

bool is_newer_version(const std::string& remote_tag, const std::string& local_version) {
    auto r = parse_semver(remote_tag);
    auto l = parse_semver(local_version);
    if (!r || !l) return false;
    return *r > *l;
}

bool is_valid_release_download_url(const std::string& url, const std::string& repo) {
    std::string prefix = "https://github.com/" + repo + "/releases/download/";
    return url.rfind(prefix, 0) == 0 && url.size() > prefix.size();
}

bool parse_latest_release_payload(
    const std::string& json_body,
    UpdateReleaseInfo& out_info,
    std::string& out_reason,
    const std::string& target_asset,
    const std::string& repo) {
    out_info = {};
    JsonValue payload;
    std::string perr;
    if (!JsonValue::parse(json_body, payload, &perr) || !payload.is_object()) {
        out_reason = "invalid_payload";
        return false;
    }
    if (payload.get_bool("draft", false) || payload.get_bool("prerelease", false)) {
        out_reason = "no_release_yet";
        return false;
    }

    std::string tag_name = trim_ascii(payload.get_string("tag_name"));
    auto sv = parse_semver(tag_name);
    if (!sv) {
        out_reason = "invalid_version";
        return false;
    }
    std::string ver_str = std::to_string((*sv)[0]) + "." + std::to_string((*sv)[1]) + "." + std::to_string((*sv)[2]);
    std::string wanted = target_asset.empty() ? get_target_asset_name() : target_asset;

    const JsonValue* assets_val = payload.get("assets");
    const JsonValue* matched = nullptr;
    if (assets_val && assets_val->is_array()) {
        for (const auto& item : assets_val->as_array()) {
            if (item.is_object() && item.get_string("name") == wanted) {
                matched = &item;
                break;
            }
        }
    }

    std::string body_str = trim_ascii(payload.get_string("body"));
    std::string asset_url;
    size_t asset_size = 0;
    std::string asset_sha;

    if (matched) {
        std::string raw_url = trim_ascii(matched->get_string("browser_download_url"));
        i64 raw_size = static_cast<i64>(matched->get_number("size", 0.0));
        if (!is_valid_release_download_url(raw_url, repo)) {
            out_reason = "untrusted_url";
            return false;
        }
        if (raw_size < 0 || static_cast<size_t>(raw_size) > kMaxUpdatePackageBytes) {
            out_reason = "invalid_size";
            return false;
        }
        asset_url = raw_url;
        asset_size = static_cast<size_t>(raw_size);
        asset_sha = extract_sha256_from_asset_or_body(*matched, body_str, wanted);
    }

    std::string html_url = trim_ascii(payload.get_string("html_url"));
    std::string repo_prefix = "https://github.com/" + repo;
    if (html_url.rfind(repo_prefix, 0) != 0) {
        html_url = repo_prefix + "/releases";
    }

    out_info.tag_name = tag_name;
    out_info.version = ver_str;
    out_info.release_name = trim_ascii(payload.get_string("name", tag_name));
    out_info.body = body_str;
    out_info.html_url = html_url;
    out_info.published_at = trim_ascii(payload.get_string("published_at"));
    out_info.asset_name = wanted;
    out_info.asset_url = asset_url;
    out_info.asset_size = asset_size;
    out_info.asset_sha256 = asset_sha;
    out_reason = "ok";
    return true;
}

std::string build_windows_updater_script(
    u32 target_pid,
    const std::filesystem::path& staged_root,
    const std::filesystem::path& install_root,
    const std::filesystem::path& rollback_root,
    const std::string& exe_name,
    const std::filesystem::path& log_path) {
    std::filesystem::path eff_log = log_path.empty() ? (install_root / "data" / "updates" / "last_update.log") : log_path;
    std::ostringstream ss;
    ss << "$ErrorActionPreference = 'Stop'\n"
       << "$TargetPid = " << target_pid << "\n"
       << "$StagedRoot = '" << ps_escape_single_quotes(staged_root) << "'\n"
       << "$InstallRoot = '" << ps_escape_single_quotes(install_root) << "'\n"
       << "$RollbackRoot = '" << ps_escape_single_quotes(rollback_root) << "'\n"
       << "$ExePath = '" << ps_escape_single_quotes(install_root / exe_name) << "'\n"
       << "$LogPath = '" << ps_escape_single_quotes(eff_log) << "'\n"
       << R"(
function Write-Log([string]$Msg) {
    $line = ('[' + (Get-Date -Format 's') + '] ' + $Msg)
    Add-Content -Path $LogPath -Value $line -Encoding UTF8 -ErrorAction SilentlyContinue
}

function Invoke-WithRetry([scriptblock]$Action, [int]$Attempts = 20, [int]$DelayMs = 250) {
    for ($n = 1; $n -le $Attempts; $n++) {
        try {
            & $Action
            return
        } catch {
            if ($n -ge $Attempts) { throw }
            Start-Sleep -Milliseconds $DelayMs
        }
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path $LogPath) | Out-Null
Write-Log "Waiting for PID $TargetPid to exit..."
for ($i = 0; $i -lt 240; $i++) {
    if (-not (Get-Process -Id $TargetPid -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Milliseconds 250
}
Start-Sleep -Milliseconds 500

try {
    New-Item -ItemType Directory -Force -Path $RollbackRoot | Out-Null
    $Preserved = @('config', 'data')
    Get-ChildItem -Force $InstallRoot | ForEach-Object {
        if ($Preserved -contains $_.Name) { return }
        $item = $_
        $dest = Join-Path $RollbackRoot $item.Name
        Invoke-WithRetry { Move-Item -Force $item.FullName $dest }
    }
    Get-ChildItem -Force $StagedRoot | ForEach-Object {
        if ($Preserved -contains $_.Name) { return }
        $dest = Join-Path $InstallRoot $_.Name
        Copy-Item -Recurse -Force $_.FullName $dest
    }
    foreach ($dirName in $Preserved) {
        $srcDir = Join-Path $StagedRoot $dirName
        $dstDir = Join-Path $InstallRoot $dirName
        if (Test-Path $srcDir) {
            if (-not (Test-Path $dstDir)) {
                Copy-Item -Recurse -Force $srcDir $dstDir
            }
        }
    }
    Write-Log "Update applied cleanly; launching $ExePath"
    Remove-Item -Recurse -Force $RollbackRoot -ErrorAction SilentlyContinue
    $StagedParent = Split-Path $StagedRoot -Parent
    if ($StagedParent -and (Split-Path $StagedParent -Leaf) -eq 'staged') {
        Remove-Item -Recurse -Force $StagedParent -ErrorAction SilentlyContinue
    } elseif (Test-Path $StagedRoot) {
        Remove-Item -Recurse -Force $StagedRoot -ErrorAction SilentlyContinue
    }
    if (Test-Path $ExePath) {
        Start-Process -FilePath $ExePath -WorkingDirectory $InstallRoot | Out-Null
    }
} catch {
    Write-Log ("Update failed; rolling back: " + $_.Exception.Message)
    Get-ChildItem -Force $RollbackRoot -ErrorAction SilentlyContinue | ForEach-Object {
        $item = $_
        $dest = Join-Path $InstallRoot $item.Name
        if (Test-Path $dest) { Remove-Item -Recurse -Force $dest -ErrorAction SilentlyContinue }
        Invoke-WithRetry { Move-Item -Force $item.FullName $dest } -Attempts 5 -DelayMs 200
    }
    if (Test-Path $ExePath) {
        Start-Process -FilePath $ExePath -WorkingDirectory $InstallRoot | Out-Null
    }
}
)";
    return ss.str();
}

UpdateManager::UpdateManager(
    const std::string& local_version,
    const std::string& repo,
    const std::filesystem::path& config_path,
    const std::filesystem::path& updates_dir,
    const std::filesystem::path& install_root,
    int is_standalone_override,
    HttpCallable http_fn,
    const std::string& target_asset,
    std::function<void()> request_quit_cb)
    : repo_(repo.empty() ? kGithubRepo : repo),
      config_path_(config_path.empty() ? default_services_config_path() : config_path),
      updates_dir_(updates_dir.empty() ? default_updates_dir() : updates_dir),
      install_root_(install_root.empty() ? find_project_or_exe_root() : install_root),
      http_(http_fn ? std::move(http_fn) : [](const std::string& m, const std::string& u, const HttpHeaders& h,
                                              const std::string& b, double t) {
          return http_execute(m, u, h, b, t);
      }),
      target_asset_(target_asset.empty() ? get_target_asset_name() : target_asset),
      request_quit_cb_(std::move(request_quit_cb)) {
    std::error_code ec;
    if (is_standalone_override >= 0) {
        is_standalone_ = (is_standalone_override != 0);
    } else {
#ifdef _WIN32
        // Standalone C++ deployment has HeroesLore3.exe beside assets/ and lacks CMakeLists.txt
        is_standalone_ = std::filesystem::exists(install_root_ / "HeroesLore3.exe", ec) &&
                         !std::filesystem::exists(install_root_ / "CMakeLists.txt", ec);
#elif defined(__ANDROID__)
        is_standalone_ = true;
#else
        is_standalone_ = !std::filesystem::exists(install_root_ / "CMakeLists.txt", ec);
#endif
    }

    state_.local_version = local_version.empty() ? kPortVersion : local_version;
    state_.repo = repo_;
    state_.startup_check = load_services_startup_check(config_path_);
}

UpdateManager::~UpdateManager() {
    shutdown();
}

void UpdateManager::shutdown() {
    stop_flag_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard<std::mutex> lk(mutex_);
    worker_running_ = false;
}

UpdaterState UpdateManager::snapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return state_;
}

bool UpdateManager::get_startup_check() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return state_.startup_check;
}

bool UpdateManager::set_startup_check(bool enabled) {
    save_services_startup_check(enabled, config_path_);
    std::lock_guard<std::mutex> lk(mutex_);
    state_.startup_check = enabled;
    return enabled;
}

bool UpdateManager::toggle_startup_check() {
    return set_startup_check(!get_startup_check());
}

bool UpdateManager::consume_pending_notice(UpdateReleaseInfo& out_rel) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (state_.pending_notice && state_.has_release) {
        state_.pending_notice = false;
        out_rel = state_.release;
        return true;
    }
    return false;
}

bool UpdateManager::is_busy_locked() const {
    return worker_running_;
}

bool UpdateManager::maybe_start_startup_check() {
    if (!get_startup_check()) return false;
    return start_check(true);
}

bool UpdateManager::start_check(bool startup) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (is_busy_locked()) return false;
        state_.phase = UpdatePhase::Checking;
        state_.error_detail.clear();
        if (!startup) state_.pending_notice = false;
        worker_running_ = true;
    }
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this, startup]() {
        run_check(startup);
        std::lock_guard<std::mutex> lk(mutex_);
        worker_running_ = false;
    });
    return true;
}

void UpdateManager::fetch_latest_sync(UpdatePhase& out_phase, UpdateReleaseInfo& out_info, bool& out_has_info,
                                      std::string& out_detail) {
    out_has_info = false;
    out_detail.clear();
    HttpResponse resp = http_(
        "GET",
        api_url(),
        {
            {"Accept", "application/vnd.github+json"},
            {"User-Agent", "HeroesLore3Recomp/" + state_.local_version},
        },
        "",
        15.0);

    if (!resp.error.empty()) {
        out_phase = UpdatePhase::Error;
        out_detail = resp.error;
        return;
    }
    if (resp.status_code == 404) {
        out_phase = UpdatePhase::NoReleaseYet;
        return;
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        out_phase = UpdatePhase::Error;
        out_detail = "HTTP " + std::to_string(resp.status_code);
        return;
    }
    if (resp.body.size() > kMaxUpdateJsonBytes) {
        out_phase = UpdatePhase::Error;
        out_detail = "Response payload exceeds safety limit";
        return;
    }

    std::string reason;
    if (!parse_latest_release_payload(resp.body, out_info, reason, target_asset_, repo_)) {
        if (reason == "no_release_yet") {
            out_phase = UpdatePhase::NoReleaseYet;
        } else {
            out_phase = UpdatePhase::Error;
            out_detail = reason;
        }
        return;
    }

    out_has_info = true;
    out_phase = is_newer_version(out_info.tag_name, state_.local_version)
                    ? UpdatePhase::UpdateAvailable
                    : UpdatePhase::UpToDate;
}

void UpdateManager::run_check(bool startup) {
    int attempts = startup ? kStartupCheckAttempts : 1;
    UpdatePhase phase = UpdatePhase::Error;
    UpdateReleaseInfo info;
    bool has_info = false;
    std::string detail;

    for (int idx = 0; idx < attempts; ++idx) {
        if (stop_flag_.load()) return;
        fetch_latest_sync(phase, info, has_info, detail);
        if (phase != UpdatePhase::Error) break;
        if (idx + 1 < attempts) {
            for (int ms = 0; ms < 800 * (idx + 1); ms += 100) {
                if (stop_flag_.load()) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    std::string ts = now_local_timestamp_str();
    std::lock_guard<std::mutex> lk(mutex_);
    state_.last_checked_at = ts;
    if (startup && phase == UpdatePhase::Error) {
        state_.phase = UpdatePhase::Idle;
        state_.error_detail.clear();
        return;
    }
    state_.phase = phase;
    state_.has_release = has_info;
    if (has_info) state_.release = info;
    state_.error_detail = detail;
    if (startup && phase == UpdatePhase::UpdateAvailable) {
        state_.pending_notice = true;
    }
}

bool UpdateManager::start_download() {
    UpdateReleaseInfo rel;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (is_busy_locked()) return false;
        if (!state_.has_release || !is_valid_release_download_url(state_.release.asset_url, repo_)) {
            state_.phase = UpdatePhase::Error;
            state_.error_detail = "No valid download asset found";
            return false;
        }
        rel = state_.release;
        state_.phase = UpdatePhase::Downloading;
        state_.downloaded_bytes = 0;
        state_.total_bytes = rel.asset_size;
        state_.error_detail.clear();
        worker_running_ = true;
    }
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this, rel]() {
        run_download(rel);
        std::lock_guard<std::mutex> lk(mutex_);
        worker_running_ = false;
    });
    return true;
}

void UpdateManager::run_download(UpdateReleaseInfo rel) {
    std::error_code ec;
    std::filesystem::create_directories(updates_dir_, ec);
    auto part_path = updates_dir_ / (rel.asset_name + ".download");
    auto final_path = updates_dir_ / rel.asset_name;

    std::string final_url;
    std::string actual_sha;
    size_t written = 0;
    std::string err;

    bool ok = http_download_file(
        rel.asset_url,
        part_path,
        "HeroesLore3Recomp/" + state_.local_version,
        kMaxUpdatePackageBytes,
        [this](size_t done, size_t total) {
            std::lock_guard<std::mutex> lk(mutex_);
            state_.downloaded_bytes = done;
            if (total > 0 && state_.total_bytes == 0) state_.total_bytes = total;
            return !stop_flag_.load();
        },
        &stop_flag_,
        final_url,
        actual_sha,
        written,
        err);

    if (ok && written == 0) {
        ok = false;
        err = "Downloaded empty file";
    }
    if (ok && rel.asset_size > 0 && written != rel.asset_size) {
        ok = false;
        err = "Size mismatch: expected " + std::to_string(rel.asset_size) +
              ", got " + std::to_string(written);
    }
    if (ok && !rel.asset_sha256.empty() && to_lower(actual_sha) != to_lower(rel.asset_sha256)) {
        ok = false;
        err = "SHA-256 verification failed";
    }
    if (ok && to_lower(rel.asset_name).size() >= 4 &&
        to_lower(rel.asset_name).rfind(".zip") == rel.asset_name.size() - 4) {
        Bytes hdr;
        if (!read_file_bytes(part_path, hdr) || hdr.size() < 2 || hdr[0] != 'P' || hdr[1] != 'K') {
            ok = false;
            err = "Downloaded file is not a valid ZIP archive";
        }
    }

    if (!ok) {
        std::filesystem::remove(part_path, ec);
        std::lock_guard<std::mutex> lk(mutex_);
        state_.phase = UpdatePhase::Error;
        state_.error_detail = err;
        return;
    }

    std::filesystem::remove(final_path, ec);
    std::filesystem::rename(part_path, final_path, ec);
    if (ec) {
        std::filesystem::remove(part_path, ec);
        std::lock_guard<std::mutex> lk(mutex_);
        state_.phase = UpdatePhase::Error;
        state_.error_detail = "Failed committing downloaded package: " + ec.message();
        return;
    }

    std::lock_guard<std::mutex> lk(mutex_);
    state_.downloaded_path = final_path;
    state_.downloaded_bytes = written;
    state_.total_bytes = written;
    state_.phase = UpdatePhase::ReadyToInstall;
}

bool UpdateManager::install_ready_update(std::string* out_msg) {
    std::filesystem::path dl_path;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        dl_path = state_.downloaded_path;
    }
    std::error_code ec;
    if (dl_path.empty() || !std::filesystem::exists(dl_path, ec)) {
        std::lock_guard<std::mutex> lk(mutex_);
        state_.phase = UpdatePhase::Error;
        state_.error_detail = "Update package not found";
        if (out_msg) *out_msg = state_.error_detail;
        return false;
    }

    std::string ext = to_lower(dl_path.extension().string());
    if (ext == ".zip") {
        auto staged_dir = updates_dir_ / "staged";
        std::filesystem::remove_all(staged_dir, ec);
        ZipArchive zf;
        if (!zf.open_from_file(dl_path)) {
            std::lock_guard<std::mutex> lk(mutex_);
            state_.phase = UpdatePhase::Error;
            state_.error_detail = "Failed opening update ZIP";
            if (out_msg) *out_msg = state_.error_detail;
            return false;
        }
        std::string zerr;
        if (!zf.extract_all_safe(staged_dir, zerr)) {
            std::lock_guard<std::mutex> lk(mutex_);
            state_.phase = UpdatePhase::Error;
            state_.error_detail = zerr;
            if (out_msg) *out_msg = state_.error_detail;
            return false;
        }

        auto staged_root = find_extracted_exe_root(staged_dir);
        {
            std::lock_guard<std::mutex> lk(mutex_);
            state_.staged_root = staged_root;
        }

#ifdef _WIN32
        if (is_standalone_) {
            std::time_t t = std::time(nullptr);
            std::tm tm_loc{};
            localtime_s(&tm_loc, &t);
            char stamp[32];
            std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d_%02d%02d%02d",
                          tm_loc.tm_year + 1900, tm_loc.tm_mon + 1, tm_loc.tm_mday,
                          tm_loc.tm_hour, tm_loc.tm_min, tm_loc.tm_sec);
            auto rollback_dir = updates_dir_ / (std::string("rollback_") + stamp);
            auto script_path = updates_dir_ / "update_windows.ps1";
            std::string script_text = build_windows_updater_script(
                GetCurrentProcessId(), staged_root, install_root_, rollback_dir,
                "HeroesLore3.exe", updates_dir_ / "last_update.log");

            // Write UTF-8 with BOM for PowerShell 5.1 compatibility.
            Bytes bom_script;
            bom_script.reserve(script_text.size() + 3);
            bom_script.push_back(0xEFu);
            bom_script.push_back(0xBBu);
            bom_script.push_back(0xBFu);
            bom_script.insert(bom_script.end(), script_text.begin(), script_text.end());
            if (!write_file_bytes(script_path, bom_script.data(), bom_script.size())) {
                std::lock_guard<std::mutex> lk(mutex_);
                state_.phase = UpdatePhase::Error;
                state_.error_detail = "Failed writing Windows updater script";
                if (out_msg) *out_msg = state_.error_detail;
                return false;
            }

            std::wstring wscript = script_path.wstring();
            std::wstring cmdline =
                L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + wscript + L"\"";
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            std::wstring workdir = updates_dir_.wstring();
            // Do not set DETACHED_PROCESS alongside CREATE_NO_WINDOW.
            BOOL ok = CreateProcessW(
                nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, workdir.c_str(), &si, &pi);
            if (!ok) {
                std::lock_guard<std::mutex> lk(mutex_);
                state_.phase = UpdatePhase::Error;
                state_.error_detail = "Failed launching PowerShell updater (" + std::to_string(GetLastError()) + ")";
                if (out_msg) *out_msg = state_.error_detail;
                return false;
            }
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            {
                std::lock_guard<std::mutex> lk(mutex_);
                state_.phase = UpdatePhase::Installing;
            }
            if (request_quit_cb_) request_quit_cb_();
            if (out_msg) *out_msg = staged_root.string();
            return true;
        }
#endif

        {
            std::lock_guard<std::mutex> lk(mutex_);
            state_.phase = UpdatePhase::ManualStaged;
        }
        if (out_msg) *out_msg = staged_root.string();
        return true;
    }

#ifdef __ANDROID__
    if (ext == ".apk") {
        if (!install_android_apk(dl_path.u8string())) {
            std::lock_guard<std::mutex> lk(mutex_);
            state_.phase = UpdatePhase::Error;
            state_.error_detail = "Failed launching Android APK installer";
            if (out_msg) *out_msg = state_.error_detail;
            return false;
        }
        {
            std::lock_guard<std::mutex> lk(mutex_);
            state_.phase = UpdatePhase::ManualStaged;
        }
        if (out_msg) *out_msg = dl_path.string();
        return true;
    }
#elif !defined(_WIN32)
    if (ext == ".appimage") {
        ::chmod(dl_path.c_str(), 0755);
    }
#endif

    {
        std::lock_guard<std::mutex> lk(mutex_);
        state_.phase = UpdatePhase::ManualStaged;
    }
    if (out_msg) *out_msg = dl_path.string();
    return true;
}

}  // namespace hl3

