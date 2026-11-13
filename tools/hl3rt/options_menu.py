"""Responsive Port Options & Google Drive Cloud Save modal overlay for Heroes Lore 3.

Adapts dynamically to desktop, ultrawide, and mobile portrait/landscape viewports
with unified button typography, Soltia navy-and-gold RPG styling, keyboard/gamepad
navigation, and full mouse/touch hit-testing.
"""

from __future__ import annotations

import webbrowser
from dataclasses import dataclass
from enum import Enum
from typing import Any, Callable

import pygame
from OpenGL.GL import (
    GL_ALL_ATTRIB_BITS,
    GL_BLEND,
    GL_CLAMP_TO_EDGE,
    GL_LINEAR,
    GL_MODELVIEW,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_PROJECTION,
    GL_QUADS,
    GL_RGBA,
    GL_SCISSOR_TEST,
    GL_SRC_ALPHA,
    GL_TEXTURE_2D,
    GL_TEXTURE_MAG_FILTER,
    GL_TEXTURE_MIN_FILTER,
    GL_TEXTURE_WRAP_S,
    GL_TEXTURE_WRAP_T,
    GL_UNSIGNED_BYTE,
    glBegin,
    glBindTexture,
    glBlendFunc,
    glColor4f,
    glDisable,
    glEnable,
    glEnd,
    glGenTextures,
    glLoadIdentity,
    glMatrixMode,
    glOrtho,
    glPopAttrib,
    glPopMatrix,
    glPushAttrib,
    glPushMatrix,
    glTexCoord2f,
    glTexImage2D,
    glTexParameteri,
    glVertex2f,
    glViewport,
)

from .cloud_save import CloudSaveManager, CloudStatus, LocalSaveInfo
from .display import UI_ASPECT_CYCLE, normalize_language
from .i18n import get_active_language, t


class ModalPage(str, Enum):
    OPTIONS = "options"
    CLOUD = "cloud"
    CONFIRM_UPLOAD = "confirm_upload"
    CONFIRM_RESTORE = "confirm_restore"
    CONFIRM_DISCONNECT = "confirm_disconnect"


@dataclass
class ModalLayout:
    win_w: int
    win_h: int
    scale: float
    is_portrait: bool
    is_compact: bool
    panel: pygame.Rect
    close_rect: pygame.Rect
    header_h: int
    footer_h: int
    pad: int
    gap: int
    btn_h: int
    # Page-specific computed rects (in window coordinates)
    option_rows: list[pygame.Rect]
    option_left_arrows: list[pygame.Rect]
    option_right_arrows: list[pygame.Rect]
    back_rect: pygame.Rect
    remote_card: pygame.Rect
    local_card: pygame.Rect
    auth_box: pygame.Rect
    open_browser_rect: pygame.Rect
    refresh_rect: pygame.Rect
    status_bar: pygame.Rect
    action_buttons: list[pygame.Rect]
    confirm_buttons: list[pygame.Rect]
    font_title_sz: int
    font_sub_sz: int
    font_label_sz: int
    font_small_sz: int
    font_code_sz: int


