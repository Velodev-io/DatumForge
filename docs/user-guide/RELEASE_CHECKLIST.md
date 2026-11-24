# DatumForge Release Checklist and Traceability Register

Release: 0.1.0  
Target: macOS arm64 (Apple Silicon)  
Date: 25 November 2026  
Module Anchor: Module 12 (TECH_SPEC §12, SRS §5, SDD §§9, 13–15)  

---

## 1. Release Deliverables and Checksums

Verified in run [`20261124T200701Z`](../testing/artifacts/module-12/runs/20261124T200701Z/verification.json):

| Deliverable Artifact | File Format | SHA-256 Checksum | Size | Status |
|---|---|---|---|---|
| `DatumForge.app` | macOS Standalone App Bundle | Verified via `codesign --verify --deep --strict` | 88 MB | **Pass** |
| `DatumForge-0.1.0-mac-arm64.dmg` | UDZO Compressed Disk Image | `48f01ce2544d7ab3bba79e8f8fade746699b7d3080c0501faae024b6290b16e7` | 37,529,374 bytes | **Pass** |
| `DatumForge-0.1.0-mac-arm64.zip` | Standalone Zip Archive | `b9a2f05c5df2dea70d70a332aa03bdb4af699d766b8ef31917392140bbd717a6` | 32,695,479 bytes | **Pass** |
| `package-manifest.json` | Package Metadata & Hashes | Verified | 1,520 bytes | **Pass** |

---

## 2. Requirement Verification Summary

| Requirement ID | Test ID | Description | Result | Evidence Location |
|---|---|---|---|---|
| **FR-040** | T-40 | Shipped macOS arm64 release build installs, launches, models, saves/reopens, recovers, and exports without dev tools; Windows/Linux/macOS x86_64/13.0 Blocked. | **Blocked** *(macOS arm64 Pass)* | `docs/testing/module-12.md`, `tests/module12/contracts.cpp`, `shipped-launch-native.txt` |
| **NFR-015** | T-55 | Release bundle includes version manifest, dependency notices, dynamic `libcad_planegcs.dylib`, excludes test-only endpoints, preserves user projects; notarization/other OS Blocked. | **Blocked** *(macOS arm64 Pass)* | `bundle-inspection.json`, `version-manifest.json`, `NOTICES.txt` |
| **AC-01** | T-44 | 3D solid feature modeling (extrude, cut, hole, pattern) on packaged `cad-worker`. | **Pass** *(macOS)* | `installed-smoke-contracts-native.txt` (`mesh indices = 1872`) |
| **AC-02** | T-44 | Parametric history editing, semantic undo, and redo cycles on packaged `cad-worker`. | **Pass** *(macOS)* | `installed-smoke-contracts-native.txt` |
| **AC-03** | T-45 | Standalone `.cadproj` persistence save and reopen on packaged `cad-worker`. | **Pass** *(macOS)* | `installed-smoke-contracts-native.txt` (`225280 bytes`) |
| **AC-04** | T-47 | Assembly modeling, multi-component placement, and 100 mm Distance mate (`freedom 5`) on packaged `cad-worker`. | **Pass** *(macOS)* | `installed-smoke-contracts-native.txt` (`2 instances`) |
| **AC-05** | T-48 | Associative 2-sheet drawing (Front + Top views) and vector PDF on packaged `cad-worker`. | **Pass** *(macOS)* | `installed-smoke-contracts-native.txt` (`9860 bytes`, `%PDF-`) |
| **AC-06** | T-46 | Neutral CAD exchange (STEP AP214 and STL export and STEP re-import) on packaged `cad-worker`. | **Pass** *(macOS)* | `installed-smoke-contracts-native.txt` (`47976 / 36084 bytes`) |
| **AC-07** | T-40 | Prior-manifest migration copy, failed-migration preservation, worker crash recovery (`rev 2`), and user project preservation. | **Pass** *(macOS)* | `installed-smoke-contracts-native.txt` |

---

## 3. Bundled Dependencies & License Notices

| Component | Pinned Version | License Type | Dynamic Library / Delivery | Notice File |
|---|---|---|---|---|
| Open CASCADE Technology | 7.9.2 | LGPL-2.1 with Exception | 29 dylibs in `Contents/Frameworks/` | `LICENSES/OCCT_LGPL_21.txt` |
| Qt base | 6.10.0 | LGPL-3.0 | 6 dylibs + plugins in `Contents/` | `LICENSES/Qt6/` |
| PlaneGCS | FreeCAD 1.0.2 rev | LGPL-2.0+ | `libcad_planegcs.dylib` in `Contents/Frameworks/` | `LICENSES/PlaneGCS_LGPL_2_0.txt` |
| Eigen | 3.4.0 | MPL-2.0 | Header library | `LICENSES/Eigen_MPL2.txt` |
| Boost | 1.86.0 | BSL-1.0 | Header library | `LICENSES/Boost_BSL_1_0.txt` |
| SQLite | 3.50.4 | Public Domain | Compiled into persistence layer | `LICENSES/SQLite_Public_Domain.txt` |
| Ninja | 1.13.1 | Apache-2.0 | Build tool (not bundled in runtime) | `LICENSES/Ninja_Apache_2_0.txt` |

---

## 4. Known Limitations and Deferred Acceptance Gates

1. **Windows x86_64, Linux, and macOS x86_64/13.0 Packages**:
   - Status: **Blocked**.
   - Native Windows MSI/EXE installer, Linux DEB/AppImage packages, and Intel/macOS 13.0 runtime verification require target platform hosts/runners.
2. **Apple Developer ID and Apple Notarization**:
   - Status: **Blocked (Credentials Gap)**.
   - Code signing is verified using macOS ad-hoc identity (`codesign -s -`). Gatekeeper notarization credentials are not provisioned in this offline environment.
3. **Physical 8 GB Reference Memory Target**:
   - Verified on physical Apple M2 8 GB host under Module 11 budgets (B0–B3 within bounds, zero swap thrashing).
