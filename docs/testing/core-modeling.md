# Core-modeling execution evidence

[26 October sketch tool selection repair](module-4-tool-selection.md) records the
later drawing-mode fix and its regression evidence. It adds no CM2 feature or
milestone acceptance claim.

Updated: 1 November 2026. CM0–CM9 authorized on 22 October, after Module 10 and before Module 11.
Original work branch: `feature/core-modeling-expansion`; local review commits were
authorized on 25 October and are stacked through `review/core-modeling-integration`.
No merge/push. Preserve all earlier
work and 6.1b/6.4c/7.4c Blocked gates. [Task contracts](../CORE_MODELING.md) and
[coverage requirements](../research/core-modeling-coverage.json) define scope.

The 25 October integrated review-tree run passed 69 enabled CTests (one deliberate
failure probe disabled), formatting, static analysis, architecture, coverage and
specification checks, and the available-Mac native workflows at 100–200% scales.
[Verification record](artifacts/core-modeling/20261025T084406.781929Z/verification/verification.json)
contains the per-step results and source identity. This confirms the implemented
CM1 and straight-line CM2 slice; remaining CM2 methods and full acceptance stay
open. This note and the branch metadata were recorded after the run.

## Current execution status

CM0 is in progress. The page-indexed register is scheduled coverage, not proof of
257 working capabilities. Operand matrices and restrictions remain in review;
coverage integrity checks do not establish semantic completeness. CM1 has an
implemented rectangle/slot methods, exact conic/rational spline editors and broader
sketch relations, including dependency-ordered Pierce. The available-Mac
implementation slice is verified below; projected model-edge/external-vertex cases
remain CM4 dependencies.
CM1–CM9 remain incomplete until their production, persistence,
native and verification tasks have actual evidence.

CM3 now implements the approved dimension and interaction interfaces; the
[completion record](core-modeling-cm3-completion.md) distinguishes individual
passing behavior tests from the final enabled-suite/static/native verification
and full acceptance. Historical CM3 slice evidence below retains its original
scope.

The old source/specification/module reports retain their original scope. The
extension does not certify Windows/Linux, physical device/assistive technology,
installed builds or complete physical 8 GB resource budgets. Module 11 is not
started. Unsupported geometry and missing redistributable catalog data must stay
visible rather than becoming approximate meshes or invented dimensions.

## Source availability

The user confirmed that the original PDFs were accidentally deleted. Before that
deletion, the prior review captured original hashes, all page text, contact sheets
and selected detailed page renders. These retained files are now preserved in the
[private research cache](../research/course-source-cache/README.md), with a
[cache hash manifest](../research/course-source-cache/cache-manifest.json).
Original-PDF rehashing and new detailed renders were unavailable during that run.
Unverifiable diagrams/restrictions remained open; cache preservation did not
certify them.

On **23 October 2026**, the user restored the PDFs to the same Downloads folder.
[Fresh source verification](artifacts/core-modeling/source-restoration-2026-10-23.json)
confirmed that all **11 PDFs** match their original SHA-256 hashes, covering the
same 448 pages. The missing-source blocker is cleared and original diagrams are
available for further review. Coverage reconciliation and geometry/implementation
gates remain open. Historical verification logs retain the source availability
that applied when they ran; this restoration is separate evidence.

## Fresh pre-extension baseline

[Baseline source identity](artifacts/core-modeling/20261022T165836.974869Z/baseline/source-identity.json)
and [host result](artifacts/core-modeling/20261022T165836.974869Z/baseline/host-verification.json)
record the existing binaries before CM production changes were rebuilt.
Configure/build passed. On-host CTest passed all **54 enabled tests** in 267.01 s;
the deliberate harness failure remained disabled. The first sandboxed CTest
attempt could not listen on local IPC and was interrupted; its log is preserved.
That environment failure is not a geometry result or a passing test run.
Baseline binaries stayed unchanged through the host run; subsequent source edits
were rebuilt afterward. Older module evidence was not relabeled.

## CM0 isolated feasibility

Build target: `cm0_feasibility`; source remains under
`tests/core_modeling/feasibility.cpp`, excluded from production linking.
[Probe results](artifacts/core-modeling/20261022T172029.031895Z/verification/feasibility.json)
retain the deliberate nonzero exit status for a blocked capability.

| Probe | Observed result | Limit |
|---|---|---|
| Full round on opposing box edges | Blocked: OCCT fillet builder did not complete | No reduced-radius substitute; no general face-set adapter established |
| Multi-thickness shell | Valid exact planar result, volume 1626 mm³ | Curved/asymmetric general cases still need fixtures |
| Surface termination | Changed inclined plane gives 4800 / 5600 mm³ | Actual plane termination, not all curved surfaces/start/end modes |
| Centerline loft seed | Circular-section frustum, 5497.787143782139 mm³ | Ordinary linear seed only |
| Constrained cubic spline | Dedicated point-on-BSpline constraint solves point Y=2 and control Y=7/3 | General rational, trimmed and tangent combinations unproven |
| Curved centerline + auxiliary guide | Valid quarter-circle result, 937.612838030134 mm³; reference 937.612418103489 | 0.1 mm³ feasibility tolerance only; no product-tolerance claim |

