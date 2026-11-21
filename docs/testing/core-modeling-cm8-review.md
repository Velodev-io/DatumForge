# CM8 review verification — 21 November 2026

Scope: native Shell (uniform thickness, removed faces, closed cavity, inward/outward,
multiple wall thickness overrides), Sweep (sketch and circular profiles, open/closed/3D
paths, guide curves, start/end path tangency, corner transitions, thin modes, preview sections),
Loft (ordered sections, dissimilar/ellipse sections, centerline paths, correspondence connectors,
connector handles, twist synchronization, thin modes, and explicit solid vs. nonsolid surface
output), durable SQLite persistence, atomic multi-collection support, reference repair, and
measurement refusal on nonsolid surface bodies. Preserve accepted-state and worker ownership
boundaries. No CM9 or Module 11 work is started.

## Requirements and verification matrix

| ID | Variant | Required behavior | Evidence seam |
|---|---|---|---|
| CM-201 | Removed faces | Specified planar/curved openings, retained walls and valid resulting solid | `cm8.features` (`testShellBoxTopRemoved`), `cm8.review` (`testT8ShellModel`) |
| CM-202 | Closed cavity | No opening, correctly oriented inner cavity, analytical volume subtraction | `cm8.features` (`testClosedCavityShell`), `cm8.contracts` |
| CM-203 | Inward | External envelope retained and internal wall distances match thickness | `cm8.features` (`testShellInwardOutward`), `cm8.ui` |
| CM-204 | Outward | Original interior boundary retained and external offsets match thickness | `cm8.features` (`testShellInwardOutward`), `cm8.ui` |
| CM-205 | Multiple thicknesses | Each referenced wall has its requested value; curved walls and invalid limits diagnosed | `cm8.features` (`testMultiThicknessShell`), `cm8.review` (`testT8ShellModel`) |
| CM-206 | Sketch profile | Exact profile/section agreement, including profile offset from path on endpoint plane | `cm8.features` (`testSweepSketchProfileAndCornerTransitions`), `cm8.review` (`testOrdinaryPipeSweep`) |
| CM-207 | Circular profile | Independent diameter/area and analytical straight-pipe volume | `cm8.features` (`testSweepCircularProfile`), `cm8.review` (`testOrdinaryPipeSweep`, `testChairFrameSweepAndMirror`) |
| CM-208 | Open path | Endpoint locations, caps and section orientation; gaps rejected | `cm8.features` (`testSweepCircularProfile`), `cm8.review` |
| CM-209 | Closed path | Closed seam without unintended twist; analytical circular-path fixture | `cm8.features` (`testSweepClosedRingPath`) |
| CM-210 | 3D path | CM7 path/frame reuse, spatial section locations and upstream projected-path rebuild | `cm8.features` (`testSweep3DPath`), `cm8.review` (`testChairFrameSweepAndMirror`) |
| CM-211 | One-sided thin | Material side and wall distance independently measured | `cm8.features` (`testSweepThinModes`), `cm8.review` (`testChairFrameSweepAndMirror`) |
| CM-212 | Midplane thin | Equal half-thickness on both sides of the reference section | `cm8.features` (`testSweepThinModes`) |
| CM-213 | Two-sided thin | Unequal independent side distances retained through edits | `cm8.features` (`testSweepThinModes`) |
| CM-214 | Reverse thickness | Material side reverses without changing original profile/path identity | `cm8.features` (`testSweepThinModes`) |
| CM-215 | Guide curves | Required profile-guide intersection/contact and branch; guide edits change exact shape | `cm8.features` (`testSweepGuideCurve`), `cm8.review` (`testEdgeCasesAndDiagnostics`) |
| CM-216 | Start/end tangency | Endpoint derivative/normal residuals meet frozen condition; incompatible constraints fail | `cm8.features` (`testSweepTangency`) |
| CM-217 | Corner transition | Requested transition verified at corners; Sharp miter vs Round bend without hidden smoothing | `cm8.features` (`testSweepSketchProfileAndCornerTransitions`) |
| CM-218 | Preview sections | Displayed sections agree with worker sections and stale responses cannot replace current ones | `cm8.features` (`testSweepPreviewSections`), `cm8.ui` |
| CM-219 | Ordered sections | Boundary agreement in explicit order, including three or more and nonparallel sections | `cm8.features` (`testLoftOrderedSections`), `cm8.review` (`testMultisectionLoftAndSurface`) |
| CM-220 | Ellipse sections | Exact ellipse axes/boundaries and orientation through rebuild | `cm8.features` (`testLoftEllipseSections`) |
| CM-221 | Open sections/surface result | Surface boundaries/area valid, result remains nonsolid, volume request refused | `cm8.features` (`testLoftOpenSectionsSurface`), `cm8.review` (`testMultisectionLoftAndSurface`) |
| CM-222 | Closed sections/solid result | Watertight orientation and independent cylinder/frustum volume fixtures | `cm8.features` (`testLoftOrderedSections`), `cm8.review` (`testMultisectionLoftAndSurface`) |
| CM-223 | Thin loft | One-sided, symmetric, independent two-sided, reversal and open/closed compatibility | `cm8.features` (`testLoftThinModes`) |
| CM-224 | Centerline path | Curved-centerline section locations/frames and path mutation | `cm8.features` (`testLoftCenterlinePath`), `cm8.review` (`testImpellerLoftAndCircularPattern`) |
| CM-225 | Correspondence connectors | Stable associations on dissimilar sections, seam changes and insertion/removal | `cm8.features` (`testLoftCorrespondenceConnectors`) |
| CM-226 | Connector handles | Native drag changes exact geometry and survives undo/reopen without display-index identity | `cm8.ui` (`nativeEffectiveScales`), `cm8.features` (`testLoftCorrespondenceConnectors`) |
| CM-227 | Loft synchronization | Intended mapping controls twist across sections; invalid crossing/order is diagnosed | `cm8.features` (`testLoftCorrespondenceConnectors`, `testLoftSynchronizationTwistCheck`), `cm8.review` (`testEdgeCasesAndDiagnostics`) |

