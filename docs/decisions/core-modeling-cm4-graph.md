# CM4.a — typed geometry references and model dependency graph

1 November 2026. The user authorized CM4.a with “okk go ahead with cm4.a”.
Work starts at `c11c3da` on `review/cm4-a-20261101`. Preserve all earlier evidence,
the native C++/Qt/OCCT/PlaneGCS architecture, the physical 8 GB baseline and deferred
6.1b/6.4c/7.4c. Stop before Module 11. No merge or push.

## Scope and approach

Implement the CM4.a structural contract in production and connect existing
sketch-Pierce and feature-history evaluation to it. One kernel-free graph handles
Sketch, DatumPlane, DatumAxis, DerivedCurve and Feature nodes. Exact construction,
face/edge evolution and repair, durable support collections and datum UI belong
to CM4.b–e. Structural reference acceptance does not claim geometric resolution.

Extending only the feature graph would leave cross-kind cycles undetectable.
Keeping independent validators would duplicate ordering rules. The chosen shared
validator retains the existing feature-history and sketch collection interfaces
while making their references explicit. This is an implementation decision under
the authorized CM4.a specification, not a new user-approved product requirement.

## Reference and graph contracts

- Version-1 `GeometryReference` carries document scope, owning object UUID,
  geometry role, optional sketch entity UUID or typed topology token, and an
  optional Start/End endpoint or finite curve parameter. No display indices.
  Unknown keys/versions, malformed UUIDs/tokens, invalid selector combinations
  and incompatible source roles fail explicitly.
- `Self` resolves in the supplied document; an explicit matching document UUID
  also resolves there. Other-document geometry edges are refused until document
  closure/evolution support is implemented. Existing cross-document profile
  gathering still supplies project-unique sketch nodes to the legacy adapter.
- Graph node IDs are unique across kinds. Typed member references must identify
  an existing member of the appropriate role. Missing sources/members are
  `ReferenceMissing`; cycles are `DependencyCycle`; history-order violations
  are `DependencyOrder`. Validate the complete graph before returning a plan.
- Topological ties retain input collection order. Optional explicit history lists
  constrain their listed nodes, including dependencies through unlisted nodes.
  Legacy sketches remain unordered collections; feature history remains ordered.
  New datum/support callers must supply their complete modeling history.
- Graph metadata admits at most 132 nodes (the existing 32-profile impact union
  plus 100-feature bound), 512 references per node and 256 members per sketch.
  Existing project, geometry, transport and solver admission limits stay intact.
  These metadata bounds are not physical-memory or latency acceptance evidence.
- Graph traversal yields deterministic evaluation order, direct dependencies
  and transitive descendants. Feature consumers keep their existing feature-only
  result shape. Source-sketch dependency hashes propagate into consuming feature
  fingerprints, without persisting a new schema or cache payload.

## Verification

Use real graph/reference/model APIs with hand-derived mixed-kind fixtures.
Exercise missing/wrong-kind/member references, cross-document refusal, duplicate
identities, self/mixed cycles, direct and transitive history violations, bounded
admission and a long chain. Verify legacy Pierce ordering, feature history,
upstream sketch invalidation and unrelated-branch reuse. Run full enabled CTest,
pinned formatting, changed-source Clang analysis and ownership/coverage/spec
checks. This task adds no native interaction; no new native/platform/device or
resource acceptance is inferred. CM4 coverage rows remain open for CM4.b–e.
