# Module 2 evidence

Status: **available-Mac document/persistence foundation verified; full platform
completion Blocked**. The user authorized Module 2 on 19 October 2026:
“go-ahead with moudle 2”. [Scope record](../decisions/implementation-scope.json).
Module 3 has not started and requires new authorization.

## Implementation and task scope

The lettered implementation sequence is 2.1a → 2.1b → 2.2a → 2.2b → 2.3a →
2.3b → 2.4a → 2.4b → 2.4c. Domain/storage tests were expanded at each stage;
failures and regression results are retained below. Independent Mac work continued
despite unavailable Windows/Linux prerequisites, as STEP_ZERO permits.

| Task | Result and evidence scope |
|---|---|
| 2.1a | Pass: Part/Assembly/Drawing metadata, stable UUIDs, SI parameters and units, project/document revisions, strict worker validation. |
| 2.1b | Pass: request-hash/revision-bound durable deduplication, create/switch/close, bounded semantic undo/redo; no per-edit BRep copies. |
| 2.2a | Pass: strict schema, indexes/FKs, WAL/FULL private working database, durable ledger, incremental object streams and authoritative/cache classification. |
| 2.2b | Pass on available Mac: standalone snapshot/backup publication, Save As new project identity, saved/current revisions, canonical/identity locks, future-schema rejection, migration-copy skeleton. Other OS publication semantics remain Blocked. |
| 2.3a | Pass: real worker death/restart restores committed state; application restart offers verified newer recovery; explicit Saved versus Recoverable choice. |
| 2.3b | Pass on available Mac: document/saved/history/job/export/recovery roots and collection; failed writes retain original snapshots and report errors. |
| 2.4a | Pass: all three document kinds, IDs/units/reopen, native tabs and unsaved warning, duplicate replay after identity/revision changes, undo/redo boundaries. Full downstream CAD acceptance remains pending. |
| 2.4b | Blocked for full platform completion: available Mac process-exit/SIGKILL and lost-publication-ack cases pass; Windows/Linux filesystems unavailable. Physical power-loss testing is separately Blocked. |
| 2.4c | Blocked for full platform completion: available Mac aliases/live writer/schema/migration/SQLite quota/denied-write/missing-object cases pass; native Windows/Linux matrix unavailable. Physical volume exhaustion Not run. |

Seven task rows are checked for their explicit available-target scope; 2.4b/2.4c
remain unchecked in [TECH_SPEC](../TECH_SPEC.md) and [task status](task-status.json).
All full product requirement rows touched here remain Blocked for missing platforms
or later-module integration. This is not full R1 CAD acceptance.

The desktop now exposes New/Open/Save/Save As, document tabs, metadata/units and
typed parameter editing, undo/redo, and recovery choices. Accepted edits are labelled
recoverable; explicit saved revision is separate. Worker failure leaves cached
state visibly stale until recovery completes. The actual native screenshots show
[saved metadata](artifacts/module-2/module2-native.png) and
[recovered newer work](artifacts/module-2/module2-recovered-native.png).
Modeling, mates and drawing content remain unavailable until their owning modules.

Only one worker owns exact geometry and project writes. The UI owns display meshes
and typed projections. No SQLite, OCCT or Qt Test dependency is linked into the
desktop. No production fault-control commands were added. Tests use separate
executables and marked disposable roots and refuse release recovery paths.
Architectural decisions, limits and unresolved retention/migration choices are in
[the Module 2 ADR](../decisions/module-2-persistence.md).

## Reproduction and actual results

From a logged-in Mac desktop, with pinned dependencies already provisioned:

```sh
python3 tools/verify_module2.py --native
python3 tools/clean_reproduce.py --module 2
python3 docs/scripts/check_specs.py --write-report
```

Final command exit codes/durations are in
[verification.json](artifacts/module-2/verification.json). The uncommitted initial
repository is on `historical/module-2`; it has no remote or initial commit. Exact tested
inputs and aggregate SHA-256 are in
[source-identity.json](artifacts/module-2/source-identity.json).
[Dependency pins](../../cmake/dependencies.lock.json),
[tool pins](../../cmake/tools.lock.json) and the Qt ARM patch remain content-locked.

| Check | Observed result / raw evidence |
|---|---|
| Configure and build | Pass; [configure](artifacts/module-2/configure.txt), [build](artifacts/module-2/build.txt). |
| CTest | 18 enabled entries Pass, zero failures; [log](artifacts/module-2/ctest.txt). The deliberate-failure entry is Disabled by design; failure-reporting checks its nonzero exit independently. |
| Storage/process faults | 35 cases Pass using the production persistence library and actual SQLite/files/processes; [per-case matrix](artifacts/module-2/fault-matrix.json). |
| Native Module 2 | Cocoa window: three kinds, parameter input 25 mm → 0.025 m, invalid draft retained, undo/redo, tab switching, unsaved close choice, Save action and new-window startup recovery chooser; [Qt Test log](artifacts/module-2/native-module2.txt), [native metadata](artifacts/module-2/native.json). |
| Module 1 regression | Existing viewport/reference/lifecycle/adversarial tests pass in CTest. Separate native runs pass at effective 100/125/150/175/200% on one Retina display; [100](artifacts/module-2/native-module1-scale100.txt), [125](artifacts/module-2/native-module1-scale125.txt), [150](artifacts/module-2/native-module1-scale150.txt), [175](artifacts/module-2/native-module1-scale175.txt), [200](artifacts/module-2/native-module1-scale200.txt). |
| Format/analyzer | Pass with pinned clang-format and no reported Clang analyzer warnings; [format](artifacts/module-2/format.txt), [analysis](artifacts/module-2/static-analysis.txt). |
| Ownership/bundle | Source and actual linked-library inspection Pass; packaged worker matches built worker; [record](artifacts/module-2/architecture.json). |
| Research preservation | All 97 baseline research files unchanged; no remote; [isolation](artifacts/module-2/research-isolation.txt). |
| Clean reproduction | Fresh source export/configure/build and all enabled CTest entries Pass using verified source archives and existing local dependency binaries; [log](artifacts/module-2/clean-reproduction.txt). No second dependency compilation, installed build or other OS is implied. |

