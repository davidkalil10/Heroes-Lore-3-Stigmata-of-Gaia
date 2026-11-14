#include "options_modal.h"
#include "display_config.h"
#include "i18n.h"
#include "net_http.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hl3 {

namespace {

std::string fmt_mb(size_t bytes) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return std::string(buf);
}

std::string fmt_kb(size_t bytes) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(bytes) / 1024.0);
    return std::string(buf);
}

int px_s(float base, float scale, int min_v = 1) {
    return std::max(min_v, static_cast<int>(std::lround(base * scale)));
}

struct CloudButtonSpec {
    std::string label;
    std::string action;
    bool enabled = true;
    bool primary = false;
    bool danger = false;
};

struct UpdateButtonSpec {
    std::string label;
    std::string action;
    bool enabled = true;
    bool primary = false;
};

std::vector<CloudButtonSpec> build_cloud_buttons(const CloudStatus& st, const LocalSaveInfo& local,
                                                 const std::string& l) {
    std::vector<CloudButtonSpec> btns;
    if (st.authorizing) {
        if (!st.verification_url.empty()) {
            btns.push_back({tr("cloud.auth.open_browser", l), "open_browser", true, true, false});
        }
        btns.push_back({tr("cloud.btn.cancel_auth", l), "cancel_auth", true, false, true});
    } else if (!st.connected) {
        btns.push_back({tr("cloud.btn.connect", l), "connect", !st.busy, true, false});
    } else {
        btns.push_back({tr("cloud.btn.upload", l), "ask_upload", !st.busy && local.exists, true, false});
        btns.push_back({tr("cloud.btn.restore", l), "ask_restore", !st.busy && st.backup_exists, false, false});
        btns.push_back({tr("cloud.btn.refresh", l), "refresh", !st.busy, false, false});
        btns.push_back({tr("cloud.btn.disconnect", l), "ask_disconnect", !st.busy, false, true});
    }
    btns.push_back({tr("common.back", l), "back", true, false, false});
    return btns;
}

std::vector<UpdateButtonSpec> build_update_buttons(const UpdaterState& ust, const std::string& l) {
    std::vector<UpdateButtonSpec> btns;
    bool busy = (ust.phase == UpdatePhase::Checking || ust.phase == UpdatePhase::Downloading ||
                 ust.phase == UpdatePhase::Installing);
    bool has_dl = ust.has_release && !ust.release.asset_url.empty();

    if (ust.phase == UpdatePhase::UpdateAvailable && has_dl) {
        btns.push_back({tr_fmt("update.btn.download", {{"tag", ust.release.tag_name}}, l), "download", true, true});
        btns.push_back({tr("update.btn.check", l), "check", true, false});
    } else if (ust.phase == UpdatePhase::ReadyToInstall) {
        btns.push_back({tr("update.btn.install", l), "install", true, true});
        btns.push_back({tr("update.btn.check", l), "check", true, false});
    } else if (ust.phase == UpdatePhase::ManualStaged) {
        btns.push_back({tr("update.btn.open_staged", l), "open_staged", true, true});
        btns.push_back({tr("update.btn.check", l), "check", true, false});
    } else {
        btns.push_back({tr("update.btn.check", l), "check", !busy, true});
    }
    btns.push_back({tr("update.btn.github", l), "open_github", true, false});
    btns.push_back({tr("common.back", l), "back", true, false});
    return btns;
}

}  // namespace

PortOptionsModal::PortOptionsModal(CloudSaveManager* cloud, UpdateManager* updater, Callbacks cb)
    : cloud_(cloud), updater_(updater), cb_(std::move(cb)) {}

PortOptionsModal::~PortOptionsModal() = default;

void PortOptionsModal::mark_dirty() {
    dirty_ = true;
}

void PortOptionsModal::invalidate_cache() {
    dirty_ = true;
}

void PortOptionsModal::reset_gl(GLBridge& gl) {
    if (tex_id_ != 0) {
        gl.destroy_texture(tex_id_);
        tex_id_ = 0;
    }
    tex_w_ = 0;
    tex_h_ = 0;
    dirty_ = true;
}

std::string PortOptionsModal::lang() const {
    return cb_.get_language ? cb_.get_language() : kDefaultLanguage;
}

UiRect PortOptionsModal::compute_box(int win_w, int win_h) const {
    int safe_w = std::max(1, win_w);
    int safe_h = std::max(1, win_h);
    int min_bh = std::min(safe_h, 340);
    int max_bh = std::max(min_bh, safe_h - 16);
    int bh = std::clamp(static_cast<int>(std::lround(safe_h * 0.88)), min_bh, max_bh);
    int min_bw = std::min(safe_w, 460);
    int max_bw = std::max(min_bw, safe_w - 16);
    int desired_bw = static_cast<int>(std::lround(std::min(safe_w * 0.90, bh * 1.68)));
    int bw = std::clamp(desired_bw, min_bw, max_bw);
    int bx = (safe_w - bw) / 2;
    int by = (safe_h - bh) / 2;
    return {bx, by, bw, bh};
}

void PortOptionsModal::open(ModalPage start_page) {
    is_open_ = true;
    page_ = start_page;
    selected_ = 0;
    cloud_focus_ = 0;
    update_focus_ = 0;
    confirm_focus_ = 1;
    dirty_ = true;
    if (start_page == ModalPage::Cloud && cloud_) {
        auto st = cloud_->get_status();
        if (st.connected && !st.busy) cloud_->refresh_backup();
    }
}

void PortOptionsModal::close() {
    is_open_ = false;
    dirty_ = true;
}

void PortOptionsModal::poll() {
    if (!is_open_) return;
    u32 now = SDL_GetTicks();
    bool live = false;
    if (cloud_) {
        auto cs = cloud_->get_status();
        if (cs.restore_ready && !cs.busy) {
            std::filesystem::path bdir;
            if (cloud_->consume_pending_restore(&bdir)) {
                is_open_ = false;
                dirty_ = true;
                if (cb_.on_cloud_restore_ready) cb_.on_cloud_restore_ready();
                return;
            }
        }
        if (cs.busy || cs.authorizing) live = true;
    }
    if (updater_) {
        auto us = updater_->snapshot();
        if (us.phase == UpdatePhase::Checking || us.phase == UpdatePhase::Downloading ||
            us.phase == UpdatePhase::Installing) {
            live = true;
        }
    }
    if (live && now - last_live_poll_ms_ >= 90u) {
        last_live_poll_ms_ = now;
        dirty_ = true;
    }
}

