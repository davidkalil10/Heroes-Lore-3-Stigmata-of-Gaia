#!/usr/bin/env python3
"""Compiles validated PT-BR translation chunks (`localization/chunks/*.json`) into
runtime binary text tables under `assets/texts/`:

  * assets/texts/t_pt-br           - 6,734 BE u32 offsets + UTF-16BE string blob
  * assets/texts/hdr_pt-br         - patched 244-byte `hdr` matching PT-BR blob length
  * assets/texts/InGame_txt_2_pt-br - localized `dat/InGame_txt_2` binary ("Mochila cheia.", "Sim")
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import struct
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT_JSON = ROOT / "project.json"
CATALOG_PATH = ROOT / "reference" / "analysis" / "text_catalog_en.json"
CHUNKS_DIR = ROOT / "localization" / "chunks"
OUT_DIR = ROOT / "assets" / "texts"
APP_PREFIX = "Payload/Heroes Lore.app/"

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_ptbr_chunk import validate_chunk_file  # noqa: E402

EXPECTED_CHUNKS = (
    "01_ui_menus",
    "02_world_classes_skills",
    "03_quests",
    "04a_items_1",
    "04b_items_2",
    "05_story_01",
    "06_story_02",
    "07_story_03",
    "08_story_04",
    "09_story_05",
    "10_story_06",
    "11_story_07",
)


def read_orig_hdr() -> bytes:
    project = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
    with zipfile.ZipFile(ROOT / project["original_apk"]["path"]) as apk:
        ipa_bytes = apk.read("assets/game.ipa")
    if hashlib.sha256(ipa_bytes).hexdigest() != project["nested_ipa"]["sha256"]:
        raise ValueError("IPA hash mismatch")
    with zipfile.ZipFile(io.BytesIO(ipa_bytes)) as zf:
        return zf.read(APP_PREFIX + "newres/texts/hdr")


def pack_t_file(strings: list[str]) -> tuple[bytes, int]:
    blob = bytearray()
    offsets: list[int] = [0]
    for s in strings:
        blob.extend(s.encode("utf-16-be"))
        offsets.append(len(blob))
    off_table = struct.pack(f">{len(offsets)}I", *offsets)
    return off_table + bytes(blob), len(blob)


def pack_ingame_txt_2(entries: list[str]) -> bytes:
    encoded = [s.encode("utf-8") + b"\x00" for s in entries]
    header_size = 4 + 2 * len(encoded)
    offsets = []
    pos = header_size
    blob = bytearray()
    for raw in encoded:
        offsets.append(pos)
        blob.extend(raw)
        pos += len(raw)
    total_size = pos
    hdr = struct.pack("<HH", total_size, len(encoded))
    off_bytes = struct.pack(f"<{len(offsets)}H", *offsets)
    return hdr + off_bytes + bytes(blob)


def build_texts(allow_partial: bool = False) -> dict:
    cat = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    en_entries = cat["entries"]
    n_slots = len(en_entries)
    pt_by_idx: dict[int, str] = {}
    loaded_chunks = []
    missing_chunks = []

    for cname in EXPECTED_CHUNKS:
        cpath = CHUNKS_DIR / f"{cname}.json"
        if not cpath.exists():
            if not allow_partial:
                raise FileNotFoundError(f"Missing PT-BR chunk: {cpath}")
            missing_chunks.append(cname)
            continue
        ok, errs = validate_chunk_file(cpath)
        if not ok:
            raise ValueError(f"Validation failed in {cpath}:\n  " + "\n  ".join(errs[:20]))
        data = json.loads(cpath.read_text(encoding="utf-8"))
        trans = data.get("translations", data)
        for k, v in trans.items():
            idx = int(k)
            if idx in pt_by_idx:
                raise ValueError(f"Duplicate slot {idx} across chunks")
            pt_by_idx[idx] = v
        loaded_chunks.append(cname)

    if not allow_partial and len(pt_by_idx) != n_slots:
        missing_slots = sorted(set(range(n_slots)) - set(pt_by_idx))
        raise ValueError(f"Expected {n_slots} slots, got {len(pt_by_idx)} (missing {missing_slots[:10]})")

    final_strings: list[str] = []
    for idx in range(n_slots):
        if idx in pt_by_idx:
            final_strings.append(pt_by_idx[idx])
        else:
            final_strings.append(en_entries[idx]["en"])

    t_ptbr_bytes, blob_len = pack_t_file(final_strings)
    orig_hdr = bytearray(read_orig_hdr())
    # Patch langIdx=0 ('en   ') blob byte count at offset 216
    struct.pack_into(">I", orig_hdr, 216, blob_len)

    ig2_bytes = pack_ingame_txt_2(["Mochila cheia.", "Sim"])

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "t_pt-br").write_bytes(t_ptbr_bytes)
    (OUT_DIR / "hdr_pt-br").write_bytes(bytes(orig_hdr))
    (OUT_DIR / "InGame_txt_2_pt-br").write_bytes(ig2_bytes)

    return {
        "loaded_chunks": loaded_chunks,
        "missing_chunks": missing_chunks,
        "translated_slots": len(pt_by_idx),
        "total_slots": n_slots,
        "t_ptbr_bytes": len(t_ptbr_bytes),
        "blob_bytes": blob_len,
        "ig2_bytes": len(ig2_bytes),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile PT-BR localization assets.")
    parser.add_argument(
        "--allow-partial",
        action="store_true",
        help="Fallback to English for chunks not yet written (dev testing only).",
    )
    args = parser.parse_args()
    res = build_texts(allow_partial=args.allow_partial)
    print(
        f"[PTBR-TEXT] Built assets/texts/ ({res['translated_slots']}/{res['total_slots']} slots from "
        f"{len(res['loaded_chunks'])}/{len(EXPECTED_CHUNKS)} chunks; "
        f"t_pt-br={res['t_ptbr_bytes']} B, blob={res['blob_bytes']} B, ig2={res['ig2_bytes']} B)"
    )
    if res["missing_chunks"]:
        print(f"[PTBR-TEXT] Pending chunks (EN fallback active): {res['missing_chunks']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

