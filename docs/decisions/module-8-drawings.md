# Module 8 drawing contracts

Authorized 22 October 2026 with Modules 9 and 10; stop before Module 11.
Prior Module 6.1b/6.4c and 7.4c gates stay Blocked. The single worker owns exact
geometry, projection, authoritative transactions and PDF publication. The UI owns
only immutable vectors and sheet display, and uses the same kernel-free painter.

`drawing-v1` is an additive authoritative manifest in the existing strict v1 SQL
schema. One validated collection per Drawing properties entity is stored in
`drawing_views`; `source_id` identifies the owner document, while the versioned
collection holds actual source/view/annotation IDs. Undo/redo uses semantic deltas.
Old builds reject the new manifest; no unsupported geometry is interpreted silently.

Sheets use physical mm, explicit SI display units, a numeric model-to-paper scale,
first/third-angle projection, up to eight views and thirty annotations per sheet.
Front looks down +Z with +X right; Top looks along -Y; Right along +X; Isometric
uses (1,-1,1). Projected Top/Right views align to an earlier Front base and obey the
sheet projection convention. Independent orthographic/isometric base views are
also supported. Source revision is the maximum monotonic document revision in
the recursive source closure, including suppressed-component structural edits.
Unrelated project edits do not change it. BOM has a separate revision.

Projection uses pinned OCCT 7.9.2 `HLRBRep_Algo` and `HLRBRep_HLRToShape`, following
the [upstream hidden-line algorithm contract](https://github.com/Open-Cascade-SAS/OCCT/blob/V7_9_2/dox/user_guides/modeling_algos/modeling_algos.md).
Visible sharp/outline and optional hidden sharp/outline edges are vectorized with
0.01 mm maximum requested sheet chord deviation; these are vector polylines,
not exact analytic PDF curves. Raster fallback is absent. Views exceeding the
printable area fail visibly and cannot be exported as current. Point/control
admission is bounded; arbitrary industrial drawings are not accepted implicitly.

Linear dimensions measure straight exact edges. Angular dimensions use two straight
edges in the same Part body; radius/diameter/center marks use circular edges.
Source geometry is measured in SI independently of placement and display override.
An override always carries a measured-value disclosure. Part view anchors use
body ID and topology fingerprint, never edge array indices. Changed/deleted or
ambiguous tokens become Dangling. The user replaces an anchor through the draft
editor; replacement is undoable. Assembly-view dimensions and arbitrary point-to-
point dimensions are currently unsupported and must remain explicit limitations.
Text can use physical sheet anchors.

BOM is a flattened leaf-Part list keyed by stable document ID; suppressed branches
are excluded, hidden and unloaded occurrences included. Exact projection resolves
unloaded definitions on demand without changing their load flags; hidden geometry
is excluded from drawing paths. Current/resolved geometry is required.

Display reevaluates geometry and reports Stale until an accepted UpdateDrawing
records source revisions. Updating never silently reassigns annotation references.
PDF and complete portable copies reject stale/unresolved/dangling drawings.
Portable copies include all local documents and retained authoritative sources,
new project identity, stable internal IDs, and complete closure manifest.

B5 v1 is preserved unchanged. The additive `B5-layout-v2.json` records the fit issue:
full B3 cannot fit its original A4/1:2 recipe. B1 remains 1:2 (80 mm -> 40 mm);
the assembly sheet uses 3:40. This changes fixture presentation, not the SRS B5
counts or physical 8 GB/performance budgets. No platform/resource gates close here.
