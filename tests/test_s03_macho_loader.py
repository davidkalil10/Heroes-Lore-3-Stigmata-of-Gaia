"""Automated tests for S03: Mach-O ARMv6 loader, Objective-C dispatch and headless boot."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.runtime import Runtime  # noqa: E402


class S03MachoLoaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rt = Runtime()
        cls.rt.boot_to_app_main()
        cls.rt.run_launch()

    def test_boot_to_app_main(self):
        self.assertIsNotNone(self.rt.pending_app_main)
        self.assertEqual(self.rt.pending_app_main["delegate"], "AppDelegate")

    def test_launch_instantiated_core_objects(self):
        delegate = self.rt.state.get("delegate")
        self.assertIsNotNone(delegate)
        cls, _ = self.rt.objc.class_of(delegate)
        self.assertEqual(cls.name, "AppDelegate")

        # Verify TouchResponder and Window exist in runtime state
        touch_resp = None
        for k, obj in self.rt.objc.hobj.items():
            c, _ = self.rt.objc.class_of(k)
            if c and c.name == "TouchResponder":
                touch_resp = k
                break
        self.assertIsNotNone(touch_resp, "TouchResponder should be instantiated during launch")

    def test_frame_rendering_via_timer(self):
        delegate = self.rt.state["delegate"]
        initial_frames = self.rt.state.get("frames_presented", 0) or 0
        for _ in range(3):
            self.rt.send(delegate, "onTimer:", 0)
        current_frames = self.rt.state.get("frames_presented", 0) or 0
        self.assertEqual(current_frames - initial_frames, 3)

    def test_synthetic_touch_event(self):
        touch_resp = None
        for k, obj in self.rt.objc.hobj.items():
            c, _ = self.rt.objc.class_of(k)
            if c and c.name == "TouchResponder":
                touch_resp = k
                break
        self.assertIsNotNone(touch_resp)
        tset = self.rt.objc.new_obj("NSSet", [])
        evt = self.rt.objc.new_obj("UIEvent", {})
        # Should execute without throwing GuestFault or CPU exceptions
        self.rt.send(touch_resp, "touchesBegan:withEvent:", tset, evt)


if __name__ == "__main__":
    unittest.main()

