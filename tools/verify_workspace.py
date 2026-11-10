#!/usr/bin/env python3
"""Verify Heroes Lore 3 scaffold integrity, reference snapshots, APK/IPA hashes, and status files."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT_JSON = ROOT / "project.json"
HL5_SNAPSHOT_JSON = ROOT / "reference" / "HL5_SNAPSHOT.json"
HL4_SNAPSHOT_JSON = ROOT / "reference" / "HL4_SNAPSHOT.json"
JAR_PORTS_JSON = ROOT / "reference" / "JAR_PORTS_DOCS.json"
BOOTSTRAP_VALIDATION_JSON = ROOT / "reference" / "analysis" / "BOOTSTRAP_VALIDATION.json"

REQUIRED_FILES = [
    ".gitignore",
    ".gitattributes",
    "AGENTS.md",
    "PROMPT_NOVA_SESSAO.md",
    "README.md",
    "project.json",
    "docs/ROADMAP.md",
    "docs/STATUS.json",
    "docs/STATUS.md",
    "docs/HANDOFF.md",
    "docs/DECISIONS.md",
    "docs/HL4_AND_HL5_LESSONS.md",
    "docs/ARCHITECTURE.md",
    "docs/PORTING_MAP.md",
    "docs/PREVIOUS_PORTS.md",
    "docs/FEATURE_PARITY.md",
    "docs/VALIDATION.md",
    "docs/SESSION_PROTOCOL.md",
    "docs/RESEARCH_LOG.md",
    "docs/tasks/S02_RUNTIME_CONTRACT.md",
    "docs/tasks/S03_MACHO_LOADER.md",
    "docs/tasks/S04_DESKTOP_HOST.md",
    "docs/tasks/S05_GAMEPLAY_VALIDATION.md",
    "docs/tasks/S08_VIEWPORT_EXPANSION.md",
    "docs/tasks/S09_TEXT_EXTRACTION.md",
    "docs/tasks/S12_MULTIPLATFORM_CI.md",
    "reference/HL5_SNAPSHOT.json",
    "reference/HL4_SNAPSHOT.json",
    "reference/JAR_PORTS_DOCS.json",
    "reference/README.md",
    "research/sessions/TEMPLATE.md",
    "research/sessions/2026-11-09-01.md",
    "tools/README.md",
    "tools/project_status.py",
    "tools/inspect_apk.py",
    "tools/probe_macho_contract.py",
    "tools/run_pc.py",
    "tools/hl3rt/runtime.py",
    "tools/hl3rt/gl_bridge.py",
    "tools/hl3rt/hle_audio.py",
    "docs/RUNTIME_CONTRACT.md",
    "tests/test_probe_macho_contract.py",
    "tests/test_s03_macho_loader.py",
    "tests/test_run_pc.py",
    "tools/verify_workspace.py",
]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def verify_manifest(manifest_path: Path, base_dir: Path, label: str, errors: list[str]) -> int:
    if not manifest_path.exists():
        errors.append(f"Missing {label} manifest: {manifest_path.relative_to(ROOT)}")
        return 0
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    checked = 0
    if "projects" in data:
        for proj in data["projects"]:
            proj_name = proj["project"]
            for item in proj.get("files", []):
                rel = f"{proj_name}/{item['path']}"
                target = base_dir / rel
                if not target.exists():
                    errors.append(f"Missing {label} file: {target.relative_to(ROOT)}")
                    continue
                actual_sha = sha256_file(target)
                if actual_sha != item["sha256"]:
                    errors.append(f"SHA-256 mismatch in {label} {rel}: expected {item['sha256']}, got {actual_sha}")
                checked += 1
        return checked

    files = data.get("files", [])
    for item in files:
        rel = item["path"]
        target = base_dir / rel
        if not target.exists():
            errors.append(f"Missing {label} file: {target.relative_to(ROOT)}")
            continue
        actual_bytes = target.stat().st_size
        actual_sha = sha256_file(target)
        if "bytes" in item and actual_bytes != item["bytes"]:
            errors.append(f"Size mismatch in {label} {rel}: expected {item['bytes']}, got {actual_bytes}")
        if actual_sha != item["sha256"]:
            errors.append(f"SHA-256 mismatch in {label} {rel}: expected {item['sha256']}, got {actual_sha}")
        checked += 1
    return checked


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--record",
        action="store_true",
        help="Write reference/analysis/BOOTSTRAP_VALIDATION.json after verification",
    )
    args = parser.parse_args()

    errors: list[str] = []

    for rel in REQUIRED_FILES:
        if not (ROOT / rel).exists():
            errors.append(f"Missing required scaffold file: {rel}")

    apk_verified = False
    if PROJECT_JSON.exists():
        project = json.loads(PROJECT_JSON.read_text(encoding="utf-8"))
        apk_info = project.get("original_apk", {})
        apk_rel = apk_info.get("path")
        expected_sha = apk_info.get("sha256")
        if apk_rel:
            apk_path = ROOT / apk_rel
            if not apk_path.exists():
                errors.append(f"Missing original APK: {apk_rel}")
            else:
                actual_sha = sha256_file(apk_path)
                if actual_sha != expected_sha:
                    errors.append(f"Original APK SHA-256 mismatch: expected {expected_sha}, got {actual_sha}")
                else:
                    apk_verified = True

    hl5_checked = verify_manifest(HL5_SNAPSHOT_JSON, ROOT / "reference" / "hl5_snapshot", "HL5 snapshot", errors)
    hl4_checked = verify_manifest(HL4_SNAPSHOT_JSON, ROOT / "reference" / "hl4_snapshot", "HL4 snapshot", errors)
    jar_checked = verify_manifest(JAR_PORTS_JSON, ROOT / "reference" / "jar_ports_docs", "JAR ports docs", errors)

    status_proc = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "project_status.py"), "check"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if status_proc.returncode != 0:
        errors.append(f"project_status.py check failed: {status_proc.stderr.strip() or status_proc.stdout.strip()}")

    inspect_proc = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "inspect_apk.py"), "--check"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if inspect_proc.returncode != 0:
        errors.append(f"inspect_apk.py --check failed: {inspect_proc.stderr.strip() or inspect_proc.stdout.strip()}")

    probe_proc = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "probe_macho_contract.py"), "--check"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if probe_proc.returncode != 0:
        errors.append(
            f"probe_macho_contract.py --check failed: {probe_proc.stderr.strip() or probe_proc.stdout.strip()}"
        )

    if errors:
        for err in errors:
            print(f"ERROR: {err}", file=sys.stderr)
        return 1

    report = {
        "status": "ok",
        "required_files_checked": len(REQUIRED_FILES),
        "apk_sha256_verified": apk_verified,
        "hl5_snapshot_files_verified": hl5_checked,
        "hl4_snapshot_files_verified": hl4_checked,
        "jar_ports_docs_verified": jar_checked,
        "project_status_check": "ok",
        "static_apk_ipa_macho_check": "ok",
    }

    if args.record:
        BOOTSTRAP_VALIDATION_JSON.parent.mkdir(parents=True, exist_ok=True)
        BOOTSTRAP_VALIDATION_JSON.write_text(
            json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n"
        )

    print(
        f"OK: Workspace verified ({len(REQUIRED_FILES)} scaffold files, "
        f"{hl4_checked} HL4 snapshot files, {hl5_checked} HL5 snapshot files, "
        f"{jar_checked} JAR docs, APK+IPA+Mach-O static analysis verified)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
