# Decision: Core-Modeling CM9 Semantic Contracts

## Status
Accepted — 22 November 2026.

## Context
Core-modeling milestone CM9 delivers standard and custom hole sizing catalogs, material assignment,
mass and section properties evaluation, appearance overrides, display styles, and named modeling sections
across 30 coverage register rows (CM-228 through CM-257) sourced from course manuals T5 *Lesson 1 / Lesson 2*
(pages 27–29, 34–35, 41–42) and T7 *Lesson 1 / Lesson 2* (pages 7–8, 21, 29, 33–34).

Following the architectural separation established across Modules 1–10 and CM1–CM8:
- The desktop UI owns semantic drafts, user inputs, camera navigation, and display meshes.
- The CAD worker owns exact OpenCASCADE evaluation, physical/geometric property calculation, and authoritative model writes.
- SI units (meters, radians, kilograms, seconds) are preserved across all contracts.
- Missing density reports explicit diagnostics without guessing defaults.

---

## Semantic Schema

### 1. Hole Catalog Resolution Contract

A catalog query resolves standard fastener criteria into concrete SI dimensions that feed the existing CM6 `Hole` feature intent.

#### Catalog Query Request (`kind: "HoleCatalogQuery"`)
- `version`: integer 1.
- `dataset`: string identifier of the standard family (`"ansi_inch"`, `"ansi_metric"`, `"iso"`, `"din"`, `"bsi"`, `"jis"`, `"gb"`, `"gost"`, `"custom"`).
- `standard`: standard specification string (e.g., `"B18.3.4M"`, `"ISO 4762"`, `"DIN 912"`).
- `type`: fastener or hole type (e.g., `"SocketButtonHeadCapScrew"`, `"HexBolt"`, `"SocketHeadCapScrew"`).
- `size`: nominal size designation (e.g., `"M8"`, `"1/4-20"`, `"#10-24"`).
- `fit`: clearance fit class (e.g., `"Close"`, `"Normal"`, `"Loose"`, `"Fine"`, `"Medium"`, `"Coarse"`, `"Class 1"`, `"Class 2"`).
- `custom_overrides`: optional map of explicit dimension overrides (e.g., `{"diameter": 0.0085, "counterbore_diameter": 0.014, "counterbore_depth": 0.0042}`).

#### Catalog Resolution Result (`kind: "HoleCatalogResolved"`)
- `valid`: boolean flag.
- `dataset`: string.
- `standard`: string.
- `type`: string.
- `size`: string.
- `fit`: string.
- `family`: resolved CM6 hole family (`"Simple"`, `"Counterbore"`, `"Countersink"`, `"Drilled"`, `"Tapped"`, `"PipeTap"`).
- `diameter`: clearance or tap drill diameter in meters ($> 0$).
- `counterbore_diameter`: optional counterbore diameter in meters.
- `counterbore_depth`: optional counterbore depth in meters.
- `countersink_diameter`: optional countersink outer diameter in meters.
- `countersink_angle`: optional countersink included angle in radians.
- `drill_angle`: optional drill point angle in radians (default $118^\circ = 2.0594885$ rad).
- `pitch`: optional thread pitch in meters for tapped holes.
- `cosmetic_thread`: optional boolean indicating whether cosmetic thread should be generated.
- `thread_designation`: optional thread callout string (e.g., `"M8x1.25"`).

If the dataset/standard/type/size/fit combination is unsupported, resolution fails with `UnsupportedOperation` or reports `valid: false` with an actionable `diagnostic` string.

---

## 2. Materials and Physical Properties Contract

