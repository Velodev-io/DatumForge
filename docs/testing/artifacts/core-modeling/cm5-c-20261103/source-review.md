# CM5.c author source review

3 November 2026. Author self-review; no automatic delegation. Independent review
remains required before merge. Final test/static results are recorded separately.

- Native ownership preserved: only semantic ModelingRecords enters client/UI
  libraries. Exact evaluation, candidate inspection, provenance and writes stay
  in the single worker. No new third-party dependency or functioning native tool.
- New metadata is strict/versioned and bounded. Original support/feature/sketch
  schemas remain readable; reader rank 10 is sticky across undo and other document
  types. Active and retained-history downgraded files are rejected; older source
  overwrites require an upgraded copy, with source-byte preservation tested.
- Definitions stay authoritative: semantic intents and actual Boolean/draft trace
  metadata only, no persisted BRep. Save/recovery/history replay compares complete
  deterministic provenance and unique reference evolution against exact results.
- Omitting intents preserves them in both preview and commit; explicit empty arrays
  remove them. Existing support repair also preserves intents. Accepted CM5-only
  Parts are recognized for display despite an empty legacy feature collection.
- Repair uses complete pending definitions, explicit owner/path, matching revision,
  fingerprint and exact candidate membership. Replacement is validated and every
  descendant rebuilt within the existing atomic set_model transaction. Missing
  and split references preserve last-valid accepted state until explicit repair.
- Per-intent Boolean provenance captures actual publication/draft calls, excluding
  read-only cap-query traces. Target-stage/body membership and output identities
  retain the CM5.b exact-admission checks. Metadata does not expand resource caps.
- File tests use marked disposable roots. No user application restart, real saved
  project mutation, private recovery root or installed/platform acceptance claim.
- Review correction: an omitted-intent preview originally differed from the same
  commit. Retained red/green tests now enforce matching omission/empty semantics.
- Full-suite correction: three legacy SQL manifest-upgrade updates had one extra
  binding after adding the new reader marker. Matching placeholder counts were
  restored; Module 3/5/6 persistence regressions pass, and a new full verification
  snapshot is required. The first partial run remains marked Fail.
- Scoped remaining boundaries: CM5.b geometry restrictions, CM5.d/e native/course
  workflows, full installed/platform/device/resource acceptance. Deferred gates
  6.1b/6.4c/7.4c unchanged; Module 11 not started.
