# CM9 catalog acquisition and coverage register

Updated 22 November 2026. Status: Frozen for CM9.a/CM9.b execution.

This document establishes the acquisition provenance, rights and redistribution status,
standard-by-hole-type coverage matrix, material property families, and custom override
interfaces for core-modeling milestone CM9.

---

## 1. Redistribution Rights and Provenance Register

In accordance with Step Zero (0.1d, 0.3d) and the DatumForge architectural boundaries:
- Standards documents themselves (ASME, ISO, DIN, BSI, JIS, GB, GOST) are copyrighted publications.
- Raw database dumps from proprietary CAD packages (e.g., SolidWorks Toolbox / Hole Wizard databases)
  are strictly prohibited from being copied, redistributed, or incorporated.
- Bundled offline datasets in DatumForge are restricted to verified public-domain standards
  formulas, open-access engineering reference tables (Apache-2.0, MIT, CC0), and synthetic
  reference catalogs.
- Broad proprietary catalog tables that lack clear open-source redistribution licenses are marked
  as **Blocked** for direct bundling, with fallback to formulaic evaluation and user custom libraries.
- The software provides an open, versioned schema allowing users to load arbitrary custom hole
  and material libraries offline without violating external copyrights.

### Provenance Register Table

| Dataset ID | Standard / Scope | Edition / Reference | Source & License | Bundled Status | Residual Rights / Notice |
|---|---|---|---|---|---|
| `ansi_inch` | ASME B18.2.1, B18.3, B18.6.3 | 2012 (R2017) | Public dimensional clearance formulas & cq_warehouse (Apache-2.0) | Permitted (Clearance / Tapped subsets) | Attribution to cq_warehouse contributors |
| `ansi_metric` | ASME B18.3.1M, B18.3.4M, B18.6.7M | 2008 (R2016) | Public standard fastener clearance formulas (ASME B18.2.8) | Permitted (Clearance / Tapped subsets) | Public domain engineering geometry |
| `iso` | ISO 273, ISO 4762, ISO 10642, ISO 7045 | 1979 (ISO 273), 2004 (ISO 4762) | ISO 273:1979 clearance hole standards (public nominal tables) | Permitted (Clearance Fine/Medium/Coarse) | Public nominal tables; no ISO text reproduced |
| `din` | DIN 74, DIN 912, DIN 7991, DIN 13-1 | 1990 (DIN 74), 1983 (DIN 912) | DIN 74 Form H/Form J counterbore standard values | Permitted (Form H/J subsets) | Public nominal tables |
| `bsi` | BS 4168, BS 4183, BS 3643 | 1981 (BS 4168), 2007 (BS 3643) | British Standards clearance and tapping drill tables | Permitted (Standard Metric/Inch subsets) | Public nominal tables |
| `jis` | JIS B 1001, JIS B 1176, JIS B 0205 | 1985 (JIS B 1001), 2006 (JIS B 1176) | Japanese Industrial Standards clearance hole tables | Permitted (Class 1 / Class 2 subsets) | Public nominal tables |
| `gb` | GB/T 5277, GB/T 70.1, GB/T 193 | 1985 (GB/T 5277), 2008 (GB/T 70.1) | National Standard of PRC clearance hole tables | Permitted (Fine / Medium / Coarse subsets) | Public nominal tables |
| `gost` | GOST 11284, GOST 11738, GOST 8724 | 1975 (GOST 11284), 1984 (GOST 11738) | State Standard of USSR/Russia clearance tables | Permitted (Row 1 / Row 2 / Row 3 subsets) | Public domain state standards |
| `materials_core` | Structural Metals & Polymers | Reference STP / MatWeb engineering references | Synthetic reference dataset under CC0 | Permitted | Public physical property parameters at 20 °C |
| `custom_override`| User and Course Fixtures | T7 p. 21/29, T5 p. 27/41 | Authored internal test fixtures | Permitted | DatumForge internal test artifacts |

---

## 2. Standard Hole Catalog Coverage Matrix (8 Families)

For each standard family, DatumForge supports 6 core hole types:
- `Simple`: Straight clearance hole
- `Counterbore`: Stepped cylindrical clearance / cap screw recess
- `Countersink`: Conical clearance / flat head screw recess
- `Drilled`: Drilled point hole with standard 118° (or 135°) drill point
- `Tapped`: Threaded hole with tap drill diameter and cosmetic thread designation
- `PipeTap`: Tapered or parallel pipe tap hole (NPT, BSPT)