### Material Definition (`kind: "Material"`)
- `version`: integer 1.
- `id`: unique UUID or stable identifier (e.g., `"mat_steel_1020"`).
- `name`: human-readable display name (e.g., `"Plain Carbon Steel 1020"`).
- `family`: material family category (`"Steel"`, `"Aluminum"`, `"Copper"`, `"Titanium"`, `"Iron"`, `"Plastics"`, `"Custom"`).
- `properties`: map of SI physical parameters:
  - `density_kg_m3`: material density in $\text{kg}/\text{m}^3$ ($> 0$, $\le 10^6$). Mandatory for mass/inertia calculations.
  - `elastic_modulus_pa`: optional Young's modulus in $\text{Pa}$ ($> 0$).
  - `poissons_ratio`: optional Poisson's ratio ($-1 < \nu < 0.5$).
  - `shear_modulus_pa`: optional shear modulus in $\text{Pa}$ ($> 0$).
  - `thermal_conductivity_w_mk`: optional thermal conductivity in $\text{W}/(\text{m}\cdot\text{K})$ ($> 0$).
  - `specific_heat_j_kgk`: optional specific heat capacity in $\text{J}/(\text{kg}\cdot\text{K})$ ($> 0$).
  - `yield_strength_pa`: optional yield strength in $\text{Pa}$ ($> 0$).
  - `tensile_strength_pa`: optional ultimate tensile strength in $\text{Pa}$ ($> 0$).
- `appearance`: optional default appearance definition for the material (diffuse color, roughness, metalness).

### Material Assignment
- Scope: Part-level default assignment or Body-level specific override.
- Body override takes precedence over Part-level material for that body.
- Missing density behavior: If no density is assigned to a body or part, geometric evaluation (volume, surface area, centroid) succeeds, while mass and inertia evaluation sets `density_missing: true` and omits mass/inertia values without guessing a fallback density.

---

## 3. Exact Mass Properties Evaluation Contract

Evaluates the physical mass and inertia properties from exact OpenCASCADE solid bodies.

### Evaluation Request (`command: "EvaluateMassProperties"`)
- `revision`: integer current document revision.
- `scope`: evaluation scope:
  - `type == "Part"`: all solid bodies in the active part.
  - `type == "Body"`: single solid body identified by `body` UUID.
  - `type == "AssemblyOccurrence"`: occurrence identified by `occurrence` string, with accepted rigid transform applied.
- `frame`: reference coordinate system:
  - `"Part"`: part coordinate origin $(0, 0, 0)$.
  - `"CenterOfMass"`: evaluated center of mass $\mathbf{c}$.

### Evaluation Result (`kind: "MassPropertiesResult"`)
- `revision`: integer revision.
- `valid`: boolean flag.
- `solids_count`: integer count of evaluated solid bodies ($> 0$ for solid result).
- `volume_m3`: exact volume in $\text{m}^3$.
- `surface_area_m2`: exact total boundary surface area in $\text{m}^2$.
- `density_kg_m3`: effective density in $\text{kg}/\text{m}^3$ (null if missing).
- `density_missing`: boolean flag indicating whether density was omitted.
- `mass_kg`: total mass in $\text{kg}$ (null if density is missing).
- `center_of_mass_m`: 3D coordinates $[x, y, z]$ in meters.
- `inertia_tensor_kg_m2`: symmetric $3 \times 3$ matrix or 6 components $[I_{xx}, I_{yy}, I_{zz}, I_{xy}, I_{xz}, I_{yz}]$ in $\text{kg}\cdot\text{m}^2$:
  $$I_{xx} = \int (y^2 + z^2) \rho \, dV, \quad I_{xy} = -\int x y \rho \, dV$$
- `principal_moments_kg_m2`: 3 principal moments of inertia $[I_1, I_2, I_3]$ in ascending order ($I_1 \le I_2 \le I_3$).
- `principal_axes`: $3 \times 3$ orthonormal matrix $[\mathbf{v}_1, \mathbf{v}_2, \mathbf{v}_3]$ where $\mathbf{v}_i$ is the unit eigenvector for $I_i$.
  - Invariance handling: When eigenvalues repeat (e.g. spherical or cubic symmetry where $I_1 = I_2 = I_3$), any orthonormal basis spanning the invariant subspace is valid and numerically stable.
- If scope contains only non-solid surface bodies, `solids_count` is 0, `volume_m3` is 0, and `valid` is false with `reason: "Non-solid surface bodies have no volume or mass"`.

---

## 4. Exact Planar Section Properties Evaluation Contract

Evaluates 2D cross-sectional properties for planar sketch regions or planar solid faces.

