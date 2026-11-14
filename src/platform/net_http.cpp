#include "net_http.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#elif defined(__ANDROID__)
#include <SDL.h>
#include <jni.h>
#else
#include <curl/curl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/wait.h>
#endif
#endif

namespace hl3 {

namespace {

#ifdef _WIN32
std::wstring utf8_to_wide(std::string_view s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring w(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), needed);
    return w;
}

std::string wide_to_utf8(std::wstring_view w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string s(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        s.data(), needed, nullptr, nullptr);
    return s;
}

struct ParsedUrl {
    bool ok = false;
    bool secure = true;
    std::wstring host;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    std::wstring path_and_query;
};

ParsedUrl parse_url_parts(const std::string& url) {
    ParsedUrl out;
    std::wstring wurl = utf8_to_wide(url);
    if (wurl.empty()) return out;

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host_buf[512]{};
    wchar_t path_buf[4096]{};
    wchar_t extra_buf[4096]{};
    uc.lpszHostName = host_buf;
    uc.dwHostNameLength = static_cast<DWORD>(std::size(host_buf));
    uc.lpszUrlPath = path_buf;
    uc.dwUrlPathLength = static_cast<DWORD>(std::size(path_buf));
    uc.lpszExtraInfo = extra_buf;
    uc.dwExtraInfoLength = static_cast<DWORD>(std::size(extra_buf));

    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &uc)) {
        return out;
    }
    if (uc.nScheme != INTERNET_SCHEME_HTTPS && uc.nScheme != INTERNET_SCHEME_HTTP) {
        return out;
    }
    out.ok = true;
    out.secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    out.host = std::wstring(uc.lpszHostName, uc.dwHostNameLength);
    out.port = uc.nPort;
    out.path_and_query = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength) +
                         std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (out.path_and_query.empty()) out.path_and_query = L"/";
    return out;
}

struct WinHttpHandle {
    HINTERNET h = nullptr;
    ~WinHttpHandle() {
        if (h) WinHttpCloseHandle(h);
    }
    operator HINTERNET() const { return h; }
};

std::string format_win32_error(DWORD code) {
    wchar_t* msg = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&msg), 0, nullptr);
    std::string out = "WinHTTP error " + std::to_string(code);
    if (len > 0 && msg) {
        out = trim_ascii(wide_to_utf8(std::wstring_view(msg, len))) + " (" + std::to_string(code) + ")";
        LocalFree(msg);
    }
    return out;
}
#elif defined(__ANDROID__)
struct AndroidDlCtx {
    std::function<bool(size_t, size_t)> progress_cb;
    const std::atomic<bool>* stop_flag = nullptr;
    std::atomic<bool> cancelled{false};
};

struct AndroidActivityRef {
    JNIEnv* env = nullptr;
    jobject activity = nullptr;
    jclass cls = nullptr;

    AndroidActivityRef() {
        env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        if (!env) return;
        activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (!activity) return;
        cls = env->GetObjectClass(activity);
    }

    ~AndroidActivityRef() {
        if (!env) return;
        if (cls) env->DeleteLocalRef(cls);
        if (activity) env->DeleteLocalRef(activity);
    }

    bool ok() const { return env && activity && cls; }
};

std::string jstring_to_utf8(JNIEnv* env, jstring js) {
    if (!env || !js) return {};
    const char* chars = env->GetStringUTFChars(js, nullptr);
    if (!chars) return {};
    std::string out(chars);
    env->ReleaseStringUTFChars(js, chars);
    return out;
}

