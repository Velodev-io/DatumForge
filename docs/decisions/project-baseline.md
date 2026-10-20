# Module 0 project baseline

Recorded 19 October 2026, tasks 0.1a–0.1d. Workspace and repository root:
`/Users/binova/Documents/Projects/Suru/Solidworks Clone`.

“Solidworks Clone” is provisional. No final name, stable application ID, product
license, remote destination, or public distribution is selected. Development
identity is `local.solidworks-clone.dev`; tests use `local.solidworks-clone.test`.
Neither is a registered release identity. Do not configure an invented remote.

The frozen requirements remain SRS v0.1: native C++20/Qt desktop plus one local
worker, macOS/Windows/Linux candidates, 8 GB **physical system RAM**, local SSD,
offline core, one project writer, SI semantic units and millimeter kernel units.
R1 includes parts, assemblies, drawings, exchange and recovery. R2 exclusions in
SRS §3 remain excluded. The old 4 GB target is superseded. No budget or scope
requirement is relaxed by a feasibility result.

## Evidence provenance (0.1d)

| Evidence | Source | Boundary |
|---|---|---|
| Public concepts | [Handbook](../../research/SOLIDWORKS_KNOWLEDGE_HANDBOOK.md) | Public documentation; no recovered private code/schema |
| Inspected screenshots | [Catalog](../../research/ui-atlas/UI_SCREEN_CATALOG.md), [notes](../../research/ui-atlas/UI_DESIGN_NOTES.md), [atlas](../../research/ui-atlas/index.html) | Publisher artwork is reference material, never app assets |
| Installer observations | [Inventory](../../research/local-evidence/installer_inventory.json) | Static observations only, no full CAD installation run |
| Proposed design | [SDD](../SDD.md) | Untested until task-specific evidence exists |

Preservation is checked against [SHA-256 baseline](../testing/artifacts/module-0/research-baseline.json).

## Open decisions (0.1c)

Owners are roles, not invented people. A later-module owner does not close a gate.

| ID | Owner | Status / blocking scope | Evidence needed |
|---|---|---|---|
| OD-01 | Product owner | Unresolved; release identity/license blocked | Final name, application ID, product license and distribution route |
| OD-02 | Build maintainer | Mac compatibility verified; other platform pins/validation Blocked | Immutable sources/hashes, compatibility builds and native OS matrix |
| OD-03 | Sketch maintainer | Initial extraction/diagnostics feasible; Module 3 production integration pending | Pinned PlaneGCS extraction, residual/drag/conflict corpus and notices |
| OD-04 | Renderer maintainer | Unresolved; Mac viewport gated | Native GPU/picking/scale/resize/driver results, future Metal route |
| OD-05 | Assembly maintainer | Initial three-body numerical feasibility passed; Module 7 breadth pending | Grounding, rank/conflicts/cycles/rotation and cancel corpus |
| OD-06 | Engine maintainer | Provisional caps; Modules 0/11 | Measured admission/pressure limits against unchanged SRS budgets |
| OD-07 | Validation maintainer | Physical 8 GB Mac available; other reference hardware Blocked | Real 8 GB host per published target, exact hardware and raw measurements |
| OD-08 | UI validation maintainer | Unresolved; native/release gates blocked | Keyboard, assistive technology, scaling, installed GUI automation matrix |
