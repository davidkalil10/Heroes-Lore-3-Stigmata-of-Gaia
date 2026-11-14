#include "input_controller.h"

#include "../audio/audio_mixer.h"
#include "i18n.h"
#include "options_modal.h"
#include "png_codec.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace hl3 {

namespace {

constexpr float kDpadCenterX = 65.0f;
constexpr float kDpadCenterY = 255.0f;

constexpr float kTitleCarouselCenterX = 240.0f;
constexpr float kTitleCarouselCenterY = 272.0f;

constexpr float kCharKeiX = 160.0f;
constexpr float kCharKeiY = 96.0f;
constexpr float kCharRitsX = 320.0f;
constexpr float kCharRitsY = 96.0f;
constexpr float kCharConfirmX = 415.0f;
constexpr float kCharConfirmY = 295.0f;
constexpr float kBtnBackX = 28.0f;
constexpr float kBtnBackY = 28.0f;

constexpr float kClassArrowPrevX = 73.0f;
constexpr float kClassArrowPrevY = 71.0f;
constexpr float kClassArrowNextX = 400.0f;
constexpr float kClassArrowNextY = 71.0f;
constexpr float kClassConfirmX = 415.0f;
constexpr float kClassConfirmY = 295.0f;

constexpr float kSummaryConfirmX = 356.0f;
constexpr float kSummaryConfirmY = 295.0f;

constexpr float kCutsceneSkipX = 408.0f;
constexpr float kCutsceneSkipY = 28.0f;
constexpr float kDialogueAdvanceX = 240.0f;
constexpr float kDialogueAdvanceY = 160.0f;

constexpr float kPopupConfirmX = 395.0f;
constexpr float kPopupConfirmY = 220.0f;
constexpr float kPopupCancelX = 395.0f;
constexpr float kPopupCancelY = 100.0f;
constexpr float kTutorialNextX = 385.0f;
constexpr float kTutorialNextY = 207.0f;
constexpr float kTutorialCloseX = 385.0f;
constexpr float kTutorialCloseY = 74.0f;

constexpr float kHudMapX = 23.5f;
constexpr float kHudMapY = 22.0f;
constexpr float kHudPotion1X = 29.0f;
constexpr float kHudPotion1Y = 107.0f;
constexpr float kHudPotion2X = 29.0f;
constexpr float kHudPotion2Y = 157.0f;
constexpr float kHudSkill1X = 451.0f;
constexpr float kHudSkill1Y = 184.0f;
constexpr float kHudSkill2X = 451.0f;
constexpr float kHudSkill2Y = 134.0f;
constexpr float kHudSkill3X = 451.0f;
constexpr float kHudSkill3Y = 84.0f;
constexpr float kHudAttackPrimaryX = 450.0f;
constexpr float kHudAttackPrimaryY = 258.0f;
constexpr float kHudAttackSecondaryX = 408.5f;
constexpr float kHudAttackSecondaryY = 289.0f;
constexpr float kHudTalkPeacefulX = 450.0f;
constexpr float kHudTalkPeacefulY = 258.0f;
constexpr float kHudTalkCombatX = 397.0f;
constexpr float kHudTalkCombatY = 239.0f;
constexpr float kHudPortraitMenuX = 200.0f;
constexpr float kHudPortraitMenuY = 292.0f;

constexpr float kPauseMenuBackX = 28.0f;
constexpr float kPauseMenuBackY = 28.0f;
constexpr std::array<float, 6> kPauseMenuItemsY = {45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f};

double now_monotonic() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

bool is_cancel_cid(u16 cid) {
    return cid == 199 || cid == 207 || cid == 306 || cid == 4709 || cid == 10602 || cid == 13202 || cid == 13302;
}

bool is_corner_back(const UiNode& c) {
    return (c.cid == 199 || c.cid == 207 || c.cid == 306 || c.cid == 4709) && c.cx < 55.0f && c.cy < 55.0f;
}

std::string utf16be_to_utf8(const Bytes& raw) {
    std::string out;
    out.reserve(raw.size());
    size_t i = 0;
    while (i + 1 < raw.size()) {
        u32 cp = (u32(raw[i]) << 8) | u32(raw[i + 1]);
        i += 2;
        if (cp >= 0xD800u && cp <= 0xDBFFu && i + 1 < raw.size()) {
            u32 lo = (u32(raw[i]) << 8) | u32(raw[i + 1]);
            if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                cp = 0x10000u + (((cp - 0xD800u) << 10) | (lo - 0xDC00u));
                i += 2;
            }
        }
        if (cp < 0x80u) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }
    return out;
}

Bytes utf8_to_utf16be(std::string_view s) {
    Bytes out;
    out.reserve(s.size() * 2);
    size_t i = 0;
    while (i < s.size()) {
        u8 b0 = static_cast<u8>(s[i]);
        u32 cp = 0xFFFDu;
        if ((b0 & 0x80u) == 0u) {
            cp = b0;
            i += 1;
        } else if ((b0 & 0xE0u) == 0xC0u && i + 1 < s.size()) {
            cp = ((u32(b0 & 0x1Fu) << 6) | u32(static_cast<u8>(s[i + 1]) & 0x3Fu));
            i += 2;
        } else if ((b0 & 0xF0u) == 0xE0u && i + 2 < s.size()) {
            cp = ((u32(b0 & 0x0Fu) << 12) | (u32(static_cast<u8>(s[i + 1]) & 0x3Fu) << 6) |
                  u32(static_cast<u8>(s[i + 2]) & 0x3Fu));
            i += 3;
        } else if ((b0 & 0xF8u) == 0xF0u && i + 3 < s.size()) {
            cp = ((u32(b0 & 0x07u) << 18) | (u32(static_cast<u8>(s[i + 1]) & 0x3Fu) << 12) |
                  (u32(static_cast<u8>(s[i + 2]) & 0x3Fu) << 6) | u32(static_cast<u8>(s[i + 3]) & 0x3Fu));
            i += 4;
        } else {
            i += 1;
        }

        if (cp >= 0x10000u && cp <= 0x10FFFFu) {
            u32 sub = cp - 0x10000u;
            u16 hi = static_cast<u16>(0xD800u + (sub >> 10));
            u16 lo = static_cast<u16>(0xDC00u + (sub & 0x3FFu));
            out.push_back(static_cast<u8>(hi >> 8));
            out.push_back(static_cast<u8>(hi & 0xFFu));
            out.push_back(static_cast<u8>(lo >> 8));
            out.push_back(static_cast<u8>(lo & 0xFFu));
        } else {
            u16 u = static_cast<u16>(cp & 0xFFFFu);
            out.push_back(static_cast<u8>(u >> 8));
            out.push_back(static_cast<u8>(u & 0xFFu));
        }
    }
    return out;
}

UiNode make_virtual_port_btn(u32 cid, float fx0, float fx1, float fy0, float fy1) {
    UiNode n;
    n.ptr = 0;
    n.ctype = 2;
    n.cid = cid;
    n.vis = 3;
    n.fx0 = fx0;
    n.fx1 = fx1;
    n.fy0 = fy0;
    n.fy1 = fy1;
    n.cx = (fx0 + fx1) * 0.5f;
    n.cy = (fy0 + fy1) * 0.5f;
    n.px = static_cast<i32>(std::lround(n.cx));
    n.py = static_cast<i32>(std::lround(n.cy));
    n.pw = -1;
    n.ph = -1;
    return n;
}

std::string make_osd_key(const std::string& badge, const std::string& title, const std::string& sub, RgbaColor c,
                         float ui_scale) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%u,%u,%u,%.2f|", c.r, c.g, c.b, ui_scale);
    return std::string(buf) + badge + "|" + title + "|" + sub;
}

}  // namespace

InputController::InputController() = default;

InputController::~InputController() {
    shutdown_controllers();
}

void InputController::set_assets_root(const std::filesystem::path& assets_dir) {
    assets_dir_ = assets_dir;
    if (!ribbon_loaded_) {
        ribbon_loaded_ = true;
        std::filesystem::path p = assets_dir_ / "ui_button_ribbon.png";
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) {
            load_png_rgba_file(p, ribbon_w_, ribbon_h_, ribbon_rgba_);
        }
    }
}

void InputController::set_host_actions(HostActions actions) {
    actions_ = std::move(actions);
}

void InputController::reset_runtime_state() {
    active_touches_.clear();
    pending_tap_ups_.clear();
    menu_index_ = 0;
    focus_ctx_ = FocusContext{};
    focus_idx_ = 0;
    focus_cid_ = 0;
    skill_tab_idx_ = 0;
    about_patched_buf1_ = 0;
    about_patched_lang_.clear();
    injected_btn_tex_ = 0;
    injected_btn_tex_key_.clear();
}

void InputController::reset_gl(GLBridge& gl) {
    if (osd_tex_ != 0) {
        gl.destroy_texture(osd_tex_);
        osd_tex_ = 0;
    }
    osd_tex_key_.clear();
    if (injected_btn_tex_ != 0) {
        gl.destroy_texture(injected_btn_tex_);
        injected_btn_tex_ = 0;
    }
    injected_btn_tex_key_.clear();
}

void InputController::init_controllers() {
    int count = SDL_NumJoysticks();
    std::printf("[HL3 PC] Detected %d gamepad(s)\n", count);
    for (int i = 0; i < count; ++i) {
        handle_controller_added(i);
    }
}

void InputController::shutdown_controllers() {
    for (auto& [jid, gc] : controllers_) {
        (void)jid;
        if (gc) SDL_GameControllerClose(gc);
    }
    controllers_.clear();
    for (auto& [jid, js] : raw_joysticks_) {
        (void)jid;
        if (js) SDL_JoystickClose(js);
    }
    raw_joysticks_.clear();
}

