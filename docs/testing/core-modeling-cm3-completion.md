# CM3 completion — implementation and verification

The user approved the [interface and behavior plan](../decisions/core-modeling-cm3-completion-plan.md)
on 31 October 2026. Work is on `review/cm3-completion-20261031`, after `c30de06`.
Production implementation covers the approved current planar interfaces. Available-Mac development-build
verification is **Pass**; full acceptance remains **Blocked**. See the
[interaction contract](../decisions/core-modeling-cm3-interactions.md).

Tests are added one behavior at a time, followed by the minimum implementation
and a green run before selecting the next behavior. Tests exercise public model,
store, worker and native widget interfaces. No private-method mocks or direct SQL
assertions are used by the new corpus. All project roots are marked disposable.

## Recorded cycles

Evidence is under [the TDD folder](artifacts/core-modeling/cm3-completion-20261031/tdd/identity-red.txt).

| Behavior | Red evidence | Green evidence | Result |
| --- | --- | --- | --- |
| Label edits retain dimension identity and the other driving constraint | `identity-red.txt` | `identity-green.txt` | Pass; existing editor regression also passed |
| SI increment and drawing-mark metadata survive public save/reopen and undo/redo | `metadata-red.txt` | `metadata-green-final.txt` | Pass |
| Unit-aware mm/in spin increments preserve expressions until an explicit step | `increment-ui-red.txt` | `increment-ui-green.txt` | Pass |
| Explicit drawing-import mark changes no geometry | `drawing-mark-red.txt` | `drawing-mark-green.txt` | Pass |
| Reverse signed components and retain named expressions | `reverse-component-red.txt` | `reverse-component-green.txt` | Pass |
| Reverse aligned Length/Distance without negating their positive magnitudes | `reverse-aligned-red.txt` | `reverse-aligned-green.txt` | Pass |
| Reject a claimed reversal when fixed endpoints prevent it | `reverse-blocked-red.txt` | `reverse-blocked-green.txt` | Pass |
| Reverse centerline diameter side with its fixed axis preserved | `reverse-centerline-red.txt` | `reverse-centerline-green.txt` | Pass |
| Shared-datum multi-target ordinate placement | `ordinate-series-red.txt` | `ordinate-series-green.txt` | Pass |
| Connected line-chain driving length | `path-lines-red.txt` | `path-lines-green.txt` | Pass |
| Circular arc joins and true length, independently checked with OCCT | `path-arc-red.txt` | `path-arc-green.txt` | Pass |
| Circle, ellipse, elliptical arc, parabola and rational spline path lengths | `path-curves-red.txt` | `path-curves-green.txt` | Pass; independent OCCT checks |
| Closed-path association cannot silently open to satisfy an impossible length | `path-closed-red.txt` | `path-closed-green.txt` | Pass |
| Path selection, worker measurement and driving editor | `path-editor-red.txt` | `path-editor-green.txt` | Pass |
| Temporary point drag targets preserve drivers and durable revision | `drag-protocol-red.txt` | `drag-protocol-green.txt` | Pass |
| Pointer release and one draft undo step | `drag-pointer-red.txt` | `drag-pointer-green-stationary.txt` | Pass; intermediate failures retained |
| Worker drag targets for lines, all current curve families, rectangles, slots and constant-length paths | `drag-families-red.txt` | `drag-families-green.txt` | Pass |
| Pointer body dragging, last-valid retention and Escape | `drag-curve-pointer-red.txt` / existing behavior | `drag-curve-pointer-green.txt`, `drag-last-valid-regression.txt`, `drag-cancel-regression.txt` | Pass; last-valid/cancel regressions started green |
| Seven course relation families, deterministic bounded worker proposals | `inference-red.txt`, `inference-families-red.txt` | `inference-green.txt`, `inference-families-green.txt` | Pass |
| Exact conic/spline point contacts and line tangency | `inference-curves-red.txt`, `inference-curve-tangent-red.txt` | `inference-curves-green-interior.txt`, `inference-curve-tangent-green-seeded.txt` | Pass; failed initialization attempts retained |
| Explicit inference review, Cancel and confirmation | `inference-confirm-red.txt` | `inference-confirm-green.txt` | Pass |
| Automatic relations toggle and unconfirmed visible/accessibility guides | `inference-automatic-red.txt` | `inference-automatic-green.txt` | Pass; disabling adds no constraints |
| Fully Define preserves existing line drivers, shape and deterministic IDs | `fully-define-line-red.txt` | `fully-define-line-green.txt` | Pass |
| Selected scope leaves unrelated geometry movable | `fully-define-selected-red.txt` | `fully-define-selected-green.txt` | Pass |
| Circle, circular arc, ellipse, elliptical arc and parabola definition | corresponding `fully-define-*-red.txt` | corresponding `fully-define-*-green.txt` | Pass; one family implemented per cycle |
| Spline and rectangle/straight-slot/curved-slot construction preservation | existing behavior | `fully-define-groups-regression.txt` | Pass; started green |
| Fully Define review scope, remaining freedom, Cancel/Apply in mm/in | `fully-define-review-red.txt` | `fully-define-review-green.txt` | Pass |

