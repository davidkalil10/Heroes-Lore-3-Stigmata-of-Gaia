#include "cloud_save.h"
#include "../core/bundle.h"
#include "../core/json_mini.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace hl3 {

namespace {

std::string xor_decode_bytes(const u8* data, size_t len, u8 key = 0x5Au) {
    std::string out(len, '\0');
    for (size_t i = 0; i < len; ++i) {
        out[i] = static_cast<char>(data[i] ^ key);
    }
    return out;
}

bool is_reserved_win_name(std::string_view stem_ascii) {
    std::string u;
    u.reserve(stem_ascii.size());
    for (char c : stem_ascii) {
        u.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (u == "CON" || u == "PRN" || u == "AUX" || u == "NUL") return true;
    if (u.size() == 4 && (u.rfind("COM", 0) == 0 || u.rfind("LPT", 0) == 0)) {
        return u[3] >= '1' && u[3] <= '9';
    }
    return false;
}

std::string to_lower_ascii(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string utc_now_iso8601() {
    std::time_t t = std::time(nullptr);
    std::tm tm_utc{};
#ifdef _WIN32
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                  tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    return std::string(buf);
}

double system_clock_sec() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

std::string escape_drive_query(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\'' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

}  // namespace

std::string get_google_client_id() {
    static constexpr u8 kEnc[] = {
        108, 104, 108, 99, 104, 98, 111, 110, 98, 107, 111, 107, 119, 107, 104, 98,
        59, 42, 50, 54, 59, 52, 46, 110, 104, 111, 54, 61, 109, 47, 57, 108, 55,
        106, 43, 53, 44, 48, 47, 108, 52, 63, 60, 41, 61, 116, 59, 42, 42, 41,
        116, 61, 53, 53, 61, 54, 63, 47, 41, 63, 40, 57, 53, 52, 46, 63, 52,
        46, 116, 57, 53, 55,
    };
    return xor_decode_bytes(kEnc, sizeof(kEnc));
}

std::string get_google_client_secret() {
    static constexpr u8 kEnc[] = {
        29, 21, 25, 9, 10, 2, 119, 45, 107, 28, 106, 56, 21, 48, 106, 99, 109,
        107, 44, 11, 98, 119, 119, 61, 23, 24, 48, 43, 106, 8, 25, 109, 104,
        45, 10,
    };
    return xor_decode_bytes(kEnc, sizeof(kEnc));
}

std::filesystem::path default_data_root() {
    return find_writable_data_root() / "data";
}

std::filesystem::path default_auth_path() {
    return find_writable_data_root() / "config" / "cloud_auth.json";
}

std::string cloud_platform_name() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__SWITCH__)
    return "Switch";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Linux";
#endif
}

std::string format_epoch_local(double ts) {
    if (ts <= 0.0) return {};
    std::time_t t = static_cast<std::time_t>(ts);
    std::tm tm_loc{};
#ifdef _WIN32
    if (localtime_s(&tm_loc, &t) != 0) return {};
#else
    if (!localtime_r(&t, &tm_loc)) return {};
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
                  tm_loc.tm_year + 1900, tm_loc.tm_mon + 1, tm_loc.tm_mday,
                  tm_loc.tm_hour, tm_loc.tm_min);
    return std::string(buf);
}

std::time_t utc_tm_to_time_t(int y, int mo, int d, int hh, int mm, int ss) {
    // Howard Hinnant civil-to-days formula: portable UTC epoch without timegm/_mkgmtime.
    const int64_t yr = static_cast<int64_t>(y) - (mo <= 2 ? 1 : 0);
    const int64_t era = (yr >= 0 ? yr : yr - 399) / 400;
    const int64_t yoe = yr - era * 400;
    const int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = era * 146097 + doe - 719468;
    return static_cast<std::time_t>(days * 86400 + hh * 3600 + mm * 60 + ss);
}

std::string format_iso_local(const std::string& iso_str) {
    std::string raw = trim_ascii(iso_str);
    if (raw.empty()) return {};

    int y = 0, mo = 0, d = 0, hh = 0, mm = 0, ss = 0;
    if (std::sscanf(raw.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &hh, &mm, &ss) >= 5) {
        const std::time_t utc_t = utc_tm_to_time_t(y, mo, d, hh, mm, ss);
        if (utc_t > 0 && (!raw.empty() && raw.back() == 'Z')) {
            return format_epoch_local(static_cast<double>(utc_t));
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", y, mo, d, hh, mm);
        return std::string(buf);
    }
    return raw.size() > 16 ? raw.substr(0, 16) : raw;
}

bool is_safe_save_path(const std::string& name) {
    if (name.empty() || name.size() >= 240) return false;
    if (name.find('\\') != std::string::npos || name.front() == '/' || name.back() == '/') return false;

    std::vector<std::string> parts;
    size_t start = 0;
    while (start < name.size()) {
        size_t slash = name.find('/', start);
        if (slash == std::string::npos) {
            parts.push_back(name.substr(start));
            break;
        }
        parts.push_back(name.substr(start, slash - start));
        start = slash + 1;
    }
    if (parts.size() < 2) return false;
    if (parts[0] != "internal" && parts[0] != "external") return false;

    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& part = parts[i];
        if (part.empty() || part.front() == '.' || part.back() == '.' || part.back() == ' ') return false;
        for (unsigned char ch : part) {
            if (ch < 32 || ch > 126) return false;
            if (ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' ||
                ch == '<' || ch == '>' || ch == '|') {
                return false;
            }
        }
        size_t dot = part.find('.');
        std::string_view stem(part.data(), dot == std::string::npos ? part.size() : dot);
        if (is_reserved_win_name(stem)) return false;
    }
    return true;
}

