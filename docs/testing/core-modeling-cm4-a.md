# CM4.a — graph contracts and production integration

1 November 2026. User instruction: “okk go ahead with cm4.a”. Base `c11c3da`;
branch `review/cm4-a-20261101`. [Contract](../decisions/core-modeling-cm4-graph.md)
and [implementation plan](../superpowers/plans/2026-11-01-cm4-a.md).

## Scope

Production code now defines version-1 typed geometry references and a shared,
bounded model dependency graph for sketches, datum planes, datum axes, derived
curves and features. It validates source/member roles, unique identities, document
scope, cycles and explicit history order, including transitive dependencies
through nodes without legacy history ordinals.

The existing feature planner and Pierce solver use this validator. Their legacy
feature-only result and sketch collection formats remain intact. Source-sketch
hashes propagate to consuming feature fingerprints. Cross-Part profile gathering
retains a shared sketch's Pierce dependency closure. Existing SQLite/sketch
schemas and compatibility manifests are unchanged; no datum definition is written.

Datum nodes in graph tests are structural production-model inputs, not exact
plane/axis construction. CM4.b–e, all 22 CM4 course workflows and full native/
platform/device/installed/resource acceptance remain open. General external
geometry edges, face attachment, topology evolution/repair, ModelEdit transactions,
datum visibility and Normal To are not implemented by CM4.a. Preserve deferred
6.1b/6.4c/7.4c; Module 11 is not started.

## Red/green evidence

All paths below are under [the CM4.a artifact folder](artifacts/core-modeling/cm4-a-20261101/).
The initial reference and graph API reds are expected missing-header compile
failures, not behavioral failures. Subsequent reds exercise real compiled APIs.

| Behavior | Red | Green |
| --- | --- | --- |
| Versioned reference round trip | `references-api-red.txt` (API absent) | `references-api-green.txt` |
| Selector/role validation | `reference-selectors-red.txt` | `reference-selectors-green.txt` |
| Mixed-kind ordering and descendants | `graph-api-red.txt` (API absent) | `graph-api-green.txt` |
| Identity/source/member/scope validation | `graph-integrity-red.txt` | `graph-integrity-green.txt` |
| Self and mixed-kind cycles | `graph-cycles-red.txt` | `graph-cycles-green.txt` |
| Direct/transitive history violations | `graph-history-red.txt` | `graph-history-green.txt` |
| Bounds and 132-node chain | `graph-bounds-red.txt` | `graph-bounds-green.txt` |
| Missing Pierce source in feature planning | `plan-source-red.txt` | `plan-source-green.txt` |
| Upstream hash invalidation / independent reuse | `source-hash-red.txt` | `source-hash-green.txt` |
| External-profile source closure / durable round trip | `profile-closure-red.txt` | `profile-closure-green.txt` |

`cm4.graph` tests reference parsing, mixed-node structural graphs, ordering,
cycles, malformed inputs, missing roles/members and bounds. `cm4.plan` uses the
production feature planner. `cm4.store` uses the production store/solver/OCCT with
two Parts, a shared Pierce-dependent profile, failed-edit revision retention,
undo/redo and saved reopen; its analytical volume is 4e-6 m³. Storage tests use
marked disposable roots with the existing recovery-path refusal boundary.

## Verification

The full development build passed (`build.txt`). Pinned clang-format 21.1.8 and
whitespace checks passed (`format.txt`). Clang analysis passed for all nine
changed/new C++ translation units (`compile-commands.json`, `static-analysis.txt`).
Ownership passed (`architecture.json`): the UI links no kernel/database/test
runtime, the exact worker links OCCT, and the packaged worker matches.

The sandbox IPC preflight retained worker-start failures in `module1.contracts`
(`ipc-preflight.txt`). The full host-IPC run passed **79 enabled CTests, zero
failures, in 381.40 seconds** (`ctest.txt`); the deliberate harness failure probe
was disabled. All three new CM4 entries and the prior CM1–CM3 tests passed.
Coverage, research isolation and specifications passed. All 216 recorded source/
configuration hashes stayed unchanged through verification (`source-identity.json`,
`source-integrity.txt`). [Machine-readable result](artifacts/core-modeling/cm4-a-20261101/verification.json).
The first final specification check correctly refused a checked CM4.a task before
its evidence registry was updated; `spec-status-registration-failure.txt` records
that mismatch and correction. The subsequent final specification run passed.

CM4.a structural implementation and available-Mac development-build verification
are **Pass**. Native checks for this task are **Not run** (no new interaction was
added); full CM4 acceptance remains open. Offscreen CTests do not certify native
rendering, physical input, another platform or physical 8 GB resource acceptance.

Final review is an author self-review, honoring the workspace's instruction
against automatic delegation. A separate human/independent review is still
required before merging. No merge or push.
