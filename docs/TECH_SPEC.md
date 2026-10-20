# Technical Specification and Implementation Plan — Solidworks Clone

**Version:** 0.1 · **Date:** 19 October 2026  
**Status:** Modules 1–3 available-Mac scope implemented and scoped verification recorded; cross-platform matrix Blocked. Module 4.0 workspace UI is implemented/verified for available-Mac scope; exact features 4.1–4.4 have available-Mac implementation and scoped evidence; Modules 5–12 have not started; full product acceptance remains pending.
**Constraints:** Native macOS/Windows/Linux, local core, **8 GB system RAM minimum**.  
**Authority:** [SRS](SRS.md) → [SDD](SDD.md) → contracts below → ordered modules. [Step Zero](STEP_ZERO.md) is Module 0.

## 1. Execution and completion policy

Follow **Module 0 → Module 1 → … → Module 12**. Within a module follow numbered sections and lettered tasks in order. Parallel work is allowed only after its input contracts exist and independent ownership is clear; this plan does not authorize automatic delegation. Each lettered task is separately reviewable. All checkboxes start unchecked.

Every module has purpose, prerequisites, required tests, pass criteria, completion check, and an evidence file. Requirements that need a later module must remain pending there; a stub cannot satisfy an end-to-end requirement. A module may be partially implemented while still Not run/Blocked for native or platform checks. Do not mark it complete until applicable required evidence exists.

Test critical geometry, references, state transitions, durable writes, cancellation, and visible interactions. Prefer analytic invariants and independent expected results to tests that merely echo the implementation. Record actual commands and build/OS/fixture identifiers. Regression-test affected earlier behavior after changes; broad repeat runs need a concrete reason. All R1 requirements remain required until explicitly revised.

## 2. Dependency and build contract

| Component | Chosen direction | Step Zero pin/acceptance requirement |
|---|---|---|
| Language | C++20, RAII, explicit ownership | Compile all selected libraries with compatible ABI/runtime settings |
| UI | Qt 6 Widgets, Core, Gui, Network for local IPC, Test in test builds | Exact compatible patch and selected modules; no implicit GPL-only add-ons |
| Geometry | OCCT with required modeling/meshing/exchange/visualization modules | Immutable version/hash; omit unused optional media/VR modules |
| Sketch | PlaneGCS adapter | Immutable upstream revision, extraction/build dependencies, solver diagnostic corpus |
| Math / assembly | Eigen plus our bounded rigid mate layer | Exact version and selected solver/rotation parameterization after prototype |
| Persistence | Bundled SQLite | Exact version/configuration, foreign keys enabled, persistence crash tests |
| Build | CMake + Ninja | Exact supported versions and platform-specific presets |
| Tests | CTest, selected lightweight C++ runner, Qt Test, native/manual harness | Confirm failure exit status and native coverage before claiming support |

Do not choose the newest library merely because a live documentation URL points to it. Record patches, compiler flags, dependency hashes, license notices, and build options in a checked-in manifest. Updates require a deliberate dependency change and affected geometry/reference/recovery/performance tests.

### 2.1 Proposed source layout

```text
apps/desktop/                  Qt shell, property panels, UI projections
apps/cad-worker/               Worker host, local IPC, scheduling
src/domain/                   IDs, units, documents, commands, expressions
src/evaluation/               Feature graph, rebuilds, result fingerprints
src/references/               Topology tracking and repair
src/sketch/                   Entity/constraint model and solver adapter
src/assembly/                 Occurrences, mates, interference
src/drawing/                  Sheets, projections, annotations, BOM/PDF
src/persistence/              SQLite, snapshots, recovery, migrations
src/exchange/                 STEP, IGES, STL adapters
src/geometry/occt/            Kernel operations and validity checks
src/render/                   Renderer boundary, meshes, picking, overlays
src/protocol/                 DTOs, CBOR framing, limits, versions
tests/unit/                   Pure domain/math behavior
tests/integration/            Real kernel/SQLite/IPC/process tests
tests/native/                 Installed/native interaction procedures
tests/fixtures/               B0–B6 generators and manifests
cmake/                        Dependency and platform configuration
docs/testing/                 Actual module/acceptance evidence
docs/                         This specification set
research/                     Existing evidence, preserved
```

The layout now exists. Module 0 experiments live in tests; source/application folders remain reserved for production modules. Build presets and actual results are documented in [cmake/README](../cmake/README.md) and Module 0 evidence. Unrun platform presets are candidate commands, not support claims.

## 3. Protocol and command contracts

### 3.1 Control envelope

```text
Frame: uint32 big-endian byte length + CBOR map
Initial control-frame cap: 1 MiB, checked before allocation
Request: protocol_version, request_id, operation_id, worker_epoch,
         project_id, document_id, base_revision, command_session_id,
         preview_generation, command_type, payload
Reply:   correlation fields, result_revision, status, diagnostics,
         data | display_manifest
```

Protocol v1 forbids native pointers, arbitrary file paths as shared-buffer handles, unbounded recursion, and unknown command execution. Initial limits: nesting ≤32, strings ≤16 KiB unless a type declares a smaller cap, arrays ≤100,000 elements, and at most 32 outstanding ordinary control requests per UI session. Validate typed field ranges as well as frame size. Store these limits in one versioned configuration; oversized legitimate work needs a chunked/batched contract, not disabled checks.

Large display payloads use ≤32 MiB shared-buffer segments with an initial aggregate UI display-transfer budget of 256 MiB. Validate offset+length overflow, index range, finite vertex values, triangle counts, revision/epoch, checksum, and ownership before GPU upload. A bounded chunked fallback must obey the same total budget. CPU/GPU cache budgets are separate and measured. These limits are proposed and calibrated in Step Zero.

### 3.2 Command families

| Command | Inputs | Success result / rejection conditions |
|---|---|---|
| `CreateDocument` | Type, name, units | New stable ID and revision; reject unsupported type/units |
| `PreviewFeature` | Session/generation, typed definition, references | Temporary mesh/diagnostics; reject stale base or invalid input |
| `CommitFeature` | Operation ID, base revision, full typed definition | One durable semantic revision; identical repeated operation ID returns prior result |
| `EditParameters` | Feature ID, typed values/expressions | Rebuilt result or explicit failure/stale dependents |
| `ResolveReference` | Owner/role, chosen replacement, current base revision | Undoable explicit replacement or invalid-context rejection |
| `Undo` / `Redo` | Document ID, base revision | New durable revision or documented history-boundary result |
| `InsertOccurrence` / `EditMate` | Definition/occurrence IDs, transforms/references | Valid occurrence/mate change; no occurrence/assembly cycles |
| `CreateDrawingView` | Model revision, projection, sheet placement/scale | Revision-tagged view or explicit unsupported/stale status |
| `SaveProject` | Destination capability, requested revision, operation ID | Published saved revision and checksum; newer edits remain modified |
| `ImportGeometry` / `ExportGeometry` | Chosen file capability, units/options, target revision | Validated result or preserved source/destination on failure |
| `CancelJob` / `QueryOperation` | Job or operation ID + session | Actual cancel/commit/outcome state; never invented success |

