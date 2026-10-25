# Requirements and acceptance traceability

**Version:** 0.1 · **Date:** 19 October 2026 · **Status:** Modules 1–5 available-Mac subsets verified; Module 6 available-Mac Part exchange/copy scope verified; full product requirements Not run or Blocked.

[SRS](SRS.md) owns each requirement and its acceptance test definition. [SDD](SDD.md) sections describe the design. [TECH_SPEC](TECH_SPEC.md) modules own implementation and evidence; earlier modules supply prerequisites, later modules complete integration. [Step Zero](STEP_ZERO.md) owns T-00 feasibility/setup tests and does not satisfy the product acceptance rows below.

## Requirement coverage

| Requirement | SDD sections | Primary module | Acceptance test | Current result |
|---|---|---|---|---|
| FR-001 | 10 | 1 | T-01 | Blocked |
| FR-002 | 3, 9 | 2 | T-02 | Blocked |
| FR-003 | 5, 10, 11 | 1 | T-03 | Blocked |
| FR-004 | 4, 10 | 1 | T-04 | Blocked |
| FR-005 | 10 | 9 | T-05 | Blocked |
| FR-006 | 3, 9 | 2 | T-06 | Blocked |
| FR-007 | 9 | 2 | T-07 | Blocked |
| FR-008 | 4, 9 | 2 | T-08 | Blocked |
| FR-009 | 9 | 2 | T-09 | Blocked |
| FR-010 | 9 | 2 | T-10 | Blocked |
| FR-011 | 9, 11 | 6 | T-11 | Blocked |
| FR-012 | 3, 7 | 3 | T-12 | Blocked |
| FR-013 | 7 | 3 | T-13 | Blocked |
| FR-014 | 7 | 3 | T-14 | Blocked |
| FR-015 | 4, 7 | 3 | T-15 | Blocked |
| FR-016 | 3, 7, 13 | 3 | T-16 | Blocked |
| FR-017 | 8 | 4 | T-17 | Blocked |
| FR-018 | 8 | 4 | T-18 | Blocked |
| FR-019 | 6 | 5 | T-19 | Blocked |
| FR-020 | 6 | 5 | T-20 | Blocked |
| FR-021 | 6 | 5 | T-21 | Blocked |
| FR-022 | 4, 8 | 4 | T-22 | Blocked |
| FR-023 | 8, 11 | 6 | T-23 | Blocked |
| FR-024 | 11 | 6 | T-24 | Blocked |
| FR-025 | 8, 11 | 4 | T-25 | Blocked |
| FR-026 | 3, 7 | 7 | T-26 | Blocked |
| FR-027 | 7 | 7 | T-27 | Blocked |
| FR-028 | 3, 12 | 7 | T-28 | Blocked |
| FR-029 | 6, 7 | 7 | T-29 | Blocked |
| FR-030 | 7, 8 | 7 | T-30 | Blocked |
| FR-031 | 11 | 8 | T-31 | Blocked |
| FR-032 | 6, 11 | 8 | T-32 | Blocked |
| FR-033 | 11 | 8 | T-33 | Blocked |
| FR-034 | 4, 9 | 10 | T-34 | Blocked |
| FR-035 | 4, 5 | 1 | T-35 | Blocked |
| FR-036 | 12, 13 | 9 | T-36 | Blocked |
| FR-037 | 1, 13 | 9 | T-37 | Blocked |
| FR-038 | 5, 13 | 9 | T-38 | Blocked |
| FR-039 | 9 | 10 | T-39 | Blocked |
| FR-040 | 13, 15 | 12 | T-40 | Not run |
| NFR-001 | 12 | 11 | T-41 | Not run |
| NFR-002 | 12 | 11 | T-42 | Blocked |
| NFR-003 | 12 | 11 | T-43 | Not run |
| NFR-004 | 5, 12 | 11 | T-44 | Not run |
| NFR-005 | 4, 7, 12 | 11 | T-45 | Not run |
| NFR-006 | 9, 12 | 11 | T-46 | Not run |
| NFR-007 | 8, 11 | 4 | T-47 | Blocked |
| NFR-008 | 3, 8 | 4 | T-48 | Blocked |
| NFR-009 | 9, 12 | 5 | T-49 | Blocked |
| NFR-010 | 10 | 9 | T-50 | Blocked |
| NFR-011 | 4, 9 | 10 | T-51 | Blocked |
| NFR-012 | 6, 8, 15 | 11 | T-52 | Blocked |
| NFR-013 | 2, 5 | 1 | T-53 | Blocked |
| NFR-014 | 13 | 9 | T-54 | Blocked |
| NFR-015 | 13, 15 | 12 | T-55 | Not run |
| NFR-016 | 4, 9, 12, 13 | 10 | T-56 | Blocked |

