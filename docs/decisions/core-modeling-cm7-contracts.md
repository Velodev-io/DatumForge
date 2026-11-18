# Decision: Core-Modeling CM7 Semantic Contracts

## Status
Accepted — 17 November 2026.

## Context
Core-modeling milestone CM7 encompasses Spatial sketches and derived curves across 14 coverage
register rows (CM-038 to CM-040, and CM-190 to CM-200) sourced from course manuals T4 *Add Relations*
(pages 14–15) and T8 *Advance Modelling Tools* (pages 18–27, 48–49).

Following the architectural separation established across Modules 1–6 and CM1–CM6, spatial sketch
authoring, plane-relative relations, derived curve definitions, branch identities, and endpoint/tangent
frames are specified as immutable, versioned semantic contracts. The desktop owns semantic drafts and
display geometry; the sole worker owns solving, exact geometry, rebuilds and accepted writes.

## Semantic Schema

### 1. Common Invariants
- Schema version: integer 1.
- Exact key sets; unexpected or extraneous keys throw `InvalidInput`.
- Strict SI semantic units (meters, radians).
- All referenced external targets use typed `domain::GeometryReference` (roles: `Plane`, `Face`, `Edge`, `Vertex`, `Axis`, `Point`, `Curve`, `Object`).
- Admission limits: maximum 32 spatial sketches per document; maximum 64 entities and 64 constraints per spatial sketch; maximum 32 faces in sketch-on-faces; maximum 32 members in composite curves; maximum 132 total model dependency nodes.
- Dependency order and cycle detection in the history graph are strictly checked and enforced.

### 2. Spatial Entities (`cad::sketch::SpatialEntity`)
Every spatial entity carries an immutable UUID `id`, boolean `construction`, and typed `kind`:
- **Point**: 3D Cartesian coordinates `(x, y, z)` in SI meters (`[-1000, 1000]`). Must not define endpoints or poles.
- **Line**: references start point `a` and end point `b`. Requires `a != b` and both endpoints existing as `Point` entities.
- **Arc**: circular arc in 3D space defined by center point `center`, endpoints `a` and `b`, positive radius in `(0, 1000]`, and plane unit normal vector `normal` (`||normal|| = 1`). Points `center`, `a`, `b` must all differ.
- **Spline**: 3D B-spline / NURBS defined by 2 to 64 control poles (`poles`), integer degree `degree` in `[1, 7]` with `degree < poles.size()`, optional positive weights, and optional non-decreasing finite knot vector matching `poles.size() + degree + 1` with positive span.

### 3. Spatial Constraints and Relations (`cad::sketch::SpatialConstraint`)
Constraints carry an immutable UUID `id`, typed `kind`, entity operands `a`, `b`, optional `plane` reference, `value`, `driving` flag, and `reversed` flag (`third` is unused and must be empty):
- **ParallelYZ**: forces a line to be parallel to the local YZ plane of a selected plane (local X component is zero). Requires Line entity `a` and `plane` reference (`Plane` or `Face`).
- **ParallelZX**: forces a line to be parallel to the local ZX plane of a selected plane (local Y component is zero). Requires Line entity `a` and `plane` reference (`Plane` or `Face`).
- **AlongZ**: forces a line to be normal to a selected plane (local X and Y components are zero). Requires Line entity `a` and `plane` reference (`Plane` or `Face`).
- **AlongX / AlongY**: forces a line to be parallel to local X or local Y axis of the reference plane or global coordinate system.
- **OnPlane**: constrains a point or curve to lie directly on a selected datum plane or planar face (signed point-to-plane distance equals zero). Requires entity `a` and `plane` reference (`Plane` or `Face`).
- **Distance**: driving or reference spatial dimension between two points, a point and a line, two lines, or an entity and a plane (entity `b` and `plane` are mutually exclusive). Value in `(0, 1000]` meters.
- **Angle**: driving or reference spatial angular dimension between two lines or a line and a plane (entity `b` and `plane` are mutually exclusive). Value in `(0, pi)` radians.
- **Coincident, Perpendicular, Parallel, Tangent, EqualLength, Fixed**: 3D geometric relations between spatial entities (`EqualLength`, `Parallel`, `Perpendicular` require Line entities; `Tangent` requires curve entities; `Fixed` requires a Point entity; relations do not accept plane references or values).

### 4. Active Drafting Plane Mode
- The `SpatialSketch` schema includes an `active_plane` field representing the transient editor drafting orientation (`"XY"`, `"YZ"`, or `"ZX"`).
- Switching the active drafting plane (e.g. via Tab during interactive authoring) alters projection feedback only. It does NOT alter 3D anchor coordinates and does NOT inject implicit persistent `OnPlane` constraints.

