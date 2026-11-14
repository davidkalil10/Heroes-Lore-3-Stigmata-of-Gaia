#pragma once

#include "../core/common.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hl3 {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::string final_url;
    size_t content_length = 0;
    std::string error;
};

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

using HttpCallable = std::function<HttpResponse(
    const std::string& method,
    const std::string& url,
    const HttpHeaders& headers,
    const std::string& body,
    double timeout_sec)>;

HttpResponse http_execute(
    const std::string& method,
    const std::string& url,
    const HttpHeaders& headers = {},
    const std::string& body = {},
    double timeout_sec = 20.0);

bool http_download_file(
    const std::string& url,
    const std::filesystem::path& dest_path,
    const std::string& user_agent,
    size_t max_bytes,
    const std::function<bool(size_t done, size_t total)>& progress_cb,
    const std::atomic<bool>* stop_flag,
    std::string& out_final_url,
    std::string& out_sha256,
    size_t& out_written,
    std::string& out_err);

std::string url_encode(std::string_view s);
std::string build_form_urlencoded(
    std::initializer_list<std::pair<std::string_view, std::string_view>> params);

std::string base64_encode(const u8* data, size_t len);
inline std::string base64_encode(const Bytes& bytes) {
    return base64_encode(bytes.data(), bytes.size());
}
bool base64_decode(std::string_view b64, Bytes& out);

void open_url_in_browser(const std::string& url);
void open_folder_in_explorer(const std::filesystem::path& dir);
bool install_android_apk(const std::string& apk_path);

}  // namespace hl3