Other R1 commands use the same envelope/validation rules. An operation ID is bound to the canonical request content; reusing it with different content is an error. Durable commit records and their outcome lookup survive worker restart. Request IDs correlate transport messages; they are not durable idempotency IDs.

### 3.3 Errors and state

Structured error fields: `code`, human message key, affected entity IDs, operation ID, retryability, recoverable revision, technical detail reference. Minimum codes: `InvalidInput`, `UnitMismatch`, `UnsupportedOperation`, `StaleRevision`, `StaleSession`, `ConstraintConflict`, `SolveFailure`, `InvalidGeometry`, `ReferenceMissing`, `ReferenceAmbiguous`, `DependencyCycle`, `ResourceLimit`, `StorageFull`, `PermissionDenied`, `ProjectLocked`, `IncompatibleSchema`, `ProtocolViolation`, `WorkerUnavailable`, and `Canceled`.

Display states must distinguish **Preview**, **Computing**, **Committed in memory**, **Recoverable revision N**, **Saving revision N**, **Saved revision N**, and **Failed**. A commit is not acknowledged as recoverable until the working database transaction is durable. A transient in-memory candidate may be discarded after failed persistence. “Saved” applies to the current model only if its revision equals the published snapshot revision.

## 4. Persistence and data contracts

### 4.1 Logical schema

The migration implementation will materialize these logical tables with typed constraints and indexes:

| Table | Key / required content |
|---|---|
| `project_meta` | Singleton project ID, schema/format version, manifest, saved-generation metadata |
| `documents` | Document ID, type, name, units, revision |
| `entities` | Stable entity ID unique inside the project DB, document ID, kind, definition version; cross-project handles also carry project ID |
| `features` | Entity ID FK, feature type/version, ordinal, typed definition, suppression |
| `dependencies` | Owner and target entity FKs, semantic role; indexed both directions |
| `parameters` | Owner FK, stable name, dimension/type, literal or parsed expression definition |
| `references` | Owner/target context, role, reference token, resolution state |
| `sketch_entities` / `constraints` | Entity FK, geometry/constraint type and version, bounded typed definition |
| `occurrences` / `mates` | Entity FK, referenced definition, transform / mate roles and value |
| `drawing_views` / `annotations` | Entity FK, source revision, sheet/projection/anchor metadata |
| `objects` | Content hash, kind, encoding/version, size, payload BLOB, authoritative/cache classification |
| `object_refs` | Owner/revision/history/job root → object FK |
| `commands` | Unique operation ID, request hash, base/new revision, intent/inverse, durable outcome |
| `evaluation_results` | Feature/revision/fingerprint, object refs, validity and diagnostics |
| `migrations` | Applied version, migration identity/checksum, timestamp |

Foreign keys enforce identity integrity; application validators enforce domain rules and acyclic graphs. Validate typed definitions on read and write even when the database is locally produced. Prefer explicit columns for indexed/query-critical fields and bounded versioned CBOR for variable command-specific fields. Do not store executable Python/JavaScript or deserialize arbitrary native objects.

### 4.2 Database and file behavior

- Working DB: one worker writer, `foreign_keys=ON`, WAL journal, `synchronous=FULL`, bounded busy policy, short write transactions. Verify actual settings on open.
- Saved `.cadproj`: self-contained snapshot without required external WAL/SHM sidecars. Use backup/snapshot APIs; do not byte-copy a changing working DB.
- Pin and verify the actual source revision throughout a save. If the requested revision is no longer available when the operation starts, return a revision error or offer the current revision explicitly. Newer edits may continue only when snapshot isolation and WAL growth have been proven safe; they cannot change the revision advertised by the saved snapshot.
- Initial project admission cap: 2 GiB saved file; individual authoritative object cap: 128 MiB; source exchange cap: 100 MiB. Supported acceptance workload B4 remains smaller. Check declared, streamed, and expanded sizes. Revisit caps only with measured memory/storage evidence.
- Stage large data to a private bounded temporary stream, hash it, then use incremental SQLite blob writes. Account for staging, working DB/WAL, temporary snapshot, previous destination, and retained backups before admission.
- Required source blobs must commit with references. Cache removal cannot remove regeneration inputs. GC marks retained revisions/undo/jobs before sweeping unreferenced blobs.
- Save As creates a new portable project identity while preserving entity/document identity within that copy, remaps project-scoped dependencies, and keeps the original unchanged. Linked-vs-copied dependencies must be explicit.
- Portable copy packages selected dependency closure inside a new snapshot and records source identities/revisions. Missing/unsupported references prevent a false “complete” result.
- Migration operates on a copy, checks version compatibility, validates, and publishes atomically. Never upgrade the only good file in place.

## 5. Modeling and interaction contracts

### 5.1 Initial feature parameter schemas

| Feature | Required R1 options | Deliberately deferred options |
|---|---|---|
| Sketch | Planar support, entity definitions, constraints, construction flags | 3D sketches and spline constraints |
| Extrude / Cut | Closed profile(s), finite depth, forward/reverse/symmetric extent, add/new-body or cut target | Up-to-surface, draft, thin feature |
| Revolve / Revolve Cut | Closed profile, in-plane axis, angle in (0, 2π], direction, operation/target | Thin revolve and arbitrary invalid self-intersection |
| Fillet | Supported selected edges, positive constant radius | Variable radius and face fillets |
| Chamfer | Supported selected edges, equal positive distance | Two-distance and angle-distance variants |
| Hole | Planar support, center locations, diameter, blind depth or through-all target | Full thread/countersink/counterbore standard library |
| Linear Pattern | Source feature/body, direction, integer count ≥1, spacing | Arbitrary geometry-driven patterns |
| Circular Pattern | Source feature/body, axis, integer count ≥1, angular spacing/full distribution | Curve/table patterns |
| Mirror | Source feature/body, plane, copy/fuse intent where valid | Arbitrary deformation |

