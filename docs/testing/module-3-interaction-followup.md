# Module 3 interaction follow-up

Historical circle-only closeout. The later [input/direct-editing repair](module-3-input-repair.md)
and [4.0 workspace evidence](module-4.md) supersede current interaction and scope
statements below, including mandatory grid snapping and Escape discarding a draft.
The following results describe their original dated source; they remain unchanged
as historical evidence.

Date: 20 October 2026. Status: **feasible available-Mac Module 3 follow-up closed;
full platform completion Blocked**. Stop before Module 4. No Module 4 implementation
was performed or authorized.

The user reported that holding and dragging while creating a circle did not change
its radius. Source inspection and the [pre-fix regression](artifacts/module-3/interaction-followup/circle-red.txt)
confirmed that mouse press immediately added a center and a fixed-radius circle.
The original Module 3 evidence covered numeric circle creation and dragging an
existing point, which did not establish this creation gesture. Its broader
completion claim did not cover this missing interaction. [Historical evidence](module-3.md)
is retained, including failures, test logs, source identities and screenshots.

## Behavior and boundaries

- Circle: press the snapped center, hold and move to preview the radius, then
  release to add one center/circle to the local sketch draft. The radius readout
  follows document display units. A zero-radius release adds nothing. Mouse motion
  holds only transient display coordinates; it neither creates semantic entities
  nor queues worker operations or changes the project revision.
- Escape cancels unfinished circle/vertex input first and retains completed draft
  geometry. With no unfinished input, Escape cancels the sketch edit. The explicit
  Cancel sketch edit button discards the entire edit. Tool switching drops
  unfinished input. A release after cancellation/tool switch cannot add a circle.
- Line/polyline/rectangle: first vertex shows a moving segment/opposite corner;
  second click completes geometry in the draft. Invalid vertices preserve the
  prior draft and pending input. Polyline retains its shared last endpoint until
  finished by tool switching or sketch acceptance.
- Numeric X/Y and Radius creation remains separate from pointer snapping and
  preserves typed SI precision. Arc input remains center plus numeric
  radius/start/sweep; a three-point arc pointer workflow is not implemented.
- Tool fields are contextual: radius for circle/arc, angles for arc, no creation
  coordinates in Select/drag. Empty display says No sketch instead of Accepted
  sketch. Native Qt rendering, the single worker's solver/project-write ownership,
  dependency pins, schema and physical 8 GB baseline remain unchanged.

The existing 1 mm pointer grid and 0.15 mm model-space snap tolerance still apply.
Screen-distance snapping preferences are a later UX decision; this follow-up does
not claim complete SolidWorks interaction parity or the finished product layout.

## Verification

The added `module3.gestures` test uses the real UI and worker with marked disposable
recovery/saved-file roots. It checks visible intermediate radii at 5 and 10 mm,
replacement of the previous preview, no entities until release, zero-size refusal,
Escape/tool switching, no project writes before acceptance, numeric circle/arc
creation, contextual fields, line/polyline/rectangle previews, complete pointer
line/rectangle creation, mm/inch display units, final 10 mm SI radius, stable
accepted state, undo/redo, save/reopen and cancel after solving. The native Module 3
slot also runs these workflows for both unit systems at each scale.

