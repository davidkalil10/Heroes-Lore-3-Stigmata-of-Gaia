#!/usr/bin/env python3
"""Package the standalone native C++17 Heroes Lore 3 PC distribution and Windows ZIP."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.version import (
    ASSET_WINDOWS_X64,
    GITHUB_REPOSITORY,
    ORIGINAL_GAME_VERSION,
    PORT_AUTHOR,
    PORT_VERSION,
    PORT_YEAR,
)

DEFAULT_APK = ROOT / "Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk"
DEFAULT_EXE = ROOT / "build_cpp" / "HeroesLore3.exe"
DIST_DIR = ROOT / "dist"
STAGE_DIR = DIST_DIR / "HeroesLore3_PC"
ZIP_PATH = DIST_DIR / ASSET_WINDOWS_X64

FORBIDDEN_NONSYSTEM_DLLS = (
    "libgcc",
    "libstdc++",
    "libwinpthread",
    "libzlib",
    "zlib1",
    "sdl2",
    "msys-",
)

README_TEMPLATE = """========================================================================
  HEROES LORE 3: STIGMATA OF GAIA - STANDALONE PC EDITION (C++17)
  Port by {author} ({year}) - v{port_version}
  Original iOS Game Version: {original_version} (com.ea.heroesloresg.bv)
========================================================================

100% Native C++17 + SDL2 + OpenGL + Unicorn ARMv6 + OpenAL Audio Mixer.
Single standalone executable - NO Python, NO Java, NO external DLLs,
and NO redundant Android APK wrapper required.

HOW TO PLAY:
  Double-click `HeroesLore3.exe`.
  Saves are stored automatically in `data/internal/Documents/`.
  Settings are persisted in `config/` (`audio.json`, `display.json`,
  `services.json`).

RECOMMENDED EXTRACT LOCATION:
  Extract to a normal user folder (e.g. Desktop or C:\\Games\\HeroesLore3)
  so local saves, Cloud Save tokens, and OTA updates can write cleanly
  without Administrator privileges.

HOTKEYS & CONTROLS:
  Move / Navigate ....... WASD / Arrow Keys / Left Stick / D-Pad
  Interact / Confirm .... E / Enter / Space / Z / Gamepad A
  Attack / Skill 1..3 ... J / L / H / U, I, O (3, 4, 5) / Gamepad X, Y
  Potions 1 / 2 ......... Q (1) / K (2) / Gamepad LB, RB
  Pause / Menu .......... P / C / Gamepad Start
  Back / Close .......... Esc / Backspace / Gamepad B
  Skip Cutscene ......... S / Tab / Gamepad Y
  Minimap ............... M / Gamepad Select
  Audio Volume / Mute ... + / - / N
  Live Language (EN/PT-BR) F7  (boots in English; instant live switch)
  Target FPS (30/60) .... F8
  Aspect Ratio Cycle .... F9   (Auto -> 16:9 -> Original 3:2)
  Fullscreen Toggle ..... F11
  Screenshot (PNG) ...... F12  (saved to `screenshots/`)

PORT OPTIONS, CLOUD SAVE & UPDATES:
  Open from Title Screen -> Help & About -> Port Options & Cloud
  or In-Game Pause Menu -> System -> Port Options & Cloud
  - Switch FPS (30/60), Aspect Ratio, and Language (EN / PT-BR) live
  - Back up and restore saves safely via Google Drive OAuth Device Flow
  - Check & install updates directly from GitHub Releases:
    https://github.com/{github_repo}/releases

CLI LAUNCH EXAMPLES:
  HeroesLore3.exe --aspect 16:9 --fps 60
  HeroesLore3.exe --fullscreen
  HeroesLore3.exe --scale 3
  HeroesLore3.exe --lang pt-br
