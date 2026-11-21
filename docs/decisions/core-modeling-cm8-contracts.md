# Decision: Core-Modeling CM8 Semantic Contracts

## Status
Accepted — 21 November 2026.

## Context
Core-modeling milestone CM8 encompasses Shell, Sweep and Loft modeling features across 27 coverage
register rows (CM-201 through CM-227) sourced from course manual T8 *Advance Modelling Tools* (pages
10–12, 14–17, 22–23, 27–28, 32–37, 43–49, 52).

Following the architectural separation established across Modules 1–6 and CM1–CM7, Shell, Sweep and Loft
operations are defined as immutable, versioned semantic contracts. The desktop UI owns semantic drafts,
inputs and transient display meshes; the CAD worker owns exact OpenCASCADE evaluation, rebuilds, and
authoritative model writes.

Surfaces remain strictly distinct from solids across all layers. Surface results cannot satisfy solid-only
operations or measurements, and invalid or canceled operations preserve the accepted model state without
alteration.

## Semantic Schema

### 1. Common Invariants
- Schema version: integer 1.
- Strict key whitelists; unexpected, misspelled, or extraneous keys throw `InvalidInput`.
- Strict SI semantic units (meters, radians).
- All referenced external targets use typed `domain::GeometryReference` (roles: `Plane`, `Face`, `Edge`, `Vertex`, `Axis`, `Point`, `Curve`, `Object`).
- Admission limits: maximum 32 profile regions, maximum 16 guides, maximum 32 loft sections, maximum 64 connectors, maximum 64 shell remove faces / thickness overrides, maximum 128 KiB intent payload size.
- Dependency order and cycle detection in the history graph are strictly checked and enforced.

### 2. Shell Intent (`kind: "Shell"`)
A feature that hollows out a solid producing body, optionally removing opening faces and applying per-face thickness overrides:
- `version`: integer 1.
- `id`: unique UUID.
- `kind`: `"Shell"`.
- `target`: reference to the target producing body `{source: GeometryReference(Object), body: UUID}`. Must belong to the target stage.
- `thickness`: default wall thickness in meters (`(0, 1000]`).
- `inward`: boolean flag. `true` offsets walls inward (retaining external envelope, CM-203); `false` offsets walls outward (retaining internal cavity boundary, CM-204).
- `remove_faces`: array of 0 to 64 face references (`role == "Face"`).
  - Empty array indicates a closed cavity (CM-202).
  - Non-empty array specifies openings created by removing the referenced faces (CM-201).
  - Duplicate face references throw `InvalidInput`.
  - All referenced faces must belong to the target stage.
- `overrides`: array of 0 to 64 per-face thickness overrides (CM-205).
  - Each item: `{face: GeometryReference(Face), thickness: double in (0, 1000]}`.
  - Referenced face must belong to the target stage.
  - Duplicate override faces throw `InvalidInput`.
  - A face cannot be both in `remove_faces` and `overrides` (throws `InvalidInput`).

### 3. Sweep Intent (`kind: "Sweep"`)
Sweeps a profile along an open, closed, or 3D path, guided by optional curves, corner transitions, and tangency constraints:
- `version`: integer 1.
- `id`: unique UUID.
- `kind`: `"Sweep"`.
- `operation`: Boolean intent (`"NewBody"`, `"Add"`, or `"Cut"`).
- `target`: `null` for `"NewBody"`; required body reference `{source, body}` for `"Add"` and `"Cut"`.
- `profile`: profile definition:
  - `type == "Sketch"`: closed or open sketch contour (`sketch`, `mode` in `{"AllClosed", "SelectedRegions", "OpenChains"}`, `regions`).
  - `type == "Circular"`: analytical circular profile with positive `diameter` in `(0, 1000]` meters (CM-207).
- `path`: path definition:
  - `reference`: `GeometryReference` (role `Curve` or `Edge`) referencing a 2D sketch, 3D spatial sketch, derived curve, or solid model edge.
  - `reverse`: boolean flag to reverse path traversal orientation.
- `guides`: array of 0 to 16 guide curve references (`role == "Curve"` or `Edge`, CM-215). Guides cannot duplicate each other or the path.
- `corner_transition`: corner transition policy (`"Round"` or `"Sharp"`, CM-217). Corners are explicitly modeled; no silent smoothing.
- `start_tangency`: condition at path start (`"None"` or `"PathTangent"`, CM-216).
- `end_tangency`: condition at path end (`"None"` or `"PathTangent"`, CM-216).
- `thin`: thin wall settings (`"Solid"`, `"OneDirection"`, `"Midplane"`, `"TwoDirection"`, with `thickness` and `reverse`, CM-211..CM-214).
  - If profile is `OpenChains` and thin mode is `Solid`: requires `"Cut"` operation and `flip_side` boolean; otherwise rejected.
- `preview_sections`: integer in `[0, 32]` specifying bounded section previews along the path (CM-218).

### 4. Loft Intent (`kind: "Loft"`)
Blends through two or more ordered sections with optional centerline, guide curves, correspondence connectors, and start/end conditions:
- `version`: integer 1.
- `id`: unique UUID.
- `kind`: `"Loft"`.
- `output`: `"Solid"` or `"Surface"` (CM-221, CM-222).
  - If `"Surface"`: operation must be `"NewBody"`, target must be `null`, thin mode must be `"Solid"`. Result publishes as a distinct non-solid surface body; solid-only operations refuse it.
  - If `"Solid"`: operation is `"NewBody"`, `"Add"`, or `"Cut"`. Sections must be closed (AllClosed or closed SelectedRegions; `OpenChains` rejected with `InvalidInput`).
- `operation`: `"NewBody"`, `"Add"`, or `"Cut"`.
- `target`: `null` for `"NewBody"`; required body reference `{source, body}` for `"Add"` and `"Cut"`.
- `sections`: ordered array of 2 to 32 section profiles (`sketch`, `mode`, `regions`, CM-219, CM-220).
  - Order is explicit and preserved.
  - Topology must be consistent: all sections must be open or all sections must be closed (mixing open and closed throws `InvalidInput`).
- `centerline`: optional centerline definition `{reference: GeometryReference(Curve/Edge), reverse: bool}` (CM-224). Centerline guides the section placement and orientation frames.
- `guides`: array of 0 to 16 guide curve references (`role == "Curve"` or `Edge`).
- `connectors`: array of 0 to 64 correspondence connectors (CM-225, CM-226, CM-227):
  - Each connector has an immutable UUID `id` and an ordered array of `points` matching `sections.size()`.
  - Each point has `section` (integer 0 to N-1) and normalized parameter `parameter` in `[0, 1]`.
  - Crossing / synchronization check: for any pair of connectors, their relative order along the section parameter space must not invert or cross on any section (`InvalidInput` "Loft connectors must not cross").
- `start_condition`: condition at first section (`"None"`, `"NormalToSection"`, `"TangentToFace"`).
- `end_condition`: condition at last section (`"None"`, `"NormalToSection"`, `"TangentToFace"`).
- `thin`: thin wall settings (`"Solid"`, `"OneDirection"`, `"Midplane"`, `"TwoDirection"`, CM-223).

### 5. Graph Integration and Dependency Rules
- All new feature kinds (`Shell`, `Sweep`, `Loft`) register as `Feature` graph nodes in `evaluation::Graph`.
- Paths and guide curves can reference `Sketch`, `SpatialSketch`, `DerivedCurve`, or `Feature` edges.
- Section profiles reference planar `Sketch` objects.
- Graph validation checks cycle detection (`DependencyCycle`) and history ordering (`DependencyOrder`). Upstream targets must precede downstream operations.
