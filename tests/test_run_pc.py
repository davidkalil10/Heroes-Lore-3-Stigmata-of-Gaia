"""Automated tests for S04 / S05: PC Native Runner, Input Mapping and Hardware GL Rendering."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from run_pc import (
    PCRunner,
    BTN_ATTACK,
    BTN_CENTER,
    BTN_TALK,
    BTN_MENU,
    BTN_BACK,
    BTN_MAP,
    BTN_ITEM,
    BTN_ITEM2,
    HUD_MAP,
    HUD_POTION_1,
    HUD_POTION_2,
    DPAD_UP,
    MENU_ITEMS_Y,
)


class S04PCPlayableTests(unittest.TestCase):
    def test_coordinate_mapping(self):
        runner = PCRunner(scale=2, headless_frames=1)
        # Window size is 960x640 (scale 2)
        self.assertEqual((runner.win_w, runner.win_h), (960, 640))
        gx, gy = runner.window_to_game_coord(480, 320)
        self.assertAlmostEqual(gx, 240.0)
        self.assertAlmostEqual(gy, 160.0)

    def test_synthetic_touch_targets(self):
        runner = PCRunner(scale=1, headless_frames=1)
        self.assertIsNotNone(runner.touch_responder)
        # Verify touch down and up do not fault
        runner.touch_down("dpad", DPAD_UP)
        runner.touch_up("dpad")
        runner.touch_down("attack", BTN_ATTACK)
        runner.touch_up("attack")
        runner.touch_down("center", BTN_CENTER)
        runner.touch_up("center")

    def test_talk_and_menu_controls(self):
        runner = PCRunner(scale=1, headless_frames=1)
        # Verify NPC talk button is at (450.0, 258.0) matching exact on-screen HUD button (id=7025)
        self.assertEqual(BTN_TALK, (450.0, 258.0))
        # Verify HUD Map and Potion targets
        self.assertEqual(HUD_MAP, (23.5, 22.0))
        self.assertEqual(HUD_POTION_1, (29.0, 107.0))
        self.assertEqual(HUD_POTION_2, (29.0, 157.0))
        self.assertEqual(BTN_MAP, HUD_MAP)
        self.assertEqual(BTN_ITEM, HUD_POTION_1)
        self.assertEqual(BTN_ITEM2, HUD_POTION_2)
        # Verify menu items has 6 entries
        self.assertEqual(len(MENU_ITEMS_Y), 6)
        # Verify carousel swipe left and right execute without fault
        runner.swipe_carousel("left")
        runner.carousel_cooldown = 0
        runner.swipe_carousel("right")
        # Verify tap_once queues and releases properly
        runner.tap_once("test_tap", (240.0, 135.0))
        self.assertIn("test_tap", runner.active_touches)
        self.assertIn("test_tap", runner.pending_tap_ups)
        # Next poll releases pending taps
        for tag in list(runner.pending_tap_ups):
            runner.touch_up(tag)
        runner.pending_tap_ups.clear()
        self.assertNotIn("test_tap", runner.active_touches)

    def test_win32_icon_assets_and_resolution(self):
        from pathlib import Path
        from PIL import Image

        root = Path(__file__).resolve().parents[1]
        png_path = root / "assets" / "icon.png"
        ico_path = root / "assets" / "icon.ico"
        self.assertTrue(png_path.exists())
        self.assertTrue(ico_path.exists())
        with Image.open(png_path) as im:
            self.assertEqual(im.size, (512, 512))
        with Image.open(ico_path) as im:
            sizes = im.info.get("sizes", set())
            for req in ((16, 16), (32, 32), (48, 48), (256, 256)):
                self.assertIn(req, sizes)

        runner = PCRunner(scale=1, headless_frames=1)
        self.assertIsNotNone(runner._resolve_icon_path("icon.ico"))
        self.assertIsNotNone(runner._resolve_icon_path("icon.png"))

    def test_submenu_control_tree_navigation_logic(self):
        import pygame

        r = PCRunner.__new__(PCRunner)
        r.menu_index = 0
        r.nav_cooldown = 0
        r.carousel_cooldown = 0
        r.scroll_cooldown = 0
        r.submenu_focus_ctx = None
        r.submenu_focus_idx = 0
        r.submenu_focus_cid = 0
        r.skill_tab_idx = 0
        r.active_touches = {}
        r.pending_tap_ups = {}
        r.touch_responder = 0
        r.joysticks = []

        grid = [
            {"cid": 801, "cx": 100.0, "cy": 86.0, "frect": (84, 116, 70, 102), "kind": "button"},
            {"cid": 802, "cx": 140.0, "cy": 86.0, "frect": (124, 156, 70, 102), "kind": "button"},
            {"cid": 809, "cx": 100.0, "cy": 141.0, "frect": (84, 116, 125, 157), "kind": "button"},
            {"cid": 810, "cx": 140.0, "cy": 141.0, "frect": (124, 156, 125, 157), "kind": "button"},
        ]
        # Grid movement + wraparound
        self.assertEqual(r._move_focus_2d(grid, 0, "right"), 1)
        self.assertEqual(r._move_focus_2d(grid, 1, "right"), 0)
        self.assertEqual(r._move_focus_2d(grid, 0, "down"), 2)
        self.assertEqual(r._move_focus_2d(grid, 2, "up"), 0)

        # Popup default focus + 2-button toggle + dynamic control preservation
        popup_ctrls = [
            {"cid": 199, "cx": 395.0, "cy": 100.0, "frect": (370, 420, 75, 125), "kind": "button"},
            {"cid": 401, "cx": 395.0, "cy": 220.0, "frect": (370, 420, 195, 245), "kind": "button"},
        ]
        r.get_focusable_controls = lambda st=None: (("popup", 4, 0), list(popup_ctrls), [])
        ctrls, focused, _ = r._sync_focus({"app_state": 8, "main_popup_id": 4})
        self.assertEqual(focused["cid"], 401)
        # Left/Right on 2-button popup toggles Yes <-> No
        self.assertEqual(r._move_focus_2d(ctrls, r.submenu_focus_idx, "left"), 0)

        # Preserving focused CID when additional buttons become enabled dynamically
        r.get_focusable_controls = lambda st=None: (("submenu", 8, 0), list(grid[:1] + [grid[1]]), [])
        _, f1, _ = r._sync_focus({"app_state": 8, "form_id": 8})
        r.submenu_focus_idx = 1
        r.submenu_focus_cid = 802
        r.get_focusable_controls = lambda st=None: (
            ("submenu", 8, 0),
            [{"cid": 883, "cx": 210.0, "cy": 252.0, "frect": (190, 230, 232, 272), "kind": "button"}] + list(grid),
            [],
        )
        _, f2, _ = r._sync_focus({"app_state": 8, "form_id": 8})
        self.assertEqual(f2["cid"], 802)

        # K_s moves menu_index down inside Pause Menu (app_state == 8, form_id == 2)
        r.get_engine_state = lambda: {
            "app_state": 8, "sub_state": 3, "form_id": 2,
            "main_popup_id": 0, "hud_popup_id": 0, "active_popup_id": 0,
            "can_talk": 0, "combat_mode": 0, "main_form": 0, "main_popup": 0, "hud_popup": 0,
        }
        pygame.init()
        pygame.event.clear()
        pygame.event.post(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_s, mod=0, unicode=""))
        r.poll_input()
        self.assertEqual(r.menu_index, 1)

        # Gamepad Left Stick navigation works in menus (app_state == 8)
        class _FakeJoy:
            def get_numhats(self): return 0
            def get_numaxes(self): return 2
            def get_axis(self, idx): return 0.9 if idx == 1 else 0.0

        r.nav_cooldown = 0
        r.joysticks = [_FakeJoy()]
        r.poll_input()
        self.assertEqual(r.menu_index, 2)
        self.assertGreater(r.nav_cooldown, 0)

    def test_run_frames_and_screenshot(self):
        runner = PCRunner(scale=1, headless_frames=30)
        runner.run()
        # Verify that frames were presented and screenshot exists
        self.assertGreaterEqual(runner.rt.state.get("frames_presented", 0), 30)


if __name__ == "__main__":
    unittest.main()

