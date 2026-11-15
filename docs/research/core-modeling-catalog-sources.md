# CM9 catalog acquisition register

Reviewed 22 October 2026. Status: candidates only. No tables, material values or
third-party generators have been imported into DatumForge. Broad standards
coverage and dataset redistribution remain open gates. This register does not
approve a library for bundling or claim its values meet a particular standard.

## Candidate sources

| Candidate | What was verified at the primary source | Unresolved before import |
|---|---|---|
| cq_warehouse | The [repository](https://github.com/gumyr/cq_warehouse) identifies Apache-2.0; [LICENSE](https://github.com/gumyr/cq_warehouse/blob/main/LICENSE) and the [fastener module header](https://raw.githubusercontent.com/gumyr/cq_warehouse/main/src/cq_warehouse/fastener.py) were inspected. Its [clearance CSV](https://raw.githubusercontent.com/gumyr/cq_warehouse/main/src/cq_warehouse/clearance_hole_sizes.csv) includes metric and inch size keys with Close/Normal/Loose choices. | Pin an immutable revision; establish each table's provenance, license applicability, units and edition; validate drill-code conversion and tolerances. These observations do not establish all ANSI/ISO/DIN/BSI/JIS/GB/GOST families, tapped/pipe-tapped variants or standards editions. No CadQuery runtime is proposed. |
| FreeCAD Fasteners Workbench | Its [license file](https://raw.githubusercontent.com/shaise/FreeCAD_FastenersWB/master/LICENSE) contains GPL version 2. The [contribution guide](https://raw.githubusercontent.com/shaise/FreeCAD_FastenersWB/master/CONTRIBUTING.md) describes FsData CSVs for fastener dimensions, lengths and ranges. | Determine license/provenance per table and compatibility with the intended distribution. Fastener head dimensions are not automatically Hole Wizard counterbore/countersink recommendations. Establish family/edition coverage and hole-specific semantics independently. No generator code or data imported. |
| FreeCAD Supplemental Materials | The [repository](https://github.com/FreeCAD/Supplemental-Materials) separates asset and code licenses. Its [material licensing policy](https://raw.githubusercontent.com/FreeCAD/Supplemental-Materials/Latest/Documentation/Licensing.md) requires source redistribution rights and says each material carries its own license. | Review each selected material, upstream source, attribution/share-alike obligations, density units, property temperature/conditions and version. A repository-level license is insufficient evidence for every entry. No materials imported. |

These are acquisition candidates, not an assertion that all content can be
redistributed. In particular, a standard's name, a public web page or a freely
downloaded manufacturer table is not evidence of redistribution permission.
The per-entry provenance requirement is DatumForge's implementation gate, not a
legal conclusion about every file in these projects.

## Required import record

For each admitted dataset, record source URL, immutable revision/archive hash,
license text and scope, copyright/attribution, source standard and edition,
covered families and sizes, units, tolerances, property conditions, validation
fixtures and explicit gaps. Keep dataset identity separate from standard identity.
Never label an absent edition as current or infer aliases between standards
without evidence. Rejected and pending sources remain visible in this register.

The project stores catalog version and resolved numeric values with accepted
features/material assignments. A catalog refresh may offer an explicit update;
it must not change existing projects by lookup. User custom libraries carry their
own provenance and are not silently added to the bundled catalog.

## Completion gaps

ANSI inch/metric, ISO, DIN, BSI, JIS, GB and GOST still need a per-family edition
and rights matrix, numerical verification and broad coverage. Material families
still need the corresponding property/provenance matrix. A small demonstration
table cannot close CM9.a or CM9.e. Independently authored custom-hole/material
interfaces can proceed while acquisition remains open.

The roadmap's [SolidWorks custom-material workflow](https://help.solidworks.com/2016/english/SolidWorks/sldworks/t_custom_material_creating.htm?id=20.1.2)
is a behavior reference only. The linked 2016 page could not be fetched during
this review. No SolidWorks material or Hole Wizard library is a data source.
The retained training review is private research and is excluded from shipping
catalogs and installers.
