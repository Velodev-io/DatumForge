# Module 4 exact part features and measurement

Current authorization note: the following is the preserved Module 4 closeout. The subsequent user “yes” authorized Module 5; see [Module 5 evidence](module-5.md). The current boundary is **before Module 6 / 6.1a**.

Date: 20 October 2026. **Status: all feasible available-Mac implementation and verification complete; full platform/device completion Blocked.**
The user explicitly authorized remaining Module 4 tasks starting at 4.1a.
Stop before **Module 5 / 5.1a** and ask before proceeding.
[Authorization](../decisions/implementation-scope.json).

The original [Module 4.0 workspace report](module-4-workspace.md), its immutable
runs, [Module 3 input repairs](module-3-input-repair.md), research and user files
are preserved. The existing Features/Sketch/Evaluate tabs and left manager were
extended with real exact-feature controls. No agents, remote, history rewrite,
publication or existing user app session were used.

## Implementation

[Part adapter decisions](../decisions/module-4-features.md) specify typed SI
schemas, supported cases, profile/tolerance checks, admission, topology provenance,
measurement, material density and the Module 5 boundary.

| Task | Verified available-Mac scope |
|---|---|
| 4.1a — Pass | Exact extrusion and revolve; NewBody/Add/Cut, closed profile/holes/arcs/circles, direction/extent/axis, disposable preview and durable acceptance. |
| 4.1b — Pass | Finite/unit/type/range, closure/self-intersection, solid count, shell validity and tolerance checks. Failed operations retain the committed model. |
| 4.2a — Pass | Constant fillet, equal-distance chamfer, blind/through cylindrical holes, linear/circular exact patterns and planar mirror. |
| 4.2b — Pass | Actual generated/modified/deleted OCCT maps, source/target IDs and explicit missing/ambiguous tokens. Copied-body edges resolve inside their source result. |
| 4.3a — Pass | Revision-bound exact edge length/angle/radius, face area/radius, body area/volume/bounds; valid assigned density produces mass. |
| 4.3b — Pass | Left contextual feature fields, Preview/Accept/Cancel, feature tree and parameter editing, timings, initial semantic history and full ordered replay after sketch edits. |
| 4.4a — Pass | Analytic and production-dispatcher corpus for all eight families, including valid/invalid, discard, commit/dedup, undo/redo and save/reopen. |
| 4.4b — Pass | Exact measurements unchanged by mesh deflection/display mode; SI scale corpus and mm/in sketch regression, invalid sizes/profiles/axes. |
| 4.4c — Blocked (Mac subset Pass) | Native B1 sketch → extrusion → cut/hole/pattern → fillet → dimension/feature edit → undo/redo → save/reopen → SIGKILL/recovery. Full platform/device completion remains Blocked. |

Only the serial worker owns exact shapes, numerical solving and project writes.
The UI holds typed semantic/display projections and meshes. Versioned semantic
collections reuse schema v1 with a new compatibility manifest; prior saved files
are not rewritten on read and older workers refuse feature definitions.

Module 4 conservatively replays ordered history atomically. General dependency
invalidation, result caches, suppression/delete/reorder and split/merge repair
remain **unstarted Module 5 work**, not implicit completed capabilities.

## Reproduction and evidence