Define body/feature pattern semantics explicitly; a graphics-only duplicate is not an editable Boolean feature. Count/spacing/radius bounds and invalid combination checks are part of the schema. Feature deletion/reordering must first analyze dependencies and show the proposed impact.

### 5.2 Selection, units, and expressions

Selection tokens include document/occurrence context, entity role, topology token, and result revision. Filters constrain valid candidate types. Hover, committed selection, preview inputs, and unresolved references have distinct appearances plus text/status cues.

Expressions support numeric literals with units, named scalar references, arithmetic, parentheses, and an allowlisted set of dimensionally meaningful math functions. No general code evaluation. Initial bounds: ≤4 KiB expression text, ≤256 AST nodes, ≤32 nesting levels. Validate cycles, unit dimensions, domain errors, and finite results before geometry work.

Use SI semantic values and one kernel-unit adapter as specified in SDD. Sketch plane transforms must be orthonormal within tolerance; invalid transforms are rejected. Tolerance selection is scale-aware and recorded with outputs/import warnings.

### 5.3 UI state machine

`Idle → SelectingInputs → EditingParameters → PreviewPending → PreviewReady → CommitPending → Idle`; invalid/failure states preserve editable inputs. Cancel releases previews and restores the last committed context. Closing/switching a document resolves an active command explicitly; it may not apply it silently.

Enter accepts only when inputs are valid and keyboard focus permits command acceptance; in a multiline/text editor it retains that editor's expected behavior. Escape first closes a transient picker/dropdown, then cancels the active command, then clears ordinary selection. Repeated Escape must not delete features. Record actual platform shortcut differences.

Use a command registry with ID, label, aliases, icon, supported contexts, input schema, availability reason, shortcut, and property-editor factory. Both layouts and command search use it. Accessible names and focus order belong to the actual widgets. Property-editor control schemas cannot replace specialized geometry interaction.

### 5.4 Assembly and drawing contracts

Rigid transforms are stored as normalized rotation + translation with explicit conventions. Mate references are occurrence-scoped; component-definition IDs alone cannot identify which bolt is constrained. Grounding eliminates global freedom. Remaining freedom/conflicts refer to the actual solved system.

Drawing sheet coordinates use explicit physical units and view scale. Model dimensions query exact geometry at a known source revision; overridden display text is visibly marked and does not alter geometry. BOM rows distinguish part definitions and occurrence quantities, with an explicit suppression policy. PDF acceptance measures known lengths on a page and compares text values to model dimensions.

## 6. Implementation modules

| Module | Area | Prerequisites |
|---|---|---|
| 0 | Repository, toolchain, fixtures, feasibility | Specifications and research |
| 1 | Native shell, viewport, worker, command transport | 0, relevant feasibility gates |
| 2 | Document model, persistence, recovery foundation | 1 |
| 3 | Planar sketching, constraints, expressions | 2 |
| 4 | Exact part features and measurements | 3 |
| 5 | Rebuilds, topology references, repair | 4; dependency/reference contracts apply earlier |
| 6 | Neutral exchange and portable copies | 5 |
| 7 | Rigid assemblies and mates | 5–6 |
| 8 | Associative drawings and PDF/BOM | 7 |
| 9 | Unified interaction, accessibility, offline/security | 1–8; boundaries apply throughout |
| 10 | Recovery/migration/undo fault hardening | 2–9; basic recovery is not deferred until here |
| 11 | Real 8 GB performance and cross-module acceptance | 0–10 |
| 12 | Packaging, upgrades, installed-build validation | 11 |

### Module 0 — Step Zero

**Purpose:** Prove the foundations and resolve blocking architecture choices.  
**Prerequisites:** Specifications and preserved research.  
**Tasks and required tests:** Follow [0.1a–0.10f in STEP_ZERO](STEP_ZERO.md). Do not maintain a second divergent copy of those checkboxes.  
**Test pass criteria:** Reproducible native slice, valid solver/kernel/reference/storage experiments, isolated harness, and honest per-platform evidence.  
**Completion check:** Step Zero completion check is satisfied; unresolved required platform gates remain visible.  
**Evidence:** `docs/testing/module-0.md`. **Current status:** Blocked (partial available-target feasibility; unavailable platform gates remain open).


### Module 1 — Native shell, viewport, and local worker

**Purpose:** Deliver a working cross-platform native workspace with revision-aware rendering and an isolated CAD process.  
**Prerequisites:** Module 0 native/IPC/renderer gates; resolved data ownership and candidate support matrix.  
**Requirement/design anchors:** FR-001, FR-003–FR-004, FR-035; SDD §§2, 4–5, 10–11.

#### 1.1 — Build the application frame

- [x] **1.1a:** Create the Qt main window, dock layout, document tabs, command registry, and persistent layout preferences; use original icons and clear unavailable-command reasons.
- [x] **1.1b:** Create explicit UI state for active document, edit mode, selection, command session, preview generation, and save/job status. Avoid an independent mutable model in each panel.

#### 1.2 — Implement worker lifecycle and transport

- [x] **1.2a:** Start one CAD worker using private local IPC, negotiate versions/epoch, validate typed envelopes/limits, and expose correlated errors.
- [x] **1.2b:** Implement preview coalescing, bounded requests, operation-outcome query, disconnect/restart, and abandoned buffer cleanup. Worker death must leave the UI able to offer recovery.

#### 1.3 — Implement the viewport boundary

- [x] **1.3a:** Display worker-generated meshes, instances, cameras, highlights, and selection IDs; route exact geometry queries to the worker.
- [x] **1.3b:** Support orbit/pan/zoom/fit/view modes, basic overlays, resize/docking, and stale-result rejection; measure the native slice on each available platform.

#### 1.4 — Required module tests

- [x] **1.4a — T-01, T-03, T-04, T-35, NFR-013 / T-53:** Verify shell placement, navigation, synchronized selection, explicit preview states, and architectural ownership on real native builds.
- [x] **1.4b — Protocol failure corpus:** Inject old epochs, out-of-order preview generations, lost acknowledgments, malformed control frames, invalid mesh indices, oversized allocation requests, and worker termination. Expect rejection/cleanup without UI model corruption.
- [ ] **1.4c — Native graphics matrix:** Verify picking/overlays at supported scales and X11/Wayland where applicable. Record actual Mac/Windows/Linux results separately; a box display is not part-modeling acceptance.

**Test pass criteria:** A usable native shell renders correct revision-tagged data, rejects stale/malformed messages, and survives worker exit on every currently verified target.

