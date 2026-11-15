# Module 8 — associative drawings, annotations, BOM and vector PDF

Date: 22 October 2026. Explicitly authorized with Modules 9 and 10, in order;
stop before Module 11. Topic branch `feature/modules-8-10-drawings-hardening` retains
all preexisting Module 5–7 changes. No commit/push/history rewrite is authorized.
Module 6.1b/6.4c and 7.4c remain deferred and Blocked. Independent exact projection
and local closure work uses the verified available-Mac services, without inheriting
full platform/resource acceptance.

Implementation: [drawing contracts](../decisions/module-8-drawings.md). The worker
owns exact HLR, measured annotations, source revisions, transactions and PDF
publication; the native desktop owns vector display. Sheets support units,
size/scale, first/third-angle placement, base/projected/isometric views, optional
hidden lines, dimensions, center marks, text and a flattened stable-ID BOM.
Missing/ambiguous anchors remain Dangling; source edits mark views/BOM Stale.
Explicit anchor replacement retains annotation identity and is undoable.
PDF and complete portable closure require current, resolved, repaired drawings.

## Reproduction and verification scope

```sh
python3 tools/verify_modules8_10.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

- `module8.geometry`: analytic cylinder radius/diameter/center/linear/angular
  measurements, measured-value disclosure under a text override, real changed
  topology, deleted sources, invalid/nonfinite definitions.
- `module8.drawing`: actual B1 including fillet and frozen 100-occurrence/10-definition/
  99-mate B3; two sheets, five exact views, 80 mm measured dimension and an actual
  40 mm vector at 1:2, ten BOM rows totaling 100, hidden lines, vector PDF, save,
  portable copy without original saved file, restart, source revisions, dangling,
  repair/undo/redo and destination preservation on stale export refusal.
- `module8.contracts`: native panel and real worker creation, noncommit preview,
  accept/cancel, text, PDF, worker recovery, source update and current/stale states.
- `nativeDrawing`: Cocoa workflow and screenshots at five effective scales in
  the final runner. Headless tests alone do not certify graphics/focus.

Original B5 v1 is preserved. The additive `tests/fixtures/B5-layout-v2.json` records
that the full B3 cannot fit A4 at the old 1:2 recipe. B1 remains 1:2; the assembly
sheet uses 3:40. Page bounds are enforced rather than silently clipping views.
Qt emits nominal A4 at 842×595 PDF points (whole-point page-box rounding, under
0.5 point); model-to-paper vectors retain their tested physical scale. Paths are
vector polylines with requested 0.01 mm sheet chord error; no raster fallback.

Focused tests passed; final available-Mac verification is recorded below. Development
logs, including corrected integration/fixture failures, remain in
`artifacts/module-8/dev`. The PDFKit procedure checks page geometry, extractable
text and absence of raster images and renders both actual pages for visual review.

## Remaining acceptance limits

Assembly-view dimensions, arbitrary point-to-point dimensions, selected-subgraph
copies, arbitrary industrial projections and a full drafting standard are not
claimed. Part-view supported dimensions and all-local portable projects are the
implemented scope. Physical device, assistive-technology, Windows/Linux/minimum
macOS/other GPU and installed builds remain Blocked. Real 8 GB full physical/
shared/GPU/FPS budgets remain Module 11; it is not started.


## Final available-Mac verification

Run `20261022T031119.905623Z` on macOS-26.6.2-arm64-arm-64bit-Mach-O / Apple M2 /
**8,589,934,592 physical RAM bytes (8 GiB)**. Branch
`feature/modules-8-10-drawings-hardening`, commit `d1fce435b1e9e4f05b7f36fab6392f870fedd8d7` plus all preserved dirty inputs.
Aggregate tested source SHA-256: `eb6225b47536ec9bcc9a864d10dc132b55de935857c2a9ea831cb26e8d13b0de`.

[Verification](artifacts/module-10/runs/20261022T031119.905623Z/verification.json),
[source identity](artifacts/module-10/runs/20261022T031119.905623Z/source-identity.json),
[complete CTest](artifacts/module-10/runs/20261022T031119.905623Z/ctest.txt),
[architecture](artifacts/module-10/runs/20261022T031119.905623Z/architecture.json),
[offline](artifacts/module-10/runs/20261022T031119.905623Z/offline/offline.json),
[visual review](artifacts/module-10/runs/20261022T031119.905623Z/visual-review.json), and
[clean reproduction](artifacts/module-10/runs/20261022T031119.905623Z/clean-reproduction-details.txt)
record exact commands and outcomes. **All 52 enabled CTest cases passed**; the
intentional failure probe remains Disabled. Configure/build, formatting/static
analysis, ownership/research isolation, verified network-denial workflows and
frozen corpora, five-scale Cocoa drawing/keyboard checks, prior native Modules 1–7
and fresh source application reproduction passed. Dependency archives/pins and
existing dependency binaries were reused; dependencies were not rebuilt and no
installed release is certified.

[Actual B5 PDF](artifacts/module-10/runs/20261022T031119.905623Z/module8-B5.pdf) and its
[PDFKit inspection](artifacts/module-10/runs/20261022T031119.905623Z/pdf/pdf-inspection.json)
are separate from screenshots. [Exact CAD crash matrix](artifacts/module-10/runs/20261022T031119.905623Z/module10-fault-matrix.json)
has 29 SIGKILL cases (27 exact CAD and two PDF publication cuts); [prior storage matrix](artifacts/module-10/runs/20261022T031119.905623Z/module2-fault-matrix.json)
has 35 cases. Earlier development failures and superseded verification runs are retained. No failed/unrun check is promoted to full acceptance.

Deferred **6.1b/6.4c/7.4c remain Blocked**, unchanged. Full native/platform/device/
assistive-technology/physical-resource/filesystem/installed acceptance remains
Blocked or Not run as stated above. **Stop before Module 11. No commit or push.**

[Final evidence consistency](artifacts/module-10/runs/20261022T031119.905623Z/evidence-consistency.json) and
[documentation structure check](artifacts/module-10/runs/20261022T031119.905623Z/spec-validation.json) passed. The latter
checks documentation structure and links only, not product acceptance.
