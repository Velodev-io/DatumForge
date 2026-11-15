# Plan: complete CM6.a–e

Prepared 15 November 2026. This is a planning deliverable. Its checkboxes are
future execution work, not implementation or verification claims. No code was
built and no test was run to write it.

## Objective and boundary

Complete [CM6 — Holes, ribs, repetition and treatments](../../CORE_MODELING.md#cm6--holes-ribs-repetition-and-treatments)
for all 41 coverage rows CM-149–CM-189 in the
[coverage register](../../research/core-modeling-coverage.json), sourced from
T7 *Modelling Tools* pages 9–48. Extend the existing Module 4 exact adapters and
the CM5 intent/persistence/preview stack. Do not build a parallel feature runtime.

Execute one lettered task at a time: a contracts → b engine → c persistence →
d native UI → e verification. Within b–d, work family by family in the order
given below. Finish each task's implementation, focused verification and evidence
before starting the task that depends on it. Record a blocked prerequisite and
continue with independent work. Do not start CM7–CM9, Module 11, or any assembly
or drawing expansion. That includes cosmetic-thread display in drawing views and
the T7 section view (CM-247–249, CM9).

Keep native C++/Qt/OCCT 7.9.2, offline operation, SI semantic values, the physical
8 GB baseline, existing admission caps and deferred gates 6.1b/6.4c/7.4c. The
desktop owns semantic drafts and display meshes; the sole worker owns exact
geometry, rebuilding and writes. No kernel objects in the UI. No visible control
may offer an operation that is not implemented. Do not delegate automatically.

## Verified starting information

These records were inspected during planning; no tests were rerun.

| Item | Observed state | Consequence for CM6 |
|---|---|---|
| Base | `main` = `1d42a10` “Merge reviewed CM5 implementation and prerequisite stack”; `review/cm5-integration-review-20261115` has no commits beyond it | Branch CM6 from `main` |
| CM5 status prose | `CORE_MODELING.md` still shows CM5.d/e unchecked on `main` and on the integration branch, even though `task.md` says the CM5 plan is complete | Reconcile in CM5's own follow-up, not inside CM6. CM6 relies on the merged code, not on the checkbox |
| Working tree | `review/cm5-d-20261114` has uncommitted edits to `src/render/Viewport.cpp/.hpp` and `tests/core_modeling/workflow_contracts_cm5.cpp`. They duplicate existing lines: `auto tip` is declared twice, an `if` block has no closing brace and a test data column is added twice. They will not compile as-is | **The user decides** whether to discard or repair them before CM6 branching. Do not branch CM6 from this dirty tree |
| Module 4 adapters | [module-4-features](../../decisions/module-4-features.md): constant-radius fillet and equal-distance chamfer on 1–32 explicit edges; simple cylindrical hole on a planar face (Blind or Through All); one-direction linear/circular pattern of a feature or body, count 1–32 with raw unit vectors; mirror with raw origin/normal | CM6 extends these. Directions and planes must become typed CM4 references so upstream changes propagate |
| CM5 stack | Versioned `features::ModelingIntent` (Extrude/Revolve/Draft), `ModelingExtents.cpp` end conditions, CM5.c durable intents with sticky manifest and candidate repair, CM5.d `PreviewModelSupports` display-mesh preview and manipulators | Reuse for hole terminations, persistence, repair and live previews |
| CM4 stack | Typed `GeometryReference`, face-attached sketches, Convert Entities, typed datums | Use for hole placement sketches and for pattern/mirror references |
| CM0 feasibility | `full_round_seed` **Blocked** (OCCT edge-seeded builder failed); general face-set blend not established | CM-183 and CM-184 start Blocked |
| CM9 catalogs | [Catalog register](../../research/core-modeling-catalog-sources.md): candidates only; no dataset imported; redistribution rights unresolved | Catalog-driven Hole Wizard and thread sizes cannot fully close until CM9.a |
| Coverage | 39 rows Not run, 2 Blocked (CM-183, CM-184) | All 41 rows need linked analytical and native evidence |

## Completion means four separate things

1. **Production implementation:** each variant has contracts, exact worker
   evaluation, durable storage and an accessible native tool.
2. **Available-Mac development verification:** current-source tests, analyses,
   exact geometry checks and scoped native workflows pass, with retained logs.
3. **Course-workflow verification:** each CM6 row has passing analytical and
   native evidence, and the actual T7 models rebuild and survive edits and reopen.
   A simple cut or a plain edge fillet does not close Hole Wizard, face fillet or
   full-round coverage.
4. **Full acceptance:** other platforms, devices, assistive technology, installed
   builds and physical 8 GB gates. These stay Blocked or Not run.

## Risk register: decide early, timebox, never substitute

| Risk | Rows | Plan |
|---|---|---|
| **R1. Face and full-round fillet:** OCCT 7.9.2 has only an edge-seeded public fillet API; the CM0 full-round probe failed | CM-183, CM-184; T7 Tutorial 3 pp. 40–42 | Experiment ladder in b.6. If no exact construction passes, the rows stay Blocked and Tutorial 3 replay is recorded as partial at the full-round step. No reduced radius, mesh blend or approximation |
| **R2. Catalog data:** Hole Wizard standards, fastener sizes and thread designations need CM9 data with verified redistribution rights | CM-164, and the standard-sized forms of CM-157–161 and CM-169 | Build the catalog *interface* and resolved-value storage now. Custom sizing works fully. Until CM9.a admits a dataset, standard/type/size pickers are not presented as functioning. Only test-local fixture catalogs that are never shipped |
| **R3. Disabling tangent propagation:** OCCT builds contours along tangent-continuous edges automatically | CM-189 | Prove which states OCCT can represent. If propagation off cannot be honored, refuse it with an explicit diagnostic. Never propagate silently when the user cleared it |
| **R4. Face pattern and face mirror** need the selected faces to form a closed tool body (T7 p. 10) | CM-173, CM-178 | Sew the faces into a solid tool and refuse non-closed sets. Experiment first; the row stays Blocked if sewing is not exact |
| **R5. Rib extension:** material must extend to the body for both the Parallel and Normal directions | CM-149–155 | Compare `BRepFeat_MakeLinearForm` against a thickened chain bounded by the CM5 Up To Next extent, then choose one adapter |
| **R6. Two-direction instance count:** a 32 × 32 grid is 1,024 tools | CM-175 | Keep the current per-direction cap and add a total-instance cap. Any increase needs measured memory and time evidence |

## 0. Baseline, scope and fixtures

- [ ] **0.1 — Resolve the dirty tree and freeze the base.** Get the user's decision
  on the uncommitted CM5.d edits. Create `review/cm6-0-<date>` from `main`
  (`1d42a10`). Record source identity, host and toolchain.
- [ ] **0.2 — Run the current-source baseline.** Use the pinned configure/build, all
  enabled CTests, formatting, architecture, isolation, core-modeling and spec
  checks. Store artifacts under `docs/testing/artifacts/core-modeling/cm6-0-<date>/`.
  Record failures as baseline facts; do not fix unrelated code.
- [ ] **0.3 — Freeze course fixtures and oracles from the retained PDFs.** The
  dimensions live in the figures (7-1, 7-2, 7-25, 7-26, 7-28, 7-37 and 7-44 to
  7-53), not in the extracted text. Read those pages from the retained course
  cache or PDFs and record each value with its page. A value the figure does not
  show stays “unresolved”; do not invent one. Text-confirmed values so far:
  - **Tutorial 1 (pp. 2–24):** a 13 mm rib, Both Sides, Parallel to Sketch, on
    the Front Plane; simple holes Ø10 and Ø14 Through All, placed then dimensioned;
    a two-direction linear pattern (spacing 43, 5 instances in direction 1);
    an ANSI Metric counterbore for a socket button-head cap screw (B18.3.4M, M8)
    with custom sizes 8.5 / 14 / 4.2 mm, Through All, patterned; and mirror of a
    cut feature.
  - **Tutorial 2 (pp. 25–34):** a revolved base (CM5); an Ø8 cut Through All in
    both directions; an M42x3.0 ANSI Metric machine-thread cosmetic thread on an
    inner circular edge, Up To Next, displayed shaded; and an R5 constant
    fillet. The section view is CM9 and out of scope.
  - **Tutorial 3 (pp. 35–49):** contour extrudes (CM5); full-round fillets on
    both sides; a legacy Simple hole, Ø10, Through All, placed on an arc center
    by coincident snap; mirror of that hole about the Front Plane; and
    angle-distance chamfers of 1.5 mm at 45° with tangent propagation.
  - **Exercises 1–2 (pp. 51–52):** independent fixtures if their figures are
    fully dimensioned.
- [ ] **0.4 — Reconcile course options that are not in the register.** The course
  mentions Instances to Skip (p. 19), the Simple Hole tool as distinct from Hole
  Wizard (p. 14), vertex chamfer (p. 2 objectives), face fillet with hold line
  (p. 50), pattern callouts (p. 18) and the closed-face requirement for face
  mirror (p. 10). Propose register additions to the user; do not add rows
  silently. Map every row to named test functions; an identifier alone does not
  count as a test.

## CM6.a — semantic contracts

Extend `features::ModelingIntent` with new kinds instead of widening the legacy
`Feature::parse`. Keep the same rules: exact keys, no silent defaults, typed CM4
references, bounded size, graph projection and history/order validation.
Decisions go in `docs/decisions/core-modeling-cm6-contracts.md`.

- [ ] **a.1 — Common rules and coexistence with legacy features.** Module 4
  Fillet, Chamfer, Hole, LinearPattern, CircularPattern and Mirror features keep
  loading, rebuilding and editing unchanged. New commands author CM6 intents;
  legacy nodes open their existing editor. Upgrades happen only on copies.
  Define stable sub-identities: a pattern instance is `seed UUID + (i, j)`; a hole
  instance uses the placement point's UUID.
- [ ] **a.2 — Hole.** Each hole carries:
  - **Family:** Simple, Counterbore, Countersink, Drilled, Tapped, PipeTap,
    Legacy{Simple, SimpleDrilled, CounterboreDrill, CountersinkDrill, Tapered}.
  - **Sizing:** Custom, or Catalog{dataset id, dataset version, standard, type,
    size, fit}. The resolved section values are always stored: diameters, depths,
    countersink angle, drill-point angle, taper and tap-drill diameter.
  - **Placement:** a face reference plus a placement-sketch UUID and its point
    entity UUIDs.
  - **End condition:** the CM5 extent set (Blind, Through All, Up To Next, Up To
    Vertex, Up To Surface, Offset From Surface), plus whether depth is measured to
    the shoulder or to the drill tip.
  - **Tapped holes:** thread metadata whose cosmetic flag is mandatory.
- [ ] **a.3 — Cosmetic thread.** It references a circular edge or cylindrical face,
  or is owned by a hole. It stores designation, standard/type/size metadata,
  internal or external, major/minor/pitch values and an end condition (Blind
  depth, Through, Up To Next). The contract cannot produce solid geometry. The
  shaded display setting is a document display property, separate from geometry.
- [ ] **a.4 — Rib.** It stores a sketch with an open or closed chain (CM5
  region/chain descriptors), thickness, side (Both, First or Second), direction
  (Parallel or Normal to sketch), the material-side flip, and the target body.
- [ ] **a.5 — Pattern and Mirror.**
  - **Pattern:** Linear{dir1, optional dir2, each with a typed direction reference
    (edge, axis or plane normal), reverse, spacing and count} or Circular{axis
    reference, count, equal spacing or total angle, reverse}.
  - **Seeds:** features, faces or bodies.
  - **Mirror:** a plane reference (datum plane or planar face) and the same seed
    kinds.
  - **Feature-seed evaluation:** decide whether the seed tool is transformed or
    re-evaluated per instance. This matters for Up To Next holes, so it is a
    recorded design choice tested against the course.
- [ ] **a.6 — Fillet and Chamfer.**
  - **Fillet:** Constant{r, edges}, MultipleRadius{edge→r}, Variable{edge, control
    points (parameter, r), transition}, Face{set1, set2, r}, FullRound{set1,
    center, set2}, plus a propagation flag.
  - **Chamfer:** Equal{d}, DistanceDistance{d1, d2}, AngleDistance{d, angle}, with
    flip and a reference face fixing which side is which.
  - **Caps:** existing caps apply (1–32 edges).
- [ ] **a.7 — Contract tests.** Add `tests/core_modeling/cm6_contracts.cpp`
  (`cm6.contracts`): round-trip, rejection of unknown, inactive or nonfinite
  fields, reference kinds, graph cycles and order, and admission caps.

## CM6.b — exact engine through the read-only worker preview

Put new adapters beside `ModelingFeatures.cpp`, for example
`geometry/occt/Holes.cpp`, `Ribs.cpp`, `Repetition.cpp` and `Treatments.cpp`, using
the shared `validateShape`, `recordShapeHistory`, topology tokens and the CM5
evaluation entry point. Every slice has an analytical oracle independent of the
kernel. Write the test first, then the implementation.

- [ ] **b.1 — Treatments (lowest risk, used most in the course).** Cover Constant,
  MultipleRadius and Variable fillets (`BRepFilletAPI_MakeFillet`: per-edge radius
  and radius laws or points), and Equal, DistanceDistance and AngleDistance
  chamfers (`AddDD`/`AddDA` with a reference face) including flip. Settle R3.
  Oracles: removed volume (1 − π/4)·r²·L for a 90° convex edge; d²/2·L,
  d₁d₂/2·L and d·(d·tanθ)/2·L for chamfers; ruled-surface checks for variable
  radius.
- [ ] **b.2 — Holes.** Revolve an exact 2D half-section into a tool for each
  family:
  - drill point 118°, or the stored angle;
  - countersink cone, and counterbore step;
  - tapered hole as a cone;
  - pipe tap with an explicit taper;
  - tapped hole with a tap-drill core only, so threads add no solid.
  Place one tool per placement point, take the end condition from
  `ModelingExtents`, then Boolean-cut. Oracles: closed-form frustum, cylinder and
  cone volumes for each family; the counterbore oracle uses the course's
  8.5 / 14 / 4.2 mm.
- [ ] **b.3 — Cosmetic threads.** Worker-side resolution of the referenced edge
  or face and its end condition. Output: display descriptors only (radii, axial
  extent, frame). Test that the solid volume and topology are unchanged.
- [ ] **b.4 — Patterns and mirror.** Two-direction grid; circular equal or total
  angle; typed direction, axis and plane resolution (so moving an edge or datum
  re-orients the pattern). Implement feature, body and face seeds, settling R4 for
  face seeds, and the total-instance cap (R6). Oracles: instance centroids, count
  × seed volume when there is no overlap, and a reflected centroid and equal
  volume for mirror.
- [ ] **b.5 — Ribs.** Settle R5, then cover Both, First and Second sides, Parallel
  and Normal directions, flip, and open or closed chains. Oracles: rib volume
  equals thickness × the swept area bounded by the body. Refuse a rib that never
  meets the body.
- [ ] **b.6 — Face and full-round fillet experiment ladder (timeboxed, R1).**
  Keep the experiments under `tests/core_modeling/feasibility*`:
  - **E1:** check whether any public OCCT 7.9.2 face-face blend path exists.
  - **E2:** exact restricted full round between parallel planar side faces with a
    planar or cylindrical center face. Replace the center face with a
    half-cylinder or toroidal segment swept along the center-face spine, built by
    exact Boolean operations.
  - **E3:** a rolling-ball surface (GeomFill) sewn into the solid.
  Promote to production only for the cases that pass exact analytical checks,
  and document the restriction. Otherwise CM-183/184 stay Blocked with evidence.
- [ ] **b.7 — Failure and lifecycle.** Cover missing or ambiguous edges, faces
  and references; kernel failures; and stale, canceled or duplicate previews.
  In every case the accepted state must stay unchanged. Add
  `cm6.engine.*` and `cm6.worker` tests.

## CM6.c — persistence, provenance and repair

- [ ] **c.1 — Extend the CM5.c intent collection.** Bump the sticky reader-manifest
  marker. Older readers refuse files that contain CM6 intents; older projects open
  unchanged; any upgrade happens on a copy.
- [ ] **c.2 — Store resolved values with catalog identity.** Reopening must never
  look a value up again. A catalog change can offer an explicit update but never
  rewrites accepted meaning. Store the thread metadata, and keep the cosmetic
  display setting as document display state.
- [ ] **c.3 — Provenance and repair.** Pattern and mirror instance histories and
  hole-instance identities must be stable across reopen. Missing or ambiguous
  edge, face and reference targets go through CM5.c candidate repair. There is no
  fallback by display index.
- [ ] **c.4 — Durability tests.** Round-trip, undo/redo, failed or interrupted
  commit, worker restart, and portable copy. Add `cm6.store` and `cm6.repair`.

## CM6.d — native authoring, selection filters and previews

Reuse the FeaturePanel/PropertyManager, the CM5.d preview and manipulator path,
the command registry, the tree, SupportEditor and Smart Dimension (CM3). Hole
Wizard's Type/Positions tabs justify splitting out `HolePanel` and
`TreatmentPanel` helpers; do not add a plugin framework.

- [ ] **d.1 — Selection filters.** Vertex, Edge, Face, Body and Feature filters in
  the viewport and tree. The worker validates topology picks. Add selection lists
  per field (face sets 1 / center / 2, Directions 1 and 2, seeds).
- [ ] **d.2 — Treatment panels and previews.** A fillet type radio group;
  per-edge radius callouts for Multiple radius; vertex or point callouts for
  Variable radius; a Chamfer mode with the flip arrow (also clickable in the
  viewport); Tangent propagation on by default (T7 p. 48). Previews are live
  exact meshes. Controls for Face and Full-round appear only if b.6 promoted them.
- [ ] **d.3 — Hole Wizard and Simple Hole.**
  - **Type tab:** family buttons, custom sizing fields and end condition. Catalog
    pickers appear only when a dataset is admitted (R2).
  - **Positions tab:** pick a face, which opens a face-attached placement sketch
    with a point tool, coincident snap to arc centers (T7 p. 44) and Smart
    Dimension. Editing Hole1's sketch afterwards rebuilds the hole (T7 pp. 15–16).
  - **Tree:** the placement sketch is nested under the hole feature.
- [ ] **d.4 — Cosmetic thread.** A thread panel; a thread node nested under its
  feature (T7 p. 31); a “Shaded cosmetic threads” document display option; and a
  viewport overlay that is visibly not geometry.
- [ ] **d.5 — Rib, Pattern and Mirror panels.** Direction arrows and reverse
  clicks in the viewport; spacing and count callouts; a second-direction rollout;
  plane or face selection for Mirror.
- [ ] **d.6 — Integration.** Escape cancels; Accept is enabled only for the
  current successful draft; one accepted edit is one undo step; double-click
  edits; unrelated commands are disabled while a draft is open.
- [ ] **d.7 — Native verification at five scales.** 100%, 125%, 150%, 175% and
  200%, with mm and inch units, using both keyboard and pointer paths. Add
  `cm6.workflow_contracts`.

## CM6.e — course, upstream-change and final verification

- [ ] **e.1 — Replay T7 Tutorials 1–3 natively from the 0.3 fixtures.** Replay them
  through the tools, save, reopen and compare against the oracles. Tutorial 3's
  full-round step is recorded as Blocked unless b.6 succeeded, and the remaining
  steps are still verified. Section view stays CM9.
- [ ] **e.2 — Upstream and failure mutations.** Change base-sketch dimensions,
  move datums, move hole placement points, change pattern counts and seed sizes,
  and delete referenced edges. Each change must rebuild or route to explicit
  repair, with undo and reopen intact.
- [ ] **e.3 — Final current-source verification.** Full build, all enabled CTests,
  changed-source Clang analyses, format, architecture, isolation, core-modeling
  and spec checks. Add `tools/verify_cm6.py`, following
  `tools/verify_cm4_workflows.py`, with timestamped artifacts.
- [ ] **e.4 — Close coverage honestly.** Update each of the 41 rows with
  evidence. Expected outcome: rows reach available-Mac Pass where they pass, and
  CM-183/184 and the catalog-driven part of CM-164 stay Blocked on R1 and CM9.
  Whole-CM6 full acceptance stays Blocked. Update `CORE_MODELING.md`,
  `docs/testing/core-modeling.md`, `task-status.json` and README status together.

## Coverage matrix: all 41 CM6 rows

Every row also needs save/reopen and either an upstream-change or a
failure/repair scenario.

| ID | Pages (T7) | Variant | Analytical oracle | Native case | Dependency / blocker |
|---|---|---|---|---|---|
| CM-149 | 11–13 | Rib open/closed profile | Volume of the bounded web | Tutorial 1 rib | R5 |
| CM-150 | 11–13 | Rib both sides | t/2 each side of the sketch plane | Tutorial 1, 13 mm | R5 |
| CM-151 | 11–13 | Rib first side | Full t on side 1 | Side toggle | R5 |
| CM-152 | 11–13 | Rib second side | Full t on side 2 | Side toggle | R5 |
| CM-153 | 11–13 | Parallel to sketch | In-plane extension to body | Tutorial 1 | R5 |
| CM-154 | 11–13 | Normal to sketch | Normal extension to body | Direction toggle | R5 |
| CM-155 | 11–13 | Flip material side | Opposite partition | Flip arrow | R5 |
| CM-156 | 14–16 | Simple hole | πd²/4·L | Tutorial 1 Ø10/Ø14, place then dimension | CM4 face sketch |
| CM-157 | 20–23 | Counterbore | Two stacked cylinders, 8.5/14/4.2 mm | Tutorial 1 counterbore | Custom sizing; catalog via CM9 |
| CM-158 | 21 | Countersink | Cone frustum + cylinder | Independent fixture | Same |
| CM-159 | 21 | Drilled | Cylinder + 118° tip cone | Independent fixture | Same |
| CM-160 | 21 | Tapped | Tap-drill core only; thread metadata | Independent fixture | Same; no helix |
| CM-161 | 21 | Pipe tap | Tapered frustum | Independent fixture | Same |
| CM-162 | 43–44 | Legacy custom profile | Simple Ø10, Through All | Tutorial 3 legacy hole | — |
| CM-163 | 21 | Tapered hole | Cone frustum | Independent fixture | — |
| CM-164 | 21 | Standard/type/size | Resolved values match the dataset row | ANSI Metric / B18.3.4M / M8 | **CM9.a dataset** |
| CM-165 | 22 | Custom sizing | Overrides stored and used | Tutorial 1 “Show custom sizing” | — |
| CM-166 | 22–23, 44 | Placement sketch | Point positions from dimensions | Positions tab, arc-center snap | CM3, CM4 |
| CM-167 | 22–23 | Multiple positions | N × single-hole volume | Multi-point placement | — |
| CM-168 | 15, 22 | Termination | CM5 extent oracles | Through All plus each extent | CM5 extents |
| CM-169 | 29 | Thread designation | No volume or topology change | M42x3.0 | Catalog value via CM9; custom works |
| CM-170 | 29 | Thread depth/end | Display extent matches Up To Next | Tutorial 2 | — |
| CM-171 | 30–31 | Shaded cosmetic display | Display-only overlay | Document option | — |
| CM-172 | 17–19, 23–24 | Feature pattern | Instance centroids and count | Tutorial 1 hole patterns | a.5 decision |
| CM-173 | 10 | Face pattern | Closed-tool volume × n | Independent fixture | R4 |
| CM-174 | 10 | Body pattern | n × body volume | Independent fixture | — |
| CM-175 | 17–19 | Two linear directions | Grid centroids | Spacing 43, 5 instances | R6 |
| CM-176 | 17 | Circular distribution | Rotated centroids | Independent fixture | — |
| CM-177 | 17–18 | Reference direction/axis | Moved edge re-orients pattern | Edge pick | CM4 refs |
| CM-178 | 9–10, 45 | Mirror feature/face/body | Reflected centroid, equal volume | Tutorial 1 cut; Tutorial 3 hole | R4 for faces |
| CM-179 | 10, 45 | Face/plane mirror | Reflection about a moved plane | Front Plane / planar face | CM4 refs |
| CM-180 | 32–33 | Constant fillet | (1 − π/4)r²L | Tutorial 2 R5 | — |
| CM-181 | 33 | Multiple radius | Sum over edges | Per-edge callouts | — |
| CM-182 | 32 | Variable radius | Integral of the radius law | Vertex callouts | — |
| CM-183 | 32 | Face fillet | Rolling-ball cross-section | Face sets 1/2 | **R1 Blocked** |
| CM-184 | 40–42 | Full round | Half-cylinder of the face gap | Tutorial 3 | **R1 Blocked** |
| CM-185 | 47 | Equal chamfer | d²/2·L | Distance-distance + Equal | — |
| CM-186 | 47 | Unequal chamfer | d₁d₂/2·L | Distance-distance | — |
| CM-187 | 47 | Angle-distance | d·d·tanθ/2·L | Tutorial 3, 1.5 mm at 45° | — |
| CM-188 | 47 | Flip direction | Swapped legs | Flip arrow | — |
| CM-189 | 48 | Tangent propagation | Chain edge set | Tutorial 3 default on | R3 |

## Verification commands

These are the existing commands to reuse; this plan did not run them.

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug -R '^cm6\.' --output-on-failure
ctest --preset macos-debug --output-on-failure
python3 tools/format_sources.py
python3 tools/check_architecture.py build/macos-debug
python3 tools/check_isolation.py
python3 tools/check_core_modeling.py
python3 docs/scripts/check_specs.py
git diff --check
```

Use `tools/static_analysis.py` with real compile commands for each changed
translation unit. Each record includes base/head/source identity, host/build,
enabled-test count, failures, native scales and units, fixture paths,
expected/actual geometry, preserved gates and delivery state.

## Branch sequence

Use stacked review branches from `main`: `review/cm6-0-<date>`, then
`review/cm6-a-<date>` through `review/cm6-e-<date>`, each recording its stacked
base. Under the 1 November standing authorization, commit and push each
successfully verified lettered task and confirm the remote SHA. No merge and no
force-push. This planning file does not complete any lettered task.

**First executable step:** 0.1, which needs the user's decision on the
uncommitted CM5.d viewport and test edits.
