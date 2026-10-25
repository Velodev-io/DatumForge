# All Arc methods and EDP graph comparison

Date: 22 October 2026. Explicit user follow-up to 3.1b/3.2a/3.2b. Stop before
Module 11. Preserve the earlier Centerpoint report, all Module 8–10 work and
6.1b/6.4c/7.4c Blocked gates. No commit/push.

Verification status: **Pass for the requested available-Mac scope**. All 54 enabled tests, changed-source analysis and native workflows passed. Full acceptance remains Blocked; unrun checks are listed below.

## Why the screenshot still showed fixed-size arcs

The original pointer Arc implementation called numeric creation on its first click,
using the 2 mm / 90° defaults. The earlier Centerpoint repair replaced that path,
but the active interactive app was still an older running executable: PID 4047
started at 09:08:02 local, before the repaired binary's 09:32:04 modification time.
Rebuilding does not replace code already loaded in a running process. The supplied
screenshot therefore did not establish behavior of the rebuilt app. The user's
unsaved app was not closed or used as a test target. Automated cases launch their
own applications and workers in marked disposable roots.

## Reference material actually inspected

The requested chat is [EDP Exam](https://chatgpt.com/c/6ad8ed75-cca0-83ee-aab6-044b67175157).
Most older assistant replies displayed “This response couldn’t load”; they were
not treated as read. Its `T2 PART IN 2D.pdf` attachment was opened in the chat
viewer, and a matching local filename/initial pages was found at
`/Users/binova/Downloads/T SHEETS FOR FACULTY ONLY/T2 PART IN 2D.pdf`.
The local PDF was read with PDFKit; pages 17–18 were rendered and visually reviewed.
No checksum equivalence with the remote attachment is claimed.
[Source metadata](artifacts/module-3/all-arc-runs/20261022T050338Z/reference-source.json)
records the local SHA-256 and access limits. [Extracted arc pages](artifacts/module-3/all-arc-runs/20261022T050338Z/course-arc-pages.json)
and [page 17](artifacts/module-3/all-arc-runs/20261022T050338Z/course-page-17.png) /
[page 18](artifacts/module-3/all-arc-runs/20261022T050338Z/course-page-18.png)
are durable source evidence.

The exercise draws connected lines through (0,0), (60,0), (60,35), (50,35),
(50,25), (40,25) mm. While Line remains active, move away and return to its last
endpoint to enter Arc. Moving down then left produces a normal semicircle with
R10 mm and 180°; completing it returns to Line. Continue left 10, up 10, left 10
and close to the origin. This is a **normal intent within Tangent Arc**, not a
fourth Arc tool. The test follows this actual sequence, in mm and inch documents,
and verifies one arc, nine lines, endpoint relations and save/reopen.

Official help defines [Centerpoint](https://help.solidworks.com/2025/english/solidworks/sldworks/t_Sketching_Centerpoint_Arcs.htm),
[3 Point](https://help.solidworks.com/2026/english/SolidWorks/Sldworks/t_Sketching_3_Point_Arcs.htm)
and [Tangent](https://help.solidworks.com/2026/english/SolidWorks/Sldworks/t_Sketching_Tangent_Arcs.htm)
as distinct methods. Tangent includes tangent/normal intent, four intent directions
and eight possible results. Reference comparison is documentation/course based;
no installed SolidWorks session was used.

## Behavior comparison

| Method or interaction | Before this follow-up | Implemented behavior |
|---|---|---|
| Centerpoint | Earlier repair supplied three-click preview, but the user's running app predates it | Center → start/radius → end; held radius sizing also supported; live unit-aware radius, angle and direction; CW/CCW and major arcs |
| 3 Point | Unavailable | Start → end → bend; moving the third point changes curvature/radius continuously; collinear/duplicate points cannot finish |
| Tangent | Unavailable | Click an existing Line/Arc endpoint, move and click the end, or press/drag/release; live radius/angle; stored tangent relation |
| Normal intent | Unavailable | Initial movement along the normal selects a perpendicular tangent direction; return near the source to reset intent; eight axis/curvature outcomes checked |
| Line transition | Unavailable | Chained Line move-away/return switches to Arc; completing Arc resumes the prior Line/Polyline and joins its next support point |
| Tool discovery | One Arc action | Shared Centerpoint/3 Point/Tangent actions, flyout and Properties selector; remembers the last method across tool switching in the session |
| Unfinished input | Earlier Centerpoint transient state | All arc methods stay local until valid completion; Escape/tool changes cancel the pending primitive; Preview/Finish refuse it |

Choose the Arc arrow in the Sketch toolbar to see all three tools. In Tangent mode,
select a source curve before clicking a shared endpoint if more than one eligible
curve ends there. The source is not silently guessed. Independent Centerpoint and
3 Point arcs do not infer joins. Numeric center/radius/start/sweep creation remains
available for independent arcs; Tangent uses its source endpoint. Live on-canvas
text is a readout, not a dynamic numeric-entry editor.

## Current semantic graph

All methods persist the same bounded Arc entity: center Point ID, SI radius,
start angle and positive sweep. Clockwise construction exchanges endpoint order
and records the correct source endpoint selector. No persistent display mesh or
exact geometry object is added to the UI.

```mermaid
flowchart LR
    S[Source Line or Arc endpoint] --> J[ArcTangent or ArcNormal relation]
    J --> A[New Arc endpoint]
    A --- E[Other Arc endpoint]
    E --> C[ArcCoincident relation]
    C --> P[Following Line support Point]
```

| Graph record | Meaning and ownership |
|---|---|
| `ArcTangent` / `ArcNormal` | Stable relation ID; `a` new Arc ID, `b` source Line/Arc ID; `value` own endpoint 0/1, `second` source endpoint 0/1. Equality of endpoint coordinates plus parallel/perpendicular outgoing tangents. |
| `ArcCoincident` | Same selectors; target can also be Point with selector 0. Equality only. The resumed Line uses its actual support Point. |
| Solver-only endpoints | Worker-owned PlaneGCS ArcRules and endpoint variables. Included in the existing 256 numerical-variable cap; never serialized. |
| Validation | Worker checks selectors/types, finite/radius/sweep bounds, endpoint and angular residuals, rank/freedom, branch continuity and stored angle agreement. Failed candidates cannot replace accepted state. |

The existing v1 record shape is retained, with new explicit constraint kinds. Existing
projects are not migrated. A project using these kinds requires the current reader;
older-build acceptance is not claimed. UI pointer math only constructs a bounded
semantic preview. The single worker still owns solve, exact geometry and writes.
No request, transaction or entity insertion occurs per pointer move.

## Verification and reproduction

Run `python3 tools/verify_all_arcs.py --native`, then
`python3 docs/scripts/check_specs.py --write-report`. The runner freezes relevant
source hashes, records actual command exits and uses marked disposable roots;
production/release recovery roots are refused by the shared test launch paths.
[Final command ledger](artifacts/module-3/all-arc-runs/20261022T162147.341204Z/verification.json)
records exact requested checks. No tests operate on the interactive user's draft.

Focused red/green artifacts are retained under
`artifacts/module-3/all-arc-runs/20261022T050338Z`: unsupported-method and solver
failures before implementation; eight-intent/resource/branch regressions; and
`stored-angle-red.txt` / `stored-angle-green.txt`, which demonstrate rejection of
a stored arc whose solver would otherwise silently repair its start angle.
Development failures are retained: the first build had a test variable-name
collision; an early toolbar lookup caused native-window startup crashes and was
fixed by looking under the actual parented command tabs; a debugger probe timed
out without a useful stack; one premature invocation used a stale test executable
before build completion. Subsequent focused runs passed. An initial full runner
`20261022T161922.220675Z` was explicitly interrupted during build to add the stored-angle
guard; it is not counted as a completed verification.

Final run `20261022T162147.341204Z` on macOS 26.6.2 arm64: **54 enabled CTest cases
passed, zero failures**, in 273.14 seconds. `harness.deliberate_failure` remains
Disabled. Configure/build, pinned formatting, Clang analysis of eight changed C++
translation units, architecture ownership and research isolation all exited zero.

Native Cocoa `nativeArcs` passed at 100/125/150/175/200% effective scale, covering
Centerpoint plus 3 Point/Tangent/Normal in mm/in, worker Preview/Finish, semantic
relations and durable save/reopen, and the EDP Line transition. Prior
`nativeSketches` regressions passed at 100/200%. [Manual visual review](artifacts/module-3/all-arc-runs/20261022T162147.341204Z/visual-review.json)
records the exact six-image subset; no unreviewed image is implied to have been
manually checked. Input tests cover all eight intent outcomes, centerpoint held
radius sizing, collinear/major 3 Point arcs and ambiguous endpoint refusal. Solver
tests cover line/arc sources, both directions and tangent/normal at three model
scales, radius edits, invalid selectors, branch preservation, bounded auxiliary
variables and invalid stored angles.

[Source identity](artifacts/module-3/all-arc-runs/20261022T162147.341204Z/source-identity.json):
branch `feature/modules-8-10-drawings-hardening`, original HEAD `d1fce435b1e9e4f05b7f36fab6392f870fedd8d7`
plus preserved edits. Aggregate tested-source SHA-256
`b601d89913542201654c1f91b19c1a4b6e7c2ef0864089ced38b004f26e2b0de`. Frozen inputs stayed unchanged during verification;
prior full-suite hashes are not relabeled as evidence for this later source.

The updated development app was launched and left running after verification.
[Startup record](artifacts/module-3/all-arc-runs/20261022T162147.341204Z/startup.json)
records its managed session, app/worker PIDs, visible welcome window and marked
development root. No existing project or recovery was selected or modified.
A final prelaunch check found no remaining original CAD process; this task had not
closed it. The window is ready for New/Open. [Evidence consistency](artifacts/module-3/all-arc-runs/20261022T162147.341204Z/evidence-consistency.json)
checks the frozen inputs, original HEAD, exact deferred-gate records and Module 11
boundary after documentation updates.

## Remaining scope and preserved gates

All three core arc types are implemented for available Mac development scope.
Tangent sources are Line and Arc; ellipse/spline entities are unavailable. General
curve relations, full SolidWorks PropertyManager parity, in-canvas numeric-entry
locking, automatic line/arc tangent toggling by every SolidWorks gesture and
cross-session last-tool preference are not claimed. Direct dragging of constrained
joins remains conservative; dimension edits and worker Preview are authoritative.

Five effective native scales on one Retina display are application-event checks,
not physical mouse/trackpad certification. Other OS/architectures, assistive
technology, installed builds and full physical 8 GB workload/resource gates remain
Blocked/Not run. No new clean dependency rebuild is claimed for this follow-up;
prior clean-source and offline evidence remains historical. Deferred 6.1b/6.4c/7.4c
records stay exactly unchanged. Module 11 is unstarted. No commit/push.
