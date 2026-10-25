# Module 5 dependencies, history and reference repair

Date: 20 October 2026. **Status: all feasible available-Mac implementation and verification complete;
full platform/device/resource acceptance Blocked.** Module 5 was explicitly
authorized by the user's “yes” after the Module 4 closeout. Stop before
**Module 6 / 6.1a**. [Authorization](../decisions/implementation-scope.json).

The existing Module 3 interaction repairs and Module 4 workspace/features remain
in place. Historical reports, research and test runs are preserved. The prior
[Module 5 publication snapshot report](artifacts/module-5/preexisting-module-5-report.md)
and [publication checks](../PUBLISHING.md) describe an earlier incomplete snapshot;
they are not the final implementation's verification. Concurrent user publication
changed the current branch/commit and configured the remote. This work preserves
those changes and performs no reset, push or history rewrite.

## Implementation and task scope

[Evaluation decisions](../decisions/module-5-evaluation.md) describe semantic
schemas, graph edges, fingerprints, cache ownership, stale checkpoints, explicit
repair and supported multi-document dependencies.

| Task | Verified available-Mac scope |
|---|---|
| 5.1a | Explicit feature/profile/expression/reference graph, affected descendants, cycle/forward-order refusal, versioned fingerprints and deterministic order. |
| 5.1b | Rename, suppress/unsuppress, dependency-valid reorder and explicit cascade delete; worker impact preview; acknowledged stale history preserves last-valid display. |
| 5.2a | Actual OCCT history and constrained role matching; direct/history/split/merge/deleted/missing/candidate/ambiguous outcomes remain distinct. |
| 5.2b | Left History/repair controls show owner, former role, affected dependents, candidate metadata and highlighted geometry; replacement is explicit, revision/fingerprint validated, deduplicated and undoable. |
| 5.3a | Immutable 128-entry / 256 MiB estimated result cache, stale exact-measurement refusal, trimming without loss of semantic/undo/recovery authority. Exact export belongs to the still-unstarted Module 6 and must retain the valid-revision gate. |
| 5.3b | Frozen 100-feature B2 history, project-local cross-document sketch consumers, semantic edit/undo/redo, checkpoint save/reopen and isolated worker-kill recovery. |
| 5.4a | Upstream edits, suppression/delete/reorder, expression and feature cycles, exact affected sets and reuse of an unchanged independent branch. |
| 5.4b | Actual kernel split/merge/deletion, removed pattern copies, shifted edge candidates and symmetric ambiguity without arbitrary reassignment. |
| 5.4c | B2 correctness/cache subset implemented. Full NFR-009 live-object/physical-memory proof and NFR-012 platform/engine acceptance remain Blocked. |

Only the serial worker owns exact geometry and project writes. UI models are
semantic/display projections and meshes. Result-cache entries contain feature
effects; non-destructive Booleans and copied mesh inputs protect shared shapes.
Schema-v1 features remain readable; schema-v2 suppression/expressions and the
history compatibility manifest prevent older workers from silently dropping data.
Failed preview does not commit. Acknowledged failed history stores semantic
last-valid checkpoints and conspicuous stale status; exact measurement refuses it.

## Reproduction

