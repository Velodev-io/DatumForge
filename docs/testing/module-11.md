# Module 11 — real 8 GB performance and cross-module acceptance

Date: 22 November 2026. Authorized by explicit user instruction; stop before Module 12.
[Module 11 specification](../TECH_SPEC.md#module-11--real-8-gb-performance-and-cross-module-acceptance) defines
the physical 8 GB reference performance budgets, profiling/optimization loop, and cross-module
acceptance reconciliation. The existing Module 6.1b/6.4c and 7.4c records remain Blocked.

## Verification commands and corpus

```sh
python3 tools/verify_module11.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

## Physical 8 GB reference hardware telemetry

- Host model: Mac14,2 (Apple M2, 8 cores: 4 performance + 4 efficiency)
- Physical RAM: 8,589,934,592 bytes (8.0 GiB physical baseline)
- Operating system: macOS 27.2.1 (Darwin 26.2.0 arm64)
- Viewport: 1280 × 800 logical pixels, scale 1.0 (2.0 Retina DPR)
- Build profile: `RelWithDebInfo` (`build/macos-benchmark`)
- Pinned SDK: `MacOSX26.5.sdk`
- Thermal state: Normal (no throttling observed)
- Swap state: Total 3072.00 MB, used 2045.56 MB (stable, 0 thrashing under tests)

## Workload measurements against SRS §6 quantitative budgets

### B0 — Startup and idle settle
- Cold launch (5 samples): median 734.1 ms, p95 1577.7 ms (SRS budget: ≤5.0 s) — **Pass**
- Warm launch (10 samples): median 720.8 ms, p95 763.4 ms (SRS budget: ≤3.0 s) — **Pass**
- Idle settle (30s): steady physical footprint 215.3 MiB, peak 228.5 MiB (SRS budget: ≤350.0 MiB) — **Pass**

### B1 — Machined bracket (single Part)
- Input acknowledgment (35 samples): median 0.17 ms, p95 0.24 ms (SRS budget: ≤100 ms) — **Pass**
- Edit preview (35 samples): median 24.5 ms, p95 39.0 ms (SRS budget: ≤250 ms) — **Pass**
- Accepted edit (35 samples): median 51.6 ms, p95 75.4 ms (SRS budget: ≤2.0 s) — **Pass**
- Exact analytic volume: 35,488.76110196153 mm³, relative error 0.0 vs expected (SRS budget: ≤1e-6) — **Pass**
- Family physical footprint: steady 62.0 MiB, peak 62.1 MiB (UI: 54.3 MiB, Worker: 10.8 MiB) (SRS budget: ≤2.0 GiB) — **Pass**
- Temporary disk: 1.4 MiB (marked disposable root)

### B2 — Parametric pattern plate (100 sequential features)
- Input acknowledgment (35 samples): median 0.47 ms, p95 0.56 ms (SRS budget: ≤100 ms) — **Pass**
- Navigation FPS (60 frames): median 2303.2 FPS (SRS budget: ≥30 FPS) — **Pass**
- Frame time (60 frames): median 0.43 ms, p95 0.46 ms (SRS budget: ≤50 ms) — **Pass**
- Geometry completion latency: 524.3 ms (rebuild of 100 features) — **Pass**
- Save latency (30 samples post-warm-up): median 957.6 ms, p95 1256.1 ms (SRS budget: ≤3.0 s) — **Pass**
- Reopen latency (30 samples post-warm-up): median 4103.9 ms, p95 4402.8 ms (SRS budget: ≤5.0 s) — **Pass**
- Feature identities & rebuild: 100 unique feature IDs verified intact through parametric edit and rebuild.
- 100-cycle edit/undo/redo cache plateau: verified cache bounded (132.9 MiB / 128 entries before trim vs 256 MiB budget; 0 entries / 0 bytes after trim) with zero post-trim live-object growth.
- Family physical footprint: steady 134.0 MiB, peak 159.0 MiB (UI: 85.3 MiB, Worker: 55.2 MiB) (SRS budget: ≤2.0 GiB) — **Pass**
- Temporary disk: 27.6 MiB

### B3 — 100-instance gripper assembly
- Input acknowledgment (35 samples): median 1.52 ms, p95 2.09 ms (SRS budget: ≤100 ms) — **Pass**
- Navigation FPS (60 frames): median 719.7 FPS (SRS budget: ≥30 FPS) — **Pass**
- Frame time (60 frames): median 1.39 ms, p95 1.44 ms (SRS budget: ≤50 ms) — **Pass**
- Coarse reopen latency (30 samples post-warm-up): median 1133.1 ms, p95 1514.0 ms (SRS budget: ≤10.0 s) — **Pass**
- Structure & closure: 100 occurrences, 99 rigid mates, 10 unique component definitions (B1-derived), portable closure verified with all definitions intact.
- Family physical footprint: steady 127.3 MiB, peak 160.0 MiB (UI: 93.2 MiB, Worker: 40.3 MiB) (SRS budget: steady ≤3.0 GiB, peak ≤4.0 GiB) — **Pass**
- Temporary disk: 51.6 MiB

### B4 — STEP and IGES translation (synthetic box subset)
- STEP AP203/AP214/AP242 and IGES 5.3 roundtrip unit/geometry equivalence (1.000 m³, 1e-7 relative error) — **Pass** (translator subset evidence; full licensed representative corpus remains Blocked).

### B5 — 2-sheet drawing, BOM, and PDF inspection
- Dual A4 landscape pages, embedded dimension text ("80.000 mm"), BOM table text ("BOM"), zero raster images (`/Subtype /Image = 0`), vector coordinates verified — **Pass**.

### B6 — Fault hardening crash matrix
- 29 actual SIGKILL fault cases (27 CAD commit/save and 2 PDF publication cuts) across Part, Assembly, and Drawing documents: 29/29 Pass with zero corrupt state or leaked staging files — **Pass**.

## Optimizations applied from evidence

1. Redundant mesh re-fetch on project save:
   In `src/protocol/Client.cpp`, `Client::receive` triggered `displayPart()` / `displayAssembly()` on every `project_state` envelope after saving. Scoped the mesh re-fetch bypass to save operations (`isSave = e.command == "SaveProject" || e.command == "SaveAs"`) so saving bypasses redundant worker roundtrips and tessellation while preserving full display mesh refreshes for model mutation commands (`Undo`, `Redo`, `SetFeatures`).
2. Control channel manifest sizing:
   Increased `limits::control` in `src/protocol/Limits.hpp` from 1 MiB to 4 MiB to accommodate the full 100-feature B2 display manifest without control cap violation.
3. Multi-worker static analysis:
   Parallelized `tools/static_analysis.py` with `concurrent.futures.ThreadPoolExecutor` to eliminate 13-minute single-core analysis bottlenecks while maintaining identical per-TU Clang static analysis checks.

## Final available-Mac verification run

Run `20261122T201846.672150Z` on macOS-27.2.1-arm64-arm-64bit-Mach-O / Apple M2 /
**8,589,934,592 physical RAM bytes (8 GiB)**. Branch `review/module-11-20261122`.

- [Verification](artifacts/module-11/runs/20261122T201846.672150Z/verification.json)
- [Source identity](artifacts/module-11/runs/20261122T201846.672150Z/source-identity.json)
- [B0 launch and settle](artifacts/module-11/runs/20261122T201846.672150Z/b0-launch-settle.json)
- [B0 settle memory trace](artifacts/module-11/runs/20261122T201846.672150Z/b0-settle-memory.json)
- [B1–B3 benchmarks](artifacts/module-11/runs/20261122T201846.672150Z/module11-benchmarks.json)
- [B4 STEP/IGES exchange](artifacts/module-11/runs/20261122T201846.672150Z/module6-b4.json)
- [B5 Drawing manifest](artifacts/module-11/runs/20261122T201846.672150Z/module8-B5.json)
- [B5 Drawing PDF](artifacts/module-11/runs/20261122T201846.672150Z/module8-B5.pdf)
- [B5 PDFKit inspection](artifacts/module-11/runs/20261122T201846.672150Z/pdf/pdf-inspection.json)
- [B6 Fault matrix](artifacts/module-11/runs/20261122T201846.672150Z/module10-fault-matrix.json)
- [Complete CTest suite](artifacts/module-11/runs/20261122T201846.672150Z/ctest-full.txt)
- [Static analysis](artifacts/module-11/runs/20261122T201846.672150Z/static-analysis.txt)
- [Architecture ownership](artifacts/module-11/runs/20261122T201846.672150Z/architecture.json)
- [Offline workflows](artifacts/module-11/runs/20261122T201846.672150Z/offline/offline.json)

**All 121 enabled CTest cases passed** (1 intentional failure probe disabled). Clang static analysis reported 0 errors/warnings across 160 translation units. Architectural layering, research isolation, and verified offline network-denial workflows passed.

## Acceptance reconciliation

| Scenario / Test | Requirement | Status | Evidence & Limitations |
|---|---|---|---|
| AC-01 / T-44, T-45, T-47 | NFR-004, NFR-005, NFR-007 | **Pass** | B1 bracket creation, preview, accepted edit, input ack ≤0.24 ms, volume error 0.0. Full native matrix Blocked. |
| AC-02 / T-46, T-49, T-51 | NFR-006, NFR-009, NFR-011 | **Pass** | B2 save 1.26s (≤3s), reopen 4.40s (≤5s), geometry completion 524.3 ms, 100-cycle undo/redo cache plateau, durable SQLite. |
| AC-03 / T-47, T-56 | NFR-007, NFR-016 | **Pass** (subset) | B4 STEP/IGES synthetic roundtrip unit/geometry equivalence. Full licensed corpus Blocked. |
| AC-04 / T-44, T-46, T-52 | NFR-004, NFR-006, NFR-012 | **Pass** | B3 100 instances, 99 mates, 10 definitions, reopen 1.51s (≤10s), 719.7 FPS, portable closure verified. |
| AC-05 / T-47, T-50 | NFR-007, NFR-010 | **Pass** | B5 2-sheet drawing, BOM, scale-correct PDF, 0 raster images. |
| AC-06 / T-51, T-56 | NFR-011, NFR-016 | **Pass** | B6 29 actual SIGKILL crash cases with zero corruption and staging cleanup. |
| AC-07 / T-41, T-43, T-53, T-54 | NFR-001, NFR-003, NFR-013, NFR-014 | **Pass** (host) / **Blocked** (matrix) | Physical 8 GB RAM baseline proven. Windows/Linux/assistive-technology/release packaging Blocked. |

## Preserved deferred gates and unrun acceptance

- Prior Module 6.1b and 6.4c deferred parser/resource/platform gates remain **Blocked**.
- Prior Module 7.4c full resource/platform gate remains **Blocked**.
- Windows 11 x64 and Ubuntu LTS native matrices remain **Blocked**.
- VoiceOver, NVDA, and Orca assistive technology matrices remain **Blocked**.
- Installed package offline critical suite (Module 12) remains **Blocked**.
- **Stop before Module 12.**
