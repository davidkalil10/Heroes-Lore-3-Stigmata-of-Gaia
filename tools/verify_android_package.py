#!/usr/bin/env python3
"""Verify built HL3 Android APKs (assets, Mach-O IPA, ABIs, SDL_main, DT_NEEDED closure, icons)."""

from __future__ import annotations

import argparse
import hashlib
import struct
import zipfile
from pathlib import Path

EM_ARM = 40
EM_X86_64 = 62
EM_AARCH64 = 183

ABI_MACHINES: dict[str, tuple[int, int]] = {
    "arm64-v8a": (EM_AARCH64, 2),
    "armeabi-v7a": (EM_ARM, 1),
    "x86_64": (EM_X86_64, 2),
}

ALLOWED_ANDROID_SYSTEM_LIBS = {
    "libc.so",
    "libm.so",
    "libdl.so",
    "liblog.so",
    "libandroid.so",
    "libGLESv1_CM.so",
    "libGLESv2.so",
    "libEGL.so",
    "libOpenSLES.so",
    "libz.so",
}


def inspect_elf(data: bytes, name: str) -> tuple[int, int, set[str], list[str]]:
    if len(data) < 64 or data[:4] != b"\x7fELF":
        raise ValueError(f"Not a valid ELF shared object: {name}")
    elf_class = data[4]
    if data[5] != 1:
        raise ValueError(f"Expected little-endian ELF: {name}")

    if elf_class == 2:
        e_machine = struct.unpack_from("<H", data, 18)[0]
        e_shoff = struct.unpack_from("<Q", data, 40)[0]
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 58)
        sh_fmt = "<IIQQQQIIQQ"
    elif elf_class == 1:
        e_machine = struct.unpack_from("<H", data, 18)[0]
        e_shoff = struct.unpack_from("<I", data, 32)[0]
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 46)
        sh_fmt = "<IIIIIIIIII"
    else:
        raise ValueError(f"Unsupported ELF class {elf_class} in {name}")

    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sections.append(struct.unpack_from(sh_fmt, data, off))

    def read_cstr(blob: bytes, offset: int) -> str:
        end = blob.find(b"\x00", offset)
        return blob[offset:end].decode("utf-8", errors="replace") if end >= 0 else ""

    dynsym_exports: set[str] = set()
    needed: list[str] = []

    for sh in sections:
        _, sh_type, _, _, sh_offset, sh_size, sh_link, _, _, sh_entsize = sh
        if sh_link >= len(sections):
            continue
        strtab_off = sections[sh_link][4]
        strtab_sz = sections[sh_link][5]
        strtab = data[strtab_off : strtab_off + strtab_sz]

        if sh_type == 11 and sh_entsize > 0:  # SHT_DYNSYM
            count = sh_size // sh_entsize
            for i in range(count):
                rec = sh_offset + i * sh_entsize
                if elf_class == 2:
                    st_name, _, _, st_shndx = struct.unpack_from("<IBBH", data, rec)
                else:
                    st_name, _, _, _, _, st_shndx = struct.unpack_from("<IIIBBH", data, rec)
                if st_shndx != 0 and st_name > 0:
                    dynsym_exports.add(read_cstr(strtab, st_name))

        elif sh_type == 6:  # SHT_DYNAMIC
            step = 16 if elf_class == 2 else 8
            fmt = "<QQ" if elf_class == 2 else "<II"
            for off in range(sh_offset, sh_offset + sh_size, step):
                d_tag, d_val = struct.unpack_from(fmt, data, off)
                if d_tag == 0:
                    break
                if d_tag == 1:  # DT_NEEDED
                    needed.append(read_cstr(strtab, d_val))

    return e_machine, elf_class, dynsym_exports, needed


def verify_apk(apk_path: Path, abis: list[str]) -> None:
    if not apk_path.exists():
        raise FileNotFoundError(f"APK not found: {apk_path}")

    with zipfile.ZipFile(apk_path) as zf:
        names = set(zf.namelist())
        required_assets = [
            "assets/game.ipa",
            "assets/ui_button_ribbon.png",
            "assets/icon.png",
            "assets/fonts/font_mainmenu.rff",
            "assets/texts/t_pt-br",
        ]
        for req in required_assets:
            if req not in names:
                raise AssertionError(f"Missing APK asset {req} in {apk_path.name}")

        ipa_sha = hashlib.sha256(zf.read("assets/game.ipa")).hexdigest()
        adaptive = [n for n in names if n.startswith("res/mipmap-anydpi-v26/") and n.endswith("ic_launcher.xml")]
        if not adaptive:
            # AGP release builds with resource optimization shorten res/ paths (e.g. res/BW.xml),
            # while keeping compiled binary adaptive-icon XMLs and ic_launcher inside resources.arsc.
            arsc = zf.read("resources.arsc") if "resources.arsc" in names else b""
            adaptive_bin = []
            for n in names:
                if n.startswith("res/") and n.endswith(".xml"):
                    raw = zf.read(n)
                    if b"adaptive-icon" in raw or "adaptive-icon".encode("utf-16le") in raw:
                        adaptive_bin.append(n)
            if not adaptive_bin or b"ic_launcher" not in arsc:
                raise AssertionError(f"Missing adaptive launcher icon XML in {apk_path.name}")

        actual_abis = {n.split("/")[1] for n in names if n.startswith("lib/") and n.endswith(".so")}
        if actual_abis != set(abis):
            raise AssertionError(f"Expected ABIs {sorted(abis)}, found {sorted(actual_abis)} in {apk_path.name}")

        for abi in abis:
            for lib in ("libmain.so", "libSDL2.so", "libc++_shared.so"):
                key = f"lib/{abi}/{lib}"
                if key not in names:
                    raise AssertionError(f"Missing native library {key} in {apk_path.name}")

        for n in sorted(names):
            if not n.startswith("lib/") or not n.endswith(".so"):
                continue
            abi = n.split("/")[1]
            e_machine, elf_class, exports, needed = inspect_elf(zf.read(n), n)
            exp_machine, exp_class = ABI_MACHINES[abi]
            if (e_machine, elf_class) != (exp_machine, exp_class):
                raise AssertionError(f"ABI mismatch for {n}: got ({e_machine}, {elf_class})")
            if n.endswith("/libmain.so") and "SDL_main" not in exports:
                raise AssertionError(f"Missing SDL_main symbol in {n}")
            if n.endswith("/libSDL2.so"):
                for sym in (
                    "Java_org_libsdl_app_SDLActivity_nativeSetupJNI",
                    "Java_org_libsdl_app_HIDDeviceManager_HIDDeviceRegisterCallback",
                ):
                    if sym not in exports:
                        raise AssertionError(f"Missing JNI export {sym} in {n}")
            for dep in needed:
                if dep not in ALLOWED_ANDROID_SYSTEM_LIBS and f"lib/{abi}/{dep}" not in names:
                    raise AssertionError(f"Unbundled DT_NEEDED dependency {dep} in {n}")

    print(f"PASS Android APK verified: {apk_path.name} (ABIs={','.join(abis)}, ipa_sha={ipa_sha[:12]})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--abis", nargs="+", default=["arm64-v8a"])
    args = parser.parse_args()
    verify_apk(args.apk, args.abis)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
