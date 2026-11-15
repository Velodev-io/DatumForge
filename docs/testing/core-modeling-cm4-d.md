# CM4.d — native datum selection, visibility and Normal To

3 November 2026. User authorization: “okk go ahead with cm$.d”, following the
CM4.d/e review. Branch `review/cm4-d-20261103`, base `b52da18`.
[Contract](../decisions/core-modeling-cm4-native.md) and
[plan](../superpowers/plans/2026-11-03-cm4-d.md).

## Production scope

The native model tree and Reference geometry browser select principal planes/axes,
accepted sketch planes and every evaluated custom datum output, including multiple
planes with stable IDs. Checkboxes synchronize session-local visibility between
both trees and the viewport. A Datum selection filter picks displayed plane borders,
center marks and axis lines. Hidden geometry cannot be picked; hidden rows can
still be selected for inspection and navigation. Normal To is available from the
browser, Evaluate tab, command search and Ctrl+8 (platform shortcut mapping applies).
It aligns the full plane/sketch frame orthographically, preserves zoom and centers
the selected origin. An axis has no unique Normal To frame and disables the action.
Standard views, orbit, pan and fit remain available, including datum-only Parts.

`SupportGeometry` is a revision-checked read-only worker command. It rebuilds
original accepted support definitions, returns only bounded semantic datum frames
and directions, and changes neither history nor accepted revision. The client
validates type, identity, finite coordinates, orthonormal frames and unit axes.
Epoch/session/document/revision correlation prevents obsolete display publication.
Revision changes remove the old custom frame pending fresh inspection; failures
preserve accepted solids and permit explicit Refresh. Visibility is not saved
geometry or an undoable edit. Exact geometry, solving and writes remain worker-owned.

This task implements selection/display/navigation for accepted supports. Support
creation/editing, attachment/Convert Entities authoring and the full upstream
edit/repair/undo/reopen course workflows are not exposed by these controls and
remain open for CM4.e/follow-up native authoring. Existing supported-Part loose
sketch/feature/history commands remain refused; no frozen frame workaround is added.

## Verification

`cm4.datum_contracts` uses marked disposable roots with real worker IPC. It covers
strict malformed/bounded datum parsing; independent tilted/reversed camera
projection; pointer selection through both trees and viewport; keyboard visibility;
multiple outputs; hidden picking refusal; axis refusal; mm/in display; accepted
revision retention; refreshed upstream frames; document isolation and worker restart.
It runs on both a datum-only Part and a Part with an exact solid. The existing
`cm4.support_worker` additionally checks read-only inspection and stale-revision
refusal directly at the worker/store seam.

Reproduce: `python3 tools/verify_cm4_native.py --native` on the provisioned Mac,
with local IPC and logged-in desktop access. Without `--native`, UI CTest is
headless and cannot certify native rendering. Artifacts are preserved in the
[execution folder](artifacts/core-modeling/cm4-d-20261103/).

An initial compile refused an ambiguous Qt `const char* + QChar` expression in
axis display identity construction. Explicit `QString` construction corrected it.
The first focused offscreen test passed, then the initial native 100% mm/in test
passed and its screenshots were inspected. Final verification passed in
[the immutable execution folder](artifacts/core-modeling/cm4-d-20261103/verification/20261103T051241.992649Z/).

- Full development build: Pass.
- Full enabled CTest: **88 passed, zero failures**, 417.21 seconds. The one
  deliberately failing harness probe remained Disabled; it is not counted as passed.
- Pinned formatting, 12 changed-source Clang analyses, native dependency ownership,
  research isolation, coverage and specification checks: Pass.
- Cocoa native datum contracts: **7 QtTest cases passed at each of five effective
  scales (100/125/150/175/200%)**, using mm/in and datum-only/solid fixtures.
  Existing native workspace checks passed at 100% and 200%.
- Screenshots inspected: initial 100% mm and final 100% inch / 200% solid-overlay
  captures; controls, visibility state, selected datum and origin units are legible.
- Source-integrity hash comparison: Pass. The host reports physical 8 GiB RAM;
  this is not full physical-8-GB workload acceptance or a latency certification.

[Machine-readable verification](artifacts/core-modeling/cm4-d-20261103/verification/20261103T051241.992649Z/verification.json)
records commands, durations, retained acceptance boundaries and native scope.
The first post-verification checkbox update was refused until CM4.d was registered
in `task-status.json`; the refusal is retained beside the final passing
specification report. No source or test assertion changed for this documentation correction.

Production implementation and available-Mac development/native verification:
**Pass for CM4.d selection/display/navigation scope**.
Full acceptance: Blocked for CM4.e course workflows, native authoring, installed
builds, Windows/Linux, physical input/assistive technology and physical 8 GB.
Preserve deferred 6.1b/6.4c/7.4c. Module 11 is not started.
