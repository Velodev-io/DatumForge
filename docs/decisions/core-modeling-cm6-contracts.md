# Decision: Core-Modeling CM6 Semantic Contracts

## Status
Accepted — 15 November 2026.

## Context
Core-modeling milestone CM6 encompasses Holes, Ribs, Repetition (Patterns and Mirror),
and Treatments (Fillets and Chamfers) across 41 coverage register rows (CM-149 to CM-189)
sourced from course manual T7 *Modelling Tools* (pages 9–48).

Following the architecture established in CM5, these modeling features are defined as
typed, versioned semantic intents within `cad::features::ModelingIntent`, rather than
loosely structured parameters in the legacy `Feature` class.

## Semantic Schema

### 1. Common Invariants
- Schema version: integer 1.
- Exact key sets; unexpected or extraneous keys throw `InvalidInput`.
- SI semantic units (meters, radians).
- All referenced targets use typed `domain::GeometryReference` (roles: `Plane`, `Face`, `Edge`, `Vertex`, `Axis`, `Point`, `Object`/`Body`).
- Every intent specifies an immutable unique `id` (UUID), `kind`, and `operation` (`NewBody`, `Add`, `Cut`).
- Admission limits: maximum 100 intents / 128 KiB per document; maximum 32 edges per treatment; maximum 64 placement points per hole; maximum 32 count per pattern direction; maximum 1024 total pattern instances.
- Cycles and order dependencies in the history graph are strictly checked and rejected.

### 2. Feature Kinds

#### Hole
- `kind`: `"Hole"`
- `operation`: `"Cut"`
- `target`: `{"source": GeometryReference(Object), "body": UUID}`
- `family`: `"Simple"`, `"Counterbore"`, `"Countersink"`, `"Drilled"`, `"Tapped"`, `"PipeTap"`, `"LegacySimple"`, `"LegacySimpleDrilled"`, `"LegacyCounterboreDrill"`, `"LegacyCountersinkDrill"`, `"LegacyTapered"`
- `placement`: `{"face": GeometryReference(Face), "sketch": UUID, "points": [UUID, ...]}`
- `sizing`:
  - `mode`: `"Custom"` or `"Catalog"`
  - `diameter`: SI meters (> 0)
  - `depth`: SI meters (> 0)
  - Family-specific parameters:
    - Counterbore: `counterbore_diameter`, `counterbore_depth`
    - Countersink: `countersink_diameter`, `countersink_angle`
    - Drilled: `drill_angle` (e.g. 118° in radians)
    - Tapered / PipeTap: `taper_angle`
    - Tapped: `tap_drill_diameter`, `thread`: `{"cosmetic": bool, "designation": string, "pitch": double, "depth": double}`
  - If `mode` == `"Catalog"`: stores dataset identity (`dataset`, `version`, `standard`, `type`, `size`, `fit`) alongside resolved dimensions.
- `termination`:
  - `mode`: `"Blind"`, `"ThroughAll"`, `"UpToNext"`, `"UpToVertex"`, `"UpToSurface"`, `"OffsetFromSurface"`
  - `depth_reference`: `"Shoulder"` or `"Tip"`
  - Optional `reference`: `GeometryReference` (for surface/vertex extents)
  - Optional `offset`: double, `offset_reverse`: bool

#### CosmeticThread
- `kind`: `"CosmeticThread"`
- `target`: `{"source": GeometryReference(Object), "body": UUID}`
- `reference`: `GeometryReference` (role `"Edge"` or `"Face"`)
- `designation`: string (e.g., `"M42x3.0"`)
- `standard`: string
- `type`: string
- `size`: string
- `thread_type`: `"Internal"` or `"External"`
- `major_diameter`: double (> 0)
- `minor_diameter`: double (> 0)
- `pitch`: double (> 0)
- `termination`: `{"mode": "Blind" | "ThroughAll" | "UpToNext", "depth": double}`
- Geometry rule: Solid volume and topology remain unchanged. Output consists of display descriptors only.

#### Rib
- `kind`: `"Rib"`
- `operation`: `"Add"`
- `target`: `{"source": GeometryReference(Object), "body": UUID}`
- `profile`: profile map containing `sketch` UUID, `mode` (`"SelectedRegions"` or `"OpenChains"`), and region curve boundaries.
- `thickness`: double (> 0)
- `side`: `"Both"`, `"First"`, or `"Second"`
- `direction`: `"Parallel"` (in-plane) or `"Normal"` (perpendicular to sketch plane)
- `flip_material`: bool

#### Repetition: LinearPattern, CircularPattern, Mirror
- `kind`: `"LinearPattern"`, `"CircularPattern"`, or `"Mirror"`
- `operation`: `"NewBody"`, `"Add"`, or `"Cut"`
- `target`: required for `Add` and `Cut`
- `seed_kind`: `"Feature"`, `"Face"`, or `"Body"`
- `seed`:
  - If `"Feature"`: feature UUID
  - If `"Body"`: body selection map
  - If `"Face"`: array of 1–32 face `GeometryReference` objects forming a closed tool
- `LinearPattern`:
  - `direction1`: `{"reference": GeometryReference(Edge|Axis|Plane), "spacing": double, "count": int (1–32), "reverse": bool}`
  - Optional `direction2`: same structure; count1 * count2 <= 1024
- `CircularPattern`:
  - `axis`: `GeometryReference` (role `"Axis"` or `"Edge"`)
  - `count`: int (1–360)
  - `equal_spacing`: bool
  - `angle`: double (radians, 0 < angle <= 2π)
  - `reverse`: bool
- `Mirror`:
  - `plane`: `GeometryReference` (role `"Plane"` or `"Face"`)

#### Treatments: Fillet and Chamfer
- `kind`: `"Fillet"` or `"Chamfer"`
- `target`: body selection map
- `propagate_tangent`: bool (default true)
- `Fillet`:
  - `method`: `"Constant"`, `"MultipleRadius"`, `"Variable"`, `"Face"`, `"FullRound"`
  - `Constant`: `radius` (> 0), `edges`: 1–32 edge `GeometryReference`s
  - `MultipleRadius`: `radii`: array of 1–32 `{edge: GeometryReference, radius: double}`
  - `Variable`: `edge`: edge `GeometryReference`, `control_points`: array of `{parameter: double, radius: double}`, `transition`: `"Smooth"` | `"Linear"`
  - `Face`: `faces1`: array of face refs, `faces2`: array of face refs, `radius`: double
  - `FullRound`: `side_faces1`: array of face refs, `center_faces`: array of face refs, `side_faces2`: array of face refs
- `Chamfer`:
  - `method`: `"EqualDistance"`, `"DistanceDistance"`, `"AngleDistance"`
  - `edges`: 1–32 edge `GeometryReference`s
  - `EqualDistance`: `distance`: double (> 0)
  - `DistanceDistance`: `distance1`: double, `distance2`: double, `flip`: bool, optional `reference_face`
  - `AngleDistance`: `distance`: double, `angle`: double (0 < angle < π/2), `flip`: bool, optional `reference_face`
