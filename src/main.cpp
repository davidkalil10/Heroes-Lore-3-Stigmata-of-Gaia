#include "audio/audio_mixer.h"
#include "core/bundle.h"
#include "core/common.h"
#include "core/runtime.h"
#include "hle/gl_bridge.h"
#include "platform/cloud_save.h"
#include "platform/display_config.h"
#include "platform/i18n.h"
#include "platform/input_controller.h"
#include "platform/options_modal.h"
#include "platform/png_codec.h"
#include "platform/updater.h"

#include <SDL.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif
#ifdef __SWITCH__
#include <switch.h>
#include <sys/stat.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

namespace {

using namespace hl3;

struct CliOptions {
    int scale = 2;
    bool fullscreen = false;
    bool headless = false;
    std::string aspect;
    int fps = 0;
    int frames = 0;
    std::string lang;
    std::filesystem::path ipa_path;
};

void print_usage() {
    std::printf(
        "Heroes Lore 3: Stigmata of Gaia Recomp v%s (PC Native C++)\n"
        "Usage: HeroesLore3 [options]\n"
        "  --scale N        Window scale factor (default: 2 -> 960x640)\n"
        "  --fullscreen     Launch in fullscreen mode\n"
        "  --headless       Hide window (for automated/CI testing)\n"
        "  --aspect MODE    original | 3:2 | 16:9 | 16:10 | 20:9 | 4:3 | auto\n"
        "  --fps N          Target FPS cap (30 or 60)\n"
        "  --frames N       Run for N frames, capture screenshot, then exit\n"
        "  --lang CODE      Initial language override (en or pt-BR)\n"
        "  --ipa PATH       Explicit path to assets/game.ipa\n"
        "  -h, --help       Show this help message\n",
        kPortVersion);
}

bool parse_cli(int argc, char** argv, CliOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            print_usage();
            std::exit(0);
        } else if (a == "--fullscreen") {
            opts.fullscreen = true;
        } else if (a == "--headless") {
            opts.headless = true;
        } else if (a == "--scale" && i + 1 < argc) {
            opts.scale = std::max(1, std::atoi(argv[++i]));
        } else if (a == "--aspect" && i + 1 < argc) {
            opts.aspect = argv[++i];
        } else if (a == "--fps" && i + 1 < argc) {
            opts.fps = std::atoi(argv[++i]);
        } else if (a == "--frames" && i + 1 < argc) {
            opts.frames = std::max(0, std::atoi(argv[++i]));
        } else if (a == "--lang" && i + 1 < argc) {
            opts.lang = argv[++i];
        } else if (a == "--ipa" && i + 1 < argc) {
            opts.ipa_path = std::filesystem::u8path(argv[++i]);
        } else {
            std::fprintf(stderr, "[HL3 PC] Unknown or incomplete argument: %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

std::filesystem::path resolve_asset_file(const std::filesystem::path& root, const std::string& filename) {
    std::error_code ec;
    std::filesystem::path p1 = root / "assets" / filename;
    if (std::filesystem::exists(p1, ec)) return p1;
    if (filename == "icon.png") {
        std::filesystem::path p2 = root / "reference" / "extracted_ipa" / "Payload" / "Heroes Lore.app" / "Icon.png";
        if (std::filesystem::exists(p2, ec)) return p2;
    }
    return {};
}

void write_fatal_log(const std::filesystem::path& root, const std::string& msg) {
    try {
        std::filesystem::path log_path = root.empty() ? std::filesystem::path("hl3_error.log") : (root / "hl3_error.log");
        std::ofstream f(log_path, std::ios::trunc);
        if (f) f << msg << "\n";
    } catch (...) {
    }
}

class PcHost {
public:
    explicit PcHost(CliOptions opts) : opts_(std::move(opts)) {}
    ~PcHost() { shutdown(); }

    int run() {
        root_dir_ = find_project_or_exe_root();
        if (!opts_.ipa_path.empty() && std::filesystem::exists(opts_.ipa_path)) {
            auto p = opts_.ipa_path.parent_path();
            if (p.filename() == "assets") root_dir_ = p.parent_path();
        }

        DisplayConfig disk_cfg = (opts_.frames == 0) ? load_display_config() : DisplayConfig{};
        cfg_.aspect_mode = !opts_.aspect.empty() ? normalize_aspect_mode(opts_.aspect)
                                                 : normalize_aspect_mode(disk_cfg.aspect_mode);
        cfg_.target_fps = (opts_.fps > 0) ? normalize_fps(opts_.fps) : normalize_fps(disk_cfg.target_fps);
        cfg_.language = !opts_.lang.empty() ? normalize_language(opts_.lang) : normalize_language(disk_cfg.language);
        set_active_i18n_language(cfg_.language);

        scale_ = std::max(1, opts_.scale);
        fullscreen_ = opts_.fullscreen;
        if (cfg_.aspect_mode == "16:9" && opts_.frames == 0) {
            win_w_ = static_cast<int>(std::lround(320.0 * (16.0 / 9.0) * scale_));
            win_h_ = 320 * scale_;
        } else {
            win_w_ = 480 * scale_;
            win_h_ = 320 * scale_;
        }
        windowed_w_ = win_w_;
        windowed_h_ = win_h_;

        if (!init_window()) return 1;
        init_services();
        if (!init_runtime()) return boot_cancelled_ ? 0 : 2;

        input_.set_assets_root(root_dir_.empty() ? std::filesystem::path("assets") : (root_dir_ / "assets"));
        input_.set_host_actions(build_actions());
        input_.init_controllers();

        running_ = true;
        if (opts_.frames == 0 && updater_) {
            updater_->maybe_start_startup_check();
        }

        std::printf(
            "[HL3 PC] Main loop started (Port v%s, %d FPS, Aspect=%s, Lang=%s). "
            "F7=Language, F8=FPS, F9=Aspect, F11=Fullscreen, F12=Screenshot, Esc=Back/Pause.\n",
            kPortVersion, cfg_.target_fps, cfg_.aspect_mode.c_str(), cfg_.language.c_str());

        int frame = 0;
        using clock = std::chrono::steady_clock;
        auto next_tick = clock::now();

        while (running_) {
#ifdef __SWITCH__
            if (!appletMainLoop()) {
                running_ = false;
                break;
            }
#endif
            step_frame(frame);
            if (app_backgrounded_) {
                SDL_Delay(50);
                continue;
            }
            ++frame;

            if (opts_.frames > 0 && frame >= opts_.frames) {
                std::printf("[HL3 PC] Reached target %d frames in test mode.\n", opts_.frames);
                if (!screenshot_saved_this_run_) {
                    capture_screenshot_now();
                }
                break;
            }

            if (!opts_.headless) {
                auto period = std::chrono::microseconds(1000000 / std::max(1, cfg_.target_fps));
                next_tick += period;
                auto now = clock::now();
                if (now + period < next_tick) {
                    next_tick = now + period;
                }
                if (next_tick > now) {
                    auto rem_ms = std::chrono::duration_cast<std::chrono::milliseconds>(next_tick - now).count();
                    if (rem_ms > 2) {
                        SDL_Delay(static_cast<Uint32>(rem_ms - 1));
                    }
                    while (clock::now() < next_tick) {
                        std::this_thread::yield();
                    }
                } else {
                    next_tick = now;
                }
            }
        }

        shutdown();
        return 0;
    }

private:
    bool init_window() {
#ifdef _WIN32
        if (HMODULE shell = LoadLibraryW(L"shell32.dll")) {
            using PfnAppId = HRESULT(WINAPI*)(PCWSTR);
            auto fn = reinterpret_cast<PfnAppId>(GetProcAddress(shell, "SetCurrentProcessExplicitAppUserModelID"));
            if (fn) fn(L"DavidBraga.HeroesLore3.Recomp.PC");
        }
#endif
#ifdef __ANDROID__
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
        fullscreen_ = true;
#else
#ifdef __SWITCH__
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        fullscreen_ = true;
#endif
        SDL_SetMainReady();
#endif
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
            std::fprintf(stderr, "[HL3 PC] SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
        SDL_GameControllerEventState(SDL_ENABLE);
        SDL_JoystickEventState(SDL_ENABLE);

#if defined(__ANDROID__) || defined(__SWITCH__)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

        Uint32 flags = SDL_WINDOW_OPENGL;
        if (opts_.headless) {
            flags |= SDL_WINDOW_HIDDEN;
        } else {
            flags |= SDL_WINDOW_SHOWN;
        }
        if (fullscreen_) {
            SDL_DisplayMode dm{};
            if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
                win_w_ = dm.w;
                win_h_ = dm.h;
            }
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        std::string title =
#if defined(__ANDROID__)
            std::string("Heroes Lore 3: Stigmata of Gaia Recomp v") + kPortVersion + " (Android)";
#elif defined(__SWITCH__)
            std::string("Heroes Lore 3: Stigmata of Gaia Recomp v") + kPortVersion + " (Switch)";
#elif defined(__linux__)
            std::string("Heroes Lore 3: Stigmata of Gaia Recomp v") + kPortVersion + " (Linux Native C++)";
#else
            std::string("Heroes Lore 3: Stigmata of Gaia Recomp v") + kPortVersion + " (PC Native C++)";
#endif
        window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w_, win_h_,
                                   flags);
        if (!window_) {
            std::fprintf(stderr, "[HL3 PC] SDL_CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }

        apply_window_icons();

        gl_ctx_ = SDL_GL_CreateContext(window_);
        if (!gl_ctx_) {
            std::fprintf(stderr, "[HL3 PC] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
            return false;
        }
        SDL_GL_MakeCurrent(window_, gl_ctx_);
        if (opts_.headless || opts_.frames > 0) {
            SDL_GL_SetSwapInterval(0);
        } else if (SDL_GL_SetSwapInterval(1) != 0) {
            SDL_GL_SetSwapInterval(0);
        }

        SDL_GetWindowSize(window_, &win_w_, &win_h_);
        if (!opts_.headless) {
            SDL_ShowWindow(window_);
            SDL_RaiseWindow(window_);
            clear_and_swap_boot_backdrop();
            clear_and_swap_boot_backdrop();
            pump_boot_events();
        }
        return true;
    }

    void clear_and_swap_boot_backdrop() {
        if (!window_ || !gl_ctx_) return;
        using PfnViewport = void(APIENTRY*)(int, int, int, int);
        using PfnClearColor = void(APIENTRY*)(float, float, float, float);
        using PfnClear = void(APIENTRY*)(unsigned int);
        auto vp = reinterpret_cast<PfnViewport>(SDL_GL_GetProcAddress("glViewport"));
        auto cc = reinterpret_cast<PfnClearColor>(SDL_GL_GetProcAddress("glClearColor"));
        auto cl = reinterpret_cast<PfnClear>(SDL_GL_GetProcAddress("glClear"));
        if (vp && cc && cl) {
            vp(0, 0, win_w_, win_h_);
            cc(0.04f, 0.07f, 0.14f, 1.0f);
            cl(0x00004000u);  // GL_COLOR_BUFFER_BIT
        }
        SDL_GL_SwapWindow(window_);
    }

    bool pump_boot_events() {
        SDL_Event ev;
        bool need_redraw = false;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                boot_cancelled_ = true;
                return false;
            }
            if (ev.type == SDL_WINDOWEVENT) {
                if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                    boot_cancelled_ = true;
                    return false;
                }
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED ||
                    ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    apply_client_size(ev.window.data1, ev.window.data2);
                    need_redraw = true;
                } else if (ev.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    need_redraw = true;
                }
            }
        }
        if (need_redraw && !opts_.headless) {
            if (gl_ && gl_->is_gl_ready() && boot_progress_ > 0.0f) {
                draw_boot_progress(boot_progress_);
            } else {
                clear_and_swap_boot_backdrop();
            }
        }
        return true;
    }

    void draw_boot_progress(float progress) {
        boot_progress_ = std::max(0.0f, std::min(1.0f, progress));
        if (!gl_ || !gl_->is_gl_ready() || !window_ || opts_.headless) return;
        gl_->overlay_begin_window(win_w_, win_h_);
        gl_->clear_color(0.04f, 0.07f, 0.14f, 1.0f);
        gl_->clear(0x00004000u);

        float ui_scale = std::max(1.0f, std::min(2.4f, static_cast<float>(win_h_) / 420.0f));
        float bar_w = std::min(static_cast<float>(win_w_) * 0.52f, 340.0f * ui_scale);
        float bar_h = std::max(12.0f, 14.0f * ui_scale);
        float bx = (static_cast<float>(win_w_) - bar_w) * 0.5f;
        float by = (static_cast<float>(win_h_) - bar_h) * 0.5f;

        gl_->overlay_fill_rect(bx - 4.0f, by - 4.0f, bar_w + 8.0f, bar_h + 8.0f, 0.07f, 0.10f, 0.19f, 0.96f);
        float fill_w = std::max(0.0f, (bar_w - 4.0f) * boot_progress_);
        if (fill_w > 0.5f) {
            gl_->overlay_fill_rect(bx + 2.0f, by + 2.0f, fill_w, bar_h - 4.0f, 0.25f, 0.62f, 0.96f, 1.0f);
        }
        gl_->overlay_outline_rect(bx - 4.0f, by - 4.0f, bar_w + 8.0f, bar_h + 8.0f,
                                  std::max(1.0f, 1.5f * ui_scale), 0.90f, 0.76f, 0.36f, 0.95f);
        gl_->overlay_end();
        SDL_GL_SwapWindow(window_);
    }

    void apply_window_icons() {
#if !defined(__ANDROID__) && !defined(__SWITCH__)
        std::filesystem::path png_path = resolve_asset_file(root_dir_, "icon.png");
        if (!png_path.empty()) {
            int w = 0, h = 0;
            Bytes rgba;
            if (load_png_rgba_file(png_path, w, h, rgba) && w > 0 && h > 0) {
                SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
                    rgba.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
                if (surf) {
                    SDL_SetWindowIcon(window_, surf);
                    SDL_FreeSurface(surf);
                }
            }
        }
#ifdef _WIN32
        std::filesystem::path ico_path = resolve_asset_file(root_dir_, "icon.ico");
        if (ico_path.empty()) return;
        SDL_SysWMinfo wm{};
        SDL_VERSION(&wm.version);
        if (!SDL_GetWindowWMInfo(window_, &wm) || wm.subsystem != SDL_SYSWM_WINDOWS) return;
        HWND hwnd = wm.info.win.window;
        if (!hwnd) return;

        int sm_cx = GetSystemMetrics(SM_CXSMICON);
        int sm_cy = GetSystemMetrics(SM_CYSMICON);
        int bg_cx = GetSystemMetrics(SM_CXICON);
        int bg_cy = GetSystemMetrics(SM_CYICON);
        if (sm_cx <= 0) sm_cx = 16;
        if (sm_cy <= 0) sm_cy = 16;
        if (bg_cx <= 0) bg_cx = 32;
        if (bg_cy <= 0) bg_cy = 32;

        std::wstring wp = ico_path.wstring();
        HANDLE hbig = LoadImageW(nullptr, wp.c_str(), IMAGE_ICON, bg_cx, bg_cy, LR_LOADFROMFILE);
        HANDLE hsm = LoadImageW(nullptr, wp.c_str(), IMAGE_ICON, sm_cx, sm_cy, LR_LOADFROMFILE);
        if (!hbig) hbig = LoadImageW(nullptr, wp.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        if (!hsm) hsm = hbig;

        if (hbig) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hbig));
            SetClassLongPtrW(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(hbig));
        }
        if (hsm) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hsm));
            SetClassLongPtrW(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(hsm));
        }
        HWND con = GetConsoleWindow();
        if (con) {
            if (hbig) SendMessageW(con, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hbig));
            if (hsm) SendMessageW(con, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hsm));
        }
        if (win32_big_icon_) DestroyIcon(static_cast<HICON>(win32_big_icon_));
        if (win32_sm_icon_ && win32_sm_icon_ != win32_big_icon_) DestroyIcon(static_cast<HICON>(win32_sm_icon_));
        win32_big_icon_ = hbig;
        win32_sm_icon_ = (hsm != hbig) ? hsm : nullptr;
