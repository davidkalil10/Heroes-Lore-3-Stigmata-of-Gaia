#pragma once

#include "../hle/gl_bridge.h"
#include "cloud_save.h"
#include "ui_renderer.h"
#include "updater.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

namespace hl3 {

enum class ModalPage {
    Options,
    Cloud,
    Update,
    ConfirmUpload,
    ConfirmRestore,
    ConfirmDisconnect,
};

class PortOptionsModal {
public:
    struct Callbacks {
        std::function<int()> get_fps;
        std::function<void(int, bool)> set_fps;
        std::function<std::string()> get_aspect;
        std::function<void(const std::string&, bool, bool)> set_aspect;
        std::function<std::string()> get_language;
        std::function<void(const std::string&, bool)> set_language;
        std::function<void()> on_cloud_restore_ready;
        std::function<void()> flush_before_upload;
    };

    PortOptionsModal(CloudSaveManager* cloud, UpdateManager* updater, Callbacks cb);
    ~PortOptionsModal();

    PortOptionsModal(const PortOptionsModal&) = delete;
    PortOptionsModal& operator=(const PortOptionsModal&) = delete;

    bool is_open() const { return is_open_; }
    bool needs_redraw() const { return dirty_; }
    ModalPage page() const { return page_; }
    UiRect compute_box(int win_w, int win_h) const;

    void open(ModalPage start_page = ModalPage::Options);
    void close();
    void invalidate_cache();
    void reset_gl(GLBridge& gl);

    void poll();
    void draw_gl(GLBridge& gl, int win_w, int win_h);

    bool handle_mouse_down(int mx, int my, int win_w, int win_h);
    bool handle_mouse_motion(int mx, int my, int win_w, int win_h);
    bool handle_key(SDL_Keycode key);
    void handle_nav(const std::string& dir);
    void handle_confirm();
    void handle_cancel();

private:
    struct Hitbox {
        UiRect rect;
        std::string action;
        int index = 0;
    };

    void mark_dirty();
    std::string lang() const;

    void activate_options_row(int delta);
    void activate_action(const std::string& action);

    void rebuild_canvas(GLBridge& gl, int box_w, int box_h);
    void draw_options_page(RgbaCanvas& c, int w, int h, float scale);
    void draw_cloud_page(RgbaCanvas& c, int w, int h, float scale);
    void draw_update_page(RgbaCanvas& c, int w, int h, float scale);
    void draw_confirm_page(RgbaCanvas& c, int w, int h, float scale);

    void draw_pill_button(RgbaCanvas& c, const UiRect& r, const std::string& label,
                          bool selected, bool enabled, float scale, const std::string& action, int idx,
                          bool primary = false, bool danger = false);

    CloudSaveManager* cloud_ = nullptr;
    UpdateManager* updater_ = nullptr;
    Callbacks cb_;

    bool is_open_ = false;
    ModalPage page_ = ModalPage::Options;
    int selected_ = 0;
    int cloud_focus_ = 0;
    int update_focus_ = 0;
    int confirm_focus_ = 1;  // 0=Confirm, 1=Cancel

    bool dirty_ = true;
    bool was_live_ = false;
    u32 last_live_poll_ms_ = 0;
    std::string last_state_sig_;
    u32 tex_id_ = 0;
    int tex_w_ = 0;
    int tex_h_ = 0;
    int last_win_w_ = 0;
    int last_win_h_ = 0;

    std::vector<Hitbox> hitboxes_;
};

}  // namespace hl3
