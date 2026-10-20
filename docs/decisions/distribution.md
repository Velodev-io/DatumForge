# Dependency and distribution register (0.3d)

Status: **Blocked for final product distribution**. The product owner must select
the product license, application ID and distribution route (OD-01). Development
source compatibility builds may continue. No external publication is authorized.

| Selected component | Source notice / implementation obligation |
|---|---|
| Qt base 6.10.0 selected runtime modules | LGPL-3.0 option and component-specific third-party notices; use shared builds, preserve corresponding source/patches/build instructions and user replacement/relinking rights. Qt Test is test-only. Do not add GPL-only modules implicitly. |
| OCCT 7.9.2 | LGPL-2.1 plus OCCT exception; retain license, exception, notices and corresponding library source/changes. |
| PlaneGCS at FreeCAD 1.0.2 revision | File headers specify Library GPL v2 or later; retain headers/source and extraction shims. Static solver in experiments needs a deliberate distribution/relinking plan before release. |
| Eigen 3.4.0 | MPL-2.0 core; include notices and account for any separately licensed modules if used. Spike uses core/dense geometry/SVD. |
| Boost 1.86.0 | Boost Software License 1.0; selected header code and notices. |
| SQLite 3.50.4 | Public-domain dedication in amalgamation source; retain source provenance and compile configuration. |
| Ninja 1.13.1 | Apache-2.0 build tool, not application runtime. |

The exact source archive's LICENSES/COPYING files govern these pins. The fetched
sources remain in `.deps/src`; `tools/collect_notices.py` records license and Qt
third-party attribution files with hashes. The register is an engineering inventory,
not a completed legal/distribution review. Packaging must include appropriate texts,
corresponding source or compliant provision, patches and relinking materials.
Signing/notarization credentials are not stored in the repository.

STEP/IGES/STL use the selected OCCT modules. Proprietary SolidWorks/Parasolid/native
translators have no selected supplier, license or cost commitment and remain R2.
No paid translator is assumed necessary for the offline R1 core.

Primary references: [version-specific Qt licensing](https://doc.qt.io/qt-6.10/licensing.html),
[OCCT license](https://dev.opencascade.org/doc/overview/html/occt_public_license.html).
Release review must inspect the pinned source texts, not infer the terms of these
versions from newer live documentation.