**Completion check:** UI/worker/render contracts work with real processes; untested platform rows remain pending.

**Evidence:** `docs/testing/module-1.md`. **Current status:** 8 lettered tasks Pass on the available Mac reference-workspace scope; 1.4c Blocked pending other native platforms/hardware. Subsequent Module 2 work is recorded below.


### Module 2 — Document semantics, persistence, and recovery foundation

**Purpose:** Establish stable document identity, transactional commands, and real save/reopen before adding CAD breadth.  
**Prerequisites:** Module 1; Step Zero durability/locking/migration experiments.  
**Requirement/design anchors:** FR-002, FR-006–FR-010, FR-034, FR-039; SDD §§3–4, 9.

#### 2.1 — Implement documents and semantic transactions

- [x] **2.1a:** Implement part/assembly/drawing records, entities, typed parameters, units, revisions, and project identity; validate all incoming mutations in the worker.
- [x] **2.1b:** Add operation-id deduplication bound to request hash and revision. Implement document create/switch/close, command deltas, and basic undo/redo without retaining full BRep copies per edit.

#### 2.2 — Implement working and saved storage

- [x] **2.2a:** Materialize schema/indexes/foreign keys, bounded object streams, private working DB, durable command commits, and authoritative-vs-cache object classification.
- [x] **2.2b:** Implement standalone snapshot publication, Save As, saved/current revision tracking, canonical writer locks, version rejection, and a migration-copy skeleton.

#### 2.3 — Implement early recovery

- [x] **2.3a:** Recover committed semantic revisions after worker restart and offer saved versus recoverable versions on application restart.
- [x] **2.3b:** Add retained-root tracking for active jobs, history, and recovery; support storage errors without inventing success or deleting the previous valid project.

#### 2.4 — Required module tests

- [x] **2.4a — T-02, T-06–T-10:** Create all document kinds, persist identities and units, switch/close with unsaved changes, replay duplicate commands, and exercise undo/redo boundaries.
- [ ] **2.4b — T-07–T-08, T-34, T-39, NFR-011 / T-51:** Use real temporary SQLite/files to kill before/after working commit and saved-snapshot publication; expect exactly the documented durable/saved revision and intact originals.
- [ ] **2.4c — Writer/schema/storage failures:** Try alias paths, a live second writer, newer schema, failed migration, disk full, denied write, and missing blob. Expect safe refusal and an actionable recovery path.

**Test pass criteria:** Durable acknowledgment, saved-file publication, identities, locking, and recovery match the contract with actual filesystem/process fault evidence.

**Completion check:** Document/persistence tests pass; later geometry and dependency acceptance is explicitly still pending.

**Evidence:** `docs/testing/module-2.md`. **Current status:** 7 lettered tasks Pass for the documented available-Mac foundation; 2.4b/2.4c Blocked for full native platform completion despite passing Mac cases. Windows/Linux and physical power-loss checks remain Blocked. Module 3 was subsequently authorized; its separate evidence follows.


### Module 3 — Planar sketches, constraints, and expressions

**Purpose:** Create editable dimensioned sketches with trustworthy conflict and solve feedback.  
**Prerequisites:** Module 2; viable pinned 2D solver adapter from Module 0.  
**Requirement/design anchors:** FR-012–FR-016; SDD §§3, 7–8, 10.

#### 3.1 — Create planar sketch editing

- [x] **3.1a:** Implement support-plane coordinate frames, points/lines/polylines/rectangles/circles/arcs, construction flags, snapping, and persistent sketch IDs.
- [x] **3.1b:** Integrate viewport/entity selection, dimensions, truthful empty/edit context, tool-specific fields, and command previews with semantic transactions. Circle pointer input must press at the center, resize continuously while held, and finish into the local draft on release; Escape cancels the in-progress primitive or leaves the drawing tool while retaining completed draft geometry; explicit Cancel Sketch discards the whole edit. Line/polyline/rectangle input must show the pending segment or opposite corner before completion. Numeric creation remains precise and separate from pointer snapping. Reopened on 20 October 2026 for a stable camera during viewport/dock resizing, screen-distance snapping with an optional grid, click/drag creation, trackpad pan/pinch, direct selection properties/handles, bounded draft undo/redo, and tool cancellation that preserves completed draft geometry.

#### 3.2 — Integrate constraints and diagnosis

- [x] **3.2a:** Map supported constraint/entity combinations to solver variables with defined ownership; implement fixed, geometric, and dimensional constraints.
- [x] **3.2b:** Validate finite outputs, residuals, radii, degeneracy, degrees of freedom, redundancy/conflict reports, and drag branch continuity; preserve the accepted state on failure.

#### 3.3 — Implement units and expressions

- [x] **3.3a:** Implement SI semantics, display-unit parsing/formatting, dimensional arithmetic, name references, AST limits, and cycle detection without general code evaluation.
- [x] **3.3b:** Make dimension editing, constraint deletion, undo/redo, save/reopen, and canceled dragging coherent operations.

#### 3.4 — Required module tests

- [x] **3.4a — T-12–T-16:** Run valid, underdefined, fully defined, redundant, contradictory, singular, and degenerate sketches; verify geometric residuals and declared diagnostics.
- [x] **3.4b — Unit/expression corpus:** Check mm/inch conversion, degrees/radians, mixed-unit arithmetic, invalid dimensions, negative radii, nonfinite numbers, cyclic names, and AST limits.
- [ ] **3.4c — Native interaction and persistence:** Create B1 sketches through the UI; cancel a drag/edit, undo/redo, save/reopen, and verify identities/values. Check actual focus and keyboard behavior. Exercise circle press/move/release at multiple radii, transient cancellation, zero-size refusal, tool switching, numeric creation, and line/rectangle live previews; verify no project revision changes before explicit sketch acceptance.

**Test pass criteria:** Supported sketches solve and edit correctly; invalid/ambiguous numerical outcomes are exposed without changing accepted geometry.

**Completion check:** B1 sketch stage works natively and offline with real solver and persistence evidence.

**Evidence:** `docs/testing/module-3.md` and the separate interaction repair reports. **Current status:** 3.1b repairs and expanded direct-editing/draft-history checks Pass for available-Mac scope; see `docs/testing/module-3-input-repair.md` and the combined workspace run. Historical results remain scoped to their original coverage. 3.4c is Blocked for full native platform completion; actual Mac interaction evidence is recorded separately. Windows/Linux and OS-network-disabled acceptance remain Blocked. Later authorization initially covered 4.0a–c workspace UI; the subsequent explicit Module 4 instruction now covers 4.1a onward and stops before Module 5.


