#!/usr/bin/env python3
"""Deterministic deep probe of the Heroes Lore 3 Mach-O ARMv6 runtime contract (step S02).

Reads ``assets/game.ipa`` from the original APK (hash-verified against project.json) and produces:

* reference/analysis/macho_import_bindings.json  - every imported symbol with its guest addresses
  (stub, __nl_symbol_ptr, __la_symbol_ptr, external data relocations, ARM ``bl`` call sites).
* reference/analysis/objc_classes_detailed.json  - Objective-C 2.0 class_t / class_ro_t decoding.
* reference/analysis/resource_formats.json       - PVR / RFF / InGame_txt / dat / boss / comm headers.
* reference/analysis/RUNTIME_CONTRACT_IMPORTS.md - import list grouped by HLE subsystem (appendix).

Use ``--check`` to verify that the committed files match what this script generates.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import struct
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from inspect_apk import parse_macho_armv6  # noqa: E402

PROJECT_JSON = ROOT / "project.json"
ANALYSIS_DIR = ROOT / "reference" / "analysis"
APP_PREFIX = "Payload/Heroes Lore.app/"

S_NON_LAZY_SYMBOL_POINTERS = 0x6
S_LAZY_SYMBOL_POINTERS = 0x7
S_SYMBOL_STUBS = 0x8
INDIRECT_SYMBOL_LOCAL = 0x80000000
INDIRECT_SYMBOL_ABS = 0x40000000

# ---------------------------------------------------------------------------
# Mach-O helpers
# ---------------------------------------------------------------------------


class Image:
    """Minimal guest-address view over the Mach-O file bytes."""

    def __init__(self, data: bytes, inventory: dict, symbols: list[dict]):
        self.data = data
        self.inv = inventory
        self.symbols = symbols
        self.segments = []
        self.sections: dict[str, dict] = {}
        for seg in inventory["segments"]:
            vmaddr = int(seg["vmaddr"], 16)
            self.segments.append((seg["segname"], vmaddr, seg["vmsize"], seg["fileoff"], seg["filesize"]))
            for sec in seg["sections"]:
                self.sections[f"{sec['segname']}.{sec['sectname']}"] = {
                    "addr": int(sec["addr"], 16),
                    "size": sec["size"],
                    "offset": sec["offset"],
                    "flags": int(sec["flags"], 16),
                    "reserved1": sec["reserved1"],
                    "reserved2": sec["reserved2"],
                }

    def off(self, vaddr: int) -> int | None:
        for _name, vmaddr, vmsize, fileoff, filesize in self.segments:
            if vmaddr <= vaddr < vmaddr + vmsize:
                delta = vaddr - vmaddr
                return fileoff + delta if delta < filesize else None
        return None

    def u32(self, vaddr: int) -> int:
        o = self.off(vaddr)
        if o is None:
            raise ValueError(f"unmapped guest address 0x{vaddr:08x}")
        return struct.unpack_from("<I", self.data, o)[0]

    def cstr(self, vaddr: int) -> str:
        o = self.off(vaddr)
        if o is None:
            return ""
        end = self.data.find(b"\x00", o)
        return self.data[o:end].decode("utf-8", errors="replace")

    def section_of(self, vaddr: int) -> str | None:
        for name, sec in self.sections.items():
            if sec["addr"] <= vaddr < sec["addr"] + sec["size"]:
                return name
        return None


def h(value: int) -> str:
    return f"0x{value:08x}"


def read_indirect_table(data: bytes, dysymtab: dict) -> list[int]:
    off = dysymtab["indirectsymoff"]
    return list(struct.unpack_from(f"<{dysymtab['nindirectsyms']}I", data, off))


def read_external_relocs(data: bytes, dysymtab: dict, data_base: int) -> list[tuple[int, int, int, int, int]]:
    """Returns (guest_addr, symbolnum, length, pcrel, type) for each external relocation."""
    out = []
    for i in range(dysymtab["nextrel"]):
        r_address, word = struct.unpack_from("<iI", data, dysymtab["extreloff"] + i * 8)
        symbolnum = word & 0x00FFFFFF
        pcrel = (word >> 24) & 1
        length = (word >> 25) & 3
        extern = (word >> 27) & 1
        rtype = (word >> 28) & 0xF
        if not extern:
            raise ValueError("non-external relocation found in extrel table")
        out.append((data_base + r_address, symbolnum, length, pcrel, rtype))
    return out


def classify_import(name: str, dylib: str | None) -> str:
    base = name[1:] if name.startswith("_") else name
    d = (dylib or "").lower()
    if name.startswith("_OBJC_CLASS_$_") or name.startswith("_OBJC_METACLASS_$_"):
        cls = name.split("$_", 1)[1]
        for prefix, group in (
            ("EAGL", "uikit_eagl_quartzcore"),
            ("NS", "objc_foundation"),
            ("UI", "uikit_eagl_quartzcore"),
            ("CA", "uikit_eagl_quartzcore"),
            ("AV", "audio_openal_avfoundation"),
            ("MP", "audio_openal_avfoundation"),
            ("GK", "offline_neutralized"),
            ("SK", "offline_neutralized"),
        ):
            if cls.startswith(prefix):
                return group
        return "objc_foundation"
    if "opengles" in d or (base.startswith("gl") and len(base) > 2 and base[2].isupper()):
        return "opengles1"
    if "openal" in d or (base.startswith("al") and len(base) > 2 and base[2].isupper()):
        return "audio_openal_avfoundation"
    if any(k in d for k in ("audiotoolbox", "coreaudio", "avfoundation", "mediaplayer")):
        return "audio_openal_avfoundation"
    if "sqlite3" in d or base.startswith("sqlite3_"):
        return "sqlite3"
    if "libz" in d:
        return "zlib"
    if any(k in d for k in ("systemconfiguration", "gamekit", "storekit")):
        return "offline_neutralized"
    if any(k in d for k in ("uikit", "quartzcore")) or base.startswith(("UI", "kEAGL", "CA")):
        return "uikit_eagl_quartzcore"
    if any(k in d for k in ("foundation", "corefoundation", "coregraphics", "libobjc")) or base.startswith(
        ("NS", "CF", "kCF", "CG", "objc_", "_objc_")
    ):
        return "objc_foundation"
    if "libstdc++" in d or "libgcc" in d or base.startswith(("_Z", "__cxa", "__gxx", "__gnu", "_Unwind")):
        return "libc_cxx"
    return "libc_cxx"


def build_bindings(img: Image) -> dict:
    inv = img.inv
    dysym = inv["dysymtab"]
    symbols = img.symbols
    dylibs = [d["name"] for d in inv["dylibs"]]
    indirect = read_indirect_table(img.data, dysym)

    def dylib_of(sym: dict) -> str | None:
        ordinal = (int(sym["n_desc"], 16) >> 8) & 0xFF
        if 1 <= ordinal <= len(dylibs):
            return dylibs[ordinal - 1]
        return None

    imports: dict[int, dict] = {}
    for idx in range(dysym["iundefsym"], dysym["iundefsym"] + dysym["nundefsym"]):
        sym = symbols[idx]
        dl = dylib_of(sym)
        imports[idx] = {
            "symbol_index": idx,
            "name": sym["name"],
            "dylib": dl,
            "group": classify_import(sym["name"], dl),
            "stub_addrs": [],
            "nl_symbol_ptr_addrs": [],
            "la_symbol_ptr_addrs": [],
            "data_relocation_addrs": [],
            "bl_call_sites": [],
        }

    unresolved_local = 0
    defined_pointers: list[dict] = []
    for sec_name, sec in img.sections.items():
        stype = sec["flags"] & 0xFF
        if stype == S_SYMBOL_STUBS:
            count = sec["size"] // sec["reserved2"]
            key, step = "stub_addrs", sec["reserved2"]
        elif stype == S_NON_LAZY_SYMBOL_POINTERS:
            count, key, step = sec["size"] // 4, "nl_symbol_ptr_addrs", 4
        elif stype == S_LAZY_SYMBOL_POINTERS:
            count, key, step = sec["size"] // 4, "la_symbol_ptr_addrs", 4
        else:
            continue
        for i in range(count):
            entry = indirect[sec["reserved1"] + i]
            addr = sec["addr"] + i * step
            if entry & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS):
                unresolved_local += 1
                continue
            if entry not in imports:
                # Pointer to a symbol defined by the image itself (e.g. __mh_execute_header).
                defined_pointers.append({"slot": h(addr), "section": sec_name, "symbol": symbols[entry]["name"],
                                         "value": symbols[entry]["n_value"]})
                continue
            imports[entry][key].append(h(addr))

    # External relocations (data words bound to imported symbols: class refs, CFString isa, vtables...).
    data_base = int(inv["segments"][0]["vmaddr"], 16)  # r_address is relative to the first segment (__PAGEZERO = 0)
    for addr, symnum, length, pcrel, rtype in read_external_relocs(img.data, dysym, data_base):
        if symnum not in imports:
            raise ValueError(f"external relocation references non-import symbol {symnum}")
        if img.section_of(addr) is None or length != 2 or pcrel or rtype != 0:
            raise ValueError(f"unexpected external relocation at 0x{addr:08x}")
        imports[symnum]["data_relocation_addrs"].append(h(addr))

    # Heuristic ARM-mode scan for bl/b into the stub section.
    text = img.sections["__TEXT.__text"]
    stubs = img.sections["__TEXT.__symbol_stub4"]
    stub_to_sym: dict[int, int] = {}
    for sym_idx, imp in imports.items():
        for s in imp["stub_addrs"]:
            stub_to_sym[int(s, 16)] = sym_idx
    for a in range(text["addr"], text["addr"] + text["size"], 4):
        ins = struct.unpack_from("<I", img.data, img.off(a))[0]
        if (ins & 0x0E000000) != 0x0A000000:
            continue
        imm = ins & 0x00FFFFFF
        if imm & 0x800000:
            imm -= 1 << 24
        target = (a + 8 + (imm << 2)) & 0xFFFFFFFF
        if stubs["addr"] <= target < stubs["addr"] + stubs["size"] and target in stub_to_sym:
            kind = "bl" if (ins & 0x01000000) else "b"
            imports[stub_to_sym[target]]["bl_call_sites"].append(f"{h(a)}:{kind}:cond{ins >> 28:x}")

    for imp in imports.values():
        if not (imp["stub_addrs"] or imp["nl_symbol_ptr_addrs"] or imp["data_relocation_addrs"]):
            raise ValueError(f"import {imp['name']} has no stub, pointer or relocation")

    out_imports = []
    for sym_idx in sorted(imports):
        imp = imports[sym_idx]
        imp["bl_call_site_count"] = len(imp["bl_call_sites"])
        imp["bl_call_sites"] = imp["bl_call_sites"][:12]
        out_imports.append(imp)

    group_counts: dict[str, int] = {}
    for imp in out_imports:
        group_counts[imp["group"]] = group_counts.get(imp["group"], 0) + 1

    return {
        "note": "Guest addresses are the unrelocated, non-PIE addresses of the Mach-O image (fixed base 0x1000).",
        "import_count": len(out_imports),
        "indirect_symbol_count": len(indirect),
        "indirect_local_or_abs_entries": unresolved_local,
        "dylibs": dylibs,
        "defined_symbol_pointers": defined_pointers,
        "sections": {
            name: {"addr": h(s["addr"]), "size": s["size"], "reserved1": s["reserved1"], "reserved2": s["reserved2"]}
            for name, s in img.sections.items()
            if (s["flags"] & 0xFF) in (S_NON_LAZY_SYMBOL_POINTERS, S_LAZY_SYMBOL_POINTERS, S_SYMBOL_STUBS)
        },
        "group_counts": dict(sorted(group_counts.items())),
        "imports": out_imports,
    }


def build_objc(img: Image) -> dict:
    inv = img.inv
    dysym = inv["dysymtab"]
    symbols = img.symbols
    data_base = int(inv["segments"][0]["vmaddr"], 16)

    # Imported-symbol bindings applied by dyld to data words (e.g. superclass of an NSObject subclass).
    bound: dict[int, str] = {}
    for addr, symnum, _l, _p, _t in read_external_relocs(img.data, dysym, data_base):
        bound[addr] = symbols[symnum]["name"]

    def ptr(addr: int) -> tuple[int, str | None]:
        """Returns (raw_value, import_symbol_name_if_bound)."""
        return img.u32(addr), bound.get(addr)

    def read_methods(list_addr: int) -> list[dict]:
        if list_addr == 0:
            return []
        entsize_flags, count = img.u32(list_addr), img.u32(list_addr + 4)
        entsize = entsize_flags & ~3
        methods = []
        for i in range(count):
            m = list_addr + 8 + i * entsize
            name_ptr, types_ptr, imp = img.u32(m), img.u32(m + 4), img.u32(m + 8)
            methods.append(
                {
                    "selector": img.cstr(name_ptr),
                    "types": img.cstr(types_ptr),
                    "imp": h(imp & ~1),
                    "imp_isa": "thumb" if imp & 1 else "arm",
                    "entry_addr": h(m),
                }
            )
        return methods

    def read_ivars(list_addr: int) -> list[dict]:
        if list_addr == 0:
            return []
        entsize_flags, count = img.u32(list_addr), img.u32(list_addr + 4)
        entsize = entsize_flags & ~3
        ivars = []
        for i in range(count):
            e = list_addr + 8 + i * entsize
            off_ptr, name_ptr, type_ptr, align, size = (img.u32(e + 4 * k) for k in range(5))
            ivars.append(
                {
                    "name": img.cstr(name_ptr),
                    "type": img.cstr(type_ptr),
                    "offset": img.u32(off_ptr) if off_ptr else None,
                    "size": size,
                    "alignment_log2": align,
                }
            )
        return ivars

    def read_protocol_names(list_addr: int) -> list[str]:
        if list_addr == 0:
            return []
        count = img.u32(list_addr)
        names = []
        for i in range(count):
            p = img.u32(list_addr + 4 + 4 * i)
            names.append(img.cstr(img.u32(p + 4)))
        return names

    def read_ro(class_addr: int) -> dict:
        data_word = img.u32(class_addr + 16)
        ro = data_word & ~3
        flags, inst_start, inst_size, _ivar_layout, name_ptr, base_methods, base_protocols, ivars, _weak, props = (
            img.u32(ro + 4 * k) for k in range(10)
        )
        return {
            "ro_addr": h(ro),
            "flags": h(flags),
            "is_meta": bool(flags & 1),
            "is_root": bool(flags & 2),
            "instance_start": inst_start,
            "instance_size": inst_size,
            "name": img.cstr(name_ptr),
            "methods": read_methods(base_methods),
            "protocols": read_protocol_names(base_protocols),
            "ivars": read_ivars(ivars),
            "has_properties": props != 0,
        }

    classlist = img.sections["__DATA.__objc_classlist"]
    classes = []
    for i in range(classlist["size"] // 4):
        slot = classlist["addr"] + 4 * i
        caddr = img.u32(slot)
        isa, isa_sym = ptr(caddr)
        sup, sup_sym = ptr(caddr + 4)
        ro = read_ro(caddr)
        meta = read_ro(isa) if isa else None
        classes.append(
            {
                "class_addr": h(caddr),
                "metaclass_addr": h(isa),
                "name": ro["name"],
                "superclass_addr": h(sup),
                "superclass_import": sup_sym,
                "superclass_in_image": None,  # filled below
                "instance_size": ro["instance_size"],
                "instance_start": ro["instance_start"],
                "protocols": ro["protocols"],
                "ivars": ro["ivars"],
                "instance_methods": ro["methods"],
                "class_methods": meta["methods"] if meta else [],
                "metaclass_isa_import": isa_sym,
            }
        )
    by_addr = {int(c["class_addr"], 16): c["name"] for c in classes}
    for c in classes:
        c["superclass_in_image"] = by_addr.get(int(c["superclass_addr"], 16))
        if c["superclass_import"]:
            c["superclass"] = c["superclass_import"].replace("_OBJC_CLASS_$_", "")
        else:
            c["superclass"] = c["superclass_in_image"]

    # Selector references -> names (selector pointers point directly at __objc_methname strings).
    selrefs = img.sections["__DATA.__objc_selrefs"]
    selector_refs = []
    for i in range(selrefs["size"] // 4):
        slot = selrefs["addr"] + 4 * i
        selector_refs.append({"slot": h(slot), "selector": img.cstr(img.u32(slot))})

    classrefs = img.sections["__DATA.__objc_classrefs"]
    class_refs = []
    for i in range(classrefs["size"] // 4):
        slot = classrefs["addr"] + 4 * i
        value = img.u32(slot)
        class_refs.append(
            {
                "slot": h(slot),
                "import": bound.get(slot),
                "in_image": by_addr.get(value) if slot not in bound else None,
            }
        )

    protolist = img.sections["__DATA.__objc_protolist"]
    protocols = []
    for i in range(protolist["size"] // 4):
        p = img.u32(protolist["addr"] + 4 * i)
        protocols.append(img.cstr(img.u32(p + 4)))

    total_methods = sum(len(c["instance_methods"]) + len(c["class_methods"]) for c in classes)
    return {
        "class_count": len(classes),
        "protocols": protocols,
        "total_method_count": total_methods,
        "selector_ref_count": len(selector_refs),
        "class_ref_count": len(class_refs),
        "classes": classes,
        "selector_refs": selector_refs,
        "class_refs": class_refs,
    }


# ---------------------------------------------------------------------------
# Resource format probes
# ---------------------------------------------------------------------------

PVR_V1_PIXEL_TYPES = {
    0x10: "OGL_RGBA_4444",
    0x11: "OGL_RGBA_5551",
    0x12: "OGL_RGBA_8888",
    0x13: "OGL_RGB_565",
    0x14: "OGL_RGB_555",
    0x15: "OGL_RGB_888",
    0x16: "OGL_I_8",
    0x17: "OGL_AI_88",
    0x18: "OGL_PVRTC2",
    0x19: "OGL_PVRTC4",
    0x1A: "OGL_BGRA_8888",
}


def probe_pvr(blob: bytes) -> dict:
    if len(blob) < 52:
        return {"error": "short"}
    header_size, height, width, mips, flags, data_size, bpp, rm, gm, bm, am, tag, surfaces = struct.unpack_from(
        "<13I", blob, 0
    )
    info = {
        "version": "PVR v2 (legacy 52-byte header)" if tag == 0x21525650 else "unknown",
        "header_size": header_size,
        "width": width,
        "height": height,
        "mipmap_count": mips,
        "flags": h(flags),
        "pixel_type": h(flags & 0xFF),
        "pixel_format": PVR_V1_PIXEL_TYPES.get(flags & 0xFF, "unknown"),
        "twiddled": bool(flags & 0x200),
        "has_mipmaps": bool(flags & 0x100),
        "alpha_in_header": bool(flags & 0x8000),
        "data_size": data_size,
        "bits_per_pixel": bpp,
        "surface_count": surfaces,
        "file_size": len(blob),
        "size_consistent": header_size + data_size == len(blob),
    }
    return info


def probe_rff(blob: bytes) -> dict:
    if len(blob) < 12:
        return {"error": "short"}
    count_a = struct.unpack_from(">H", blob, 8)[0]
    count_b = struct.unpack_from(">H", blob, 10)[0]
    codes = list(struct.unpack_from(f">{count_a}H", blob, 12)) if 12 + 2 * count_a <= len(blob) else []
    tail = len(blob) - (12 + 2 * count_a)
    return {
        "header_prefix_hex": blob[:8].hex(" "),
        "magic_bytes": blob[:3].hex(" "),
        "glyph_count_be16_at_8": count_a,
        "second_count_be16_at_10": count_b,
        "codepoints_be16_from_offset_12": {
            "count": len(codes),
            "first": codes[:4],
            "last": codes[-3:],
            "min": min(codes) if codes else None,
            "max": max(codes) if codes else None,
            "sorted_non_decreasing": codes == sorted(codes),
        },
        "bytes_after_codepoint_table": tail,
        "bytes_per_glyph_after_table": round(tail / count_a, 3) if count_a else None,
        "file_size": len(blob),
    }


def probe_ingame_txt(blob: bytes) -> dict:
    total = struct.unpack_from("<H", blob, 0)[0]
    count = struct.unpack_from("<H", blob, 2)[0]
    offsets = struct.unpack_from(f"<{count}H", blob, 4)
    strings = []
    for i, start in enumerate(offsets):
        end = blob.find(b"\x00", start)
        strings.append(blob[start:end])
    return {
        "layout": "u16le total_size; u16le string_count; u16le offsets[count] (absolute); NUL-terminated strings",
        "declared_total_size": total,
        "actual_size": len(blob),
        "string_count": count,
        "first_string_offset": offsets[0],
        "offset_table_end": 4 + 2 * count,
        "offsets_monotonic": list(offsets) == sorted(offsets),
        "first_strings": [s.decode("utf-8", errors="replace") for s in strings[:6]],
        "sha256_of_strings": hashlib.sha256(b"\x00".join(strings)).hexdigest(),
    }


def probe_generic_dirs(zf: zipfile.ZipFile, directory: str) -> dict:
    families: dict[str, dict] = {}
    for info in sorted(zf.infolist(), key=lambda x: x.filename):
        if not info.filename.startswith(APP_PREFIX + directory + "/") or info.filename.endswith("/"):
            continue
        name = info.filename.rsplit("/", 1)[1]
        if "_" in name:
            family = "_" + name.rsplit("_", 1)[1]
        elif "." in name:
            family = "." + name.rsplit(".", 1)[1]
        else:
            family = "<none>"
        fam = families.setdefault(family, {"count": 0, "total_bytes": 0, "min_bytes": None, "max_bytes": 0, "sample": None})
        fam["count"] += 1
        fam["total_bytes"] += info.file_size
        fam["min_bytes"] = info.file_size if fam["min_bytes"] is None else min(fam["min_bytes"], info.file_size)
        fam["max_bytes"] = max(fam["max_bytes"], info.file_size)
        if fam["sample"] is None:
            fam["sample"] = {"file": name, "head_hex": zf.read(info.filename)[:16].hex(" ")}
    return dict(sorted(families.items()))


def build_resources(zf: zipfile.ZipFile) -> dict:
    pvrs, rffs = {}, {}
    sprites_spr = {}
    for info in sorted(zf.infolist(), key=lambda x: x.filename):
        name = info.filename
        if not name.startswith(APP_PREFIX) or name.endswith("/"):
            continue
        rel = name[len(APP_PREFIX):]
        low = rel.lower()
        if low.endswith(".pvr"):
            pvrs[rel] = probe_pvr(zf.read(name))
        elif low.endswith(".rff"):
            rffs[rel] = probe_rff(zf.read(name))
        elif low.endswith(".spr"):
            blob = zf.read(name)
            sprites_spr[rel] = {"size": len(blob), "head_hex": blob[:24].hex(" ")}

    audio = {"mp3": [], "wav": []}
    for info in sorted(zf.infolist(), key=lambda x: x.filename):
        if info.filename.startswith(APP_PREFIX + "snd/") and not info.filename.endswith("/"):
            rel = info.filename[len(APP_PREFIX):]
            blob = zf.read(info.filename)
            if rel.endswith(".wav"):
                entry = {"file": rel, "bytes": len(blob), "riff": blob[:4] == b"RIFF"}
                if blob[:4] == b"RIFF" and blob[12:16] == b"fmt ":
                    fmt, ch, rate, _br, _ba, bits = struct.unpack_from("<HHIIHH", blob, 20)
                    entry.update({"format_tag": fmt, "channels": ch, "sample_rate": rate, "bits": bits})
                audio["wav"].append(entry)
            elif rel.endswith(".mp3"):
                audio["mp3"].append({"file": rel, "bytes": len(blob), "head_hex": blob[:4].hex(" ")})

    return {
        "pvr_count": len(pvrs),
        "pvr_pixel_formats": dict(sorted({f: sum(1 for v in pvrs.values() if v.get("pixel_format") == f) for f in {v.get("pixel_format") for v in pvrs.values()}}.items())),
        "pvr": pvrs,
        "rff": rffs,
        "sprites_spr": sprites_spr,
        "in_game_text": probe_ingame_txt(zf.read(APP_PREFIX + "dat/InGame_txt")),
        "dat_families": probe_generic_dirs(zf, "dat"),
        "boss_families": probe_generic_dirs(zf, "boss"),
        "comm_families": probe_generic_dirs(zf, "comm"),
        "audio": audio,
    }


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------


def build_imports_markdown(bindings: dict) -> str:
    lines = [
        "# ApÃªndice: Imports Mach-O agrupados por subsistema HLE (`S02`)",
        "",
        "Gerado deterministicamente por `python tools/probe_macho_contract.py`. NÃ£o editar Ã  mÃ£o.",
        "",
        "| Grupo | Imports |",
        "| --- | ---: |",
    ]
    for group, count in bindings["group_counts"].items():
        lines.append(f"| `{group}` | {count} |")
    lines.append("")
    groups: dict[str, list[dict]] = {}
    for imp in bindings["imports"]:
        groups.setdefault(imp["group"], []).append(imp)
    for group in sorted(groups):
        lines += [f"## `{group}` ({len(groups[group])})", "", "| SÃ­mbolo | Dylib | Stub | Chamadas `bl` |", "| --- | --- | --- | ---: |"]
        for imp in sorted(groups[group], key=lambda i: i["name"]):
            dylib = (imp["dylib"] or "?").rsplit("/", 1)[-1]
            stub = ", ".join(imp["stub_addrs"]) or "-"
            lines.append(f"| `{imp['name']}` | {dylib} | {stub} | {imp['bl_call_site_count']} |")
        lines.append("")
    return "\n".join(lines)


def generate() -> dict[str, str]:
    project = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
    apk_path = ROOT / project["original_apk"]["path"]
    with zipfile.ZipFile(apk_path) as apk:
        ipa_bytes = apk.read("assets/game.ipa")
    if hashlib.sha256(ipa_bytes).hexdigest() != project["nested_ipa"]["sha256"]:
        raise ValueError("Nested IPA SHA-256 mismatch")
    with zipfile.ZipFile(io.BytesIO(ipa_bytes)) as ipa:
        macho = ipa.read(APP_PREFIX + "Heroes Lore")
        if hashlib.sha256(macho).hexdigest() != project["macho_executable"]["sha256"]:
            raise ValueError("Mach-O SHA-256 mismatch")
        inventory, symbols, _objc_meta = parse_macho_armv6(macho)
        img = Image(macho, inventory, symbols)
        bindings = build_bindings(img)
        objc = build_objc(img)
        resources = build_resources(ipa)

    def dump(obj: dict) -> str:
        return json.dumps(obj, indent=2, ensure_ascii=True) + "\n"

    return {
        "macho_import_bindings.json": dump(bindings),
        "objc_classes_detailed.json": dump(objc),
        "resource_formats.json": dump(resources),
        "RUNTIME_CONTRACT_IMPORTS.md": build_imports_markdown(bindings),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify committed analysis files are up to date")
    args = parser.parse_args()
    artifacts = generate()
    if args.check:
        errors = []
        for name, text in artifacts.items():
            path = ANALYSIS_DIR / name
            if not path.exists():
                errors.append(f"Missing analysis file: {path.relative_to(ROOT)}")
            elif path.read_text(encoding="utf-8") != text:
                errors.append(f"Outdated analysis file: {path.relative_to(ROOT)}")
        if errors:
            for e in errors:
                print(f"ERROR: {e}", file=sys.stderr)
            return 1
        print("Mach-O runtime contract probe files are up to date and verified.")
        return 0
    ANALYSIS_DIR.mkdir(parents=True, exist_ok=True)
    for name, text in artifacts.items():
        (ANALYSIS_DIR / name).write_text(text, encoding="utf-8", newline="\n")
    print(f"Wrote {len(artifacts)} artifacts to {ANALYSIS_DIR.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
