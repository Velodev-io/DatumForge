# Centerpoint Arc pointer interaction repair

Date: 22 October 2026. User requested SolidWorks-style arc sizing and selected
**Centerpoint: center → start → end**. No Module 11 work, no commit or push.
Existing edits/research/evidence and deferred 6.1b/6.4c/7.4c remain preserved.

Status: **Pass for documented available-Mac Centerpoint Arc scope**; full
platform/physical-device/assistive-technology/resource/installed acceptance remains
Blocked or Not run.

The previous pointer Arc path immediately invoked numeric creation on the first
click, using the default 2 mm radius / 0-degree start / 90-degree sweep. The red
regression records that first-click semantic emission. This was an interaction
gap; prior numeric-arc tests did not certify dynamic arc sizing.

The matching reference is official [SolidWorks Centerpoint Arc help](https://help.solidworks.com/2025/english/solidworks/sldworks/t_Sketching_Centerpoint_Arcs.htm):
center, move/click start/radius, move/click end. [3 Point Arc](https://help.solidworks.com/2026/english/SolidWorks/Sldworks/t_Sketching_3_Point_Arcs.htm)
and [Tangent Arc](https://help.solidworks.com/2026/english/SolidWorks/Sldworks/t_Sketching_Tangent_Arcs.htm)
are distinct methods, not claimed by this selected-method repair. No installed
SolidWorks comparison or exact visual/behavior parity for every option is claimed.

Center and start remain transient world-space anchors. Pointer motion previews
radius/start and then signed sweep with unit-aware live text and persistent canvas
prompts even when Properties is hidden. The selected radius stays fixed while
placing the end. Signed angular increments pass the atan2 branch and support
minor/major arcs in both directions. Clockwise arcs use the existing positive-sweep
representation by exchanging endpoint order, preserving geometry without a schema
change. Zero radius, coincident angular endpoints and full turns cannot finish.
Escape cancels unfinished input; another Escape selects, and tool changes discard
unfinished input. Numeric creation is separate and cancels pending pointer input.

Only a completed arc adds one bounded local undo edit. No project transaction,
worker request or entity-list rebuild occurs per pointer move. Preview/Finish are
blocked while the primitive is unfinished. The worker validates completed sketches;
accepted history, save/reopen and IDs use the existing authoritative paths.

Tests use marked disposable roots and refuse release recovery paths. The running
user app/draft is never used or stopped by these tests. The old running process
needs a normal restart after the user finishes/preserves its draft to use new code.

## Verification

Run `python3 tools/verify_arc_interaction.py --native` from the checkout root, then
`python3 docs/scripts/check_specs.py --write-report`. The runner never starts or
stops the interactive user app; native test windows use marked disposable roots.

[Red reproduction](artifacts/module-3/arc-runs/20261022T034856Z/red-input.txt)
shows `centerpointArcIsTransient`: first click emitted one semantic creation call
instead of zero. Earlier mm/in numeric-arc tests had passed because they exercised
the coordinate button, not this pointer path. Pointer-state and visible-preview
assertions now cover the actual call site.

Final run `20261022T040046.253795Z` on macOS-26.6.2-arm64-arm-64bit-Mach-O: [command ledger](artifacts/module-3/arc-runs/20261022T040046.253795Z/verification.json).

[Complete CTest](artifacts/module-3/arc-runs/20261022T040046.253795Z/ctest.txt): **53 enabled
cases passed**, zero failures. The deliberate failure probe remains Disabled. This
includes new `module3.arcs`, numeric mm/in creation, existing shape gestures, real
solver/persistence, B1/B2/B3/B5 and crash/recovery regressions. Unit input assertions
check changing preview size before placement, CW/CCW and 270-degree arcs across
the angular branch, zero/full-turn refusal, tool cancellation, resize and native
pinch while pending. The new worker/UI mm/in flow verifies no accepted state
before Finish, unfinished Preview/Finish refusal, no worker requests or entity
insertions during 60 pointer moves, draft undo/redo, exact completed arc values,
one durable sketch acceptance, stable IDs through durable undo/redo and save/reopen.

Configure/build, pinned formatting, [Clang analysis of the five changed C++
translation units](artifacts/module-3/arc-runs/20261022T040046.253795Z/changed-source-analysis.txt),
[ownership](artifacts/module-3/arc-runs/20261022T040046.253795Z/architecture.json), research
isolation, Cocoa Arc mm/in workflows at 100/125/150/175/200% effective scale and
prior native sketch regressions at 100/200% passed. [Manual visual review](artifacts/module-3/arc-runs/20261022T040046.253795Z/visual-review.json)
records the exact screenshot subset. No separate clean/dependency rebuild or
installed-build validation was run for this localized interaction repair.

[Source identity](artifacts/module-3/arc-runs/20261022T040046.253795Z/source-identity.json):
branch `feature/modules-8-10-drawings-hardening`, original commit `d1fce435b1e9e4f05b7f36fab6392f870fedd8d7` plus all
preserved edits. Aggregate tested source SHA-256 `e99455b7c01a34a7ba302736bdd3dd82594cb43d45d7fd4b2579270cd98519b2`; frozen
source inputs remained unchanged during and after verification. Prior Module 8–10
full-run hashes remain historical and are not relabeled as testing this later code.

The full support, physical device, native accessibility, resource and installed-build
gates remain Blocked/Not run; 6.1b/6.4c/7.4c records are unchanged. Native scaling
uses one display, and QTest pointer/pinch events do not certify physical hardware.
Three-point arcs, tangent/normal intent zones, autotransition and automatic arc
tangency constraints remain unsupported. **Module 11 is unstarted. No commit/push.**

Subsequent explicit user authorization expands the implementation in [all-arc follow-up](module-3-all-arcs.md). Unsupported-mode statements above describe this historical Centerpoint-only run, not the later implementation. Its frozen source hash and 53-test result are unchanged.