### Module 4 — Exact part features and measurement

**Purpose:** Align the native modeling workspace, then convert design definitions into valid precise solids with useful editing and measurement.  
**Prerequisites:** Module 3; prototype topology/history contract from Module 0. Full repair UX arrives in Module 5.  
**Requirement/design anchors:** FR-001, FR-003–FR-005, FR-017–FR-018, FR-022, FR-025; SDD §§6–8, 10–11.

#### 4.0 — Align the native modeling workspace before adding solid tools

The user authorized the proposed implementation order on 20 October 2026: repair Module 3 interaction first, then execute 4.0a → 4.0b → 4.0c and verify available targets. That historical authorization covered workspace tasks only. The subsequent instruction explicitly authorizes all remaining Module 4 tasks, starting with 4.1a; stop before Module 5. Existing feature task IDs remain unchanged. See the implementation scope record.

- [x] **4.0a:** Introduce SolidWorks-style **Features / Sketch / Evaluate command tabs** using the existing shared action registry, menus, search, shortcuts and real command availability. Group discoverable sketch tools and contextual commands with original icons and text. Unimplemented operations must remain unavailable with reasons; no decorative functioning controls. Preserve native macOS global menus and native Windows/Linux behavior.
- [x] **4.0b:** Put the active **PropertyManager on the left**, sharing the manager region with the model/feature tree. Provide a command title, selection prompts, contextual fields, persistent accept/cancel and diagnostics. Show planes, origin and sketches under the active document, adding feature/history nodes as their engines arrive. Keep the central viewport dominant, the right resource Task Pane optional/collapsible, and documents/units/edit/save/job status along the bottom. Preserve stable IDs, tree state, keyboard focus and worker transaction ownership when switching panels.
- [ ] **4.0c — T-01, T-03–T-05 layout acceptance:** Compare actual native screenshots and workflows against the preserved UI atlas and SDD §10. Verify command discovery, switching tree/PropertyManager, circle creation/edit/cancel, disabled reasons, pan/zoom/selection, document switching and save status at 1280×800 and 100/125/150/175/200% scaling. Test supported native platforms when available; mark unavailable environments Blocked. Record deliberate differences from SolidWorks and check for clipped controls, inaccessible actions and extra geometry/project writes.

#### 4.1 — Implement extrusion and revolve

- [x] **4.1a:** Implement the exact R1 schemas for add/new-body/cut extrusion and revolve, profile validation, axis/direction/extent controls, and preview/commit.
- [x] **4.1b:** Validate units, body count, closure, self-intersection, tolerance growth, and kernel errors; preserve last committed geometry on rejected operations.

#### 4.2 — Implement the remaining initial features

- [x] **4.2a:** Add constant-radius fillet, equal-distance chamfer, simple hole, linear/circular pattern, and mirror in the declared supported cases.
- [x] **4.2b:** Emit provenance/history maps for generated/modified/deleted topology as each operation is introduced. Unsupported variants stay unavailable with explanations.

#### 4.3 — Implement measurements and material density

- [x] **4.3a:** Query exact lengths/angles/radii/area/volume/bounds and optional density-based mass at an explicit result revision.
- [x] **4.3b:** Integrate feature parameter editing, previews, per-feature timing, cancel paths, and initial history transactions; retain worker ownership of exact BRep.

#### 4.4 — Required module tests

- [x] **4.4a — T-17–T-18, T-22, T-25:** Compare known boxes, cylinders, bracket dimensions/volume, hole counts, and supported feature outcomes to analytic expected values; verify exact output validity.
- [x] **4.4b — NFR-007–NFR-008 / T-47–T-48:** Change mesh detail and display mode; expect unchanged exact measurements/precise results. Test unit scales, tolerances, invalid radius and self-intersecting profiles.
- [ ] **4.4c — Native B1 vertical slice:** Sketch → extrude → cut/pattern → fillet → dimension edit → undo/redo → save/reopen → worker restart. Expect correct shape, parameters, and durable state.

**Test pass criteria:** The 4.0 native command-tab/left-PropertyManager layout checks pass for declared targets, and every supported initial feature has valid, invalid, cancel, persistence, and analytic-invariant coverage.

**Completion check:** The planned modeling layout and B1 modeling slice pass with native evidence; large history/reference mutation corpus is owned by Module 5.

**Evidence:** `docs/testing/module-4.md`. **Current status:** 4.0a/4.0b Pass for available-Mac scope; 4.0c Blocked for full platform/device acceptance despite passing Cocoa workflows and screenshots at all five effective scales. 4.1a–4.4b Pass for the available-Mac scope in the task register. 4.4c has passing native B1 evidence at all five effective scales but remains Blocked for full native platform/device completion. Dependency graphs/reference repair are unstarted. Stop before Module 5 (5.1a).


### Module 5 — Dependencies, rebuilds, topology references, and repair

**Purpose:** Make model edits dependable instead of relying on incidental face order.  
**Prerequisites:** Module 4 and provenance emitted by every feature adapter.  
**Requirement/design anchors:** FR-009, FR-019–FR-021; SDD §§3–4, 6, 9, 12.

#### 5.1 — Implement dependency evaluation

- [ ] **5.1a:** Build explicit feature/expression/reference dependencies; implement affected-descendant invalidation, cycle rejection, versioned fingerprints, and deterministic evaluation order.
- [ ] **5.1b:** Implement rename/suppress/delete/reorder with impact preview, valid dependency constraints, and visibly stale last-valid results after failures.

#### 5.2 — Implement reference evolution and repair

- [ ] **5.2a:** Resolve semantic references using kernel history plus constrained geometry/context matching; represent split/merge/deleted and ambiguous outcomes explicitly.
- [ ] **5.2b:** Build a repair workspace showing owner, former role, candidate geometry, and affected dependents; replacement is an explicit undoable command.

#### 5.3 — Make history and caches coherent

- [ ] **5.3a:** Bound immutable result caches, prevent stale-revision export/measurement, and preserve authoritative inputs during trimming.
- [ ] **5.3b:** Complete B2 history, multi-document dependency changes, undo/redo retention, and recoverable semantic rebuild after worker restart.

#### 5.4 — Required module tests

