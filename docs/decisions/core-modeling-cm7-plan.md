# CM7 plan — Spatial sketches and derived curves

Prepared 17 November 2026. Planning only; all execution tasks below are unchecked.
Scope owner: [CORE_MODELING](../CORE_MODELING.md). Requirements and existing test
identities: [coverage register](../research/core-modeling-coverage.json).

## Outcome and boundary

Users can author constrained spatial paths, derive curves from sketches/faces,
compose paths, create editable helix/spiral curves, and reuse stable endpoints
and tangent directions for datum/sketch supports. Edits, repair, undo and reopen
retain the original semantic definitions and rebuild their dependents.

CM7 supplies the projected path and chair-frame path for CM8. Sweeping the chair
tube, shelling, lofting and their finished-solid acceptance remain CM8. Do not
start CM8, CM9 or Module 11 as part of this plan. Preserve CM6 face-blend/catalog
blockers and deferred gates 6.1b/6.4c/7.4c.

Keep native Qt/C++/OCCT/Eigen, planar PlaneGCS, offline execution, SI storage,
existing admission budgets and the physical 8 GB baseline. The desktop owns
semantic drafts and display geometry; the sole worker owns solving, exact
geometry, rebuilds and accepted writes. No automatic delegation.

## Starting evidence and integration points

The planning checkout is clean `main` before these documentation edits. Current
[CM6 evidence](../testing/core-modeling-cm6-review.md) records supported production
and available-Mac verification, with separate acceptance blockers. This plan did
not rerun that verification. All 14 CM7 coverage rows currently say **Not run**.

Reuse these existing seams, extending their callers and tests together:

| Area | Existing seam | CM7 work |
|---|---|---|
| Semantic identities and graph | `src/domain/GeometryReference.*`, `src/evaluation/Graph.*`, `src/model/Supports.*` | Spatial members, branch identities, typed dependencies and validation; `DerivedCurve` is a graph kind, not yet proof of a CM7 engine |
| Sketch solving | `src/sketch/Sketch.*`, `Solver.*`, `Curves.*`; installed `cad_eigen` target | Separate spatial model/solver adapter; retain planar representation and solver behavior |
| Exact evaluation | `src/geometry/occt/SketchCurve.*`, `Supports.*`, `SupportReferences.*` | Exact spatial curves, derived branches and endpoint/tangent support resolution |
| Worker and transport | `apps/cad-worker/Projects.hpp`, `main.cpp`, `src/protocol/` | Bounded read-only preview, atomic accepted edits, diagnostics and display transport |
| Persistence | `src/persistence/ProjectStore.*`, existing semantic records/manifests | Versioned spatial definitions, replay, compatibility, repair and recovery |
| Native tools | `apps/desktop/SketchPanel.*`, `SketchEditingUI.cpp`, `DimensionUI.cpp`, `SupportEditor.*`, `DatumUI.cpp`, `MainWindow.*`; `src/render/Viewport.*`, `SketchView.*` | Spatial authoring and selection, dimensions, branch previews, support picking and command state |
| Build and verification | `CMakeLists.txt`, `tests/core_modeling/`, `tools/verify_cm6_review.py`, `tools/check_core_modeling.py` | Register spatial targets; reuse evidence conventions with CM7-specific cases |

New small spatial model/solver and exact-curve files are expected. Choose final
names during CM7.a; do not build a generic solver framework or replace existing
planar APIs. Audit every reader/editor/export/portable-copy caller of collections
being extended so an older edit path cannot silently discard spatial data.

## Ordered execution checklist

### Preparation — scope and baseline

- [x] Freeze base/head/source hashes, inspect local guidance and Step Zero
  prerequisites in lettered order, and establish a current build/test baseline
  using [build guidance](../../cmake/README.md). Record blockers and continue
  independent work; inherited milestone checkboxes are not a fresh baseline.
