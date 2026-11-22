# CM9 review verification — 22 November 2026

Scope: native Hole sizing from offline and custom catalogs across ANSI Inch, ANSI Metric,
ISO, DIN, BSI, JIS, GB, and GOST standards, materials and mass properties evaluation (exact
volume, surface area, mass, center of mass, inertia tensor, and principal moments), planar
section properties evaluation (area, centroid, second moments of area, and principal moments),
appearance overrides across Part, Body, Feature, and Face levels with precedence and reset,
model display modes (Hidden Lines Visible and Shadows), and named Section Views (offset
manipulation, flip direction, and live viewport clipping). Durable SQLite persistence with
sticky manifest rank 14 (`cm9-eval-display=1`), atomic multi-collection support, reference
repair, and replay without live catalog dependency. Preserve accepted-state and worker
ownership boundaries. Stop before Module 11.

## Requirements and verification matrix

| ID | Variant | Required behavior | Evidence seam |
|---|---|---|---|
| CM-228 | Offline material families | Standard material library with physical density, Poisson's ratio, yield strength across Steel, Iron, Aluminum, Plastics | `cm9.contracts` (`testMaterialContracts`), `cm9.ui` |
| CM-229 | Search/favorites/custom libraries | Search filtering, favorite toggle, custom material creation with user-defined physical properties | `cm9.contracts` (`testMaterialContracts`), `cm9.ui` |
| CM-230 | Physical properties | Property inspection, mass/inertia evaluation from material density, explicit override density support | `cm9.contracts` (`testMaterialContracts`), `cm9.features` (`testMassPropertiesEvaluation`) |
| CM-231 | Part appearance | Document-level material/appearance default applied to all bodies lacking overrides | `cm9.contracts` (`testAppearancePrecedenceContracts`), `cm9.store` |
| CM-232 | Body appearance | Body-level appearance override taking precedence over Part-level appearance | `cm9.contracts` (`testAppearancePrecedenceContracts`), `cm9.store` |
| CM-233 | Feature appearance | Feature-level appearance override applied to feature-generated faces | `cm9.contracts` (`testAppearancePrecedenceContracts`), `cm9.store` |
| CM-234 | Face/surface appearance | Highest precedence face-level appearance override, individual face coloring, reset capability | `cm9.contracts` (`testAppearancePrecedenceContracts`), `cm9.repair` |
| CM-235 | Shadows | Shaded mode floor/contact shadows toggle in Viewport and WorkspaceChrome | `cm9.features` (`testViewportDisplayAppearanceAndClipping`), `cm9.ui` |
| CM-236 | Density/mass | Analytical mass calculation (`m = rho * V`) from assigned material density or override density | `cm9.features` (`testMassPropertiesEvaluation`), `cm9.ui` |
| CM-237 | Volume/surface area | Exact OCCT volume and surface area integration for single and multi-body parts | `cm9.features` (`testMassPropertiesEvaluation`), `cm9.ui` |
| CM-238 | Center of mass | Exact center of mass `(x, y, z)` in part frame and local body frame | `cm9.features` (`testMassPropertiesEvaluation`), `cm9.ui` |
| CM-239 | Principal axes/moments | Exact principal moments of inertia `(Ix, Iy, Iz)` and principal coordinate system orientation | `cm9.features` (`testMassPropertiesEvaluation`), `cm9.ui` |
| CM-240 | Inertia tensor | Full 3x3 symmetric inertia tensor taken at center of mass and aligned with output coordinates | `cm9.features` (`testMassPropertiesEvaluation`), `cm9.ui` |
| CM-241 | Sketch section area | Exact 2D area integration of closed planar sketch profile | `cm9.features` (`testSectionPropertiesEvaluation`), `cm9.ui` |
| CM-242 | Section centroid local/part | Exact centroid `(cx, cy)` in sketch coordinates and translated 3D part frame coordinates | `cm9.features` (`testSectionPropertiesEvaluation`), `cm9.ui` |
| CM-243 | Section inertia/polar/principal moments | Moments of inertia `(Ixx, Iyy, Ixy)`, polar moment `J`, and principal axes angle/moments | `cm9.features` (`testSectionPropertiesEvaluation`), `cm9.ui` |
| CM-244 | Planar-face section properties | Cross-sectional evaluation from selected planar boundary face with inner/outer loops | `cm9.features` (`testSectionPropertiesEvaluation`), `cm9.ui` |
| CM-245 | Hidden lines visible | Wireframe/occlusion display mode rendering visible edges solid and hidden edges dashed | `cm9.features` (`testViewportDisplayAppearanceAndClipping`), `cm9.ui` |
| CM-246 | Shaded with edges | Solid shaded faces with prominent contrasting feature and boundary edges | `cm9.features` (`testViewportDisplayAppearanceAndClipping`), `cm9.ui` |
| CM-247 | Named section view | Persistent named cross-section view with unique ID and label | `cm9.contracts` (`testSectionViewContracts`), `cm9.store`, `cm9.ui` |
| CM-248 | Section plane/face selection | Sectioning plane defined by datum plane reference or planar face selection | `cm9.contracts` (`testSectionViewContracts`), `cm9.repair`, `cm9.ui` |
| CM-249 | Section offset manipulator | Numeric offset spin and dynamic drag manipulator, flip normal direction toggle | `cm9.features` (`testViewportDisplayAppearanceAndClipping`), `cm9.ui` |
| CM-250 | ANSI inch | Offline catalog for ANSI inch standard fasteners (hex, socket, countersunk, counterbore) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`) |
| CM-251 | ANSI metric | Offline catalog for ANSI metric standard fasteners (B18.3.1M, B18.3.4M, B18.6.7M) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`), `cm9.ui` |
| CM-252 | ISO | Offline catalog for ISO standard fasteners (ISO 4762, ISO 10642, ISO 7045) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`) |
| CM-253 | DIN | Offline catalog for DIN standard fasteners (DIN 912, DIN 7991, DIN 931) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`) |
| CM-254 | BSI | Offline catalog for British Standard fasteners (BS 4168, BS 3643) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`) |
| CM-255 | JIS | Offline catalog for Japanese Industrial Standard fasteners (JIS B 1176, JIS B 1012) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`) |
| CM-256 | GB | Offline catalog for Chinese National Standard fasteners (GB/T 70.1, GB/T 5782) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`) |
| CM-257 | GOST | Offline catalog for GOST fasteners (GOST 11738, GOST 7798) | `cm9.contracts` (`testHoleCatalogContracts`), `cm9.features` (`testHoleCatalogResolutionAndOverrides`) |

## Supported scope and remaining restrictions

- Hole sizing resolves offline standard dimensions across 8 standard datasets (`ansi_metric`,
  `ansi_inch`, `iso`, `din`, `bsi`, `jis`, `gb`, `gost`) with socket head, button head, flat
  countersunk, and hex head fastener types across Close, Normal, and Loose fits.
- Sizing snapshots are persisted directly in Hole intents, guaranteeing offline model replay
  and exact rebuild parity without requiring live catalog database availability.
- Material library provides default materials across Steel, Iron, Aluminum, Brass, Copper,
  Titanium, Plastics, and Wood with density, elastic modulus, Poisson's ratio, tensile strength,
  yield strength, and thermal conductivity. Custom materials are fully supported.
- Mass properties evaluation computes exact OCCT solid volume, surface area, mass, center of
  mass, inertia tensor, and principal moments/axes. Single-body and multi-body scopes are supported.
- Section properties evaluation computes exact 2D area, centroid in local and part frames,
  moments of inertia ($I_{xx}, I_{yy}, I_{xy}$), polar moment of inertia ($J$), and principal
  moments and axes from planar sketch profiles or selected planar boundary faces.
- Appearance overrides enforce strict precedence: Face > Feature > Body > Part. Appearance
  overrides can be edited, reset, and survive undo/redo and project save/reopen.
- Section views allow defining clipping planes by datum plane or planar face reference, with
  offset adjustment and normal flipping. Live Viewport clipping is reflected in native rendering.
- Model display modes support Shaded, Shaded with Edges, Hidden Lines Removed, and Hidden Lines
  Visible (dashed occlusion rendering), with contact/ground shadows toggle.
- Physical input devices, assistive technology, Windows/Linux, installed release,
  and physical 8 GB resource gates remain open/deferred. Existing gates 6.1b, 6.4c, 7.4c
  remain Blocked.

## Verification record

Run `python3 tools/verify_cm9_review.py --native`. It freezes source hashes, builds,
runs all enabled CTests, pinned formatting, changed-source Clang analyses,
architecture/isolation/coverage/specification checks, followed by native Cocoa/OpenGL
workflows in mm/in at observed 100/125/150/175/200% effective DPR scaling. Tests use marked
disposable roots; no user project or release recovery path is used.

Artifacts generated under:
`docs/testing/artifacts/core-modeling/cm9-review-20261122/verification/20261122T122725.874056Z/`
- Build, 120/120 enabled CTests, Pinned clang-format: all PASS.
- Changed-source Clang static analysis (12 files): PASS with 0 warnings.
- Architecture, Research-isolation, Coverage (257/257 CM requirements), Specifications: all PASS.
- Native workflows (100%, 125%, 150%, 175%, 200% DPR): all PASS with native widget interactions and screenshots captured.
- Status: `Pass: development-build scope; blocked acceptance remains separate`.
