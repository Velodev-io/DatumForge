# DatumForge — specification set

**Version:** 0.1 · **Date:** 19 October 2026 · **Status:** Prior Modules 1–3 Mac evidence preserved; Module 3 input repair and authorized Module 4.0 UI verified on available Mac; full matrix Blocked; Module 4 exact features verified on available Mac; Module 5 verified for available-Mac scope; full resource/platform acceptance Blocked; Module 6 authorized on 21 October 2026; available-Mac Part exchange/copy verification recorded; full resource/platform gates Blocked; Module 7 explicitly authorized on 21 October 2026; assembly implementation and available-Mac verification complete; Modules 8–10 authorized on 22 October 2026; available-Mac verification complete; full acceptance Blocked; stop before Module 11.

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
| Planar sketches, constraints and expressions | Available-Mac implementation including [later input/direct-editing repair](testing/module-3-input-repair.md); see [Module 3 evidence](testing/module-3.md) |
| Command tabs / left PropertyManager / direct sketch editing | Available-Mac [input repair](testing/module-3-input-repair.md) and [4.0 workspace evidence](testing/module-4.md); physical-device/full platform acceptance Blocked |
| Solid features and measurement | Module 4 implementation; available-Mac verification in [Module 4 evidence](testing/module-4.md) |
| Dependencies, history and reference repair | Available-Mac [Module 5 evidence](testing/module-5.md); full live-object/resource/platform acceptance Blocked |
| Neutral exchange / portable Part copies | Module 6 implementation; [available-Mac verification](testing/module-6.md) recorded; assembly closure extends in Module 7; drawings and broader resource/platform acceptance remain pending |
| Rigid assemblies, mates, loading and interference | Module 7 implemented; [available-Mac verification](testing/module-7.md) complete; 7.4c full resource acceptance remains Blocked; full resource/platform acceptance Blocked |
| Drawings, unified interaction and recovery hardening | Modules 8–10 implemented; available-Mac verification complete; full platform/resource/assistive-technology acceptance Blocked; stop before Module 11 |
| CM0–CM9 core-modeling expansion | Authorized; CM1 rectangle/slot methods, exact conic/spline editors, broader sketch relations and cross-sketch Pierce implemented with versioned persistence and available-Mac verification; CM2 straight-line trim/extend has available-Mac slice verification; whole CM2 remains in progress; CM3 driving/reference/component dimensions and the native editor have available-Mac slice verification; CM3 centerline/ordinate/circular arc-length additions have available-Mac engine/persistence and scoped native verification, with failed attempts and broader interaction-repeatability limits retained; remaining CM3 path dimensions/inference/drag and projected/external operands and full course/resource/platform gates remain open; see [execution evidence](testing/core-modeling.md) |

Module 0 experiments remain isolated under tests and tools. The user authorized Modules 1 and 2 on 19 October 2026. Their available-Mac implementation and tests are recorded separately. Cross-platform completion remains Blocked. The user explicitly authorized Module 3 in the subsequent instruction; its implementation and verification are recorded in [Module 3 evidence](testing/module-3.md). The subsequent instruction on 20 October authorizes Module 3 input/direct-editing repair followed by Module 4.0a–c UI and verification. The user subsequently authorized all remaining Module 4 tasks starting with 4.1a on 20 October 2026. Module 5 was subsequently authorized and has available-Mac verification; Module 6 was authorized on 21 October 2026; Module 7 was explicitly authorized on 21 October 2026, leaving Module 6.1b/6.4c for later; that historical stop was superseded by explicit Modules 8–10 authorization on 22 October 2026; stop before Module 11. Unavailable platform and hardware checks remain Blocked.

## Workspace UI scope

The user requested explicit UI scheduling on 20 October 2026. [Module 4.0a–c](TECH_SPEC.md#40--align-the-native-modeling-workspace-before-adding-solid-tools) adds Features/Sketch/Evaluate command tabs, a left PropertyManager shared with the model tree, and the overall modeling layout before solid-feature controls. Module 9 completes consistency and accessibility across document types. The user subsequently authorized this implementation order. These UI tasks are implemented and verified for available-Mac scope in [workspace evidence](testing/module-4.md), with full native/device acceptance Blocked. The next instruction explicitly authorized remaining Module 4 exact features; [feature adapter decisions](decisions/module-4-features.md) define that implementation. The current authorized boundary is before Module 11; Module 5 feasible implementation and available-Mac verification are complete; 5.4c full resource acceptance remains Blocked.

## Checking the specification set

From the workspace root, run `python3 docs/scripts/check_specs.py --write-report`. This checks local links, requirement/test IDs, traceability coverage, module structure, and evidence references for checked tasks. The generated [SPEC_VALIDATION.json](SPEC_VALIDATION.json) records document hashes and structural results. It does not run application tests or establish CAD feasibility.


Modules 8–10 were explicitly authorized together on 22 October 2026, in order,
with no commit or push. [Drawing contracts](decisions/module-8-drawings.md),
[interaction/privacy contracts](decisions/module-9-interaction.md), and
[recovery contracts](decisions/module-10-recovery.md) record implementation scope.
Durable evidence is maintained in [Module 8](testing/module-8.md),
[Module 9](testing/module-9.md) and [Module 10](testing/module-10.md).
Module 6.1b/6.4c and 7.4c remain deferred and Blocked. Module 11 is not started.

[Centerpoint Arc interaction repair](testing/module-3-centerpoint-arc.md) records the explicit 22 October follow-up to Module 3.1b, including the selected SolidWorks-style pointer sequence and separate validation evidence. It does not start Module 11.

[All-arc behavior and geometry graph comparison](testing/module-3-all-arcs.md) records the subsequent explicit request for all three methods and the EDP Exam attachment comparison. It extends the earlier Centerpoint evidence; full acceptance remains Blocked.

## Authorized core-modeling expansion

The user authorized [CM0–CM9](CORE_MODELING.md) on 22 October 2026, after Module 10
and before Module 11. [Coverage](research/core-modeling-coverage.json) schedules
the T2–T8 variants; [evidence](testing/core-modeling.md) distinguishes production
work from isolated feasibility. This authorization does not close deferred gates
or authorize Modules 11–12 or assembly/drawing expansion. Local feature commits
were separately authorized on 25 October; no merge or push was authorized.
