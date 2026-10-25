# Module 10 failure and cancellation contracts

Authorized 22 October 2026; stop before Module 11. Existing physical/filesystem,
resource, installed-build, platform and Module 6/7 gates remain Blocked.

Semantic operations keep the durable operation-ID/request-hash ledger. A retry with
the same semantic request returns the recorded outcome before revision admission;
reuse with changed intent is refused. Exactly-once here means one durable semantic
operation under one operation ID, not arbitrary duplicated UI intents with new IDs.
Part/Assembly/Drawing crash tests regenerate actual exact geometry and check
identities, revisions, command count, saved hashes and backup hashes.

Existing working/saved roots remain separate. Undo/redo roots and document/saved/
recovery/job/export object roots protect authoritative imports. History is bounded
by StoreOptions entries and bytes; ledger admission is separately capped at
100,000 operations. Recovery discards process-owned job/export roots from a dead
process; current and retained history roots remain. Display and result caches are
reconstructible and governed separately. Count/byte tests do not establish total
OCCT live-object or physical-memory plateau acceptance.

Explicit save retains its standalone validation, low-space admission, sibling
staging, backup, flush and atomic publication. Mac uses fsync/F_FULLFSYNC plus
directory flush; Windows uses its existing replacement/flush adapter; Linux uses
fsync/rename. Only the available Mac implementation is exercised here. Real
SQLITE_FULL page quotas and denied writes are tested without filling user disks.
Physical full-volume, storage power-loss and other native filesystem matrices
remain Blocked.

The supported older corpus consists of the strict schema-v1 historical semantic
manifests already accepted by the project. `migrationCopy` pins a read transaction,
validates the source, backs up to a sibling temporary copy, upgrades the recognized
manifest to drawing-v1 with a migration provenance record, validates again, flushes
and publishes. IDs and semantic parameters remain stable; the original stays
unchanged. Unregistered schema 0, future schema, future feature definitions,
unknown encodings and corrupt/missing objects remain refused. There is no claim
of a converter for imaginary prior authoritative formats.

Heavy sketch/feature/history/assembly/drawing jobs run on one serialized worker
job thread, while the worker socket loop accepts cancellation. No second exact
writer exists. Queued normal requests wait or receive an explicit busy response.
Cancellation is checked before computation, after computation, and immediately
before durable commit. A precommit cancellation rolls back semantic state and
ledger changes. A durable commit wins any later cancellation; no postcommit
checkpoint can turn it into Canceled. Derived caches can be reconstructed from
committed authority. PDF/export publication similarly checks before replacement,
then reports success even if cancellation arrives after publication.

`Cancel running computation` requests a safe checkpoint; `Stop worker - recover
durable work` uses the existing forced process-stop/restart boundary for a stuck
native call. Reconnect uses a fresh epoch and explicit recovery/operation query.
Late cancel acknowledgments cannot overwrite an already observed terminal result.
Old drawing preview/display results now use the same epoch/context/generation/
revision visibility gate as other display results. No fault-control IPC endpoint
or environment switch is added to the production worker.


PDF staging is now registered in the same durable ownership journal as save
staging. Recovery verifies the staging file identity before removing an owned
`.cad-pdf-*` file; a published/missing staging name clears its journal entry.
SIGKILL immediately before/after PDF replacement verifies prior destination
preservation or complete new PDF publication and orphan cleanup. Post-publication
acknowledgment/flush failures are UncertainExport, never a claim that cancellation
prevented publication. Private recovery paths cannot be PDF destinations.