```sh
python3 tools/verify_module4.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

The [feature verification record](artifacts/module-4/features-verification.json)
records exact commands, durations, host and immutable run directory. It is separate
from the historical workspace verification record. Use a logged-in Mac desktop
with pinned local dependencies. Every test uses a marked disposable root and
refuses release/unmarked recovery paths. Native tests launch their own windows
and worker, never reusing a user project or restarting the user's existing app.

The clean-source procedure exports to a temporary directory and verifies cached
archives, then uses the existing pinned dependency binaries. It does not certify
a second from-scratch dependency build, installed package or another platform.

## Actual final verification

Final run: `features-runs/20261020T113302.472224Z` on macOS 26.6.2 arm64,
Apple M2, **8,589,934,592 physical bytes (8 GiB)**. Cocoa reports OpenGL 4.1.
The repository remains uncommitted, with no remote, on `historical/module-4`.
Individual source hashes identify the tested checkout; the tested aggregate is
`2047d1d8d3dc99a8f16953f023c1368631b21641cf072d4cd4b48f3ffbcafdd3`.
[Source consistency](artifacts/module-4/features-runs/20261020T113302.472224Z/evidence-consistency.json)
verifies every listed input plus unchanged dependency/tool/Qt-patch and frozen B1 pins.

| Verification | Actual result and evidence |
|---|---|
| Configure/build | Pass: [configure](artifacts/module-4/features-runs/20261020T113302.472224Z/configure.txt), [build](artifacts/module-4/features-runs/20261020T113302.472224Z/build.txt). |
| Full enabled CTest | [31 passed, zero failures](artifacts/module-4/features-runs/20261020T113302.472224Z/ctest.txt), 35.08 seconds. The deliberate failure remains Disabled by design; the harness failure-reporting checks pass. |
| Formatter / analyzer | [Pinned format Pass](artifacts/module-4/features-runs/20261020T113302.472224Z/format.txt); [Clang analyzer Pass](artifacts/module-4/features-runs/20261020T113302.472224Z/static-analysis.txt), no reported warnings. |
| Ownership / preservation | [Actual linked desktop/worker and packaged-worker check Pass](artifacts/module-4/features-runs/20261020T113302.472224Z/architecture.json); [97 research files unchanged, no remote](artifacts/module-4/features-runs/20261020T113302.472224Z/research-isolation.txt). |
| Native exact-feature workflow | Pass at [100%](artifacts/module-4/features-runs/20261020T113302.472224Z/native-features-100.txt), [125%](artifacts/module-4/features-runs/20261020T113302.472224Z/native-features-125.txt), [150%](artifacts/module-4/features-runs/20261020T113302.472224Z/native-features-150.txt), [175%](artifacts/module-4/features-runs/20261020T113302.472224Z/native-features-175.txt), [200%](artifacts/module-4/features-runs/20261020T113302.472224Z/native-features-200.txt); zero failed/skipped cases. |
| Prior native behavior | Workspace and Module 3 native workflows Pass at every scale; see [exact commands/results](artifacts/module-4/features-runs/20261020T113302.472224Z/verification.json). [Module 1](artifacts/module-4/features-runs/20261020T113302.472224Z/native-module1-regression.txt) and [Module 2](artifacts/module-4/features-runs/20261020T113302.472224Z/native-module2-regression.txt) native regressions Pass. |
| Clean source | [Export/configure/build and 31 enabled tests Pass](artifacts/module-4/features-runs/20261020T113302.472224Z/clean-reproduction-details.txt), with verified cached archives and existing pinned dependency binaries; clean tests take 44.41 seconds. |
| Visual review | [Reviewed subset Pass](artifacts/module-4/features-runs/20261020T113302.472224Z/visual-review.json): [100% feature fields](artifacts/module-4/features-runs/20261020T113302.472224Z/native-100/features-editing.png), [200% feature fields](artifacts/module-4/features-runs/20261020T113302.472224Z/native-200/features-editing.png), [100% model tree](artifacts/module-4/features-runs/20261020T113302.472224Z/native-100/features-B1.png). Intermediate scales retain native interaction/visibility assertions. |

The exact kernel corpus reproduces the frozen B1 final **35,488.76110196153 mm³**
volume. The native workflow explicitly changes outer width from 80 to 90 mm and
checks **40,488.76110196153 mm³**, including the unchanged four holes and fillet.
The screenshots show that later edited state, not a changed frozen fixture.
All native feature runs save revision 18, kill the isolated test worker after a
newer recoverable revision 19, recover 19, then reopen the unchanged saved revision
18 and verify stable definitions, geometry and volume. Native face picking,
feature parameter editing, failed preview refusal, Escape cancellation, parent
sketch edits, undo/redo, density-based mass and display invariance are asserted.

Per-feature times are diagnostics, not SRS latency/FPS acceptance. Effective DPRs
1/1.25/1.5/1.75/2 use one Retina display. Running these workflows on physical 8 GiB
hardware does not close Module 11 budgets. Full 4.0c/4.4c platform/device gates stay
Blocked; 4.1a–4.4b are checked only for the explicit available-Mac scope.

## Failures and corrections retained

Development logs are retained under [features-dev](artifacts/module-4/features-dev).
Initial compiler errors in CBOR/widget conversion and the first test harness are
preserved alongside successful rebuilds. The sandbox-only IPC handshake failure
is distinct from a host-side test result. The host UI test found Preview remaining
disabled on the second command; new commands now reset their own button/input
state. The first Cocoa run failed the Escape assertion; the native harness now
requires active-window and viewport focus before exercising the shortcut, and
retains the actual cancellation assertion. Screenshot review moved diagnostics
out of the form's available height, collapsed timings, and corrected the viewport's
old reference-scene label. Copied-body dressing and deterministic feature-local body identity have focused geometry regressions.
Two earlier full verification attempts were explicitly interrupted after source
review changes; it is not final source-identity evidence.

## Remaining blockers

| Gate | Actual state |
|---|---|
| Physical mouse/trackpad ergonomics/preferences | Blocked; automated Qt input is not real device delivery. |
| Windows/Linux/X11/Wayland, Intel/minimum macOS, additional GPUs/displays | Blocked; current arm64 M2/Retina only. Effective scale overrides do not prove five physical displays. |
| Native assistive technology, alternate appearance/contrast | Blocked; current native visual subset only. |
| Controlled OS-network-disabled workflow | Blocked; local IPC/no account does not prove OS network-isolation acceptance. |
| Full physical 8 GB B0–B5 budgets | Blocked; current hardware has 8 GiB, but Module 11 resource/FPS/latency/launch gates remain unrun. |
| Installed packages/signing/upgrades, identity/license decisions | Blocked; Modules 10/12 and OD owners retain these gates. |
| Physical power loss/full-volume, prior-format conversion | Existing Module 2/10/12 gates remain; SIGKILL and quota-full are narrower evidence. |

The stopping point is **before 5.1a**. No Module 5 task has been implemented or checked.