- [ ] **5.4a — T-19–T-21:** Edit upstream dimensions, suppress/delete/reorder features, introduce cycles, and verify affected rebuild sets, stale markers, and unchanged unrelated branches.
- [ ] **5.4b — Topology failure corpus:** Split/delete/merge faces, vary pattern counts, and create symmetric candidates. Expect correct supported history tracking and explicit ambiguity rather than arbitrary reassignment.
- [ ] **5.4c — T-09, NFR-009, NFR-012 / T-49, T-52:** Run B2 edit/undo/redo and repeated equivalent rebuilds, compare invariants within tolerance, inspect retained roots and post-trim live objects.

**Test pass criteria:** Dependency-invalid edits are rejected and topology changes produce correct references or explicit repair needs; no false-current result is published.

**Completion check:** B2 semantic/rebuild correctness and topology mutation corpus pass with real engine evidence.

**Evidence:** `docs/testing/module-5.md`. **Current status:** Not run.


### Module 6 — Neutral exchange and portable project copies

**Purpose:** Exchange useful exact/mesh data without misrepresenting recovered design intent.  
**Prerequisites:** Module 5; bounded stream and snapshot mechanisms from Module 2.  
**Requirement/design anchors:** FR-011, FR-023–FR-024, FR-038; SDD §§8–9, 11–13.

#### 6.1 — Implement background import

- [ ] **6.1a:** Import STEP/IGES with units, provenance, source blob retention, shape validity, warnings, and explicit healing choices.
- [ ] **6.1b:** Expose bounded job progress/cancellation, source-size admission, parser failure, and peak memory behavior; preserve source files.

#### 6.2 — Implement exports

- [ ] **6.2a:** Export precise STEP/IGES from a declared valid revision and STL from explicit units/detail settings.
- [ ] **6.2b:** Stream to temporary output and publish only on success; compare round-trip invariants and report unsupported/lost information.

#### 6.3 — Implement dependency closure and portable copy

- [ ] **6.3a:** Collect assembly/drawing/source dependencies by identity/revision, report missing references, and embed selected required definitions/blobs in a new snapshot.
- [ ] **6.3b:** Make linked-versus-copied sources, new project identity, manifest, and overwrite behavior explicit; complete richer assembly/drawing cases in Modules 7–8.

#### 6.4 — Required module tests

- [ ] **6.4a — T-23–T-24:** Run B4 STEP/IGES imports and exports with correct/wrong units, valid and defective geometry, cancellation, and translation warnings. No proprietary feature-history claim is allowed.
- [ ] **6.4b — T-11:** Make a portable part project, remove original source locations, reopen the copy, and check manifest closure; repeat assembly/drawing coverage when those modules exist.
- [ ] **6.4c — T-38, NFR-016 / T-56:** Feed truncated, oversized, high-expansion, and parser-failure fixtures; inject disk full at export. Expect bounded rejection and preserved valid destinations.

**Test pass criteria:** Supported exchange preserves declared invariants, reports translation limits, and handles failure without partial publication.

**Completion check:** B4 exchange correctness passes; assembly/drawing dependency cases remain assigned to their later modules.

**Evidence:** `docs/testing/module-6.md`. **Current status:** Not run.


### Module 7 — Rigid assemblies, mates, loading, and interference

**Purpose:** Compose shared part definitions into editable, diagnosed rigid assemblies.  
**Prerequisites:** Modules 5–6; feasible 3D mate approach from Module 0.  
**Requirement/design anchors:** FR-026–FR-030, FR-011; SDD §§3, 6–7, 11–12.

#### 7.1 — Implement occurrence and loading semantics

- [ ] **7.1a:** Insert part/subassembly occurrences with stable IDs, shared definitions, transforms, grounding, and cycle checks.
- [ ] **7.1b:** Implement distinct hidden/suppressed/unloaded/unresolved/stale states, coarse-first display, resolve on demand, and bounded definition caches.

#### 7.2 — Implement mates and update propagation

- [ ] **7.2a:** Implement the declared rigid planar/axial mate set with solver freedom/conflict diagnostics, drag/preview/cancel, and deterministic accepted transforms.
- [ ] **7.2b:** Propagate part changes to affected mates/results, support explicit component replacement/relink, and preserve occurrence context in references.

#### 7.3 — Implement interference and portability

- [ ] **7.3a:** Compute supported exact solid interference with touching/overlap/failure distinctions and result highlights.
- [ ] **7.3b:** Complete assembly dependency-copy/BOM-source metadata and persistence/recovery of occurrences/mates; benchmark B3 while correctness is being developed.

#### 7.4 — Required module tests

- [ ] **7.4a — T-26–T-29:** Build B3, verify independent occurrence transforms and shared definitions, ground/mate parts, check free motion, conflict cycles, broken references, replacement, and undo.
- [ ] **7.4b — T-30:** Use known touching, disjoint, overlapping, and unsupported bodies; compare expected intersections and ensure failed evaluation is not reported as no interference.
- [ ] **7.4c — T-11, T-28, NFR-002 / T-42:** Open portable assemblies without originals; measure loading/instance/cache behavior and verify unload/reload does not lose authoritative inputs.

**Test pass criteria:** Supported mates and occurrence updates behave correctly, broken relationships are diagnosable, and sharing/loading reduces duplication without changing results.

**Completion check:** B3 correctness, mate conflict corpus, portable assembly copy, and restart recovery pass.

**Evidence:** `docs/testing/module-7.md`. **Current status:** Not run.


### Module 8 — Associative drawings, annotations, BOM, and PDF

**Purpose:** Generate engineering documentation tied to actual model identity and revision.  
**Prerequisites:** Module 7 plus exact projection/reference and portable-copy services.  
**Requirement/design anchors:** FR-031–FR-033, FR-011; SDD §§3, 6, 10–11.

#### 8.1 — Implement sheets and model views

- [ ] **8.1a:** Create sheet sizes, scale, units, projection convention, and associative base/projected/isometric views with hidden-line processing.
- [ ] **8.1b:** Track view-source revisions and update status; maintain physical sheet coordinates and current/stale semantics.

#### 8.2 — Implement dimensions and BOM

- [ ] **8.2a:** Add supported dimensions, center marks, text, annotation anchors, and dangling-reference handling; distinguish display override from measured value.
- [ ] **8.2b:** Generate BOM rows/quantities using an explicit occurrence/suppression policy and stable part identifiers.

#### 8.3 — Implement PDF and portable drawing projects

- [ ] **8.3a:** Export vector paths/text at correct page size/scale; disclose any raster fallback and reject unsupported/stale-current claims.
- [ ] **8.3b:** Complete drawing dependency closure, save/reopen/undo, and source-change update workflows with recoverable transactions.

