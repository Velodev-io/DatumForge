# DatumForge — specification set

**Version:** 0.1 · **Date:** 19 October 2026 · **Status:** Module 0 feasibility implementation underway; production modules not started.

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
| Dependency integration / native application | Module 0 source builds and experiments; see [actual evidence](testing/module-0.md) |
| Windows, Linux, or Mac product builds | Product builds Not run; available-target feasibility is recorded separately |
| Real 8 GB workload validation | Not run |
| Feature, assembly, drawing, recovery, and release tests | Not run |

Module 0 work is isolated under tests and tools. Product implementation and release packaging remain future engineering work. Module 1 requires explicit user approval. Unavailable platform and hardware checks remain Blocked.

## Checking the specification set

From the workspace root, run `python3 docs/scripts/check_specs.py --write-report`. This checks local links, requirement/test IDs, traceability coverage, module structure, and evidence references for checked tasks. The generated [SPEC_VALIDATION.json](SPEC_VALIDATION.json) records document hashes and structural results. It does not run application tests or establish CAD feasibility.