"""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def find_msys_tool(name: str) -> str | None:
    found = shutil.which(name)
    if found:
        return found
    candidate = Path(r"C:\msys64\ucrt64\bin") / f"{name}.exe"
    if candidate.exists():
        return str(candidate)
    return None


def verify_static_pe(exe_path: Path) -> None:
    data = bytearray(exe_path.read_bytes())
    if len(data) < 0x100 or data[:2] != b"MZ":
        raise RuntimeError(f"Invalid PE executable: {exe_path}")
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_off : pe_off + 4] != b"PE\x00\x00":
        raise RuntimeError(f"Missing PE signature in {exe_path}")
    opt_off = pe_off + 24
    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic != 0x20B:
        raise RuntimeError(f"Expected PE32+ (x64) image in {exe_path}, got 0x{magic:04x}")
    dll_chars = struct.unpack_from("<H", data, opt_off + 70)[0]
    if dll_chars & 0x4000:
        # Clear IMAGE_DLLCHARACTERISTICS_GUARD_CF cleanly if present so Unicorn JIT longjmp works.
        struct.pack_into("<H", data, opt_off + 70, dll_chars & ~0x4000)
        exe_path.write_bytes(bytes(data))

    objdump = find_msys_tool("objdump")
    if objdump:
        proc = subprocess.run([objdump, "-p", str(exe_path)], capture_output=True, text=True, check=False)
        if proc.returncode == 0:
            bad_dlls: list[str] = []
            for line in proc.stdout.splitlines():
                if "DLL Name:" not in line:
                    continue
                dll = line.split("DLL Name:", 1)[1].strip().lower()
                if any(tok in dll for tok in FORBIDDEN_NONSYSTEM_DLLS):
                    bad_dlls.append(dll)
            if bad_dlls:
                raise RuntimeError(f"Non-static runtime DLL dependency detected: {', '.join(bad_dlls)}")


def extract_verified_ipa(apk_path: Path, out_ipa_path: Path, project_meta: dict) -> None:
    nested = project_meta.get("nested_ipa", {})
    ipa_entry = nested.get("apk_entry", "assets/game.ipa")
    expected_bytes = int(nested.get("bytes", 0))
    expected_sha = str(nested.get("sha256", "")).lower()

    if not apk_path.exists():
        raise FileNotFoundError(f"Original APK not found: {apk_path}")

    with zipfile.ZipFile(apk_path, "r") as zf:
        raw_ipa = zf.read(ipa_entry)

    if expected_bytes and len(raw_ipa) != expected_bytes:
        raise RuntimeError(f"Extracted {ipa_entry} byte count {len(raw_ipa)} != expected {expected_bytes}")
    actual_sha = sha256_bytes(raw_ipa).lower()
    if expected_sha and actual_sha != expected_sha:
        raise RuntimeError(f"Extracted {ipa_entry} SHA-256 mismatch: {actual_sha} != {expected_sha}")

    out_ipa_path.parent.mkdir(parents=True, exist_ok=True)
    out_ipa_path.write_bytes(raw_ipa)


def build_cpp_release() -> None:
    env = os.environ.copy()
    msys_paths = [r"C:\msys64\ucrt64\bin", r"C:\msys64\usr\bin"]
    existing_paths = [p for p in msys_paths if Path(p).exists()]
    if existing_paths:
        env["PATH"] = os.pathsep.join(existing_paths) + os.pathsep + env.get("PATH", "")

    cmake_bin = find_msys_tool("cmake") or shutil.which("cmake", path=env.get("PATH", "")) or "cmake"
    if not (ROOT / "third_party" / "unicorn" / "CMakeLists.txt").exists():
        subprocess.run([sys.executable, str(ROOT / "tools" / "fetch_build_deps.py"), "--windows"], cwd=ROOT, env=env, check=True)
    if not (ROOT / "build_cpp" / "build.ninja").exists():
        subprocess.run(
            [cmake_bin, "-B", "build_cpp", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"],
            cwd=ROOT,
            env=env,
            check=True,
        )
    subprocess.run(
        [cmake_bin, "--build", "build_cpp", "--target", "HeroesLore3", "-j"],
        cwd=ROOT,
        env=env,
        check=True,
    )


def copy_runtime_assets(dst_assets: Path) -> None:
    src_assets = ROOT / "assets"
    for fname in ("icon.ico", "icon.png", "ui_button_ribbon.png"):
        src = src_assets / fname
        if src.exists():
            shutil.copy2(src, dst_assets / fname)

    for sub in ("fonts", "texts", "locales", "branding", "ui"):
        src_sub = src_assets / sub
        if src_sub.exists():
            shutil.copytree(src_sub, dst_assets / sub, dirs_exist_ok=True)


def write_clean_configs(dst_config: Path) -> None:
    dst_config.mkdir(parents=True, exist_ok=True)
    audio_cfg = {
        "master_volume": 1.0,
        "bgm_volume": 0.61,
        "sfx_volume": 1.0,
        "muted": False,
    }
    display_cfg = {
        "aspect_mode": "auto",
        "target_fps": 30,
        "language": "en",
    }
    services_cfg = {
        "auto_check_updates": True,
    }
    (dst_config / "audio.json").write_text(json.dumps(audio_cfg, indent=2) + "\n", encoding="utf-8", newline="\n")
    (dst_config / "display.json").write_text(json.dumps(display_cfg, indent=2) + "\n", encoding="utf-8", newline="\n")
    (dst_config / "services.json").write_text(
        json.dumps(services_cfg, indent=2) + "\n", encoding="utf-8", newline="\n"
    )


def prepare_clean_data(dst_data: Path, include_dev_saves: bool) -> None:
    docs = dst_data / "internal" / "Documents"
    ext = dst_data / "external"
    docs.mkdir(parents=True, exist_ok=True)
    ext.mkdir(parents=True, exist_ok=True)
    (docs / ".gitkeep").write_text("", encoding="utf-8")
    (ext / ".gitkeep").write_text("", encoding="utf-8")

    if include_dev_saves:
        src_docs = ROOT / "data" / "internal" / "Documents"
        if src_docs.exists():
            for item in src_docs.rglob("*"):
                if item.is_file() and item.name != ".gitkeep":
                    rel = item.relative_to(src_docs)
                    dest = docs / rel
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(item, dest)


def create_release_zip(stage_dir: Path, zip_path: Path) -> None:
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    if zip_path.exists():
        zip_path.unlink()

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for file_path in sorted(stage_dir.rglob("*")):
            if not file_path.is_file():
                continue
            if file_path.suffix.lower() in (".apk", ".log"):
                continue
            arcname = file_path.relative_to(stage_dir).as_posix()
            zf.write(file_path, arcname)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", action="store_true", help="Compile Release C++ HeroesLore3.exe before packaging")
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE, help="Path to compiled HeroesLore3.exe")
    parser.add_argument("--no-strip", action="store_true", help="Skip stripping symbols on the packaged executable")
    parser.add_argument("--include-dev-saves", action="store_true", help="Copy local saves into the package")
    parser.add_argument("--skip-zip", action="store_true", help="Do not create the Windows x64 release ZIP")
    args = parser.parse_args()

    project_meta = json.loads((ROOT / "project.json").read_text(encoding="utf-8"))
    if project_meta.get("port_version") != PORT_VERSION:
        raise RuntimeError(
            f"Version mismatch: project.json={project_meta.get('port_version')!r} != version.py={PORT_VERSION!r}"
        )

    exe_src = args.exe.resolve()
    if args.build or not exe_src.exists():
        build_cpp_release()
        exe_src = DEFAULT_EXE.resolve()

    if not exe_src.exists():
        raise FileNotFoundError(f"Compiled HeroesLore3.exe not found: {exe_src}")

    if STAGE_DIR.exists():
        shutil.rmtree(STAGE_DIR)
    STAGE_DIR.mkdir(parents=True, exist_ok=True)

    exe_dst = STAGE_DIR / "HeroesLore3.exe"
    shutil.copy2(exe_src, exe_dst)
    if not args.no_strip:
        strip_bin = find_msys_tool("strip")
        if strip_bin:
            subprocess.run([strip_bin, "-s", str(exe_dst)], check=True)
    verify_static_pe(exe_dst)

    shutil.copy2(ROOT / "project.json", STAGE_DIR / "project.json")
    (STAGE_DIR / "README.txt").write_text(
        README_TEMPLATE.format(
            author=PORT_AUTHOR,
            year=PORT_YEAR,
            port_version=PORT_VERSION,
            original_version=ORIGINAL_GAME_VERSION,
            github_repo=GITHUB_REPOSITORY,
        ),
        encoding="utf-8",
        newline="\n",
    )

    dst_assets = STAGE_DIR / "assets"
    extract_verified_ipa(DEFAULT_APK, dst_assets / "game.ipa", project_meta)
    copy_runtime_assets(dst_assets)
    write_clean_configs(STAGE_DIR / "config")
    prepare_clean_data(STAGE_DIR / "data", args.include_dev_saves)

    exe_bytes = exe_dst.stat().st_size
    print(f"Packaged standalone C++ EXE: {exe_dst} ({exe_bytes:,} bytes, SHA256={sha256_file(exe_dst)})")

    if not args.skip_zip:
        create_release_zip(STAGE_DIR, ZIP_PATH)
        zip_bytes = ZIP_PATH.stat().st_size
        print(f"Packaged Windows x64 ZIP:   {ZIP_PATH} ({zip_bytes:,} bytes, SHA256={sha256_file(ZIP_PATH)})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
