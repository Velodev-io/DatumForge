# DatumForge core-modeling expansion

Version 0.1. Authorized 22 October 2026 by the user's direct request to implement
the T2–T8 plan. Execute after Module 10 and before Module 11. Local feature
commits were separately authorized on 25 October; no merge/push without review.
Preserve 6.1b/6.4c/7.4c and all historical evidence. No assembly/drawing expansion.

## Completion contract

The [coverage register](research/core-modeling-coverage.json) is the additive CM
requirements/traceability register. Each row names source PDF pages, milestone,
lettered tasks, an analytical-fixture identifier and a native-workflow identifier.
Identifiers scheduled for implementation are not executable tests or evidence.
Every described variant must have actual passing evidence to close its row.
The [execution record](testing/core-modeling.md) distinguishes feasibility,
implemented production slices and whole-milestone/full-platform acceptance.

Retain pinned native C++/Qt/OCCT/PlaneGCS/Eigen, offline core, SI semantics and the
physical 8 GB baseline. The desktop owns drafts/display geometry; the sole worker
owns exact curves, solving, caches and writes. No kernel/SQL objects in the UI.
No new visible functioning controls may claim an unimplemented operation.

## Shared architecture work

Extend evaluation into a graph of sketches, datums, derived curves and solids;
face-attached sketches depend on their producing feature. Typed references carry
document/entity/topology role plus optional endpoint/parameter. Resolve missing
or ambiguous topology explicitly, never by display index. Persist versioned
semantic collections in existing SQLite tables with a new compatibility manifest;
read old projects and upgrade on copies. Older readers refuse new semantics.

Add versioned PreviewModelEdit/CommitModelEdit for atomic multi-collection edits,
retaining revisions, request deduplication, undo and recovery. Distinguish solid,
surface and reference-curve results; volume tools refuse nonsolids. Coalesce exact
previews into one running computation and one replaceable pending draft. Preserve
current admission caps; increased limits require measurements, not assumptions.

## Lettered execution tasks

Each milestone follows **a contracts → b engine → c persistence → d native UI →
e verification**. Do not mark an entire milestone complete for a partial slice.

### CM0 — Coverage, baseline and isolated feasibility

- [ ] **CM0.a:** Freeze page-indexed coverage, requirement/test identities, authorization and preserved gates.
- [x] **CM0.b:** Capture a fresh configure/build/CTest/static/ownership baseline and source identity. See [scoped baseline result](testing/artifacts/core-modeling/20261023T170331.720328Z/verification/baseline-scope.json); its later native activation failure is retained separately.
- [ ] **CM0.c:** Run isolated face/full-round fillet, multi-thickness shell, surface-termination, constrained-spline and guided-loft experiments against pinned libraries.
- [x] **CM0.d:** Record successful and blocked capabilities, analytical invariants and nonproduction boundaries; schedule independent downstream work.
- [x] **CM0.e:** Validate coverage/evidence integrity and baseline preservation. Experiments do not certify production adapters.

### CM1 — Sketch primitives, relations and construction intent

- [ ] **CM1.a:** Define versioned curve/group/reference and relation contracts, with stable IDs and unchanged admission limits.
- [ ] **CM1.b:** Implement all rectangle/slot methods; ellipse/elliptical arc/parabola/spline geometry; broader planar relation adaptation and independent residual/rank checks.
- [ ] **CM1.c:** Persist semantic groups and curve definitions; verify compatibility, identity checks, undo/reopen and failure-safe commits.
- [ ] **CM1.d:** Add familiar native creation methods, live previews, dimension controls and relation selection; preserve all arc gestures.
- [ ] **CM1.e:** Verify each covered primitive/relation through exact solving, interaction, save/reopen and upstream edits; whole milestone stays open until all variants pass.

### CM2 — Sketch editing

- [ ] **CM2.a:** Define split/descendant identities and explicit affected-constraint disclosure.
- [ ] **CM2.b:** Implement trim/extend modes, sketch fillet/chamfer, chain offsets/caps, mirrors/patterns and move/copy/rotate/scale, retaining associative construction intent.
- [ ] **CM2.c:** Persist definitions/mappings and undo; ambiguous references require repair; no silent controlling-dimension deletion.
- [ ] **CM2.d:** Add selection, live previews and operation-specific native controls.
- [ ] **CM2.e:** Verify analytical geometry, constraint retention and transactional workflows, including documented unsupported offset cases.

