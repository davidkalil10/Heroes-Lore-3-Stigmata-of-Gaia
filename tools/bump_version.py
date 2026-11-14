#!/usr/bin/env python3
"""Inspect, validate, or bump the synchronized Heroes Lore 3 Recomp version across all platforms.

Never commits or pushes git changes automatically.

Managed version targets:
  - tools/hl3rt/version.py        (`PORT_VERSION`, consumed by Python runtime, CI, Switch `nacptool`)
  - src/core/common.h             (`kPortVersion`, compiled into Windows, Linux, Android, and Switch binaries)
  - project.json                  (`port_version`, workspace metadata manifest)
  - android/app/build.gradle      (`versionName` and monotonically incremented `versionCode`)
  - .github/workflows/build.yml   (`workflow_dispatch.inputs.release_tag` example/default)
  - README.md                     (port version badge, feature heading, About credit example, OTA note)
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

DEFAULT_ROOT = Path(__file__).resolve().parents[1]

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
PY_VERSION_RE = re.compile(r'^(PORT_VERSION\s*=\s*")([0-9]+\.[0-9]+\.[0-9]+)(")', re.MULTILINE)
CPP_VERSION_RE = re.compile(
    r'^(inline constexpr const char\*\s+kPortVersion\s*=\s*")([0-9]+\.[0-9]+\.[0-9]+)(";)',
    re.MULTILINE,
)
GRADLE_VERSION_NAME_RE = re.compile(r'^(\s*versionName\s+["\'])([0-9]+\.[0-9]+\.[0-9]+)(["\'])', re.MULTILINE)
GRADLE_VERSION_CODE_RE = re.compile(r'^(\s*versionCode\s+)(\d+)\b', re.MULTILINE)
WORKFLOW_DEFAULT_TAG_RE = re.compile(r"^(\s*default:\s*['\"]v)([0-9]+\.[0-9]+\.[0-9]+)(['\"])", re.MULTILINE)
WORKFLOW_DESC_TAG_RE = re.compile(r"(Release tag \(e\.g\.\s*v)([0-9]+\.[0-9]+\.[0-9]+)(\))")
README_PORT_PATTERNS = [
    re.compile(r"(Runtime Nativo C\+\+17 \(`v)([0-9]+\.[0-9]+\.[0-9]+)(`\))"),
    re.compile(r"(https://img\.shields\.io/badge/Port-v)([0-9]+\.[0-9]+\.[0-9]+)(-[0-9a-fA-F]+)"),
    re.compile(r"(Recursos da versão `v)([0-9]+\.[0-9]+\.[0-9]+)(`)"),
    re.compile(r"(David Kalil Braga \(2026\) - v)([0-9]+\.[0-9]+\.[0-9]+)(`\*\*)"),
    re.compile(r"(mais nova que a instalada \(`v)([0-9]+\.[0-9]+\.[0-9]+)(`\))"),
]


def parse_semver(value: str) -> tuple[int, int, int]:
    clean = value.strip()
    if clean.lower().startswith("v"):
        clean = clean[1:]
    m = SEMVER_RE.match(clean)
    if not m:
        raise ValueError(f"Invalid semantic version {value!r}; expected X.Y.Z")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def _paths(root: Path) -> dict[str, Path]:
    return {
        "version_py": root / "tools" / "hl3rt" / "version.py",
        "common_h": root / "src" / "core" / "common.h",
        "project_json": root / "project.json",
        "build_gradle": root / "android" / "app" / "build.gradle",
        "workflow_yml": root / ".github" / "workflows" / "build.yml",
        "readme_md": root / "README.md",
    }


def read_all_versions(root: Path = DEFAULT_ROOT) -> tuple[dict[str, str], int]:
    p = _paths(root)
    versions: dict[str, str] = {}

    py_m = PY_VERSION_RE.search(p["version_py"].read_text(encoding="utf-8"))
    if not py_m:
        raise ValueError(f"Could not find PORT_VERSION in {p['version_py']}")
    versions["tools/hl3rt/version.py"] = py_m.group(2)

    proj = json.loads(p["project_json"].read_text(encoding="utf-8"))
    json_ver = str(proj.get("port_version", "")).strip()
    if not json_ver:
        raise ValueError(f"Missing 'port_version' in {p['project_json']}")
    versions["project.json"] = json_ver

    if p["common_h"].exists():
        cpp_m = CPP_VERSION_RE.search(p["common_h"].read_text(encoding="utf-8"))
        if not cpp_m:
            raise ValueError(f"Could not find kPortVersion in {p['common_h']}")
        versions["src/core/common.h"] = cpp_m.group(2)

    if p["build_gradle"].exists():
        gradle_txt = p["build_gradle"].read_text(encoding="utf-8")
        gn_m = GRADLE_VERSION_NAME_RE.search(gradle_txt)
        gc_m = GRADLE_VERSION_CODE_RE.search(gradle_txt)
        if not gn_m or not gc_m:
            raise ValueError(f"Could not find versionName/versionCode in {p['build_gradle']}")
        versions["android/app/build.gradle"] = gn_m.group(2)
        android_code = int(gc_m.group(2))
    else:
        android_code = 0

    if p["workflow_yml"].exists():
        wf_txt = p["workflow_yml"].read_text(encoding="utf-8")
        wf_def = WORKFLOW_DEFAULT_TAG_RE.search(wf_txt)
        wf_desc = WORKFLOW_DESC_TAG_RE.search(wf_txt)
        if wf_def:
            versions[".github/workflows/build.yml (default)"] = wf_def.group(2)
        if wf_desc:
            versions[".github/workflows/build.yml (description)"] = wf_desc.group(2)

    if p["readme_md"].exists():
        readme_txt = p["readme_md"].read_text(encoding="utf-8")
        found_readme = set()
        for idx, pat in enumerate(README_PORT_PATTERNS):
            matches = pat.findall(readme_txt)
            if matches:
                for m in matches:
                    found_readme.add(m[1])
            else:
                raise ValueError(f"Missing README port version pattern #{idx + 1} in {p['readme_md']}")
        if len(found_readme) != 1:
            raise ValueError(f"Inconsistent README port versions: {sorted(found_readme)}")
        versions["README.md"] = next(iter(found_readme))

    return versions, android_code


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


def write_all_versions(new_ver: str, new_android_code: int, root: Path = DEFAULT_ROOT, dry_run: bool = False) -> list[str]:
    parse_semver(new_ver)
    p = _paths(root)
    updated_files: list[str] = []

    def _sub_file(path: Path, rel: str, specs: list[tuple[re.Pattern[str], str, int]]) -> None:
        if not path.exists():
            return
        text = path.read_text(encoding="utf-8")
        for pat, repl, min_count in specs:
            text, count = pat.subn(repl, text)
            if count < min_count:
                raise ValueError(f"Expected >= {min_count} replacement(s) in {path} for {pat.pattern!r}, got {count}")
        if not dry_run:
            path.write_text(text, encoding="utf-8", newline="\n")
        updated_files.append(rel)

    _sub_file(
        p["version_py"],
        "tools/hl3rt/version.py",
        [(PY_VERSION_RE, rf"\g<1>{new_ver}\g<3>", 1)],
    )
    _sub_file(
        p["common_h"],
        "src/core/common.h",
        [(CPP_VERSION_RE, rf"\g<1>{new_ver}\g<3>", 1)],
    )

    proj = json.loads(p["project_json"].read_text(encoding="utf-8"))
    proj["port_version"] = new_ver
    if not dry_run:
        p["project_json"].write_text(json.dumps(proj, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
    updated_files.append("project.json")

    if p["build_gradle"].exists():
        _sub_file(
            p["build_gradle"],
            "android/app/build.gradle",
            [
                (GRADLE_VERSION_NAME_RE, rf"\g<1>{new_ver}\g<3>", 1),
                (GRADLE_VERSION_CODE_RE, rf"\g<1>{new_android_code}", 1),
            ],
        )

    if p["workflow_yml"].exists():
        wf_txt = p["workflow_yml"].read_text(encoding="utf-8")
        wf_specs: list[tuple[re.Pattern[str], str, int]] = []
        if WORKFLOW_DEFAULT_TAG_RE.search(wf_txt):
            wf_specs.append((WORKFLOW_DEFAULT_TAG_RE, rf"\g<1>{new_ver}\g<3>", 1))
        if WORKFLOW_DESC_TAG_RE.search(wf_txt):
            wf_specs.append((WORKFLOW_DESC_TAG_RE, rf"\g<1>{new_ver}\g<3>", 1))
        if wf_specs:
            _sub_file(p["workflow_yml"], ".github/workflows/build.yml", wf_specs)

    if p["readme_md"].exists():
        _sub_file(
            p["readme_md"],
            "README.md",
            [(pat, rf"\g<1>{new_ver}\g<3>", 1) for pat in README_PORT_PATTERNS],
        )

    return updated_files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", nargs="?", help="patch | minor | major | X.Y.Z")
    parser.add_argument("--current", action="store_true", help="Print current synchronized port version")
    parser.add_argument("--check", action="store_true", help="Verify all platform version files are synchronized")
    parser.add_argument("--dry-run", action="store_true", help="Show target version and files without writing")
    parser.add_argument("--android-code", type=int, default=None, help="Explicit next Android versionCode")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="Workspace root override (for tests)")
    args = parser.parse_args()

    try:
        versions, android_code = read_all_versions(args.root)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    unique = set(versions.values())
    if args.check:
        try:
            for v in unique:
                parse_semver(v)
        except ValueError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1
        if len(unique) != 1:
            details = ", ".join(f"{k}={v!r}" for k, v in versions.items())
            print(f"ERROR: Version mismatch: {details}", file=sys.stderr)
            return 1
        cur_ver = next(iter(unique))
        if _paths(args.root)["build_gradle"].exists() and android_code < 1:
            print(f"ERROR: Invalid Android versionCode {android_code}", file=sys.stderr)
            return 1
        print(
            f"OK: Port version synchronized at v{cur_ver} "
            f"(Android versionCode {android_code}, {len(versions)} checks)"
        )
        return 0

    if len(unique) != 1:
        details = ", ".join(f"{k}={v!r}" for k, v in versions.items())
        print(f"ERROR: Version mismatch before bump: {details}", file=sys.stderr)
        return 1

    cur_ver = next(iter(unique))
    if args.current or not args.target:
        print(cur_ver)
        return 0

    try:
        new_ver = compute_next(cur_ver, args.target)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if parse_semver(new_ver) <= parse_semver(cur_ver):
        print(f"ERROR: New version {new_ver} must be greater than current {cur_ver}", file=sys.stderr)
        return 1

    next_code = args.android_code if args.android_code is not None else (android_code + 1)
    if _paths(args.root)["build_gradle"].exists() and next_code <= android_code:
        print(
            f"ERROR: Next Android versionCode ({next_code}) must exceed current ({android_code})",
            file=sys.stderr,
        )
        return 1

    changed = write_all_versions(new_ver, next_code, root=args.root, dry_run=args.dry_run)
    prefix = "[dry-run] " if args.dry_run else ""
    print(
        f"{prefix}Updated port version: v{cur_ver} -> v{new_ver}; "
        f"Android versionCode {android_code} -> {next_code} ({', '.join(changed)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
