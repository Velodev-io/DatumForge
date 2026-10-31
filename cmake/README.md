# Source builds and commands

Module 0 experiments plus the authorized Modules 1–10 native workspace, persistence, sketch, exact feature and dependency/history foundation. `dependencies.lock.json` pins upstream source archives and extracted
PlaneGCS files by SHA-256. Qt's archive was also checked against its official
checksum. Source commit identities for OCCT/Eigen/FreeCAD are immutable. Ninja's
tag archive is content-locked. Builds never select floating branches or silently
use Homebrew libraries. Downloading is a setup step; built experiments run locally.

Required tools: Python 3.12+ (tested 3.14.7), CMake 4.3.1, native C/C++ compiler.
Mac tested compiler: Apple Clang 21.0.0 (`clang-2100.1.1.101`), macOS 26.5 SDK,
libc++, deployment target 13.0; deployment to 13.0 itself is **Blocked** until tested.
Windows MSVC/runtime pins and Ubuntu GCC/glibc pins are **Blocked**, not inherited
from Mac. Do not publish them as supported based on preset existence.

```sh
python3 tools/fetch_dependencies.py
python3 tools/fetch_tools.py
python3 tools/build_dependencies.py --jobs 2
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug --output-on-failure
cmake --build build/macos-debug --target format-check
cmake --build build/macos-debug --target static-analysis
```

`format-check` uses content-locked clang-format 21.1.8 plus whitespace/UTF-8 checks.
The Mac arm64 formatter wheel is pinned in `tools.lock.json`; other native wheels
remain Blocked pending platform provisioning.
`static-analysis` uses Clang's analyzer with real compile flags on project-owned apps/src/tests, excluding vendor and generated sources.
Windows/Linux presets are candidate commands; unrun checks remain Blocked.
`cpack --config build/macos-debug/CPackSourceConfig.cmake -B build/packages` makes
a source archive of experiments, **not** an installable product. Installed product
packaging/signing is Blocked until Module 12 and OD-01 are resolved.

Build options are centralized in `tools/build_dependencies.py`. Shared Qt Core,
Gui, Widgets, Network, OpenGLWidgets and Qt Test; bundled Qt third-party code;
no SQL plugin, DBus, ICU, OpenSSL, browser runtime or optional Qt add-ons. OCCT
modeling/meshing/exchange use shared libraries without TBB, Draw, Tcl/Tk, media,
VR or the top-level Visualization module. Exchange can pull transitive service/view
toolkits; the UI does not link them. OCCT exceptions stay enabled and feasibility
optimization is `-O1 -DNDEBUG`. The alternate Qt GL mesh spike evaluates the renderer
boundary; OCCT AIS/Metal are not implemented by that option. Eigen/Boost are
pinned headers; bundled SQLite disables loadable extensions and legacy quoted
strings, enables threads and foreign keys. PlaneGCS numerical files remain
unchanged; generated export/config/console/Boost include shims replace FreeCAD
host integration. Console output is local; no model paths are logged by a service.

Qt 6.10.0 needs `patches/qt-6.10.0-arm-yield.patch` for the observed Clang 21 ARM
intrinsic declaration failure. The patch includes the public ARM header and is
idempotently applied by the build script. The initial failed build is retained.
Do not suppress the diagnostic globally. Record the patch hash with build evidence.

Do not run two builds against the same dependency/build directory concurrently.
`apps/desktop` and `src/render` own Qt UI/display data, `src/protocol` owns the
bounded typed transport, and `apps/cad-worker` plus `src/geometry/occt` own exact
reference geometry. The production desktop links neither OCCT, SQLite nor Qt Test.
`module1_contracts` and `module1_adversary` are separate test executables.
`.deps` and `build` are ignored.

Run `python3 tools/verify_module1.py --native` from a logged-in Mac desktop terminal
for configure/build/CTest/format/analyzer/ownership/research checks and separate
native 100/125/150/175/200% effective-scale runs. Without `--native`, CTest is
headless and does not certify graphics. The scale overrides use one Retina display.
Use `python3 tools/clean_reproduce.py --module 1` to reproduce a fresh source build
and headless suite using verified local dependency binaries. Neither command
builds an installed release or certifies another OS. The Mac `.app` is a development
bundle with local dependency rpaths; its sibling worker is refreshed when changed.
`CAD_GEOMETRY_SPIKES=OFF` disables old OCCT experiments, not the Module 1 worker.