bool InputController::should_ignore_joystick_device(const char* name, int num_buttons, int num_hats) {
    if (num_buttons <= 0 && num_hats <= 0) {
        return true;
    }
    if (!name || name[0] == '\0') {
        return false;
    }
    std::string lower;
    lower.reserve(std::char_traits<char>::length(name));
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p != 0; ++p) {
        lower.push_back(static_cast<char>(std::tolower(*p)));
    }
    static constexpr const char* kIgnoredSubstrings[] = {
        "accelerometer", "gyro",     "magnetometer", "gravity",      "compass",
        "rotation",      "sensor",   "orientation",  "proximity",    "pressure",
        "temperature",   "light",    "fingerprint",  "uinput-fpc",   "gf3208",
        "gf_key",        "touchscreen", "virtual-search", "gpio-keys",
    };
    for (const char* token : kIgnoredSubstrings) {
        if (lower.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void InputController::handle_controller_added(int device_index) {
    SDL_JoystickID jid = SDL_JoystickGetDeviceInstanceID(device_index);
    if (jid >= 0 && (controllers_.count(jid) || raw_joysticks_.count(jid))) {
        return;
    }
    if (SDL_IsGameController(device_index)) {
        SDL_GameController* gc = SDL_GameControllerOpen(device_index);
        if (gc) {
            SDL_Joystick* js = SDL_GameControllerGetJoystick(gc);
            SDL_JoystickID id = js ? SDL_JoystickInstanceID(js) : jid;
            controllers_[id] = gc;
            const char* name = SDL_GameControllerName(gc);
            std::printf("[HL3 PC] Initialized gamepad: %s\n", name ? name : "Unknown Controller");
        }
    } else {
        SDL_Joystick* js = SDL_JoystickOpen(device_index);
        if (js) {
            const char* name = SDL_JoystickName(js);
            int num_buttons = SDL_JoystickNumButtons(js);
            int num_hats = SDL_JoystickNumHats(js);
            if (should_ignore_joystick_device(name, num_buttons, num_hats)) {
                std::printf("[HL3 PC] Ignoring non-gamepad input device: %s (buttons=%d hats=%d)\n",
                            name ? name : "Unknown", num_buttons, num_hats);
                SDL_JoystickClose(js);
                return;
            }
            SDL_JoystickID id = SDL_JoystickInstanceID(js);
            raw_joysticks_[id] = js;
            std::printf("[HL3 PC] Initialized joystick: %s\n", name ? name : "Unknown Joystick");
        }
    }
}

void InputController::release_all_touches(Runtime& rt) {
    std::vector<std::string> tags;
    tags.reserve(active_touches_.size());
    for (const auto& [tag, _] : active_touches_) {
        tags.push_back(tag);
    }
    for (const std::string& tag : tags) {
        touch_up(rt, tag);
    }
    pending_tap_ups_.clear();
    modal_consumed_click_ = false;
    modal_consumed_fingers_.clear();
    nav_cooldown_ = 0;
}

void InputController::handle_controller_removed(SDL_JoystickID instance_id) {
    if (auto it = controllers_.find(instance_id); it != controllers_.end()) {
        if (it->second) SDL_GameControllerClose(it->second);
        controllers_.erase(it);
    }
    if (auto it = raw_joysticks_.find(instance_id); it != raw_joysticks_.end()) {
        if (it->second) SDL_JoystickClose(it->second);
        raw_joysticks_.erase(it);
    }
}

EngineState InputController::get_engine_state(Runtime& rt) const {
    EngineState st;
    Machine& m = rt.machine();
    try {
        u32 app = m.rd32(0x000d7dd4u);
        if (!app) return st;
        st.app_state = m.rd32(app + 0x78u);
        u32 ui_main = m.rd32(app + 0x40u);
        u32 ui_hud = m.rd32(app + 0x44u);
        st.main_form = ui_main ? m.rd32(ui_main + 0x98u) : 0u;
        st.main_popup = ui_main ? m.rd32(ui_main + 0xa0u) : 0u;
        st.hud_popup = ui_hud ? m.rd32(ui_hud + 0xa0u) : 0u;

        st.form_id = st.main_form ? m.rd16(st.main_form + 0x14u) : 0;
        st.main_popup_id = st.main_popup ? m.rd16(st.main_popup + 0x14u) : 0;
        st.hud_popup_id = st.hud_popup ? m.rd16(st.hud_popup + 0x14u) : 0;

        st.sub_state = m.rds8(0x000de30cu);
        st.can_talk = m.rd8(0x000de0adu);
        st.combat_mode = m.rd8(0x000de0acu);
        st.active_popup_id = st.main_popup_id ? st.main_popup_id : st.hud_popup_id;
    } catch (...) {
        return EngineState{};
    }
    return st;
}

bool InputController::read_ui_node(Runtime& rt, u32 ptr, UiNode& out) const {
    if (!ptr) return false;
    Machine& m = rt.machine();
    try {
        out.ptr = ptr;
        out.ctype = m.rd16(ptr + 0x04u);
        u8 pos_raw[8]{};
        if (!m.rdbytes(ptr + 0x08u, 8, pos_raw)) return false;
        out.px = read_s16_le(pos_raw + 0);
        out.py = read_s16_le(pos_raw + 2);
        out.pw = read_s16_le(pos_raw + 4);
        out.ph = read_s16_le(pos_raw + 6);
        out.cid = m.rd16(ptr + 0x14u);
        out.vis = m.rd16(ptr + 0x16u);
        u8 f_raw[16]{};
        if (!m.rdbytes(ptr + 0x18u, 16, f_raw)) return false;
        out.fx0 = bits_to_f32(read_u32_le(f_raw + 0));
        out.fx1 = bits_to_f32(read_u32_le(f_raw + 4));
        out.fy0 = bits_to_f32(read_u32_le(f_raw + 8));
        out.fy1 = bits_to_f32(read_u32_le(f_raw + 12));
        out.next_sibling = m.rd32(ptr + 0x30u);
        out.first_child = m.rd32(ptr + 0x38u);
    } catch (...) {
        return false;
    }

    float max_w = rt.gl_bridge() ? static_cast<float>(rt.gl_bridge()->layout().patch_w) + 40.0f : 520.0f;
    float max_h = rt.gl_bridge() ? static_cast<float>(rt.gl_bridge()->layout().patch_h) + 40.0f : 360.0f;
    bool frect_ok = std::isfinite(out.fx0) && std::isfinite(out.fx1) && std::isfinite(out.fy0) &&
                    std::isfinite(out.fy1) && (out.fx1 - out.fx0) >= 2.0f && (out.fy1 - out.fy0) >= 2.0f &&
                    out.fx0 > -40.0f && out.fx1 < max_w && out.fy0 > -40.0f && out.fy1 < max_h;
    if (!frect_ok) {
        if (out.pw >= 4 && out.ph >= 4 && out.px > -40 && (out.px + out.pw) < static_cast<i32>(max_w) &&
            out.py > -40 && (out.py + out.ph) < static_cast<i32>(max_h)) {
            out.fx0 = static_cast<float>(out.px);
            out.fx1 = static_cast<float>(out.px + out.pw);
            out.fy0 = static_cast<float>(out.py);
            out.fy1 = static_cast<float>(out.py + out.ph);
        } else {
            out.fx0 = out.fx1 = out.fy0 = out.fy1 = 0.0f;
        }
    }
    out.cx = (out.fx0 + out.fx1) * 0.5f;
    out.cy = (out.fy0 + out.fy1) * 0.5f;
    out.is_slider = (out.ctype == 13);
    return true;
}

bool InputController::find_active_overlay(Runtime& rt, u32 root_ptr, const std::vector<u16>& target_cids,
                                          UiNode& out) const {
    if (!root_ptr || target_cids.empty()) return false;
    std::unordered_map<u16, UiNode> found;
    std::vector<u32> queue;
    queue.push_back(root_ptr);
    std::unordered_set<u32> visited;
    size_t qhead = 0;

    while (qhead < queue.size() && visited.size() < 160) {
        u32 ptr = queue[qhead++];
        if (!ptr || visited.count(ptr)) continue;
        visited.insert(ptr);
        UiNode node;
        if (!read_ui_node(rt, ptr, node)) continue;
        for (u16 tcid : target_cids) {
            if (node.cid == tcid && (node.vis & 0x0003u) == 0x0003u) {
                found[tcid] = node;
            }
        }
        u32 ch = node.first_child;
        int steps = 0;
        while (ch && !visited.count(ch) && steps < 64) {
            queue.push_back(ch);
            UiNode chn;
            ch = read_ui_node(rt, ch, chn) ? chn.next_sibling : 0u;
            ++steps;
        }
    }
    for (u16 tcid : target_cids) {
        if (auto it = found.find(tcid); it != found.end()) {
            out = it->second;
            return true;
        }
    }
    return false;
}

void InputController::find_slider_arrows(Runtime& rt, const UiNode& slider, float& px, float& py,
                                         float& nx, float& ny) const {
    px = slider.fx0 + 18.0f;
    py = slider.cy;
    nx = slider.fx1 - 18.0f;
    ny = slider.cy;
    std::vector<u32> queue;
    if (slider.first_child) queue.push_back(slider.first_child);
    std::unordered_set<u32> visited;
    size_t qhead = 0;
    while (qhead < queue.size() && visited.size() < 24) {
        u32 ptr = queue[qhead++];
        if (!ptr || visited.count(ptr)) continue;
        visited.insert(ptr);
        UiNode ch;
        if (!read_ui_node(rt, ptr, ch)) continue;
        if (ch.cid == 198 && ch.fx1 > ch.fx0) {
            px = ch.cx;
            py = ch.cy;
        } else if (ch.cid == 197 && ch.fx1 > ch.fx0) {
            nx = ch.cx;
            ny = ch.cy;
        }
        if (ch.first_child) queue.push_back(ch.first_child);
        if (ch.next_sibling) queue.push_back(ch.next_sibling);
    }
}

void InputController::collect_subtree_controls(Runtime& rt, u32 root_ptr, bool keep_back,
                                               std::vector<UiNode>& out_controls,
                                               std::vector<UiNode>& out_scrolls) const {
    out_controls.clear();
    out_scrolls.clear();
    if (!root_ptr) return;

    std::vector<UiNode> raw_controls;
    std::unordered_set<u32> visited;

    auto walk = [&](auto& self_ref, u32 ptr, bool is_root) -> void {
        if (!ptr || visited.count(ptr) || visited.size() > 256) return;
        visited.insert(ptr);
        UiNode node;
        if (!read_ui_node(rt, ptr, node)) return;

        bool valid_rect = (node.fx1 - node.fx0) >= 4.0f && (node.fy1 - node.fy0) >= 4.0f &&
                          node.cx >= 0.0f && node.cx <= 480.0f && node.cy >= 0.0f && node.cy <= 320.0f;
        bool vis_ok = (node.vis & 0x0003u) == 0x0003u;

        if (node.ctype == 1 || node.ctype == 14 || node.ctype == 15) {
            if (!vis_ok && !is_root) return;
            u32 ch = node.first_child;
            int steps = 0;
            while (ch && steps < 64) {
                self_ref(self_ref, ch, false);
                UiNode chn;
                ch = read_ui_node(rt, ch, chn) ? chn.next_sibling : 0u;
                ++steps;
            }
            return;
        }
        if (node.ctype == 13) {
            if (vis_ok && valid_rect) {
                find_slider_arrows(rt, node, node.prev_x, node.prev_y, node.next_x, node.next_y);
                raw_controls.push_back(node);
            }
            return;
        }
        if (node.ctype == 2 || node.ctype == 4) {
            if (vis_ok && valid_rect) {
                raw_controls.push_back(node);
            }
            u32 ch = node.first_child;
            int steps = 0;
            while (ch && steps < 32) {
                self_ref(self_ref, ch, false);
                UiNode chn;
                ch = read_ui_node(rt, ch, chn) ? chn.next_sibling : 0u;
                ++steps;
            }
            return;
        }
        if (node.ctype == 10) {
            if (vis_ok && valid_rect && (node.fy1 - node.fy0) >= 36.0f) {
                out_scrolls.push_back(node);
            }
        }
    };

    UiNode root_node;
    if (!read_ui_node(rt, root_ptr, root_node)) return;
    if (root_node.ctype == 1 || root_node.ctype == 14 || root_node.ctype == 15) {
        u32 ch = root_node.first_child;
        int steps = 0;
        while (ch && steps < 64) {
            walk(walk, ch, false);
            UiNode chn;
            ch = read_ui_node(rt, ch, chn) ? chn.next_sibling : 0u;
            ++steps;
        }
    } else {
        walk(walk, root_ptr, true);
    }

    // Deduplicate overlapping identical controls
    std::unordered_set<u64> seen;
    for (const auto& c : raw_controls) {
        u64 rx = static_cast<u64>(static_cast<i64>(std::llround(c.cx)) & 0xFFFFu);
        u64 ry = static_cast<u64>(static_cast<i64>(std::llround(c.cy)) & 0xFFFFu);
        u64 key = (u64(c.cid) << 32) | (rx << 16) | ry;
        if (!seen.count(key)) {
            seen.insert(key);
            out_controls.push_back(c);
        }
    }

    if (!keep_back) {
        std::vector<UiNode> non_back;
        non_back.reserve(out_controls.size());
        for (const auto& c : out_controls) {
            if (!is_corner_back(c)) non_back.push_back(c);
        }
        if (!non_back.empty() || !out_scrolls.empty()) {
            out_controls = std::move(non_back);
        }
    }
}

void InputController::patch_help_about_layout(Runtime& rt, u32 main_form_ptr) {
    if (!main_form_ptr) return;
    static const std::unordered_map<u32, i16> kTargets = {
        {11301, 33}, {11302, 69}, {11303, 105}, {11304, 141}, {11305, 177}, {11306, 213},
    };
    std::vector<UiNode> ctrls, scrolls;
    collect_subtree_controls(rt, main_form_ptr, false, ctrls, scrolls);
    Machine& m = rt.machine();
    for (const auto& c : ctrls) {
        auto it = kTargets.find(c.cid);
        if (it == kTargets.end() || c.ptr == 0) continue;
        i16 t_cy = it->second;
        if (c.py != t_cy || std::abs(c.cy - static_cast<float>(t_cy)) > 0.5f) {
            try {
                u8 py_bytes[2];
                write_u16_le(py_bytes, static_cast<u16>(t_cy));
                m.wrbytes(c.ptr + 0x0au, py_bytes, 2);
                float fy0 = static_cast<float>(t_cy) - 15.0f;
                float fy1 = static_cast<float>(t_cy) + 15.0f;
                u8 f_bytes[16];
                write_u32_le(f_bytes + 0, f32_to_bits(93.0f));
                write_u32_le(f_bytes + 4, f32_to_bits(393.0f));
                write_u32_le(f_bytes + 8, f32_to_bits(fy0));
                write_u32_le(f_bytes + 12, f32_to_bits(fy1));
                m.wrbytes(c.ptr + 0x18u, f_bytes, 16);
            } catch (...) {
            }
        }
    }
}

FocusContext InputController::get_focusable_controls(Runtime& rt, const EngineState& st,
                                                     std::vector<UiNode>& out_controls,
                                                     std::vector<UiNode>& out_scrolls) {
    out_controls.clear();
    out_scrolls.clear();
    if (st.main_popup_id && st.main_popup) {
        collect_subtree_controls(rt, st.main_popup, false, out_controls, out_scrolls);
        return {"popup", st.main_popup_id, 0, true};
    }
    if (st.hud_popup_id && st.hud_popup_id != 130 && st.hud_popup) {
        collect_subtree_controls(rt, st.hud_popup, false, out_controls, out_scrolls);
        return {"hud_popup", st.hud_popup_id, 0, true};
    }
    if (st.app_state == 8 && st.form_id != 2 && st.main_form) {
        u32 root_ptr = st.main_form;
        u16 overlay_cid = 0;
        std::vector<u16> cands;
        if (st.form_id == 8) cands = {837, 836};
        else if (st.form_id == 46) cands = {4708};
        if (!cands.empty()) {
            UiNode ov;
            if (find_active_overlay(rt, root_ptr, cands, ov)) {
                root_ptr = ov.ptr;
                overlay_cid = ov.cid;
            }
        }
        collect_subtree_controls(rt, root_ptr, false, out_controls, out_scrolls);
        if (st.form_id == 3 && overlay_cid == 0) {
            out_controls.push_back(make_virtual_port_btn(99003, 90.0f, 390.0f, 256.0f, 286.0f));
        }
        return {"submenu", st.form_id, overlay_cid, true};
    }
    if (st.app_state == 1 && st.form_id != 0 && st.form_id != 100 && st.form_id != 41 && st.form_id != 42 &&
        st.form_id != 43 && st.main_form) {
        if (st.form_id == 113) {
            patch_help_about_layout(rt, st.main_form);
        }
        collect_subtree_controls(rt, st.main_form, false, out_controls, out_scrolls);
        if (st.form_id == 113) {
            out_controls.push_back(make_virtual_port_btn(99113, 93.0f, 393.0f, 234.0f, 264.0f));
        }
        return {"pregame", st.form_id, 0, true};
    }
    return {};
}

const UiNode* InputController::sync_focus(Runtime& rt, const EngineState& st,
                                          std::vector<UiNode>& out_controls,
                                          std::vector<UiNode>& out_scrolls) {
    FocusContext ctx = get_focusable_controls(rt, st, out_controls, out_scrolls);
    if (!ctx.valid) {
        focus_ctx_ = FocusContext{};
        focus_idx_ = 0;
        focus_cid_ = 0;
        return nullptr;
    }
    if (out_controls.empty()) {
        focus_ctx_ = ctx;
        focus_idx_ = 0;
        focus_cid_ = 0;
        return nullptr;
    }

    if (ctx != focus_ctx_) {
        int idx = 0;
        if ((ctx.kind == "popup" || ctx.kind == "hud_popup") && out_controls.size() >= 2) {
            int best_i = 0;
            double best_score = -1e9;
            for (int i = 0; i < static_cast<int>(out_controls.size()); ++i) {
                double score = static_cast<double>(out_controls[i].cy) +
                               (is_cancel_cid(out_controls[i].cid) ? -1000.0 : 0.0);
                if (score > best_score) {
                    best_score = score;
                    best_i = i;
                }
            }
            idx = best_i;
        }
        focus_ctx_ = ctx;
        focus_idx_ = idx;
        focus_cid_ = out_controls[idx].cid;
    } else {
        int matched = -1;
        if (focus_cid_ != 0) {
            for (int i = 0; i < static_cast<int>(out_controls.size()); ++i) {
                if (out_controls[i].cid == focus_cid_) {
                    matched = i;
                    break;
                }
            }
        }
        if (matched >= 0) {
            focus_idx_ = matched;
        } else {
            focus_idx_ = std::max(0, std::min(focus_idx_, static_cast<int>(out_controls.size()) - 1));
            focus_cid_ = out_controls[focus_idx_].cid;
        }
    }
    return &out_controls[focus_idx_];
}

int InputController::move_focus_2d(const std::vector<UiNode>& controls, int cur_idx,
                                   const std::string& dir_name) const {
    if (controls.size() <= 1) return 0;
    const UiNode& cur = controls[cur_idx];
    float cx = cur.cx;
    float cy = cur.cy;

    int best_idx = -1;
    double best_score = 1e18;
    for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
        if (i == cur_idx) continue;
        float dx = controls[i].cx - cx;
        float dy = controls[i].cy - cy;
        double score = 0.0;
        if (dir_name == "right" && dx > 4.0f) score = dx + std::abs(dy) * 2.5;
        else if (dir_name == "left" && dx < -4.0f) score = -dx + std::abs(dy) * 2.5;
        else if (dir_name == "down" && dy > 4.0f) score = dy + std::abs(dx) * 2.5;
        else if (dir_name == "up" && dy < -4.0f) score = -dy + std::abs(dx) * 2.5;
        else continue;

        if (score < best_score) {
            best_score = score;
            best_idx = i;
        }
    }
    if (best_idx >= 0) return best_idx;

    if (dir_name == "right" || dir_name == "left") {
        std::vector<int> same_row;
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            if (std::abs(controls[i].cy - cy) <= 28.0f) same_row.push_back(i);
        }
        if (same_row.size() > 1) {
            int chosen = same_row[0];
            for (int idx : same_row) {
                if (dir_name == "right" && controls[idx].cx < controls[chosen].cx) chosen = idx;
                if (dir_name == "left" && controls[idx].cx > controls[chosen].cx) chosen = idx;
            }
            return chosen;
        }
        if (controls.size() == 2) return 1 - cur_idx;
        return cur_idx;
    }

    if (dir_name == "down" || dir_name == "up") {
        int chosen = 0;
        for (int i = 1; i < static_cast<int>(controls.size()); ++i) {
            if (dir_name == "down") {
                if (controls[i].cy < controls[chosen].cy ||
                    (std::abs(controls[i].cy - controls[chosen].cy) < 0.5f &&
                     std::abs(controls[i].cx - cx) < std::abs(controls[chosen].cx - cx))) {
                    chosen = i;
                }
            } else {
                if (controls[i].cy > controls[chosen].cy ||
                    (std::abs(controls[i].cy - controls[chosen].cy) < 0.5f &&
                     std::abs(controls[i].cx - cx) > std::abs(controls[chosen].cx - cx))) {
                    chosen = i;
                }
            }
        }
        return chosen;
    }
    return cur_idx;
}