bool clear_jni_exception(JNIEnv* env, std::string& out_err, const char* fallback) {
    if (!env || !env->ExceptionCheck()) return false;
    env->ExceptionDescribe();
    env->ExceptionClear();
    out_err = fallback ? fallback : "JNI call threw an exception";
    return true;
}
#else
void ensure_curl_global_init() {
    static std::once_flag once;
    std::call_once(once, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

void configure_curl_ca(CURL* curl) {
    if (!curl) return;
    if (const char* env_ca = std::getenv("HL3_CA_BUNDLE"); env_ca && env_ca[0] != '\0') {
        curl_easy_setopt(curl, CURLOPT_CAINFO, env_ca);
        return;
    }
    if (const char* curl_ca = std::getenv("CURL_CA_BUNDLE"); curl_ca && curl_ca[0] != '\0') {
        curl_easy_setopt(curl, CURLOPT_CAINFO, curl_ca);
        return;
    }
#ifdef __SWITCH__
    std::error_code ec;
    if (std::filesystem::exists("romfs:/cacert.pem", ec)) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
    }
#endif
}

struct CurlWriteBuf {
    Bytes* dst = nullptr;
    size_t max_bytes = 64u * 1024u * 1024u;
    bool overflow = false;
};

size_t curl_write_mem_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<CurlWriteBuf*>(userdata);
    if (!ctx || !ctx->dst) return 0;
    size_t n = size * nmemb;
    if (n == 0) return 0;
    if (ctx->dst->size() + n > ctx->max_bytes) {
        ctx->overflow = true;
        return 0;
    }
    const auto* b = reinterpret_cast<const u8*>(ptr);
    ctx->dst->insert(ctx->dst->end(), b, b + n);
    return n;
}

struct CurlDownloadCtx {
    std::ofstream* out = nullptr;
    Bytes* all_data = nullptr;
    size_t max_bytes = 0;
    size_t written = 0;
    size_t expected_total = 0;
    bool cancelled = false;
    std::string err;
    std::function<bool(size_t, size_t)> progress_cb;
    const std::atomic<bool>* stop_flag = nullptr;
    CURL* curl = nullptr;
};

size_t curl_write_file_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<CurlDownloadCtx*>(userdata);
    if (!ctx || !ctx->out) return 0;
    if (ctx->stop_flag && ctx->stop_flag->load()) {
        ctx->err = "Cancelled on shutdown";
        ctx->cancelled = true;
        return 0;
    }
    size_t n = size * nmemb;
    if (n == 0) return 0;
    if (ctx->written + n > ctx->max_bytes) {
        ctx->err = "Download stream exceeds safety limit";
        return 0;
    }
    ctx->out->write(ptr, static_cast<std::streamsize>(n));
    if (!ctx->out->good()) {
        ctx->err = "Disk write failed during download";
        return 0;
    }
    const auto* b = reinterpret_cast<const u8*>(ptr);
    ctx->all_data->insert(ctx->all_data->end(), b, b + n);
    ctx->written += n;
    if (ctx->expected_total == 0 && ctx->curl) {
        curl_off_t cl = -1;
        if (curl_easy_getinfo(ctx->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl) == CURLE_OK && cl > 0) {
            ctx->expected_total = static_cast<size_t>(cl);
        }
    }
    if (ctx->progress_cb && !ctx->progress_cb(ctx->written, ctx->expected_total)) {
        ctx->err = "Download cancelled";
        ctx->cancelled = true;
        return 0;
    }
    return n;
}

int curl_xferinfo_cb(void* userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<CurlDownloadCtx*>(userdata);
    if (!ctx) return 0;
    if (ctx->stop_flag && ctx->stop_flag->load()) {
        ctx->err = "Cancelled on shutdown";
        ctx->cancelled = true;
        return 1;
    }
    if (dltotal > 0 && ctx->expected_total == 0) {
        ctx->expected_total = static_cast<size_t>(dltotal);
    }
    size_t done = ctx->written > 0 ? ctx->written : static_cast<size_t>(std::max<curl_off_t>(0, dlnow));
    if (ctx->progress_cb && !ctx->progress_cb(done, ctx->expected_total)) {
        ctx->err = "Download cancelled";
        ctx->cancelled = true;
        return 1;
    }
    return 0;
}
#endif

}  // namespace

