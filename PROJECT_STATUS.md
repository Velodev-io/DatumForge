# DatumForge — Project Status

Living checklist maintained across sessions.
Format: `[ ]` not started, `[/]` in progress, `[x]` done, `[~]` deferred/blocked.

Last Updated: 2026-11-14

---

## 1. Core Modules (M0–M10 Completed on Mac; M11–M12 Out of Scope)

- [x] **Module 0: Step Zero & Feasibility** — Research baseline, toolchains, isolated feasibility experiments.
- [x] **Module 1: Architecture & IPC** — Native desktop shell (Qt 6) + dedicated CAD worker process (OCCT/PlaneGCS) over local IPC.
- [x] **Module 2: Document Model & Persistence** — SQLite document store, atomic commits, revisions, undo/redo, crash recovery.
- [x] **Module 3: 2D Sketching & Constraint Solving** — Planar sketches, PlaneGCS geometric constraints, equations, direct-editing repair.
- [x] **Module 4: Modeling Workspace & Solid Features**
  - [x] **Module 4.0:** Command tabs (Features/Sketch/Evaluate), left PropertyManager/Feature Tree, viewport chrome.
  - [x] **Module 4.1+:** OCCT solid features (extrude, revolve, fillets, chamfers, draft, boolean cut/join, measurement).
- [x] **Module 5: History DAG & Reference Repair** — Parameter dependency graph, upstream topology tracking, explicit reference repair.
- [x] **Module 6: Neutral CAD Exchange** — STEP, IGES, STL, OBJ export/import and portable Part copies.
- [~] **Module 6.1b / 6.4c:** Non-Mac platform exchange & installed release packaging *(Deferred / Blocked)*.
- [x] **Module 7: Assemblies & Mates** — Multi-part assembly hierarchy, rigid mates, transform solver, interference detection.
- [~] **Module 7.4c:** Physical 8 GB assembly stress validation *(Deferred / Blocked)*.
- [x] **Module 8: 2D Drawings** — Standard orthographic views, section views, detail projections, dimension display.
- [x] **Module 9: Unified Interaction & Polish** — Cross-document shortcut consistency, UI styling, high-DPI handling.
- [x] **Module 10: Hardening & Fault Tolerance** — Headless validation suite, corrupted-file recovery, IPC disconnect resilience.
- [ ] **Module 11: Advanced Surfaces, Sheet Metal & Weldments** *(Not started; requires explicit user authorization)*.
- [ ] **Module 12: Production Packaging & Release** *(Not started)*.

---

## 2. Core-Modeling Expansion (CM0–CM9, T2–T8 Syllabus Alignment)

- [x] **CM0: Baseline & Feasibility** — Freeze page-indexed coverage, analytical fixtures, verification scripts.
- [x] **CM1: Sketch Primitives & Relations** — Rectangles, slots, conics, splines, Pierce cross-sketch relations.
- [x] **CM2: Sketch Editing**
  - [x] Straight-line trim (to closest, inside/outside) & dynamic extend slice.
  - [x] Power trim, corner trim, sketch fillet/chamfer, offset entities, sketch transforms & patterns.
- [x] **CM3: Dimensioning & Driving Geometry** — Driving/reference dimensions, true path length, grouped ordinates, constrained drag, Fully Define proposals.
- [x] **CM4: Datums & Associative Sketch Support**
  - [x] CM4.a: Typed geometry references & dependency validation.
  - [x] CM4.b: Exact datum planes/axes engine & worker preview.
  - [x] CM4.c: Durable support persistence & topology evolution tracking.
  - [x] CM4.d: Native datum selection, viewport display & Normal To navigation.
  - [x] CM4.e: Native support authoring & upstream plane/axis repair workflows.
- [x] **CM5: Complex Features & Draft**
  - [x] Baseline & Acceptance Matrix Freeze *(Tasks 0.1–0.3 complete: audit, current-source baseline HEAD 37501e8 verified with 97 passing CTests and 22 analyzed TUs, 29-row acceptance matrix frozen)*.
  - [x] CM5.a: Intent contracts (contours, independent starts/ends, thin features, draft; verified on current source with focused assertions for total Midplane, inactive Direction 2, independent drafts, offset side independence, one-turn radians, and flip-side alone on open non-thin cuts).
  - [x] CM5.b: Exact transient modeling previews in CAD worker *(Complete: exact profile boundaries, straight/curved thin sections, directed trimmed intervals, disconnected/collapsing offset rejections, topological Vertex/Face references on solid bodies, signed offset starts, opposite independent directions, independent angular revolve directions, non-coplanar axis rejection, open solid revolve rejection, inward/outward draft, and geometry failure boundaries; 8/8 CM5 CTests passing)*.
  - [x] CM5.c: Durable persistence, Boolean provenance & explicit repair *(Verified on available-Mac development scope, branch `review/cm5-c-delivery-20261114`; native controls and full acceptance remain open)*.
  - [x] CM5.d: Native on-screen depth/thickness/draft manipulators & authoring *(Complete: FeaturePanel Extrude/Revolve/Draft controls, linear depth & angular draft handles in Viewport, exact display preview protocol, 5-scale verification at 100–200% DPR, mm/in, arbitrary sketch planes; review repairs merged into main)*.
  - [x] CM5.e: Complete course workflow verification *(Complete: T5 Tutorial 1 asymmetric draft, T5 thin extrusion midplane, T6 selected/reused contours, T6 open-profile cut, T8 standalone draft, upstream mutation & rebuild, save/reopen, 98/98 enabled CTests passing; merged into main)*.
- [ ] **CM6: Hole Wizard, Ribs, Patterns & Treatments** *(Scheduled; not started)*.
- [ ] **CM7: 3D Curves & Spatial Sketches** *(Scheduled; not started)*.
- [ ] **CM8: Sweeps, Lofts & Shells** *(Scheduled; not started)*.
- [ ] **CM9: Offline Catalogs, Materials & Evaluation Tools** *(Scheduled; not started)*.

---

## 3. Active Workstream: Smart Dimension Interaction Parity (Tasks A–C Scope)

- [x] **Task A: Acceptance Matrix & Baseline Plan** — Created page-indexed matrix (`review/smart-dimension-a-20261104`, committed & pushed `fb797f7`).
- [/] **Task B: Persistent Smart Dimension Mode** — Persistent Selecting/Placing/EditingValue states, flyouts, candidate hover/selection, repeated placement (`review/smart-dimension-b-20261104`; CTests & 5-scale native verification pass; full suite in progress).
- [ ] **Task C: Selection & Inference Rules** — Orientation inference (horizontal/vertical/aligned), point-line perpendicular distance, angular sector solving.
- [~] **Tasks D–G: Compact Modify, Advanced Graphics & Drawing Import** *(Deferred / future scope)*.

---

## 4. Platform & Acceptance Gates

- [x] **macOS Development Environment:** Native build, CTest suite, and 100%–200% scaling verified.
- [~] **Windows / Linux Native Verification:** Deferred until application development is complete per user instruction.
- [~] **Physical 8 GB RAM Resource Target:** Deferred until application development is complete per user instruction.