std::pair<float, float> InputController::ui_main_offset(Runtime& rt, const EngineState& st) const {
    if (!rt.gl_bridge()) return {0.0f, 0.0f};
    if (st.app_state == 0 || st.app_state == 1 || st.app_state == 256) return {0.0f, 0.0f};
    const auto& lay = rt.gl_bridge()->layout();
    return {static_cast<float>(lay.ui_dx), static_cast<float>(lay.ui_dy)};
}

std::pair<float, float> InputController::main_pt(Runtime& rt, float x, float y, const EngineState& st,
                                                 bool is_hud_popup) const {
    if (is_hud_popup) return {x, y};
    auto [dx, dy] = ui_main_offset(rt, st);
    return {x + dx, y + dy};
}

std::pair<float, float> InputController::hud_pt(Runtime& rt, float x, float y) const {
    if (!rt.gl_bridge()) return {x, y};
    const auto& lay = rt.gl_bridge()->layout();
    float dx = 0.0f;
    if (x < 150.0f) dx = 0.0f;
    else if (x <= 330.0f) dx = static_cast<float>(lay.hud_dx_center);
    else dx = static_cast<float>(lay.hud_dx_right);

    float dy = 0.0f;
    if (y < 100.0f) dy = 0.0f;
    else if (y < 180.0f) dy = static_cast<float>(lay.hud_dy_bottom / 2);
    else dy = static_cast<float>(lay.hud_dy_bottom);

    return {x + dx, y + dy};
}

bool InputController::get_injected_port_btn_spec(Runtime& rt, const EngineState& st,
                                                 InjectedButtonSpec& out) const {
    if (st.active_popup_id != 0) return false;
    if (st.app_state == 1 && st.form_id == 113) {
        out = {99113, 93.0f, 393.0f, 234.0f, 264.0f, 0.0f, 0.0f};
        return true;
    }
    if (st.app_state == 8 && st.form_id == 3) {
        auto [ox, oy] = ui_main_offset(rt, st);
        out = {99003, 90.0f, 390.0f, 256.0f, 286.0f, ox, oy};
        return true;
    }
    return false;
}

