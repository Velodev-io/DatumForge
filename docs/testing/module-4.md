# Module 4.0 workspace evidence

Date: 20 October 2026. Status: **feasible available-Mac implementation verified;
full platform/device acceptance Blocked**. Scope: **4.0a–c only**, after Module 3 input/direct-editing
repairs. Full Module 4 and exact features 4.1–4.4 are unimplemented and untested.
[Scope record](../decisions/implementation-scope.json). Stop before **4.1a**.

## Implementation and acceptance boundary

The implementation order was Module 3 input repair → 4.0a shared command tabs →
4.0b left manager/layout → 4.0c available-native verification. Full platform/device
acceptance remains Blocked. Historical Module 0–3 logs, screenshots, failed runs,
research and the user's IDE configuration are preserved. No agents were delegated
work; no remote, release or publication was performed.

| Task | Implemented scope |
|---|---|
| 4.0a | Features / Sketch / Evaluate tabs; original icons and short labels; shared QAction registry for toolbar/menu/search/shortcuts; contextual availability. Exact features remain disabled with reasons. Native macOS global menus remain. |
| 4.0b | Left Model / Properties / Document manager; contextual selection fields, near-heading Finish/Cancel and visible diagnostics; collapsed numeric/advanced sections; origin, planes and sketches in the tree; preserved expansion/selection by stable IDs; optional right help Task Pane; dominant central graphics; document/edit/unit/save/job status below. |
| 4.0c | Actual Cocoa workflows and screenshots, scale/visibility checks, mouse and synthetic trackpad events, draft/accepted persistence and prior-module regressions. Other native platforms, physical-device behavior and assistive technology remain Blocked. |

[Module 3 repair evidence](module-3-input-repair.md) details the camera, snapping,
click/drag creation, handles/properties, draft history and conservative constraints.
Only the serial worker solves PlaneGCS/OCCT and writes project state. UI copies are
bounded semantic/display projections. Drag events do not queue worker commands or
rebuild entity lists. No schema, dependency pin or B1 recipe change is required.

## Reproduction and current run

```sh
python3 tools/verify_workspace.py --native
python3 tools/clean_reproduce.py --module 4
python3 docs/scripts/check_specs.py --write-report
```

Use a logged-in Mac desktop and pinned local dependencies. Tests use marked
disposable roots and refuse release recovery paths. The user's running application
and unsaved project are not restarted or used as fixtures. A fresh application
launch from the rebuilt bundle is required to see changes in an already-open old
process; save work before doing so.

The [verification record](artifacts/module-4/verification.json) contains exact
commands, host inventory, durations and results. It points to its immutable
run directory. A clean-source check uses a temporary source export plus verified
cached archives and existing dependency binaries; it does not establish installed
packages, another OS or a second from-scratch dependency build.

## Actual final results

