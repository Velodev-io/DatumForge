# Software Requirements Specification — Solidworks Clone

**Version:** 0.1 · **Date:** 19 October 2026  
**Status:** Proposed requirements; no implementation or product acceptance tests completed.  
**Baseline:** Native desktop CAD, macOS + Windows + Linux, 8 GB system RAM.  
**Companions:** [SDD](SDD.md), [TECH_SPEC](TECH_SPEC.md), [STEP_ZERO](STEP_ZERO.md), [traceability](TRACEABILITY.md).

## 1. Purpose and interpretation

Define a useful, precise mechanical CAD product with a SolidWorks-familiar layout, dependable local documents, clear failures, and bounded resource use. The initial release must complete a real part → assembly → drawing workflow. A visual shell or triangle viewer alone does not satisfy this specification.

“Shall” identifies a requirement. “Target” identifies a proposed quantitative acceptance budget to calibrate in Step Zero. Requirements retain their IDs after edits. R1 means required for the initial release; R2 means explicitly deferred. Partial milestones are not R1 completion.

## 2. Users and assumptions

Primary users are mechanical designers, students, makers, and SolidWorks users seeking a native cross-platform workflow. One local OS user operates one editing application instance per project. Multiple documents may be open; the same project cannot have two writers. No shared network-drive or concurrent cloud-folder editing is supported in R1.

The application shall use the installed machine's compute and storage for core operations. Internet access, an account, paid translation services, or a SolidWorks installation shall not be prerequisites for core R1 workflows.

The project name is provisional. No assumption is made about public branding, repository remote, commercial distribution, or final product license. Those choices are recorded during Step Zero and do not justify inventing a remote or copying source assets.

## 3. Release boundaries

### 3.1 R1 scope

- Native document workspace, selection/navigation, contextual command editing, familiar and simplified layouts.
- Parts with planar constrained sketches and an explicit initial feature set.
- Editable feature history, expressions, dependency-aware rebuilds, reference diagnostics, undo/redo.
- Local project save/open, recoverable changes, portable project copies, versioned storage.
- STEP/IGES geometry exchange, STL mesh export, and PDF drawing output.
- Rigid assemblies with a bounded initial mate set and shared component definitions.
- Basic associative engineering drawings with model views, dimensions, and BOM.
- Offline operation, native installed-build tests on all supported platforms, measured 8 GB performance.

### 3.2 R2 / excluded from R1

Advanced surfacing, sheet metal and flat patterns, weldments/cut lists, configurable families/design tables, 3D sketching, deformable/flexible assemblies, motion simulation, FEA/CFD, CAM, electrical/routing, photorealistic rendering, enterprise PDM, cloud collaboration, scripting/plugins, and full GD&T/manufacturing certification.

Direct `.SLDPRT`, `.SLDASM`, and `.SLDDRW` import/export, Parasolid exchange, and SolidWorks macro/add-in compatibility are separate future interoperability decisions. Neutral-format geometry exchange does not imply feature-history or native drawing compatibility. Exact SolidWorks pixel matching and complete feature parity are not R1 acceptance conditions.

## 4. Operating environment

| Item | Proposed R1 baseline | Verification policy |
|---|---|---|
| RAM | **8 GB physical system RAM**; 16 GB recommended for larger work | Real 8 GB validation required; limiting a process on a larger host is supplementary |
| CPU | 64-bit processor; four logical cores in the reference machine | Record exact hardware; avoid assuming identical timing across CPUs |
| Disk | Local SSD; 5 GB free for installation, plus measured project/temp needs | Check operation-specific free space; do not assume 5 GB fits all projects |
| Display | 1280 × 800 logical pixels; 100–200% desktop scale | Panels must remain operable without hiding accept/cancel |
| GPU | Integrated GPU acceptable if required viewport features pass | Prototype OpenGL 3.3 path; Mac future renderer decision remains a gate |
| macOS | Candidate macOS 13+, arm64 and x86_64 | Publish support only for tested OS/architecture combinations |
| Windows | Candidate Windows 11 x86_64 | Windows ARM and Windows 10 not R1 promises |
| Linux | Candidate Ubuntu 22.04/24.04 x86_64 | Build with an explicit glibc baseline; test X11 and Wayland sessions |