Module 2 verification (writes evidence under `docs/testing/artifacts/module-2`,
preserving Module 0/1 runs):

```sh
python3 tools/verify_module2.py --native
python3 tools/clean_reproduce.py --module 2
python3 docs/scripts/check_specs.py --write-report
```

`module2_domain`, `module2_storage`, `module2_dispatch`, `module2_contracts` and
`module2_fault_worker` are separate test executables. The fault worker links the
production persistence library and observes its transaction/publication boundaries;
no fault-control command or environment hook is shipped in the product worker.
Native tests require a logged-in desktop. All storage tests require marked
disposable temp roots and refuse release recovery directories. See
[Module 2 evidence](../docs/testing/module-2.md) for limits and blocked platforms.


Module 3 verification (each execution keeps a separate timestamped evidence folder):

```sh
python3 tools/verify_module3.py --native
python3 tools/clean_reproduce.py --module 3
python3 docs/scripts/check_specs.py --write-report
```

`module3_model`, `module3_solver`, `module3_expressions`, `module3_persistence`
and `module3_contracts` test primitives/frames, the real pinned PlaneGCS adapter,
parsed typed arithmetic, actual SQLite history, and worker/native UI workflows.
The desktop links only the semantic/display model; the numerical solver and
persistence stay in the single worker. Native tests require a logged-in desktop;
headless Qt runs do not certify graphics or focus. The available Mac runs on
physical 8 GiB hardware; full SRS resource budgets remain for Module 11. See
[Module 3 evidence](../docs/testing/module-3.md) and
[adapter decisions/limits](../docs/decisions/module-3-sketches.md).


Module 3 interaction repair and authorized Module 4.0 workspace verification:

```sh
python3 tools/verify_workspace.py --native
python3 tools/clean_reproduce.py --module 4
python3 docs/scripts/check_specs.py --write-report
```

This runner preserves timestamped records under `docs/testing/artifacts/module-4`.
It runs the complete enabled CTest suite, static checks, Module 3 and workspace
Cocoa workflows at five effective scales, and the prior native module regressions.
`module3_input` checks camera/input math and synthetic trackpad events;
`module3_contracts shapeEditing` checks draft editing with the real worker;
`module4_workspace` checks shared actions, left properties, tree state, document
switching and layout restoration. GUI/IPC tests require a logged-in native session
and local IPC permission. Physical-device, other-platform and full 8 GB resource
acceptance remain separate gates. See [workspace evidence](../docs/testing/module-4.md).

Module 4 feature verification: `python3 tools/verify_module4.py --native --clean` on the logged-in available Mac. Each run preserves logs under `docs/testing/artifacts/module-4/features-runs/`; latest feature status is separate from historical workspace evidence. Tests use marked disposable roots; no existing user app session is reused. See [Module 4 evidence](../docs/testing/module-4.md).

Module 5 verification: `python3 tools/verify_module5.py --native --clean` on the
logged-in available Mac. Timestamped runs under `docs/testing/artifacts/module-5/runs/`
record the full suite, native history/repair plus prior-module regressions at five
effective scales, static checks and a fresh source build. `module5_plan`,
`module5_engine`, `module5_persistence`, `module5_b2` and `module5_contracts` test
production dependency evaluation, actual OCCT topology changes, semantic checkpoint
persistence, the frozen 100-feature/100-cycle B2 workload and isolated worker/UI
lifecycle. B2 cache accounting does not establish physical-memory or full SRS
performance acceptance. See [Module 5 evidence](../docs/testing/module-5.md) and
[evaluation decisions](../docs/decisions/module-5-evaluation.md). Modules 6 and 7 were subsequently authorized; stop before Module 8.


Module 6 verification:

```sh
python3 tools/verify_module6.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

Timestamped evidence is preserved under `docs/testing/artifacts/module-6/runs/`.
`module6_exchange`, `module6_persistence` and `module6_contracts` cover the real
pinned translators, analytic B4 unit/geometry/loss variants, atomic source/feature
storage, retention and revision gates, failure-safe export, portable closure and
real worker/native UI cancellation/recovery. Cocoa runs use automated Qt file
pickers at five effective scales on the available display. OS-native picker and
physical-device checks remain separate. Sampled process footprints do not certify
arbitrary parser expansion or full B4 budgets. See [Module 6 evidence](../docs/testing/module-6.md).
Stop before Module 8; Module 6.1b/6.4c gates remain deferred.

Module 7 assembly verification: `python3 tools/verify_module7.py --native --clean`.
See [assembly contracts](../docs/decisions/module-7-assemblies.md) and
[Module 7 evidence](../docs/testing/module-7.md). Module 6.1b/6.4c gates remain
deferred; stop before Module 8.


Modules 8–10 verification (explicitly authorized 22 October 2026):

```sh
python3 tools/verify_modules8_10.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

Run from the logged-in available Mac with local socket and GUI access. Timestamped
records under `docs/testing/artifacts/module-10/runs/` include the complete enabled
CTest suite, B5 actual vector PDF and native PDFKit rendering, exact CAD crash
matrix, cancellation/restart, five-scale Cocoa drawing/keyboard checks, prior
native regressions, OS-enforced offline workflows, static checks and clean source
reproduction. This uses the existing pinned dependency binaries, not an installed
release. The earlier Module 8 stop boundaries above are historical; stop before
Module 11. Module 6.1b/6.4c and 7.4c remain deferred and Blocked. See
[Module 8](../docs/testing/module-8.md), [Module 9](../docs/testing/module-9.md),
and [Module 10](../docs/testing/module-10.md) for exact verification scope.

Centerpoint Arc follow-up: `python3 tools/verify_arc_interaction.py --native` records the complete CTest suite, changed-source Clang analysis, architecture/isolation, and five-scale Cocoa Centerpoint Arc workflows in marked disposable roots. Prior sketch native workflows are regressions at the endpoint scales. See [arc evidence](../docs/testing/module-3-centerpoint-arc.md). It does not restart a user app, certify physical input/installed builds, or start Module 11.

All-method Arc follow-up: `python3 tools/verify_all_arcs.py --native` runs the complete suite, changed-source analysis, ownership/isolation and five effective Cocoa scales, including the EDP normal-arc exercise in mm/in. `ctest --preset macos-debug -R "module3.(input|solver|all_arcs)$" --output-on-failure` is the focused check. See [scope and outcomes](../docs/testing/module-3-all-arcs.md). Marked disposable roots only; no user app restart or installed/device acceptance is inferred.

Core modeling: `python3 tools/verify_core_modeling.py --native` records the full
enabled CTest suite, pinned format/Clang analysis, ownership, research/coverage
checks and native CM1 plus the CM2 straight-line editing slice at five effective
scales. Focused CM2 checks are `ctest --preset macos-debug -R '^cm2\.'
--output-on-failure`. Native/IPC tests require the logged-in Mac and local socket
access. `cm2.editing` checks analytical geometry, disclosure, IDs and persistence;
`cm2.editing_contracts` checks real worker and native widget workflows. Both use
marked disposable roots. See [CM2 contracts](../docs/decisions/core-modeling-cm2-editing.md)
and [scoped evidence](../docs/testing/core-modeling.md). Remaining CM2 operations,
other platforms, installed/device/resource acceptance and Module 11 remain open.

CM3 dimension slice: `ctest --preset macos-debug -R '^cm3\.' --output-on-failure`
checks the exact solver/persistence corpus and worker-backed editor. The common
`python3 tools/verify_core_modeling.py --native` runner additionally checks native
CM3 mm/in workflows and label editing at five effective scales, preserving logs
and source identity. See [dimension contracts](../docs/decisions/core-modeling-cm3-dimensions.md)
and [scoped evidence](../docs/testing/core-modeling.md). Other CM3 dimension,
inference, Fully Define, drag and full platform/resource gates remain open.

Associated CM3 dimensions: `python3 tools/verify_cm3_associated.py` records all
enabled CTests, changed-source/all-CM3-test Clang analysis, ownership/research/spec
checks and native CM3 at five scales, plus endpoint native workspace/arc regressions.
Its scope and retained activation-only retry policy are in the
[associated dimension contract](../docs/decisions/core-modeling-cm3-associated-dimensions.md).
It does not certify whole CM3, other-platform or full physical 8 GB acceptance.
