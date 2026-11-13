#!/usr/bin/env python3
"""Heroes Lore 3: Stigmata of Gaia - PC Native Host Runner (S04 / S05).

Provides an interactive window with hardware-accelerated OpenGL ES 1.1 rendering,
complete ground-truth memory-driven keyboard mapping, gamepad/controller support,
and audio playback.

Controls:
  * Movement: D-Pad / Left Stick or Arrow Keys / WASD
  * Talk / Interact / Confirm: E / Enter / Space / Gamepad A
  * Attack: J / Space / Gamepad X
  * Cancel / Back / Pause: Escape / Backspace / Gamepad B
  * Skip Cutscene: S / Tab / Gamepad Y / Gamepad Start
  * Character Select: Left (Kei) / Right (Rits), Enter Confirm, Esc Back
  * Class Select: Left / Right arrows, Enter Confirm, Esc Back
  * Pause & Submenus: Arrows / WASD / D-Pad / Left Stick move focus, Enter / A confirm,
                      LB / RB or [ / ] or Tab switch pages/tabs, Esc / B back
  * Quick Potions: Q / K / Gamepad LB / RB (during field gameplay)
  * Quick Skills: U / 1, I / 2 (RB), O / 3
  * Mouse: Left Click & Drag anywhere for 1:1 direct touch input!
  * Volume Controls: + / = Increase Volume, - Decrease Volume, M Toggle Mute
  * F8: Toggle Target FPS (30 / 60)
  * F9: Cycle Aspect Ratio (Original 3:2 / 16:9 / Auto True Widescreen)
  * F11: Toggle Fullscreen
  * F12: Capture Screenshot
"""

from __future__ import annotations

import argparse
import ctypes
import datetime as dt
import math
import struct
import sys
import time
from pathlib import Path

import pygame
from pygame.locals import *
import OpenGL
OpenGL.ERROR_CHECKING = False
from OpenGL.GL import *
from PIL import Image

if getattr(sys, "frozen", False):
    _exe_dir = Path(sys.executable).resolve().parent
    if (_exe_dir / "project.json").exists():
        ROOT = _exe_dir
    elif hasattr(sys, "_MEIPASS") and (Path(sys._MEIPASS) / "project.json").exists():
        ROOT = Path(sys._MEIPASS)
    else:
        ROOT = _exe_dir
else:
    ROOT = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.runtime import Runtime
from hl3rt.gl_bridge import GLBridge
from hl3rt.hle_uikit import Touch
from hl3rt.display import (
    UI_ASPECT_CYCLE,
    compute_viewport_layout,
    load_display_config,
    normalize_aspect_mode,
    normalize_fps,
    save_display_config,
)

# Exact Control Coordinates & Targets (480x320 landscape)
DPAD_CENTER = (65.0, 255.0)

# Title Carousel (form 100)
TITLE_CAROUSEL_CENTER = (240.0, 272.0)

# Character Select (form 41)
CHAR_KEI_CARD = (160.0, 96.0)     # id=4102 Kei
CHAR_RITS_CARD = (320.0, 96.0)    # id=4101 Rits
CHAR_CONFIRM = (415.0, 295.0)     # id=4103 Green checkmark
BTN_BACK = (28.0, 28.0)           # id=199 Back arrow (top-left)

# Class Select (form 42)
CLASS_ARROW_PREV = (73.0, 71.0)   # id=4207 < arrow
CLASS_ARROW_NEXT = (400.0, 71.0)  # id=4206 > arrow
CLASS_CONFIRM = (415.0, 295.0)    # id=4201 Green checkmark

# Character Summary (form 43)
SUMMARY_CONFIRM = (356.0, 295.0)  # id=4302 Start Game checkmark

# Cutscene & Dialogues (form 44, sub_state 4 / 22)
CUTSCENE_SKIP = (408.0, 28.0)     # id=4401 SKIP X button
DIALOGUE_ADVANCE = (240.0, 160.0) # Screen tap to advance dialogue lines

# Popups (id=130 Tutorial, id=132 System, id=133 Confirmation)
POPUP_CONFIRM = (395.0, 220.0)    # id=13301 / id=13201 Yes / Confirm checkmark
POPUP_CANCEL = (395.0, 100.0)     # id=13302 / id=13202 No / Cancel X
TUTORIAL_NEXT = (385.0, 207.0)    # id=13001 Next tutorial page checkmark
TUTORIAL_CLOSE = (385.0, 74.0)    # id=13002 Close tutorial X button

# Field HUD Buttons (form 70 - confirmed via live control tree dump)
HUD_MAP = (23.5, 22.0)                # id=7011 Minimap / Compass button (top-left)
HUD_POTION_1 = (29.0, 107.0)          # id=7002 Upper Potion / Quick Item 1 (left side)
HUD_POTION_2 = (29.0, 157.0)          # id=7003 Lower Potion / Quick Item 2 (left side)
HUD_SKILL_1 = (451.0, 184.0)          # id=7006 Quick skill 1 (right side)
HUD_SKILL_2 = (451.0, 134.0)          # id=7005 Quick skill 2 (right side)
HUD_SKILL_3 = (451.0, 84.0)           # id=7004 Quick skill 3 (right side)
HUD_ATTACK_PRIMARY = (450.0, 258.0)   # id=7007 Attack button (combat mode)
HUD_ATTACK_SECONDARY = (408.5, 289.0) # id=7008 Secondary attack / guard (combat mode)
HUD_TALK_PEACEFUL = (450.0, 258.0)    # id=7025 Speech bubble talk button (peaceful mode)
HUD_TALK_COMBAT = (397.0, 239.0)      # id=7026 Speech bubble talk button (combat mode)
HUD_PORTRAIT_MENU = (200.0, 292.0)    # id=7009 Character portrait (opens Pause Menu)
HUD_STATUS = (23.5, 22.0)             # id=7011 Avatar / status / map

# Pause Menu (form 2)
PAUSE_MENU_BACK = (28.0, 28.0)     # id=207 Back button
PAUSE_MENU_ITEMS_Y = [45.0, 90.0, 135.0, 180.0, 225.0, 270.0]
# 0: CHARACTER (id=201), 1: EQUIPMENT (id=203), 2: SKILLS (id=204),
# 3: ITEMS (id=202), 4: QUESTS (id=205), 5: SYSTEM (id=206)

# Legacy aliases for backwards compatibility and test suites
BTN_ATTACK = HUD_ATTACK_PRIMARY
BTN_TALK = HUD_TALK_PEACEFUL
BTN_CENTER = TITLE_CAROUSEL_CENTER
BTN_MENU = HUD_PORTRAIT_MENU
BTN_MAP = HUD_MAP
BTN_ITEM = HUD_POTION_1
BTN_ITEM2 = HUD_POTION_2
BTN_SKILL1 = HUD_SKILL_1
BTN_SKILL2 = HUD_SKILL_2
BTN_SKILL3 = HUD_SKILL_3
DPAD_UP = (65.0, 225.0)
MENU_ITEMS_Y = PAUSE_MENU_ITEMS_Y


def log_boot(msg: str):
    try:
        with open("hl3_boot.log", "a", encoding="utf-8") as f:
            f.write(msg + "\n")
    except Exception:
        pass


