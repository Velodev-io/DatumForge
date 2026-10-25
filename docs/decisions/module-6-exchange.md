# Module 6 — neutral exchange and portable copies

Date: 21 October 2026. Authorization: [scope register](implementation-scope.json).
Native Qt Widgets remain the UI; the serial worker owns OCCT geometry and project
writes. The [evidence](../testing/module-6.md) distinguishes implemented behavior
from unmeasured resource/platform acceptance. Stop before Module 7.

## Imported bodies and source authority

`Imported` is a versioned semantic feature in the existing feature graph. Its
parameters bind SHA-256 source identity, STEP/IGES format, file-unit or explicit
unit override, explicit `None`/`Fix` healing, original basename and a bounded
translation report. Source staging and hashing run in the background worker; the UI sends no source bytes. Original source bytes are streamed into an authoritative
SQLite object in the same transaction as the feature and undo record. Failure or
cancellation before commit rolls both back. Request hashes bind import operation
IDs to content and options; retries do not create a second feature.

When no preselected source hash is supplied, the worker derives it from its staged stream before binding the durable request hash. The worker rebuilds from embedded source bytes, verifies their hash, and uses the
existing bounded result cache for subsequent immutable effects. Current features,
last-valid checkpoints, undo/redo, recovery and saved snapshots retain required
source objects. Cache trimming never removes source authority. The compatibility
manifest gains `exchange-v1`; older workers must refuse it. No SQL schema version
change or in-place migration of existing projects is needed.

Kernel coordinates remain mm. STEP's declared system conversion and IGES header
conversion establish file units; overrides apply an explicit ratio after normal
conversion. Unknown/missing/mixed STEP length units are refused. Overrides and
healing are recorded and warned. Automatic optional shape-processing flags are
disabled; required translator representation construction still occurs. Explicit
ShapeFix uses 0.0001 mm precision / 0.0001 mm maximum tolerance and records before/
after volume, area, bounds and topology. Valid closed solids only, up to nine per
import; free surfaces/curves and assembly/mapped/external references are refused.
Imported solids must satisfy the existing 0.0001 mm vertex/edge/face tolerance ceiling and positive-volume/closed-shell checks. Imported geometry does not recover proprietary feature history. This adapter
reports loss of names, colors, PMI, layers and structure; source bytes retain the
original metadata. The colored/named B4 test verifies a geometry round trip plus
an explicit loss warning, not metadata preservation in the editable model.

## Jobs, limits and cancellation

One background job thread executes import/export while the worker's local IPC
loop remains responsive to `CancelJob`. Other project requests are refused while
the job owns the worker. All OCCT/project mutations remain serial; the UI never
owns exact shapes. Progress reports have at most 100 distinct percentages.
Cancellation is checked while staging, at translation boundaries, through OCCT's
progress interface, and immediately before durable commit/publication. The reader's
initial parser call may not poll cancellation; the UI says cancellation is pending
until an actual terminal outcome. A 120-second client deadline stops an unresponsive
worker and reports an uncertain outcome requiring recovery/inspection. Cancellation
after commit/publication cannot turn success into a false canceled result.

Admission bounds include 100 MiB input/output, 64 KiB parser records, 200,000 source
records/entities, 50,000 faces, 200,000 edges, nine solid bodies and finite coordinates
within 1000 m. Authoritative sources retain the existing 128 MiB per-object cap;
projects retain the 2 GiB cap. Translation/model-check failures reject partial
results. STL detail is 0.01–5 mm, meshing is serial/cancelable, and output admission
is one million triangles. Topology/triangle/output bounds are checked after the
corresponding kernel phase; they are **not a hard pre-allocation bound** on an
arbitrary hostile parser/kernel. Abrupt termination can leave transient import/export staging files; normal RAII cleanup is tested, but full orphan-staging retention/cleanup proof belongs to Modules 10–11. Full high-expansion/physical-peak acceptance is
Blocked pending a broader corpus and measured resource containment. Native sampled
footprints are evidence for the exercised workflows only.

## Export publication and revision validity

STEP/IGES/STL export requires the declared current document/project revision and
refuses a stale checkpoint. The user chooses all current bodies or one body,
length units and STL detail. STL's unitless coordinates follow the chosen units;
the UI/report carries that convention. Geometry is written beside the destination
to a temporary file. STEP/IGES output is reimported and compared for solid count,
volume and area before publication. Loss warnings are returned. A final cancel
check, flush and atomic replacement precede success. Existing project databases,
active project/recovery paths, and filename/format mismatches are refused.
Existing geometry outputs require an explicit overwrite choice.

Successful exports record a durable operation/result including checksum, revision
and units without changing the document or saved-project revision. A crash after
publication and before that ledger transaction may leave an unrecorded output; detected post-publication flush/ledger errors report `PublicationUncertain`, with no claim that the old output survived.
query reports Unknown and the file/checksum must be inspected. A repeated recorded
operation returns its original publication report; it does not assert an external
program has left the output file unchanged.

## Portable closure

`InspectClosure` validates and enumerates all project-local Part documents by
identity/revision and authoritative objects by hash/size. A portable copy embeds
all those definitions, sources and retained undo roots in a new standalone snapshot
with a new project identity, preserving document/entity identities. The durable
`PortableCopy` save record embeds the source identity/revision, closure manifest
and `Copied` policy. There are no external source links to resolve later. The
current project remains open and its saved path/identity remain unchanged.
Existing portable destinations are refused; no implicit overwrite occurs.

Assembly/drawing documents cause explicit refusal of complete closure until their
richer semantics exist in Modules 7–8. Selected subassembly/drawing closure is
assigned there. Part-project copy currently includes every local Part document,
which also closes cross-document sketch dependencies without guessing a subset.
