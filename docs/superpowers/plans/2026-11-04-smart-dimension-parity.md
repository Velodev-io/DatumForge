# Smart Dimension interaction parity with SolidWorks

Authorized 4 November 2026 by the user's complete Markdown handoff. This is the
saved execution plan; unchecked items are requirements, not evidence.

## 1. Objective and boundaries

Make DatumForge's Smart Dimension match every behavior identified in the
comparison, using the supplied SolidWorks training sheets as acceptance reference.
Reuse the dimension engine, worker preview, expressions, constraints, undo,
recovery and persistence. Implement missing interaction and graphics; extend
geometry semantics only where required selections cannot currently be represented.

This is a CM3 interaction follow-up with separately implemented drawing
integration. Do not start Module 11, close 6.1b/6.4c/7.4c, or claim compatibility
with every SolidWorks release.

References: T4 ADD RELATIONS PDF pp.19–27,32–33; T5 ADD PART FEATURES PDF
pp.12–15,33–34; T10 DRAWING PDF p.32. Originals are in
`/Users/binova/Downloads/T SHEETS FOR FACULTY ONLY`. Preserve private research
restrictions: never redistribute PDF pages in product resources/public artifacts.

## 2. Starting conditions and implementation rules

- Read workspace guidance and current CM3 contracts before editing.
- Inspect branch, dirty files and build. Preserve unrelated CM5.c changes.
- Record starting source identity and tests; prior six dimension passes do not
  prove the new interaction works.
- Use ponytail: smallest complete changes, existing paths, no dependencies,
  second dimension engine, general framework or unrelated refactoring.
- Primarily change DimensionUI.cpp, SketchView and WorkspaceChrome.cpp; extend
  solver, storage and drawing only for the requirements below.
- Preserve the advanced editor and its testable object names. Share validation
  and application with the compact editor only as needed.
- Execute and verify one checklist task at a time. Never weaken assertions.
- Commit/push each verified task on its review branch; stage only its changes.
  Never merge, force-push or include unrelated CM5.c work.
- Record prerequisites blocking isolated commits/reliable validation and continue
  independent work.

## 3. Required behavior and implementation sequence

### Task A — Define the acceptance matrix

- [x] Create a page-indexed checklist for every requirement below, separately
  recording baseline, implementation, tests and native verification.
- [x] Distinguish implemented CM3 mathematics from remaining interaction parity.
- [x] Preserve historical verification records.

| Area | Required result |
| --- | --- |
| Activation | Start before selection and consume valid preselection. |
| Selection | Lines, circles, arcs, points, line pairs, centerlines and arc-angle gesture. |
| Placement | Pointer-following dimension graphics; click to place. |
| Value editing | Compact Modify with units, expressions, preview, accept, restore/cancel. |
| Repetition | Stay in dimension mode after acceptance. |
| Graphics | Extension/dimension lines, arrows, radial leaders, angular arcs. |
| Special modes | Horizontal, Vertical, Ordinate, Horizontal/Vertical Ordinate, Path Length. |
| Editing | Double-click labels; move labels without changing geometry. |
| Drawing | Reference dimensions and functional import of marked dimensions. |

### Task B — Add persistent Smart Dimension mode

- [ ] Rename visible command Smart Dimension; preserve ID and aliases.
- [ ] Add flyout using the existing toolbar-menu pattern.
- [ ] Enable whenever sketch editable/worker ready without requiring selection.
- [ ] Sketch UI owns transient Selecting, Placing, EditingValue states.
- [ ] Keep picks, hover, previews and retained centerline/datum outside storage.
- [ ] Clicks select operands instead of creating/dragging geometry.
- [ ] Highlight operands and show a concise next-action prompt.
- [ ] Consume valid preselection; explain invalid selection without mutation.
- [ ] Acceptance clears ordinary operands and resumes selection; retain only
  the centerline/datum required for repeated workflows.
- [ ] Escape cancels candidate, then exits mode; tool/document/sketch/epoch
  changes cancel transient state.

### Task C — Match selection and inference rules

- [ ] Circle defaults Diameter; arc defaults Radius.
- [ ] Lines/two points: pointer chooses horizontal/vertical/aligned; explicit
  Horizontal/Vertical modes lock orientation.
- [ ] Resolve endpoints/centers to stable point IDs; never duplicate centers.
- [ ] Point-line/parallel-line distances are perpendicular; H/V presentation
  applies to corresponding axis-aligned cases.
- [ ] Nonparallel lines give angular preview; pointer chooses sector and solving
  preserves the chosen branch.
- [ ] One circular arc's center/endpoints resolve to its existing Sweep.
- [ ] Profile/construction centerline supports linear diameter; crossing axis
  switches radial/full-diameter intent with explicit preview.
- [ ] Retain centerline for successive dimensions.
- [ ] Ambiguity requires explicit selection, never nearest-entity substitution.

Reuse kinds; add only PointLineDistance and ParallelLineDistance with stable
operands. Reuse PlaneGCS point-line distance; parallel-line distance also preserves
parallelism. Independently measure and validate residuals.

