# CM8 completion plan — Shell, sweep and loft

Prepared 21 November 2026. **Planning only: no CM8 implementation or runtime
verification is claimed.** Scope owner: [CORE_MODELING](../CORE_MODELING.md).
The [coverage register](../research/core-modeling-coverage.json) owns the 27
requirements CM-201–CM-227 and their existing analytical/native identities.

## Outcome and scope

Users can create, edit and rebuild shells, sweeps and lofts, including every
tracked advanced mode, through native tools. Original definitions, section order,
guide choices, connector correspondence and topology references survive save,
reopen, undo, portable copy and upstream edits. Invalid or canceled work preserves
the accepted model. Surface outputs remain distinct from solids.

Complete CM8.a → b → c → d → e in order, verifying each before dependent work.
Record blocked prerequisites and continue independent work. A blocked required
variant leaves CM8 incomplete; disabling its control is safe interim behavior,
not completion. Stop before CM9 and Module 11. Reuse existing mirror/pattern
features for course workflows; do not expand assembly/drawing scope or unrelated
sketch capabilities. Preserve deferred gates 6.1b/6.4c/7.4c and CM6 blockers.

Keep native C++/Qt/OCCT, pinned dependencies, offline operation, SI semantics,
existing admission limits and the physical 8 GB baseline. The desktop owns drafts
and display meshes; the sole worker owns exact geometry, rebuilds and writes.
Do not delegate automatically. During implementation, commit and push each
successfully verified lettered task on its separate review branch using configured
origin; verify the remote branch. No merge or force-push. This planning deliverable
does not complete a lettered task or trigger implementation/commit/push.

## Starting evidence and risks

Planning base: `e3ec6e27ae4f1c1f0103a4fe178813d973d0b535`, initially clean
`review/cm7-e-20261119`; planning branch: `review/cm8-plan-20261121`.
[CM7 evidence](../testing/core-modeling-cm7-review.md) records spatial/derived
curves, support frames, persistence and available-Mac native verification.
The milestone summary records 107 enabled CTests passed. This plan did not rerun
them, and CM7 path evidence does not establish finished CM8 solids.
All 27 CM8 coverage rows currently remain **Not run**.

The retained [CM0 feasibility result](../testing/artifacts/core-modeling/20261022T174052.110799Z/verification/feasibility.json)
contains a planar asymmetric-wall shell seed, a straight two-circle loft seed,
and a curved centerline/auxiliary-guide seed. It explicitly does not establish
curved multi-thickness shells, arbitrary guides, connector correspondence or
production acceptance. These are the first technical risks to investigate.

The retained T8 text was inspected for shell rules, sweep profile placement,
thin sweeps, start/end tangency, centerline lofts, connectors and preview sections.
Course diagrams and exact dimensions still need visual verification before
fixture freeze. Some modes are named without exhaustive semantics; freeze their
supported option/combination contract explicitly, without inventing course claims.
CM-223 additionally requires all thickness modes from the approved roadmap.

## Files and callers to extend

Reuse current seams; choose any new adapter filenames during CM8.a. Do not create
a second modeling engine, generic feature framework or parallel persistence path.

| Area | Existing files/seams | Required reach |
|---|---|---|
| Semantic model | `src/features/ModelingIntent.*`, `ModelingRecords.*`, `Model.*`; `src/domain/GeometryReference.*`; `src/evaluation/Graph.*` | Versioned operations, typed operands, dependency ordering, validation and repair paths |
| Exact model | `src/geometry/occt/ModelingFeatures.*`, `ModelingProfiles.*`, `Supports.*`, `SupportReferences.*`, `SpatialCurve.*`, `Part.*` | Shell/sweep/loft adapters, CM7 path reuse, distinct surface results, topology provenance |
| Model editing/worker | `src/model/Supports.*`, `SupportEditing.*`; `apps/cad-worker/Projects.hpp`, `main.cpp`; `src/protocol/` | Existing `PreviewModelSupports`/`SetModelSupports`, bounded preview/inspection, atomic edits and diagnostics |
| Storage | `src/persistence/ProjectStore.*`, semantic records and reader manifests | Original definitions, correspondence, compatible replay, history, copy/export closure and reference repair |
| Native tools | `apps/desktop/FeaturePanel.*`, `FeatureUI.cpp`, `SupportEditor.*`, `MainWindow.*`; `src/render/Viewport.*` | Face/path/section/guide picking, ordered lists, connector handles, previews, units and command state |
| Verification/docs | `tests/core_modeling/`, `CMakeLists.txt`, `tools/verify_cm7_review.py`, `tools/check_core_modeling.py`, `cmake/README.md` | CM8 tests/runner following current conventions, fixtures, evidence and updated contracts/traceability |

