#!/usr/bin/env python3
"""Fetch pinned C++ third-party dependencies for HL3 builds (never reuses other ports' IDs)."""

from __future__ import annotations

import argparse
import shutil
import ssl
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))

from patch_unicorn_common import patch_unicorn_crc32  # noqa: E402
from patch_unicorn_switch import patch_switch_unicorn  # noqa: E402

DEPS: dict[str, tuple[str, str]] = {
    "unicorn": ("https://github.com/unicorn-engine/unicorn.git", "8028ec436f2d9376525352dd38ed9ed6b9f6be10"),
    "unicorn-tci": ("https://github.com/1rhino2/unicorn-tci.git", "e3f075a62742913bb734a55a474aec65c9e5745e"),
    "SDL2": ("https://github.com/libsdl-org/SDL.git", "release-2.30.12"),
}


def ensure_repo(name: str) -> Path:
    url, ref = DEPS[name]
    folder = ROOT / "third_party" / name
    if not (folder / ".git").exists():
        folder.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "init", str(folder)], check=True)
        subprocess.run(["git", "-C", str(folder), "remote", "add", "origin", url], check=True)
    else:
        cur = subprocess.run(["git", "-C", str(folder), "rev-parse", "HEAD"], capture_output=True, text=True)
        if cur.returncode == 0 and cur.stdout.strip() == ref:
            return folder
        # Handle tag refs like release-2.30.12 cleanly if already checked out.
        tag_cur = subprocess.run(
            ["git", "-C", str(folder), "describe", "--tags", "--exact-match"], capture_output=True, text=True
        )
        if tag_cur.returncode == 0 and tag_cur.stdout.strip() == ref:
            return folder

    subprocess.run(["git", "-C", str(folder), "fetch", "--depth", "1", "origin", ref], check=True)
    subprocess.run(["git", "-C", str(folder), "checkout", "--detach", "FETCH_HEAD"], check=True)
    return folder


def ensure_cacert() -> Path:
    dest = ROOT / "third_party" / "certs" / "cacert.pem"
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and "BEGIN CERTIFICATE" in dest.read_text(encoding="ascii", errors="ignore"):
        return dest

    try:
        import certifi  # type: ignore

        src = Path(certifi.where())
        if src.exists():
            shutil.copy2(src, dest)
            return dest
    except Exception:
        pass

    ctx = ssl.create_default_context()
    req = urllib.request.Request("https://curl.se/ca/cacert.pem", headers={"User-Agent": "HL3Recomp-Build/1.0"})
    with urllib.request.urlopen(req, context=ctx, timeout=30) as resp:
        data = resp.read()
    if b"BEGIN CERTIFICATE" not in data:
        raise RuntimeError("Downloaded cacert.pem does not contain PEM certificates")
    dest.write_bytes(data)
    return dest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows", action="store_true")
    parser.add_argument("--linux", action="store_true")
    parser.add_argument("--android", action="store_true")
    parser.add_argument("--switch", action="store_true")
    parser.add_argument("--tci", action="store_true")
    parser.add_argument("--sdl", action="store_true")
    parser.add_argument("--all", action="store_true")
    args = parser.parse_args()

    want_switch = args.switch or args.tci or args.all
    want_unicorn = args.windows or args.linux or args.android or args.all or (not want_switch)
    want_sdl = args.android or args.sdl or args.all

    if want_unicorn:
        uc_dir = ensure_repo("unicorn")
        patch_unicorn_crc32(uc_dir)
        print(f"OK: unicorn ready at {uc_dir}")

    if want_switch:
        tci_dir = ensure_repo("unicorn-tci")
        patch_switch_unicorn(tci_dir / "unicorn-engine-sys-tci")
        ca = ensure_cacert()
        print(f"OK: unicorn-tci + CA bundle ready ({ca})")

    if want_sdl:
        sdl_dir = ensure_repo("SDL2")
        print(f"OK: SDL2 ready at {sdl_dir}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
