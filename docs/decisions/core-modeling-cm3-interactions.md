# CM3 dimensions and interaction completion

Date: 1 November 2026. Approved scope: [completion plan](core-modeling-cm3-completion-plan.md).
Implementation and verification are tracked separately in the
[execution record](../testing/core-modeling-cm3-completion.md). This contract
does not certify installed builds, physical input, assistive technology,
Windows/Linux or the full resource/whole-course acceptance matrix.

## Publication and ownership

The UI owns semantic drafts and display geometry. The existing worker owns
PlaneGCS, exact curve contacts, length integration and project writes. All new
interactions use `PreviewSketches`; only explicit draft confirmation followed by
`SetSketches` publishes a durable sketch. Preview does not advance the document
revision. Temporary drag equations and unconfirmed proposals are never stored.

An editor is tied to its source document/context, worker epoch and accepted
revision. Changing any of those invalidates the editor. A restarted worker
discards old draft undo state, gestures and proposal dialogs even when recovery
restores the same accepted revision. Client generations fence superseded preview
responses; the existing single running request/latest pending preview queue is
retained.

## Dimension controls and ordinate series

The existing label double-click editor retains the selected constraint identity,
including when driving and reference dimensions share operands. It supports SI
stored increments displayed in document units, explicit numeric steps, and a
drawing-import mark. Changing the mark/increment preserves a named expression;
an explicit step evaluates that expression and creates a numeric value.
The mark is metadata for future drawing import, not a drawing-import feature.

Component reversal negates the expression while preserving names. Aligned
Length/Distance reversal changes geometric sense while retaining positive
magnitude. Centerline reversal changes the chosen axis side. Apply is disabled
when exact preview cannot establish the requested reversal. Reference dimensions
remain measurements and cannot reverse or carry driving expressions.

The Ordinates action accepts multiple target points, a selected point datum or
the sketch origin, X/Y orientation and driving/reference mode. Origin use creates
or reuses a construction datum with an origin anchor. Each dimension has its own
stable identity and placement. Cancel creates no datum, relation or draft edit.

## Exact path dimensions

`PathLength` stores `members`, a matching Boolean `directions` vector and an
optional `closed` association. Membership is an ordered, connected chain of
1–64 distinct current curve entities. The first member equals operand `a`;
operands `b` and `third` are empty. Missing/disconnected/duplicate members,
branching or revisited intermediate junctions and broken closure are rejected.
The UI orders selected identities; the worker validates the stored association.

Lines and circular curves have analytic lengths. Exact parametric derivatives
are integrated for ellipses, elliptical arcs, parabolas and rational splines,
with spline knot splits, adaptive Gauss quadrature and explicit work/convergence
limits. Renderer samples never determine a path value. Tests compare curve
lengths independently with OCCT. Driving equations preserve both total length
and member joins, including final closure. Reopening a label preserves the
original member identities and orientation. Deleting any standalone member drops
the dependent path dimension. Referenced construction groups require explicit
relation repair before deletion; straight-line trim previews disclose a path
dimension affected through any member before the existing removal confirmation.

## Worker interaction requests

`PreviewSketches` accepts the existing `sketches` array and an optional typed
`interaction` map. Existing preview/diagnostic response shapes are unchanged.

| Kind | Additional fields | Behavior |
| --- | --- | --- |
| `Drag` | `sketch`, `entity`, `from: [x,y]`, `to: [x,y]` | Soft worker targets with permanent driving equations preserved |
| `Infer` | `sketch`, `selection: [id...]`, `tolerance` | Deterministic independent relation proposals; empty selection means all |
| `FullyDefine` | `sketch`, `scope: All/Selected`, `selection: [id...]` | Current-shape dimension proposals and remaining-freedom diagnostics |

Coordinates and lengths are SI. Requests have strict keys, identities and
bounded numeric/selection admission. Unknown kinds and invalid inputs fail
without publishing. Existing limits remain: 16 sketches/document, 256 entities,
256 numerical variables including auxiliary arc endpoints, and 512 constraints.
Driving paths additionally admit at most 1024 expanded length/join equations
per sketch, checked before allocating solver objects. A path contributes one
length equation plus two for each nontrivial join; shared line endpoints and
intrinsically closed circles/ellipses do not add duplicate join equations.
This guards solver-object amplification without claiming a full RAM benchmark.