The linear seed's result is separate from the later curved probe. Its original
“unproven” limit describes what that individual seed establishes. Neither proves
arbitrary guide intersections, section correspondence or editable connectors.
The earlier generic spline-constraint failure and the subsequent specialized
adapter probe are preserved in the
[initial probe evidence](artifacts/core-modeling/20261022T165836.974869Z/cm0/feasibility-initial.json)
and [specialized probe evidence](artifacts/core-modeling/20261022T165836.974869Z/cm0/feasibility-specialized-spline.json).
Pinned upstream library sources were not modified.

## CM1 relation slice

[Relation contracts](../decisions/core-modeling-relations.md) define the exact
supported operands and new sketch/project manifests. Point-pair Horizontal and
Vertical, point-on-line/circle Coincident, Midpoint, Collinear, circle/arc
Concentric and Coradial, and equal circular radii are implemented through the
worker solver, independent residual checks, persistence and native relation UI.
No new primitive, semantic construction group or multi-collection ModelEdit
protocol is claimed here.

The initial full current-tree run passed **57 enabled tests** in 279.65 s,
including `cm1.relations`, `cm1.contracts` and `cm0.coverage`:
[CTest output](artifacts/core-modeling/20261022T172029.031895Z/verification/ctest.txt).
The disabled deliberate harness failure remained disabled. Native scaling and
static analysis are recorded by the verification runner as they finish; this
initial result does not preapprove subsequent changes.

That run was stopped during static analysis after a separate marked-root test
proved that upgraded semantics could overwrite a legacy saved file. The
[failing reproduction](artifacts/core-modeling/upgrade-copy-red/result.json)
is preserved. The worker now refuses that overwrite with `UpgradeCopyRequired`.
Native tests attempt it, verify unchanged bytes, then use Save As to a new file.
Saving an already-upgraded copy normally, including a duplicate save request, is
also covered.

A second [failing reproduction](artifacts/core-modeling/history-manifest-red/result.json)
proved that a forged lower manifest was accepted when only retained redo history
contained the new semantics. The next verification run was superseded while CTest
was still running. Validation now scans retained intent/undo/redo definitions as
well as active/checkpoint sketches; a lower manifest is refused with
`IncompatibleSchema`. The corrected snapshot reopens and redoes successfully.
Neither superseded run is labeled full final acceptance.

Tests include three scales of analytical point alignment; independent residuals
and freedom; fixed-line midpoint/collinearity; circle incidence; all circle/arc
pairs for concentric/coradial/equal with reversed operands; conflicting refusal;
duplicate requests; sticky compatibility after undo, features, assemblies and
drawings; original saved bytes; save/reopen; worker restart; draft preview/cancel;
mm/in entry; and stable entity IDs. Existing disposable-root guards remain active.

## 22 October relation-slice verification

Command: `python3 tools/verify_core_modeling.py --native` on the available
Apple M2 Mac with 8,589,934,592 bytes physical RAM. This is an observed test host,
not full physical 8 GB performance/resource acceptance.

[Final result](artifacts/core-modeling/20261022T174052.110799Z/verification/verification.json),
[source identity](artifacts/core-modeling/20261022T174052.110799Z/verification/source-identity.json)
and [input changes from baseline](artifacts/core-modeling/20261022T174052.110799Z/verification/changes-from-baseline.json)
record the branch, unchanged HEAD, frozen input hashes, commands and results.
The runner checked that no verification source input changed during execution.
A newly included hash input may predate this work; it is not automatically a new
file or an upstream library modification.

| Check | Result |
|---|---|
| Configure and full development build | Pass |
| Full existing + new CTest suite | 57 enabled tests passed, 258.29 s; one deliberate failure disabled |
| Pinned format / source checks | Pass |
| Clang static analysis of compilation commands | Pass |
| Native ownership / packaged worker consistency | Pass |
| Protected research / coverage / specification integrity | Pass |
| Native relation workflows | Pass at effective DPR 1, 1.25, 1.5, 1.75 and 2 |
| Native existing arc regression | Pass at DPR 1 and 2 |
| CM0 geometry capability probes | Five narrow successes; full-round seed remains Blocked; general face-set blend unproven |

Each native scale runs four point-pair workflows (Horizontal/Vertical × mm/in) and
seven additional relation workflows. Every exposed native window asserts the
actual Qt pixel ratio; native runs do not use the offscreen backend. Tests include
preview-only state, acceptance, undo/redo, save/reopen, stable IDs and worker
restart for the point-pair cases. Test fixture construction and list/combo
selection use Qt test APIs; this is not a manual physical-pointer/device study.

Preview images were visually reviewed for
[100% Midpoint](artifacts/core-modeling/20261022T174052.110799Z/verification/native-100/cm1-midpoint-preview.png)
and [200% Coradial](artifacts/core-modeling/20261022T174052.110799Z/verification/native-200/cm1-coradial-preview.png).
The PropertyManager remains scrollable; these screenshots do not certify every
control position or every platform's font/device settings.

CM0.d/e close only capability recording and evidence integrity. At that date,
CM0.a/b/c retained their open/blocked scope; no whole CM milestone is complete. CM1's selected
relation checks pass, while all required operand combinations, remaining CM1
primitives/groups, unified dependency/ModelEdit contracts and CM2–CM9 production
work remain open. No course coverage percentage or full milestone completion is
inferred from this run. Native historical-reader execution, all interrupted
acceptance variants, broader portable-copy/reference-repair cases, cache stress,
other platforms/devices/assistive technology and installed-build checks remain
unverified unless separately covered by their historical module evidence.

