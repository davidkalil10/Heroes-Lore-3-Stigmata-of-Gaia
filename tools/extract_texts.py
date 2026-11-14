#!/usr/bin/env python3
"""Deterministic S09 extractor & baseline auditor for Heroes Lore 3 texts and fonts.

Reads ``assets/game.ipa`` from the original hash-verified APK (never modifying
``reference/extracted_ipa/``) and generates:

* ``reference/analysis/text_catalog_en.json`` - All 6,733 indexed strings in
  ``newres/texts/t_en``, header metadata from ``newres/texts/hdr``, control/color
  token signatures, anomaly audit (mojibake, romanized Korean, empty slots), plus
  ``dat/InGame_txt_2``, ``en.lproj/Localizable.strings``, ``dat/InGame_txt``, and
  unused EA SDK UTF-16LE JSON files.
* ``reference/analysis/font_metrics.json`` - Complete ``.rff`` + ``.pvr`` metrics
  for ``font_hud``, ``font_mainmenu``, ``font_menu_medium``, and
  ``font_menu_small``, atlas free-row headroom, and PT-BR diacritic coverage.

Use ``--check`` to verify committed JSON outputs match deterministic extraction.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
import struct
import sys
import zipfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT_JSON = ROOT / "project.json"
ANALYSIS_DIR = ROOT / "reference" / "analysis"
APP_PREFIX = "Payload/Heroes Lore.app/"

PTBR_DIACRITICS = "ÁÀÂÃÉÊÍÓÔÕÚÜÇáàâãéêíóôõúüç"
COLOR_PREFIXES = ("{", "}", "|", "^", "#", "&")
SUSPICIOUS_RE = re.compile(r"[^\x00-\x7f…’‘“”–—ïºª°™®©×÷]")


def open_ipa(project: dict) -> tuple[zipfile.ZipFile, bytes]:
    apk_path = ROOT / project["original_apk"]["path"]
    with zipfile.ZipFile(apk_path) as apk:
        ipa_bytes = apk.read("assets/game.ipa")
    ipa_sha = hashlib.sha256(ipa_bytes).hexdigest()
    if ipa_sha != project["nested_ipa"]["sha256"]:
        raise ValueError(f"IPA hash mismatch: {ipa_sha}")
    zf = zipfile.ZipFile(io.BytesIO(ipa_bytes))
    macho = zf.read(APP_PREFIX + "Heroes Lore")
    if hashlib.sha256(macho).hexdigest() != project["macho_executable"]["sha256"]:
        raise ValueError("Mach-O hash mismatch")
    return zf, macho


def parse_hdr(hdr: bytes) -> dict:
    # Offsets discovered in TextUtils disassembly (0x011c00..0x011f40)
    unk0 = hdr[0]
    chunk_count = hdr[6]
    lang_count = hdr[7]
    flags_8_10 = list(hdr[8:10])
    languages = []
    pos = 10
    for _ in range(lang_count):
        tag = hdr[pos : pos + 5].decode("ascii", errors="replace")
        lang_id = hdr[pos + 5]
        meta = hdr[pos + 6]
        off_start, off_end = struct.unpack_from(">II", hdr, pos + 7)
        languages.append(
            {
                "tag": tag,
                "tag_stripped": tag.strip(),
                "id": lang_id,
                "meta": meta,
                "name_start": off_start,
                "name_end": off_end,
            }
        )
        pos += 15
    # Immediately after the 7*15 language table (u16be byte length at 113..115 = 98 bytes),
    # UTF-16BE display names run from offset 115 to 213.
    name_blob_start = 10 + lang_count * 15  # 115
    for lang in languages:
        s_off = name_blob_start + lang["name_start"]
        e_off = name_blob_start + lang["name_end"]
        lang["display_name"] = hdr[s_off:e_off].decode("utf-16-be", errors="replace")
    string_count = struct.unpack_from(">H", hdr, 214)[0]
    blob_sizes = list(struct.unpack_from(f">{lang_count}I", hdr, 216))
    for i, lang in enumerate(languages):
        lang["blob_bytes"] = blob_sizes[i]
    return {
        "byte_size": len(hdr),
        "sha256": hashlib.sha256(hdr).hexdigest(),
        "chunk_count": chunk_count,
        "lang_count": lang_count,
        "unk0": unk0,
        "chunk_flags": flags_8_10,
        "string_count_u16be_at_214": string_count,
        "hdr_tail_bytes_212_214": list(hdr[212:214]),
        "languages": languages,
    }


def decode_t_file(data: bytes, count: int) -> list[str]:
    offs = struct.unpack_from(f">{count + 1}I", data, 0)
    blob_start = (count + 1) * 4
    expected_size = blob_start + offs[-1]
    if expected_size != len(data):
        raise ValueError(f"t_file size mismatch: expected {expected_size}, got {len(data)}")
    return [
        data[blob_start + offs[i] : blob_start + offs[i + 1]].decode("utf-16-be", errors="strict")
        for i in range(count)
    ]


def classify_slot(idx: int, text: str) -> str:
    if idx < 4210:
        return "story_dialogue_and_events"
    if 4210 <= idx <= 4252:
        # Note: 4229..4246 contain flashback dialogue interleaved inside credits table
        if idx in range(4229, 4247):
            return "story_dialogue_and_events"
        return "credits_and_prologue_flashback"
    if 4253 <= idx <= 4421:
        return "story_dialogue_and_labels"
    if 4422 <= idx <= 4671:
        return "locations_and_areas"
    if 4672 <= idx <= 4769:
        return "characters_classes_and_enemies"
    if 4770 <= idx <= 5649:
        return "equipment_items_and_descriptions"
    if 5650 <= idx <= 6056:
        return "quests_and_objectives"
    if 6057 <= idx <= 6217:
        return "skills_and_passives"
    return "ui_menus_and_system"


def extract_tokens(text: str) -> list[str]:
    tokens: list[str] = []
    if text and text[0] in COLOR_PREFIXES:
        tokens.append(f"lead:{text[0]}")
    if "#" in text:
        tokens.append("#")
    if "||" in text:
        tokens.append("||")
    if ";" in text:
        tokens.append(";")
    for m in re.findall(r"%[0-9.]*[sdifcxu]", text):
        tokens.append(m)
    return tokens


def parse_ingame_txt_2(data: bytes) -> dict:
    total_size, count = struct.unpack_from("<HH", data, 0)
    offsets = struct.unpack_from(f"<{count}H", data, 4)
    entries = []
    for off in offsets:
        end = data.find(b"\x00", off)
        entries.append(
            {
                "offset": off,
                "raw_hex": data[off:end].hex(),
                "text": data[off:end].decode("utf-8", errors="replace"),
            }
        )
    return {
        "byte_size": len(data),
        "declared_total_size": total_size,
        "count": count,
        "entries": entries,
    }


def parse_rff_and_pvr(rff_bytes: bytes, pvr_bytes: bytes) -> dict:
    magic, flags, line_h, asc_pad, u6, u7, c1, c2 = struct.unpack_from(">3sBBBBBHH", rff_bytes, 0)
    if magic != b"!\x01\x02":
        raise ValueError(f"Unexpected RFF magic: {magic!r}")
    header_size = 12
    cps = list(struct.unpack_from(f">{c1}H", rff_bytes, header_size))
    offs = list(struct.unpack_from(f">{c1}H", rff_bytes, header_size + 2 * c1))
    blob_start = header_size + 4 * c1
    expected_len = blob_start + 2 * c1 + 8 * c2
    if expected_len != len(rff_bytes):
        raise ValueError(f"RFF length mismatch: {len(rff_bytes)} != {expected_len}")

    glyphs = []
    max_x = 0
    max_y = 0
    total_comps = 0
    for i, cp in enumerate(cps):
        off = offs[i]
        adv, ncomp = struct.unpack_from("<BB", rff_bytes, blob_start + off)
        comps = []
        for k in range(ncomp):
            tx, ty, ox, oy, gw, gh = struct.unpack_from("<HHbbBB", rff_bytes, blob_start + off + 2 + k * 8)
            comps.append({"tx": tx, "ty": ty, "ox": ox, "oy": oy, "w": gw, "h": gh})
            max_x = max(max_x, tx + gw)
            max_y = max(max_y, ty + gh)
        total_comps += ncomp
        glyphs.append(
            {
                "codepoint": cp,
                "char": chr(cp),
                "advance": adv,
                "components": comps,
            }
        )

    # Parse PVR v2 header (52 bytes little-endian)
    (
        pvr_hdr_len,
        pvr_h,
        pvr_w,
        mip_count,
        pvr_flags,
        data_len,
        bpp,
        rmask,
        gmask,
        bmask,
        amask,
        pvr_tag,
        surf_count,
    ) = struct.unpack_from("<13I", pvr_bytes, 0)

    cp_set = {g["codepoint"] for g in glyphs}
    missing_ptbr = [ch for ch in PTBR_DIACRITICS if ord(ch) not in cp_set]
    present_ptbr = [ch for ch in PTBR_DIACRITICS if ord(ch) in cp_set]

    return {
        "rff": {
            "byte_size": len(rff_bytes),
            "magic_hex": magic.hex(),
            "flags": flags,
            "line_height": line_h,
            "ascender_pad": asc_pad,
            "reserved_bytes": [u6, u7],
            "codepoint_count": c1,
            "component_count": c2,
            "parsed_component_total": total_comps,
            "sorted_codepoints": cps == sorted(cps),
            "ptbr_present": present_ptbr,
            "ptbr_missing": missing_ptbr,
            "glyphs": glyphs,
        },
        "pvr": {
            "header_size": pvr_hdr_len,
            "width": pvr_w,
            "height": pvr_h,
            "mip_count": mip_count,
            "flags_hex": f"0x{pvr_flags:08x}",
            "pixel_format_id": f"0x{pvr_flags & 0xff:02x}",
            "pixel_format_name": {0x12: "OGL_RGBA_8888", 0x16: "OGL_AI_88", 0x17: "OGL_AI_88"}.get(
                pvr_flags & 0xff, f"UNKNOWN_0x{pvr_flags & 0xff:02x}"
            ),
            "bpp": bpp,
            "data_length": data_len,
            "tag": pvr_bytes[44:48].decode("ascii", errors="replace"),
            "max_used_x": max_x,
            "max_used_y": max_y,
            "free_bottom_rows": pvr_h - max_y,
        },
    }


def collect_auxiliary_texts(zf: zipfile.ZipFile, macho: bytes) -> dict:
    # 1. Localizable.strings (UTF-16LE/BE or UTF-8)
    loc_raw = zf.read(APP_PREFIX + "en.lproj/Localizable.strings")
    try:
        loc_text = loc_raw.decode("utf-16")
    except UnicodeDecodeError:
        loc_text = loc_raw.decode("utf-8", errors="replace")

    # 2. Legacy dat/InGame_txt
    ig1 = zf.read(APP_PREFIX + "dat/InGame_txt")

    # 3. Root UTF-16LE JSON files (EA Mobile SDK leftovers)
    ea_jsons = {}
    for fname in ("help_text.txt", "dmg_strings.txt", "store_strings.txt", "image_names.txt"):
        raw = zf.read(APP_PREFIX + fname)
        decoded = raw.decode("utf-16-le", errors="replace").lstrip("\ufeff")
        ea_jsons[fname] = {
            "byte_size": len(raw),
            "macho_references": int(fname.encode("ascii") in macho),
            "preview": decoded[:240],
        }

    return {
        "newres_charmap": {
            "path": "newres/texts/charmap",
            "byte_size": zf.getinfo(APP_PREFIX + "newres/texts/charmap").file_size,
        },
        "en_lproj_Localizable_strings": {
            "path": "en.lproj/Localizable.strings",
            "byte_size": len(loc_raw),
            "content": loc_text.strip(),
        },
        "dat_InGame_txt_2": {
            "path": "dat/InGame_txt_2",
            "macho_string_present": b"/dat/InGame_txt_2" in macho,
            **parse_ingame_txt_2(zf.read(APP_PREFIX + "dat/InGame_txt_2")),
        },
        "dat_InGame_txt_legacy": {
            "path": "dat/InGame_txt",
            "byte_size": len(ig1),
            "sha256": hashlib.sha256(ig1).hexdigest(),
            "macho_string_present": b"InGame_txt\x00" in macho,
        },
        "root_ea_sdk_txt_files": ea_jsons,
    }


def extract_all() -> tuple[dict, dict]:
    project = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
    zf, macho = open_ipa(project)

    hdr_bytes = zf.read(APP_PREFIX + "newres/texts/hdr")
    hdr_meta = parse_hdr(hdr_bytes)
    n_slots = hdr_meta["string_count_u16be_at_214"]

    t_en_bytes = zf.read(APP_PREFIX + "newres/texts/t_en")
    t_ko_bytes = zf.read(APP_PREFIX + "newres/texts/t_ko")
    en_strings = decode_t_file(t_en_bytes, n_slots)
    ko_strings = decode_t_file(t_ko_bytes, n_slots)

    section_counts: Counter[str] = Counter()
    section_nonempty_counts: Counter[str] = Counter()
    token_counts: Counter[str] = Counter()
    char_freq: Counter[str] = Counter()
    anomalies = []
    entries = []

    for idx, text in enumerate(en_strings):
        section = classify_slot(idx, text)
        section_counts[section] += 1
        if text:
            section_nonempty_counts[section] += 1
        tokens = extract_tokens(text)
        for tok in tokens:
            token_counts[tok] += 1
        char_freq.update(text)

        reasons = []
        if SUSPICIOUS_RE.search(text):
            reasons.append("suspicious_non_ascii_or_mojibake")
        if "kaerikteomyeong" in text.lower():
            reasons.append("romanized_korean_leak")
        if "???" in text:
            pass  # legitimate in dialog/unknown names
        if reasons:
            anomalies.append(
                {
                    "index": idx,
                    "reasons": reasons,
                    "en": text,
                    "ko": ko_strings[idx],
                }
            )

        entries.append(
            {
                "index": idx,
                "section": section,
                "tokens": tokens,
                "en": text,
                "ko": ko_strings[idx],
            }
        )

    non_empty = [s for s in en_strings if s]
    text_catalog = {
        "schema_version": "1.0",
        "step": "S09",
        "source_apk": project["original_apk"]["path"],
        "text_header": hdr_meta,
        "summary": {
            "total_slots": n_slots,
            "non_empty_slots": len(non_empty),
            "empty_slots": n_slots - len(non_empty),
            "unique_non_empty_strings": len(set(non_empty)),
            "t_en_byte_size": len(t_en_bytes),
            "t_en_sha256": hashlib.sha256(t_en_bytes).hexdigest(),
            "section_total_counts": dict(sorted(section_counts.items())),
            "section_non_empty_counts": dict(sorted(section_nonempty_counts.items())),
            "token_counts": dict(sorted(token_counts.items())),
            "distinct_codepoints_used_in_t_en": len(char_freq),
            "non_ascii_codepoints_in_t_en": sorted(ch for ch in char_freq if ord(ch) > 127),
            "anomalies_count": len(anomalies),
        },
        "anomalies": anomalies,
        "auxiliary_texts": collect_auxiliary_texts(zf, macho),
        "entries": entries,
    }

    fonts = {}
    font_names = ("font_hud", "font_mainmenu", "font_menu_medium", "font_menu_small")
    for name in font_names:
        rff_b = zf.read(APP_PREFIX + f"newres/fonts/{name}.rff")
        pvr_b = zf.read(APP_PREFIX + f"newres/fonts/{name}.pvr")
        fonts[name] = parse_rff_and_pvr(rff_b, pvr_b)

    font_metrics = {
        "schema_version": "1.0",
        "step": "S09",
        "ptbr_required_diacritics": list(PTBR_DIACRITICS),
        "font_engine_notes": {
            "rff_parser_va": "0x0000f9dc",
            "glyph_lookup_va": "0x0000f710",
            "text_renderer_va": "0x0008c488",
            "inline_color_parser_va": "0x0004f940",
            "textutils_set_language_va": "0x0001230c",
            "fontmgr_load_set_va": "0x00093798",
        },
        "fonts": fonts,
    }
    return text_catalog, font_metrics


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract and audit HL3 S09 texts and fonts.")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify committed JSON reports match deterministic extraction without overwriting.",
    )
    args = parser.parse_args()

    text_catalog, font_metrics = extract_all()
    ANALYSIS_DIR.mkdir(parents=True, exist_ok=True)
    text_out = ANALYSIS_DIR / "text_catalog_en.json"
    font_out = ANALYSIS_DIR / "font_metrics.json"

    text_json = json.dumps(text_catalog, indent=2, ensure_ascii=False) + "\n"
    font_json = json.dumps(font_metrics, indent=2, ensure_ascii=False) + "\n"

    if args.check:
        for path, expected in ((text_out, text_json), (font_out, font_json)):
            if not path.exists():
                print(f"[S09 CHECK FAIL] Missing {path}")
                return 1
            actual = path.read_text(encoding="utf-8")
            if actual != expected:
                print(f"[S09 CHECK FAIL] {path} differs from fresh extraction")
                return 1
        print("[S09 CHECK PASS] text_catalog_en.json and font_metrics.json match baseline.")
        return 0

    text_out.write_text(text_json, encoding="utf-8")
    font_out.write_text(font_json, encoding="utf-8")
    s = text_catalog["summary"]
    print(f"[S09] Wrote {text_out} ({s['total_slots']} slots, {s['non_empty_slots']} non-empty, {s['anomalies_count']} anomalies)")
    print(f"[S09] Wrote {font_out} ({len(font_metrics['fonts'])} fonts):")
    for fname, fdata in font_metrics["fonts"].items():
        r = fdata["rff"]
        p = fdata["pvr"]
        print(
            f"  - {fname}: {r['codepoint_count']} glyphs, {p['width']}x{p['height']} "
            f"{p['pixel_format_name']}, used_y={p['max_used_y']}/{p['height']}, "
            f"missing PT-BR={r['ptbr_missing']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