| Check | Actual result |
|---|---|
| Pre-fix reproduction | Fail as expected: mouse press added 2 entities instead of 0; retained [red log](artifacts/module-3/interaction-followup/circle-red.txt). |
| Focused final regression | Pass: [mm/inch gesture tests](artifacts/module-3/interaction-followup/gestures-final.txt), no failed/skipped checks. |
| Existing B1 workflow plus gestures | Pass headless: [expanded test log](artifacts/module-3/interaction-followup/expanded-green.txt). Offscreen graphics warnings are expected and do not certify native rendering. |
| Complete CTest | [Pass](artifacts/module-3/runs/20261020T035603.673933Z/ctest.txt): 24 enabled tests, zero failures; one intentional failure probe remains Disabled and its reporting is checked separately. |
| Fresh source build | Pass: [source export/configure/build and 24 enabled tests](artifacts/module-3/clean-reproduction.txt), using verified cached archives and existing dependency binaries; no installed-build or second dependency-build claim. |
| Build / format | Pass: [build](artifacts/module-3/runs/20261020T035603.673933Z/build.txt), [pinned formatter and whitespace](artifacts/module-3/runs/20261020T035603.673933Z/format.txt). |
| Static analysis / architecture / research | Pass: [Clang analyzer, no warnings](artifacts/module-3/runs/20261020T035603.673933Z/static-analysis.txt), [source and linked-library ownership](artifacts/module-3/runs/20261020T035603.673933Z/architecture.json), [97 research files unchanged](artifacts/module-3/runs/20261020T035603.673933Z/research-isolation.txt). |
| Native Module 3 | Pass at [100%](artifacts/module-3/runs/20261020T035603.673933Z/native-module3-100.txt), [125%](artifacts/module-3/runs/20261020T035603.673933Z/native-module3-125.txt), [150%](artifacts/module-3/runs/20261020T035603.673933Z/native-module3-150.txt), [175%](artifacts/module-3/runs/20261020T035603.673933Z/native-module3-175.txt), [200%](artifacts/module-3/runs/20261020T035603.673933Z/native-module3-200.txt); actual Cocoa B1 and pointer workflows in both mm/in, zero failed/skipped cases. |
| Prior module regressions | Pass: native [Module 1](artifacts/module-3/runs/20261020T035603.673933Z/native-module1-regression.txt) and [Module 2](artifacts/module-3/runs/20261020T035603.673933Z/native-module2-regression.txt). |
| Visual review | [100% mm circle](artifacts/module-3/runs/20261020T035603.673933Z/native-100/circle-drag-mm.png), [200% inch circle](artifacts/module-3/runs/20261020T035603.673933Z/native-200/circle-drag-in.png) and [100% accepted B1](artifacts/module-3/runs/20261020T035603.673933Z/native-100/sketches-native.png) inspected: live unit readout, dashed circle, contextual rows and persistent actions visible. [Review limits](artifacts/module-3/runs/20261020T035603.673933Z/visual-review.json). |
| Source identity / evidence consistency | [Pass](artifacts/module-3/runs/20261020T035603.673933Z/evidence-consistency.json): historical [tested inputs](artifacts/module-3/runs/20261020T035603.673933Z/source-identity.json) match; pins and frozen B1 unchanged. |

The [full run record](artifacts/module-3/runs/20261020T035603.673933Z/verification.json) contains exact commands, exit codes,
durations and fresh host inventory. Source aggregate SHA-256:
`adf06e6ed1920cc4e790ae9ed48a2c0f7b99cfa86ef2d7e03c62a4f5e9b68b9b`.
The repository is still unborn on `historical/module-3`; file hashes identify this tested
checkout without inventing a commit. Source reproduction used a temporary export
and local dependency binaries. Scale overrides use one Retina display, not five
physical monitors. Earlier Module 0–2 research/evidence and the original Module 3
closeout remain preserved. No agents were delegated work.

An initial post-fix visual assertion failed because it located the pointer before
the newly contextual form finished resizing the viewport. The [captured preview](artifacts/module-3/interaction-followup/circle-layout-diagnostic.png)
showed the correct 5 mm radius. Waiting for the layout before mapping model
coordinates corrected the harness; the original [failure log](artifacts/module-3/interaction-followup/circle-green-initial.txt)
and the subsequent [passing run](artifacts/module-3/interaction-followup/circle-green-layout.txt)
are retained. No product behavior was bypassed and no production test hook was added.

## Planned layout and remaining gates

At the user's explicit request, [TECH_SPEC 4.0a–c](../TECH_SPEC.md#40--align-the-native-modeling-workspace-before-adding-solid-tools)
now schedules Features/Sketch/Evaluate command tabs, the left PropertyManager/tree
switch, a dominant central viewport, optional right Task Pane, bottom document/edit/
units/save/job status, and native layout/workflow acceptance before 4.1a exact
features. Existing feature task IDs are preserved. Module 9 completes cross-document
consistency, familiar/simplified modes and accessibility. SDD §10, traceability,
README and future evidence placeholders point to these owners. All new future
tasks remain unchecked and unimplemented; authorization still stops before Module 4.

Windows/Linux native builds/UI/filesystem checks, Intel/minimum macOS/additional
GPUs, installed packages/signing, controlled OS-network-disabled execution,
assistive technology and full physical 8 GB B0–B5 resource-budget acceptance remain
**Blocked**, with owners and limitations in the [main evidence](module-3.md#host-blockers-and-unresolved-decisions).
One Mac or headless passing tests cannot close those gates. The available host is
an Apple M2 with 8,589,934,592 bytes physical RAM; no FPS/memory/p95/launch budget is
inferred from these correctness checks. No Module 4 implementation was performed.

Later scope note: the [22 October all-arc follow-up](module-3-all-arcs.md) supersedes this historical run's unsupported arc-mode statements; its original evidence is preserved.
