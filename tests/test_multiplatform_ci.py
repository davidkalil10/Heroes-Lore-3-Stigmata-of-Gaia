#!/usr/bin/env python3
"""Unit tests for HL3 multiplatform build scripts, Switch/Android verifiers, and CI workflow."""

from __future__ import annotations

import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from hl3rt.version import (  # noqa: E402
    ANDROID_PACKAGE_ID,
    ASSET_ANDROID_APK,
    ASSET_LINUX_X86_64,
    ASSET_SWITCH_NRO,
    ASSET_WINDOWS_X64,
    PORT_VERSION,
    SWITCH_TITLE_ID,
)
from patch_unicorn_common import REPLACEMENTS as COMMON_REPLACEMENTS, patch_unicorn_crc32  # noqa: E402
from patch_unicorn_switch import REPLACEMENTS as SWITCH_REPLACEMENTS, patch_switch_unicorn  # noqa: E402
from verify_android_package import EM_AARCH64, EM_ARM, verify_apk  # noqa: E402
from verify_switch_package import verify_nro  # noqa: E402


def build_minimal_elf(e_machine: int, elf_class: int, exports: list[str], needed: list[str]) -> bytes:
    # Build a minimal little-endian ELF shared object containing .dynstr, .dynsym, and .dynamic.
    dynstr = bytearray(b"\x00")
    sym_offsets = {}
    for sym in exports:
        sym_offsets[sym] = len(dynstr)
        dynstr.extend(sym.encode("utf-8") + b"\x00")
    needed_offsets = []
    for lib in needed:
        needed_offsets.append(len(dynstr))
        dynstr.extend(lib.encode("utf-8") + b"\x00")

    shstr_names = [b"", b".dynstr", b".dynsym", b".dynamic", b".shstrtab"]
    shstrtab = bytearray()
    sh_name_off = {}
    for name in shstr_names:
        sh_name_off[name] = len(shstrtab)
        shstrtab.extend(name + b"\x00")

    if elf_class == 2:
        ehdr_size = 64
        dynsym = bytearray(24)  # STN_UNDEF
        for sym in exports:
            dynsym.extend(struct.pack("<IBBHQQ", sym_offsets[sym], 0x12, 0, 1, 0x1000, 16))
        dynamic = bytearray()
        for off in needed_offsets:
            dynamic.extend(struct.pack("<QQ", 1, off))  # DT_NEEDED
        dynamic.extend(struct.pack("<QQ", 0, 0))
        sh_fmt = "<IIQQQQIIQQ"
        dynsym_ent = 24
        dyn_ent = 16
    else:
        ehdr_size = 52
        dynsym = bytearray(16)
        for sym in exports:
            dynsym.extend(struct.pack("<IIIBBH", sym_offsets[sym], 0x1000, 16, 0x12, 0, 1))
        dynamic = bytearray()
        for off in needed_offsets:
            dynamic.extend(struct.pack("<II", 1, off))
        dynamic.extend(struct.pack("<II", 0, 0))
        sh_fmt = "<IIIIIIIIII"
        dynsym_ent = 16
        dyn_ent = 8

    off_dynstr = ehdr_size
    off_dynsym = off_dynstr + len(dynstr)
    off_dynamic = off_dynsym + len(dynsym)
    off_shstr = off_dynamic + len(dynamic)
    off_shdr = off_shstr + len(shstrtab)

    if elf_class == 2:
        ehdr = struct.pack(
            "<16sHHIQQQIHHHHHH",
            b"\x7fELF\x02\x01\x01" + b"\x00" * 9,
            3,
            e_machine,
            1,
            0,
            0,
            off_shdr,
            0,
            ehdr_size,
            0,
            0,
            64,
            5,
            4,
        )
    else:
        ehdr = struct.pack(
            "<16sHHIIIIIHHHHHH",
            b"\x7fELF\x01\x01\x01" + b"\x00" * 9,
            3,
            e_machine,
            1,
            0,
            0,
            off_shdr,
            0,
            ehdr_size,
            0,
            0,
            40,
            5,
            4,
        )

    shdrs = bytearray()
    shdrs.extend(struct.pack(sh_fmt, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    shdrs.extend(struct.pack(sh_fmt, sh_name_off[b".dynstr"], 3, 2, 0, off_dynstr, len(dynstr), 0, 0, 1, 0))
    shdrs.extend(struct.pack(sh_fmt, sh_name_off[b".dynsym"], 11, 2, 0, off_dynsym, len(dynsym), 1, 1, 8, dynsym_ent))
    shdrs.extend(struct.pack(sh_fmt, sh_name_off[b".dynamic"], 6, 3, 0, off_dynamic, len(dynamic), 1, 0, 8, dyn_ent))
    shdrs.extend(struct.pack(sh_fmt, sh_name_off[b".shstrtab"], 3, 0, 0, off_shstr, len(shstrtab), 0, 0, 1, 0))

    return bytes(ehdr + dynstr + dynsym + dynamic + shstrtab + shdrs)


def build_synthetic_nro(title_id_hex: str, include_cacert: bool = True) -> bytes:
    native = bytearray(0x100)
    native[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", native, 0x18, len(native))

    fake_jpg = b"\xff\xd8\xff\xe0" + b"\x00" * 128 + b"\xff\xd9"
    nacp = bytearray(0x4000)
    tid = int(title_id_hex, 16)
    for off in (0x3038, 0x3078, 0x30B0):
        struct.pack_into("<Q", nacp, off, tid)

    romfs = bytearray(1024 * 1024 + 64)
    marker_parts = [b"game.ipa", b"t_pt-br"]
    if include_cacert:
        marker_parts.extend([b"cacert.pem", b"BEGIN CERTIFICATE"])
    blob = b"|".join(marker_parts)
    romfs[: len(blob)] = blob

    aset_hdr_size = 56
    icon_off = aset_hdr_size
    nacp_off = icon_off + len(fake_jpg)
    romfs_off = nacp_off + len(nacp)

    aset = bytearray(aset_hdr_size)
    aset[0:4] = b"ASET"
    struct.pack_into("<II", aset, 0, int.from_bytes(b"ASET", "little"), 0)
    struct.pack_into("<QQ", aset, 8, icon_off, len(fake_jpg))
    struct.pack_into("<QQ", aset, 24, nacp_off, len(nacp))
    struct.pack_into("<QQ", aset, 40, romfs_off, len(romfs))
    return bytes(native + aset + fake_jpg + nacp + romfs)


class MultiplatformCiTests(unittest.TestCase):
    def test_unicorn_common_patch_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            for rel, old, _ in COMMON_REPLACEMENTS:
                p = base / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(f"// header\n{old}\n", encoding="utf-8")
            self.assertEqual(patch_unicorn_crc32(base), len(COMMON_REPLACEMENTS))
            self.assertEqual(patch_unicorn_crc32(base), 0)

        real_uc = ROOT / "third_party" / "unicorn"
        if real_uc.exists():
            self.assertEqual(patch_unicorn_crc32(real_uc), 0)

    def test_unicorn_switch_patch_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            seen: dict[Path, list[tuple[str, str]]] = {}
            for rel, old, new in (*COMMON_REPLACEMENTS, *SWITCH_REPLACEMENTS):
                seen.setdefault(base / rel, []).append((old, new))
            for p, items in seen.items():
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text("\n".join(old for old, _ in items) + "\n", encoding="utf-8")
            first = patch_switch_unicorn(base)
            self.assertEqual(first, len(COMMON_REPLACEMENTS) + len(SWITCH_REPLACEMENTS))
            self.assertEqual(patch_switch_unicorn(base), 0)

        real_tci = ROOT / "third_party" / "unicorn-tci" / "unicorn-engine-sys-tci"
        if real_tci.exists():
            self.assertEqual(patch_switch_unicorn(real_tci), 0)

    def test_verify_android_package_contract(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            apk = Path(tmp) / "test.apk"
            lib_arm64_main = build_minimal_elf(EM_AARCH64, 2, ["SDL_main"], ["libSDL2.so", "libc++_shared.so", "liblog.so"])
            lib_arm64_sdl = build_minimal_elf(EM_AARCH64, 2, ["SDL_Init"], ["libGLESv2.so", "libOpenSLES.so", "libc.so"])
            lib_arm64_cxx = build_minimal_elf(EM_AARCH64, 2, ["_Znwm"], ["libc.so", "libm.so"])
            lib_arm32_main = build_minimal_elf(EM_ARM, 1, ["SDL_main"], ["libSDL2.so", "libc++_shared.so"])

            with zipfile.ZipFile(apk, "w") as zf:
                zf.writestr("assets/game.ipa", b"ipa-bytes")
                zf.writestr("assets/ui_button_ribbon.png", b"png")
                zf.writestr("assets/icon.png", b"png")
                zf.writestr("assets/fonts/font_mainmenu.rff", b"rff")
                zf.writestr("assets/texts/t_pt-br", b"ptbr")
                zf.writestr("res/mipmap-anydpi-v26/ic_launcher.xml", "<adaptive-icon/>")
                zf.writestr("lib/arm64-v8a/libmain.so", lib_arm64_main)
                zf.writestr("lib/arm64-v8a/libSDL2.so", lib_arm64_sdl)
                zf.writestr("lib/arm64-v8a/libc++_shared.so", lib_arm64_cxx)

            verify_apk(apk, ["arm64-v8a"])

            # Bad DT_NEEDED check
            bad_apk = Path(tmp) / "bad_dep.apk"
            bad_main = build_minimal_elf(EM_AARCH64, 2, ["SDL_main"], ["libmissing_vendor.so"])
            with zipfile.ZipFile(bad_apk, "w") as zf:
                zf.writestr("assets/game.ipa", b"ipa-bytes")
                zf.writestr("assets/ui_button_ribbon.png", b"png")
                zf.writestr("assets/icon.png", b"png")
                zf.writestr("assets/fonts/font_mainmenu.rff", b"rff")
                zf.writestr("assets/texts/t_pt-br", b"ptbr")
                zf.writestr("res/mipmap-anydpi-v26/ic_launcher.xml", "<adaptive-icon/>")
                zf.writestr("lib/arm64-v8a/libmain.so", bad_main)
                zf.writestr("lib/arm64-v8a/libSDL2.so", lib_arm64_sdl)
                zf.writestr("lib/arm64-v8a/libc++_shared.so", lib_arm64_cxx)
                zf.writestr("lib/armeabi-v7a/libmain.so", lib_arm32_main)
            with self.assertRaises(AssertionError):
                verify_apk(bad_apk, ["arm64-v8a"])

    def test_verify_switch_package_contract(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp) / "good.nro"
            good.write_bytes(build_synthetic_nro(SWITCH_TITLE_ID, include_cacert=True))
            verify_nro(good)

            bad_tid = Path(tmp) / "bad_tid.nro"
            bad_tid.write_bytes(build_synthetic_nro("0100000000000000", include_cacert=True))
            with self.assertRaises(AssertionError):
                verify_nro(bad_tid)

            bad_ca = Path(tmp) / "bad_ca.nro"
            bad_ca.write_bytes(build_synthetic_nro(SWITCH_TITLE_ID, include_cacert=False))
            with self.assertRaises(AssertionError):
                verify_nro(bad_ca)

    def test_ci_workflow_and_ids_consistent(self) -> None:
        wf = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
        for job in ("build-windows:", "build-linux:", "build-android:", "build-switch:", "release:"):
            self.assertIn(job, wf)
        for asset in (
            ASSET_WINDOWS_X64,
            ASSET_LINUX_X86_64,
            ASSET_ANDROID_APK,
            ASSET_SWITCH_NRO,
            "HeroesLore3.exe",
            "heroes_lore_3_android_arm64.apk",
            "SHA256SUMS.txt",
        ):
            self.assertIn(asset, wf)

        gradle_txt = (ROOT / "android" / "app" / "build.gradle").read_text(encoding="utf-8")
        self.assertIn(ANDROID_PACKAGE_ID, gradle_txt)
        self.assertIn(PORT_VERSION, gradle_txt)

        manifest_txt = (ROOT / "android" / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")
        self.assertIn("com.davidbraga.heroeslore3.HeroesLore3Activity", manifest_txt)
        self.assertIn("com.davidbraga.heroeslore3.ApkFileProvider", manifest_txt)

        # Ensure HL3 Title ID does not reuse HL4/HL5 identifiers.
        self.assertEqual(SWITCH_TITLE_ID, "0100F3A300030000")
        self.assertNotIn("01005E14", SWITCH_TITLE_ID)
        self.assertNotIn("0100F4A5", SWITCH_TITLE_ID)


if __name__ == "__main__":
    unittest.main()
