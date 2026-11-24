# Module 12 — Packaging, upgrades, installed builds, and release

Date: 25 November 2026 (authorized 23 November 2026).
[Module 12 specification](../TECH_SPEC.md#module-12--packaging-upgrades-installed-builds-and-release) defines
native release packaging, clean isolated installation verification, schema-copy upgrades, operating
documentation, and installed critical smoke suite validation. Prior Module 6.1b, 6.4c, and 7.4c records remain Blocked.

## Verification commands and corpus

```sh
python3 tools/verify_module12.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

## Physical reference hardware telemetry

- Host model: Mac14,2 (Apple M2, 8 cores: 4 performance + 4 efficiency)
- Physical RAM: 8,589,934,592 bytes (8.0 GiB physical baseline)
- Operating system: macOS 27.2.1 (Darwin 27.2.0 arm64)
- Viewport: 1280 × 800 logical pixels, native Cocoa Retina / offscreen
- Release build profile: `Release` (`build/macos-release`, `CAD_RELEASE_PACKAGE=ON`, `BUILD_TESTING=OFF`)
- Test suite build profile: `RelWithDebInfo` (`build/macos-benchmark`, `BUILD_TESTING=ON`)
- Pinned SDK: `MacOSX26.5.sdk` (deployment target `13.0`)
- Thermal state: Normal (no throttling observed)

## Resolution of OD-01 (Product Identity, Licensing & Signing)

As documented in [`docs/decisions/module-12-packaging.md`](../decisions/module-12-packaging.md) and [`docs/decisions/distribution.md`](../decisions/distribution.md):
1. **Product Name & Application ID**: `DatumForge`, Bundle ID `com.datumforge.desktop`, Version `0.1.0`.
2. **Distribution Licensing**: Proprietary commercial / evaluation software built on dynamically linked open-source libraries. Accompanied by full dependency notices (`NOTICES.txt`) and component license texts (`LICENSES/`) satisfying LGPL-3.0 (Qt 6), LGPL-2.1 with Exception (OCCT 7.9.2), LGPL-2.0+ (PlaneGCS dynamically linked via `libcad_planegcs.dylib`), MPL-2.0 (Eigen), BSL-1.0 (Boost), Apache-2.0 (Ninja), and Public Domain (SQLite).
3. **Code Signing Policy**: Standalone bundle signed using ad-hoc signature (`codesign --force --sign -`) passing strict deep verification (`codesign --verify --deep --strict`). External Apple Developer ID certificate and Apple Notarization credentials are unprovisioned in the local environment and remain explicitly **Blocked**.
4. **Hardening & Endpoint Exclusion**: Release package builds with `-DCAD_RELEASE_PACKAGE=ON`, excluding `--benchmark-launch`, `--benchmark-settle`, `--recovery-root`, `--settings`, and `CAD_WORKER_EXECUTABLE` overrides, and omitting `BUILD_TESTING` probes (`transport_test`, `deliberate_failure`). Verified via `strings`, `nm -gU`, and `--help` inspection (`forbidden_test_strings_found: []`).

## Packaged Artifacts and Distribution Checksums

Deliverables generated in `out/packages/` (run [`20261124T200701Z`](artifacts/module-12/runs/20261124T200701Z/verification.json)):

| Artifact | File Format | SHA-256 Checksum | Size | Status |
|---|---|---|---|---|
| `DatumForge.app` | Standalone App Bundle | Verified via `codesign --verify --deep --strict` | 88 MB | **Pass** |
| `DatumForge-0.1.0-mac-arm64.dmg` | UDZO Compressed Disk Image | `48f01ce2544d7ab3bba79e8f8fade746699b7d3080c0501faae024b6290b16e7` | 37,529,374 bytes | **Pass** |
| `DatumForge-0.1.0-mac-arm64.zip` | Standalone Zip Archive | `b9a2f05c5df2dea70d70a332aa03bdb4af699d766b8ef31917392140bbd717a6` | 32,695,479 bytes | **Pass** |
| `package-manifest.json` | Package Metadata & Manifest | Machine-readable JSON | 1,520 bytes | **Pass** |

### Bundle Internal Layout
- `Contents/MacOS/DatumForge`: Main Qt 6 GUI client executable (Mach-O 64-bit arm64, stripped).
- `Contents/MacOS/cad-worker`: Dedicated OCCT/PlaneGCS CAD worker helper executable (Mach-O 64-bit arm64, stripped).
- `Contents/Frameworks/`: 36 logical runtime shared library dependencies (71 filesystem entries including version symlinks):
  - 6 Qt 6.10.0 dynamic libraries (`libQt6Widgets.6.dylib`, `libQt6Gui.6.dylib`, `libQt6Core.6.dylib`, `libQt6OpenGLWidgets.6.dylib`, `libQt6OpenGL.6.dylib`, `libQt6DBus.6.dylib` / `Network`).
  - 29 Open CASCADE Technology (OCCT 7.9.2) dynamic libraries (`libTKernel`, `libTKMath`, `libTKBRep`, `libTKTopAlgo`, `libTKGeomBase`, `libTKGeomAlgo`, `libTKPrim`, `libTKBO`, `libTKBool`, `libTKFillet`, `libTKOffset`, `libTKMesh`, `libTKService`, `libTKHLR`, `libTKDESTEP`, `libTKDESTL`, `libTKDEIGES`, `libTKXSBase`, etc.).
  - 1 dynamically linked PlaneGCS library (`libcad_planegcs.dylib`) preserving LGPL-2.0+ user relinking rights.
- `Contents/PlugIns/`: Bundled Qt platform plugins (`platforms/libqcocoa.dylib`, `platforms/libqoffscreen.dylib`, `platforms/libqminimal.dylib`), styles (`styles/libqmacstyle.dylib`), and image formats.
- `Contents/Resources/`:
  - `qt.conf`: Prefix mapping plugins to `PlugIns` and libraries to `Frameworks`.
  - `version-manifest.json`: Product metadata, engine versions, and build identification.
  - `NOTICES.txt`: Complete dependency notices and relinking instructions.
  - `LICENSES/`: Directory of individual open-source license texts.

## Clean Isolated Installation and Network Denial Verification

### 1. Unmodified Release App Clean Isolated & Native Cocoa Launch (Tasks 12.2a, 12.4a)
- The unmodified shipped `DatumForge.app/Contents/MacOS/DatumForge` (compiled with `CAD_RELEASE_PACKAGE=ON`, zero test probes) launched under sanitised environment (`PATH="/usr/bin:/bin"`, empty `DYLD_*`, isolated `HOME` inside a temporary directory) and macOS `/usr/bin/sandbox-exec` strict network denial (`(deny network* (remote ip "*:*")) (deny network* (local ip "*:*"))`), in both **offscreen** (`pid 4533`, worker `4541`) and **native macOS Cocoa** (`pid 4548`, worker `4558`) modes.
- Result: GUI spawned child `cad-worker` over local domain socket IPC, settled cleanly with zero premature exit, and `lsof` inspection confirmed zero loaded libraries from `.deps/` or `build/`.

### 2. Installed Critical Smoke Suite AC-01–AC-07 (Task 12.4b / T-40)
Executed in both **offscreen** (`installed-smoke-contracts.txt`, 3.1 s) and **native macOS Cocoa** (`installed-smoke-contracts-native.txt`, 9.1 s) modes under `/usr/bin/sandbox-exec` network denial with `CAD_WORKER_EXECUTABLE` pointing to the shipped `DatumForge.app/Contents/MacOS/cad-worker`:
- **AC-01 (Solid Part Modeling)**: Profile rectangle and B1 bracket features evaluated; display mesh contains 1,872 triangle indices. — **Pass**
- **AC-02 (Parametric Undo/Redo Durability)**: `SetParameter`, `Undo`, and `Redo` executed across sequential revisions (`revBeforeEdit` → `+1` → `+2` → `+3`). — **Pass**
- **AC-03 (Persistence Save & Reopen)**: Project saved to standalone `.cadproj` file (225,280 bytes), closed, and reopened cleanly with document hierarchy intact. — **Pass**
- **AC-04 (Assemblies & Rigid Mates)**: Assembly document created; two Part occurrences instantiated; **100 mm Distance mate** added and solved (`freedom 5` diagnostic verified). — **Pass**
- **AC-05 (Drawings & Vector PDF)**: **2-sheet Drawing** created (Front view on Sheet 1, Top view on Sheet 2); vector PDF exported (9,860 bytes) with valid `%PDF-` header. — **Pass**
- **AC-06 (Neutral CAD Exchange)**: Shipped worker exported STEP AP214 (47,976 bytes) and STL (36,084 bytes); fresh Part document re-imported STEP file cleanly (1,872 mesh indices). — **Pass**
- **AC-07 (Upgrade Migration, Worker Crash Recovery, & Uninstall Preservation)**:
  - Pre-drawing `ProjectStore::migrationCopy` created `migrated-store.cadproj` while preserving `legacy-store.cadproj` SHA-256 unchanged; injected failure during `migration_validated` left destination absent; migrated store opened cleanly via `openProject`.
  - Worker crash recovery triggered via `c->restart()` restored active project ID and revision 2 (`durableRev`).
  - User project copy preservation verified outside application bundle directory. — **Pass**

### 3. Whole-Suite & Static Verification (Run `20261124T200701Z`)
- **Full CTest suite**: `122/122` enabled tests passed (`harness.deliberate_failure` intentionally disabled), including `module12.contracts`.
- **Static analysis**: Clang static analyzer completed cleanly (`exit_code: 0`, 185 output lines across project TUs).
- **Formatting & Ownership**: `tools/format_sources.py` passed; `tools/check_architecture.py` passed on both `build/macos-benchmark` and `build/macos-release`; `tools/check_isolation.py` and `docs/scripts/check_specs.py --write-report` passed with 0 errors.

## Evidence Summary

| Task / Test | Requirement | Status | Evidence & Limitations |
|---|---|---|---|
| 12.1a / T-40 | FR-040 | **Pass** (macOS arm64) / **Blocked** (Win/Linux) | Standalone `DatumForge.app` (88 MB), `.dmg`, and `.zip` bundled with 36 runtime libraries (Qt 6.10.0, OCCT 7.9.2, dynamic `libcad_planegcs.dylib`). Windows/Linux packages remain **Blocked**. |
| 12.1b / T-55 | NFR-015 | **Pass** (macOS arm64) / **Blocked** (Notarization) | OD-01 resolved; notices/licenses/manifest bundled; test probes (`--benchmark-launch`, `--benchmark-settle`, `--recovery-root`, `CAD_WORKER_EXECUTABLE`) excluded from release binary; ad-hoc signed. Apple Developer ID notarization **Blocked**. |
| 12.2a / T-40 | FR-040 | **Pass** (macOS arm64) | Shipped `DatumForge.app` launched cleanly in `PATH=/usr/bin:/bin`, no `DYLD_*`, under `sandbox-exec` network denial in offscreen and native Cocoa modes (`lsof` verified). |
| 12.2b / T-51 | NFR-011 | **Pass** (macOS arm64) | Prior-manifest `migrationCopy`, failure preservation, migrated open, worker crash restart recovery, and user project preservation verified. |
| 12.3a / Doc | N/A | **Pass** | [`docs/user-guide/INSTALLATION.md`](../user-guide/INSTALLATION.md) published. |
| 12.3b / Doc | N/A | **Pass** | [`docs/user-guide/RELEASE_CHECKLIST.md`](../user-guide/RELEASE_CHECKLIST.md) published with exact SHA-256 package checksums. |
| 12.4a / T-40, T-55 | FR-040, NFR-015 | **Blocked** (macOS arm64 Pass) | Bundle inspection, symbol/strings audit, `--version`/`--help`, and isolated launch pass on macOS arm64; cross-platform & notarization gates keep full `FR-040`/`NFR-015` **Blocked**. |
| 12.4b / AC-01–AC-07 | FR-040, NFR-004–NFR-016 | **Pass** (macOS arm64 worker + native/offscreen) | All AC-01–AC-07 smoke workflows pass against shipped packaged `cad-worker` under `sandbox-exec` network denial. |
| 12.4c / Review | All | **Pass** | Traceability, requirements, and task status registers reconciled; missing platform/signing distributions preserved as **Blocked**. |

## Preserved Deferred Acceptance Gates

- Module 6.1b / 6.4c deferred Windows/Linux exchange and non-Mac packaging remain **Blocked**.
- Module 7.4c physical 8 GB assembly stress validation remains **Blocked**.
- Windows 11 x64 installer (`.msi`) and Linux distribution package (`.deb` / `.AppImage`) remain **Blocked**.
- macOS x86_64 and macOS 13.0 runtime verification remain **Blocked**.
- Apple Developer ID codesign and Apple Notarization remain **Blocked** pending external credentials provisioning.