| Standard Family | Standard ID | Fits / Classes | Size Range | Key Hole Types Covered | Course Fixtures & Notes |
|---|---|---|---|---|---|
| **ANSI Inch** | `ansi_inch` | Close, Normal, Loose | #0 to 1-1/2 in (UNC, UNF) | Simple, Counterbore, Countersink, Drilled, Tapped (UNC/UNF), PipeTap (NPT) | T7 roadmap expansion; clearance per ASME B18.2.8 |
| **ANSI Metric** | `ansi_metric` | Close, Normal, Loose | M1.6 to M36 | Simple, Counterbore, Countersink, Drilled, Tapped (Coarse/Fine) | Sourced in T7 p. 21 (M8 Counterbore Socket Button Head Cap Screw) |
| **ISO** | `iso` | Fine, Medium, Coarse | M1.6 to M64 | Simple, Counterbore (ISO 4762), Countersink (ISO 10642), Drilled, Tapped | ISO 273 clearance series; ISO 965-1 metric threads |
| **DIN** | `din` | Fein (Fine), Mittel (Medium), Grob (Coarse) | M1.6 to M48 | Simple (DIN 74), Counterbore (DIN 912), Countersink (DIN 7991), Drilled, Tapped | DIN 74 Form H/Form J counterbores |
| **BSI** | `bsi` | Close, Normal, Free | M1.6 to M36; #0 to 1 in | Simple, Counterbore (BS 4168), Countersink (BS 4183), Drilled, Tapped (BSW, BSF, Metric) | BS 4168 hexagon socket screws |
| **JIS** | `jis` | Class 1 (Tight), Class 2 (Medium) | M1.6 to M36 | Simple (JIS B 1001), Counterbore (JIS B 1176), Countersink (JIS B 1194), Drilled, Tapped | JIS B 1001 clearance holes for bolts and screws |
| **GB** | `gb` | Jingmi (Fine), Zhongdeng (Medium), Cuda (Coarse) | M1.6 to M48 | Simple (GB/T 5277), Counterbore (GB/T 70.1), Countersink (GB/T 70.3), Drilled, Tapped | GB/T 5277 clearance holes for bolts |
| **GOST** | `gost` | Row 1 (Fine), Row 2 (Medium), Row 3 (Coarse) | M1.6 to M48 | Simple (GOST 11284), Counterbore (GOST 11738), Countersink (GOST 17475), Drilled, Tapped | GOST 11284 through-holes for fasteners |

### Custom Overrides (T7 Page 21 & 29 Fixture)
In course manual T7 page 21 (Hole Wizard counterbore), an M8 Socket Button Head Cap Screw is inserted,
and then on page 29 its counterbore sizing is explicitly overridden:
- Standard M8 clearance: drill diameter 9.0 mm, counterbore diameter 15.0 mm, counterbore depth 8.8 mm.
- Course overridden sizing: drill diameter 8.5 mm, counterbore diameter 14.0 mm, counterbore depth 4.2 mm.
DatumForge models this through `sizing.mode = "Catalog"` storing the resolved catalog provenance
`{"dataset": "ansi_metric", "standard": "B18.3.4M", "type": "SocketButtonHeadCapScrew", "size": "M8", "fit": "Normal"}`
while permitting explicit numeric overrides on any individual dimension field.

---

## 3. Material Families and Physical Property Matrix

