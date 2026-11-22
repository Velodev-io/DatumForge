# CM9 execution plan — Catalogs, material/evaluation and model display

Prepared 22 November 2026 from `review/cm8-e-20261121` at `e865997`. The
[roadmap](../CORE_MODELING.md#cm9--catalogs-materialevaluation-and-model-display)
defines CM9.a–e. The [coverage register](../research/core-modeling-coverage.json)
contains 30 CM9 rows, CM-228 through CM-257, all marked **Not run** at planning
time. The [CM8 review](../testing/core-modeling-cm8-review.md) records 115 enabled
CTests and native checks at five scales for its available-Mac development run.
Capture a fresh baseline when CM9.a starts.

CM9 delivers offline standard and custom hole sizing, material assignment,
mass and section properties, appearance overrides, display styles and named
modeling sections. Keep the desktop responsible for interaction and display
meshes. The serial worker owns exact OCCT evaluation and project writes. Carry
forward the native macOS/Windows/Linux architecture, SI semantics, the physical
8 GB baseline and Blocked gates 6.1b, 6.4c and 7.4c. Stop before Module 11.

## Existing implementation to build on

| Area | Current entry point | CM9 work |
|---|---|---|
| Hole sizing | `src/features/ModelingIntent.cpp`, `src/geometry/occt/Holes.cpp`, `apps/desktop/FeaturePanel.cpp` | Resolve offline catalog rows to the existing CM6 SI sizing/termination fields. Preserve custom sizing and legacy catalog records, including records whose source version is absent. |
| Materials and measurement | `MaterialDensityKgM3` in `src/domain/Documents.hpp`, `MeasurePart` in `apps/cad-worker/Projects.hpp`, `Part::measure()` in `src/geometry/occt/Part.cpp` | Preserve edge/face measurement and legacy density. Add scoped evaluation for mass/inertia and planar section properties. Keep heavier calculations out of ordinary `DisplayPart` replies. |
| Existing assemblies | `DisplayAssembly` and `ProjectStore::assemblyResult` | Reuse accepted occurrence transforms and exact Part resolution for the read-only mass/planar-face cases named in T5. Keep assembly authoring and drawing publication outside CM9. |
| Display and picking | `MainWindow.cpp`, `src/render/Viewport.*`, `src/protocol/Mesh.*` | Reuse native view controls and worker-published body/face roles. Add verified occlusion, appearance, shadows and clipping with picking that matches the display. |
| Storage and transport | `src/persistence/ProjectStore.*`, `src/protocol/Client.cpp`, worker command dispatch | Add versioned semantic records and a reader requirement where needed. Reuse revision guards, history, recovery, portable copy and typed reference repair. |

## CM9.a — Freeze scope, rights and contracts

- [ ] Follow applicable [Step Zero](../STEP_ZERO.md) gates in lettered order.
  Freeze the base/source identity, configure/build with pinned dependencies and
  capture the enabled CTest baseline. Record disabled tests and blocked host or
  fixture prerequisites. Use marked disposable roots for worker and UI runs.
- [ ] Reconcile the 30 rows against retained T5/T7 pages and visually confirm
  course dimensions where diagrams are required. Freeze the material-family
  and property matrix and a standards-by-hole-type, size-range and fit matrix.
  Mark which requirements come from course examples and which come from the
  broader roadmap. The T7 M8 counterbore tutorial explicitly overrides its
  size. Use that as a custom-override fixture and verify standard defaults from
  a separately traceable dataset.
- [ ] Extend the [catalog acquisition register](../research/core-modeling-catalog-sources.md).
  For each table or material entry, record immutable source and hash, edition,
  units, tolerances or conditions, attribution and redistribution rights.
  Record unknown or disallowed data as Blocked with impact on specific rows.
  Ship only verified data and its required notices. The independent custom,
  evaluation and display work proceeds while acquisition stays open.
- [ ] Specify bounded dataset IDs, versions and stable entry IDs, SI units and
  validation, material defaults/overrides and missing-density behavior. Freeze
  measurement scope, frame, tensor sign convention and degeneracy handling.
  Validate how the T5 assembly cases can resolve accepted occurrences and face
  references through the existing worker. Record any unresolved scope as open.
- [ ] Define typed appearance targets and section plane/face references. Start
  with face/surface → feature → body → part precedence and verify it against
  the source workflows and current topology provenance before locking the
  contract. Define default/user planes, planar faces, signed normal offsets and
  the retained half-space. Document missing/ambiguous target behavior and add
  focused contract checks for invalid combinations and compatibility.

**Exit gate** — The matrix, contract tests and source decisions are reviewable.
Unresolved redistribution or broad coverage keeps CM9.a's data gate open.

## CM9.b — Implement catalog resolution, exact evaluation and display

- [ ] Load admitted datasets offline with bounded input, stable keys and strict
  duplicate/unit/range checks. Resolve a chosen hole to concrete dimensions
  and feed the existing CM6 adapter. Route custom entries and overrides through
  the same validation. Unsupported family/fit/size combinations report an
  explicit diagnostic. Preserve CM6 termination restrictions and cosmetic
  thread behavior.
- [ ] Evaluate volume, surface area, mass, center of mass, inertia tensor and
  principal moments/axes from exact worker shapes at the accepted revision.
  Sum density-weighted contributions in a common frame, applying rotations and
  the parallel-axis theorem before diagonalization. Cover whole Part, selected
  solid body and documented read-only assembly scope. Separate nonsolid area
  from solid volume/mass. Report missing density without assuming a material;
  keep valid geometry results available. Compare repeated eigenvalues and axes
  using invariant subspaces or residuals where directions are non-unique.
- [ ] Evaluate planar section properties from closed, nonintersecting sketch
  regions and selected planar faces, including holes and disjoint regions where
  valid. Reuse exact profile/face construction. Return area, centroid in local
  and Part coordinates, centroidal second moments, polar moment and principal
  axes/moments. Handle tilted frames, stable transformed results for existing
  occurrences, and open, intersecting, nonplanar or stale selections.
- [ ] Implement Hidden Lines Visible with visible lines and dashed occluded
  segments, verify Shaded with Edges, and add a shadow toggle for shaded modes.
  Apply resolved appearance to display roles. Implement view-only clipping
  with matching picking. Keep any exact intersection or cap computation in
  the worker and treat its output as display data. Section views leave feature
  history, measurement inputs and exchange geometry intact.

**Exit gate** — Focused worker/transport tests and analytical comparisons pass
for supported scopes. Display checks distinguish occluded edges from Wireframe
and verify bounded previews and invalid-input behavior.

## CM9.c — Persist accepted meaning and view definitions

- [ ] Persist source dataset/version/entry identity and resolved numeric values
  for accepted holes and materials. Model replay and portable copy work with
  changed or absent catalogs. Applying updated values is an explicit edit with
  normal validation and undo. Preserve legacy `MaterialDensityKgM3` and older
  hole snapshots without fabricating provenance. Define a single effective
  density source, including any verified body override.
- [ ] Store appearance overrides and named modeling sections as semantic view
  records separate from feature geometry. Persist stable IDs, typed targets,
  reference frames/offsets and orientation, as applicable. Accept only proven
  unique topology evolution. Missing/split/ambiguous faces require explicit
  repair or removal. Saved section views follow their resolved support frame
  after an upstream edit.
- [ ] Audit compatibility across open, history, recovery and portable copy.
  Keep the new reader requirement sticky across retained history where needed.
  Apply changes atomically with revision checks and preserve omitted collections
  on unrelated edits. Verify SQL bindings, legacy replay, older-reader refusal,
  interrupted commits, worker restart, undo/redo and catalog replacement.

**Exit gate** — Reopen and portable copy preserve accepted dimensions, material
properties and view definitions. Failure and stale requests preserve accepted state.

## CM9.d — Native authoring and inspection

- [ ] Add catalog filters and search to the Hole panel, with clear provenance,
  valid standards/types/sizes/fits and explicit custom overrides. Integrate
  material search, favorites, custom libraries, property inspection and Apply.
  Preserve keyboard navigation, accessible labels and unit-aware inputs.
- [ ] Provide Evaluate views for mass and section properties. Show the active
  scope, reference frame and units, with centroid/principal directions where
  defined. Show partial or invalid results explicitly and discard stale replies.
- [ ] Add scoped appearance selection, style and shadows controls, and named
  section create/edit/activate/remove. Allow a datum plane or planar face as
  section support, with numeric and pointer offset controls. Coalesce previews
  to one running computation and one replaceable pending draft. Cancel restores
  the prior view while accepted geometry and selection semantics stay intact.

**Exit gate** — Worker-backed native keyboard and pointer scenarios cover
create/edit/apply/cancel/reopen, mm/in, and observed 100/125/150/175/200% scales.

## CM9.e — Verify the complete matrix and record evidence

The groups below cover every existing CM9 row. CM9.a expands them into
per-ID executable fixtures and native workflow cases before any status changes.

| Coverage IDs | Source | Planned evidence |
|---|---|---|
| CM-228–230 | T5 pp. 27–29, 41–42 | Offline family/property provenance matrix, search/favorites and custom libraries. Validate conditions and units, plus explicit reassignment and catalog refresh. |
| CM-231–235 | T5 pp. 27–29, 41–42 | Part, body, feature and face/surface appearance; precedence, reset, edit/reopen and ambiguous topology; visible shaded-shadow change. |
| CM-236–240 | T5 pp. 28, 34–35 | Cuboid and cylinder closed-form results, asymmetric mixed-density shape, translated/rotated inertia, principal degeneracy, surface-only response and existing-assembly scope. Reconstruct the T5 material/mass workflow with admitted inputs. |
| CM-241–244 | T5 pp. 28, 34–35; loop restriction p. 36 | Offset inner loop, multiple regions and tilted planar face; independent area/centroid/second moments and axes. Rebuild the T5 pre-revolve section workflow and verify invalid loops and stale selections. |
| CM-245–246 | T7 pp. 7–8, 33–34 | Render known visible and occluded edges under rotation, verify distinct dashed/solid lines and shaded edges, and check picking and unchanged exact measurements. |
| CM-247–249 | T7 pp. 7–8, 33–34 | Name/reopen and toggle the T7 modeling section, select plane or planar face, compare numeric and drag offsets, then test upstream movement and broken support repair. |
| CM-250–257 | T7 pp. 21, 29 + roadmap expansion | ANSI inch, ANSI metric, ISO, DIN, BSI, JIS, GB and GOST each have their own rights/edition/type/size/fit matrix. Compare independent source values and resulting hole sections. Exercise mm/in, custom overrides, offline replay and unavailable combinations. Keep each incomplete family row open. |

- [ ] Use the [build commands](../../cmake/README.md) and reuse the evidence
  pattern in [`verify_cm8_review.py`](../../tools/verify_cm8_review.py). Add
  focused `cm9.*` CTests, run all enabled CTests and retain source identity,
  format, changed-source analysis, ownership, coverage and spec checks. Retain
  negative and superseded runs alongside the final native evidence. Preserve
  frozen B0–B6 and measure repeated preview/edit/reopen and cache behavior.
- [ ] Record available-Mac production and native verification separately from
  per-row course evidence and full acceptance. Catalog gaps, missing course
  fixtures, physical 8 GB, Windows/Linux, installed builds, physical devices
  and assistive-technology gates retain their observed status. Update the
  coverage register and roadmap only from observed evidence.
- [ ] For each successfully verified implementation task use its own review
  branch, commit and push the scoped change to configured `origin`, and verify
  the remote ref. Leave branches for review and avoid merges or force-pushes.