void PortOptionsModal::activate_options_row(int delta) {
    (void)delta;
    if (selected_ == 0 && cb_.get_fps && cb_.set_fps) {
        cb_.set_fps(cb_.get_fps() == 60 ? 30 : 60, true);
    } else if (selected_ == 1 && cb_.get_aspect && cb_.set_aspect) {
        std::string cur = cb_.get_aspect();
        int idx = 0;
        for (int i = 0; i < static_cast<int>(kUiAspectCycle.size()); ++i) {
            if (cur == kUiAspectCycle[i]) {
                idx = i;
                break;
            }
        }
        int step = (delta < 0) ? -1 : 1;
        int next = (idx + step + static_cast<int>(kUiAspectCycle.size())) % static_cast<int>(kUiAspectCycle.size());
        cb_.set_aspect(kUiAspectCycle[next], true, true);
    } else if (selected_ == 2 && cb_.get_language && cb_.set_language) {
        std::string nxt = (cb_.get_language() == "en") ? "pt-BR" : "en";
        cb_.set_language(nxt, true);
    } else if (selected_ == 3) {
        page_ = ModalPage::Cloud;
        cloud_focus_ = 0;
        if (cloud_) {
            auto st = cloud_->get_status();
            if (st.connected && !st.busy) cloud_->refresh_backup();
        }
    } else if (selected_ == 4 && updater_) {
        updater_->toggle_startup_check();
    } else if (selected_ == 5) {
        page_ = ModalPage::Update;
        update_focus_ = 0;
        if (updater_) {
            auto us = updater_->snapshot();
            if (us.phase == UpdatePhase::Idle || us.phase == UpdatePhase::Error) {
                updater_->start_check(false);
            }
        }
    } else if (selected_ == 6) {
        close();
        return;
    }
    mark_dirty();
}

void PortOptionsModal::activate_action(const std::string& action) {
    if (action == "back") {
        handle_cancel();
        return;
    }
    if (page_ == ModalPage::Options && action.rfind("opt_", 0) == 0) {
        int idx = std::atoi(action.substr(4).c_str());
        selected_ = std::clamp(idx, 0, 6);
        activate_options_row(1);
        return;
    }

    if (action == "connect" && cloud_) {
        cloud_->start_login();
    } else if (action == "open_browser" && cloud_) {
        auto st = cloud_->get_status();
        if (!st.verification_url.empty()) open_url_in_browser(st.verification_url);
    } else if (action == "cancel_auth" && cloud_) {
        cloud_->cancel_login();
    } else if (action == "refresh" && cloud_) {
        cloud_->refresh_backup();
    } else if (action == "ask_upload") {
        page_ = ModalPage::ConfirmUpload;
        confirm_focus_ = 1;
    } else if (action == "ask_restore") {
        page_ = ModalPage::ConfirmRestore;
        confirm_focus_ = 1;
    } else if (action == "ask_disconnect") {
        page_ = ModalPage::ConfirmDisconnect;
        confirm_focus_ = 1;
    } else if (action == "confirm_yes") {
        if (page_ == ModalPage::ConfirmUpload && cloud_) {
            cloud_->upload_backup(cb_.flush_before_upload);
            page_ = ModalPage::Cloud;
        } else if (page_ == ModalPage::ConfirmRestore && cloud_) {
            cloud_->restore_backup();
            page_ = ModalPage::Cloud;
        } else if (page_ == ModalPage::ConfirmDisconnect && cloud_) {
            cloud_->disconnect();
            page_ = ModalPage::Cloud;
        }
        cloud_focus_ = 0;
    } else if (action == "confirm_no") {
        page_ = ModalPage::Cloud;
    } else if (action == "check" && updater_) {
        updater_->start_check(false);
    } else if (action == "download" && updater_) {
        updater_->start_download();
    } else if (action == "install" && updater_) {
        updater_->install_ready_update();
    } else if (action == "open_staged" && updater_) {
        auto us = updater_->snapshot();
        if (!us.staged_root.empty()) open_folder_in_explorer(us.staged_root);
        else if (!us.downloaded_path.empty()) open_folder_in_explorer(us.downloaded_path.parent_path());
    } else if (action == "open_github" && updater_) {
        auto us = updater_->snapshot();
        open_url_in_browser(us.has_release ? us.release.html_url : updater_->releases_url());
    }
    mark_dirty();
}

void PortOptionsModal::handle_nav(const std::string& dir) {
    if (!is_open_) return;
    std::string l = lang();
    if (page_ == ModalPage::Options) {
        if (dir == "up") {
            selected_ = (selected_ - 1 + 7) % 7;
        } else if (dir == "down") {
            selected_ = (selected_ + 1) % 7;
        } else if (dir == "left" || dir == "right") {
            if (selected_ == 0 || selected_ == 1 || selected_ == 2 || selected_ == 4) {
                activate_options_row(dir == "left" ? -1 : 1);
                return;
            }
        }
    } else if (page_ == ModalPage::Cloud && cloud_) {
        auto st = cloud_->get_status();
        auto loc = cloud_->get_local_info();
        auto btns = build_cloud_buttons(st, loc, l);
        int n = static_cast<int>(btns.size());
        if (n > 0) {
            if (dir == "left" || dir == "up") cloud_focus_ = (cloud_focus_ - 1 + n) % n;
            else if (dir == "right" || dir == "down") cloud_focus_ = (cloud_focus_ + 1) % n;
        }
    } else if (page_ == ModalPage::Update && updater_) {
        auto us = updater_->snapshot();
        auto btns = build_update_buttons(us, l);
        int n = static_cast<int>(btns.size());
        if (n > 0) {
            if (dir == "left" || dir == "up") update_focus_ = (update_focus_ - 1 + n) % n;
            else if (dir == "right" || dir == "down") update_focus_ = (update_focus_ + 1) % n;
        }
    } else {
        if (dir == "left" || dir == "right" || dir == "up" || dir == "down") {
            confirm_focus_ = 1 - confirm_focus_;
        }
    }
    mark_dirty();
}

