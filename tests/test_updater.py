"""Automated tests for versioning, GitHub Releases OTA updater, and About menu port credit."""

from __future__ import annotations

import hashlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile
from pathlib import Path

import pygame

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.cloud_save import CloudSaveManager
from hl3rt.display import DEFAULT_LANGUAGE
from hl3rt.i18n import get_active_language, set_active_language
from hl3rt.options_menu import ModalPage, PortOptionsModal
from hl3rt.updater import (
    UpdateManager,
    UpdatePhase,
    build_windows_updater_script,
    is_newer_version,
    is_valid_release_download_url,
    load_services_prefs,
    parse_latest_release_payload,
    parse_semver,
    save_services_prefs,
)
from hl3rt.version import (
    ASSET_WINDOWS_X64,
    GITHUB_REPOSITORY,
    ORIGINAL_GAME_VERSION,
    PORT_AUTHOR,
    PORT_VERSION,
    PORT_YEAR,
)
from run_pc import PCRunner


class _FakeHTTPResponse:
    def __init__(self, payload_bytes: bytes, status: int = 200, url: str = "https://github.com/ok"):
        self._bio = io.BytesIO(payload_bytes)
        self.status = status
        self._url = url
        self.headers = {"Content-Length": str(len(payload_bytes))}

    def read(self, n: int = -1) -> bytes:
        return self._bio.read(n)

    def geturl(self) -> str:
        return self._url

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        return False


