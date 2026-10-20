# Module 0 / Step Zero — implementation evidence

**Date:** 19 October 2026. **Overall status: Blocked (partial available-target feasibility).**
**39 lettered tasks verified; 9 remain Blocked. Module 1 has not started and requires explicit user approval.**

The native architecture and physical **8 GB** minimum are unchanged. These are isolated
Module 0 experiments, not an R1 CAD product. All T-01–T-56 product acceptance rows
remain **Not run**. Research is unchanged: all **97 files** match the original SHA-256 baseline.
Local Git was initialized without a remote or commit; uncommitted source/build hashes
identify the work. No final product name, release application ID or product license
was selected.

## Build, environment and fixture identity

- [Build/source/dependency manifest](artifacts/module-0/build-manifest.json),
  [source lock](../../cmake/dependencies.lock.json), [tool pins](../../cmake/tools.lock.json).
- Available hardware: real **8 GB Apple M2 MacBook Air**, arm64, macOS 26.6.2 build
  25G83, 8-core integrated M2 GPU. Native driver reports **OpenGL 4.1 Metal - 90.5**;
  that string is not evidence of a custom Metal renderer. [Sanitized inventory](artifacts/module-0/environment.json).
- Apple Clang 21.0.0 (`clang-2100.1.1.101`), SDK 26.5, CMake 4.3.1, Ninja 1.13.1,
  C++20 application/libc++; deployment target 13.0 is a flag, not tested minimum-OS support.
- Source-built Qt 6.10.0 plus recorded ARM intrinsic-header patch, OCCT 7.9.2
  shared/with exceptions/`-O1 -DNDEBUG`, SQLite 3.50.4, Eigen 3.4.0, Boost 1.86.0,
  PlaneGCS FreeCAD revision `256fc7eff3379911ab5daf88e10182c509aa8052`.
- [B0–B6 recipe hashes](../../tests/fixtures/manifest.json). B1 spike evaluates
  **15 faces / 1 solid / 35488.761101962 mm³**, matching its independent formula
  `35960 - 150*pi`. B4 box is 10×20×30 mm / 6 input faces / 6000 mm³;
  generated STEP is 15416 bytes, IGES 13284 bytes. Each output hash is retained
  in the run log; STEP translator header/path metadata can vary between runs.
  B2/B3/B5 are deterministic semantic recipes, not evaluated production models.

## Actual test outcomes

