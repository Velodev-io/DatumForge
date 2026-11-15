# Module 1 evidence

Status: **available Mac implementation verified; cross-platform completion Blocked**.
The user explicitly authorized Module 1 on 19 October 2026: “proceed with module 1”.
[Scope record](../decisions/implementation-scope.json). This records the Module 1 boundary; subsequent Module 2 work is recorded in [module-2](module-2.md).

**Task register:** 1.1a–1.3b and 1.4a–1.4b are Pass for the documented available-Mac reference scope; 1.4c remains Blocked. See [task-status.json](task-status.json).

## Implemented boundary

The production `cad-desktop` is a C++20 Qt Widgets application with top menus and
command toolbar/search, left model/property tabs, central viewport, bottom document
and status context, optional right resources, original geometric icons, and saved
native dock/window preferences. New/Open/Save/Sketch/Extrude/Accept feature have
explicit unavailable reasons. No reference artwork is bundled.

One `Client` owns the UI state: temporary document/project/session IDs, worker epoch,
revision/generation, edit mode, selection, preview phase, stale flag, and save/job/error
status. Panels project that state and immutable meshes. The worker alone owns OCCT
shapes and validates exact reference queries. The desktop links no OCCT, SQLite or
Qt Test; Qt Test and the adversarial peer are separate executables. Layout preferences
are ordinary user settings, not a project database.

This is a temporary, explicitly labelled **reference scene**, not a saved part.
The worker constructs a validated 10 × 20 × 30 mm box (6 faces, 12 edges,
12 triangles, 36 expanded display vertices) or 100 translated occurrences sharing
one display definition. Dimensions 1–1000 mm produce disposable previews; Cancel
rebuilds the original box. Semantic face/edge roles and occurrence IDs connect tree,
highlights and exact OCCT area/length/volume queries. They are reference-fixture IDs,
not the persistent topological-reference engine planned for Modules 2–5. Commands
explicitly name their millimeter input units; production SI document semantics are
not implemented here.

Private local IPC uses a user-only temporary directory/socket (candidate user-only
Windows named pipe), a fresh 256-bit inherited-stdin capability and UUID epoch,
strict version-1 envelopes, exact request/operation/context/revision/generation
correlation, structured errors, bounded definite CBOR and checksummed chunked
meshes. Limits are 1 MiB control frames, 16 KiB strings/chunks, 32 MiB mesh segments,
256 MiB aggregate transfer reservation and 32 outstanding requests. The client runs
one request at a time plus a single replaceable preview slot; a 201-change burst
never exceeds two preview requests in flight/queued. Worker dispatch is serial and
also coalesces queued previews. Context/generation gates discard superseded results.

A bounded 32-result **worker-session cache** supports same-content operation replay
(including all mesh chunks) and outcome query. Reusing an operation ID with different
content fails. Superseded-generation replay fails. Query after restart returns
`UnknownNotDurable`. This cache makes no durable exactly-once guarantee. Worker death
keeps the prior display visibly stale, clears selections/transfers/queues, and offers
Restart. Document recovery, accepted feature transactions, persistence and a durable
operation ledger require Module 2 or later; no failed worker is claimed to recover a
saved project in this module.

The Qt/OpenGL renderer supports perspective/isometric and front/top/right
orthographic cameras, orbit/pan/wheel zoom/F-to-fit, shaded/edge/wireframe/hidden-edge
modes, logical-coordinate CPU picking with depth checks, face/edge/body/component
filters, highlights, text overlays, resize/docking and context cleanup. Display data
never supplies exact measurements. This is the ADR-06 initial mesh-renderer route;
Metal, OCCT AIS and other backend/platform support are not implied.

## Actual verification and artifacts

Run from a logged-in Mac desktop terminal after provisioning the pinned dependencies:

```sh
python3 tools/verify_module1.py --native
python3 tools/clean_reproduce.py --module 1
python3 docs/scripts/check_specs.py --write-report
```

[Verification commands, exit codes and durations](artifacts/module-1/verification.json),
[uncommitted source input hashes](artifacts/module-1/source-identity.json),
[build log](artifacts/module-1/build.txt), [CTest](artifacts/module-1/ctest.txt),
[clean source reproduction](artifacts/module-1/clean-reproduction.txt),
[format check](artifacts/module-1/format.txt),
[Clang analysis](artifacts/module-1/static-analysis.txt),
[linked-library/source ownership](artifacts/module-1/architecture.json),
[research preservation](artifacts/module-1/research-isolation.txt).
The repository has no initial commit or remote; source hashes identify this build.
Clean reproduction exports the source into a disposable root, verifies pinned source
archive hashes and rebuilds our application against existing local built libraries.
It is not a second dependency build, remote clone, installed build or another OS.