| Check | Result and raw evidence |
|---|---|
| Configure/build | Pass: [configure](artifacts/module-4/runs/20261020T060237.801495Z/configure.txt), [build](artifacts/module-4/runs/20261020T060237.801495Z/build.txt). |
| Enabled CTest suite | Pass: [27 enabled tests, zero failures](artifacts/module-4/runs/20261020T060237.801495Z/ctest.txt), 27.18 seconds. The deliberate-failure probe is Disabled by design; failure-reporting tests independently require its nonzero result. |
| Format/analyzer | Pass: [pinned formatter](artifacts/module-4/runs/20261020T060237.801495Z/format.txt), [Clang analyzer](artifacts/module-4/runs/20261020T060237.801495Z/static-analysis.txt), no reported analyzer warnings. |
| Ownership/research | Pass: [actual links and packaged worker](artifacts/module-4/runs/20261020T060237.801495Z/architecture.json); [97 research files unchanged, no remote](artifacts/module-4/runs/20261020T060237.801495Z/research-isolation.txt). |
| Native Module 3 | Pass: [100%](artifacts/module-4/runs/20261020T060237.801495Z/native-module3-100.txt), [125%](artifacts/module-4/runs/20261020T060237.801495Z/native-module3-125.txt), [150%](artifacts/module-4/runs/20261020T060237.801495Z/native-module3-150.txt), [175%](artifacts/module-4/runs/20261020T060237.801495Z/native-module3-175.txt), [200%](artifacts/module-4/runs/20261020T060237.801495Z/native-module3-200.txt). Cocoa B1 editing/focus, circle gestures in mm/in, shape properties/history, save/reopen and SIGKILL recovery; zero failed/skipped cases. |
| Native workspace | Pass: [100%](artifacts/module-4/runs/20261020T060237.801495Z/native-workspace-100.txt), [125%](artifacts/module-4/runs/20261020T060237.801495Z/native-workspace-125.txt), [150%](artifacts/module-4/runs/20261020T060237.801495Z/native-workspace-150.txt), [175%](artifacts/module-4/runs/20261020T060237.801495Z/native-workspace-175.txt), [200%](artifacts/module-4/runs/20261020T060237.801495Z/native-workspace-200.txt). Actual shared toolbar actions, left fields, dimension dialog, constraints, document switching, tree expansion and layout restoration. |
| Native regression | Pass: [Module 1](artifacts/module-4/runs/20261020T060237.801495Z/native-module1-regression.txt), [Module 2](artifacts/module-4/runs/20261020T060237.801495Z/native-module2-regression.txt), zero failed/skipped cases. |
| Fresh source | Pass: [temporary export/configure/build/27 enabled tests](artifacts/module-4/runs/20261020T060237.801495Z/clean-reproduction.txt), with [verified cached archives](artifacts/module-4/runs/20261020T060237.801495Z/clean-reproduction-command.txt) and existing dependency binaries. |
| Visual comparison | [Pass for reviewed subset](artifacts/module-4/runs/20261020T060237.801495Z/visual-review.json): [100% editing](artifacts/module-4/runs/20261020T060237.801495Z/native-100/workspace-editing.png), [100% model](artifacts/module-4/runs/20261020T060237.801495Z/native-100/workspace-model.png), [200% editing](artifacts/module-4/runs/20261020T060237.801495Z/native-200/workspace-editing.png), [200% model](artifacts/module-4/runs/20261020T060237.801495Z/native-200/workspace-model.png), [inch-circle preview](artifacts/module-4/runs/20261020T060237.801495Z/native-200/circle-drag-in.png). Intermediate scales have native visibility assertions; alternate appearance/accessibility is Blocked. |
| Identity/pins | [Pass](artifacts/module-4/runs/20261020T060237.801495Z/evidence-consistency.json): all listed current [source inputs](artifacts/module-4/runs/20261020T060237.801495Z/source-identity.json) match the tested hashes; dependency/tool/Qt-patch pins and frozen B1 are unchanged from earlier evidence. |

Tested aggregate SHA-256: `983ae15d49115bf3193f6b2c56dbc90810d35032db85365b0d5b41f2c37875c9`. The repository has no initial commit or
remote and remains on `historical/module-3`; file hashes identify this tested checkout.
The current host is macOS 26.6.2 arm64, Apple M2 with **8,589,934,592 bytes physical
RAM (8 GiB)**. Cocoa reports OpenGL 4.1; effective DPRs 1/1.25/1.5/1.75/2 use a single
Retina display. No release platform support or full resource budget is inferred.

