#include "core/bundle.h"
#include "core/common.h"
#include "hle/gl_bridge.h"
#include "platform/cloud_save.h"
#include "platform/display_config.h"
#include "platform/i18n.h"
#include "platform/input_controller.h"
#include "platform/net_http.h"
#include "platform/options_modal.h"
#include "platform/png_codec.h"
#include "platform/ui_renderer.h"
#include "platform/updater.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_fails = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_fails;
    }
}

void test_i18n() {
    hl3::set_active_i18n_language("en");
    check(hl3::get_active_i18n_language() == "en", "i18n active en");
    check(hl3::tr("common.back") == "Back", "i18n en common.back");
    std::string cred = hl3::about_credit_prefix("en", "1.0.3");
    check(cred.find("David Kalil Braga (2026)") != std::string::npos, "i18n credit author");
    check(cred.find("v1.0.3") != std::string::npos, "i18n credit version");

    hl3::set_active_i18n_language("pt-br");
    check(hl3::get_active_i18n_language() == "pt-BR", "i18n normalize pt-BR");
    check(hl3::tr("common.back") == "Voltar", "i18n pt-br common.back");
    std::string dl = hl3::tr_fmt("update.btn.download", {{"tag", "v1.0.4"}});
    check(dl.find("v1.0.4") != std::string::npos, "i18n tr_fmt tag");

    check(std::string(hl3::kAssetWindowsX64) == "heroes_lore_3_windows_x64.zip", "asset const win64");
    check(std::string(hl3::kAssetLinuxX64) == "heroes_lore_3_linux_x86_64.AppImage", "asset const linux");
    check(std::string(hl3::kAssetAndroidApk) == "heroes_lore_3_android_universal.apk", "asset const android");
    check(std::string(hl3::kAssetSwitchNro) == "heroes_lore_3.nro", "asset const switch");
#ifdef _WIN32
    check(hl3::get_target_asset_name() == "heroes_lore_3_windows_x64.zip", "target asset win64");
#endif
    hl3::set_active_i18n_language("en");
}

void test_net_http_helpers() {
    hl3::Bytes bin = {0x00, 0x01, 0x7F, 0x80, 0xFE, 0xFF, 'H', 'L', '3'};
    std::string b64 = hl3::base64_encode(bin);
    hl3::Bytes decoded;
    check(hl3::base64_decode(b64, decoded), "base64 decode ok");
    check(decoded == bin, "base64 roundtrip exact");
    check(hl3::url_encode("a b/c?d") == "a%20b%2Fc%3Fd", "url_encode");
}

void test_updater(const std::filesystem::path& tmp_root) {
    auto v = hl3::parse_semver("v1.0.3");
    check(v.has_value() && (*v)[0] == 1 && (*v)[1] == 0 && (*v)[2] == 3, "parse_semver");
    check(hl3::is_newer_version("v1.0.4", "1.0.3"), "is_newer_version true");
    check(!hl3::is_newer_version("1.0.3", "1.0.3"), "is_newer_version equal false");
    check(!hl3::is_newer_version("1.0.2", "1.0.3"), "is_newer_version older false");
    check(hl3::is_valid_release_download_url(
              "https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases/download/v1.0.4/"
              "heroes_lore_3_windows_x64.zip"),
          "valid release url");

    const std::string payload = R"({
      "tag_name": "v1.0.4",
      "name": "Heroes Lore 3 Recomp v1.0.4",
      "body": "Release notes",
      "html_url": "https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases/tag/v1.0.4",
      "published_at": "2026-11-14T00:00:00Z",
      "assets": [
        {
          "name": "heroes_lore_3_windows_x64.zip",
          "size": 123456,
          "browser_download_url": "https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases/download/v1.0.4/heroes_lore_3_windows_x64.zip"
        }
      ]
    })";
    hl3::UpdateReleaseInfo rel;
    std::string reason;
    check(hl3::parse_latest_release_payload(payload, rel, reason, "heroes_lore_3_windows_x64.zip"),
          "parse_latest_release_payload");
    check(rel.tag_name == "v1.0.4" && rel.asset_size == 123456, "parsed release fields");

    std::string ps = hl3::build_windows_updater_script(
        1234, tmp_root / "staged", tmp_root / "install", tmp_root / "rollback", "HeroesLore3.exe",
        tmp_root / "update.log");
    check(ps.find("Start-Process -FilePath") != std::string::npos, "updater script restarts exe");
    check(ps.find("'config'") != std::string::npos && ps.find("'data'") != std::string::npos,
          "updater script preserves config and data");

    auto mock_http = [&](const std::string& method, const std::string& url, const hl3::HttpHeaders&,
                         const std::string&, double) -> hl3::HttpResponse {
        (void)method;
        if (url.find("releases/latest") != std::string::npos) {
            return {200, payload, url, payload.size(), {}};
        }
        return {404, {}, url, 0, "not found"};
    };

    hl3::UpdateManager mgr("1.0.3", hl3::kGithubRepo, tmp_root / "config" / "services.json",
                           tmp_root / "updates", tmp_root / "install", 1, mock_http,
                           "heroes_lore_3_windows_x64.zip");
    check(mgr.start_check(true), "UpdateManager start_check");
    for (int i = 0; i < 100; ++i) {
        if (mgr.snapshot().phase != hl3::UpdatePhase::Checking) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto st = mgr.snapshot();
    check(st.phase == hl3::UpdatePhase::UpdateAvailable, "UpdateManager detected v1.0.4");
    hl3::UpdateReleaseInfo notice;
    check(mgr.consume_pending_notice(notice) && notice.tag_name == "v1.0.4", "consume_pending_notice");
    mgr.shutdown();
}