No commit or push. Modules 11–12 remain unstarted. The exact saved records for
6.1b/6.4c/7.4c remain Blocked and unchanged.

## Catalog workstream

[Acquisition candidates](../research/core-modeling-catalog-sources.md) record
primary-source observations and unresolved per-dataset provenance, licensing,
edition and coverage questions. No third-party catalog has been bundled. All
broad CM9 catalog completion gates remain open.

## 24 October rectangle construction slice

[Rectangle contracts](../decisions/core-modeling-rectangles.md) and the
[original T2 page 34 review](artifacts/core-modeling/rectangle-source-review/review.json)
define Corner, Center, 3 Point Corner, 3 Point Center and Parallelogram. Each is
implemented as an identified semantic group, with ordered perimeter members,
owned shape relations and construction centers/diagonals for center methods.
The three center inputs are center, edge midpoint and corner. Only perimeter
sides form the solid profile. Existing entity, constraint and numerical-variable
admission limits remain unchanged. No upstream dependency code was modified.

Properties preserve member/group IDs while changing anchor/center, side lengths,
rotation and parallelogram side angle. Size edits add/update explicit Length
dimensions and expressions. Angle fields edit current pose; driving angular
dimensions belong to CM3. Unconstrained group dragging preserves shape. Other
controlling relations yield a specific diagnostic; current code does not claim
general constraint-preserving dragging. Intrinsic shape relations cannot be
silently removed. Externally controlled/linked group deletion requires prior
explicit relation removal or repair. Legacy ungrouped rectangles remain readable.

Sketch schema 3 adds typed groups and requires `sketch-groups=1` in the semantic
manifest. Schema 1/2 encoding remains unchanged without groups. Validation checks
active sketches, checkpoints and retained intent/undo/redo. Reader requirements
remain sticky after undo and other edit kinds. Existing legacy and relation-only
files cannot be overwritten by newer semantics; a new Save As destination is
required. Portable copies carry resolved definitions and stable IDs.

The [fresh pre-rectangle baseline](artifacts/core-modeling/20261023T170331.720328Z/verification/baseline-scope.json)
passes CM0.b's configure/build, 57 enabled CTests, pinned format, Clang and worker
ownership scope. Its overall verification invocation remains **Fail** because
the subsequent native relation window did not become active. Both symlink and
actual copied application launch probes failed the same assertion. The user
confirmed an unlocked normal desktop; the later
[current native rectangle retry](artifacts/core-modeling/native-unlocked-retry/result.json)
passed. The original activation cause remains unexplained; no activation or DPR
assertion was weakened. Old failed logs are preserved.

`cm1.rectangles` covers five methods at scales 0.001/1/1000, independent residuals
and freedom, area/volume, pose/size edits, stable IDs, malformed roles, admission,
conflicts with unchanged accepted state, deduplication, sticky/forged manifests,
undo/redo, save/reopen and portable copies. `cm1.rectangle_contracts` covers all
five in mm/in, partial-input Escape, preview without durable writes, one draft
undo step, property changes, acceptance, save/reopen and worker restart. Pointer
workflows cover click sequences and staged drags with profile/construction flags.
Native equivalents retain exposed/active-window and actual DPR assertions.

The first rectangle full run passed 58 of 59 enabled tests but failed the
existing Module 4 lifecycle. The
[failure and no-op diagnosis](artifacts/core-modeling/rectangle-click-regression/diagnosis.json)
show a stationary selection click reconstructing a typed group, introducing
floating-point differences and queuing an unnecessary preview. A regression
asserting exact unchanged maps failed before the fix. The shared edit boundary
now preserves the original sketch for identical drag endpoints, after validating
the selected identity. The original lifecycle and targeted rectangle checks pass;
temporary instrumentation was removed. That failed full invocation is retained,
not relabeled. The new corpus also asserts the expected 4/5/6 freedoms and the
one-freedom reduction from a driving side dimension.

### Final rectangle-slice verification

Command: `python3 tools/verify_core_modeling.py --native`, available Apple M2 Mac,
8,589,934,592 bytes physical RAM. This host does not establish full SRS physical
8 GB resource acceptance. The [final result](artifacts/core-modeling/20261024T045204.326018Z/verification/verification.json),
[frozen source identity](artifacts/core-modeling/20261024T045204.326018Z/verification/source-identity.json),
[changes from the pre-rectangle baseline](artifacts/core-modeling/20261024T045204.326018Z/verification/changes-from-baseline.json)
and [preservation checks](artifacts/core-modeling/20261024T045204.326018Z/verification/preservation.json)
record actual commands, branch/HEAD and results. All **186** verification source
inputs retained their recorded hashes through the complete run. No commit/push.

| Check | Observed result |
|---|---|
| Configure / full development build | Pass |
| Full CTest suite | **59 enabled tests passed**, 286.44 s; deliberate harness failure disabled |
| Pinned formatting / Clang analysis | Pass; 90 project source compilation units analyzed |
| Worker ownership / research isolation / coverage / specification structure | Pass |
| Native relations and all five rectangle workflows | Pass at effective DPR **1, 1.25, 1.5, 1.75, 2** |
| Existing native arc regression | Pass at DPR 1 and 2 |
| Original T2–T8 source PDF hashes | Pass |
| Isolated CM0 geometry probes | Same five narrow successes; full-round/general face-set blend blockers retained |

