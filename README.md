# DatumForge

Native, offline mechanical CAD project for macOS, Windows and Linux, with an
8 GB physical RAM baseline. Release support is provisional.
Start with [the specification index](docs/README.md). See [branches and publication checks](docs/PUBLISHING.md).

**Modules 1–3 are implemented on the available Mac:** native shell/reference
viewport, one exact-geometry worker, persistent documents, semantic undo/redo,
Save/Save As, recovery, planar sketch editing, PlaneGCS constraints and typed
expressions. See [Module 3 evidence and blockers](docs/testing/module-3.md).
Module 3 input repairs and the authorized Module 4.0 workspace add command tabs,
a left PropertyManager and direct shape editing. See [workspace evidence and
blockers](docs/testing/module-4.md). Windows/Linux and full R1 acceptance remain
Blocked or Not run. Remaining Module 4 exact features are authorized and implemented; available-Mac verification passes; full platform/device gates remain Blocked. Module 5 dependency/history/repair implementation is verified for available-Mac scope; full resource/platform acceptance remains Blocked. See [Module 5 evidence](docs/testing/module-5.md); Module 6 is authorized and implemented; available-Mac verification is recorded in [Module 6 evidence](docs/testing/module-6.md). Module 7 is authorized and implemented; see [assembly evidence](docs/testing/module-7.md). Modules 8–10 are explicitly authorized and implemented; available-Mac verification is complete; full acceptance remains Blocked. Stop before Module 11.

Build and launch from this checkout:

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
open build/macos-debug/cad-desktop.app
python3 tools/verify_modules8_10.py --native --clean
```

Use **New** to create a document, edit its name/units/typed parameters in the
Document tab, and use Save or Save As for a standalone `.cadproj` snapshot.
Committed changes remain recoverable separately from the saved snapshot. Part documents support planar sketches and the Module 4 exact feature set. Rigid assemblies/mates and associative drawing/PDF content are implemented in Modules 7–8.

For sketching, create a Part and choose **Create sketch** in the Features or
Sketch command tab. The left Properties tab contains the active sketch controls;
Model contains the document tree, origin, planes and saved sketches. Double-click
a plane to start a sketch or a sketch to edit it. Command search and native menus
use the same actions.

New sketches start in Select. Choose Line, Rectangle or Circle in the Sketch tab;
click the active drawing tool again to return to Select. The canvas header names
the active tool. Lines and rectangles accept
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
as `Width = 80 mm` remain available. Open the Arc arrow in the Sketch toolbar to
choose **Centerpoint Arc**, **3 Point Arc**, or **Tangent Arc**; it remembers the last
method. Centerpoint uses center → start/radius → end, with optional held dragging
for the radius. 3 Point uses start → end → bend. Tangent starts at a line or arc
endpoint; move along the source tangent or its normal, then click/release the end.
Return to the source endpoint to choose another intent. Radius and angle update
while drawing. At a shared endpoint, select the source curve first. While chaining
Line, move away and return to its last endpoint to enter Arc; completing it returns
to Line. Numeric radius/start/sweep input remains available for independent arcs.
See [arc behavior and graph comparison](docs/testing/module-3-all-arcs.md). **Solve preview** runs the
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
Accept a closed sketch, then choose **Extrude** or **Revolve**. The left Properties
panel provides profile, operation, target, direction/extent and axis controls.
Choose **Preview**, then **Accept feature** to record a recoverable transaction;
**Cancel** or Escape discards a cancelable feature draft. Select current edges for
constant fillet/equal-distance chamfer, or use Hole/Linear pattern/Circular pattern/
Mirror for supported exact cases. Double-click a feature tree node to edit its
parameters. Accepted sketch edits validate all project-local consumers atomically; dependency fingerprints reuse unchanged results.

Evaluate measures exact current geometry in the worker; display modes and mesh
detail do not change dimensions. Assign a positive density for mass. Feature timings
expand on demand. See [supported feature cases and limits](docs/decisions/module-4-features.md)
and [Module 4 verification](docs/testing/module-4.md). Use **History / repair** in Features to preview rename, suppression, deletion, reorder or parameter expressions. Failed history requires explicit stale acknowledgment and retains the last-valid display; exact measurement is unavailable until repaired. Inspect a missing reference, highlight a candidate and explicitly replace it; Undo restores the previous history. See [Module 5 decisions and limits](docs/decisions/module-5-evaluation.md). Module 9 completes
cross-document consistency and accessibility.

## Development

Use C++20, Qt Widgets and one worker. The UI receives meshes and IDs; exact OCCT
geometry and project writes belong to the worker. Dependencies are isolated in
`.deps`, generated builds in `build`, tests in disposable roots. Release data is
never a test fixture. See [CONTRIBUTING](CONTRIBUTING.md) for verified commands and
platform limitations as they become available.


For drawings, create a Drawing document and choose **Edit drawing**. Add a sheet,
choose a Part/Assembly source, place base/projected/isometric views, and add supported
Part-edge dimensions, center marks or sheet text. Preview validates the draft;
Accept records a recoverable semantic transaction. Source edits mark views/BOM
stale. Update views and explicitly replace dangling anchors before vector PDF
export or a complete portable copy. See [drawing evidence](docs/testing/module-8.md).

**Workspace → Find command** uses native Find, with aliases such as boss, mate and
PDF. Shortcut preferences and Simplified layout use the same commands/model.
Diagnostics export is explicit and excludes model text and source paths by default.
Heavy compute can be canceled at a safe boundary or its worker stopped and
recovered. **Upgrade older project copy** validates supported historical manifests
and publishes a new file while preserving the source. See
[interaction](docs/testing/module-9.md) and [recovery](docs/testing/module-10.md).
Deferred 6.1b/6.4c/7.4c and all unrun platform/resource/installed checks stay Blocked.
