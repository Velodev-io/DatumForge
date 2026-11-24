# DatumForge User Guide & Installation Manual

Version: 0.1.0  
Architecture: macOS arm64 (Apple Silicon)  
Date: 23 November 2026  

---

## 1. System Requirements

| Component | Minimum Specification | Recommended Specification |
|---|---|---|
| Operating System | macOS 13.0 (Ventura) or later | macOS 14.0 (Sonoma), 15.0 (Sequoia), or 26+ |
| Processor | Apple Silicon M1 (8 cores) | Apple Silicon M2, M3, or M4 (8+ cores) |
| System RAM | 8.0 GB physical RAM | 16.0 GB or higher |
| Graphics / Display | OpenGL 3.3 Core Profile support | Retina / High-DPI display (100%–200% DPR) |
| Storage | 500 MB free disk space | 5 GB SSD storage for engineering projects |
| Network | **None** (100% offline, zero network access required) | None |

---

## 2. Installation Procedure

### 2.1 Installing from Disk Image (.dmg)

1. Download or mount `DatumForge-0.1.0-mac-arm64.dmg`.
2. In the mounted disk image window, drag `DatumForge.app` into the `Applications` folder shortcut.
3. Eject the disk image.
4. Open `DatumForge` from `/Applications` or via Spotlight (`Cmd + Space` → `DatumForge`).

### 2.2 Security and Code Signing Notice

DatumForge is distributed with local ad-hoc code signing. If macOS Gatekeeper displays a notification indicating that the developer cannot be verified:
1. Open **System Settings** → **Privacy & Security**.
2. Scroll to the **Security** section where `DatumForge.app` is listed.
3. Click **Open Anyway** and confirm your macOS administrator credentials.
4. Subsequent launches will start directly without prompts.

---

## 3. Modeling Conventions and Core Capabilities

DatumForge provides parametric, feature-based 3D solid modeling, assemblies, and associative 2D drawings:

- **Units**: Default unit system is millimeters (mm), kilograms (kg), and seconds (s). English inches (in) and centimeters (cm) are selectable per document.
- **Parametric Sketches**: 2D sketches support geometric primitives (lines, arcs, circles, rectangles, slots, splines) and dimensional constraints solved by PlaneGCS.
- **Solid Features**: Extrusions, revolves, fillets, chamfers, draft angles, linear/circular patterns, and Hole Wizard features.
- **Assemblies**: Hierarchical assemblies with rigid mates, degrees-of-freedom tracking, and collision/interference detection.
- **Drawings**: Multi-sheet associative drawings with standard orthographic projections (Front, Top, Right, Isometric), section views, and vector PDF publication.
- **Neutral Exchange**: STEP AP203/AP214/AP242 and IGES 5.3 import/export, plus binary/ASCII STL export.

---

## 4. Crash Recovery and Diagnostic Reporting

### 4.1 Automatic Recovery
DatumForge uses an atomic SQLite-backed recovery journal. If the application terminates abnormally (e.g. power failure or forced termination):
1. Relaunch DatumForge.
2. The **Project Recovery Dialog** will display uncommitted modifications detected in the recovery root.
3. Select **Recover** to restore the session or **Discard** to reopen the last explicitly saved file.

### 4.2 Uninstall and Data Preservation
To uninstall DatumForge:
- Move `DatumForge.app` from `/Applications` to the Trash.
- **Your project files (`.cadproj`), drawings (`.pdf`), and exported models (`.step`, `.stl`) will NEVER be deleted or modified by uninstalling the application.**

---

## 5. Third-Party Licenses and Relinking Rights

DatumForge bundles dynamically linked shared libraries per the LGPL-3.0 and LGPL-2.1 licenses.
To inspect bundled licenses, see `Contents/Resources/LICENSES/` inside the application bundle or open `Contents/Resources/NOTICES.txt`.
Users may replace bundled Qt 6 shared libraries in `Contents/Frameworks/` with compatible ABI-compliant libraries per the LGPL-3.0 user relinking terms.
