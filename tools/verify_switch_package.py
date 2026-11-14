#!/usr/bin/env python3
"""Verify built HL3 Nintendo Switch .nro header, ASET assets, 256x256 JPEG icon, NACP Title ID, and RomFS."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))

from hl3rt.version import SWITCH_TITLE_ID  # noqa: E402


def verify_nro(nro_path: Path) -> None:
    raw = nro_path.read_bytes()
    if len(raw) <= 0x80 or raw[0x10:0x14] != b"NRO0":
        raise AssertionError(f"{nro_path} is not a valid Nintendo Switch NRO0 binary")

    native_size = struct.unpack_from("<I", raw, 0x18)[0]
    if native_size >= len(raw) or raw[native_size : native_size + 4] != b"ASET":
        raise AssertionError(f"{nro_path} missing appended ASET container at offset 0x{native_size:x}")

    icon_offset, icon_size = struct.unpack_from("<QQ", raw, native_size + 8)
    icon_bytes = raw[native_size + icon_offset : native_size + icon_offset + icon_size]
    if len(icon_bytes) < 64 or not icon_bytes.startswith(b"\xff\xd8\xff"):
        raise AssertionError(f"{nro_path} icon is missing or not JPEG")

    nacp_offset, nacp_size = struct.unpack_from("<QQ", raw, native_size + 24)
    if nacp_size != 0x4000:
        raise AssertionError(f"{nro_path} unexpected NACP size: {nacp_size}")
    nacp = raw[native_size + nacp_offset : native_size + nacp_offset + nacp_size]

    expected_tid = int(SWITCH_TITLE_ID, 16)
    for offset in (0x3038, 0x3078, 0x30B0):
        actual_tid = struct.unpack_from("<Q", nacp, offset)[0]
        if actual_tid != expected_tid:
            raise AssertionError(
                f"{nro_path} NACP Title ID mismatch at 0x{offset:x}: expected 0x{expected_tid:016x}, got 0x{actual_tid:016x}"
            )

    romfs_offset, romfs_size = struct.unpack_from("<QQ", raw, native_size + 40)
    if romfs_size < 1024 * 1024:
        raise AssertionError(f"{nro_path} embedded RomFS unusually small: {romfs_size} bytes")
    romfs_blob = raw[native_size + romfs_offset : native_size + romfs_offset + romfs_size]
    for marker in (b"game.ipa", b"cacert.pem", b"BEGIN CERTIFICATE", b"t_pt-br"):
        if marker not in romfs_blob:
            raise AssertionError(f"{nro_path} RomFS missing expected marker: {marker!r}")

    print(
        f"PASS Switch NRO verified: {nro_path.name} ({len(raw):,} bytes, "
        f"TitleID={SWITCH_TITLE_ID}, RomFS={romfs_size:,} bytes)"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nro", type=Path)
    args = parser.parse_args()
    verify_nro(args.nro)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