void PortOptionsModal::handle_confirm() {
    if (!is_open_) return;
    std::string l = lang();
    if (page_ == ModalPage::Options) {
        activate_options_row(1);
        return;
    }
    if (page_ == ModalPage::Cloud && cloud_) {
        auto st = cloud_->get_status();
        auto loc = cloud_->get_local_info();
        auto btns = build_cloud_buttons(st, loc, l);
        int idx = std::clamp(cloud_focus_, 0, std::max(0, static_cast<int>(btns.size()) - 1));
        if (!btns.empty() && btns[idx].enabled) {
            activate_action(btns[idx].action);
        }
        return;
    }
    if (page_ == ModalPage::Update && updater_) {
        auto us = updater_->snapshot();
        auto btns = build_update_buttons(us, l);
        int idx = std::clamp(update_focus_, 0, std::max(0, static_cast<int>(btns.size()) - 1));
        if (!btns.empty() && btns[idx].enabled) {
            activate_action(btns[idx].action);
        }
        return;
    }
    activate_action(confirm_focus_ == 0 ? "confirm_yes" : "confirm_no");
}

void PortOptionsModal::handle_cancel() {
    if (!is_open_) return;
    if (page_ == ModalPage::ConfirmUpload || page_ == ModalPage::ConfirmRestore ||
        page_ == ModalPage::ConfirmDisconnect) {
        page_ = ModalPage::Cloud;
        mark_dirty();
        return;
    }
    if (page_ == ModalPage::Cloud || page_ == ModalPage::Update) {
        page_ = ModalPage::Options;
        mark_dirty();
        return;
    }
    close();
}

bool PortOptionsModal::handle_key(SDL_Keycode key) {
    if (!is_open_) return false;
    if (key == SDLK_ESCAPE || key == SDLK_BACKSPACE) {
        handle_cancel();
        return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
        handle_confirm();
        return true;
    }
    if (key == SDLK_UP || key == SDLK_w) {
        handle_nav("up");
        return true;
    }
    if (key == SDLK_DOWN || key == SDLK_s) {
        handle_nav("down");
        return true;
    }
    if (key == SDLK_LEFT || key == SDLK_a) {
        handle_nav("left");
        return true;
    }
    if (key == SDLK_RIGHT || key == SDLK_d) {
        handle_nav("right");
        return true;
    }
    if (key == SDLK_r) {
        if (page_ == ModalPage::Cloud && cloud_) {
            auto st = cloud_->get_status();
            if (st.connected && !st.busy) cloud_->refresh_backup();
        } else if (page_ == ModalPage::Update && updater_) {
            updater_->start_check(false);
        }
        mark_dirty();
        return true;
    }
    return true;
}

bool PortOptionsModal::handle_mouse_down(int mx, int my, int win_w, int win_h) {
    if (!is_open_) return false;
    UiRect box = compute_box(win_w, win_h);
    if (!box.contains(mx, my)) {
        handle_cancel();
        return true;
    }
    int lx = mx - box.x;
    int ly = my - box.y;
    for (const auto& hb : hitboxes_) {
        if (hb.rect.contains(lx, ly)) {
            if (page_ == ModalPage::Options) selected_ = hb.index;
            else if (page_ == ModalPage::Cloud) cloud_focus_ = hb.index;
            else if (page_ == ModalPage::Update) update_focus_ = hb.index;
            else confirm_focus_ = hb.index;
            activate_action(hb.action);
            return true;
        }
    }
    return true;
}

bool PortOptionsModal::handle_mouse_motion(int mx, int my, int win_w, int win_h) {
    if (!is_open_) return false;
    UiRect box = compute_box(win_w, win_h);
    int lx = mx - box.x;
    int ly = my - box.y;
    for (const auto& hb : hitboxes_) {
        if (hb.rect.contains(lx, ly)) {
            int* ptr = nullptr;
            if (page_ == ModalPage::Options) ptr = &selected_;
            else if (page_ == ModalPage::Cloud) ptr = &cloud_focus_;
            else if (page_ == ModalPage::Update) ptr = &update_focus_;
            else ptr = &confirm_focus_;
            if (ptr && *ptr != hb.index) {
                *ptr = hb.index;
                mark_dirty();
            }
            return true;
        }
    }
    return false;
}

void PortOptionsModal::draw_pill_button(RgbaCanvas& c, const UiRect& r, const std::string& label,
                                        bool selected, bool enabled, float scale, const std::string& action, int idx,
                                        bool primary, bool danger) {
    int rad = std::max(6, static_cast<int>(std::round(8.0f * scale)));
    RgbaColor bg{28, 44, 78, 240};
    RgbaColor border{84, 128, 198, 210};
    RgbaColor fg{232, 240, 255, 255};

    if (!enabled) {
        bg = {20, 28, 46, 175};
        border = {56, 72, 102, 140};
        fg = {120, 134, 160, 210};
    } else if (danger) {
        bg = selected ? RgbaColor{128, 36, 46, 250} : RgbaColor{82, 26, 36, 235};
        border = selected ? RgbaColor{255, 136, 136, 255} : RgbaColor{198, 76, 86, 220};
    } else if (primary) {
        bg = selected ? RgbaColor{40, 102, 186, 255} : RgbaColor{28, 74, 142, 245};
        border = selected ? RgbaColor{245, 204, 82, 255} : RgbaColor{110, 178, 255, 235};
    } else if (selected) {
        bg = {40, 68, 118, 250};
        border = {245, 204, 82, 255};
    }

    int stroke_w = selected ? px_s(2.0f, scale, 2) : px_s(1.0f, scale, 1);
    c.fill_rounded_rect(r.x, r.y, r.w, r.h, rad, bg);
    c.stroke_rounded_rect(r.x, r.y, r.w, r.h, rad, stroke_w, border);

    auto& fs = UiFontSystem::instance();
    int min_fs = px_s(10.0f, scale, 10);
    int fs_px = px_s(15.0f, scale, 12);
    int inset = px_s(14.0f, scale, 10);
    while (fs_px > min_fs && fs.measure_width(label, fs_px, true) > r.w - inset) --fs_px;
    int tw = fs.measure_width(label, fs_px, true);
    int th = fs.line_height(fs_px, true);
    fs.draw_text_shadow(c, label, r.x + (r.w - tw) / 2, r.y + (r.h - th) / 2 - px_s(1.0f, scale), fs_px, true, fg);

    if (enabled) hitboxes_.push_back({r, action, idx});
}