| Checked spline-tangent subset survives normal preview/commit | `inference-recheck-red.txt` | `inference-recheck-green.txt` | Pass |
| Unique general curve-pair contacts and ambiguous-contact refusal | `inference-curve-pair-red.txt` | `inference-curve-pair-green.txt`, `inference-curve-pair-families.txt`, `inference-ambiguity.txt` | Pass |
| Stored path rejects a revisited branching junction | `path-branch-red.txt` | `path-branch-green.txt` | Pass |
| Worker epoch discards stale drag drafts | `drag-restart.txt` | `drag-restart-green.txt` | Pass |
| All four editor types invalidate stale staged drafts on restart | `proposal-restart-red.txt`, `editors-restart-red.txt` | `proposal-restart-green.txt`, `editors-restart-green.txt` | Pass |
| Identity repair survives durable undo/redo and saved reopen | existing repaired behavior | `identity-history-reopen.txt` | Pass; started green |
| Composite schema-8 history, duplicate replay, commit boundaries, recovery and portable copy | existing persistence behavior | `completion-persistence.txt` | Pass; started green |
| Origin-based vertical driving/reference ordinates and keyboard scope/Cancel/Apply | existing implemented behavior | `ordinate-origin-and-keyboard-regression.txt` | Pass; started green |
| Escape after release while a drag request is pending | existing implemented behavior | `drag-released-cancel-red.txt` | Pass; despite the attempt filename this regression started green |

| Non-first path-member deletion, group protection and trim disclosure | `path-member-delete-red.txt`, `path-group-red.txt`, `path-trim-red.txt` | corresponding `*-green.txt` | Pass |
| Delete selection cannot interrupt command availability refresh | `legacy-shape-diagnosis.txt` | `legacy-shape-green.txt` | Pass; missing selection handling repaired |
| Worker drag lifecycle and durable radius preservation in existing workspace | `drag-feedback-red.txt` | `drag-feedback-green-final.txt` | Pass; preview state/feedback retained |

`metadata-green.txt` retains a failed test-fixture attempt: the saved file was
incorrectly placed inside private recovery storage and was correctly refused.
The fixture was corrected to save to a separate marked disposable directory;
the production containment rule was preserved.

The pointer green attempts and diagnosis are retained. A properties-pane layout
reflow moved the canvas by 280 pixels. Release now compares the last physical
pointer position, avoiding a second target from unchanged physical input. The
test also releases at the same global cursor coordinate after layout reflow,
rather than reusing an obsolete local pixel coordinate. Original functional
assertions, including the exact driver and one undo step, were retained.