#ifdef __ANDROID__
extern "C" JNIEXPORT jboolean JNICALL
Java_com_davidbraga_heroeslore3_HeroesLore3Activity_nativeDownloadProgress(
    JNIEnv*, jclass, jlong ctx_ptr, jlong done, jlong total) {
    auto* ctx = reinterpret_cast<AndroidDlCtx*>(static_cast<intptr_t>(ctx_ptr));
    if (!ctx) return JNI_TRUE;
    if (ctx->stop_flag && ctx->stop_flag->load()) {
        ctx->cancelled.store(true);
        return JNI_FALSE;
    }
    if (ctx->progress_cb &&
        !ctx->progress_cb(static_cast<size_t>(std::max<jlong>(0, done)),
                          static_cast<size_t>(std::max<jlong>(0, total)))) {
        ctx->cancelled.store(true);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}
#endif

HttpResponse http_execute(
    const std::string& method,
    const std::string& url,
    const HttpHeaders& headers,
    const std::string& body,
    double timeout_sec) {
    HttpResponse resp;
#ifdef _WIN32
    ParsedUrl pu = parse_url_parts(url);
    if (!pu.ok) {
        resp.error = "Invalid URL: " + url;
        return resp;
    }

    std::string ua = "HeroesLore3Recomp/1.0";
    for (const auto& h : headers) {
        if (h.first == "User-Agent" && !h.second.empty()) {
            ua = h.second;
            break;
        }
    }

    WinHttpHandle session{WinHttpOpen(utf8_to_wide(ua).c_str(),
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.h) {
        session.h = WinHttpOpen(utf8_to_wide(ua).c_str(),
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session.h) {
        resp.error = format_win32_error(GetLastError());
        return resp;
    }

    int t_ms = static_cast<int>(timeout_sec * 1000.0);
    if (t_ms < 1000) t_ms = 1000;
    WinHttpSetTimeouts(session.h, t_ms, t_ms, t_ms, t_ms);

    WinHttpHandle connect{WinHttpConnect(session.h, pu.host.c_str(), pu.port, 0)};
    if (!connect.h) {
        resp.error = format_win32_error(GetLastError());
        return resp;
    }

    DWORD flags = pu.secure ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wmethod = utf8_to_wide(method.empty() ? "GET" : method);
    WinHttpHandle req{WinHttpOpenRequest(connect.h, wmethod.c_str(), pu.path_and_query.c_str(),
                                         nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags)};
    if (!req.h) {
        resp.error = format_win32_error(GetLastError());
        return resp;
    }

    std::wstring hdr_block;
    for (const auto& h : headers) {
        if (h.first == "User-Agent") continue;
        hdr_block += utf8_to_wide(h.first) + L": " + utf8_to_wide(h.second) + L"\r\n";
    }
    if (!hdr_block.empty()) {
        WinHttpAddRequestHeaders(req.h, hdr_block.c_str(), static_cast<DWORD>(hdr_block.size()),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    LPVOID body_ptr = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
    DWORD body_len = static_cast<DWORD>(body.size());
    if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body_ptr, body_len, body_len, 0)) {
        resp.error = format_win32_error(GetLastError());
        return resp;
    }

    if (!WinHttpReceiveResponse(req.h, nullptr)) {
        resp.error = format_win32_error(GetLastError());
        return resp;
    }

    DWORD status = 0;
    DWORD status_sz = sizeof(status);
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_sz, WINHTTP_NO_HEADER_INDEX);
    resp.status_code = static_cast<int>(status);

    DWORD cl = 0;
    DWORD cl_sz = sizeof(cl);
    if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &cl, &cl_sz, WINHTTP_NO_HEADER_INDEX)) {
        resp.content_length = static_cast<size_t>(cl);
    }

    wchar_t final_buf[2048]{};
    DWORD final_sz = sizeof(final_buf);
    if (WinHttpQueryOption(req.h, WINHTTP_OPTION_URL, final_buf, &final_sz)) {
        resp.final_url = wide_to_utf8(final_buf);
    } else {
        resp.final_url = url;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req.h, &avail)) {
            resp.error = format_win32_error(GetLastError());
            return resp;
        }
        if (avail == 0) break;
        size_t old_sz = resp.body.size();
        if (old_sz + avail > 64u * 1024u * 1024u) {
            resp.error = "Response too large";
            return resp;
        }
        resp.body.resize(old_sz + avail);
        DWORD read = 0;
        if (!WinHttpReadData(req.h, resp.body.data() + old_sz, avail, &read)) {
            resp.error = format_win32_error(GetLastError());
            return resp;
        }
        resp.body.resize(old_sz + read);
    }
    return resp;
