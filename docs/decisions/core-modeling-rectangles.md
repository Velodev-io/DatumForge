# CM1 rectangle constructions

Version 0.1, implemented 24 October 2026. Contract for the rectangle production slice; status and
actual verification are maintained in [core-modeling evidence](../testing/core-modeling.md).
Whole CM1 remains open. No commit/push or Module 11 work.

## Creation and design intent

T2 page 34 names Corner, Center, 3 Point Corner, 3 Point Center and Parallelogram.
The restored PDF hash matches the original review. The [official rectangle help](https://help.solidworks.com/2026/English/SolidWorks/Sldworks/c_Rectangles.htm)
and [center rectangle sequence](https://help.solidworks.com/2023/english/SolidWorks/sldworks/t_Sketching_3_Point_Center_Rectangles.htm)
resolve the supplementary pointer sequence. The [official education guide](https://www.solidworks.com/sites/default/files/2023-04/Fundamentals3DDesign_SIM_ENG_SV.pdf)
identifies the three center inputs as center, edge midpoint and corner.

| Method | Inputs | Shape intent |
|---|---|---|
| Corner | Opposite corners | Axis-aligned four sides |
| Center | Center, corner | Axis-aligned, symmetric about retained center |
| 3 Point Corner | First corner, second corner, third-side cursor | First edge gives rotation; project the last input onto its normal |
| 3 Point Center | Center, edge midpoint, corner cursor | Center-to-midpoint gives the half-span; project the last input along the edge |
| Parallelogram | Three consecutive corners | Preserve opposite parallel sides, permit non-right angles |

Reject zero spans, collinearity and oversized generated collections before adding
members. Preserve existing 256 entity / 512 constraint caps. Construction/profile
flags are explicit. Center methods retain a construction center and diagonals;
only the four perimeter sides form the profile. Shape relations remain worker
constraints, not a preview-only assertion. Pointer previews use draft arithmetic;
the worker solves and validates accepted geometry.

## Semantic collections

Persist a typed Rectangle group to sketch version 3: stable group ID, creation method,
ordered four vertex/edge IDs, optional center/diagonals and owned shape-relation
IDs. Group identity never depends on display indices or geometric recognition.
Dimension relations use the existing expression/dimension model; no duplicate
numeric width cache may disagree with the accepted geometry.

Version 1/2 payloads keep their existing encoding. Version 3 requires a separate
`sketch-groups=1` manifest suffix. Active sketches, feature checkpoints and retained
intent/undo/redo definitions must all match the reader requirement. The requirement
is sticky after undo and after feature/exchange/assembly/drawing edits. Upgrading
an existing saved file requires a separate destination; a version-3 project must
not overwrite a file declaring only the earlier relation manifest.

Group validation checks role types, connectivity, unique ownership and exact
owned-relation operands. A malformed, missing or conflicting role is diagnosed;
never reconstruct group identity by proximity. Legacy ungrouped rectangles remain
readable and keep existing editing behavior.

## Editing and native workflow

Expose the five methods in the Rectangle PropertyManager. Two-input methods allow
click-click and click-drag. Three-input methods allow successive clicks and the
corresponding staged drags. Show a local preview at each meaningful stage. Escape
cancels incomplete input. Completion adds one draft undo entry; only Finish Sketch
writes a durable edit. Incomplete or invalid input cannot commit a partial group.

Properties edit anchor/center, side lengths, rotation for angled methods, and side
angle for a parallelogram. Rotation and side-angle fields edit the current pose;
they do not create persistent driving angular dimensions (CM3 remains open).
Preserve all member IDs. Numeric group size edits add
or update explicit Length dimensions; expressions retain their existing semantics.
Dragging without controlling dimensions preserves group shape; constrained-drag
completion is CM3. Current constrained drags are refused with a specific
`ConstrainedGeometry` reason. Property resizing supports Length drivers,
unchanged Fixed anchors and compatible Horizontal/Vertical relations; other
external controls are refused before mutation.
Do not silently remove group-owned shape relations or external controlling
dimensions. Deleting a group with external relations requires explicit prior
constraint removal/repair; deleting an unconstrained group removes its owned
members and relations together.

## Verification scope

Analytical rotated/negative-coordinate fixtures; center and parallel/perpendicular
residuals; dimension edits; stable identities; bounded admission; malformed groups;
all three schema versions and forged lower manifests, including retained history;
undo/redo; worker restart; save/reopen/portable copies; transactional refusal;
native staged input, preview/cancel and effective DPR 1–2. Existing arc, Part,
assembly and drawing checks continue. Unavailable platforms and full resource
acceptance remain Blocked, not inferred from development tests.

Available-Mac verification on 24 October passes 59 enabled CTests, Clang and
native workflows at effective DPR 1/1.25/1.5/1.75/2. The
[scoped evidence](../testing/core-modeling.md) retains earlier failed runs and
per-variant/whole-milestone limits. A stationary selection is an exact no-op at
the shared edit boundary, preserving geometry and avoiding an unnecessary solve;
missing selection identities still fail explicitly.