Schema 8 and the sticky `sketch-interactions=1` manifest carry new dimension
metadata. Existing sketches continue to select their older encodings when they
have no new semantics. Final compatibility and failure matrices remain to run.

## Remaining work and acceptance

The checked-proposal, persistence/recovery and restart edge cases above pass.
The final available-Mac results are recorded below. No CM3 lettered task or
coverage row claims full acceptance from these development-build checks. Existing hardware/platform/installed
and resource blockers remain separate. Preserve 6.1b/6.4c/7.4c; Module 11 is not
started. No merge or push.

## Retained complete-suite failure and contract adjustments

The first final run, `verification/20261101T101630.278301Z`, retained 73 passing
and three failing enabled entries (76 enabled; one deliberate failure disabled).
It stopped before static/native verification. The failures were investigated:

- The shape test asserted no worker command during a drag. That assertion was
  superseded by the explicitly approved worker-solved interface; it now verifies
  one-running/one-pending coalescing, completed worker previews, unchanged durable
  revision/accepted geometry and the original no-list-churn checks. Latency
  telemetry records observed preview results and queue depth, not an invented
  zero-command count. Its later deletion step exposed a real stale-selection
  availability exception, fixed in production.
- The draft test expected an immediate local hit position. It now waits for the
  worker and checks the same target in sketch coordinates after selection layout
  settles. Geometry/history assertions remain.
- The compact workspace test clicked an Apply button outside the scroll viewport
  and assumed radius-driven circles could never move. It now scrolls the actual
  enabled control into view and checks the approved worker behavior: the centre
  may move, while radius, driver identity/expression and durable publication are
  preserved. It also exposed the completed-preview state/feedback reset. That
  production lifecycle issue is repaired; successful drags retain PreviewReady
  and visible feedback, including keyboard-Finish eligibility.

The legacy assertions were updated for the approved asynchronous interface, not
removed to hide geometry or persistence failures. Failed outputs and intermediate
attempts remain alongside their final focused results.

The second run, `verification/20261101T105050.781372Z`, passed all **76 enabled
CTests in 361.74 seconds** and pinned formatting. Changed-source analysis stopped
on a nullable test-fixture handle after failed setup; explicit fixture guards
then passed focused Clang analysis (`tdd/fixture-analysis.txt`). Native checks did
not run in that attempt. The source identity and failure report are retained.

A public-model admission probe (`tdd/path-equation-budget-probe.cpp`) admitted
1040 expanded path equations under the old object-count limits. The permanent
`expandedPathBudget` case now verifies 975 admitted / 1040 rejected by the explicit
1024-equation guard before any solver allocation. Red/green evidence is
`path-equation-budget-qtest-red.txt` / `path-equation-budget-green.txt`. The probe
does not allocate the oversized solver or claim a memory/latency measurement.

The combined focused rerun also exposed the release/Escape race in
`tdd/focused-final.txt`. `cancelGesture` now restores the pregesture draft even
after physical release while a worker request remains pending; the held and
released cases pass in `tdd/drag-release-escape-green.txt`. A zero-delay dispatched
Escape avoids adding artificial keyboard delay to that pending-request case.

## Native harness diagnosis and retained failures

`verification/20261101T111639.615947Z` passed all 76 enabled CTests in
361.58 seconds, pinned formatting, changed-source Clang analysis, architecture,
research isolation, coverage and specification checks. Its first native 100%
run retained eight fixture-start failures (34 passing cases), without sufficient
stage diagnostics to establish their cause. An unchanged-binary retry passed
the initial restart cases, then timed out in a dimension dialog. The process
sample and all failed outputs remain under that run; they are not passing native
evidence.

