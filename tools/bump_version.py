#!/usr/bin/env python3
"""Inspect, validate, or bump the synchronized Heroes Lore 3 Recomp port version."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION_PY = ROOT / "tools" / "hl3rt" / "version.py"
PROJECT_JSON = ROOT / "project.json"

SEMVER_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
PY_VERSION_RE = re.compile(r'^(PORT_VERSION\s*=\s*")([0-9]+\.[0-9]+\.[0-9]+)(")', re.MULTILINE)


def parse_semver(value: str) -> tuple[int, int, int]:
    clean = value.strip()
    if clean.lower().startswith("v"):
        clean = clean[1:]
    m = SEMVER_RE.match(clean)
    if not m:
        raise ValueError(f"Invalid semantic version {value!r}; expected X.Y.Z")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def read_versions() -> tuple[str, str]:
    py_text = VERSION_PY.read_text(encoding="utf-8")
    m = PY_VERSION_RE.search(py_text)
    if not m:
        raise ValueError(f"Could not find PORT_VERSION in {VERSION_PY}")
    py_ver = m.group(2)

    proj = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
    json_ver = str(proj.get("port_version", "")).strip()
    if not json_ver:
        raise ValueError(f"Missing 'port_version' in {PROJECT_JSON}")
    return py_ver, json_ver


def write_version(new_ver: str) -> None:
    parse_semver(new_ver)
    py_text = VERSION_PY.read_text(encoding="utf-8")
    new_py, count = PY_VERSION_RE.subn(rf"\g<1>{new_ver}\g<3>", py_text, count=1)
    if count != 1:
        raise ValueError(f"Failed updating PORT_VERSION in {VERSION_PY}")
    VERSION_PY.write_text(new_py, encoding="utf-8", newline="\n")

    proj = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
    proj["port_version"] = new_ver
    PROJECT_JSON.write_text(json.dumps(proj, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")


def compute_next(current: str, target: str) -> str:
    major, minor, patch = parse_semver(current)
    low = target.strip().lower()
    if low == "patch":
        return f"{major}.{minor}.{patch + 1}"
    if low == "minor":
        return f"{major}.{minor + 1}.0"
    if low == "major":
        return f"{major + 1}.0.0"
    a, b, c = parse_semver(target)
    return f"{a}.{b}.{c}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", nargs="?", help="patch | minor | major | X.Y.Z")
    parser.add_argument("--current", action="store_true", help="Print current synchronized port version")
    parser.add_argument("--check", action="store_true", help="Verify version files are synchronized")
    parser.add_argument("--dry-run", action="store_true", help="Show target version without writing files")
    args = parser.parse_args()

    py_ver, json_ver = read_versions()
    if args.check:
        parse_semver(py_ver)
        parse_semver(json_ver)
        if py_ver != json_ver:
            print(
                f"ERROR: Version mismatch: version.py={py_ver!r} != project.json={json_ver!r}",
                file=sys.stderr,
            )
            return 1
        print(f"OK: Port version synchronized at v{py_ver}")
        return 0

    if args.current or not args.target:
        print(py_ver)
        return 0

    if py_ver != json_ver:
        print(
            f"ERROR: Version mismatch before bump: version.py={py_ver!r} != project.json={json_ver!r}",
            file=sys.stderr,
        )
        return 1

    new_ver = compute_next(py_ver, args.target)
    if args.dry_run:
        print(f"[dry-run] {py_ver} -> {new_ver}")
        return 0

    write_version(new_ver)
    print(f"Updated port version: {py_ver} -> {new_ver} (tag v{new_ver})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