CM2.a–e remain **In progress**. The [straight-line editing slice](testing/core-modeling.md#25-october-cm2--straight-line-trimextend-slice) has production implementation and available-Mac verification; Power trim, Corner, other curve operands, treatments, offsets, transforms and full per-variant acceptance remain open. Passing this slice does not close CM2.

### CM3 — Dimensions, inference and constrained dragging

- [ ] **CM3.a:** Define driving/reference dimensions, placements, expressions and centerline/path associations.
- [ ] **CM3.b:** Implement all dimension variants, proposed automatic relations, temporary drag targets and deterministic Fully Define proposals.
- [ ] **CM3.c:** Persist only confirmed relations and accepted dimensions; reference values never drive geometry.
- [ ] **CM3.d:** Add on-canvas unit-aware editor, preview/restore and visible inference guides.
- [ ] **CM3.e:** Replay two-point Horizontal, revolved-profile diameter and constant-path-length workflows; preserve last-valid drag preview on failure.

CM3 production implementation now covers the approved current planar interfaces:
identity-safe dimension editing, reversal/increments/drawing metadata, multi-target
ordinates, true path length, temporary constrained drag targets, confirmed
inference guides and deterministic Fully Define All/Selected proposals.
[Completion contracts](decisions/core-modeling-cm3-interactions.md) define schema 8,
worker ownership, confirmation and resource bounds. The
[execution record](testing/core-modeling-cm3-completion.md) preserves individual
red/green cycles and final verification status. CM3.a–e have **Pass** production
implementation and available-Mac development-build verification (all 76 enabled
tests, static checks and five native scales, with affected-test follow-up). Their
overall status remains **Blocked** for unrun full course/platform/device/installed/
resource acceptance.
Earlier [dimension](decisions/core-modeling-cm3-dimensions.md) and
[associated-dimension](decisions/core-modeling-cm3-associated-dimensions.md)
contracts/evidence retain their original historical scope.

### CM4 — Datums and associative sketch support

- [x] **CM4.a:** Extend the dependency graph to support typed geometry references and reject cycles/order violations. Production structural implementation and available-Mac development-build verification: [CM4.a evidence](testing/core-modeling-cm4-a.md). This does not close CM4.b–e or whole-CM4 acceptance.
- [x] **CM4.b:** Implement described plane/axis modes, face attachment, Convert Entities and external sketch references. Exact engine/read-only worker implementation and available-Mac development verification: [CM4.b evidence](testing/core-modeling-cm4-b.md). Durable definitions/evolution/repair, native controls and full CM4 acceptance remain CM4.c–e.
- [ ] **CM4.c:** Persist support definitions and unique reference evolution; require explicit missing/ambiguous repair.
- [ ] **CM4.d:** Add native datum selection/visibility and Normal To navigation.
- [ ] **CM4.e:** Change upstream supports, rebuild descendants, repair, undo and reopen without freezing the former frame.

### CM5 — Extrude/revolve, regions, thin features and draft

- [ ] **CM5.a:** Define contour selections, independent start/end conditions, direction/depth and thickness/draft semantics.
- [ ] **CM5.b:** Implement exact starts/extents, open cuts, thin extrude/revolve and standalone draft. Surface termination remains a geometric dependency, never a substituted fixed depth.
- [ ] **CM5.c:** Persist definitions, Boolean provenance and repair behavior.
- [ ] **CM5.d:** Add native depth/thickness/draft manipulators and bounded exact previews.
- [ ] **CM5.e:** Verify drafted 10 mm/15 mm example, thin profiles and reference-terminated features under upstream changes.

### CM6 — Holes, ribs, repetition and treatments

- [ ] **CM6.a:** Define Hole Wizard families/positions, cosmetic threads, rib controls, pattern selections and treatment variants.
- [ ] **CM6.b:** Implement custom and catalog-driven holes, ribs, two-direction patterns, feature/face/body repetition, advanced fillets and chamfers with propagation.
- [ ] **CM6.c:** Retain standard/version/resolved values and thread metadata; do not model cosmetic threads as helical solids.
- [ ] **CM6.d:** Add native position sketches, selection filters and treatment previews.
- [ ] **CM6.e:** Use nontrivial per-variant fixtures; simple cuts/edge fillets do not close Hole Wizard/face/full-round coverage. Catalog coverage depends on CM9.

### CM7 — Spatial sketches and derived curves

- [ ] **CM7.a:** Define spatial curve entities, branch references and plane/axis/dimension constraints.
- [ ] **CM7.b:** Implement a separate Eigen 3D constraint adapter, projected/composite curves and helix/spiral definitions; retain planar PlaneGCS.
- [ ] **CM7.c:** Persist spatial IDs, endpoints and tangent frames for downstream supports.
- [ ] **CM7.d:** Add active-plane switching, spatial inference and explicit projection-branch selection.
- [ ] **CM7.e:** Verify projected paths/chair frame, conflicts, ambiguous projections and discontinuities.

### CM8 — Shell, sweep and loft

- [ ] **CM8.a:** Define solid/surface outputs, thickness/guide/section/connector and corner-transition contracts.
- [ ] **CM8.b:** Implement OCCT shell/offset, pipe-shell sweep/centerline adapters and ordinary through-section lofts, including every described advanced mode.
- [ ] **CM8.c:** Persist correspondence and topology history; invalid computations preserve accepted state.
- [ ] **CM8.d:** Add native section/guide/connector selection and preview controls; no silent path smoothing.
- [ ] **CM8.e:** Verify shell, pipe, chair frame, impeller and multisection loft workflows, including edits and invalid geometry.

### CM9 — Catalogs, material/evaluation and model display

- [ ] **CM9.a:** Freeze broad standards/material coverage and verify redistribution rights; missing data blocks its own gate while independent coding continues.
- [ ] **CM9.b:** Implement offline catalog/custom entries, material properties, exact mass/inertia/section measurements and model display styles/section views.
- [ ] **CM9.c:** Persist dataset version and resolved values; catalog updates never rewrite accepted model meaning. Store appearance and named modeling sections separately from solid geometry.
- [ ] **CM9.d:** Add search/favorites/custom libraries, evaluation results, appearance selection and section manipulators.
- [ ] **CM9.e:** Verify all catalog families and model scopes; broad coverage, not a few sample entries, is required. Do not copy proprietary libraries or invent dimensions.

## Verification and release

For every slice: analytical/independent geometry checks; stable IDs; edits and
dependency changes; save/reopen/portable copy; undo/redo; failed/canceled/duplicate
commands; stale responses; worker restart and interrupted commit. Native checks
cover keyboard and pointer workflows, units and 100–200% effective scaling.
Use marked disposable roots; never attach a test to a user project or recovery.

Preserve frozen B0–B6 and add independent course fixtures. Measure bounded
previews/caches and repeated edit/undo/redo after trimming. Existing SRS latency,
resource and exactness targets are unchanged. Unavailable platform/device tests
remain Blocked. Final Module 11/12 work needs subsequent scope authorization;
neither is started by this extension. No full milestone or course percentage may
be inferred from passing experiments, buttons or partial implementations.