#### 8.4 — Required module tests

- [ ] **8.4a — T-31–T-33:** Run B5 and verify projections, sheet scale, measured lengths, dimension values, font/text rendering, BOM counts, and PDF page geometry.
- [ ] **8.4b — Associativity and missing-source corpus:** Edit/delete source geometry and components; expect updated or explicitly dangling annotations, current/stale view status, and no silently reassigned dimensions.
- [ ] **8.4c — T-11, AC-05:** Copy the complete drawing project, remove originals, reopen, update, export, and inspect the actual PDF. A screenshot-only print does not pass.

**Test pass criteria:** B5 produces accurate associative views/annotations/BOM and a visually checked, scale-correct PDF.

**Completion check:** Drawing model changes, broken references, portable copy, persistence, and PDF output pass end to end.

**Evidence:** `docs/testing/module-8.md`. **Current status:** Not run.


### Module 9 — Unified interaction, accessibility, offline operation, and input safety

**Purpose:** Make the implemented CAD workflow consistent, discoverable, accessible, and independent of network services.  
**Prerequisites:** Modules 1–8; offline and input-validation boundaries apply from their introduction.  
**Requirement/design anchors:** FR-005, FR-036–FR-038; NFR-010, NFR-014; SDD §§5, 10, 13.

#### 9.1 — Unify command interactions

- [ ] **9.1a:** Complete shared property-panel grammar, selection-role prompts, reasons for disabled controls, command search/aliases, and shortcut configuration across part, assembly and drawing commands. Build on the command tabs and left PropertyManager delivered in 4.0; do not defer the basic modeling layout until this module.
- [ ] **9.1b:** Complete familiar/simplified layouts using identical commands and model semantics; verify Enter/Escape/focus/transient-dialog behavior with real workflows. Recheck the 4.0 layout against the UI atlas across all document types, dock restoration, keyboard traversal and accessibility; preserve native platform conventions.

#### 9.2 — Verify accessibility and platform conventions

- [ ] **9.2a:** Provide accessible widget names/roles, keyboard traversal, non-color status cues, scalable controls, and readable selection/error states.
- [ ] **9.2b:** Check Mac/Windows/Linux conventions, native file dialogs, dock layouts at 1280×800 logical resolution, and 100–200% scaling.

#### 9.3 — Harden offline, validation, and diagnostics

- [ ] **9.3a:** Run the actual core workflows with network disabled; remove required external assets, authentication, telemetry, and dev services.
- [ ] **9.3b:** Bound/validate project/import/IPC/expression inputs; rotate/redact local logs, expose per-feature/job performance, and export diagnostics only on explicit user action.

#### 9.4 — Required module tests

- [ ] **9.4a — T-05, NFR-010 / T-50:** Run keyboard-only create/edit/repair/save workflows and actual native accessibility/scaling checks per platform; record untested assistive-technology combinations.
- [ ] **9.4b — T-36–T-38, NFR-014 / T-54:** Complete AC-01–AC-05 offline, inspect diagnostics/redaction, and test malformed inputs without arbitrary command execution or silent uploads.
- [ ] **9.4c — Native visual review:** Compare placement/workflows against the atlas where supported; verify clear prompts, current/stale/save states, and no active-looking unfinished controls.

**Test pass criteria:** Implemented workflows are operable offline and by keyboard with native visual/accessibility evidence and bounded unsafe-input handling.

**Completion check:** AC-01–AC-05 UX/security checks pass on the declared support matrix; remaining native gaps block the relevant support claim.

**Evidence:** `docs/testing/module-9.md`. **Current status:** Not run.


### Module 10 — Recovery, migration, undo, and cancellation fault hardening

**Purpose:** Complete failure handling across the full CAD workflow rather than deferring basic recovery until release.  
**Prerequisites:** Modules 2–9; foundation recovery already exists and has been tested in Module 2.  
**Requirement/design anchors:** FR-007–FR-010, FR-034–FR-035, FR-039; SDD §§4–6, 9, 12.

#### 10.1 — Complete durable history and crash recovery

- [ ] **10.1a:** Exercise durable operation deduplication, lost acknowledgments, retained undo/redo roots, worker restart, full-app restart, and recovery selection across parts/assemblies/drawings.
- [ ] **10.1b:** Distinguish last explicitly saved from newer recoverable revision and prove garbage collection cannot remove required history/import/job objects.

#### 10.2 — Harden save, migration, and filesystem behavior

- [ ] **10.2a:** Finish platform-specific atomic replacement/flush, standalone snapshot validation, Save As, backup retention, project lock/alias behavior, and low-disk admission.
- [ ] **10.2b:** Migrate supported prior versions on copies; reject future/unsupported authoritative geometry versions, failed migrations, missing objects, and corrupted snapshots without overwriting originals.

#### 10.3 — Close concurrency and cancellation races

- [ ] **10.3a:** Inject cancellation before/after compute, before durable commit, and before publication; define exactly one terminal outcome and clean staged resources.
- [ ] **10.3b:** Kill stuck workers safely, discard old epoch messages, reconcile uncertain operations by ID, and resume from actual durable state without duplicate effects.

#### 10.4 — Required module tests

- [ ] **10.4a — T-08–T-10, T-34–T-35, T-39, NFR-011 / T-51:** Run a commit/save/recovery crash matrix for all document types; compare project revision, command count, geometry invariants, and saved destination hashes.
- [ ] **10.4b — NFR-009, NFR-016 / T-49, T-56:** Run disk full, denied permissions, truncated DB/blob, lock aliases, migration failure, object GC, history cap, and repeated edit/undo/redo fixtures.
- [ ] **10.4c — AC-06 and cancellation race corpus:** Force uncertain acknowledgments and old-process replies. Expect one durable command or a clear canceled outcome, no stale model overwrite, and no orphaned unbounded buffers.

**Test pass criteria:** Acknowledged durable work recovers and prior saved projects survive all specified injected failures; unresolved native/filesystem gaps remain explicit blockers.

**Completion check:** Full B6 fault corpus, prior-version migration corpus, and recovery across parts/assemblies/drawings pass.

**Evidence:** `docs/testing/module-10.md`. **Current status:** Not run.


### Module 11 — Real 8 GB performance and cross-module acceptance

**Purpose:** Measure and validate the agreed hardware promise and complete requirement evidence.  
**Prerequisites:** Modules 0–10; frozen B0–B6 fixture manifests and real reference hardware.  
**Requirement/design anchors:** NFR-001–NFR-016; SDD §§12–15; SRS §§6–9.