bool InputController::patch_guest_about_credits(Runtime& rt, const std::string& lang) {
    Machine& m = rt.machine();
    try {
        u32 tu = m.rd32(0x000d8220u);
        if (!tu) return false;
        u32 buf_ptr_arr = m.rd32(tu + 0x20u);
        u32 offs_ptr_arr = m.rd32(tu + 0x24u);
        if (!buf_ptr_arr || !offs_ptr_arr) return false;
        u32 buf1 = m.rd32(buf_ptr_arr + 4u);
        u32 offs1 = m.rd32(offs_ptr_arr + 4u);
        if (!buf1 || !offs1) return false;
        if (about_patched_buf1_ == buf1 && about_patched_lang_ == normalize_language(lang)) return true;

        u32 o0 = m.rd32(offs1 + 6632u * 4u);
        u32 o1 = m.rd32(offs1 + 6633u * 4u);
        u32 total_end = m.rd32(offs1 + 6733u * 4u);
        if (!(o0 <= o1 && o1 <= total_end && total_end < 4u * 1024u * 1024u)) return false;

        Bytes cur_raw = m.rdbytes(buf1 + o0, o1 - o0);
        std::string cur_s = utf16be_to_utf8(cur_raw);
        if (cur_s.find("David Kalil Braga") != std::string::npos) {
            about_patched_buf1_ = buf1;
            about_patched_lang_ = normalize_language(lang);
            return true;
        }

        std::string new_s = about_credit_prefix(lang, kPortVersion) + cur_s;
        Bytes new_raw = utf8_to_utf16be(new_s);
        i32 delta = static_cast<i32>(new_raw.size()) - static_cast<i32>(o1 - o0);

        Bytes tail_bytes;
        if (total_end > o1) tail_bytes = m.rdbytes(buf1 + o1, total_end - o1);

        u32 max_tbl = m.rd32(tu + 0x18u);
        u32 max_sz = max_tbl ? m.rd32(max_tbl + 4u) : 0u;
        u32 needed = std::max<u32>(max_sz, static_cast<u32>(i64(total_end) + delta + 1024));
        u32 avail = m.alloc_size(buf1);

        u32 target_buf = buf1;
        if (avail >= static_cast<u32>(i64(total_end) + std::max<i32>(0, delta))) {
            m.wrbytes(buf1 + o0, new_raw);
            if (!tail_bytes.empty()) m.wrbytes(buf1 + o0 + static_cast<u32>(new_raw.size()), tail_bytes);
        } else {
            u32 new_buf = m.malloc_bytes(needed);
            if (o0 > 0) m.wrbytes(new_buf, m.rdbytes(buf1, o0));
            m.wrbytes(new_buf + o0, new_raw);
            if (!tail_bytes.empty()) m.wrbytes(new_buf + o0 + static_cast<u32>(new_raw.size()), tail_bytes);
            m.wr32(buf_ptr_arr + 4u, new_buf);
            if (max_tbl && max_sz < needed) m.wr32(max_tbl + 4u, needed);
            target_buf = new_buf;
        }

        constexpr size_t kTailCount = 6734u - 6633u;
        Bytes tail_off_bytes = m.rdbytes(offs1 + 6633u * 4u, kTailCount * 4u);
        for (size_t i = 0; i < kTailCount; ++i) {
            u32 old_v = read_u32_le(tail_off_bytes.data() + i * 4u);
            u32 shifted = static_cast<u32>(static_cast<i64>(old_v) + delta);
            write_u32_le(tail_off_bytes.data() + i * 4u, shifted);
        }
        m.wrbytes(offs1 + 6633u * 4u, tail_off_bytes);
        about_patched_buf1_ = target_buf;
        about_patched_lang_ = normalize_language(lang);
        return true;
    } catch (...) {
        return false;
    }
}

void InputController::refresh_guest_banner_live(Runtime& rt, u32 app) {
    Machine& m = rt.machine();
    u32 app_state = m.rd32(app + 0x78u);
    if (app_state != 2u && app_state != 8u) return;
    u32 gw = m.rd32(0x00039d30u);
    if (!gw || m.rd32(gw + 0x40u) <= 0x20u) return;
    if (!m.rd32(app + 0x1cu) || !m.rd32(app + 0x20u) || !m.rd32(app + 0x28u)) return;
    i32 banner_tid = m.rds16(gw + 0x611au);
    if (banner_tid < 0) return;
    u8 prev_page = m.rd8(gw + 0x611du);
    m.call(0x0002787cu, {static_cast<u32>(banner_tid), 0u, 0u}, rt.host_sp());
    i32 new_lines = m.rds8(gw + 0x611cu);
    if (prev_page > 0 && new_lines > 0) {
        i32 page = prev_page;
        while (page >= new_lines && page >= 3) page -= 3;
        if (page >= 0 && page < new_lines) m.wr8(gw + 0x611du, static_cast<u8>(page));
    }
}

void InputController::apply_guest_language_live(Runtime& rt, const std::string& lang) {
    rt.bundle().set_language(lang);
    Machine& m = rt.machine();
    try {
        u32 tu = m.rd32(0x000d8220u);
        if (tu) {
            Bytes t_bytes;
            rt.bundle().read_asset("newres/texts/t_en", t_bytes);
            u32 blob_bytes = (t_bytes.size() > 6734u * 4u) ? static_cast<u32>(t_bytes.size() - 6734u * 4u) : 0u;
            u32 chunk_idx = (m.rds8(tu + 0x29u) > 1) ? 1u : 0u;
            u32 sizes_tbl = m.rd32(tu + 0x14u);
            if (sizes_tbl) {
                u32 lang0_sizes = m.rd32(sizes_tbl);
                if (lang0_sizes) m.wr32(lang0_sizes + chunk_idx * 4u, blob_bytes);
            }
            u32 max_tbl = m.rd32(tu + 0x18u);
            if (max_tbl && m.rd32(max_tbl + chunk_idx * 4u) < blob_bytes) {
                m.wr32(max_tbl + chunk_idx * 4u, blob_bytes);
                u32 bufs_tbl = m.rd32(tu + 0x20u);
                if (bufs_tbl) m.wr32(bufs_tbl + chunk_idx * 4u, 0u);
            }
            m.wr8(tu + 0x34u, 0xFFu);
            about_patched_buf1_ = 0;
            m.call(0x0001230cu, {0u}, rt.host_sp());
        }
        patch_guest_about_credits(rt, lang);

        u32 app = m.rd32(0x000d7dd4u);
        if (app) {
            for (u32 mgr_off : {0x40u, 0x44u}) {
                u32 mgr = m.rd32(app + mgr_off);
                if (!mgr) continue;
                u32 root = m.rd32(mgr + 0x90u);
                if (!root) continue;
                u32 vtable = m.rd32(root);
                if (!vtable) continue;
                u32 vfn = m.rd32(vtable + 0x3cu);
                if (vfn) m.call(vfn, {root}, rt.host_sp());
            }
            refresh_guest_banner_live(rt, app);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HL3 PC] Warning during live guest language refresh: %s\n", e.what());
    }
}

void InputController::invalidate_language_cache() {
    injected_btn_tex_key_.clear();
}

void InputController::show_osd(const std::string& badge, const std::string& title, const std::string& subtitle,
                               RgbaColor accent, double duration_sec) {
    double now = now_monotonic();
    osd_badge_ = badge;
    osd_title_ = title;
    osd_subtitle_ = subtitle;
    osd_accent_ = accent;
    osd_started_ = now;
    osd_until_ = now + std::max(0.2, duration_sec);
}

void InputController::touch_down(Runtime& rt, const std::string& tag, float x, float y) {
    if (active_touches_.count(tag) || rt.touch_responder == 0) return;
    double ts = now_monotonic();
    u32 t_obj = rt.objc().new_obj_with("UITouch", TouchPoint{x, y, x, y, 0u, ts});
    u32 evt_obj = rt.objc().new_obj_with("UIEvent", std::vector<u32>{t_obj});
    u32 set_obj = rt.objc().new_obj_with("NSSet", std::vector<u32>{t_obj});
    active_touches_[tag] = {t_obj, evt_obj, set_obj};
    rt.send(rt.touch_responder, "touchesBegan:withEvent:", {set_obj, evt_obj});
}

void InputController::touch_move(Runtime& rt, const std::string& tag, float x, float y) {
    auto it = active_touches_.find(tag);
    if (it == active_touches_.end() || rt.touch_responder == 0) return;
    const ActiveTouch& at = it->second;
    if (auto* tp = rt.objc().get_payload<TouchPoint>(at.touch_obj)) {
        tp->prev_x = tp->x;
        tp->prev_y = tp->y;
        tp->x = x;
        tp->y = y;
        tp->phase = 1u;
        tp->timestamp = now_monotonic();
    }
    rt.send(rt.touch_responder, "touchesMoved:withEvent:", {at.set_obj, at.event_obj});
}

void InputController::touch_up(Runtime& rt, const std::string& tag) {
    auto it = active_touches_.find(tag);
    if (it == active_touches_.end() || rt.touch_responder == 0) return;
    ActiveTouch at = it->second;
    active_touches_.erase(it);
    if (auto* tp = rt.objc().get_payload<TouchPoint>(at.touch_obj)) {
        tp->phase = 3u;
        tp->timestamp = now_monotonic();
    }
    rt.send(rt.touch_responder, "touchesEnded:withEvent:", {at.set_obj, at.event_obj});
}

void InputController::tap_once(Runtime& rt, const std::string& tag, float x, float y, int hold_frames) {
    touch_down(rt, tag, x, y);
    pending_tap_ups_[tag] = std::max(1, hold_frames);
}

void InputController::swipe_carousel(Runtime& rt, const std::string& dir) {
    if (carousel_cooldown_ > 0 || rt.touch_responder == 0) return;
    carousel_cooldown_ = 14;
    float start_x = (dir == "right") ? 320.0f : 160.0f;
    float end_x = (dir == "right") ? 160.0f : 320.0f;
    float y = 280.0f;
    double ts = now_monotonic();
    u32 t_obj = rt.objc().new_obj_with("UITouch", TouchPoint{start_x, y, start_x, y, 0u, ts});
    u32 evt_obj = rt.objc().new_obj_with("UIEvent", std::vector<u32>{t_obj});
    u32 set_obj = rt.objc().new_obj_with("NSSet", std::vector<u32>{t_obj});
    rt.send(rt.touch_responder, "touchesBegan:withEvent:", {set_obj, evt_obj});
    for (int step = 1; step <= 4; ++step) {
        if (auto* tp = rt.objc().get_payload<TouchPoint>(t_obj)) {
            tp->prev_x = tp->x;
            tp->prev_y = tp->y;
            tp->x = start_x + (end_x - start_x) * (static_cast<float>(step) / 4.0f);
            tp->phase = 1u;
            tp->timestamp = now_monotonic();
        }
        rt.send(rt.touch_responder, "touchesMoved:withEvent:", {set_obj, evt_obj});
    }
    if (auto* tp = rt.objc().get_payload<TouchPoint>(t_obj)) {
        tp->phase = 3u;
        tp->timestamp = now_monotonic();
    }
    rt.send(rt.touch_responder, "touchesEnded:withEvent:", {set_obj, evt_obj});
}

