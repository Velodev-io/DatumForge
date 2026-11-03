# CM5.a — contour and solid-feature intent contracts

3 November 2026. Authorized by “proceed with cm5.a”; branch
`review/cm5-a-20261103`, base `1b51191`.
[Contract](../decisions/core-modeling-cm5-intent.md),
[plan](../superpowers/plans/2026-11-03-cm5-a.md).

## Production scope

The kernel-free feature-model library defines an immutable version-1
`features::ModelingIntent` API. It validates contour/region descriptors, separate
extrude starts/ends, independent second direction and draft, reverse, thin
extrude/revolve, solid/revolved cuts, open-cut side and standalone neutral-plane
draft. It exposes all typed dependencies to the CM4 graph and requires complete
history, correct Sketch/Feature source kinds, member identities and local scope.
`graphNode()` is a dependency projection; complete admission uses `graph()`.

Serialization round trips preserve SI values and explicit selection/reference
intent. These are in-memory CBOR round trips, not saved-project or undo/reopen
evidence. No new accepted feature kind, compatibility manifest, command, geometry
adapter or functioning UI control is added. The existing accepted feature parser
continues to refuse these definitions. CM5.b–e remain unimplemented by this task.

## Red/green evidence

All logs are under [the task artifact folder](artifacts/core-modeling/cm5-a-20261103/).

| Behavior | Red | Green |
| --- | --- | --- |
| Initial contract API and legacy refusal | `api-red.txt`: missing header | `api-green.txt` |
| Start/end, second direction, revolve, thin and draft modes | `modes-red.txt`: draft angle not yet supported | `modes-green.txt` |
| Selected regions, open cuts, profile admission | `selection-red.txt`: AllClosed-only rejection | `selection-green-graph-red.txt`: both mode/profile groups pass |
| Complete dependency history and source-kind validation | `selection-green-graph-red.txt`: missing-history rejection absent | `graph-green.txt` |
| Self/explicit-current-document alias for draft faces | `scope-alias-red.txt`: lexical scope comparison rejects valid alias | `scope-alias-green.txt` |

The first expanded test build had an `auto` declaration with two different types;
`modes-red-build.txt` retains that compiler failure, and
`modes-red-build-corrected.txt` records its correction before the behavioral red.
No compile failure is presented as a behavioral result.
Trailing horizontal whitespace emitted by Ninja in the two compiler-failure
logs was trimmed for repository whitespace checks; diagnostic content is retained.

The contract corpus covers all registered parameter families, finite/range/type
and inactive-field refusal, malformed profile descriptors, 256-use and 32-region
admission edges, open/closed thickness policies, independent 10 mm / 35 degree
and 15 mm / zero draft values, one-turn revolve bounds, same-sketch reuse, value
immutability, missing/wrong-kind members, scope aliases, cross-document refusal,
upstream closure, cycles and order. These are structural/semantic tests: exact
connectivity, planar/straight/face/body membership, branch resolution, solid
validity and the actual drafted course solid require CM5.b/e.

## Verification

The [final verification folder](artifacts/core-modeling/cm5-a-20261103/verification/20261103T093035.623376Z/)
contains the exact commands, logs and machine-readable result:

- Full development build: Pass.
- Full enabled CTest: **90 passed, zero failures**, 426.53 seconds. The deliberate
  harness failure probe remained Disabled and is not counted as passed.
- Pinned clang-format 21.1.8 and both changed-source Clang analyses: Pass.
- UI/worker ownership, research isolation, coverage, specifications and whitespace:
  Pass. Final documentation/task registration is checked separately.
- Source integrity: all **263** frozen source/configuration hashes matched.

CM5.a production semantic contracts and available-Mac development-build
verification are **Pass**. All 29 whole-workflow coverage rows remain In progress.

The sandbox IPC preflight failed in unchanged worker-start/lifecycle cases;
`ipc-preflight.txt` preserves the result. Full worker/IPC verification requires
host IPC permission. Native CM5 interaction is not applicable to CM5.a because
this task adds no UI or worker endpoint. New engine/persistence/native checks
remain Not run for CM5.b–e; full course/platform/device/installed/physical 8 GB
resource acceptance remains Blocked. Preserve 6.1b/6.4c/7.4c; Module 11 is not
started. Final review is author self-review, not independent merge approval.

[Author source review](artifacts/core-modeling/cm5-a-20261103/source-review.md)
records the parser/graph/scope checks. No source changes followed the final run.
