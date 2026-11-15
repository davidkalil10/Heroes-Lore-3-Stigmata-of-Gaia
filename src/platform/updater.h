#pragma once

#include "../core/common.h"
#include "net_http.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace hl3 {

inline constexpr bool kDefaultStartupUpdateCheck = true;
inline constexpr size_t kMaxUpdateJsonBytes = 1u * 1024u * 1024u;
inline constexpr size_t kMaxUpdatePackageBytes = 256u * 1024u * 1024u;
inline constexpr int kStartupCheckAttempts = 3;

enum class UpdatePhase {
    Idle,
    Checking,
    UpToDate,
    UpdateAvailable,
    NoReleaseYet,
    Error,
    Downloading,
    ReadyToInstall,
    Installing,
    ManualStaged,
};

struct UpdateReleaseInfo {
    std::string tag_name;
    std::string version;
    std::string release_name;
    std::string body;
    std::string html_url;
    std::string published_at;
    std::string asset_name;
    std::string asset_url;
    size_t asset_size = 0;
    std::string asset_sha256;
};

struct UpdaterState {
    UpdatePhase phase = UpdatePhase::Idle;
    std::string local_version = kPortVersion;
    std::string original_version = kOriginalVersion;
    std::string repo = kGithubRepo;
    bool startup_check = kDefaultStartupUpdateCheck;
    bool has_release = false;
    UpdateReleaseInfo release;
    std::string error_detail;
    size_t downloaded_bytes = 0;
    size_t total_bytes = 0;
    std::filesystem::path downloaded_path;
    std::filesystem::path staged_root;
    bool pending_notice = false;
    std::string last_checked_at;
};

std::filesystem::path default_services_config_path();
std::filesystem::path default_updates_dir();

bool load_services_startup_check(const std::filesystem::path& path = {});
std::filesystem::path save_services_startup_check(bool enabled, const std::filesystem::path& path = {});

std::optional<std::array<int, 3>> parse_semver(const std::string& version_or_tag);
bool is_newer_version(const std::string& remote_tag, const std::string& local_version = kPortVersion);
bool is_valid_release_download_url(const std::string& url, const std::string& repo = kGithubRepo);

bool parse_latest_release_payload(
    const std::string& json_body,
    UpdateReleaseInfo& out_info,
    std::string& out_reason,
    const std::string& target_asset = {},
    const std::string& repo = kGithubRepo);

std::string build_windows_updater_script(
    u32 target_pid,
    const std::filesystem::path& staged_root,
    const std::filesystem::path& install_root,
    const std::filesystem::path& rollback_root,
    const std::string& exe_name = "HeroesLore3.exe",
    const std::filesystem::path& log_path = {});

std::string normalize_switch_nro_path(const std::string& raw_exe_path);

bool install_portable_nro_update(
    const std::filesystem::path& package_path,
    const std::filesystem::path& target_path,
    size_t expected_size,
    const std::string& expected_sha256,
    const std::function<void()>& before_swap,
    std::string& out_err);

class UpdateManager {
public:
    UpdateManager(
        const std::string& local_version = kPortVersion,
        const std::string& repo = kGithubRepo,
        const std::filesystem::path& config_path = {},
        const std::filesystem::path& updates_dir = {},
        const std::filesystem::path& install_root = {},
        int is_standalone_override = -1,
        HttpCallable http_fn = {},
        const std::string& target_asset = {},
        std::function<void()> request_quit_cb = {},
        std::string self_exe_path = {},
        std::function<void()> before_swap_cb = {});
    ~UpdateManager();

    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;

    std::string releases_url() const { return "https://github.com/" + repo_ + "/releases"; }
    std::string api_url() const { return "https://api.github.com/repos/" + repo_ + "/releases/latest"; }

    UpdaterState snapshot() const;
    bool get_startup_check() const;
    bool set_startup_check(bool enabled);
    bool toggle_startup_check();

    bool consume_pending_notice(UpdateReleaseInfo& out_rel);
    bool maybe_start_startup_check();
    bool start_check(bool startup = false);
    bool start_download();
    bool install_ready_update(std::string* out_msg = nullptr);
    void shutdown();

private:
    bool is_busy_locked() const;
    void fetch_latest_sync(UpdatePhase& out_phase, UpdateReleaseInfo& out_info, bool& out_has_info,
                           std::string& out_detail);
    void run_check(bool startup);
    void run_download(UpdateReleaseInfo rel);

    std::string repo_;
    std::filesystem::path config_path_;
    std::filesystem::path updates_dir_;
    std::filesystem::path install_root_;
    bool is_standalone_ = false;
    HttpCallable http_;
    std::string target_asset_;
    std::function<void()> request_quit_cb_;
    std::string self_exe_path_;
    std::function<void()> before_swap_cb_;

    mutable std::mutex mutex_;
    std::atomic<bool> stop_flag_{false};
    std::thread worker_;
    bool worker_running_ = false;
    UpdaterState state_;
};

}  // namespace hl3