UI staging for FR-001, FR-003–FR-005: Module 1 supplies the native shell; [TECH_SPEC 4.0](TECH_SPEC.md#40--align-the-native-modeling-workspace-before-adding-solid-tools) explicitly owns command tabs, the left PropertyManager and the modeling layout before solid-feature controls. Module 9 completes cross-document consistency and accessibility. Primary owners above remain unchanged. The authorized [4.0 workspace implementation](testing/module-4.md) and [Module 3 input repair](testing/module-3-input-repair.md) supply available-Mac subset evidence, while full requirements remain Blocked for platform/device/accessibility/later-engine acceptance.

## Milestone acceptance

| Scenario | Primary implementation owners | Required evidence |
|---|---|---|
| AC-01 | Modules 1, 3, 4, 9 | Native bracket creation, constraints, preview/cancel, features, undo/redo, offline state |
| AC-02 | Modules 2, 4, 5, 10 | Real save/reopen, IDs/parameters, dependency rebuild, reference failure and durability |
| AC-03 | Modules 4, 6 | Real translators and tolerance-aware geometry comparison |
| AC-04 | Modules 6, 7 | Real assembly/mate solves, update/repair, instancing, portable closure |
| AC-05 | Modules 6, 8 | Source-edit associativity, dangling annotations, BOM, visually checked scale-correct PDF |
| AC-06 | Modules 1, 2, 10 | Process-kill and lost-acknowledgment matrix with real SQLite/files |
| AC-07 | Modules 9, 11, 12 | Physical 8 GB results, native accessibility, offline and installed artifacts per platform |

## Evidence policy

Future evidence lives in `docs/testing/module-N.md`, plus referenced logs/images/raw samples. Test rows may link multiple procedures; a row passes only when all applicable required procedures pass. A module's unit tests cannot alone close its native, physical-hardware, or installed-build gates. Preserve failures and reasons for Blocked/Not applicable outcomes.

Module 0 feasibility is recorded in [module-0](testing/module-0.md), its task status register, and [the product requirement register](testing/requirements.json). T-00 results do not close any T-01–T-56 row above. The user authorized Module 1 on 19 October 2026. [Module 1 evidence](testing/module-1.md) covers the Mac reference-workspace subsets of T-01/T-03/T-04/T-35/T-53. The associated full requirement rows remain Blocked for platform or later-module checks. The user subsequently authorized Module 2; [its evidence](testing/module-2.md) covers Mac document/persistence subsets T-02/T-06–T-10/T-34/T-39/T-51. Those full rows remain Blocked for the documented native matrix, conversion or later CAD integration; no complete product test is closed. The user explicitly authorized Module 3 afterward. [Module 3 evidence](testing/module-3.md) covers the available-Mac T-12–T-16 sketch/constraint/expression and native editing subsets. The [interaction follow-up](testing/module-3-interaction-followup.md) separately reproduces and closes the missing circle creation gesture on the available Mac; its native mm/in and scale evidence does not certify other platforms. Those full requirements remain Blocked for the unavailable native platform matrix and broader acceptance. The later user instruction authorized Module 3 input repairs followed by 4.0a–c workspace UI. [Combined evidence](testing/module-4.md) records the current scope and remaining blockers. The subsequent direct instruction explicitly authorized remaining Module 4 exact features from 4.1a. [Part adapter decisions](decisions/module-4-features.md) and the updated Module 4 report record that scope. That Module 4 report records its historical stop before Module 5; the subsequent user reply authorized Module 5. Platform/device gates remain Blocked.

The documentation checker verifies IDs, links, module/task structure, and status consistency only. It does not execute any CAD, UI, persistence, platform, or performance acceptance test.

Module 4 adds available-Mac T-17/T-18/T-22/T-25/T-47/T-48 exact-feature, analytic, measurement, failure/persistence and native B1 evidence. Those full requirements remain Blocked for the documented support/device/later-workload acceptance matrix. See [current Module 4 evidence](testing/module-4.md); the earlier [workspace report](testing/module-4-workspace.md) is retained separately. That was the historical Module 4 stopping point; Module 5 is now authorized and verified for available-Mac scope.

Module 5 adds available-Mac T-19–T-21 dependency/history/reference-repair and T-09 B2 undo/recovery evidence. Its 100-cycle B2 and retained-root review provide narrower T-49/T-52 correctness/cache evidence; total live-object/physical-resource and full platform acceptance remain Blocked. See [Module 5 evidence](testing/module-5.md) and [evaluation decisions](decisions/module-5-evaluation.md). Module 6 was subsequently authorized; stop before **Module 7 / 7.1a**.

Module 6 adds available-Mac T-11 portable Part closure, T-23/T-24 unit-aware exact/mesh exchange and narrower T-38/T-56 malformed/failure/cancel evidence. Full resource/industrial/platform acceptance stays Blocked; assembly/drawing closure remains Modules 7–8. See [Module 6 evidence](testing/module-6.md) and [exchange decisions](decisions/module-6-exchange.md). Stop before **Module 7 / 7.1a**.

Module 7 was explicitly authorized on 21 October 2026, with Module 6.1b/6.4c
deferred for later. [Assembly evidence](testing/module-7.md) and
[assembly contracts](decisions/module-7-assemblies.md) cover the available-Mac
T-26–T-30 / T-11 rigid assembly, mate, interference, loading, portable-copy and
recovery subsets. Full platform/device and physical/shared/GPU resource gates
remain Blocked. Stop before **Module 8 / 8.1a**.


Modules 8–10 were explicitly authorized on 22 October 2026. The historical stop
above is superseded; stop before **Module 11 / 11.1a**. [Module 8 evidence](testing/module-8.md)
adds T-31–T-33/T-11/AC-05 exact drawing/PDF/closure subsets. [Module 9 evidence](testing/module-9.md)
adds T-05/T-36–T-38/T-50/T-54 native keyboard, metadata, privacy and verified
network-denial subsets. [Module 10 evidence](testing/module-10.md) adds
T-08–T-10/T-34–T-35/T-39/T-49/T-51/T-56 and AC-06 exact CAD crash/cancel,
manifest-upgrade and retention subsets. Full product rows remain Blocked for
untested platform/device/assistive-technology/resource/installed acceptance.
Deferred 6.1b/6.4c/7.4c statuses are preserved.

FR-012 / T-12 and FR-005 interaction follow-up: [Centerpoint Arc repair](testing/module-3-centerpoint-arc.md) covers pointer radius/start/end creation, local preview/cancel, unit conversion and semantic undo/save/reopen. Prior full native/device/resource gates remain unchanged.

FR-005 / T-05, FR-012 / T-12 and FR-013 / T-13: [all-arc comparison](testing/module-3-all-arcs.md) extends 3.1b/3.2a/3.2b with three native Arc methods, live creation sizing, tangent/normal endpoint graph relations and EDP Line transition. Full native/device/AT/resource gates remain unchanged.

## Core-modeling extension traceability

Version 0.1 extension: [coverage register](research/core-modeling-coverage.json)
maps each CM requirement to a source sheet/page, CM milestone and lettered tasks,
analytical fixture, native workflow and evidence. [Task contracts](CORE_MODELING.md)
and [execution evidence](testing/core-modeling.md) complement the unchanged
FR/NFR register. Planned fixture names are not passing test evidence.

CM-001–CM-005 map to `cm1.rectangles`, `cm1.rectangle_contracts` and
`cm1_contracts nativeRectangles`; method names identify the corresponding corpus
case. [Rectangle contracts](decisions/core-modeling-rectangles.md) distinguish
implemented group/size semantics from deferred CM3 constrained dragging and
angular dimensions. Per-row status retains incomplete broader acceptance gates.

CM-006–CM-012 map to `cm1.slots`, `cm1.slot_contracts`, and native Slot/pointer
workflows. CM-013–CM-016 map to `cm1.curves`, `cm1.curve_contracts` and native
curve/pointer workflows. CM-025–CM-037 have additional `cm1.intent` and expanded
native relation cases; CM-028 maps to `cm1.pierce`/`cm1.pierce_contracts` and native
Pierce branch repair. These are executable case mappings, not blanket per-row
acceptance. Projected-edge/external-vertex operand cases remain dependent on CM4.
[Continuation evidence](testing/core-modeling.md) retains precise verification
scope and failures, including compatibility and interrupted acceptance tests.
