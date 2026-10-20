# Module 4 exact part features and measurement

Date: 20 October 2026. **Status: final available-Mac verification in progress.**
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

| Task | Implemented scope; verification must complete before checking |
|---|---|
| 4.1a | Exact extrusion and revolve; NewBody/Add/Cut, closed profile/holes/arcs/circles, direction/extent/axis, disposable preview and durable acceptance. |
| 4.1b | Finite/unit/type/range, closure/self-intersection, solid count, shell validity and tolerance checks. Failed operations retain the committed model. |
| 4.2a | Constant fillet, equal-distance chamfer, blind/through cylindrical holes, linear/circular exact patterns and planar mirror. |
| 4.2b | Actual generated/modified/deleted OCCT maps, source/target IDs and explicit missing/ambiguous tokens. Copied-body edges resolve inside their source result. |
| 4.3a | Revision-bound exact edge length/angle/radius, face area/radius, body area/volume/bounds; valid assigned density produces mass. |
| 4.3b | Left contextual feature fields, Preview/Accept/Cancel, feature tree and parameter editing, timings, initial semantic history and full ordered replay after sketch edits. |
| 4.4a | Analytic and production-dispatcher corpus for all eight families, including valid/invalid, discard, commit/dedup, undo/redo and save/reopen. |
| 4.4b | Exact measurements unchanged by mesh deflection/display mode; SI scale corpus and mm/in sketch regression, invalid sizes/profiles/axes. |
| 4.4c | Native B1 sketch → extrusion → cut/hole/pattern → fillet → dimension/feature edit → undo/redo → save/reopen → SIGKILL/recovery. Full platform/device completion remains Blocked. |

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
old reference-scene label. Copied-body dressing has a focused geometry regression.
An earlier full verification attempt was explicitly interrupted after source
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