- [x] Expand the 14 coverage rows below into concrete analytical/native cases.
  Read T4 PDF pages 14–15 and T8 pages 18–21, 25–27, 48–49 in the retained course
  cache; inspect adjacent diagram pages 24 and 22 for chair dimensions and the
  endpoint-normal support example. Retain source hashes/page references. The
  text cache was inspected during planning; diagram dimensions still require
  visual verification before freezing course fixtures.
- [x] Freeze the bounded spatial entity/constraint list from those sources.
  Start with points and connected lines for the chair workflow, then the
  remaining required curve primitives; a line-only slice cannot close CM-190.
  Define helix/spiral parameter modes explicitly: CM-199 is a roadmap prerequisite,
  and its source note does not claim the course specifies all creation methods.
  Record unsupported modes as open requirements, never silently omit them.

### CM7.a — Semantic contracts and dependency rules

- [x] Specify versioned spatial entities with stable entity/point IDs, 3D
  coordinates, construction intent and orientation; derived definitions carry
  source references, projection mode/direction, selected branch and parameter
  ranges, ordered composite members, or helix/spiral parameters.
- [x] Specify plane/axis-relative relations, coincidence, dimensions and any
  other required constraints from preparation. Distinguish driving/reference
  values and active drawing-plane state from persistent On Plane constraints.
  Use the selected plane's local frame for ParallelYZ, ParallelZX and AlongZ;
  world-axis substitution is incorrect for rotated datums.
- [x] Define endpoint, tangent and frame semantics for reversal, closed seams,
  corners, inflections and zero derivatives. A tangent fixes a normal plane but
  does not determine its in-plane rotation: specify a stable reference/up-vector
  policy and explicit singularity handling. Do not silently smooth corners.
- [x] Extend typed graph validation with explicit spatial membership and source
  roles, ordering/cycle checks and unchanged aggregate resource bounds. Do not
  let a spatial sketch satisfy an existing planar-profile requirement by accident.
  Validate finite values, units, cardinalities, IDs and supported combinations
  at parsing boundaries. Unknown schema versions must fail safely.
- [x] Add contract/round-trip/graph tests and a short CM7 contract decision record.
  **Gate:** valid definitions round-trip; malformed, cyclic and wrong-role inputs
  fail deterministically without changing accepted state. Engine work remains open.

### CM7.b — Worker-only spatial solver and exact curves

- [x] Implement a separate Eigen spatial solver, preserving PlaneGCS for planar
  sketches. Define variable scaling, convergence, rank/remaining freedom and
  redundant/conflicting constraint reporting. Bound iterations, variables and
  cancellation work. Reuse existing diagnostic conventions where applicable.
- [x] Verify independent geometric residuals and rank, including rigid-body
  freedom, rotated planes, unit changes, nearly singular systems, inconsistent
  dimensions and failed dragging. Preserve the last accepted/valid state; a
  numerical optimizer returning success alone does not establish a valid sketch.
- [x] Implement exact worker curves for admitted spatial entities and both
  projection modes. Sketch-on-sketch must produce a spatial curve satisfying
  both source projections; it is not planar Convert Entities. Sketch-on-face
  must respect trimmed planar/curved faces and producing-stage dependencies.
- [x] Enumerate projection branches with deterministic semantic identities and
  source provenance. Return candidates for ambiguity; accept only the selected
  branch. Missing/split/merged branches require explicit repair, never nearest
  display segment or result-list index selection.
- [x] Implement ordered/reversible composite paths with continuity diagnostics,
  and exact editable helix/spiral definitions for the frozen parameter modes.
  Reject disconnected or ambiguous chains where a single path is required;
  preserve intentional corners. Validate pitch/radius/turns/orientation and
  degeneracies using each mode's contract, not blanket assumptions.
- [x] Extend worker read-only previews and typed curve/support inspection.
  Tessellation is display output, not authoritative curve geometry. Coalesce
  previews to one running and one replaceable pending request; reject stale
  results and retain existing memory/message limits.
- [x] **Gate:** independent analytical geometry/solver checks and real-worker
  preview/cancellation tests pass. Isolated experiments stay under tests; no
  production fallback to sampled curves or frozen geometry to mask failures.

