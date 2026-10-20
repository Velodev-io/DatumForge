# Source builds and commands

Module 0 only. `dependencies.lock.json` pins upstream source archives and extracted
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
`static-analysis` uses Clang's analyzer with real compile flags on our spikes.
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
Empty `src`/`apps` directories express the proposed layout; application features
are intentionally absent until Module 1 approval. `.deps` and `build` are ignored.