#elif defined(__ANDROID__)
    AndroidActivityRef act;
    if (!act.ok()) {
        resp.error = "Android JNI activity unavailable";
        return resp;
    }
    JNIEnv* env = act.env;
    jmethodID mid = env->GetStaticMethodID(
        act.cls, "httpExecute",
        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[BI)[Ljava/lang/Object;");
    if (!mid || env->ExceptionCheck()) {
        clear_jni_exception(env, resp.error, "Missing HeroesLore3Activity.httpExecute");
        return resp;
    }

    std::string m_str = method.empty() ? "GET" : method;
    jstring jmethod = env->NewStringUTF(m_str.c_str());
    jstring jurl = env->NewStringUTF(url.c_str());
    jclass str_cls = env->FindClass("java/lang/String");
    jsize hdr_n = static_cast<jsize>(headers.size());
    jobjectArray jkeys = env->NewObjectArray(hdr_n, str_cls, nullptr);
    jobjectArray jvals = env->NewObjectArray(hdr_n, str_cls, nullptr);
    if (str_cls) env->DeleteLocalRef(str_cls);

    for (jsize i = 0; i < hdr_n; ++i) {
        jstring jk = env->NewStringUTF(headers[static_cast<size_t>(i)].first.c_str());
        jstring jv = env->NewStringUTF(headers[static_cast<size_t>(i)].second.c_str());
        env->SetObjectArrayElement(jkeys, i, jk);
        env->SetObjectArrayElement(jvals, i, jv);
        env->DeleteLocalRef(jk);
        env->DeleteLocalRef(jv);
    }

    jbyteArray jbody = nullptr;
    if (!body.empty()) {
        jbody = env->NewByteArray(static_cast<jsize>(body.size()));
        env->SetByteArrayRegion(jbody, 0, static_cast<jsize>(body.size()),
                                reinterpret_cast<const jbyte*>(body.data()));
    }

    int timeout_ms = std::max(1000, static_cast<int>(timeout_sec * 1000.0));
    auto res_arr = static_cast<jobjectArray>(env->CallStaticObjectMethod(
        act.cls, mid, jmethod, jurl, jkeys, jvals, jbody, static_cast<jint>(timeout_ms)));

    if (jmethod) env->DeleteLocalRef(jmethod);
    if (jurl) env->DeleteLocalRef(jurl);
    if (jkeys) env->DeleteLocalRef(jkeys);
    if (jvals) env->DeleteLocalRef(jvals);
    if (jbody) env->DeleteLocalRef(jbody);

    if (clear_jni_exception(env, resp.error, "Java httpExecute threw exception")) {
        if (res_arr) env->DeleteLocalRef(res_arr);
        return resp;
    }
    if (!res_arr || env->GetArrayLength(res_arr) < 3) {
        if (res_arr) env->DeleteLocalRef(res_arr);
        resp.error = "Invalid httpExecute return array";
        return resp;
    }

    jobject o0 = env->GetObjectArrayElement(res_arr, 0);
    jobject o1 = env->GetObjectArrayElement(res_arr, 1);
    jobject o2 = env->GetObjectArrayElement(res_arr, 2);

    int status = -1;
    if (o0) {
        jclass int_cls = env->GetObjectClass(o0);
        jmethodID int_val = env->GetMethodID(int_cls, "intValue", "()I");
        if (int_val) status = static_cast<int>(env->CallIntMethod(o0, int_val));
        env->DeleteLocalRef(int_cls);
        env->DeleteLocalRef(o0);
    }
    std::string text1 = jstring_to_utf8(env, static_cast<jstring>(o1));
    if (o1) env->DeleteLocalRef(o1);

    if (status < 0) {
        if (o2) env->DeleteLocalRef(o2);
        env->DeleteLocalRef(res_arr);
        resp.error = text1.empty() ? "Android HTTP transport error" : text1;
        return resp;
    }

    resp.status_code = status;
    resp.final_url = text1.empty() ? url : text1;
    if (o2) {
        auto jbytes = static_cast<jbyteArray>(o2);
        jsize len = env->GetArrayLength(jbytes);
        if (len > 0) {
            resp.body.resize(static_cast<size_t>(len));
            env->GetByteArrayRegion(jbytes, 0, len, reinterpret_cast<jbyte*>(resp.body.data()));
        }
        resp.content_length = resp.body.size();
        env->DeleteLocalRef(o2);
    }
    env->DeleteLocalRef(res_arr);
    return resp;
#else
    ensure_curl_global_init();
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "curl_easy_init failed";
        return resp;
    }

    char errbuf[CURL_ERROR_SIZE]{};
    CurlWriteBuf wbuf{&resp.body, 64u * 1024u * 1024u, false};
    std::string ua = "HeroesLore3Recomp/1.0";
    struct curl_slist* hdr_list = nullptr;
    for (const auto& h : headers) {
        if (h.first == "User-Agent" && !h.second.empty()) {
            ua = h.second;
            continue;
        }
        std::string line = h.first + ": " + h.second;
        hdr_list = curl_slist_append(hdr_list, line.c_str());
    }

    long timeout_ms = std::max(1000L, static_cast<long>(timeout_sec * 1000.0));
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_mem_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wbuf);
    if (hdr_list) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);
    configure_curl_ca(curl);

    std::string m = method.empty() ? "GET" : method;
    if (m == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.empty() ? "" : body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else if (m != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, m.c_str());
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        resp.body.clear();
        if (wbuf.overflow) {
            resp.error = "Response too large";
        } else if (errbuf[0] != '\0') {
            resp.error = errbuf;
        } else {
            resp.error = curl_easy_strerror(rc);
        }
    } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        resp.status_code = static_cast<int>(status);
        curl_off_t cl = -1;
        if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl) == CURLE_OK && cl >= 0) {
            resp.content_length = static_cast<size_t>(cl);
        } else {
            resp.content_length = resp.body.size();
        }
        char* eff = nullptr;
        if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff) == CURLE_OK && eff) {
            resp.final_url = eff;
        } else {
            resp.final_url = url;
        }
    }

    if (hdr_list) curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);
    return resp;
