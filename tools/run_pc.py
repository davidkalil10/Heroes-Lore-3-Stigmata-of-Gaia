#!/usr/bin/env python3
"""Heroes Lore 3: Stigmata of Gaia - PC Native Host Runner (S04 / S05).

Provides an interactive window with hardware-accelerated OpenGL ES 1.1 rendering,
complete keyboard mapping, full gamepad/controller support, and audio playback.

Controls:
  * Movement: D-Pad / Left Stick or Arrow Keys / WASD
  * Talk / Interact: E / Space / Enter / Gamepad A
  * Attack / Confirm: Gamepad A / J / Space
  * Skill 1: Gamepad X / U / 1
  * Skill 2: Gamepad Y / I / 2
  * Skill 3: Gamepad RB / O / 3
  * Use Potion / Item: Gamepad LB / K / Q
  * Touch to Start / Center Confirm: Gamepad A / Enter / Z
  * Pause / In-game Menu: Gamepad Start / Escape / Tab
  * Character Status: Gamepad Select / P
  * Mouse: Left Click & Drag anywhere for 1:1 direct touch input!
  * Volume Controls: + / = Increase Volume, - Decrease Volume, M Toggle Mute
  * F11: Toggle Fullscreen
  * F12: Capture Screenshot
"""

from __future__ import annotations

import argparse
import datetime as dt
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

# Virtual touch target coordinates (480x320 landscape)
# D-Pad center confirmed at (60.0, 265.0) with radius 45.0 in game engine control table
DPAD_CENTER = (60.0, 265.0)
DPAD_UP = (60.0, 230.0)
DPAD_DOWN = (60.0, 300.0)
DPAD_LEFT = (25.0, 265.0)
DPAD_RIGHT = (95.0, 265.0)
DPAD_UP_LEFT = (35.0, 240.0)
DPAD_UP_RIGHT = (85.0, 240.0)
DPAD_DOWN_LEFT = (35.0, 290.0)
DPAD_DOWN_RIGHT = (85.0, 290.0)