bool collect_local_saves(const std::filesystem::path& data_root, SaveFileMap& out_files, std::string* err) {
    out_files.clear();
    size_t total = 0;
    std::error_code ec;

    for (const char* group : {"internal", "external"}) {
        std::filesystem::path base_dir = data_root / group;
        if (std::filesystem::is_symlink(base_dir, ec)) {
            if (err) *err = std::string("Save directory is a symlink: ") + group;
            return false;
        }
        if (!std::filesystem::exists(base_dir, ec) || !std::filesystem::is_directory(base_dir, ec)) {
            continue;
        }
        std::filesystem::recursive_directory_iterator it(
            base_dir, std::filesystem::directory_options::none, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; it != end; it.increment(ec)) {
            if (ec) continue;
            const auto& entry = *it;
            std::string fname = entry.path().filename().string();
            if (!fname.empty() && fname[0] == '.') {
                if (entry.is_directory(ec)) it.disable_recursion_pending();
                continue;
            }
            if (entry.is_symlink(ec)) {
                if (err) *err = "Save symlinks are unsupported: " + fname;
                return false;
            }
            if (entry.is_directory(ec)) continue;
            if (!entry.is_regular_file(ec)) continue;

            std::string rel = entry.path().lexically_relative(data_root).generic_string();
            if (!is_safe_save_path(rel)) {
                if (err) *err = "Unsupported save filename: " + rel;
                return false;
            }
            Bytes data;
            if (!read_file_bytes(entry.path(), data)) {
                if (err) *err = "Failed reading local save file: " + rel;
                return false;
            }
            total += data.size();
            if (out_files.size() >= kMaxBackupFiles || total > kMaxBackupBytes) {
                if (err) *err = "Save backup exceeds maximum limits";
                return false;
            }
            out_files[rel] = std::move(data);
        }
    }
    return true;
}

LocalSaveInfo inspect_local_saves(const std::filesystem::path& data_root) {
    LocalSaveInfo info;
    info.platforms_summary = cloud_platform_name();
    std::set<std::string> slots;
    double latest_mtime = 0.0;
    std::error_code ec;

    for (const char* group : {"internal", "external"}) {
        std::filesystem::path base_dir = data_root / group;
        if (std::filesystem::is_symlink(base_dir, ec) ||
            !std::filesystem::exists(base_dir, ec) ||
            !std::filesystem::is_directory(base_dir, ec)) {
            continue;
        }
        std::filesystem::recursive_directory_iterator it(
            base_dir, std::filesystem::directory_options::none, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; it != end; it.increment(ec)) {
            if (ec) continue;
            const auto& entry = *it;
            std::string fn = entry.path().filename().string();
            if (!fn.empty() && fn[0] == '.') {
                if (entry.is_directory(ec)) it.disable_recursion_pending();
                continue;
            }
            if (entry.is_symlink(ec) || entry.is_directory(ec) || !entry.is_regular_file(ec)) continue;

            std::string rel = entry.path().lexically_relative(data_root).generic_string();
            if (!is_safe_save_path(rel)) continue;

            auto fsize = entry.file_size(ec);
            if (!ec) {
                info.file_count += 1;
                info.total_bytes += static_cast<size_t>(fsize);
            }
            auto ftime = entry.last_write_time(ec);
            if (!ec) {
                auto sys_tp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
                double secs = std::chrono::duration<double>(sys_tp.time_since_epoch()).count();
                if (secs > latest_mtime) latest_mtime = secs;
            }
            if (fn.rfind("Hero3SlotSave_10_", 0) == 0 || fn.rfind("Hero3GameSave_10_", 0) == 0) {
                size_t us = fn.rfind('_');
                if (us != std::string::npos) slots.insert(fn.substr(us + 1));
            } else if (fn.rfind("HL3_Autosave", 0) == 0) {
                info.has_autosave = true;
            }
        }
    }

    info.exists = (info.file_count > 0);
    info.slot_count = static_cast<int>(slots.size());
    info.updated_local = format_epoch_local(latest_mtime);
    return info;
}

std::string build_save_summary(const LocalSaveInfo& info) {
    double kb = static_cast<double>(info.total_bytes) / 1024.0;
    char buf[192];
    if (info.slot_count > 0) {
        std::snprintf(buf, sizeof(buf), "%s | %d slot(s) | %d files (%.1f KB)",
                      cloud_platform_name().c_str(), info.slot_count, info.file_count, kb);
    } else {
        std::snprintf(buf, sizeof(buf), "%s | %d files (%.1f KB)",
                      cloud_platform_name().c_str(), info.file_count, kb);
    }
    return std::string(buf);
}

