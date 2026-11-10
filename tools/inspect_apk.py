#!/usr/bin/env python3
"""Deterministic static inspector and extractor for Heroes Lore 3 APK + nested iOS game.ipa + Mach-O ARMv6."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import plistlib
import shutil
import struct
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT_JSON = ROOT / "project.json"
ANALYSIS_DIR = ROOT / "reference" / "analysis"
EXTRACTED_APK_DIR = ROOT / "reference" / "extracted_apk"
EXTRACTED_IPA_DIR = ROOT / "reference" / "extracted_ipa"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_uleb128(data: bytes, offset: int) -> tuple[int, int]:
    result = 0
    shift = 0
    while True:
        byte = data[offset]
        offset += 1
        result |= (byte & 0x7F) << shift
        if (byte & 0x80) == 0:
            return result, offset
        shift += 7


def parse_axml_strings(data: bytes) -> list[str]:
    if len(data) < 8:
        return []
    magic, _total_size = struct.unpack_from("<II", data, 0)
    if magic != 0x00080003:
        return []
    chunk_type, header_size, _chunk_size, string_count, _style_count, flags, strings_start, _styles_start = (
        struct.unpack_from("<HHIIIIII", data, 8)
    )
    if chunk_type != 0x0001:
        return []
    is_utf8 = bool(flags & (1 << 8))
    offsets_base = 8 + header_size
    strings_base = 8 + strings_start
    strings: list[str] = []
    for idx in range(string_count):
        str_off = struct.unpack_from("<I", data, offsets_base + idx * 4)[0]
        pos = strings_base + str_off
        if is_utf8:
            _, pos = read_uleb128(data, pos)
            byte_len, pos = read_uleb128(data, pos)
            raw = data[pos : pos + byte_len]
            strings.append(raw.decode("utf-8", errors="replace"))
        else:
            char_len = struct.unpack_from("<H", data, pos)[0]
            pos += 2
            if char_len & 0x8000:
                high = struct.unpack_from("<H", data, pos)[0]
                pos += 2
                char_len = ((char_len & 0x7FFF) << 16) | high
            raw = data[pos : pos + char_len * 2]
            strings.append(raw.decode("utf-16le", errors="replace"))
    return strings


def parse_dex_info(data: bytes) -> dict:
    if len(data) < 0x70 or not data.startswith(b"dex\n"):
        raise ValueError("Invalid DEX header")
    string_ids_size, string_ids_off = struct.unpack_from("<II", data, 0x38)
    type_ids_size, type_ids_off = struct.unpack_from("<II", data, 0x40)
    method_ids_size, _ = struct.unpack_from("<II", data, 0x58)
    class_defs_size, _ = struct.unpack_from("<II", data, 0x60)

    strings: list[str] = []
    for i in range(string_ids_size):
        str_data_off = struct.unpack_from("<I", data, string_ids_off + i * 4)[0]
        _, pos = read_uleb128(data, str_data_off)
        end = data.find(b"\x00", pos)
        raw = data[pos:end] if end != -1 else data[pos:]
        strings.append(raw.decode("utf-8", errors="replace"))

    type_descriptors: list[str] = []
    for i in range(type_ids_size):
        desc_idx = struct.unpack_from("<I", data, type_ids_off + i * 4)[0]
        if 0 <= desc_idx < len(strings):
            type_descriptors.append(strings[desc_idx])

    return {
        "header": {
            "magic": data[:8].decode("ascii", errors="replace").strip("\x00"),
            "file_size": len(data),
            "string_ids_size": string_ids_size,
            "type_ids_size": type_ids_size,
            "method_ids_size": method_ids_size,
            "class_defs_size": class_defs_size,
        },
        "type_descriptors": type_descriptors,
        "interesting_strings": [
            s
            for s in strings
            if any(
                k in s.lower()
                for k in ("touchhle", "heroes", "ea.", "sdl", "ipa", "landscape", "openal", "gles")
            )
        ],
    }


def extract_null_terminated_strings(blob: bytes) -> list[str]:
    result: list[str] = []
    for part in blob.split(b"\x00"):
        if part:
            result.append(part.decode("utf-8", errors="replace"))
    return result


def parse_macho_armv6(data: bytes) -> tuple[dict, list[dict], dict]:
    magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags = struct.unpack_from("<7I", data, 0)
    if magic != 0xFEEDFACE:
        raise ValueError(f"Expected 32-bit Mach-O magic 0xFEEDFACE, got 0x{magic:08x}")

    off = 28
    segments: list[dict] = []
    sections_by_name: dict[str, tuple[int, int, int]] = {}
    dylibs: list[dict] = []
    symtab_info: dict = {}
    dysymtab_info: dict = {}
    unixthread_info: dict = {}
    uuid_hex: str | None = None

    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<2I", data, off)
        if cmd == 1:  # LC_SEGMENT
            segname = data[off + 8 : off + 24].split(b"\x00", 1)[0].decode("ascii", errors="replace")
            vmaddr, vmsize, fileoff, filesize, maxprot, initprot, nsects, segflags = struct.unpack_from(
                "<8I", data, off + 24
            )
            sec_off = off + 56
            sec_list: list[dict] = []
            for _s in range(nsects):
                sectname = data[sec_off : sec_off + 16].split(b"\x00", 1)[0].decode("ascii", errors="replace")
                seg_s = data[sec_off + 16 : sec_off + 32].split(b"\x00", 1)[0].decode("ascii", errors="replace")
                s_addr, s_size, s_offset, s_align, s_reloff, s_nreloc, s_flags, r1, r2 = struct.unpack_from(
                    "<9I", data, sec_off + 32
                )
                sec_list.append(
                    {
                        "sectname": sectname,
                        "segname": seg_s,
                        "addr": f"0x{s_addr:08x}",
                        "size": s_size,
                        "offset": s_offset,
                        "align": s_align,
                        "flags": f"0x{s_flags:08x}",
                        "reserved1": r1,
                        "reserved2": r2,
                    }
                )
                sections_by_name[f"{seg_s}.{sectname}"] = (s_addr, s_size, s_offset)
                sec_off += 68
            segments.append(
                {
                    "segname": segname,
                    "vmaddr": f"0x{vmaddr:08x}",
                    "vmsize": vmsize,
                    "fileoff": fileoff,
                    "filesize": filesize,
                    "maxprot": maxprot,
                    "initprot": initprot,
                    "flags": f"0x{segflags:08x}",
                    "sections": sec_list,
                }
            )
        elif cmd == 2:  # LC_SYMTAB
            symoff, nsyms, stroff, strsize = struct.unpack_from("<4I", data, off + 8)
            symtab_info = {"symoff": symoff, "nsyms": nsyms, "stroff": stroff, "strsize": strsize}
        elif cmd == 11:  # LC_DYSYMTAB
            vals = struct.unpack_from("<18I", data, off + 8)
            keys = [
                "ilocalsym",
                "nlocalsym",
                "iextdefsym",
                "nextdefsym",
                "iundefsym",
                "nundefsym",
                "tocoff",
                "ntoc",
                "modtaboff",
                "nmodtab",
                "extrefsymoff",
                "nextrefsyms",
                "indirectsymoff",
                "nindirectsyms",
                "extreloff",
                "nextrel",
                "locreloff",
                "nlocrel",
            ]
            dysymtab_info = dict(zip(keys, vals))
        elif cmd == 5:  # LC_UNIXTHREAD
            flavor, count = struct.unpack_from("<2I", data, off + 8)
            if count >= 17:
                regs = struct.unpack_from("<17I", data, off + 16)
                unixthread_info = {
                    "flavor": flavor,
                    "count": count,
                    "r0_to_r12": [f"0x{r:08x}" for r in regs[:13]],
                    "sp": f"0x{regs[13]:08x}",
                    "lr": f"0x{regs[14]:08x}",
                    "pc": f"0x{regs[15]:08x}",
                    "cpsr": f"0x{regs[16]:08x}",
                }
        elif cmd in (12, 13, 24):  # LC_LOAD_DYLIB, LC_ID_DYLIB, LC_LOAD_WEAK_DYLIB
            name_off, timestamp, current_version, compat_version = struct.unpack_from("<4I", data, off + 8)
            raw_name = data[off + name_off : off + cmdsize].split(b"\x00", 1)[0]
            dylibs.append(
                {
                    "cmd": cmd,
                    "name": raw_name.decode("utf-8", errors="replace"),
                    "timestamp": timestamp,
                    "current_version": f"0x{current_version:08x}",
                    "compat_version": f"0x{compat_version:08x}",
                }
            )
        elif cmd == 27:  # LC_UUID
            uuid_hex = data[off + 8 : off + 24].hex()
        off += cmdsize

    symbols: list[dict] = []
    if symtab_info:
        symoff = symtab_info["symoff"]
        nsyms = symtab_info["nsyms"]
        stroff = symtab_info["stroff"]
        strsize = symtab_info["strsize"]
        strtab = data[stroff : stroff + strsize]
        for idx in range(nsyms):
            n_strx, n_type, n_sect, n_desc, n_value = struct.unpack_from("<IBBH I", data, symoff + idx * 12)
            end = strtab.find(b"\x00", n_strx)
            name = strtab[n_strx:end].decode("utf-8", errors="replace") if end != -1 else ""
            symbols.append(
                {
                    "index": idx,
                    "name": name,
                    "n_type": f"0x{n_type:02x}",
                    "n_sect": n_sect,
                    "n_desc": f"0x{n_desc:04x}",
                    "n_value": f"0x{n_value:08x}",
                    "defined": (n_type & 0x0E) != 0,
                }
            )

    mod_init_ptrs: list[str] = []
    if "__DATA.__mod_init_func" in sections_by_name:
        _s_addr, s_size, s_off = sections_by_name["__DATA.__mod_init_func"]
        count = s_size // 4
        ptrs = struct.unpack_from(f"<{count}I", data, s_off)
        mod_init_ptrs = [f"0x{p:08x}" for p in ptrs]

    objc_classes: list[str] = []
    if "__TEXT.__objc_classname" in sections_by_name:
        _, s_size, s_off = sections_by_name["__TEXT.__objc_classname"]
        objc_classes = extract_null_terminated_strings(data[s_off : s_off + s_size])

    objc_selectors: list[str] = []
    if "__TEXT.__objc_methname" in sections_by_name:
        _, s_size, s_off = sections_by_name["__TEXT.__objc_methname"]
        objc_selectors = extract_null_terminated_strings(data[s_off : s_off + s_size])

    objc_methtypes: list[str] = []
    if "__TEXT.__objc_methtype" in sections_by_name:
        _, s_size, s_off = sections_by_name["__TEXT.__objc_methtype"]
        objc_methtypes = extract_null_terminated_strings(data[s_off : s_off + s_size])

    macho_inventory = {
        "sha256": sha256_bytes(data),
        "bytes": len(data),
        "header": {
            "magic": f"0x{magic:08x}",
            "cputype": cputype,
            "cpusubtype": cpusubtype,
            "architecture": "ARMv6 32-bit",
            "filetype": filetype,
            "ncmds": ncmds,
            "sizeofcmds": sizeofcmds,
            "flags": f"0x{flags:08x}",
            "uuid": uuid_hex,
        },
        "entrypoint_unixthread": unixthread_info,
        "mod_init_func_pointers": mod_init_ptrs,
        "dylibs": dylibs,
        "segments": segments,
        "symtab": symtab_info,
        "dysymtab": dysymtab_info,
        "symbol_summary": {
            "total": len(symbols),
            "defined": sum(1 for s in symbols if s["defined"]),
            "undefined_imports": sum(1 for s in symbols if not s["defined"]),
        },
    }

    objc_metadata = {
        "class_count": len(objc_classes),
        "classes": objc_classes,
        "selector_count": len(objc_selectors),
        "selectors": objc_selectors,
        "method_type_count": len(objc_methtypes),
        "method_types": objc_methtypes,
    }

    return macho_inventory, symbols, objc_metadata


def build_analysis_artifacts() -> dict[str, str]:
    project = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
    apk_rel = project["original_apk"]["path"]
    apk_path = ROOT / apk_rel
    if not apk_path.exists():
        raise FileNotFoundError(f"Missing APK: {apk_path}")

    apk_bytes = apk_path.read_bytes()
    apk_sha256 = sha256_bytes(apk_bytes)
    if apk_sha256 != project["original_apk"]["sha256"]:
        raise ValueError(f"APK SHA-256 mismatch: expected {project['original_apk']['sha256']}, got {apk_sha256}")

    with zipfile.ZipFile(io.BytesIO(apk_bytes), "r") as apk_zf:
        apk_entries = []
        for info in sorted(apk_zf.infolist(), key=lambda x: x.filename):
            apk_entries.append(
                {
                    "filename": info.filename,
                    "file_size": info.file_size,
                    "compress_size": info.compress_size,
                    "crc32": f"0x{info.CRC:08x}",
                }
            )
        manifest_strings = parse_axml_strings(apk_zf.read("AndroidManifest.xml"))
        dex_info = parse_dex_info(apk_zf.read("classes.dex"))
        touchhle_options = apk_zf.read("assets/touchHLE_default_options.txt").decode("utf-8", errors="replace")
        ipa_bytes = apk_zf.read("assets/game.ipa")

    ipa_sha256 = sha256_bytes(ipa_bytes)
    if ipa_sha256 != project["nested_ipa"]["sha256"]:
        raise ValueError(f"Nested IPA SHA-256 mismatch: expected {project['nested_ipa']['sha256']}, got {ipa_sha256}")

    with zipfile.ZipFile(io.BytesIO(ipa_bytes), "r") as ipa_zf:
        ipa_entries = []
        prefix_counts: dict[str, int] = {}
        ext_counts: dict[str, int] = {}
        for info in sorted(ipa_zf.infolist(), key=lambda x: x.filename):
            ipa_entries.append(
                {
                    "filename": info.filename,
                    "file_size": info.file_size,
                    "compress_size": info.compress_size,
                    "crc32": f"0x{info.CRC:08x}",
                }
            )
            if not info.filename.endswith("/"):
                parts = info.filename.split("/")
                top = "/".join(parts[:3]) if len(parts) >= 3 else parts[0]
                prefix_counts[top] = prefix_counts.get(top, 0) + 1
                ext = Path(info.filename).suffix.lower() or "<no_ext>"
                ext_counts[ext] = ext_counts.get(ext, 0) + 1

        info_plist_raw = plistlib.loads(ipa_zf.read("Payload/Heroes Lore.app/Info.plist"))
        macho_bytes = ipa_zf.read("Payload/Heroes Lore.app/Heroes Lore")

    macho_sha256 = sha256_bytes(macho_bytes)
    if macho_sha256 != project["macho_executable"]["sha256"]:
        raise ValueError(
            f"Mach-O SHA-256 mismatch: expected {project['macho_executable']['sha256']}, got {macho_sha256}"
        )

    macho_inventory, macho_symbols, objc_metadata = parse_macho_armv6(macho_bytes)

    apk_inventory_doc = {
        "apk_path": apk_rel,
        "bytes": len(apk_bytes),
        "sha256": apk_sha256,
        "entry_count": len(apk_entries),
        "entries": apk_entries,
    }
    ipa_inventory_doc = {
        "apk_entry": "assets/game.ipa",
        "bytes": len(ipa_bytes),
        "sha256": ipa_sha256,
        "entry_count": len(ipa_entries),
        "prefix_counts": dict(sorted(prefix_counts.items())),
        "extension_counts": dict(sorted(ext_counts.items())),
        "entries": ipa_entries,
    }
    android_manifest_doc = {
        "source": "AndroidManifest.xml",
        "string_count": len(manifest_strings),
        "strings": manifest_strings,
    }

    csv_buf = io.StringIO()
    writer = csv.writer(csv_buf, lineterminator="\n")
    writer.writerow(["index", "name", "n_type", "n_sect", "n_desc", "n_value", "defined"])
    for sym in macho_symbols:
        writer.writerow(
            [
                sym["index"],
                sym["name"],
                sym["n_type"],
                sym["n_sect"],
                sym["n_desc"],
                sym["n_value"],
                "true" if sym["defined"] else "false",
            ]
        )

    initial_findings_md = "\n".join(
        [
            "# Initial Static Findings - Heroes Lore 3: Stigmata of Gaia (`S01`)",
            "",
            "Este arquivo é gerado deterministicamente por `python tools/inspect_apk.py`.",
            "",
            "## 1. Pacote Externo (`.apk`) e Wrapper touchHLE",
            "",
            f"- **Arquivo**: `{apk_rel}`",
            f"- **Tamanho**: `{len(apk_bytes)}` bytes",
            f"- **SHA-256**: `{apk_sha256}`",
            f"- **Entradas ZIP**: `{len(apk_entries)}`",
            "- **Natureza do APK**: Wrapper Android do emulador de iOS **touchHLE** (`lib/arm64-v8a/libtouchHLE.so`, `libSDL2.so`, `libc++_shared.so`) contendo o pacote iOS em `assets/game.ipa`.",
            f"- **`assets/touchHLE_default_options.txt`**: `{touchhle_options.strip().splitlines()[-1]}`",
            "",
            "## 2. Pacote iOS Interno (`assets/game.ipa`)",
            "",
            f"- **Tamanho**: `{len(ipa_bytes)}` bytes",
            f"- **SHA-256**: `{ipa_sha256}`",
            f"- **Entradas ZIP**: `{len(ipa_entries)}`",
            f"- **Bundle Identifier**: `{info_plist_raw.get('CFBundleIdentifier')}`",
            f"- **Versão**: `{info_plist_raw.get('CFBundleVersion')}` (`MinimumOSVersion` `{info_plist_raw.get('MinimumOSVersion')}`)",
            f"- **Orientação Inicial**: `{info_plist_raw.get('UIInterfaceOrientation')}` (`480x320` paisagem)",
            f"- **Localizações Inclusas**: `{', '.join(info_plist_raw.get('CFBundleLocalizations', []))}`",
            "",
            "## 3. Executável Mach-O 32-bit ARMv6 (`Payload/Heroes Lore.app/Heroes Lore`)",
            "",
            f"- **Tamanho**: `{len(macho_bytes)}` bytes",
            f"- **SHA-256**: `{macho_sha256}`",
            f"- **Header Mach-O**: `magic={macho_inventory['header']['magic']}`, `arch={macho_inventory['header']['architecture']}`, `ncmds={macho_inventory['header']['ncmds']}`, `uuid={macho_inventory['header']['uuid']}`",
            f"- **Entrypoint (`LC_UNIXTHREAD` PC)**: `{macho_inventory['entrypoint_unixthread'].get('pc')}`",
            f"- **Inicializadores C++ (`__mod_init_func`)**: `{len(macho_inventory['mod_init_func_pointers'])}` funções (`0x000cc000`)",
            f"- **Classes Objective-C**: `{objc_metadata['class_count']}` (`AppDelegate`, `GLView`, `TouchResponder`, `SocketBtDelegate`, `EAMTX_*`, `DMG*`, `SBJson*`)",
            f"- **Seletores Objective-C**: `{objc_metadata['selector_count']}`",
            f"- **Símbolos (`LC_SYMTAB`)**: `{macho_inventory['symbol_summary']['total']}` total (`{macho_inventory['symbol_summary']['defined']}` definidos, `{macho_inventory['symbol_summary']['undefined_imports']}` importados)",
            "",
        ]
    )

    return {
        "apk_inventory.json": json.dumps(apk_inventory_doc, indent=2, ensure_ascii=True) + "\n",
        "android_manifest.json": json.dumps(android_manifest_doc, indent=2, ensure_ascii=True) + "\n",
        "dex_strings.json": json.dumps(dex_info, indent=2, ensure_ascii=True) + "\n",
        "touchhle_options.txt": touchhle_options.replace("\r\n", "\n"),
        "ipa_inventory.json": json.dumps(ipa_inventory_doc, indent=2, ensure_ascii=True) + "\n",
        "info_plist.json": json.dumps(info_plist_raw, indent=2, ensure_ascii=True, default=str) + "\n",
        "macho_inventory.json": json.dumps(macho_inventory, indent=2, ensure_ascii=True) + "\n",
        "macho_symbols.csv": csv_buf.getvalue(),
        "objc_metadata.json": json.dumps(objc_metadata, indent=2, ensure_ascii=True) + "\n",
        "INITIAL_FINDINGS.md": initial_findings_md,
    }


def safe_extract_zip(zf: zipfile.ZipFile, dest_dir: Path) -> int:
    if dest_dir.exists():
        for child in dest_dir.iterdir():
            if child.name == ".gitkeep":
                continue
            if child.is_dir():
                shutil.rmtree(child)
            else:
                child.unlink()
    else:
        dest_dir.mkdir(parents=True, exist_ok=True)
    (dest_dir / ".gitkeep").write_text("\n", encoding="utf-8", newline="\n")

    extracted = 0
    root_resolved = dest_dir.resolve()
    for info in zf.infolist():
        if info.filename.endswith("/"):
            continue
        target = (dest_dir / info.filename).resolve()
        if not str(target).startswith(str(root_resolved)):
            raise ValueError(f"Unsafe zip path: {info.filename}")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(zf.read(info.filename))
        extracted += 1
    return extracted


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify reference/analysis matches APK/IPA bytes")
    parser.add_argument(
        "--extract",
        action="store_true",
        help="Also extract outer APK to reference/extracted_apk and nested IPA to reference/extracted_ipa",
    )
    args = parser.parse_args()

    artifacts = build_analysis_artifacts()

    if args.check:
        errors: list[str] = []
        for filename, expected_text in artifacts.items():
            path = ANALYSIS_DIR / filename
            if not path.exists():
                errors.append(f"Missing analysis file: {path.relative_to(ROOT)}")
                continue
            actual_text = path.read_text(encoding="utf-8")
            if actual_text != expected_text:
                errors.append(f"Outdated analysis file: {path.relative_to(ROOT)}")
        if errors:
            for err in errors:
                print(f"ERROR: {err}", file=sys.stderr)
            return 1
        print("Static APK + IPA + Mach-O analysis files are up to date and verified.")
        return 0

    ANALYSIS_DIR.mkdir(parents=True, exist_ok=True)
    for filename, content in artifacts.items():
        (ANALYSIS_DIR / filename).write_text(content, encoding="utf-8", newline="\n")
    print(f"Wrote {len(artifacts)} analysis artifacts to {ANALYSIS_DIR.relative_to(ROOT)}")

    if args.extract:
        project = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
        apk_path = ROOT / project["original_apk"]["path"]
        with zipfile.ZipFile(apk_path, "r") as apk_zf:
            apk_count = safe_extract_zip(apk_zf, EXTRACTED_APK_DIR)
            ipa_bytes = apk_zf.read("assets/game.ipa")
        with zipfile.ZipFile(io.BytesIO(ipa_bytes), "r") as ipa_zf:
            ipa_count = safe_extract_zip(ipa_zf, EXTRACTED_IPA_DIR)
        print(f"Extracted {apk_count} APK files to {EXTRACTED_APK_DIR.relative_to(ROOT)}")
        print(f"Extracted {ipa_count} IPA files to {EXTRACTED_IPA_DIR.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