At each native scale, rectangle tests run ten mm/in coordinate/property cases and
ten click/staged-drag cases, including construction flags. Native windows assert
exposure, activation and actual Qt DPR. Qt APIs drive fixture/list/combo setup;
this is not a manual physical-device study. The earlier activation failure did
not recur; its original cause remains unknown. No assertion was weakened.

The [visual review](artifacts/core-modeling/20261024T045204.326018Z/verification/visual-review.json)
records the native [100% three-point center preview](artifacts/core-modeling/20261024T045204.326018Z/verification/native-100/native-rectangle-3-Point-Center-mm-preview.png)
and [200% parallelogram preview](artifacts/core-modeling/20261024T045204.326018Z/verification/native-200/native-rectangle-Parallelogram-mm-preview.png).
The full shapes and applicable properties are visible after Zoom to fit; the
PropertyManager scrolls, and long numeric input strings scroll horizontally.
These two images do not certify every label position, font or device.

This closes the **selected available-Mac rectangle slice**, not all CM1.a–e or
all per-row acceptance gates. Complete per-method upstream-solid/failure matrices,
interrupted acceptance variants, cache stress and the remaining broader
resource/platform/device gates stay open. Shared historical regression checks
are not automatically per-variant course acceptance.
Whole CM1 remains incomplete. CM0.a coverage reconciliation and CM0.c difficult
geometry proof remain open/blocked. CM2–CM9, full resource/device/platform and
installed-build gates remain open. Modules 11–12 are not started.

## 25 October CM1 continuation — available-Mac verification

The user requested CM1 completion. [Continuation contracts](../decisions/core-modeling-cm1-primitives.md)
cover four slot methods, exact conic/rational B-spline definitions, broader
relation operands and schema-4 compatibility. Historical 24 October results remain
scoped to that source snapshot; they do not certify these subsequent edits.

Targeted slot and curve engine logs are under
`artifacts/core-modeling/cm1-continuation-20261025/`. Initial adapter failures were
retained: native Difference uses second-minus-first, and the pinned proportional
Equal reduction aliases two unknowns even for a ratio. Project-owned adaptation
uses the exact equation diameter-minus-radius equals radius; upstream sources
were not changed. Independent residual/overall-length tests caught both issues.

Slot analytical/geometry/persistence and Qt-widget seeds passed. Exact conic and
rational-spline profile volumes and constrained cubic editing seeds passed.
The widget spline completion exposed a Return shortcut collision and a reentrant
stale-selection crash during draft Undo. Both were repaired and the targeted
curve workflow passes; [diagnosis](artifacts/core-modeling/cm1-continuation-20261025/diagnosis.json)
retains the original failures and cleanup. The final frozen-source result below verifies the implemented slice; whole
course/milestone closure remains open.

Pierce now has typed cross-sketch references, exact worker curve/plane
intersections, dependency ordering and explicit branch repair. Targeted tests
cover eight source families at three scales, missing/cyclic/ambiguous references,
upstream feature rebuild, save/reopen/copies and sticky compatibility. Interruption/cancellation, bounded cache trimming and the native scale checks
are recorded below; broader resource acceptance remains open. Projected model-edge
and external-vertex operands remain CM4 prerequisites; whole CM1.a–e remain
open. Deferred gates 6.1b/6.4c/7.4c and the stop before Module 11 are unchanged.

### Final corrected CM1 implementation-slice verification

Command: `python3 tools/verify_core_modeling.py --native`, on the available Apple
M2 Mac with 8,589,934,592 bytes physical RAM. [Result](artifacts/core-modeling/20261025T065352.783818Z/verification/verification.json),
[source identity](artifacts/core-modeling/20261025T065352.783818Z/verification/source-identity.json),
[changes from the 24 October snapshot](artifacts/core-modeling/20261025T065352.783818Z/verification/changes-from-rectangle-snapshot.json)
and [preservation](artifacts/core-modeling/20261025T065352.783818Z/verification/preservation.json) record the actual commands and scope.
All **199** source inputs remained frozen through the run, aggregate SHA-256
`233b98bb7c4e84c7e44eb12f2ec44e87e36cc0392081a7a75d1bd1f2e0b2b06e`.
HEAD and topic branch are unchanged; no commit/push.

| Check | Observed result |
|---|---|
| Configure / complete development build | Pass |
| Full existing and new CTest suite | **67 enabled tests passed**, 315.80 s; deliberate harness failure disabled |
| Pinned format / Clang analysis | Pass; **98** compilation units analyzed |
| UI/worker ownership, packaged worker, research isolation, coverage/specification structure | Pass |
| Native relations, rectangles, slots, conics/splines, Pierce and click/drag creation | Pass at actual Qt DPR **1, 1.25, 1.5, 1.75, 2** |
| Existing native arc regression | Pass at DPR 1 and 2 |
| Source/research/deferred-gate preservation | All 11 original PDFs / 448 pages, 52 cached files and exact 6.1b/6.4c/7.4c records preserved |
| CM0 isolated probes | Same five narrow successes; full-round/general face-set blend blockers retained |

The [earlier full snapshot](artifacts/core-modeling/20261025T063157.053441Z/verification/verification.json)
passed its 67 CTests and analysis, but was stopped during native scaling after
[visual review](artifacts/core-modeling/20261025T063157.053441Z/verification/visual-review.json)
found overlapping automatic slot dimensions. Transient label separation/leaders,
shortest round-trip numeric text and nonredundant literal annotations repaired it.
The corrected snapshot was rebuilt and verified afresh. No native assertion,
resource limit, solver residual or pinned dependency was weakened.

