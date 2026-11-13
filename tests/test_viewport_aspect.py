"""Unit tests for S08: Configurable FPS, Display Config, and True Widescreen (original / 16:9 / auto)."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.display import (  # noqa: E402
    compute_viewport_layout,
    load_display_config,
    normalize_aspect_mode,
    normalize_fps,
    save_display_config,
)
from hl3rt.gl_bridge import GLBridge  # noqa: E402
from hl3rt.widescreen import encode_arm_rot_imm  # noqa: E402
from run_pc import (  # noqa: E402
    BTN_ATTACK,
    BTN_CENTER,
    HUD_MAP,
    HUD_POTION_1,
    PCRunner,
)


class S08ViewportAndFPSUnitTests(unittest.TestCase):
    def test_fps_normalization(self):
        self.assertEqual(normalize_fps(30), 30)
        self.assertEqual(normalize_fps(60), 60)
        self.assertEqual(normalize_fps("60"), 60)
        self.assertEqual(normalize_fps("30"), 30)
        self.assertEqual(normalize_fps(None), 30)

    def test_aspect_normalization(self):
        self.assertEqual(normalize_aspect_mode("original"), "original")
        self.assertEqual(normalize_aspect_mode("3:2"), "original")
        self.assertEqual(normalize_aspect_mode("16:9"), "16:9")
        self.assertEqual(normalize_aspect_mode("auto"), "auto")

    def test_display_config_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "display.json"
            save_display_config({"aspect_mode": "16:9", "target_fps": 60}, p)
            loaded = load_display_config(p)
            self.assertEqual(loaded["aspect_mode"], "16:9")
            self.assertEqual(loaded["target_fps"], 60)

    def test_arm_rot_imm_encoding_for_all_mult4(self):
        for v in range(320, 1024, 4):
            enc = encode_arm_rot_imm(v)
            rot = ((enc >> 8) & 0xF) * 2
            imm8 = enc & 0xFF
            decoded = ((imm8 >> rot) | (imm8 << (32 - rot))) & 0xFFFFFFFF if rot else imm8
            self.assertEqual(decoded, v)

    def test_viewport_layout_modes_no_stretch(self):
        # 1. Original (3:2) in 1280x720 -> pillarboxed 1080x720 at x=100, logical 480x320
        orig = compute_viewport_layout("original", 1280, 720, world_active=True)
        self.assertEqual((orig.ortho_w, orig.ortho_h), (480.0, 320.0))
        self.assertEqual((orig.patch_w, orig.patch_h), (480, 320))
        self.assertEqual(orig.viewport_rect, (100, 0, 1080, 720))
        self.assertFalse(orig.true_widescreen)

        # 2. 16:9 in 1280x720 -> full window, true widescreen, zero distortion
        w169 = compute_viewport_layout("16:9", 1280, 720, world_active=True)
        self.assertTrue(w169.true_widescreen)
        self.assertEqual(w169.viewport_rect, (0, 0, 1280, 720))
        self.assertEqual(w169.ortho_h, 320.0)
        self.assertAlmostEqual(w169.ortho_w / w169.ortho_h, 1280.0 / 720.0, places=4)
        self.assertEqual(w169.patch_w, 572)  # ceil mult4 >= 568.89

        # 3. Auto in 1024x768 (4:3) -> vertical expansion 480x360, full window, zero distortion
        auto_43 = compute_viewport_layout("auto", 1024, 768, world_active=True)
        self.assertTrue(auto_43.true_widescreen)
        self.assertEqual(auto_43.viewport_rect, (0, 0, 1024, 768))
        self.assertAlmostEqual(auto_43.ortho_w, 480.0, places=3)
        self.assertAlmostEqual(auto_43.ortho_h, 360.0, places=3)
        self.assertEqual((auto_43.patch_w, auto_43.patch_h), (480, 360))
        self.assertGreater(auto_43.hud_dy_bottom, 0)

        # 4. Auto in 1440x640 (9:4 / ~20:9) -> horizontal expansion 720x320, full window
        auto_wide = compute_viewport_layout("auto", 1440, 640, world_active=True)
        self.assertTrue(auto_wide.true_widescreen)
        self.assertEqual(auto_wide.viewport_rect, (0, 0, 1440, 640))
        self.assertAlmostEqual(auto_wide.ortho_w, 720.0, places=3)
        self.assertEqual(auto_wide.patch_w, 720)

        # 5. Non-world artwork screen (Title/Char select) stays centered 480x320 even under 16:9
        title_169 = compute_viewport_layout("16:9", 1280, 720, world_active=False)
        self.assertFalse(title_169.true_widescreen)
        self.assertEqual((title_169.ortho_w, title_169.ortho_h), (480.0, 320.0))
        self.assertEqual(title_169.viewport_rect, (100, 0, 1080, 720))

    def test_gl_bridge_mode_and_world_transitions(self):
        bridge = GLBridge(None, 1280, 720, aspect_mode="original")
        self.assertEqual((bridge.game_w, bridge.game_h), (480.0, 320.0))
        self.assertEqual(bridge.viewport_rect, (100, 0, 1080, 720))

        bridge.set_aspect_mode("16:9")
        self.assertAlmostEqual(bridge.game_w, 568.8888, places=2)
        self.assertEqual(bridge.game_h, 320.0)
        self.assertEqual(bridge.viewport_rect, (0, 0, 1280, 720))
        self.assertTrue(bridge.layout.true_widescreen)

        bridge.set_world_active(False)
        self.assertEqual((bridge.game_w, bridge.game_h), (480.0, 320.0))
        self.assertEqual(bridge.viewport_rect, (100, 0, 1080, 720))
        self.assertFalse(bridge.layout.true_widescreen)

        bridge.set_world_active(True)
        bridge.update_viewport(1024, 768)
        bridge.set_aspect_mode("auto")
        self.assertAlmostEqual(bridge.game_w, 480.0, places=2)
        self.assertAlmostEqual(bridge.game_h, 360.0, places=2)
        self.assertEqual(bridge.viewport_rect, (0, 0, 1024, 768))

    def test_runner_widescreen_coords_offsets_and_hud_pts(self):
        runner = PCRunner(
            scale=2,
            headless_frames=1,
            aspect_mode="16:9",
            target_fps=60,
        )
        self.assertEqual(runner.aspect_mode, "16:9")
        self.assertEqual(runner.target_fps, 60)

        # In boot/title state (world_active=False), even under 16:9 window (1138x640),
        # coordinates map to centered 480x320 viewport and ui_main_offset is (0, 0).
        runner.rt.gl_bridge.set_world_active(False)
        runner.game_w = float(runner.rt.gl_bridge.game_w)
        runner.game_h = float(runner.rt.gl_bridge.game_h)
        self.assertEqual((runner.game_w, runner.game_h), (480.0, 320.0))
        self.assertEqual(runner._ui_main_offset({"app_state": 0}), (0.0, 0.0))

        # When world_active=True under 16:9, viewport expands to ~569x320
        runner.rt.gl_bridge.set_world_active(True)
        runner.game_w = float(runner.rt.gl_bridge.game_w)
        runner.game_h = float(runner.rt.gl_bridge.game_h)
        self.assertGreater(runner.game_w, 560.0)
        gx_center, gy_center = runner.window_to_game_coord(runner.win_w * 0.5, runner.win_h * 0.5)
        self.assertAlmostEqual(gx_center, runner.game_w * 0.5, places=1)
        self.assertAlmostEqual(gy_center, 160.0, places=1)

        dx, dy = runner._ui_main_offset({"app_state": 2})
        self.assertEqual(dx, float(runner.rt.gl_bridge.layout.ui_dx))
        self.assertEqual(dy, 0.0)
        self.assertGreater(dx, 40.0)

        # Left-anchored HUD icons stay on the left edge; right/center shift cleanly
        self.assertEqual(runner._hud_pt(HUD_MAP), HUD_MAP)
        self.assertEqual(runner._hud_pt(HUD_POTION_1), HUD_POTION_1)
        self.assertEqual(
            runner._hud_pt(BTN_CENTER),
            (BTN_CENTER[0] + runner.rt.gl_bridge.layout.hud_dx_center, BTN_CENTER[1]),
        )
        self.assertEqual(
            runner._hud_pt(BTN_ATTACK),
            (BTN_ATTACK[0] + runner.rt.gl_bridge.layout.hud_dx_right, BTN_ATTACK[1]),
        )


if __name__ == "__main__":
    unittest.main()