### Section Evaluation Request (`command: "EvaluateSectionProperties"`)
- `revision`: integer revision.
- `target`:
  - `type == "Sketch"`: planar sketch contour (`sketch` UUID, `mode` in `{"AllClosed", "SelectedRegions"}`).
  - `type == "Face"`: planar solid face reference `GeometryReference(Face)`.

### Section Evaluation Result (`kind: "SectionPropertiesResult"`)
- `revision`: integer revision.
- `valid`: boolean flag.
- `area_m2`: total cross-sectional area in $\text{m}^2$.
- `perimeter_m`: total perimeter of all outer and inner boundary loops in meters.
- `centroid_local_m`: 2D centroid $[u_c, v_c]$ in sketch/section plane coordinates.
- `centroid_part_m`: 3D centroid $[x_c, y_c, z_c]$ in Part global coordinates.
- `second_moments_m4`: centroidal second moments of area in $\text{m}^4$:
  $$I_{uu} = \iint (v - v_c)^2 \, du\,dv, \quad I_{vv} = \iint (u - u_c)^2 \, du\,dv, \quad I_{uv} = -\iint (u - u_c)(v - v_c) \, du\,dv$$
- `polar_moment_m4`: polar moment of area $J = I_{uu} + I_{vv}$ in $\text{m}^4$.
- `principal_moments_m4`: principal second moments $[I_1, I_2]$ in $\text{m}^4$:
  $$I_{1,2} = \frac{I_{uu} + I_{vv}}{2} \pm \sqrt{\left(\frac{I_{uu} - I_{vv}}{2}\right)^2 + I_{uv}^2}$$
- `principal_angle_rad`: angle $\theta$ in radians between the local $u$-axis and the first principal axis.
- Boundary handling:
  - Nested inner loops are treated as cutouts (holes) and subtracted from area and moments.
  - Multiple disjoint closed loops sum their areas and apply the 2D parallel-axis theorem to combine moments.
  - Open curve chains, self-intersecting loops, or non-planar faces throw `UnsupportedOperation` or report `valid: false`.

---

## 5. Appearance Precedence and Overrides Contract

Appearance defines the visual rendering attributes applied to the model in the desktop viewport.

### Appearance Specification (`kind: "Appearance"`)
- `color_rgba`: array of 4 normalized floats $[r, g, b, a] \in [0, 1]^4$.
- `roughness`: float $\in [0, 1]$ (default 0.5).
- `metalness`: float $\in [0, 1]$ (default 0.0).
- `transparency`: float $\in [0, 1]$ (default 0.0, where 0 is fully opaque).

### Appearance Override Record (`kind: "AppearanceOverride"`)
- `id`: unique UUID.
- `target`: typed reference identifying the entity being styled:
  - `level == "Face"`: `GeometryReference(Face)` (role `"Face"`, stable topological token).
  - `level == "Feature"`: feature UUID.
  - `level == "Body"`: body UUID.
  - `level == "Part"`: part-wide appearance.
- `appearance`: Appearance specification.

### Precedence Resolution Rule
When rendering a triangle or face in the viewport, the effective appearance resolves in strict order:
$$\text{Face Override} \succ \text{Feature Override} \succ \text{Body Override} \succ \text{Part Override} \succ \text{Material Default}$$
Removing an override at any level immediately restores the appearance determined by the lower-precedence levels.

---

## 6. Named Section Views Contract

Named section views define view-only cutting planes for model inspection.

### Section View Record (`kind: "SectionView"`)
- `version`: integer 1.
- `id`: unique UUID.
- `name`: string name (e.g., `"Section View A"`, `"Transverse Cut"`).
- `active`: boolean flag indicating whether this section view is currently active.
- `support`: reference to the cutting plane definition:
  - `GeometryReference` (role `"Plane"` referencing a datum plane, or role `"Face"` referencing a planar solid face).
- `offset`: signed distance offset in meters along the plane normal vector.
- `flip`: boolean flag to flip the retained half-space direction.
- `cap_color_rgba`: optional RGBA color for the section capping plane.
- `clip_mode`: `"ViewOnly"`. The cutting plane clips only the visual mesh representation in the viewport; exact BRep geometry, modeling features, and history are strictly unmodified.
- Associativity: If the underlying datum plane or planar face moves or re-solves, the section view plane follows the updated support frame.