#endif
}

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
    std::string& out_err) {
    out_final_url.clear();
    out_sha256.clear();
    out_written = 0;
    out_err.clear();

#ifdef _WIN32
    ParsedUrl pu = parse_url_parts(url);
    if (!pu.ok || !pu.secure) {
        out_err = "Non-HTTPS download URL blocked";
        return false;
    }

    std::wstring ua = utf8_to_wide(user_agent.empty() ? "HeroesLore3Recomp/1.0" : user_agent);
    WinHttpHandle session{WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.h) {
        session.h = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session.h) {
        out_err = format_win32_error(GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session.h, 30000, 30000, 30000, 30000);

    WinHttpHandle connect{WinHttpConnect(session.h, pu.host.c_str(), pu.port, 0)};
    if (!connect.h) {
        out_err = format_win32_error(GetLastError());
        return false;
    }
    WinHttpHandle req{WinHttpOpenRequest(connect.h, L"GET", pu.path_and_query.c_str(),
                                         nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE)};
    if (!req.h) {
        out_err = format_win32_error(GetLastError());
        return false;
    }

    if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req.h, nullptr)) {
        out_err = format_win32_error(GetLastError());
        return false;
    }

    DWORD status = 0;
    DWORD status_sz = sizeof(status);
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_sz, WINHTTP_NO_HEADER_INDEX);
    if (status < 200 || status >= 300) {
        out_err = "HTTP " + std::to_string(status);
        return false;
    }

    wchar_t final_buf[2048]{};
    DWORD final_sz = sizeof(final_buf);
    if (WinHttpQueryOption(req.h, WINHTTP_OPTION_URL, final_buf, &final_sz)) {
        out_final_url = wide_to_utf8(final_buf);
        if (out_final_url.rfind("https://", 0) != 0) {
            out_err = "Non-HTTPS redirect blocked: " + out_final_url;
            return false;
        }
    } else {
        out_final_url = url;
    }

    DWORD cl = 0;
    DWORD cl_sz = sizeof(cl);
    size_t expected_total = 0;
    if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &cl, &cl_sz, WINHTTP_NO_HEADER_INDEX)) {
        expected_total = static_cast<size_t>(cl);
    }
    if (expected_total > max_bytes) {
        out_err = "Download exceeds size limit";
        return false;
    }

    std::error_code ec;
    if (dest_path.has_parent_path()) {
        std::filesystem::create_directories(dest_path.parent_path(), ec);
    }
    std::ofstream out(dest_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        out_err = "Could not open staging file for write";
        return false;
    }

    // Stream into disk and compute SHA-256 over chunks via incremental hash or buffer.
    // Note: max update ZIP is ~30 MB, well within RAM for single-pass FIPS 180-4 digest.
    Bytes all_data;
    if (expected_total > 0) all_data.reserve(expected_total);
    std::vector<u8> buf(65536);

    for (;;) {
        if (stop_flag && stop_flag->load()) {
            out_err = "Cancelled on shutdown";
            return false;
        }
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req.h, &avail)) {
            out_err = format_win32_error(GetLastError());
            return false;
        }
        if (avail == 0) break;
        if (buf.size() < avail) buf.resize(avail);
        DWORD got = 0;
        if (!WinHttpReadData(req.h, buf.data(), avail, &got)) {
            out_err = format_win32_error(GetLastError());
            return false;
        }
        if (got == 0) break;
        out_written += got;
        if (out_written > max_bytes) {
            out_err = "Download stream exceeds safety limit";
            return false;
        }
        out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(got));
        if (!out.good()) {
            out_err = "Disk write failed during download";
            return false;
        }
        all_data.insert(all_data.end(), buf.data(), buf.data() + got);
        if (progress_cb && !progress_cb(out_written, expected_total)) {
            out_err = "Download cancelled";
            return false;
        }
    }

    out.flush();
    out.close();
    out_sha256 = sha256_hex(all_data);
    return true;