The [final visual review](artifacts/core-modeling/20261025T065352.783818Z/verification/visual-review.json) covers the 100%/200% curved
slot, 200% spline controls and 100% Pierce editor. Property fields retain complete
values and can scroll horizontally. This is Qt-driven native verification with
strict window exposure/activation/DPR assertions, not a physical-device study.

The implemented additions are four Slot methods with Center to Center / Overall
Length controls and optional dimensions; exact ellipse, elliptical arc, parabola
and rational control-point B-spline definitions/editors; point/curve incidence,
three-operand symmetry/intersection, Merge and location Fix; multi-point alignment
and multi-line collinearity; broader circular/conic/spline tangent combinations;
and cross-sketch Pierce with explicit missing/ambiguous branch repair. Existing
rectangle and arc behavior remains under regression. Control-point splines have
clamped knots and optional C0 closure; periodic/fit-spline creation is not claimed.

Engine cases include analytical areas/volumes, three geometry scales, independent
residual/freedom checks, 120 generic tangent combinations, nontrivial symmetry,
circular internal/external tangency and trimmed-contact refusal. Pierce tests
cover eight source families, dependency order independent of storage order,
source edits rebuilding a dependent solid, cycles, missing/type-changed sources,
coplanar overlap and explicit branch selection. They also cover legacy-file byte
preservation, forged lower manifests with redo-only new content, portable copies,
deduplication, cancellation, stale requests, injected interruption before/after
commit, recover/retry and 20 edit/undo/redo cycles with cache trimming. These
fault-injection cases are distinct from the existing Module 10 process-crash
matrix, which also passes in the full suite. Tests use marked disposable roots.

Native runs exercise mm/in entry, parameter/control edits, preview without a
durable revision, completion/cancel, draft and durable undo/redo, save/reopen and
worker restart for the new tool workflows. Pointer tests cover click and staged
drag input for all four Slot and four exact-curve tools. Native relation cases
include the multi-selection, symmetry, intersection, Merge and spline-tangent
extensions. All stored references use semantic IDs/roles/parameters, not display
array indices. The UI holds bounded display samples; exact curves and solving
remain worker-owned through the shared kernel adapter.

**Whole CM1 and its per-row course gate remain open.** Projected model-edge,
external-vertex and external projected-curve operands need CM4 association/repair
adapters; the coverage register retains those cases as Blocked rather than
substituting internal sketches. Complete per-variant upstream/failure/resource
matrices remain open where no fixture is recorded. General datum/solid/sketch
dependency integration and multi-collection PreviewModelEdit/CommitModelEdit are
still broader roadmap work. The passing source snapshot does not certify other
platforms, installed builds, physical devices/assistive technology or full SRS
physical 8 GB budgets. CM0 geometry blockers, CM2–CM9 and deferred gates remain
visible. Modules 11–12 are not started.

## 25 October CM2 — straight-line trim/extend slice

CM2 now has its first production editing slice. [Contracts](../decisions/core-modeling-cm2-editing.md)
define **Trim to closest**, **Trim away inside**, **Trim away outside**,
**Dynamic extend**, and **Extend to apparent intersection** for straight-line
targets. Finite line/circle/arc boundaries are supported; apparent intersection
uses one explicitly selected line's infinite support. Geometry is computed in the
sole worker from SI semantic operands, never from display samples.

A unique remaining line retains its ID. An interior split retires the original
line and records independent descendant IDs. Cut endpoints get new point IDs;
neighboring geometry stays put. Original point entities are retained, including
points that become standalone; this slice does not implicitly garbage-collect
those points. Missing cross-sketch references are refused rather than rebound to
an arbitrary descendant. Atomic cross-sketch replacement/repair remains part of
the broader ModelEdit work; a split with an unresolved Pierce dependency cannot
be accepted through this slice.

The worker returns a candidate plus affected constraint IDs, kinds, operands and
expressions/values. Preview changes neither draft nor durable revision. Native
properties require explicit approval of the listed removals before Apply, which
makes one draft undo step; Finish Sketch uses the existing atomic SetSketches
transaction. Horizontal/Vertical relations on a uniquely retained line remain;
other affected relations and dimensions are disclosed. Rectangle/slot group
members are refused so trimming cannot erase their construction intent.

Schema 5 persists bounded edit provenance and descendant/removal mappings; the
sticky `sketch-edits=1` compatibility manifest protects active, undo/redo and
checkpoint content. Old files remain readable and are upgraded on copies.
Existing 256-entity/512-constraint/16-sketch/256 KiB limits remain; edit history is
capped at 64 records. Preview scheduling allows one running request and one
replaceable pending draft. Generation/context/revision checks discard stale
results, and applying requires the complete source draft to remain unchanged.

### Verification and retained failures

