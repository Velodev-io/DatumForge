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

Full available-Mac development-build verification is in progress. The final runner
records enabled CTest, pinned format, twelve changed/affected-source Clang analyses,
UI/worker ownership, research isolation, coverage/specification and source hashes.
No final full-suite pass is inferred before those commands complete.

The first full run is retained in
`verification/20261103T170926.949891Z/` with status Fail. It was stopped after
legacy Module 3 persistence reported `column index out of range`, followed by
related interaction failures. The new reader marker had been added to three
SQL binding lists without matching placeholders in sketch/history/import updates.
All three statements were corrected. `manifest-bindings-green.txt` records the
affected Module 3/5/6 persistence tests passing before a fresh full run. The partial
run is not a completed suite or a platform/gesture-repeatability result.

## Remaining acceptance

CM5.b's documented geometry restrictions remain unchanged. New native authoring,
manipulators and complete course workflows remain CM5.d/e. Windows/Linux, installed
releases, physical devices/assistive technology and full physical 8 GB workload
acceptance remain Blocked/unrun. No new resource-capacity claim. Preserve deferred
6.1b/6.4c/7.4c and stop before Module 11. Author self-review is separate from the
independent review required before merge.
