# DatumForge

Native, offline mechanical CAD project for macOS, Windows and Linux, with an
8 GB physical RAM baseline. Release support is provisional.
Start with [the specification index](docs/README.md).

**Module 1 is implemented on the available Mac:** native shell, dock preferences,
reference viewport, exact worker measurement, bounded private IPC, preview/cancel
and restart. See [evidence and remaining blockers](docs/testing/module-1.md).
Module 2 has not started. This temporary reference workspace does not save projects.
Windows/Linux and full R1 acceptance remain Blocked or Not run.

Build and launch from this checkout:

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
open build/macos-debug/cad-desktop.app
python3 tools/verify_module1.py --native
```

Use **Load reference box**, select a face/edge/body, and choose **Measure selection**.
Right drag orbits, middle drag pans, the wheel zooms, and F fits the scene.
Property dimensions are temporary previews; Cancel restores 10 × 20 × 30 mm.
The app uses the pinned local dependency cache; it is not an installed release.

## Development

Use C++20, Qt Widgets and one worker. The UI receives meshes and IDs; exact OCCT
geometry and project writes belong to the worker. Dependencies are isolated in
`.deps`, generated builds in `build`, tests in disposable roots. Release data is
never a test fixture. See [CONTRIBUTING](CONTRIBUTING.md) for verified commands and
platform limitations as they become available.