void test_cloud_save(const std::filesystem::path& tmp_root) {
    check(hl3::is_safe_save_path("internal/Documents/slot0.sav"), "safe save path ok");
    check(!hl3::is_safe_save_path("Documents/slot0.sav"), "reject ungrouped path");
    check(!hl3::is_safe_save_path("../secret.txt"), "reject parent traversal");
    check(!hl3::is_safe_save_path("/abs/path"), "reject abs path");

    std::filesystem::path data_root = tmp_root / "cloud_data";
    std::filesystem::path docs = data_root / "internal" / "Documents";
    std::filesystem::create_directories(docs);
    hl3::Bytes save0 = {'H', 'L', '3', '_', 'S', 'A', 'V', '0', 1, 2, 3, 4};
    check(hl3::write_file_bytes(docs / "save0.sav", save0.data(), save0.size()), "write sample save");

    hl3::SaveFileMap collected;
    std::string err;
    check(hl3::collect_local_saves(data_root, collected, &err), "collect_local_saves");
    check(collected.size() == 1, "collected 1 save file");

    std::string packed_json;
    check(hl3::pack_saves(collected, "windows", "1 file", packed_json, &err), "pack_saves");

    hl3::SaveFileMap unpacked;
    hl3::UnpackMeta meta;
    check(hl3::unpack_saves(packed_json, unpacked, meta, &err), "unpack_saves");
    check(meta.platform == "windows" && unpacked == collected, "pack/unpack roundtrip exact");

    std::filesystem::path restore_root = tmp_root / "restore_data";
    std::filesystem::path backup_dir;
    check(hl3::restore_saves(restore_root, unpacked, backup_dir, &err), "restore_saves");
    hl3::Bytes read_back;
    check(hl3::read_file_bytes(restore_root / "internal" / "Documents" / "save0.sav", read_back) &&
              read_back == save0,
          "restored save bytes exact");
}

void test_png_and_ui(const std::filesystem::path& root, const std::filesystem::path& tmp_root) {
    int rw = 0, rh = 0;
    hl3::Bytes rrgba;
    std::string err;
    check(hl3::load_png_rgba_file(root / "assets" / "ui_button_ribbon.png", rw, rh, rrgba, &err),
          "load ui_button_ribbon.png");
    check(rw == 608 && rh == 66 && rrgba.size() == size_t(608 * 66 * 4), "ribbon PNG dims 608x66");

    std::filesystem::path tmp_png = tmp_root / "roundtrip.png";
    hl3::Bytes img(16 * 16 * 4, 0);
    for (int i = 0; i < 16 * 16; ++i) {
        img[i * 4 + 0] = 0x12;
        img[i * 4 + 1] = 0xAB;
        img[i * 4 + 2] = 0xCD;
        img[i * 4 + 3] = 0xFF;
    }
    check(hl3::save_rgba_png_file(tmp_png, 16, 16, img.data(), false, false), "save temp PNG");
    int tw = 0, th = 0;
    hl3::Bytes trgba;
    check(hl3::load_png_rgba_file(tmp_png, tw, th, trgba, &err), "reload temp PNG");
    check(tw == 16 && th == 16 && trgba == img, "PNG save/load pixel roundtrip");

    hl3::RgbaCanvas osd = hl3::build_osd_canvas("60 FPS", "Frame Rate: 60 FPS", "Smooth mode",
                                               {110, 222, 255, 255}, 1.0f);
    check(osd.width() > 80 && osd.height() > 24 && !osd.empty(), "build_osd_canvas non-empty");

    hl3::RgbaCanvas btn = hl3::build_injected_button_canvas("PORT OPTIONS & CLOUD", rrgba, rw, rh);
    check(btn.width() == rw && btn.height() == rh && !btn.empty(), "build_injected_button_canvas dims");
}

