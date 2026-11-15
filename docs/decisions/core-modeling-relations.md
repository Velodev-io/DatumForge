# CM1 planar relation extension

Date: 22 October 2026. Status: implemented slice; verification in
[core-modeling evidence](../testing/core-modeling.md). This does not close CM1.

## Supported operands

| Relation | Added operands | Geometric meaning |
|---|---|---|
| Horizontal / Vertical | Two distinct explicit points | Equal Y / equal X in the sketch frame |
| Coincident | Point then line or circle | Point lies on the infinite supporting line or circle |
| Midpoint | Point then line | Point lies on the line and its perpendicular bisector |
| Collinear | Two lines | Parallel directions and a shared supporting line |
| Concentric | Two circles/arcs, in either combination | Coincident centers |
| Coradial | Two circles/arcs, in either combination | Coincident centers and equal radii |
| Equal | Circle/arc or arc/arc | Equal supporting radii; sweep angles remain independent |

Existing one-line Horizontal/Vertical, point/point Coincident and line/line or
circle/circle Equal retain their existing meaning. The native relation selector
normalizes point/curve ordering because the entity list returns document order,
rather than click order. Unsupported operands produce an operand-specific reason.
The UI only enables a relation for supported operands.

Arc coincidence, midpoint of an arc, external projected topology, point/vertex
Concentric, intersection, symmetry, merge, pierce and the remaining tangent/equal
combinations are not implemented by this slice. Supporting-line coincidence is
not a bounded-segment intersection or a merge operation. No control claims these
additional operations are implemented.

## Solver and ownership

PlaneGCS remains the worker-side planar solver. New adapters use its native
point/line, point/circle, perpendicular-bisector, parallel and center/radius
constraints. Independent normalized residual and numerical freedom/rank checks
are extended alongside the adapter. A solver success flag alone is insufficient.
No upstream solver source is changed. Existing arc-join rules remain intact.

Draft previews use the existing bounded sketch-preview path. Finish Sketch
accepts the complete sketch once; preview and Cancel do not persist a relation.
This slice does not implement PreviewModelEdit/CommitModelEdit, construction
groups, conics or the unified datum/curve dependency graph. Those contracts stay
scheduled in [CM1–CM4](../CORE_MODELING.md).

## Semantic compatibility

Old sketch definitions keep version 1. A sketch requiring one of the new operand
combinations emits version 2. Version 1 containing a new combination is rejected;
both versions use the existing entity/constraint collection layout and stable IDs.
The existing SQLite schema and admission caps remain unchanged.

Accepting version 2 requires the additive project manifest suffix
`;core-modeling-v1;sketch-relations=2`. The requirement remains after undo: retained
history can still contain version 2. Later feature, exchange, assembly or drawing
edits must retain it. Project validation checks active sketches, checkpoints and
retained command intent/undo/redo definitions against the manifest, including a
file whose active sketch has been undone to version 1. Migration-copy paths
refuse to downgrade this manifest.
Earlier readers reject the unrecognized manifest rather than interpreting a
partial sketch.

That earlier-reader refusal follows the existing strict manifest whitelist; a
separate historical-reader executable was not run. The new-reader tests directly
exercise forged lower manifests in active state and retained history.

Opening an old saved project does not alter its saved bytes. As with the existing
project lifecycle, edits are recoverable in the worker-owned workspace. The worker
rejects overwriting an existing non-core manifest with core-modeling content using
`UpgradeCopyRequired`, directing the user to Save As with a new filename. This
also applies after undo because the reader requirement is sticky. An already-core
saved copy can be updated normally. Verification attempts the refused overwrite,
checks unchanged source bytes and accepts a separate saved destination. This is
not a general new schema migration framework; future curve/group schemas will
require their own versioned contracts.

Tests use marked disposable roots and the existing refusal of release recovery
paths. Historical module reports retain their original scope.

The subsequent [rectangle schema-3 extension](core-modeling-rectangles.md) adds
typed construction groups and a further manifest suffix. This relation-slice
schema-2 evidence and its source identities remain historical.
