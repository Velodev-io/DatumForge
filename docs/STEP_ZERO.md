# Step Zero — repository, contracts, and feasibility

**Version:** 0.1 · **Date:** 19 October 2026  
**Status:** Partially implemented and verified on the available Mac. Module 0 remains Blocked; see [task evidence](testing/module-0.md).  
**Purpose:** Establish a reproducible, measurable foundation before implementing production CAD modules.  
**Prerequisites:** These proposed specifications and the existing research. No SolidWorks installation is required.  
**Outputs:** Version manifest, support matrix, repository conventions, isolated test harness, fixture manifests, feasibility results, and recorded architecture decisions.

Step Zero is **Module 0** in [TECH_SPEC](TECH_SPEC.md). Follow the earlier planning convention: complete `0.1a`, then `0.1b`, then the next item. Check an item only after its outcome is verified. A document or build script existing is not proof that the build runs. Do not copy DeskCore's checked tasks or results into this project.

## 0.1 — Establish identity, scope, and evidence

- [x] **0.1a — Record project identity:** Preserve this workspace and research. Keep “Solidworks Clone” provisional until a product name and stable application ID are selected. Record repository location; configure no remote without its actual destination.
- [x] **0.1b — Freeze constraints:** Record macOS/Windows/Linux native targets, 8 GB physical RAM baseline, offline core, one writer per project, local SSD storage, and the SRS R1/R2 boundary. The superseded 4 GB target is not a release requirement.
- [x] **0.1c — Register decisions:** Create the OD-01–OD-08 register from SRS, owners, blocking/deferred status, and evidence needed. Do not substitute assumptions for resolved choices.
- [x] **0.1d — Preserve provenance:** Link the handbook and atlas; distinguish public documentation, inspected screenshots, installer observations, and untested design in the project evidence register.

## 0.2 — Establish repository conventions

- [x] **0.2a — Adopt or initialize Git safely:** Check for existing/nested repositories and uncommitted work before initialization. Establish branch and review conventions; do not rewrite history.
- [x] **0.2b — Create the source layout:** Adopt the proposed TECH_SPEC layout, keeping documentation, application, worker, core, adapters, fixtures, and generated output separate.
- [x] **0.2c — Establish project guidance:** Add development README, contribution instructions, and applicable AGENTS guidance covering module order, ownership, tests, and actual dependency commands. Preserve existing user instructions.
- [x] **0.2d — Configure exclusions:** Ignore builds, downloaded dependencies, recovery databases/WAL files, real models, signing secrets, local logs, and temporary fixtures. Track safe configuration, source/license manifests, and reproducible dependency pins.

## 0.3 — Pin toolchains and dependency strategy

- [x] **0.3a — Inventory environments:** Record available Mac architecture, Windows/Linux hosts or runners, real 8 GB hardware, GPU/driver, OS versions, compiler, CMake, and Ninja. Missing platforms remain Blocked for validation, not assumed supported.
- [ ] **0.3b — Pin compatible versions:** Select exact Qt, OCCT, SQLite, Eigen, solver revision, compiler, and build-tool versions after a compatibility build. Record immutable URLs/commits and checksums. Do not label a floating `main` branch reproducible.
- [x] **0.3c — Choose dependency delivery:** Use one pinned source/build manifest with consistent ABI/compiler/runtime settings. A source-based CMake dependency strategy is the default; distribution packages may supply dev tools but shall not silently alter release dependencies.
- [ ] **0.3d — Resolve distribution constraints:** Record the intended product license and module-specific obligations for selected dependencies, notices, source availability/relinking where applicable, and future translator costs. This is an implementation/distribution decision, not permission to assume any dependency has no conditions.
- [ ] **0.3e — Define actual commands:** Establish configure/build/test/format/static-analysis/package commands and environment presets on all candidate platforms. Proposed command examples must remain labeled until executable targets exist.

## 0.4 — Isolate environments and establish test evidence

