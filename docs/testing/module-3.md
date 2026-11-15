# Module 3 evidence

Current scope note: the later explicit Module 4 instruction supersedes the historical 4.1a stop boundary below. See [authorization](../decisions/implementation-scope.json). Stop before Module 5; earlier evidence remains scoped to its original run.

Status: **available-Mac Module 3 baseline and follow-ups; full platform completion Blocked**.
Current interaction/shape-editing behavior is recorded in [the later input repair](module-3-input-repair.md)
and [Module 4.0 workspace evidence](module-4.md). The dated logs and results below
remain historical; their hashes identify their own tested source, not the new UI.
A [pre-repair copy](artifacts/module-4/workspace-dev/module-3-before-input-workspace.md)
is preserved.

Original scope: The user explicitly authorized Module 3 on
19 October 2026 and required stopping before Module 4. Execution continued on
20 October. [Scope record](../decisions/implementation-scope.json).

The previous session left a placeholder and no Module 3 implementation. Work
followed 3.1a → 3.1b → 3.2a → 3.2b → 3.3a → 3.3b → 3.4a → 3.4b → 3.4c, with
incremental model, storage, solver, parser and interaction evidence retained in
[the artifact directory](artifacts/module-3). No agents were delegated work.
Module 4 had not started at this baseline. The later authorization covers only
workspace tasks 4.0a–c; exact features remain beyond 4.1a.

The user found a circle-creation gesture gap after the original closeout: the
old tests covered numeric circles and point dragging but did not test circle
press/drag/release. The earlier completion statement was too broad for that
interaction. See the [interaction follow-up](module-3-interaction-followup.md) for
the reproduced failure, added behavior and current validation. The original
results below remain historical evidence of their actual coverage.

## Implementation and task scope

| Task | Result and actual scope |
|---|---|
| 3.1a | Pass: SI support frames; points, shared-endpoint lines/polylines/rectangles, circles and arcs; construction flags; point/grid snapping; stable UUIDs and bounded typed serialization. |
| 3.1b | Pass on available Mac: native planar viewport/entity selection, dimension labels, circle press/drag/release and transient cancellation, line/polyline/rectangle previews, contextual fields/empty state, editable drafts, worker previews, generation-bound cancellation and durable semantic acceptance. [Circle follow-up](module-3-interaction-followup.md) and [later input/direct-editing repair](module-3-input-repair.md). |
| 3.2a | Pass: real pinned PlaneGCS mapping with owned variables/constants, fixed anchors and the documented geometric/dimensional combinations. Unsupported selections are disabled with reasons and rejected in the worker. |
| 3.2b | Pass: finite/degeneracy/radius checks, independent residuals, Jacobian-rank freedom, redundant/conflicting constraint IDs, line-direction and tangent-side continuity, preserved accepted state on failure. |
| 3.3a | Pass: SI/display conversion, dimensional arithmetic/functions/names, cycle/domain/nonfinite rejection, 4 KiB / 256-node / 32-level bounds; no general code evaluation. |
| 3.3b | Pass: accepted named/dimension edits and constraint deletion are single durable commands; canceled drags/edits, undo/redo, save/reopen retain identities and accepted values. |
| 3.4a / T-12–T-16 | Pass on available Mac: valid, underdefined, fully defined, redundant, contradictory, unsupported, singular and degenerate sketches; analytic invariants and declared diagnostics. |
| 3.4b | Pass: mm/inch, degrees/radians, mixed units, invalid dimensions, negative dimensions/radii, nonfinite/domain errors, cyclic/reserved names and expression bounds. |
| 3.4c | Blocked for full native platform completion. Mac Cocoa B1 plus mm/in gesture interaction/scaling/focus/persistence/recovery cases are recorded; Windows/Linux native runners and a controlled OS-network-disabled procedure are unavailable. |

Eight rows are checked for their explicit available-target scope in
[TECH_SPEC](../TECH_SPEC.md); 3.4c remains unchecked. Full FR-012–FR-016 rows remain
Blocked in traceability rather than being closed by unit tests or one platform.