void InputController::scroll_box(Runtime& rt, const UiNode& box, const std::string& dir_name, const EngineState& st) {
    if (scroll_cooldown_ > 0 || rt.touch_responder == 0) return;
    scroll_cooldown_ = 8;
    bool is_hud = (st.hud_popup_id != 0);
    auto [ox, oy] = is_hud ? std::pair<float, float>{0.0f, 0.0f} : ui_main_offset(rt, st);
    float cx = box.cx + ox;
    float fy0 = box.fy0 + oy;
    float fy1 = box.fy1 + oy;
    float mid_y = (fy0 + fy1) * 0.5f;
    float span = std::min(44.0f, std::max(16.0f, (fy1 - fy0) * 0.35f));
    float start_y = (dir_name == "down") ? (mid_y + span * 0.5f) : (mid_y - span * 0.5f);
    float end_y = (dir_name == "down") ? (mid_y - span * 0.5f) : (mid_y + span * 0.5f);

    double ts = now_monotonic();
    u32 t_obj = rt.objc().new_obj_with("UITouch", TouchPoint{cx, start_y, cx, start_y, 0u, ts});
    u32 evt_obj = rt.objc().new_obj_with("UIEvent", std::vector<u32>{t_obj});
    u32 set_obj = rt.objc().new_obj_with("NSSet", std::vector<u32>{t_obj});
    rt.send(rt.touch_responder, "touchesBegan:withEvent:", {set_obj, evt_obj});
    for (int step = 1; step <= 4; ++step) {
        if (auto* tp = rt.objc().get_payload<TouchPoint>(t_obj)) {
            tp->prev_x = tp->x;
            tp->prev_y = tp->y;
            tp->y = start_y + (end_y - start_y) * (static_cast<float>(step) / 4.0f);
            tp->phase = 1u;
            tp->timestamp = now_monotonic();
        }
        rt.send(rt.touch_responder, "touchesMoved:withEvent:", {set_obj, evt_obj});
    }
    if (auto* tp = rt.objc().get_payload<TouchPoint>(t_obj)) {
        tp->phase = 3u;
        tp->timestamp = now_monotonic();
    }
    rt.send(rt.touch_responder, "touchesEnded:withEvent:", {set_obj, evt_obj});
}

void InputController::handle_page_tab(Runtime& rt, int delta) {
    EngineState st = get_engine_state(rt);
    if (st.active_popup_id != 0 || st.app_state != 8) return;
    if (st.form_id == 2) {
        int n = static_cast<int>(kPauseMenuItemsY.size());
        menu_index_ = (menu_index_ + delta % n + n) % n;
        return;
    }
    if (st.form_id == 10) {
        std::vector<UiNode> ctrls, scrolls;
        sync_focus(rt, st, ctrls, scrolls);
        std::vector<UiNode> tab_ctrls;
        for (const auto& c : ctrls) {
            if (c.cid == 1005 || c.cid == 1003 || c.cid == 1004) tab_ctrls.push_back(c);
        }
        std::sort(tab_ctrls.begin(), tab_ctrls.end(), [](const UiNode& a, const UiNode& b) { return a.cx < b.cx; });
        if (!tab_ctrls.empty()) {
            int n = static_cast<int>(tab_ctrls.size());
            skill_tab_idx_ = (skill_tab_idx_ + delta % n + n) % n;
            const auto& tgt = tab_ctrls[skill_tab_idx_];
            auto [tx, ty] = main_pt(rt, tgt.cx, tgt.cy, st);
            tap_once(rt, "skill_tab", tx, ty, 2);
        } else {
            static const float kCoords[3][2] = {{120.0f, 25.0f}, {240.0f, 25.0f}, {360.0f, 25.0f}};
            skill_tab_idx_ = (skill_tab_idx_ + delta % 3 + 3) % 3;
            auto [tx, ty] = main_pt(rt, kCoords[skill_tab_idx_][0], kCoords[skill_tab_idx_][1], st);
            tap_once(rt, "skill_tab", tx, ty, 2);
        }
        return;
    }
    u16 prev_cid = 0, next_cid = 0;
    if (st.form_id == 8) {
        prev_cid = 818;
        next_cid = 817;
    } else if (st.form_id == 118) {
        prev_cid = 11809;
        next_cid = 11810;
    } else if (st.form_id == 6) {
        prev_cid = 640;
        next_cid = 641;
    }
    if (prev_cid != 0) {
        std::vector<UiNode> ctrls, scrolls;
        sync_focus(rt, st, ctrls, scrolls);
        u16 want = (delta < 0) ? prev_cid : next_cid;
        for (const auto& c : ctrls) {
            if (c.cid == want) {
                auto [tx, ty] = main_pt(rt, c.cx, c.cy, st);
                tap_once(rt, "sub_page", tx, ty, 2);
                return;
            }
        }
    }
}

void InputController::handle_confirm(Runtime& rt) {
    EngineState st = get_engine_state(rt);
    if (st.active_popup_id != 0) {
        if (st.hud_popup_id == 130) {
            tap_once(rt, "tut_confirm", kTutorialNextX, kTutorialNextY, 2);
        } else {
            std::vector<UiNode> ctrls, scrolls;
            const UiNode* focused = sync_focus(rt, st, ctrls, scrolls);
            if (focused) {
                float cx = focused->is_slider ? focused->next_x : focused->cx;
                float cy = focused->is_slider ? focused->next_y : focused->cy;
                auto [tx, ty] = main_pt(rt, cx, cy, st);
                tap_once(rt, "popup_confirm", tx, ty, 2);
            } else {
                auto [tx, ty] = main_pt(rt, kPopupConfirmX, kPopupConfirmY, st);
                tap_once(rt, "popup_confirm", tx, ty, 2);
            }
        }
        return;
    }

    if (st.app_state == 8) {
        if (st.form_id == 2) {
            float y = kPauseMenuItemsY[menu_index_];
            auto [tx, ty] = main_pt(rt, 240.0f, y, st);
            tap_once(rt, "pause_opt", tx, ty, 2);
        } else {
            std::vector<UiNode> ctrls, scrolls;
            const UiNode* focused = sync_focus(rt, st, ctrls, scrolls);
            if (focused) {
                if (focused->cid == 99003) {
                    if (actions_.open_port_modal) actions_.open_port_modal();
                } else if (focused->is_slider) {
                    auto [tx, ty] = main_pt(rt, focused->next_x, focused->next_y, st);
                    tap_once(rt, "sub_confirm", tx, ty, 2);
                } else {
                    if (st.form_id == 10) {
                        if (focused->cid == 1005) skill_tab_idx_ = 0;
                        else if (focused->cid == 1003) skill_tab_idx_ = 1;
                        else if (focused->cid == 1004) skill_tab_idx_ = 2;
                    }
                    auto [tx, ty] = main_pt(rt, focused->cx, focused->cy, st);
                    tap_once(rt, "sub_confirm", tx, ty, 2);
                }
            } else {
                auto [tx, ty] = main_pt(rt, kPopupConfirmX, kPopupConfirmY, st);
                tap_once(rt, "sub_confirm", tx, ty, 2);
            }
        }
        return;
    }

    if (st.app_state == 1) {
        if (st.form_id == 100) tap_once(rt, "title_start", kTitleCarouselCenterX, kTitleCarouselCenterY, 2);
        else if (st.form_id == 41) tap_once(rt, "char_confirm", kCharConfirmX, kCharConfirmY, 2);
        else if (st.form_id == 42) tap_once(rt, "class_confirm", kClassConfirmX, kClassConfirmY, 2);
        else if (st.form_id == 43) tap_once(rt, "summary_confirm", kSummaryConfirmX, kSummaryConfirmY, 2);
        else {
            std::vector<UiNode> ctrls, scrolls;
            const UiNode* focused = sync_focus(rt, st, ctrls, scrolls);
            if (focused) {
                if (focused->cid == 99113) {
                    if (actions_.open_port_modal) actions_.open_port_modal();
                } else {
                    tap_once(rt, "menu_confirm", focused->cx, focused->cy, 2);
                }
            } else {
                tap_once(rt, "menu_confirm", kPopupConfirmX, kPopupConfirmY, 2);
            }
        }
        return;
    }

    if (st.app_state == 2) {
        if (st.sub_state == 4 || st.sub_state == 22) {
            auto [tx, ty] = main_pt(rt, kDialogueAdvanceX, kDialogueAdvanceY, st);
            tap_once(rt, "dialogue_adv", tx, ty, 2);
            return;
        }
        if (st.sub_state == 7) {
            auto [tx, ty] = main_pt(rt, kDialogueAdvanceX, kDialogueAdvanceY, st);
            tap_once(rt, "msg_dismiss", tx, ty, 2);
            return;
        }
        if (st.sub_state == 3) {
            if (st.can_talk) {
                if (active_touches_.count("dpad")) touch_up(rt, "dpad");
                auto [tx, ty] = hud_pt(rt, st.combat_mode ? kHudTalkCombatX : kHudTalkPeacefulX,
                                       st.combat_mode ? kHudTalkCombatY : kHudTalkPeacefulY);
                tap_once(rt, "hud_talk", tx, ty, 2);
            } else if (st.combat_mode) {
                auto [tx, ty] = hud_pt(rt, kHudAttackPrimaryX, kHudAttackPrimaryY);
                tap_once(rt, "hud_attack", tx, ty, 2);
            } else {
                auto [tx, ty] = hud_pt(rt, kHudTalkPeacefulX, kHudTalkPeacefulY);
                tap_once(rt, "hud_interact", tx, ty, 2);
            }
        }
    }
}

void InputController::handle_cancel(Runtime& rt) {
    EngineState st = get_engine_state(rt);
    if (st.active_popup_id != 0) {
        if (st.hud_popup_id == 130) {
            tap_once(rt, "tut_close", kTutorialCloseX, kTutorialCloseY, 2);
        } else {
            std::vector<UiNode> ctrls, scrolls;
            sync_focus(rt, st, ctrls, scrolls);
            const UiNode* cancel_ctrl = nullptr;
            for (const auto& c : ctrls) {
                if (is_cancel_cid(c.cid)) {
                    cancel_ctrl = &c;
                    break;
                }
            }
            if (cancel_ctrl) {
                auto [tx, ty] = main_pt(rt, cancel_ctrl->cx, cancel_ctrl->cy, st);
                tap_once(rt, "popup_cancel", tx, ty, 2);
            } else {
                auto [tx, ty] = main_pt(rt, kPopupCancelX, kPopupCancelY, st);
                tap_once(rt, "popup_cancel", tx, ty, 2);
            }
        }
        return;
    }

    if (st.app_state == 8) {
        if (st.form_id == 46 && st.main_form) {
            UiNode ov;
            if (find_active_overlay(rt, st.main_form, {4708}, ov)) {
                std::vector<UiNode> ctrls, scrolls;
                collect_subtree_controls(rt, ov.ptr, true, ctrls, scrolls);
                const UiNode* close_btn = ctrls.empty() ? nullptr : &ctrls[0];
                for (const auto& c : ctrls) {
                    if (c.cid == 4709 || c.cid == 199 || c.cid == 207) {
                        close_btn = &c;
                        break;
                    }
                }
                if (close_btn) {
                    auto [tx, ty] = main_pt(rt, close_btn->cx, close_btn->cy, st);
                    tap_once(rt, "quest_detail_close", tx, ty, 2);
                    return;
                }
            }
        }
        if (st.form_id == 8 && st.main_form) {
            UiNode ov;
            if (find_active_overlay(rt, st.main_form, {837, 836}, ov)) {
                std::vector<UiNode> ctrls, scrolls;
                collect_subtree_controls(rt, ov.ptr, true, ctrls, scrolls);
                for (const auto& c : ctrls) {
                    if (c.cid == 199 || c.cid == 207 || c.cid == 306) {
                        auto [tx, ty] = main_pt(rt, c.cx, c.cy, st);
                        tap_once(rt, "item_ov_cancel", tx, ty, 2);
                        return;
                    }
                }
            }
        }
        auto [tx, ty] = main_pt(rt, kPauseMenuBackX, kPauseMenuBackY, st);
        tap_once(rt, "pause_back", tx, ty, 2);
        return;
    }

    if (st.app_state == 1) {
        if (st.form_id == 40 || st.form_id == 41 || st.form_id == 42 || st.form_id == 43 ||
            (st.form_id >= 112 && st.form_id <= 118)) {
            tap_once(rt, "pregame_back", kBtnBackX, kBtnBackY, 2);
        }
        return;
    }

    if (st.app_state == 2) {
        if (st.sub_state == 4 || st.sub_state == 22) {
            handle_skip(rt);
            return;
        }
        if (st.sub_state == 7) {
            auto [tx, ty] = main_pt(rt, kDialogueAdvanceX, kDialogueAdvanceY, st);
            tap_once(rt, "msg_dismiss", tx, ty, 2);
            return;
        }
        if (st.sub_state == 3) {
            auto [tx, ty] = hud_pt(rt, kHudPortraitMenuX, kHudPortraitMenuY);
            tap_once(rt, "open_pause", tx, ty, 2);
        }
    }
}

