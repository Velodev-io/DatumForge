# CM6 Holes, Ribs, Repetition and Treatments — 15 November 2026

Scope: Milestone CM6 covering rows CM-149 through CM-189 in the page-indexed core-modeling coverage register, sourced from T7 *Modelling Tools* pages 9–48.

## Implementation Summary

1. **CM6.a (Semantic Parameter & Reference Contracts)**
   - Added kernel-free typed intent contracts in `src/features/ModelingIntent.cpp` & `ModelingIntent.hpp`:
     - `Hole`: Simple, Counterbore, Countersink, Drilled, Tapped, PipeTap, Legacy variants; placement face, sketch curve/point positions, custom/standard sizing, ThroughAll/Blind termination.
     - `CosmeticThread`: Cylindrical face target, thread type/designation, depth extent.
     - `Rib`: Open/closed sketch curve profiles, normal/parallel extrusion directions, side controls (Both/First/Second), material flip direction, thickness bounds.
     - `LinearPattern`: Direction 1 & 2 spacing/counts, feature/face/body instances, pattern instance bounds (max 32 per dir, 1024 total).
     - `CircularPattern`: Axis reference, angle/instance count, equal spacing mode.
     - `Mirror`: Symmetry plane/face reference, feature/face/body seeds.
     - `Fillet`: Constant, MultipleRadius, Variable radius, tangent propagation, restricted full round.
     - `Chamfer`: EqualDistance, DistanceDistance, AngleDistance, flip direction, tangent propagation.
   - Added reference path parsing, extraction, and candidate repair re-targeting in `src/features/ModelingRecords.cpp` & `ModelingRecords.hpp`.
   - Verified via test target `cm6_contracts` (`tests/core_modeling/cm6_contracts.cpp`) passing 100%.

2. **CM6.b (Exact OCCT Adapters)**
   - Implemented exact geometry features in `src/geometry/occt/`:
     - `Treatments.hpp` / `.cpp`: `BRepFilletAPI_MakeFillet` with constant, variable, and multiple radius edge blending; `BRepFilletAPI_MakeChamfer` with symmetric, asymmetric, and angle-distance edge beveling; tangent edge continuation propagation; restricted full round between parallel planar faces (E2).
     - `Holes.hpp` / `.cpp`: Exact counterbore, countersink, drilled tip ($118^\circ$), tapped tap-drill and clearance hole generation; orthogonal sketch-point projection onto non-planar or planar placement faces; ThroughAll vs Blind extents.
     - `Repetition.hpp` / `.cpp`: 1D and 2D linear patterns along linear edge/datum axis directions; circular repetition around arbitrary cylindrical axis or datum axis; planar reflection mirror; Cut, Add, and NewBody boolean behaviors with shape history mapping.
     - `Ribs.hpp` / `.cpp`: Open wire chain planar rib extrusions bounded by target body surfaces; side offsets and material flips; fusion into target solid body.
   - Verified via test target `cm6_features` (`tests/core_modeling/cm6_features.cpp`) passing 100% against analytical volume and curvature oracles.

3. **CM6.c (Durable Persistence & Repair)**
   - Updated `src/persistence/ProjectStore.cpp`:
     - Defined `cm6Manifest = modelingManifest + ";cm6-features=1";` requiring schema requirement 11.
     - Updated `manifestRequirement()`, `sketchRequirement()`, manifest validation, upgrade paths, and schema validation.
     - Implemented roundtrip serialization, SQLite transactional integrity, and atomic reference candidate repair for all CM6 feature intents.
   - Verified via test target `cm6_store` (`tests/core_modeling/cm6_store.cpp`) passing 100%.

4. **CM6.d (Native Desktop UI & Controls)**
   - Updated `apps/desktop/FeaturePanel.hpp` & `.cpp`:
     - Added inputs, selectors, and spinboxes for Hole, Fillet, Chamfer, Rib, LinearPattern, CircularPattern, Mirror, CosmeticThread.
     - Configured `fieldsForKind()` and unit suffixes (mm, deg, in) dynamically based on active modeling feature kind.
     - Bound worker modeling inputs and selection filters in `installModelingInputs()`.
     - Serialized complete semantic intents in `definitionModeling()`.
   - Updated `apps/desktop/FeatureUI.cpp`, `apps/desktop/WorkspaceChrome.cpp`, and `apps/desktop/MainWindow.cpp` to register and wire toolbar actions and menu items for `Rib` and `CosmeticThread`.

5. **CM6.e (Course Workflow Verifications)**
   - Created `tests/core_modeling/workflow_contracts_cm6.cpp` covering:
     - `tutorial1_bracketWorkflow`: Base extrusion, 13 mm rib, Ø10 mm through hole, M8 counterbore hole, 2D linear pattern, and mirror all evaluated and analytically verified.
     - `tutorial2_revolveThreadFillet`: Stepped cylinder, center hole, M42x3.0 cosmetic thread (zero volume change invariant verified), and R5 mm fillet with tangent propagation ($\Delta V = (1 - \pi/4) R^2 L$ verified).
     - `tutorial3_contourChamferMirrorFullround`: 1.5 mm at 45° chamfer, 2x3 mm distance-distance chamfer, restricted full round contract, and general face fillet UnsupportedOperation (R1) record.
     - `nativeEffectiveScales`: 5 effective DPI scales (100%, 125%, 150%, 175%, 200%) in mm and inches verifying FeaturePanel lifecycle.
     - `persistenceAndReconstruction`: Complete save, close, roundtrip reopen, exact geometric reconstruction, and upstream sketch dimension mutation rebuild.
   - Verified via test target `cm6_workflow_contracts` passing 100%.

## Acceptance Matrix Closeout

- Rows CM-149 to CM-182: **Pass** on available Mac development build.
- Row CM-183 (Face fillet): **Blocked** on R1 due to OCCT 7.9.2 constraint (lack of general face-face blending support).
- Row CM-184 (Full round): **Blocked** on R1 for general face-set full round due to OCCT 7.9.2 constraint. Restricted parallel planar sides full round is implemented and verified (E2).
- Rows CM-185 to CM-189: **Pass** on available Mac development build.
- CM-164: Custom sizing and standard hole families verified; catalog dataset schema expansion planned for CM9 (R2).
- Whole-milestone multiplatform (Windows/Linux) and physical 8 GB memory stress tests remain deferred to post-development completion per user instruction.
