#!/usr/bin/env python3
"""Prepare verified HL3 assets, icons, and staging trees for Linux, Android, and Switch."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))

from hl3rt.version import ANDROID_PACKAGE_ID, APP_DISPLAY_NAME, SWITCH_TITLE_ID  # noqa: E402

PORT_RUNTIME_DIRS = ("fonts", "texts")
PORT_RUNTIME_FILES = ("ui_button_ribbon.png", "icon.png")

LINUX_SYSTEM_LIB_PREFIXES = (
    "linux-vdso.",
    "ld-linux",
    "libc.so",
    "libm.so",
    "libpthread.so",
    "libdl.so",
    "librt.so",
    "libresolv.so",
    "libnss_",
    "libGL.so",
    "libGLX.so",
    "libGLdispatch.so",
    "libOpenGL.so",
    "libEGL.so",
    "libGLES",
    "libdrm",
    "libgbm",
    "libwayland-",
    "libX11",
    "libXau",
    "libXdmcp",
    "libXext",
    "libXfixes",
    "libXi",
    "libXrandr",
    "libXrender",
    "libXcursor",
    "libXinerama",
    "libXss",
    "libXxf86vm",
    "libxcb",
    "libxkbcommon",
    "libstdc++.so",
    "libgcc_s.so",
    "libasound.so",
    "libpulse",
    "libpipewire",
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_and_read_ipa() -> bytes:
    proj = json.loads((ROOT / "project.json").read_text(encoding="utf-8"))
    apk_info = proj.get("original_apk", {})
    apk_path = ROOT / apk_info["path"]
    if not apk_path.exists():
        raise FileNotFoundError(f"Original APK missing: {apk_path}")
    apk_bytes = apk_path.read_bytes()
    actual_apk_sha = sha256_bytes(apk_bytes)
    expected_apk_sha = apk_info.get("sha256", "")
    if expected_apk_sha and actual_apk_sha != expected_apk_sha:
        raise ValueError(f"Original APK SHA-256 mismatch: expected {expected_apk_sha}, got {actual_apk_sha}")
    with zipfile.ZipFile(io.BytesIO(apk_bytes)) as zf:
        ipa_bytes = zf.read("assets/game.ipa")
    return ipa_bytes


def write_verified_ipa(dest_file: Path) -> None:
    ipa_bytes = verify_and_read_ipa()
    expected_sha = sha256_bytes(ipa_bytes)
    if dest_file.exists() and sha256_bytes(dest_file.read_bytes()) == expected_sha:
        return
    dest_file.parent.mkdir(parents=True, exist_ok=True)
    dest_file.write_bytes(ipa_bytes)


def copy_runtime_overrides(assets_dest: Path) -> None:
    src_assets = ROOT / "assets"
    assets_dest.mkdir(parents=True, exist_ok=True)
    for rel_dir in PORT_RUNTIME_DIRS:
        src = src_assets / rel_dir
        dst = assets_dest / rel_dir
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(src, dst)
    for rel_file in PORT_RUNTIME_FILES:
        src = src_assets / rel_file
        if src.exists():
            shutil.copy2(src, assets_dest / rel_file)


def generate_android_icons(res_dir: Path) -> None:
    from PIL import Image

    icon_rgba = Image.open(ROOT / "assets" / "icon.png").convert("RGBA")
    icon_rgb = icon_rgba.convert("RGB")

    mipmaps = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }
    for folder, px in mipmaps.items():
        d = res_dir / folder
        d.mkdir(parents=True, exist_ok=True)
        resized = icon_rgba.resize((px, px), Image.Resampling.LANCZOS)
        resized.save(d / "ic_launcher.png")
        resized.save(d / "ic_launcher_round.png")

    # 108dp canvas with centered 72dp safe region (216..1080 -> 432 canvas with 288 artwork).
    drawable = res_dir / "drawable"
    drawable.mkdir(parents=True, exist_ok=True)
    fg = Image.new("RGBA", (432, 432), (24, 30, 42, 255))
    fg.paste(icon_rgb.resize((288, 288), Image.Resampling.LANCZOS), (72, 72))
    fg.save(drawable / "ic_launcher_foreground.png")

    anydpi = res_dir / "mipmap-anydpi-v26"
    anydpi.mkdir(parents=True, exist_ok=True)
    adaptive_xml = (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
        '    <background android:drawable="@color/ic_launcher_background" />\n'
        '    <foreground android:drawable="@drawable/ic_launcher_foreground" />\n'
        "</adaptive-icon>\n"
    )
    (anydpi / "ic_launcher.xml").write_text(adaptive_xml, encoding="utf-8", newline="\n")
    (anydpi / "ic_launcher_round.xml").write_text(adaptive_xml, encoding="utf-8", newline="\n")

    values = res_dir / "values"
    values.mkdir(parents=True, exist_ok=True)
    (values / "ic_launcher_background.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        "<resources>\n"
        '    <color name="ic_launcher_background">#181E2A</color>\n'
        "</resources>\n",
        encoding="utf-8",
        newline="\n",
    )


def ensure_android_keystore() -> Path:
    keystore = ROOT / "android" / "app" / "heroes_lore_3.keystore"
    if keystore.exists():
        return keystore
    keystore.parent.mkdir(parents=True, exist_ok=True)
    keytool = shutil.which("keytool")
    if not keytool:
        raise FileNotFoundError("keytool not found in PATH (required to create Android signing keystore)")
    subprocess.run(
        [
            keytool,
            "-genkeypair",
            "-v",
            "-keystore",
            str(keystore),
            "-alias",
            "heroeslore3",
            "-keyalg",
            "RSA",
            "-keysize",
            "2048",
            "-validity",
            "10000",
            "-storepass",
            "heroeslore3",
            "-keypass",
            "heroeslore3",
            "-dname",
            "CN=David Kalil Braga, OU=HeroesLore3Recomp, O=DavidKalilBraga, L=BR, C=BR",
        ],
        check=True,
    )
    return keystore


def prepare_android(with_keystore: bool = True) -> None:
    build_root = ROOT / "android" / "app" / "build"
    assets_dir = build_root / "staged-assets"
    res_dir = build_root / "staged-res"
    write_verified_ipa(assets_dir / "game.ipa")
    copy_runtime_overrides(assets_dir)
    generate_android_icons(res_dir)
    if with_keystore:
        ensure_android_keystore()
    print(f"OK: Staged Android assets ({assets_dir}), icons ({res_dir}), package {ANDROID_PACKAGE_ID}")


def prepare_switch(romfs_dir: Path, icon_out: Path | None) -> None:
    from PIL import Image

    romfs_dir = romfs_dir.resolve()
    assets_dir = romfs_dir / "assets"
    write_verified_ipa(assets_dir / "game.ipa")
    copy_runtime_overrides(assets_dir)

    cacert = ROOT / "third_party" / "certs" / "cacert.pem"
    if not cacert.exists():
        raise FileNotFoundError(f"Missing Switch CA bundle: {cacert} (run tools/fetch_build_deps.py --switch first)")
    shutil.copy2(cacert, romfs_dir / "cacert.pem")

    if icon_out is not None:
        icon_out.parent.mkdir(parents=True, exist_ok=True)
        img = Image.open(ROOT / "assets" / "icon.png").convert("RGB").resize((256, 256), Image.Resampling.LANCZOS)
        # Switch hbmenu requires non-progressive baseline JPEG 256x256.
        img.save(icon_out, format="JPEG", quality=95, progressive=False)
    print(f"OK: Staged Switch RomFS ({romfs_dir}) for Title ID {SWITCH_TITLE_ID}")


def prepare_linux_appdir(exe_path: Path, appdir: Path, bundle_libs: bool = True) -> None:
    from PIL import Image

    if not exe_path.exists():
        raise FileNotFoundError(f"Linux binary missing: {exe_path}")
    appdir = appdir.resolve()
    if appdir.exists():
        shutil.rmtree(appdir)
    bindir = appdir / "usr" / "bin"
    libdir = appdir / "usr" / "lib"
    bindir.mkdir(parents=True, exist_ok=True)
    libdir.mkdir(parents=True, exist_ok=True)

    dst_exe = bindir / "HeroesLore3"
    shutil.copy2(exe_path, dst_exe)
    dst_exe.chmod(0o755)

    assets_dir = bindir / "assets"
    write_verified_ipa(assets_dir / "game.ipa")
    copy_runtime_overrides(assets_dir)

    icon_img = Image.open(ROOT / "assets" / "icon.png").convert("RGBA").resize((256, 256), Image.Resampling.LANCZOS)
    icon_img.save(appdir / "heroes_lore_3.png")

    desktop = (
        "[Desktop Entry]\n"
        "Type=Application\n"
        f"Name={APP_DISPLAY_NAME}\n"
        "Exec=HeroesLore3\n"
        "Icon=heroes_lore_3\n"
        "Categories=Game;RolePlaying;\n"
        "Terminal=false\n"
    )
    (appdir / "heroes_lore_3.desktop").write_text(desktop, encoding="utf-8", newline="\n")

    apprun = (
        "#!/bin/sh\n"
        'HERE="$(dirname "$(readlink -f "$0")")"\n'
        'export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
        'exec "$HERE/usr/bin/HeroesLore3" "$@"\n'
    )
    apprun_path = appdir / "AppRun"
    apprun_path.write_text(apprun, encoding="utf-8", newline="\n")
    apprun_path.chmod(0o755)

    if bundle_libs and sys.platform.startswith("linux"):
        ldd_out = subprocess.check_output(["ldd", str(dst_exe)], text=True)
        for line in ldd_out.splitlines():
            parts = line.split()
            if "=>" not in parts:
                continue
            name = parts[0]
            if name.startswith(LINUX_SYSTEM_LIB_PREFIXES):
                continue
            idx = parts.index("=>")
            if idx + 1 >= len(parts):
                continue
            src_lib = Path(parts[idx + 1])
            if src_lib.is_file():
                shutil.copy2(src_lib, libdir / name)

    print(f"OK: Staged Linux AppDir at {appdir}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_ipa = sub.add_parser("extract-ipa", help="Extract & verify assets/game.ipa from original APK")
    p_ipa.add_argument("--out", type=Path, default=ROOT / "assets" / "game.ipa")

    p_and = sub.add_parser("android", help="Stage Android APK assets, launcher icons, and keystore")
    p_and.add_argument("--no-keystore", action="store_true")

    p_sw = sub.add_parser("switch", help="Stage Switch RomFS directory and 256x256 JPEG icon")
    p_sw.add_argument("--romfs", type=Path, default=ROOT / "build_switch" / "romfs")
    p_sw.add_argument("--icon-out", type=Path, default=ROOT / "build_switch" / "icon.jpg")

    p_lin = sub.add_parser("linux", help="Stage Linux AppDir for appimagetool")
    p_lin.add_argument("--exe", type=Path, required=True)
    p_lin.add_argument("--appdir", type=Path, default=ROOT / "AppDir")
    p_lin.add_argument("--no-libs", action="store_true")

    args = parser.parse_args()
    if args.cmd == "extract-ipa":
        write_verified_ipa(args.out)
        print(f"OK: Extracted verified game.ipa to {args.out}")
    elif args.cmd == "android":
        prepare_android(with_keystore=not args.no_keystore)
    elif args.cmd == "switch":
        prepare_switch(args.romfs, args.icon_out)
    elif args.cmd == "linux":
        prepare_linux_appdir(args.exe, args.appdir, bundle_libs=not args.no_libs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

