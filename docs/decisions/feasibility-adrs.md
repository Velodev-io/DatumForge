# Module 0 feasibility ADR register

19 October 2026. These decisions govern experiments, not a released product.
Exact archive URLs, commits and SHA-256 are in [the source lock](../../cmake/dependencies.lock.json).
Actual commands and outcomes are in [Module 0 evidence](../testing/module-0.md).
All production requirements and physical 8 GB targets remain unchanged.

| ID | Selected experimental option and evidence | Consequence / revisit trigger |
|---|---|---|
| ADR-01 | C++20 + Qt Widgets 6.10.0, shared Core/Gui/Widgets/Network/OpenGLWidgets and test-only Qt Test. Mac Clang 21 compatibility requires the tracked ARM-header patch. | Keep dense native widgets and platform focus/accessibility gates. Revisit on measured native accessibility/rendering/footprint failure. Windows/Linux support remains Blocked. |
| ADR-02 | OCCT 7.9.2, immutable commit in lock, shared libs, exceptions enabled, `-O1 -DNDEBUG` feasibility build. `cad.geometry_topology` owns the real invariant/invalid-input evidence. | Never disable exceptions to conceal construction failures; every supported operation still needs its own corpus. Revisit on geometry/reference robustness failures or incompatible import payload versions. |
| ADR-03 | PlaneGCS from FreeCAD 1.0.2 at `256fc7eff3379911ab5daf88e10182c509aa8052`, Eigen 3.4.0 and Boost 1.86.0. Numerical files unchanged; generated export/config/logging/include shims isolate the host API. Rectangle/circle, DOF/drag, residual and redundancy/conflict corpus passes. | Retain source headers/notices and extraction record. Parameter direction is explicitly `second−first`; do not infer conventions. Revisit on scale/drag/diagnostic corpus failures. Larger constraint/entity combinations remain Module 3. |
| ADR-04 | One local QProcess worker, private QLocalServer endpoint and inherited-stdin capability, version/epoch validation and bounded CBOR/chunk fallback. No TCP listener or cloud service. | UI owns meshes/IDs only; inspect actual linked libraries and worker restart. Full command/durability integration remains Modules 1/2. Shared memory is optional; bounded chunks are the initial fallback. Revisit on measured transport/interaction limits. |
| ADR-05 | Bundled SQLite 3.50.4, private WAL/FULL/FK working DB and serialized backup into validated standalone snapshots. Real process exits/external kills, lost-ack dedup, disk full, permission denial and alias locks pass independently. | Experimental schema is not the final product format. Keep portable snapshot distinct from newer recovery. OS filesystem/power-loss and replacement-lock identity need explicit native hardening in Modules 2/10/12. Revisit if embedded-object streaming or B2/B4 save budgets fail. |
| ADR-06 | Evaluate a Qt/OpenGL 3.3 mesh renderer with CPU candidate picking, GPU geometry and text overlay. UI links no OCCT/SQLite. OCCT Visualization is disabled as a top-level module; exchange may compile transitive service/view libraries, never authoritative UI BRep. | This is an alternative feasibility prototype, not a completed OCCT AIS or Metal route. Retain a renderer boundary. Native visual/scaling/driver evidence and future Metal-backed path remain OD-04 gates; avoid private Qt APIs. |
| ADR-07 | Own semantic commands/references. Prototype tests use operation-content hashes, stable synthetic IDs and kernel Modified/IsDeleted history with explicit symmetric ambiguity. | No array index becomes persistent topology identity. Final schema, dependency engine and repair UI remain Modules 2–5; revisit on unresolved robustness/cost evidence, not by silently reassigning references. |

## Assembly numerical consequence (OD-05)

The initial Eigen layer uses grounded rigid poses, local SO(3) exponential updates,
finite-difference Jacobians and rank-revealing SVD with bounded iterations. The
three-body planar/axial/angle prototype reports 6/2/0 remaining DOF, accepts a
consistent cycle, rejects contradictory distances, handles near-π yaw and uses a
deterministic temporary seed to escape antiparallel-normal stationarity. Cancel
restores accepted poses. The original singular branch limitation was exposed before
the seed fix. Larger graphs, all supported mate combinations and native drag are
still required in Module 7; this is numerical feasibility, not assembly acceptance.

## Remaining decisions

OD-01 product name/application ID/license/distribution is unresolved. OD-02 has
only available-host compatibility evidence, not a complete support matrix. OD-04
requires native renderer evidence and a production route decision. OD-06 initial
caps in `tests/support/limits.hpp` are prototype gates with bounded pressure/eviction
tests; full workload headroom calibration remains Module 11. OD-07/08 retain all
unavailable hardware/accessibility/native installed checks. See [gate owners](deferred-gates.md).

## Module 1 follow-up — 19 October 2026

The user authorized Module 1. The Qt/OpenGL mesh boundary in ADR-06 is now the
initial development implementation on the measured M2 Mac, using public Qt GL
APIs, depth-aware CPU selection and worker-side exact queries. This does not
resolve OD-04 for minimum macOS, Intel, other drivers or a Metal adapter.
Bounded typed session IPC now lives under src/protocol; ADR-04 durable command
ownership still requires Module 2. See [actual results and limitations](../testing/module-1.md).
