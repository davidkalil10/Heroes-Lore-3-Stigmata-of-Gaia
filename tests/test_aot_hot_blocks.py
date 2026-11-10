"""Automated tests for S07: CPU Performance, AOT Hot Blocks and Unicorn Oracle."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.aot_blocks import AOT_REGISTRY
from hl3rt.runtime import Runtime


class S07AOTPerformanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rt = Runtime()
        cls.rt.boot_to_app_main()

    def test_phase_profile_artifact_exists_and_valid(self):
        """CPU profile JSON must contain all 4 phases with non-zero instruction counts."""
        profile_path = ROOT / "reference" / "analysis" / "cpu_profile_phases.json"
        self.assertTrue(profile_path.exists(), "cpu_profile_phases.json missing")

        with open(profile_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        self.assertIn("phases", data)
        phases = data["phases"]
        for p in ["boot", "launch", "title", "menu"]:
            self.assertIn(p, phases)
            self.assertGreater(phases[p]["total_instructions"], 0)
            self.assertGreater(phases[p]["unique_basic_blocks"], 0)
            self.assertGreater(len(phases[p]["top_hot_blocks"]), 0)

    def test_aot_blocks_registry_and_opcode_guards(self):
        """AOT hot block registry must have >=15 blocks with working opcode guards."""
        self.assertGreaterEqual(len(AOT_REGISTRY.blocks), 15)
        self.assertEqual(len(AOT_REGISTRY.blocks), len(AOT_REGISTRY.guards))

        m = self.rt.m
        for pc, guard_fn in AOT_REGISTRY.guards.items():
            # Must pass on pristine binary
            self.assertTrue(guard_fn(m), f"Guard failed for 0x{pc:08x} on pristine image")

            # Must fail when tampered
            orig = m.rd32(pc)
            m.wr32(pc, orig ^ 0xFF)
            self.assertFalse(guard_fn(m), f"Guard failed to detect modification at 0x{pc:08x}")
            m.wr32(pc, orig)

    def test_c_plus_plus_hot_blocks_header_format(self):
        """src/arm_hot_blocks.inc must exist and define HOT_BLOCK_COUNT and guard_hot_block."""
        hdr_path = ROOT / "src" / "arm_hot_blocks.inc"
        self.assertTrue(hdr_path.exists(), "arm_hot_blocks.inc missing")

        text = hdr_path.read_text(encoding="utf-8")
        self.assertIn("static constexpr unsigned HOT_BLOCK_COUNT =", text)
        self.assertIn("bool guard_hot_block(unsigned index, const uint32_t* memory)", text)
        self.assertIn("static constexpr HotBlock HOT_BLOCKS[]", text)

    def test_oracle_verification_report(self):
        """aot_oracle_report.json must report result PASS and >= 15 verified blocks."""
        report_path = ROOT / "reference" / "analysis" / "aot_oracle_report.json"
        self.assertTrue(report_path.exists(), "aot_oracle_report.json missing")

        with open(report_path, "r", encoding="utf-8") as f:
            rep = json.load(f)

        self.assertEqual(rep.get("result"), "PASS")
        self.assertGreaterEqual(rep.get("total_blocks", 0), 15)
        self.assertGreater(rep.get("total_comparisons", 0), 0)

    def test_runtime_phase_counters(self):
        """Runtime Machine must maintain execution counters by phase."""
        m = self.rt.m
        self.assertIn("boot", m.counters)
        self.assertGreater(len(m.counters["boot"]), 0)


if __name__ == "__main__":
    unittest.main()
