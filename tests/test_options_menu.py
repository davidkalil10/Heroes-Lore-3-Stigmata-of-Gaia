"""Automated tests for Port Options UI, responsive layout scaling, i18n, and dual menu integration."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

import pygame

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.cloud_save import CloudSaveManager, HttpResponse
from hl3rt.display import load_display_config, save_display_config
from hl3rt.i18n import get_active_language, normalize_language, set_active_language, t
from hl3rt.options_menu import ModalPage, PortOptionsModal, compute_modal_layout
from run_pc import PCRunner


class HL3OptionsMenuTests(unittest.TestCase):
    def test_i18n_default_and_live_switch(self):
        set_active_language("en")
        self.assertEqual(get_active_language(), "en")
        self.assertEqual(normalize_language("PT_br"), "pt-BR")
        self.assertEqual(t("options.title", "en"), "PORT OPTIONS & CLOUD")
        self.assertEqual(t("options.title", "pt-BR"), "OPÇÕES DO PORT & CLOUD")

        with tempfile.TemporaryDirectory() as tmp:
            cfg_file = Path(tmp) / "display.json"
            save_display_config({"aspect_mode": "16:9", "target_fps": 60, "language": "pt-BR"}, cfg_file)
            loaded = load_display_config(cfg_file)
            self.assertEqual(loaded, {"aspect_mode": "16:9", "target_fps": 60, "language": "pt-BR"})

    def test_responsive_layout_desktop_wide_and_mobile_portrait(self):
        viewports = [
            (480, 320),    # Compact 3:2 small window
            (960, 640),    # Default PC window
            (1536, 793),   # Maximized wide PC desktop
            (480, 800),    # Mobile Portrait (WVGA)
            (1080, 2400),  # High-DPI Mobile Portrait (20:9 phone)
        ]
        for vw, vh in viewports:
            for page in (ModalPage.OPTIONS, ModalPage.CLOUD, ModalPage.UPDATE, ModalPage.CONFIRM_RESTORE):
                lay = compute_modal_layout(vw, vh, page)
                # Panel stays inside viewport with clean margins
                self.assertGreater(lay.panel.width, 200, f"{vw}x{vh} {page}")
                self.assertGreater(lay.panel.height, 150, f"{vw}x{vh} {page}")
                self.assertLessEqual(lay.panel.right, vw, f"{vw}x{vh} {page}")
                self.assertLessEqual(lay.panel.bottom, vh, f"{vw}x{vh} {page}")
                self.assertGreaterEqual(lay.panel.x, 0)
                self.assertGreaterEqual(lay.panel.y, 0)

                if page == ModalPage.OPTIONS:
                    self.assertEqual(len(lay.option_rows), 6)
                    for i, rc in enumerate(lay.option_rows):
                        self.assertTrue(lay.panel.contains(rc), f"row {i} outside panel on {vw}x{vh}")
                    self.assertTrue(lay.panel.contains(lay.back_rect))
                elif page == ModalPage.UPDATE:
                    self.assertTrue(lay.panel.contains(lay.remote_card), f"update info outside panel on {vw}x{vh}")
                    self.assertTrue(lay.panel.contains(lay.local_card), f"update notes outside panel on {vw}x{vh}")
                    self.assertTrue(lay.panel.contains(lay.status_bar), f"update status outside panel on {vw}x{vh}")
                    for i, b1 in enumerate(lay.action_buttons):
                        self.assertTrue(lay.panel.contains(b1), f"update btn {i} outside panel on {vw}x{vh}")
                        for j, b2 in enumerate(lay.action_buttons):
                            if i != j:
                                self.assertFalse(b1.colliderect(b2), f"update btns {i} & {j} overlap on {vw}x{vh}")
                elif page == ModalPage.CLOUD:
                    # Landscape desktop keeps cards side-by-side & 4 action buttons in 1 row
                    # Portrait stacks cards vertically & uses 2x2 tactile button grid
                    if vh > vw:
                        self.assertTrue(lay.is_portrait)
                        self.assertGreater(lay.local_card.y, lay.remote_card.y)
                        self.assertGreater(lay.action_buttons[2].y, lay.action_buttons[0].y)
                        # Proportional scaling ensures mobile fonts/buttons stay large & touchable
                        if vw >= 1000:
                            self.assertGreaterEqual(lay.btn_h, 90)
                            self.assertGreaterEqual(lay.font_label_sz, 24)
                    else:
                        self.assertFalse(lay.is_portrait)
                        if vw >= 700:
                            self.assertGreater(lay.local_card.x, lay.remote_card.x)
                            self.assertEqual(lay.action_buttons[0].y, lay.action_buttons[3].y)
                    # Action buttons must never overlap each other
                    for i, b1 in enumerate(lay.action_buttons):
                        self.assertTrue(lay.panel.contains(b1), f"btn {i} outside panel on {vw}x{vh}")
                        for j, b2 in enumerate(lay.action_buttons):
                            if i != j:
                                self.assertFalse(b1.colliderect(b2), f"btn {i} & {j} overlap on {vw}x{vh}")

    def test_modal_keyboard_mouse_and_restore_callback(self):
        pygame.init()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data_root = root / "data"
            docs = data_root / "internal" / "Documents"
            docs.mkdir(parents=True)
            (docs / "Hero3GameSave_10_3").write_bytes(b"SAV")
            auth_path = root / "config" / "cloud_auth.json"
            auth_path.parent.mkdir(parents=True, exist_ok=True)
            auth_path.write_text(json.dumps({"refresh_token": "RT", "access_token": "AT", "expires_at": 9999999999}), encoding="utf-8")

            def dummy_http(method, url, headers, body, timeout):
                return HttpResponse(200, json.dumps({"files": []}))

            cloud = CloudSaveManager(data_root=data_root, auth_path=auth_path, http_fn=dummy_http)
            state = {"fps": 30, "aspect": "auto", "lang": "en", "reloaded": 0}

            def set_lang(v):
                state["lang"] = v
                set_active_language(v)

            modal = PortOptionsModal(
                cloud=cloud,
                get_fps=lambda: state["fps"],
                set_fps=lambda v: state.__setitem__("fps", v),
                get_aspect=lambda: state["aspect"],
                set_aspect=lambda v: state.__setitem__("aspect", v),
                get_lang=lambda: state["lang"],
                set_lang=set_lang,
                on_restore_applied=lambda: state.__setitem__("reloaded", state["reloaded"] + 1),
            )
            modal.open(ModalPage.OPTIONS)
            # Row 0 = FPS
            modal.handle_key(pygame.K_RIGHT)
            self.assertEqual(state["fps"], 60)
            # Row 1 = Aspect Ratio
            modal.handle_key(pygame.K_DOWN)
            modal.handle_key(pygame.K_RETURN)
            self.assertEqual(state["aspect"], "16:9")
            # Row 2 = Language EN -> PT-BR live
            modal.handle_key(pygame.K_DOWN)
            modal.handle_key(pygame.K_RIGHT)
            self.assertEqual(state["lang"], "pt-BR")
            self.assertEqual(get_active_language(), "pt-BR")
            # Row 3 = Open Cloud page
            modal.handle_key(pygame.K_DOWN)
            modal.handle_key(pygame.K_RETURN)
            cloud.wait_idle(timeout=2.0)
            self.assertEqual(modal.page, ModalPage.CLOUD)
            # Render surface smoke check in both portrait mobile and desktop landscape
            s_pc = modal._render_surface(960, 640, state["lang"])
            self.assertEqual(s_pc.get_size(), (960, 640))
            s_mob = modal._render_surface(480, 800, state["lang"])
            self.assertEqual(s_mob.get_size(), (480, 800))
            # Esc from Cloud -> Options -> Closed
            modal.handle_key(pygame.K_ESCAPE)
            self.assertEqual(modal.page, ModalPage.OPTIONS)
            modal.handle_key(pygame.K_ESCAPE)
            self.assertFalse(modal.is_open)

            # Simulate pending restore ready -> closes modal and triggers runtime reload callback
            modal.open(ModalPage.CLOUD)
            cloud._pending_restore = {"internal/Documents/Hero3GameSave_10_3": b"CLOUD_RESTORED"}
            cloud._update_status(restore_ready=True, busy=False)
            modal.poll()
            self.assertEqual(state["reloaded"], 1)
            self.assertFalse(modal.is_open)
            self.assertEqual((docs / "Hero3GameSave_10_3").read_bytes(), b"CLOUD_RESTORED")

    def test_runner_injects_dual_port_buttons_without_leaking_touch(self):
        r = PCRunner.__new__(PCRunner)
        r._ensure_focus_attrs()
        r._ensure_port_modal()
        r.language = "en"
        r.active_touches = {}
        r.pending_tap_ups = {}
        r.touch_responder = 0
        r.joysticks = []
        r._patch_help_about_layout = lambda ptr: None

        # Pre-game Help & About (form_id=113) exposes cid=99113 right below HELP
        r._collect_subtree_controls = lambda ptr: (
            [{"ptr": 1, "ctype": 2, "cid": 11306, "vis": 3, "pos": (243, 213, -1, -1), "frect": (93.0, 393.0, 198.0, 228.0), "cx": 243.0, "cy": 213.0, "kind": "button"}],
            [],
        )
        ctx1, c1, _ = r.get_focusable_controls({
            "app_state": 1, "form_id": 113, "main_popup_id": 0, "hud_popup_id": 0,
            "active_popup_id": 0, "main_form": 1234, "main_popup": 0, "hud_popup": 0,
        })
        self.assertEqual(ctx1, ("pregame", 113, 0))
        self.assertEqual([c["cid"] for c in c1], [11306, 99113])
        self.assertEqual(c1[-1]["frect"], (93.0, 393.0, 234.0, 264.0))

        # In-game System (form_id=3) exposes cid=99003 right below MAIN MENU
        r._collect_subtree_controls = lambda ptr: (
            [{"ptr": 2, "ctype": 2, "cid": 305, "vis": 3, "pos": (240, 221, -1, -1), "frect": (90.0, 390.0, 206.0, 236.0), "cx": 240.0, "cy": 221.0, "kind": "button"}],
            [],
        )
        ctx2, c2, _ = r.get_focusable_controls({
            "app_state": 8, "form_id": 3, "main_popup_id": 0, "hud_popup_id": 0,
            "active_popup_id": 0, "main_form": 5678, "main_popup": 0, "hud_popup": 0,
        })
        self.assertEqual(ctx2, ("submenu", 3, 0))
        self.assertEqual([c["cid"] for c in c2], [305, 99003])
        self.assertEqual(c2[-1]["frect"], (90.0, 390.0, 256.0, 286.0))


if __name__ == "__main__":
    unittest.main()
