# CM5.b — exact solid-intent engine

3 November 2026. Authorized by “go ahead with 5.b” in the CM5 conversation.
Base `c4070c3`, branch `review/cm5-b-20261103`. Preserve the native architecture,
physical 8 GB baseline, all prior evidence and deferred 6.1b/6.4c/7.4c. Stop
before CM5.c and Module 11. Commit/push verified scope; no merge or force-push.

## Approach

Consume [CM5.a intent](core-modeling-cm5-intent.md) in the existing serial exact
worker. Extend the existing read-only PreviewModelSupports payload with an
optional versioned `intents` array; existing callers and result fields remain
valid. Reevaluate the original support model, then the transient ordered intents.
Carry actual producing-stage identities and typed references through every
operation. No new durable definitions, manifest, accepted-feature parser or
native tools; persistence and native controls remain CM5.c/d.

The chosen extension reuses support resolution, cancellation, revisions and
preview coalescing. A separate exact process would violate worker ownership;
accepting unpersistable definitions through SetFeatures would cross CM5.c.
New intents follow the complete original support history; inserting new support
nodes between new solid intents is not introduced by this read-only API.

## Exact operations

- Build exact directed trimmed sketch wires, selected regions/holes and reused
  source profiles. Validate connectivity, self-intersections, hole containment,
  duplicates and overlap with the pinned kernel; adjacent regions may be united.
  Keep existing exact profile/body/topology admission. No fitted/polyline fallback.
- Construct thin wall sections with exact planar offsets and explicit material
  side, total midplane thickness, independent two-sided thickness and open-chain
  end closure. Reject offset degeneracy and intersecting/ambiguous results.
- Apply independent extrusion directions, starts/extents and per-direction draft;
  exact reference termination retains the actual limiting support geometry.
  A bounded construction envelope is only an algorithm workspace, never a
  replacement semantic depth. Refuse unresolved, backward or ambiguous limits.
- Revolve exact regions/thin sections about a uniquely resolved coplanar straight
  axis; enforce one-turn, direction, self-intersection and solid validity rules.
- Standalone neutral-plane draft modifies explicitly selected target-stage faces.
  Kernel-supported analytic face families are tested independently. Unsupported
  topology-changing or curved draft combinations fail explicitly and are recorded;
  they are never silently replaced by a different operation.
- NewBody publishes stable feature-derived body identities. Add/Cut must use the
  selected current body stage, change volume in the correct direction and retain
  one valid target solid. Missing, disconnected, empty and split results fail.
  Preserve actual kernel Generated/Modified/Deleted history without yet saving it.

## Validation

Use independent SI volume/bound equations for directed 10/15 mm extrusion,
inclined/curved termination, changed upstream support, offsets, both cut sides,
thin open/closed sections, revolve and neutral-plane draft. Test missing/ambiguous
references, wrong target stage, crossings, offset collapse, cancellation and
admission. Test real store and worker preview with unchanged accepted revision,
geometry and command outcome; marked disposable roots only. Run the full enabled
suite, pinned format, changed-source analysis, ownership/coverage/specification
and frozen source checks. No native controls or platform/resource acceptance
are implied. Document every remaining exact limitation separately from passing
mode fixtures; do not close whole CM5/course rows for engine-only evidence.

## Exact implementation boundaries

The preview carries version-1 intent maps in the optional `intents` array. No
schema upgrade is inferred from this transient transport extension. Older workers
refuse the unknown field. Original support nodes are evaluated first; new intents
then follow in explicit order and can reference earlier intent stages. New datums
interleaved between transient intents are not supported by this endpoint.

Reference termination clips a finite exact construction prism by the selected
support half-spaces, or selects the start-adjacent exact solid cell for body
termination. It checks actual cap provenance, whole-profile footprint coverage,
finite face domains, and disappearance of workspace end caps. Bounding boxes and
one interior ray select a bounded query range and branch, never the result depth;
exact kernel intersections construct and validate the result. Multiple admissible
surface intersections, reversed or partial footprint coverage fail explicitly.
Thin offsets use exact intersection joins, with open/closed offset-result modes
and explicit end connectors. No approximation flag or tolerance inflation.

Admission retains 100 combined features / 128 KiB definitions, 256 KiB control
and result limits, 132 graph nodes and ten bodies, plus the existing exact shape
validity, face, tolerance and display limits. Exact profiles remain bounded to
16 regions/thin boundaries; metadata admission alone does not promise geometry
admission. An interior seed search uses at most 31 by 31 UV probes after the
area-centroid attempt; failure to find a seed is an explicit error, not a guess.
Cancellation checkpoints surround kernel work; long OCCT calls retain existing
cooperative cancellation granularity rather than a new preemption guarantee.

Draft supports OCCT's analytic plane/cylinder/cone family when topology remains
valid; automated independent equations cover planar faces and cylindrical walls.
Conical/complex tangent chains need broader fixtures before course acceptance.
Draft on a nonplanar start/neutral support and drafted open non-thin cuts are
explicitly refused. Ordinary open cuts require endpoints outside the target's
projection and a connected exact cut curtain; a chain that cannot partition its
target uniquely fails. These combination restrictions remain visible engineering
limits, not substitute geometry or whole-course acceptance. Other curve-offset,
topology-changing draft and arbitrary imported-surface cases retain kernel validity
and ambiguity refusal. The full page-indexed course corpus remains CM5.e.

All failures leave the input semantic model and accepted project revision intact.
Preview stage/tool/history maps stay transient; durable Boolean provenance and
repair behavior remain CM5.c. The response is the existing semantic measurement
preview, not a new native mesh manipulator or authoring control.
