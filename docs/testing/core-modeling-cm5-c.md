# CM5.c — durable modeling definitions, provenance and explicit repair

3 November 2026. Authorized by “continue” after CM5.b. Base `2f20c2d`, branch
`review/cm5-c-20261103`. [Storage/repair contract](../decisions/core-modeling-cm5-persistence.md)
and [plan](../superpowers/plans/2026-11-03-cm5-c.md).

## Production scope

Saved support collections now admit version-1 `modeling` metadata containing
original CM5 intents and deterministic per-intent Boolean/draft provenance. The
sole exact worker rebuilds original supports and then the intents. Active state,
undo/redo and recovery retain the same definitions, producing-stage/body IDs and
unique reference-evolution evidence. No authoritative BRep is serialized.

Actual Boolean/draft Generated/Modified/Deleted maps are separated from cap-query
history. Every accepted intent has typed provenance; saved/replayed collections
must match exact reconstruction. Missing/ambiguous topology cannot silently resolve
to a display index or nearest face. NewBody profile provenance remains the original
semantic profile selection; it is not mislabeled as a Boolean operation.

The compatibility manifest appends `;modeling-intents=1` (reader rank 10).
Active and retained-history metadata require it; undo never lowers the requirement.
Old projects remain readable, and Save As to a new copy is required before replacing
an older-format source. Unknown metadata versions/fields, duplicate identities,
forged provenance and lowered reader requirements fail without changing source files.

`SetModelSupports` accepts an optional intents array. Omission in both preview and
commit preserves saved intents; an explicit empty array removes them. Existing
support/sketch edits therefore cannot silently discard CM5 geometry. The client
validates accepted modeling metadata and displays the accepted Part even when its
legacy features array is empty. New native creation/manipulator controls remain
CM5.d; no new functioning control is claimed here.

`ModelingReferenceCandidates` inspects the producing stage before a selected
pending intent. `ResolveModelingReference` checks revision, the entire pending
definition fingerprint, a whitelisted typed reference path and current candidate
membership, then rebuilds and commits the full model atomically. Face/Edge/Vertex
topology candidates use exact identities. Explicit source/member/body edits use
the complete-model commit path and require a valid full rebuild.

## Scoped tests and retained evidence

Logs are under [the task artifact folder](artifacts/core-modeling/cm5-c-20261103/).

| Behavior | Red / retained attempt | Passing focused evidence |
| --- | --- | --- |
| Metadata/version/path contract API | `records-api-red.txt` | `provenance-records-green.txt` |
| Per-intent actual Boolean provenance API | `provenance-api-red.txt` | `provenance-records-green.txt` |
| Durable intent payload and history | `store-red.txt` | `store-green.txt` |
| Explicit reference inspection/repair API | `repair-api-red.txt` | `repair-green.txt`, `split-repair.txt` |
| Client/worker repair transport | `transport-red.txt` | `transport-green.txt` |
| Omitted-vs-empty preview semantics | `omitted-preview-red.txt` | `omitted-preview-green.txt` |

Initial API reds are expected missing-header/member compile failures. The first
record fixture had a missing brace, and the expanded transport fixture used an
incorrect mesh accessor and QVariant conversion; the corresponding build logs
retain these corrections separately from behavioral failures.

`cm5.records` validates semantic metadata and explicit path replacement without
mutating input definitions. `cm5.features` checks real Boolean trace ownership.
`cm5.store` uses production SQLite/OCCT with marked disposable recovery/saved roots:
commit/dedup, independent volume equations, upstream changes, failed/canceled edits,
undo/redo, save/reopen, recovery, explicit deletion and preservation by older editors.
It verifies upgrade-copy refusal and unchanged source bytes. Forged output provenance
and lower reader manifests in active data or redo-only history are refused.

`cm5.repair` retains last-valid state for a moved missing face and a genuinely split
Boolean face. It rejects stale fingerprints and noncandidate replacements, commits
only the chosen exact replacement, and verifies provenance/evolution, undo/redo and
saved reopen. `cm5.transport` checks accepted metadata, display publication, recovery,
real candidate/repair requests and history through the client and actual worker.
Existing preview latest-only/cancel/restart checks remain in that corpus.

The final review found and corrected preview/commit asymmetry: omitted intents had
been preserved by commits but omitted by previews. The focused red showed a zero
preview volume for a saved CM5 Part. Both paths now preserve omission and honor an
explicit empty array. Test roots are marked disposable; release/private recovery
paths remain refused. Host IPC is required by transport tests.

## Final verification

The [final timestamped verification folder](artifacts/core-modeling/cm5-c-20261103/verification/20261103T171243.072151Z/)
records the exact commands, logs and machine-readable result:

- Full development build: Pass.
- Full enabled CTest: **97 passed, zero failures**, 432.33 seconds. The deliberate
  harness failure probe remained Disabled and is not counted as passed.
- Pinned clang-format 21.1.8 and all **twelve** changed-source Clang analyses: Pass.
- UI/worker ownership, research isolation, coverage, specifications and whitespace: Pass.
- Source integrity: all **280** frozen source/configuration hashes matched.

CM5.c production durable modeling definitions and available-Mac development-build
persistence/repair verification are **Pass**. All 29 whole-workflow coverage rows
remain In progress. New native workflow verification is Not run.

