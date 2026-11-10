"""Parser tests for the S02 Mach-O / resource probe. Run: python -m unittest discover -s tests"""

from __future__ import annotations

import json
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import probe_macho_contract as probe  # noqa: E402

ANALYSIS = ROOT / "reference" / "analysis"


class ProbeUnitTests(unittest.TestCase):
    def test_pvr_v2_header(self):
        header = struct.pack("<13I", 52, 4, 8, 0, 0x8012, 4 * 8 * 4, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000, 0x21525650, 1)
        info = probe.probe_pvr(header + b"\x00" * 128)
        self.assertEqual((info["width"], info["height"]), (8, 4))
        self.assertEqual(info["pixel_format"], "OGL_RGBA_8888")
        self.assertTrue(info["size_consistent"])

    def test_ingame_txt_layout(self):
        strings = [b"AB", b"CDE"]
        table_end = 4 + 2 * len(strings)
        offs, cur = [], table_end
        for s in strings:
            offs.append(cur)
            cur += len(s) + 1
        blob = struct.pack("<HH", cur, len(strings)) + struct.pack("<2H", *offs) + b"AB\x00CDE\x00"
        info = probe.probe_ingame_txt(blob)
        self.assertEqual(info["string_count"], 2)
        self.assertEqual(info["first_strings"], ["AB", "CDE"])
        self.assertEqual(info["declared_total_size"], info["actual_size"])


class ProbeGeneratedArtifactTests(unittest.TestCase):
    def test_bindings_complete(self):
        d = json.loads((ANALYSIS / "macho_import_bindings.json").read_text(encoding="utf-8"))
        self.assertEqual(d["import_count"], 389)
        self.assertEqual(d["indirect_symbol_count"], 753)
        for imp in d["imports"]:
            self.assertTrue(imp["stub_addrs"] or imp["nl_symbol_ptr_addrs"] or imp["data_relocation_addrs"], imp["name"])

    def test_objc_core_classes(self):
        d = json.loads((ANALYSIS / "objc_classes_detailed.json").read_text(encoding="utf-8"))
        names = {c["name"]: c for c in d["classes"]}
        self.assertEqual(d["class_count"], 22)
        for required in ("AppDelegate", "GLView", "TouchResponder"):
            self.assertIn(required, names)
        self.assertEqual(names["GLView"]["superclass"], "UIView")
        selectors = {m["selector"] for m in names["TouchResponder"]["instance_methods"]}
        self.assertEqual(selectors, {"touchesBegan:withEvent:", "touchesMoved:withEvent:", "touchesEnded:withEvent:", "init"})

    def test_resources_are_uncompressed_pvr(self):
        d = json.loads((ANALYSIS / "resource_formats.json").read_text(encoding="utf-8"))
        self.assertTrue(set(d["pvr_pixel_formats"]) <= {"OGL_RGBA_8888", "OGL_AI_88"})
        self.assertEqual(d["in_game_text"]["string_count"], 243)


if __name__ == "__main__":
    unittest.main()