Command: `python3 tools/verify_core_modeling.py --native`, on the available Apple
M2 Mac with 8,589,934,592 bytes physical RAM. This does not establish full SRS
physical 8 GB resource acceptance. [Final result](artifacts/core-modeling/20261025T080645.353363Z/verification/verification.json),
[source identity](artifacts/core-modeling/20261025T080645.353363Z/verification/source-identity.json),
[changes from CM1](artifacts/core-modeling/20261025T080645.353363Z/verification/changes-from-cm1-snapshot.json)
and [preservation](artifacts/core-modeling/20261025T080645.353363Z/verification/preservation.json)
record actual commands and scope. All **205** source inputs remained frozen
through the run, aggregate SHA-256
`7f25d5de016dafe2d4cc6800ae15ad5d211048bf10152e54943543b686f2f67b`.
No source files were removed relative to the recorded CM1 snapshot. Branch and
HEAD remain unchanged; no commit or push.

| Check | Observed result |
|---|---|
| Configure / full development build | Pass |
| Full CTest suite | **69 enabled tests passed**, 330.77 s; deliberate harness failure disabled |
| Pinned format / Clang analysis | Pass; **103** compilation units analyzed |
| UI/worker ownership, packaged worker, research isolation, coverage/specification structure | Pass |
| Native CM1 regressions and all five CM2 line methods | Pass at actual effective Qt DPR **1, 1.25, 1.5, 1.75, 2** |
| Existing native arc regression | Pass at DPR 1 and 2 |
| Research/source/deferred gates | All 11 original PDFs / 448 pages, 52 cached files, exact 6.1b/6.4c/7.4c records preserved |
| Isolated CM0 probes | Same five narrow successes; full-round/general face-set blending blockers retained |

[Engine tests](../../tests/core_modeling/editing.cpp) check the five methods,
three scales, rotated/transformed operands, circular/arc boundaries, analytical
fragment lengths, preserved neighboring endpoints, invariant relation retention,
dimension disclosure, ambiguous/overlapping/missing boundary refusal, construction
protection and admission/history caps. Persistence checks cover stable accepted
IDs/maps, legacy-file byte preservation, sticky/forged lower manifests with
redo-only new semantics, deduplication, undo/redo, reopen and portable copies.
An attempted trim opening an existing extruded profile is rejected with unchanged
accepted sketches, revision and exact volume. A Pierce dependency on a split
source is refused without choosing a descendant.

[UI/worker tests](../../tests/core_modeling/editing_contracts.cpp) exercise all
five methods in mm/in at each native scale: property selection and canvas picks,
preview/cancel, explicit dimension-loss decision, prevention of Finish bypass,
one draft undo step, durable undo/redo, save/reopen and worker restart. A canceled
in-flight preview delivers no candidate; twenty immediate previews retain at
most two outstanding requests and only the latest result. Tests use marked
disposable roots and cannot attach to release recovery paths.

Native runs assert window exposure/activation and actual DPR. They use the
production OpenGL format and verify a visible reference renderer before entering
the QWidget sketch canvas. Qt APIs set fixtures, combo/list/numeric values and
consent; native button/pointer events exercise the workflow. This is not a manual
physical-device study or exhaustive keyboard/assistive-technology acceptance.
[Visual review](artifacts/core-modeling/20261025T080645.353363Z/verification/visual-review.json)
covers [100%](artifacts/core-modeling/20261025T080645.353363Z/verification/native-100/cm2-inside-disclosure.png)
and [200%](artifacts/core-modeling/20261025T080645.353363Z/verification/native-200/cm2-inside-disclosure.png)
inside-trim disclosure previews. Controls and the two line remnants are legible;
the PropertyManager scrolls and numeric fields retain full values.

The [earlier full run](artifacts/core-modeling/20261025T075641.079768Z/verification/verification.json)
failed `module3.interaction`. Adding sketch previews to the generic stale-reply
filter retired canceled requests without notifying idle controls. The original
worker/UI test was strengthened at the exact cancellation boundary and
[failed before](artifacts/core-modeling/cm2-line-editing-20261025/cancel-regression-red.txt)
the fix; [the focused rerun](artifacts/core-modeling/cm2-line-editing-20261025/cancel-regression-green.txt)
and final full suite pass. Stale geometry is still discarded; retiring the
request now emits the idle-state notification.

[Diagnosis](artifacts/core-modeling/cm2-line-editing-20261025/diagnosis.json) also
retains the initial sandbox IPC denial, a test comparing `/var` with canonical
`/private/var` despite a successful save, and the new native harness initially
omitting the production OpenGL format. Visual inspection rejected that early
apparently green workflow. The corrected harness requires graphics initialization
and no graphics failure on a visible renderer. These failures/probes are not
relabeled as final acceptance; no existing assertion or resource limit was
weakened and no temporary debug logging remains.

**Whole CM2.a–e remain In progress; all per-row course gates stay open.** This closes
only the selected production line-editing slice on the available Mac. Power trim,
Corner, conic/spline boundaries and non-line targets, fillets/chamfers, offsets,
mirrors/patterns and move/copy/rotate/scale remain unimplemented here. Atomic
multi-collection repair, complete per-variant interrupted-commit/crash/cache and
keyboard/device/resource/platform/installed-build matrices remain open. Shared
historical recovery tests do not automatically close those per-variant gates.
Existing CM0/CM1/CM4 prerequisites and deferred 6.1b/6.4c/7.4c remain unchanged.
The next independent CM2 work is Power trim/Corner and additional exact curve
operands using the identity/disclosure contract. Modules 11–12 are not started.

## CM3 — driving/reference and component dimension slice

Work branch: `feature/cm3-dimensions`, based on the existing integration tree with
the uncommitted sketch tool-selection repair preserved. No commit, merge or push
was performed for this slice.

