# CM4.b — exact datum and sketch-support engine

1 November 2026. Authorized by “go ahead with cm4.b”. Start at `997e1db`, on
`review/cm4-b-20261101`. Preserve all prior evidence and deferred 6.1b/6.4c/7.4c;
stop before Module 11. Commit/push this review branch after successful scoped
verification under the standing 1 November instruction; no merge/force-push.

## Scope and approach

Use the CM4.a typed graph and the pinned native OCCT/PlaneGCS worker. Add semantic
transient support definitions, exact reference resolution, datum evaluation,
sketch attachment and orthographic Convert Entities. Evaluate mixed model history
in dependency order, rebuilding exact feature prefixes from the updated sketches.
Expose a versioned read-only `PreviewModelSupports` command. It changes neither
accepted geometry nor durable revision and adds no functioning native controls.
Durable support collections, unique topology evolution/repair and native selection
remain CM4.c/d; full course/reopen/native acceptance remains CM4.e.

A separate datum-only utility would not exercise dependent solids; putting kernel
objects in sketch/UI types would violate ownership. The chosen worker engine
returns only bounded semantic frames, curves, sketch data and measurements across
IPC. These are implementation decisions under the existing authorized task.

## Exact construction rules

- Principal XY/XZ/YZ planes and X/Y/Z axes have explicit base-datum records;
  offset/angle operations do not need a synthetic sketch to stand in for them.
- Planes: Offset (signed SI distance, 1–32 equally spaced outputs), ParallelPoint,
  Angle about a coplanar line/edge/axis, ThreePoints, LinePoint, TwoLines,
  NormalCurve, TangentSurface and Midplane. ThreePoints/LinePoint/TwoLines cover
  the course's through-points/lines family. Degenerate/skew operands fail.
- Angle is radians. A curve-normal plane requires an explicit parameter or
  Start/End selector. Nonplanar tangent planes use explicit normalized trimmed
  face UV fractions in [0,1]; trim exclusion and surface singularities fail.
  Nonparallel midplanes require an explicit Internal/External bisector choice.
- Reverse orientation flips a plane's v direction (preserving u/origin), or an
  axis direction; signed offset controls which side receives the plane. Multiple
  plane output UUIDs derive from the definition identity and semantic slot.
- Axes: FaceAxis (cylindrical/conical face), Line (straight curve/edge/axis),
  TwoPlanes, TwoPoints and PointNormal (point plus plane/face normal). Curved
  edges do not masquerade as straight axes. Parallel plane intersections,
  coincident points and missing/ambiguous topology are explicit errors.
- Attachments reevaluate planar-face/datum-plane support, optional SI offset,
  in-plane angle and reversed normal. Sketch-local coordinates retain their
  meaning in the resulting frame; the former world frame is not frozen.
- Convert Entities accepts sketch points/curves and exact model edges/face
  boundaries. Projection is orthographic along the target normal. Exact trimmed
  rational B-splines carry projected conics/splines; straight edges remain lines.
  There is no tessellation or fitted-curve substitution. Rotation, positive
  uniform scale (including the course's 0.75) and translation are target-local.
  Generated IDs derive from conversion/source semantic identity. Generated point
  coordinates are fixed for the subsequent sketch solve; controlling input
  dimensions/relations are preserved, with conflicts refused atomically.
  Later references to generated sketch members carry a bounded member declaration
  from a prior preview. Structural planning checks the identity/role; exact
  reevaluation verifies that member before use. Declarations neither bypass
  exact Part validation nor write project data.

## Reference and execution boundaries

Typed source object/member/topology identity must resolve uniquely in the exact
producing feature stage, never by display ordinal or nearest candidate. Curve
parameters follow existing semantic conventions; an edge parameter is a fraction
of its oriented finite trimmed interval. Face UV fractions are dimensionless.
All world lengths use SI; kernel coordinates remain millimetres.

The preview definition carries base sketches, feature definitions, datums,
attachments, conversions and a complete explicit mixed history. The graph rejects
cycles/order violations before kernel evaluation. Existing same-Part external
sketch entities are supported; general external-document geometry remains refused
until explicit closure/evolution is implemented. Existing shared-Part profile
gathering remains available. Converted output is reconstructed from base input
on each preview, not appended to the previous preview or silently committed.

Preserve existing 16-sketch exact admission, 100 features, 132 graph nodes,
256 entities/variables, 512 constraints and 256 KiB control/project limits.
Datums/conversions share the graph budget; large exact projections fail their
existing curve/solver limits. No increased resource claim or new durable schema.
Cancellation/revision/epoch checks and one-running/one-replaceable-pending preview
remain worker/client responsibilities. Long OCCT calls retain existing cancellation
granularity; no stronger preemption claim is made.

## Verification

Test every construction family with independent SI equations and exact OCCT
curve/solid invariants, including upstream edits, missing/ambiguous references,
orientation, singularities, bounds, graph cycles and failed/canceled previews.
Cover mixed face → datum → sketch → feature history and transformed conversions.
Test real worker/client transport, stale revision and unchanged accepted state.
Run full enabled CTest, pinned format, changed-source Clang analysis, ownership,
coverage/specification and source-integrity checks. Native/platform/installed/
physical-device/resource acceptance remains unrun and Blocked.