void PortOptionsModal::draw_options_page(RgbaCanvas& c, int w, int h, float scale) {
    std::string l = lang();
    auto& fs = UiFontSystem::instance();
    int pad_x = static_cast<int>(std::round(26.0f * scale));
    int top_y = static_cast<int>(std::round(20.0f * scale));
    int f_title = std::max(18, static_cast<int>(std::round(24.0f * scale)));
    int f_sub = std::max(11, static_cast<int>(std::round(13.5f * scale)));
    int f_row = std::max(14, static_cast<int>(std::round(17.0f * scale)));
    int f_desc = std::max(11, static_cast<int>(std::round(12.5f * scale)));

    fs.draw_text_shadow(c, tr("options.title", l), pad_x, top_y, f_title, true, {248, 236, 184, 255});
    fs.draw_text(c, tr_fmt("options.subtitle", {{"version", kPortVersion}}, l), pad_x,
                 top_y + fs.line_height(f_title, true) + px_s(4.0f, scale), f_sub, false, {168, 198, 238, 240});

    int list_y = top_y + px_s(58.0f, scale);
    int footer_h = px_s(74.0f, scale);
    int avail_h = h - list_y - footer_h;
    int row_gap = px_s(7.0f, scale, 6);
    int row_h = std::max(px_s(36.0f, scale, 34), (avail_h - row_gap * 5) / 6);

    int cur_fps = cb_.get_fps ? cb_.get_fps() : kDefaultFpsTarget;
    std::string cur_aspect = cb_.get_aspect ? cb_.get_aspect() : kDefaultAspectMode;
    bool c_conn = cloud_ ? cloud_->get_status().connected : false;
    bool startup_chk = updater_ ? updater_->get_startup_check() : true;
    UpdaterState ust = updater_ ? updater_->snapshot() : UpdaterState{};

    struct RowDef {
        std::string title;
        std::string desc;
        std::string val;
        bool accent_green = false;
    };
    std::vector<RowDef> rows = {
        {tr("options.row.fps", l), tr("options.row.fps.desc", l),
         tr(cur_fps == 60 ? "options.val.fps.60" : "options.val.fps.30", l), false},
        {tr("options.row.aspect", l), tr("options.row.aspect.desc", l),
         tr(std::string("options.val.aspect.") +
                (cur_aspect == "16:9" ? "16:9" : (cur_aspect == "original" ? "original" : "auto")),
            l),
         false},
        {tr("options.row.language", l), tr("options.row.language.desc", l),
         tr(l == "pt-BR" ? "options.val.lang.pt-BR" : "options.val.lang.en", l), false},
        {tr("options.row.cloud", l), tr("options.row.cloud.desc", l),
         tr(c_conn ? "options.val.cloud.connected" : "options.val.cloud.disconnected", l), c_conn},
        {tr("options.row.startup_updates", l), tr("options.row.startup_updates.desc", l),
         tr(startup_chk ? "options.val.toggle.on" : "options.val.toggle.off", l), startup_chk},
        {tr("options.row.updates", l), tr("options.row.updates.desc", l),
         (ust.phase == UpdatePhase::UpdateAvailable && ust.has_release)
             ? tr_fmt("options.val.updates.available", {{"tag", ust.release.tag_name}}, l)
             : (ust.phase == UpdatePhase::Checking
                    ? tr("options.val.updates.checking", l)
                    : tr_fmt("options.val.updates.check", {{"version", kPortVersion}}, l)),
         ust.phase == UpdatePhase::UpdateAvailable},
    };

    int row_w = w - pad_x * 2;
    int rad = px_s(9.0f, scale, 6);
    int row_stroke = px_s(1.0f, scale, 1);
    int sel_stroke = px_s(2.0f, scale, 2);
    int pill_rad = px_s(6.0f, scale, 4);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        int ry = list_y + i * (row_h + row_gap);
        bool sel = (selected_ == i);
        RgbaColor rbg = sel ? RgbaColor{30, 56, 102, 240} : RgbaColor{18, 30, 58, 220};
        RgbaColor rbr = sel ? RgbaColor{245, 204, 82, 255} : RgbaColor{64, 100, 158, 175};
        c.fill_rounded_rect(pad_x, ry, row_w, row_h, rad, rbg);
        c.stroke_rounded_rect(pad_x, ry, row_w, row_h, rad, sel ? sel_stroke : row_stroke, rbr);

        int tx = pad_x + px_s(14.0f, scale);
        int text_gap = px_s(2.0f, scale);
        int ty = ry + std::max(px_s(4.0f, scale),
                               (row_h - fs.line_height(f_row, true) - fs.line_height(f_desc, false) - text_gap) / 2);
        fs.draw_text_shadow(c, rows[i].title, tx, ty, f_row, true,
                            sel ? RgbaColor{255, 246, 206, 255} : RgbaColor{236, 242, 255, 255});
        fs.draw_text(c, rows[i].desc, tx, ty + fs.line_height(f_row, true) + text_gap, f_desc, false,
                     {158, 186, 226, 225});

        int vw = fs.measure_width(rows[i].val, f_row, true);
        int pill_w = std::max(px_s(140.0f, scale), vw + px_s(28.0f, scale));
        int pill_h = std::min(row_h - px_s(10.0f, scale, 6), px_s(30.0f, scale));
        int px = pad_x + row_w - pill_w - px_s(12.0f, scale);
        int py = ry + (row_h - pill_h) / 2;
        RgbaColor pbg = rows[i].accent_green ? RgbaColor{24, 92, 68, 245} : RgbaColor{26, 46, 86, 240};
        RgbaColor pfg = rows[i].accent_green ? RgbaColor{146, 248, 198, 255} : RgbaColor{245, 214, 114, 255};
        c.fill_rounded_rect(px, py, pill_w, pill_h, pill_rad, pbg);
        c.stroke_rounded_rect(px, py, pill_w, pill_h, pill_rad, row_stroke,
                              rows[i].accent_green ? RgbaColor{88, 220, 158, 210} : RgbaColor{98, 146, 214, 190});
        fs.draw_text(c, rows[i].val, px + (pill_w - vw) / 2,
                     py + (pill_h - fs.line_height(f_row, true)) / 2 - px_s(1.0f, scale), f_row, true, pfg);

        hitboxes_.push_back({{pad_x, ry, row_w, row_h}, "opt_" + std::to_string(i), i});
    }

    int back_w = static_cast<int>(std::round(138.0f * scale));
    int back_h = static_cast<int>(std::round(36.0f * scale));
    int back_y = h - static_cast<int>(std::round(52.0f * scale));
    draw_pill_button(c, {w - pad_x - back_w, back_y, back_w, back_h}, tr("common.back", l),
                     selected_ == 6, true, scale, "back", 6);
    fs.draw_text(c, tr("options.footer.hints", l), pad_x,
                 h - static_cast<int>(std::round(34.0f * scale)), f_desc, false, {146, 174, 212, 220});
}