Audit callers of every changed collection/result type: legacy feature/sketch
editors, suppress/reorder/delete, repair, display/tree selection, measurement,
exchange and portable copy. `kernel::Part` currently stores shapes in `bodies`;
do not assume surface support is solved by adding a UI enum. Audit solid-only
validation, Boolean consumers, meshing, topology and volume tools together.

## Ordered execution checklist

### Preparation — source freeze and feasibility

- [ ] Read Step Zero prerequisites in lettered order; record existing blockers.
  Freeze current source/base hashes and capture a fresh configure/build/full-test
  baseline using [build guidance](../../cmake/README.md). Preserve historical
  failures and disabled-test counts; do not infer fresh success from CM7 reports.
- [ ] Expand all 27 rows below into executable case mappings. Verify T8 source
  identity against the coverage register; inspect full tutorial diagrams for the
  shell, pipe/projected path, chair, impeller and three-section loft. Use retained
  sources; record unavailable originals/diagrams as fixture blockers, never guess
  dimensions. Reuse CM7 path fixtures and CM5 thickness semantics where applicable.
- [ ] Freeze the option matrix for tangency, corner transitions, path orientation,
  guide contact, centerline section placement, thin modes and connector behavior.
  Preserve source-vs-roadmap attribution, including guide/profile intersection
  and the fact that a sweep profile need not intersect its path.
- [ ] Run narrowly scoped pinned-OCCT experiments under `tests/core_modeling/`
  for curved/per-face multi-thickness, guided/cornered sweeps, centerline lofts and
  connectors with dissimilar sections. Record exact invariants and failure modes.
  Ordinary through-section loft success alone does not prove guide or connector
  control. Decide the smallest exact adapter for each mode from observed results.

### CM8.a — Contracts, identities and output types

- [ ] Define versioned Shell, Sweep and Loft definitions in the existing modeling
  intent path. Shell: producing body/stage, removable faces, inward/outward default
  thickness and per-face overrides. Sweep: sketch/circular profile, ordered and
  oriented sketch/curve/edge path, guides, start/end conditions, corner policy and
  thin settings. Loft: at least two ordered sections, solid/surface intent,
  optional centerline, thin settings and explicit connector correspondence.
- [ ] Specify section orientation/seams, stable connector IDs and parameter-based
  handles on straight and curved sections. Define synchronization, insertion,
  removal and reorder behavior without using display index as identity. Define
  closed-path seam transport and frame behavior at corners/singular tangents.
- [ ] Validate finite SI quantities, positive dimensions, IDs, cardinalities,
  duplicate/conflicting operands, source roles, producing-stage ownership,
  unsupported combinations, ordering and cycles at trust boundaries. Closed
  solid sections and open surface sections have distinct validation. Clarify thin
  material side and reversal, including independent two-sided values.
- [ ] Define result typing across worker, storage, UI and consumers. Surfaces
  cannot silently satisfy solid-only measurements or operations. Decide supported
  create/merge behavior using existing modeling conventions and explicit errors.
- [ ] Add malformed-input, round-trip, dependency and compatibility contract
  tests; record decisions in `docs/decisions/core-modeling-cm8-contracts.md`.
  **Gate:** contracts are bounded, deterministic and preserve legacy semantics;
  no claim of exact geometry, persisted CM8 features or native tools yet.

### CM8.b — Exact worker adapters

- [ ] Shell: implement uniform removed-face and closed-cavity shells first, then
  inward/outward and multiple thicknesses on planar and curved fixtures. Check
  wall measurements, valid boundaries, cavity orientation and self-intersections.
  Include fillet/chamfer interactions and excessive thickness; do not silently
  replace per-face thickness with a uniform value or publish invalid geometry.
- [ ] Sweep: use existing exact planar/spatial/derived paths and OCCT pipe-shell
  facilities where proven. Cover sketch and circular profiles, open/closed/3D
  paths, reversal and closed seams. Respect endpoint-plane placement without
  requiring the profile itself to intersect the path. Preserve intentional corners.
- [ ] Add one-sided/midplane/two-sided thin sweeps and reversal, guide contact
  and branch checks, start/end tangency, explicit corner transitions and bounded
  preview sections. Verify section/frame behavior and requested geometric
  constraints; a valid shape that ignores an option is still a failure.