### Dragging

Select/drag sends temporary targets to PlaneGCS's auxiliary subsystem. Permanent
constraints remain hard equations. Points, line/control geometry, circles/arcs,
conics/splines and rectangle/slot groups are supported. Circle/arc perimeter
targets permit radius changes when radius is free and centre motion when radius
is driven. Every result passes residual and branch checks. A failed target
retains the last valid display/draft. Release waits for the latest request and
records one draft undo step; Escape restores the pregesture draft. An unchanged
physical pointer does not generate another target when properties move the
canvas beneath it.

### Inference

Automatic relations defaults off. Enabling it requests proposals after creating
or moving geometry, shows blue dashed unconfirmed guides and provides an explicit
review. Disabling it or pressing Escape dismisses proposals. Review lists checked
relations, exact preview and Cancel/Apply. Unchecking a proposal invalidates Apply
until the checked subset is revalidated from the original draft.

The seven course families are Horizontal, Vertical, Coincident, Midpoint,
Intersection, Tangent and Perpendicular. Exact point projections and curve-pair
extrema extend contact inference to current conics and rational splines.
Trim bounds, derivative direction and competing equal contacts are checked;
ambiguous/parallel contacts are not arbitrarily chosen. Newly added near
line/curve tangencies receive a bounded local numerical warm start in ordinary
preview/commit as well as inference. It uses the stored contact, does not search for another branch, and limits the
initial endpoint adjustment to 1% of line extent. Exact validation is unchanged.

Proposal IDs derive deterministically from sketch identity and operands.
Candidates are kept only when they solve, reduce freedom and remain within the
recognition tolerance of the source geometry. Recognition tolerance is positive
and at most 10 mm. Work is bounded by five seconds, 512 proposal attempts and
512 exact contact queries; an exceeded budget is an explicit failure asking for
a smaller selection. These are admission guards, not measured latency claims.

### Fully Define

All/Selected scope produces deterministic, independent current-value dimensions,
retaining existing constraint identities, expressions, placements and shape.
Selected expands the selected entities' control points and scalars; it does not
freeze unrelated geometry. One identified construction origin is anchored;
geometry is not replaced by blanket Fixed constraints. A proposal that adds no
independent dimension does not add an unused anchor.

Coordinates relative to the origin define point/line and spline controls.
Radius, minor radius, start angle and sweep define circular/elliptical curves.
Parabola trim start/span are length-valued curve parameters. Knots, rational
weights and spline trim metadata remain existing model parameters, outside the
solver's movable-variable set. Current rectangle/slot relations are preserved.
The dialog shows remaining freedom; reaching zero is measured by the worker,
not inferred from the number of listed dimensions. Checked subsets are solved
again. The origin anchor required by generated dimensions is retained.

The new editable dimension kinds are `MinorRadius`, `StartAngle`, `TrimStart`
and `TrimSpan`; elliptical-arc `Sweep` is also supported. StartAngle and Sweep
are angular. Parabola trims are lengths along its local parameter direction,
not arc lengths. Existing unit/expression/reference/placement rules apply.

## Persistence and compatibility

Schema 8 stores path associations, increment/drawing metadata, reversed aligned
dimensions and the new scalar dimension semantics. The sticky project manifest
adds `sketch-interactions=1`. Older schemas are emitted for sketches without new
semantics; lower schema claims with new semantics are refused. Save, portable
copy and retained history use the existing manifest admission/migration paths.

Public-store tests cover save/reopen, undo/redo, portable copies without their
source file, duplicate command replay, before/after-commit interruption and
recovery reopen. Worker/UI tests cover uncommitted preview restart and editor
invalidation. Physical process/storage crash matrices retain separate Module 10
evidence; a caught test interruption is not labeled a power-loss experiment.

Preserve deferred 6.1b/6.4c/7.4c. Module 11 is not started. No merge or push.
