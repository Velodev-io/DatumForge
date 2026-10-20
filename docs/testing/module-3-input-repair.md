# Module 3 input and direct editing repair

Current scope note: the later explicit Module 4 instruction supersedes the historical 4.1a stop boundary below. See [authorization](../decisions/implementation-scope.json). Stop before Module 5; earlier evidence remains scoped to its original run.

Date: 20 October 2026. Status: **feasible available-Mac repair verified; full
platform/physical-device acceptance Blocked**. Scope: repair the user-reported resize, trackpad and
post-creation editing gaps before authorized Module 4.0 workspace work. Stop before
4.1a exact features. [Authorization](../decisions/implementation-scope.json).
The earlier [Module 3 baseline](module-3.md) and [circle follow-up](module-3-interaction-followup.md)
remain historical evidence; they did not establish these additional interactions.

## Reproduction, implementation and boundaries

The [initial regression](artifacts/module-3/input-repair/input-red.txt) reproduced
four failures: viewport resizing moved the model center; trackpad pixel scrolling
was not panning; the mandatory 1 mm grid rounded a fractional radius; click-based
circle creation was absent. The original implementation had a size-dependent
origin, angle-only wheel handling, unconditional grid snapping and a circle path
requiring a held mouse button. Those causes explain the reported behavior without
assuming a trackpad hardware fault.

The repair was implemented before the command-tab/left-panel work:

| Interaction | Current behavior and limits |
|---|---|
| Resizing | Model camera center/scale and pending anchors are independent of viewport/dock size; forward/inverse transforms agree. Fit includes complete circle/arc extents. |
| Pointer precision | Existing-point snapping uses eight logical pixels; 1 mm grid is optional and off initially. Numeric input bypasses pointer snapping. |
| Mouse and trackpad paths | Click/move/click or press/drag/release circles, two-input/drag lines and rectangles, pixel-scroll pan, native pinch with a fixed cursor anchor, Space + left drag and middle pan. Physical-device delivery remains Blocked. |
| After creation | Escape switches to selection and preserves completed geometry. Selected shapes expose contextual properties; double-click reopens accepted sketches. Circle radial handles resize radius, center points move; rectangle corners resize and edges translate. |
| Constraints | Driving dimensions can be edited in Properties or Dimension; free dragging conservatively refuses affected constraints except horizontal/vertical and explains the relation. The worker still validates all previews/acceptance. |
| Rectangle semantics | Isolated axis-aligned four-line loops are recognized without schema changes or ID replacement. Corner crossing/collapse is rejected. Connected networks can fall back to ordinary point/line editing. New rectangles have four explicit orientation relations; old projects are not rewritten on read. |
| Draft history | Up to 40 semantic sketch snapshots support Undo/Redo, property edits and deletion without project revision changes. Explicit Finish Sketch accepts one durable transaction. Cancel Sketch discards the whole edit; Escape cancels transient input/tool state. |
| Responsiveness | Each drag move updates only the bounded local projection and selection fields. Entity-list rebuilds and worker commands occur after the gesture, not per move. No exact geometry or writes were moved into the UI. |

Arcs retain numeric radius/start/sweep controls; three-point arcs and arbitrary
nonlinear constrained dragging remain unsupported. Full snapping preferences,
feature dependencies and broad accessibility remain later work. See the
[adapter decisions](../decisions/module-3-sketches.md) for semantic admission bounds.

## Actual verification

The [stage-one native run](artifacts/module-3/input-repair/native-stage1.txt) passed
before the layout conversion. Its screenshots and source hashes are retained.
The final combined [workspace verification](module-4.md) reruns the expanded
Module 3 tests against the new layout and records each command and scale.

The added tests cover stable resize/inverse transforms, fractional circle sizing,
click-based creation, whole-circle fit and synthetic pan/pinch; direct rectangle
corner preservation and rejection, radius edits, constrained movement and deletion;
then real worker previews, no accepted mutation before Finish, draft/durable
undo/redo, save/reopen and unchanged IDs. A 60-move native trace asserts zero
entity-list rebuilds and zero worker requests during the drag. Its timing includes
application event dispatch and event processing only; it is neither physical
trackpad latency nor an SRS p95/FPS budget result.

[Final combined run](artifacts/module-4/verification.json): configure/build, all
27 enabled CTest entries, formatter, static analyzer, ownership/research checks,
fresh-source build, Cocoa workflows at 100/125/150/175/200% and earlier native
module regressions Pass. The intentional failure probe remains Disabled and is
checked independently. [Source consistency](artifacts/module-4/runs/20261020T060237.801495Z/evidence-consistency.json)
and [visual review](artifacts/module-4/runs/20261020T060237.801495Z/visual-review.json) record the final tested inputs
and scope. The [workspace report](module-4.md#actual-final-results) links every log
and the actual event-processing timing summaries. Physical trackpad/mouse ergonomics, Windows/Linux, Intel/minimum macOS,
additional GPUs, assistive technology, installed builds, controlled OS-network-
disabled execution and full physical 8 GB B0–B5 resource acceptance remain
**Blocked**. All research and old evidence remain preserved; no user project was
used by these tests.
