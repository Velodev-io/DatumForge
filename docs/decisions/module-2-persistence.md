# Module 2 — semantic storage decisions

Date: 19 October 2026. Scope: authorized document/persistence foundation on the
available Mac; native Windows/Linux and physical power-loss gates remain Blocked.
See [evidence](../testing/module-2.md) and [authorization](implementation-scope.json).

## Identity, units and revisions

Project, document, properties-entity and operation identities are canonical UUIDs.
Part, Assembly and Drawing currently contain metadata and typed parameters. Future
feature/constraint/occurrence/mate/drawing tables are materialized but must be empty
until their owning modules can validate their definitions. No geometry is inferred
from a metadata parameter. Length, angle, mass and scalar values persist in SI;
display-unit changes do not rescale stored values. The existing reference box has
its own explicitly millimeter-based adapter and is not project geometry.

The wire `revision` is a project-wide monotonic sequence because cross-document
transactions, one writer, undo and snapshot publication need one ordering. A
document's revision is the project sequence of its last semantic edit; gaps are
valid. View-only switch/close does not create semantic revisions. Undo/redo is
project-wide, commits a new sequence, and stores semantic inverse/forward deltas.
Creating a new edit truncates the redo branch. Document/entity IDs survive reopen
and Save As; Save As assigns a new project identity and leaves the source intact.

Durable deduplication binds operation UUID to canonical semantic request content,
project/document context and base revision. Epoch, request, session and generation
are transport identifiers, so a retry after restart can find the original commit.
Changed-content reuse fails. Query reports DurablyRecorded only with a ledger row;
unknown operations remain Unknown. Save and attachment hashes bind the destination
or declared content rather than an expiring file capability. Duplicate lookup
precedes identity/revision preconditions changed by the original operation.

## Working database and saved file

Only the serial worker writes SQLite. The desktop projects typed state and meshes;
it links neither SQLite nor OCCT. Schema v1 uses STRICT tables, foreign keys,
indexes, a migration identity and an exact compiled schema fingerprint. Open checks
integrity, semantic records/deltas and streamed object hashes. Unexpected triggers,
altered constraints, nonempty future definitions and newer versions are refused.

Each private working database uses WAL, synchronous FULL, fullfsync and
checkpoint_fullfsync on Mac. Backup pins a source read transaction, copies pages
incrementally and validates the resulting revision. Saved files use DELETE journal
mode and have no required WAL companion. SQLite documents the snapshot behavior of
its [backup API](https://sqlite.org/backup.html), the scope of
[atomic commit](https://sqlite.org/atomiccommit.html), and the platform dependence
of [synchronous settings](https://sqlite.org/pragma.html#pragma_synchronous).
Those mechanisms do not establish unrun power-loss or other-platform guarantees.

Save writes an intent durably, stages and validates a sibling snapshot, flushes it,
preserves the prior standalone file as `.bak`, replaces the destination, and flushes
the directory before acknowledging success. Mac uses fsync plus F_FULLFSYNC for
files. Saved revision advances only after publication. A process can die after
publication but before acknowledgment: recovery validates the published operation
and request hash to reconcile the intent. An ordinary exception before publication
starts removes its non-uncertain intent; an error once replacement may have begun
retains the intent. Errors never invent Saved status. Owned staging cleanup requires
an exact recorded path/file identity and a valid working database.

Canonical normalized/case-folded path locks, OS file-identity locks, project UUID
locks and workspace locks cooperate. The UUID lock also prevents a writer opening
an old hardlink after atomic replacement changes the destination inode. OS process
termination releases locks. This is a local single-user development-root protocol,
not a network-filesystem or multi-user distributed lock guarantee. Windows APIs
and Linux branches remain candidates requiring native execution.

Migration is deliberately a copy-and-refuse skeleton: it stages a separate copy,
then rejects an older format without a registered converter, preserving the
original. There is no released predecessor format or supported conversion yet.
Module 10 must add actual converters, compatibility fixtures and rollback checks.

## Recovery, retained objects and limits

Application startup offers newer verified recovery independently from the saved
file. Open offers Saved versus Recoverable when both exist. A worker restart
recovers the indexed working database; the UI keeps cached state visibly stale
until the new worker verifies it. Closing warns about unsaved changes and offers
Keep recoverable or Cancel. Saving first creates a portable snapshot.

An atomic index selects one active workspace per project. Choosing Saved publishes
a new working workspace and retains the previous directory without deletion. The
old branch currently has no recovery-browser UI; choosing Saved supersedes its
automatic offer. This non-destructive retention decision needs a reviewed archive/
selection workflow in Modules 10/12 before release. The 128-workspace admission
limit refuses more work rather than deleting older recovery implicitly.

Objects are opaque-v1 content-addressed attachments, classified authoritative or
cache, with incremental SHA-256 and 64 KiB read/write chunks. They are not a CAD
exchange/import feature. Document, saved, bounded history, job/export and recovery
roots prevent collection while needed. Startup clears abandoned runtime job roots
only after acquiring the workspace lock; committed documents/history remain.

Current conservative limits: 2 GiB database, 128 MiB individual object, 128 documents,
1,024 parameters, 100,000 ledger records, 32 uncertain save intents, 256 runtime
job/export roots, 128 undo entries and 8 MiB undo deltas by default, and an 8 MiB
SQLite page cache. Collected query results have a 32 MiB accounting budget; ledger validation streams rows. History test overrides remain bounded production options, not
product fault endpoints. Disk admission reserves 64 MiB plus working/snapshot/
backup estimates. These are safeguards, not measured full B0–B5 memory/performance
acceptance. The physical 8 GB baseline and SRS budgets remain unchanged.

Qt 6.10 QStorageInfo reported APFS's read-only system root for a writable firmlink
destination on the actual host. The storage probe now queries the destination
mount directly (statfs on Mac, statvfs on Linux). Mac regression evidence passes;
the Linux implementation still requires its native runner.
