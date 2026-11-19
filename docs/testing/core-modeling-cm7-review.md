# CM7 review verification — 19 November 2026

Scope: native spatial sketching (3D sketches), plane-relative relations (ParallelYZ,
ParallelZX, AlongZ), active drafting plane with Tab switching, derived curve tools
(SketchOnSketch, SketchOnFaces, CompositeCurve, HelixSpiral), endpoint/tangent frames,
durable definitions, replay, reference repair, and nonsolid evaluation refusal.
Preserve accepted-state and worker ownership boundaries. No CM8–CM9 or Module 11 work
is started.

## Requirements and verification matrix

| ID | Variant | Required behavior | Evidence seam |
|---|---|---|---|
| CM-038 | ParallelYZ | Line's local X component is zero; rotating selected plane re-solves correctly | `cm7.solver` (`testParallelYZRotatedPlane`), `cm7.contracts` |
| CM-039 | ParallelZX | Line's local Y component is zero; tested with non-world datum plane | `cm7.solver` (`testParallelZXNonWorldDatum`), `cm7.contracts` |
| CM-040 | AlongZ | Direction is normal to selected plane; conflicting direction constraints report failure | `cm7.solver` (`testAlongZConflicting`), `cm7.review` (`testChairFramePathFixture`) |
| CM-190 | 3D sketch entities | Points, lines, arcs, splines admitted; exact curves and evaluations evaluated in worker | `cm7.curves` (`testSpatialEntitiesExactCurves`), `cm7.contracts` (`testSpatialEntities`) |
| CM-191 | 3D dimensions | Spatial dimensions independently measured; driving/reference distinction, units | `cm7.solver` (`testAdmittedEntitiesAndDimensions`, `testReferenceDimensionDoesNotConstrain`), `cm7.workflow_contracts` |
| CM-192 | Active-plane switching | Tab key switches planes before/after first click, preserving anchor/world geometry without moving coordinates | `cm7.workflow_contracts` (`nativeEffectiveScales`), `SketchPanel` active-plane logic |
| CM-193 | Axis/plane inference | Visible proposals, explicit confirmation; unconfirmed proposals do not survive commit | `cm7.contracts` (`testInferenceProposals`), `cm7.workflow_contracts` |
| CM-194 | On Plane | Signed point-to-plane distance within tolerance; moved offset datum plane rebuilds path | `cm7.solver` (`testOnPlaneAndMovedDatum`), `cm7.review` (`testChairFramePathFixture`) |
| CM-195 | Sketch-on-sketch projection | Satisfies both original projections; source sketch mutation updates curve correctly | `cm7.curves` (`testSketchOnSketchProjection`), `cm7.review` (`testT8ProjectedPathFixture`) |
| CM-196 | Sketch-on-face projection | Lies on selected trimmed face along vector or normal to face; boundary miss detection | `cm7.curves` (`testSketchOnFacesProjection`), `cm7.contracts` |
| CM-197 | Projection branch selection | Multiple candidate branches require explicit choice; survives benign rebuild, reports missing on loss | `cm7.curves` (`testSketchOnSketchProjection`), `cm7.store` (`testBranchAmbiguityAndRepair`), `cm7.review` |
| CM-198 | Composite path | Order, orientation and endpoints match sources; gaps, branch ambiguities, and corner limit tangents reported | `cm7.curves` (`testCompositeCurveGapsAndDiscontinuities`), `cm7.review` (`testCompositeAndEdgeCasesFixture`) |
| CM-199 | Helix/spiral | Independent radius, axial rise/turn, handedness, taper angle; spiral planar containment; 4 modes verified | `cm7.curves` (`testHelixSpiralAllModes`), `cm7.review`, `cm7.workflow_contracts` |
| CM-200 | Stable endpoint/tangent frames | Endpoint/tangent frames agree with independent derivatives; datum plane and sketch attachment rebuild | `cm7.curves` (`testStableEndpointTangentFramesAndDatumAttachment`), `cm7.review` (`testT8ProjectedPathFixture`) |

## Supported scope and remaining restrictions

- 3D sketches support Points, Lines, Arcs, and B-splines. Planar constraints
  (ParallelYZ, ParallelZX, AlongZ) require a plane reference (or default to world
  XY plane frame). World constraints (AlongX, AlongY) operate globally or relative to plane.
- Plane switching via Tab key cycles XY -> YZ -> ZX without displacing existing
  anchor points or injecting unintended OnPlane constraints.
- Derived curves (SketchOnSketch, SketchOnFaces, CompositeCurve, HelixSpiral) evaluate
  strictly on the worker side using OpenCASCADE geometry (`libcad_support_geometry`).
  The desktop UI manipulates CBOR definitions and requests preview/evaluation via Client.
- Reference curves publish as distinct non-solid reference objects in the model tree
  and are explicitly refused if selected as profiles for Extrude or Revolve solid features.
- Endpoint-normal datum planes (`NormalCurve` mode) evaluate Frenet frames using the
  stable up-vector policy and support downstream sketch attachment.
- Course model reconstructions: T8 projected curve path fixture and chair-frame path
  fixture verified. Swept tube along the path and mirror feature belong to CM8.
- Physical input devices, assistive technology, Windows/Linux, installed release,
  and physical 8 GB resource gates remain open/deferred. Existing gates 6.1b, 6.4c, 7.4c
  remain Blocked.

## Verification record

Run `python3 tools/verify_cm7_review.py --native`. It freezes source hashes, builds,
runs all enabled CTests, pinned formatting, changed-source Clang analyses,
architecture/isolation/coverage/specification checks, followed by native Cocoa/OpenGL
workflows in mm/in at observed 100/125/150/175/200% effective DPR scaling. Tests use marked
disposable roots; no user project or release recovery path is used.

Artifacts generated under:
`docs/testing/artifacts/core-modeling/cm7-review-20261119/verification/20261119T045656.790738Z/`
- Build, CTest, Format, Changed-source static analysis: all PASS.
- Architecture, Research-isolation, Coverage, Specifications: all PASS.
- Native workflows (100%, 125%, 150%, 175%, 200% DPR): all PASS with screenshots captured.