#endif
#endif
    }

    void init_services() {
        if (opts_.frames > 0) {
            cloud_.reset();
            updater_.reset();
            modal_.reset();
            return;
        }
        cloud_ = std::make_unique<CloudSaveManager>();
        updater_ = std::make_unique<UpdateManager>(
            kPortVersion, kGithubRepo, std::filesystem::path{}, std::filesystem::path{},
            std::filesystem::path{}, -1, HttpCallable{}, std::string{}, [this]() { running_ = false; });

        PortOptionsModal::Callbacks cb;
        cb.get_fps = [this]() { return cfg_.target_fps; };
        cb.set_fps = [this](int fps, bool notify) { set_target_fps(fps, notify); };
        cb.get_aspect = [this]() { return cfg_.aspect_mode; };
        cb.set_aspect = [this](const std::string& mode, bool resize_win, bool notify) {
            set_aspect_mode(mode, resize_win, notify);
        };
        cb.get_language = [this]() { return cfg_.language; };
        cb.set_language = [this](const std::string& lang, bool notify) { set_language(lang, notify); };
        cb.on_cloud_restore_ready = [this]() { reload_runtime_to_title(); };
        cb.flush_before_upload = [this]() {
            if (rt_) rt_->flush_open_files();
        };
        modal_ = std::make_unique<PortOptionsModal>(cloud_.get(), updater_.get(), std::move(cb));
    }

    bool init_runtime() {
        boot_cancelled_ = false;
        boot_progress_ = 0.0f;
        std::printf("[HL3 PC] Initializing Mach-O ARMv6 runtime and OpenGL bridge...\n");
        if (!pump_boot_events()) return false;
        rt_ = std::make_unique<Runtime>();
        std::string err;
        std::filesystem::path target = !opts_.ipa_path.empty() ? opts_.ipa_path : root_dir_;
        if (!rt_->init(target, {}, false, &err)) {
            std::fprintf(stderr, "[HL3 PC] Runtime init failed: %s\n", err.c_str());
            write_fatal_log(find_writable_data_root(), "Runtime init failed: " + err);
            return false;
        }
        rt_->bundle().set_language(cfg_.language);
        if (!pump_boot_events()) return false;

        gl_ = std::make_unique<GLBridge>(&rt_->machine(), win_w_, win_h_, cfg_.aspect_mode);
        if (!gl_->init_gl()) {
            std::fprintf(stderr, "[HL3 PC] GLBridge init_gl failed\n");
            write_fatal_log(find_writable_data_root(), "GLBridge init_gl failed");
            return false;
        }
        draw_boot_progress(0.45f);
        if (!pump_boot_events()) return false;

        audio_ = std::make_unique<AudioMixer>(find_writable_data_root() / "config" / "audio.json");
        audio_->init_device();

        rt_->set_gl_bridge(gl_.get());
        rt_->set_audio_mixer(audio_.get());
        draw_boot_progress(0.65f);
        if (!pump_boot_events()) return false;

        rt_->present_hook = [this]() {
            if (gl_ && rt_) {
                input_.draw_injected_port_buttons(*gl_, *rt_, cfg_.language);
                if (!modal_ || !modal_->is_open()) {
                    input_.draw_menu_cursor(*gl_, *rt_);
                }
                input_.draw_osd(*gl_, win_w_, win_h_);
                if (modal_ && modal_->is_open()) {
                    modal_->draw_gl(*gl_, win_w_, win_h_);
                }
            }
            if (screenshot_requested_) {
                screenshot_requested_ = false;
                capture_screenshot_now();
            }
            SDL_GL_SwapWindow(window_);
        };

        std::printf("[HL3 PC] Booting to UIApplicationMain...\n");
        rt_->boot_to_app_main();
        draw_boot_progress(0.85f);
        if (!pump_boot_events()) return false;

        std::printf("[HL3 PC] Launching AppDelegate and core views...\n");
        rt_->run_launch();
        boot_progress_ = 0.0f;
        if (!pump_boot_events()) return false;
        std::printf("[HL3 PC] TouchResponder active at 0x%08x\n", rt_->touch_responder);
        return true;
    }

    void reload_runtime_to_title() {
        std::printf("[HL3 PC] Reloading runtime to Title Screen after Cloud Restore...\n");
        if (audio_) {
            audio_->save_config();
            audio_->stop_bgm();
            audio_->shutdown_device();
        }
        if (gl_) {
            input_.reset_gl(*gl_);
            if (modal_) modal_->reset_gl(*gl_);
            gl_->shutdown_gl();
        }
        audio_.reset();
        gl_.reset();
        rt_.reset();
        input_.reset_runtime_state();

        if (!init_runtime()) {
            running_ = false;
            return;
        }
        input_.show_osd(tr("osd.restore.badge", cfg_.language), tr("osd.restore.title", cfg_.language),
                        tr("osd.restore.sub", cfg_.language), {104, 228, 168, 255}, 3.0);
    }

    bool is_window_maximized() const {
        if (fullscreen_ || !window_) return false;
        return (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0u;
    }

    void save_prefs_if_interactive() const {
        if (opts_.frames == 0) {
            save_display_config(cfg_);
        }
    }

    void apply_client_size(int new_w, int new_h) {
        new_w = std::max(320, new_w);
        new_h = std::max(240, new_h);
        if (new_w == win_w_ && new_h == win_h_) return;
        win_w_ = new_w;
        win_h_ = new_h;
        if (!fullscreen_ && !is_window_maximized()) {
            windowed_w_ = win_w_;
            windowed_h_ = win_h_;
        }
        if (gl_) gl_->update_viewport(win_w_, win_h_);
    }

    void sync_window_size() {
        if (opts_.frames > 0 || !window_) return;
        int w = 0, h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        if (w >= 320 && h >= 240 && (w != win_w_ || h != win_h_)) {
            apply_client_size(w, h);
        }
    }

    void set_aspect_mode(const std::string& mode, bool resize_window, bool notify) {
        cfg_.aspect_mode = normalize_aspect_mode(mode);
        if (resize_window && !fullscreen_ && !is_window_maximized() && opts_.frames == 0 && window_) {
            if (cfg_.aspect_mode == "16:9") {
                int nw = static_cast<int>(std::lround(320.0 * (16.0 / 9.0) * scale_));
                int nh = 320 * scale_;
                SDL_SetWindowSize(window_, nw, nh);
                apply_client_size(nw, nh);
            } else if (cfg_.aspect_mode == "original") {
                int nw = 480 * scale_;
                int nh = 320 * scale_;
                SDL_SetWindowSize(window_, nw, nh);
                apply_client_size(nw, nh);
            }
        }
        if (gl_) gl_->set_aspect_mode(cfg_.aspect_mode);
        save_prefs_if_interactive();
        if (notify) {
            std::string key = (cfg_.aspect_mode == "auto" || cfg_.aspect_mode == "16:9" ||
                               cfg_.aspect_mode == "original")
                                  ? cfg_.aspect_mode
                                  : std::string(kDefaultAspectMode);
            RgbaColor acc = {245, 204, 82, 255};
            if (key == "auto") acc = {104, 228, 168, 255};
            else if (key == "16:9") acc = {110, 222, 255, 255};
            input_.show_osd(tr("osd.aspect." + key + ".badge", cfg_.language),
                            tr("osd.aspect." + key + ".title", cfg_.language),
                            tr("osd.aspect." + key + ".sub", cfg_.language), acc, 2.4);
        }
        if (gl_) {
            std::printf("[HL3 PC] Aspect Mode: %s (%.0fx%.0f)\n", cfg_.aspect_mode.c_str(), gl_->game_w(),
                        gl_->game_h());
        }
    }

    void set_target_fps(int fps, bool notify) {
        cfg_.target_fps = normalize_fps(fps);
        save_prefs_if_interactive();
        if (notify) {
            if (cfg_.target_fps == 60) {
                input_.show_osd("60 FPS", tr("osd.fps.60.title", cfg_.language),
                                tr("osd.fps.60.sub", cfg_.language), {110, 222, 255, 255}, 2.4);
            } else {
                input_.show_osd("30 FPS", tr("osd.fps.30.title", cfg_.language),
                                tr("osd.fps.30.sub", cfg_.language), {245, 204, 82, 255}, 2.4);
            }
        }
        std::printf("[HL3 PC] Target FPS: %d\n", cfg_.target_fps);
    }

    void set_language(const std::string& lang, bool notify) {
        cfg_.language = set_active_i18n_language(normalize_language(lang));
        save_prefs_if_interactive();
        input_.invalidate_language_cache();
        if (modal_) modal_->invalidate_cache();
        if (rt_) input_.apply_guest_language_live(*rt_, cfg_.language);
        if (notify) {
            input_.show_osd(tr("osd.lang.badge", cfg_.language), tr("osd.lang.title", cfg_.language),
                            tr("osd.lang.sub", cfg_.language), {245, 204, 82, 255}, 2.4);
        }
        std::printf("[HL3 PC] Language: %s\n", cfg_.language.c_str());
    }

    void toggle_fullscreen() {
        if (!window_) return;
        fullscreen_ = !fullscreen_;
        if (fullscreen_) {
            if (!is_window_maximized()) {
                windowed_w_ = win_w_;
                windowed_h_ = win_h_;
            }
            SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
        } else {
            SDL_SetWindowFullscreen(window_, 0);
            SDL_SetWindowSize(window_, std::max(320, windowed_w_), std::max(240, windowed_h_));
        }
        int nw = 0, nh = 0;
        SDL_GetWindowSize(window_, &nw, &nh);
        if (nw >= 320 && nh >= 240) {
            win_w_ = nw;
            win_h_ = nh;
        }
        if (gl_) gl_->update_viewport(win_w_, win_h_);
        input_.show_osd(tr("osd.fullscreen.badge", cfg_.language),
                        tr(fullscreen_ ? "osd.fullscreen.on.title" : "osd.fullscreen.off.title", cfg_.language),
                        tr_fmt("osd.fullscreen.sub", {{"aspect", cfg_.aspect_mode}}, cfg_.language),
                        fullscreen_ ? RgbaColor{104, 228, 168, 255} : RgbaColor{245, 204, 82, 255}, 2.4);
    }

    void capture_screenshot_now() {
        if (!gl_) return;
        int w = 0, h = 0;
        Bytes rgba;
        if (!gl_->read_framebuffer_rgba(w, h, rgba)) return;

        std::error_code ec;
        std::filesystem::path out_dir = (!root_dir_.empty() && std::filesystem::exists(root_dir_ / "reference", ec))
                                            ? (root_dir_ / "reference" / "analysis")
                                            : (find_writable_data_root() / "screenshots");
        std::filesystem::create_directories(out_dir, ec);

        std::time_t now = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &now);
#else
        localtime_r(&now, &tm);
#endif
        char ts[32];
        std::snprintf(ts, sizeof(ts), "%04d%02d%02d_%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        std::filesystem::path out_path = out_dir / (std::string("screenshot_") + ts + ".png");
        if (save_rgba_png_file(out_path, w, h, rgba.data(), true, true)) {
            screenshot_saved_this_run_ = true;
            std::printf("[HL3 PC] Screenshot saved to: %s\n", out_path.u8string().c_str());
        }
    }

    InputController::HostActions build_actions() {
        InputController::HostActions a;
        a.toggle_language = [this]() {
            set_language((cfg_.language == "en") ? "pt-BR" : "en", true);
        };
        a.toggle_fps = [this]() {
            set_target_fps((cfg_.target_fps == 30) ? 60 : 30, true);
        };
        a.cycle_aspect = [this]() {
            int cur_idx = 0;
            for (int i = 0; i < static_cast<int>(kUiAspectCycle.size()); ++i) {
                if (cfg_.aspect_mode == kUiAspectCycle[i]) {
                    cur_idx = i;
                    break;
                }
            }
            const char* next_mode = kUiAspectCycle[(cur_idx + 1) % kUiAspectCycle.size()];
            set_aspect_mode(next_mode, true, true);
        };
        a.toggle_fullscreen = [this]() { toggle_fullscreen(); };
        a.request_screenshot = [this]() { screenshot_requested_ = true; };
        a.request_quit = [this]() { running_ = false; };
        a.on_window_resized = [this](int w, int h) { apply_client_size(w, h); };
        a.on_window_sync_check = [this]() { sync_window_size(); };
        a.open_port_modal = [this]() {
            if (modal_) modal_->open(ModalPage::Options);
        };
        a.get_language = [this]() { return cfg_.language; };
        return a;
    }

    void step_frame(int frame) {
        if (!rt_) return;
        input_.tick_timers_and_touches(*rt_);

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_APP_WILLENTERBACKGROUND) {
                app_backgrounded_ = true;
                if (audio_) {
                    audio_->set_paused(true);
                    audio_->save_config();
                }
                if (rt_) rt_->flush_open_files();
                save_prefs_if_interactive();
                continue;
            }
            if (ev.type == SDL_APP_DIDENTERFOREGROUND) {
                app_backgrounded_ = false;
                if (audio_) audio_->set_paused(false);
                if (window_) {
                    int w = 0, h = 0;
                    SDL_GetWindowSize(window_, &w, &h);
                    if (w >= 320 && h >= 240) apply_client_size(w, h);
                }
                continue;
            }
            if (ev.type == SDL_APP_TERMINATING) {
                running_ = false;
                break;
            }
            if (!input_.process_sdl_event(ev, *rt_, modal_.get(), win_w_, win_h_)) {
                break;
            }
        }
        if (!running_ || app_backgrounded_) return;

        input_.update_continuous_input(*rt_, modal_.get());
        if (modal_) modal_->poll();
        sync_window_size();

        input_.patch_guest_about_credits(*rt_, cfg_.language);
        if (updater_) {
            UpdateReleaseInfo notice;
            if (updater_->consume_pending_notice(notice)) {
                input_.show_osd(tr("osd.update.badge", cfg_.language),
                                tr_fmt("osd.update.title", {{"tag", notice.tag_name}}, cfg_.language),
                                tr("osd.update.sub", cfg_.language), {104, 228, 168, 255}, 4.5);
            }
        }

        EngineState st = input_.get_engine_state(*rt_);
        if (st.app_state == 1 && st.form_id == 113 && st.active_popup_id == 0 && st.main_form) {
            input_.patch_help_about_layout(*rt_, st.main_form);
        }
        if (gl_) {
            gl_->set_world_active(st.app_state != 0 && st.app_state != 1 && st.app_state != 256);
            gl_->begin_frame();
        }
        if (opts_.frames > 0 && frame + 1 >= opts_.frames) {
            screenshot_requested_ = true;
        }
        rt_->send(rt_->delegate_obj(), "onTimer:", {0u});
    }

    void shutdown() {
        if (shutdown_done_) return;
        shutdown_done_ = true;

        if (updater_) updater_->shutdown();
        if (audio_) {
            audio_->save_config();
            audio_->shutdown_device();
        }
        save_prefs_if_interactive();

        if (gl_) {
            input_.reset_gl(*gl_);
            if (modal_) modal_->reset_gl(*gl_);
            gl_->shutdown_gl();
        }
        input_.shutdown_controllers();

        audio_.reset();
        gl_.reset();
        rt_.reset();
        modal_.reset();
        cloud_.reset();
        updater_.reset();

#ifdef _WIN32
        if (win32_big_icon_) {
            DestroyIcon(static_cast<HICON>(win32_big_icon_));
            win32_big_icon_ = nullptr;
        }
        if (win32_sm_icon_) {
            DestroyIcon(static_cast<HICON>(win32_sm_icon_));
            win32_sm_icon_ = nullptr;
        }
#endif
        if (gl_ctx_) {
            SDL_GL_DeleteContext(gl_ctx_);
            gl_ctx_ = nullptr;
        }
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        SDL_Quit();
    }

    CliOptions opts_;
    std::filesystem::path root_dir_;
    DisplayConfig cfg_;
    int scale_ = 2;
    int win_w_ = 960;
    int win_h_ = 640;
    int windowed_w_ = 960;
    int windowed_h_ = 640;
    bool fullscreen_ = false;
    bool running_ = false;
    bool app_backgrounded_ = false;
    bool boot_cancelled_ = false;
    float boot_progress_ = 0.0f;
    bool screenshot_requested_ = false;
    bool screenshot_saved_this_run_ = false;
    bool shutdown_done_ = false;

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_ctx_ = nullptr;
#ifdef _WIN32
    HANDLE win32_big_icon_ = nullptr;
    HANDLE win32_sm_icon_ = nullptr;