### 5. Derived Curves (`cad::model::DerivedCurve`)
Carries an immutable UUID `id`, descriptive `name`, schema version 1, and `kind`:
- **SketchOnSketch**: projects two distinct planar sketches (`sketch1` and `sketch2`, role `Object` or `Curve`) onto one another along their respective sketch plane normals, producing a 3D intersection curve. Requires `sketch1.object != sketch2.object`. Includes deterministic `BranchSelection` (`index`, `token`, `reversed`, parameter interval `[start_param, end_param]`).
- **SketchOnFaces**: projects a source sketch (`sketch`, role `Object` or `Curve`) onto 1–32 target faces (`faces`, role `Face`). Projection `mode` is `"AlongVector"` (requires `direction` reference with role `Axis`, `Edge`, or `Plane`) or `"NormalToFace"` (forbids `direction`). Includes `BranchSelection`.
- **CompositeCurve**: ordered chain of 1–32 curve or edge members (`members`), each carrying a source `GeometryReference` (role `Curve` or `Edge`) and individual `reversed` boolean. Flag `closed` indicates closed loop topology.
- **HelixSpiral**: defines editable spatial helices and planar spirals:
  - Modes: `"PitchAndRevolution"`, `"HeightAndRevolution"`, `"HeightAndPitch"`, `"Spiral"`.
  - Base reference: `axis` (role `Axis`, `Edge`, or `Plane`), optional `center` (role `Point` or `Vertex`).
  - Common: radius > 0, start angle in `[0, 2*pi)`, handedness `clockwise` (boolean).
  - Mode-specific validation:
    - `PitchAndRevolution`: pitch > 0, revolutions > 0, taper angle in `[0, pi/2)`.
    - `HeightAndRevolution`: height > 0, revolutions > 0, taper angle in `[0, pi/2)`.
    - `HeightAndPitch`: height > 0, pitch > 0, taper angle in `[0, pi/2)`.
    - `Spiral`: pitch > 0, revolutions > 0; height and taper angle must be zero (strictly planar).

### 6. Endpoint, Tangent and Frame Semantics (`cad::model::CurveFrame`)
- **Reference Up-Vector Policy**: The tangent vector `T` determines the normal plane orthogonal to the curve, but does not determine in-plane rotation. The canonical frame `{T, N, B}` uses a stable reference up-vector policy:
  - Default reference up-vector: world Z `{0, 0, 1}`.
  - Near-singularity switch: if `|T . Z| > 0.999`, the reference up-vector automatically switches to world Y `{0, 1, 0}`.
  - Normal `N = normalize(up - (up . T) * T)`.
  - Binormal `B = cross(T, N)`.
  - Deterministically right-handed: `cross(T, N) = B` with determinant +1.
- **Singularity Handling**: If `||dP/du|| < 1e-12`, the frame is undefined. An explicit `InvalidInput` error is thrown; zero derivatives are never assigned arbitrary frames.
- **Reversal Semantics**: When `reversed == true`, tangent is negated `T' = -T`, binormal is negated `B' = -B`, and normal `N' = N` is preserved. Right-handed orthonormal frame properties are strictly preserved.
- **Corner Semantics**: At C0 tangent discontinuities (corners between composite segments or polylines), evaluating without specifying limit side or branch throws `InvalidInput` ("Ambiguous tangent at curve corner") via `CurveFrame::evaluateCorner`. Corners are preserved and never silently smoothed. Explicit limit sides (`"Left"` / `"Right"`) evaluate the canonical frame using the respective one-sided limit derivative.
- **Closed Seam Semantics**: Closed periodic curves evaluate identically and continuously at parameters `u = 0.0` and `u = 1.0`.

### 7. Graph Integration and Security Gate
- The model dependency graph (`cad::evaluation::Graph`) is extended with the `SpatialSketch` node kind alongside `Sketch`, `DatumPlane`, `DatumAxis`, `DerivedCurve`, and `Feature`.
- `SpatialSketch` nodes own member entities of type `Point` and `Curve`.
- **CRITICAL GATE**: `SpatialSketch` does NOT satisfy `ref.role == "Plane"`. Planar features (e.g. Extrude, Revolve) requiring a planar profile reject a spatial sketch with `ReferenceMissing`, preventing spatial sketches from accidentally satisfying planar requirements.
- Downstream datum planes (e.g. `mode == "NormalCurve"` at `endpoint == "Start"`) resolve `DerivedCurve` and `SpatialSketch` curves reliably.
- Cyclic dependencies between spatial sketches, datum planes, and derived curves are rejected with `DependencyCycle`. History ordering violations are rejected with `DependencyOrder`.
