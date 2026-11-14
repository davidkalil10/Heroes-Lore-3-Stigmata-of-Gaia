#!/usr/bin/env python3
"""Validates a PT-BR translation chunk JSON against the S09 English catalog and .rff glyph coverage."""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG_PATH = ROOT / "reference" / "analysis" / "text_catalog_en.json"
FONT_PATHS = (
    ROOT / "assets" / "fonts" / "font_mainmenu.rff",
    ROOT / "assets" / "fonts" / "font_menu_medium.rff",
    ROOT / "assets" / "fonts" / "font_menu_small.rff",
)

COLOR_PREFIXES = ("{", "}", "|", "^", "#", "&")
FMT_RE = re.compile(r"%[0-9.]*[sdifcxu]")


def load_supported_chars() -> set[str]:
    # Intersection of supported glyphs across menu fonts (except ASCII control markers kept elsewhere)
    # Use font_menu_small union? Wait: menu_small renders dialogues, items, quests;
    # mainmenu & menu_medium render main titles & labels. We check against font_menu_small + common control chars.
    chars = None
    for fp in FONT_PATHS:
        b = fp.read_bytes()
        _, _, _, _, _, _, c1, _ = struct.unpack_from(">3sBBBBBHH", b, 0)
        cps = set(struct.unpack_from(f">{c1}H", b, 12))
        s = {chr(c) for c in cps}
        if fp.name == "font_menu_small.rff":
            small_set = s
        chars = s if chars is None else (chars | s)
    # Control prefixes & formatting chars handled natively by TextUtils / renderer even if absent from UI font
    return small_set | set(COLOR_PREFIXES) | {"\n", "\r", "\t", "@", "<", ">", "=", "~", "_"}


def validate_chunk_file(chunk_path: Path, expected_indices: list[int] | None = None) -> tuple[bool, list[str]]:
    cat = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    en_entries = {e["index"]: e["en"] for e in cat["entries"]}
    allowed_chars = load_supported_chars()

    data = json.loads(chunk_path.read_text(encoding="utf-8"))
    translations = data.get("translations", data)
    if not isinstance(translations, dict):
        return False, ["Chunk JSON must be a dict or contain a 'translations' dict mapping str(index)->pt_str"]

    errors: list[str] = []
    seen: set[int] = set()
    for k, pt in translations.items():
        if not k.isdigit():
            errors.append(f"Non-numeric key {k!r}")
            continue
        idx = int(k)
        if idx not in en_entries:
            errors.append(f"Invalid slot index {idx}")
            continue
        if not isinstance(pt, str):
            errors.append(f"[{idx}] Value must be string, got {type(pt).__name__}")
            continue
        seen.add(idx)
        en = en_entries[idx]

        if not en:
            if pt != "":
                errors.append(f"[{idx}] Empty EN slot must remain empty, got {pt!r}")
            continue

        if not pt.strip():
            errors.append(f"[{idx}] Non-empty EN {en!r} got empty PT translation")
            continue

        # Leading color prefix
        if en[0] in COLOR_PREFIXES and not pt.startswith(en[0]):
            errors.append(f"[{idx}] Missing leading prefix {en[0]!r}: en={en!r} pt={pt!r}")

        # Speaker prefix X[Name]#
        m_en = re.match(r"^[{}|^#&]*\[([^\]]+)\]#", en)
        if m_en:
            m_pt = re.match(r"^[{}|^#&]*\[([^\]]+)\]#", pt)
            if not m_pt:
                errors.append(f"[{idx}] Broken speaker prefix '[Name]#': en={en!r} pt={pt!r}")

        # Delimiter / control token parity
        for sym in ("#", ";", "||", "{", "}", "|"):
            if en.count(sym) != pt.count(sym):
                errors.append(
                    f"[{idx}] {sym!r} count mismatch ({en.count(sym)} != {pt.count(sym)}): en={en!r} pt={pt!r}"
                )

        en_fmts = FMT_RE.findall(en)
        pt_fmts = FMT_RE.findall(pt)
        if en_fmts != pt_fmts:
            errors.append(f"[{idx}] printf placeholder mismatch ({en_fmts} != {pt_fmts}): en={en!r} pt={pt!r}")

        bad_chars = sorted({ch for ch in pt if ch not in allowed_chars})
        if bad_chars:
            errors.append(f"[{idx}] Unsupported .rff characters {bad_chars} in pt={pt!r}")

    if expected_indices is not None:
        exp_set = set(expected_indices)
        missing = sorted(exp_set - seen)
        extra = sorted(seen - exp_set)
        if missing:
            errors.append(f"Missing {len(missing)} indices (first 10: {missing[:10]})")
        if extra:
            errors.append(f"Unexpected {len(extra)} extra indices (first 10: {extra[:10]})")

    return len(errors) == 0, errors


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: python tools/validate_ptbr_chunk.py <chunk.json> [<src_chunk.json>]")
        return 2
    chunk_path = Path(sys.argv[1])
    expected = None
    if len(sys.argv) >= 3:
        src = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))
        expected = [int(item["index"]) for item in src["items"]]
    ok, errs = validate_chunk_file(chunk_path, expected)
    if ok:
        print(f"[VALIDATE PASS] {chunk_path}")
        return 0
    print(f"[VALIDATE FAIL] {chunk_path}: {len(errs)} error(s):")
    for e in errs[:30]:
        print(" ", e)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
