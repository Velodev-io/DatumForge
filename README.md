# DatumForge

Native, offline mechanical CAD project for macOS, Windows and Linux, with an
8 GB physical RAM baseline. Release support is provisional.
Start with [the specification index](docs/README.md).

**Modules 1–3 are implemented on the available Mac:** native shell/reference
viewport, one exact-geometry worker, persistent documents, semantic undo/redo,
Save/Save As, recovery, planar sketch editing, PlaneGCS constraints and typed
expressions. See [Module 3 evidence and blockers](docs/testing/module-3.md).
Module 3 input repairs and the authorized Module 4.0 workspace add command tabs,
a left PropertyManager and direct shape editing. See [workspace evidence and
blockers](docs/testing/module-4.md). Windows/Linux and full R1 acceptance remain
Blocked or Not run. Stop before 4.1a exact features.

Build and launch from this checkout:

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
open build/macos-debug/cad-desktop.app
python3 tools/verify_workspace.py --native
```

Use **New** to create a document, edit its name/units/typed parameters in the
Document tab, and use Save or Save As for a standalone `.cadproj` snapshot.
Committed changes remain recoverable separately from the saved snapshot. Part documents also support planar sketches; solid features, mates and drawing
content require later modules.

For sketching, create a Part and choose **Create sketch** in the Features or
Sketch command tab. The left Properties tab contains the active sketch controls;
Model contains the document tree, origin, planes and saved sketches. Double-click
a plane to start a sketch or a sketch to edit it. Command search and native menus
use the same actions.

Choose Line, Rectangle or Circle in the Sketch tab. Lines and rectangles accept
two clicks or a press/drag/release gesture. A circle supports clicking its center,
moving, then clicking its radius; holding and dragging also works. The camera and
first vertex stay in model coordinates when the drawing area changes size. Two-
finger scrolling pans, pinch zooms at the pointer, and Space + left drag also pans.
A mouse wheel zooms; F fits all geometry. Grid snap is optional and initially off.
These trackpad paths have automated event coverage; physical-device checks remain
Blocked.

Press Escape to leave a drawing tool or cancel unfinished input, keeping completed
geometry. In Select mode, drag a rectangle corner to resize it, an edge to move it,
a circle circumference to resize it, or its center to move it. Select a shape to
edit its coordinates, radius or rectangle width/height in Properties. A driving
constraint can block free dragging; edit its dimension using **Dimension** or the
expression field. Delete removes the selected shape; Undo/Redo applies to the
current draft until **Finish Sketch** accepts one recoverable operation. To edit
accepted geometry, double-click it or its sketch tree node. **Cancel Sketch**
discards the entire edit.

**Numeric input** and **Relations and expressions** expand on demand. Numeric
creation uses document display units without pointer snapping; expressions such
as `Width = 80 mm` remain available. Arcs use a center and numeric radius/start/
sweep; a three-point arc gesture is not implemented. **Solve preview** runs the
worker without accepting the draft. Enter accepts a solved preview only with
viewport focus. Save explicitly publishes the portable project. See
[supported combinations and current limits](docs/decisions/module-3-sketches.md).

Use **Load reference box**, select a face/edge/body, and choose **Measure selection**.
Right drag orbits, middle drag pans, the wheel zooms, and F fits the scene.
Property dimensions are temporary previews; Cancel restores 10 × 20 × 30 mm.
The app uses the pinned local dependency cache; it is not an installed release.

The workspace follows the atlas placement of Features/Sketch/Evaluate commands,
the left manager, central graphics, optional right Task Pane and bottom document/
units/save status. Icons are original and the theme follows the native palette.
Extrude, Revolve and other unfinished solid tools remain disabled with reasons.
Module 9 completes cross-document consistency and accessibility; exact features
start at the separate, currently unauthorized 4.1a boundary.

## Development

Use C++20, Qt Widgets and one worker. The UI receives meshes and IDs; exact OCCT
geometry and project writes belong to the worker. Dependencies are isolated in
`.deps`, generated builds in `build`, tests in disposable roots. Release data is
never a test fixture. See [CONTRIBUTING](CONTRIBUTING.md) for verified commands and
platform limitations as they become available.