BTN_ATTACK = (420.0, 270.0)
BTN_SKILL1 = (355.0, 270.0)
BTN_SKILL2 = (365.0, 215.0)
BTN_SKILL3 = (420.0, 185.0)
BTN_ITEM = (455.0, 140.0)
BTN_START = (240.0, 280.0)
BTN_CENTER = (240.0, 280.0)
BTN_MENU = (450.0, 25.0)
BTN_STATUS = (50.0, 25.0)


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

    def _init_runtime(self):
        log_boot("_init_runtime: start")
        print("[HL3 PC] Initializing Mach-O ARMv6 runtime and OpenGL bridge...")
        self.rt = Runtime()
        log_boot("_init_runtime: Runtime created")
        self.rt.gl_bridge = GLBridge(self.rt.m)
        log_boot("_init_runtime: GLBridge created")
        self.rt.present_hook = lambda: pygame.display.flip()

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

    def window_to_game_coord(self, wx: int, wy: int) -> tuple[float, float]:
        if hasattr(self.rt, "gl_bridge") and self.rt.gl_bridge:
            return self.rt.gl_bridge.window_to_game_coord(wx, wy)
        gx = max(0.0, min(float(self.game_w), float(wx) / self.win_w * self.game_w))
        gy = max(0.0, min(float(self.game_h), float(wy) / self.win_h * self.game_h))
        return gx, gy

    # ------------------------------------------------------------- Input poll
    def poll_input(self):
        for event in pygame.event.get():
            if event.type == QUIT:
                self.running = False
                return

            # Mouse / Direct Touch
            elif event.type == MOUSEBUTTONDOWN and event.button == 1:
                gx, gy = self.window_to_game_coord(event.pos[0], event.pos[1])
                self.touch_down("mouse", (gx, gy))

            elif event.type == MOUSEBUTTONUP and event.button == 1:
                self.touch_up("mouse")

            elif event.type == MOUSEMOTION and ("mouse" in self.active_touches):
                gx, gy = self.window_to_game_coord(event.pos[0], event.pos[1])
                self.touch_move("mouse", (gx, gy))

            # Keyboard Hotkeys
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
                elif event.key == K_m:
                    if hasattr(self.rt, "audio_mixer"):
                        muted = self.rt.audio_mixer.toggle_mute()
                        print(f"[HL3 Audio] Mute: {'ON' if muted else 'OFF'}")

            # Gamepad Hot-plug
            elif event.type == JOYDEVICEADDED:
                try:
                    js = pygame.joystick.Joystick(event.device_index)
                    js.init()
                    self.joysticks.append(js)
                    print(f"[HL3 PC] Gamepad connected: {js.get_name()}")
                except Exception as e:
                    print(f"[HL3 PC] Gamepad error: {e}")

        # Polling Keyboard states for continuous button / D-Pad mapping
        keys = pygame.key.get_pressed()

        # D-Pad Movement mapping (smooth continuous vector input)
        dx, dy = 0.0, 0.0
        if keys[K_LEFT] or keys[K_a]:
            dx -= 1.0
        if keys[K_RIGHT] or keys[K_d]:
            dx += 1.0
        if keys[K_UP] or keys[K_w]:
            dy -= 1.0
        if keys[K_DOWN] or keys[K_s]:
            dy += 1.0

        # Check Gamepad Analog Stick & D-Pad Hat
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
            # Track normalized facing direction for NPC interaction
            self.facing_x = dx / length
            self.facing_y = dy / length
            # D-Pad stick center is (60.0, 265.0) with radius 35.0 (hitbox is 45.0)
            target_x = 60.0 + nx * 35.0
            target_y = 265.0 + ny * 35.0

            if "dpad" not in self.active_touches:
                self.touch_down("dpad", (60.0, 265.0))
            self.touch_move("dpad", (target_x, target_y))
        else:
            if "dpad" in self.active_touches:
                self.touch_up("dpad")

        # Action Buttons
        def check_btn(tag, coord, k_cond, j_btn_idx):
            active = k_cond
            if not active:
                for js in self.joysticks:
                    if j_btn_idx is not None and js.get_numbuttons() > j_btn_idx:
                        if js.get_button(j_btn_idx):
                            active = True
                            break
            if active:
                self.touch_down(tag, coord)
            else:
                self.touch_up(tag)

        # Talk / Interact Target (NPC or speech bubble in front of player at (240, 160))
        interact_coord = (
            240.0 + self.facing_x * 45.0,
            160.0 + self.facing_y * 45.0 - 15.0,
        )

        # Talk / Interact: E, Enter, Space, J, or Gamepad Button 0 [A]
        # In Heroes Lore 3, talking is initiated by touching the NPC or their speech bubble in the game world.
        interact_active = keys[K_e] or keys[K_RETURN] or keys[K_SPACE] or keys[K_j]
        check_btn("interact", interact_coord, interact_active, 0)

        # Attack / Confirm (J, Space, Gamepad Button 0 [A])
        check_btn("attack", BTN_ATTACK, keys[K_j] or keys[K_SPACE], 0)
        # Center Touch to Start / Dialogue advance (Enter, Z, E)
        check_btn("center", BTN_CENTER, keys[K_RETURN] or keys[K_z] or keys[K_e], None)
        # Skill 1 (U, 1, Gamepad Button 2 [X])
        check_btn("skill1", BTN_SKILL1, keys[K_u] or keys[K_1], 2)
        # Skill 2 (I, 2, Gamepad Button 3 [Y])
        check_btn("skill2", BTN_SKILL2, keys[K_i] or keys[K_2], 3)
        # Skill 3 (O, 3, Gamepad Button 5 [RB])
        check_btn("skill3", BTN_SKILL3, keys[K_o] or keys[K_3], 5)
        # Item (K, Q, Gamepad Button 4 [LB])
        check_btn("item", BTN_ITEM, keys[K_k] or keys[K_q], 4)
        # Menu (Escape, Tab, Gamepad Button 7 [Start] / Button 1 [B])
        check_btn("menu", BTN_MENU, keys[K_ESCAPE] or keys[K_TAB], 7)
        # Status (P, Gamepad Button 6 [Select])
        check_btn("status", BTN_STATUS, keys[K_p], 6)

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
            # Tick one frame of game simulation and rendering
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
        print(f"[HL3 Fatal Error]\n{err}", file=sys.stderr)
        raise


if __name__ == "__main__":
    main()

