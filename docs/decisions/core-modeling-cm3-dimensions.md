# CM3 — driving/reference and component dimension slice

This records the first, schema-6 slice. The subsequent
[associated-dimension contracts](core-modeling-cm3-associated-dimensions.md)
extend it with centerline diameter, ordinates and true circular-arc length.

This is an implementation slice of CM3.a–e, authorized within CM0–CM9. Whole CM3
remains In progress. The source requirements are T4 pages 20–24 and 32–33 and the
page-indexed CM-076–097 register. This slice does not implement centerline diameter,
ordinate groups, path/true arc length, automatic relations, Fully Define or
constraint-preserving dragging. It does not start CM4 or Module 11.

## Semantic contract

Existing Length, Distance, Angle, Radius, Diameter, Sweep and SlotLength retain
their meanings. HorizontalDistance and VerticalDistance accept either an oriented
line or two ordered distinct points. They measure `second - first` along sketch X
or Y, in SI meters. Signed values including zero are valid within ±1 km; aligned
distances retain the existing positive-length requirements. A negative value
reverses the component direction; this is not a full reverse-dimension command.

Dimensions default to driving. Reference dimensions have no driving expression
and never enter PlaneGCS equations, independent residuals or the constraint rank.
After solving all driving relations, the worker regenerates each reference value
from the resulting geometry. Accepted-project validation checks those regenerated
values too. A corrupt cached readout is not accepted as an authoritative target.
Branch continuity compares driving definitions only: adding a readout or changing
its value cannot add a restriction or disable an existing driving-geometry guard.
Reference values cannot block ordinary geometry edits or be rewritten as driving
expressions by the direct Properties editor.

Optional label placements are finite sketch-plane X/Y coordinates in meters,
bounded to ±1 km. They do not enter solver variables or alter geometry. Rendering
clamps labels to the visible canvas and avoids collisions; the stored placement
does not depend on the current zoom or display scaling. Reference labels use
parentheses and the relation list names their Reference mode.

## Compatibility and ownership

Sketch schema 6 carries an explicit Boolean `driving` field on each dimensional
constraint and optional two-number `placement`. It requires the existing groups
and edit-provenance arrays, which may be empty. Sketches without new semantics
retain their existing version. Downgrading a component dimension or removing the
schema-6 metadata contract is refused. Non-dimensional relations cannot carry a
reference mode or placement. Existing entity/constraint/variable caps are unchanged.

The sticky semantic manifest appends `sketch-dimensions=1` after `sketch-edits=1`.
All live state, command/history deltas and checkpoints must fit the manifest,
including after undo. New semantics in an opened older file require Save As to a
different filename; the original remains usable by the older reader. Current
readers continue to read versions 1–5. There is no SQL-table schema migration.

The desktop owns draft definitions, selection and display labels. The existing
PreviewSketches worker command owns solving and regeneration. SetSketches remains
the durable, deduplicated, atomic publication boundary. The UI links no solver,
kernel or SQL implementation.

## Editor and failure behavior

Dimension opens a native editor over the sketch for a line, two points, two lines,
circle or arc. Available modes follow the actual operands. A label double-click
edits that stable dimension identity. Values accept explicit units and named
expressions; bare lengths use document units and bare angles use degrees. Label
coordinates use document units. Switching to Reference disables the expression.

Preview solves a separate candidate without mutating the draft, revision or undo
stack. Apply is enabled only after a valid exact response. Editing an input,
Restore, Cancel or closing the editor invalidates preview generation and disables
Apply. Late responses cannot install a candidate. Apply adds one draft undo step;
Finish publishes through SetSketches. Failed unit/geometry/constraint checks leave
the prior draft and durable state available. This slice does not change the
existing constrained-drag rejection or claim last-valid constrained-drag support.

The workflow regression also checks two-entity selection followed by circles/arcs.
Selection cleanup filters the list without mutating it during iteration, and a
new document revision clears prior selection identities. List selection updates
the shared Dimension action, including two-point selection and clearing selection.

## Verification scope

`cm3.dimensions` exercises signed components at three SI scales, line/point
operands, independent measurements, rank, reference regeneration, invalid units
and metadata, version refusal, exact stored-value validation, duplicate commands,
failed commits, old-file preservation, sticky history, undo/redo and saved reopen.
`cm3.dimension_contracts` uses the real worker with disposable marked roots for
mm/in editor modes, invalid units, canceled in-flight requests, restore/apply,
reference conversion and canvas label double-click. Its separate native entry
checks activation and effective scale; offscreen passing is not native evidence.

See the [execution record](../testing/core-modeling.md) for actual run outcomes.
Full variant, interrupted-commit/recovery/portable-copy matrix, Windows/Linux,
installed builds, physical input/accessibility and physical 8 GB acceptance remain
open. No whole CM3 task or coverage row is closed by this slice.