#### 11.1 — Measure the resource and interaction budgets

- [ ] **11.1a:** Run the frozen cold/warm, B1–B5 modeling/navigation/rebuild/save/open procedures; collect raw samples, p95/median, UI/worker/GPU memory accounting, swap pressure, and temp disk.
- [ ] **11.1b:** Separate steady from peak, count unique component definitions as well as occurrences, and record driver/OS/power/viewport/background conditions. Do not infer results from a library minimum.

#### 11.2 — Optimize from evidence

- [ ] **11.2a:** Profile allocation/rebuild/render/save costs, then optimize the demonstrated bottleneck using instancing, cache limits, admission control, partial rebuilds, or bounded parallelism.
- [ ] **11.2b:** Repeat affected measurements after changes, compare correctness invariants and resource regressions, and preserve the same acceptance fixtures.

#### 11.3 — Complete the acceptance register

- [ ] **11.3a:** Run AC-01–AC-07 on each declared target; reconcile every FR/NFR against traceability and actual module evidence.
- [ ] **11.3b:** Document limitations and any proposed scope/budget change explicitly. An unmet gate remains Fail/Blocked until resolved or the requirement is deliberately revised.

#### 11.4 — Required module tests

- [ ] **11.4a — T-41–T-49:** Measure all resource, startup, response, exactness, tolerance, save/open, and retention targets with prescribed samples; retain raw results and measurement scripts.
- [ ] **11.4b — T-52–T-54, T-56:** Verify repeatable equivalent outcomes, ownership, privacy, pressure responses, and unsupported-graphics behavior under measured workloads.
- [ ] **11.4c — AC-07 and full traceability review:** Inspect per-platform native/offline/accessibility/fault evidence; report missing hardware/tests as Blocked. A larger host with a process cap cannot alone establish the 8 GB promise.

**Test pass criteria:** SRS workload budgets and required correctness gates pass on each claimed target, with reproducible raw measurements and no integrity regression.

**Completion check:** All requirements have actual evidence or remain visibly release-blocking; no performance number is a placeholder.

**Evidence:** `docs/testing/module-11.md`. **Current status:** Not run.


### Module 12 — Packaging, upgrades, installed builds, and release

**Purpose:** Deliver installable applications whose real installed behavior satisfies the specification.  
**Prerequisites:** Module 11 acceptance evidence and resolved distribution/signing choices.  
**Requirement/design anchors:** FR-040, NFR-015; SDD §§9, 13–15.

#### 12.1 — Build native distributions

- [ ] **12.1a:** Package native Mac arm64/x86_64 artifacts as selected, Windows x86_64 installer, and an explicitly targeted Linux package; bundle required runtime libraries and dependency notices.
- [ ] **12.1b:** Apply platform signing/notarization where the distribution route requires it using supplied credentials; retain artifact hashes/manifests and omit test-only endpoints/drivers.

#### 12.2 — Verify clean installation and upgrades

- [ ] **12.2a:** Install on clean supported systems without the development toolchain; run modeling/assembly/drawing/save/reopen/export/recovery offline using the shipped artifact.
- [ ] **12.2b:** Upgrade a prior installed build with representative projects/settings; confirm dependency loading, schema-copy migration, rollback/recovery, permissions, and uninstall preserving user projects.

#### 12.3 — Publish the operating and evidence documentation

- [ ] **12.3a:** Prepare installation, supported hardware/OS, modeling limits, unit/export conventions, troubleshooting, recovery, migration, and diagnostic-sharing instructions.
- [ ] **12.3b:** Complete the release checklist linking requirement evidence, known limitations, source/license notices, package hashes, and actual installed-build results. External publication remains a separate action.

#### 12.4 — Required module tests

- [ ] **12.4a — T-40, NFR-015 / T-55:** Inspect and launch the installed artifacts on every supported target; verify notices/version manifest, no test command backdoor, and no dev service dependency.
- [ ] **12.4b — AC-01–AC-07 installed smoke/critical suite:** Run the critical workflow and failure subset on shipped binaries; check clean install, upgrade, data preservation, offline use, PDF/exchange outputs, and native scaling.
- [ ] **12.4c — Release evidence review:** Match artifact hashes/build IDs to tested results and unresolved issues. Missing required signing/hardware/native gates block the affected distribution instead of being reported Passed.

**Test pass criteria:** The actual distributable builds pass required installed workflows and preserve data through upgrade/uninstall on the declared matrix.

**Completion check:** R1 release evidence is complete; distribution limitations are explicit and all required acceptance gates pass.

**Evidence:** `docs/testing/module-12.md`. **Current status:** Not run.


## 7. Evidence record format

Each future module evidence file shall contain:

```text
Module / task / requirement / test IDs:
Status: Not run | Pass | Fail | Blocked | Not applicable (reason)
Build/commit and dependency-manifest hash:
OS / architecture / CPU / physical RAM / GPU / driver:
Fixture ID / version / hash / complexity counts:
Procedure or actual commands:
Expected result:
Observed result and raw measurement samples:
Logs / screenshots / exported geometry or PDF / crash traces:
Known limits / deferred gate / owning follow-up:
```

Keep test failures and unavailable environments visible. Visual screenshots, engine tests, mocked transport, native end-to-end flows, physical-hardware performance, and installed artifacts are different evidence classes. Only mark a lettered task complete after its actual completion condition is verified.

## 8. Current implementation status

Module 0 feasibility evidence remains in [module-0](testing/module-0.md). The user authorized Module 1 on 19 October 2026; see [Mac reference-workspace implementation and native evidence](testing/module-1.md). Its platform matrix remains Blocked. T-01/T-03/T-04/T-35/T-53 subsets have available-Mac evidence; full associated requirements remain Blocked and other product tests remain Not run. Module 2 document metadata, working/saved persistence, semantic undo/redo and recovery subsets are verified on the available Mac; see [Module 2 evidence](testing/module-2.md). Full associated requirement rows remain Blocked. Module 3 planar sketches, constraints, expressions and transactional editing have scoped available-Mac evidence in [Module 3](testing/module-3.md); full platform completion remains Blocked. Module 4.0 workspace UI is implemented/verified for available-Mac scope; exact features 4.1–4.4 now have available-Mac implementation and verified subsets in [Module 4](testing/module-4.md), with full native platform/device completion Blocked. Modules 5–12 have not started; stop before 5.1a. Frozen fixture recipes, reference display and small storage fixtures do not establish SRS resource budgets. Documentation checks verify structure/evidence links only.