#elif defined(__ANDROID__)
    AndroidActivityRef act;
    if (!act.ok()) {
        out_err = "Android JNI activity unavailable";
        return false;
    }
    volatile auto keep_native_progress =
        &Java_com_davidbraga_heroeslore3_HeroesLore3Activity_nativeDownloadProgress;
    (void)keep_native_progress;
    JNIEnv* env = act.env;
    jmethodID mid = env->GetStaticMethodID(
        act.cls, "downloadFile",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JJ)[Ljava/lang/Object;");
    if (!mid || env->ExceptionCheck()) {
        clear_jni_exception(env, out_err, "Missing HeroesLore3Activity.downloadFile");
        return false;
    }

    std::error_code ec;
    if (!dest_path.parent_path().empty()) {
        std::filesystem::create_directories(dest_path.parent_path(), ec);
    }

    AndroidDlCtx dl_ctx;
    dl_ctx.progress_cb = progress_cb;
    dl_ctx.stop_flag = stop_flag;

    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jdest = env->NewStringUTF(dest_path.u8string().c_str());
    jstring jua = env->NewStringUTF(user_agent.empty() ? "HeroesLore3Recomp/1.0" : user_agent.c_str());
    auto res_arr = static_cast<jobjectArray>(env->CallStaticObjectMethod(
        act.cls, mid, jurl, jdest, jua, static_cast<jlong>(max_bytes),
        static_cast<jlong>(reinterpret_cast<intptr_t>(&dl_ctx))));

    if (jurl) env->DeleteLocalRef(jurl);
    if (jdest) env->DeleteLocalRef(jdest);
    if (jua) env->DeleteLocalRef(jua);

    if (clear_jni_exception(env, out_err, "Java downloadFile threw exception")) {
        if (res_arr) env->DeleteLocalRef(res_arr);
        return false;
    }
    if (!res_arr || env->GetArrayLength(res_arr) < 4) {
        if (res_arr) env->DeleteLocalRef(res_arr);
        out_err = "Invalid downloadFile return array";
        return false;
    }

    jobject o0 = env->GetObjectArrayElement(res_arr, 0);
    jobject o1 = env->GetObjectArrayElement(res_arr, 1);
    jobject o2 = env->GetObjectArrayElement(res_arr, 2);
    jobject o3 = env->GetObjectArrayElement(res_arr, 3);

    bool ok = false;
    if (o0) {
        jclass b_cls = env->GetObjectClass(o0);
        jmethodID b_val = env->GetMethodID(b_cls, "booleanValue", "()Z");
        if (b_val) ok = (env->CallBooleanMethod(o0, b_val) == JNI_TRUE);
        env->DeleteLocalRef(b_cls);
        env->DeleteLocalRef(o0);
    }
    std::string s1 = jstring_to_utf8(env, static_cast<jstring>(o1));
    std::string s2 = jstring_to_utf8(env, static_cast<jstring>(o2));
    if (o1) env->DeleteLocalRef(o1);
    if (o2) env->DeleteLocalRef(o2);

    jlong written = 0;
    if (o3) {
        jclass l_cls = env->GetObjectClass(o3);
        jmethodID l_val = env->GetMethodID(l_cls, "longValue", "()J");
        if (l_val) written = env->CallLongMethod(o3, l_val);
        env->DeleteLocalRef(l_cls);
        env->DeleteLocalRef(o3);
    }
    env->DeleteLocalRef(res_arr);

    if (!ok) {
        out_err = s1.empty() ? "Download failed" : s1;
        return false;
    }
    out_final_url = s1.empty() ? url : s1;
    out_sha256 = s2;
    out_written = static_cast<size_t>(std::max<jlong>(0, written));
    return true;
#else
    ensure_curl_global_init();
    std::error_code ec;
    if (!dest_path.parent_path().empty()) {
        std::filesystem::create_directories(dest_path.parent_path(), ec);
    }
    std::ofstream out(dest_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        out_err = "Failed to open destination file for download";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        out_err = "curl_easy_init failed";
        return false;
    }

    Bytes all_data;
    CurlDownloadCtx ctx;
    ctx.out = &out;
    ctx.all_data = &all_data;
    ctx.max_bytes = max_bytes;
    ctx.progress_cb = progress_cb;
    ctx.stop_flag = stop_flag;
    ctx.curl = curl;

    char errbuf[CURL_ERROR_SIZE]{};
    const char* ua = user_agent.empty() ? "HeroesLore3Recomp/1.0" : user_agent.c_str();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_xferinfo_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    configure_curl_ca(curl);

    CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    char* eff = nullptr;
    if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff) == CURLE_OK && eff) {
        out_final_url = eff;
    } else {
        out_final_url = url;
    }
    curl_easy_cleanup(curl);

    out.flush();
    out.close();

    if (rc != CURLE_OK) {
        std::filesystem::remove(dest_path, ec);
        if (!ctx.err.empty()) {
            out_err = ctx.err;
        } else if (errbuf[0] != '\0') {
            out_err = errbuf;
        } else {
            out_err = curl_easy_strerror(rc);
        }
        return false;
    }
    if (status < 200 || status >= 300) {
        std::filesystem::remove(dest_path, ec);
        out_err = "HTTP " + std::to_string(status);
        return false;
    }

    out_written = ctx.written;
    out_sha256 = sha256_hex(all_data);
    return true;
