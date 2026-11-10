#!/usr/bin/env python3
"""CPU Profiling by Phase for Heroes Lore 3 (S07).

Measures basic block execution counts, instruction frequencies, and hotspot locations
across phases: 'boot', 'launch', 'title' (60 frames), and 'menu'.
Outputs deterministic report to reference/analysis/cpu_profile_phases.json.
"""

from __future__ import annotations

import argparse
import io
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

import capstone
from unicorn import UC_HOOK_BLOCK
from unicorn.arm_const import UC_ARM_REG_CPSR

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.hle_uikit import Touch
from hl3rt.runtime import Runtime


def profile_execution(title_frames: int = 60, menu_frames: int = 30) -> dict:
    rt = Runtime()

    block_counts: dict[str, Counter] = defaultdict(Counter)
    insn_counts: dict[str, int] = defaultdict(int)
    mnemonic_counts: dict[str, Counter] = defaultdict(Counter)

    md_arm = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    md_thumb = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

    def on_block(uc, address, size, user_data):
        phase = rt.m.phase
        block_counts[phase][address] += 1
        cpsr = uc.reg_read(UC_ARM_REG_CPSR)
        is_thumb = bool(cpsr & 0x20)
        insn_size = 2 if is_thumb else 4
        num_insns = max(1, size // insn_size)
        insn_counts[phase] += num_insns

    rt.m.uc.hook_add(UC_HOOK_BLOCK, on_block)

    print("[HL3 Profile] Phase 1: Boot...")
    rt.m.phase = "boot"
    rt.boot_to_app_main()

    print("[HL3 Profile] Phase 2: Launch...")
    rt.m.phase = "launch"
    rt.run_launch()

    delegate = rt.state["delegate"]
    touch_responder = 0
    for k, obj in rt.objc.hobj.items():
        cls, _ = rt.objc.class_of(k)
        if cls and cls.name == "TouchResponder":
            touch_responder = k
            break

    print(f"[HL3 Profile] Phase 3: Title ({title_frames} frames)...")
    rt.m.phase = "title"
    for _ in range(title_frames):
        rt.send(delegate, "onTimer:", 0)

    print(f"[HL3 Profile] Phase 4: Menu ({menu_frames} frames)...")
    rt.m.phase = "menu"
    if touch_responder:
        # Simulate tap at center (240, 160) to enter menu
        t = Touch(240.0, 160.0, 0)
        t_obj = rt.objc.new_obj("UITouch", t)
        evt_obj = rt.objc.new_obj("UIEvent", {"touches": [t_obj]})
        set_obj = rt.objc.new_obj("NSSet", [t_obj])
        rt.send(touch_responder, "touchesBegan:withEvent:", set_obj, evt_obj)
        t.phase = 3
        rt.send(touch_responder, "touchesEnded:withEvent:", set_obj, evt_obj)

    for _ in range(menu_frames):
        rt.send(delegate, "onTimer:", 0)

    # Disassemble and catalog top blocks
    binary_data = rt.bundle.macho
    report_phases = {}

    for phase in ["boot", "launch", "title", "menu"]:
        total_insns = insn_counts[phase]
        unique_blocks = len(block_counts[phase])
        top_blocks = []

        for addr, count in block_counts[phase].most_common(20):
            # Check if address is in guest binary
            block_info = {
                "address": f"0x{addr:08x}",
                "executions": count,
                "pct_of_phase": round(count / max(1, sum(block_counts[phase].values())) * 100, 2),
            }
            if 0x1000 <= addr < len(binary_data):
                # Sample 4 bytes to check ARM vs Thumb
                raw = binary_data[addr : addr + 16]
                insns_arm = list(md_arm.disasm(raw, addr))
                insns_thumb = list(md_thumb.disasm(raw, addr))
                block_info["disasm_arm"] = [f"{i.mnemonic} {i.op_str}" for i in insns_arm[:3]]
                block_info["disasm_thumb"] = [f"{i.mnemonic} {i.op_str}" for i in insns_thumb[:3]]
                block_info["opcodes_hex"] = raw[:8].hex()
            elif addr == 0x7F100000:
                block_info["label"] = "RET_STUB (bx lr)"
            top_blocks.append(block_info)

        report_phases[phase] = {
            "total_instructions": total_insns,
            "unique_basic_blocks": unique_blocks,
            "total_block_executions": sum(block_counts[phase].values()),
            "top_hot_blocks": top_blocks,
        }

    report = {
        "project": "Heroes Lore 3: Stigmata of Gaia",
        "title_frames": title_frames,
        "menu_frames": menu_frames,
        "phases": report_phases,
    }
    return report


def main():
    parser = argparse.ArgumentParser(description="Profile CPU execution phases for Heroes Lore 3")
    parser.add_argument("--title-frames", type=int, default=60, help="Frames to profile in title phase (default: 60)")
    parser.add_argument("--menu-frames", type=int, default=30, help="Frames to profile in menu phase (default: 30)")
    parser.add_argument(
        "--output",
        type=str,
        default="reference/analysis/cpu_profile_phases.json",
        help="Output JSON file path",
    )
    args = parser.parse_args()

    report = profile_execution(title_frames=args.title_frames, menu_frames=args.menu_frames)

    out_path = ROOT / args.output
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print(f"\n[HL3 Profile] Profile report saved to: {out_path.relative_to(ROOT)}")
    for phase, pinfo in report["phases"].items():
        print(
            f"  Phase {phase:7s}: {pinfo['total_instructions']:,} insns across {pinfo['unique_basic_blocks']} unique blocks"
        )


if __name__ == "__main__":
    main()