def compute_modal_layout(
    win_w: int,
    win_h: int,
    page: ModalPage,
    authorizing: bool = False,
) -> ModalLayout:
    w = max(320, int(win_w))
    h = max(240, int(win_h))
    aspect = float(w) / float(h)
    is_portrait = (h > w) or (aspect < 1.08)

    if is_portrait:
        raw_scale = min(float(w) / 440.0, float(h) / 720.0)
        scale = max(0.75, min(3.4, raw_scale))
        is_compact = w < 420 or h < 560
    else:
        raw_scale = min(float(w) / 860.0, float(h) / 560.0)
        scale = max(0.62, min(2.6, raw_scale))
        is_compact = w < 640 or h < 420

    pad = max(10, int(round(18 * scale)))
    gap = max(6, int(round(10 * scale)))

    font_title_sz = max(14, int(round(20 * scale)))
    font_sub_sz = max(10, int(round(12 * scale)))
    font_label_sz = max(12, int(round(15 * scale)))
    font_small_sz = max(10, int(round(12 * scale)))
    font_code_sz = max(18, int(round(26 * scale)))

    if page in (ModalPage.CONFIRM_UPLOAD, ModalPage.CONFIRM_RESTORE, ModalPage.CONFIRM_DISCONNECT):
        if is_portrait:
            mw = min(int(w * 0.92), w - pad)
        else:
            mw = min(int(w * 0.68), max(420, int(580 * scale)), w - pad)
        btn_h = max(32, int(round(40 * scale)))
        header_h = max(38, int(round(52 * scale)))
        body_h = max(72, int(round(108 * scale)))
        mh = min(int(h * 0.86), header_h + pad + body_h + gap + btn_h + pad)
        px = (w - mw) // 2
        py = (h - mh) // 2
        panel = pygame.Rect(px, py, mw, mh)
        close_sz = max(22, int(round(28 * scale)))
        close_rect = pygame.Rect(panel.right - pad - close_sz, panel.y + (header_h - close_sz) // 2, close_sz, close_sz)

        bw = (mw - pad * 2 - gap) // 2
        by = panel.bottom - pad - btn_h
        c_btns = [
            pygame.Rect(panel.x + pad, by, bw, btn_h),
            pygame.Rect(panel.x + pad + bw + gap, by, bw, btn_h),
        ]
        return ModalLayout(
            win_w=w, win_h=h, scale=scale, is_portrait=is_portrait, is_compact=is_compact,
            panel=panel, close_rect=close_rect, header_h=header_h, footer_h=0,
            pad=pad, gap=gap, btn_h=btn_h,
            option_rows=[], option_left_arrows=[], option_right_arrows=[],
            back_rect=pygame.Rect(0, 0, 0, 0),
            remote_card=pygame.Rect(0, 0, 0, 0), local_card=pygame.Rect(0, 0, 0, 0),
            auth_box=pygame.Rect(0, 0, 0, 0), open_browser_rect=pygame.Rect(0, 0, 0, 0),
            refresh_rect=pygame.Rect(0, 0, 0, 0), status_bar=pygame.Rect(0, 0, 0, 0),
            action_buttons=[], confirm_buttons=c_btns,
            font_title_sz=font_title_sz, font_sub_sz=font_sub_sz,
            font_label_sz=font_label_sz, font_small_sz=font_small_sz,
            font_code_sz=font_code_sz,
        )

    # Panel dimensions for OPTIONS and CLOUD
    if is_portrait:
        mw = min(int(w * 0.94), w - max(10, int(12 * scale)))
        max_mh = int(h * 0.90)
    else:
        mw = min(int(w * 0.86), max(460, int(840 * scale)), w - max(12, int(16 * scale)))
        max_mh = int(h * 0.92)

    header_h = max(36, int(round((54 if not is_compact else 42) * scale)))
    footer_h = max(36, int(round((52 if not is_compact else 40) * scale)))
    close_sz = max(22, int(round(28 * scale)))

    if page == ModalPage.OPTIONS:
        n_rows = 4
        row_h = max(34, int(round((56 if not is_compact else 42) * scale)))
        row_gap = max(6, int(round((10 if not is_compact else 7) * scale)))
        desired_mh = header_h + pad + n_rows * row_h + (n_rows - 1) * row_gap + gap + footer_h + pad // 2
        mh = min(max_mh, max(desired_mh, int(h * 0.50)))
        avail_body = mh - header_h - footer_h - pad - gap
        row_h = max(28, (avail_body - (n_rows - 1) * row_gap) // n_rows)

        panel = pygame.Rect((w - mw) // 2, (h - mh) // 2, mw, mh)
        close_rect = pygame.Rect(panel.right - pad - close_sz, panel.y + (header_h - close_sz) // 2, close_sz, close_sz)

        rows: list[pygame.Rect] = []
        l_arrows: list[pygame.Rect] = []
        r_arrows: list[pygame.Rect] = []
        y_cur = panel.y + header_h + pad // 2
        inner_w = mw - pad * 2
        pill_w = max(120, min(int(inner_w * (0.44 if not is_portrait else 0.48)), int(280 * scale)))
        arr_w = max(24, int(round(32 * scale)))

        for _ in range(n_rows):
            rc = pygame.Rect(panel.x + pad, y_cur, inner_w, row_h)
            rows.append(rc)
            px1 = rc.right - int(10 * scale)
            r_arr = pygame.Rect(px1 - arr_w, rc.y + 3, arr_w, row_h - 6)
            l_arr = pygame.Rect(px1 - pill_w, rc.y + 3, arr_w, row_h - 6)
            l_arrows.append(l_arr)
            r_arrows.append(r_arr)
            y_cur += row_h + row_gap

        back_w = max(110, int(round(160 * scale)))
        back_h = max(28, int(round(36 * scale)))
        back_rect = pygame.Rect(panel.x + pad, panel.bottom - footer_h + (footer_h - back_h) // 2 - 2, back_w, back_h)

        return ModalLayout(
            win_w=w, win_h=h, scale=scale, is_portrait=is_portrait, is_compact=is_compact,
            panel=panel, close_rect=close_rect, header_h=header_h, footer_h=footer_h,
            pad=pad, gap=gap, btn_h=back_h,
            option_rows=rows, option_left_arrows=l_arrows, option_right_arrows=r_arrows,
            back_rect=back_rect,
            remote_card=pygame.Rect(0, 0, 0, 0), local_card=pygame.Rect(0, 0, 0, 0),
            auth_box=pygame.Rect(0, 0, 0, 0), open_browser_rect=pygame.Rect(0, 0, 0, 0),
            refresh_rect=pygame.Rect(0, 0, 0, 0), status_bar=pygame.Rect(0, 0, 0, 0),
            action_buttons=[], confirm_buttons=[],
            font_title_sz=font_title_sz, font_sub_sz=font_sub_sz,
            font_label_sz=font_label_sz, font_small_sz=font_small_sz,
            font_code_sz=font_code_sz,
        )

    # ModalPage.CLOUD
    btn_h = max(32, int(round((42 if not is_portrait else 48) * scale)))
    status_bar_h = max(28, int(round(34 * scale)))
    hint_h = max(18, int(round(22 * scale)))

    # 2x2 grid in portrait or narrow windows; 1 horizontal row on normal/wide landscape
    two_by_two = is_portrait or mw < int(560 * max(0.85, scale))
    btns_area_h = (btn_h * 2 + gap) if two_by_two else btn_h

    cards_min_h = max(140 if two_by_two else 96, int(round((220 if two_by_two else 150) * scale)))
    if authorizing:
        cards_min_h = max(cards_min_h, int(round((210 if is_portrait else 165) * scale)))
    desired_mh = header_h + pad // 2 + cards_min_h + gap + status_bar_h + gap + btns_area_h + gap // 2 + hint_h + pad // 2
    mh = min(max_mh, max(desired_mh, int(h * 0.62)))
    panel = pygame.Rect((w - mw) // 2, (h - mh) // 2, mw, mh)

    close_rect = pygame.Rect(panel.right - pad - close_sz, panel.y + (header_h - close_sz) // 2, close_sz, close_sz)
    ref_w = max(70, int(round(98 * scale)))
    ref_h = close_sz
    refresh_rect = pygame.Rect(close_rect.x - gap - ref_w, close_rect.y, ref_w, ref_h)

    inner_x = panel.x + pad
    inner_w = mw - pad * 2
    cards_y = panel.y + header_h + pad // 2
    bottom_fixed = pad // 2 + hint_h + gap // 2 + btns_area_h + gap + status_bar_h + gap
    cards_h = max(84, mh - (cards_y - panel.y) - bottom_fixed)

    if authorizing:
        auth_box = pygame.Rect(inner_x, cards_y, inner_w, cards_h)
        ob_w = max(120, min(inner_w // 2, int(round(190 * scale))))
        ob_h = max(28, int(round(34 * scale)))
        open_browser_rect = pygame.Rect(auth_box.centerx - ob_w // 2, auth_box.bottom - ob_h - max(6, pad // 2), ob_w, ob_h)
        remote_card = pygame.Rect(0, 0, 0, 0)
        local_card = pygame.Rect(0, 0, 0, 0)
    elif is_portrait or inner_w < int(440 * max(0.8, scale)):
        half_h = max(40, (cards_h - gap) // 2)
        remote_card = pygame.Rect(inner_x, cards_y, inner_w, half_h)
        local_card = pygame.Rect(inner_x, cards_y + half_h + gap, inner_w, cards_h - half_h - gap)
        auth_box = pygame.Rect(0, 0, 0, 0)
        open_browser_rect = pygame.Rect(0, 0, 0, 0)
    else:
        half_w = (inner_w - gap) // 2
        remote_card = pygame.Rect(inner_x, cards_y, half_w, cards_h)
        local_card = pygame.Rect(inner_x + half_w + gap, cards_y, inner_w - half_w - gap, cards_h)
        auth_box = pygame.Rect(0, 0, 0, 0)
        open_browser_rect = pygame.Rect(0, 0, 0, 0)

    status_bar = pygame.Rect(inner_x, cards_y + cards_h + gap, inner_w, status_bar_h)
    btns_y = status_bar.bottom + gap

    action_buttons: list[pygame.Rect] = []
    if two_by_two:
        bw = (inner_w - gap) // 2
        for i in range(4):
            r = i // 2
            c = i % 2
            action_buttons.append(pygame.Rect(inner_x + c * (bw + gap), btns_y + r * (btn_h + gap), bw, btn_h))
    else:
        bw = (inner_w - gap * 3) // 4
        for i in range(4):
            action_buttons.append(pygame.Rect(inner_x + i * (bw + gap), btns_y, bw, btn_h))

    return ModalLayout(
        win_w=w, win_h=h, scale=scale, is_portrait=is_portrait, is_compact=is_compact,
        panel=panel, close_rect=close_rect, header_h=header_h, footer_h=0,
        pad=pad, gap=gap, btn_h=btn_h,
        option_rows=[], option_left_arrows=[], option_right_arrows=[],
        back_rect=pygame.Rect(0, 0, 0, 0),
        remote_card=remote_card, local_card=local_card,
        auth_box=auth_box, open_browser_rect=open_browser_rect,
        refresh_rect=refresh_rect, status_bar=status_bar,
        action_buttons=action_buttons, confirm_buttons=[],
        font_title_sz=font_title_sz, font_sub_sz=font_sub_sz,
        font_label_sz=font_label_sz, font_small_sz=font_small_sz,
        font_code_sz=font_code_sz,
    )


class PortOptionsModal:
    """Self-contained OpenGL overlay modal for Port Options and Cloud Save."""

    def __init__(
        self,
        cloud: CloudSaveManager,
        get_fps: Callable[[], int],
        set_fps: Callable[[int], None],
        get_aspect: Callable[[], str],
        set_aspect: Callable[[str], None],
        get_lang: Callable[[], str],
        set_lang: Callable[[str], None],
        on_restore_applied: Callable[[], None] | None = None,
    ):
        self.cloud = cloud
        self.get_fps = get_fps
        self.set_fps = set_fps
        self.get_aspect = get_aspect
        self.set_aspect = set_aspect
        self.get_lang = get_lang
        self.set_lang = set_lang
        self.on_restore_applied = on_restore_applied

        self.is_open: bool = False
        self.page: ModalPage = ModalPage.OPTIONS
        self.focus_idx: int = 0
        self.hover_idx: int = -1
        self.hover_close: bool = False
        self.hover_refresh: bool = False
        self.hover_browser: bool = False

        self._gl_tex: int = 0
        self._tex_key: tuple | None = None
        self._tex_size: tuple[int, int] = (0, 0)
        self._last_layout: ModalLayout | None = None
        self._font_cache: dict[tuple[int, bool], pygame.font.Font] = {}
        self._local_info_cache: LocalSaveInfo | None = None
        self._local_info_tick: int = 0

    def open(self, start_page: ModalPage = ModalPage.OPTIONS):
        self.is_open = True
        self.page = start_page
        self.focus_idx = 0
        self.hover_idx = -1
        self._tex_key = None
        self._local_info_cache = self.cloud.get_local_info()
        if start_page == ModalPage.CLOUD:
            st = self.cloud.get_status()
            if st.connected and not st.busy:
                self.cloud.refresh_backup()

    def close(self):
        if self.cloud.get_status().authorizing:
            self.cloud.cancel_login()
        self.is_open = False
        self.page = ModalPage.OPTIONS
        self._tex_key = None

    def invalidate_cache(self):
        self._tex_key = None

    def _font(self, size: int, bold: bool = False) -> pygame.font.Font:
        key = (max(8, int(size)), bool(bold))
        cached = self._font_cache.get(key)
        if cached is not None:
            return cached
        if not pygame.font.get_init():
            pygame.font.init()
        try:
            f = pygame.font.SysFont("segoeui,dejavusans,arial,sans", key[0], bold=key[1])
        except Exception:
            f = pygame.font.Font(None, key[0] + 3)
        self._font_cache[key] = f
        return f

    def _fit_uniform_button_font(self, labels: list[str], rects: list[pygame.Rect], base_sz: int, min_sz: int = 9) -> pygame.font.Font:
        if not labels or not rects:
            return self._font(base_sz, bold=True)
        pad_px = 16
        sz = base_sz
        while sz > min_sz:
            f = self._font(sz, bold=True)
            fits = True
            for lbl, rc in zip(labels, rects):
                avail = max(24, rc.width - pad_px)
                if f.size(lbl)[0] > avail:
                    fits = False
                    break
            if fits:
                return f
            sz -= 1
        return self._font(min_sz, bold=True)

    # ------------------------------------------------------------------ Actions
    def _set_page(self, page: ModalPage, default_focus: int = 0):
        self.page = page
        self.focus_idx = default_focus
        self.hover_idx = -1
        self._tex_key = None
        if page == ModalPage.CLOUD:
            self._local_info_cache = self.cloud.get_local_info()
            st = self.cloud.get_status()
            if st.connected and not st.busy:
                self.cloud.refresh_backup()

    def _go_back(self):
        if self.page in (ModalPage.CONFIRM_UPLOAD, ModalPage.CONFIRM_RESTORE, ModalPage.CONFIRM_DISCONNECT):
            self._set_page(ModalPage.CLOUD, default_focus=0)
        elif self.page == ModalPage.CLOUD:
            if self.cloud.get_status().authorizing:
                self.cloud.cancel_login()
            self._set_page(ModalPage.OPTIONS, default_focus=3)
        else:
            self.close()

    def _adjust_option_row(self, row_idx: int, direction: int):
        if row_idx == 0:
            cur_fps = self.get_fps()
            self.set_fps(60 if cur_fps == 30 else 30)
        elif row_idx == 1:
            cur_asp = self.get_aspect()
            idx = UI_ASPECT_CYCLE.index(cur_asp) if cur_asp in UI_ASPECT_CYCLE else 0
            nxt = UI_ASPECT_CYCLE[(idx + direction) % len(UI_ASPECT_CYCLE)]
            self.set_aspect(nxt)
        elif row_idx == 2:
            cur_lang = normalize_language(self.get_lang())
            self.set_lang("pt-BR" if cur_lang == "en" else "en")
        elif row_idx == 3:
            self._set_page(ModalPage.CLOUD, default_focus=0)
        self._tex_key = None

    def _trigger_cloud_button(self, btn_idx: int):
        st = self.cloud.get_status()
        if btn_idx == 0:
            if st.busy and not st.authorizing:
                return
            if not st.connected:
                if not st.authorizing:
                    self.cloud.start_login()
            else:
                local_info = self.cloud.get_local_info()
                if not local_info.exists:
                    # Show instant localized message by launching upload guard
                    self.cloud.upload_backup()
                elif st.backup_exists:
                    self._set_page(ModalPage.CONFIRM_UPLOAD, default_focus=1)
                else:
                    self.cloud.upload_backup()
        elif btn_idx == 1:
            if st.busy or not st.connected or not st.backup_exists:
                return
            self._set_page(ModalPage.CONFIRM_RESTORE, default_focus=1)
        elif btn_idx == 2:
            if st.authorizing:
                self.cloud.cancel_login()
            elif st.connected and not st.busy:
                self._set_page(ModalPage.CONFIRM_DISCONNECT, default_focus=1)
        elif btn_idx == 3:
            self._go_back()
        self._tex_key = None

    def _trigger_confirm_button(self, btn_idx: int):
        if btn_idx == 1:  # Cancel (safe default)
            self._set_page(ModalPage.CLOUD, default_focus=0)
            return
        if self.page == ModalPage.CONFIRM_UPLOAD:
            self.cloud.upload_backup()
            self._set_page(ModalPage.CLOUD, default_focus=0)
        elif self.page == ModalPage.CONFIRM_RESTORE:
            self.cloud.restore_backup()
            self._set_page(ModalPage.CLOUD, default_focus=3)
        elif self.page == ModalPage.CONFIRM_DISCONNECT:
            self.cloud.disconnect()
            self._set_page(ModalPage.CLOUD, default_focus=0)

    def poll(self):
        if not self.is_open:
            return
        self._local_info_tick += 1
        if self._local_info_tick % 30 == 0:
            self._local_info_cache = self.cloud.get_local_info()

        st = self.cloud.get_status()
        if st.restore_ready and not st.busy:
            res = self.cloud.consume_pending_restore()
            self._local_info_cache = self.cloud.get_local_info()
            self._tex_key = None
            if res is not None and self.on_restore_applied is not None:
                self.close()
                self.on_restore_applied()

    # ------------------------------------------------------------------ Input
    def _nav_count(self) -> int:
        if self.page == ModalPage.OPTIONS:
            return 5  # 4 setting rows + Back
        if self.page == ModalPage.CLOUD:
            return 4
        return 2

    def handle_nav(self, dir_name: str):
        if not self.is_open:
            return
        n = self._nav_count()
        if self.page == ModalPage.OPTIONS:
            if dir_name == "up":
                self.focus_idx = (self.focus_idx - 1) % n
            elif dir_name == "down":
                self.focus_idx = (self.focus_idx + 1) % n
            elif dir_name in ("left", "right") and 0 <= self.focus_idx <= 3:
                self._adjust_option_row(self.focus_idx, -1 if dir_name == "left" else 1)
        elif self.page == ModalPage.CLOUD:
            layout = self._last_layout
            two_by_two = bool(layout and (layout.is_portrait or layout.panel.width < int(560 * max(0.85, layout.scale))))
            if two_by_two:
                if dir_name in ("left", "right"):
                    self.focus_idx ^= 1
                elif dir_name in ("up", "down"):
                    self.focus_idx ^= 2
            else:
                if dir_name in ("left", "up"):
                    self.focus_idx = (self.focus_idx - 1) % n
                elif dir_name in ("right", "down"):
                    self.focus_idx = (self.focus_idx + 1) % n
        else:
            if dir_name in ("left", "right", "up", "down"):
                self.focus_idx = 1 - self.focus_idx
        self._tex_key = None

    def handle_confirm(self):
        if not self.is_open:
            return
        if self.page == ModalPage.OPTIONS:
            if self.focus_idx == 4:
                self._go_back()
            else:
                self._adjust_option_row(self.focus_idx, 1)
        elif self.page == ModalPage.CLOUD:
            self._trigger_cloud_button(self.focus_idx)
        else:
            self._trigger_confirm_button(self.focus_idx)

    def handle_cancel(self):
        if self.is_open:
            self._go_back()

    def handle_key(self, key: int) -> bool:
        if not self.is_open:
            return False
        if key in (pygame.K_UP, pygame.K_w):
            self.handle_nav("up")
        elif key in (pygame.K_DOWN, pygame.K_s):
            self.handle_nav("down")
        elif key in (pygame.K_LEFT, pygame.K_a):
            self.handle_nav("left")
        elif key in (pygame.K_RIGHT, pygame.K_d):
            self.handle_nav("right")
        elif key in (pygame.K_RETURN, pygame.K_SPACE, pygame.K_e, pygame.K_z):
            self.handle_confirm()
        elif key in (pygame.K_ESCAPE, pygame.K_BACKSPACE):
            self.handle_cancel()
        elif key == pygame.K_r and self.page == ModalPage.CLOUD:
            st = self.cloud.get_status()
            if st.connected and not st.busy:
                self.cloud.refresh_backup()
                self._tex_key = None
        return True

    def handle_mouse_motion(self, wx: int, wy: int, win_w: int, win_h: int):
        if not self.is_open:
            return
        lay = self._last_layout or compute_modal_layout(win_w, win_h, self.page, self.cloud.get_status().authorizing)
        pt = (int(wx), int(wy))
        old = (self.hover_idx, self.hover_close, self.hover_refresh, self.hover_browser)
        self.hover_close = lay.close_rect.collidepoint(pt)
        self.hover_refresh = (self.page == ModalPage.CLOUD and lay.refresh_rect.collidepoint(pt))
        self.hover_browser = (self.page == ModalPage.CLOUD and lay.open_browser_rect.width > 0 and lay.open_browser_rect.collidepoint(pt))
        self.hover_idx = -1
        if self.page == ModalPage.OPTIONS:
            for i, rc in enumerate(lay.option_rows):
                if rc.collidepoint(pt):
                    self.hover_idx = i
                    break
            if self.hover_idx == -1 and lay.back_rect.collidepoint(pt):
                self.hover_idx = 4
        elif self.page == ModalPage.CLOUD:
            for i, rc in enumerate(lay.action_buttons):
                if rc.collidepoint(pt):
                    self.hover_idx = i
                    break
        else:
            for i, rc in enumerate(lay.confirm_buttons):
                if rc.collidepoint(pt):
                    self.hover_idx = i
                    break
        if self.hover_idx >= 0:
            self.focus_idx = self.hover_idx
        if (self.hover_idx, self.hover_close, self.hover_refresh, self.hover_browser) != old:
            self._tex_key = None

    def handle_mouse_down(self, wx: int, wy: int, win_w: int, win_h: int) -> bool:
        if not self.is_open:
            return False
        st = self.cloud.get_status()
        lay = self._last_layout or compute_modal_layout(win_w, win_h, self.page, st.authorizing)
        pt = (int(wx), int(wy))

        if lay.close_rect.collidepoint(pt):
            self._go_back()
            return True
        if not lay.panel.collidepoint(pt):
            self._go_back()
            return True

        if self.page == ModalPage.OPTIONS:
            for i, rc in enumerate(lay.option_rows):
                if rc.collidepoint(pt):
                    self.focus_idx = i
                    if i < 3 and lay.option_left_arrows[i].collidepoint(pt):
                        self._adjust_option_row(i, -1)
                    else:
                        self._adjust_option_row(i, 1)
                    return True
            if lay.back_rect.collidepoint(pt):
                self._go_back()
                return True
            return True

        if self.page == ModalPage.CLOUD:
            if lay.refresh_rect.collidepoint(pt) and st.connected and not st.busy:
                self.cloud.refresh_backup()
                self._tex_key = None
                return True
            if lay.open_browser_rect.width > 0 and lay.open_browser_rect.collidepoint(pt) and st.verification_url:
                try:
                    webbrowser.open(st.verification_url)
                except Exception:
                    pass
                return True
            for i, rc in enumerate(lay.action_buttons):
                if rc.collidepoint(pt):
                    self.focus_idx = i
                    self._trigger_cloud_button(i)
                    return True
            return True

        for i, rc in enumerate(lay.confirm_buttons):
            if rc.collidepoint(pt):
                self.focus_idx = i
                self._trigger_confirm_button(i)
                return True
        return True

    # ------------------------------------------------------------------ Render
    @staticmethod
    def _wrap_text(font: pygame.font.Font, text: str, max_w: int) -> list[str]:
        lines: list[str] = []
        for para in str(text).split("\n"):
            words = para.split(" ")
            cur = ""
            for w in words:
                cand = f"{cur} {w}".strip() if cur else w
                if font.size(cand)[0] <= max_w or not cur:
                    cur = cand
                else:
                    lines.append(cur)
                    cur = w
            if cur:
                lines.append(cur)
        return lines or [""]

    @staticmethod
    def _draw_gradient_panel(surf: pygame.Surface, rect: pygame.Rect, top_c: tuple, bot_c: tuple, radius: int, alpha: int = 246):
        tmp = pygame.Surface((max(1, rect.w), max(1, rect.h)), pygame.SRCALPHA)
        for row in range(tmp.get_height()):
            frac = row / max(1, tmp.get_height() - 1)
            r = int(top_c[0] + (bot_c[0] - top_c[0]) * frac)
            g = int(top_c[1] + (bot_c[1] - top_c[1]) * frac)
            b = int(top_c[2] + (bot_c[2] - top_c[2]) * frac)
            pygame.draw.line(tmp, (r, g, b, alpha), (0, row), (tmp.get_width() - 1, row))
        mask = pygame.Surface(tmp.get_size(), pygame.SRCALPHA)
        pygame.draw.rect(mask, (255, 255, 255, 255), (0, 0, tmp.get_width(), tmp.get_height()), border_radius=radius)
        tmp.blit(mask, (0, 0), special_flags=pygame.BLEND_RGBA_MIN)
        surf.blit(tmp, rect.topleft)

    def _draw_button(
        self,
        surf: pygame.Surface,
        rect: pygame.Rect,
        label: str,
        font: pygame.font.Font,
        focused: bool,
        enabled: bool = True,
        tone: str = "blue",
        radius: int = 8,
    ):
        if not enabled:
            top_c, bot_c, border_c, text_c = (28, 34, 52), (20, 24, 38), (70, 82, 108), (125, 136, 158)
        elif tone == "gold":
            top_c = (158, 120, 36) if focused else (118, 88, 26)
            bot_c = (104, 76, 18) if focused else (78, 56, 16)
            border_c = (255, 224, 118) if focused else (218, 174, 72)
            text_c = (255, 252, 238)
        elif tone == "danger":
            top_c = (158, 54, 54) if focused else (112, 38, 38)
            bot_c = (102, 28, 28) if focused else (74, 24, 24)
            border_c = (255, 168, 160) if focused else (196, 106, 106)
            text_c = (255, 244, 242)
        elif tone == "emerald":
            top_c = (38, 128, 86) if focused else (28, 96, 64)
            bot_c = (22, 84, 54) if focused else (18, 62, 42)
            border_c = (130, 244, 184) if focused else (86, 184, 132)
            text_c = (242, 255, 248)
        else:
            top_c = (48, 88, 156) if focused else (30, 56, 108)
            bot_c = (28, 54, 104) if focused else (18, 36, 72)
            border_c = (255, 214, 96) if focused else (116, 158, 224)
            text_c = (255, 250, 236)

        self._draw_gradient_panel(surf, rect, top_c, bot_c, radius=radius, alpha=245)
        bw = 2 if focused and enabled else 1
        pygame.draw.rect(surf, (*border_c, 255), rect, width=bw, border_radius=radius)
        if focused and enabled:
            inner = rect.inflate(-4, -4)
            pygame.draw.rect(surf, (255, 235, 150, 95), inner, width=1, border_radius=max(3, radius - 2))

        txt_s = font.render(label, True, text_c)
        sh_s = font.render(label, True, (6, 10, 20))
        tx = rect.centerx - txt_s.get_width() // 2
        ty = rect.centery - txt_s.get_height() // 2
        if enabled:
            surf.blit(sh_s, (tx + 1, ty + 1))
        surf.blit(txt_s, (tx, ty))

    def _render_surface(self, win_w: int, win_h: int, lang: str) -> pygame.Surface:
        st = self.cloud.get_status()
        local_info = self._local_info_cache or self.cloud.get_local_info()
        lay = compute_modal_layout(win_w, win_h, self.page, authorizing=st.authorizing)
        self._last_layout = lay

        surf = pygame.Surface((lay.win_w, lay.win_h), pygame.SRCALPHA)
        surf.fill((6, 10, 20, 176))

        # Outer shadow & Soltia royal RPG frame
        p = lay.panel
        rad = max(8, int(round(12 * lay.scale)))
        sh_off = max(4, int(round(6 * lay.scale)))
        pygame.draw.rect(surf, (0, 0, 0, 170), p.move(sh_off, sh_off), border_radius=rad)
        self._draw_gradient_panel(surf, p, (20, 34, 68), (11, 18, 38), radius=rad, alpha=255)
        pygame.draw.rect(surf, (232, 188, 76, 255), p, width=max(2, int(round(2 * lay.scale))), border_radius=rad)
        pygame.draw.rect(surf, (132, 178, 240, 110), p.inflate(-6, -6), width=1, border_radius=max(4, rad - 3))

        # Header
        f_title = self._font(lay.font_title_sz, bold=True)
        f_sub = self._font(lay.font_sub_sz, bold=False)
        f_lbl = self._font(lay.font_label_sz, bold=True)
        f_sm = self._font(lay.font_small_sz, bold=False)

        if self.page == ModalPage.OPTIONS:
            title_txt = t("options.title", lang)
            sub_txt = t("options.subtitle", lang)
        elif self.page == ModalPage.CLOUD:
            title_txt = t("cloud.title", lang)
            sub_txt = t("cloud.subtitle", lang)
        elif self.page == ModalPage.CONFIRM_UPLOAD:
            title_txt = t("confirm.upload.title", lang)
            sub_txt = ""
        elif self.page == ModalPage.CONFIRM_RESTORE:
            title_txt = t("confirm.restore.title", lang)
            sub_txt = ""
        else:
            title_txt = t("confirm.disconnect.title", lang)
            sub_txt = ""

        # Clamp title font if narrow portrait
        t_sz = lay.font_title_sz
        max_title_w = p.width - lay.pad * 2 - lay.close_rect.width - (lay.refresh_rect.width + lay.gap if lay.refresh_rect.width else 0) - 8
        while t_sz > 11 and f_title.size(title_txt)[0] > max_title_w:
            t_sz -= 1
            f_title = self._font(t_sz, bold=True)

        tx = p.x + lay.pad
        ty = p.y + (lay.header_h - f_title.get_height() - (f_sub.get_height() + 2 if sub_txt else 0)) // 2
        surf.blit(f_title.render(title_txt, True, (8, 12, 22)), (tx + 1, ty + 1))
        surf.blit(f_title.render(title_txt, True, (250, 214, 108)), (tx, ty))
        if sub_txt:
            surf.blit(f_sub.render(sub_txt, True, (166, 198, 238)), (tx, ty + f_title.get_height() + 1))

        sep_y = p.y + lay.header_h
        pygame.draw.line(surf, (232, 188, 76, 120), (p.x + lay.pad, sep_y), (p.right - lay.pad, sep_y), width=1)

        # Close button [X] (geometric cross avoids missing-glyph boxes across fonts/platforms)
        self._draw_button(
            surf, lay.close_rect, "", self._font(max(12, lay.font_label_sz), bold=True),
            focused=self.hover_close, tone="danger" if self.hover_close else "blue",
            radius=max(5, int(6 * lay.scale)),
        )
        cx, cy = lay.close_rect.center
        arm = max(4, int(round(lay.close_rect.width * 0.24)))
        x_w = max(2, int(round(2.0 * lay.scale)))
        pygame.draw.line(surf, (255, 250, 238, 255), (cx - arm, cy - arm), (cx + arm, cy + arm), width=x_w)
        pygame.draw.line(surf, (255, 250, 238, 255), (cx + arm, cy - arm), (cx - arm, cy + arm), width=x_w)

        # ------------------------------------------------- PAGE: OPTIONS
        if self.page == ModalPage.OPTIONS:
            fps = self.get_fps()
            asp = self.get_aspect()
            cur_lang = normalize_language(self.get_lang())
            rows_meta = [
                (
                    t("options.row.fps", lang),
                    t("options.row.fps.desc", lang),
                    t(f"options.val.fps.{fps}", lang),
                    True,
                    (110, 222, 255) if fps == 60 else (245, 204, 82),
                ),
                (
                    t("options.row.aspect", lang),
                    t("options.row.aspect.desc", lang),
                    t(f"options.val.aspect.{asp}", lang),
                    True,
                    (104, 228, 168),
                ),
                (
                    t("options.row.language", lang),
                    t("options.row.language.desc", lang),
                    t(f"options.val.lang.{cur_lang}", lang),
                    True,
                    (245, 204, 82),
                ),
                (
                    t("options.row.cloud", lang),
                    t("options.row.cloud.desc", lang),
                    t("options.val.cloud.connected" if st.connected else "options.val.cloud.disconnected", lang),
                    False,
                    (104, 228, 168) if st.connected else (164, 206, 255),
                ),
            ]
            for i, (r_title, r_desc, val_str, has_chevrons, val_col) in enumerate(rows_meta):
                rc = lay.option_rows[i]
                is_foc = (self.focus_idx == i)
                top_c = (40, 70, 126) if is_foc else (24, 42, 82)
                bot_c = (26, 46, 88) if is_foc else (16, 28, 56)
                self._draw_gradient_panel(surf, rc, top_c, bot_c, radius=max(6, int(8 * lay.scale)), alpha=255)
                brd = (255, 214, 96) if is_foc else (92, 134, 202)
                pygame.draw.rect(surf, (*brd, 240), rc, width=2 if is_foc else 1, border_radius=max(6, int(8 * lay.scale)))

                # Value pill bounds (from left chevron to right chevron)
                l_arr = lay.option_left_arrows[i]
                r_arr = lay.option_right_arrows[i]
                pill_rect = pygame.Rect(l_arr.x, l_arr.y + 2, r_arr.right - l_arr.x, l_arr.height - 4)
                pygame.draw.rect(surf, (14, 24, 48, 255), pill_rect, border_radius=max(5, int(6 * lay.scale)))
                pygame.draw.rect(surf, (*val_col, 165), pill_rect, width=1, border_radius=max(5, int(6 * lay.scale)))

                # Text left side
                avail_lw = max(60, pill_rect.x - rc.x - int(18 * lay.scale))
                r_f = f_lbl
                r_sz = lay.font_label_sz
                while r_sz > 10 and r_f.size(r_title)[0] > avail_lw:
                    r_sz -= 1
                    r_f = self._font(r_sz, bold=True)
                l_s = r_f.render(r_title, True, (255, 250, 236))
                d_s = f_sm.render(r_desc, True, (166, 194, 232)) if not lay.is_compact and f_sm.size(r_desc)[0] <= avail_lw else None
                lx = rc.x + int(12 * lay.scale)
                if d_s:
                    total_th = l_s.get_height() + d_s.get_height() + 2
                    ly = rc.centery - total_th // 2
                    surf.blit(l_s, (lx, ly))
                    surf.blit(d_s, (lx, ly + l_s.get_height() + 2))
                else:
                    surf.blit(l_s, (lx, rc.centery - l_s.get_height() // 2))

                # Value + arrows inside pill
                v_sz = lay.font_label_sz
                v_f = self._font(v_sz, bold=True)
                max_vw = pill_rect.width - (l_arr.width + r_arr.width + 6 if has_chevrons else 14)
                while v_sz > 9 and v_f.size(val_str)[0] > max_vw:
                    v_sz -= 1
                    v_f = self._font(v_sz, bold=True)
                vs = v_f.render(val_str, True, val_col)
                surf.blit(vs, (pill_rect.centerx - vs.get_width() // 2, pill_rect.centery - vs.get_height() // 2))

                if has_chevrons:
                    c_col = (255, 224, 118, 255) if is_foc else (172, 204, 244, 255)
                    cw = max(4, int(round(4.5 * lay.scale)))
                    ch = max(5, int(round(6.0 * lay.scale)))
                    lcx, lcy = l_arr.center
                    rcx, rcy = r_arr.center
                    pygame.draw.polygon(surf, c_col, [(lcx + cw // 2, lcy - ch), (lcx - cw // 2, lcy), (lcx + cw // 2, lcy + ch)])
                    pygame.draw.polygon(surf, c_col, [(rcx - cw // 2, rcy - ch), (rcx + cw // 2, rcy), (rcx - cw // 2, rcy + ch)])

            # Back button & hint footer
            bf = self._fit_uniform_button_font([t("common.back", lang)], [lay.back_rect], lay.font_label_sz)
            self._draw_button(
                surf, lay.back_rect, t("common.back", lang), bf,
                focused=(self.focus_idx == 4), tone="blue", radius=max(6, int(7 * lay.scale)),
            )
            hint_key = "options.footer.hints.compact" if (lay.is_portrait or lay.is_compact) else "options.footer.hints"
            hint_txt = t(hint_key, lang)
            hf = f_sm
            h_avail = p.right - lay.pad - (lay.back_rect.right + lay.gap)
            if hf.size(hint_txt)[0] <= h_avail:
                hs = hf.render(hint_txt, True, (150, 178, 218))
                surf.blit(hs, (p.right - lay.pad - hs.get_width(), lay.back_rect.centery - hs.get_height() // 2))

        # ------------------------------------------------- PAGE: CLOUD
        elif self.page == ModalPage.CLOUD:
            # Refresh button in header
            self._draw_button(
                surf, lay.refresh_rect, t("cloud.btn.refresh", lang),
                self._font(max(10, lay.font_small_sz), bold=True),
                focused=self.hover_refresh,
                enabled=(st.connected and not st.busy),
                tone="blue",
                radius=max(5, int(6 * lay.scale)),
            )

            # Connection indicator pill next to Refresh button if space allows
            if not lay.is_compact and lay.refresh_rect.x - (p.x + lay.pad + 260) > 40:
                badge_txt = t(
                    "cloud.status.authorizing" if st.authorizing else ("cloud.status.connected" if st.connected else "cloud.status.disconnected"),
                    lang,
                )
                b_col = (245, 204, 82) if st.authorizing else ((104, 228, 168) if st.connected else (175, 190, 214))
                bs = f_sm.render(f"● {badge_txt}", True, b_col)
                surf.blit(bs, (lay.refresh_rect.x - lay.gap - bs.get_width(), lay.refresh_rect.centery - bs.get_height() // 2))

            card_r = max(6, int(8 * lay.scale))
            if st.authorizing and lay.auth_box.width > 0:
                ab = lay.auth_box
                self._draw_gradient_panel(surf, ab, (38, 56, 98), (22, 34, 66), radius=card_r, alpha=255)
                pygame.draw.rect(surf, (245, 204, 82, 235), ab, width=2, border_radius=card_r)

                s1 = f_sub.render(t("cloud.auth.step", lang), True, (206, 226, 250))
                url_txt = st.verification_url or "https://www.google.com/device"
                us = f_lbl.render(url_txt, True, (118, 224, 255))
                s2 = f_sub.render(t("cloud.auth.code_label", lang), True, (206, 226, 250))
                code_f = self._font(lay.font_code_sz, bold=True)
                cs = code_f.render(st.user_code or "----", True, (255, 228, 110))
                dots = "." * ((pygame.time.get_ticks() // 400) % 4)
                ws = f_sm.render(f"{t('cloud.auth.waiting', lang)} {dots}", True, (176, 202, 234))

                cur_y = ab.y + max(6, int(8 * lay.scale))
                for piece in (s1, us, s2, cs, ws):
                    surf.blit(piece, (ab.centerx - piece.get_width() // 2, cur_y))
                    cur_y += piece.get_height() + max(2, int(4 * lay.scale))

                if lay.open_browser_rect.width > 0:
                    self._draw_button(
                        surf, lay.open_browser_rect, t("cloud.auth.open_browser", lang),
                        self._font(max(11, lay.font_small_sz), bold=True),
                        focused=self.hover_browser, tone="gold", radius=max(5, int(6 * lay.scale)),
                    )
            else:
                # Cloud Card & Local Card
                for is_remote, rc in ((True, lay.remote_card), (False, lay.local_card)):
                    self._draw_gradient_panel(surf, rc, (24, 42, 82), (16, 26, 54), radius=card_r, alpha=255)
                    accent_c = (110, 214, 255) if is_remote else (104, 228, 168)
                    pygame.draw.rect(surf, (*accent_c, 155), rc, width=1, border_radius=card_r)

                    inner_pad = max(8, int(12 * lay.scale))
                    ct = t("cloud.card.remote.title" if is_remote else "cloud.card.local.title", lang)
                    surf.blit(f_lbl.render(ct, True, accent_c), (rc.x + inner_pad, rc.y + inner_pad // 2 + 2))
                    cy = rc.y + inner_pad // 2 + f_lbl.get_height() + max(4, int(6 * lay.scale))
                    pygame.draw.line(surf, (*accent_c, 65), (rc.x + inner_pad, cy), (rc.right - inner_pad, cy), 1)
                    cy += max(4, int(6 * lay.scale))

                    lines_out: list[tuple[str, tuple[int, int, int]]] = []
                    if is_remote:
                        if not st.connected:
                            lines_out.append((t("cloud.card.remote.unknown", lang), (170, 188, 214)))
                        elif not st.backup_exists:
                            lines_out.append((t("cloud.card.remote.none", lang), (186, 200, 224)))
                        else:
                            lines_out.append((f"{t('cloud.field.date', lang)}: {st.backup_date_local or st.backup_date}", (248, 246, 236)))
                            if st.backup_platform:
                                lines_out.append((f"{t('cloud.field.platform', lang)}: {st.backup_platform}", (184, 214, 248)))
                            if st.backup_summary:
                                lines_out.append((st.backup_summary, (160, 192, 228)))
                    else:
                        if not local_info.exists:
                            lines_out.append((t("cloud.card.local.none", lang), (170, 188, 214)))
                        else:
                            kb = local_info.total_bytes / 1024.0
                            lines_out.append((f"{t('cloud.field.date', lang)}: {local_info.updated_local}", (248, 246, 236)))
                            sum_txt = (
                                t("cloud.summary.slots", lang, slots=local_info.slot_count, files=local_info.file_count, kb=kb)
                                if local_info.slot_count > 0
                                else t("cloud.summary.files_only", lang, files=local_info.file_count, kb=kb)
                            )
                            lines_out.append((sum_txt, (164, 228, 192)))

                    avail_tw = max(40, rc.width - inner_pad * 2)
                    for txt_line, col in lines_out:
                        for w_line in self._wrap_text(f_sm, txt_line, avail_tw):
                            if cy + f_sm.get_height() <= rc.bottom - 4:
                                surf.blit(f_sm.render(w_line, True, col), (rc.x + inner_pad, cy))
                                cy += f_sm.get_height() + 2

            # Feedback bar
            sb = lay.status_bar
            msg = st.message_detail if (not st.message_key and st.message_detail) else t(st.message_key or "cloud.msg.ready_disconnected", lang)
            if not st.last_op_ok:
                sb_bg, sb_brd, sb_col = (72, 26, 32, 255), (235, 112, 112), (255, 222, 222)
            elif st.busy or st.authorizing:
                sb_bg, sb_brd, sb_col = (58, 48, 22, 255), (245, 204, 82), (255, 242, 194)
            else:
                sb_bg, sb_brd, sb_col = (20, 46, 42, 255), (88, 184, 138), (218, 250, 232)
            pygame.draw.rect(surf, sb_bg, sb, border_radius=max(5, int(6 * lay.scale)))
            pygame.draw.rect(surf, (*sb_brd, 210), sb, width=1, border_radius=max(5, int(6 * lay.scale)))
            sb_font = f_sm
            sb_sz = lay.font_small_sz
            while sb_sz > 9 and sb_font.size(msg)[0] > sb.width - 16:
                sb_sz -= 1
                sb_font = self._font(sb_sz, bold=False)
            ms = sb_font.render(msg, True, sb_col)
            surf.blit(ms, (sb.x + int(10 * lay.scale), sb.centery - ms.get_height() // 2))

            # 4 Cloud Action Buttons with Uniform Typography
            b0_lbl = t("cloud.btn.upload" if st.connected else "cloud.btn.connect", lang)
            b1_lbl = t("cloud.btn.restore", lang)
            b2_lbl = t("cloud.btn.cancel_auth" if st.authorizing else "cloud.btn.disconnect", lang)
            b3_lbl = t("common.back", lang)
            btn_specs = [
                (b0_lbl, not (st.busy and not st.authorizing) and not (not st.connected and st.authorizing), "emerald" if st.connected else "gold"),
                (b1_lbl, st.connected and st.backup_exists and not st.busy, "gold"),
                (b2_lbl, st.authorizing or (st.connected and not st.busy), "danger" if st.connected or st.authorizing else "blue"),
                (b3_lbl, True, "blue"),
            ]
            uni_f = self._fit_uniform_button_font([s[0] for s in btn_specs], lay.action_buttons, lay.font_label_sz)
            for idx, (lbl, enabled, tone) in enumerate(btn_specs):
                self._draw_button(
                    surf, lay.action_buttons[idx], lbl, uni_f,
                    focused=(self.focus_idx == idx), enabled=enabled, tone=tone,
                    radius=max(6, int(7 * lay.scale)),
                )

            # Bottom hint
            hint_txt = t("cloud.footer.hints", lang)
            hs = f_sm.render(hint_txt, True, (148, 178, 218))
            if hs.get_width() <= p.width - lay.pad * 2:
                surf.blit(hs, (p.centerx - hs.get_width() // 2, p.bottom - max(16, int(18 * lay.scale)) - hs.get_height() // 2))

        # ------------------------------------------------- PAGE: CONFIRM_*
        else:
            date_str = st.backup_date_local or st.backup_date or t("common.unknown", lang)
            if self.page == ModalPage.CONFIRM_UPLOAD:
                body_txt = t("confirm.upload.body" if st.backup_exists else "confirm.upload.body_new", lang, date=date_str)
                act_lbl, act_tone = t("confirm.action.upload", lang), "gold"
            elif self.page == ModalPage.CONFIRM_RESTORE:
                body_txt = t("confirm.restore.body", lang, date=date_str)
                act_lbl, act_tone = t("confirm.action.restore", lang), "gold"
            else:
                body_txt = t("confirm.disconnect.body", lang)
                act_lbl, act_tone = t("confirm.action.disconnect", lang), "danger"

            cy = p.y + lay.header_h + lay.pad
            for line in self._wrap_text(f_lbl, body_txt, p.width - lay.pad * 2):
                ls = f_lbl.render(line, True, (238, 244, 255))
                surf.blit(ls, (p.centerx - ls.get_width() // 2, cy))
                cy += ls.get_height() + max(4, int(5 * lay.scale))

            labels = [act_lbl, t("common.cancel", lang)]
            uni_f = self._fit_uniform_button_font(labels, lay.confirm_buttons, lay.font_label_sz)
            self._draw_button(
                surf, lay.confirm_buttons[0], act_lbl, uni_f,
                focused=(self.focus_idx == 0), tone=act_tone, radius=max(6, int(8 * lay.scale)),
            )
            self._draw_button(
                surf, lay.confirm_buttons[1], labels[1], uni_f,
                focused=(self.focus_idx == 1), tone="blue", radius=max(6, int(8 * lay.scale)),
            )

        return surf

    def draw_gl(self, win_w: int, win_h: int):
        if not self.is_open:
            return
        lang = get_active_language()
        st = self.cloud.get_status()
        local_info = self._local_info_cache or self.cloud.get_local_info()
        anim_tick = (pygame.time.get_ticks() // 400) % 4 if st.authorizing else 0
        key = (
            win_w, win_h, self.page.value, self.focus_idx,
            self.hover_idx, self.hover_close, self.hover_refresh, self.hover_browser,
            self.get_fps(), self.get_aspect(), lang,
            st, local_info.file_count, local_info.total_bytes, anim_tick,
        )
        if self._gl_tex == 0 or self._tex_key != key:
            surf = self._render_surface(win_w, win_h, lang)
            tw, th = surf.get_size()
            raw = pygame.image.tostring(surf, "RGBA", False)
            if self._gl_tex == 0:
                self._gl_tex = int(glGenTextures(1))
            glBindTexture(GL_TEXTURE_2D, self._gl_tex)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw)
            self._tex_key = key
            self._tex_size = (tw, th)

        tw, th = self._tex_size
        glPushAttrib(GL_ALL_ATTRIB_BITS)
        glDisable(GL_SCISSOR_TEST)
        glViewport(0, 0, int(win_w), int(win_h))
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        glOrtho(0.0, float(win_w), float(win_h), 0.0, -1.0, 1.0)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()

        glEnable(GL_TEXTURE_2D)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glBindTexture(GL_TEXTURE_2D, self._gl_tex)
        glColor4f(1.0, 1.0, 1.0, 1.0)
        glBegin(GL_QUADS)
        glTexCoord2f(0.0, 0.0); glVertex2f(0.0, 0.0)
        glTexCoord2f(1.0, 0.0); glVertex2f(float(tw), 0.0)
        glTexCoord2f(1.0, 1.0); glVertex2f(float(tw), float(th))
        glTexCoord2f(0.0, 1.0); glVertex2f(0.0, float(th))
        glEnd()

        glPopMatrix()
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)
        glPopAttrib()
        glEnable(GL_TEXTURE_2D)
        glColor4f(1.0, 1.0, 1.0, 1.0)

    def reset_gl(self):
        self._gl_tex = 0
        self._tex_key = None
