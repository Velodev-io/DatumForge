#!/usr/bin/env python3
"""Check specification structure and links; this does not test a CAD product."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import unquote, urlsplit


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-report", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    names = ["README.md", "SRS.md", "SDD.md", "TECH_SPEC.md",
             "STEP_ZERO.md", "TRACEABILITY.md", "REFERENCES.md"]
    errors: list[str] = []
    texts: dict[str, str] = {}
    for name in names:
        path = root / name
        if not path.is_file():
            errors.append(f"Missing document: {name}")
            continue
        text = path.read_text(encoding="utf-8")
        texts[name] = text
        if not text.startswith("# ") or "0.1" not in text:
            errors.append(f"Missing title/version: {name}")
        fences = re.findall(r"^```", text, re.MULTILINE)
        if len(fences) % 2:
            errors.append(f"Unclosed fenced block: {name}")
        for href in re.findall(r"\[[^\]]+\]\(([^)]+)\)", text):
            target = href.strip().strip("<>")
            parsed = urlsplit(target)
            if parsed.scheme or not parsed.path:
                continue
            destination = Path(unquote(parsed.path))
            if not destination.is_absolute():
                destination = path.parent / destination
            generated_report = args.write_report and destination.resolve() == root / "SPEC_VALIDATION.json"
            if not destination.exists() and not generated_report:
                errors.append(f"Broken local link in {name}: {target}")

    srs = texts.get("SRS.md", "")
    requirements = re.findall(
        r"^\| ((?:FR|NFR)-\d{3}) \| .+ \| (T-\d{2}) \|$", srs, re.MULTILINE)
    expected = {f"FR-{i:03d}": f"T-{i:02d}" for i in range(1, 41)}
    expected.update({f"NFR-{i:03d}": f"T-{i + 40:02d}" for i in range(1, 17)})
    if dict(requirements) != expected or len(requirements) != len(expected):
        errors.append("SRS requirement/test IDs differ from the v0.1 baseline")

    trace = re.findall(
        r"^\| ((?:FR|NFR)-\d{3}) \| ([\d, ]+) \| (\d+) \| (T-\d{2}) \| (?:Not run|Blocked|Pass|Fail|Not applicable) \|$",
        texts.get("TRACEABILITY.md", ""), re.MULTILINE)
    if {rid: tid for rid, _, _, tid in trace} != expected or len(trace) != len(expected):
        errors.append("Traceability has missing/duplicate/incorrect requirement or test rows")
    for rid, sections, module, _ in trace:
        if not 0 <= int(module) <= 12:
            errors.append(f"Invalid module for {rid}")
        for section in sections.split(","):
            if not re.search(rf"^## {int(section)}\. ", texts.get("SDD.md", ""), re.MULTILINE):
                errors.append(f"Missing SDD section for {rid}: {section}")

    tech = texts.get("TECH_SPEC.md", "")
    modules = re.findall(r"^### Module (\d+) — ", tech, re.MULTILINE)
    if modules != [str(i) for i in range(13)]:
        errors.append("TECH_SPEC must contain Modules 0–12 once, in order")
    for block in re.split(r"(?=^### Module \d+ — )", tech, flags=re.MULTILINE)[1:]:
        for marker in ("**Purpose:**", "**Prerequisites:**", "**Completion check:**",
                       "**Evidence:**", "**Current status:**"):
            if marker not in block:
                errors.append(f"Incomplete module contract: {block.splitlines()[0]} / {marker}")
        if not re.search(r"(?:Test|test) pass criteria:", block):
            errors.append(f"Missing module pass criteria: {block.splitlines()[0]}")

    step = texts.get("STEP_ZERO.md", "")
    if not all(re.search(rf"^## 0\.{i} — ", step, re.MULTILINE) for i in range(1, 11)):
        errors.append("Step Zero must contain sections 0.1–0.10")
    task_ids = re.findall(r"^- \[[ xX]\] \*\*(\d+\.\d+[a-z])", tech + "\n" + step, re.MULTILINE)
    checked = re.findall(r"^- \[[xX]\] \*\*(\d+\.\d+[a-z])", tech + "\n" + step, re.MULTILINE)
    evidence_path = root / "testing/task-status.json"
    if checked and not evidence_path.exists():
        errors.append("Checked tasks require testing/task-status.json evidence")
    if evidence_path.exists():
        evidence = json.loads(evidence_path.read_text())
        for task in checked:
            record = evidence.get(task, {})
            if record.get("status") != "Pass" or not record.get("evidence"):
                errors.append(f"Checked task lacks Pass evidence: {task}")
            for artifact in record.get("evidence", []):
                if not (root.parent / artifact).is_file():
                    errors.append(f"Missing task evidence: {task} / {artifact}")
        scope_path=root / "decisions/implementation-scope.json"
        authorized={0}
        if scope_path.exists():
            authorization=json.loads(scope_path.read_text())
            authorized=set(authorization.get("authorized_modules",[0]))
            if 1 in authorized and authorization.get("module_1_authorization",{}).get("quote") != "proceed with module 1":
                errors.append("Module 1 requires recorded explicit user authorization")
        if 2 in authorized and authorization.get("module_2_authorization",{}).get("quote") != "go-ahead with moudle 2":
            errors.append("Module 2 requires recorded explicit user authorization")
        if 3 in authorized and not authorization.get("module_3_authorization",{}).get("quote"):
            errors.append("Module 3 requires recorded explicit user authorization")
        if authorization.get("stop_before_module") != max(authorized)+1:
            errors.append("Authorization stop boundary must follow the last authorized module")
        partial = authorization.get("interaction_and_workspace_authorization", {})
        permitted_tasks = set(partial.get("authorized_tasks", []))
        if permitted_tasks and (not partial.get("quote") or permitted_tasks != {"4.0a", "4.0b", "4.0c"} or (4 not in authorized and authorization.get("stop_before_task") != "4.1a")):
            errors.append("Partial Module 4 authorization must explicitly permit only 4.0a-c and stop before 4.1a")
        if 4 in authorized and (not authorization.get("module_4_authorization", {}).get("quote") or (5 not in authorized and authorization.get("stop_before_task") != "5.1a")):
            errors.append("Full Module 4 requires explicit user authorization and a stop before 5.1a")
        if 5 in authorized and (not authorization.get("module_5_authorization", {}).get("quote") or authorization.get("stop_before_task") != "6.1a"):
            errors.append("Module 5 requires explicit user authorization and a stop before 6.1a")
        if any(int(task.split(".")[0]) not in authorized and task not in permitted_tasks for task in checked):
            errors.append("Checked task is outside the explicitly authorized module/task scope")
    if len(set(task_ids)) != len(task_ids):
        errors.append("Duplicate lettered implementation task IDs")
    for text, label in ((srs, "SRS"), (tech, "TECH_SPEC"), (step, "STEP_ZERO")):
        if "8 GB" not in text:
            errors.append(f"Missing 8 GB baseline in {label}")

    report = {
        "scope": "Documentation structure only; no product acceptance tests executed",
        "checked_at_utc": datetime.now(timezone.utc).isoformat(),
        "status": "pass" if not errors else "fail",
        "product_implementation": "Modules 1-4 scoped implementation on the available Mac; see task and module evidence",
        "product_acceptance_tests": "Available-Mac subsets are recorded per task; full R1 requirements remain Not run or Blocked",
        "documents": {name: {"sha256": hashlib.sha256(text.encode()).hexdigest(),
                             "lines": len(text.splitlines()), "words": len(text.split())}
                      for name, text in texts.items()},
        "requirements": len(requirements), "traceability_rows": len(trace),
        "modules": len(modules), "unchecked_engineering_tasks": len(task_ids) - len(checked),
        "completed_engineering_tasks": len(checked),
        "errors": errors,
    }
    if args.write_report:
        (root / "SPEC_VALIDATION.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: val for key, val in report.items() if key != "documents"}, indent=2))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
