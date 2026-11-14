#!/usr/bin/env python3
"""Generates PT-BR extended bitmap font atlases (`.rff` + `.pvr`) into `assets/fonts/`.

Adds crisp, native pixel-matched glyphs for the 5 Brazilian Portuguese characters
missing from the original iOS bundle (`Ã`, `Ô`, `Õ`, `ã`, `õ`) across:
  * font_mainmenu
  * font_menu_medium
  * font_menu_small

(`font_hud` already includes all PT-BR diacritics via multi-component `.rff` glyphs.)

Reads the pristine original APK/IPA and writes extended copies to `assets/fonts/`
without ever modifying `reference/extracted_ipa/` or the original APK.
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
OUT_DIR = ROOT / "assets" / "fonts"
APP_PREFIX = "Payload/Heroes Lore.app/"

TARGET_FONTS = ("font_mainmenu", "font_menu_medium", "font_menu_small")

# Each synthetic character: (target_char, base_char, accent_source_char, reference_height_char)
SYNTHESIS_PLAN = (
    ("Ã", "A", "Ñ", "Â"),
    ("Õ", "O", "Ñ", "Ó"),
    ("Ô", "O", "Â", "Ó"),
    ("ã", "a", "ñ", "â"),
    ("õ", "o", "ñ", "ô"),
)


def open_ipa() -> zipfile.ZipFile:
    project = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
    with zipfile.ZipFile(ROOT / project["original_apk"]["path"]) as apk:
        ipa_bytes = apk.read("assets/game.ipa")
    if hashlib.sha256(ipa_bytes).hexdigest() != project["nested_ipa"]["sha256"]:
        raise ValueError("IPA hash mismatch")
    return zipfile.ZipFile(io.BytesIO(ipa_bytes))


def unpack_rff(rff_bytes: bytes) -> dict:
    magic, flags, line_h, asc_pad, u6, u7, c1, c2 = struct.unpack_from(">3sBBBBBHH", rff_bytes, 0)
    if magic != b"!\x01\x02":
        raise ValueError(f"Bad RFF magic {magic!r}")
    cps = list(struct.unpack_from(f">{c1}H", rff_bytes, 12))
    offs = list(struct.unpack_from(f">{c1}H", rff_bytes, 12 + 2 * c1))
    bs = 12 + 4 * c1
    glyphs: dict[int, dict] = {}
    for i, cp in enumerate(cps):
        adv, ncomp = struct.unpack_from("<BB", rff_bytes, bs + offs[i])
        comps = [
            list(struct.unpack_from("<HHbbBB", rff_bytes, bs + offs[i] + 2 + k * 8))
            for k in range(ncomp)
        ]
        glyphs[cp] = {"cp": cp, "adv": adv, "comps": comps}
    return {
        "flags": flags,
        "line_h": line_h,
        "asc_pad": asc_pad,
        "u6": u6,
        "u7": u7,
        "glyphs": glyphs,
    }


def pack_rff(rff_data: dict) -> bytes:
    sorted_cps = sorted(rff_data["glyphs"].keys())
    c1 = len(sorted_cps)
    c2 = sum(len(rff_data["glyphs"][cp]["comps"]) for cp in sorted_cps)
    offs: list[int] = []
    blob = bytearray()
    for cp in sorted_cps:
        g = rff_data["glyphs"][cp]
        offs.append(len(blob))
        blob.extend(struct.pack("<BB", g["adv"], len(g["comps"])))
        for comp in g["comps"]:
            blob.extend(struct.pack("<HHbbBB", *comp))
    hdr = struct.pack(
        ">3sBBBBBHH",
        b"!\x01\x02",
        rff_data["flags"],
        rff_data["line_h"],
        rff_data["asc_pad"],
        rff_data["u6"],
        rff_data["u7"],
        c1,
        c2,
    )
    cp_table = struct.pack(f">{c1}H", *sorted_cps)
    off_table = struct.pack(f">{c1}H", *offs)
    return hdr + cp_table + off_table + bytes(blob)


def unpack_pvr_rgba(pvr_bytes: bytes) -> tuple[list[int], int, int, int, list[list[tuple[int, int, int, int]]]]:
    hdr = list(struct.unpack_from("<13I", pvr_bytes, 0))
    hdr_len, h, w, _mips, pflags, _dlen, bpp = hdr[:7]
    fmt = pflags & 0xFF
    raw = pvr_bytes[hdr_len:]
    pixels: list[list[tuple[int, int, int, int]]] = []
    if fmt in (0x16, 0x17) and bpp == 16:
        for y in range(h):
            row = []
            for x in range(w):
                lum, alpha = raw[(y * w + x) * 2 : (y * w + x) * 2 + 2]
                row.append((lum, lum, lum, alpha))
            pixels.append(row)
    elif fmt == 0x12 and bpp == 32:
        for y in range(h):
            row = []
            for x in range(w):
                r, g, b, a = raw[(y * w + x) * 4 : (y * w + x) * 4 + 4]
                row.append((r, g, b, a))
            pixels.append(row)
    else:
        raise ValueError(f"Unsupported PVR format fmt=0x{fmt:02x} bpp={bpp}")
    return hdr, w, h, fmt, pixels


def pack_pvr_rgba(hdr: list[int], w: int, h: int, fmt: int, pixels: list[list[tuple[int, int, int, int]]]) -> bytes:
    out = bytearray()
    if fmt in (0x16, 0x17):
        for y in range(h):
            for x in range(w):
                r, _g, _b, a = pixels[y][x]
                out.append(r)
                out.append(a)
    elif fmt == 0x12:
        for y in range(h):
            for x in range(w):
                r, g, b, a = pixels[y][x]
                out.extend((r, g, b, a))
    else:
        raise ValueError(f"Unsupported PVR format fmt=0x{fmt:02x}")
    hdr = list(hdr)
    hdr[5] = len(out)
    return struct.pack("<13I", *hdr) + bytes(out)


def crop_patch(pixels: list[list[tuple[int, int, int, int]]], x: int, y: int, w: int, h: int) -> list[list[tuple[int, int, int, int]]]:
    return [pixels[y + r][x : x + w] for r in range(h)]


def extract_accent_patch(
    pixels: list[list[tuple[int, int, int, int]]],
    rff_glyphs: dict[int, dict],
    accent_src_char: str,
) -> list[list[tuple[int, int, int, int]]]:
    # Determine exact accent height by matching against the corresponding ASCII base letter
    base_ascii = {
        "Ñ": "N",
        "ñ": "n",
        "Â": "A",
        "â": "a",
        "Ô": "O",
        "ô": "o",
    }[accent_src_char]
    ax, ay, _ox, _oy, aw, ah = rff_glyphs[ord(accent_src_char)]["comps"][0]
    _bx, _by, _box, _boy, _bw, bh = rff_glyphs[ord(base_ascii)]["comps"][0]
    acc_rows = ah - bh
    # Include 1 extra halo row below if present? Wait: bottom bh rows match base letter 100%,
    # so top acc_rows rows contain the pure accent glyph + its outline halo!
    patch = crop_patch(pixels, ax, ay, aw, acc_rows)
    # Trim empty horizontal margins of the accent itself for accurate centering over any letter
    non_empty_cols = [
        c for c in range(aw) if any(patch[r][c][3] > 0 for r in range(acc_rows))
    ]
    non_empty_rows = [
        r for r in range(acc_rows) if any(patch[r][c][3] > 0 for c in range(aw))
    ]
    if not non_empty_cols or not non_empty_rows:
        return patch
    c0, c1 = min(non_empty_cols), max(non_empty_cols) + 1
    r0, r1 = min(non_empty_rows), max(non_empty_rows) + 1
    return [row[c0:c1] for row in patch[r0:r1]]


def composite_pixel(dst: tuple[int, int, int, int], src: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    if src[3] == 0:
        return dst
    if dst[3] == 0:
        return src
    # Preserve bright glyph core where accent and letter outlines slightly touch
    return (
        max(dst[0], src[0]),
        max(dst[1], src[1]),
        max(dst[2], src[2]),
        max(dst[3], src[3]),
    )


def compose_glyph_patch(
    pixels: list[list[tuple[int, int, int, int]]],
    rff_glyphs: dict[int, dict],
    base_char: str,
    accent_src_char: str,
    ref_height_char: str,
) -> tuple[int, int, int, list[list[tuple[int, int, int, int]]]]:
    base_g = rff_glyphs[ord(base_char)]
    bx, by, box, boy, bw, bh = base_g["comps"][0]
    ref_h = rff_glyphs[ord(ref_height_char)]["comps"][0][5]
    acc_patch = extract_accent_patch(pixels, rff_glyphs, accent_src_char)
    acc_h = len(acc_patch)
    acc_w = len(acc_patch[0])

    # Reserve at least ref_h total height (or acc_h + 1 + bh if taller) so bottom sits cleanly on oy=0 baseline
    total_h = max(ref_h, acc_h + bh)
    out = [[(0, 0, 0, 0) for _ in range(bw)] for _ in range(total_h)]

    # Place base letter at bottom so baseline oy stays identical
    base_patch = crop_patch(pixels, bx, by, bw, bh)
    y_base = total_h - bh
    for r in range(bh):
        for c in range(bw):
            out[y_base + r][c] = base_patch[r][c]

    # Find visual center of the base letter top portion for clean optical centering
    top_cols = [c for r in range(min(3, bh)) for c in range(bw) if base_patch[r][c][3] >= 64]
    if top_cols:
        base_cx = (min(top_cols) + max(top_cols) + 1) / 2.0
    else:
        base_cx = bw / 2.0
    x_acc = round(base_cx - acc_w / 2.0)
    x_acc = max(0, min(bw - acc_w, x_acc))

    # Place accent vertically right above base letter (with at most 1px shared halo if total_h == ref_h)
    y_acc = max(0, y_base - acc_h - (1 if total_h - bh - acc_h >= 2 else 0))
    for r in range(acc_h):
        for c in range(acc_w):
            px = acc_patch[r][c]
            if 0 <= y_acc + r < total_h and 0 <= x_acc + c < bw:
                out[y_acc + r][x_acc + c] = composite_pixel(out[y_acc + r][x_acc + c], px)

    return base_g["adv"], box, boy, out


def build_font(zf: zipfile.ZipFile, font_name: str) -> dict:
    rff_raw = zf.read(APP_PREFIX + f"newres/fonts/{font_name}.rff")
    pvr_raw = zf.read(APP_PREFIX + f"newres/fonts/{font_name}.pvr")
    rff_data = unpack_rff(rff_raw)
    hdr, w, h, fmt, pixels = unpack_pvr_rgba(pvr_raw)

    # Locate lowest occupied row in atlas
    max_used_y = max(
        comp[1] + comp[5]
        for g in rff_data["glyphs"].values()
        for comp in g["comps"]
    )
    cursor_x = 2
    cursor_y = max_used_y + 3
    row_max_h = 0
    added = {}

    for target_ch, base_ch, acc_ch, ref_ch in SYNTHESIS_PLAN:
        cp = ord(target_ch)
        if cp in rff_data["glyphs"]:
            continue
        adv, ox, oy, patch = compose_glyph_patch(pixels, rff_data["glyphs"], base_ch, acc_ch, ref_ch)
        ph = len(patch)
        pw = len(patch[0])
        if cursor_x + pw + 2 > w:
            cursor_x = 2
            cursor_y += row_max_h + 3
            row_max_h = 0
        if cursor_y + ph + 1 > h:
            raise RuntimeError(f"Not enough free space in {font_name}.pvr ({w}x{h}) for {target_ch!r}")
        for r in range(ph):
            for c in range(pw):
                pixels[cursor_y + r][cursor_x + c] = patch[r][c]
        rff_data["glyphs"][cp] = {
            "cp": cp,
            "adv": adv,
            "comps": [[cursor_x, cursor_y, ox, oy, pw, ph]],
        }
        added[target_ch] = {"tx": cursor_x, "ty": cursor_y, "w": pw, "h": ph, "adv": adv}
        cursor_x += pw + 4
        row_max_h = max(row_max_h, ph)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    new_rff = pack_rff(rff_data)
    new_pvr = pack_pvr_rgba(hdr, w, h, fmt, pixels)
    (OUT_DIR / f"{font_name}.rff").write_bytes(new_rff)
    (OUT_DIR / f"{font_name}.pvr").write_bytes(new_pvr)
    return {
        "glyph_count": len(rff_data["glyphs"]),
        "rff_bytes": len(new_rff),
        "pvr_bytes": len(new_pvr),
        "added": added,
        "pixels": pixels,
        "rff": rff_data,
    }


def render_verification_sheet(built: dict[str, dict], out_path: Path) -> None:
    from PIL import Image, ImageDraw

    lines = [
        "HEROES LORE III - PT-BR FONT TEST",
        "AÃÂÁÀ OÕÔÓÒ NÑ EÊÉ IÍ UÚ CÇ",
        "aãâáà oõôóò nñ eêé ií uú cç",
        "Ação, Opções, Configurações, Dragão, Ônibus",
        "Coração do Guardião - Recompensas & Missões",
    ]
    canvas = Image.new("RGBA", (960, 560), (24, 28, 38, 255))
    draw = ImageDraw.Draw(canvas)
    y_cursor = 14

    for font_name in TARGET_FONTS:
        draw.text((16, y_cursor), f"[{font_name}]", fill=(180, 210, 255, 255))
        y_cursor += 16
        rff = built[font_name]["rff"]
        pixels = built[font_name]["pixels"]
        lh = rff["line_h"]
        glyphs = rff["glyphs"]

        for text in lines:
            x_cursor = 20
            baseline_y = y_cursor + lh
            for ch in text:
                g = glyphs.get(ord(ch)) or glyphs.get(ord(" "))
                if g is None:
                    continue
                for tx, ty, ox, oy, gw, gh in g["comps"]:
                    if gw == 0 or gh == 0:
                        continue
                    patch_img = Image.new("RGBA", (gw, gh))
                    flat = [pixels[ty + r][tx + c] for r in range(gh) for c in range(gw)]
                    patch_img.putdata(flat)
                    paste_y = baseline_y - gh - oy
                    canvas.alpha_composite(patch_img, (x_cursor + ox, paste_y))
                x_cursor += g["adv"]
            y_cursor += lh + 4
        y_cursor += 10

    out_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out_path)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build PT-BR extended fonts into assets/fonts/.")
    parser.add_argument(
        "--preview",
        type=Path,
        default=None,
        help="Optional output PNG path rendering sample PT-BR strings across all 3 fonts.",
    )
    args = parser.parse_args()

    zf = open_ipa()
    built = {}
    for name in TARGET_FONTS:
        built[name] = build_font(zf, name)
        print(
            f"[PTBR-FONT] {name}: {built[name]['glyph_count']} glyphs, added {list(built[name]['added'].keys())}"
        )

    if args.preview:
        render_verification_sheet(built, args.preview)
        print(f"[PTBR-FONT] Preview saved to {args.preview}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