#endif
}

std::string url_encode(std::string_view s) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0xFu]);
            out.push_back(kHex[c & 0xFu]);
        }
    }
    return out;
}

std::string build_form_urlencoded(
    std::initializer_list<std::pair<std::string_view, std::string_view>> params) {
    std::string out;
    bool first = true;
    for (const auto& kv : params) {
        if (!first) out.push_back('&');
        first = false;
        out += url_encode(kv.first);
        out.push_back('=');
        out += url_encode(kv.second);
    }
    return out;
}

std::string base64_encode(const u8* data, size_t len) {
    static constexpr char kB64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        u32 b0 = data[i];
        u32 b1 = (i + 1 < len) ? data[i + 1] : 0u;
        u32 b2 = (i + 2 < len) ? data[i + 2] : 0u;
        u32 triple = (b0 << 16) | (b1 << 8) | b2;
        out.push_back(kB64[(triple >> 18) & 0x3Fu]);
        out.push_back(kB64[(triple >> 12) & 0x3Fu]);
        out.push_back((i + 1 < len) ? kB64[(triple >> 6) & 0x3Fu] : '=');
        out.push_back((i + 2 < len) ? kB64[triple & 0x3Fu] : '=');
    }
    return out;
}

bool base64_decode(std::string_view b64, Bytes& out) {
    out.clear();
    if (b64.empty()) return true;
    if ((b64.size() % 4) != 0) return false;

    auto decode_char = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return 26 + (c - 'a');
        if (c >= '0' && c <= '9') return 52 + (c - '0');
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    size_t pad = 0;
    if (b64[b64.size() - 1] == '=') ++pad;
    if (b64[b64.size() - 2] == '=') ++pad;

    out.reserve((b64.size() / 4) * 3 - pad);
    for (size_t i = 0; i < b64.size(); i += 4) {
        bool last_quartet = (i + 4 == b64.size());
        int v0 = decode_char(static_cast<unsigned char>(b64[i]));
        int v1 = decode_char(static_cast<unsigned char>(b64[i + 1]));
        if (v0 < 0 || v1 < 0) return false;

        int v2 = 0;
        int v3 = 0;
        bool p2 = (b64[i + 2] == '=');
        bool p3 = (b64[i + 3] == '=');
        if (!last_quartet && (p2 || p3)) return false;
        if (p2 && !p3) return false;
        if (!p2) {
            v2 = decode_char(static_cast<unsigned char>(b64[i + 2]));
            if (v2 < 0) return false;
        }
        if (!p3) {
            v3 = decode_char(static_cast<unsigned char>(b64[i + 3]));
            if (v3 < 0) return false;
        }

        // Canonical padding bits check (matches Python validate=True + canonical re-encode check)
        if (p2 && (v1 & 0x0F) != 0) return false;
        if (!p2 && p3 && (v2 & 0x03) != 0) return false;

        u32 triple = (u32(v0) << 18) | (u32(v1) << 12) | (u32(v2) << 6) | u32(v3);
        out.push_back(static_cast<u8>((triple >> 16) & 0xFFu));
        if (!p2) out.push_back(static_cast<u8>((triple >> 8) & 0xFFu));
        if (!p3) out.push_back(static_cast<u8>(triple & 0xFFu));
    }
    return true;
}