void test_android_port_regressions() {
    // 1. Sensor / accelerometer / zero-button virtual joysticks must be filtered out cleanly.
    check(hl3::InputController::should_ignore_joystick_device("Android Accelerometer", 0, 0),
          "ignore Android Accelerometer 0-btn");
    check(hl3::InputController::should_ignore_joystick_device("Goldfish 3-axis Accelerometer", 2, 0),
          "ignore Accelerometer by name");
    check(hl3::InputController::should_ignore_joystick_device("Bosch Gyroscope Sensor", 0, 0),
          "ignore Gyroscope sensor");
    check(hl3::InputController::should_ignore_joystick_device("uinput-fpc", 1, 0),
          "ignore fingerprint virtual device");
    check(hl3::InputController::should_ignore_joystick_device("Unnamed Axis", 0, 0),
          "ignore 0-button 0-hat device");
    check(!hl3::InputController::should_ignore_joystick_device("Xbox Wireless Controller", 15, 1),
          "accept Xbox Wireless Controller");
    check(!hl3::InputController::should_ignore_joystick_device("Generic USB Joystick", 10, 0),
          "accept USB joystick with buttons");

    // 2. Options modal fills high-DPI mobile screens proportionally instead of a tiny 980x620 box.
    hl3::PortOptionsModal modal(nullptr, nullptr, {});
    hl3::UiRect phone_qhd = modal.compute_box(3120, 1440);
    check(phone_qhd.h >= 1200 && phone_qhd.w >= 1900, "QHD+ phone modal scales cleanly");
    check(phone_qhd.x == (3120 - phone_qhd.w) / 2 && phone_qhd.y == (1440 - phone_qhd.h) / 2,
          "QHD+ phone modal centered");
    hl3::UiRect phone_fhd = modal.compute_box(2400, 1080);
    check(phone_fhd.h >= 900 && phone_fhd.w >= 1450, "20:9 FHD+ phone modal scales cleanly");
    hl3::UiRect desktop = modal.compute_box(960, 640);
    check(desktop.w >= 800 && desktop.w <= 944 && desktop.h >= 540 && desktop.h <= 624,
          "Desktop 960x640 modal stays well-proportioned");

    // 3. Target aspect rect fills entire viewport for Auto/16:9 (used by pre-game backdrop + gameplay).
    hl3::RectI t_auto = hl3::compute_target_aspect_rect("auto", 3120, 1440);
    check(t_auto == (hl3::RectI{0, 0, 3120, 1440}), "auto target rect covers full 3120x1440");
    hl3::RectI t_169 = hl3::compute_target_aspect_rect("16:9", 3120, 1440);
    check(t_169.h == 1440 && t_169.w == 2560 && t_169.x == 280, "16:9 target rect on 3120x1440");
    hl3::RectI t_orig = hl3::compute_target_aspect_rect("original", 3120, 1440);
    check(t_orig.h == 1440 && t_orig.w == 2160 && t_orig.x == 480, "original target rect on 3120x1440");

    // 4. GLBridge tex_parameter_f routes filter params safely (no crash headless, GL_NEAREST preserved).
    hl3::GLBridge headless_gl(nullptr, 960, 640, "auto");
    headless_gl.tex_parameter_f(0x0DE1u, 0x2801u, static_cast<float>(0x2601u));  // GL_TEXTURE_MIN_FILTER <- GL_LINEAR
    headless_gl.tex_parameter_f(0x0DE1u, 0x2800u, static_cast<float>(0x2601u));  // GL_TEXTURE_MAG_FILTER <- GL_LINEAR
}