### Task D — Placement graphics and compact Modify

- [ ] Generate display geometry from operands/model-space label position.
- [ ] Render H/V/aligned extensions/arrows, radius/diameter leaders, centerline
  diameters and angular arcs.
- [ ] Share layout for previews, accepted labels and hit tests.
- [ ] Keep text/arrows readable across zoom/scales; respect units, reference
  parentheses, angle units and named expressions.
- [ ] Pointer-place new dimensions; retain numeric advanced placement fields.
- [ ] Label dragging changes placement only, never geometry.
- [ ] Compact Modify near label, clamped to viewport.
- [ ] Keep value, Preview/Accept/Cancel visible; Advanced contains increments,
  reverse, reference and drawing mark.
- [ ] Enter accepts only successful current worker validation.
- [ ] Cancel restores original candidate/placement; rejected values leave
  actionable errors and editor open.
- [ ] One draft undo step per accepted dimension; Finish Sketch publishes.
- [ ] Double-click exact dimension ID, including shared reference/driver operands.
- [ ] Fence asynchronous results by generation/context/revision/epoch.

### Task E — Complete special workflows

- [ ] Ordinates: place zero datum, successive point/straight-entity targets,
  retaining stable associations.
- [ ] Aligned labels/leaders; reuse individual IDs and driving/reference rules.
- [ ] No automatic Modify per ordinate; exit then double-click for editing.
- [ ] Path Length: reuse chain ordering, exact worker measurement and 64-member
  limit; add interactive selection/placement.
- [ ] Preserve constant total length in supported constrained dragging; explicitly
  reject branches/disconnections/duplicates.
- [ ] Preserve true circular Arc Length, distinct from chords/sweep.
- [ ] Preserve Fully Define, inference review and constrained dragging.

### Task F — Compatibility and drawing integration

- [ ] Keep old encodings for existing semantics.
- [ ] Schema 9 for new distances/angular presentation; never reinterpret schema 8.
- [ ] Sticky reader capability after CM5.c, including active/history/recovery/copy.
- [ ] Update every manifest admission/SQL binding consistently; retain Save As
  requirement for older sources.
- [ ] No SQLite table change; extend semantic payloads.
- [ ] Explicit functional Import marked dimensions drawing command.
- [ ] Associate source document/sketch/constraint and destination view IDs;
  worker recomputes values/projected anchors without guessed BRep edge matches.
- [ ] Imported reference annotations become stale through existing source revision
  tracking; deleted sources dangle and require explicit repair/removal.
- [ ] Deduplicate by source dimension/view; exclude unmarked dimensions.
- [ ] Interactive drawing Linear/Angular/Radius/Diameter selection and placement.
- [ ] Drawing measurements never drive Parts.
- [ ] Version associations, preserve old records, reuse renderer/exporter.

### Task G — Documentation and handoff evidence

- [ ] Document operands, exact gestures, keys and compatibility changes.
- [ ] Update page-indexed coverage without unsupported acceptance claims.
- [ ] Record native screenshots/results beside numerical tests; headless tests
  do not verify graphics.
- [ ] Final comparison identifies each closed or explicitly blocked gap.
- [ ] Never label complete while any required comparison behavior is deferred.

## 4. Verification and acceptance

Use existing harnesses and marked disposable roots, never the user's open document.

Focused tests cover activation/preselection/repetition/Escape/tool switching;
circle/arc defaults and line/point orientations; point-line/parallel-line distances
and degenerate operands; angular sectors/arc-angle; centerline crossing/reuse/order;
placement/label dragging/zoom/pan; editor expressions/units/increments/reverse/
reference/cancel; conflicts/input changes/stale replies/restart; ordinate series;
connected and invalid paths/constant-length dragging; save/reopen/undo/recovery/
portable copies/downgrade refusal/source preservation; drawing reference/import/
deduplication/source edits/deletion/PDF export.

Run the six existing CM3 dimension/completion tests after affected changes and
all enabled CTests after integration. Run pinned formatting, affected-source
Clang analysis, ownership/coverage/spec checks. Native mm/in workflows must pass
100/125/150/175/200% effective scales. Inspect linear/angular/radial/diameter/
centerline/ordinate/path graphics; recheck sketch creation, all arc gestures,
selection, constrained dragging and drawing export.

Physical devices, assistive technology, Windows/Linux, installed releases and full
resource checks remain Not run/Blocked unless performed. Completion requires
all acceptance rows implemented, focused/regression tests passing, native
interaction/graphics evidence, verified compatibility and an isolated change.

## 5. Assumptions and execution handoff

Parity means supplied course workflows/comparison differences, not proprietary
assets or undocumented release behavior. Use original icons/native theme and
compact Modify with existing advanced functionality. Preserve admission limits,
worker ownership, error handling and persistence safeguards. Drawing integration
is a separate final slice and remains required for complete parity. No production
edits were made while preparing the supplied plan.

Execute A–G in order using ponytail; preserve CM5.c and review/publication rules.
Report implementation, native verification and full acceptance separately.
