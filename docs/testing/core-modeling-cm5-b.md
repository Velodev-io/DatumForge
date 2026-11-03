# CM5.b — exact solid intent engine and read-only worker preview

3 November 2026. Authorized by “go ahead with 5.b”, followed by “continue”.
Base `c4070c3`, branch `review/cm5-b-20261103`.
[Engine decisions](../decisions/core-modeling-cm5-engine.md),
[CM5.a contracts](../decisions/core-modeling-cm5-intent.md) and
[plan](../superpowers/plans/2026-11-03-cm5-b.md).

## Production scope

The existing exact worker evaluates transient version-1 CM5 intents after a
complete original support model. `PreviewModelSupports` accepts an optional
`intents` array while retaining its result version/fields and existing generation,
revision, cancellation and one-running/one-replaceable-pending lifecycle. Accepted
feature definitions, saved schemas and manifests remain unchanged.

The engine builds exact whole/selected profile regions and holes, adjacent-region
unions, oriented trimmed curve boundaries, thin open/closed sections, exact
starts/extents, independent directions and draft, solid/thin revolve and cuts,
open-profile cut sides and standalone neutral-plane draft. NewBody uses stable
feature-derived body identities; Add/Cut must name a current producing stage and
retain one target solid with the intended volume change. Actual kernel history,
tools and stages are retained only in the transient result for CM5.c integration.

Surface termination uses exact selected support geometry; changing the support
changes the exact result. Finite construction envelopes are not semantic depths.
Exact cap provenance, trimmed-domain coverage, profile footprint and removal of
workspace end caps are checked. Missing, ambiguous, backward and partially
covering limits are refused. UpToNext/Body selects the start-adjacent exact solid
cell of the explicit target. No display tessellation, nearest face or cached
fixed-depth fallback is used.

## Geometry and worker verification corpus

- Exact whole/selected rectangle area, holes, adjacent cells and source reuse;
  disconnected intervals, overlapping cells and touching holes refused.
- Closed rectangle and circle thin bands, inward/outward sides, open-chain end
  connectors, independent opposite thicknesses, total Midplane thickness and
  offset collapse. Existing 16-boundary exact profile admission retained.
- Independent directed 10/15 mm extrusion; reverse, total Midplane, signed Offset
  and Vertex starts; UpToVertex, inclined plane termination, changed upstream
  plane, normal surface offset and surface start. Independent area/volume/bounds.
- Independent outward 35-degree/10 mm plus zero-draft/15 mm equation, inward
  draft, drafted Midplane, planar surface termination and UpToNext/Body draft.
- Solid/full/partial/midplane and closed/open thin revolve; exact derived-axis
  resolution; real revolved cut with an independent annular volume equation.
- ThroughAll cut, body-terminated Add, stale target-stage rejection, blind thin
  cut and refused split-solid annular cut. Open cuts verify both sides and blind
  depth. Spherical termination uses an independent integrated cap-volume equation.
- Standalone planar draft and cylindrical-wall-to-cone draft volume equations;
  accepted input Part is unchanged. Missing references, reversed/crossing/partial
  limits, identity collisions, cancellation and pre-kernel byte admission.
- Direct Projects/ProjectStore preview checks exact measurement/evaluation order,
  unchanged revision/project state/display and nondurable operation outcome;
  repeats, stale revision, canceled/malformed intents and durable-parser refusal.
- Client/worker transport checks response admission, latest-only input bursts,
  cancel-before-result and restart with unchanged revision/document records.
  Storage uses marked disposable roots; release/private recovery paths stay refused.

These are analytical engine fixtures, not the full T5 course model or new native
authoring acceptance. Native controls/mesh manipulators remain CM5.d/e.

## Retained red/green and diagnosis evidence

Logs are in [the CM5.b artifact folder](artifacts/core-modeling/cm5-b-20261103/).
Initial API reds are missing-header compile failures; behavioral reds follow real
compiled APIs. The first worker test used nonexistent query command names and was
corrected before recording the missing-optional-payload behavioral red. Compiler
failures caused by missing type headers and an unsupported OCCT initializer-list
constructor remain visible in their individual build logs.