bool pack_saves(const SaveFileMap& files, const std::string& platform, const std::string& summary,
                std::string& out_json, std::string* err) {
    if (files.empty()) {
        if (err) *err = "No local saves to upload";
        return false;
    }
    if (files.size() > kMaxBackupFiles) {
        if (err) *err = "Too many save files";
        return false;
    }

    size_t total = 0;
    std::set<std::string> seen_lower;
    JsonValue::Object encoded_files;
    for (const auto& [rel_path, raw] : files) {
        if (!is_safe_save_path(rel_path)) {
            if (err) *err = "Invalid save path: " + rel_path;
            return false;
        }
        std::string folded = to_lower_ascii(rel_path);
        if (!seen_lower.insert(folded).second) {
            if (err) *err = "Colliding save filename: " + rel_path;
            return false;
        }
        total += raw.size();
        if (total > kMaxBackupBytes) {
            if (err) *err = "Save backup too large";
            return false;
        }
        JsonValue::Object fobj;
        fobj["data"] = JsonValue::make_string(base64_encode(raw));
        fobj["sha256"] = JsonValue::make_string(sha256_hex(raw));
        encoded_files[rel_path] = JsonValue::make_object(std::move(fobj));
    }

    std::string plat = platform.empty() ? cloud_platform_name() : platform;
    std::string eff_sum = summary;
    if (eff_sum.empty()) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%s | %zu files (%.1f KB)",
                      plat.c_str(), encoded_files.size(), static_cast<double>(total) / 1024.0);
        eff_sum = buf;
    }

    JsonValue::Object root;
    root["app"] = JsonValue::make_string(kCloudAppTag);
    root["version"] = JsonValue::make_number(static_cast<double>(kCloudFormatVersion));
    root["package_id"] = JsonValue::make_string(kAndroidPackageId);
    root["switch_title_id"] = JsonValue::make_string(kSwitchTitleId);
    root["platform"] = JsonValue::make_string(plat);
    root["timestamp"] = JsonValue::make_string(utc_now_iso8601());
    root["summary"] = JsonValue::make_string(eff_sum);
    root["files"] = JsonValue::make_object(std::move(encoded_files));

    out_json = JsonValue::make_object(std::move(root)).dump(0);
    return true;
}

bool unpack_saves(const std::string& text, SaveFileMap& out_files, UnpackMeta& out_meta,
                  std::string* err) {
    out_files.clear();
    out_meta = {};
    if (text.size() > kMaxJsonBytes) {
        if (err) *err = "Backup too large or invalid encoding";
        return false;
    }

    JsonValue doc;
    std::string parse_err;
    if (!JsonValue::parse(text, doc, &parse_err) || !doc.is_object()) {
        if (err) *err = "Invalid backup JSON: " + parse_err;
        return false;
    }
    if (doc.get_string("app") != kCloudAppTag || doc.get_int("version") != kCloudFormatVersion) {
        if (err) *err = "Backup belongs to a different game or format";
        return false;
    }

    const JsonValue* files_val = doc.get("files");
    if (!files_val || !files_val->is_object() || files_val->as_object().empty()) {
        if (err) *err = "Empty or invalid backup files table";
        return false;
    }
    const auto& files_obj = files_val->as_object();
    if (files_obj.size() > kMaxBackupFiles) {
        if (err) *err = "Backup contains too many files";
        return false;
    }

    std::set<std::string> seen_lower;
    size_t total = 0;
    for (const auto& [rel_path, entry] : files_obj) {
        if (!is_safe_save_path(rel_path)) {
            if (err) *err = "Unsafe backup path: " + rel_path;
            return false;
        }
        std::string folded = to_lower_ascii(rel_path);
        if (!seen_lower.insert(folded).second) {
            if (err) *err = "Backup has colliding filenames: " + rel_path;
            return false;
        }
        if (!entry.is_object()) {
            if (err) *err = "Invalid file entry: " + rel_path;
            return false;
        }
        const JsonValue* data_v = entry.get("data");
        const JsonValue* sha_v = entry.get("sha256");
        if (!data_v || !data_v->is_string() || !sha_v || !sha_v->is_string()) {
            if (err) *err = "Missing file data or checksum: " + rel_path;
            return false;
        }
        const std::string& b64_text = data_v->as_string();
        Bytes raw;
        if (!base64_decode(b64_text, raw) || base64_encode(raw) != b64_text) {
            if (err) *err = "Invalid or noncanonical base64 payload: " + rel_path;
            return false;
        }
        if (to_lower_ascii(sha256_hex(raw)) != to_lower_ascii(sha_v->as_string())) {
            if (err) *err = "Backup checksum mismatch: " + rel_path;
            return false;
        }
        total += raw.size();
        if (total > kMaxBackupBytes) {
            if (err) *err = "Backup exceeds size limit";
            return false;
        }
        out_files[rel_path] = std::move(raw);
    }

    out_meta.platform = doc.get_string("platform");
    out_meta.timestamp = doc.get_string("timestamp");
    out_meta.summary = doc.get_string("summary");
    return true;
}

