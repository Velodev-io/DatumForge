# Smart Dimension interaction parity — execution evidence

Authorized 4 November 2026. Execute [Tasks A–G](../superpowers/plans/2026-11-04-smart-dimension-parity.md)
sequentially. [Page-indexed acceptance](../research/smart-dimension-acceptance.json)
records implementation, focused testing, native verification and acceptance
independently. Historical CM3 verification is preserved unchanged.

## Starting state and Task A

Starting HEAD `2f20c2d48276fc4557b696d361ac36fcb96ca152`, originally on
`review/cm5-c-20261103`. [Starting identity](artifacts/core-modeling/smart-dimension-20261104/starting-state.json)
records all 68 unrelated working files and their SHA-256 hashes. CM5.c is not part
of this change and its incomplete verification is not promoted to a pass.
Task A review branch: `review/smart-dimension-a-20261104`.

Source inspection confirms the existing `sketch.dimension` command is labelled
Dimension and requires a selection. `DimensionUI.cpp` opens the advanced modal
editor immediately; it already supports units, expressions, reference readouts,
identity-based edits and worker previews. `SketchView` draws text and a simple
leader and already retains stable label hit rectangles. Existing mathematics
includes component distances, angular/radius/diameter/sweep, centerline diameter,
ordinates, arc length and exact connected paths. None proves the new interaction.

Task A documentation verification: Pass. Specification and coverage checks passed.
The baseline build passed (282 build steps); initial sandbox UI tests cannot
handshake with the worker. A separate host-IPC baseline is recorded without
overwriting the sandbox attempt.

Task A writes the plan and requirement checklist and clarifies the CM3 status.
The source PDFs remain private; no page image/text is included in public artifacts.
Baseline build and six focused CTests are recorded in this task's artifact folder.

## Implementation and native verification

Task B adds persistent Selecting/Placing/EditingValue state to SketchPanel, the
Smart Dimension label with the existing command identity, a toolbar flyout and
explicit Horizontal/Vertical entry points. Activation does not require selection.
Transient picks highlight and place a candidate through the existing advanced
worker-validated editor. Ordinary operands clear after acceptance; construction
centerline retention remains supported. Escape cancels the unfinished candidate
before exiting; tool/context/revision/worker changes invalidate transient state.

The new `smartDimensionMode` test covers mm/in activation, preselection, invalid
preselection, placement, repeated acceptance, no geometry drag, two-stage Escape,
tool switching and one undo step per dimension. Existing tests now include a
placement click and explicitly reselect for each new operation, retaining their
identity, geometry, preview/cancel, history and saved-reopen assertions. The
restart case also places before restarting its open editor.

Eight affected CTests passed in 33.35 seconds before the final repeated-dimension
assertions; those new assertions separately passed. Final full-suite verification
is pending. Pinned format, seven changed-source Clang analyses, ownership,
isolation, coverage and specification checks passed. Native mm/in mode workflows
passed at 100/125/150/175/200% effective scales, with screenshots. This verifies
Task B mode behavior, not Task D conventional graphics/compact editor. Retained
failed attempts exposed reference-preselection initialization and obsolete fixture
selection assumptions; corrections preserve the original assertions.

Tasks C–G: Not started. Overall parity remains incomplete.

## Blocked acceptance

Full parity remains open until every required behavior passes. Physical input
devices, assistive technologies, Windows/Linux, installed releases and the full
physical 8 GB resource matrix remain unrun/Blocked. Deferred 6.1b/6.4c/7.4c are
unchanged. Module 11 is not started. No merge or force-push is authorized.