- [x] **0.4a — Separate data roots:** Use unique development/test/release identities and temporary project/recovery roots. Automated tests shall refuse known production roots.
- [x] **0.4b — Configure test runners:** Establish CTest with chosen C++ tests, Qt Test for UI components, and a native GUI harness/manual matrix. Include a deliberate failing case to prove nonzero failure reporting.
- [x] **0.4c — Build failure fixtures:** Provide injectable disk/write/permission errors, controlled process termination, stale IPC messages, and deterministic clocks/IDs where needed. Use real SQLite/temp files for integrity tests.
- [x] **0.4d — Establish the evidence register:** Create `docs/testing/module-N.md` and a requirement/test register with Not run/Pass/Fail/Blocked/Not applicable statuses, build/environment fields, procedures, actual results, and artifact paths.
- [x] **0.4e — Define native verification:** Document how macOS, Windows, Linux X11/Wayland, keyboard/focus, accessibility, scaling, and installed builds will be checked. A mocked widget or IPC test does not replace these gates.

## 0.5 — Freeze fixtures and resource measurement

- [x] **0.5a — Implement B0–B6 fixture definitions:** Supply versioned generators or licensed inputs, deterministic parameters, hashes, expected geometric invariants, source units, complexity counts, and corruption variants. Future-feature fixtures can begin as generators/manifests and remain unexecuted until their owning module.
- [x] **0.5b — Record reference hardware:** Identify at least one real 8 GB system per published platform before release. Where unavailable during Step Zero, record the gap and owner; do not claim cross-platform performance from the available Mac.
- [x] **0.5c — Define measurement methods:** Record cold/warm launch, steady/peak physical footprint, shared-memory accounting, GPU memory if observable, input p95, frame times, rebuild time, save/open time, and disk/temp usage.
- [x] **0.5d — Establish sampling procedure:** Use fixed viewport/detail, 5 cold launches, 10 warm launches, and 30 scripted interaction samples after warm-up; record all samples, medians/p95, failures, background load, and power settings. Tune statistically inadequate procedures explicitly before release.
- [ ] **0.5e — Calibrate admission limits:** Prototype payload caps, history/cache budgets, estimated working headroom, and pressure responses. Use SRS targets as starting gates; any changes need a rationale and synchronized spec update.

## 0.6 — Prove native shell, viewport, and worker transport

- [x] **0.6a — Compile a minimal native slice:** Run a Qt shell and a worker linked to the selected OCCT build. Create a known box in the worker and display it in the UI on each available candidate platform.
- [x] **0.6b — Prove the ownership boundary:** Transfer display meshes and entity IDs without a second authoritative BRep in the UI. Verify face/edge highlighting, picking, overlay placement, camera movement, and worker-side exact measurement.
- [x] **0.6c — Prove IPC limits and epochs:** Exercise version handshake, bounded CBOR, shared/chunked mesh transport, stale generations, session restart, lost acknowledgment, and cleanup of abandoned buffers.
- [ ] **0.6d — Evaluate the Mac renderer:** Measure supported Mac GPU/OS behavior, scaling, docking/resizing, and driver failure. Record whether OCCT/OpenGL meets the initial support gate and the future Metal-backed route; do not call a route implemented without evidence.
- [ ] **0.6e — Profile the slice:** Record idle/box/100-instance display footprint and navigation responsiveness on available hardware. This is feasibility evidence, not B2/B3 CAD acceptance.

## 0.7 — Prove sketch, feature, reference, and mate feasibility

- [x] **0.7a — Integrate the 2D solver:** Solve a dimensioned rectangle/circle fixture and an underdefined drag; diagnose a redundant and an inconsistent set; reject nonfinite/degenerate output. Record extraction/build changes and licenses for the pinned PlaneGCS revision.
- [x] **0.7b — Construct exact geometry:** Extrude and cut a valid profile, apply one constant-radius fillet, verify units/analytic invariants, and reject an invalid construction without corrupting the previous result.
- [x] **0.7c — Exercise topology evolution:** Change a parent dimension, split/delete a face, and present a symmetric ambiguous match. Verify history mapping where available and explicit ambiguity where it is not.
- [x] **0.7d — Prototype rigid assembly solving:** Ground one body and apply planar/axial mates to a second and third; check remaining freedom, consistent cycles, contradictory distance, rotation singularities, and undoable cancel. Select the numerical approach only after these results.
- [x] **0.7e — Record scope consequences:** Resolve solver/kernel failures with an alternative or an explicit R1 change. An unavailable feature may not remain represented as a completed capability.

