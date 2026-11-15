# CM3 — centerline diameter, ordinates and true arc length

This extends the [schema-6 dimension slice](core-modeling-cm3-dimensions.md).
CM3.a–e remain In progress; the remaining path-length/inference/Fully Define/drag
work is not closed. Source coverage is CM-082/083/084/093 (T4 pages 20–24/32–33
and T5 pages 12–15). Existing 6.1b/6.4c/7.4c gates remain Blocked; Module 11 is
not started. Native C++/Qt/PlaneGCS/OCCT ownership and admission caps are unchanged.

## Definitions

`CenterlineDiameter` stores the profile point or line in `a`, a construction line
axis in `b`, and the positive full diameter in SI meters. The axis has its stable
endpoint identities and orientation. Required `reversed` selects the right side
of that oriented axis; false selects the left. The worker enforces signed
perpendicular distance equal to half the diameter. A line profile contributes
one equation for each endpoint, also making that line parallel to its axis.
The signed adapter has analytic derivatives, including aliased parameters, and
does not use unsigned distance followed by branch rejection. It follows an axis
translated past the old profile seed. Independent model residuals/rank still
validate the result.

Point and line diameter readouts measure twice the absolute perpendicular
distance and never drive geometry. A reference line must be parallel to its axis;
otherwise its varying distance is explicitly `ReferenceAmbiguous`. Missing,
degenerate or non-construction axes are rejected. No display index or nearest
line substitutes for an axis. The editor accepts axis-first or profile-first
selection. Parallel line/axis pairs default to diameter; nonparallel pairs retain
Angle as the default. Explicit driving diameter can align a nonparallel profile.
Converting a readout back to driving confirms its current geometric side, even
if upstream axis edits crossed the readout's former side.
Labels use the diameter symbol. The fixed-positive diameter limit remains >1 nm
and ≤1 km, with side stored separately.

`HorizontalOrdinate` and `VerticalOrdinate` store the datum-origin point in `a`
and target point in `b`, measuring target minus origin along sketch X/Y. Signed
values and zero are allowed within ±1 km. Multiple per-point dimensions can share
the same stable datum identity; moving it regenerates their targets/readouts.
The native editor identifies the first selected point as the origin, and labels
show X/Y. Grouped ordinate layout/leader editing is not implemented by this slice.

`ArcLength` applies only to a true circular arc. Its exact equation is
`radius * positive sweep_radians = length_meters`. Free arcs use their sweep;
joined arcs use the adapter's end-angle minus start-angle representation. The
worker owns the product constraint and its analytic derivatives. Independent
residuals and rank use the regenerated model's radius/sweep. Editing a driving
radius dimension therefore changes sweep when length drives;
readout mode only measures the result.
No chord, polyline, ellipse or spline length approximation is used. Existing
positive-radius, trim and numerical-variable limits remain in force.

## Persistence and interaction

Sketch schema 7 is required for these four kinds and requires the centerline
side Boolean. Older sketches keep their existing schema. Versions 1–6 remain
readable. Downgrade attempts are refused even if new fields are removed. The
sticky manifest appends `sketch-associated-dimensions=1`; live definitions,
checkpoints and retained command/undo history must all fit it. A schema-6 source
file needs Save As to another filename after adding these semantics. Undo cannot
lower the reader requirement while new redo history remains. Portable copies
preserve definitions and associations. No SQL-table migration is necessary.

The existing native dimension editor, exact worker preview, Apply/Restore/Cancel,
draft undo and durable SetSketches publication are reused. Combo-box display names
are separate from typed dimension kinds. Reference mode cannot carry expressions.
The UI still owns drafts/display geometry; it links no exact kernel/solver/SQL.

## Verification boundaries

`cm3.associated_dimensions` checks point/line diameters on both sides of translated
and rotated axes at three SI scales; free-axis derivative behavior; upstream axes
moving across the old seed; independent residual/rank and reference values;
signed/shared-datum ordinate changes; free/joined arc lengths under radius edits;
and the real OCCT 100 mm/135 mm revolve volume with a 135 mm profile height.
Persistence checks cover schema-6 preservation, schema-7 refusal, duplicate/failed
commands, sticky undo-only history, saved reopen, undo/redo and portable copies.

`cm3.dimension_contracts` adds line/axis and point/axis selection in both orders,
ordinates and ArcLength to the real-worker mm/in editor corpus. It checks the
shared action, invalid units, cancel-in-flight, Restore/Apply, stable IDs, native
label reopening, readout conversion, and diameter save/worker restart.
The separate native entry checks window activation and five effective scales.
`tools/verify_cm3_associated.py` records a fresh build, all enabled CTests,
formatting, Clang analysis of changed production/all CM3 test translation units,
ownership/research/coverage/specification checks, native CM3 at five scales and
native workspace/arc regressions at the endpoint scales. It retains an activation
failure before one explicitly classified activation-only retry; geometry failures
are never automatically retried or relabeled.

Actual results belong in the [execution record](../testing/core-modeling.md).
This is not full course/operand/interrupted-commit/resource/platform acceptance.
Driving multientity path length, grouped ordinate placement, other dimension
controls, inference, Fully Define and constrained dragging remain open.
