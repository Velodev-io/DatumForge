# Module 10 — recovery, migration, undo and cancellation fault hardening

Date: 22 October 2026. Authorized after Modules 8 and 9; stop before Module 11.
No commit or push. [Recovery contracts](../decisions/module-10-recovery.md) define
operation deduplication, history/object roots, save/migration publication and the
cancellation/commit boundary. The existing Module 6.1b/6.4c and 7.4c records remain
Blocked and are captured in `artifacts/module-10/deferred-gates.json`.

## Verification commands and corpus

```sh
python3 tools/verify_modules8_10.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

- `module10.crash_matrix`: 29 actual SIGKILL cases (27 CAD commit/save and two PDF publication cuts) across exact Part, rigid Assembly
  and associative Drawing documents. Kill before/after compute, before/after
  durable commit, during snapshot copy, after validation, before/after publication
  and after save recording. PDF pre/post-publication kills verify a complete old/new
  PDF and registered staging cleanup. Compare revisions, operation count, volume/pose/sheet
  invariants, current drawing state, prior saved and backup hashes, exactly-once
  retry and staging cleanup. Every recovery query uses a new process.
- Existing `module2.crash_matrix`: 35 local process/filesystem cases, including real
  SQLITE_FULL page quotas, denied destination permissions, symlink/hardlink
  writer aliases, future/missing/corrupt authoritative data and unsupported schema
  conversion refusal. Exact count is taken from the run artifact.
- `module10.hardening`: cancellation at compute/commit cuts, duplicate retry,
  100 edit/undo/redo cycles under five-entry/8 KiB retention, GC protection of
  document/history/job/export objects, source-preserving recognized historical
  manifest upgrade, failed upgrade and truncated database refusal.
- `module10.contracts`: actual worker cancellation, forced restart from a progress
  callback, operation reconciliation, zero abandoned transfer allocation, old
  epoch/session/generation visibility refusal, and migration through file-capability
  IPC. The restart case reproduced a real stale socket-batch race and now protects
  the socket/epoch boundary.

Migration supports known strict schema-v1 historical semantic manifests, on a
copy; no schema-0 or unknown authoritative converter is invented. The source is
pinned and validated; the backup helper reuses an existing read transaction.
The upgraded copy is validated and flushed before exclusive new-name publication.
Original IDs, parameters and original source bytes are preserved.

Focused exact crash matrix and restart fixes have passed; final available-Mac verification is recorded below. Development failures and fixes remain under
`artifacts/module-10/dev`, including corrected fixture root/field errors and the
pinned-read backup transaction failure. Tests use marked disposable roots outside
saved destinations and refuse release recovery paths.

## Unrun acceptance

Mac local files/processes are the executable scope. Physical full-volume exhaustion,
storage power-loss, Windows/Linux/native filesystem variants, installed builds,
minimum macOS, physical device and assistive-technology acceptance remain Blocked.
The available-Mac tests do not prove unique shared/GPU footprint, full resource
budgets or OCCT live-object plateau. Module 11 remains unstarted. Uncertain
post-publication flush outcomes remain explicit and require inspecting durable
state rather than blindly replaying a new operation ID.


## Final available-Mac verification

Run `20261022T031119.905623Z` on macOS-26.6.2-arm64-arm-64bit-Mach-O / Apple M2 /
**8,589,934,592 physical RAM bytes (8 GiB)**. Branch
`feature/modules-8-10-drawings-hardening`, commit `d1fce435b1e9e4f05b7f36fab6392f870fedd8d7` plus all preserved dirty inputs.
Aggregate tested source SHA-256: `eb6225b47536ec9bcc9a864d10dc132b55de935857c2a9ea831cb26e8d13b0de`.

[Verification](artifacts/module-10/runs/20261022T031119.905623Z/verification.json),
[source identity](artifacts/module-10/runs/20261022T031119.905623Z/source-identity.json),
[complete CTest](artifacts/module-10/runs/20261022T031119.905623Z/ctest.txt),
[architecture](artifacts/module-10/runs/20261022T031119.905623Z/architecture.json),
[offline](artifacts/module-10/runs/20261022T031119.905623Z/offline/offline.json),
[visual review](artifacts/module-10/runs/20261022T031119.905623Z/visual-review.json), and
[clean reproduction](artifacts/module-10/runs/20261022T031119.905623Z/clean-reproduction-details.txt)
record exact commands and outcomes. **All 52 enabled CTest cases passed**; the
intentional failure probe remains Disabled. Configure/build, formatting/static
analysis, ownership/research isolation, verified network-denial workflows and
frozen corpora, five-scale Cocoa drawing/keyboard checks, prior native Modules 1–7
and fresh source application reproduction passed. Dependency archives/pins and
existing dependency binaries were reused; dependencies were not rebuilt and no
installed release is certified.

[Actual B5 PDF](artifacts/module-10/runs/20261022T031119.905623Z/module8-B5.pdf) and its
[PDFKit inspection](artifacts/module-10/runs/20261022T031119.905623Z/pdf/pdf-inspection.json)
are separate from screenshots. [Exact CAD crash matrix](artifacts/module-10/runs/20261022T031119.905623Z/module10-fault-matrix.json)
has 29 SIGKILL cases (27 exact CAD and two PDF publication cuts); [prior storage matrix](artifacts/module-10/runs/20261022T031119.905623Z/module2-fault-matrix.json)
has 35 cases. Earlier development failures and superseded verification runs are retained. No failed/unrun check is promoted to full acceptance.

Deferred **6.1b/6.4c/7.4c remain Blocked**, unchanged. Full native/platform/device/
assistive-technology/physical-resource/filesystem/installed acceptance remains
Blocked or Not run as stated above. **Stop before Module 11. No commit or push.**

[Final evidence consistency](artifacts/module-10/runs/20261022T031119.905623Z/evidence-consistency.json) and
[documentation structure check](artifacts/module-10/runs/20261022T031119.905623Z/spec-validation.json) passed. The latter
checks documentation structure and links only, not product acceptance.
