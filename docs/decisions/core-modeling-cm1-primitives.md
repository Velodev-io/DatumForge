# CM1 remaining primitive and relation contracts

25 October 2026. Authorized continuation of CM1; preserve the rectangle/relation
snapshots and deferred gates. No commit/push or Module 11 work.

Implement in order: semantic contracts, worker adapters, persistence, native input,
then independent geometry and lifecycle evidence. Whole CM1 remains open until
all listed operands and failure gates have passing evidence.

## Slots

Four methods: Straight (arc centers, width cursor), Centerpoint Straight (midpoint,
end center, width cursor), 3 Point Arc (start/end/bend, width cursor), Centerpoint
Arc (center/start/end, width cursor). Reuse the established arc canonicalization
and retain exact circular boundaries; no polyline replacement. Width is twice the
perpendicular/radial cursor distance, and must leave a positive inner radius and
nonintersecting end caps. Straight slots expose Center to Center / Overall Length;
curved slots expose path radius and sweep. Optional automatic driving dimensions
use width and the selected length convention, or width/radius/sweep for curved
slots. A selected convention remains persisted, not inferred from display labels.

Typed Slot groups own boundary, cap centers, construction centerline/path and
intrinsic relation identities. Parameter edits preserve these IDs. Native pointer
motion uses local construction arithmetic; only acceptance is durable. Existing
256 entities, 256 numerical variables (including synthetic arc endpoints), 512
constraints and 256 KiB sketch payload limits remain unchanged.

## Exact curves

Ellipse/elliptical arc: center/focus control points, minor radius and trimmed
parameter range. Parabola: vertex/focus points and trimmed parameters. Splines:
identified control points, degree, knot multiplicities, weights and trimmed range.
Display sampling is transient. OCCT receives exact conic/B-spline definitions.
The worker adapts the pinned PlaneGCS representations; no upstream modifications.

## Relations and dependencies

Keep independent residual/freedom evaluation with every new solver adapter.
Intersection requires a point and two lines; symmetry has two entities and a
centerline, not a two-operand substitute. Merge preserves explicit point identity
semantics. Tangency must cover the documented arc/circle/ellipse/spline operands
and retain contact branches. Fix-location behavior is distinct from freezing all
shape variables. Unsupported or inconsistent combinations report specific reasons.

Pierce references an entity of another sketch (T4 pp. 18), requiring typed source
references, branch resolution and dependency-ordered reevaluation. Record that
prerequisite explicitly; same-sketch point coincidence does not close Pierce.
Sketch support/3D dependency work must not be silently claimed by a planar tool.

## Compatibility and evidence

Sketch schema 4 and an additive `sketch-primitives=1` semantic manifest fence new
Slot/curve/relation definitions. Existing schema 1/2/3 encodings remain unchanged.
Validate active state, feature checkpoints and retained command/undo/redo records;
reader requirements remain sticky and saved-file upgrades require a new copy.

Per-variant evidence includes exact area/volume where available, residuals/rank,
parameter/upstream edits, cancellation, conflict refusal, stable IDs, deduplication,
undo/redo, worker restart, save/reopen/copies and native 100–200% scaling. Planned
fixtures and contracts are not passing evidence.

## Implemented Pierce contract

`CurveReference` uses document scope `Self` (owning Part), sketch/entity UUIDs,
semantic curve kind, a finite parameter interval and its resolved parameter.
Other documents are explicitly refused. Exact point/line/circular/conic/rational
B-spline intersections use the same worker adapter as solid profiles. A unique
root inside the stored interval is required. Missing roots, changed source roles,
missing source identities, coplanar overlap, multiple roots and cycles are
explicit failures. Native branch choices carry interval/parameter values, never
a display-array ordinal. Reevaluate source sketches before dependent sketches;
collection order in persistence stays unchanged. No face-attachment or projected
model-edge implementation is inferred from this sketch dependency subset.

Spline creation is by control points, with degree 1–5, clamped rational knots,
weights and trimmed parameters editable in Properties. Closed input repeats the
first identified control point and has a C0 seam; it is not periodic or fit-point
interpolation. Future knot insertion/fit-spline tools must have their own explicit
contracts. Merge retains both semantic point IDs linked by its persistent relation,
so connected curves and controlling dimensions keep their references. Multi-point
alignment and multi-line collinearity expand into identified pairwise relations
in one undoable draft operation. Parameterized tangent contacts persist orientation;
trimmed-arc contacts outside the selected range fail explicitly.

The complete course operand register also requires projected-edge and external
vertex relations. Those depend on CM4 association/repair adapters. Keep those
rows and whole CM1 acceptance open; never advertise a same-sketch stand-in.