- [ ] Loft: implement ordinary through-sections, then curved centerline control,
  exact connector synchronization and thin modes. Cover ellipse/dissimilar and
  nonparallel sections, three or more sections, open surfaces and closed solids.
  Connector edits must change the exact result, not merely move overlay graphics.
  Prove all requested boundaries and correspondence survive parameterization.
- [ ] Integrate explicit solid/surface publication and topology history. Validate
  geometric tolerances and topology using independent measurements. Reuse existing
  exceptions/diagnostics, revision checks and cancellation boundaries. Keep exact
  previews to one running computation plus one replaceable pending draft; reject
  stale responses and retain admission/cache limits.
- [ ] **Gate:** production adapter tests and real-worker preview tests pass for
  every required mode and negative case. Experiments alone cannot close CM8.b.
  No sampled-mesh geometry fallback, silent smoothing or ignored advanced options.

### CM8.c — Persistence, rebuild and repair

- [ ] Extend semantic records and compatibility manifests, storing original
  definitions, result type, thickness overrides, path/section order, guides,
  connector IDs/parameters and provenance. Rebuild exact shapes on replay.
  Audit SQL placeholders/bindings and all old/new schema paths.
- [ ] Keep previews and accepted edits consistent on omitted versus explicitly
  empty collections. Preserve unrelated CM4–CM7 data. Keep sticky reader
  requirements through undo/history; read old projects, upgrade on copies and
  make older readers refuse unsupported semantics safely.
- [ ] Track generated/modified/deleted faces, section seams and downstream
  dependencies. Unique proven evolution may carry references forward; ambiguous,
  split, merged or missing references require explicit repair. Repair must bind
  to the complete pending model, matching revision/fingerprint and current typed
  candidates, with atomic descendant rebuild.
- [ ] Test reopen/replay, portable copy, undo/redo, duplicate/stale commands,
  worker restart, interrupted commit, cancellation and invalid geometry. Compare
  accepted revision, definitions, geometry and history before/after failures.
  **Gate:** these pass in marked disposable roots, including upstream edits and
  unrelated legacy edits; never use user projects or release recovery paths.

### CM8.d — Native authoring and editing

- [ ] Add native Shell selection for removed/multi-thickness faces and direction;
  Sweep selection for profile/path/guides, thin/tangency/corner controls and section
  previews; Loft ordered sections, centerline, output/thin controls and connectors.
  Reuse existing property panels and typed selection/preview plumbing.
- [ ] Support adding and dragging connector handles on straight/curved profiles,
  initial single connector for circular/elliptical/closed-spline end sections,
  synchronization and section reorder. Highlight exact referenced geometry and
  show useful invalid/ambiguous operand diagnostics. Expose every persisted value
  through edit/reopen; no geometry option may be a cosmetic-only control.
- [ ] Keep preview, accepted and failed states distinguishable. Cancel and failed
  acceptance restore accepted state; late results cannot overwrite a newer draft.
  Provide unit-aware inputs, keyboard operation, focus and accessible labels;
  surfaces display/select correctly and refuse solid-only operations clearly.
- [ ] **Gate:** worker-backed create/edit/accept/cancel/repair/undo/reopen cases
  pass in mm/in at observed native 100/125/150/175/200% effective scales. Check
  pointer and keyboard workflows; headless/synthetic tests do not certify physical
  devices or assistive technology. Preserve earlier sketch/feature gestures.

### CM8.e — Complete workflows and closure

- [ ] Reconstruct the T8 shell tutorial, ordinary pipe, projected-path sweep,
  thin chair frame plus existing mirror, impeller centerline loft plus existing
  circular pattern, and multisection loft. Use verified course dimensions, add
  independent analytical fixtures, and verify upstream edits and save/reopen.
- [ ] Exercise invalid thickness, self-intersection, gaps, singular frames,
  guide misses/ambiguity, crossed connectors, section deletion/reorder, invalid
  mixed section types, stale previews and failed repairs. Confirm accepted state
  retention and usable diagnostics, not only absence of a crash.
- [ ] Run focused CM8 tests, then the full enabled regression suite, pinned format,
  changed-source Clang analysis, architecture/research-isolation/coverage/spec
  checks and native workflows. Measure bounded previews/cache and repeated edits
  and undo/redo after trimming. Preserve B0–B6; do not equate these measurements
  with full 8 GB or cross-platform acceptance.
- [ ] Create CM8 verification runner/evidence using existing conventions; record
  source hashes, commands, actual test counts, failures, expected/actual geometry,
  native units/scales and artifact paths in `docs/testing/core-modeling-cm8-review.md`.
  Update CORE_MODELING, coverage, SDD/traceability where affected, build guidance,
  README and task checklist together, retaining older evidence.