void InputController::handle_skip(Runtime& rt) {
    EngineState st = get_engine_state(rt);
    if (st.active_popup_id == 133) {
        auto [tx, ty] = main_pt(rt, kPopupConfirmX, kPopupConfirmY, st);
        tap_once(rt, "skip_yes", tx, ty, 2);
        return;
    }
    if (st.sub_state == 4 || st.sub_state == 22 || st.form_id == 44) {
        auto [tx, ty] = main_pt(rt, kCutsceneSkipX, kCutsceneSkipY, st);
        tap_once(rt, "cutscene_skip", tx, ty, 2);
    }
}

void InputController::handle_nav(Runtime& rt, const std::string& dir_name) {
    EngineState st = get_engine_state(rt);
    if (st.app_state == 1 && st.form_id == 100) {
        if (dir_name == "left" || dir_name == "up") swipe_carousel(rt, "left");
        else if (dir_name == "right" || dir_name == "down") swipe_carousel(rt, "right");
        return;
    }
    if (st.app_state == 1 && st.form_id == 41) {
        if (dir_name == "left" || dir_name == "up") tap_once(rt, "sel_kei", kCharKeiX, kCharKeiY, 2);
        else if (dir_name == "right" || dir_name == "down") tap_once(rt, "sel_rits", kCharRitsX, kCharRitsY, 2);
        return;
    }
    if (st.app_state == 1 && st.form_id == 42) {
        if (dir_name == "left" || dir_name == "up") tap_once(rt, "class_prev", kClassArrowPrevX, kClassArrowPrevY, 2);
        else if (dir_name == "right" || dir_name == "down")
            tap_once(rt, "class_next", kClassArrowNextX, kClassArrowNextY, 2);
        return;
    }
    if (st.app_state == 8 && st.form_id == 2 && st.active_popup_id == 0) {
        int n = static_cast<int>(kPauseMenuItemsY.size());
        if (dir_name == "up" || dir_name == "left") menu_index_ = (menu_index_ - 1 + n) % n;
        else if (dir_name == "down" || dir_name == "right") menu_index_ = (menu_index_ + 1) % n;
        return;
    }

    std::vector<UiNode> ctrls, scrolls;
    const UiNode* focused = sync_focus(rt, st, ctrls, scrolls);
    if (ctrls.empty() && scrolls.empty()) return;

    if (focused && focused->is_slider) {
        bool is_hud = (st.hud_popup_id != 0);
        if (dir_name == "left") {
            auto [tx, ty] = main_pt(rt, focused->prev_x, focused->prev_y, st, is_hud);
            tap_once(rt, "slider_prev", tx, ty, 2);
            return;
        }
        if (dir_name == "right") {
            auto [tx, ty] = main_pt(rt, focused->next_x, focused->next_y, st, is_hud);
            tap_once(rt, "slider_next", tx, ty, 2);
            return;
        }
    }

    if (!scrolls.empty() && (dir_name == "up" || dir_name == "down")) {
        if (ctrls.size() <= 1 || !focused) {
            scroll_box(rt, scrolls[0], dir_name, st);
            return;
        }
        int next_idx = move_focus_2d(ctrls, focus_idx_, dir_name);
        if (std::abs(ctrls[next_idx].cy - focused->cy) < 14.0f) {
            scroll_box(rt, scrolls[0], dir_name, st);
            return;
        }
    }

    if (ctrls.size() > 1) {
        int next_idx = move_focus_2d(ctrls, focus_idx_, dir_name);
        focus_idx_ = next_idx;
        focus_cid_ = ctrls[next_idx].cid;
    }
}

void InputController::tick_timers_and_touches(Runtime& rt) {
    if (carousel_cooldown_ > 0) --carousel_cooldown_;
    if (nav_cooldown_ > 0) --nav_cooldown_;
    if (scroll_cooldown_ > 0) --scroll_cooldown_;

    std::vector<std::string> to_release;
    for (auto it = pending_tap_ups_.begin(); it != pending_tap_ups_.end();) {
        if (it->second <= 1) {
            to_release.push_back(it->first);
            it = pending_tap_ups_.erase(it);
        } else {
            it->second -= 1;
            ++it;
        }
    }
    for (const auto& tag : to_release) {
        touch_up(rt, tag);
    }
}

void InputController::handle_gamepad_button(int btn, Runtime& rt, PortOptionsModal* modal) {
    if (modal && modal->is_open()) {
        if (btn == 0) modal->handle_confirm();
        else if (btn == 1 || btn == 7) modal->handle_cancel();
        return;
    }
    EngineState st = get_engine_state(rt);
    bool is_field = (st.app_state == 2 && st.sub_state == 3 && st.active_popup_id == 0);
    if (btn == 0) {
        handle_confirm(rt);
    } else if (btn == 1) {
        handle_cancel(rt);
    } else if (btn == 2) {
        if (is_field) {
            auto [tx, ty] = st.combat_mode ? hud_pt(rt, kHudAttackPrimaryX, kHudAttackPrimaryY)
                                           : hud_pt(rt, kHudSkill1X, kHudSkill1Y);
            tap_once(rt, st.combat_mode ? "pad_attack" : "skill1", tx, ty, 2);
        }
    } else if (btn == 3) {
        if (st.sub_state == 4 || st.sub_state == 22 || st.form_id == 44) handle_skip(rt);
        else if (is_field) {
            auto [tx, ty] = hud_pt(rt, kHudAttackSecondaryX, kHudAttackSecondaryY);
            tap_once(rt, "attack2", tx, ty, 2);
        }
    } else if (btn == 4) {
        if (is_field) {
            auto [tx, ty] = hud_pt(rt, kHudPotion1X, kHudPotion1Y);
            tap_once(rt, "potion1", tx, ty, 3);
        } else if (st.app_state == 8 && st.active_popup_id == 0) {
            handle_page_tab(rt, -1);
        }
    } else if (btn == 5) {
        if (is_field) {
            auto [tx, ty] = hud_pt(rt, kHudPotion2X, kHudPotion2Y);
            tap_once(rt, "potion2", tx, ty, 3);
        } else if (st.app_state == 8 && st.active_popup_id == 0) {
            handle_page_tab(rt, 1);
        }
    } else if (btn == 6) {
        if (is_field) {
            auto [tx, ty] = hud_pt(rt, kHudMapX, kHudMapY);
            tap_once(rt, "hud_map", tx, ty, 3);
        }
    } else if (btn == 7) {
        if (st.sub_state == 4 || st.sub_state == 22 || st.form_id == 44) handle_skip(rt);
        else if (is_field) {
            auto [tx, ty] = hud_pt(rt, kHudPortraitMenuX, kHudPortraitMenuY);
            tap_once(rt, "open_pause", tx, ty, 3);
        } else if (st.app_state == 8) {
            handle_cancel(rt);
        }
    }
}