| Evidence class | Result and practical limit | Actual evidence |
|---|---|---|
| Complete initial host CTest | **Pass: 11 enabled tests**, one intentionally disabled direct-failure entry. Separate failure-reporting test proves executable exit 1 and CTest nonzero exit. | [CTest log](artifacts/module-0/ctest.txt), [JUnit](artifacts/module-0/ctest.xml), [failure proof](artifacts/module-0/harness-failure-proof.txt) |
| Final direct checks | **Pass: 11/11** including expected failure exit, protocol, storage, OCCT, PlaneGCS, mates, real process IPC, fixture determinism, isolation and statistics. | [Per-test stdout/stderr/exit codes](artifacts/module-0/final-direct-tests.json) |
| Final native binary | **Pass**: real GPU, face/edge picks/highlights, visible overlay, resize/floating dock, camera samples, 100 shared-mesh occurrences and worker restart; exact worker volume 6000 mm³. | [Native log](artifacts/module-0/final-native.txt), [final screenshot](artifacts/module-0/native-final.png), [UI libraries](artifacts/module-0/ui-linked-libraries.txt) |
| Numerical CAD | **Pass**: analytic extrude/cut/pattern/fillet, invalid fillet preserves prior result, meshing leaves exact volume unchanged, parent dimension, split history/deletion and explicit symmetric ambiguity. | [OCCT log](artifacts/module-0/geometry.txt) |
| Sketch / assembly feasibility | **Pass**: real PlaneGCS residual/DOF/drag/redundancy/conflict corpus; grounded Eigen 3-body 6/2/0 DOF, consistent cycle, contradictory distance, near-pi and antiparallel seed/cancel checks. | [Sketch](artifacts/module-0/solver.txt), [mates](artifacts/module-0/mates.txt) |
| Real storage/process faults | **Pass: 34 operations**: WAL/FULL/FK, external kills, before/after commit and publication, lost-ack dedup/content mismatch, disk full, real chmod denial, streamed blob, FK/missing input, GC/retained roots, alias/hardlink writer and actual blocked mutation, future schema and failed-copy migration. | [Raw matrix](artifacts/module-0/storage-matrix.json) |
| Imported geometry retention | **Pass**: delete source file and display caches, retain imported STEP in real SQLite via undo/recovery/job roots, reconstruct and remeasure 6000 mm³. | [Geometry/retention log](artifacts/module-0/geometry.txt) |
| Clean source export | **Pass on pre-final harness revision**: fresh application source build and all 11 enabled tests using the SHA-256-verified local dependency cache. Does not prove a second full dependency rebuild or an installed product. | [Exact commands/log](artifacts/module-0/clean-reproduction.txt) |
| Format/static analysis | Pinned clang-format check and Clang analyzer completed; later harness-only foreground/repaint/status edits are separately noted in build history. | [Format](artifacts/module-0/format.txt), [analyzer](artifacts/module-0/static-analysis.txt) |
| Final wrapper/reproducibility closure | **Blocked**: final CMake/Ninja wrappers stalled; native executable restored by executing its exact generated linker arguments (exit 0). Final CTest wrapper hit a 30-second timeout. Direct final tests above pass; this does not turn the wrapper result into Pass. | [Final CTest timeout](artifacts/module-0/final-ctest.txt), [issues/history](artifacts/module-0/build-issues.md) |
| Other platforms / installed builds / full 8 GB acceptance | **Blocked / Not run** as applicable; no Windows/Linux/Intel Mac/minimum-OS, assistive-technology, installed or B0–B5 product budget claim. | [Gate owners](../decisions/deferred-gates.md), [56 pending requirements](requirements.json) |

## Resource observations (supplementary)

[Raw profile](artifacts/module-0/native-profile.txt) and [summary](artifacts/module-0/profile-summary.json)
record physical-footprint fields from this real 8 GB Mac. The profiling build predates
final harness-only activation/repaint/missing-callback detection edits; it is not a
final controlled acceptance build. Idle after 30 s: UI about **198.1 MiB** (one sample).
Box medians: UI **214.0 MiB**, worker **4.0 MiB** (30 each). 100-instance medians:
UI **186.3 MiB**, worker **4.0 MiB** (30 each). Preserve per-process fields; summing can
double count shared/unified allocations. This is not unique GPU accounting or a true
peak measurement. Final native test separately recorded 58 frame-callback intervals
and 60 scripted input-to-swap samples; these are prototype callbacks, not a B2/B3 FPS gate.
Desktop background load, power and thermal state were not controlled/fully captured.
A previous profile with no frame callbacks is retained, not silently averaged in.
Task 0.6e therefore remains **Blocked** for complete measurement closure.

## Reproduction commands actually used

```sh
python3 tools/fetch_dependencies.py
python3 tools/build_dependencies.py --jobs 2
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug --output-on-failure
python3 tools/clean_reproduce.py
python3 tools/format_sources.py
python3 tools/static_analysis.py build/macos-debug/compile_commands.json
python3 tests/integration/storage_matrix.py build/macos-debug/storage_spike
build/macos-debug/slice_desktop.app/Contents/MacOS/slice_desktop --native-test OUTPUT.png
build/macos-debug/slice_desktop.app/Contents/MacOS/slice_desktop --profile OUTPUT.png
python3 docs/scripts/check_specs.py --write-report
```

Dependency builds first ran with two jobs; OCCT's final source build used four jobs
on the available Mac. Source/tool provisioning needs network; the built experiments
use local data, one local worker and no account/service. Sandbox native UI/socket
access was denied; authorized host runs provide the native evidence. Current wrapper
stall means the commands are **not all currently reproducible**; preserve that blocker.
A ZIP of source experiments was generated locally, not an installer or publication.

## Lettered task evidence

The machine-readable register is [task-status.json](task-status.json). Checkboxes in
STEP_ZERO are checked only for the Pass rows below. A Pass is scoped to the described
prototype/available host; it does not close a platform or product acceptance requirement.