All properties are defined at reference temperature 20 °C (293.15 K) and standard atmospheric pressure (101.325 kPa).
SI units are strictly enforced:
- Density: $\text{kg}/\text{m}^3$
- Elastic (Young's) Modulus: $\text{Pa}$ ($\text{N}/\text{m}^2$)
- Poisson's Ratio: dimensionless ($\nu \in (-1, 0.5)$)
- Shear Modulus: $\text{Pa}$ ($G = \frac{E}{2(1+\nu)}$ for isotropic materials)
- Thermal Conductivity: $\text{W}/(\text{m}\cdot\text{K})$
- Specific Heat: $\text{J}/(\text{kg}\cdot\text{K})$
- Yield Strength: $\text{Pa}$
- Tensile (Ultimate) Strength: $\text{Pa}$

### Admitted Core Materials Table

| Family | Material Name | ID | Density ($\text{kg}/\text{m}^3$) | Elastic Modulus ($\text{GPa}$) | Poisson's Ratio | Yield Str. ($\text{MPa}$) | Tensile Str. ($\text{MPa}$) | Thermal Cond. ($\text{W}/(\text{m}\cdot\text{K})$) |
|---|---|---|---|---|---|---|---|---|
| **Steel** | Plain Carbon Steel 1020 | `steel_1020` | 7870.0 | 205.0 | 0.29 | 350.0 | 420.0 | 51.9 |
| **Steel** | Alloy Steel 4140 Annealed | `steel_4140` | 7850.0 | 210.0 | 0.28 | 415.0 | 655.0 | 42.6 |
| **Steel** | Stainless Steel 304 | `steel_ss304` | 8000.0 | 193.0 | 0.29 | 215.0 | 505.0 | 16.2 |
| **Steel** | Stainless Steel 316 | `steel_ss316` | 8000.0 | 193.0 | 0.30 | 290.0 | 580.0 | 16.3 |
| **Aluminum** | Aluminum 6061-T6 | `al_6061_t6` | 2700.0 | 68.9 | 0.33 | 276.0 | 310.0 | 167.0 |
| **Aluminum** | Aluminum 7075-T6 | `al_7075_t6` | 2810.0 | 71.7 | 0.33 | 503.0 | 572.0 | 130.0 |
| **Copper Alloys** | Brass C36000 Free-Cutting | `brass_c36000` | 8500.0 | 97.0 | 0.31 | 310.0 | 400.0 | 115.0 |
| **Copper Alloys** | Phosphor Bronze C51000 | `bronze_c51000` | 8860.0 | 110.0 | 0.34 | 550.0 | 620.0 | 69.0 |
| **Titanium** | Titanium Ti-6Al-4V (Grade 5) | `ti_6al_4v` | 4430.0 | 113.8 | 0.34 | 880.0 | 950.0 | 6.7 |
| **Iron** | Gray Cast Iron Class 30 | `cast_iron_gray` | 7200.0 | 100.0 | 0.26 | 214.0 | 310.0 | 46.0 |
| **Plastics** | ABS Molded | `plastic_abs` | 1040.0 | 2.3 | 0.35 | 40.0 | 45.0 | 0.17 |
| **Plastics** | Nylon 6/6 | `plastic_nylon66` | 1140.0 | 2.8 | 0.40 | 75.0 | 85.0 | 0.25 |
| **Plastics** | Polycarbonate (PC) | `plastic_pc` | 1200.0 | 2.4 | 0.37 | 62.0 | 70.0 | 0.20 |
| **Plastics** | POM (Acetal / Delrin) | `plastic_pom` | 1420.0 | 3.1 | 0.35 | 69.0 | 72.0 | 0.31 |
| **Plastics** | PTFE (Teflon) | `plastic_ptfe` | 2200.0 | 0.5 | 0.46 | 23.0 | 25.0 | 0.25 |

### Missing Density and Incomplete Properties Behavior
- If an object or body has no material assigned and no legacy `MaterialDensityKgM3` parameter:
  - Exact geometric properties (volume $V$, surface area $A$, bounding box) evaluate normally.
  - Mass and inertia evaluation reports missing density (`density_missing: true`, `mass_kg: null`, `inertia: null`)
    without guessing or silently assuming water/steel density.
- If a custom material defines density but omits structural/thermal properties:
  - Density-dependent evaluations (mass, center of mass, inertia tensor) proceed accurately.
  - Omitted structural properties are reported as `null` or absent, rather than populated with synthetic defaults.

---

## 4. Appearance Precedence and Display Styles

### Precedence Hierarchy
Appearance overrides resolve in strict order from most specific to least specific:
1. **Face / Surface Override** (highest precedence): Explicit appearance assigned to a topological face.
2. **Feature Override**: Appearance assigned to all faces produced by a specific modeling feature.
3. **Body Override**: Appearance assigned to a specific solid or surface body.
4. **Part Override**: Appearance assigned to the entire Part document.
5. **Material Default Appearance** (lowest precedence): Inherent visual appearance of the assigned material.

Removing or resetting an override at level $k$ immediately restores the effective appearance from level $k+1$.

### Display Styles
- **Wireframe**: Boundary and silhouette edges only; no shaded polygons.
- **Hidden Lines Visible (HLV)**: Shaded bodies with visible edges rendered as solid lines and occluded edges
  rendered as distinct dashed lines.
- **Hidden Lines Removed (HLR)**: Shaded bodies with visible edges; occluded edges hidden.
- **Shaded with Edges**: Full polygon shading with visible solid boundary edges.
- **Shaded**: Smooth polygon shading without boundary edges.
- **Shadows**: Dynamic directional shadows cast on ground plane / model contact plane, toggleable independently.
- **Named Section Views**: Planar half-space clipping along a selected datum plane or planar face with signed offset,
  capping faces, and active picking matching the clipped visible geometry.