**13 enabled CTest entries pass; zero failures.** The standalone deliberate-failure
entry remains Disabled by design; `harness.failure_reporting` invokes it independently
and verifies its nonzero result. Offscreen tests establish neither graphics nor
accessibility. The separate native suite uses a real Cocoa window and OpenGL context.

| Test / task | Actual checks and observed result |
|---|---|
| Frame/typed envelope corpus | Fragmented frames; illegal/missing/version/identity/revision/payload fields; nested duplicate/non-text keys; indefinite/tagged/invalid UTF-8/deep CBOR; oversized strings/collections/frame declarations; correlated diagnostics: rejected as expected. |
| Mesh/transfer corpus | Round-trip 100 occurrences; checksum corruption and rechecksummed out-of-range indices; >32 MiB declaration; duplicate transfer; wrong epoch/request/operation/document/generation/revision; out-of-order chunks: rejected. Eight legal 32 MiB reservations fill the 256 MiB cap; ninth rejected before allocating payloads; cleanup returns reservation to zero. |
| Real worker corpus | Wrong epoch fails; initial reference succeeds; simulated lost acknowledgment is retried with the same operation/new request and replays its chunks; changed-content reuse fails; outcome query succeeds; old preview generation fails; malformed wire terminates the worker with exit 2. |
| Client lifecycle | Exact volume 6000 mm³; 201 preview changes end at 44 × 22 × 33 mm with ≤2 preview requests; cancel during pending preview restores 10 × 20 × 30 mm; worker kill keeps stale display and clears buffers/queues; restart gets a new epoch; prior operation query is UnknownNotDurable; 100-instance load and close succeed. |
| Adversarial worker corpus | Wrong capability/version/epoch, malformed/oversized frame, invalid mesh index, oversized allocation and death during transfer: client remains alive, rejects the result, has no new display, clears request/transfer accounting and exposes an error. |
| Native T-01/T-03 subsets | 1280 × 800 logical initial shell; command unavailability reasons; tree↔viewport synchronization; face/edge/body/component picking; exact 6000 mm³ / 200 mm² / 10 mm queries; right-drag orbit, middle-drag pan, wheel zoom, keyboard F, display modes, overlays, resize, dock relocation and restored settings. |
| Native T-04/T-35 subsets | Explicit pending/stale/preview-ready states; property preview at 42 mm; cancel restores 10 mm; real-process kill/restart handled by lifecycle corpus. Accept/history/progress for actual modeling commands remain later-module checks. |
| NFR-013/T-53 subset | Source/include and actual binary-link inspection prove desktop has no exact-kernel/database/test dependency; packaged worker matches its built executable. Validation and revision/generation checks execute in the real worker. |

The Module 1 task checks cover the reference-workspace contract only. They do not
close the complete FR-001/003/004/035 or NFR-013 release requirements; their
cross-platform/later-module gaps remain **Blocked** in the requirement register.

## Native graphics and resource evidence

[Actual host snapshot](artifacts/module-1/host.json): macOS 26.6.2 (25G83), arm64,
Mac14,2 Apple M2, **8,589,934,592 bytes physical RAM (8 GiB)**, integrated Apple M2
GPU, native Retina display. Apple Clang 21, SDK 26.5, Qt 6.10.0 and OCCT 7.9.2 are
unchanged from Module 0 source builds. Source/patch/tool/dependency pins remain in
`cmake/`. The native context reports OpenGL 4.1; raw JSON records actual GL driver,
renderer/vendor strings, effective DPR, viewport dimensions, 60 camera-command to
`frameSwapped` timings, CPU submission timings and UI-test/worker memory snapshots.

| Platform / effective scale | Result | Evidence |
|---|---|---|
| Mac, 100% (Qt override 0.5 × native DPR 2) | Pass | [log](artifacts/module-1/native-scale100.txt), [raw](artifacts/module-1/native-1.json), [image](artifacts/module-1/native-1.png) |
| Mac, 125% (Qt override 0.625) | Pass | [log](artifacts/module-1/native-scale125.txt), [raw](artifacts/module-1/native-1.25.json) |
| Mac, 150% (Qt override 0.75) | Pass | [log](artifacts/module-1/native-scale150.txt), [raw](artifacts/module-1/native-1.5.json) |
| Mac, 175% (Qt override 0.875) | Pass | [log](artifacts/module-1/native-scale175.txt), [raw](artifacts/module-1/native-1.75.json) |
| Mac, native Retina 200% | Pass | [log](artifacts/module-1/native-scale200.txt), [raw](artifacts/module-1/native-2.json), [image](artifacts/module-1/native-2.png) |
| Windows native graphics/DPI/process/ACL | **Blocked** | No Windows host, compiler/runtime pin or display. |
| Linux X11 and Wayland | **Blocked** | No Linux host/session, driver or platform pin. |
| Intel/minimum macOS, external monitors/driver failure | **Blocked** | Hardware/OS/driver cases unavailable. |