The desktop draws a bounded semantic projection with native Qt painting. Only the
single worker owns PlaneGCS/OCCT state and project writes. The desktop links no
solver, SQLite, kernel or Qt Test. Sketches are stored as versioned semantic
collections in the existing schema; nested IDs/references are validated across the
project. Metadata-only Module 2 projects still open. New sketch-bearing projects
record the adapter/solver manifest and are refused by the older metadata-only
worker. No existing saved project is rewritten on read. The supported combinations,
representation, tolerance policy, admission limits and unresolved design choices
are explicit in [the Module 3 ADR](../decisions/module-3-sketches.md).

## Reproduction and original baseline results

From a logged-in Mac desktop with the pinned dependencies provisioned:

```sh
python3 tools/verify_module3.py --native
python3 tools/clean_reproduce.py --module 3
python3 docs/scripts/check_specs.py --write-report
```

Each verifier execution keeps a timestamped run, including failures. The
[latest verification record](artifacts/module-3/verification.json) identifies its
run folder, exact commands, exit codes, durations and fresh host inventory. The
repository is on topic branch `historical/module-3`, with no initial commit or remote.
The successful run's `source-identity.json` records individual input hashes,
aggregate SHA-256 and the unchanged frozen B1 fixture hash. The table below is
the original baseline; the follow-up report identifies the current tested code. It is a development
build with local dependency rpaths, not an installed release.

| Check | Observed result / evidence |
|---|---|
| 3.1a model | [Pass](artifacts/module-3/model-3.1a.txt): custom-frame round trip, off-plane/invalid-frame refusal, primitives/construction/shared endpoints, snap coordinates, identity and malformed-input corpus. |
| 3.1b persistence | [Pass](artifacts/module-3/persistence-3.1b-paths.txt): two B1 sketches, actual SQLite transactions, operation dedup, invalid edit preservation, undo/redo, save/reopen. |
| 3.2a–b solver | [Initial expanded Pass](artifacts/module-3/solver-3.2b.txt), followed by [tangent-side regression Pass](artifacts/module-3/tangent-side-green.txt). Real PlaneGCS, analytic geometry and independent rank/residual checks; scale factors 0.001 / 1 / 1000. |
| 3.3a expressions | [Pass](artifacts/module-3/expressions-3.3a.txt): unit and parser corpus, including dimensional square root, trig/inverse trig, names/cycles and bounds. |
| 3.3b persistence | [Pass](artifacts/module-3/undo-boundary-green.txt): named dimension rebuild, constraint deletion, invalid unit/cycle atomicity, undo through empty sketches/document creation, redo, actual snapshot reopen and stored-value consistency. |
| Strengthened UI/worker | [Pass](artifacts/module-3/endpoint-picking-green.txt): actual point movement/cancel, late preview rejection, two constrained B1 sketches, dimension change, constraint deletion, undo/redo, SIGKILL recovery of a newer unsaved sketch and preservation of the saved snapshot. This log uses offscreen Qt and is not native-graphics evidence. |
| Build / CTest | Pass: [build](artifacts/module-3/runs/20261020T033018.714107Z/build.txt); [23 enabled tests, zero failures](artifacts/module-3/runs/20261020T033018.714107Z/ctest.txt). The intentional failure probe remains Disabled; separate harness tests verify nonzero failure reporting. |
| Format / static analysis | Pass: [pinned formatter](artifacts/module-3/runs/20261020T033018.714107Z/format.txt); [Clang analyzer](artifacts/module-3/runs/20261020T033018.714107Z/static-analysis.txt), no analyzer warnings. |
| Native Module 3 | Pass: Cocoa [100%](artifacts/module-3/runs/20261020T033018.714107Z/native-module3-100.txt), [125%](artifacts/module-3/runs/20261020T033018.714107Z/native-module3-125.txt), [150%](artifacts/module-3/runs/20261020T033018.714107Z/native-module3-150.txt), [175%](artifacts/module-3/runs/20261020T033018.714107Z/native-module3-175.txt), [200%](artifacts/module-3/runs/20261020T033018.714107Z/native-module3-200.txt), no failed or skipped checks. Real focus, Enter/Escape, point dragging/cancel, fractional typed coordinates, B1 constraints/dimension edits, deletion/history, save/reopen and unsaved SIGKILL recovery. |
| Native regressions | Pass: [Module 1](artifacts/module-3/runs/20261020T033018.714107Z/native-module1-regression.txt), [Module 2](artifacts/module-3/runs/20261020T033018.714107Z/native-module2-regression.txt). |
| Ownership / preservation | Pass: [source and actual linked libraries](artifacts/module-3/runs/20261020T033018.714107Z/architecture.json); [97 research files unchanged, no remote](artifacts/module-3/runs/20261020T033018.714107Z/research-isolation.txt). |
| Visual review | Corrected [100% screenshot](artifacts/module-3/runs/20261020T033018.714107Z/native-100/sketches-native.png) and [200% screenshot](artifacts/module-3/runs/20261020T033018.714107Z/native-200/sketches-native.png) inspected. Wrapped labels and persistent action buttons are visible at 1280×800 logical size. Intermediate scales have native interaction and visibility assertions; this does not certify assistive technology. |
| Evidence identity | Pass: [tested source hashes](artifacts/module-3/runs/20261020T033018.714107Z/source-identity.json) matched the baseline code at its original closeout; [historical consistency record](artifacts/module-3/evidence-consistency-before-interaction-followup.json) also verifies unchanged pins, fixture hash and native DPRs. |
| Fresh-source reproduction | [Historical Pass](artifacts/module-3/clean-reproduction-before-interaction-followup.txt): fresh source export/configure/build and all 23 enabled CTest entries, using verified cached sources and existing local dependency binaries. No second dependency compilation, installed-build or other-OS claim. |

