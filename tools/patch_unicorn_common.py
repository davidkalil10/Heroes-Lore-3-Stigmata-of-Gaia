#!/usr/bin/env python3
"""Apply idempotent Unicorn zlib symbol collision fixes (crc32 -> uc_qemu_crc32)."""

from __future__ import annotations

import argparse
from pathlib import Path

REPLACEMENTS: list[tuple[str, str, str]] = [
    (
        "qemu/include/qemu/crc32c.h",
        "uint32_t crc32(uint32_t crc, const uint8_t *data, unsigned int length);",
        "uint32_t uc_qemu_crc32(uint32_t crc, const uint8_t *data, unsigned int length);",
    ),
    (
        "qemu/util/crc32c.c",
        "uint32_t crc32(uint32_t crc, const uint8_t *data, unsigned int length)",
        "uint32_t uc_qemu_crc32(uint32_t crc, const uint8_t *data, unsigned int length)",
    ),
    (
        "qemu/target/arm/helper-a64.c",
        "return crc32(acc ^ 0xffffffff, buf, bytes) ^ 0xffffffff;",
        "return uc_qemu_crc32(acc ^ 0xffffffff, buf, bytes) ^ 0xffffffff;",
    ),
    (
        "qemu/target/arm/helper.c",
        "return crc32(acc ^ 0xffffffff, buf, bytes) ^ 0xffffffff;",
        "return uc_qemu_crc32(acc ^ 0xffffffff, buf, bytes) ^ 0xffffffff;",
    ),
    (
        "qemu/target/tricore/op_helper.c",
        "return crc32(arg1, buf, 4);",
        "return uc_qemu_crc32(arg1, buf, 4);",
    ),
]


def patch_unicorn_crc32(unicorn_dir: Path) -> int:
    changed = 0
    for rel, old, new in REPLACEMENTS:
        target = unicorn_dir / rel
        if not target.exists():
            raise FileNotFoundError(f"Missing expected Unicorn file: {target}")
        text = target.read_text(encoding="utf-8")
        if new in text:
            continue
        if old not in text:
            raise ValueError(f"Unicorn crc32 patch guard failed in {target}")
        target.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
        changed += 1
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unicorn_dir", type=Path, help="Path to Unicorn source root")
    args = parser.parse_args()
    changed = patch_unicorn_crc32(args.unicorn_dir)
    print(f"OK: Unicorn uc_qemu_crc32 patch verified ({changed} file(s) updated) in {args.unicorn_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

