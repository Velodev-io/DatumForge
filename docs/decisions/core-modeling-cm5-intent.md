# CM5.a — solid modeling intent contracts

3 November 2026. Authorized by “proceed with cm5.a”; base `1b51191`, review
branch `review/cm5-a-20261103`. Execute in-session, without delegation. Commit
and push verified scope under the standing workflow; no merge or force-push.
Preserve deferred 6.1b/6.4c/7.4c, native architecture and physical 8 GB baseline.
Stop at CM5.a; exact adapters, persistence, UI and acceptance remain CM5.b–e.

## Approach and boundary

Add an immutable, validated `features::ModelingIntent` semantic contract to the
existing kernel-free feature-model library. Its version-1 CBOR representation
and graph projection are available to future adapters. It does not extend
`Feature::parse`, accepted project collections, worker commands or visible tools.
This is a production contract API, not a functioning solid-feature adapter.

CM4's typed graph and support identities are available prerequisites. The earlier
CM0 inclined-plane surface-termination experiment is preserved under tests; it
does not establish general curved starts/ends or thin/draft adapters. Those exact
feasibility/acceptance gaps remain open for CM5.b/e and do not block defining
independent semantic contracts here. Existing CM0/full-release gates are retained.

Documentation alone would leave mode combinations unenforced. Extending today's
accepted feature parser would let unimplemented operations reach the worker and
saved files. A separate strict intent parser provides executable contracts without
either change. This is the implementation choice under the authorized CM5.a
specification, following the repository's existing kernel-free model conventions.

## Common schema, ownership and references

`ModelingIntent::parse(QCborMap)` constructs only valid version-1 definitions;
`map()` returns their value representation, `graphNode()` exposes all geometry
dependencies, and `graph(upstream, history, document)` validates source kinds,
scope, complete history, cycles and ordering through the existing shared graph.
Top-level `version`, UUID `id` and `kind` are mandatory. Every map has exact keys;
unknown/inactive fields, wrong types, nonfinite numbers and unknown modes fail.
No silent defaults, clamping or downgrade to today's feature parameters.

References use CM4 `GeometryReference` identities. A body selection is
`{source: Object reference to producing Feature, body: body UUID}`; the worker
must later verify that body in the named producing stage. Point-like references
are Point/Vertex or an explicitly selected Curve/Edge endpoint/parameter. Plane
references are Plane/Face; axes are Axis/Curve/Edge without point selectors.
Exact planarity, straightness, intersection branch and body membership belong
to CM5.b. Missing/ambiguous exact references require explicit repair, never a
nearest/display-index substitute. Cross-document references remain refused.

One definition is bounded to 128 KiB and 512 references. Selection limits below
are conservative within existing graph/feature admission, not a RAM claim.
The UI owns only semantic drafts and meshes; the serial worker owns exact
resolution, preview, Boolean operations and writes. CM5.c must introduce a new
compatibility manifest before these definitions can be saved.

## Profiles and regions

Extrude/Revolve have `operation` NewBody/Add/Cut, `target` null for NewBody or a
body selection for Add/Cut, and `profile: {sketch, mode, regions}`. `sketch` is a
UUID of a Sketch graph node; reusing it never consumes or modifies that sketch.
`mode` is AllClosed (empty regions), SelectedRegions, or OpenChains.

SelectedRegions carries 1–32 regions. Each region is an array of boundary loops,
outer first then holes; OpenChains carries one boundary per region. Each boundary
contains directed curve uses `{entity, from, to, reverse}` using stable sketch
curve UUIDs and normalized trimmed-curve parameters `0 <= from < to <= 1`.
Reverse changes traversal, not the stored interval. At most 256 total curve uses
and 256 boundaries are admitted. Repeated identical intervals within one boundary
are refused; neighboring regions may share a curve. CM5.b must reconstruct exact
connectivity/closure, nesting, intersection splits and union of selected cells,
rejecting overlaps, degeneracy or ambiguous membership. Trimmed intervals are
selection intent, never tessellation indices; upstream changes that invalidate
their boundary require repair instead of freezing old world-space geometry.

OpenChains require a thin feature, or an Extrude Cut with explicit `flip_side`
choosing which side of the oriented chain removes material. `flip_side` is
mandatory only for an open, non-thin cut; inactive use is refused. The exact
adapter must refuse a chain that does not partition its target unambiguously.
With sketch normal n and directed tangent t, the local left side is n cross t;
`flip_side=false` removes that side, true removes the right. AllClosed derives
canonical outer counterclockwise/hole clockwise loops in the sketch frame;
explicit selections retain their directed traversal.

## Starts, directions and ends

Extrude/Revolve require `start`, `direction1`, nullable `direction2`, Boolean
`reverse`, and `thin`. Extrude starts:

| Start mode | Additional fields | Meaning |
| --- | --- | --- |
| SketchPlane | none | Current reevaluated sketch plane |
| Surface | reference | Exact Face/Plane start support |
| Vertex | reference | Point-like reference fixes the start station |
| Offset | distance | Signed SI offset along the sketch normal |

Revolve uses SketchPlane start and a typed `axis` reference. Its source must be
straight, coplanar with the profile and uniquely resolved by CM5.b. `reverse`
reverses the positive sketch-normal extrusion direction or the positive
right-hand angular direction about the resolved axis. Direction 2 proceeds
oppositely from the same start, with its own end and draft parameters.

Every end has `mode`. Extrude modes and extra fields:

| Mode | Fields | Meaning |
| --- | --- | --- |
| Blind | value | Positive depth in metres |
| Midplane | value | Total depth, half on each side; direction2 must be null |
| UpToVertex | reference | Point-like target station |
| UpToSurface | reference | Exact Face/Plane termination |
| OffsetFromSurface | reference, offset, offset_reverse | Nonnegative SI separation, explicit side |
| UpToBody | reference, body | Exact producing-stage Object reference and body UUID |
| UpToNext | none | First valid target boundary in that direction |
| ThroughAll | none | Complete extent through the explicitly selected target |

UpToNext/ThroughAll require Add/Cut with a target. Reference-terminated extents
carry no cached depth. Surface offset side is independent of extrusion reversal.
Midplane is valid only for direction1. Revolve supports Blind/Midplane angular
`value` in radians; combined direction angles cannot exceed 2*pi. A full turn
is one turn, not duplicate coincident material. No extrude termination is silently
interpreted as an angular termination. Positive lengths use [1e-8,1000] m;
signed start offset uses [-1000,1000] m; surface separation uses [0,1000] m;
positive angles use [1e-8,2*pi]. These retain existing scalar admission ceilings.

Vertex starts/ends define the plane perpendicular to extrusion through the
resolved point, preserving sketch-local coordinates. Surface starts/ends resolve
the actual support intersection along the extrusion rays; they do not project
an arbitrary sample to a single depth. Missing coverage, multiple admissible
branches or zero/reversed extents fail explicitly. OffsetFromSurface is a normal
offset of that support before termination; `offset_reverse` reverses its resolved
normal side independently of extrusion reversal. UpToBody ends at the first
admissible boundary of the selected body's producing stage. UpToNext/ThroughAll
search only the explicit target, not unrelated visible bodies.

## Thin and draft semantics

`thin` is `{mode: Solid}`, `{mode: OneDirection, thickness, reverse}`,
`{mode: Midplane, thickness}`, or `{mode: TwoDirection, thickness1, thickness2}`.
Midplane thickness is total, split equally. OneDirection reverse swaps material
side relative to the oriented profile; TwoDirection values are independent
positive opposite-side thicknesses. Closed profiles form wall bands; open profiles
are thickened with end closure to a solid. Offsets/self-intersections that cannot
form a valid solid fail; no automatic tolerance inflation or profile repair.
OneDirection false uses the local left side and true the right; TwoDirection
thickness1 uses left, thickness2 right. Thickness is measured normal to the
profile in its sketch plane, rather than along extrusion or around a revolve.

Extrude directions each require `draft: {angle, outward}`. Angle is a nonnegative
radian magnitude strictly below pi/2; outward chooses widening versus narrowing
away from the start along that direction. Zero is an explicit no-draft value.
Direction2 has independent draft: the T5 pp.2–3 fixture is 0.010 m / 35*pi/180
and 0.015 m / zero. Revolve has no draft field. Thickness and draft remain distinct
parameters. Outward expands material: exterior boundaries widen and hole
boundaries contract; inward reverses both. Thin offsets are defined at the start,
then both wall boundaries follow this material-side rule. Thickness is therefore
a start-section value, with no implicit constant-wall-thickness option. Exact
offset/draft construction and solid validity remain CM5.b work.

Standalone Draft uses a different exact-key shape: `target`, `neutral` Plane/Face
reference, typed `pull` axis, 1–32 unique Face `faces`, positive `angle < pi/2`,
and `outward`. The neutral plane stays fixed; pull reversal follows the resolved
axis orientation. Tangent propagation/parting-line draft are outside this contract.

## Verification and acceptance

Contract tests cover every registered CM5 mode, SI values, two-direction course
parameters, invalid combinations/types/unknown fields, finite/bound checks,
region descriptors, same-sketch reuse, graph dependencies and upstream closure,
wrong-kind/missing sources, cross-document edges, cycles and ordering. Existing
`Feature::parse` must still refuse these definitions. Tests do not invoke OCCT,
save new definitions or certify the drafted course solid. Run full enabled
CTest, pinned format, changed-source analysis and ownership/coverage/spec checks.
Coverage rows remain In progress with exact geometry/native/full acceptance Not
run. Native tests are not applicable to this contract-only change; Windows/Linux,
installed/device/resource acceptance and CM5.b–e remain open.
