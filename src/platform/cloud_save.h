#pragma once

#include "../core/common.h"
#include "net_http.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace hl3 {

inline constexpr const char* kCloudAppTag = "heroes_lore_3";
inline constexpr int kCloudFormatVersion = 1;
inline constexpr const char* kCloudFolderName = "Heroes Lore III - Stigmata of Gaia";
inline constexpr const char* kCloudFileName = "heroes_lore_3_save.json";
inline constexpr const char* kDriveScope = "https://www.googleapis.com/auth/drive.file";
inline constexpr const char* kOAuthDeviceUrl = "https://oauth2.googleapis.com/device/code";
inline constexpr const char* kOAuthTokenUrl = "https://oauth2.googleapis.com/token";
inline constexpr const char* kDriveFilesUrl = "https://www.googleapis.com/drive/v3/files";
inline constexpr const char* kDriveUploadUrl = "https://www.googleapis.com/upload/drive/v3/files";

inline constexpr size_t kMaxBackupFiles = 64;
inline constexpr size_t kMaxBackupBytes = 16u * 1024u * 1024u;
inline constexpr size_t kMaxJsonBytes = 32u * 1024u * 1024u;

std::string get_google_client_id();
std::string get_google_client_secret();

std::filesystem::path default_data_root();
std::filesystem::path default_auth_path();
std::string cloud_platform_name();
std::string format_iso_local(const std::string& iso_str);
std::string format_epoch_local(double ts);

bool is_safe_save_path(const std::string& rel);

struct LocalSaveInfo {
    bool exists = false;
    int file_count = 0;
    size_t total_bytes = 0;
    int slot_count = 0;
    bool has_autosave = false;
    std::string updated_local;
    std::string platforms_summary;
};

struct UnpackMeta {
    std::string platform;
    std::string timestamp;
    std::string summary;
};

using SaveFileMap = std::map<std::string, Bytes>;

bool collect_local_saves(const std::filesystem::path& data_root, SaveFileMap& out_files, std::string* err = nullptr);
LocalSaveInfo inspect_local_saves(const std::filesystem::path& data_root);
std::string build_save_summary(const LocalSaveInfo& info);

bool pack_saves(const SaveFileMap& files, const std::string& platform, const std::string& summary,
                std::string& out_json, std::string* err = nullptr);
bool unpack_saves(const std::string& text, SaveFileMap& out_files, UnpackMeta& out_meta,
                  std::string* err = nullptr);
bool restore_saves(const std::filesystem::path& data_root, const SaveFileMap& files,
                   std::filesystem::path& out_backup_dir, std::string* err = nullptr);

struct CloudStatus {
    bool connected = false;
    bool busy = false;
    bool authorizing = false;
    bool backup_exists = false;
    std::string backup_date;
    std::string backup_date_local;
    std::string backup_summary;
    std::string backup_platform;
    std::string user_code;
    std::string verification_url;
    std::string message_key = "cloud.msg.ready_disconnected";
    std::string message_detail;
    bool restore_ready = false;
    bool last_op_ok = true;
};

class CloudSaveManager {
public:
    using ClockFn = std::function<double()>;

    CloudSaveManager(
        const std::filesystem::path& data_root = {},
        const std::filesystem::path& auth_path = {},
        HttpCallable http_fn = {},
        ClockFn clock_fn = {});
    ~CloudSaveManager();

    CloudSaveManager(const CloudSaveManager&) = delete;
    CloudSaveManager& operator=(const CloudSaveManager&) = delete;

    const std::filesystem::path& data_root() const { return data_root_; }
    const std::filesystem::path& auth_path() const { return auth_path_; }

    CloudStatus get_status() const;
    LocalSaveInfo get_local_info() const;

    bool start_login();
    void cancel_login();
    bool refresh_backup();
    bool upload_backup(const std::function<void()>& flush_before_read = {});
    bool restore_backup();
    void disconnect();

    bool consume_pending_restore(std::filesystem::path* out_backup = nullptr);
    void wait_idle();

private:
    void load_tokens();
    void save_tokens();
    void update_status(const std::function<void(CloudStatus&)>& fn);
    bool launch_worker(std::function<void()> op, const char* initial_msg_key = nullptr, bool authorizing = false);

    bool parse_json_resp(const HttpResponse& resp, class JsonValue& out_doc, std::string& err);
    HttpHeaders auth_headers(bool json_content = false) const;
    bool ensure_token(std::string& err);
    bool find_folder(bool create, std::string& out_folder_id, std::string& err);
    bool query_backup_sync(bool& out_found, std::string& err);

    std::filesystem::path data_root_;
    std::filesystem::path auth_path_;
    HttpCallable http_;
    ClockFn clock_;

    mutable std::mutex mutex_;
    std::mutex cancel_mutex_;
    std::condition_variable cancel_cv_;
    std::atomic<bool> cancel_req_{false};
    std::atomic<bool> shutting_down_{false};
    std::thread worker_;

    std::string access_token_;
    std::string refresh_token_;
    i64 token_expiry_ = 0;
    std::string folder_id_;
    std::string file_id_;
    std::string file_version_;
    SaveFileMap pending_restore_;
    bool has_pending_restore_ = false;
    CloudStatus status_;
};

}  // namespace hl3