The fault matrix covers three kinds at seven exit boundaries (before/after semantic
commit, during backup, after validation, before/after publication, after save ledger),
four externally issued SIGKILL boundaries, symlink/hardlink/live-writer refusal,
old-hardlink locking after destination replacement, future schema, missing object,
altered schema trigger, corrupt semantic delta, actual SQLITE_FULL via a page quota,
actual denied destination write, and failed migration-copy refusal. Assertions check
exact current/saved revision, ledger outcome, original/backup hashes and owned
staging cleanup. Process exits are not power-cut evidence.

Additional storage checks stream a 4 MiB object in 64 KiB chunks, reject short or
oversized input, verify SHA-256, test retained job/history roots and collection,
exercise three-entry undo retention, and replay Save As/attachment/NewProject after
the original changes its preconditions. A 64 MiB generated query is refused by the
32 MiB collected-result budget; row streaming processes all 64 rows. Ledger
validation streams rows, so it does not materialize the entire durable history.

## Failures found and fixed

Raw failing evidence is retained, not recast as passing:

| Failure | Cause and verified correction |
|---|---|
| [Initial storage](artifacts/module-2/storage-initial.txt) | Qt QStorageInfo resolved a writable APFS firmlink to the read-only system root. Querying the actual destination mount via statfs fixed admission. The production storage suite and fault matrix pass on that filesystem. |
| [Save As replay](artifacts/module-2/storage-replay-red.txt) | Identity check ran before durable lookup, rejecting a valid duplicate after Save As changed identity. Lookup now precedes changed preconditions. |
| [Attachment replay](artifacts/module-2/storage-attachment-replay-red.txt) | Current-revision check preceded durable lookup. A committed duplicate now returns the recorded outcome without rereading the consumed input stream. [Green storage run](artifacts/module-2/storage-replay-green.txt). |
| [NewProject replay](artifacts/module-2/dispatch-replay-red.txt) | Replacement guard rejected the original operation when retried. Same-project durable replay now precedes replacement consent; changed-content reuse still fails. [Green dispatch run](artifacts/module-2/dispatch-replay-green.txt). |
| [Repeated prepublication failure](artifacts/module-2/storage-prepublication-red.txt) | Ordinary failures left uncertain intents and exhausted the 32-intent cap. Cleanup now removes known prepublication failures while preserving possibly published intents. Forty failed attempts followed by a successful save pass. [Green](artifacts/module-2/storage-prepublication-green.txt). |
| [Query memory budget](artifacts/module-2/storage-query-budget-red.txt) | Per-row limits alone allowed large aggregate allocations. A collected-result budget and streamed ledger validation now pass the generated oversized-result/streaming regression. [Green](artifacts/module-2/storage-query-budget-green.txt). |
| [Initial analyzer](artifacts/module-2/static-analysis-initial.txt) | An unused assignment was removed; the analyzer runner now fails on warnings as well as nonzero exits. The final run reports no warning. |

## Host, remaining blockers and decisions

[Fresh host inventory](artifacts/module-2/host.json): macOS 26.6.2 (25G83), arm64,
Apple M2, **8,589,934,592 bytes physical RAM (8 GiB)**. Apple Clang 21, SDK 26.5,
Qt 6.10.0, OCCT 7.9.2 and bundled SQLite 3.50.4. Native runs report Cocoa and
OpenGL 4.1 on the M2 display. Initial sandbox-denied inventory reads are preserved
in [host-sandbox-initial.json](artifacts/module-2/host-sandbox-initial.json); the
host-side read succeeded. No physical-memory baseline or SRS budget was relaxed.

| Outstanding check/decision | State and owner |
|---|---|
| Windows/Linux build, locks, flush/replacement, worker recovery and native UI | Blocked: no native runners/session/compiler; OD-02 and platform maintainers. Candidate code/presets are not passing evidence. |
| Minimum macOS/Intel Mac/additional drivers | Blocked: current arm64 host only; OD-02/OD-04. |
| Hardware power loss | Blocked: no controlled power-loss hardware/procedure; Modules 10/12. |
| Physical SSD exhaustion | Not run: actual SQLite quota-full and denied-write errors pass without filling the user's disk. Full-volume snapshot/backup cases need a disposable native volume. |
| Prior-format migration | Skeleton and failure preservation pass; no released prior-format converter exists. Supported conversions remain pending Module 10. |
| Archived recovery branch selection | Previous directories are retained; only the indexed branch is offered automatically. A reviewed archive/recovery-browser workflow and retention policy remain for Modules 10/12. The 128-workspace cap refuses new work safely. |
| Realistic B0–B5 memory/latency/shared-GPU workloads | Not run; later engines absent. Module 11/OD-07 retains the physical 8 GB and original resource budgets. |
| Native OS file chooser, assistive technology, installed packages/upgrades | Full manual/platform acceptance Blocked; real Qt native controls and worker granted-path pipeline tested here. Modules 9/12. |
| Product name/license/signing/distribution | Blocked pending owner decisions/credentials; OD-01. No remote, release or publication performed. |

Continue with Module 3 only after explicit user authorization. Existing research,
historical Module 0/1 evidence and unresolved release gates are preserved.