#endif

    std::unique_ptr<Runtime> rt_;
    std::unique_ptr<GLBridge> gl_;
    std::unique_ptr<AudioMixer> audio_;
    std::unique_ptr<CloudSaveManager> cloud_;
    std::unique_ptr<UpdateManager> updater_;
    std::unique_ptr<PortOptionsModal> modal_;
    InputController input_;
};

#ifdef __SWITCH__
struct SwitchRuntimeGuard {
    bool romfs_ok = false;
    bool socket_ok = false;
    std::FILE* log_fp = nullptr;

    SwitchRuntimeGuard() {
        mkdir("sdmc:/switch", 0777);
        mkdir("sdmc:/switch/heroes_lore_3", 0777);
        log_fp = std::fopen("sdmc:/switch/heroes_lore_3/boot.log", "w");
        Result rc_romfs = romfsInit();
        romfs_ok = R_SUCCEEDED(rc_romfs);
        Result rc_sock = socketInitializeDefault();
        socket_ok = R_SUCCEEDED(rc_sock);
        if (log_fp) {
            std::fprintf(log_fp, "[HL3 Switch] romfsInit=0x%x socketInitializeDefault=0x%x\n",
                         rc_romfs, rc_sock);
            std::fflush(log_fp);
        }
    }

    ~SwitchRuntimeGuard() {
        if (log_fp) {
            std::fprintf(log_fp, "[HL3 Switch] Exiting runtime guard\n");
            std::fflush(log_fp);
            std::fclose(log_fp);
            log_fp = nullptr;
        }
        if (socket_ok) socketExit();
        if (romfs_ok) romfsExit();
    }
};
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef __SWITCH__
    SwitchRuntimeGuard sw_guard;
#endif
    CliOptions opts;
    if (!parse_cli(argc, argv, opts)) {
        return 1;
    }
    try {
        PcHost host(std::move(opts));
        return host.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HL3 PC] Fatal exception: %s\n", e.what());
        write_fatal_log(find_writable_data_root(), std::string("Fatal exception: ") + e.what());
        return 2;
    } catch (...) {
        std::fprintf(stderr, "[HL3 PC] Fatal unknown exception\n");
        write_fatal_log(find_writable_data_root(), "Fatal unknown exception");
        return 3;
    }
}

