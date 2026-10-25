# Module 6 — neutral exchange and portable Part copies

Date: 21 October 2026. **Status: implementation complete for the available-Mac
Part workflow; available-Mac verification complete.** Full resource/platform acceptance
remains Blocked. The user explicitly authorized Module 6 with “can you continue
with module 6”; see the [scope record](../decisions/implementation-scope.json).
Stop before **Module 7 / 7.1a**.

Existing uncommitted Module 5 work, documentation, research and prior evidence are
preserved on topic branch `feature/module-6-exchange`. No commit, push or history
rewrite is part of this work. [Adapter decisions](../decisions/module-6-exchange.md)
record contracts, source ownership, translation loss, jobs and limits.

## Implementation

| Task | Implemented and tested scope / remaining gate |
|---|---|
| 6.1a | Real pinned OCCT STEP/IGES readers, explicit file/override units and None/Fix healing, positive-volume closed solid validity/tolerance checks, imported-body feature with provenance/report, atomic source-blob/feature/undo commit. Names/colors/PMI/history loss is explicit. |
| 6.1b | Background staging/hashing/translation, monotonic progress, responsive IPC cancellation, 100 MiB/record/entity/topology admission and no source modification. Parser phases may delay cancellation. Arbitrary hostile-kernel allocation and broad peak-memory acceptance remain Blocked. |
| 6.2a | Current-valid-revision STEP/IGES export, all bodies or one body; STL explicit units/chord detail, serial cancelable meshing and triangle admission. Stale checkpoints are refused. |
| 6.2b | Sibling temporary output, source-independent round-trip volume/area/solid checks, checksum, final cancel check/flush/atomic publication, explicit overwrite choice, durable export outcome and loss report. Prior destination survives injected pre-publication failure/cancel. |
| 6.3a | Validated manifest of every project-local Part definition by ID/revision and retained authoritative source by hash/size, embedded into a standalone portable snapshot; missing/corrupt source refuses closure. Assembly/drawing closure remains Modules 7–8. |
| 6.3b | Explicit Copied policy, new project identity, stable local entity/document IDs, source project/revision manifest, no implicit overwrite; original active project/saved path retained. |
| 6.4a | Frozen B4 10×20×30 mm box, STEP/IGES in mm/cm/m/in; deliberately wrong override, healing, named/colored source loss warning, surface-only refusal, invalid/truncated/parser/oversized/record-expansion and cancellation variants. Broader industrial corpus remains a resource-validation gate. |
| 6.4b | Portable Part copy, remove original source and saved project, reopen/trim/collect/rebuild/recover, verify new identity and manifest; imported-body hole and undo/redo prove the result is exact editable geometry. |
| 6.4c | Malformed/oversized/expansion/parser cases, corrupt/missing embedded sources, injected StorageFull, stale-revision and canceled-output preservation pass in focused tests. Full hostile-input allocation/physical-volume/platform acceptance remains Blocked. |

Only the worker holds exact shapes and writes. The UI uses semantic definitions
and display meshes. `exchange-v1` extends the existing compatibility manifest;
older workers refuse unsupported semantics without rewriting originals. Sources
remain authoritative across current features, last-valid checkpoints, history,
saved snapshots and recovery; the result cache is disposable. Import/export jobs
run serially on one background thread while the worker IPC loop handles cancellation.

## Reproduction and evidence

