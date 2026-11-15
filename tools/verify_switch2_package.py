#!/usr/bin/env python3
"""Verify actual libounce startup and reject Switch 1 libraries in the link map."""
import argparse
import struct
from pathlib import Path

from verify_switch_package import verify_nro


def verify_switch2_startup(raw: bytes) -> None:
    # Match nroGetOunceInfo in the pinned libounce and hbmenu: inspect only
    # the first page and step over known MOD0 extension blocks.
    raw = raw[:0x1000]
    if len(raw) < 0x80 or raw[0x10:0x14] != b"NRO0":
        raise ValueError("Not an NRO")
    mod = struct.unpack_from("<I", raw, 4)[0]
    if mod + 0x1c > len(raw) or raw[mod:mod+4] != b"MOD0":
        raise ValueError("Missing MOD0 startup")
    at = mod + 0x1c
    for _ in range(8):
        if at + 12 > len(raw):
            break
        magic = raw[at:at+4]
        if magic == b"OUN0":
            platform, version = struct.unpack_from("<II", raw, at+4)
            if platform != 2 or version != 1:
                raise ValueError("Startup is not current Switch 2 libounce")
            return
        if magic[:3] != b"LNY":
            break
        at += 12
    raise ValueError("Missing Switch 2 OUN0 startup: hbmenu would warn")


def verify_link_map(text: str) -> None:
    text = text.replace("\\", "/")
    forbidden = ["/libnx/", "/portlibs/switch/lib/libEGL.a",
                 "/portlibs/switch/lib/libglapi.a",
                 "/portlibs/switch/lib/libdrm_nouveau.a"]
    for path in forbidden:
        if path in text:
            raise ValueError("Switch 1 dependency in link map: " + path)
    for path in ["/libounce/lib/", "/portlibs/switch2/lib/"]:
        if path not in text:
            raise ValueError("Missing Switch 2 dependency in link map: " + path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nro", type=Path)
    parser.add_argument("--link-map", type=Path, required=True)
    args = parser.parse_args()
    verify_switch2_startup(args.nro.read_bytes())
    verify_link_map(args.link_map.read_text(errors="replace"))
    verify_nro(args.nro)
    print("PASS: Switch 2 OUN0 platform=2 version=1; no Switch 1 startup/graphics libraries")
