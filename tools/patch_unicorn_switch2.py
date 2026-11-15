#!/usr/bin/env python3
"""Select libounce for configure probes of the already patched Unicorn TCI."""
import argparse
from pathlib import Path
from patch_unicorn_switch import patch_once

def patch_switch2_unicorn(root: Path) -> bool:
    return patch_once(root, "qemu/configure",
        "-specs=/opt/devkitpro/libnx/switch.specs -L/opt/devkitpro/libnx/lib -lnx",
        "-specs=/opt/devkitpro/libounce/switch.specs -L/opt/devkitpro/libounce/lib -lnx")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    args = parser.parse_args()
    patch_switch2_unicorn(args.engine)
    print("OK: Unicorn configure probes use libounce (Switch 2)")