#if defined(__linux__) && !defined(__ANDROID__)
static void launch_xdg_open(const std::string& target) {
    if (target.empty()) return;
    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                dup2(devnull, 0);
                dup2(devnull, 1);
                dup2(devnull, 2);
                close(devnull);
            }
            execlp("xdg-open", "xdg-open", target.c_str(), nullptr);
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        int st = 0;
        waitpid(pid, &st, 0);
    }
}
#endif

void open_url_in_browser(const std::string& url) {
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) return;
#ifdef _WIN32
    std::wstring wurl = utf8_to_wide(url);
    ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__ANDROID__)
    AndroidActivityRef act;
    if (!act.ok()) return;
    jmethodID mid = act.env->GetStaticMethodID(act.cls, "openUrl", "(Ljava/lang/String;)V");
    if (!mid || act.env->ExceptionCheck()) {
        std::string ignored;
        clear_jni_exception(act.env, ignored, "openUrl");
        return;
    }
    jstring jurl = act.env->NewStringUTF(url.c_str());
    act.env->CallStaticVoidMethod(act.cls, mid, jurl);
    if (jurl) act.env->DeleteLocalRef(jurl);
    std::string ignored;
    clear_jni_exception(act.env, ignored, "openUrl");
#elif defined(__linux__) && !defined(__ANDROID__)
    launch_xdg_open(url);
#else
    (void)url;
#endif
}

void open_folder_in_explorer(const std::filesystem::path& dir) {
#ifdef _WIN32
    std::wstring wdir = dir.wstring();
    ShellExecuteW(nullptr, L"open", wdir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__linux__) && !defined(__ANDROID__)
    launch_xdg_open(dir.u8string());
#else
    (void)dir;
#endif
}

bool install_android_apk(const std::string& apk_path) {
#ifdef __ANDROID__
    if (apk_path.empty()) return false;
    AndroidActivityRef act;
    if (!act.ok()) return false;
    jmethodID mid = act.env->GetStaticMethodID(act.cls, "installApk", "(Ljava/lang/String;)Z");
    if (!mid || act.env->ExceptionCheck()) {
        std::string ignored;
        clear_jni_exception(act.env, ignored, "installApk");
        return false;
    }
    jstring jpath = act.env->NewStringUTF(apk_path.c_str());
    jboolean ok = act.env->CallStaticBooleanMethod(act.cls, mid, jpath);
    if (jpath) act.env->DeleteLocalRef(jpath);
    std::string ignored;
    if (clear_jni_exception(act.env, ignored, "installApk")) return false;
    return ok == JNI_TRUE;
#else
    (void)apk_path;
    return false;
#endif
}

}  // namespace hl3

