# Module 3 — sketch and expression adapter decisions

Date: 20 October 2026. Authorized scope: Module 3, stopping before Module 4.
[Evidence](../testing/module-3.md), [scope record](implementation-scope.json).

## Semantic ownership and persistence

Only the existing serial worker solves and commits sketches. The desktop holds
bounded typed projections and draws 2D paths with Qt's native painter; it owns no
PlaneGCS system, exact kernel object or database connection. The original mesh
viewport remains available for the reference scene. No browser or service is added.

A document's `sketch_entities` row is owned by its existing properties entity and
contains a versioned collection of semantic sketches. Each sketch, point, curve and
constraint has a canonical UUID, checked for uniqueness across the whole project.
Shared point references connect polyline/rectangle edges. SQL does not index nested
sketch entities individually in this adapter. This avoids changing schema v1 or
rewriting existing Module 2 projects; future indexed dependency/materialization
work must preserve these IDs and introduce an explicit compatible migration.

The first sketch edit changes the semantic manifest to `sketch-v1`, adapter 1 and
the pinned PlaneGCS revision. The new worker reads metadata-only Module 2 files;
the old metadata-only worker refuses the new manifest. Existing saved files remain
unchanged until an explicit save. Undo/redo stores bounded accepted semantic
collections; no raw solver memory or BRep copies are journaled. Save As retains
nested UUIDs inside a new project identity. Empty collections do not require a row.

Initial admission: 16 sketches per Part document, 256 entities / 512 constraints /
64 names per sketch, at most 256 numerical variables in one solve, 256 KiB of
sketch semantics across the project. The document's encoded collection also cannot
exceed 256 KiB. The full reply plus echoed request is checked against the control
frame cap before commit. These are conservative implementation limits, not B2/B3
capacity or resource-budget acceptance. History retains the existing disk/entry
caps. Assembly and Drawing sketch editing remains unavailable.

## Coordinates, primitives and selection

Lengths persist in meters, angles in radians. Validated orthonormal support axes
and an SI origin define XY/XZ/YZ or programmatically supplied planar frames. Points,
connected lines/polylines, axis-aligned rectangles, center/radius circles and
center/radius/start/positive-sweep arcs are supported, including construction flags.
Arcs exclude a zero/full-turn sweep. Coordinates/radii are bounded to 1 km;
line length/radius must exceed 1 nm. Display unit changes do not rescale geometry.

The native property form and planar viewport edit the same draft. The viewport
picks points/segments/circles/arcs, highlights selection, snaps pointer input to existing points
and a 1 mm grid, and provides pan/zoom. Typed coordinate input bypasses the display
snap grid and retains its parsed SI value. The current snap tolerance is 0.15 mm in
model coordinates; a screen-distance snapping preference remains a UX follow-up.
Polyline input shares endpoints and is finished by changing tools or accepting the
edit. Line/polyline/rectangle pointer input shows a local rubber-band preview after
the first vertex. Circle input presses at the snapped center, sizes a transient
radius while held, and releases one circle into the sketch draft. No point, curve,
solver request or durable operation is created per mouse movement; a zero-radius
release adds nothing. The explicit numeric button still creates a circle using
X/Y and Radius in display units without pointer grid rounding. Arc centers remain
click/coordinate inputs with numeric radius/start/sweep; a three-point arc gesture
is not implemented. Tool changes discard incomplete input while retaining
completed draft geometry. Radius fields appear only for circle/arc and angle fields
only for arc; the empty viewport identifies that no sketch exists. Rectangle construction creates shared corner points; horizontal/vertical and
dimensional constraints are explicit user choices, not hidden inferred constraints.

## Supported constraint combinations