| Behavior | Red/diagnosis | Green |
| --- | --- | --- |
| Exact selected profiles | `profiles-api-red.txt` | `profiles-api-green.txt` |
| Thin sections and correct open/closed offset mode | `thin-red.txt`, `offset-edges.txt`, `offset-closed-flag.txt` | `thin-sided.txt` |
| Solid intent engine API | `features-api-red.txt` | `features-api-green.txt` |
| Exact starts/extents and reference caps | `extents-red.txt`, `reference-validity.txt` | `exact-clipping.txt` |
| Revolve, Boolean and body termination | `revolve-body-red.txt`, `body-extents-red.txt` | `body-policy.txt` |
| Open cut and standalone draft | `open-draft-red.txt`, `open-green-draft-red.txt` | `draft-green.txt` |
| Drafted Midplane / reference / body limits | `draft-extents-red.txt`, `body-draft-red.txt` | `draft-extents-green.txt`, `body-draft-green.txt` |
| Finite support and full-footprint refusal | `failure-bounds-red.txt`, `cap-coverage.txt` | `cap-boundary.txt` |
| Touching-hole rejection | `touching-hole-red.txt` | `touching-hole-green.txt` |
| Combined byte budget before kernel/cancel | `admission-red.txt` | `admission-green.txt` |
| Derived curve axis availability | `derived-axis-red.txt` | `derived-axis-green.txt` |
| Exact profile boundary budget | `exact-profile-budget-red.txt` | `exact-profile-budget-green.txt` |
| Optional worker payload / nondurable preview | `worker-red-corrected.txt` | `worker-green.txt` |

Offset diagnosis showed that OCCT's open-result mode left a closed rectangle's
corner unjoined. Separately, an empty carrier face omitted the orientation input
used to distinguish offset sides. Matching open/closed mode and using the source
wire's carrier face corrected both; no tolerance relaxation or healing shortcut.
The feature-prism experiment reported completion but returned a null shape in this
standalone context. Exact Boolean support clipping replaced that unsuccessful
adapter; failures remain retained rather than presented as passing geometry.

The sandbox transport preflight could not start its worker; see
`transport-preflight.txt`. The same focused transport check subsequently passed
with authorized host IPC. Final full host verification supplies authoritative logs.

## Final verification

The [final timestamped verification folder](artifacts/core-modeling/cm5-b-20261103/verification/20261103T152143.162364Z/)
records the exact commands, logs and machine-readable result:

- Full development build: Pass.
- Full enabled CTest: **94 passed, zero failures**, 412.70 seconds. The deliberate
  harness failure probe remained Disabled and is not counted as passed.
- Pinned clang-format 21.1.8 and all **nine** changed-source Clang analyses: Pass.
- Native UI/worker ownership, research isolation, coverage, specifications and
  whitespace: Pass. Final documentation/task registration is checked separately.
- Source integrity: all **274** frozen source/configuration hashes matched.

CM5.b production exact engine and available-Mac development-build/worker
verification are **Pass for the documented engine scope**. All 29 whole-workflow
coverage rows remain In progress; the live manipulator has no engine/UI acceptance.
New native workflow verification is Not run. No source changes followed this run.

[Author source review](artifacts/core-modeling/cm5-b-20261103/source-review.md)
records ownership, reference, tolerance, admission and retained-state checks.
It is not independent review or merge approval. Trailing horizontal whitespace
from compiler/Ninja logs is trimmed for repository checks; diagnostics remain.

## Remaining boundaries

Drafted open non-thin cuts, draft from nonplanar neutral/start supports and
topology-changing/unsupported analytic draft cases are explicitly refused. Conical
and complex tangent-chain draft need broader fixtures. Arbitrary imported-surface,
complex curve-offset and full course combinations remain subject to exact
validity/ambiguity/admission failures. See the engine decision's precise limits.

No new semantic definition is saved, no durable undo/reopen/repair acceptance
is claimed and no new functioning native control exists. CM5.c–e, full page-indexed
course/platform/installed/device/assistive/physical-8-GB resource acceptance remain
open/Blocked. Preserve deferred 6.1b/6.4c/7.4c; Module 11 is not started. Author
source review is separate from independent review required before any merge.

## Delivery

Feature commit `81a596e` was pushed to `origin/review/cm5-b-20261103`; its remote
SHA matched the verified local commit. [Delivery evidence](artifacts/core-modeling/cm5-b-20261103/delivery.json)
records that check before this documentation-only follow-up. All 274 source/
configuration hashes also matched after publication. No merge or force-push;
independent review remains required before merge.