bool InputController::process_sdl_event(const SDL_Event& ev, Runtime& rt, PortOptionsModal* modal, int win_w,
                                          int win_h) {
    auto win_to_game = [&](int px, int py) -> std::pair<float, float> {
        if (rt.gl_bridge()) return rt.gl_bridge()->window_to_game_coord(px, py);
        return {
            static_cast<float>(px) / std::max(1, win_w) * 480.0f,
            static_cast<float>(py) / std::max(1, win_h) * 320.0f,
        };
    };

    switch (ev.type) {
    case SDL_QUIT:
        if (actions_.request_quit) actions_.request_quit();
        return false;

    case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_RESIZED || ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            if (actions_.on_window_resized) actions_.on_window_resized(ev.window.data1, ev.window.data2);
        } else if (ev.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
                   ev.window.event == SDL_WINDOWEVENT_RESTORED) {
            if (actions_.on_window_sync_check) actions_.on_window_sync_check();
        } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                   ev.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            release_all_touches(rt);
        }
        return true;

    case SDL_MOUSEBUTTONDOWN:
        if (ev.button.which == SDL_TOUCH_MOUSEID) return true;
        if (ev.button.button == SDL_BUTTON_LEFT) {
            if (modal && modal->is_open()) {
                modal->handle_mouse_down(ev.button.x, ev.button.y, win_w, win_h);
                modal_consumed_click_ = true;
                return true;
            }
            auto [gx, gy] = win_to_game(ev.button.x, ev.button.y);
            EngineState st = get_engine_state(rt);
            InjectedButtonSpec spec;
            if (get_injected_port_btn_spec(rt, st, spec)) {
                if (gx >= (spec.fx0 + spec.ox) && gx <= (spec.fx1 + spec.ox) &&
                    gy >= (spec.fy0 + spec.oy) && gy <= (spec.fy1 + spec.oy)) {
                    if (actions_.open_port_modal) actions_.open_port_modal();
                    modal_consumed_click_ = true;
                    return true;
                }
            }
            touch_down(rt, "mouse", gx, gy);
        }
        return true;

    case SDL_MOUSEBUTTONUP:
        if (ev.button.which == SDL_TOUCH_MOUSEID) return true;
        if (ev.button.button == SDL_BUTTON_LEFT) {
            if (modal_consumed_click_) {
                modal_consumed_click_ = false;
                return true;
            }
            touch_up(rt, "mouse");
        }
        return true;

    case SDL_MOUSEMOTION:
        if (ev.motion.which == SDL_TOUCH_MOUSEID) return true;
        if (modal && modal->is_open()) {
            modal->handle_mouse_motion(ev.motion.x, ev.motion.y, win_w, win_h);
            return true;
        }
        if (active_touches_.count("mouse")) {
            auto [gx, gy] = win_to_game(ev.motion.x, ev.motion.y);
            touch_move(rt, "mouse", gx, gy);
        }
        return true;

    case SDL_FINGERDOWN: {
        int px = static_cast<int>(std::lround(ev.tfinger.x * static_cast<float>(std::max(1, win_w))));
        int py = static_cast<int>(std::lround(ev.tfinger.y * static_cast<float>(std::max(1, win_h))));
        if (modal && modal->is_open()) {
            modal->handle_mouse_down(px, py, win_w, win_h);
            modal_consumed_fingers_.insert(ev.tfinger.fingerId);
            return true;
        }
        auto [gx, gy] = win_to_game(px, py);
        EngineState st = get_engine_state(rt);
        InjectedButtonSpec spec;
        if (get_injected_port_btn_spec(rt, st, spec)) {
            if (gx >= (spec.fx0 + spec.ox) && gx <= (spec.fx1 + spec.ox) &&
                gy >= (spec.fy0 + spec.oy) && gy <= (spec.fy1 + spec.oy)) {
                if (actions_.open_port_modal) actions_.open_port_modal();
                modal_consumed_fingers_.insert(ev.tfinger.fingerId);
                return true;
            }
        }
        std::string fkey = "finger_" + std::to_string(static_cast<long long>(ev.tfinger.fingerId));
        touch_down(rt, fkey, gx, gy);
        return true;
    }

    case SDL_FINGERMOTION: {
        int px = static_cast<int>(std::lround(ev.tfinger.x * static_cast<float>(std::max(1, win_w))));
        int py = static_cast<int>(std::lround(ev.tfinger.y * static_cast<float>(std::max(1, win_h))));
        if (modal_consumed_fingers_.count(ev.tfinger.fingerId) > 0 || (modal && modal->is_open())) {
            if (modal && modal->is_open()) {
                modal->handle_mouse_motion(px, py, win_w, win_h);
            }
            return true;
        }
        std::string fkey = "finger_" + std::to_string(static_cast<long long>(ev.tfinger.fingerId));
        if (active_touches_.count(fkey)) {
            auto [gx, gy] = win_to_game(px, py);
            touch_move(rt, fkey, gx, gy);
        }
        return true;
    }

    case SDL_FINGERUP: {
        if (modal_consumed_fingers_.erase(ev.tfinger.fingerId) > 0) {
            return true;
        }
        std::string fkey = "finger_" + std::to_string(static_cast<long long>(ev.tfinger.fingerId));
        touch_up(rt, fkey);
        return true;
    }

    case SDL_KEYDOWN: {
        SDL_Keycode key = ev.key.keysym.sym;
        if (key == SDLK_AC_BACK) {
            key = SDLK_ESCAPE;
        }
        if (key == SDLK_F7) {
            if (actions_.toggle_language) actions_.toggle_language();
            return true;
        }
        if (key == SDLK_F8) {
            if (actions_.toggle_fps) actions_.toggle_fps();
            return true;
        }
        if (key == SDLK_F9) {
            if (actions_.cycle_aspect) actions_.cycle_aspect();
            return true;
        }
        if (key == SDLK_F11) {
            if (actions_.toggle_fullscreen) actions_.toggle_fullscreen();
            return true;
        }
        if (key == SDLK_F12) {
            if (actions_.request_screenshot) actions_.request_screenshot();
            return true;
        }
        if (modal && modal->is_open()) {
            modal->handle_key(key);
            return true;
        }
        if (key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS) {
            if (rt.audio_mixer()) {
                rt.audio_mixer()->set_master_volume(rt.audio_mixer()->master_volume() + 0.1f);
                std::printf("[HL3 Audio] Volume: %d%%\n",
                            static_cast<int>(rt.audio_mixer()->master_volume() * 100.0f));
            }
            return true;
        }
        if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
            if (rt.audio_mixer()) {
                rt.audio_mixer()->set_master_volume(rt.audio_mixer()->master_volume() - 0.1f);
                std::printf("[HL3 Audio] Volume: %d%%\n",
                            static_cast<int>(rt.audio_mixer()->master_volume() * 100.0f));
            }
            return true;
        }
        if (key == SDLK_m || key == SDLK_TAB) {
            EngineState st = get_engine_state(rt);
            if (st.app_state == 2 && st.sub_state == 3 && st.active_popup_id == 0) {
                auto [tx, ty] = hud_pt(rt, kHudMapX, kHudMapY);
                tap_once(rt, "hud_map", tx, ty, 3);
            } else if (st.sub_state == 4 || st.sub_state == 22 || st.form_id == 44) {
                handle_skip(rt);
            } else if (key == SDLK_TAB && st.app_state == 8 && st.active_popup_id == 0) {
                handle_page_tab(rt, 1);
            } else if (key == SDLK_m && rt.audio_mixer()) {
                rt.audio_mixer()->toggle_mute();
                std::printf("[HL3 Audio] Muted: %s\n", rt.audio_mixer()->muted() ? "true" : "false");
            }
            return true;
        }
        if (key == SDLK_PAGEUP || key == SDLK_LEFTBRACKET) {
            handle_page_tab(rt, -1);
            return true;
        }
        if (key == SDLK_PAGEDOWN || key == SDLK_RIGHTBRACKET) {
            handle_page_tab(rt, 1);
            return true;
        }
        if (key == SDLK_n) {
            if (rt.audio_mixer()) {
                rt.audio_mixer()->toggle_mute();
                std::printf("[HL3 Audio] Muted: %s\n", rt.audio_mixer()->muted() ? "true" : "false");
            }
            return true;
        }
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE || key == SDLK_e || key == SDLK_z) {
            handle_confirm(rt);
            return true;
        }
        if (key == SDLK_ESCAPE || key == SDLK_BACKSPACE) {
            handle_cancel(rt);
            return true;
        }
        if (key == SDLK_s) {
            EngineState st = get_engine_state(rt);
            if (st.sub_state == 4 || st.sub_state == 22 || st.form_id == 44) handle_skip(rt);
            else if (st.app_state != 2 || st.active_popup_id != 0) handle_nav(rt, "down");
            return true;
        }
        if (key == SDLK_UP || key == SDLK_w) {
            handle_nav(rt, "up");
            return true;
        }
        if (key == SDLK_DOWN) {
            handle_nav(rt, "down");
            return true;
        }
        if (key == SDLK_LEFT || key == SDLK_a) {
            handle_nav(rt, "left");
            return true;
        }
        if (key == SDLK_RIGHT || key == SDLK_d) {
            handle_nav(rt, "right");
            return true;
        }

        // Field HUD & Pause shortcuts
        EngineState st = get_engine_state(rt);
        bool is_field = (st.app_state == 2 && st.sub_state == 3 && st.active_popup_id == 0);
        if (is_field) {
            if (key == SDLK_q || key == SDLK_1) {
                auto [tx, ty] = hud_pt(rt, kHudPotion1X, kHudPotion1Y);
                tap_once(rt, "potion1", tx, ty, 3);
            } else if (key == SDLK_k || key == SDLK_2) {
                auto [tx, ty] = hud_pt(rt, kHudPotion2X, kHudPotion2Y);
                tap_once(rt, "potion2", tx, ty, 3);
            } else if (key == SDLK_u || key == SDLK_3) {
                auto [tx, ty] = hud_pt(rt, kHudSkill1X, kHudSkill1Y);
                tap_once(rt, "skill1", tx, ty, 2);
            } else if (key == SDLK_i || key == SDLK_4) {
                auto [tx, ty] = hud_pt(rt, kHudSkill2X, kHudSkill2Y);
                tap_once(rt, "skill2", tx, ty, 2);
            } else if (key == SDLK_o || key == SDLK_5) {
                auto [tx, ty] = hud_pt(rt, kHudSkill3X, kHudSkill3Y);
                tap_once(rt, "skill3", tx, ty, 2);
            } else if (key == SDLK_j) {
                auto [tx, ty] = hud_pt(rt, kHudAttackPrimaryX, kHudAttackPrimaryY);
                tap_once(rt, "attack1", tx, ty, 2);
            } else if (key == SDLK_l || key == SDLK_h) {
                auto [tx, ty] = hud_pt(rt, kHudAttackSecondaryX, kHudAttackSecondaryY);
                tap_once(rt, "attack2", tx, ty, 2);
            } else if (key == SDLK_p || key == SDLK_c) {
                auto [tx, ty] = hud_pt(rt, kHudPortraitMenuX, kHudPortraitMenuY);
                tap_once(rt, "portrait_menu", tx, ty, 3);
            }
        } else if ((key == SDLK_p || key == SDLK_c) && st.app_state == 8) {
            handle_cancel(rt);
        }
        return true;
    }

    case SDL_JOYDEVICEADDED:
        handle_controller_added(ev.jdevice.which);
        return true;

    case SDL_JOYDEVICEREMOVED:
        handle_controller_removed(ev.jdevice.which);
        return true;

    case SDL_CONTROLLERDEVICEREMOVED:
        handle_controller_removed(ev.cdevice.which);
        return true;

    case SDL_CONTROLLERBUTTONDOWN: {
        int btn = -1;
        switch (ev.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A: btn = 0; break;
        case SDL_CONTROLLER_BUTTON_B: btn = 1; break;
        case SDL_CONTROLLER_BUTTON_X: btn = 2; break;
        case SDL_CONTROLLER_BUTTON_Y: btn = 3; break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: btn = 4; break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: btn = 5; break;
        case SDL_CONTROLLER_BUTTON_BACK: btn = 6; break;
        case SDL_CONTROLLER_BUTTON_START: btn = 7; break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: {
            const char* dir = (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP)      ? "up"
                              : (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)  ? "down"
                              : (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)  ? "left"
                                                                                        : "right";
            if (modal && modal->is_open()) {
                if (nav_cooldown_ == 0) {
                    modal->handle_nav(dir);
                    nav_cooldown_ = 12;
                }
            } else if (nav_cooldown_ == 0) {
                EngineState st = get_engine_state(rt);
                if (st.app_state != 2 || st.active_popup_id != 0) {
                    handle_nav(rt, dir);
                    nav_cooldown_ = 12;
                }
            }
            return true;
        }
        default: break;
        }
        if (btn >= 0) handle_gamepad_button(btn, rt, modal);
        return true;
    }

    case SDL_JOYBUTTONDOWN:
        if (raw_joysticks_.count(ev.jbutton.which)) {
            handle_gamepad_button(static_cast<int>(ev.jbutton.button), rt, modal);
        }
        return true;

    case SDL_JOYHATMOTION:
        if (raw_joysticks_.count(ev.jhat.which) && nav_cooldown_ == 0) {
            u8 hv = ev.jhat.value;
            std::string dir;
            if (hv & SDL_HAT_UP) dir = "up";
            else if (hv & SDL_HAT_DOWN) dir = "down";
            else if (hv & SDL_HAT_LEFT) dir = "left";
            else if (hv & SDL_HAT_RIGHT) dir = "right";
            if (!dir.empty()) {
                if (modal && modal->is_open()) {
                    modal->handle_nav(dir);
                    nav_cooldown_ = 12;
                } else {
                    handle_nav(rt, dir);
                    nav_cooldown_ = 12;
                }
            }
        }
        return true;

    default:
        return true;
    }
}