- [ ] Close each requirement only against its linked analytical/native evidence
  and declared acceptance scope. Report experiments, production implementation,
  available-Mac verification, complete course coverage and full acceptance
  separately. Unrun Windows/Linux, device, installed-release and physical resource
  gates remain Blocked/Not run; complete development scope does not close them.

## Coverage-to-verification matrix

Preserve each row's `cm-NNN-geometry` and `cm-NNN-native` identities. These are
planned identifiers, not proof tests exist. Map them to executable cases before
implementation. Every row also requires edit/reopen and upstream/failure coverage.
Page numbers below are T8 PDF pages, not printed book pagination.

| ID | Variant / pages | Independent acceptance check |
|---|---|---|
| CM-201 | Removed faces / 10–12 | Specified planar/curved openings, retained walls and valid resulting solid |
| CM-202 | Closed cavity / 10–12 | No opening, correctly oriented inner cavity, volume equals outer minus inner on analytical fixture |
| CM-203 | Inward / 10–12 | External envelope retained and internal wall distances match thickness |
| CM-204 | Outward / 10–12 | Original interior boundary retained and external offsets match thickness |
| CM-205 | Multiple thicknesses / 10–12 | Each referenced wall has its requested value; include curved walls and invalid intersections |
| CM-206 | Sketch profile / 14–17, 22–23, 27–28, 48–49 | Exact profile/section agreement, including profile offset from path on endpoint plane |
| CM-207 | Circular profile / same sweep pages | Independent diameter/area and analytical straight-pipe volume |
| CM-208 | Open path / same sweep pages | Endpoint locations, caps and section orientation; gaps rejected |
| CM-209 | Closed path / same sweep pages | Closed seam without unintended twist; analytical circular-path fixture |
| CM-210 | 3D path / same sweep pages | CM7 path/frame reuse, spatial section locations and upstream projected-path rebuild |
| CM-211 | One-sided thin / same sweep pages | Material side and wall distance independently measured |
| CM-212 | Midplane thin / same sweep pages | Equal half-thickness on both sides of the reference section |
| CM-213 | Two-sided thin / same sweep pages | Unequal independent side distances retained through edits |
| CM-214 | Reverse thickness / same sweep pages | Material side reverses without changing original profile/path identity |
| CM-215 | Guide curves / 48–49, 52 | Required profile-guide intersection/contact and branch; guide edits change exact shape |
| CM-216 | Start/end tangency / sweep pages | Endpoint derivative/normal residuals meet frozen condition; incompatible constraints fail |
| CM-217 | Corner transition / sweep pages | Requested transition verified at corners; no hidden path smoothing |
| CM-218 | Preview sections / sweep pages | Displayed sections agree with worker sections and stale responses cannot replace current ones |
| CM-219 | Ordered sections / 32–37, 43–48 | Boundary agreement in explicit order, including three or more and nonparallel sections |
| CM-220 | Ellipse sections / same loft pages | Exact ellipse axes/boundaries and orientation through rebuild |
| CM-221 | Open sections/surface / same loft pages | Surface boundaries/area valid, result remains nonsolid, volume request refused |
| CM-222 | Closed sections/solid / same loft pages | Watertight orientation and independent cylinder/frustum volume fixtures |
| CM-223 | Thin loft / 47, 49 | One-sided, symmetric, independent two-sided, reversal and open/closed compatibility |
| CM-224 | Centerline path / loft pages | Curved-centerline section locations/frames and path mutation; straight seed insufficient |
| CM-225 | Correspondence connectors / 35–37 | Stable associations on dissimilar sections, seam changes and insertion/removal |
| CM-226 | Connector handles / 35–37 | Native drag changes exact geometry and survives undo/reopen without display-index identity |
| CM-227 | Loft synchronization / 35–37 | Intended mapping controls twist across sections; invalid crossing/order is diagnosed |

## Verification commands and first execution task

Existing baseline commands (run during execution, not claimed by this plan):

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug --output-on-failure
python3 tools/check_core_modeling.py
python3 docs/scripts/check_specs.py
git diff --check
```

Register CM8 CTests and the native runner before publishing their commands as
available. Follow existing runner behavior for marked roots, bounded jobs, pinned
format/analyzer and source snapshots; do not run concurrent builds in one directory.

**First execution sub-prompt:** Perform preparation, freeze the 27-row case/option
matrix and complete CM8.a contracts with focused and regression evidence. Record
exact-adapter feasibility blockers explicitly. Do not implement CM8.b until its
contract prerequisites pass; do not mark CM8 complete for a partial variant set.
