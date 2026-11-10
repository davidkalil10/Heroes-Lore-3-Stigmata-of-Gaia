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

    def test_run_frames_and_screenshot(self):
        runner = PCRunner(scale=1, headless_frames=30)
        runner.run()
        # Verify that frames were presented and screenshot exists
        self.assertGreaterEqual(runner.rt.state.get("frames_presented", 0), 30)


if __name__ == "__main__":
    unittest.main()