These are product support proposals informed by [version-specific Qt platform documentation](https://doc.qt.io/qt-6.10/supported-platforms.html). Step Zero must pin compatible Qt/OCCT/compiler versions. “Linux support” does not mean every distribution; “macOS support” does not mean every old 8 GB Mac.

## 5. Functional requirements

All following requirements are R1. The named test is a planned acceptance scenario, not an existing test implementation.

### 5.1 Workspace and interaction

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-001 | Open a native workspace with top commands, left structure/parameters, central viewport, optional right resources, and bottom document/status context. | T-01 |
| FR-002 | Create/open/switch/close part, assembly, and drawing documents; warn about changes not explicitly saved. | T-02 |
| FR-003 | Support orbit, pan, zoom, fit, orthographic views, shading/wireframe, and synchronized tree/viewport selection with face/edge/body/component filters. | T-03 |
| FR-004 | Give each modeling command explicit input, preview, accept, cancel, invalid-input, and failure behavior; previews shall not mutate committed history. | T-04 |
| FR-005 | Provide contextual command search, shortcut configuration, visible edit context, unit-aware inputs, keyboard traversal, and an optional simplified layout. | T-05 |

### 5.2 Documents and persistence

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-006 | Store stable document, feature, entity, and component-occurrence identities plus a versioned, inspectable semantic model. | T-06 |
| FR-007 | Open/save local projects, preserve the previous valid saved state on failure, and implement Save As without accidentally overwriting the source. | T-07 |
| FR-008 | Distinguish preview, computed, committed in memory, recoverable, explicitly saved, and failed states in user-visible status. | T-08 |
| FR-009 | Undo/redo committed semantic changes as coherent operations, including feature and assembly edits, within documented retention limits. | T-09 |
| FR-010 | Detect an existing project writer, missing external references, and incompatible schema versions; prevent unsafe write access. | T-10 |
| FR-011 | Create a portable project copy containing selected assembly/drawing dependencies and imported source geometry, with a dependency manifest. | T-11 |

### 5.3 Sketching

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-012 | Create planar sketches containing points, lines, connected polylines, rectangles, circles, arcs, and construction geometry. | T-12 |
| FR-013 | Support coincidence, horizontal/vertical, parallel, perpendicular, tangent, equal, fixed, and dimensional length/distance/angle/radius constraints within documented entity combinations. | T-13 |
| FR-014 | Show underdefined, fully defined, redundant, inconsistent, and failed-solve states; identify relevant constraints without claiming constraint count alone proves consistency. | T-14 |
| FR-015 | Drag underconstrained geometry predictably, preserve accepted constraints, and cancel a drag or dimension edit without committing invalid geometry. | T-15 |
| FR-016 | Parse length/angle units and named scalar expressions; reject cycles, dimensionally invalid expressions, nonfinite values, and invalid domain values. | T-16 |

### 5.4 Features and rebuilds

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-017 | Create additive and subtractive extrusions and revolves from valid closed planar profiles, with direction and extent controls specified in TECH_SPEC. | T-17 |
| FR-018 | Provide constant-radius fillet, equal-distance chamfer, simple cylindrical hole, linear/circular pattern, and planar mirror for the supported R1 cases. | T-18 |
| FR-019 | Edit feature parameters, rename features, suppress/unsuppress, and delete with an explicit dependent-feature impact preview. Reordering shall be allowed only when dependency-valid. | T-19 |
| FR-020 | Evaluate dependencies in a valid order, rebuild affected descendants, reject cycles, and retain the last valid result with a conspicuous stale marker when rebuild fails. | T-20 |
| FR-021 | Track generated/modified geometry references across supported edits; show missing or ambiguous references and permit explicit replacement without silent arbitrary reassignment. | T-21 |
| FR-022 | Report operation-specific errors, highlight the affected inputs, and preserve the last committed model when geometry construction fails. | T-22 |

### 5.5 Exchange and measurement

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-023 | Import STEP and IGES as validated imported-body features with units, source provenance, warnings, and background progress; source files shall not be modified. | T-23 |
| FR-024 | Export selected precise geometry to STEP/IGES, export triangulated STL with explicit units/detail, and report unsupported content or translation loss. | T-24 |
| FR-025 | Measure supported lengths, angles, radii, area, volume, bounding box, and mass when a valid material density is assigned, with units and validity status. | T-25 |

### 5.6 Assemblies

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-026 | Insert multiple occurrences of a part/subassembly, share definitions, position occurrences independently, and ground a component to remove global rigid-body freedom. | T-26 |
| FR-027 | Support rigid coincident, concentric, parallel, perpendicular, distance, and angle mates for specified planar/axial references, with remaining freedom and conflict diagnostics. | T-27 |
| FR-028 | Load coarse assembly display data before exact geometry where feasible, resolve parts on demand, and distinguish hidden, suppressed, unloaded, unresolved, and stale states. | T-28 |
| FR-029 | Propagate part changes to affected assembly results, diagnose missing components/mate references, and provide an explicit replacement/relink operation. | T-29 |
| FR-030 | Evaluate supported solid-component interference with highlighted results and distinguish touching, overlap, unsupported cases, and failed evaluation. | T-30 |

### 5.7 Drawings

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-031 | Create drawing sheets with units, size, scale, projection convention, and associative base/projected/isometric model views. | T-31 |
| FR-032 | Add supported linear/angular/radius/diameter dimensions, center marks, and text; update views and mark dangling annotations after relevant model edits. | T-32 |
| FR-033 | Generate a basic assembly BOM with occurrence quantities and part identifiers, and export vector drawing PDF at the selected sheet scale. | T-33 |

### 5.8 Reliability and observability

| ID | Requirement — the application shall… | Acceptance test |
|---|---|---|
| FR-034 | Persist recovery records for accepted commands, offer recovery separately from the saved project, and preserve the last durable state through an engine crash. | T-34 |
| FR-035 | Keep the UI responsive during geometry work, show job state/progress, support safe cancellation, and isolate a failed worker from the UI. | T-35 |
| FR-036 | Display feature/job timing, cache/memory pressure, warnings, and local diagnostic export with explicit user-selected sharing; no automatic model upload. | T-36 |
| FR-037 | Complete core modeling, assembly, drawing, save/open, and neutral exchange workflows with network access disabled and no account. | T-37 |
| FR-038 | Reject malformed or oversized project/import/IPC inputs safely; importing a document shall not execute embedded scripts or arbitrary operating-system commands. | T-38 |
| FR-039 | Open supported older project versions through recoverable migration and reject newer unsupported versions without modifying the original. | T-39 |
| FR-040 | Install, launch, save/reopen, recover, and export in release builds on every published platform/architecture without development services. | T-40 |

## 6. Nonfunctional requirements

Quantitative targets below are proposed release gates. Step Zero establishes repeatable fixtures and measurement methods; later modules collect results. Any revision needs a recorded reason and cross-document update.

| ID | Requirement / proposed target | Planned verification |
|---|---|---|
| NFR-001 | Baseline workloads B0–B4 shall operate on an 8 GB reference system without OOM termination or relying on sustained swap thrashing. Swap activity is recorded, not automatically a failure. | T-41 |
| NFR-002 | B0 idle application-family memory target ≤350 MiB; B1/B2 steady target ≤2 GiB; B3/B4 steady target ≤3 GiB, peak target ≤4 GiB. Measure OS-appropriate process-family physical footprint and document shared-memory accounting. | T-42 |
| NFR-003 | Cold launch to usable empty editor target ≤5 s; warm launch ≤3 s on the reference SSD system, excluding first-install OS prompts. | T-43 |
| NFR-004 | Input acknowledgment p95 ≤100 ms under B1–B3; B2/B3 navigation median ≥30 FPS at reference viewport/detail, with p95 frame time ≤50 ms. Geometry completion latency is a separate metric. | T-44 |
| NFR-005 | On B1, ordinary dimension edit-to-preview p95 ≤250 ms; accepted single-feature edit-to-valid-result p95 ≤2 s. Long operations shall show a running state within 100 ms. | T-45 |
| NFR-006 | Explicit B2 save target ≤3 s and reopen ≤5 s; B3 reopen target ≤10 s with valid coarse display first. Durability may not be disabled to meet timing. | T-46 |
| NFR-007 | Display simplification shall never alter stored exact geometry, units, manufacturing dimensions, or precise export. | T-47 |
| NFR-008 | Geometric checks shall use scale-aware declared tolerances; standard millimeter fixtures use ≤1e-6 mm reference-point error and ≤1e-6 relative volume error where mathematically applicable. Imported tolerance deviations shall be reported. | T-48 |
| NFR-009 | Undo retention shall be bounded by configurable disk/memory budgets; a 100-cycle B2 edit/undo/redo run shall show no unexplained monotonic live-object growth after cache trimming. | T-49 |
| NFR-010 | Main workflows shall be keyboard-operable; status cannot rely on color alone; accessible names/focus order and 100–200% scaling shall pass native checks on each platform. | T-50 |
| NFR-011 | A failed save, migration, import, or canceled operation shall not destroy the last valid saved project or silently commit partial semantic changes. | T-51 |
| NFR-012 | Identical inputs and pinned engine/solver versions shall produce equivalent valid semantic/geometry outcomes within declared tolerances, not necessarily byte-identical BRep or mesh files. | T-52 |
| NFR-013 | Core UI shall not directly own authoritative kernel shapes or mutate the project database; the worker shall enforce validation and revisions. | T-53 |
| NFR-014 | Diagnostic exports shall exclude full models, source paths, and document text by default; inclusion requires an explicit selection. Local logs rotate and retain no more than the configured cap. | T-54 |
| NFR-015 | Release packages shall include dependency notices and a version manifest, exclude test-only command endpoints, and preserve user projects on uninstall. | T-55 |
| NFR-016 | Disk-full, permission denial, worker termination, truncated data, and unsupported graphics shall produce actionable errors or a documented safe fallback. | T-56 |

## 7. Workload fixtures

Freeze generators, exact parameters, expected invariants, seed, source/license, hashes, and engine version during Step Zero. These are acceptance workloads, not universal complexity limits.

| ID | Fixture definition | Main purpose |
|---|---|---|
| B0 | Empty editor, no document, settled for 30 seconds | Startup and idle footprint |
| B1 | Millimeter bracket: two sketches, ≤40 entities/80 constraints, extrusion, cut, four-hole pattern, one fillet | Interactive modeling, analytic dimensions/volume, rollback |
| B2 | Deterministic part with 100 supported features, ≤10 bodies, ≤20,000 exact faces and ≤500,000 visible triangles | History, rebuild, save/open, undo, memory |
| B3 | Rigid assembly with 100 occurrences of 10 distinct B1-derived definitions, ≤150 mates and ≤1,000,000 visible triangles | Instancing, mate solve, loading, navigation, memory |
| B4 | Curated STEP and IGES inputs, each ≤25 MiB, with units, colors where supported, and ≤50,000 faces; exact counts frozen in manifest | Translation, warnings, peak allocations, cancellation |
| B5 | Two-sheet drawing of B1/B3, ≤8 views and ≤30 annotations, one BOM | Associativity, scale, quantities, PDF correctness |
| B6 | Corrupt project/import fixtures, ambiguous topology edits, conflicting constraints, interrupted transactions, and disk/write failures | Integrity and recovery, no performance budget |

If the deterministic B2/B4 generator cannot meet both feature and face bounds, adjust the generator before measurement and record its actual contents. Do not discard a failing fixture after observing its performance. Tests run with a normal OS desktop and no unrelated heavy apps; disclose background load, power mode, temperature throttling, graphics settings, and viewport size.

## 8. Acceptance workflows

| ID | Scenario and expected result | Requirements |
|---|---|---|
| AC-01 | Start offline; create a constrained B1 bracket; preview/cancel and then accept extrusion; verify dimensions and undo/redo. | FR-001–FR-005, FR-012–FR-018 |
| AC-02 | Save B2; change a parent dimension; rebuild dependent features only; reopen and recover identities, semantic parameters, and valid geometry. | FR-006–FR-010, FR-019–FR-022 |
| AC-03 | Import B4; inspect units/warnings; measure; export/reimport STEP; validate geometry equivalence within tolerance, not recovered feature history. | FR-023–FR-025 |
| AC-04 | Build B3 with shared parts, ground and mate components, change a part, diagnose a deliberately broken mate, and make a portable copy. | FR-011, FR-026–FR-030 |
| AC-05 | Produce B5; edit the source model; update views/dimensions/BOM; mark deleted references as dangling; check exported PDF scale and readability. | FR-031–FR-033 |
| AC-06 | Terminate worker before/after persistence boundaries; restart; recover acknowledged durable commands; verify stale replies cannot change the current model. | FR-008, FR-034–FR-035, NFR-011 |
| AC-07 | On each installed OS build, run offline workflows, accessibility/scaling checks, malformed-input checks, migration failures, and resource measurements. | FR-036–FR-040, NFR-001–NFR-016 |

## 9. Release acceptance and evidence

R1 requires every FR and NFR above to be satisfied on the declared support matrix, or an explicit scope revision that updates this document. Each test records build/commit, OS, architecture, fixture hash, actual command or manual procedure, expected/observed outcome, and artifact paths. Automated logic tests, mocked IPC tests, native end-to-end tests, visual checks, and installed-build tests remain separately labeled.

Valid states are **Not run, Pass, Fail, Blocked, Not applicable with reason**. A screenshot proves appearance, not solver correctness. A successful kernel example proves that example, not an entire feature. Native Mac success does not establish Windows or Linux success. Measured performance cannot be inferred from library minimum specifications.

## 10. Open decisions and risks

OD-01: final product/application identity and distribution license. OD-02: exact dependency pins and support matrix. OD-03: PlaneGCS extraction and failure diagnostics. OD-04: Mac viewport strategy and future Metal route. OD-05: 3D mate-solver feasibility/limits. OD-06: document size/operation admission limits after profiling. OD-07: available physical test hardware. OD-08: native accessibility and GUI automation coverage.

Step Zero records resolution or an explicit later owner for each decision. Blocking feasibility failures require redesign before dependent production work. No decision is considered resolved by writing these specifications.

Implementation note (19 October 2026): the [decision register](decisions/project-baseline.md), [deferred gates](decisions/deferred-gates.md), and [Module 0 evidence](testing/module-0.md) now track these decisions. Native targets, physical 8 GB baseline, R1/R2 scope and all numerical acceptance budgets above are unchanged. Module 0 experiments do not satisfy product acceptance requirements.