bool restore_saves(const std::filesystem::path& data_root, const SaveFileMap& files,
                   std::filesystem::path& out_backup_dir, std::string* err) {
    // Re-validate package end-to-end before touching disk
    std::string repacked;
    if (!pack_saves(files, cloud_platform_name(), {}, repacked, err)) return false;
    SaveFileMap checked_files;
    UnpackMeta meta;
    if (!unpack_saves(repacked, checked_files, meta, err)) return false;

    std::error_code ec;
    std::filesystem::create_directories(data_root, ec);
    for (const char* part : {"internal", "external", "save-backups"}) {
        auto p = data_root / part;
        if (std::filesystem::is_symlink(p, ec) ||
            (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec))) {
            if (err) *err = std::string("Unsafe local save directory: ") + part;
            return false;
        }
    }

    auto stamp = std::to_string(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    auto stage = data_root / ("restore-" + stamp);
    auto backup = data_root / "save-backups" / stamp;

    std::filesystem::create_directories(stage / "internal", ec);
    std::filesystem::create_directories(stage / "external", ec);

    for (const auto& [rel_path, content] : checked_files) {
        auto target = stage / std::filesystem::path(rel_path);
        if (!write_file_bytes(target, content.data(), content.size())) {
            std::filesystem::remove_all(stage, ec);
            if (err) *err = "Failed writing staged save: " + rel_path;
            return false;
        }
    }

    std::filesystem::create_directories(backup, ec);
    std::vector<std::string> old_moved;
    std::vector<std::string> new_moved;
    bool ok = true;
    for (const char* part : {"internal", "external"}) {
        auto live_dir = data_root / part;
        if (std::filesystem::exists(live_dir, ec)) {
            std::filesystem::rename(live_dir, backup / part, ec);
            if (ec) {
                ok = false;
                if (err) *err = "Failed backing up existing saves: " + ec.message();
                break;
            }
            old_moved.emplace_back(part);
        }
        std::filesystem::rename(stage / part, live_dir, ec);
        if (ec) {
            ok = false;
            if (err) *err = "Failed moving restored saves into place: " + ec.message();
            break;
        }
        new_moved.emplace_back(part);
    }

    if (!ok) {
        for (auto it = new_moved.rbegin(); it != new_moved.rend(); ++it) {
            if (std::filesystem::exists(data_root / *it, ec)) {
                std::filesystem::rename(data_root / *it, stage / *it, ec);
            }
        }
        for (auto it = old_moved.rbegin(); it != old_moved.rend(); ++it) {
            if (std::filesystem::exists(backup / *it, ec)) {
                std::filesystem::rename(backup / *it, data_root / *it, ec);
            }
        }
        std::filesystem::remove_all(stage, ec);
        return false;
    }

    std::filesystem::remove_all(stage, ec);
    out_backup_dir = backup;
    return true;
}

CloudSaveManager::CloudSaveManager(
    const std::filesystem::path& data_root,
    const std::filesystem::path& auth_path,
    HttpCallable http_fn,
    ClockFn clock_fn)
    : data_root_(data_root.empty() ? default_data_root() : data_root),
      auth_path_(auth_path.empty() ? default_auth_path() : auth_path),
      http_(http_fn ? std::move(http_fn) : [](const std::string& m, const std::string& u, const HttpHeaders& h,
                                              const std::string& b, double t) {
          return http_execute(m, u, h, b, t);
      }),
      clock_(clock_fn ? std::move(clock_fn) : system_clock_sec) {
    load_tokens();
}

CloudSaveManager::~CloudSaveManager() {
    shutting_down_ = true;
    cancel_req_ = true;
    cancel_cv_.notify_all();
    wait_idle();
}

void CloudSaveManager::load_tokens() {
    Bytes raw;
    if (!read_file_bytes(auth_path_, raw)) return;
    JsonValue doc;
    if (!JsonValue::parse(std::string(raw.begin(), raw.end()), doc) || !doc.is_object()) return;
    std::string rtok = doc.get_string("refresh_token");
    if (!rtok.empty()) {
        refresh_token_ = rtok;
        access_token_ = doc.get_string("access_token");
        token_expiry_ = static_cast<i64>(doc.get_number("expires_at", 0.0));
        status_.connected = true;
        status_.message_key = "cloud.msg.ready_connected";
    }
}

void CloudSaveManager::save_tokens() {
    JsonValue::Object obj;
    obj["app"] = JsonValue::make_string(kCloudAppTag);
    obj["refresh_token"] = JsonValue::make_string(refresh_token_);
    obj["access_token"] = JsonValue::make_string(access_token_);
    obj["expires_at"] = JsonValue::make_number(static_cast<double>(token_expiry_));
    std::string txt = JsonValue::make_object(std::move(obj)).dump(2);

    std::error_code ec;
    if (auth_path_.has_parent_path()) std::filesystem::create_directories(auth_path_.parent_path(), ec);
    auto tmp = auth_path_;
    tmp += ".tmp";
    if (write_file_bytes(tmp, txt.data(), txt.size())) {
        std::filesystem::rename(tmp, auth_path_, ec);
    }
}

CloudStatus CloudSaveManager::get_status() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return status_;
}