B1 inputs come from the unchanged frozen recipe: 80 × 50 mm outer rectangle and
20 × 20 mm rectangle at (20,20) mm. The sketch-stage implementation has eight
curves plus eight shared support points. Each rectangle has seven constraint
objects representing eight scalar equations (fixed X/Y, four orientations and two
lengths): 14 objects / 16 equations total. These counts distinguish construction
support points and multi-equation constraints from the recipe's eight curves and
16 scalar constraints. Later edits deliberately test Width = 90 mm, a canceled
100 mm draft, constraint deletion and a recoverable 91 mm revision. Solid B1
features, volume, holes/pattern and fillet remain unrun until later modules; no
frozen fixture recipe or budget was relaxed.

Original baseline tested source aggregate SHA-256: `b3f50dcd9731763694a958cef3f6f0d391035bbd5f0181927a850740ebf0775d`.
Frozen B1 SHA-256: `253e1a78dd30735dcdfe16fce3d1843b87123e747b1f4a31b68744cdc5811a52`. The baseline native renderer
reports OpenGL 4.1 on Apple M2; raw driver/DPR details are retained in each native
JSON. No SRS memory, FPS, latency or launch budget is inferred from these checks.

## Failures found and corrected

Historical failing evidence is retained, not recast as passing:

| Failure | Cause and verified correction |
|---|---|
| Initial disposable-root refusal | The harness used a new Module 3 name outside the explicitly allowed Module 2 test namespace. The allowlist now admits only marked Module 2/3 roots under the native temp directory; release roots remain refused. |
| Initial save-path refusal | The test attempted a saved snapshot inside private recovery storage. Tests now use a separate disposable saved-file root; the production guard remains. |
| [Initial unconstrained persistence mismatch](artifacts/module-3/persistence-3.3b.txt) | Normalization/denormalization introduced tiny rounding changes even with no constraints. Unconstrained coordinates now remain exact; solving/normalization is applied when needed. |
| [Initial native shader warning](artifacts/module-3/native-initial.txt) | The new test omitted the core-profile QSurfaceFormat setup used by the product and earlier tests. Its final harness requests the same format and asserts no renderer-initialization failure. Initial interaction success is not reported as clean graphics evidence. |
| [Undo boundary](artifacts/module-3/undo-boundary-confirmed-red.txt) | An empty sketch collection retained a row referencing the document properties entity, blocking document undo by FK. Empty collection rows are removed; [green](artifacts/module-3/undo-boundary-green.txt) covers undo/redo through document creation. |
| [Tangent branch](artifacts/module-3/tangent-side-red.txt) | A circle could move to the opposite tangent side with unchanged constraints. The adapter now rejects that branch transition; [green](artifacts/module-3/tangent-side-green.txt). |
| [Strengthened drag test](artifacts/module-3/runs/20261020T031302.700701Z/ctest.txt) | Pixel rounding let a neighboring line outrank the endpoint, so the initial drag did not start on a point. Points now have priority within the pick radius; [actual movement/cancel regression](artifacts/module-3/endpoint-picking-green.txt) passes. |
| [Typed coordinate precision](artifacts/module-3/typed-coordinate-red.txt) | Form input passed through the pointer snap grid, changing 12.7 mm to 13 mm. Typed coordinates now bypass the grid; the UI-to-worker regression checks 12.7 mm and 0.25 mm SI values directly. |
| [Native focus timing](artifacts/module-3/runs/20261020T032646.667348Z/native-module3-150.txt) | A direct assertion did not observe viewport focus at 150% during the native run. The harness now reacquires an active window and clicks the editor/viewport before asserting actual focus and testing Enter. The focus assertion remains required. |
| Initial visual review | A wide form label caused horizontal clipping, and accept/cancel scrolled with inputs. The form wraps labels and its action buttons remain outside the scroll area. Final native screenshots/visibility assertions are checked separately. |

