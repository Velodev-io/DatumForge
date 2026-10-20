# DatumForge

Native, offline mechanical CAD project for macOS, Windows and Linux, with an
8 GB physical RAM baseline. Release support is provisional.
Start with [the specification index](docs/README.md).

**Modules 1–3 are implemented on the available Mac:** native shell/reference
viewport, one exact-geometry worker, persistent documents, semantic undo/redo,
Save/Save As, recovery, planar sketch editing, PlaneGCS constraints and typed
expressions. See [Module 3 evidence and blockers](docs/testing/module-3.md).
Windows/Linux and full R1 acceptance remain Blocked or Not run. Stop before
Module 4; another user instruction is required.

Build and launch from this checkout:

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
open build/macos-debug/cad-desktop.app
python3 tools/verify_module3.py --native
```

Use **New** to create a document, edit its name/units/typed parameters in the
Document tab, and use Save or Save As for a standalone `.cadproj` snapshot.
Committed changes remain recoverable separately from the saved snapshot. Part documents also support planar sketches; solid features, mates and drawing
content require later modules.

For sketching, create a Part and choose **Create sketch** in Tools (or command
search). Choose a support plane, then enter coordinates in document display units
or click in the planar viewport. Lines/rectangles use two inputs; polylines append
vertices with a moving endpoint/corner preview. For a circle, press its center,
hold and drag to size the live radius, then release to finish it in the draft.
A zero-radius release adds nothing. For numeric circles, enter X/Y and Radius and
choose **Add center with numeric radius**. Arcs use the radius/start/sweep fields
and a center input; they do not yet have a three-point pointer workflow. Select entities in the list to
add supported constraints. Named expressions use `Width = 80 mm`; dimensions may
refer to `Width`. **Solve preview** computes without committing; **Accept sketch
edit** creates one recoverable operation. Escape first cancels an unfinished
primitive, retaining completed draft geometry; Escape again cancels the sketch
edit. The Cancel sketch edit button discards the entire edit. Enter accepts a solved
preview only with viewport focus. For dragging, choose **Select / drag**, move a
point, and solve the draft. Save explicitly to publish the portable project.
See [supported combinations and current limits](docs/decisions/module-3-sketches.md).

Use **Load reference box**, select a face/edge/body, and choose **Measure selection**.
Right drag orbits, middle drag pans, the wheel zooms, and F fits the scene.
Property dimensions are temporary previews; Cancel restores 10 × 20 × 30 mm.
The app uses the pinned local dependency cache; it is not an installed release.

The upcoming [Module 4.0 tasks](docs/TECH_SPEC.md#40--align-the-native-modeling-workspace-before-adding-solid-tools) explicitly schedule SolidWorks-style command tabs, a left PropertyManager/tree switch, and the overall modeling layout before solid-feature tools. Module 9 completes cross-document consistency and accessibility. This layout work is planned; Module 4 implementation has not started.

## Development

Use C++20, Qt Widgets and one worker. The UI receives meshes and IDs; exact OCCT
geometry and project writes belong to the worker. Dependencies are isolated in
`.deps`, generated builds in `build`, tests in disposable roots. Release data is
never a test fixture. See [CONTRIBUTING](CONTRIBUTING.md) for verified commands and
platform limitations as they become available.