void PortOptionsModal::draw_cloud_page(RgbaCanvas& c, int w, int h, float scale) {
    std::string l = lang();
    auto& fs = UiFontSystem::instance();
    CloudStatus st = cloud_ ? cloud_->get_status() : CloudStatus{};
    LocalSaveInfo loc = cloud_ ? cloud_->get_local_info() : LocalSaveInfo{};

    int pad_x = static_cast<int>(std::round(26.0f * scale));
    int top_y = static_cast<int>(std::round(20.0f * scale));
    int f_title = std::max(18, static_cast<int>(std::round(23.0f * scale)));
    int f_sub = std::max(11, static_cast<int>(std::round(13.0f * scale)));
    int f_card = std::max(14, static_cast<int>(std::round(16.0f * scale)));
    int f_body = std::max(12, static_cast<int>(std::round(14.0f * scale)));

    fs.draw_text_shadow(c, tr("cloud.title", l), pad_x, top_y, f_title, true, {248, 236, 184, 255});
    fs.draw_text(c, tr("cloud.subtitle", l), pad_x, top_y + fs.line_height(f_title, true) + px_s(4.0f, scale), f_sub,
                 false, {168, 198, 238, 240});

    // Status pill top right
    std::string status_txt = tr(st.authorizing ? "cloud.status.authorizing"
                                               : (st.busy ? "cloud.status.busy"
                                                          : (st.connected ? "cloud.status.connected" : "cloud.status.disconnected")),
                                l);
    RgbaColor sc = st.connected ? RgbaColor{104, 228, 168, 255}
                                : (st.authorizing || st.busy ? RgbaColor{245, 204, 82, 255} : RgbaColor{180, 198, 226, 255});
    int spill_pad = px_s(11.0f, scale);
    int spill_rad = px_s(12.0f, scale, 8);
    int stroke_1 = px_s(1.0f, scale, 1);
    int sw = fs.measure_width(status_txt, f_sub, true) + spill_pad * 2;
    int sh = px_s(28.0f, scale);
    c.fill_rounded_rect(w - pad_x - sw, top_y + px_s(2.0f, scale), sw, sh, spill_rad, {20, 36, 68, 235});
    c.stroke_rounded_rect(w - pad_x - sw, top_y + px_s(2.0f, scale), sw, sh, spill_rad, stroke_1, sc);
    fs.draw_text(c, status_txt, w - pad_x - sw + spill_pad,
                 top_y + px_s(2.0f, scale) + (sh - fs.line_height(f_sub, true)) / 2 - px_s(1.0f, scale),
                 f_sub, true, sc);

    int card_y = top_y + px_s(62.0f, scale);
    int card_h = std::max(px_s(140.0f, scale), h - card_y - px_s(138.0f, scale));
    int gap = px_s(16.0f, scale);
    int card_w = (w - pad_x * 2 - gap) / 2;
    int card_rad = px_s(10.0f, scale, 8);
    int card_pad_x = px_s(14.0f, scale);
    int card_pad_y = px_s(12.0f, scale);

    // Left Card: Remote Backup or OAuth Code
    c.fill_rounded_rect(pad_x, card_y, card_w, card_h, card_rad, {16, 28, 54, 235});
    c.stroke_rounded_rect(pad_x, card_y, card_w, card_h, card_rad, stroke_1, {74, 118, 186, 190});
    int cx = pad_x + card_pad_x;
    int cy = card_y + card_pad_y;
    fs.draw_text_shadow(c, tr("cloud.card.remote.title", l), cx, cy, f_card, true, {138, 198, 255, 255});
    cy += fs.line_height(f_card, true) + px_s(10.0f, scale);

    if (st.authorizing && !st.user_code.empty()) {
        fs.draw_text(c, tr("cloud.auth.step", l), cx, cy, f_sub, false, {194, 214, 242, 240});
        cy += fs.line_height(f_sub, false) + px_s(2.0f, scale);
        fs.draw_text(c, st.verification_url, cx, cy, f_body, true, {245, 204, 82, 255});
        cy += fs.line_height(f_body, true) + px_s(8.0f, scale);
        fs.draw_text(c, tr("cloud.auth.code_label", l), cx, cy, f_sub, false, {194, 214, 242, 240});
        cy += fs.line_height(f_sub, false) + px_s(4.0f, scale);
        int f_code = px_s(24.0f, scale, 18);
        fs.draw_text_shadow(c, st.user_code, cx, cy, f_code, true, {128, 245, 192, 255});
    } else if (!st.connected) {
        for (const auto& line : fs.wrap_text(tr("cloud.card.remote.unknown", l), f_body, false, card_w - card_pad_x * 2)) {
            fs.draw_text(c, line, cx, cy, f_body, false, {182, 198, 224, 225});
            cy += fs.line_height(f_body, false) + px_s(3.0f, scale);
        }
    } else if (!st.backup_exists) {
        fs.draw_text(c, tr("cloud.card.remote.none", l), cx, cy, f_body, false, {182, 198, 224, 225});
    } else {
        fs.draw_text(c, tr("cloud.field.date", l) + ": " + (st.backup_date_local.empty() ? st.backup_date : st.backup_date_local),
                     cx, cy, f_body, false, {240, 246, 255, 255});
        cy += fs.line_height(f_body, false) + px_s(6.0f, scale);
        if (!st.backup_platform.empty()) {
            fs.draw_text(c, tr("cloud.field.platform", l) + ": " + st.backup_platform, cx, cy, f_body, false, {184, 214, 248, 255});
            cy += fs.line_height(f_body, false) + px_s(6.0f, scale);
        }
        for (const auto& line : fs.wrap_text(tr("cloud.field.summary", l) + ": " + st.backup_summary, f_sub, false, card_w - card_pad_x * 2)) {
            fs.draw_text(c, line, cx, cy, f_sub, false, {182, 206, 236, 235});
            cy += fs.line_height(f_sub, false) + px_s(2.0f, scale);
        }
    }

    // Right Card: Local Save
    int rx = pad_x + card_w + gap;
    c.fill_rounded_rect(rx, card_y, card_w, card_h, card_rad, {16, 28, 54, 235});
    c.stroke_rounded_rect(rx, card_y, card_w, card_h, card_rad, stroke_1, {74, 118, 186, 190});
    int rtx = rx + card_pad_x;
    int rty = card_y + card_pad_y;
    fs.draw_text_shadow(c, tr("cloud.card.local.title", l), rtx, rty, f_card, true, {138, 198, 255, 255});
    rty += fs.line_height(f_card, true) + px_s(10.0f, scale);

    if (!loc.exists) {
        fs.draw_text(c, tr("cloud.card.local.none", l), rtx, rty, f_body, false, {182, 198, 224, 225});
    } else {
        fs.draw_text(c, tr("cloud.field.date", l) + ": " + (loc.updated_local.empty() ? tr("common.unknown", l) : loc.updated_local),
                     rtx, rty, f_body, false, {240, 246, 255, 255});
        rty += fs.line_height(f_body, false) + px_s(6.0f, scale);
        std::string sum = (loc.slot_count > 0)
                              ? tr_fmt("cloud.summary.slots",
                                       {{"slots", std::to_string(loc.slot_count)},
                                        {"files", std::to_string(loc.file_count)},
                                        {"kb", fmt_kb(loc.total_bytes)}},
                                       l)
                              : tr_fmt("cloud.summary.files_only",
                                       {{"files", std::to_string(loc.file_count)},
                                        {"kb", fmt_kb(loc.total_bytes)}},
                                       l);
        for (const auto& line : fs.wrap_text(sum, f_body, false, card_w - card_pad_x * 2)) {
            fs.draw_text(c, line, rtx, rty, f_body, false, {182, 224, 206, 255});
            rty += fs.line_height(f_body, false) + px_s(3.0f, scale);
        }
    }

    // Message banner
    std::string msg = st.message_key.empty() ? st.message_detail : tr(st.message_key, l);
    int msg_y = card_y + card_h + static_cast<int>(std::round(10.0f * scale));
    RgbaColor msg_col = st.last_op_ok ? RgbaColor{174, 224, 255, 255} : RgbaColor{255, 134, 134, 255};
    fs.draw_text(c, msg, pad_x, msg_y, f_body, !st.last_op_ok, msg_col);

    // Action bar
    auto btns = build_cloud_buttons(st, loc, l);
    int btn_y = h - static_cast<int>(std::round(82.0f * scale));
    int btn_h = static_cast<int>(std::round(36.0f * scale));
    int n = static_cast<int>(btns.size());
    int bgap = static_cast<int>(std::round(8.0f * scale));
    int bw = (w - pad_x * 2 - bgap * std::max(0, n - 1)) / std::max(1, n);
    cloud_focus_ = std::clamp(cloud_focus_, 0, std::max(0, n - 1));
    for (int i = 0; i < n; ++i) {
        UiRect r{pad_x + i * (bw + bgap), btn_y, bw, btn_h};
        draw_pill_button(c, r, btns[i].label, cloud_focus_ == i, btns[i].enabled, scale,
                         btns[i].action, i, btns[i].primary, btns[i].danger);
    }

    fs.draw_text(c, tr("cloud.footer.hints", l), pad_x,
                 h - static_cast<int>(std::round(32.0f * scale)), f_sub, false, {146, 174, 212, 220});
}