[Contracts](../decisions/core-modeling-cm3-dimensions.md) define schema 6,
the sticky semantic manifest, ordered signed horizontal/vertical distances,
reference regeneration and saved annotation placements. Existing aligned,
angular, radius, diameter and sweep dimensions share the reference/editor flow.
The native editor previews an isolated candidate, requires a valid worker result
before Apply and supports Restore/Cancel. Labels can reopen the same dimension
identity. Reference values never enter equations or become controlling expressions
through direct geometry property edits.

**Available-Mac slice verification passed on 28 October, with one native window
activation retry.** The [completion record](artifacts/core-modeling/20261028T031935.641987Z/verification/completion.json)
links the exact [source identity](artifacts/core-modeling/20261028T031935.641987Z/verification/source-identity.json),
full-suite/static results and the separate native continuation. All **72 enabled
CTests passed** in 338.14 s; the pre-existing deliberate failure probe stayed
disabled. Pinned formatting, Clang analysis, architecture ownership, source and
research preservation, coverage and specification checks passed. Native CM3
mm/in workflows passed at 100/125/150/175/200% effective scales, including the
actual shared Dimension action, label double-click, invalid units, canceled
in-flight previews, Restore/Apply and driving/reference conversion. Inspected
100% mm and 200% inch canvas captures show visible reference labels and geometry.

The [primary record](artifacts/core-modeling/20261028T031935.641987Z/verification/verification.json)
retains its **Fail** status: the existing 200% curve fixture could not activate
its EllipticalArc/inch window (`QTest::qWaitForWindowActive`). The
[unchanged-source continuation](artifacts/core-modeling/20261028T031935.641987Z/verification/native-200-continuation.json)
reran that complete workflow successfully and passed all remaining 200% checks,
including CM3, line editing, workspace and arcs. Source hashes matched before
and after the continuation. This does not relabel the failed attempt as passing.

The [earlier run](artifacts/core-modeling/20261028T031221.942108Z/verification/verification.json)
was deliberately interrupted during CTest before fixing reference-mode branch
continuity. Its subsequent [focused failing regression](artifacts/core-modeling/20261028T031221.942108Z/verification/reference-continuity-red.txt)
was added after that run's initial source snapshot. The final source tests both
readout-only motion and preservation of driving branch guards. Editor regression
testing also exposed and repaired stale multi-selection identities. These earlier
records remain separate from the final passing slice evidence.

Remaining CM3 work includes linear diameter about a centerline, ordinate groups,
driving path and true arc length, general direction/increment/drawing-mark options,
inference guides and confirmed automatic relations, deterministic Fully Define,
temporary drag targets and last-valid constrained-drag previews. The broader
per-variant recovery/resource/platform gates remain open. CM3.a–e and all touched
coverage rows remain In progress. CM2's remaining work and deferred gates
6.1b/6.4c/7.4c are unchanged. Module 11 is not started.


## CM3 — associated dimensions

The [associated-dimension contracts](../decisions/core-modeling-cm3-associated-dimensions.md)
cover centerline diameter for profile points/lines, shared-datum per-point X/Y
ordinates, and true circular ArcLength. Schema 7 and the sticky associated-dimension
manifest preserve axis/origin identities, side, expressions, placements and
compatibility across undo/reopen and portable copies. The worker owns exact signed
axis and radius-times-sweep equations; UI controls reuse the earlier dimension
editor and show diameter/X/Y/arc-length labels.

**28 October outcome: production implementation present; verification incomplete.** The
[completion record](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/completion.json)
links the [source identity](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/source-identity.json).
Configure/build passed. `cm3.associated_dimensions` and `cm3.dimensions` passed in
the full run, including rotated/translated/free axes, the signed axis-crossing
regression, independent residual/rank, real OCCT revolve volumes, radius/arc-length
changes, schemas 6/7, undo/reopen and portable copy. The
[focused pre-full run](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/focused-pre-full.txt)
passed all three CM3 tests. The
[earlier failing axis regression](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/axis-upstream-red.txt)
records the unsigned-distance seed failure before the signed adapter.

The [full run](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/verification.json)
remains **Fail**: 64 of 73 enabled tests passed, eight existing-module tests timed
out, and `cm3.dimension_contracts` failed its Dimension-action enabled assertion
after preview cancellation. The pre-existing deliberate failure probe was disabled.
The [power log](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/power-events.txt)
confirms repeated Sleep/DarkWake events during verification. A process-scoped
keep-awake [widget repeat](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/awake-widget-repeat.txt)
also timed out. The [diagnosis](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/diagnosis.json)
does not assume that the action failure is solely environmental: pending-request,
selection and post-modal action readiness still need an awake-session reproduction.
No assertion, timeout or admission limit was weakened.

An [unchanged-source static continuation](artifacts/core-modeling/20261028T075209.998083Z/cm3-associated/static-continuation.json)
passed formatting, Clang analysis of changed production/all CM3 test translation
units, architecture, research preservation, coverage and specifications. The
full/native run is not passing. New native activation/scaling checks were not
reached and remain Blocked on an awake, unlocked, logged-in Mac. Prior schema-6
native captures do not certify these additions. Once the host is ready, rerun
`python3 tools/verify_cm3_associated.py`. A temporary `caffeinate -di` wrapper may
prevent idle sleep, but did not prevent the observed sleep during the repeat.