LocalSaveInfo CloudSaveManager::get_local_info() const {
    return inspect_local_saves(data_root_);
}

void CloudSaveManager::update_status(const std::function<void(CloudStatus&)>& fn) {
    std::lock_guard<std::mutex> lk(mutex_);
    fn(status_);
}

void CloudSaveManager::wait_idle() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool CloudSaveManager::launch_worker(std::function<void()> op) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (status_.busy) return false;
        status_.busy = true;
    }
    if (worker_.joinable()) worker_.join();
    cancel_req_ = false;
    worker_ = std::thread([this, func = std::move(op)]() mutable {
        func();
        update_status([](CloudStatus& s) { s.busy = false; });
    });
    return true;
}

bool CloudSaveManager::parse_json_resp(const HttpResponse& resp, JsonValue& out_doc, std::string& err) {
    if (!resp.error.empty()) {
        err = resp.error;
        return false;
    }
    std::string perr;
    if (!resp.body.empty()) {
        if (!JsonValue::parse(resp.body, out_doc, &perr)) {
            err = "Server returned HTTP " + std::to_string(resp.status_code);
            return false;
        }
    } else {
        out_doc = JsonValue::make_object({});
    }
    if (resp.status_code >= 200 && resp.status_code < 300) {
        return true;
    }
    std::string emsg;
    if (out_doc.is_object()) {
        const JsonValue* eo = out_doc.get("error");
        if (eo && eo->is_object()) {
            emsg = eo->get_string("message");
        } else if (eo && eo->is_string()) {
            emsg = out_doc.get_string("error_description");
            if (emsg.empty()) emsg = eo->as_string();
        }
    }
    err = emsg.empty() ? ("HTTP " + std::to_string(resp.status_code)) : emsg;
    return false;
}

HttpHeaders CloudSaveManager::auth_headers(bool json_content) const {
    HttpHeaders h = {{"Authorization", "Bearer " + access_token_}};
    if (json_content) h.emplace_back("Content-Type", "application/json; charset=UTF-8");
    return h;
}

bool CloudSaveManager::ensure_token(std::string& err) {
    if (refresh_token_.empty()) {
        err = "Connect Google Drive first";
        return false;
    }
    i64 now = static_cast<i64>(clock_());
    if (!access_token_.empty() && token_expiry_ > now + 60) {
        return true;
    }
    std::string cid = get_google_client_id();
    std::string csec = get_google_client_secret();
    std::string body = build_form_urlencoded({
        {"client_id", cid},
        {"client_secret", csec},
        {"refresh_token", refresh_token_},
        {"grant_type", "refresh_token"},
    });
    HttpResponse resp = http_("POST", kOAuthTokenUrl,
                              {{"Content-Type", "application/x-www-form-urlencoded"}},
                              body, 20.0);
    JsonValue j;
    if (!parse_json_resp(resp, j, err)) return false;
    access_token_ = j.get_string("access_token");
    token_expiry_ = static_cast<i64>(clock_()) + j.get_int("expires_in", 3600);
    save_tokens();
    return !access_token_.empty();
}

bool CloudSaveManager::find_folder(bool create, std::string& out_folder_id, std::string& err) {
    if (!folder_id_.empty()) {
        out_folder_id = folder_id_;
        return true;
    }
    std::string q = "name='" + escape_drive_query(kCloudFolderName) +
                    "' and mimeType='application/vnd.google-apps.folder' and trashed=false";
    std::string url = std::string(kDriveFilesUrl) + "?q=" + url_encode(q) + "&fields=files(id)&pageSize=1";
    JsonValue j;
    if (!parse_json_resp(http_("GET", url, auth_headers(), "", 20.0), j, err)) return false;
    const JsonValue* files = j.get("files");
    if (files && files->is_array() && !files->as_array().empty()) {
        folder_id_ = files->as_array()[0].get_string("id");
        out_folder_id = folder_id_;
        return true;
    }
    if (create) {
        JsonValue::Object req_obj;
        req_obj["name"] = JsonValue::make_string(kCloudFolderName);
        req_obj["mimeType"] = JsonValue::make_string("application/vnd.google-apps.folder");
        std::string body = JsonValue::make_object(std::move(req_obj)).dump(0);
        JsonValue j2;
        if (!parse_json_resp(http_("POST", kDriveFilesUrl, auth_headers(true), body, 20.0), j2, err)) {
            return false;
        }
        folder_id_ = j2.get_string("id");
    }
    out_folder_id = folder_id_;
    return true;
}