void PortOptionsModal::draw_update_page(RgbaCanvas& c, int w, int h, float scale) {
    std::string l = lang();
    auto& fs = UiFontSystem::instance();
    UpdaterState ust = updater_ ? updater_->snapshot() : UpdaterState{};

    int pad_x = static_cast<int>(std::round(26.0f * scale));
    int top_y = static_cast<int>(std::round(20.0f * scale));
    int f_title = std::max(18, static_cast<int>(std::round(23.0f * scale)));
    int f_sub = std::max(11, static_cast<int>(std::round(13.0f * scale)));
    int f_card = std::max(14, static_cast<int>(std::round(16.0f * scale)));
    int f_body = std::max(12, static_cast<int>(std::round(14.0f * scale)));

    fs.draw_text_shadow(c, tr("update.title", l), pad_x, top_y, f_title, true, {248, 236, 184, 255});
    fs.draw_text(c, tr_fmt("update.subtitle",
                           {{"version", ust.local_version}, {"original", ust.original_version}, {"repo", ust.repo}}, l),
                 pad_x, top_y + fs.line_height(f_title, true) + px_s(4.0f, scale), f_sub, false, {168, 198, 238, 240});

    int card_y = top_y + px_s(62.0f, scale);
    int card_h = std::max(px_s(140.0f, scale), h - card_y - px_s(138.0f, scale));
    int gap = px_s(16.0f, scale);
    int left_w = static_cast<int>(std::round((w - pad_x * 2 - gap) * 0.48f));
    int right_w = w - pad_x * 2 - gap - left_w;
    int card_rad = px_s(10.0f, scale, 8);
    int stroke_1 = px_s(1.0f, scale, 1);
    int card_pad_x = px_s(14.0f, scale);
    int card_pad_y = px_s(12.0f, scale);

    // Left Card: Version status & download progress
    c.fill_rounded_rect(pad_x, card_y, left_w, card_h, card_rad, {16, 28, 54, 235});
    c.stroke_rounded_rect(pad_x, card_y, left_w, card_h, card_rad, stroke_1, {74, 118, 186, 190});
    int lx = pad_x + card_pad_x;
    int ly = card_y + card_pad_y;
    fs.draw_text_shadow(c, tr("update.card.version_title", l), lx, ly, f_card, true, {138, 198, 255, 255});
    ly += fs.line_height(f_card, true) + px_s(10.0f, scale);

    for (const auto& line : fs.wrap_text(tr_fmt("update.field.installed", {{"version", ust.local_version}}, l),
                                         f_body, false, left_w - card_pad_x * 2)) {
        fs.draw_text(c, line, lx, ly, f_body, false, {240, 246, 255, 255});
        ly += fs.line_height(f_body, false) + px_s(4.0f, scale);
    }
    if (ust.has_release) {
        fs.draw_text(c, tr_fmt("update.field.remote", {{"tag", ust.release.tag_name}}, l),
                     lx, ly, f_body, true, {146, 248, 198, 255});
        ly += fs.line_height(f_body, true) + px_s(4.0f, scale);
        fs.draw_text(c, tr_fmt("update.field.asset", {{"name", ust.release.asset_name}}, l),
                     lx, ly, f_sub, false, {186, 212, 246, 240});
        ly += fs.line_height(f_sub, false) + px_s(8.0f, scale);
    } else {
        fs.draw_text(c, tr("update.field.none", l), lx, ly, f_sub, false, {182, 198, 224, 225});
        ly += fs.line_height(f_sub, false) + px_s(8.0f, scale);
    }

    if (ust.phase == UpdatePhase::Downloading) {
        int bar_w = left_w - card_pad_x * 2;
        int bar_h = px_s(14.0f, scale);
        int bar_rad = px_s(6.0f, scale, 4);
        int bar_y = ly + px_s(4.0f, scale);
        c.fill_rounded_rect(lx, bar_y, bar_w, bar_h, bar_rad, {24, 40, 74, 255});
        float frac = (ust.total_bytes > 0)
                         ? std::clamp(float(ust.downloaded_bytes) / float(ust.total_bytes), 0.0f, 1.0f)
                         : 0.35f;
        int fill_w = std::max(px_s(8.0f, scale), static_cast<int>(std::round(bar_w * frac)));
        c.fill_rounded_rect(lx, bar_y, fill_w, bar_h, bar_rad, {92, 226, 168, 255});
    }

    // Right Card: Release notes (clipped cleanly inside card!)
    int rx = pad_x + left_w + gap;
    c.fill_rounded_rect(rx, card_y, right_w, card_h, card_rad, {16, 28, 54, 235});
    c.stroke_rounded_rect(rx, card_y, right_w, card_h, card_rad, stroke_1, {74, 118, 186, 190});
    int rtx = rx + card_pad_x;
    int rty = card_y + card_pad_y;
    fs.draw_text_shadow(c, tr("update.card.notes_title", l), rtx, rty, f_card, true, {138, 198, 255, 255});
    rty += fs.line_height(f_card, true) + px_s(8.0f, scale);

    std::string notes = (ust.has_release && !ust.release.body.empty()) ? ust.release.body : tr("update.notes.empty", l);
    int clip_pad = px_s(8.0f, scale);
    c.set_clip({rx + clip_pad, rty, right_w - clip_pad * 2, card_y + card_h - rty - px_s(10.0f, scale)});
    for (const auto& line : fs.wrap_text(notes, f_sub, false, right_w - card_pad_x * 2)) {
        if (rty + fs.line_height(f_sub, false) > card_y + card_h - clip_pad) break;
        fs.draw_text(c, line, rtx, rty, f_sub, false, {212, 226, 246, 240});
        rty += fs.line_height(f_sub, false) + px_s(2.0f, scale);
    }
    c.clear_clip();

    // Status message line
    std::string status_line;
    RgbaColor scol{174, 224, 255, 255};
    switch (ust.phase) {
    case UpdatePhase::Idle: status_line = tr("update.status.idle", l); break;
    case UpdatePhase::Checking: status_line = tr("update.status.checking", l); break;
    case UpdatePhase::UpToDate: status_line = tr_fmt("update.status.up_to_date", {{"version", ust.local_version}}, l); break;
    case UpdatePhase::UpdateAvailable:
        status_line = tr_fmt("update.status.available",
                             {{"tag", ust.release.tag_name}, {"version", ust.local_version}}, l);
        scol = {128, 245, 192, 255};
        break;
    case UpdatePhase::NoReleaseYet: status_line = tr("update.status.no_release", l); break;
    case UpdatePhase::Error:
        status_line = tr_fmt("update.status.error", {{"detail", ust.error_detail}}, l);
        scol = {255, 134, 134, 255};
        break;
    case UpdatePhase::Downloading: {
        int pct = ust.total_bytes > 0 ? static_cast<int>(100.0 * double(ust.downloaded_bytes) / double(ust.total_bytes)) : 0;
        status_line = ust.total_bytes > 0
                          ? tr_fmt("update.status.downloading",
                                   {{"name", ust.release.asset_name},
                                    {"mb_done", fmt_mb(ust.downloaded_bytes)},
                                    {"mb_total", fmt_mb(ust.total_bytes)},
                                    {"pct", std::to_string(pct)}},
                                   l)
                          : tr_fmt("update.status.downloading_unknown",
                                   {{"name", ust.release.asset_name}, {"mb_done", fmt_mb(ust.downloaded_bytes)}}, l);
        break;
    }
    case UpdatePhase::ReadyToInstall:
        status_line = tr("update.status.ready", l);
        scol = {128, 245, 192, 255};
        break;
    case UpdatePhase::Installing: status_line = tr("update.status.installing", l); break;
    case UpdatePhase::ManualStaged:
        status_line = tr_fmt("update.status.manual_staged", {{"path", ust.downloaded_path.string()}}, l);
        scol = {128, 245, 192, 255};
        break;
    }

    int msg_y = card_y + card_h + static_cast<int>(std::round(10.0f * scale));
    fs.draw_text(c, status_line, pad_x, msg_y, f_body, ust.phase == UpdatePhase::Error, scol);

    auto btns = build_update_buttons(ust, l);
    int btn_y = h - static_cast<int>(std::round(82.0f * scale));
    int btn_h = static_cast<int>(std::round(36.0f * scale));
    int n = static_cast<int>(btns.size());
    int bgap = static_cast<int>(std::round(10.0f * scale));
    int bw = (w - pad_x * 2 - bgap * std::max(0, n - 1)) / std::max(1, n);
    update_focus_ = std::clamp(update_focus_, 0, std::max(0, n - 1));
    for (int i = 0; i < n; ++i) {
        UiRect r{pad_x + i * (bw + bgap), btn_y, bw, btn_h};
        draw_pill_button(c, r, btns[i].label, update_focus_ == i, btns[i].enabled, scale,
                         btns[i].action, i, btns[i].primary, false);
    }

    fs.draw_text(c, tr("update.footer.hints", l), pad_x,
                 h - static_cast<int>(std::round(32.0f * scale)), f_sub, false, {146, 174, 212, 220});
}

