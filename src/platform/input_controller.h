#pragma once

#include "../core/runtime.h"
#include "../hle/gl_bridge.h"
#include "display_config.h"
#include "ui_renderer.h"

#include <SDL.h>

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hl3 {

class PortOptionsModal;

struct EngineState {
    u32 app_state = 0;
    i32 sub_state = 0;
    u16 form_id = 0;
    u16 main_popup_id = 0;
    u16 hud_popup_id = 0;
    u16 active_popup_id = 0;
    u8 can_talk = 0;
    u8 combat_mode = 0;
    u32 main_form = 0;
    u32 main_popup = 0;
    u32 hud_popup = 0;
};

struct UiNode {
    u32 ptr = 0;
    u16 ctype = 0;
    u32 cid = 0;
    u16 vis = 0;
    i32 px = 0;
    i32 py = 0;
    i32 pw = 0;
    i32 ph = 0;
    float fx0 = 0.0f;
    float fx1 = 0.0f;
    float fy0 = 0.0f;
    float fy1 = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
    u32 next_sibling = 0;
    u32 first_child = 0;
    bool is_slider = false;
    float prev_x = 0.0f;
    float prev_y = 0.0f;
    float next_x = 0.0f;
    float next_y = 0.0f;
};

struct FocusContext {
    std::string kind;  // "popup", "hud_popup", "submenu", "pregame"
    int primary_id = 0;
    int secondary_id = 0;
    bool valid = false;

    bool operator==(const FocusContext& o) const {
        return valid == o.valid && kind == o.kind && primary_id == o.primary_id && secondary_id == o.secondary_id;
    }
    bool operator!=(const FocusContext& o) const { return !(*this == o); }
};

struct InjectedButtonSpec {
    u32 cid = 0;
    float fx0 = 0.0f;
    float fx1 = 0.0f;
    float fy0 = 0.0f;
    float fy1 = 0.0f;
    float ox = 0.0f;
    float oy = 0.0f;
};

class InputController {
public:
    struct HostActions {
        std::function<void()> toggle_language;
        std::function<void()> toggle_fps;
        std::function<void()> cycle_aspect;
        std::function<void()> toggle_fullscreen;
        std::function<void()> request_screenshot;
        std::function<void()> request_quit;
        std::function<void(int, int)> on_window_resized;
        std::function<void()> on_window_sync_check;
        std::function<void()> open_port_modal;
        std::function<std::string()> get_language;
    };

    InputController();
    ~InputController();

    void set_assets_root(const std::filesystem::path& assets_dir);
    void set_host_actions(HostActions actions);
    void reset_runtime_state();
    void reset_gl(GLBridge& gl);

    // Joystick / GameController management
    enum GamepadButtonCode {
        kGamepadConfirm = 0,
        kGamepadCancel = 1,
        kGamepadAttackPrimary = 2,
        kGamepadAttackSecondary = 3,
        kGamepadPotion1 = 4,
        kGamepadPotion2 = 5,
        kGamepadMinimap = 6,
        kGamepadStart = 7,
        kGamepadSkill3 = 8,
        kGamepadPortOptions = 9,
        kGamepadSkill1 = 10,
        kGamepadSkill2 = 11,
    };

    static bool should_ignore_joystick_device(const char* name, int num_buttons, int num_hats);
    static int map_controller_button(SDL_GameControllerButton button, bool nintendo_layout);
    static bool update_trigger_axis_state(bool& was_pressed, Sint16 value);
    bool is_nintendo_controller(SDL_JoystickID instance_id) const;
    void init_controllers();
    void shutdown_controllers();
    void handle_controller_added(int device_index);
    void handle_controller_removed(SDL_JoystickID instance_id);
    void release_all_touches(Runtime& rt);

    // State queries
    EngineState get_engine_state(Runtime& rt) const;
    bool get_injected_port_btn_spec(Runtime& rt, const EngineState& st, InjectedButtonSpec& out) const;

    // Guest patching & live localization
    bool patch_guest_about_credits(Runtime& rt, const std::string& lang);
    void patch_help_about_layout(Runtime& rt, u32 main_form_ptr);
    void apply_guest_language_live(Runtime& rt, const std::string& lang);
    void invalidate_language_cache();

    // OSD notification banner
    void show_osd(const std::string& badge, const std::string& title, const std::string& subtitle = "",
                  RgbaColor accent = {245, 204, 82, 255}, double duration_sec = 2.4);

    // Per-frame tick & event handling
    void tick_timers_and_touches(Runtime& rt);
    bool process_sdl_event(const SDL_Event& ev, Runtime& rt, PortOptionsModal* modal, int win_w, int win_h);
    void update_continuous_input(Runtime& rt, PortOptionsModal* modal);

    // Rendering overlays inside GLBridge frame
    void draw_injected_port_buttons(GLBridge& gl, Runtime& rt, const std::string& lang);
    void draw_menu_cursor(GLBridge& gl, Runtime& rt);
    void draw_osd(GLBridge& gl, int win_w, int win_h);

    // High-level actions
    void handle_confirm(Runtime& rt);
    void handle_cancel(Runtime& rt);
    void handle_skip(Runtime& rt);
    void handle_nav(Runtime& rt, const std::string& dir_name);
    void handle_page_tab(Runtime& rt, int delta);

    // Touch helpers
    void touch_down(Runtime& rt, const std::string& tag, float x, float y);
    void touch_move(Runtime& rt, const std::string& tag, float x, float y);
    void touch_up(Runtime& rt, const std::string& tag);
    void tap_once(Runtime& rt, const std::string& tag, float x, float y, int hold_frames = 2);
    void swipe_carousel(Runtime& rt, const std::string& dir);

private:
    struct ActiveTouch {
        u32 touch_obj = 0;
        u32 event_obj = 0;
        u32 set_obj = 0;
    };

    bool read_ui_node(Runtime& rt, u32 ptr, UiNode& out) const;
    bool find_active_overlay(Runtime& rt, u32 root_ptr, const std::vector<u16>& target_cids, UiNode& out) const;
    void find_slider_arrows(Runtime& rt, const UiNode& slider, float& px, float& py, float& nx, float& ny) const;
    void collect_subtree_controls(Runtime& rt, u32 root_ptr, bool keep_back,
                                  std::vector<UiNode>& out_controls, std::vector<UiNode>& out_scrolls) const;

    FocusContext get_focusable_controls(Runtime& rt, const EngineState& st,
                                        std::vector<UiNode>& out_controls,
                                        std::vector<UiNode>& out_scrolls);
    const UiNode* sync_focus(Runtime& rt, const EngineState& st,
                             std::vector<UiNode>& out_controls,
                             std::vector<UiNode>& out_scrolls);
    int move_focus_2d(const std::vector<UiNode>& controls, int cur_idx, const std::string& dir_name) const;

    std::pair<float, float> ui_main_offset(Runtime& rt, const EngineState& st) const;
    std::pair<float, float> main_pt(Runtime& rt, float x, float y, const EngineState& st,
                                    bool is_hud_popup = false) const;
    std::pair<float, float> hud_pt(Runtime& rt, float x, float y) const;

    void refresh_guest_banner_live(Runtime& rt, u32 app);
    void scroll_box(Runtime& rt, const UiNode& box, const std::string& dir_name, const EngineState& st);
    void draw_focus_rect(GLBridge& gl, const UiNode& node, float ox, float oy, float gw, float gh) const;

    void handle_gamepad_button(int btn, Runtime& rt, PortOptionsModal* modal);

    HostActions actions_;
    std::filesystem::path assets_dir_;

    // Ribbon image cache
    Bytes ribbon_rgba_;
    int ribbon_w_ = 0;
    int ribbon_h_ = 0;
    bool ribbon_loaded_ = false;

    // Guest state trackers
    u32 about_patched_buf1_ = 0;
    std::string about_patched_lang_;

    // Touch state
    std::unordered_map<std::string, ActiveTouch> active_touches_;
    std::unordered_map<std::string, int> pending_tap_ups_;
    bool modal_consumed_click_ = false;
    std::unordered_set<SDL_FingerID> modal_consumed_fingers_;

    // Navigation / focus state
    float facing_x_ = 0.0f;
    float facing_y_ = 1.0f;
    int menu_index_ = 0;
    int nav_cooldown_ = 0;
    int carousel_cooldown_ = 0;
    int scroll_cooldown_ = 0;
    FocusContext focus_ctx_;
    int focus_idx_ = 0;
    u32 focus_cid_ = 0;
    int skill_tab_idx_ = 0;

    // OSD banner state
    std::string osd_badge_;
    std::string osd_title_;
    std::string osd_subtitle_;
    RgbaColor osd_accent_{245, 204, 82, 255};
    double osd_started_ = 0.0;
    double osd_until_ = 0.0;
    u32 osd_tex_ = 0;
    std::string osd_tex_key_;
    int osd_tex_w_ = 0;
    int osd_tex_h_ = 0;

    // Injected port button GL texture
    u32 injected_btn_tex_ = 0;
    std::string injected_btn_tex_key_;

    // SDL controllers & joysticks
    std::unordered_map<SDL_JoystickID, SDL_GameController*> controllers_;
    std::unordered_map<SDL_JoystickID, SDL_Joystick*> raw_joysticks_;
    std::unordered_map<SDL_JoystickID, std::pair<bool, bool>> trigger_pressed_;
};

}  // namespace hl3

