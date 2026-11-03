# CM4.e — native support authoring and associative workflows

3 November 2026. Authorized by “go ahead” after CM4.d. Branch
`review/cm4-e-20261103`, base `8ce8f2e`.
[Contracts](../decisions/core-modeling-cm4-workflows.md) and
[plan](../superpowers/plans/2026-11-03-cm4-e.md).

## Production scope

Edit datums / supports opens a native typed editor for every existing plane/axis
construction family, original sketch attachments and Convert Entities. Sketch,
member, datum and producing-feature choices carry stable typed references.
Exact face/edge/vertex candidates include worker-derived geometry type and center
in document units; the user chooses a replacement explicitly. Original definitions
are staged in a bounded complete semantic model, previewed by the worker and
committed atomically. Generated members can be declared by staging a successful
conversion preview and then selected by subsequent external conversions. No display
ordinal, nearest-shape matching, tessellation or saved-frame fallback is used.

Native feature and sketch edits of supported Parts now submit the complete original
support model. Unchanged evaluated descendants retain original base definitions;
attached sketch frames remain association-owned. Converted members are read-only
in the ordinary sketch editor, and source edits rebuild the conversion chain.
Feature/sketch draft repair opens the same native editor with the pending complete
draft so an upstream change and explicit reference repair can commit together.
`ResolveSupportReference` retains revision/fingerprint/candidate checks.

Changing controls invalidates the last preview. Apply requires the unchanged
successfully rebuilt draft; Cancel changes no accepted revision. Epoch/document/
revision changes retire the editor. Preview presents exact frames/member counts and
volume while keeping the accepted display; it does not display a newly committed
solid before Apply. All solving, geometry and persistence stay worker-owned.

## Verification and retained diagnosis

`cm4.workflow_contracts` exercises all 16 native datum construction modes, multiple
planes, direct planar-face/datum sketch attachment, exact face-boundary projection,
0.75 scale/rotation, a declared external conversion chain, original-source editing,
upstream support changes, analytic descendant volumes, explicit split-face repair,
undo/redo, saved reopen, cancellation, changed-preview refusal and stale revision
retirement. Native cases use marked disposable recovery and separate saved-file
roots; release/private recovery destinations remain refused. Existing CM4.a–d,
engine/transport/recovery/tamper/forged-repair tests remain enabled.

- Initial compiler refusals identified Qt/C++ name collisions (`data`, `refresh`),
  a conditional CBOR-key overload and fixture type errors; these were corrected.
- `diagnosis/workflow-first.txt` retains the initial run (trailing log whitespace
  normalized to satisfy repository diff checks): 16 datum cases passed,
  then an unhandled UUID error followed a refused fixture save into recovery
  storage. Separate marked saved-file roots and explicit save/open assertions
  correct the fixture; production destination guards were preserved.
- The next focused run found the Convert Entities picker incorrectly defaulted a
  curve to its Start point. Whole-curve selection is now the conversion default;
  point-requiring datum modes keep explicit endpoint/parameter behavior.
- `diagnosis/conversion-repair.txt` retains the failing conversion regression and
  passing native split repair. A probe against the pre-link executable is retained
  as `conversion-fixed.txt` and is not counted as validation of the new build.
- `conversion-rebuilt.txt` then reached source editing. The interaction fixture
  clicked Finish before the exact preview drained; it now waits for readiness and
  asserts editing ended. Production sketch preview installation also now updates
  only the edited sketch, keeping regenerated descendants out of base-edit drafts.
- `diagnosis/conversion-green.txt` and `diagnosis/workflows-green.txt` retain the
  successful focused regressions: **24 QtTest cases passed** in the full focused
  headless set. Temporary debug instrumentation was removed.
- LLDB stalled during launch; only its identified task-owned debugger/debuggee
  processes were terminated. Test-stage instrumentation isolated the failure.

[Artifacts](artifacts/core-modeling/cm4-e-20261103/) preserve attempts separately.
Reproduce final development/native checks with
`python3 tools/verify_cm4_workflows.py --native` on the provisioned Mac with local
IPC and logged-in desktop access. CTest alone does not certify native rendering.

## Final available-Mac verification

Production implementation and development/native verification: **Pass for the
CM4.e scope recorded above**. The complete execution is preserved in
[the final timestamped folder](artifacts/core-modeling/cm4-e-20261103/verification/20261103T061241.682636Z/).

- Full development build: Pass.
- Full enabled CTest: **89 passed, zero failures**, 433.03 seconds. The deliberate
  failure harness remained Disabled and is not counted as passed.
- Pinned formatting and 11 changed-source Clang analyses: Pass.
- Native UI/worker ownership, research isolation, coverage and specification
  checks: Pass. The final documentation/task registration is checked separately.
- Native Cocoa workflows: **24 QtTest cases passed at each of five effective
  scales (100/125/150/175/200%)**, covering all 16 supported datum construction
  modes plus mm/in upstream/history/reopen, conversion chaining, split repair,
  cancellation/stale draft and direct face attachment/projection workflows.
- Existing native datum and workspace regressions passed at 100% and 200%.
- Screenshots reviewed: corrected 100% repair controls, final 125% inch upstream
  editor, final 200% explicit split repair and exact face-boundary conversion.
- Source integrity: all 259 frozen source/configuration hashes matched. The host
  reports physical 8 GiB RAM; this does not certify full resource workloads.

[Machine-readable verification](artifacts/core-modeling/cm4-e-20261103/verification/20261103T061241.682636Z/verification.json)
records exact commands, durations and acceptance boundaries. No source changes
were made after this final run.

Full source-course/platform/installed/device/assistive/physical-8-GB resource
acceptance remains **Blocked**; page-indexed CM4 rows stay In progress. No whole
CM4 course percentage or complete product acceptance is inferred. Preserve deferred 6.1b/6.4c/7.4c;
Module 11 and CM5 are not started by this task.

The 100% native layout rerun passed both explicit repair and face-boundary
projection after widening the form fields. The replacement description now shows
its geometry type and full center coordinates. A prior attempt to run that layout
check was not executed: automatic approval review reported an account usage limit;
the user's subsequent “continue” resumed the authorized work. The retry ran through
normal approval review and passed; no approval check was bypassed.

[Source review](artifacts/core-modeling/cm4-e-20261103/source-review.md) records the
final author review and scope rulings. It is not independent review or merge
approval. The accepted mesh remains displayed during exact draft preview; planar
interaction previews keep their existing scope until strict model admission.