void PortOptionsModal::draw_confirm_page(RgbaCanvas& c, int w, int h, float scale) {
    std::string l = lang();
    auto& fs = UiFontSystem::instance();
    CloudStatus st = cloud_ ? cloud_->get_status() : CloudStatus{};
    std::string bdate = !st.backup_date_local.empty() ? st.backup_date_local : tr("common.unknown", l);

    std::string title, body, act_lbl;
    bool danger = false;
    if (page_ == ModalPage::ConfirmUpload) {
        title = tr("confirm.upload.title", l);
        body = st.backup_exists ? tr_fmt("confirm.upload.body", {{"date", bdate}}, l)
                                : tr("confirm.upload.body_new", l);
        act_lbl = tr("confirm.action.upload", l);
    } else if (page_ == ModalPage::ConfirmRestore) {
        title = tr("confirm.restore.title", l);
        body = tr_fmt("confirm.restore.body", {{"date", bdate}}, l);
        act_lbl = tr("confirm.action.restore", l);
    } else {
        title = tr("confirm.disconnect.title", l);
        body = tr("confirm.disconnect.body", l);
        act_lbl = tr("confirm.action.disconnect", l);
        danger = true;
    }

    int pad_x = static_cast<int>(std::round(42.0f * scale));
    int f_title = std::max(20, static_cast<int>(std::round(24.0f * scale)));
    int f_body = std::max(14, static_cast<int>(std::round(16.0f * scale)));
    int y = h / 4;
    fs.draw_text_shadow(c, title, pad_x, y, f_title, true, {248, 236, 184, 255});
    y += fs.line_height(f_title, true) + px_s(18.0f, scale);
    for (const auto& line : fs.wrap_text(body, f_body, false, w - pad_x * 2)) {
        fs.draw_text(c, line, pad_x, y, f_body, false, {222, 234, 252, 255});
        y += fs.line_height(f_body, false) + px_s(5.0f, scale);
    }

    int bw = px_s(180.0f, scale);
    int bh = px_s(42.0f, scale);
    int by = h - px_s(82.0f, scale);
    int b_half_gap = px_s(14.0f, scale);
    draw_pill_button(c, {w / 2 - bw - b_half_gap, by, bw, bh}, act_lbl, confirm_focus_ == 0, true, scale,
                     "confirm_yes", 0, !danger, danger);
    draw_pill_button(c, {w / 2 + b_half_gap, by, bw, bh}, tr("common.cancel", l), confirm_focus_ == 1, true, scale,
                     "confirm_no", 1, false, false);
}