Each native Module 3 run recorded 60 dispatched drag moves, **zero worker commands
and zero entity-list rebuilds during movement**. The [raw timing samples' summary](artifacts/module-4/runs/20261020T060237.801495Z/native-100/input-latency.json)
is application event dispatch plus event processing, without an instrumented
physical input/presentation timestamp. Observed p95 values are 14.09 / 13.55 /
12.94 / 11.69 / 11.59 ms at the five scales; maxima are 16.34 / 16.15 / 16.11 /
16.12 / 16.07 ms. This is a diagnostic trace, not an SRS latency/FPS pass or proof
of physical trackpad smoothness.

## Failures retained and corrected

- [Input regression](artifacts/module-3/input-repair/input-red.txt): four expected
  failures reproduced size-dependent camera movement, missing pixel pan, mandatory
  grid radius rounding and missing click-based circle creation. Focused model,
  input, worker and native regressions now pass.
- [Initial workspace build](artifacts/module-4/workspace-dev/build-initial.txt): a
  refactoring identifier error was corrected; the final complete build passes.
- [First native layout capture](artifacts/module-4/workspace-dev/native-candidate/workspace-editing.png):
  custom tab padding clipped Evaluate/Document and the scrolling diagnostic could
  be hidden. Native tab sizing and a diagnostic outside the scroll area correct
  those issues. Finish/Cancel remain visible near the heading.
- [First visibility assertion](artifacts/module-4/workspace-dev/native-final-layout.txt):
  the test compared the unwrapped QLabel size hint before layout updated. It now
  processes the layout and requires the full height for the actual text width;
  [native rerun](artifacts/module-4/workspace-dev/native-layout-reviewed.txt) and all
  final scales pass. The visibility assertion remains mandatory.

## Pending physical-device procedure

These manual checks are **Blocked / unrun** until a person uses real hardware in
the rebuilt app. Save any existing work before restarting the older running app.
Use a disposable new Part: draw circles using both tap/move/tap and held drag;
resize the window between inputs; pan with two fingers and pinch before completing
a rectangle; press Escape and drag a rectangle corner/circle circumference; edit
width/height/radius; Undo/Redo; Finish and Save/reopen. Repeat with natural scrolling
and tap-to-click preferences recorded. Check pointer-to-geometry alignment,
continuous previews, accidental gesture completion and retained saved values.
No physical-device pass is implied by the automated event paths above.

## Native reference comparison and deliberate differences

The preserved [UI atlas notes](../../research/ui-atlas/UI_DESIGN_NOTES.md) and
[screen catalog](../../research/ui-atlas/UI_SCREEN_CATALOG.md), especially IMG-006,
IMG-024 and IMG-079, establish spatial responsibilities, not universal pixels or
live interaction behavior. The comparison checks top task commands, the left
model/command region, contextual fields/confirmation, central graphics, optional
right supporting pane and bottom context. [SDD §10](../SDD.md#10-ui-and-interaction-architecture)
remains the design owner.

The app uses original icon drawings, native Qt/macOS controls, a system-palette
light/dark canvas and text-labelled actions. Command tabs sit above their groups;
the left manager uses text tabs. Finish/Cancel are explicit buttons. No vendor
screenshots, logos or artwork are product assets. The right pane currently offers
navigation help; libraries, appearances and integrations are not implemented.
Configuration/display managers, confirmation-corner behavior, 3D feature history,
assembly/drawing tools and complete keyboard/accessibility parity remain later
work. These are deliberate scope differences; a familiar layout is not complete
SolidWorks compatibility.

## Remaining blockers

| Check / decision | Status and owner |
|---|---|
| Physical trackpad and mouse ergonomics/preferences | Blocked: automation dispatches Qt events; a person must exercise real hardware. Module 9 / input acceptance. |
| Windows/Linux, X11/Wayland, native menus/focus/filesystems | Blocked: no native runners/session/compiler; OD-02. Candidate source/presets are not passing evidence. |
| Intel/minimum macOS/additional GPUs/displays | Blocked: current arm64 M2/Retina only; OD-02/OD-04. Scale overrides are not five physical monitors. |
| Full assistive technology and theme/contrast matrix | Blocked: native accessibility tools and alternate system appearance were not exercised; Module 9 / OD-08. |
| Controlled OS-network-disabled workflow | Blocked: no controlled native network isolation procedure; Module 9. Ordinary local execution does not certify network-disabled acceptance. |
| Installed packages/signing/upgrades and product identity/license | Blocked: owner decisions/credentials and package environments unavailable; Modules 10/12, OD-01. |
| Full physical 8 GB B0–B5 resource budgets | Blocked: later feature/assembly/drawing engines/workloads are absent; Module 11 / OD-07. Running on 8 GiB hardware alone does not close memory/FPS/latency/launch requirements. |
| Power loss/full-volume and prior-format conversion | Existing Modules 2/10/12 gates remain; process SIGKILL and SQLite quota-full are not physical power loss/full SSD evidence. |

The full T-01/T-03–T-05 and T-12–T-16 requirements remain Blocked for their missing
platform/device/later-engine acceptance. **4.1a and all later exact-feature tasks
remain unchecked; further implementation requires another user instruction.**
