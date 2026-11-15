# Module 7 rigid assembly contracts

The user authorized Module 7 on 21 October 2026 and deferred the remaining Module
6 parser/resource gates. Stop before Module 8. This extends the existing native
C++/Qt desktop and sole geometry/database worker; no web or cloud dependency.

## Definitions, placements and durable semantics

A Part or Assembly document is a shared project-local definition. Each occurrence
owns a stable UUID, source revision, name, SI translation, canonical normalized
quaternion, grounding, hidden, suppressed and unloaded flags. Occurrence UUIDs
remain stable when explicitly replacing/relinking the definition. Nested reference
paths retain root occurrence, child occurrence IDs, body ID and exact topology
token. Unknown definitions are represented as unresolved; no automatic relinking.
Cycles are rejected even through suppressed occurrences. Admission is 100 roots,
100 unsuppressed flattened leaves, eight nesting levels, 150 mates and 256 KiB of
semantic assembly CBOR. These are application admission caps, not peak RAM proofs.

The existing SQLite schema is retained. One aggregate `occurrences` row, keyed by
the Assembly's DocumentProperties entity, stores its versioned semantic collection
of occurrences and mates in `transform`; `definition_id` identifies that Assembly.
The reserved normalized `mates` table remains unused. `set_assembly` forward and
inverse deltas use the normal durable/idempotent command ledger, revision fence,
undo/redo and recovery. This explicit aggregate policy avoids pretending that the
reserved v1 rows already implement a normalized assembly schema. Empty collections
remove the aggregate row, allowing creation undo. `assembly-v1` extends the existing
compatibility manifest; older readers must refuse the new semantics.

## Loading and display

Definitions generate one coarse display mesh per definition revision. Occurrences
supply rigid transforms and triangle/edge ranges; UI selection, fitting and drawing
use those ranges. Binary mesh v2 adds quaternion and range metadata; v1 remains
accepted for prior part/reference displays. The desktop links only semantic models
and display data; exact shapes and SQLite remain worker owned.

Hidden occurrences are not drawn but remain in evaluation/BOM. Suppressed
occurrences and their mates are excluded from evaluation/BOM. Unloaded occurrences
use available coarse meshes without resolving exact geometry; a cold unloaded
occurrence has a state row and no display until explicitly loaded. Unresolved and
stale have separate status and flag fields. Cached coarse geometry is disposable,
process local, and limited to 32 definitions / 64 MiB of counted numeric arrays.
Exact definition ownership is capped at 32 entries; geometric commands invalidate
coarse results, and unloading releases the exact/result caches. A shared definition
may still be resolved for another loaded occurrence. These entry/array caps do not
bound OCCT internal allocation or GPU resources.

Coarse display is reused before exact requests for references/interference. Cold
loaded definitions rebuild coarse display from their authoritative exact inputs;
there is no portable persistent thumbnail/mesh cache or progressive detail renderer
in this scope. `source_changed` reports source revision changes separately from a
missing reference or stale result. Source changes trigger current evaluation, and
missing geometric tokens require explicit repair rather than a guessed mate.

## Mates and transactional editing

Supported rigid constraints are coincident point/point or plane/plane; concentric
axes; parallel/perpendicular/angle directed planes or axes; nonnegative point
distance and signed plane distance. Align/opposed controls oriented coincidence,
parallelism and concentricity. General angle is 0–pi radians. Planar distance is a
signed offset along the first plane normal; add parallelism when both planes must
stay parallel. Arbitrary splines, tangent/gear/flexible/contact mates are outside
this declared solver set.

The worker solves six rigid freedoms per movable occurrence using scaled SI
residuals, finite differences, SO(3) updates and Eigen SVD rank. Grounding removes
variables. Deterministic perturbations escape singular parallel/coincident seeds;
backtracking bounds each step. Rank reports local remaining freedom; inconsistent
or nonconvergent systems preserve the submitted placements and identify involved
mates. The reported conflict set is conservative, not a minimum conflict proof.
No partially converged placement is committed. This is a bounded iterative solver,
not a guarantee of convergence for every satisfiable nonlinear assembly.

The Assembly tab/PropertyManager supports insertion, transforms/grounding/states,
explicit replacement, exact planar/axial reference choices, mate editing, preview,
Accept/Cancel and component drag previews. Drafts disable document switches and
save/history commands. Accept commits only a valid solve. Escape/Cancel restores
the acknowledged mesh/model. A grounded component refuses drag.

## Interference and portable copies

Exact interference transforms current valid positive-volume solid bodies, computes
OCCT common volume, then distinguishes touching from disjoint with an exact
separation query when common volume is zero. Overlap uses volume above 1e-15 m³;
touching uses separation at most 1e-6 mm. Kernel failures are `Failed`, not Disjoint.
Missing/stale/unloaded or unsupported nonsolid inputs refuse the operation. The UI
lists non-disjoint results and highlights the first overlapping occurrence.

Portable Copied policy embeds all project-local Part/Assembly definitions and all
retained authoritative source blobs, preserves local document/occurrence/mate IDs,
records the source project/revision and creates a new project identity. Closure
checks all local assemblies, including unloaded metadata using a temporary resolve
validation. It never changes the current active project. BOM-source quantities count
unsuppressed flattened leaves (including hidden/unloaded), grouped by definition;
BOM tables/drawings and selected-subgraph export remain future modules.

## Verification limits

The frozen B3 recipe supplies 100 occurrences / 10 B1-derived exact definitions /
99 distance mates. Local DOF is intentionally 495. Analytic box interference and
mate corpora complement B3; sampling records explicit test/worker physical footprints
on the available 8 GiB Mac. Full SRS peak/steady/shared/GPU/FPS budgets, physical
input/accessibility, Windows/Linux, minimum macOS and installed builds remain
Blocked. The wider Module 6 hostile-parser gates remain deferred, not passed.