class PCRunner:
    def __init__(
        self,
        scale: int = 2,
        fullscreen: bool = False,
        headless_frames: int = 0,
        aspect_mode: str | None = None,
        target_fps: int | None = None,
    ):
        log_boot(f"PCRunner.__init__: scale={scale} aspect={aspect_mode} fps={target_fps}")
        self.scale = max(1, scale)
        cfg = load_display_config() if headless_frames == 0 else {}
        if aspect_mode is None:
            aspect_mode = cfg.get("aspect_mode", "original")
        if target_fps is None:
            target_fps = cfg.get("target_fps", 30)
        self.aspect_mode = normalize_aspect_mode(aspect_mode)
        self.target_fps = normalize_fps(target_fps)
        if self.aspect_mode == "16:9" and headless_frames == 0:
            self.win_w = int(round(320.0 * (16.0 / 9.0) * self.scale))
            self.win_h = 320 * self.scale
        else:
            self.win_w, self.win_h = 480 * self.scale, 320 * self.scale
        self._windowed_size = (self.win_w, self.win_h)
        self.game_w, self.game_h = 480.0, 320.0
        self.fullscreen = fullscreen
        self.headless_frames = headless_frames
        self.running = False
        self.clock = pygame.time.Clock()
        self.active_touches: dict[str, tuple] = {}
        self.touch_responder = 0
        self.delegate = 0
        self.joysticks = []
        self.facing_x = 0.0
        self.facing_y = 1.0  # default facing down
        self.menu_index = 0
        self.nav_cooldown = 0
        self.carousel_cooldown = 0
        self.scroll_cooldown = 0
        self.submenu_focus_ctx: tuple | None = None
        self.submenu_focus_idx = 0
        self.submenu_focus_cid = 0
        self.skill_tab_idx = 0
        self.pending_tap_ups: dict[str, int] = {}

        log_boot("Calling _init_window")
        self._init_window()
        log_boot("Calling _init_runtime")
        self._init_runtime()
        log_boot("Calling _init_joysticks")
        self._init_joysticks()
        log_boot("PCRunner init complete")

    def _resolve_icon_path(self, filename: str) -> Path | None:
        candidates = [ROOT / "assets" / filename]
        meipass = getattr(sys, "_MEIPASS", None)
        if meipass:
            candidates.append(Path(meipass) / "assets" / filename)
        if filename == "icon.png":
            candidates.append(ROOT / "reference" / "extracted_ipa" / "Payload" / "Heroes Lore.app" / "Icon.png")
        for candidate in candidates:
            if candidate.exists():
                return candidate
        return None

    def _apply_win32_icon(self):
        if sys.platform != "win32":
            return
        try:
            wm_info = pygame.display.get_wm_info()
            hwnd = wm_info.get("window")
            if not hwnd:
                return
            ico_path = self._resolve_icon_path("icon.ico")
            if not ico_path:
                return

            user32 = ctypes.windll.user32
            ptr_type = ctypes.c_void_p

            user32.LoadImageW.argtypes = [
                ptr_type, ctypes.c_wchar_p, ctypes.c_uint,
                ctypes.c_int, ctypes.c_int, ctypes.c_uint,
            ]
            user32.LoadImageW.restype = ptr_type
            user32.SendMessageW.argtypes = [ptr_type, ctypes.c_uint, ctypes.c_size_t, ctypes.c_ssize_t]
            user32.SendMessageW.restype = ctypes.c_ssize_t

            IMAGE_ICON = 1
            LR_LOADFROMFILE = 0x0010
            LR_DEFAULTSIZE = 0x0040
            WM_SETICON = 0x0080
            ICON_SMALL = 0
            ICON_BIG = 1
            GCLP_HICON = -14
            GCLP_HICONSM = -34

            sm_cx = user32.GetSystemMetrics(49) or 16  # SM_CXSMICON
            sm_cy = user32.GetSystemMetrics(50) or 16  # SM_CYSMICON
            bg_cx = user32.GetSystemMetrics(11) or 32  # SM_CXICON
            bg_cy = user32.GetSystemMetrics(12) or 32  # SM_CYICON

            hicon_big = user32.LoadImageW(None, str(ico_path), IMAGE_ICON, bg_cx, bg_cy, LR_LOADFROMFILE)
            hicon_sm = user32.LoadImageW(None, str(ico_path), IMAGE_ICON, sm_cx, sm_cy, LR_LOADFROMFILE)
            if not hicon_big:
                hicon_big = user32.LoadImageW(None, str(ico_path), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE)
            if not hicon_sm:
                hicon_sm = hicon_big

            self._win32_hicons = (hicon_big, hicon_sm)
            hwnd_ptr = ptr_type(hwnd)
            if hicon_big:
                user32.SendMessageW(hwnd_ptr, WM_SETICON, ICON_BIG, hicon_big)
            if hicon_sm:
                user32.SendMessageW(hwnd_ptr, WM_SETICON, ICON_SMALL, hicon_sm)

            set_cls_ptr = getattr(user32, "SetClassLongPtrW", None) or user32.SetClassLongW
            set_cls_ptr.argtypes = [ptr_type, ctypes.c_int, ctypes.c_ssize_t]
            set_cls_ptr.restype = ctypes.c_ssize_t
            if hicon_big:
                set_cls_ptr(hwnd_ptr, GCLP_HICON, hicon_big)
            if hicon_sm:
                set_cls_ptr(hwnd_ptr, GCLP_HICONSM, hicon_sm)

            # Also set console icon when launched with a console window
            kernel32 = ctypes.windll.kernel32
            kernel32.GetConsoleWindow.restype = ptr_type
            con_hwnd = kernel32.GetConsoleWindow()
            if con_hwnd:
                if hicon_big:
                    user32.SendMessageW(con_hwnd, WM_SETICON, ICON_BIG, hicon_big)
                if hicon_sm:
                    user32.SendMessageW(con_hwnd, WM_SETICON, ICON_SMALL, hicon_sm)
        except Exception as e:
            print(f"[HL3 PC] Could not apply Win32 window icon: {e}")

    def _init_window(self):
        if sys.platform == "win32":
            try:
                ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(
                    "DavidBraga.HeroesLore3.Recomp.PC"
                )
            except Exception:
                pass
        pygame.init()
        # Set Pygame icon BEFORE set_mode() so SDL2 never creates the window with pygame_icon.bmp
        icon_path = self._resolve_icon_path("icon.png")
        if icon_path is not None:
            try:
                icon_surf = pygame.image.load(str(icon_path))
                icon_32 = pygame.transform.smoothscale(icon_surf, (32, 32))
                pygame.display.set_icon(icon_32)
            except Exception as e:
                print(f"[HL3 PC] Could not preload window icon: {e}")

        flags = DOUBLEBUF | OPENGL | RESIZABLE
        if self.fullscreen:
            try:
                desktop_sizes = pygame.display.get_desktop_sizes()
                if desktop_sizes:
                    self.win_w, self.win_h = desktop_sizes[0]
            except Exception:
                pass
            flags = DOUBLEBUF | OPENGL | FULLSCREEN
        pygame.display.set_caption("Heroes Lore 3: Stigmata of Gaia Recomp (PC Native)")
        self.window = pygame.display.set_mode((self.win_w, self.win_h), flags)
        self._apply_win32_icon()

    def get_engine_state(self) -> dict:
        if not hasattr(self, "rt") or not self.rt or not hasattr(self.rt, "m"):
            return {
                "app_state": 0, "sub_state": 0, "form_id": 0, "main_popup_id": 0,
                "hud_popup_id": 0, "active_popup_id": 0, "can_talk": 0, "combat_mode": 0,
                "main_form": 0, "main_popup": 0, "hud_popup": 0,
            }
        m = self.rt.m
        try:
            app = m.rd32(0x000d7dd4)
            if not app:
                return {
                    "app_state": 0, "sub_state": 0, "form_id": 0, "main_popup_id": 0,
                    "hud_popup_id": 0, "active_popup_id": 0, "can_talk": 0, "combat_mode": 0,
                    "main_form": 0, "main_popup": 0, "hud_popup": 0,
                }
            app_state = m.rd32(app + 0x78)
            ui_main = m.rd32(app + 0x40)
            ui_hud = m.rd32(app + 0x44)
            main_form = m.rd32(ui_main + 0x98) if ui_main else 0
            main_popup = m.rd32(ui_main + 0xa0) if ui_main else 0
            hud_form = m.rd32(ui_hud + 0x98) if ui_hud else 0
            hud_popup = m.rd32(ui_hud + 0xa0) if ui_hud else 0

            form_id = m.rd16(main_form + 0x14) if main_form else 0
            main_popup_id = m.rd16(main_popup + 0x14) if main_popup else 0
            hud_popup_id = m.rd16(hud_popup + 0x14) if hud_popup else 0

            sub_state = m.rds8(0x000de30c)
            can_talk = m.rd8(0x000de0ad)
            combat_mode = m.rd8(0x000de0ac)

            active_popup_id = main_popup_id or hud_popup_id

            return {
                "app_state": app_state,
                "sub_state": sub_state,
                "form_id": form_id,
                "main_popup_id": main_popup_id,
                "hud_popup_id": hud_popup_id,
                "active_popup_id": active_popup_id,
                "can_talk": can_talk,
                "combat_mode": combat_mode,
                "main_form": main_form,
                "main_popup": main_popup,
                "hud_popup": hud_popup,
            }
        except Exception:
            return {
                "app_state": 0, "sub_state": 0, "form_id": 0, "main_popup_id": 0,
                "hud_popup_id": 0, "active_popup_id": 0, "can_talk": 0, "combat_mode": 0,
                "main_form": 0, "main_popup": 0, "hud_popup": 0,
            }

    def _ensure_focus_attrs(self):
        if not hasattr(self, "submenu_focus_ctx"):
            self.submenu_focus_ctx = None
            self.submenu_focus_idx = 0
            self.submenu_focus_cid = 0
            self.skill_tab_idx = 0
            self.scroll_cooldown = 0

    def _read_ui_node(self, ptr: int) -> dict | None:
        if not ptr or not hasattr(self, "rt") or not self.rt or not hasattr(self.rt, "m"):
            return None
        m = self.rt.m
        try:
            ctype = m.rd16(ptr + 0x04)
            px, py, pw, ph = struct.unpack("<4h", m.rdbytes(ptr + 0x08, 8))
            cid = m.rd16(ptr + 0x14)
            vis = m.rd16(ptr + 0x16)
            fx0, fx1, fy0, fy1 = struct.unpack("<4f", m.rdbytes(ptr + 0x18, 16))
            next_sibling = m.rd32(ptr + 0x30)
            first_child = m.rd32(ptr + 0x38)
        except Exception:
            return None

        max_w = (float(self.rt.gl_bridge.layout.patch_w) + 40.0) if getattr(self.rt, "gl_bridge", None) else 520.0
        max_h = (float(self.rt.gl_bridge.layout.patch_h) + 40.0) if getattr(self.rt, "gl_bridge", None) else 360.0
        frect_ok = (
            all(math.isfinite(v) for v in (fx0, fx1, fy0, fy1))
            and (fx1 - fx0) >= 2.0
            and (fy1 - fy0) >= 2.0
            and fx0 > -40.0 and fx1 < max_w
            and fy0 > -40.0 and fy1 < max_h
        )
        if not frect_ok:
            if pw >= 4 and ph >= 4 and px > -40 and (px + pw) < max_w and py > -40 and (py + ph) < max_h:
                fx0, fx1, fy0, fy1 = float(px), float(px + pw), float(py), float(py + ph)
            else:
                fx0, fx1, fy0, fy1 = 0.0, 0.0, 0.0, 0.0

        cx = (fx0 + fx1) * 0.5
        cy = (fy0 + fy1) * 0.5
        return {
            "ptr": ptr,
            "ctype": ctype,
            "cid": cid,
            "vis": vis,
            "pos": (px, py, pw, ph),
            "frect": (fx0, fx1, fy0, fy1),
            "cx": cx,
            "cy": cy,
            "next_sibling": next_sibling,
            "first_child": first_child,
            "kind": "slider" if ctype == 13 else "button",
        }

    def _find_active_overlay(self, root_ptr: int, target_cids: tuple[int, ...]) -> dict | None:
        if not root_ptr or not target_cids:
            return None
        found: dict[int, dict] = {}
        queue = [root_ptr]
        visited: set[int] = set()
        while queue and len(visited) < 160:
            ptr = queue.pop(0)
            if not ptr or ptr in visited:
                continue
            visited.add(ptr)
            node = self._read_ui_node(ptr)
            if not node:
                continue
            if node["cid"] in target_cids and (node["vis"] & 0x0003) == 0x0003:
                found[node["cid"]] = node
            ch = node["first_child"]
            steps = 0
            while ch and ch not in visited and steps < 64:
                queue.append(ch)
                chn = self._read_ui_node(ch)
                ch = chn["next_sibling"] if chn else 0
                steps += 1
        for tcid in target_cids:
            if tcid in found:
                return found[tcid]
        return None

    def _find_slider_arrows(self, slider_node: dict) -> tuple[tuple[float, float], tuple[float, float]]:
        fx0, fx1, fy0, fy1 = slider_node["frect"]
        cy = slider_node["cy"]
        prev_coord = (fx0 + 18.0, cy)
        next_coord = (fx1 - 18.0, cy)
        queue = [slider_node["first_child"]]
        visited: set[int] = set()
        while queue and len(visited) < 24:
            ptr = queue.pop(0)
            if not ptr or ptr in visited:
                continue
            visited.add(ptr)
            ch = self._read_ui_node(ptr)
            if not ch:
                continue
            if ch["cid"] == 198 and ch["frect"][1] > ch["frect"][0]:
                prev_coord = (ch["cx"], ch["cy"])
            elif ch["cid"] == 197 and ch["frect"][1] > ch["frect"][0]:
                next_coord = (ch["cx"], ch["cy"])
            if ch["first_child"]:
                queue.append(ch["first_child"])
            if ch["next_sibling"]:
                queue.append(ch["next_sibling"])
        return prev_coord, next_coord

    def _collect_subtree_controls(self, root_ptr: int, keep_back: bool = False) -> tuple[list[dict], list[dict]]:
        controls: list[dict] = []
        scroll_boxes: list[dict] = []
        if not root_ptr:
            return controls, scroll_boxes

        visited: set[int] = set()

        def walk(ptr: int, is_root: bool = False):
            if not ptr or ptr in visited or len(visited) > 256:
                return
            visited.add(ptr)
            node = self._read_ui_node(ptr)
            if not node:
                return

            ctype = node["ctype"]
            vis = node["vis"]
            fx0, fx1, fy0, fy1 = node["frect"]
            valid_rect = (fx1 - fx0) >= 4.0 and (fy1 - fy0) >= 4.0 and 0.0 <= node["cx"] <= 480.0 and 0.0 <= node["cy"] <= 320.0

            if ctype in (1, 14, 15):
                if (vis & 0x0003) != 0x0003 and not is_root:
                    return
                ch = node["first_child"]
                steps = 0
                while ch and steps < 64:
                    walk(ch, False)
                    chn = self._read_ui_node(ch)
                    ch = chn["next_sibling"] if chn else 0
                    steps += 1
                return

            if ctype == 13:
                if (vis & 0x0003) == 0x0003 and valid_rect:
                    p_coord, n_coord = self._find_slider_arrows(node)
                    node["prev_coord"] = p_coord
                    node["next_coord"] = n_coord
                    controls.append(node)
                return

            if ctype in (2, 4):
                if (vis & 0x0003) == 0x0003 and valid_rect:
                    controls.append(node)
                ch = node["first_child"]
                steps = 0
                while ch and steps < 32:
                    walk(ch, False)
                    chn = self._read_ui_node(ch)
                    ch = chn["next_sibling"] if chn else 0
                    steps += 1
                return

            if ctype == 10:
                if (vis & 0x0003) == 0x0003 and valid_rect and (fy1 - fy0) >= 36.0:
                    scroll_boxes.append(node)

        root_node = self._read_ui_node(root_ptr)
        if not root_node:
            return controls, scroll_boxes
        if root_node["ctype"] in (1, 14, 15):
            ch = root_node["first_child"]
            steps = 0
            while ch and steps < 64:
                walk(ch, False)
                chn = self._read_ui_node(ch)
                ch = chn["next_sibling"] if chn else 0
                steps += 1
        else:
            walk(root_ptr, True)

        # Deduplicate overlapping identical controls if any
        seen_keys: set[tuple[int, int, int]] = set()
        deduped: list[dict] = []
        for c in controls:
            k = (c["cid"], int(round(c["cx"])), int(round(c["cy"])))
            if k not in seen_keys:
                seen_keys.add(k)
                deduped.append(c)

        # Filter out redundant top-left corner Back button when real targets or scroll views exist
        def _is_corner_back(c: dict) -> bool:
            return c["cid"] in (199, 207, 306, 4709) and c["cx"] < 55.0 and c["cy"] < 55.0

        if not keep_back:
            non_back = [c for c in deduped if not _is_corner_back(c)]
            if non_back or scroll_boxes:
                deduped = non_back

        return deduped, scroll_boxes

    def get_focusable_controls(self, st: dict | None = None) -> tuple[tuple | None, list[dict], list[dict]]:
        if st is None:
            st = self.get_engine_state()
        main_popup_id = st.get("main_popup_id", 0)
        hud_popup_id = st.get("hud_popup_id", 0)
        app_state = st.get("app_state", 0)
        form_id = st.get("form_id", 0)

        if main_popup_id and st.get("main_popup"):
            ctrls, scrolls = self._collect_subtree_controls(st["main_popup"])
            return ("popup", int(main_popup_id), 0), ctrls, scrolls

        if hud_popup_id and hud_popup_id != 130 and st.get("hud_popup"):
            ctrls, scrolls = self._collect_subtree_controls(st["hud_popup"])
            return ("hud_popup", int(hud_popup_id), 0), ctrls, scrolls

        if app_state == 8 and form_id != 2 and st.get("main_form"):
            root_ptr = st["main_form"]
            overlay_cid = 0
            overlay_candidates = {8: (837, 836), 46: (4708,)}.get(form_id, ())
            if overlay_candidates:
                ov = self._find_active_overlay(root_ptr, overlay_candidates)
                if ov:
                    root_ptr = ov["ptr"]
                    overlay_cid = ov["cid"]
            ctrls, scrolls = self._collect_subtree_controls(root_ptr)
            return ("submenu", int(form_id), int(overlay_cid)), ctrls, scrolls

        if app_state == 1 and form_id not in (0, 100, 41, 42, 43) and st.get("main_form"):
            ctrls, scrolls = self._collect_subtree_controls(st["main_form"])
            return ("pregame", int(form_id), 0), ctrls, scrolls

        return None, [], []

    def _sync_focus(self, st: dict) -> tuple[list[dict], dict | None, list[dict]]:
        self._ensure_focus_attrs()
        ctx, controls, scroll_boxes = self.get_focusable_controls(st)
        if ctx is None:
            self.submenu_focus_ctx = None
            self.submenu_focus_idx = 0
            self.submenu_focus_cid = 0
            return [], None, []

        if not controls:
            self.submenu_focus_ctx = ctx
            self.submenu_focus_idx = 0
            self.submenu_focus_cid = 0
            return [], None, scroll_boxes

        if ctx != self.submenu_focus_ctx:
            idx = 0
            if ctx[0] in ("popup", "hud_popup") and len(controls) >= 2:
                # Default popup focus to the Confirm / Yes checkmark (usually bottom/right, excluding Cancel IDs)
                cancel_cids = {199, 207, 306, 4709, 10602, 13202, 13302}
                best_i = 0
                best_score = -1e9
                for i, c in enumerate(controls):
                    score = c["cy"] + (0.0 if c["cid"] not in cancel_cids else -1000.0)
                    if score > best_score:
                        best_score = score
                        best_i = i
                idx = best_i
            self.submenu_focus_ctx = ctx
            self.submenu_focus_idx = idx
            self.submenu_focus_cid = controls[idx]["cid"]
        else:
            # Keep focus anchored to the same control ID if new controls appear (e.g. ITEMS USE/DROP enabled)
            matched = -1
            if self.submenu_focus_cid:
                for i, c in enumerate(controls):
                    if c["cid"] == self.submenu_focus_cid:
                        matched = i
                        break
            if matched >= 0:
                self.submenu_focus_idx = matched
            else:
                self.submenu_focus_idx = max(0, min(self.submenu_focus_idx, len(controls) - 1))
                self.submenu_focus_cid = controls[self.submenu_focus_idx]["cid"]

        return controls, controls[self.submenu_focus_idx], scroll_boxes

    def _move_focus_2d(self, controls: list[dict], cur_idx: int, dir_name: str) -> int:
        if len(controls) <= 1:
            return 0
        cur = controls[cur_idx]
        cx, cy = cur["cx"], cur["cy"]

        best_idx = -1
        best_score = float("inf")
        for i, cand in enumerate(controls):
            if i == cur_idx:
                continue
            dx = cand["cx"] - cx
            dy = cand["cy"] - cy
            if dir_name == "right" and dx > 4.0:
                score = dx + abs(dy) * 2.5
            elif dir_name == "left" and dx < -4.0:
                score = -dx + abs(dy) * 2.5
            elif dir_name == "down" and dy > 4.0:
                score = dy + abs(dx) * 2.5
            elif dir_name == "up" and dy < -4.0:
                score = -dy + abs(dx) * 2.5
            else:
                continue
            if score < best_score:
                best_score = score
                best_idx = i

        if best_idx >= 0:
            return best_idx

        # Wraparound handling when hitting an edge
        if dir_name in ("right", "left"):
            same_row = [i for i, c in enumerate(controls) if abs(c["cy"] - cy) <= 28.0]
            if len(same_row) > 1:
                if dir_name == "right":
                    return min(same_row, key=lambda i: controls[i]["cx"])
                return max(same_row, key=lambda i: controls[i]["cx"])
            if len(controls) == 2:
                return 1 - cur_idx
            return cur_idx

        if dir_name in ("down", "up"):
            if dir_name == "down":
                return min(range(len(controls)), key=lambda i: (controls[i]["cy"], abs(controls[i]["cx"] - cx)))
            return max(range(len(controls)), key=lambda i: (controls[i]["cy"], -abs(controls[i]["cx"] - cx)))

        return cur_idx

    def _save_display_prefs(self):
        if getattr(self, "headless_frames", 0) == 0:
            save_display_config({
                "aspect_mode": self.aspect_mode,
                "target_fps": self.target_fps,
            })

    def _resize_window(self, new_w: int, new_h: int):
        if (new_w, new_h) == (self.win_w, self.win_h):
            return
        try:
            if sys.platform == "win32":
                hwnd = pygame.display.get_wm_info().get("window")
                if hwnd:
                    user32 = ctypes.windll.user32
                    class _RECT(ctypes.Structure):
                        _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long), ("right", ctypes.c_long), ("bottom", ctypes.c_long)]
                    rc = _RECT(0, 0, int(new_w), int(new_h))
                    hwnd_p = ctypes.c_void_p(hwnd)
                    style = user32.GetWindowLongW(hwnd_p, -16)
                    ex_style = user32.GetWindowLongW(hwnd_p, -20)
                    user32.AdjustWindowRectEx(ctypes.byref(rc), style, False, ex_style)
                    # SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE (0x0016)
                    user32.SetWindowPos(hwnd_p, 0, 0, 0, rc.right - rc.left, rc.bottom - rc.top, 0x0016)
                    self.win_w, self.win_h = int(new_w), int(new_h)
                    self._windowed_size = (self.win_w, self.win_h)
                    if hasattr(self, "rt") and self.rt and hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
                        self.rt.gl_bridge.update_viewport(self.win_w, self.win_h)
                    return
            self.window = pygame.display.set_mode((new_w, new_h), DOUBLEBUF | OPENGL | RESIZABLE)
            self._apply_win32_icon()
            self.win_w, self.win_h = int(new_w), int(new_h)
            self._windowed_size = (self.win_w, self.win_h)
        except Exception:
            pass

    def set_aspect_mode(self, mode: str, resize_window: bool = False):
        self.aspect_mode = normalize_aspect_mode(mode)
        if resize_window and not self.fullscreen and getattr(self, "headless_frames", 0) == 0:
            if self.aspect_mode == "16:9":
                new_w = int(round(320.0 * (16.0 / 9.0) * self.scale))
                new_h = 320 * self.scale
            elif self.aspect_mode == "original":
                new_w, new_h = 480 * self.scale, 320 * self.scale
            else:
                new_w, new_h = self.win_w, self.win_h
            self._resize_window(new_w, new_h)
        if hasattr(self, "rt") and self.rt and hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
            self.rt.gl_bridge.set_aspect_mode(self.aspect_mode)
            self.game_w = float(self.rt.gl_bridge.game_w)
            self.game_h = float(self.rt.gl_bridge.game_h)
        self._save_display_prefs()
        print(f"[HL3 PC] Aspect Mode: {self.aspect_mode.upper()} ({self.game_w:.0f}x{self.game_h:.0f})")

    def set_target_fps(self, fps: int):
        self.target_fps = normalize_fps(fps)
        self._save_display_prefs()
        print(f"[HL3 PC] Target FPS: {self.target_fps}")

    def _ui_main_offset(self, st: dict | None = None) -> tuple[float, float]:
        if not hasattr(self, "rt") or not self.rt or not getattr(self.rt, "gl_bridge", None):
            return 0.0, 0.0
        state = st if st is not None else self.get_engine_state()
        if state.get("app_state", 0) in (0, 1, 256):
            return 0.0, 0.0
        lay = self.rt.gl_bridge.layout
        return float(lay.ui_dx), float(lay.ui_dy)

    def _main_pt(self, coord: tuple[float, float], st: dict | None = None, is_hud_popup: bool = False) -> tuple[float, float]:
        if is_hud_popup:
            return coord
        dx, dy = self._ui_main_offset(st)
        return coord[0] + dx, coord[1] + dy

    def _hud_pt(self, coord: tuple[float, float]) -> tuple[float, float]:
        if not hasattr(self, "rt") or not self.rt or not getattr(self.rt, "gl_bridge", None):
            return coord
        lay = self.rt.gl_bridge.layout
        x, y = coord
        if x < 150.0:
            dx = 0.0
        elif x <= 330.0:
            dx = float(lay.hud_dx_center)
        else:
            dx = float(lay.hud_dx_right)
        if y < 100.0:
            dy = 0.0
        elif y < 180.0:
            dy = float(lay.hud_dy_bottom // 2)
        else:
            dy = float(lay.hud_dy_bottom)
        return x + dx, y + dy

    def _scroll_box(self, box: dict, dir_name: str, st: dict | None = None):
        self._ensure_focus_attrs()
        if self.scroll_cooldown > 0 or not self.touch_responder:
            return
        self.scroll_cooldown = 8
        is_hud = bool(st and st.get("hud_popup_id"))
        ox, oy = (0.0, 0.0) if is_hud else self._ui_main_offset(st)
        cx = box["cx"] + ox
        fy0, fy1 = box["frect"][2] + oy, box["frect"][3] + oy
        mid_y = (fy0 + fy1) * 0.5
        span = min(44.0, max(16.0, (fy1 - fy0) * 0.35))
        if dir_name == "down":
            # Drag finger upward to scroll content downward
            start_y, end_y = mid_y + span * 0.5, mid_y - span * 0.5
        else:
            start_y, end_y = mid_y - span * 0.5, mid_y + span * 0.5

        t = Touch(cx, start_y, 0)
        t_obj = self.rt.objc.new_obj("UITouch", t)
        evt_obj = self.rt.objc.new_obj("UIEvent", {"touches": [t_obj]})
        set_obj = self.rt.objc.new_obj("NSSet", [t_obj])
        self.rt.send(self.touch_responder, "touchesBegan:withEvent:", set_obj, evt_obj)
        for step in range(1, 5):
            t.prev_x, t.prev_y = t.x, t.y
            t.y = start_y + (end_y - start_y) * (step / 4.0)
            t.phase = 1
            self.rt.send(self.touch_responder, "touchesMoved:withEvent:", set_obj, evt_obj)
        t.phase = 3
        self.rt.send(self.touch_responder, "touchesEnded:withEvent:", set_obj, evt_obj)

    def handle_page_tab(self, delta: int):
        """Switch tabs or pages in submenus via LB/RB or Tab/[ /]."""
        self._ensure_focus_attrs()
        st = self.get_engine_state()
        if st.get("active_popup_id"):
            return
        app_state = st.get("app_state", 0)
        form_id = st.get("form_id", 0)
        if app_state != 8:
            return

        if form_id == 2:
            self.menu_index = (self.menu_index + delta) % len(PAUSE_MENU_ITEMS_Y)
            return

        if form_id == 10:  # SKILLS tabs: PASSIVE (1005), PRIMARY (1003), SECONDARY (1004)
            controls, _, _ = self._sync_focus(st)
            tab_ctrls = sorted([c for c in controls if c["cid"] in (1005, 1003, 1004)], key=lambda c: c["cx"])
            if tab_ctrls:
                self.skill_tab_idx = (self.skill_tab_idx + delta) % len(tab_ctrls)
                tgt = tab_ctrls[self.skill_tab_idx]
                self.tap_once("skill_tab", self._main_pt((tgt["cx"], tgt["cy"]), st), hold_frames=2)
            else:
                coords = [(120.0, 25.0), (240.0, 25.0), (360.0, 25.0)]
                self.skill_tab_idx = (self.skill_tab_idx + delta) % len(coords)
                self.tap_once("skill_tab", self._main_pt(coords[self.skill_tab_idx], st), hold_frames=2)
            return

        page_cid_pairs = {
            8: (818, 817),       # ITEMS < / >
            118: (11809, 11810), # HELP < / >
            6: (640, 641),       # EQUIPMENT page < / >
        }
        pair = page_cid_pairs.get(form_id)
        if pair:
            controls, _, _ = self._sync_focus(st)
            want_cid = pair[0] if delta < 0 else pair[1]
            for c in controls:
                if c["cid"] == want_cid:
                    self.tap_once("sub_page", self._main_pt((c["cx"], c["cy"]), st), hold_frames=2)
                    return

    def _draw_focus_rect(self, frect: tuple[float, float, float, float], offset: tuple[float, float] = (0.0, 0.0), bounds: tuple[float, float] = (480.0, 320.0)):
        fx0, fx1, fy0, fy1 = frect
        ox, oy = offset
        bw, bh = bounds
        pad = 2.0
        x0 = max(2.0, fx0 + ox - pad)
        x1 = min(bw - 2.0, fx1 + ox + pad)
        y0 = max(2.0, fy0 + oy - pad)
        y1 = min(bh - 2.0, fy1 + oy + pad)
        cy = (y0 + y1) * 0.5
        cx = (x0 + x1) * 0.5

        pulse = 0.8 + 0.2 * math.sin(pygame.time.get_ticks() * 0.008)

        # Dark backing border for high contrast against light/bright UI panels
        glColor4f(0.05, 0.05, 0.1, 0.7)
        glLineWidth(4.0)
        glBegin(GL_LINE_LOOP)
        glVertex2f(x0, y0)
        glVertex2f(x1, y0)
        glVertex2f(x1, y1)
        glVertex2f(x0, y1)
        glEnd()

        # Crisp golden selection frame
        glColor4f(1.0, 0.88 * pulse, 0.18, 0.96)
        glLineWidth(2.2)
        glBegin(GL_LINE_LOOP)
        glVertex2f(x0, y0)
        glVertex2f(x1, y0)
        glVertex2f(x1, y1)
        glVertex2f(x0, y1)
        glEnd()

        # Indicator triangle (left side if space allows, otherwise top)
        glColor4f(1.0, 0.92, 0.28, 1.0)
        glBegin(GL_TRIANGLES)
        if x0 >= 14.0:
            glVertex2f(x0 - 10.0, cy - 5.5)
            glVertex2f(x0 - 2.5, cy)
            glVertex2f(x0 - 10.0, cy + 5.5)
        else:
            ty = max(10.0, y0)
            glVertex2f(cx - 5.5, ty - 9.0)
            glVertex2f(cx + 5.5, ty - 9.0)
            glVertex2f(cx, ty - 2.0)
        glEnd()

    def _draw_menu_cursor(self):
        st = self.get_engine_state()
        app_state = st.get("app_state", 0)
        form_id = st.get("form_id", 0)
        active_popup = st.get("active_popup_id", 0)

        is_pause_main = (app_state == 8 and form_id == 2 and not active_popup)
        focused = None
        if not is_pause_main:
            _, focused, _ = self._sync_focus(st)
            if not focused:
                return

        try:
            gb = self.rt.gl_bridge if hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge else None
            vx, vy, vw, vh = gb.viewport_rect if gb else (0, 0, self.win_w, self.win_h)
            gw = float(gb.game_w) if gb else 480.0
            gh = float(gb.game_h) if gb else 320.0
            ox, oy = (0.0, 0.0) if st.get("hud_popup_id") else self._ui_main_offset(st)
            glPushAttrib(GL_ALL_ATTRIB_BITS)
            glViewport(vx, vy, vw, vh)
            glMatrixMode(GL_PROJECTION)
            glPushMatrix()
            glLoadIdentity()
            glOrtho(0.0, gw, gh, 0.0, -1.0, 1.0)
            glMatrixMode(GL_MODELVIEW)
            glPushMatrix()
            glLoadIdentity()

            glDisable(GL_TEXTURE_2D)
            glEnable(GL_BLEND)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

            if is_pause_main:
                y = PAUSE_MENU_ITEMS_Y[self.menu_index] + oy

                # Golden glowing selection frame around option (x: 82 to 398, centered by ox)
                glColor4f(1.0, 0.85, 0.2, 0.9)
                glLineWidth(2.5)
                glBegin(GL_LINE_LOOP)
                glVertex2f(82.0 + ox, y - 18.0)
                glVertex2f(398.0 + ox, y - 18.0)
                glVertex2f(398.0 + ox, y + 18.0)
                glVertex2f(82.0 + ox, y + 18.0)
                glEnd()

                # Left pointer arrow ▶
                glColor4f(1.0, 0.9, 0.3, 1.0)
                glBegin(GL_TRIANGLES)
                glVertex2f(66.0 + ox, y - 9.0)
                glVertex2f(77.0 + ox, y)
                glVertex2f(66.0 + ox, y + 9.0)
                glEnd()

                # Right pointer arrow ◀
                glBegin(GL_TRIANGLES)
                glVertex2f(414.0 + ox, y - 9.0)
                glVertex2f(403.0 + ox, y)
                glVertex2f(414.0 + ox, y + 9.0)
                glEnd()
            elif focused is not None:
                self._draw_focus_rect(focused["frect"], offset=(ox, oy), bounds=(gw, gh))

            glPopMatrix()
            glMatrixMode(GL_PROJECTION)
            glPopMatrix()
            glMatrixMode(GL_MODELVIEW)
            glPopAttrib()
            glEnable(GL_TEXTURE_2D)
        except Exception:
            pass

    def _present(self):
        self._draw_menu_cursor()
        pygame.display.flip()

    def _init_runtime(self):
        log_boot("_init_runtime: start")
        print("[HL3 PC] Initializing Mach-O ARMv6 runtime and OpenGL bridge...")
        self.rt = Runtime()
        log_boot("_init_runtime: Runtime created")
        self.rt.gl_bridge = GLBridge(
            self.rt.m,
            win_w=self.win_w,
            win_h=self.win_h,
            aspect_mode=self.aspect_mode,
        )
        self.game_w = float(self.rt.gl_bridge.game_w)
        self.game_h = float(self.rt.gl_bridge.game_h)
        log_boot("_init_runtime: GLBridge created")
        self.rt.present_hook = self._present

        print("[HL3 PC] Booting to UIApplicationMain...")
        self.rt.boot_to_app_main()
        log_boot("_init_runtime: boot_to_app_main done")

        print("[HL3 PC] Launching AppDelegate and core views...")
        self.rt.run_launch()
        log_boot("_init_runtime: run_launch done")
        self.delegate = self.rt.state["delegate"]

        for k, obj in self.rt.objc.hobj.items():
            cls, _ = self.rt.objc.class_of(k)
            if cls and cls.name == "TouchResponder":
                self.touch_responder = k
                break
        print(f"[HL3 PC] TouchResponder active at 0x{self.touch_responder:08x}")
        log_boot(f"_init_runtime: TouchResponder=0x{self.touch_responder:08x}")

    def _init_joysticks(self):
        pygame.joystick.init()
        count = pygame.joystick.get_count()
        print(f"[HL3 PC] Detected {count} gamepad(s)")
        for i in range(count):
            try:
                js = pygame.joystick.Joystick(i)
                js.init()
                self.joysticks.append(js)
                print(f"[HL3 PC] Initialized gamepad {i}: {js.get_name()}")
            except Exception as e:
                print(f"[HL3 PC] Failed to init gamepad {i}: {e}")

    # ------------------------------------------------------------- Touch input
    def touch_down(self, tag: str, coord: tuple[float, float]):
        if tag in self.active_touches or not self.touch_responder:
            return
        x, y = coord
        t = Touch(x, y, 0)  # phase 0 = began
        t_obj = self.rt.objc.new_obj("UITouch", t)
        evt_obj = self.rt.objc.new_obj("UIEvent", {"touches": [t_obj]})
        set_obj = self.rt.objc.new_obj("NSSet", [t_obj])
        self.active_touches[tag] = (t, t_obj, evt_obj, set_obj)
        self.rt.send(self.touch_responder, "touchesBegan:withEvent:", set_obj, evt_obj)

    def touch_move(self, tag: str, coord: tuple[float, float]):
        entry = self.active_touches.get(tag)
        if not entry or not self.touch_responder:
            return
        t, t_obj, evt_obj, set_obj = entry
        t.prev_x, t.prev_y = t.x, t.y
        t.x, t.y = coord
        t.phase = 1  # phase 1 = moved
        self.rt.send(self.touch_responder, "touchesMoved:withEvent:", set_obj, evt_obj)

    def touch_up(self, tag: str):
        entry = self.active_touches.pop(tag, None)
        if not entry or not self.touch_responder:
            return
        t, t_obj, evt_obj, set_obj = entry
        t.phase = 3  # phase 3 = ended
        self.rt.send(self.touch_responder, "touchesEnded:withEvent:", set_obj, evt_obj)

    def tap_once(self, tag: str, coord: tuple[float, float], hold_frames: int = 2):
        self.touch_down(tag, coord)
        self.pending_tap_ups[tag] = max(1, hold_frames)

    def swipe_carousel(self, direction: str):
        if self.carousel_cooldown > 0 or not self.touch_responder:
            return
        self.carousel_cooldown = 14  # Cooldown between carousel swipes (~230ms)
        if direction == "right":  # Next option (drag right-to-left)
            start_x, end_x = 320.0, 160.0
        else:  # Previous option (drag left-to-right)
            start_x, end_x = 160.0, 320.0
        y = 280.0

        t = Touch(start_x, y, 0)
        t_obj = self.rt.objc.new_obj("UITouch", t)
        evt_obj = self.rt.objc.new_obj("UIEvent", {"touches": [t_obj]})
        set_obj = self.rt.objc.new_obj("NSSet", [t_obj])
        self.rt.send(self.touch_responder, "touchesBegan:withEvent:", set_obj, evt_obj)
        for step in range(1, 5):
            interp_x = start_x + (end_x - start_x) * (step / 4.0)
            t.prev_x, t.prev_y = t.x, t.y
            t.x = interp_x
            t.phase = 1
            self.rt.send(self.touch_responder, "touchesMoved:withEvent:", set_obj, evt_obj)
        t.phase = 3
        self.rt.send(self.touch_responder, "touchesEnded:withEvent:", set_obj, evt_obj)

    def window_to_game_coord(self, wx: int, wy: int) -> tuple[float, float]:
        if hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
            return self.rt.gl_bridge.window_to_game_coord(wx, wy)
        gx = max(0.0, min(float(self.game_w), float(wx) / self.win_w * self.game_w))
        gy = max(0.0, min(float(self.game_h), float(wy) / self.win_h * self.game_h))
        return gx, gy

    # ------------------------------------------------------------- High-Level Actions
    def handle_confirm(self):
        """Unified confirmation handler based on live ground-truth engine state."""
        st = self.get_engine_state()
        app_state = st["app_state"]
        form_id = st["form_id"]
        sub_state = st["sub_state"]
        active_popup = st["active_popup_id"]

        # 1. Active modal popup has top priority
        if active_popup:
            if st["hud_popup_id"] == 130:
                self.tap_once("tut_confirm", TUTORIAL_NEXT, hold_frames=2)
            else:
                _, focused, _ = self._sync_focus(st)
                if focused:
                    coord = focused["next_coord"] if focused.get("kind") == "slider" else (focused["cx"], focused["cy"])
                    self.tap_once("popup_confirm", self._main_pt(coord, st), hold_frames=2)
                else:
                    self.tap_once("popup_confirm", self._main_pt(POPUP_CONFIRM, st), hold_frames=2)
            return

        # 2. Pause Menu / Submenus (app_state == 8)
        if app_state == 8:
            if form_id == 2:
                y = PAUSE_MENU_ITEMS_Y[self.menu_index]
                self.tap_once("pause_opt", self._main_pt((240.0, y), st), hold_frames=2)
            else:
                _, focused, _ = self._sync_focus(st)
                if focused:
                    if focused.get("kind") == "slider":
                        self.tap_once("sub_confirm", self._main_pt(focused["next_coord"], st), hold_frames=2)
                    else:
                        if form_id == 10 and focused["cid"] in (1005, 1003, 1004):
                            tab_order = {1005: 0, 1003: 1, 1004: 2}
                            self.skill_tab_idx = tab_order[focused["cid"]]
                        self.tap_once("sub_confirm", self._main_pt((focused["cx"], focused["cy"]), st), hold_frames=2)
                else:
                    self.tap_once("sub_confirm", self._main_pt(POPUP_CONFIRM, st), hold_frames=2)
            return

        # 3. Pre-game menus (app_state == 1)
        if app_state == 1:
            if form_id == 100:  # Title screen
                self.tap_once("title_start", TITLE_CAROUSEL_CENTER, hold_frames=2)
            elif form_id == 41:  # Character select
                self.tap_once("char_confirm", CHAR_CONFIRM, hold_frames=2)
            elif form_id == 42:  # Class select
                self.tap_once("class_confirm", CLASS_CONFIRM, hold_frames=2)
            elif form_id == 43:  # Character summary
                self.tap_once("summary_confirm", SUMMARY_CONFIRM, hold_frames=2)
            else:
                _, focused, _ = self._sync_focus(st)
                if focused:
                    self.tap_once("menu_confirm", (focused["cx"], focused["cy"]), hold_frames=2)
                else:
                    self.tap_once("menu_confirm", POPUP_CONFIRM, hold_frames=2)
            return

        # 4. Gameplay & Cutscenes (app_state == 2)
        if app_state == 2:
            if sub_state in (4, 22):
                # Dialogue / Cutscene: advance text
                self.tap_once("dialogue_adv", self._main_pt(DIALOGUE_ADVANCE, st), hold_frames=2)
                return
            if sub_state == 7:
                # System message: dismiss
                self.tap_once("msg_dismiss", self._main_pt(DIALOGUE_ADVANCE, st), hold_frames=2)
                return
            if sub_state == 3:
                # In field: if NPC talk prompt is active, talk!
                if st["can_talk"]:
                    if "dpad" in self.active_touches:
                        self.touch_up("dpad")
                    talk_pos = HUD_TALK_COMBAT if st["combat_mode"] else HUD_TALK_PEACEFUL
                    self.tap_once("hud_talk", self._hud_pt(talk_pos), hold_frames=2)
                elif st["combat_mode"]:
                    self.tap_once("hud_attack", self._hud_pt(HUD_ATTACK_PRIMARY), hold_frames=2)
                else:
                    self.tap_once("hud_interact", self._hud_pt(HUD_TALK_PEACEFUL), hold_frames=2)
                return

    def handle_cancel(self):
        """Unified cancel / back / close handler."""
        st = self.get_engine_state()
        app_state = st["app_state"]
        form_id = st["form_id"]
        sub_state = st["sub_state"]
        active_popup = st["active_popup_id"]

        # 1. Active modal popup: close / cancel
        if active_popup:
            if st["hud_popup_id"] == 130:
                self.tap_once("tut_close", TUTORIAL_CLOSE, hold_frames=2)
            else:
                controls, _, _ = self._sync_focus(st)
                cancel_cids = (199, 207, 306, 4709, 10602, 13202, 13302)
                cancel_ctrl = next((c for c in controls if c["cid"] in cancel_cids), None)
                if cancel_ctrl:
                    self.tap_once("popup_cancel", self._main_pt((cancel_ctrl["cx"], cancel_ctrl["cy"]), st), hold_frames=2)
                else:
                    self.tap_once("popup_cancel", self._main_pt(POPUP_CANCEL, st), hold_frames=2)
            return

        # 2. Pause Menu / Submenus
        if app_state == 8:
            if form_id == 46 and st.get("main_form"):
                ov = self._find_active_overlay(st["main_form"], (4708,))
                if ov:
                    ctrls, _ = self._collect_subtree_controls(ov["ptr"], keep_back=True)
                    close_btn = next((c for c in ctrls if c["cid"] in (4709, 199, 207)), ctrls[0] if ctrls else None)
                    if close_btn:
                        self.tap_once("quest_detail_close", self._main_pt((close_btn["cx"], close_btn["cy"]), st), hold_frames=2)
                        return
            if form_id == 8 and st.get("main_form"):
                ov = self._find_active_overlay(st["main_form"], (837, 836))
                if ov:
                    ctrls, _ = self._collect_subtree_controls(ov["ptr"], keep_back=True)
                    cancel_btn = next((c for c in ctrls if c["cid"] in (199, 207, 306)), None)
                    if cancel_btn:
                        self.tap_once("item_ov_cancel", self._main_pt((cancel_btn["cx"], cancel_btn["cy"]), st), hold_frames=2)
                        return
            self.tap_once("pause_back", self._main_pt(PAUSE_MENU_BACK, st), hold_frames=2)
            return

        # 3. Pre-game menus
        if app_state == 1:
            if form_id in (41, 42, 43, 40):
                self.tap_once("pregame_back", BTN_BACK, hold_frames=2)
            return

        # 4. Gameplay & Cutscenes
        if app_state == 2:
            if sub_state in (4, 22):
                self.handle_skip()
                return
            if sub_state == 7:
                self.tap_once("msg_dismiss", self._main_pt(DIALOGUE_ADVANCE, st), hold_frames=2)
                return
            if sub_state == 3:
                # In field: open Pause Menu
                self.tap_once("open_pause", self._hud_pt(HUD_PORTRAIT_MENU), hold_frames=2)
                return

    def handle_skip(self):
        """Skip cutscenes or confirm skip."""
        st = self.get_engine_state()
        if st.get("active_popup_id") == 133:
            # Skip confirmation popup: Yes!
            self.tap_once("skip_yes", self._main_pt(POPUP_CONFIRM, st), hold_frames=2)
            return
        if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
            self.tap_once("cutscene_skip", self._main_pt(CUTSCENE_SKIP, st), hold_frames=2)

    def handle_nav(self, dir_name: str):
        """Discrete directional navigation for menus and carousels."""
        st = self.get_engine_state()
        app_state = st["app_state"]
        form_id = st["form_id"]

        # Title screen carousel
        if app_state == 1 and form_id == 100:
            if dir_name in ("left", "up"):
                self.swipe_carousel("left")
            elif dir_name in ("right", "down"):
                self.swipe_carousel("right")
            return

        # Character select: Kei (Left) vs Rits (Right)
        if app_state == 1 and form_id == 41:
            if dir_name in ("left", "up"):
                self.tap_once("sel_kei", CHAR_KEI_CARD, hold_frames=2)
            elif dir_name in ("right", "down"):
                self.tap_once("sel_rits", CHAR_RITS_CARD, hold_frames=2)
            return

        # Class select: Previous (Left) vs Next (Right)
        if app_state == 1 and form_id == 42:
            if dir_name in ("left", "up"):
                self.tap_once("class_prev", CLASS_ARROW_PREV, hold_frames=2)
            elif dir_name in ("right", "down"):
                self.tap_once("class_next", CLASS_ARROW_NEXT, hold_frames=2)
            return

        # Pause Menu: move selection index without auto-clicking
        if app_state == 8 and form_id == 2 and not st.get("active_popup_id"):
            if dir_name in ("up", "left"):
                self.menu_index = (self.menu_index - 1) % len(PAUSE_MENU_ITEMS_Y)
            elif dir_name in ("down", "right"):
                self.menu_index = (self.menu_index + 1) % len(PAUSE_MENU_ITEMS_Y)
            return

        # Live UI control-tree navigation (Popups, Pause Submenus, unknown Pregame screens)
        controls, focused, scroll_boxes = self._sync_focus(st)
        if not controls and not scroll_boxes:
            return

        if focused and focused.get("kind") == "slider":
            is_hud = bool(st.get("hud_popup_id"))
            if dir_name == "left":
                self.tap_once("slider_prev", self._main_pt(focused["prev_coord"], st, is_hud), hold_frames=2)
                return
            if dir_name == "right":
                self.tap_once("slider_next", self._main_pt(focused["next_coord"], st, is_hud), hold_frames=2)
                return

        if scroll_boxes and dir_name in ("up", "down"):
            if len(controls) <= 1:
                self._scroll_box(scroll_boxes[0], dir_name, st)
                return
            next_idx = self._move_focus_2d(controls, self.submenu_focus_idx, dir_name)
            if abs(controls[next_idx]["cy"] - focused["cy"]) < 14.0:
                self._scroll_box(scroll_boxes[0], dir_name, st)
                return

        if len(controls) > 1:
            next_idx = self._move_focus_2d(controls, self.submenu_focus_idx, dir_name)
            self.submenu_focus_idx = next_idx
            self.submenu_focus_cid = controls[next_idx]["cid"]

    # ------------------------------------------------------------- Input poll
    def poll_input(self):
        self._ensure_focus_attrs()
        if self.carousel_cooldown > 0:
            self.carousel_cooldown -= 1
        if self.nav_cooldown > 0:
            self.nav_cooldown -= 1
        if self.scroll_cooldown > 0:
            self.scroll_cooldown -= 1

        # Release timed taps
        tags_to_release = []
        for tag, rem in list(self.pending_tap_ups.items()):
            if rem <= 1:
                tags_to_release.append(tag)
                del self.pending_tap_ups[tag]
            else:
                self.pending_tap_ups[tag] = rem - 1
        for tag in tags_to_release:
            self.touch_up(tag)

        for event in pygame.event.get():
            if event.type == QUIT:
                self.running = False
                return

            elif event.type == VIDEORESIZE:
                self.win_w = max(320, int(event.w))
                self.win_h = max(240, int(event.h))
                if not self.fullscreen:
                    self._windowed_size = (self.win_w, self.win_h)
                if hasattr(self, "rt") and self.rt and hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
                    self.rt.gl_bridge.update_viewport(self.win_w, self.win_h)
                    self.game_w = float(self.rt.gl_bridge.game_w)
                    self.game_h = float(self.rt.gl_bridge.game_h)

            # Mouse / Direct 1:1 Touch
            elif event.type == MOUSEBUTTONDOWN and event.button == 1:
                gx, gy = self.window_to_game_coord(event.pos[0], event.pos[1])
                self.touch_down("mouse", (gx, gy))

            elif event.type == MOUSEBUTTONUP and event.button == 1:
                self.touch_up("mouse")

            elif event.type == MOUSEMOTION and ("mouse" in self.active_touches):
                gx, gy = self.window_to_game_coord(event.pos[0], event.pos[1])
                self.touch_move("mouse", (gx, gy))

            # Discrete Keydown Events
            elif event.type == KEYDOWN:
                if event.key == K_F8:
                    self.set_target_fps(60 if self.target_fps == 30 else 30)
                elif event.key == K_F9:
                    cur = self.aspect_mode if self.aspect_mode in UI_ASPECT_CYCLE else "original"
                    nxt = UI_ASPECT_CYCLE[(UI_ASPECT_CYCLE.index(cur) + 1) % len(UI_ASPECT_CYCLE)]
                    self.set_aspect_mode(nxt, resize_window=True)
                elif event.key == K_F11:
                    self.fullscreen = not self.fullscreen
                    if self.fullscreen:
                        self._windowed_size = (self.win_w, self.win_h)
                        try:
                            desktop_sizes = pygame.display.get_desktop_sizes()
                            if desktop_sizes:
                                self.win_w, self.win_h = desktop_sizes[0]
                        except Exception:
                            pass
                        flags = DOUBLEBUF | OPENGL | FULLSCREEN
                    else:
                        self.win_w, self.win_h = getattr(self, "_windowed_size", (480 * self.scale, 320 * self.scale))
                        flags = DOUBLEBUF | OPENGL | RESIZABLE
                    self.window = pygame.display.set_mode((self.win_w, self.win_h), flags)
                    self._apply_win32_icon()
                    if hasattr(self, "rt") and self.rt and hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
                        self.rt.gl_bridge.update_viewport(self.win_w, self.win_h)
                        self.game_w = float(self.rt.gl_bridge.game_w)
                        self.game_h = float(self.rt.gl_bridge.game_h)
                elif event.key == K_F12:
                    self.capture_screenshot()
                elif event.key in (K_EQUALS, K_PLUS, getattr(pygame, "K_KP_PLUS", K_PLUS)):
                    if hasattr(self.rt, "audio_mixer"):
                        cur = self.rt.audio_mixer.master_volume
                        self.rt.audio_mixer.set_master_volume(cur + 0.1)
                        print(f"[HL3 Audio] Volume: {int(self.rt.audio_mixer.master_volume * 100)}%")
                elif event.key in (K_MINUS, getattr(pygame, "K_KP_MINUS", K_MINUS)):
                    if hasattr(self.rt, "audio_mixer"):
                        cur = self.rt.audio_mixer.master_volume
                        self.rt.audio_mixer.set_master_volume(cur - 0.1)
                        print(f"[HL3 Audio] Volume: {int(self.rt.audio_mixer.master_volume * 100)}%")
                # Map / Minimap (field), Skip (cutscene), Tab switch (submenu), or Audio Mute (M)
                elif event.key in (K_m, K_TAB):
                    st = self.get_engine_state()
                    if st.get("app_state") == 2 and st.get("sub_state") == 3 and not st.get("active_popup_id"):
                        self.tap_once("hud_map", self._hud_pt(HUD_MAP), hold_frames=3)
                    elif st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    elif event.key == K_TAB and st.get("app_state") == 8 and not st.get("active_popup_id"):
                        self.handle_page_tab(1)
                    elif event.key == K_m:
                        if hasattr(self.rt, "audio_mixer"):
                            self.rt.audio_mixer.toggle_mute()
                            print(f"[HL3 Audio] Muted: {self.rt.audio_mixer.muted}")

                # Page / Tab cycling in Submenus
                elif event.key in (K_PAGEUP, K_LEFTBRACKET):
                    self.handle_page_tab(-1)
                elif event.key in (K_PAGEDOWN, K_RIGHTBRACKET):
                    self.handle_page_tab(1)

                # Audio mute toggle (N or Ctrl+M)
                elif event.key == K_n:
                    if hasattr(self.rt, "audio_mixer"):
                        self.rt.audio_mixer.toggle_mute()
                        print(f"[HL3 Audio] Muted: {self.rt.audio_mixer.muted}")

                # Confirm / Talk / Advance: Enter / Space / E / Z
                elif event.key in (K_RETURN, K_SPACE, K_e, K_z):
                    self.handle_confirm()

                # Cancel / Back / Pause: Escape / Backspace
                elif event.key in (K_ESCAPE, K_BACKSPACE):
                    self.handle_cancel()

                # Skip cutscene (when in cutscene) or Down navigation in menus
                elif event.key == K_s:
                    st = self.get_engine_state()
                    if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    elif st.get("app_state") != 2 or st.get("active_popup_id"):
                        self.handle_nav("down")

                # Menu navigation arrows
                elif event.key in (K_UP, K_w):
                    self.handle_nav("up")
                elif event.key == K_DOWN:
                    self.handle_nav("down")
                elif event.key in (K_LEFT, K_a):
                    self.handle_nav("left")
                elif event.key in (K_RIGHT, K_d):
                    self.handle_nav("right")

                # Quick Potions & Combat Skills (only during active field gameplay)
                elif event.key in (K_q, K_1, K_k, K_2, K_u, K_3, K_i, K_4, K_o, K_5, K_j, K_l, K_h, K_p, K_c):
                    st = self.get_engine_state()
                    is_field = st.get("app_state") == 2 and st.get("sub_state") == 3 and not st.get("active_popup_id")
                    if is_field:
                        if event.key in (K_q, K_1):
                            self.tap_once("potion1", self._hud_pt(HUD_POTION_1), hold_frames=3)
                        elif event.key in (K_k, K_2):
                            self.tap_once("potion2", self._hud_pt(HUD_POTION_2), hold_frames=3)
                        elif event.key in (K_u, K_3):
                            self.tap_once("skill1", self._hud_pt(HUD_SKILL_1), hold_frames=2)
                        elif event.key in (K_i, K_4):
                            self.tap_once("skill2", self._hud_pt(HUD_SKILL_2), hold_frames=2)
                        elif event.key in (K_o, K_5):
                            self.tap_once("skill3", self._hud_pt(HUD_SKILL_3), hold_frames=2)
                        elif event.key == K_j:
                            self.tap_once("attack1", self._hud_pt(HUD_ATTACK_PRIMARY), hold_frames=2)
                        elif event.key in (K_l, K_h):
                            self.tap_once("attack2", self._hud_pt(HUD_ATTACK_SECONDARY), hold_frames=2)
                        elif event.key in (K_p, K_c):
                            self.tap_once("portrait_menu", self._hud_pt(HUD_PORTRAIT_MENU), hold_frames=3)
                    elif event.key in (K_p, K_c) and st.get("app_state") == 8:
                        self.handle_cancel()

            # Gamepad Button Events
            elif event.type == JOYBUTTONDOWN:
                btn = event.button
                st = self.get_engine_state()
                is_field = st.get("app_state") == 2 and st.get("sub_state") == 3 and not st.get("active_popup_id")
                if btn == 0:  # A (Confirm / Talk)
                    self.handle_confirm()
                elif btn == 1:  # B (Cancel / Back)
                    self.handle_cancel()
                elif btn == 2:  # X (Attack Primary / Skill 1 in field)
                    if is_field:
                        if st.get("combat_mode"):
                            self.tap_once("pad_attack", self._hud_pt(HUD_ATTACK_PRIMARY), hold_frames=2)
                        else:
                            self.tap_once("skill1", self._hud_pt(HUD_SKILL_1), hold_frames=2)
                elif btn == 3:  # Y (Skip cutscene or Secondary Attack in field)
                    if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    elif is_field:
                        self.tap_once("attack2", self._hud_pt(HUD_ATTACK_SECONDARY), hold_frames=2)
                elif btn == 4:  # LB (Potion 1 in field, Prev Tab/Page in submenus)
                    if is_field:
                        self.tap_once("potion1", self._hud_pt(HUD_POTION_1), hold_frames=3)
                    elif st.get("app_state") == 8 and not st.get("active_popup_id"):
                        self.handle_page_tab(-1)
                elif btn == 5:  # RB (Potion 2 in field, Next Tab/Page in submenus)
                    if is_field:
                        self.tap_once("potion2", self._hud_pt(HUD_POTION_2), hold_frames=3)
                    elif st.get("app_state") == 8 and not st.get("active_popup_id"):
                        self.handle_page_tab(1)
                elif btn == 6:  # Select (Minimap / Map in field)
                    if is_field:
                        self.tap_once("hud_map", self._hud_pt(HUD_MAP), hold_frames=3)
                elif btn == 7:  # Start (Pause Menu / Skip / Back)
                    if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    elif is_field:
                        self.tap_once("open_pause", self._hud_pt(HUD_PORTRAIT_MENU), hold_frames=3)
                    elif st.get("app_state") == 8:
                        self.handle_cancel()

            # Gamepad D-pad Hat Events
            elif event.type == JOYHATMOTION:
                hx, hy = event.value
                if self.nav_cooldown == 0:
                    if hy > 0:
                        self.handle_nav("up")
                        self.nav_cooldown = 12
                    elif hy < 0:
                        self.handle_nav("down")
                        self.nav_cooldown = 12
                    if hx < 0:
                        self.handle_nav("left")
                        self.nav_cooldown = 12
                    elif hx > 0:
                        self.handle_nav("right")
                        self.nav_cooldown = 12

            elif event.type == JOYDEVICEADDED:
                try:
                    js = pygame.joystick.Joystick(event.device_index)
                    js.init()
                    self.joysticks.append(js)
                    print(f"[HL3 PC] Gamepad connected: {js.get_name()}")
                except Exception as e:
                    print(f"[HL3 PC] Gamepad error: {e}")

        # Continuous Field Movement (D-Pad)
        st = self.get_engine_state()
        if st["app_state"] == 2 and st["sub_state"] == 3 and not st["active_popup_id"]:
            keys = pygame.key.get_pressed()
            dx, dy = 0.0, 0.0
            if keys[K_LEFT] or keys[K_a]:
                dx -= 1.0
            if keys[K_RIGHT] or keys[K_d]:
                dx += 1.0
            if keys[K_UP] or keys[K_w]:
                dy -= 1.0
            if keys[K_DOWN] or keys[K_s]:
                dy += 1.0

            for js in self.joysticks:
                if js.get_numhats() > 0:
                    hx, hy = js.get_hat(0)
                    if hx < 0:
                        dx = -1.0
                    elif hx > 0:
                        dx = 1.0
                    if hy > 0:
                        dy = -1.0
                    elif hy < 0:
                        dy = 1.0
                if js.get_numaxes() >= 2:
                    ax = js.get_axis(0)
                    ay = js.get_axis(1)
                    deadzone = 0.25
                    if abs(ax) > deadzone:
                        dx = float(ax)
                    if abs(ay) > deadzone:
                        dy = float(ay)

            if abs(dx) > 0.05 or abs(dy) > 0.05:
                length = (dx * dx + dy * dy) ** 0.5
                mag = min(1.0, length)
                nx = (dx / length) * mag
                ny = (dy / length) * mag
                self.facing_x = dx / length
                self.facing_y = dy / length
                dpad_center = self._hud_pt(DPAD_CENTER)
                target_x = dpad_center[0] + nx * 30.0
                target_y = dpad_center[1] + ny * 30.0

                if "dpad" not in self.active_touches:
                    self.touch_down("dpad", dpad_center)
                self.touch_move("dpad", (target_x, target_y))
            else:
                if "dpad" in self.active_touches:
                    self.touch_up("dpad")
        else:
            if "dpad" in self.active_touches:
                self.touch_up("dpad")

            # Gamepad Left Stick & held D-Pad discrete navigation in menus / popups
            if (st["app_state"] != 2 or st["active_popup_id"]) and self.nav_cooldown == 0:
                nav_dir = None
                for js in self.joysticks:
                    if js.get_numhats() > 0:
                        hx, hy = js.get_hat(0)
                        if hy > 0:
                            nav_dir = "up"
                        elif hy < 0:
                            nav_dir = "down"
                        elif hx < 0:
                            nav_dir = "left"
                        elif hx > 0:
                            nav_dir = "right"
                    if not nav_dir and js.get_numaxes() >= 2:
                        ax = float(js.get_axis(0))
                        ay = float(js.get_axis(1))
                        menu_deadzone = 0.55
                        if abs(ax) > menu_deadzone or abs(ay) > menu_deadzone:
                            if abs(ay) >= abs(ax):
                                nav_dir = "up" if ay < 0 else "down"
                            else:
                                nav_dir = "left" if ax < 0 else "right"
                    if nav_dir:
                        self.handle_nav(nav_dir)
                        self.nav_cooldown = 12
                        break

    def capture_screenshot(self) -> Path:
        pixels = glReadPixels(0, 0, self.win_w, self.win_h, GL_RGBA, GL_UNSIGNED_BYTE)
        img = Image.frombytes("RGBA", (self.win_w, self.win_h), pixels).transpose(Image.FLIP_TOP_BOTTOM).convert("RGB")
        out_dir = ROOT / "reference" / "analysis"
        out_dir.mkdir(parents=True, exist_ok=True)
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = out_dir / f"screenshot_{ts}.png"
        img.save(out_path)
        print(f"[HL3 PC] Screenshot saved to: {out_path.relative_to(ROOT)}")
        return out_path

    def step_frame(self):
        self.poll_input()
        if hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
            st = self.get_engine_state()
            self.rt.gl_bridge.set_world_active(st.get("app_state", 0) not in (0, 1, 256))
            self.game_w = float(self.rt.gl_bridge.game_w)
            self.game_h = float(self.rt.gl_bridge.game_h)
            self.rt.gl_bridge.begin_frame()
        self.rt.send(self.delegate, "onTimer:", 0)

    def run(self):
        self.running = True
        frame = 0
        print(
            f"[HL3 PC] Main loop started ({self.target_fps} FPS, Aspect={self.aspect_mode.upper()}). "
            "F8=FPS, F9=Aspect, F11=Fullscreen, F12=Screenshot, Esc=Back/Pause."
        )
        while self.running:
            self.step_frame()
            self.clock.tick(self.target_fps)
            frame += 1
            if self.headless_frames > 0 and frame >= self.headless_frames:
                print(f"[HL3 PC] Reached target {self.headless_frames} frames in test mode.")
                self.capture_screenshot()
                break

        print("[HL3 PC] Shutting down...")
        if hasattr(self.rt, "audio_mixer"):
            self.rt.audio_mixer.save_config()
        self._save_display_prefs()
        pygame.quit()


def main():
    try:
        parser = argparse.ArgumentParser(description="Heroes Lore 3 PC Native Host Runner")
        parser.add_argument("--scale", type=int, default=2, help="Window scale factor (default: 2 -> 960x640)")
        parser.add_argument("--fullscreen", action="store_true", help="Launch in fullscreen mode")
        parser.add_argument("--aspect", choices=("original", "3:2", "16:9", "16:10", "20:9", "4:3", "auto"), default=None, help="Aspect mode (original, 16:9, auto)")
        parser.add_argument("--fps", type=int, choices=(30, 60), default=None, help="Target FPS cap (30 or 60)")
        parser.add_argument("--frames", type=int, default=0, help="Run for N frames then exit (automated testing)")
        args = parser.parse_args()

        runner = PCRunner(
            scale=args.scale,
            fullscreen=args.fullscreen,
            headless_frames=args.frames,
            aspect_mode=args.aspect,
            target_fps=args.fps,
        )
        runner.run()
    except BaseException as exc:
        import traceback
        err = traceback.format_exc()
        try:
            with open("hl3_error.log", "w", encoding="utf-8") as f:
                f.write(f"Exception type: {type(exc)}\n{err}\n")
                f.flush()
        except Exception:
            pass


if __name__ == "__main__":
    main()