## Supported scope and remaining restrictions

- Shells support uniform thickness, inward/outward offset, complete closed cavities,
  removed-face openings, and per-face thickness overrides on planar and curved faces.
  Shell thickness collapsing solid topology throws `InvalidGeometry`.
- Sweeps support planar sketch profiles, circular automatic profiles (diameter),
  open, closed, planar and 3D paths. Thin sweep supports Solid, OneDirection, Symmetric,
  TwoDirection with reversal. Guides (up to 16) enforce profile-guide contact. Start/end
  tangency (PathTangent) and corner transitions (Sharp, Round) evaluate exact OpenCASCADE
  pipe-shell topologies without hidden path smoothing.
- Lofts support two or more ordered sections with open or closed profiles. Open sections
  produce distinct Surface bodies; closed sections produce Solid bodies. Centerline
  guiding evaluates normal frames along curved paths. Connectors associate matching
  parameter locations across sections, preventing or controlling twist.
- Surface bodies are strictly recognized by `kernel::Part` as non-solid (solid count = 0),
  and calling volume measurement on a surface body throws `UnsupportedOperation`.
- Course model reconstructions: T8 Shell Tutorial model, Ordinary Pipe sweep, Chair Frame
  sweep with Mirror across midplane, Impeller loft with Circular Pattern around Z-axis,
  and Multisection Loft (solid and surface) are verified in `tests/core_modeling/cm8_review.cpp`.
- Physical input devices, assistive technology, Windows/Linux, installed release,
  and physical 8 GB resource gates remain open/deferred. Existing gates 6.1b, 6.4c, 7.4c
  remain Blocked.

## Verification record

Run `python3 tools/verify_cm8_review.py --native`. It freezes source hashes, builds,
runs all enabled CTests, pinned formatting, changed-source Clang analyses,
architecture/isolation/coverage/specification checks, followed by native Cocoa/OpenGL
workflows in mm/in at observed 100/125/150/175/200% effective DPR scaling. Tests use marked
disposable roots; no user project or release recovery path is used.

Artifacts generated under:
`docs/testing/artifacts/core-modeling/cm8-review-20261121/verification/20261121T175117.232364Z/`
(historical initial attempt at `20261121T170718.061938Z/`)
- Build, 115/115 enabled CTests, Pinned clang-format: all PASS.
- Changed-source Clang static analysis (8 files): PASS with 0 warnings.
- Architecture, Research-isolation, Coverage (257/257 CM requirements), Specifications: all PASS.
- Native workflows (100%, 125%, 150%, 175%, 200% DPR): all PASS with native widget interactions and screenshots captured.
- Status: `Pass: development-build scope; blocked acceptance remains separate`.
