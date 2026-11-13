"""Automated unit tests for HL3 Google Drive Cloud Save, isolation, validation, and atomic restore."""

from __future__ import annotations

import base64
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.cloud_save import (
    ANDROID_PACKAGE_ID,
    CLOUD_APP_TAG,
    CLOUD_FILE_NAME,
    CLOUD_FOLDER_NAME,
    SWITCH_TITLE_ID,
    CloudSaveManager,
    HttpResponse,
    collect_local_saves,
    format_iso_local,
    get_google_client_id,
    get_google_client_secret,
    inspect_local_saves,
    is_safe_save_path,
    pack_saves,
    restore_saves,
    unpack_saves,
)


class HL3CloudSaveTests(unittest.TestCase):
    def test_hl3_isolation_constants_and_oauth_decode(self):
        self.assertEqual(CLOUD_APP_TAG, "heroes_lore_3")
        self.assertEqual(CLOUD_FOLDER_NAME, "Heroes Lore III - Stigmata of Gaia")
        self.assertEqual(CLOUD_FILE_NAME, "heroes_lore_3_save.json")
        self.assertEqual(ANDROID_PACKAGE_ID, "com.davidbraga.heroeslore3")
        self.assertEqual(SWITCH_TITLE_ID, "0100F3A300030000")
        cid = get_google_client_id()
        csec = get_google_client_secret()
        self.assertTrue(cid.endswith(".apps.googleusercontent.com"))
        self.assertTrue(csec.startswith("GOCSPX-"))

    def test_safe_save_path_validation(self):
        self.assertTrue(is_safe_save_path("internal/Documents/Hero3GameSave_10_3"))
        self.assertTrue(is_safe_save_path("external/extra.bin"))
        for bad in (
            "",
            "Hero3GameSave_10_3",
            "config/cloud_auth.json",
            "../evil",
            "internal/../etc/passwd",
            "internal/Documents/.hidden",
            "internal/Documents/CON",
            "internal/Documents/com1.txt",
            "internal\\Documents\\save",
            "/internal/Documents/save",
            "internal/Documents/bad?",
            "internal/" + ("a" * 240),
        ):
            self.assertFalse(is_safe_save_path(bad), f"Expected unsafe path rejected: {bad!r}")

    def test_pack_unpack_and_tamper_rejection(self):
        files = {
            "internal/Documents/Hero3GameSave_10_3": b"SAVE-BYTES-1234",
            "internal/Documents/HL3_SlotInfo_0": b"\x18Z\x00\x00",
        }
        pkg_text = pack_saves(files, platform="Windows", summary="Windows | 1 slot")
        doc = json.loads(pkg_text)
        self.assertEqual(doc["app"], "heroes_lore_3")
        self.assertEqual(doc["package_id"], "com.davidbraga.heroeslore3")
        self.assertEqual(doc["switch_title_id"], "0100F3A300030000")

        unpacked, meta = unpack_saves(pkg_text)
        self.assertEqual(unpacked, files)
        self.assertEqual(meta["platform"], "Windows")

        # Reject foreign game backup
        foreign = dict(doc)
        foreign["app"] = "heroes_lore_5"
        with self.assertRaises(ValueError):
            unpack_saves(json.dumps(foreign))

        # Reject checksum mismatch
        bad_sha = json.loads(pkg_text)
        bad_sha["files"]["internal/Documents/HL3_SlotInfo_0"]["sha256"] = "0" * 64
        with self.assertRaises(ValueError):
            unpack_saves(json.dumps(bad_sha))

        # Reject path traversal & colliding names
        bad_path = json.loads(pkg_text)
        raw = b"abc"
        bad_path["files"]["internal/../../evil"] = {
            "data": base64.b64encode(raw).decode("ascii"),
            "sha256": hashlib.sha256(raw).hexdigest(),
        }
        with self.assertRaises(ValueError):
            unpack_saves(json.dumps(bad_path))

        colliding = json.loads(pkg_text)
        colliding["files"]["internal/documents/hl3_slotinfo_0"] = {
            "data": base64.b64encode(raw).decode("ascii"),
            "sha256": hashlib.sha256(raw).hexdigest(),
        }
        with self.assertRaises(ValueError):
            unpack_saves(json.dumps(colliding))

    def test_atomic_restore_and_rollback_backup(self):
        with tempfile.TemporaryDirectory() as tmp:
            data_root = Path(tmp) / "data"
            docs = data_root / "internal" / "Documents"
            docs.mkdir(parents=True)
            (docs / "Hero3GameSave_10_0").write_bytes(b"OLD_SAVE")
            (docs / "options.ini").write_bytes(b"OLD_OPT")

            info = inspect_local_saves(data_root)
            self.assertTrue(info.exists)
            self.assertEqual(info.file_count, 2)
            self.assertEqual(info.slot_count, 1)
            self.assertEqual(set(collect_local_saves(data_root).keys()), {
                "internal/Documents/Hero3GameSave_10_0",
                "internal/Documents/options.ini",
            })

            new_files = {
                "internal/Documents/Hero3GameSave_10_3": b"CLOUD_SAVE_DATA",
                "internal/Documents/HL3_Autosave_0": b"AUTO",
            }
            backup_dir = restore_saves(data_root, new_files)
            self.assertTrue((backup_dir / "internal" / "Documents" / "Hero3GameSave_10_0").exists())
            self.assertEqual((backup_dir / "internal" / "Documents" / "Hero3GameSave_10_0").read_bytes(), b"OLD_SAVE")
            self.assertFalse((docs / "Hero3GameSave_10_0").exists())
            self.assertEqual((docs / "Hero3GameSave_10_3").read_bytes(), b"CLOUD_SAVE_DATA")
            self.assertEqual((docs / "HL3_Autosave_0").read_bytes(), b"AUTO")

    def test_utc_iso_formats_to_local_string(self):
        s = format_iso_local("2026-11-13T15:30:00Z")
        self.assertRegex(s, r"^\d{4}-\d{2}-\d{2} \d{2}:\d{2}$")

    def test_mocked_oauth_device_flow_upload_migrate_and_restore(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data_root = root / "data"
            auth_path = root / "config" / "cloud_auth.json"
            docs = data_root / "internal" / "Documents"
            docs.mkdir(parents=True)
            (docs / "Hero3GameSave_10_3").write_bytes(b"LOCAL_PROGRESS")

            drive_state = {
                "folders": {},
                "files": {
                    "legacy_id_1": {
                        "id": "legacy_id_1",
                        "name": CLOUD_FILE_NAME,
                        "parents": ["root"],
                        "modifiedTime": "2026-11-13T12:00:00Z",
                        "description": "Android | 1 slot",
                        "version": "5",
                        "content": pack_saves({"internal/Documents/Hero3GameSave_10_3": b"REMOTE_SAVE_V5"}, platform="Android"),
                    }
                },
                "token_polls": 0,
                "patched_parents": False,
            }

            def fake_http(method: str, url: str, headers: dict, body: str, timeout: float) -> HttpResponse:
                if url == "https://oauth2.googleapis.com/device/code":
                    return HttpResponse(200, json.dumps({
                        "device_code": "DEV123",
                        "user_code": "ABCD-EFGH",
                        "verification_url": "https://www.google.com/device",
                        "expires_in": 600,
                        "interval": 1,
                    }))
                if url == "https://oauth2.googleapis.com/token":
                    if "grant_type=refresh_token" in body:
                        return HttpResponse(200, json.dumps({"access_token": "AT_NEW", "expires_in": 3600}))
                    drive_state["token_polls"] += 1
                    if drive_state["token_polls"] == 1:
                        return HttpResponse(428, json.dumps({"error": "authorization_pending"}))
                    return HttpResponse(200, json.dumps({
                        "access_token": "AT_1",
                        "refresh_token": "RT_1",
                        "expires_in": 3600,
                    }))
                if method == "GET" and url.startswith("https://www.googleapis.com/drive/v3/files?"):
                    if "mimeType%3D%27application/vnd.google-apps.folder%27" in url:
                        fl = [{"id": fid} for fid in drive_state["folders"]]
                        return HttpResponse(200, json.dumps({"files": fl}))
                    # Query files
                    f = drive_state["files"].get("legacy_id_1")
                    if not f:
                        return HttpResponse(200, json.dumps({"files": []}))
                    if "in%20parents" in url and not any(fid in f["parents"] for fid in drive_state["folders"]):
                        return HttpResponse(200, json.dumps({"files": []}))
                    return HttpResponse(200, json.dumps({"files": [{
                        "id": f["id"],
                        "modifiedTime": f["modifiedTime"],
                        "description": f["description"],
                        "version": f["version"],
                        "parents": f["parents"],
                    }]}))
                if method == "POST" and url == "https://www.googleapis.com/drive/v3/files":
                    drive_state["folders"]["folder_hl3"] = {"name": CLOUD_FOLDER_NAME}
                    return HttpResponse(200, json.dumps({"id": "folder_hl3"}))
                if method == "PATCH" and "addParents=" in url:
                    drive_state["files"]["legacy_id_1"]["parents"] = ["folder_hl3"]
                    drive_state["patched_parents"] = True
                    return HttpResponse(200, json.dumps({"id": "legacy_id_1"}))
                if method == "GET" and "?alt=media" in url:
                    return HttpResponse(200, drive_state["files"]["legacy_id_1"]["content"])
                if method == "PATCH" and "uploadType=media" in url:
                    drive_state["files"]["legacy_id_1"]["content"] = body
                    drive_state["files"]["legacy_id_1"]["version"] = "6"
                    return HttpResponse(200, json.dumps({"id": "legacy_id_1"}))
                if method == "PATCH" and "/legacy_id_1" in url:
                    return HttpResponse(200, json.dumps({"id": "legacy_id_1"}))
                return HttpResponse(404, "{}")

            mgr = CloudSaveManager(data_root=data_root, auth_path=auth_path, http_fn=fake_http)
            # Speed up poll wait using 0s by pre-cancelling? No, wait=1s only twice -> <2s
            mgr.start_login()
            mgr.wait_idle(timeout=6.0)
            st = mgr.get_status()
            self.assertTrue(st.connected)
            self.assertTrue(auth_path.exists())
            self.assertTrue(drive_state["patched_parents"])
            self.assertTrue(st.backup_exists)
            self.assertEqual(st.backup_platform, "Android")

            # Test restore from cloud backup
            mgr.restore_backup()
            mgr.wait_idle(timeout=5.0)
            self.assertTrue(mgr.get_status().restore_ready)
            bak = mgr.consume_pending_restore()
            self.assertIsNotNone(bak)
            self.assertEqual((docs / "Hero3GameSave_10_3").read_bytes(), b"REMOTE_SAVE_V5")

            # Test optimistic conflict detection if version changes before upload
            drive_state["files"]["legacy_id_1"]["version"] = "99"
            (docs / "Hero3GameSave_10_3").write_bytes(b"NEW_LOCAL")
            mgr.upload_backup()
            mgr.wait_idle(timeout=5.0)
            self.assertEqual(mgr.get_status().message_key, "cloud.msg.conflict_changed")

            # Disconnect removes token file cleanly
            mgr.disconnect()
            self.assertFalse(mgr.get_status().connected)
            self.assertFalse(auth_path.exists())


if __name__ == "__main__":
    unittest.main()

