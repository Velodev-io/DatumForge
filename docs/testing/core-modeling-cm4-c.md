# CM4.c — durable support definitions and explicit repair

Work began 1 November 2026; continued 3 November. User authorization:
“go ahead with CM4.c”, followed by “continue”. Branch
`review/cm4-c-20261101`. [Contracts](../decisions/core-modeling-cm4-persistence.md)
and [plan](../superpowers/plans/2026-11-01-cm4-c.md).

## Production scope

`SetModelSupports` commits original versioned datum/attachment/conversion/base
sketch/feature definitions and mixed history with evaluated sketches and exact
reference-evolution evidence. One semantic delta owns both collections, including
the inverse before supports existed. The worker remains the sole exact-geometry
owner and writer. The existing SQLite schema is unchanged; the sticky
`;model-supports=1` manifest makes older readers refuse the extended semantics.
Undo and edits to other document types cannot downgrade retained requirements.

Open/recovery independently rebuild the original definitions and compare both
evaluated sketches and reference evidence. Saved evaluated frames are never the
source for support reconstruction. SaveAs and portable closure retain the model.
Project state exposes `model_supports` separately from evaluated sketches.

Automatic reference evolution uses unique direct signatures or a one-to-one,
possibly multi-step Modified kernel-history lineage at the producing feature
stage. Splits, merges, duplicate signatures, deletions and missing results require
an explicit replacement. Original source references retain their semantic identity;
the independently recomputed evolution record identifies the current topology.
There is no nearest-shape, ordinal or frozen-frame substitution. Changes without
provable history, including newly shaped source vertices, require explicit repair.

`SupportReferenceCandidates` inspects a datum/conversion/attachment topology
operand at its exact history position. `ResolveSupportReference` checks project
revision, model fingerprint, operand slot and membership in current exact typed
candidates, then atomically rebuilds the complete replacement model. Candidate
inspection has a 256-candidate reply bound; ordinary exact lookup retains its
existing geometry admission. Missing source objects or generated sketch members
are reselected explicitly in the complete typed `SetModelSupports` definition.
Face-boundary generated members without provable correspondence are not guessed.
Loose feature/sketch edits to supported Parts are refused to preserve associations.

## Verification coverage

- `cm4.support_store`: real OCCT/PlaneGCS/SQLite commit, idempotent replay, revision
  refusal, original-definition reopen/recovery, undo/redo, moved supports, real
  cut-face evolution, explicit split-face repair, forged/stale repair refusal,
  conversion at 0.75 scale, principal axis/multiple planes, SaveAs/closure and
  tampered-definition/downgraded-manifest refusal. Analytic volumes and frames
  are asserted independently of the persistence implementation.
- `cm4.support_evolution`: real OCCT one-to-one cut and split-face history;
  multi-step lineage and merge contract cases; missing references fail.
- `cm4.support_worker` / `cm4.support_transport`: real durable commands,
  cancellation immediately before commit, prior-state undo, restart/recovery,
  typed state publication and explicit repair over the process/socket/client.
- Existing CM4.a/b and prior-module tests remain part of the enabled full suite.

Artifacts: [CM4.c execution folder](artifacts/core-modeling/cm4-c-20261101/).
Reproduce with `python3 tools/verify_model_supports.py` on the provisioned Mac,
with local socket access. Tests use marked disposable roots and refuse release
recovery paths. No new native datum controls are added in this task.

## Retained failures and diagnosis

- `store-red.txt` records the expected unimplemented durable command failure.
- `evolution-red.txt` ran before its executable was available and is **Not run**,
  not a behavioral red. `evolution-compiled-red.txt` exercises the actual engine:
  single-step lookup failed the two-step lineage assertion. Transitive unique
  lineage fixes that unchanged assertion; the compiled green is retained.
- The first persisted split fixture cut completely through the body, violating
  the pre-existing single-body Cut contract before reference resolution. The
  diagnosis is retained. A half-depth groove splits the top face while preserving
  one solid; its independently derived volume is 0.0000122 m³.
- The first complete suite contains numerous very long wall-clock timeout and
  worker-readiness failures during an interrupted host run. Its original log is
  preserved as `ctest-first-attempt.txt`; these are not passing evidence.
- That suite also exposed a reproducible regression: matching every `supports`
  field upgraded existing slot construction groups to the new manifest. The
  detector now matches a typed model collection, preserving the old slot manifest.
  `manifest-regression-red.txt` retains the existing test failure before the fix.
- Existing duplicate static-library linker warnings remain visible in build logs.
- `cm3-intermittent-attempt/` retains a full run with a failure in the unchanged
  `Completion::fullyDefineGroups(Slot)` exact-response repeatability assertion.
  The isolated rerun and five further repeats passed (`cm3-completion-rerun.txt`,
  `cm3-repeatability.txt`). CM3 solver/test sources were not changed. This
  intermittent failure is retained as a known regression-suite stability concern;
  a later green run does not establish that its cause is fixed.
- A separate exact-kernel review probe (`review-probe-red.txt`) reproduced two
  additional repair cases: later split history incorrectly affected an earlier
  stage's direct reference, and an attached conversion consumed its missing
  frame before operand inspection. `review-regressions-red.txt` reproduces both through production APIs.
  Stage-prefix history filtering/direct lookup and early attachment inspection
  fix them; `review-regressions-green.txt` retains the focused green run.
  `before-review-repairs/` preserves the earlier 87-test/full-static pass and its
  source identity; it is not evidence for the later review repairs.

## Final status

Final development build and all **87 enabled CTests passed**, with zero failures
in **383.02 seconds**; the deliberate harness failure probe was disabled. Pinned
formatting, Clang analysis of 12 changed/relevant translation units, ownership,
coverage, research isolation and specification checks passed. All **246 source and
configuration hashes** stayed unchanged. Desktop ownership and the packaged
worker match passed. The earlier CM3 repeatability failure remains a documented
concern, despite its later isolated and full-run passes.

CM4.c production implementation and available-Mac development verification are
**Pass**. Native verification is **Not run** and full CM4 acceptance remains
**Blocked**. Consult the
[machine-readable result](artifacts/core-modeling/cm4-c-20261101/verification.json).
Native datum controls/navigation remain CM4.d; complete CM4.e course/native
workflows and installed/platform/device/physical-resource acceptance remain open.
Preserve deferred 6.1b/6.4c/7.4c and stop before Module 11. Author self-review is
separate from the review required before merging; no automatic delegation.