Whole CM3 remains In progress. Multientity driving path length, grouped ordinate
placement, broader dimension controls, inference, Fully Define, constrained
dragging and the full course/recovery/resource/platform gates remain open.
This preserves the earlier evidence, tool-selection repair and deferred
6.1b/6.4c/7.4c gates. No commit, merge, push or Module 11 work.


### 29 October — awake-session verification

The user confirmed an awake, unlocked host. The
[completion record](artifacts/core-modeling/20261029T055300.331472Z/cm3-associated/completion.json)
records **73 enabled CTests passing** in 349.22 s, with the existing deliberate
failure probe still disabled. The earlier Dimension-action failure did not
reproduce in four [awake preflight repeats](artifacts/core-modeling/20261029T055300.331472Z/cm3-associated/awake-widget-preflight.json)
or the full suite; no production change was made for it. This does not prove that
sleep was its sole cause. Earlier failed/suspended records retain their statuses.

The native run exposed a separate harness synchronization error: the test queried
an empty hit rectangle before paintEvent published the label. The original
[failure](artifacts/core-modeling/20261029T055300.331472Z/cm3-associated/cm3-native-100.txt)
is retained. The only source change after the passing full suite was
`tests/core_modeling/dimension_contracts.cpp`: wait for the actual painted label,
then arm the modal driver immediately before the click. All original functional
assertions remain. Production inputs exactly match the passing full-suite build.
The [post-correction source identity](artifacts/core-modeling/20261029T055300.331472Z/cm3-associated/source-identity-after-test-sync.json)
records that one-file delta. The updated widget test passed four more repeats,
formatting and Clang reanalysis; see the
[followup record](artifacts/core-modeling/20261029T055300.331472Z/cm3-associated/verification-after-test-sync.json).
The full suite preceded this test-only change; its results were not relabeled as
a second full-suite run.

Native CM3 mm/in workflows passed at **100/125/150/175/200% effective scales**.
The [native matrix completion](artifacts/core-modeling/20261029T055300.331472Z/cm3-associated/native-matrix-completion.json)
checks source identity before/after and includes workspace/arc regressions at the
endpoint scales. Changed-production/all-CM3-test Clang analysis, formatting,
ownership, research preservation, coverage and specification checks passed.
Visual inspection of 200% mm diameter and 125% inch arc-length captures confirmed
visible reference symbols/values and geometry; this is not physical-input testing.

One [legacy native arc-count failure](artifacts/core-modeling/20261029T055300.331472Z/cm3-associated/arcs-native-100-after-test-sync.txt)
was retained: one tangent relation was observed instead of two. No production
change was made. A complete isolated rerun, two further consecutive 100% reruns,
and the 200% endpoint run passed. Its cause remains unproven, and broader native
interaction repeatability stays open. The successful finite workflows do not
close that broader gate or claim every attempt passed.

This closes the selected available-Mac dimension-slice verification, not CM3.a–e
or full course/resource/platform acceptance. Remaining CM3 tasks and deferred
6.1b/6.4c/7.4c are unchanged. No commit, merge, push or Module 11 work.

### 31 October — local commit review

The user requested review and local commits of the pending changes. The tool
selection repair is isolated on `review/sketch-tool-selection-20261031`; the CM3
dimension changes are on the stacked `review/cm3-dimensions-20261031` branch.
No merge or push is authorized. Earlier no-commit statements above describe their
historical verification runs.

Review found a pending `TECH_SPEC.md` reset that removed the core-modeling
extension and incorrectly marked Modules 7–10 as unstarted. The retained
specification text was restored; the original diff is preserved in the
[review artifacts](artifacts/review/20261031-precommit/tech-spec-rejected-reset.patch).
The CM3 application/test inputs match the 29 October post-synchronization native
source identity. All seven staged tool-selection code/build files also matched
their 26 October passing record. No production code changed during this review.

The [review ledger](artifacts/review/20261031-precommit/review.json) records fresh
build, CTest, changed-source Clang analysis and structural checks, including the
interrupted sandbox IPC attempt separately from the host-permission run. Native
scale/device/platform/resource checks were not rerun; earlier failures and
deferred gates remain open. CM3 as a whole remains In progress; Module 11 is not
started.

Fresh review checks passed: **73 enabled CTests in 345.59 s**, with the deliberate
failure probe disabled; development configure/build; pinned formatting; Clang
analysis of all 16 changed C++ translation units; architecture, research isolation,
coverage and specifications. The sandbox attempt was interrupted after local IPC
denials; the complete passing suite ran with host permissions on identical sources.


### 1 November — CM3 completion verification

The approved current-planar CM3 implementation is complete: identity-safe
reference editing, reverse/increment/drawing metadata controls, grouped
ordinates, true path length, worker-constrained dragging, confirmed inference
and Fully Define All/Selected. The [completion record](core-modeling-cm3-completion.md)
retains every red/green cycle and failed native attempt. Its full-suite baseline
passed 76 enabled tests and static/structural checks; an affected-test follow-up
passed after the test-only repaint correction. The complete native corpus,
identity and mm/in dimensions passed at 100/125/150/175/200%, with workspace and
arc checks at both endpoints. Production inputs match the full-suite baseline;
final follow-up source integrity passed. Full course/hardware/platform/installed/
resource acceptance stays Blocked, separately from implementation and available-Mac
verification. Preserve 6.1b/6.4c/7.4c; stop before Module 11. No merge or push.