void PortOptionsModal::rebuild_canvas(GLBridge& gl, int box_w, int box_h) {
    hitboxes_.clear();
    float scale = std::clamp(std::min(static_cast<float>(box_w) / 860.0f, static_cast<float>(box_h) / 550.0f),
                             0.72f, 2.8f);
    RgbaCanvas c(box_w, box_h, {0, 0, 0, 0});
    int rad = px_s(14.0f, scale, 10);
    int inner_off = px_s(2.0f, scale, 2);
    int inner_stroke = px_s(1.0f, scale, 1);
    int outer_stroke = px_s(2.0f, scale, 2);
    c.fill_rounded_gradient_v(0, 0, box_w, box_h, rad, {18, 32, 64, 246}, {10, 18, 38, 248});
    c.stroke_rounded_rect(inner_off, inner_off, box_w - inner_off * 2, box_h - inner_off * 2,
                          std::max(8, rad - inner_off), inner_stroke, {126, 178, 245, 115});
    c.stroke_rounded_rect(0, 0, box_w, box_h, rad, outer_stroke, {245, 204, 82, 235});

    if (page_ == ModalPage::Options) draw_options_page(c, box_w, box_h, scale);
    else if (page_ == ModalPage::Cloud) draw_cloud_page(c, box_w, box_h, scale);
    else if (page_ == ModalPage::Update) draw_update_page(c, box_w, box_h, scale);
    else draw_confirm_page(c, box_w, box_h, scale);

    if (tex_id_ == 0 || tex_w_ != box_w || tex_h_ != box_h) {
        if (tex_id_ != 0) gl.destroy_texture(tex_id_);
        tex_id_ = gl.create_rgba_texture(box_w, box_h, c.data(), true);
    } else {
        gl.update_rgba_texture(tex_id_, box_w, box_h, c.data());
    }
    tex_w_ = box_w;
    tex_h_ = box_h;
}

void PortOptionsModal::draw_gl(GLBridge& gl, int win_w, int win_h) {
    if (!is_open_ || win_w <= 0 || win_h <= 0) return;

    UiRect box = compute_box(win_w, win_h);
    if (win_w != last_win_w_ || win_h != last_win_h_ || tex_w_ != box.w || tex_h_ != box.h) {
        last_win_w_ = win_w;
        last_win_h_ = win_h;
        dirty_ = true;
    }

    if (dirty_ || tex_id_ == 0) {
        dirty_ = false;
        rebuild_canvas(gl, box.w, box.h);
    }

    gl.overlay_begin_window(win_w, win_h);
    gl.overlay_fill_rect(0.0f, 0.0f, float(win_w), float(win_h),
                         4.0f / 255.0f, 8.0f / 255.0f, 18.0f / 255.0f, 0.72f);
    if (tex_id_ != 0) {
        gl.overlay_textured_rect(tex_id_, float(box.x), float(box.y), float(box.w), float(box.h));
    }
    gl.overlay_end();
}

}  // namespace hl3