| Constraint | Supported entities and meaning |
|---|---|
| Fixed | One point; stored X/Y anchors, preserved during dragging |
| Coincident | Two distinct point IDs; both coordinates equal |
| Horizontal / Vertical | One line; equal endpoint Y / X |
| Parallel / Perpendicular | Two distinct lines; normalized cross / dot residual |
| Tangent | Line followed by circle; infinite supporting line tangent to circumference |
| Equal | Two lines (length) or two circles (radius) |
| Length | One line; positive endpoint distance |
| Distance | Two distinct points; positive Euclidean distance |
| Angle | Two lines; directed angle from first to second, within ±360 degrees |
| Radius | One circle or arc; positive radius |

Unsupported combinations are disabled with a reason in the selection form and
rejected independently by the worker. Fixed lines/circles use fixed endpoints or
centers plus dimensions. Arc endpoint/coincidence/tangency and general curve
constraints are unavailable; free arc start/sweep variables remain visible in the
freedom count and cannot produce a false fully-defined result.

All variables and constants outlive the local PlaneGCS system. Coordinates are
translated and normalized by the current sketch extent/radius (minimum scale
1 micrometer). The pinned upstream solver is unchanged. DogLeg iteration counts
are bounded at 100, without a sketch-size multiplier. Independent analytic
residuals must be finite and at most 1e-7 in normalized length/angular quantities.
Post-solve geometry is checked again for radius/line degeneracy and finite bounds.
Freedom is derived from the numerical Jacobian rank (pivot threshold 1e-7), not
constraint count. PlaneGCS redundant/partially-redundant/conflict tags map back to
persistent constraint IDs. Conflicting or failed candidates cannot be committed.
Stored accepted geometry/expressions are revalidated on read; no silent repair is
performed. Standard fixture analytic checks have tighter independent expectations;
this adapter policy does not close the full NFR-008 acceptance gate.

Dragging starts from the accepted coordinates and uses the edited seed. When
constraints have not changed, a line-direction branch flip is rejected with an
explicit smaller-step/edit-constraints message. Cancellation restores the entire
accepted collection. Tangent side continuity is checked separately. No claim is
made that this bounded adapter implements arbitrary nonlinear branch navigation.

## Expressions and editing transactions

The parsed arithmetic language supports literals with units (`mm`, `cm`, `m`,
`in`, `deg`, `rad`, `kg`, `g`), names, `pi`, `+ - * / ^`, parentheses, `abs`, `sqrt`,
`sin/cos/tan`, `asin/acos/atan`, and two-argument `min/max`. Multiplication is
explicit except a numeric literal followed by a unit. Powers require an integer
scalar exponent within ±8. Dimensions track length, angle and mass independently.
Names are sketch-local; document metadata parameters are not implicit dependencies.
Limits remain 4 KiB text, 256 AST nodes and 32 parse/name nesting levels. Cycles,
unknown/reserved names, mismatched dimensions, nonfinite/domain failures and
nonpositive length/radius dimensions are rejected without general code evaluation.

Preview evaluates disposable candidates in the worker. Accept recomputes against
the current project revision and durably commits once. Invalid drafts remain
editable with textual status/constraint identification. Cancel invalidates the
preview generation, so a late reply cannot restore a canceled draft. Unrelated
project operations are disabled while editing; application close explicitly asks
to discard a draft. A pending commit cannot be described as canceled. Enter accepts
only a solved preview with viewport focus; Enter in a value editor does not commit.
Escape first cancels pending circle/vertex input and retains completed draft
geometry; when no input is pending it cancels the editable sketch draft. The
explicit Cancel sketch edit button always discards the whole edit. Every accepted dimension edit, constraint deletion,
undo and redo is one durable semantic operation.

## Remaining decisions and gates

Windows/Linux, Intel/minimum macOS, installed builds and native assistive-technology
checks require their real environments. The available M2 has physical 8 GiB RAM;
this is not a full 8 GB B0–B5 performance result. The application workflow uses no
network service, but an OS-network-disabled run is separately Blocked until a
controlled procedure is available. Full snapping preferences, advanced arc
constraints, reference-plane attachment to future solids, indexed feature history,
and broad resource/performance acceptance remain explicit later work. Module 4
requires another user instruction.