## 0.8 — Prove durable storage and recovery boundaries

- [x] **0.8a — Prototype working/project storage:** Create a private working SQLite DB and a standalone `.cadproj` snapshot containing semantic records and required imported blobs. Demonstrate bounded streaming and foreign-key validation.
- [ ] **0.8b — Prove explicit save:** Publish a consistent sibling temporary snapshot with flush/replacement semantics on each platform filesystem; retain the prior valid destination on injected failure.
- [x] **0.8c — Kill at persistence boundaries:** Terminate before commit, after durable working commit, during snapshot creation, and after snapshot publication/before acknowledgment. Record which revision must recover and compare actual results.
- [x] **0.8d — Prove duplicate-writer and migration behavior:** Try aliases of one project, unsupported future schema, failed migration, and missing references. Refuse unsafe writes and preserve originals.
- [x] **0.8e — Confirm authoritative data retention:** Remove display caches and rebuild. Verify imported source geometry remains available and undo/recovery roots prevent deletion of required blobs.

## 0.9 — Record decisions and dependent work

- [x] **0.9a — Update ADRs:** For each SDD ADR, record selected option, exact versions, test results, consequences, and revisit trigger. A failed spike is evidence requiring a decision, not an acceptable hidden exception.
- [x] **0.9b — Freeze the first vertical slice:** Define the smallest production workflow for Modules 1–4: shell → planar sketch → extrusion → dimension edit → save/reopen → crash recovery. Map it to SRS requirements and actual tests.
- [x] **0.9c — Register deferred gates:** Explicitly assign unavailable platform, hardware, accessibility, advanced fixture, and installed-build checks to an owning module and release blocker. Do not declare Step Zero fully verified on untested platforms.
- [x] **0.9d — Synchronize documents:** Update SRS/SDD/TECH_SPEC/traceability with resolved facts; retain an evidence history for changed budgets or scope.

## 0.10 — Required module tests

- [x] **0.10a — T-00-SETUP:** Reproduce the minimal slice from a clean checkout using pins and documented commands. Expected: deterministic dependency selection, successful available-platform builds, and a correctly failing harness probe.
- [x] **0.10b — T-00-ISOLATION:** Run fixtures in disposable roots and inspect touched paths. Expected: production projects/recovery and existing research remain untouched; no real data/secrets tracked.
- [x] **0.10c — T-00-NATIVE:** Run shell/mesh/picking/measurement/worker-restart experiments. Expected: correct entity mapping, bounded transfer, stale reply rejection, and a still-responsive shell.
- [x] **0.10d — T-00-CAD:** Run solver/geometry/topology/mate spikes with known valid and invalid cases. Expected: correct invariants, explicit ambiguous/conflict states, and unchanged prior model on failure.
- [ ] **0.10e — T-00-STORAGE:** Run commit/save/migration crash matrix using real SQLite/files. Expected: acknowledged durable operations recover exactly once and prior saved data survives failures.
- [x] **0.10f — T-00-REVIEW:** Inspect version/license/support/evidence registers. Expected: each unresolved decision has an owner, no measured claim lacks a result, and unavailable platform tests remain clearly Blocked.

**Evidence file:** `docs/testing/module-0.md` — create during implementation, not fabricated by this specification.

**Pass criteria:** Applicable spikes and setup tests pass on available targets; blocking architecture failures are resolved. Cross-platform feasibility remains incomplete while a required platform is untested. Partial foundation work may proceed on verified targets if it does not depend on the unresolved decision; release support may not be claimed.

**Completion check:** A reproducible native slice, proven command/persistence ownership, viable geometry/solver/reference approach, bounded resource transport, isolated tests, and explicit support/decision records exist. Every required test has actual evidence. **Current result: Blocked — remaining platform/release feasibility gaps. Module 1 was subsequently authorized by the user on 19 October 2026; see testing/module-1.md.**