Bounded preflights under `preflight-native-100/` traced callbacks entering the
actual `QDialog::exec` loop and returning after the dialog hid, while Cocoa
remained asleep in that loop. A readiness-only helper and an experimental early
production rejection did not resolve the stall. The production experiment was
removed. The test helper now waits for an exposed modal in a deeper event loop
and interrupts the dispatcher after a callback has hidden it, allowing the
owning dialog loop to observe its existing exit state after nested test waits.
It cannot accept a dialog or close a still-visible one. Native readiness and
all four restart cases passed with this harness-only change
(`modal-harness-only.txt`: eight passes); the full native 100% preflight passed
44 cases, zero failures and zero skips in 26.014 seconds (`completion-all.txt`).
Fixture failures now report their precise stage and observed DPR. Existing
geometry, durable revision, cancellation, epoch and history assertions remain.
The final frozen-source verification is recorded separately below when finished.

The next frozen-source attempt, `verification/20261101T123052.015375Z`, passed
all 76 enabled CTests in 341.21 seconds and formatting, then stopped on a Clang
potential leak diagnostic for a copied `std::function` capture in the legacy
dimension-test modal callback. The callback now references the enclosing action,
whose lifetime exceeds its scoped timer; no production code changed. Both
modal-consuming CM3 test files pass focused analysis in
`tdd/modal-analysis-final.txt`. This attempt did not run the native matrix.

`verification/20261101T124037.301140Z` passed the complete 76-test suite
(334.04 seconds), formatting, all changed-source Clang analysis and structural
checks. At 100%, completion (44 cases), identity/native dimensions, workspace
and arc regressions passed. At 125%, completion again passed all 44 cases;
the older dimension workflow then failed waiting for a painted label hit
rectangle (visible/enabled canvas). Its attempted reference conversion therefore
did not occur. That failed output remains retained. The label-hit wait now
requests a real canvas repaint before inspecting paint-generated hit geometry;
it still double-clicks that rectangle and retains all mode/identity assertions.
This changes only the native/widget test fixture, not production rendering.


## Final available-Mac result — 1 November 2026

**Production implementation: Pass for the approved current planar interfaces.**
**Available-Mac development-build verification: Pass. Full acceptance: Blocked.**

- [Full-suite/static baseline](artifacts/core-modeling/cm3-completion-20261031/verification/20261101T124037.301140Z/verification.json):
  all 76 enabled CTests passed in 334.04 seconds; the deliberate-failure probe
  remains disabled. Pinned formatting, changed-source Clang analysis,
  architecture, isolation, page-indexed coverage and specification checks passed.
- [Final native follow-up](artifacts/core-modeling/cm3-completion-20261031/verification/20261101T125243.188910Z/verification.json):
  validates that only the dimension-test repaint changed after that baseline;
  rebuild, both affected widget CTests (10.54 seconds), formatting and affected
  Clang analysis passed. Production inputs match the full-suite baseline.
- Native completion corpus: **44 passing QtTest cases, zero failures/skips at
  each of 100/125/150/175/200% effective scales**. The mm/in dimension corpus and
  label-identity/history/saved-reopen regression passed at every scale.
  Workspace and arc regressions passed at both 100% and 200% endpoints.
- The final follow-up recorded `source_unchanged: true`. Its executable evidence
  script is retained as `tdd/verify-native-followup.py`; all failed attempts remain.
- Inspected native screenshots include Fully Define at 100%/200%, ordinates in
  inches at 100%, and Path Length in inches at 150%. Dialog controls and action
  buttons are visible; long numeric text uses the line edits' horizontal scroll.

The available Mac has **8,589,934,592 bytes physical RAM**. This is host identity,
not evidence for the full 8 GB resource matrix. Physical mouse/trackpad/pen/touch,
assistive technologies, signed/installed builds, Windows/Linux and full
course/resource acceptance remain unrun and Blocked. CM3.a–e and CM-076–097 now
record completed implementation/available-Mac verification separately from that
blocked full acceptance. Historical gates 6.1b/6.4c/7.4c are unchanged; Module 11
was not started. No merge or push.
