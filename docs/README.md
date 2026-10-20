# DatumForge — specification set

**Version:** 0.1 · **Date:** 19 October 2026 · **Status:** Modules 1–3 available-Mac scope verified; Module 3 evidence recorded separately; cross-platform matrix Blocked; stop before Module 4.

This project will deliver a native, offline-first mechanical CAD application for macOS, Windows, and Linux, with a SolidWorks-familiar workspace and an **8 GB system RAM minimum**. DatumForge is the product name; the local workspace name does not imply complete SolidWorks compatibility.

## Read and implement in this order

| Document | Responsibility |
|---|---|
| [SRS](SRS.md) | What the product must do, release scope, measurable requirements, and acceptance scenarios |
| [SDD](SDD.md) | How components, model state, geometry, references, storage, and UI cooperate |
| [Step Zero](STEP_ZERO.md) | Repository, toolchains, dependency choices, feasibility experiments, fixtures, and evidence before production implementation |
| [Technical specification](TECH_SPEC.md) | Implementable contracts and numbered Modules 0–12 with lettered tasks and required tests |
| [Traceability](TRACEABILITY.md) | Requirement → design section → implementation module → acceptance test mapping |
| [References](REFERENCES.md) | Sources, historical planning convention, evidence boundaries, and research links |

The format follows the earlier DeskCore plan: **Module 0 → Module 1 → Module 2**, and within each module **1.1 → 1.1a → 1.1b**. Every module names its purpose, prerequisites, required tests, pass criteria, and completion check. All engineering checkboxes begin unchecked. A document being written does not mean its requirements have been implemented or its tests have passed.

## Authority and changes

- The latest explicit user instruction controls scope. The user raised the original 4 GB goal to **8 GB**; 4 GB is not a release requirement.
- SRS owns product behavior and acceptance budgets. SDD owns design. TECH_SPEC owns concrete contracts and implementation sequence. STEP_ZERO owns initial feasibility gates. Link to the owner instead of silently redefining it.
- Proposed OS versions, dependency versions, performance budgets, and workload sizes require Step Zero evidence. They are not current measured capability claims.
- Record changes across affected documents, traceability, fixtures, and tests together. Never relax a failed acceptance target silently.
- Deferred capabilities must not appear as functioning controls. Future scope is distinct from an unfinished R1 requirement.
- Core R1 workflows shall work without an account, cloud backend, browser runtime, or SolidWorks installation. Optional future integrations must preserve that property.

## Current status

| Item | Status |
|---|---|
| Public SolidWorks research and UI atlas | Available; see References for limitations |
| SRS, SDD, TECH_SPEC, Step Zero | Written as proposed specifications |
| Dependency integration / native application | Native shell/worker, persistence and planar sketch/constraint/expression editing implemented on Mac; see [Module 3 evidence](testing/module-3.md) |
| Windows, Linux, or Mac product builds | Mac development build tested; Windows/Linux Blocked; installed release builds Not run |
| Real 8 GB workload validation | Not run |
| Document metadata, saved/recoverable revisions, semantic undo/redo | Available-Mac foundation tested; full requirement acceptance Blocked |
| Planar sketches, constraints and expressions | Available-Mac implementation including [circle drag/preview follow-up](testing/module-3-interaction-followup.md); see [Module 3 evidence](testing/module-3.md) |
| Solid features, mates, drawing content, and release tests | Not run |

Module 0 experiments remain isolated under tests and tools. The user authorized Modules 1 and 2 on 19 October 2026. Their available-Mac implementation and tests are recorded separately. Cross-platform completion remains Blocked. The user explicitly authorized Module 3 in the subsequent instruction; its implementation and verification are recorded in [Module 3 evidence](testing/module-3.md). Stop before Module 4 and ask the user before proceeding. Unavailable platform and hardware checks remain Blocked.

## Upcoming UI work

The user requested explicit UI scheduling on 20 October 2026. [Module 4.0a–c](TECH_SPEC.md#40--align-the-native-modeling-workspace-before-adding-solid-tools) will add Features/Sketch/Evaluate command tabs, a left PropertyManager shared with the model tree, and the overall modeling layout before solid-feature controls. Module 9 completes consistency and accessibility across document types. These tasks remain unimplemented and require Module 4 authorization; the stop boundary remains unchanged.

## Checking the specification set

From the workspace root, run `python3 docs/scripts/check_specs.py --write-report`. This checks local links, requirement/test IDs, traceability coverage, module structure, and evidence references for checked tasks. The generated [SPEC_VALIDATION.json](SPEC_VALIDATION.json) records document hashes and structural results. It does not run application tests or establish CAD feasibility.