bool CloudSaveManager::query_backup_sync(bool& out_found, std::string& err) {
    out_found = false;
    if (!ensure_token(err)) return false;
    std::string folder;
    if (!find_folder(false, folder, err)) return false;

    file_id_.clear();
    file_version_.clear();
    JsonValue frec;
    bool has_rec = false;
    std::string esc_file = escape_drive_query(kCloudFileName);

    if (!folder.empty()) {
        std::string q = "name='" + esc_file + "' and '" + folder + "' in parents and trashed=false";
        std::string url = std::string(kDriveFilesUrl) + "?q=" + url_encode(q) +
                          "&fields=files(id,modifiedTime,description,version,parents)"
                          "&orderBy=modifiedTime%20desc&pageSize=1";
        JsonValue j;
        if (!parse_json_resp(http_("GET", url, auth_headers(), "", 20.0), j, err)) return false;
        const JsonValue* files = j.get("files");
        if (files && files->is_array() && !files->as_array().empty()) {
            frec = files->as_array()[0];
            has_rec = true;
        }
    }

    if (!has_rec) {
        std::string q_any = "name='" + esc_file + "' and trashed=false";
        std::string url_any = std::string(kDriveFilesUrl) + "?q=" + url_encode(q_any) +
                              "&fields=files(id,modifiedTime,description,version,parents)"
                              "&orderBy=modifiedTime%20desc&pageSize=1";
        JsonValue j_any;
        if (!parse_json_resp(http_("GET", url_any, auth_headers(), "", 20.0), j_any, err)) return false;
        const JsonValue* files_any = j_any.get("files");
        if (files_any && files_any->is_array() && !files_any->as_array().empty()) {
            frec = files_any->as_array()[0];
            has_rec = true;
            if (folder.empty()) {
                std::string dummy_err;
                find_folder(true, folder, dummy_err);
            }
            if (!folder.empty()) {
                std::vector<std::string> old_parents;
                bool has_target = false;
                if (const JsonValue* pa = frec.get("parents"); pa && pa->is_array()) {
                    for (const auto& pv : pa->as_array()) {
                        std::string ps = pv.as_string();
                        if (ps == folder) has_target = true;
                        old_parents.push_back(ps);
                    }
                }
                if (!has_target) {
                    std::string old_joined;
                    for (size_t i = 0; i < old_parents.size(); ++i) {
                        if (i > 0) old_joined.push_back(',');
                        old_joined += old_parents[i];
                    }
                    std::string mig_url = std::string(kDriveFilesUrl) + "/" +
                                          url_encode(frec.get_string("id")) +
                                          "?addParents=" + url_encode(folder);
                    if (!old_joined.empty()) mig_url += "&removeParents=" + url_encode(old_joined);
                    JsonValue mig_j;
                    std::string mig_err;
                    parse_json_resp(http_("PATCH", mig_url, auth_headers(true), "{}", 20.0), mig_j, mig_err);
                }
            }
        }
    }

    if (has_rec && frec.is_object()) {
        file_id_ = frec.get_string("id");
        file_version_ = frec.get_string("version");
        std::string mod = frec.get_string("modifiedTime");
        std::string desc = frec.get_string("description");
        std::string plat;
        size_t pipe = desc.find('|');
        if (pipe != std::string::npos) plat = trim_ascii(std::string_view(desc).substr(0, pipe));
        update_status([&](CloudStatus& s) {
            s.backup_exists = true;
            s.backup_date = mod;
            s.backup_date_local = format_iso_local(mod);
            s.backup_summary = desc;
            s.backup_platform = plat;
        });
        out_found = true;
        return true;
    }

    update_status([](CloudStatus& s) {
        s.backup_exists = false;
        s.backup_date.clear();
        s.backup_date_local.clear();
        s.backup_summary.clear();
        s.backup_platform.clear();
    });
    return true;
}