void test_modal_live_poll_redraws(const std::filesystem::path& tmp_root) {
    // Verify PortOptionsModal::poll() marks dirty_ = true immediately when async Cloud/Update workers finish
    // even after draw_gl() already cleared dirty_ while busy == true.
    std::filesystem::path data_root = tmp_root / "modal_live_data";
    std::filesystem::path docs = data_root / "internal" / "Documents";
    std::filesystem::create_directories(docs);
    hl3::Bytes sample_save = {'H', 'L', '3', 'T', 1, 2, 3, 4};
    check(hl3::write_file_bytes(docs / "save0.sav", sample_save.data(), sample_save.size()),
          "write modal live save");

    std::filesystem::path auth_path = tmp_root / "modal_auth.json";
    const std::string auth_json =
        R"({"app":"heroes_lore_3","refresh_token":"ref123","access_token":"acc123","expires_at":4102444800})";
    check(hl3::write_file_bytes(auth_path, auth_json.data(), auth_json.size()), "write modal auth tokens");

    std::string cur_mod = "2026-11-14T12:00:00Z";
    std::string cur_ver = "1";
    auto mock_cloud_http = [&](const std::string& method, const std::string& url, const hl3::HttpHeaders&,
                               const std::string&, double) -> hl3::HttpResponse {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        if (method == "GET" && url.find("mimeType") != std::string::npos) {
            return {200, R"({"files":[{"id":"folder1"}]})", url, 0, {}};
        }
        if (method == "GET" && url.find("drive/v3/files") != std::string::npos) {
            std::string body = std::string(R"({"files":[{"id":"file1","modifiedTime":")") + cur_mod +
                               R"(","description":"windows | 1 slot","version":")" + cur_ver +
                               R"(","parents":["folder1"]}]})";
            return {200, body, url, body.size(), {}};
        }
        if (method == "PATCH") {
            cur_mod = "2026-11-14T12:05:00Z";
            cur_ver = "2";
            return {200, R"({"id":"file1","name":"heroes_lore_3_save.json"})", url, 0, {}};
        }
        return {404, {}, url, 0, "unexpected"};
    };

    hl3::CloudSaveManager cloud(data_root, auth_path, mock_cloud_http, []() { return 1794690000.0; });
    hl3::PortOptionsModal::Callbacks cb;
    cb.get_fps = []() { return 60; };
    cb.get_aspect = []() { return std::string("auto"); };
    cb.get_language = []() { return std::string("pt-BR"); };

    hl3::PortOptionsModal modal(&cloud, nullptr, cb);
    hl3::GLBridge headless_gl(nullptr, 960, 640);

    // Phase A: open Cloud page -> refresh_backup() starts async check
    modal.open(hl3::ModalPage::Cloud);
    check(cloud.get_status().message_key == "cloud.msg.checking", "refresh starts with checking message immediately");
    modal.poll();
    modal.draw_gl(headless_gl, 960, 640);
    check(!modal.needs_redraw(), "draw_gl clears dirty while refresh busy");

    for (int i = 0; i < 200; ++i) {
        if (!cloud.get_status().busy) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(!cloud.get_status().busy && cloud.get_status().message_key == "cloud.msg.found",
          "refresh_backup finished with found");
    modal.poll();
    check(modal.needs_redraw(), "poll marks dirty_ = true immediately when refresh finishes");
    modal.draw_gl(headless_gl, 960, 640);
    check(!modal.needs_redraw(), "draw_gl clears dirty after refresh result");

    // Phase B: simulate user selecting Upload -> ConfirmUpload -> Confirm
    modal.handle_confirm();  // focus 0 = ask_upload -> ModalPage::ConfirmUpload
    check(modal.page() == hl3::ModalPage::ConfirmUpload, "entered ConfirmUpload page");
    modal.handle_nav("left");  // move focus from Cancel (1) to Confirm (0)
    modal.handle_confirm();    // confirm_yes -> upload_backup() & ModalPage::Cloud
    check(modal.page() == hl3::ModalPage::Cloud, "returned to Cloud page on upload");
    check(cloud.get_status().message_key == "cloud.msg.uploading", "upload starts with uploading message immediately");

    modal.poll();
    modal.draw_gl(headless_gl, 960, 640);
    check(!modal.needs_redraw(), "draw_gl clears dirty while upload busy");

    for (int i = 0; i < 200; ++i) {
        if (!cloud.get_status().busy) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(!cloud.get_status().busy && cloud.get_status().message_key == "cloud.msg.uploaded",
          "upload_backup finished with uploaded");
    check(cloud.get_status().backup_date == "2026-11-14T12:05:00Z", "upload refreshed backup timestamp");
    modal.poll();
    check(modal.needs_redraw(), "poll marks dirty_ = true immediately when upload finishes");
    cloud.wait_idle();
}

}  // namespace

int main() {
    std::filesystem::path root = hl3::find_project_or_exe_root();
    std::filesystem::path tmp_root = std::filesystem::temp_directory_path() / "hl3_cpp_platform_tests";
    std::error_code ec;
    std::filesystem::remove_all(tmp_root, ec);
    std::filesystem::create_directories(tmp_root, ec);

    test_i18n();
    test_net_http_helpers();
    test_updater(tmp_root);
    test_cloud_save(tmp_root);
    test_png_and_ui(root, tmp_root);
    test_android_port_regressions();
    test_modal_live_poll_redraws(tmp_root);

    std::filesystem::remove_all(tmp_root, ec);

    if (g_fails == 0) {
        std::printf("All C++ platform service tests PASSED!\n");
        return 0;
    }
    std::fprintf(stderr, "Platform service tests failed: %d failure(s)\n", g_fails);
    return 1;
}

