#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "../../src/audio/audio_mixer.h"
#include "../../src/core/json_mini.h"
#include "../../src/core/runtime.h"
#include "../../src/hle/gl_bridge.h"
#include "../../src/platform/display_config.h"

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int check_unit_helpers() {
    auto l16 = hl3::compute_viewport_layout("16:9", 1138, 640, true);
    if (!l16.true_widescreen || l16.patch_w < 568 || l16.patch_h != 320 || l16.ui_dx <= 0) {
        std::fprintf(stderr, "16:9 layout check failed: pw=%d ph=%d dx=%d\n", l16.patch_w, l16.patch_h, l16.ui_dx);
        return 10;
    }
    auto l_orig = hl3::compute_viewport_layout("original", 1280, 720, true);
    if (l_orig.true_widescreen || l_orig.patch_w != 480 || l_orig.patch_h != 320) {
        std::fprintf(stderr, "Original layout check failed\n");
        return 11;
    }
    auto l_title = hl3::compute_viewport_layout("16:9", 1280, 720, false);
    if (l_title.true_widescreen || l_title.ui_dx != 0 || l_title.patch_w != 480) {
        std::fprintf(stderr, "Static screen layout check failed\n");
        return 12;
    }
    hl3::JsonValue j;
    if (!hl3::JsonValue::parse(R"({"a":12,"b":"ok\n\u00e9","c":[true,null]})", j)) {
        std::fprintf(stderr, "JsonValue parse failed\n");
        return 13;
    }
    if (j.get_int("a") != 12 || j.get_string("b") != "ok\n\xC3\xA9") {
        std::fprintf(stderr, "JsonValue roundtrip mismatch\n");
        return 14;
    }
    return 0;
}

}  // namespace

int main() {
    if (int rc = check_unit_helpers(); rc != 0) return rc;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_Window* win = SDL_CreateWindow(
        "HL3 Headless Test",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        960,
        640,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!win) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 3;
    }
    SDL_GL_MakeCurrent(win, ctx);

    int exit_code = 0;
    {
        hl3::Runtime rt;
        std::string err;
        if (!rt.init({}, {}, false, &err)) {
            std::fprintf(stderr, "Runtime init failed: %s\n", err.c_str());
            exit_code = 4;
        } else {
            hl3::GLBridge gl(&rt.machine(), 960, 640, "auto");
            if (!gl.init_gl()) {
                std::fprintf(stderr, "GLBridge init_gl failed\n");
                exit_code = 5;
            } else {
                hl3::AudioMixer audio;
                audio.init_device();

                rt.set_gl_bridge(&gl);
                rt.set_audio_mixer(&audio);

                int presents = 0;
                rt.present_hook = [&]() {
                    ++presents;
                    SDL_GL_SwapWindow(win);
                };

                rt.boot_to_app_main();
                rt.run_launch();

                if (rt.touch_responder == 0) {
                    std::fprintf(stderr, "TouchResponder was not allocated\n");
                    exit_code = 6;
                } else {
                    for (int frame = 0; frame < 60; ++frame) {
                        hl3::u32 app = rt.machine().rd32(0x000D7DD4u);
                        hl3::u32 app_state = app ? rt.machine().rd32(app + 0x78u) : 0u;
                        gl.set_world_active(app_state != 0u && app_state != 1u && app_state != 256u);
                        gl.begin_frame();
                        rt.send(rt.delegate_obj(), "onTimer:", {0u});
                    }

                    // Render one more frame clean into backbuffer before reading pixels (SwapWindow swaps front/back).
                    gl.begin_frame();
                    rt.present_hook = [&]() { ++presents; };
                    rt.send(rt.delegate_obj(), "onTimer:", {0u});

                    int rw = 0, rh = 0;
                    hl3::Bytes rgba;
                    gl.read_framebuffer_rgba(rw, rh, rgba);
                    int non_black = 0;
                    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
                        if (rgba[i] > 8 || rgba[i + 1] > 8 || rgba[i + 2] > 8) ++non_black;
                    }
                    std::printf("Headless 60-frame complete: presents=%d fb=%dx%d non_black_pixels=%d touch=%s unimpl=%zu\n",
                                presents, rw, rh, non_black,
                                hl3::hex32(rt.touch_responder).c_str(),
                                rt.machine().unimplemented_counts().size());
                    if (presents < 50 || non_black < 1000) {
                        std::fprintf(stderr, "Expected active rendered frames, got presents=%d non_black=%d\n",
                                     presents, non_black);
                        exit_code = 7;
                    }
                }
            }
        }
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return exit_code;
}