def _make_sample_release_zip(inner_wrapper: bool = True) -> bytes:
    bio = io.BytesIO()
    prefix = "HeroesLore3_PC/" if inner_wrapper else ""
    with zipfile.ZipFile(bio, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(f"{prefix}HeroesLore3.exe", b"MZ_FAKE_NEW_EXE")
        zf.writestr(f"{prefix}tools/hl3rt/version.py", b"PORT_VERSION = '1.1.0'\n")
        zf.writestr(f"{prefix}config/display.json", b"{}\n")
    return bio.getvalue()


class HL3UpdaterTests(unittest.TestCase):
    def test_version_sync_check_script_passes(self):
        self.assertEqual(ORIGINAL_GAME_VERSION, "1.0.1")
        self.assertEqual(PORT_AUTHOR, "David Kalil Braga")
        self.assertEqual(PORT_YEAR, "2026")
        proc = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "bump_version.py"), "--check"],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn(f"v{PORT_VERSION}", proc.stdout)

    def test_semver_comparison_and_url_validation(self):
        self.assertEqual(parse_semver("v1.0.0"), (1, 0, 0))
        self.assertEqual(parse_semver("V2.10.3"), (2, 10, 3))
        self.assertIsNone(parse_semver("1.0"))
        self.assertIsNone(parse_semver("1.0.0-beta"))

        self.assertTrue(is_newer_version("v1.0.1", "1.0.0"))
        self.assertTrue(is_newer_version("1.1.0", "1.0.9"))
        self.assertFalse(is_newer_version("v1.0.0", "1.0.0"))
        self.assertFalse(is_newer_version("v0.9.9", "1.0.0"))

        self.assertTrue(
            is_valid_release_download_url(
                f"https://github.com/{GITHUB_REPOSITORY}/releases/download/v1.0.1/{ASSET_WINDOWS_X64}"
            )
        )
        self.assertFalse(is_valid_release_download_url(f"http://github.com/{GITHUB_REPOSITORY}/releases/download/x.zip"))
        self.assertFalse(is_valid_release_download_url("https://evil.example.com/payload.zip"))

    def test_release_payload_security_and_draft_filtering(self):
        draft_payload = {"tag_name": "v9.9.9", "draft": True, "assets": []}
        info, reason = parse_latest_release_payload(draft_payload, target_asset=ASSET_WINDOWS_X64)
        self.assertIsNone(info)
        self.assertEqual(reason, UpdatePhase.NO_RELEASE_YET)

        pre_payload = {"tag_name": "v9.9.9", "prerelease": True, "assets": []}
        info, reason = parse_latest_release_payload(pre_payload, target_asset=ASSET_WINDOWS_X64)
        self.assertIsNone(info)
        self.assertEqual(reason, UpdatePhase.NO_RELEASE_YET)

        evil_payload = {
            "tag_name": "v1.1.0",
            "assets": [
                {
                    "name": ASSET_WINDOWS_X64,
                    "browser_download_url": "https://attacker.invalid/malware.zip",
                    "size": 1234,
                }
            ],
        }
        info, reason = parse_latest_release_payload(evil_payload, target_asset=ASSET_WINDOWS_X64)
        self.assertIsNone(info)
        self.assertEqual(reason, "untrusted_url")

    def test_services_prefs_roundtrip_and_404_private_repo(self):
        import urllib.error

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            cfg = root / "config" / "services.json"
            self.assertTrue(load_services_prefs(cfg)["check_updates_at_startup"])
            save_services_prefs({"check_updates_at_startup": False}, cfg)
            self.assertFalse(load_services_prefs(cfg)["check_updates_at_startup"])

            def http_404(req, timeout=15.0):
                raise urllib.error.HTTPError(req.full_url, 404, "Not Found", {}, None)

            mgr = UpdateManager(
                local_version="1.0.0",
                config_path=cfg,
                updates_dir=root / "data" / "updates",
                install_root=root,
                http_open=http_404,
                target_asset=ASSET_WINDOWS_X64,
            )
            self.assertFalse(mgr.maybe_start_startup_check())
            self.assertTrue(mgr.start_check(startup=False))
            mgr._worker.join(timeout=3.0)
            st = mgr.snapshot()
            self.assertEqual(st.phase, UpdatePhase.NO_RELEASE_YET)
            self.assertIsNone(mgr.consume_pending_notice())

    def test_update_check_download_sha256_verify_and_stage_install(self):
        zip_bytes = _make_sample_release_zip(inner_wrapper=True)
        zip_sha = hashlib.sha256(zip_bytes).hexdigest()
        api_payload = {
            "tag_name": "v1.2.0",
            "name": "Heroes Lore 3 Recomp v1.2.0",
            "body": "## Changes\n- Better performance\n- Bugfixes",
            "html_url": f"https://github.com/{GITHUB_REPOSITORY}/releases/tag/v1.2.0",
            "published_at": "2026-12-01T12:00:00Z",
            "draft": False,
            "prerelease": False,
            "assets": [
                {
                    "name": ASSET_WINDOWS_X64,
                    "browser_download_url": (
                        f"https://github.com/{GITHUB_REPOSITORY}/releases/download/v1.2.0/{ASSET_WINDOWS_X64}"
                    ),
                    "size": len(zip_bytes),
                    "digest": f"sha256:{zip_sha}",
                }
            ],
        }

        def fake_http(req, timeout=15.0):
            url = req.full_url
            if url.endswith("/releases/latest"):
                return _FakeHTTPResponse(json.dumps(api_payload).encode("utf-8"), 200, url)
            if ASSET_WINDOWS_X64 in url:
                return _FakeHTTPResponse(zip_bytes, 200, "https://objects.githubusercontent.com/dl/pkg.zip")
            raise AssertionError(f"Unexpected URL {url}")

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            cfg = root / "config" / "services.json"
            upd_dir = root / "data" / "updates"
            mgr = UpdateManager(
                local_version="1.0.0",
                config_path=cfg,
                updates_dir=upd_dir,
                install_root=root,
                is_standalone=False,
                http_open=fake_http,
                target_asset=ASSET_WINDOWS_X64,
            )
            self.assertTrue(mgr.maybe_start_startup_check())
            mgr._worker.join(timeout=3.0)
            st = mgr.snapshot()
            self.assertEqual(st.phase, UpdatePhase.UPDATE_AVAILABLE)
            self.assertIsNotNone(st.release)
            self.assertEqual(st.release.tag_name, "v1.2.0")
            notice = mgr.consume_pending_notice()
            self.assertIsNotNone(notice)
            self.assertIsNone(mgr.consume_pending_notice())

            # Test SHA-256 mismatch failure handling
            bad_zip = zip_bytes + b"CORRUPT"
            bad_mgr = UpdateManager(
                local_version="1.0.0",
                config_path=cfg,
                updates_dir=upd_dir / "bad",
                install_root=root,
                is_standalone=False,
                http_open=lambda req, timeout=15.0: (
                    _FakeHTTPResponse(json.dumps(api_payload).encode("utf-8"), 200, req.full_url)
                    if req.full_url.endswith("/releases/latest")
                    else _FakeHTTPResponse(bad_zip, 200, "https://objects.githubusercontent.com/bad.zip")
                ),
                target_asset=ASSET_WINDOWS_X64,
            )
            bad_mgr.start_check(startup=False)
            bad_mgr._worker.join(timeout=3.0)
            bad_mgr.start_download()
            bad_mgr._worker.join(timeout=3.0)
            self.assertEqual(bad_mgr.snapshot().phase, UpdatePhase.ERROR)

            # Good download + non-destructive staging check
            self.assertTrue(mgr.start_download())
            mgr._worker.join(timeout=3.0)
            st_dl = mgr.snapshot()
            self.assertEqual(st_dl.phase, UpdatePhase.READY_TO_INSTALL)
            self.assertTrue(st_dl.downloaded_path and st_dl.downloaded_path.exists())

            ok, staged = mgr.install_ready_update()
            self.assertTrue(ok)
            self.assertEqual(mgr.snapshot().phase, UpdatePhase.MANUAL_STAGED)
            self.assertTrue((Path(staged) / "HeroesLore3.exe").exists())

            # Validate Windows PS1 updater script generation preserves config & data + retry logic
            ps1 = build_windows_updater_script(
                target_pid=99999,
                staged_root=Path(staged),
                install_root=root,
                rollback_root=upd_dir / "rollback_test",
            )
            self.assertIn("$Preserved = @('config', 'data')", ps1)
            self.assertIn("RollbackRoot", ps1)
            self.assertIn("Invoke-WithRetry", ps1)

            if sys.platform == "win32":
                # Verify standalone Windows updater uses UTF-8 BOM and valid Popen flags (no DETACHED_PROCESS)
                (root / "HeroesLore3.exe").write_bytes(b"MZ")
                quit_called = []
                popen_calls = []

                class _FakeProc:
                    pid = 4242

                def fake_popen(args, **kwargs):
                    popen_calls.append((args, kwargs))
                    return _FakeProc()

                standalone_mgr = UpdateManager(
                    local_version="1.0.0",
                    config_path=root / "config" / "services.json",
                    updates_dir=upd_dir,
                    install_root=root,
                    is_standalone=True,
                    target_asset=ASSET_WINDOWS_X64,
                    request_quit_cb=lambda: quit_called.append(True),
                )
                standalone_mgr.state.release = st_dl.release
                standalone_mgr.state.downloaded_path = st_dl.downloaded_path
                with mock.patch("hl3rt.updater.subprocess.Popen", side_effect=fake_popen):
                    ok_inst, _ = standalone_mgr.install_ready_update()
                self.assertTrue(ok_inst)
                self.assertEqual(standalone_mgr.snapshot().phase, UpdatePhase.INSTALLING)
                self.assertEqual(quit_called, [True])
                self.assertEqual(len(popen_calls), 1)
                p_args, p_kwargs = popen_calls[0]
                self.assertIn("-File", p_args)
                self.assertTrue((upd_dir / "update_windows.ps1").read_bytes().startswith(b"\xef\xbb\xbf"))
                flags = int(p_kwargs.get("creationflags", 0))
                detached = getattr(subprocess, "DETACHED_PROCESS", 0)
                if detached:
                    self.assertEqual(flags & detached, 0)
                self.assertEqual(p_kwargs.get("stdin"), subprocess.DEVNULL)

    def test_modal_update_page_render_and_navigation(self):
        pygame.init()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            cloud = CloudSaveManager(data_root=root / "data", auth_path=root / "config" / "cloud_auth.json")
            upd = UpdateManager(
                local_version="1.0.0",
                config_path=root / "config" / "services.json",
                updates_dir=root / "data" / "updates",
                install_root=root,
                http_open=lambda req, timeout=15.0: _FakeHTTPResponse(
                    json.dumps({"tag_name": "v1.0.0", "name": "v1.0.0", "body": "Initial release", "assets": []}).encode("utf-8")
                ),
                target_asset=ASSET_WINDOWS_X64,
            )
            modal = PortOptionsModal(
                cloud=cloud,
                get_fps=lambda: 30,
                set_fps=lambda v: None,
                get_aspect=lambda: "auto",
                set_aspect=lambda v: None,
                get_lang=lambda: "en",
                set_lang=lambda v: None,
                updater=upd,
            )
            modal.open(ModalPage.OPTIONS)
            # Row 4 toggles startup update check
            for _ in range(4):
                modal.handle_key(pygame.K_DOWN)
            self.assertEqual(modal.focus_idx, 4)
            modal.handle_key(pygame.K_RIGHT)
            self.assertFalse(upd.get_startup_check())
            # Row 5 enters UPDATE page
            modal.handle_key(pygame.K_DOWN)
            self.assertEqual(modal.focus_idx, 5)
            modal.handle_key(pygame.K_RETURN)
            if upd._worker:
                upd._worker.join(timeout=3.0)
            self.assertEqual(modal.page, ModalPage.UPDATE)
            self.assertEqual(upd.snapshot().phase, UpdatePhase.UP_TO_DATE)
            surf = modal._render_surface(960, 640, "pt-BR")
            self.assertEqual(surf.get_size(), (960, 640))
            surf_p = modal._render_surface(480, 800, "en")
            self.assertEqual(surf_p.get_size(), (480, 800))
            modal.handle_key(pygame.K_ESCAPE)
            self.assertEqual(modal.page, ModalPage.OPTIONS)
            self.assertEqual(modal.focus_idx, 5)

    def test_about_menu_slot_6632_has_port_credit_and_version_live(self):
        cfg_path = ROOT / "config" / "display.json"
        orig_cfg = cfg_path.read_text(encoding="utf-8") if cfg_path.exists() else None
        orig_lang = get_active_language()
        try:
            set_active_language("en")
            runner = PCRunner(scale=1, headless_frames=0)
            runner.set_language("en", notify=False)
            for _ in range(25):
                runner.step_frame()
            m = runner.rt.m
            import struct

            def read_6632() -> str:
                tu = m.rd32(0x000D8220)
                buf1 = m.rd32(m.rd32(tu + 0x20) + 4)
                offs1 = m.rd32(m.rd32(tu + 0x24) + 4)
                o0, o1 = struct.unpack("<2I", m.rdbytes(offs1 + 6632 * 4, 8))
                return bytes(m.rdbytes(buf1 + o0, o1 - o0)).decode("utf-16-be")

            s_en = read_6632()
            self.assertIn(f"Port by David Kalil Braga (2026) - v{PORT_VERSION}", s_en)
            self.assertIn("Customer service:", s_en)

            runner.set_language("pt-BR", notify=False)
            for _ in range(5):
                runner.step_frame()
            s_pt = read_6632()
            self.assertIn(f"Port por David Kalil Braga (2026) - v{PORT_VERSION}", s_pt)
            self.assertIn("Atendimento ao cliente:", s_pt)
        finally:
            set_active_language(orig_lang or DEFAULT_LANGUAGE)
            if orig_cfg is not None:
                cfg_path.write_text(orig_cfg, encoding="utf-8")
            pygame.quit()

    def test_bundle_loads_standalone_game_ipa_without_apk(self):
        import shutil
        import hl3rt.machine as hm

        project = json.loads((ROOT / "project.json").read_text(encoding="utf-8"))
        with zipfile.ZipFile(ROOT / project["original_apk"]["path"], "r") as apk:
            ipa_bytes = apk.read("assets/game.ipa")

        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            (t / "project.json").write_text(json.dumps(project), encoding="utf-8")
            (t / "assets").mkdir()
            (t / "assets" / "game.ipa").write_bytes(ipa_bytes)
            shutil.copytree(ROOT / "assets" / "fonts", t / "assets" / "fonts")
            shutil.copytree(ROOT / "assets" / "texts", t / "assets" / "texts")

            with mock.patch.object(hm, "ROOT", t):
                bundle = hm.Bundle()
            self.assertEqual(len(bundle.macho), int(project["macho_executable"]["bytes"]))
            self.assertGreater(len(bundle.read("newres/texts/t_en")), 100000)

    def test_bump_version_updates_all_platforms_in_sandbox(self):
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            (t / "tools" / "hl3rt").mkdir(parents=True)
            (t / "src" / "core").mkdir(parents=True)
            (t / "android" / "app").mkdir(parents=True)
            (t / ".github" / "workflows").mkdir(parents=True)

            (t / "tools" / "hl3rt" / "version.py").write_text('PORT_VERSION = "1.0.3"\n', encoding="utf-8")
            (t / "src" / "core" / "common.h").write_text(
                'inline constexpr const char* kPortVersion = "1.0.3";\n', encoding="utf-8"
            )
            (t / "project.json").write_text(json.dumps({"port_version": "1.0.3"}) + "\n", encoding="utf-8")
            (t / "android" / "app" / "build.gradle").write_text(
                "android {\n    defaultConfig {\n        versionCode 4\n        versionName '1.0.3'\n    }\n}\n",
                encoding="utf-8",
            )
            (t / ".github" / "workflows" / "build.yml").write_text(
                "        description: 'Release tag (e.g. v1.0.3)'\n        default: 'v1.0.3'\n",
                encoding="utf-8",
            )
            (t / "README.md").write_text(
                "Runtime Nativo C++17 (`v1.0.3`)\n"
                "https://img.shields.io/badge/Port-v1.0.3-d49b35\n"
                "https://img.shields.io/badge/Original_iOS-v1.0.1-6b7280\n"
                "Recursos da versão `v1.0.3`\n"
                "**`Port by David Kalil Braga (2026) - v1.0.3`** / **`Port por David Kalil Braga (2026) - v1.0.3`**\n"
                "mais nova que a instalada (`v1.0.3`)\n",
                encoding="utf-8",
            )

            proc = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "bump_version.py"), "--root", str(t), "patch"],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertIn("v1.0.3 -> v1.0.4", proc.stdout)
            self.assertIn("versionCode 4 -> 5", proc.stdout)

            chk = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "bump_version.py"), "--root", str(t), "--check"],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(chk.returncode, 0, chk.stdout + chk.stderr)
            self.assertIn("Android versionCode 5", chk.stdout)
            # Original iOS version must remain untouched in README.md
            readme_after = (t / "README.md").read_text(encoding="utf-8")
            self.assertIn("Original_iOS-v1.0.1", readme_after)
            self.assertNotIn("v1.0.3", readme_after)


if __name__ == "__main__":
    unittest.main()