bool CloudSaveManager::start_login() {
    return launch_worker([this]() {
        update_status([](CloudStatus& s) {
            s.authorizing = true;
            s.user_code.clear();
            s.verification_url.clear();
            s.last_op_ok = true;
            s.message_key = "cloud.msg.requesting_code";
            s.message_detail.clear();
        });

        std::string cid = get_google_client_id();
        std::string csec = get_google_client_secret();
        HttpHeaders form_hdr = {{"Content-Type", "application/x-www-form-urlencoded"}};
        std::string dev_body = build_form_urlencoded({{"client_id", cid}, {"scope", kDriveScope}});

        JsonValue j;
        std::string err;
        if (!parse_json_resp(http_("POST", kOAuthDeviceUrl, form_hdr, dev_body, 20.0), j, err)) {
            update_status([&](CloudStatus& s) {
                s.authorizing = false;
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = err;
            });
            return;
        }

        std::string device_code = j.get_string("device_code");
        std::string user_code = j.get_string("user_code");
        std::string verify_url = j.get_string("verification_url", "https://www.google.com/device");
        int interval = std::max(1, static_cast<int>(j.get_int("interval", 5)));
        int expires_in = std::max(30, static_cast<int>(j.get_int("expires_in", 1800)));
        double deadline = clock_() + static_cast<double>(expires_in);

        update_status([&](CloudStatus& s) {
            s.authorizing = true;
            s.user_code = user_code;
            s.verification_url = verify_url;
            s.message_key = "cloud.msg.open_code";
            s.message_detail.clear();
        });

        while (!cancel_req_.load() && !shutting_down_.load() && clock_() < deadline) {
            {
                std::unique_lock<std::mutex> clk(cancel_mutex_);
                cancel_cv_.wait_for(clk, std::chrono::seconds(interval),
                                    [this]() { return cancel_req_.load() || shutting_down_.load(); });
            }
            if (cancel_req_.load() || shutting_down_.load()) break;

            std::string tok_body = build_form_urlencoded({
                {"client_id", cid},
                {"client_secret", csec},
                {"device_code", device_code},
                {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
            });
            HttpResponse resp = http_("POST", kOAuthTokenUrl, form_hdr, tok_body, 20.0);
            JsonValue res;
            if (!resp.body.empty()) JsonValue::parse(resp.body, res);

            if (resp.status_code == 200 && res.is_object() && !res.get_string("access_token").empty() &&
                !cancel_req_.load()) {
                access_token_ = res.get_string("access_token");
                std::string rt_new = res.get_string("refresh_token");
                if (!rt_new.empty()) refresh_token_ = rt_new;
                token_expiry_ = static_cast<i64>(clock_()) + res.get_int("expires_in", 3600);
                folder_id_.clear();
                save_tokens();
                update_status([](CloudStatus& s) {
                    s.connected = true;
                    s.authorizing = false;
                    s.user_code.clear();
                    s.verification_url.clear();
                    s.last_op_ok = true;
                    s.message_key = "cloud.msg.connected";
                    s.message_detail.clear();
                });
                bool found = false;
                std::string qerr;
                query_backup_sync(found, qerr);
                return;
            }

            std::string oerr = res.is_object() ? res.get_string("error") : "";
            if (oerr == "authorization_pending") continue;
            if (oerr == "slow_down") {
                interval += 5;
                continue;
            }
            std::string edesc = res.is_object() ? res.get_string("error_description") : "";
            if (edesc.empty()) edesc = !oerr.empty() ? oerr : (!resp.error.empty() ? resp.error : "Google authorization denied or expired");
            update_status([&](CloudStatus& s) {
                s.authorizing = false;
                s.user_code.clear();
                s.verification_url.clear();
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = edesc;
            });
            return;
        }

        update_status([](CloudStatus& s) {
            s.authorizing = false;
            s.user_code.clear();
            s.verification_url.clear();
            s.last_op_ok = false;
            s.message_key = "cloud.msg.auth_cancelled";
            s.message_detail.clear();
        });
    });
}

void CloudSaveManager::cancel_login() {
    cancel_req_ = true;
    cancel_cv_.notify_all();
}

bool CloudSaveManager::refresh_backup() {
    return launch_worker([this]() {
        update_status([](CloudStatus& s) {
            s.last_op_ok = true;
            s.message_key = "cloud.msg.checking";
            s.message_detail.clear();
        });
        bool found = false;
        std::string err;
        if (!query_backup_sync(found, err)) {
            update_status([&](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = err;
            });
            return;
        }
        update_status([found](CloudStatus& s) {
            s.last_op_ok = true;
            s.message_key = found ? "cloud.msg.found" : "cloud.msg.not_found";
            s.message_detail.clear();
        });
    });
}

bool CloudSaveManager::upload_backup(const std::function<void()>& flush_before_read) {
    if (flush_before_read) flush_before_read();
    SaveFileMap files;
    std::string err;
    if (!collect_local_saves(data_root_, files, &err)) {
        update_status([&](CloudStatus& s) {
            s.last_op_ok = false;
            s.message_key.clear();
            s.message_detail = err;
        });
        return false;
    }
    if (files.empty()) {
        update_status([](CloudStatus& s) {
            s.last_op_ok = false;
            s.message_key = "cloud.msg.no_local_save";
            s.message_detail.clear();
        });
        return false;
    }

    LocalSaveInfo linfo = get_local_info();
    std::string summary = build_save_summary(linfo);
    std::string package;
    if (!pack_saves(files, cloud_platform_name(), summary, package, &err)) {
        update_status([&](CloudStatus& s) {
            s.last_op_ok = false;
            s.message_key.clear();
            s.message_detail = err;
        });
        return false;
    }

    std::string expected_id = file_id_;
    std::string expected_ver = file_version_;
    return launch_worker([this, package = std::move(package), summary = std::move(summary),
                          expected_id, expected_ver]() {
        update_status([](CloudStatus& s) {
            s.last_op_ok = true;
            s.message_key = "cloud.msg.uploading";
            s.message_detail.clear();
        });
        bool found = false;
        std::string werr;
        if (!query_backup_sync(found, werr)) {
            update_status([&](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = werr;
            });
            return;
        }
        if (!expected_id.empty() && (file_id_ != expected_id || file_version_ != expected_ver)) {
            update_status([](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key = "cloud.msg.conflict_changed";
                s.message_detail.clear();
            });
            return;
        }

        std::string folder;
        if (!find_folder(true, folder, werr)) {
            update_status([&](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = werr;
            });
            return;
        }

        JsonValue res_j;
        if (!file_id_.empty()) {
            std::string up_url = std::string(kDriveUploadUrl) + "/" + url_encode(file_id_) + "?uploadType=media";
            if (!parse_json_resp(http_("PATCH", up_url, auth_headers(true), package, 20.0), res_j, werr)) {
                update_status([&](CloudStatus& s) {
                    s.last_op_ok = false;
                    s.message_key.clear();
                    s.message_detail = werr;
                });
                return;
            }
            JsonValue::Object desc_obj;
            desc_obj["description"] = JsonValue::make_string(summary);
            std::string meta_url = std::string(kDriveFilesUrl) + "/" + url_encode(file_id_);
            parse_json_resp(http_("PATCH", meta_url, auth_headers(true),
                                  JsonValue::make_object(std::move(desc_obj)).dump(0), 20.0),
                            res_j, werr);
        } else {
            static constexpr const char* kBoundary = "HL3_Backup_2026_boundary";
            JsonValue::Array parents;
            parents.push_back(JsonValue::make_string(folder));
            JsonValue::Object meta_obj;
            meta_obj["name"] = JsonValue::make_string(kCloudFileName);
            meta_obj["parents"] = JsonValue::make_array(std::move(parents));
            meta_obj["description"] = JsonValue::make_string(summary);
            std::string meta_s = JsonValue::make_object(std::move(meta_obj)).dump(0);

            std::string body;
            body.reserve(meta_s.size() + package.size() + 256);
            body += "--";
            body += kBoundary;
            body += "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n";
            body += meta_s;
            body += "\r\n--";
            body += kBoundary;
            body += "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n";
            body += package;
            body += "\r\n--";
            body += kBoundary;
            body += "--\r\n";

            HttpHeaders hdrs = auth_headers(false);
            hdrs.emplace_back("Content-Type", std::string("multipart/related; boundary=") + kBoundary);
            if (!parse_json_resp(http_("POST", std::string(kDriveUploadUrl) + "?uploadType=multipart",
                                       hdrs, body, 20.0), res_j, werr)) {
                update_status([&](CloudStatus& s) {
                    s.last_op_ok = false;
                    s.message_key.clear();
                    s.message_detail = werr;
                });
                return;
            }
        }

        query_backup_sync(found, werr);
        update_status([](CloudStatus& s) {
            s.last_op_ok = true;
            s.message_key = "cloud.msg.uploaded";
            s.message_detail.clear();
        });
    });
}

bool CloudSaveManager::restore_backup() {
    std::string expected_id = file_id_;
    std::string expected_ver = file_version_;
    return launch_worker([this, expected_id, expected_ver]() {
        update_status([](CloudStatus& s) {
            s.last_op_ok = true;
            s.message_key = "cloud.msg.downloading";
            s.message_detail.clear();
        });
        bool found = false;
        std::string werr;
        if (!query_backup_sync(found, werr)) {
            update_status([&](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = werr;
            });
            return;
        }
        if (file_id_.empty()) {
            update_status([](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = "No cloud backup found";
            });
            return;
        }
        if (!expected_id.empty() && (file_id_ != expected_id || file_version_ != expected_ver)) {
            update_status([](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key = "cloud.msg.conflict_changed";
                s.message_detail.clear();
            });
            return;
        }

        std::string dl_url = std::string(kDriveFilesUrl) + "/" + url_encode(file_id_) + "?alt=media";
        HttpResponse resp = http_("GET", dl_url, auth_headers(), "", 20.0);
        if (resp.status_code != 200) {
            update_status([&](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = !resp.error.empty()
                                       ? resp.error
                                       : ("Could not download backup (HTTP " + std::to_string(resp.status_code) + ")");
            });
            return;
        }

        SaveFileMap files;
        UnpackMeta meta;
        if (!unpack_saves(resp.body, files, meta, &werr)) {
            update_status([&](CloudStatus& s) {
                s.last_op_ok = false;
                s.message_key.clear();
                s.message_detail = werr;
            });
            return;
        }

        {
            std::lock_guard<std::mutex> lk(mutex_);
            pending_restore_ = std::move(files);
            has_pending_restore_ = true;
            status_.restore_ready = true;
            status_.last_op_ok = true;
            status_.message_key = "cloud.msg.restored_reloading";
            status_.message_detail.clear();
        }
    });
}

bool CloudSaveManager::consume_pending_restore(std::filesystem::path* out_backup) {
    SaveFileMap files;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (status_.busy || !status_.restore_ready || !has_pending_restore_) return false;
        files = std::move(pending_restore_);
        pending_restore_.clear();
        has_pending_restore_ = false;
    }

    std::filesystem::path backup_dir;
    std::string err;
    if (!restore_saves(data_root_, files, backup_dir, &err)) {
        update_status([&](CloudStatus& s) {
            s.restore_ready = false;
            s.last_op_ok = false;
            s.message_key.clear();
            s.message_detail = err;
        });
        return false;
    }

    update_status([](CloudStatus& s) {
        s.restore_ready = false;
        s.last_op_ok = true;
    });
    if (out_backup) *out_backup = backup_dir;
    return true;
}

void CloudSaveManager::disconnect() {
    cancel_login();
    wait_idle();
    std::error_code ec;
    std::filesystem::remove(auth_path_, ec);

    std::lock_guard<std::mutex> lk(mutex_);
    access_token_.clear();
    refresh_token_.clear();
    token_expiry_ = 0;
    folder_id_.clear();
    file_id_.clear();
    file_version_.clear();
    pending_restore_.clear();
    has_pending_restore_ = false;

    status_ = CloudStatus{};
    status_.connected = false;
    status_.message_key = "cloud.msg.disconnected";
    status_.last_op_ok = true;
}

}  // namespace hl3

