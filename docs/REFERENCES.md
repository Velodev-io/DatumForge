# References and evidence boundaries

**Version:** 0.1 · **Checked:** 19 October 2026.

## Project evidence

| ID | Reference | How it is used |
|---|---|---|
| E-01 | [SolidWorks knowledge handbook](../research/SOLIDWORKS_KNOWLEDGE_HANDBOOK.md) | Publicly documented CAD concepts and explicit unknowns; not a recovered source tree |
| E-02 | [UI design notes](../research/ui-atlas/UI_DESIGN_NOTES.md) | Reviewed screen placement, command-panel grammar, workflow notes, and coverage gaps |
| E-03 | [UI screen catalog](../research/ui-atlas/UI_SCREEN_CATALOG.md) | Reference surfaces and screenshot provenance |
| E-04 | [Offline UI atlas](../research/ui-atlas/index.html) | Visual reference; screenshots belong to their publishers |
| E-05 | [Installer inventory](../research/local-evidence/installer_inventory.json) | Static installer observations only; not evidence of the CAD editor's private implementation |

We have not run a full SolidWorks installation, recovered its source, or verified its private CAD database schema. SRS performance budgets remain unverified product targets. Module 0 prototype measurements, when available, are separately scoped in its evidence file. Public user complaints are design inputs, not proof that a current release has an unfixed defect.

## Primary technical sources

| ID | Source | Decision supported |
|---|---|---|
| R-01 | [Qt 6.10 supported platforms](https://doc.qt.io/qt-6.10/supported-platforms.html) | Candidate desktop OS/architecture baselines; this is a version-specific page, not a claim that 6.10 is the newest release |
| R-02 | [Qt user interfaces](https://doc.qt.io/qt-6.10/topics-ui.html) | Widgets suitability for traditional desktop interfaces |
| R-03 | [Qt licensing](https://doc.qt.io/qt-6/licensing.html) | Module-specific licensing choices to resolve against the selected source version |
| R-04 | [OCCT overview](https://dev.opencascade.org/doc/overview/html/) | Modeling, visualization, exchange, portability, and license information; live URL currently redirects to OCCT documentation |
| R-05 | [FreeCAD PlaneGCS header](https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Sketcher/App/planegcs/GCS.h) | Candidate 2D solver, source-level API and license; pin an immutable revision before reuse |
| R-06 | [SQLite atomic commit](https://sqlite.org/atomiccommit.html) | Transaction durability assumptions and filesystem caveats |
| R-07 | [Apple: migrating OpenGL to Metal](https://developer.apple.com/documentation/metal/migrating-opengl-code-to-metal) | Mac renderer longevity risk and need for a renderer boundary |
| R-08 | [SolidWorks system requirements](https://www.solidworks.com/support/system-requirements) | Comparison context; does not establish our resource usage |
| R-09 | [SolidWorks UI overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) | Workspace responsibilities, alongside actual screenshot inspection |
| R-10 | [SolidWorks sketch-relations FAQ](https://help.solidworks.com/2025/english/Solidworks/sldworks/c_FAQ_Sketch_Relations.htm) | Existing dangling/overdefined relation behavior and repair concepts |
| R-11 | [SolidWorks background import](https://help.solidworks.com/2026/english/SolidWorks/sldworks/t_import_models_background_process.htm?id=18.3) | Current background-import capability; avoid presenting it as an unresolved omission |
| R-12 | [SQLite backup API](https://sqlite.org/backup.html) | Supported snapshot copying and error handling; verify consistent source-revision behavior in our implementation |
| R-13 | [SQLite PRAGMA documentation](https://sqlite.org/pragma.html#pragma_synchronous) | Selected durability/foreign-key/journaling settings and platform-dependent behavior |

## Previous-session convention

The earlier chat **“Build an offline Supabase-like app”** and its actual saved plan were inspected for format:

DeskCore Project Specification and Architecture (prior local planning reference; not bundled)

Reused conventions: Module 0, hierarchical lettered tasks, explicit prerequisites, required module tests, expected results, completion checks, and honest Not run/Blocked statuses. DeskCore's Tauri/React/Rust stack, business requirements, implementation results, and checked tasks are not transferred to this CAD project.

## Reading source claims

All architecture choices below are proposals for this product. OCCT is a geometry foundation, not a complete feature-based CAD application. PlaneGCS does not supply our full sketch UI or 3D mates. SQLite does not implement geometry or make multiple independent files atomic. A STEP-imported solid does not imply recovery of SolidWorks feature history. Precise performance and support claims require tests on our pinned dependencies and shipped builds.