### CM7.c — Durable definitions, replay and reference repair

- [ ] Extend existing SQLite semantic storage and compatibility manifests for
  spatial sketches and derived curves. Persist IDs, original definitions,
  branch selection and endpoint/frame provenance; recompute exact outputs on
  replay. Persisted derived evidence must be checked, not trusted as frozen shape.
- [ ] Extend the actual `PreviewModelSupports`/`SetModelSupports` path for atomic
  multi-collection edits, or introduce versioned ModelEdit only where necessary.
  Audit this seam first: shared architecture's proposed ModelEdit names are not
  proof that those commands already exist. Do not create parallel writer paths.
- [ ] Preserve omitted collections on unrelated edits; an explicit empty
  collection has separately validated deletion semantics. Maintain revision
  checks, deduplication and sticky reader requirements across retained history.
  Verify old projects remain readable, upgrades use copies and older readers
  refuse new semantics without damaging files.
- [ ] Implement explicit branch/endpoint repair against the complete pending
  model and matching revision/fingerprint. Rebuild affected descendants in one
  atomic operation. Cancel, invalid repair, worker failure or stale candidates
  must leave accepted state unchanged.
- [ ] **Gate:** save/reopen, deterministic replay, portable copy, undo/redo,
  duplicate commands, restart and interrupted-commit tests pass in marked
  disposable roots. Include upstream mutations and unrelated-edit retention.

### CM7.d — Native spatial authoring and derived-curve tools

- [ ] Add entry/exit and editing of a 3D sketch using existing native command,
  draft and property-panel patterns. Display the active XY/YZ/ZX plane and triad;
  Tab switches planes before or after anchoring a segment without moving that
  anchor or implicitly adding On Plane constraints.
- [ ] Add spatial axis/plane inference with visible proposals and explicit
  confirmation; persist only confirmed constraints. Provide unit-aware spatial
  dimensions, relation selection, constrained dragging and useful conflict
  diagnostics. Existing planar gestures and focus handling must still work.
- [ ] Add projected/composite/helix/spiral authoring, source selection, branch
  highlighting and explicit branch choice. Support edit/reopen round trips for
  every exposed value. Ambiguous results cannot be accepted until resolved.
- [ ] Allow stable curve/endpoint selection for datum planes and sketch support;
  use worker tangent/frame results. Publish reference curves distinctly from
  solids, including tree/selection behavior and clear nonsolid evaluation refusal.
- [ ] **Gate:** worker-backed native create/edit/accept/cancel/repair/undo/reopen
  workflows pass in mm/in and observed effective scales 100, 125, 150, 175 and
  200 percent. Verify keyboard and pointer paths, focus and accessible labels;
  synthetic events do not certify physical devices or assistive technology.

### CM7.e — Workflow evidence and completion review

- [ ] Reproduce the T8 projected path, including an endpoint-normal plane and
  supported sketch attachment. Mutate both source sketches and the target face;
  verify rebuild, branch ambiguity, repair and persistence.
- [ ] Reproduce the chair-frame **path** and its relations/dimensions using the
  verified diagram and the offset-plane endpoint constraint. Verify plane
  switching, edits and reopen. The finished swept/mirrored tube remains CM8.
- [ ] Exercise composite gaps/corners/reversal, closed seams, helix/spiral edits,
  degenerate tangents, solver conflicts, stale previews and upstream deletion.
  Verify downstream support frames, not only displayed path appearance.
- [ ] Run focused CM7 tests followed by the enabled regression suite, changed-
  source analyses, formatting, architecture/isolation/specification checks and
  native cases. Measure bounded preview/cache behavior and repeated edit/undo
  cycles without treating a larger machine as physical 8 GB acceptance.
- [ ] Record current source identity, commands, actual enabled/disabled test
  counts, failures, expected/actual geometry, native scales/units and fixture
  paths in a CM7 evidence record. Update SDD, traceability/coverage and the
  milestone summary together; keep historical failures and unrelated evidence.
