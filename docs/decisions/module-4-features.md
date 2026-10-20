# Module 4 exact part adapter

Date: 20 October 2026. Authorization: [scope record](implementation-scope.json).
Implementation and verification: [Module 4](../testing/module-4.md). Module 5 is
outside the current authorization; dependency graph optimization and reference
repair are not implemented here.

## Ownership, units and semantic storage

The desktop links the bounded `features::Feature` semantic schema and display
meshes. The one serial worker constructs OCCT shapes, evaluates ordered feature
collections, validates commands, measures and writes project state. UI projections
contain no exact shapes. Accepted features, preview definitions, operation IDs,
base revisions, session epochs and preview generations use the existing bounded
local protocol. Preview/cancel do not write project semantics. Accept recomputes
and commits one durable revision; failure leaves the prior semantic model intact.

A document's existing properties entity owns a version-1 collection in the
`features` table (version 1, ordinal 0). Nested UUIDs are checked across documents,
properties, sketches and features. The schema fingerprint stays unchanged.
`features-v1;OCCT=7.9.2;part-adapter=1` extends the existing sketch manifest. Earlier
metadata/sketch projects still open; older workers refuse feature-bearing files.
Reads do not rewrite original saved files. Features are semantic definitions;
there are no serialized authoritative BReps or per-edit BRep journal copies.
Ordered replay reconstructs results after undo, reopen and worker recovery.
Body IDs derive from the creating feature UUID and its local copy number;
unrelated preceding bodies cannot rename copied results.

Lengths persist in meters; angles in radians. The exact adapter converts once to
millimeters. Display inputs use document units and explicitly labelled degrees.
`MaterialDensityKgM3` is a reserved positive scalar document parameter interpreted
as kg/m³, capped at 1,000,000. Its assignment is a normal durable/undoable parameter
command. Zero query density means no supplied density; mass is reported only with
a valid density. No density is inferred from a material name or display mesh.

## Supported operations and admission

- Extrusion: closed nonconstruction lines/arcs/circles, connected loops, disjoint
  profiles and one nesting level of holes; Forward/Reverse/Symmetric finite depth;
  NewBody/Add/Cut. Open, branched, touching, self-intersecting and invalid profiles
  are rejected. Construction geometry does not contribute boundary curves.
- Revolve: the same profiles, a unit in-plane axis and in-plane origin, positive
  angle up to 2π, Forward/Reverse, NewBody/Add/Cut. Invalid self-intersection is
  rejected. Up-to-surface, thin and draft variants are absent from the controls.
- Fillet/chamfer: 1–32 explicit edges of the current target result, positive
  constant radius / equal distance. No variable radius, face fillet or asymmetric
  chamfer controls. Kernel failure remains an editable error.
- Hole: cylindrical diameter, center strictly inside a planar face, normal unit
  direction, blind finite depth or through-all target. A blind hole follows the
  supplied inward direction. Through-all spans the target bounds in both axis
  directions. Edge centers and nonplanar support are rejected. Threads, standards,
  countersinks and counterbores remain absent.
- Linear/circular pattern: earlier feature/body source, unit direction or axis,
  count 1–32 including the existing seed, finite positive spacing or full-circle
  distribution. NewBody creates exact solid copies; Add fuses with a target; Cut
  repeats a volumetric source tool. Cut-pattern tool provenance consists of the
  generated copies; the seed is already present in the model. Count 1 is an
  identity feature and retains its source result/tool. Multiple turns/duplicate
  seed angles are rejected. A pattern is not a display-only instance.
- Mirror: an earlier exact source/body, plane origin/normal, NewBody copy or
  Add/Cut intent. Reflection is an exact copied transform, subject to the same
  validity and Boolean checks.

All parameter maps are typed and reject unknown/missing fields and unsupported
variants. Initial admission: 100 features / 128 KiB per document, ten solids per
part, 20,000 faces per result, 500,000 displayed triangles, existing control and
32 MiB segment caps. These are conservative limits, not B2 capacity/performance
acceptance. Patterns are bounded before publication. A target Boolean must remain
one solid and change volume in the intended direction; disconnected additions,
missed cuts, total removal and split-body cuts are rejected explicitly.

Every shape is checked for kernel validity, positive oriented volume, closed
shells, finite results and vertex/edge/face tolerance ≤0.0001 mm. Profile joining
uses 0.0000001 mm. Tolerances are never silently inflated. These bounds are not a
claim that all geometries attain the tighter SRS analytic-fixture error target;
those fixtures are compared independently.

## Provenance and reference boundary

Adapters emit actual OCCT Generated/Modified/IsDeleted maps for supported input
edges/faces, including Boolean inputs and copied transforms. Source sketch,
feature, target and body identities remain explicit in semantic definitions.
Topology tokens hash bounded geometric signatures, independent of traversal
indices and orientation. Duplicate signatures are removed from the selectable
map and never arbitrarily resolved. Display curve segments have separate segment
suffixes that resolve to their underlying exact edge token.

A fillet/chamfer references an explicit earlier feature result and its edge token;
that source must still be the current target stage when replay reaches the
operation. Unchanged supported edge signatures can survive an upstream edit.
Missing/ambiguous signatures fail with ReferenceMissing and retain the accepted
model. There is no general split/merge matching, automatic repair, face-index
persistence, suppression/delete/reorder UI or affected-descendant cache in this
module. Those are Module 5 tasks. Module 4 parameter/sketch edits conservatively
replay the full admitted ordered history, atomically rejecting invalid descendants.

## Measurement and UI lifecycle

Exact queries require the current explicit project result revision; stale bases
are rejected. Supported queries include body area/volume/optimal bounds, edge
length, circular-edge/cylindrical-face radius, planar-face area, two-straight-edge
angle and optional density-based mass. Results carry SI units and validity;
unsupported selections fail instead of returning zero. Mesh deflection and display
mode never define an engineering measurement.

The existing Features/Sketch/Evaluate tabs and left manager are retained. Feature
commands open a contextual left PropertyManager with labelled profile, target,
source, extent, axis and parameter controls. Preview/Accept/Cancel and diagnostics
remain above its scroll area. Changed inputs invalidate acceptance. A pending
commit cannot be canceled as if it had no durable effect. Double-click a feature
tree node to edit its typed parameters; accepted sketch edits rebuild descendants.
Draft commands disable unrelated project/document actions. Escape discards a
cancelable feature draft and restores the accepted display. Per-feature worker
compute times are diagnostic samples, not SRS p95 acceptance.

## Remaining gates

Physical mouse/trackpad ergonomics, native assistive technology, alternate
appearance, Windows/Linux/X11/Wayland, Intel/minimum macOS, installed packages,
controlled network-disabled workflows and full 8 GB B0–B5 resource budgets remain
Blocked. A development build on the current physical 8 GiB M2 does not close
those gates. Preserve the historical 4.0 and Module 3 evidence independently.
