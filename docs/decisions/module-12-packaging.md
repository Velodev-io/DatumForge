# Module 12 — Packaging, Upgrades, Installed Builds, and Release ADR

Status: **Pass for native macOS arm64 standalone distribution; Windows and Linux packages remain Blocked**.
Date: 23 November 2026.
Anchors: FR-040, NFR-015, AC-01–AC-07; SDD §§9, 13–15.

---

## 1. Context and Scope

Module 12 delivers the distributable native application package, resolves open decisions OD-01 (product release identity, distribution license, and code signing policy), and validates that the shipped standalone binary behaves correctly in an isolated clean environment without development toolchains, development directories (`.deps`), or active development environment variables (`PATH`, `DYLD_*`).

Per workspace instructions and project rules:
- Native macOS arm64 standalone distribution bundle (`DatumForge.app`) and compressed disk image (`DatumForge-0.1.0-mac-arm64.dmg`) are packaged and verified.
- Unprovisioned Windows x86_64 and Linux packages remain documented as **Blocked** pending respective build and packaging runners.
- External Apple Developer ID and Apple Notarization credentials are not provisioned in the repository/isolated environment; ad-hoc code signing (`codesign --sign -`) is applied to all binaries, frameworks, and disk images, and the credentials gap is explicitly recorded.

---

## 2. Decision Record

### 2.1 Resolution of OD-01 (Product Identity, License, and Signing)

| Decision Item | Resolution | Rationale |
|---|---|---|
| Product Name | **DatumForge** | Pinned release name reflecting parametric precision CAD workbench. |
| Application Identifier | `com.datumforge.desktop` | Standard reverse-DNS application identifier for macOS bundle and user preferences. |
| Organization Name | `DatumForge` | Namespaces application support, preferences, and recovery storage. |
| Distribution License | **Proprietary Commercial / Source-Available Evaluation** | The core application is licensed for evaluation and authorized engineering use, while strictly complying with all third-party open-source obligations. |
| Third-Party Obligations | **LGPL-3.0, LGPL-2.1 + Exception, LGPL-2.0+, MPL-2.0, BSL-1.0, Public Domain** | Qt 6.10.0, OCCT 7.9.2, and PlaneGCS (`libcad_planegcs.dylib`) are dynamically linked as shared libraries. Eigen 3.4.0 is MPL-2.0. SQLite 3.50.4 is public domain. Relinking rights are preserved by packaging standalone dylibs under `Contents/Frameworks/`. |
| Code Signing Policy | **Ad-Hoc Signing (`codesign --sign -`)** | Due to unprovisioned Apple Developer ID credentials in the offline build environment, all dylibs, helper binaries, main bundle, and `.dmg` receive valid ad-hoc signatures with strict deep verification. |
| Debug / Test Endpoints | **Omitted and Stripped** | Release binaries are built with `-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCAD_RELEASE_PACKAGE=ON`, stripped of debug symbols (`strip -x`), and exclude `--benchmark-launch`, `--benchmark-settle`, `--recovery-root`, `CAD_WORKER_EXECUTABLE`, `transport_test`, and `deliberate_failure`. |

### 2.2 macOS Standalone Bundle Architecture

The application bundle `DatumForge.app` is constructed with full isolation:
```text
DatumForge.app/
├── Contents/
│   ├── Info.plist               (Application identity, LSMinimumSystemVersion 13.0, high-DPI capable)
│   ├── PkgInfo                  (APPLDFRG)
│   ├── MacOS/
│   │   ├── DatumForge           (Main desktop GUI executable, stripped)
│   │   ├── cad-worker           (Dedicated geometry worker helper binary, stripped)
│   │   └── cad-desktop          (Compatibility symlink -> DatumForge)
│   ├── Frameworks/              (Bundled runtime shared libraries — 36 dependencies)
│   │   ├── libQt6Core.6.dylib, libQt6Gui.6.dylib, libQt6Widgets.6.dylib, ...
│   │   ├── libcad_planegcs.dylib (Dynamically linked PlaneGCS LGPL-2.0+ solver)
│   │   └── libTKernel.7.9.2.dylib, libTKMath.7.9.2.dylib, libTKBRep.7.9.2.dylib, ... (29 OCCT libs)
│   ├── PlugIns/                 (Self-contained Qt plugins)
│   │   ├── platforms/libqcocoa.dylib, libqoffscreen.dylib
│   │   ├── styles/libqmacstyle.dylib
│   │   └── imageformats/libqjpeg.dylib, libqico.dylib, libqgif.dylib
│   ├── Resources/
│   │   ├── qt.conf              (Configures Qt to load from PlugIns and Frameworks)
│   │   ├── version-manifest.json (Version, build ID, kernel, schema, license metadata)
│   │   ├── NOTICES.txt          (Full third-party attributions and relinking instructions)
│   │   └── LICENSES/            (Exact bundled license texts: LGPL, MPL, BSL, Public Domain)
│   └── lib -> Frameworks        (Symlink for dylibs referencing @loader_path/../../lib)
```

### 2.3 Dyld Runtime Path Resolution

Build-tree rpaths (pointing to `/Users/.../.deps/install/lib`) are systematically deleted using `install_name_tool -delete_rpath`.
The following relative rpaths are embedded:
- `Contents/MacOS/DatumForge`: `@executable_path/../Frameworks`, `@loader_path/../Frameworks`
- `Contents/MacOS/cad-worker`: `@executable_path/../Frameworks`, `@loader_path/../Frameworks`
- `Contents/PlugIns/*/*.dylib`: `@loader_path/../../Frameworks`, `@executable_path/../Frameworks`

This ensures dyld loads runtime dependencies exclusively from inside the app bundle, preventing any dependency on developer tools or ambient system packages.

### 2.4 Clean Installation and Upgrade Isolation

Per SDD §9:
1. **Schema Migration**: When opening projects created with legacy schemas, `ProjectStore::migrationCopy` verifies the source, creates a staged copy, executes migration steps, validates the migrated copy, and preserves the original file untouched.
2. **Crash Recovery**: The worker's private SQLite working database maintains a recovery journal. On restart, uncommitted or recovered states are offered without overwriting user projects.
3. **Uninstall Preservation**: Removing `DatumForge.app` leaves user `.cadproj` projects and export files intact in the user's Documents and work directories.
