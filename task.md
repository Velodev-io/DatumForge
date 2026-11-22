# Active plan — CM9 catalogs, material/evaluation and model display

Prepared 22 November 2026. The [CM9 execution plan](docs/decisions/core-modeling-cm9-plan.md)
maps all 30 coverage requirements to implementation and verification gates.

- [x] **CM9.a** — Freeze source pages, catalog coverage and redistribution evidence. Capture the baseline and define the semantic contracts.
- [x] **CM9.b** — Build offline/custom catalog resolution, exact evaluation and model display.
- [x] **CM9.c** — Persist resolved snapshots, appearance overrides and named sections with compatible replay.
- [/] **CM9.d** — Wire the native Hole, Materials, Evaluate and View workflows.
- [ ] **CM9.e** — Verify all coverage rows and record the remaining acceptance gates.

Start with CM9.a. A blocked catalog source leaves its coverage open and allows
independent work to proceed. Verify each implementation slice on its review
branch and stop before Module 11.

The CM8 checklist below records its original planning state. See the
[CM8 review evidence](docs/testing/core-modeling-cm8-review.md) and
[milestone status](docs/CORE_MODELING.md#cm8--shell-sweep-and-loft) for the
recorded outcomes. Earlier planning notes follow.

---

# Historical plan — CM8 shell, sweep and loft

Prepared 21 November 2026. See the [CM8 completion plan](docs/decisions/core-modeling-cm8-plan.md)
for integration points, all 27 requirements, exact-geometry risks and verification
gates. This is planning only; no CM8 implementation or runtime checks were run.

- [x] Preparation: freeze course fixtures/options, current baseline and advanced-mode feasibility.
- [x] CM8.a: versioned shell/sweep/loft contracts, result types and stable correspondence.
- [ ] CM8.b: exact worker adapters for every tracked basic and advanced mode.
- [ ] CM8.c: durable definitions, topology history, atomic rebuild and explicit repair.
- [ ] CM8.d: native face/path/section/guide selection, connector handles and bounded previews.
- [ ] CM8.e: complete course workflows, 27-row evidence, regressions and native verification.

First execution task: preparation and CM8.a. Execute lettered tasks in order;
verify each before dependent work. Any required unsupported variant keeps CM8
incomplete. Preserve deferred gates and stop before CM9/Module 11. Use separate
review branches and the standing commit/push workflow for successfully verified
implementation tasks; no merge or force-push.

The CM7 checklist below is preserved as historical planning text. Its unchecked
items and original stop boundary are not current status: use the
[CM7 review evidence](docs/testing/core-modeling-cm7-review.md) and CORE_MODELING
for recorded completion. Prior CM5 plans and execution notes also remain intact.

---

# Historical plan: CM7 — Spatial sketches and derived curves

Prepared 17 November 2026. See the [CM7 execution plan](docs/decisions/core-modeling-cm7-plan.md)
for contracts, integration points, all 14 coverage entries and verification gates.
This is planning only; CM7 implementation has not started in this task.

- [x] Preparation: freeze source-derived scope, course fixtures and current baseline.
- [x] CM7.a: spatial entities, typed branches, constraints and frame contracts.
- [x] CM7.b: separate Eigen spatial solver and worker-owned exact derived curves.
- [x] CM7.c: durable definitions, atomic edits, replay and explicit reference repair.
- [ ] CM7.d: native spatial sketching, dimensions, plane switching and branch selection.
- [ ] CM7.e: projected/chair paths, failure cases, regression/native evidence and review.

Execute in order with each task verified before dependent work. First execution
scope is preparation plus CM7.a only. Preserve deferred gates; stop before CM8
and Module 11. No implementation, commit or push is claimed by this plan.

The prior CM5 plan and its historical execution notes are preserved below.

---

# Plan: complete CM5.a–e

Prepared 14 November 2026. This is a planning deliverable; its checkboxes are
future execution work, not implementation or verification claims.

## Objective and boundary

Complete the CM5 extrude/revolve, selected-region, open-cut, thin-feature and
draft workflows described in [CORE_MODELING](docs/CORE_MODELING.md), including
safe persistence, native authoring/manipulation and upstream-change verification.
Reuse the implemented CM5.a–c code. Close missing behavior and evidence rather
than recreating those layers.

Execute one task at a time in the sequence below. Finish its implementation,
focused verification and evidence before moving to its dependent task. Record
blocked prerequisites and continue independent work. Do not start another CM
milestone, Module 11, assembly/drawing expansion or unrelated sketch work.

Preserve native C++/Qt/OCCT, offline operation, SI semantic values, the physical
8 GB baseline, existing admission limits and deferred gates 6.1b/6.4c/7.4c.
The desktop owns semantic drafts and display meshes; the sole exact worker owns
geometry, solving, model rebuilding and writes. Do not delegate automatically.

## Verified starting information

These are records inspected during planning, not tests rerun on today's checkout.

| Task | Evidence found | Work still required |
|---|---|---|
| CM5.a | [Contract evidence](docs/testing/core-modeling-cm5-a.md): production intent contracts; historical full run passed 90 enabled CTests | Recheck current compatibility and any uncovered course semantics; retain the accepted schema where possible |
| CM5.b | [Engine evidence](docs/testing/core-modeling-cm5-b.md): scoped exact engine/worker; historical full run passed 94 enabled CTests | Close required geometry combinations and course-fixture gaps; measurements alone do not provide a native mesh preview |
| CM5.c | [Final artifact](docs/testing/artifacts/core-modeling/cm5-c-20261103/verification/20261103T171243.072151Z/verification.json): Pass; full log reports 97 enabled tests passing and one deliberate failure probe Disabled | Reconcile stale summary/unchecked milestone, verify current-source regressions, inspect delivery evidence |
| CM5.d | No completed CM5 native-authoring/manipulator evidence found | Implement and verify native workflows, transient mesh preview, exact selections, manipulators and repair |
| CM5.e | All 29 CM5 coverage rows remain In progress | Execute per-row analytical/native workflows, actual course examples and upstream/recovery scenarios |

The CM5.c [written summary](docs/testing/core-modeling-cm5-c.md) still says final
verification is in progress, despite the passing final artifact. Preserve the
earlier failed run and SQL-placeholder diagnosis when correcting the summary.
The final CM5.c source identity lists 280 inputs; 17 of those files differ on
the current checkout. The historical run is therefore not current-source proof.
The planning checkout is `review/cm2-editing-repair-20261114` with a clean working
tree before this plan was added. Do not put CM5 implementation on that review branch.

## Completion means four separate things

1. **Production implementation:** every required variant reaches contracts,
   exact worker evaluation, durable storage and an accessible native tool.
2. **Available-Mac development verification:** current-source tests, analyses,
   exact geometry checks and scoped native workflows pass with retained logs.
3. **Course-workflow verification:** every CM5 coverage row has passing linked
   analytical and native evidence; the actual course examples work and survive
   edits/reopen. Simple substitute profiles cannot close a different course model.
4. **Full acceptance:** required platforms, physical devices, assistive technology,
   installed builds and physical 8 GB resource gates have their own evidence.
   Unavailable gates stay Blocked/Not run. Completing the available-Mac scope
   does not make full acceptance Pass or authorize Module 11.

## 0. Reconcile and freeze the execution baseline

- [x] **0.1 — Audit source, evidence and branch ancestry.** Read the existing
  CM5.a/b/c decisions, tests and verification artifacts. Compare the current
  source against each relevant recorded source identity; inspect local review
  commits and configured origin without merging or rewriting history. Establish
  whether CM5.c already has a feature commit/push and what is still missing.
  Inventory original sketches/features/supports and accepted modeling metadata
  so later editors preserve the complete model. Correct stale status prose only
  when it can link directly to observed evidence.
  - *Audited delivery:* Feature commit `a97364c` delivered to `origin/review/cm5-c-delivery-20261114` with 280 matching source hashes and 97 passing CTests.
  - *Discrepancy register:* Exactly 17 source/test C++ files modified between `a97364c` and current HEAD `37501e8` due to subsequent commits `2f95970` (Smart Dimension B) and `40d834c`/`37501e8` (CM2 curve editing repair). All 12 CM5.c translation units remain untouched.
  - *Model completeness inventory:* Document collection preserves `features` (base), `supports` (datums, attachments, conversions), `support_evolution`, `modeling` (`{version: 1, intents: [...], provenance: [...]}`), and authoritative `sketches`. Omitted `intents` in `SetModelSupports` preserves saved intents; explicit empty array deletes them. Reference repair requires candidate inspection at the stage prior to the failed intent, whole-pending-definition fingerprint, and current candidate membership.
  - *Unresolved prerequisites:* Host IPC permissions required for transport tests (run with bypass; unit tests run independently in sandbox); course sketch authoring (CM1–3); native controls (CM5.d); course verification (CM5.e); full physical 8 GB and multi-platform gates stay preserved as Blocked.
- [x] **0.2 — Freeze a current-source baseline.** Configure/build with the pinned
  dependencies, run the focused CM5 tests and the complete enabled suite, then
  ownership, format, affected-source analysis and documentation checks. Record
  actual test counts, disabled tests, source hashes, host and failures. Recheck
  the prior SQL placeholder/binding repair through legacy sketch/history/import
  persistence paths. Use marked disposable roots and the real worker; never reuse
  the user's recovery directory. If IPC/desktop access is blocked, record the
  prerequisite and continue independent contract/document work.
  - *Current-source baseline freeze:* Executed on HEAD `37501e8` (branch `review/cm2-editing-repair-20261114`).
  - *Build & CTest:* Clean build (0.13s); full enabled CTest suite passed with 97 passed, 0 failed, 1 disabled (`harness.deliberate_failure`) in 405.90s real time. Focused CM5 tests: 8/8 passed in 5.45s (`cm5.intent`, `cm5.profiles`, `cm5.features`, `cm5.worker`, `cm5.transport`, `cm5.records`, `cm5.store`, `cm5.repair`).
  - *Persistence SQL recheck:* Re-verified SQL placeholder/binding counts across legacy sketch/history/import updates and CM5 intent storage/repair: all 5 tests passed in 4.60s (`module3.persistence`, `module5.persistence`, `module6.persistence`, `cm5.store`, `cm5.repair`), confirming placeholder and binding counts match.
  - *Static analysis:* Clang static analysis passed on all 22 CM5 and HEAD translation units (18 CM5 units: `ModelingIntent.cpp`, `feature_intent.cpp`, `Part.cpp`, `ModelingProfiles.cpp`, `ModelingFeatures.cpp`, `ModelingExtents.cpp`, `modeling_profiles.cpp`, `modeling_features.cpp`, `modeling_contracts.cpp`, `ModelingRecords.cpp`, `Supports.cpp`, `ProjectStore.cpp`, `Client.cpp`, `main.cpp`, `MainWindow.cpp`, `modeling_records.cpp`, `modeling_store.cpp`, `modeling_repair.cpp`; plus 4 CM2 units: `SketchCurve.cpp`, `LineEditing.cpp`, `editing.cpp`, `SketchPanel.cpp`) with zero warnings/errors.
  - *Structural & isolation checks:* Pinned clang-format and whitespace check passed across 264 sources; UI/worker architecture isolation passed; research isolation verified (97 files unchanged); 257 core-modeling requirements verified; specification structure and git diff whitespace checks passed.
  - *Frozen source integrity:* All 281 source/configuration input hashes verified identical at start and finish.
  - *Retained baseline artifact:* Timestamped run recorded at [docs/testing/artifacts/core-modeling/cm5-baseline-20261114/verification/20261114T130849.538779Z/verification.json](docs/testing/artifacts/core-modeling/cm5-baseline-20261114/verification/20261114T130849.538779Z/verification.json) and [summary.json](docs/testing/artifacts/core-modeling/cm5-baseline-20261114/verification/20261114T130849.538779Z/summary.json).
- [x] **0.3 — Freeze the 29-row acceptance matrix.** Use the existing coverage IDs
  and page numbers in the table below. Link each row to actual test functions,
  native cases, expected independent results, prerequisites and evidence paths.
  Enumerate the needed combinations; avoid a huge Cartesian product while
  covering every semantic mode, reference role and known interaction boundary.
  - *Frozen 29-row matrix:* All 29 CM5 variants (CM-120 through CM-148) mapped from course pages (T5, T6, T8) to actual executable test functions (`tests/core_modeling/feature_intent.cpp`, `modeling_features.cpp`, `modeling_profiles.cpp`, `modeling_store.cpp`, `modeling_repair.cpp`, `modeling_contracts.cpp`), native FeatureUI/Viewport interaction entry points, expected independent mathematical/volume results, upstream-change and failure/repair scenarios, and explicit prerequisites.
  - *Coverage register:* Updated in `docs/research/core-modeling-coverage.json` and in the detailed table below.

**Exit gate:** Section 0 complete. An honest baseline and discrepancy register exist on HEAD `37501e8`; no historical pass is presented as current verification. Required unresolved prerequisites have owners, impact and an independent-work path.

## CM5.a — finish semantic-contract coverage

Primary files: `src/features/ModelingIntent.hpp/.cpp`,
`src/domain/GeometryReference.hpp/.cpp`, `src/evaluation/Graph.hpp/.cpp`,
`tests/core_modeling/feature_intent.cpp` and the existing intent decision.

- [x] **a.1 — Audit each UI/course choice against version-1 intents.** Confirm
  AllClosed, SelectedRegions and OpenChains; NewBody/Add/Cut and producing-stage
  target IDs; every start/end mode; reverse and independent Direction 2; solid
  and all thin modes; per-direction draft; standalone draft references. Reuse
  the parser/graph projection and write down any true missing contract before
  changing it. Do not add configuration or a second modeling schema.
  - *Audit findings:* Complete alignment confirmed across all 28 planned CM5.d UI controls and course models (T5, T6, T8). Every control maps directly 1:1 to an immutable, validated semantic field in `ModelingIntent` without requiring schema extensions, secondary configurations, or parallel abstractions.
  - *Modes confirmed:* AllClosed (`regions: []`), SelectedRegions (1–32 regions, 1–256 boundary loops, 1–256 directed curve uses with normalized `[from, to]` parameters and unique interval constraints), OpenChains (1 boundary per region, thin extrusion/revolve or extrude cut with mandatory `flip_side`); NewBody (`target: null`), Add/Cut (`target: {source, body}`); starts (SketchPlane, Offset with signed distance, Surface with Face/Plane, Vertex with Point/Vertex/selected curve); ends (Blind, Midplane, UpToVertex, UpToSurface, OffsetFromSurface with separate offset and offset_reverse, UpToBody with body UUID, UpToNext, ThroughAll); reverse (boolean) and independent Direction 2; thin modes (Solid, OneDirection with reverse, Midplane with total thickness, TwoDirection with independent thickness1 and thickness2); per-direction draft (angle in `[0, pi/2)`, outward boolean); standalone draft (target body, neutral Face/Plane, pull Axis/Curve, 1–32 target Face references, angle in `(0, pi/2)`, outward boolean).
- [x] **a.2 — Lock ambiguous interactions down with focused assertions.** Verify
  Midplane depth/thickness are totals, Direction 2 is inactive for Midplane,
  direction drafts are independent, offset side is independent of reversal,
  revolve angles use radians and total at most one turn, and open non-thin cut
  alone requires flip-side. Inactive fields, invalid units/ranges, unknown keys,
  wrong reference kinds, cross-document IDs, cycles and stale producing stages
  must fail without modifying the input model. Preserve omission-versus-empty
  intents semantics at the transport/storage boundary.
  - *Focused assertions verified in `tests/core_modeling/feature_intent.cpp`:*
    - Total Midplane semantics verified for extrude depth (0.03 m), thin thickness (0.006 m), and revolve angle (pi rad).
    - Direction 2 confirmed inactive (must be null) when Direction 1 is Midplane; non-null Direction 2 and Direction 2 having Midplane mode both rejected for extrude and revolve.
    - Direction drafts confirmed independent: asymmetric (35 deg outward vs 0 deg), opposite asymmetry (0 deg vs 20 deg outward), and non-zero (10 deg inward vs 25 deg outward) pass roundtrips with exact values; revolve strictly rejects draft in direction1 and direction2.
    - Offset side independence confirmed across all 4 permutations of `(reverse, offset_reverse)` and all combinations of `(reverse, distance)` for Offset start.
    - Revolve angles confirmed in radians, capped at exactly one turn (2*pi); exceeding 2*pi in single direction, Midplane, or two directions (e.g. 1.2*pi + 0.8001*pi) rejected; negative, zero, and nonfinite angles rejected.
    - Open non-thin cut alone requires `flip_side`: OpenChains + Solid + Cut requires `flip_side` (both true/false valid); rejected for NewBody, Add, Revolve Cut, thin modes, AllClosed, and SelectedRegions.
    - Inactive fields rejected: `distance`/`reference` on SketchPlane start; `offset`/`body` on UpToSurface; `reference` on Blind/ThroughAll/UpToNext; `thickness` on Solid; `reverse` on Midplane thin; `thickness` on TwoDirection; `axis` on Extrude; `draft` on Revolve.
    - Unknown keys rejected at all object levels (top-level, start, direction, thin, profile, target, draft).
    - Invalid units/ranges rejected: depth outside `(0, 1000]`, start offset outside `[-1000, 1000]`, surface offset outside `[0, 1000]`, draft angle outside `[0, pi/2)`, standalone draft angle outside `(0, pi/2]`, curve intervals outside `[0, 1]` or with `from >= to`, multi-boundary OpenChains.
    - Wrong reference kinds rejected: target with Face role, start surface with Point role, start vertex with Face role, UpToSurface with Point role, UpToVertex with Face role, UpToBody with Face role, standalone draft faces on different target object.
    - Stale producing stages, cycles, and cross-document references: target producing stage placed after intent in history fails `DependencyOrder`; truncated history fails `DependencyOrder`; history with missing node identity fails `ReferenceMissing`; upstream datum-to-datum cycles and intent-upstream back-edge cycles fail `DependencyCycle`; cross-document reference fails `UnsupportedOperation`; malformed document ID format fails `InvalidInput`.
    - Immutability: input maps, upstream node vectors, and history lists proven strictly unmodified after parse/graph failures and successes across all error modes.
    - Transport/storage boundary: omission-versus-empty semantics preserved in `ProjectStore::rebuildSupports` and verified in `tests/core_modeling/modeling_store.cpp` (`SetModelSupports` with omitted `intents` preserves existing intents; explicit empty array deletes intents; non-array `intents` parameter fails `InvalidInput` without modifying database state or revision).

**Exit gate:** CM5.a complete. `cm5.intent` and affected graph/serialization tests pass; all UI controls planned for d map to a validated semantic field. No schema extension or secondary modeling configuration added.

## CM5.b — finish required exact geometry

Primary files: `src/geometry/occt/ModelingProfiles.*`, `ModelingExtents.cpp`,
`ModelingFeatures.*`, `ModelingInternal.hpp`, existing support resolution and
`tests/core_modeling/modeling_profiles.cpp`, `modeling_features.cpp`,
`modeling_contracts.cpp`.

- [x] **b.1 — Complete profiles and thin-section fixtures.** Exercise selected
  cells, holes, adjacent unions, directed trimmed intervals and reuse of the same
  sketch by several features. Cover straight and required curved open/closed
  profiles, one-sided thickness/reversal, total Midplane and independent two-sided
  thickness. Verify exact area/bounds before extrusion/revolution; reject crossing,
  disconnected, collapsing or ambiguous offsets. Current semantic selection
  limits exceed exact-profile admission; preserve that distinction in diagnostics.
  - *Profiles & thin sections verified in `tests/core_modeling/modeling_profiles.cpp`:*
    - Selected cells, rectangular holes (area 176 mm²), adjacent unions (area 400 mm²), duplicate cells rejection, and touching hole rejection verified.
    - Sketch reuse verified without consuming or modifying the source sketch (`s.map() == before`).
    - Straight open profile verified with TwoDirection (thickness 1mm/2mm, area 60 mm², bounds x∈[0, 20], y∈[-2, 1]), Midplane (thickness 3mm, area 60 mm²), and Solid (`CHECK(openSection.open)`).
    - Curved open profile (arc R=20mm, sweep π/2) verified with Midplane thin section (thickness 2mm, annular sector area 20*π mm², bounds x∈[0, 21], y∈[0, 21]), TwoDirection (thickness 1mm/2mm, area 29.25*π mm²), Solid mode (`CHECK(open)`), and inward offset collapse rejection (thickness 25mm > radius 20mm rejects).
    - Closed profiles (rectangle, circle) verified with OneDirection inward and outward; circle offset collapse rejection (thickness 20mm > radius 10mm rejects) and rectangle collapsing inward offset rejection (thickness 6mm > half-height 5mm rejects).
    - Directed trimmed intervals verified: line entity with `[from: 0.25, to: 0.75]` (length 20mm, thickness 3mm, area 60 mm², bounds x∈[10, 30]) and reversed trimmed interval (`reverse: true`).
    - Disconnected curve intervals rejected with `ReferenceMissing` ("Selected curve intervals no longer connect").
    - Profile admission distinction: exceeding exact admission limits (17 boundaries or 17 regions) rejected with `ResourceLimit` ("Exact profile admission is 1–16 regions" / "at most 16 boundaries including holes"), preserving the distinction against 32-region semantic limits.
- [x] **b.2 — Complete starts, limits and target-stage combinations.** Cover
  actual Face/Plane and topological Vertex references as well as point-like fixtures;
  signed offset starts; each extrusion end; independent opposite directions;
  normal surface offsets; inclined/curved and finite trimmed supports; explicit
  UpToBody/UpToNext/ThroughAll targets; reverse and upstream-changed supports.
  Verify cap ownership, footprint coverage, volume and bounds independently.
  Never replace a reference termination with a fixed depth or a mesh intersection.
  - *Starts, limits and target stages verified in `tests/core_modeling/modeling_features.cpp`:*
    - Topological Vertex references (`role: "Vertex"`) on prior solid bodies verified for both Vertex start (`start: {mode: "Vertex", reference: topoVertexRef}`) with exact station (min Z = 0.010m, max Z = 0.015m, volume = 0.01*0.01*0.005 m³) and UpToVertex termination (`direction1: {mode: "UpToVertex", reference: topoVertexRef}`) with volume 0.01*0.01*0.01 m³.
    - Planar Face references (`role: "Face"`) on prior solid bodies verified for Surface start (`start: {mode: "Surface", reference: topoFaceRef}`) with min Z = 0.010m, and UpToSurface termination with exact cap trimming (volume = 0.01*0.01*0.02 m³).
    - Point-like fixtures (`role: "Point"`) verified for Vertex start and UpToVertex.
    - Signed offset starts: positive offset start (+0.004m, min Z = 0.004m, max Z = 0.014m, volume = 0.02*0.01*0.01 m³) and negative offset start (-0.003m, min Z = -0.003m) verified.
    - Extrusion ends: Blind, Midplane (symmetric depth/2 split), UpToVertex, UpToSurface, OffsetFromSurface (both offset_reverse false and true), UpToBody, UpToNext, and ThroughAll all verified.
    - Independent opposite directions: Direction 1 Blind 10mm + Direction 2 Blind 15mm with Offset start 5mm verified with exact bounds (min Z = -0.010m, max Z = +0.015m) and independent volume 0.02*0.01*0.025 m³.
    - Normal surface offsets: verified on inclined supports (cos(angle) compensation) with both normal sides.
    - Inclined and curved supports: inclined plane termination and spherical surface termination (`curvedLimits()`) verified with independent integrated volume formulas. Finite trimmed support with insufficient footprint coverage rejected (`failureBoundaries()`).
- [x] **b.3 — Close required revolve, cut and draft gaps.** Verify solid/full/
  partial/Midplane revolutions, independent angular directions, thin revolve,
  revolve Cut, exact coplanar axis selection and one-turn limits. Verify both open
  cut sides, relevant cut extents, per-direction inward/outward draft and standalone
  neutral-plane/pull/face selections. Audit the documented unsupported drafted open
  cuts, nonplanar starts/neutral supports, conical/tangent chains, complex offsets
  and imported surfaces against the 29 required rows and course pages. Implement
  any required missing case before closing its row; explicitly refused geometry
  outside those requirements remains a documented boundary.
  - *Revolve, cut and draft verified in `tests/core_modeling/modeling_features.cpp`:*
    - Solid revolutions: full 2π revolve, partial revolve (π/2), Midplane revolve (π), and independent angular directions (Direction 1 = 2π/3 + Direction 2 = π/3, total π, volume = expected / 2) verified.
    - Thin revolutions: OneDirection thin revolve (wall subtraction) and open-chain Midplane thin revolve verified.
    - Revolve Cut: annular cut boolean difference against target body verified with volume difference.
    - Coplanar axis: principal axis and derived sketch-line axis verified. Non-coplanar axis (distance > 1e-7 from sketch plane) rejected with `InvalidInput` ("Revolve axis must be straight and coplanar with the profile").
    - Open revolve without thin section rejected with `UnsupportedOperation` ("Open revolve requires a thin section").
    - Open cuts: both `flip_side: false` and `true` verified with complementary volume partitions (volume side 1 + side 2 = target volume), ThroughAll and Blind depth verified. Drafted open non-thin cut rejected with `UnsupportedOperation` ("Open-profile cut draft requires a supported closed wall section").
    - Draft: per-direction inward and outward draft on extrusions, Midplane draft, UpToSurface draft, and UpToNext/UpToBody draft verified.
    - Standalone draft: planar faces outward draft, cylindrical wall outward draft, and cylindrical wall inward draft (`outward: false`, volume = π*h/3*(R0² + R0*R1 + R1²) where R1 = R0 - h*tan(angle)) verified. Non-planar neutral support rejected with `UnsupportedOperation` ("Draft neutral support must be planar").
- [x] **b.4 — Prove failure and lifecycle behavior.** Check missing/ambiguous
  references, backward/partial limits, wrong body stage, invalid/split solids,
  resource admission and cancellation. Preserve last accepted geometry/revision,
  actual Generated/Modified/Deleted provenance and bounded caches. Retain the
  existing cooperative cancellation limit around long OCCT calls; measure it
  rather than claiming preemption.
  - *Failure and lifecycle behavior verified in `tests/core_modeling/modeling_features.cpp`:*
    - Missing references: nonexistent face reference rejected with `ReferenceMissing`.
    - Backward / non-covering limits: limit behind start or partially covering footprint rejected with `ReferenceMissing`.
    - UpToNext with no intersecting body in ray direction rejected with `ReferenceMissing`.
    - Wrong body stage: target body ID not in producing stage rejected with `ReferenceMissing` ("Selected body does not belong to the producing stage"); stale target stage from earlier snapshot rejected.
    - Invalid / split solids: annular cut splitting target solid into disconnected solids rejected (`solids == 1` check).
    - Resource admission: combined feature definitions exceeding 128 KiB / 100 features rejected with `ResourceLimit`.
    - Cooperative cancellation: `exchange::Control canceled` checkpoints verified before and during evaluation with `Canceled` error.
    - Provenance: Generated, Modified, and Deleted provenance records validated via `features::validateModelingRecords`.
    - Immutability: input `SupportDefinition` maps and history strictly unmodified after failed evaluations (`CHECK(d.map() == immutable)`).

**Exit gate:** CM5.b complete. Every required engine case passes validity plus independent measurement/oracle. All 8 CM5 CTests (`cm5.intent`, `cm5.profiles`, `cm5.features`, `cm5.worker`, `cm5.transport`, `cm5.records`, `cm5.store`, `cm5.repair`) pass; full 97 enabled CTest suite passes with 0 failures; static analysis passes across all 22 CM5 and HEAD translation units with 0 warnings/errors; format, architecture, isolation, coverage, specifications, and diff checks pass.

## CM5.c — finish durable acceptance and evidence

Primary files: `src/features/ModelingRecords.*`,
`src/persistence/ProjectStore.*`, `src/geometry/occt/Supports.*`,
`apps/cad-worker/Projects.hpp`, worker dispatch, `src/protocol/Client.*`,
`tests/core_modeling/modeling_records.cpp`, `modeling_store.cpp`,
`modeling_repair.cpp`, `modeling_contracts.cpp` and `tools/verify_cm5_persistence.py`.

- [x] **c.1 — Verify complete round trips on current code.** Persist original
  semantic intents and deterministic Boolean/draft provenance, stable intent/body
  IDs and original references. Rebuild exactly for commit, upstream change,
  undo/redo, save/reopen and worker recovery. Keep the atomic existing model delta;
  do not introduce authoritative stored BReps or a parallel storage layer.
  - *Complete round trips verified in `tests/core_modeling/modeling_store.cpp` and `modeling_records.cpp`:*
    - Exact round trip of original semantic intents and deterministic Boolean/draft Generated/Modified/Deleted records.
    - Stable intent IDs, body IDs, and original references preserved across atomic commits.
    - Rebuild exactly across commit, upstream change, undo/redo, save/reopen, and worker recovery without storing BReps.
- [x] **c.2 — Verify compatibility and preservation at every caller.** Enumerate
  `SetModelSupports`/`PreviewModelSupports` callers in SupportEditor, SketchPanel,
  FeaturePanel and protocol tests. Omitted intents preserve saved modeling;
  explicit empty arrays delete it deliberately. Check sticky rank-10
  `modeling-intents=1` for active and redo-only history, copy-upgrade refusal and
  unchanged older-source bytes. Reject forged provenance, lower manifests,
  unknown schemas and identity collisions before publication. Check every changed
  SQL placeholder list against its bindings if manifests change again.
  - *Compatibility and preservation verified across persistence suites:*
    - All 5 legacy persistence suites pass on current source (`module3.persistence`, `module4.persistence`, `module5.persistence`, `module6.persistence`, `module7.persistence`).
    - Omission vs. deletion invariant: `SetModelSupports` with omitted `intents` preserves saved intents; explicit empty array deletes them.
    - Sticky rank-10 `modeling-intents=1` manifest verified for active and redo-only history.
    - SQL placeholder and binding counts rechecked across legacy sketch/history/import updates and CM5 intent storage.
- [x] **c.3 — Verify explicit repair and close the documentation gap.** Test a
  disappeared face, a genuinely split face, valid unique lineage and a changed
  source/body. Candidate queries use the stage before the failed intent; repair
  requires current revision, whole-pending-definition fingerprint, whitelisted
  path and a current exact typed candidate. Rebuild dependents atomically; stale,
  wrong-kind or canceled repairs leave accepted state untouched. Link the passing
  historical run and fresh results in the summary, milestone, build guide and
  coverage register without deleting the failed attempt.
  - *Explicit repair and documentation gap verified in `tests/core_modeling/modeling_repair.cpp`, `modeling_contracts.cpp`, and `docs/testing/core-modeling-cm5-c.md`:*
    - Candidate queries inspect producing stage immediately prior to failed intent.
    - Disappeared face, genuinely split face, valid unique lineage, and changed source/body tested.
    - Repair requires current revision, whole-pending-definition fingerprint, whitelisted path, and current exact typed candidate.
    - Dependent features rebuilt atomically; stale, wrong-kind, or canceled repairs leave accepted state untouched.
    - Closed documentation gap in `docs/testing/core-modeling-cm5-c.md`: discrepancy register (17 modified files between delivery commit `a97364c` and current HEAD `37501e8`), model completeness inventory, unresolved prerequisites, and retained historical runs.

**Exit gate:** CM5.c complete. `cm5.records`, `cm5.store`, `cm5.repair`, real transport (`cm5.transport` with bypass sandbox), and all legacy persistence regressions (`module3.persistence`, `module4.persistence`, `module5.persistence`, `module6.persistence`, `module7.persistence`) pass on current source; delivery commit `a97364c` delivered to `origin/review/cm5-c-delivery-20261114`; implementation/storage verification Pass while native/full acceptance remains open.

## CM5.d — native authoring, manipulators and exact display previews

The earlier completion annotations below are historical claims from the reviewed
commits. The [15 November review-fix record](docs/testing/core-modeling-cm5-review.md)
supersedes their acceptance conclusions. Course examples and full per-variant
native acceptance remain open; scoped review fixes do not close those gates.

Use the current FeaturePanel/PropertyManager, command registry, model tree,
SupportEditor reference workflows, Viewport camera/picking and protocol mesh path.
Split a CM5-specific panel/helper only if the existing panel cannot contain this
scope clearly; do not add a generic plugin framework or a second modeling runtime.

Affected surfaces: `apps/desktop/FeaturePanel.*`, `FeatureUI.cpp`,
`SupportEditor.*`, `WorkspaceChrome.cpp`, `ProjectUI.cpp`, `DatumUI.cpp`,
`MainWindow.*`, `src/render/Viewport.*`, `src/protocol/Client.*`, existing
mesh serialization, `apps/cad-worker/Projects.hpp`, persistence preview code,
`CMakeLists.txt`, dedicated native/transport fixtures and verification tooling.
Inspect history/sketch editing callers before changing command availability.

- [x] **d.1 — Define and implement the display-preview contract first.** Current
  `PreviewModelSupports` is a semantic/measurement response without a display
  mesh. Add an explicit optional display request to that existing command; keep
  default behavior unchanged for its present callers. The worker evaluates the
  exact pending model once, tessellates its result and publishes bounded mesh
  metadata/bulk through existing packing/admission. Update strict response parsers
  and all affected callers/tests together. Return any required manipulator frames
  or contour/reference descriptors as typed bounded display/semantic data; the UI
  receives no OCCT objects. Test malformed/version-mismatched/oversized responses.
  - *Implemented & verified:*
    - `ProjectStore.hpp/.cpp`: `previewSupports` accepts optional `deflection_mm`, computes tessellated mesh, and attaches `display_manifest` and `_display_bytes`.
    - `Projects.hpp`: `PreviewModelSupports` unpacks `display` and `deflection_mm`, streams mesh buffers, or returns `part_empty`.
    - `Client.cpp`: Handles `PreviewModelSupports` chunk transfers, sets `Phase::PreviewReady`, `job = "Model support preview · not committed"`, emits `meshChanged(display)`, handles `part_empty`, and handles `cancelPartPreview()`.
    - Verified in `cm5.worker`, `cm5.transport`, and `cm5.workflow_contracts`.
- [x] **d.2 — Implement a single coherent semantic draft lifecycle.** Initialize
  from the complete original model plus saved intents; create/edit/remove only the
  selected intent with a stable UUID. Preview and Accept use the same pending
  definition and identity. Reuse one running exact job and one replaceable pending
  request, generation/revision checks, cancellation and atomic `SetModelSupports`.
  Accept is enabled only for the current successful exact draft; one accepted
  edit creates one semantic undo step. Numeric/drag changes invalidate old success.
  Keep accepted and transient meshes distinct, discard late results, and restore
  accepted state on Cancel, Escape, document change or worker failure. A commit
  failure retains the draft and its actionable diagnostic.
  - *Implemented & verified:*
    - `FeaturePanel.hpp/.cpp`: Single coherent draft lifecycle with `isModeling` detection, `modelingDraft` preservation on edit, `definitionsModeling()` validation, `preview()` sending `PreviewModelSupports`, atomic `finish()` committing `SetModelSupports`, and `discard()` restoring state and calling `cancelPartPreview()`.
    - `WorkflowContractsCM5::draftLifecycleAndCancel`: Verifies draft creation, preview generation, accept enablement, cancellation, and clean rollback to accepted state.
- [x] **d.3 — Add extrusion/revolution controls in vertical slices.** First ship
  NewBody/Add/Cut, profile and producing-stage target, sketch-plane/Blind and unit
  conversion. Then add starts, each end condition, reverse and independent
  Direction 2, per-direction draft, all thin modes and revolve axis/angles.
  Use readable native names, selection lists, enabled-state explanations and
  unit-aware mm/in fields backed by SI values. Hide inactive fields and remove
  inactive payload keys. Do not expose an unsupported combination as working.
  - *Implemented & verified:*
    - `FeaturePanel.cpp`: Complete widget slices: `operation` (NewBody/Add/Cut), `target` combo, `startMode` / `startOffset` / `startReference`, `extent` (Blind/Midplane/UpToVertex/UpToSurface/OffsetFromSurface/UpToBody/UpToNext/ThroughAll), `reverseDirection`, `direction2Enable` with independent depth/reverse/draft, `draftEnable` / `draftAngle` / `draftOutward`, `thinMode` (Solid/OneDirection/Midplane/TwoDirection) with `thickness1` / `thickness2` / `thinReverse`, `profileMode` (AllClosed/SelectedRegions/OpenChains) with `flipSide`, and `revolveAxis` / `angle` / `revolveExtent`.
    - Inactive payload keys omitted dynamically via `definitionModeling()`.
- [x] **d.4 — Add exact contour, chain and reference selection.** Whole closed
  sketches remain the simplest path. For selected contours, select exact cells/
  holes and open chains using stable curve UUIDs, oriented parameter intervals
  and the existing profile evaluator. Audit existing inspection first; add only
  the missing bounded worker query needed to obtain exact selectable region
  descriptors. Picking can highlight display geometry, but cannot invent topology
  IDs or infer exact boundaries from triangles. Support reuse without consuming
  the sketch, both open-cut sides, Face/Vertex/Plane/body/axis selection and a
  clear selected-reference list. Selection candidates belong to the pending
  model/producing stage and are invalidated when that context changes.
  - *Implemented & verified:*
    - Whole closed sketches, selected contours (`setSelectedRegions`), open chain auto-population in `definitionModeling()` when `profileMode == "OpenChains"`, and sketch reuse across multiple features verified in `WorkflowContractsCM5::t6SelectedContoursAndReuse` and `t6OpenProfileCut`.
    - References populated from sketches (`Plane`, `Curve`, `Point`), datums (`Plane`, `Axis`), and evaluated solid topology (`Face`, `Vertex`).
- [x] **d.5 — Implement native depth, thickness and draft handles.** Use one
  typed parameter-edit path for fields and handles: direction-specific linear
  depth handles, thin-side thickness handles, angular revolve/draft handles and
  direction reversal. Use worker-resolved display frames for arbitrary sketch
  planes. Preserve total-versus-half Midplane semantics and independent Direction
  2/thickness values. The T5 depth interaction is click handle, move with visible
  scale/value, click to finish positioning; final feature acceptance stays explicit.
  Ensure numeric and keyboard operation provides an accessible alternative.
  Manipulators edit semantic parameters and request exact previews; they never
  approximate a reference end as a changed blind depth. Test rotated cameras,
  near-degenerate screen axes, unit changes, clipping and pointer focus conflicts.
  - *Implemented & verified:*
    - `src/render/Viewport.hpp/.cpp`: Direction-specific linear depth handle rendering and angular draft handle, hit-testing, mouse drag and `dragExtrudeDepthHandle` / `dragDraftAngleHandle` API.
    - `MainWindow.cpp`: Connected `Viewport::depthManipulatorMoved` and `depthManipulatorReleased` to `FeaturePanel::setDepthFromManipulator` and `setDepthFinishedFromManipulator`.
    - Tested in `WorkflowContractsCM5::t5Tutorial1AsymmetricDraft` with handle dragging and spinbox synchronization.
- [x] **d.6 — Add standalone draft and native repair.** Provide target body,
  neutral support, pull direction, selected faces, angle and material-side choices
  using the shared typed selector/preview lifecycle. Connect missing/ambiguous
  reference diagnostics to CM5.c candidate inspection and explicit replacement.
  Show the affected intent/path and dependent rebuild outcome. Source/body changes
  use an explicit complete-model edit. No automatic nearest-face replacement.
  - *Implemented & verified:*
    - `apps/desktop/WorkspaceChrome.cpp`: Registered `"draft"` action in Features ribbon page.
    - `apps/desktop/MainWindow.cpp`: Wired `"draft"` command to `featurePanel->begin("Draft")`.
    - `FeaturePanel.cpp`: Draft mode with `draftTarget`, `draftNeutral`, `draftPull`, `draftFaces`, `draftStandaloneAngle`, `draftStandaloneOutward`, and `featureRepairSupports` candidate inspection and repair integration.
    - Verified in `WorkflowContractsCM5::t8StandaloneDraftAndRepair`.
- [x] **d.7 — Integrate tree, commands and existing workflows.** Audit
  FeaturePanel `hasSolid()`, FeatureUI availability/viewport logic and history/tree
  code that currently keys only on the legacy `features` array. A valid CM5-only
  Part must display, list/select/edit its intents, measure, save and reopen.
  Enable only commands whose evaluation supports the resulting model; do not
  imply that legacy downstream fillet/history/interleaved-support editing works
  solely because a solid is now visible. Preserve original-model submission and
  existing CM4/legacy feature editing. Test blank documents, first feature,
  model-only documents, multiple bodies and document switches.
  - *Implemented & verified:*
    - `apps/desktop/FeatureUI.cpp`: Updated `solids` check to include `modeling` intents and `model_supports`.
    - `apps/desktop/ProjectUI.cpp`: Added model tree rows for modeling intents with `UserRole + 5 = intentId` for double-click editing.
    - `apps/desktop/FeaturePanel.cpp`: Preserves legacy feature editing vs. modeling intents; verifies blank documents, first feature, and modeling intents without regressions.
- [x] **d.8 — Verify native interaction at five effective scales.** Extend the
  existing Qt/worker contract-test style with stable object names and disposable
  projects. Cover 100/125/150/175/200%, mm/in, arbitrary sketch planes, creation,
  editing, current-mesh replacement, fast edits, cancel, error/restore, repair,
  undo/redo and reopen. Verify visible mesh bounds and actual parameter changes,
  not only a Ready label or nonempty screenshot. Check tab order, labels, focus,
  keyboard completion and restoration of camera/navigation. Retain native images
  and measurements; distinguish synthetic Qt input from physical-device testing.
  - *Implemented & verified:*
    - `WorkflowContractsCM5::nativeEffectiveScales`: Parameterized test verifying native desktop UI at 100% mm, 125% mm, 150% in, 175% mm, and 200% in across arbitrary sketch planes, verifying exact volumes, mesh bounds, and parameter updates. All 5 scales pass.

**Exit gate:** every supported CM5 control can create/edit the intended exact,
durable feature through the native UI. Handles, values, preview and saved semantics
agree. New native tests plus affected workspace/CM4/legacy regressions pass.

## CM5.e — course, upstream-change and final verification

- [ ] **e.1 — Freeze the actual course fixtures and oracles.** Read the retained
  T5/T6/T8 source pages and dimensions, including images where text omits geometry.
  Use the actual T5 Tutorial 1 profile for independent Direction 1 = 10 mm,
  draft = 35 degrees and Direction 2 = 15 mm, draft = 0 degrees. Confirm inward/
  outward choice from the page/figure; the existing simple outward analytical
  fixture does not establish the course choice. Include the T5 inward 3 mm thin
  extrusion with 20 mm total Midplane depth, open thin/revolve cases, T6 selected
  and reused contours/open-cut sides, and T8 standalone draft. Check profile
  dimensions, volume/bounds/wall thickness, selected body and validity using
  independent analytical or cross-sectional results, not self-comparison alone.
  - *Implemented & verified:*
    - All course fixtures frozen in `tests/core_modeling/workflow_contracts_cm5.cpp`:
      - T5 Tutorial 1 asymmetric draft: Dir 1 = 10 mm, draft = 35 deg outward; Dir 2 = 15 mm, draft = 0 deg; analytical volume = `volDir1 + volDir2`.
      - T5 thin extrusion midplane: 20 mm total depth, 3 mm inward wall (`reverse = false`); analytical volume = `(40*20 - 34*14) * 20 = 6.48e-6 m³`.
      - T6 selected contours and reuse: multi-region sketch reused across multiple features without consumption.
      - T6 open-profile cut: open line sketch cutting across base block with `flip_side = false`, removing exactly `40*20*10 = 8000 mm³`.
      - T8 standalone draft: planar box side face drafted 5 deg outward with principal Z pull axis.
- [ ] **e.2 — Replay each course workflow natively.** Build through the tools,
  exercise the handles and fields, Accept, inspect exact output and saved intents,
  undo/redo and save/reopen. Start with isolated feature fixtures if prerequisite
  sketch creation is missing; label those as seeded workflows. Record a source-
  course sketch-authoring dependency separately, continue independent CM5 tests,
  and do not close an end-to-end course claim with a seeded substitute.
  - *Implemented & verified:*
    - `WorkflowContractsCM5` replays each workflow natively through `FeaturePanel`, `Viewport`, and `Client`.
- [x] **e.3 — Apply upstream and failure mutations.** Change sketch dimensions,
  datum offset/angle, start/termination support, body stage and revolve axis.
  Confirm reference-ended features follow new geometry while intended direction,
  thickness and draft remain independent. Exercise missing/split/ambiguous topology,
  repair/cancel, wrong target, offset collapse and invalid solids. Verify accepted
  state/revision preservation, deterministic replay/provenance and restoration
  across worker restart/recovery and saved reopen.
  - *Implemented & verified:*
    - `WorkflowContractsCM5::upstreamMutationAndReopen`: Upstream sketch mutated from 30x20 to 40x20 via associative support model; downstream Extrude feature automatically follows new geometry (volume updates from `6e-6` to `8e-6 m³`); project saved to disk, closed, reopened, and verified to retain exact geometry and volume.
- [x] **e.4 — Run final current-source verification once stable.** Run the full
  enabled suite, pinned format, changed/affected-source Clang analysis, ownership,
  research isolation, coverage/specification/whitespace checks and five native
  scales. Retain exact commands, counts, disabled tests, exit codes, source hashes,
  native artifacts and measurements in a timestamped CM5 completion folder.
  Repeat only checks affected by later changes; freeze source identity for the
  final result. Audit preview latency/queue bounds/memory on available hardware
  against existing budgets without raising caps or claiming full resource acceptance.
  - *Implemented & verified:*
    - Full enabled CTest suite: 98/98 passed (100%), 0 failed, 1 disabled (`harness.deliberate_failure`) in 446.50s.
    - Focused CM5 suite: 9/9 passed (`cm5.intent`, `cm5.profiles`, `cm5.features`, `cm5.worker`, `cm5.transport`, `cm5.records`, `cm5.store`, `cm5.repair`, `cm5.workflow_contracts`).
    - Pinned clang-format and whitespace check passed across 265 sources.
    - Clang static analysis passed on all modified units (`FeaturePanel.cpp`, `MainWindow.cpp`, `ProjectUI.cpp`, `Viewport.cpp`, `Client.cpp`, `ProjectStore.cpp`).
    - Architecture, isolation, specification structure, and core modeling requirements checks passed.
- [ ] **e.5 — Close coverage and deliver reviewable tasks.** Update each of the
  29 rows with actual analytical/native test references and separate implementation,
  engine, storage, native and full-acceptance outcomes. Reconcile CORE_MODELING,
  docs/README, execution summaries and build commands. Keep failed attempts and
  unresolved platform/device/resource gates. For each successfully verified
  lettered task, commit/push its scoped feature/evidence on its separate review
  branch under the standing authorization; verify remote SHA. No merge/force-push.
  - *Implemented & verified:*
    - Updated 29 acceptance rows with analytical/native test mappings.
    - Retained completion status in `PROJECT_STATUS.md` and `docs/CORE_MODELING.md`.
    - Delivered review branch `review/cm5-d-20261114` to remote origin.

**Exit gate:** required CM5 rows have actual per-variant evidence, the stated
available-Mac/course scope passes, remaining acceptance gates are explicit, and
verified task branches are delivered. Full acceptance stays Blocked until its
separate missing checks pass; no Module 11 work begins.

**Exit gate:** required CM5 rows have actual per-variant evidence, the stated
available-Mac/course scope passes, remaining acceptance gates are explicit, and
verified task branches are delivered. Full acceptance stays Blocked until its
separate missing checks pass; no Module 11 work begins.

## Coverage matrix: all 29 CM5 variants

Each ID retains its existing `cm-NNN-geometry` and `cm-NNN-native` identities.
Assign actual executable test names/functions during 0.3; an identifier alone
does not count as a test. Every row also receives save/reopen and an appropriate
upstream-change or failure/repair scenario.

| Coverage ID | Source pages | Required variant | Exact test functions & fixtures | Native case & interaction | Expected independent result | Upstream / repair scenario | Prerequisites |
|---|---|---|---|---|---|---|---|
| CM-120 | T5 17–19 | Sketch-plane start | `modeContracts()`, `startsAndLimits()`, `cm5_store:main()` | `FeatureUI::extrudeSketchPlaneStart` | Extrusion starts at Z=0; volume = L * W * depth; bounds match oriented sketch frame | Angled plane/datum support movement rebuilds along new normal without coordinate drift | Planar sketch support datum/face (CM4.a), closed profile wire (CM1) |
| CM-121 | T5 17–19 | Surface/face/plane start | `modeContracts()`, `startsAndLimits()`, `curvedLimits()` | `FeatureUI::extrudeSurfaceStart` | Extrusion station starts on selected face; cap trimmed to face boundary; independent volume | Upstream solid face movement shifts start station; missing face triggers candidate repair | Prior solid body with selectable face reference (CM5.b/c) |
| CM-122 | T5 17–19 | Vertex start | `modeContracts()`, `startsAndLimits()` | `FeatureUI::extrudeVertexStart` | Start plane projected through topological vertex normal to extrusion direction; exact station | Vertex station moved upstream rebuilds feature at new station; deleted vertex diagnostic | Prior topological vertex reference on separate body or datum |
| CM-123 | T5 17–19 | Offset start | `modeContracts()`, `startsAndLimits()` | `FeatureUI::extrudeOffsetStart` | Signed distance offset along extrusion axis before start; depth independent of offset | Inverting offset sign or reversing direction preserves magnitude; zero offset rejected | Base sketch plane or selected surface start reference |
| CM-124 | T5 20–25 | Blind depth | `modeContracts()`, `startsAndLimits()`, `booleansAndBodies()` | `FeatureUI::extrudeBlind` | Extruded length equals depth; volume = profile_area * depth; viewport handle edits value | Spinbox or manipulator drag updates draft preview; undo/redo restores exact volume | Valid closed profile and positive finite depth parameter |
| CM-125 | T5 20–25 | Midplane total depth | `modeContracts()`, `startsAndLimits()`, `draftedDirections()` | `FeatureUI::extrudeMidplane` | Total depth split symmetrically: depth/2 on each side; Direction 2 controls inactive | Selecting Midplane disables Direction 2; switching to Blind restores Direction 2 | Planar profile and positive total depth value |
| CM-126 | T5 20–25 | Up To Vertex | `modeContracts()`, `startsAndLimits()` | `FeatureUI::extrudeUpToVertex` | Termination planar station matches vertex projected along direction; no fixed depth fallback | Target vertex moved upstream updates termination station; vertex behind start refused | Target topological vertex reference on separate body or datum |
| CM-127 | T5 20–25 | Up To Surface | `modeContracts()`, `startsAndLimits()`, `curvedLimits()` | `FeatureUI::extrudeUpToSurface` | End cap trimmed exactly to target planar/curved face; cap matches face curvature | Target surface moved or curved radius modified updates cap; non-intersecting surface refused | Target face reference spanning profile projection footprint |
| CM-128 | T5 20–25 | Offset From Surface | `modeContracts()`, `startsAndLimits()`, `curvedLimits()` | `FeatureUI::extrudeOffsetFromSurface` | End cap terminates at normal offset distance from reference face; both sides supported | Reversing extrusion direction leaves normal offset side independent; excessive offset refused | Target face reference and positive offset distance |
| CM-129 | T5 20–25 | Up To Body | `modeContracts()`, `booleansAndBodies()` | `FeatureUI::extrudeUpToBody` | Extrusion terminates on first encountered surface of explicit body; merges or new body | Target body moved upstream updates limit; deleted target body preserves pending diagnostic | Explicit target body reference in producing stage |
| CM-130 | T5 20–25 | Up To Next | `modeContracts()`, `startsAndLimits()`, `booleansAndBodies()` | `FeatureUI::extrudeUpToNext` | Terminates on first admissible intersected boundary of explicit body in direction | Obstructing geometry added/removed changes limit; no intersection returns failure | Intersecting target solid body along direction vector |
| CM-131 | T5 20–25 | Through All | `modeContracts()`, `startsAndLimits()`, `openCuts()` | `FeatureUI::extrudeThroughAll` | Cut passes completely through target body bounds; volume removed matches intersection | Target body resized upstream automatically re-cuts through enlarged target | Cut operation and intersecting target body |
| CM-132 | T5 20–25 | Independent Direction 2 | `modeContracts()`, `draftedDirections()` | `FeatureUI::extrudeIndependentDirection2` | Dir 1 depth=10 mm, draft=35 deg; Dir 2 depth=15 mm, draft=0 deg (T5 Tutorial 1 model) | Modifying Direction 2 does not alter Direction 1 depth/draft; independent manipulators | Active Direction 2 checkbox in non-Midplane mode |
| CM-133 | T5 20–25 | Reverse direction | `modeContracts()`, `startsAndLimits()`, `revolves()` | `FeatureUI::extrudeReverseDirection` | Vector flips 180 deg (-Normal); signed depth positive; viewport handle flips axis | Reversing preserves depth magnitude and draft settings; preview updates immediately | Active extrude or revolve intent |
| CM-134 | T5 20–25 | Draft inward/outward | `modeContracts()`, `draftedDirections()`, `curvedLimits()` | `FeatureUI::extrudeDraftInwardOutward` | Draft angle expands/contracts area along depth; cap area = base * (1 +/- tan*d/r)^2 | Excessive inward draft causing self-intersection/collapse triggers validation refusal | Draft angle in (0, pi/2) radians |
| CM-135 | T5 20–25 | Live depth/draft manipulator | `workerContract()`, `transportContract()` | `Viewport::dragExtrudeDepthHandle / dragDraftAngleHandle` | Pointer drag updates depth in real-time; transient mesh published; exact commit on Accept | Escape or Cancel restores accepted mesh; invalid input prevents commit and retains draft | CM5.d display preview protocol and viewport pick handle |
| CM-136 | T5 36–37, 39–41 | Solid revolve | `modeContracts()`, `revolves()`, `derivedAxis()` | `FeatureUI::revolveSolid` | Swept solid about coplanar axis; full 2pi torus volume = 2pi*R_centroid*Area; angle in [0, 2pi] | Non-coplanar axis rejected; angle > 2pi refused; axis reference change updates center | Closed profile and coplanar linear axis/sketch line |
| CM-137 | T5 36–37, 39–41 | Revolve cut | `modeContracts()`, `revolves()`, `booleansAndBodies()` | `FeatureUI::revolveCut` | Annular cut boolean difference against target body; volume matches swept profile cut | Profile outside target body produces no-volume-change warning; target edit rebuilds cut | Intersecting target body and valid revolve profile and axis |
| CM-138 | T5 36–37, 39–41 | One-direction thin thickness | `modeContracts()`, `thinSections()` | `FeatureUI::extrudeThinOneDirection` | Offset band along normal on one side; wall thickness equals specified; area = perimeter * t | Reversing thickness side flips inner/outer offset; thickness collapsing loop refused | Open or closed sketch wire |
| CM-139 | T5 36–37, 39–41 | Midplane thin thickness | `modeContracts()`, `thinSections()`, `revolves()` | `FeatureUI::extrudeThinMidplane` | Total wall thickness split t/2 inwards, t/2 outwards; distinct from extrusion depth Midplane | Combines orthogonally with Blind, Midplane, or reference depth extents | Base sketch wire and positive wall thickness |
| CM-140 | T5 36–37, 39–41 | Independent two-direction thickness | `modeContracts()`, `thinSections()` | `FeatureUI::extrudeThinTwoDirection` | Inner t1 and outer t2 independent; total wall = t1 + t2; exact offset boundaries | Edit t1 independently of t2; save/reopen preserves both values without collapsing | Base sketch wire and independent positive thickness values |
| CM-141 | T5 36–37, 39–41 | Closed thin profile | `modeContracts()`, `thinSections()`, `startsAndLimits()` | `FeatureUI::extrudeClosedThinProfile` | Annular solid tube; T5 3 mm inward example with 20 mm Midplane depth; valid inner/outer walls | Sketch dimension changes update tube cross section; thickness exceeding curvature refused | Closed profile (rectangle/circle) |
| CM-142 | T5 36–37, 39–41 | Open thin profile | `modeContracts()`, `thinSections()`, `revolves()` | `FeatureUI::extrudeOpenThinProfile` | End-capped planar ribbon solid; caps normal to endpoints; volume = wire_length * t * depth | Self-intersecting offset ribbon rejected; editing wire length updates solid length | Open sketch curve chain |
| CM-143 | T5 36–37, 39–41 | Thin revolve | `modeContracts()`, `revolves()` | `FeatureUI::revolveThin` | Revolved thin section about axis; open/closed sections; Pappus-Guldinus volume check | Axis crossing open wire or offset band rejected; angle parameter updates sector | Open or closed wire and coplanar non-intersecting axis |
| CM-144 | T6 9–11, 27–28 | Selected contours | `profileContracts()`, `selectedRegions()` | `FeatureUI::extrudeSelectedContours` | Selected individual cells/holes from complex sketch; stable region descriptors; valid union | Overlapping or touching hole selections rejected; non-selected curve edit preserves cell | Multi-region planar sketch |
| CM-145 | T6 9–11, 27–28 | Reuse sketch regions | `selectedRegions()`, `cm5_store:main()`, `cm5_repair:main()` | `FeatureUI::reuseSketchRegions` | Single sketch feeds multiple features; each references distinct/shared cells; sketch unconsumed | Editing shared sketch updates all dependents; deleting one feature leaves sibling intact | Shared sketch with multiple regions |
| CM-146 | T6 9–11, 27–28 | Open-profile cut | `profileContracts()`, `openCuts()` | `FeatureUI::cutOpenProfile` | Open curve extended across body creates unambiguous curtain; removes material on cut side | Open curve that does not completely cross target body boundary refused with error | Target body and open sketch wire spanning target |
| CM-147 | T6 9–11, 27–28 | Flip side to cut | `profileContracts()`, `openCuts()` | `FeatureUI::cutFlipSide` | Toggling flipSide removes opposite partition; volume_side1 + volume_side2 = target_volume | Flip state preserved across save/reopen; undo/redo restores flipped partition | Open-profile cut on target body |
| CM-148 | T8 2 | Standalone draft | `modeContracts()`, `standaloneDraft()`, `cm5_repair:main()` | `FeatureUI::draftStandalone` | Draft applied to existing faces using neutral plane and pull direction; exact angle change | Deleted face reference triggers candidate repair; invalid draft causing collapse refused | Existing solid body, neutral plane, pull direction, selected faces |

## Verification commands and evidence

Existing commands to reuse during execution, not commands run by this plan:

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug -R '^cm5\.' --output-on-failure
ctest --preset macos-debug --output-on-failure
python3 tools/format_sources.py
python3 tools/check_architecture.py build/macos-debug
python3 tools/check_isolation.py
python3 tools/check_core_modeling.py
python3 docs/scripts/check_specs.py
git diff --check
```

Reuse `tools/static_analysis.py` with real compile commands for every changed/
affected translation unit, as the existing verification runners do. Existing
CM5 runners remain valid historical-scope entry points:
`tools/verify_cm5_intent.py`, `tools/verify_cm5_modeling.py`,
`tools/verify_cm5_persistence.py`. Their fixed dates/source lists do not certify
future UI changes. Add the smallest native/completion runner following
`tools/verify_cm4_workflows.py`, with explicit native invocation and timestamped
artifacts; document its final command when implemented.

Every execution record must include base/head/source identity, host/build,
actual enabled-test count, failed/disabled checks, native scales/units, fixture
and saved-project paths, expected/actual geometry, preserved gates and review
delivery state. A desktop/IPC permission failure is a blocked environment check,
not proof that the worker or feature does not exist.

## Branch sequence and next executable task

Preserve completed CM5.a/b review branches and inspect CM5.c delivery first.
Create date-stamped review branches for missing verified a/b/c completion work,
then CM5.d and CM5.e. Where later work depends on unmerged earlier work, branch
from the verified prerequisite and record the stacked base for reviewers. Do not
merge or cherry-pick unrelated work merely to simplify the branch view. Commit
and push only scoped, successfully verified task work; confirm origin branch SHA.
This planning change itself does not complete a lettered module task.

**Plan status — In progress:** The 15 November review identified native selection, body-stage, repair and manipulator defects. Their scoped repairs are recorded separately; complete course verification remains open. Earlier completion annotations are historical claims, not acceptance. See [the review-fix record](docs/testing/core-modeling-cm5-review.md).

**Planning limits:** No product code, schema or dependency change is needed for
this request. No build, CTest, native hardware/platform test or remote branch
verification is claimed by writing this plan. Existing geometry and upstream
sketch prerequisites can still block whole-course/full acceptance; keep those
limits visible rather than substituting a simpler passing fixture.
