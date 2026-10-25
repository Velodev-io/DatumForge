# Module 5 evaluation, history and repair

Date: 20 October 2026. Scope: authorized Module 5 on the available Mac. Native
Windows/Linux and full hardware/resource acceptance remain Blocked. Stop before
Module 6 / 6.1a. [Authorization](implementation-scope.json) and
[verification](../testing/module-5.md).

## Authority and compatibility

Project inputs remain semantic sketches, feature definitions, expressions and
reference identities in SQLite. One serial worker owns exact geometry, solving,
result caches and all project writes. The desktop owns semantic/display
projections, transient forms and meshes; it links neither OCCT nor SQLite.

Feature schema v1 is emitted unchanged when suppression and expressions use their
defaults. Feature schema v2 adds explicit suppression and at most eight expression
bindings, each bounded to 4 KiB and supported numeric parameters. A new
`history-v2;fingerprint=1` compatibility manifest admits semantic history
collections. Existing schema-v1 projects remain readable; older workers refuse
the newer manifest rather than dropping history. Reading does not rewrite files.

A history collection stores current definitions, last-valid checkpoint definitions,
checkpoint profile sketches and a stale flag. Checkpoints are authoritative
semantic inputs for reconstructing the last-valid display; no cached BRep is a
required project input. Reopen and recovery validate the current stale state and
rebuild its checkpoint. Undo/redo restores the complete prior collection,
including the stale state and checkpoint. A failed speculative preview never
advances the project revision. Committing failed geometry requires the user's
explicit stale-result acknowledgment. Invalid dependency graphs cannot be committed.

## Dependency graph and fingerprints

`src/evaluation/Plan` records profile input hashes, source-feature dependencies,
target-body creators, prior writers of each body and feature-expression references.
Deterministic UUIDv5 copy-body identities retain their owning feature. History
order must follow the graph; cycles and forward dependencies have distinct errors.
Descendants and impact sets follow the explicit feature order. Delete requires
explicit dependent cascade; reorder is accepted only when dependency-valid.
Renaming does not change a geometry fingerprint.

Fingerprints use sorted semantic CBOR, the resolved parameters, suppression,
expressions, referenced sketch, ordered upstream fingerprints and an explicit
OCCT/adapter/tolerance/fingerprint version. They do not depend on display meshes,
feature names or process addresses. A changed input invalidates its descendants;
unchanged unrelated branches reuse immutable results. Equivalent geometry is
checked within declared tolerances, not by byte-identical BRep serialization.

Feature expressions use `F_<32 lowercase UUID hex digits>_<parameter>`, for
example `F_0123456789abcdef0123456789abcdef_depth / 2`. Lengths and angles require
units; counts are dimensionless integers. The shared arithmetic parser enforces
dimensions, domains, finiteness, cycles and text/AST/depth limits. Feature history
admits up to 1,024 symbols for the 100-feature workload; sketch environments retain
their original 64-name default. Numeric controls and expression controls use the
same validated semantic parameters. The graph admits a bounded union of up to
32 profiles for before/after impact; actual geometry evaluation retains its
16-profile admission limit.

## Immutable results and stale state

Worker results use a 128-entry LRU with a maximum 256 MiB conservative topology
estimate. Entries contain only each feature's new/touched bodies, source stage,
tool and bounded history delta. Their keys incorporate all upstream inputs.
Boolean operations set OCCT non-destructive mode. Meshing deep-copies shapes
before tessellation so viewport generation cannot mutate cached topology.

Trimming clears cache ownership and accounting. It preserves project definitions,
undo/recovery records and semantic checkpoints, so exact results can be rebuilt.
The estimate is not a measurement of OCCT's physical allocation, allocator
retention or total process/GPU footprint. Full live-object/physical-memory proof
for NFR-009 and the SRS resource budgets remains Blocked; the B2 test establishes
bounded entry/accounting behavior and post-trim regeneration only.

A failing node rolls back its own effect and marks its descendants failed;
independent branches can still evaluate. The UI conspicuously marks failed history
and displays the last-valid checkpoint. Exact measurement rejects a stale part.
Module 6 exact exports do not exist yet and must use the same valid-revision gate.
A reference-inspection preview explicitly shows current upstream source geometry,
so users can choose a replacement; it is distinguished from the stale final body.

## Reference evolution and explicit repair

Every supported adapter records actual OCCT Generated/Modified/IsDeleted history.
Role continuation follows modified descendants; generated geometry does not
silently inherit the old role. Resolution distinguishes direct, history, split,
merged, deleted, missing, candidate and ambiguous outcomes.

When history cannot uniquely retain a reference, constrained matching considers
topological type, radius/length, adjacency and bounded center context against the
former role. Candidates already present as another former role are excluded.
A unique geometric candidate still requires explicit replacement. Multiple
symmetric candidates remain ambiguous. No arbitrary face/edge index becomes an
authoritative reference. These rules are deliberately conservative and are not a
general guarantee of persistent naming for every possible OCCT mutation.

The left History/repair PropertyManager shows owner, former role and dimensions,
affected dependents and candidate geometry; long tokens are available as tooltips.
It highlights the current source candidate and sends an explicit ResolveReference
command with a source fingerprint. The worker recomputes and validates that
candidate against the current revision, then uses normal durable request dedup.
Replacement is undoable; undo restores the original stale collection and redo
restores the repaired result. Pattern-count removal, actual split/merge/deletion
and symmetric candidates have focused engine tests.

## Multi-document scope and remaining gates

Profiles can refer to project-unique sketches in another document. Editing a
shared sketch validates and rebuilds every consuming part; invalid dependent
edits remain atomic. This is tested with two part documents. General linked solid
imports, assembly references and drawing dependencies belong to Modules 6–8.

The frozen B2 recipe remains unchanged. Tests construct its 100 exact features,
compare analytic volume and mesh/face caps, run 100 edit/undo/redo cycles, trim,
regenerate, save, reopen and recover. Native history/repair workflows launch
isolated test windows and workers in marked disposable roots. They never attach
to an existing user session. Physical mouse/trackpad delivery, assistive technology,
other OS/CPU/GPU configurations, installed builds, controlled network-disabled
runs, physical power loss and complete physical 8 GB budgets remain Blocked.
