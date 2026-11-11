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
  * Pause Menu: Up / Down navigate, Enter select option, Esc resume game
  * Quick Potions: Q / K / Gamepad LB
  * Quick Skills: U / 1, I / 2 (RB), O / 3
  * Mouse: Left Click & Drag anywhere for 1:1 direct touch input!
  * Volume Controls: + / = Increase Volume, - Decrease Volume, M Toggle Mute
  * F11: Toggle Fullscreen
  * F12: Capture Screenshot
"""

from __future__ import annotations

import argparse
import datetime as dt
import struct
import sys
import time
from pathlib import Path

import pygame
from pygame.locals import *
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
    def __init__(self, scale: int = 2, fullscreen: bool = False, headless_frames: int = 0):
        log_boot(f"PCRunner.__init__: scale={scale}")
        self.scale = max(1, scale)
        self.game_w, self.game_h = 480, 320
        self.win_w, self.win_h = self.game_w * self.scale, self.game_h * self.scale
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
        self.pending_tap_ups: dict[str, int] = {}

        log_boot("Calling _init_window")
        self._init_window()
        log_boot("Calling _init_runtime")
        self._init_runtime()
        log_boot("Calling _init_joysticks")
        self._init_joysticks()
        log_boot("PCRunner init complete")

    def _init_window(self):
        pygame.init()
        flags = DOUBLEBUF | OPENGL
        if self.fullscreen:
            flags |= FULLSCREEN
        pygame.display.set_caption("Heroes Lore 3: Stigmata of Gaia Recomp (PC Native)")
        self.window = pygame.display.set_mode((self.win_w, self.win_h), flags)

        # Set official game window icon after video mode initialization
        icon_path = ROOT / "assets" / "icon.png"
        if not icon_path.exists():
            icon_path = ROOT / "reference" / "extracted_ipa" / "Payload" / "Heroes Lore.app" / "Icon.png"
        if icon_path.exists():
            try:
                icon_surf = pygame.image.load(str(icon_path)).convert_alpha()
                icon_32 = pygame.transform.smoothscale(icon_surf, (32, 32))
                pygame.display.set_icon(icon_32)
            except Exception as e:
                print(f"[HL3 PC] Could not set window icon: {e}")

    def get_engine_state(self) -> dict:
        if not hasattr(self, "rt") or not self.rt or not hasattr(self.rt, "m"):
            return {
                "app_state": 0, "sub_state": 0, "form_id": 0, "main_popup_id": 0,
                "hud_popup_id": 0, "active_popup_id": 0, "can_talk": 0, "combat_mode": 0
            }
        m = self.rt.m
        try:
            app = m.rd32(0x000d7dd4)
            if not app:
                return {
                    "app_state": 0, "sub_state": 0, "form_id": 0, "main_popup_id": 0,
                    "hud_popup_id": 0, "active_popup_id": 0, "can_talk": 0, "combat_mode": 0
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
            }
        except Exception:
            return {
                "app_state": 0, "sub_state": 0, "form_id": 0, "main_popup_id": 0,
                "hud_popup_id": 0, "active_popup_id": 0, "can_talk": 0, "combat_mode": 0
            }

    def _draw_menu_cursor(self):
        st = self.get_engine_state()
        # Strictly draw the cursor overlay ONLY in the main Pause Menu (app_state == 8 and form_id == 2)
        if st.get("app_state") != 8 or st.get("form_id") != 2:
            return
        try:
            vx, vy, vw, vh = self.rt.gl_bridge.viewport_rect if hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge else (0, 0, self.win_w, self.win_h)
            glPushAttrib(GL_ALL_ATTRIB_BITS)
            glViewport(vx, vy, vw, vh)
            glMatrixMode(GL_PROJECTION)
            glPushMatrix()
            glLoadIdentity()
            glOrtho(0.0, 480.0, 320.0, 0.0, -1.0, 1.0)
            glMatrixMode(GL_MODELVIEW)
            glPushMatrix()
            glLoadIdentity()

            glDisable(GL_TEXTURE_2D)
            glEnable(GL_BLEND)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

            y = PAUSE_MENU_ITEMS_Y[self.menu_index]

            # Golden glowing selection frame around option (x: 82 to 398)
            glColor4f(1.0, 0.85, 0.2, 0.9)
            glLineWidth(2.5)
            glBegin(GL_LINE_LOOP)
            glVertex2f(82.0, y - 18.0)
            glVertex2f(398.0, y - 18.0)
            glVertex2f(398.0, y + 18.0)
            glVertex2f(82.0, y + 18.0)
            glEnd()

            # Left pointer arrow ▶
            glColor4f(1.0, 0.9, 0.3, 1.0)
            glBegin(GL_TRIANGLES)
            glVertex2f(66.0, y - 9.0)
            glVertex2f(77.0, y)
            glVertex2f(66.0, y + 9.0)
            glEnd()

            # Right pointer arrow ◀
            glBegin(GL_TRIANGLES)
            glVertex2f(414.0, y - 9.0)
            glVertex2f(403.0, y)
            glVertex2f(414.0, y + 9.0)
            glEnd()

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
        self.rt.gl_bridge = GLBridge(self.rt.m)
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
                self.tap_once("popup_confirm", POPUP_CONFIRM, hold_frames=2)
            return

        # 2. Pause Menu / Submenus (app_state == 8)
        if app_state == 8:
            if form_id == 2:
                y = PAUSE_MENU_ITEMS_Y[self.menu_index]
                self.tap_once("pause_opt", (240.0, y), hold_frames=2)
            else:
                self.tap_once("sub_confirm", POPUP_CONFIRM, hold_frames=2)
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
                self.tap_once("menu_confirm", POPUP_CONFIRM, hold_frames=2)
            return

        # 4. Gameplay & Cutscenes (app_state == 2)
        if app_state == 2:
            if sub_state in (4, 22):
                # Dialogue / Cutscene: advance text
                self.tap_once("dialogue_adv", DIALOGUE_ADVANCE, hold_frames=2)
                return
            if sub_state == 7:
                # System message: dismiss
                self.tap_once("msg_dismiss", DIALOGUE_ADVANCE, hold_frames=2)
                return
            if sub_state == 3:
                # In field: if NPC talk prompt is active, talk!
                if st["can_talk"]:
                    if "dpad" in self.active_touches:
                        self.touch_up("dpad")
                    talk_pos = HUD_TALK_COMBAT if st["combat_mode"] else HUD_TALK_PEACEFUL
                    self.tap_once("hud_talk", talk_pos, hold_frames=2)
                elif st["combat_mode"]:
                    self.tap_once("hud_attack", HUD_ATTACK_PRIMARY, hold_frames=2)
                else:
                    self.tap_once("hud_interact", HUD_TALK_PEACEFUL, hold_frames=2)
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
                self.tap_once("popup_cancel", POPUP_CANCEL, hold_frames=2)
            return

        # 2. Pause Menu / Submenus
        if app_state == 8:
            self.tap_once("pause_back", PAUSE_MENU_BACK, hold_frames=2)
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
                self.tap_once("msg_dismiss", DIALOGUE_ADVANCE, hold_frames=2)
                return
            if sub_state == 3:
                # In field: open Pause Menu
                self.tap_once("open_pause", HUD_PORTRAIT_MENU, hold_frames=2)
                return

    def handle_skip(self):
        """Skip cutscenes or confirm skip."""
        st = self.get_engine_state()
        if st.get("active_popup_id") == 133:
            # Skip confirmation popup: Yes!
            self.tap_once("skip_yes", POPUP_CONFIRM, hold_frames=2)
            return
        if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
            self.tap_once("cutscene_skip", CUTSCENE_SKIP, hold_frames=2)

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
        if app_state == 8 and form_id == 2:
            if dir_name in ("up", "left"):
                self.menu_index = (self.menu_index - 1) % len(PAUSE_MENU_ITEMS_Y)
            elif dir_name in ("down", "right"):
                self.menu_index = (self.menu_index + 1) % len(PAUSE_MENU_ITEMS_Y)
            return

    # ------------------------------------------------------------- Input poll
    def poll_input(self):
        if self.carousel_cooldown > 0:
            self.carousel_cooldown -= 1
        if self.nav_cooldown > 0:
            self.nav_cooldown -= 1

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
                if event.key == K_F11:
                    self.fullscreen = not self.fullscreen
                    flags = DOUBLEBUF | OPENGL | (FULLSCREEN if self.fullscreen else 0)
                    self.window = pygame.display.set_mode((self.win_w, self.win_h), flags)
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
                # Map / Minimap: M or Tab (in field gameplay)
                elif event.key in (K_m, K_TAB):
                    st = self.get_engine_state()
                    if st.get("app_state") == 2 and st.get("sub_state") == 3 and not st.get("active_popup_id"):
                        self.tap_once("hud_map", HUD_MAP, hold_frames=3)
                    elif st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    elif event.key == K_m:
                        if hasattr(self.rt, "audio_mixer"):
                            self.rt.audio_mixer.toggle_mute()
                            print(f"[HL3 Audio] Muted: {self.rt.audio_mixer.muted}")

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

                # Skip cutscene: S
                elif event.key == K_s:
                    st = self.get_engine_state()
                    if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    elif st.get("app_state") == 1:
                        self.handle_nav("down")

                # Menu navigation arrows
                elif event.key in (K_UP, K_w):
                    self.handle_nav("up")
                elif event.key in (K_DOWN, K_s):
                    self.handle_nav("down")
                elif event.key in (K_LEFT, K_a):
                    self.handle_nav("left")
                elif event.key in (K_RIGHT, K_d):
                    self.handle_nav("right")

                # Quick Potions during gameplay
                elif event.key in (K_q, K_1):
                    # Upper Potion Slot (Potion 1)
                    self.tap_once("potion1", HUD_POTION_1, hold_frames=3)
                elif event.key in (K_k, K_2):
                    # Lower Potion Slot (Potion 2)
                    self.tap_once("potion2", HUD_POTION_2, hold_frames=3)

                # Combat & Skills during gameplay
                elif event.key in (K_u, K_3):
                    self.tap_once("skill1", HUD_SKILL_1, hold_frames=2)
                elif event.key in (K_i, K_4):
                    self.tap_once("skill2", HUD_SKILL_2, hold_frames=2)
                elif event.key in (K_o, K_5):
                    self.tap_once("skill3", HUD_SKILL_3, hold_frames=2)
                elif event.key == K_j:
                    self.tap_once("attack1", HUD_ATTACK_PRIMARY, hold_frames=2)
                elif event.key in (K_l, K_h):
                    self.tap_once("attack2", HUD_ATTACK_SECONDARY, hold_frames=2)
                elif event.key in (K_p, K_c):
                    self.tap_once("portrait_menu", HUD_PORTRAIT_MENU, hold_frames=3)

            # Gamepad Button Events
            elif event.type == JOYBUTTONDOWN:
                btn = event.button
                if btn == 0:  # A (Confirm / Talk)
                    self.handle_confirm()
                elif btn == 1:  # B (Cancel / Back)
                    self.handle_cancel()
                elif btn == 2:  # X (Attack Primary / Skill 1)
                    st = self.get_engine_state()
                    if st.get("combat_mode"):
                        self.tap_once("pad_attack", HUD_ATTACK_PRIMARY, hold_frames=2)
                    else:
                        self.tap_once("skill1", HUD_SKILL_1, hold_frames=2)
                elif btn == 3:  # Y (Secondary Attack / Skip / Skill 2)
                    st = self.get_engine_state()
                    if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    else:
                        self.tap_once("attack2", HUD_ATTACK_SECONDARY, hold_frames=2)
                elif btn == 4:  # LB (Upper Potion 1)
                    self.tap_once("potion1", HUD_POTION_1, hold_frames=3)
                elif btn == 5:  # RB (Lower Potion 2)
                    self.tap_once("potion2", HUD_POTION_2, hold_frames=3)
                elif btn == 6:  # Select (Minimap / Map)
                    self.tap_once("hud_map", HUD_MAP, hold_frames=3)
                elif btn == 7:  # Start (Pause Menu / Skip)
                    st = self.get_engine_state()
                    if st.get("sub_state") in (4, 22) or st.get("form_id") == 44:
                        self.handle_skip()
                    elif st.get("app_state") == 2:
                        self.tap_once("open_pause", HUD_PORTRAIT_MENU, hold_frames=3)
                    elif st.get("app_state") == 8:
                        self.handle_cancel()
                        self.tap_once("open_pause", HUD_PORTRAIT_MENU, hold_frames=2)
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
                target_x = DPAD_CENTER[0] + nx * 30.0
                target_y = DPAD_CENTER[1] + ny * 30.0

                if "dpad" not in self.active_touches:
                    self.touch_down("dpad", DPAD_CENTER)
                self.touch_move("dpad", (target_x, target_y))
            else:
                if "dpad" in self.active_touches:
                    self.touch_up("dpad")
        else:
            if "dpad" in self.active_touches:
                self.touch_up("dpad")

    def capture_screenshot(self) -> Path:
        pixels = glReadPixels(0, 0, self.win_w, self.win_h, GL_RGBA, GL_UNSIGNED_BYTE)
        img = Image.frombytes("RGBA", (self.win_w, self.win_h), pixels).transpose(Image.FLIP_TOP_BOTTOM)
        out_dir = ROOT / "reference" / "analysis"
        out_dir.mkdir(parents=True, exist_ok=True)
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = out_dir / f"screenshot_{ts}.png"
        img.save(out_path)
        print(f"[HL3 PC] Screenshot saved to: {out_path.relative_to(ROOT)}")
        return out_path

    def run(self):
        self.running = True
        frame = 0
        print("[HL3 PC] Main loop started (60 FPS). Press F12 for screenshot, Escape to exit or close window.")
        while self.running:
            self.poll_input()
            if hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
                self.rt.gl_bridge.begin_frame()
            self.rt.send(self.delegate, "onTimer:", 0)
            self.clock.tick(60)
            frame += 1
            if self.headless_frames > 0 and frame >= self.headless_frames:
                print(f"[HL3 PC] Reached target {self.headless_frames} frames in test mode.")
                self.capture_screenshot()
                break

        print("[HL3 PC] Shutting down...")
        if hasattr(self.rt, "audio_mixer"):
            self.rt.audio_mixer.save_config()
        pygame.quit()


def main():
    try:
        parser = argparse.ArgumentParser(description="Heroes Lore 3 PC Native Host Runner")
        parser.add_argument("--scale", type=int, default=2, help="Window scale factor (default: 2 -> 960x640)")
        parser.add_argument("--fullscreen", action="store_true", help="Launch in fullscreen mode")
        parser.add_argument("--frames", type=int, default=0, help="Run for N frames then exit (automated testing)")
        args = parser.parse_args()

        runner = PCRunner(scale=args.scale, fullscreen=args.fullscreen, headless_frames=args.frames)
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