No debug instrumentation or production test backdoor was introduced. Tests use
marked disposable roots and never target release recovery storage. Earlier
Module 0–2 evidence and all research files are preserved.

## Host, blockers and unresolved decisions

Fresh verifier inventory: macOS 26.6.2, arm64, Apple M2,
**8,589,934,592 bytes physical RAM (8 GiB)**. Dependencies remain Qt 6.10.0,
OCCT 7.9.2, bundled SQLite 3.50.4, Eigen 3.4.0 and pinned FreeCAD/PlaneGCS revision
`256fc7eff3379911ab5daf88e10182c509aa8052`. Compiler and source/tool archive pins
remain in the existing manifests. Cocoa scale overrides use one Retina display;
they do not prove five different monitors or all Mac GPUs.

| Gate / decision | State and owner |
|---|---|
| Windows/Linux builds, X11/Wayland, UI/focus/solver/persistence | Blocked: no native runners/session/compiler; OD-02 / platform maintainers. Candidate source/presets are not passing evidence. |
| Intel/minimum macOS, additional GPUs | Blocked: only the current arm64 M2 target is available; OD-02/OD-04. |
| OS-network-disabled workflow | Blocked: no controlled network-isolation procedure for native execution. Core code uses private local IPC and no account/network service; ordinary native execution is not a network-disabled acceptance claim. Module 9 owns the full offline matrix. |
| Native assistive technology, installed packages/signing/upgrades | Blocked pending real native environments/credentials; Modules 9/12 and OD-01/OD-08. |
| Full physical 8 GB resource budgets | Blocked for complete B0–B5 acceptance until the later engines/workloads exist. Current tests run on an actual 8 GiB host; no launch/FPS/memory/p95/SRS budget is inferred. Module 11 / OD-07. |
| Power loss, full-volume exhaustion, prior-format converters | Existing Module 2/10/12 blockers remain; process SIGKILL is not power-cut evidence. |
| Indexed sketch entities/feature dependencies and geometry support references | Collection representation is explicit; future migrations must preserve nested IDs. Exact feature/rebuild work belongs to Modules 4–5. |
| Advanced arc constraints and general nonlinear branch navigation | Unsupported combinations remain unavailable with reasons. Supported R1 combinations and conservative continuity policy are documented in the ADR. |
| Product identity/license/distribution and remote | Existing owner decisions remain open; no remote, signing, publication or history rewrite was performed. |

The original closeout stopped before Module 4. The later authorization covers
[Module 3 input repair](module-3-input-repair.md) and [4.0a–c workspace UI](module-4.md).
Stop before 4.1a exact features; further implementation requires the user's next instruction.

22 October 2026 interaction follow-up: [Centerpoint Arc pointer sizing](module-3-centerpoint-arc.md) adds the selected center → start → end workflow; historical Module 3 runs remain scoped to their original source and tests. Full platform/device acceptance remains Blocked.

22 October subsequent follow-up: [all three Arc methods and EDP graph comparison](module-3-all-arcs.md) extends 3.1b/3.2a/3.2b with live creation, tangent/normal/endpoint relations and Line transition; original runs are preserved as historical.