- [ ] Close each row only with its own linked evidence. Report separately:
  production implementation, available-Mac verification, course-path coverage
  and full acceptance. Unrun platform/device/installed-build/resource gates
  remain Blocked/Not run; passing one row cannot close the milestone.

## Coverage-to-verification matrix

Preserve each row's existing `cm-NNN-geometry` and `cm-NNN-native` identities.
Assign actual executable test functions during preparation; these names alone
are not executable evidence. Every row also needs native author/edit/reopen and
an upstream-change or failure case.

| ID | Source PDF pages | Required behavior and independent check |
|---|---|---|
| CM-038 | T4 14 | ParallelYZ: line's local X component is zero; rotate the selected plane and re-solve |
| CM-039 | T4 15 | ParallelZX: line's local Y component is zero; test a non-world datum |
| CM-040 | T4 15 | AlongZ: direction is normal to the selected plane; conflicting direction constraints fail |
| CM-190 | T8 18–21, 25–27, 48–49 | All admitted 3D entities, stable endpoints, exact evaluations and required chair-path geometry |
| CM-191 | Same T8 pages | Spatial dimensions independently measured; driving/reference distinction, units and conflicting edits |
| CM-192 | Same T8 pages | Plane switching before/after first click preserves anchor/world geometry; Tab and UI agree |
| CM-193 | Same T8 pages | Axis/plane inference has correct residual; unconfirmed proposals do not survive commit |
| CM-194 | Same T8 pages | On Plane: signed point-to-plane distance is within tolerance; moved offset datum rebuilds path |
| CM-195 | Same T8 pages | Sketch-on-sketch curve satisfies both original projections; source mutation changes the correct path |
| CM-196 | Same T8 pages | Sketch-on-face curve lies on selected trimmed face and satisfies projection direction; boundaries and misses |
| CM-197 | Same T8 pages | Multiple candidates require explicit choice; selection survives benign rebuild and invalidates on split/loss |
| CM-198 | Same T8 pages | Composite order, orientation and endpoints match sources; gaps, branching and tangent discontinuities reported |
| CM-199 | Same T8 pages; roadmap prerequisite | Helix: independent radius, axial rise/turn, handedness and length; spiral: radial law/endpoints and planar containment; test all frozen parameter modes |
| CM-200 | Same T8 pages | Endpoint/tangent frames agree with independent derivatives; reversal, seam, zero derivative and support regeneration |

## Verification commands and delivery

Reuse documented commands; these are future execution commands, not results:

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug -R '^cm7\.' --output-on-failure
ctest --preset macos-debug --output-on-failure
python3 tools/format_sources.py
python3 tools/check_architecture.py build/macos-debug
python3 tools/check_isolation.py
python3 tools/check_core_modeling.py
python3 docs/scripts/check_specs.py
git diff --check
```

Register the proposed `cm7.*` tests before invoking that filter and fail if none
match. Use `tools/static_analysis.py` with the real affected compile commands.
Add the smallest CM7 runner based on the existing CM6 runner when cases exist;
record its actual native invocation, observed scale and timestamped artifacts.
Tests must require marked disposable roots and refuse release recovery paths.

Execute a → b → c → d → e. Each lettered task gets a separate dated review branch,
scoped evidence, and a commit/push after successful verification under the
standing workflow. Stack dependent branches on verified prerequisites if needed
and record their bases; verify the origin SHA after pushing. Never merge without
review or force-push. Writing this plan does not complete a lettered task or
require a product build, commit or push.

**First execution prompt:** Carry out CM7 preparation, then CM7.a contracts only.
Freeze source-derived semantics and test mappings, audit the named integration
seams, define spatial/branch/frame contracts, and verify parser/graph behavior.
Report prerequisites and exact coverage before starting CM7.b. No engine, schema
migration or native-tool completion claim belongs to the contract gate.

**Planning limits:** No product code or schema changed; no build, solver/geometry,
native, hardware or remote verification was run for this plan. The highest risks
are numerical rank/conflict diagnosis, branch identity through topology changes,
and stable tangent-frame orientation. Prove these before presenting tools as complete.