The first full run is retained in
`verification/20261103T170926.949891Z/` with status Fail. It was stopped after
legacy Module 3 persistence reported `column index out of range`, followed by
related interaction failures. The new reader marker had been added to three
SQL binding lists without matching placeholders in sketch/history/import updates.
All three statements were corrected. `manifest-bindings-green.txt` records the
affected Module 3/5/6 persistence tests passing before the fresh full run.

[Author source review](artifacts/core-modeling/cm5-c-20261103/source-review.md)
records ownership, provenance, upgrade-copy and repair checks. It is not independent
review or merge approval.

## Remaining acceptance

CM5.b's documented geometry restrictions remain unchanged. New native authoring,
manipulators and complete course workflows remain CM5.d/e. Windows/Linux, installed
releases, physical devices/assistive technology and full physical 8 GB workload
acceptance remain Blocked/unrun. No new resource-capacity claim. Preserve deferred
6.1b/6.4c/7.4c and stop before Module 11. Author self-review is separate from the
independent review required before merge.

## Delivery

Feature commit `a97364c` was pushed to `origin/review/cm5-c-delivery-20261114`;
its remote SHA matched the local commit. All 280 source/configuration hashes
matched after publication. No merge or force-push; independent review remains
required before merge.

## Model completeness inventory

To ensure subsequent feature and sketch editors preserve the complete model
without silent omission or data loss, the accepted document collection and
persistence layers enforce strict ownership and boundary invariants:

- **Original base features (`features`):** Legacy feature array containing
  `[id, name, type, parameters]`. Rebuilt deterministically before supports.
- **Support model (`supports`):** Support definitions containing `datums`
  (planes, axes, coordinates), `attachments` (sketch attachments to planes/faces),
  and `conversions` (derived reference geometry).
- **Support evolution (`support_evolution`):** Tracking map recording persistent
  and transient datum/attachment changes across upstream rebuilds.
- **Modeling metadata (`modeling`):** Version-1 dictionary containing
  `{version: 1, intents: [...], provenance: [...]}`:
  - `version`: Integer semantic version (1). Unknown versions fail explicitly.
  - `intents`: QCborArray of version-1 intent maps (extrude/revolve, start/end
    conditions, thin-feature, draft, selected contours, open cuts).
  - `provenance`: Deterministic Boolean and draft Generated/Modified/Deleted
    records produced by the exact CAD worker.
- **Original sketches (`sketches`):** Authoritative sketch definitions containing
  curves, constraints, and dimensions, preserved atomically across model updates.
- **Omission vs. deletion invariant:** Calling `SetModelSupports` with `intents`
  omitted preserves previously saved modeling intents; providing an explicit
  array replaces them (an explicit empty array deletes all intents deliberately).
- **Reference repair invariant:** Candidate queries inspect the exact producing
  stage immediately before the failed intent. Candidate replacement requires current
  revision, whole-pending-definition fingerprint, whitelisted reference path,
  and current candidate membership; dependent features are rebuilt atomically.

## Baseline discrepancy register (a97364c vs. HEAD 37501e8)

Comparing current HEAD (`37501e8`) against the frozen 280-file source identity
of delivery commit `a97364c` confirms exactly 17 modified files from subsequent
Smart Dimension B (`2f95970`) and CM2 editing repair (`40d834c`, `37501e8`):

1. `apps/desktop/DimensionUI.cpp`
2. `apps/desktop/SketchPanel.cpp`
3. `apps/desktop/SketchPanel.hpp`
4. `apps/desktop/WorkspaceChrome.cpp`
5. `src/geometry/occt/SketchCurve.cpp`
6. `src/geometry/occt/SketchCurve.hpp`
7. `src/render/SketchView.cpp`
8. `src/render/SketchView.hpp`
9. `src/sketch/EditHistory.cpp`
10. `src/sketch/LineEditing.cpp`
11. `src/sketch/LineEditing.hpp`
12. `src/sketch/Sketch.cpp`
13. `tests/core_modeling/completion_contracts.cpp`
14. `tests/core_modeling/dimension_contracts.cpp`
15. `tests/core_modeling/editing.cpp`
16. `tests/core_modeling/editing_contracts.cpp`
17. `tests/module4/workspace.cpp`

All 12 translation units of CM5.c remain identical to `a97364c`. On current HEAD,
all 97 enabled CTests pass (126.46s real time), all 12 Clang static analyses pass,
and all specification and isolation checks pass.

## Unresolved prerequisites register

- **Host IPC environment:** Headless client/worker transport tests (`cm5.transport`,
  `cm4.support_transport`, `native.process_transport`) require host IPC permissions
  and succeed outside the sandbox; within sandboxed runners, scoped unit tests pass
  independently.
- **Course sketch authoring:** Complete course workflows (T5, T6, T8) depend on
  authentic sketch inputs; seeded fixtures validate exact modeling independently.
- **Native controls (CM5.d):** On-screen manipulators and interactive FeaturePanel
  controls remain scheduled for CM5.d.
- **Course verification (CM5.e):** Upstream rebuilds and per-variant matrix
  acceptance remain scheduled for CM5.e.
- **Platform/device gates:** Physical 8 GB baseline and other platforms remain
  preserved as Blocked.