```sh
python3 tools/verify_module6.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

The [verification record](artifacts/module-6/verification.json) links an immutable
timestamped run directory. Tests require marked disposable temp roots and refuse
release/unmarked recovery directories. They create their own windows/workers and
do not modify a preexisting app session. Native tests use automated Qt file
pickers on Cocoa, with five effective-scale overrides on one Retina display.
Headless CTest does not establish graphics. Clean reproduction builds a fresh
source export using verified cached dependency archives and existing pinned
library binaries; it does not recompile every dependency or test an installed
release. Sampled physical footprints cover the explicit test/worker family and
can double count shared pages or miss short peaks; no unique GPU/physical B4
acceptance claim follows.

## Actual final verification

Final run: `20261021T113246.906093Z`. Host: macOS 26.6.2 arm64, Apple M2,
**8,589,934,592 physical RAM bytes (8 GiB)**. The tested input identity is commit
`d1fce435b1e9e4f05b7f36fab6392f870fedd8d7` plus dirty source hashes on
`feature/module-6-exchange`; aggregate source SHA-256:
`65f95249614ea220ecd49d182479f0e66fdade9c954e97e711984703560213d0`. [Source identity](artifacts/module-6/runs/20261021T113246.906093Z/source-identity.json)
and [consistency](artifacts/module-6/runs/20261021T113246.906093Z/evidence-consistency.json)
confirm unchanged tested inputs and dependency/tool/Qt-patch/B4 pins.

| Verification | Actual result | Evidence |
|---|---|---|
| Configure/build | Pass | [commands/results](artifacts/module-6/runs/20261021T113246.906093Z/verification.json) |
| Full enabled CTest | 39 passed, zero failures; 144.65 seconds including runner overhead; deliberate failing probe stays Disabled | [CTest](artifacts/module-6/runs/20261021T113246.906093Z/ctest.txt) |
| Format / analyzer | Pass; no analyzer warnings | [format](artifacts/module-6/runs/20261021T113246.906093Z/format.txt), [analyzer](artifacts/module-6/runs/20261021T113246.906093Z/static-analysis.txt) |
| Ownership / research | Pass; desktop has no OCCT/SQLite/Qt Test link; research isolation unchanged | [ownership](artifacts/module-6/runs/20261021T113246.906093Z/architecture.json), [research](artifacts/module-6/runs/20261021T113246.906093Z/research-isolation.txt) |
| Native exchange | Pass at 100/125/150/175/200%; no failed or skipped cases | [native results](artifacts/module-6/runs/20261021T113246.906093Z/verification.json) |
| Prior native regressions | Modules 1–5 Pass | [commands/results](artifacts/module-6/runs/20261021T113246.906093Z/verification.json) |
| Clean reproduction | Pass; fresh application source build and all 39 enabled tests | [clean run](artifacts/module-6/runs/20261021T113246.906093Z/clean-reproduction-details.txt) |
| Visual review | Pass for reviewed 100%/200% subset | [review](artifacts/module-6/runs/20261021T113246.906093Z/visual-review.json), [100%](artifacts/module-6/runs/20261021T113246.906093Z/native-100/exchange.png), [200%](artifacts/module-6/runs/20261021T113246.906093Z/native-200/exchange.png) |

The [B4 record](artifacts/module-6/runs/20261021T113246.906093Z/b4-correctness.json) preserves
nine per-file hashes, byte counts, declared units and before/after invariants.
Eight plain STEP/IGES unit variants and one named/colored STEP source each retain
**one solid, six faces, 6000 mm³ volume and 2200 mm² area** within the test tolerances.
STL tests independently read binary triangles and check mm/in coordinate extents.
The analytic box recipe remains frozen; this small corpus does not establish
large industrial-input acceptance.

Native runs report DPR 1/1.25/1.5/1.75/2, OpenGL 4.1 and one viewport initialization
without graphics failures. [Memory samples](artifacts/module-6/runs/20261021T113246.906093Z/memory-summary.json)
record conservative test/worker-family sampled maxima of 146,572,848 to
590,005,880 bytes across those scale runs. These include the kernel-linked test
harness, can double count shared pages and miss short phase peaks; they are not
full B4 peak/steady/GPU budget acceptance.

Tasks 6.1a, 6.2a–6.3b and 6.4a–b are checked for available-Mac scope. Tasks 6.1b
and 6.4c retain Blocked status for the full resource/parser/platform gates.
Documentation validation is recorded separately in `spec-validation.json` in
this run directory. Stop before **Module 7 / 7.1a**.

## Retained failures and corrections

The first build had an incorrectly named feature-model link target; corrected
before running the translator tests. Initial storage tests correctly refused a
new unregistered test-root prefix; only `cad-module6-*` marked disposable roots
were added to the allowlist. The portable-path test was corrected to compare
canonical Mac `/var`/`/private/var` paths. Initial Qt picker tests used selection
APIs ineffective on the visible dialog; entering the actual filename editor
exercised acceptance correctly. A stalled isolated development test was stopped;
no existing user app session was reused. Sandboxed local IPC is denied, so real
worker/native verification runs with authorized host-side local IPC access.

Native preview exposed cancellation below the scrollable report. Progress and
Cancel now remain outside that scroll area, while the options/report scroll.
The final native matrix verifies the revised layout. The first full verification passed all 39 enabled tests and formatting, then Clang flagged a potential allocation leak through the templated queued Qt callback. Typed Qt-owned job events replace that callback; the focused worker analyzer passes. Final verification reruns the complete checks after this change. Unsupported STL import is disabled, import progress reaches 100 at commit, and post-publication flush/ledger failures explicitly report PublicationUncertain.

## Remaining blockers

- Windows/Linux/X11/Wayland, Intel/minimum macOS, other GPUs/displays and installed
  release/signing/licensing gates are unrun.
- Physical mouse/trackpad behavior, assistive technology, and OS-native file picker
  automation remain separate native gates; current input is automated.
- Arbitrary high-expansion parser/kernel memory behavior is not bounded by the
  source-size cap. Post-translation topology/triangle checks cannot prove a
  pre-allocation bound. Full B4 8 GB peak/steady/shared/GPU/swap acceptance and a
  broader industrial corpus remain Blocked for Modules 10–11.
- Optional names/colors/PMI/structure are reported as lost in the solid-body
  adapter; original source metadata is retained only in its authoritative blob.
- Healing is bounded ShapeFix, not a guarantee that arbitrary damaged geometry
  becomes valid. Unsupported/open/inverted/excess-tolerance geometry is refused.
- Abrupt worker termination can leave transient import/export staging files. Normal success/cancel/failure removes them, but a full orphan-staging cleanup/retention proof remains assigned to Modules 10–11.
- A parser phase may not poll cancellation. The UI reports pending cancellation;
  a 120-second deadline stops an unresponsive worker with an uncertain outcome.
- Physical disk-full/power-loss and full filesystem/platform publication matrices
  remain Blocked; injected pre-publication failure and process recovery are
  narrower checks. Export crash after publication/before its outcome ledger may
  require file/checksum inspection.
- Portable copies include all local Part definitions. Rich assembly/drawing and
  selected-subassembly closure belongs to Modules 7–8 and is explicitly refused
  here when those document types exist.

## Subsequent Module 7 scope

The user explicitly authorized Module 7 on 21 October 2026 and deferred
Module 6.1b/6.4c for later. The stopping boundaries above describe the historical
run. See [Module 7 evidence](module-7.md) for the new assembly scope; the current
boundary is **Module 8 / 8.1a**. No earlier hardware/platform gate is closed by
that authorization.