```sh
python3 tools/verify_module5.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

The [verification record](artifacts/module-5/verification.json) links an immutable
run directory containing exact commands, exit codes, durations and host details.
All roots are marked disposable and release/unmarked recovery paths are refused.
Native tests create their own windows and workers, without restarting or modifying
an existing user session. CTest's offscreen tests do not certify graphics; Cocoa
workflows are recorded separately at five effective scales on one Retina display.
Clean reproduction exports current source into a fresh temporary root and uses
verified cached archives and existing pinned dependency binaries. It does not
claim a second from-scratch dependency build or an installed release.

## Actual final verification

Final run: `runs/20261020T150233.569375Z`. Host: macOS 26.6.2 arm64, Apple M2,
**8,589,934,592 physical RAM bytes (8 GiB)**. The tested checkout is commit `d1fce435b1e9e4f05b7f36fab6392f870fedd8d7` plus the dirty paths and individual hashes in [source identity](artifacts/module-5/runs/20261020T150233.569375Z/source-identity.json), on `feature/model-evaluation-wip`. Aggregate source SHA-256: `cd38923bb162df567e027b7beb8a86d2dd0ef30c75fcb643140514869398ba25`. [Consistency](artifacts/module-5/runs/20261020T150233.569375Z/evidence-consistency.json) confirms the tested inputs and unchanged dependency/tool/Qt-patch and frozen B1/B2 pins. No commit or push was performed by this verification.


| Verification | Actual result | Evidence |
|---|---|---|
| Configure/build | Pass | [configure](artifacts/module-5/runs/20261020T150233.569375Z/configure.txt), [build](artifacts/module-5/runs/20261020T150233.569375Z/build.txt) |
| Full enabled CTest | 36 passed, zero failures; 137.24 seconds | [CTest log](artifacts/module-5/runs/20261020T150233.569375Z/ctest.txt); deliberate failure stays Disabled |
| Format / analyzer | Pass; no analyzer warnings | [format](artifacts/module-5/runs/20261020T150233.569375Z/format.txt), [analyzer](artifacts/module-5/runs/20261020T150233.569375Z/static-analysis.txt) |
| Ownership / research | Pass; desktop excludes exact geometry/persistence; 97 research hashes unchanged | [linked ownership](artifacts/module-5/runs/20261020T150233.569375Z/architecture.json), [research](artifacts/module-5/runs/20261020T150233.569375Z/research-isolation.txt) |
| Native history / prior workflows | Pass at 100/125/150/175/200%; no failed or skipped native cases | [commands/results](artifacts/module-5/runs/20261020T150233.569375Z/verification.json); source repair and prior Modules 1–4 regressions |
| Clean reproduction | Pass; fresh source build and 36 enabled tests | [details](artifacts/module-5/runs/20261020T150233.569375Z/clean-reproduction-details.txt) |
| Documentation | Pass: 56 requirement rows, 13 modules, 81 evidence-backed checked tasks; changed Markdown links resolve | [specification check](artifacts/module-5/runs/20261020T150233.569375Z/spec-validation.json) |
| Visual review | Pass for reviewed subset | [review](artifacts/module-5/runs/20261020T150233.569375Z/visual-review.json), [100% repair](artifacts/module-5/runs/20261020T150233.569375Z/native-100/history-repair.png), [200% repair](artifacts/module-5/runs/20261020T150233.569375Z/native-200/history-repair.png) |

The final B2 run passed with **100 features, one solid, 105 exact
faces, 14,664 visible triangles and 196,889.82327294615 mm³ initial volume**.
The original fixture SHA-256 remains
`78873aabdd68715b7868b6a08c62871bb57f2fa7ec6bf82930f49df317e37fdb`.
The final B2 executable took 90.057 seconds and recorded 128 entries, 139,354,364 estimated bytes under its 268,435,456-byte limit, 200 computations and 70,000 cache hits. [B2 results](artifacts/module-5/runs/20261020T150233.569375Z/b2-correctness.json) and [retained roots](artifacts/module-5/runs/20261020T150233.569375Z/retained-roots-review.json) record their limits. The test executes 100 edit/undo/redo cycles against real ProjectStore/OCCT results,
compares analytic volume at each step, checks bounded cache accounting, clears it,
regenerates and compares save/reopen/recovery outcomes. Entries/estimated bytes
return to zero immediately after trim. This does not measure total live OCCT
objects, physical footprint, GPU allocations, FPS or full retention budgets.

All five native history runs report effective DPRs 1/1.25/1.5/1.75/2 with OpenGL 4.1, one viewport initialization and zero recorded graphics failures. Scale overrides use one Retina display. Native history coverage uses actual worker commands: impact preview/rename,
suppression with explicit stale acknowledgment, exact-query refusal and Undo,
invalid reorder, explicit cascade delete-to-empty and Undo, unit-aware parent
expression edit causing a missing edge, stale save/reopen, source-candidate
inspection/highlight, explicit replacement, repair Undo/Redo, save and isolated
SIGKILL/recovery. Prior sketch/workspace/exact-feature and document workflows run
again to detect regressions.

## Failures and corrections retained

[Development inventory](artifacts/module-5/dev/inventory.json) preserves failed
and successful logs. Early compile failures involved CBOR/container conversion,
a missing namespace include and combo-box item conversion; these were corrected
before the focused tests passed. The topology corpus exposed generated geometry
being treated as role continuation; resolution now follows modified descendants.
Candidate matching was constrained to exclude geometry already representing a
different former role. These changes retain explicit ambiguity and repair.

The first B2 test failed because its analytic expected-volume accumulator reused
the previous cycle incorrectly. The expected value now derives from the fixed
fixture volume and the final hole's current diameter, with the same tolerance;
the recipe and acceptance bounds were not changed. Native lifecycle tests found
Cancel and reference inspection remaining disabled after earlier commands.
Command begin/finish/discard now reset their own form/button state and invalidate
old proposals. Initial visual review found raw tokens stretching the left dock;
readable role text and token tooltips keep the repair controls usable. Final scaled native interactions and the reviewed 100%/200% screenshots confirm the revised layout for the available display.

## Remaining blockers and exact boundary

| Gate | State / next owner |
|---|---|
| NFR-009 post-trim live-object growth and full physical 8 GB B0–B5 budgets | Blocked; cache estimates and a 100-cycle correctness run are narrower evidence. Module 11 must measure physical/shared/GPU/allocator behavior and complete retention budgets. |
| Full NFR-012 support matrix | Blocked; current pinned engine on one arm64 Mac only. |
| Physical mouse/trackpad ergonomics/preferences | Blocked; automated events do not prove physical-device delivery. |
| Windows/Linux/X11/Wayland, Intel/minimum macOS, additional GPU/display | Blocked; runners/hardware unavailable. |
| Native assistive technology and alternate appearance/contrast | Blocked; current Cocoa visual subset only; Module 9 owners retain acceptance. |
| Controlled OS-network-disabled workflow | Blocked; local-only commands do not establish OS-isolated network acceptance. |
| Installed packages/signing/upgrades, license/distribution decisions | Blocked; existing Modules 10/12 and decision owners retain these gates. |
| Physical power loss/full-volume storage and full conversion matrix | Blocked; isolated SIGKILL/quota tests cover narrower failure cases. |

No Module 6 task or neutral exchange implementation has started. The exact next
unauthorized task is **6.1a: STEP/IGES import**. Ask before proceeding.

## Subsequent Module 7 scope

The user explicitly authorized Module 7 on 21 October 2026 and deferred
Module 6.1b/6.4c for later. The stopping boundaries above describe the historical
run. See [Module 7 evidence](module-7.md) for the new assembly scope; the current
boundary is **Module 8 / 8.1a**. No earlier hardware/platform gate is closed by
that authorization.
