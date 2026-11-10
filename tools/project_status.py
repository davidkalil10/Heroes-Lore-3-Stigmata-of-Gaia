#!/usr/bin/env python3
"""Read, validate, update, and render Heroes Lore 3 project status."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATUS_JSON = ROOT / "docs" / "STATUS.json"
STATUS_MD = ROOT / "docs" / "STATUS.md"

VALID_STATUSES = {"pending", "in_progress", "done", "blocked"}


def load_status() -> dict:
    return json.loads(STATUS_JSON.read_text(encoding="utf-8"))


def render_markdown(data: dict) -> str:
    lines = [
        "# Status do Projeto - Heroes Lore 3 Recomp",
        "",
        "Este arquivo é gerado automaticamente a partir de `docs/STATUS.json` por `python tools/project_status.py render`.",
        "",
        f"- **Projeto**: {data['project']}",
        f"- **Atualizado em**: `{data['updated_at']}`",
        f"- **Fase geral**: `{data['overall_phase']}`",
        f"- **Etapa atual**: `{data['current_step']}`",
        f"- **Próxima etapa recomendada**: `{data['next_recommended_step']}`",
        "",
        "## Resumo Executivo",
        "",
        data["summary"],
        "",
        "## Etapas",
        "",
        "| ID | Etapa | Status | Evidências | Notas |",
        "| --- | --- | --- | --- | --- |",
    ]
    for step in data["steps"]:
        evidence = ", ".join(f"`{item}`" for item in step.get("evidence", [])) or "-"
        notes = step.get("notes", "").replace("\n", " ").strip() or "-"
        lines.append(f"| `{step['id']}` | {step['title']} | `{step['status']}` | {evidence} | {notes} |")
    lines.append("")
    return "\n".join(lines)


def save_status(data: dict) -> None:
    STATUS_JSON.write_text(json.dumps(data, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
    STATUS_MD.write_text(render_markdown(data), encoding="utf-8", newline="\n")


def validate_status(data: dict) -> list[str]:
    errors: list[str] = []
    steps = data.get("steps", [])
    ids = [step.get("id") for step in steps]
    if len(ids) != len(set(ids)):
        errors.append("Duplicate step IDs in docs/STATUS.json")
    if data.get("current_step") not in ids:
        errors.append("current_step does not match any step ID")
    if data.get("next_recommended_step") not in ids:
        errors.append("next_recommended_step does not match any step ID")
    for step in steps:
        status = step.get("status")
        if status not in VALID_STATUSES:
            errors.append(f"Invalid status for {step.get('id')}: {status}")
        for rel in step.get("evidence", []):
            if not (ROOT / rel).exists():
                errors.append(f"Missing evidence path for {step.get('id')}: {rel}")
    return errors


def cmd_show(_args: argparse.Namespace) -> int:
    data = load_status()
    print(f"Project: {data['project']}")
    print(f"Updated: {data['updated_at']}")
    print(f"Overall phase: {data['overall_phase']}")
    print(f"Current step: {data['current_step']}")
    print(f"Next recommended step: {data['next_recommended_step']}")
    print(f"Summary: {data['summary']}")
    print("")
    for step in data["steps"]:
        print(f"{step['id']}: [{step['status']}] {step['title']}")
    return 0


def cmd_render(_args: argparse.Namespace) -> int:
    data = load_status()
    errors = validate_status(data)
    if errors:
        for err in errors:
            print(f"ERROR: {err}", file=sys.stderr)
        return 1
    STATUS_MD.write_text(render_markdown(data), encoding="utf-8", newline="\n")
    print(f"Rendered {STATUS_MD.relative_to(ROOT)}")
    return 0


def cmd_check(_args: argparse.Namespace) -> int:
    data = load_status()
    errors = validate_status(data)
    if not STATUS_MD.exists():
        errors.append("docs/STATUS.md is missing")
    else:
        expected = render_markdown(data)
        actual = STATUS_MD.read_text(encoding="utf-8")
        if actual != expected:
            errors.append("docs/STATUS.md is out of date; run `python tools/project_status.py render`")
    if errors:
        for err in errors:
            print(f"ERROR: {err}", file=sys.stderr)
        return 1
    print("Status files are valid and synchronized.")
    return 0


def cmd_set(args: argparse.Namespace) -> int:
    data = load_status()
    target = None
    for step in data["steps"]:
        if step["id"] == args.step:
            target = step
            break
    if target is None:
        print(f"ERROR: Unknown step {args.step}", file=sys.stderr)
        return 1
    target["status"] = args.status
    if args.status == "done":
        target["completed_at"] = dt.date.today().isoformat()
    if args.note is not None:
        target["notes"] = args.note
    if args.current is not None:
        data["current_step"] = args.current
    if args.next_step is not None:
        data["next_recommended_step"] = args.next_step
    if args.phase is not None:
        data["overall_phase"] = args.phase
    if args.summary is not None:
        data["summary"] = args.summary
    data["updated_at"] = dt.date.today().isoformat()
    errors = validate_status(data)
    if errors:
        for err in errors:
            print(f"ERROR: {err}", file=sys.stderr)
        return 1
    save_status(data)
    print(f"Updated {args.step} -> {args.status}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("show", help="Print a concise status summary")
    sub.add_parser("render", help="Render docs/STATUS.md from docs/STATUS.json")
    sub.add_parser("check", help="Validate docs/STATUS.json and docs/STATUS.md")

    set_p = sub.add_parser("set", help="Update a step status and render docs/STATUS.md")
    set_p.add_argument("step", help="Step ID, e.g. S02")
    set_p.add_argument("status", choices=sorted(VALID_STATUSES))
    set_p.add_argument("--note", help="Replace step notes")
    set_p.add_argument("--current", help="Update current_step")
    set_p.add_argument("--next", dest="next_step", help="Update next_recommended_step")
    set_p.add_argument("--phase", help="Update overall_phase")
    set_p.add_argument("--summary", help="Update executive summary")

    args = parser.parse_args()
    if args.command == "show":
        return cmd_show(args)
    if args.command == "render":
        return cmd_render(args)
    if args.command == "check":
        return cmd_check(args)
    if args.command == "set":
        return cmd_set(args)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