The scale overrides exercise real Qt logical/device-coordinate mapping on one
physical Retina monitor; they do not certify five physical monitor configurations.
Screenshots were inspected for readable overlays/control layout and rendered geometry.
Raw data is summarized in [measurement summary](artifacts/module-1/measurements.json). Final 60-action p95 camera-command presentation proxies range from 13.70 to 15.83 ms across the five scales. Point physical footprints are 107.2–299.5 MiB for the Qt Test UI process and 4.0–4.6 MiB for its worker.
The memory sample is a point-in-time debug **test-process** footprint, including Qt
Test and a native window grab, not production steady/peak memory. The 60-command
presentation signal is a responsiveness proxy, not full navigation/FPS acceptance.
Background workload and swap were uncontrolled (host snapshot records nonzero swap).
No GPU/shared allocation partition or cold-launch/steady/peak workload procedure was
completed. Thus SRS budgets and B0–B5/AC-01–AC-07 performance remain Not run/Blocked;
100 copies of one 12-triangle box are not the B3 assembly fixture.

## Failures found and corrected

- Initial current-turn wrapper probes configure/copy/test successfully, so the earlier
  Module 0 wrapper stall did not reproduce. A sandbox-only QLocalServer permission
  denial was separated from the successful host-permission IPC tests. See
  [wrapper probes](artifacts/module-1/wrapper-probes.json) and
  [initial recheck](artifacts/module-1/module0-recheck.json).
- The first real worker test crashed on empty-cache lookup: comparing a `QMap`
  `constFind()` iterator with mutable `end()` detached the map. `cend()` fixes that
  lifecycle error; first-request and replay tests now pass. No kernel failure was
  inferred from the crash. [Initial result](artifacts/module-1/contracts-initial.txt).
- Native teardown initially called a derived slot after the viewport destructor;
  explicit context-signal disconnection before GL cleanup fixes it. The first
  layout test also requested a window taller than the available work area; the
  corrected 1440 × 760 test verifies restoration without treating Qt's screen clamp
  as a defect. [Initial native failure](artifacts/module-1/native-initial.txt),
  [subsequent diagnosis](artifacts/module-1/native-fixed.txt).
- Intermittent 175% presentation waits were investigated rather than erased by later
  passes. Ten diagnostic repetitions produced nine passes and one wait failure; that
  failure records **active=false, exposed=false, visible=true, context valid** at
  iteration 28. The window no longer met the exposed-native-presentation prerequisite.
  The harness now raises/activates and verifies exposure before each timed action,
  and exits **77 / Blocked**, rather than Passed, if exposure is lost during a wait.
  The corrected final five-scale run contains actual exposed-window samples.
  [Initial wait failure](artifacts/module-1/native-scale175-presentation-initial.txt),
  [repeat failure](artifacts/module-1/native-scale175-repeat-1.txt),
  [diagnostic observation](artifacts/module-1/native-scale175-diagnostic-6.txt).
  These environment-limited samples are retained; they are not proof of a driver fault
  and do not establish background/occluded presentation or a performance budget.
- The analyzer source filter initially matched vendor `.deps/src/sqlite`; that run
  was stopped. Filtering paths relative to the project root now analyzes only
  apps/src/tests and excludes vendors/generated MOC. Final analyzer output is kept.

## Remaining blockers and decisions

1. **1.4c Blocked:** validate Windows and Linux X11/Wayland graphics, IPC permissions,
   worker cleanup and scaling; provision pinned toolchains/reference hosts (OD-02/07/08).
2. **Renderer support decision open:** this Mac Qt/OpenGL route works on the measured
   target; minimum macOS/Intel/driver-failure/Metal longevity are not resolved (OD-04).
3. **Product/release choices open:** name, release application ID, license and
   distribution/signing remain OD-01. The current app ID is a development identity.
4. **Accessibility and resource gates open:** screen reader/focus completeness,
   installed builds, controlled cold/warm/steady/peak/shared-GPU measurements and
   full physical 8 GB workloads remain later native/Module 11/12 work.
5. **Persistence deliberately pending:** stable project/document schema, transactional
   accepted commands, save/reopen, durable outcome ledger, migration, replacement-file
   locking and actual recovery are Module 2/10 work. Module 0 filesystem/power-loss
   gaps remain open; they are not converted into passes by a reference-scene restart.

**Historical stop point: Module 1 feasible Mac work verified; platform matrix Blocked.
A later explicit user instruction authorized Module 2; see [its separate evidence](module-2.md).**
