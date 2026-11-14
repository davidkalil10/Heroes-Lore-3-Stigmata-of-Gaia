#!/usr/bin/env python3
"""Prepares disjoint translation chunk source files in scratch for S10."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG_PATH = ROOT / "reference" / "analysis" / "text_catalog_en.json"


def build_chunks(out_dir: Path) -> None:
    cat = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    entries = cat["entries"]
    out_dir.mkdir(parents=True, exist_ok=True)

    # Disjoint index partitions covering every slot 0..6732 exactly once:
    credits_and_world = (
        list(range(4210, 4229))
        + list(range(4247, 4254))
        + list(range(4422, 4770))
        + list(range(6045, 6218))
    )
    story_tail = list(range(3780, 4210)) + list(range(4229, 4247)) + list(range(4254, 4422))

    specs = [
        ("01_ui_menus", list(range(6218, 6733))),
        ("02_world_classes_skills", credits_and_world),
        ("03_quests", list(range(5650, 6045))),
        ("04a_items_1", list(range(4770, 5210))),
        ("04b_items_2", list(range(5210, 5650))),
        ("05_story_01", list(range(0, 630))),
        ("06_story_02", list(range(630, 1260))),
        ("07_story_03", list(range(1260, 1890))),
        ("08_story_04", list(range(1890, 2520))),
        ("09_story_05", list(range(2520, 3150))),
        ("10_story_06", list(range(3150, 3780))),
        ("11_story_07", story_tail),
    ]

    all_covered: list[int] = []
    manifest = {}
    for name, indices in specs:
        all_covered.extend(indices)
        items = [
            {
                "index": idx,
                "section": entries[idx]["section"],
                "tokens": entries[idx]["tokens"],
                "en": entries[idx]["en"],
                "ko": entries[idx]["ko"],
            }
            for idx in indices
        ]
        non_empty = sum(1 for it in items if it["en"])
        payload = {"chunk": name, "count": len(items), "non_empty": non_empty, "items": items}
        (out_dir / f"{name}.json").write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        manifest[name] = {"total": len(items), "non_empty": non_empty}
        print(f"  wrote {name}.json: total={len(items)}, non_empty={non_empty}")

    assert len(all_covered) == 6733 and set(all_covered) == set(range(6733)), "Partition mismatch!"
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    dest = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "localization" / "src_tmp"
    build_chunks(dest)