| Task | Result | Verified outcome or unresolved work |
|---|---|---|
| 0.1a | **Pass** | Provisional identity and workspace preserved; no remote. |
| 0.1b | **Pass** | Native targets, offline core, one writer, SSD, 8 GB and R1/R2 frozen. |
| 0.1c | **Pass** | OD-01–OD-08 have owners, evidence needs and open/blocking status. |
| 0.1d | **Pass** | Public/visual/installer/design provenance distinguished; 97 research files unchanged. |
| 0.2a | **Pass** | No existing/nested repository; local main initialized, no remote/history rewrite. |
| 0.2b | **Pass** | Proposed source/app/adapter/test layout reserved; spikes isolated under tests. |
| 0.2c | **Pass** | README, contribution/build commands and original graphify guidance retained. |
| 0.2d | **Pass** | Builds/deps/models/recovery/secrets excluded; safe pins/manifests tracked. |
| 0.3a | **Pass** | Sanitized M2/8 GB/OS/GPU/compiler inventory; unavailable hosts assigned. |
| 0.3b | **Blocked** | Mac dependency compatibility passes; Windows/Linux compiler/runtime pins and support verification unavailable. |
| 0.3c | **Pass** | Content-locked project-local source strategy and explicit ABI/options established. |
| 0.3d | **Blocked** | Dependency notices inventoried; product owner has not chosen product license/identity/distribution. |
| 0.3e | **Blocked** | Mac commands built/tested/formatted/analyzed/archived; final CMake/Ninja wrapper stall unresolved. Other-platform commands unverified. |
| 0.4a | **Pass** | Marked disposable temp roots and development/test identities; production root refused. |
| 0.4b | **Pass** | CTest, Qt Test and native matrix configured; deliberate failure proves executable/CTest nonzero exits. |
| 0.4c | **Pass** | Real SQLite/files, external kills, write/full/permission errors and stale epochs/generations exercised. |
| 0.4d | **Pass** | Module 0–12 evidence files and all 56 product requirement rows created with honest states. |
| 0.4e | **Pass** | Mac/Windows/X11/Wayland/accessibility/scaling/installed procedures and owners defined. |
| 0.5a | **Pass** | B0–B6 deterministic recipes/hashes, original source/license, invariants, bounds and corruption cases frozen; future workloads explicitly unexecuted. |
| 0.5b | **Pass** | Physical 8 GB Mac identified; Windows/Linux reference hardware gaps assigned, not claimed validated. |
| 0.5c | **Pass** | OS physical-footprint accounting, shared/GPU limitations, latency/frame/save/disk methods recorded. |
| 0.5d | **Pass** | 5 cold/10 warm/30 interaction sampling procedure and p95 estimator established; production measurements Not run. |
| 0.5e | **Blocked** | Caps/pressure/retained-root prototypes pass; realistic B2/B3/B4 admission/headroom calibration requires later engines/workloads. |
| 0.6a | **Pass** | Available Mac: native Qt shell displays box created/meshed in OCCT worker. Other platforms unavailable. |
| 0.6b | **Pass** | Mesh-only UI, face/edge picking/highlight, native label overlay and worker-side 6000 mm3 measurement demonstrated. |
| 0.6c | **Pass** | Private inherited capability, version/epoch handshake, bounded CBOR/chunk fallback, range/checksum checks, stale generations and abandoned-buffer cleanup demonstrated; lost-ack storage dedup separate. |
| 0.6d | **Blocked** | M2 OpenGL 4.1 native render/docking passes; all scales, minimum OS/Intel Mac, real driver failure and production renderer/Metal decision incomplete. |
| 0.6e | **Blocked** | Physical 8 GB slice footprint and navigation samples recorded; complete controlled/peak/GPU accounting and final-harness profile association remain incomplete. |
| 0.7a | **Pass** | Pinned PlaneGCS rectangle/circle/DOF/drag/residual/redundant/conflict corpus passes; extraction shims documented. |
| 0.7b | **Pass** | Exact extrusion/cut/four holes/fillet: analytic volume 35488.761101962 mm3, 15 faces, one solid; invalid construction retains prior model. |
| 0.7c | **Pass** | Dimension change, Modified(top) split to two faces, IsDeleted, and symmetric ReferenceAmbiguous exercised. |
| 0.7d | **Pass** | Grounded three-body planar/axial/angle prototype: 6/2/0 DOF, cycle/conflict, near-pi/antiparallel and cancel corpus passes. |
| 0.7e | **Pass** | Adapter sign and antiparallel failure resolved without R1 relaxation; larger corpora remain explicitly pending. |
| 0.8a | **Pass** | WAL/FULL/FK working DB, validated standalone snapshots and incremental 4 MiB blob in 64 KiB chunks; imported STEP retained in real SQLite. |
| 0.8b | **Blocked** | Mac temporary sibling/backup/flush/rename preserves previous snapshot on failures; Windows/Linux filesystem semantics untested. |
| 0.8c | **Pass** | Mac process exits and external kills before/after commit, interrupted snapshot, publication/lost ack recover expected revisions once. |
| 0.8d | **Pass** | Mac alias/hardlink lock rejects actual mutation; future schema, failed-copy migration and missing references rejected. Replacement identity remains production hardening. |
| 0.8e | **Pass** | Source and display caches removed; exact STEP rebuilt from SQLite authoritative BLOB with undo/recovery/job roots, volume 6000 mm3. |
| 0.9a | **Pass** | All seven ADRs record pins, outcomes, consequences and revisit triggers; unresolved choices retained. |
| 0.9b | **Pass** | Modules 1–4 first production slice frozen and mapped to FR/test IDs. |
| 0.9c | **Pass** | Native, hardware, accessibility, advanced fixture and installed gates assigned to owners/modules. |
| 0.9d | **Pass** | Specs/status/traceability synchronized; 8 GB/R1 budgets unchanged; product T-01–T-56 all Not run. |
| 0.10a | **Blocked** | Clean source export with verified dependency cache passed 11 tests before final harness edits. Final CMake/Ninja/CTest wrapper stall prevents final clean reproducibility closure. |
| 0.10b | **Pass** | Disposable roots/production-root refusal and unchanged 97-file research hash baseline verified. |
| 0.10c | **Pass** | Available Mac shell/mesh/face-edge picking/exact measurement/restart tests pass; stale/transfer adversarial corpus separate; platform gaps remain Blocked. |
| 0.10d | **Pass** | Actual pinned OCCT/PlaneGCS/Eigen small valid/invalid CAD corpus passes; larger production CAD remains pending. |
| 0.10e | **Blocked** | Mac real-file storage matrix passes; cross-platform publication/durability gates remain unavailable. |
| 0.10f | **Pass** | Reviewed pins/notices/support/evidence and unresolved OD owners; no product acceptance or untested platform presented as passed. |