void InputController::update_continuous_input(Runtime& rt, PortOptionsModal* modal) {
    EngineState st = get_engine_state(rt);
    bool modal_open = (modal && modal->is_open());
    bool in_field = (!modal_open && st.app_state == 2 && st.sub_state == 3 && st.active_popup_id == 0);

    if (!in_field) {
        if (active_touches_.count("dpad")) touch_up(rt, "dpad");
        if ((modal_open || st.app_state != 2 || st.active_popup_id != 0) && nav_cooldown_ == 0) {
            std::string nav_dir;
            for (const auto& [jid, gc] : controllers_) {
                (void)jid;
                if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP)) nav_dir = "up";
                else if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) nav_dir = "down";
                else if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) nav_dir = "left";
                else if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) nav_dir = "right";
                else {
                    float ax = static_cast<float>(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX)) / 32768.0f;
                    float ay = static_cast<float>(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY)) / 32768.0f;
                    if (std::abs(ax) > 0.55f || std::abs(ay) > 0.55f) {
                        nav_dir = (std::abs(ay) >= std::abs(ax)) ? ((ay < 0.0f) ? "up" : "down")
                                                                 : ((ax < 0.0f) ? "left" : "right");
                    }
                }
                if (!nav_dir.empty()) break;
            }
            if (nav_dir.empty()) {
                for (const auto& [jid, js] : raw_joysticks_) {
                    (void)jid;
                    if (SDL_JoystickNumHats(js) > 0) {
                        u8 hv = SDL_JoystickGetHat(js, 0);
                        if (hv & SDL_HAT_UP) nav_dir = "up";
                        else if (hv & SDL_HAT_DOWN) nav_dir = "down";
                        else if (hv & SDL_HAT_LEFT) nav_dir = "left";
                        else if (hv & SDL_HAT_RIGHT) nav_dir = "right";
                    }
                    if (nav_dir.empty() && SDL_JoystickNumAxes(js) >= 2) {
                        float ax = static_cast<float>(SDL_JoystickGetAxis(js, 0)) / 32768.0f;
                        float ay = static_cast<float>(SDL_JoystickGetAxis(js, 1)) / 32768.0f;
                        if (std::abs(ax) > 0.55f || std::abs(ay) > 0.55f) {
                            nav_dir = (std::abs(ay) >= std::abs(ax)) ? ((ay < 0.0f) ? "up" : "down")
                                                                     : ((ax < 0.0f) ? "left" : "right");
                        }
                    }
                    if (!nav_dir.empty()) break;
                }
            }
            if (!nav_dir.empty()) {
                if (modal_open) modal->handle_nav(nav_dir);
                else handle_nav(rt, nav_dir);
                nav_cooldown_ = 12;
            }
        }
        return;
    }

    int numkeys = 0;
    const Uint8* keys = SDL_GetKeyboardState(&numkeys);
    float dx = 0.0f;
    float dy = 0.0f;
    if (keys) {
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) dx -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) dx += 1.0f;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) dy -= 1.0f;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) dy += 1.0f;
    }

    for (const auto& [jid, gc] : controllers_) {
        (void)jid;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) dx = -1.0f;
        else if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) dx = 1.0f;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP)) dy = -1.0f;
        else if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) dy = 1.0f;

        float ax = static_cast<float>(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX)) / 32768.0f;
        float ay = static_cast<float>(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY)) / 32768.0f;
        if (std::abs(ax) > 0.25f) dx = ax;
        if (std::abs(ay) > 0.25f) dy = ay;
    }
    for (const auto& [jid, js] : raw_joysticks_) {
        (void)jid;
        if (SDL_JoystickNumHats(js) > 0) {
            u8 hv = SDL_JoystickGetHat(js, 0);
            if (hv & SDL_HAT_LEFT) dx = -1.0f;
            else if (hv & SDL_HAT_RIGHT) dx = 1.0f;
            if (hv & SDL_HAT_UP) dy = -1.0f;
            else if (hv & SDL_HAT_DOWN) dy = 1.0f;
        }
        if (SDL_JoystickNumAxes(js) >= 2) {
            float ax = static_cast<float>(SDL_JoystickGetAxis(js, 0)) / 32768.0f;
            float ay = static_cast<float>(SDL_JoystickGetAxis(js, 1)) / 32768.0f;
            if (std::abs(ax) > 0.25f) dx = ax;
            if (std::abs(ay) > 0.25f) dy = ay;
        }
    }

    if (std::abs(dx) > 0.05f || std::abs(dy) > 0.05f) {
        float length = std::sqrt(dx * dx + dy * dy);
        float mag = std::min(1.0f, length);
        float nx = (dx / length) * mag;
        float ny = (dy / length) * mag;
        facing_x_ = dx / length;
        facing_y_ = dy / length;
        auto [cx, cy] = hud_pt(rt, kDpadCenterX, kDpadCenterY);
        float target_x = cx + nx * 30.0f;
        float target_y = cy + ny * 30.0f;
        if (!active_touches_.count("dpad")) touch_down(rt, "dpad", cx, cy);
        touch_move(rt, "dpad", target_x, target_y);
    } else if (active_touches_.count("dpad")) {
        touch_up(rt, "dpad");
    }
}

void InputController::draw_focus_rect(GLBridge& gl, const UiNode& node, float ox, float oy, float gw, float gh) const {
    float pad = 2.0f;
    float x0 = std::max(2.0f, node.fx0 + ox - pad);
    float x1 = std::min(gw - 2.0f, node.fx1 + ox + pad);
    float y0 = std::max(2.0f, node.fy0 + oy - pad);
    float y1 = std::min(gh - 2.0f, node.fy1 + oy + pad);
    if (x1 <= x0 || y1 <= y0) return;

    float cx = (x0 + x1) * 0.5f;
    float cy = (y0 + y1) * 0.5f;
    float pulse = 0.8f + 0.2f * std::sin(static_cast<float>(SDL_GetTicks()) * 0.008f);

    gl.overlay_outline_rect(x0, y0, x1 - x0, y1 - y0, 4.0f, 0.05f, 0.05f, 0.1f, 0.7f);
    gl.overlay_outline_rect(x0, y0, x1 - x0, y1 - y0, 2.2f, 1.0f, 0.88f * pulse, 0.18f, 0.96f);

    if (x0 >= 14.0f) {
        gl.overlay_triangle(x0 - 10.0f, cy - 5.5f, x0 - 2.5f, cy, x0 - 10.0f, cy + 5.5f, 1.0f, 0.92f, 0.28f, 1.0f);
    } else {
        float ty = std::max(10.0f, y0);
        gl.overlay_triangle(cx - 5.5f, ty - 9.0f, cx + 5.5f, ty - 9.0f, cx, ty - 2.0f, 1.0f, 0.92f, 0.28f, 1.0f);
    }
}

void InputController::draw_injected_port_buttons(GLBridge& gl, Runtime& rt, const std::string& lang) {
    EngineState st = get_engine_state(rt);
    InjectedButtonSpec spec;
    if (!get_injected_port_btn_spec(rt, st, spec)) return;

    std::string label = tr("game.btn.port_options", lang);
    std::string key = lang + "|" + label;
    if (injected_btn_tex_ == 0 || injected_btn_tex_key_ != key) {
        RgbaCanvas c = build_injected_button_canvas(label, ribbon_rgba_, ribbon_w_, ribbon_h_);
        if (injected_btn_tex_ == 0) {
            injected_btn_tex_ = gl.create_rgba_texture(c.width(), c.height(), c.data(), true);
        } else {
            gl.update_rgba_texture(injected_btn_tex_, c.width(), c.height(), c.data());
        }
        injected_btn_tex_key_ = key;
    }
    if (!injected_btn_tex_) return;

    float x0 = spec.fx0 - 4.0f + spec.ox;
    float x1 = spec.fx1 + 4.0f + spec.ox;
    float y0 = spec.fy0 - 1.5f + spec.oy;
    float y1 = spec.fy1 + 1.5f + spec.oy;

    gl.overlay_begin_game(gl.game_w(), gl.game_h(), true);
    gl.overlay_textured_rect(injected_btn_tex_, x0, y0, x1 - x0, y1 - y0, 0.0f, 0.0f, 1.0f, 1.0f,
                             1.0f, 1.0f, 1.0f, 1.0f);
    gl.overlay_end();
}

void InputController::draw_menu_cursor(GLBridge& gl, Runtime& rt) {
    EngineState st = get_engine_state(rt);
    bool is_pause_main = (st.app_state == 8 && st.form_id == 2 && st.active_popup_id == 0);
    std::vector<UiNode> ctrls, scrolls;
    const UiNode* focused = is_pause_main ? nullptr : sync_focus(rt, st, ctrls, scrolls);
    if (!is_pause_main && !focused) return;

    auto [ox, oy] = (st.hud_popup_id != 0) ? std::pair<float, float>{0.0f, 0.0f} : ui_main_offset(rt, st);
    float gw = gl.game_w();
    float gh = gl.game_h();

    gl.overlay_begin_game(gw, gh, true);
    if (is_pause_main) {
        float y = kPauseMenuItemsY[menu_index_] + oy;
        gl.overlay_outline_rect(82.0f + ox, y - 18.0f, 316.0f, 36.0f, 2.5f, 1.0f, 0.85f, 0.2f, 0.9f);
        gl.overlay_triangle(66.0f + ox, y - 9.0f, 77.0f + ox, y, 66.0f + ox, y + 9.0f, 1.0f, 0.9f, 0.3f, 1.0f);
        gl.overlay_triangle(414.0f + ox, y - 9.0f, 403.0f + ox, y, 414.0f + ox, y + 9.0f, 1.0f, 0.9f, 0.3f, 1.0f);
    } else if (focused) {
        draw_focus_rect(gl, *focused, ox, oy, gw, gh);
    }
    gl.overlay_end();
}

void InputController::draw_osd(GLBridge& gl, int win_w, int win_h) {
    double now = now_monotonic();
    if (osd_title_.empty() || now >= osd_until_) return;
    double elapsed = std::max(0.0, now - osd_started_);
    double remaining = std::max(0.0, osd_until_ - now);
    double t_in = (elapsed < 0.14) ? std::min(1.0, elapsed / 0.14) : 1.0;
    double t_out = (remaining < 0.42) ? std::min(1.0, remaining / 0.42) : 1.0;
    float alpha = static_cast<float>(std::max(0.0, std::min(1.0, t_in * t_out)));
    if (alpha <= 0.01f) return;

    float ui_scale = std::max(1.0f, std::min(2.8f, static_cast<float>(win_h) / 420.0f));
    std::string key = make_osd_key(osd_badge_, osd_title_, osd_subtitle_, osd_accent_, ui_scale);
    if (osd_tex_ == 0 || osd_tex_key_ != key) {
        RgbaCanvas c = build_osd_canvas(osd_badge_, osd_title_, osd_subtitle_, osd_accent_, ui_scale);
        if (osd_tex_ == 0) {
            osd_tex_ = gl.create_rgba_texture(c.width(), c.height(), c.data(), true);
        } else {
            gl.update_rgba_texture(osd_tex_, c.width(), c.height(), c.data());
        }
        osd_tex_key_ = key;
        osd_tex_w_ = c.width();
        osd_tex_h_ = c.height();
    }
    if (!osd_tex_ || osd_tex_w_ <= 0 || osd_tex_h_ <= 0) return;

    float tw = static_cast<float>(osd_tex_w_);
    float th = static_cast<float>(osd_tex_h_);
    float x0 = (static_cast<float>(win_w) - tw) * 0.5f;
    float y0 = 14.0f * ui_scale - static_cast<float>(1.0 - t_in) * 10.0f * ui_scale;

    gl.overlay_begin_window(win_w, win_h);
    gl.overlay_textured_rect(osd_tex_, x0, y0, tw, th, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, alpha);
    gl.overlay_end();
}

}  // namespace hl3