## Remaining blockers and decisions before further production work

1. Restore reliable final CMake/Ninja/CTest wrapper execution and repeat clean
   reproduction on the final harness revision (0.3e/0.10a, build maintainer).
2. Supply Windows/Linux hosts/runners and compiler/runtime pins; minimum macOS/Intel
   Mac and native filesystem semantics remain untested (OD-02, 0.3b/0.8b/0.10e).
3. Product owner must choose product name, release application ID, product license
   and distribution route. Notices/source/relinking obligations are inventoried,
   not a completed release distribution decision (OD-01, 0.3d).
4. Complete native scale/driver/accessibility checks and select the production Mac
   renderer route; future native Metal adapter is unimplemented (OD-04/08, 0.6d).
5. Calibrate realistic workload admission/headroom and collect controlled final-build
   peak/shared/GPU measurements; full B0–B5 and real 8 GB Windows/Linux remain later
   mandatory gates (OD-06/07, 0.5e/0.6e, Module 11).

[ADRs](../decisions/feasibility-adrs.md), [all deferred gate owners](../decisions/deferred-gates.md),
and [frozen first production slice](../decisions/vertical-slice.md) retain the unresolved
choices without changing R1 or relaxing budgets. The working-file identity lock spike
is not the final saved-file replacement locking implementation. No full R1 feature
history, drawing, assembly UI, installed package or release guarantee is implemented.

**Stop point: Module 0 partial/Blocked. Module 1 remains unstarted. Explicit user
approval is required before any Module 1 work.**
