# Module 7 — rigid assemblies, mates, loading and interference

Date: 21–22 October 2026. The user explicitly authorized Module 7 and deferred
Module 6.1b/6.4c for later. Stop before **Module 8 / 8.1a**. Work is on
`feature/module-7-assemblies`; preexisting Module 5/6 changes and research are
preserved. No commit, push or history rewrite is part of this work.

Implementation and [adapter contracts](../decisions/module-7-assemblies.md) cover
shared Part/subassembly occurrences, independent rigid poses and state flags,
grounding/cycle checks, six rigid mate kinds, freedom/conflict diagnostics,
transactional drag/preview/accept/cancel, exact reference repair and explicit
replacement. The native Assembly tab and left PropertyManager expose these flows.
Exact interference distinguishes overlap/touch/disjoint/failure and highlights a
result. Portable copies retain assembly semantics, sources and BOM-source quantities
under a new project identity. Drawings remain Module 8.

## Tests and reproduction

```sh
python3 tools/verify_module7.py --native --clean
python3 docs/scripts/check_specs.py --write-report
```

| Test | Scope |
|---|---|
| `module7.model` | Planar/axial/angle chain with 6/2/0 remaining freedoms, perpendicular and point distance, singular seeds, conservative conflict diagnostics, deterministic solutions, missing occurrences, invalid quaternion, cycles and flattened admission |
| `module7.geometry` | Analytic 10×20×30 mm box: 3000 mm³ overlap, touching and disjoint; stale/missing/nonsolid refusal, coarse unload/reload, shared rigid mesh ranges, transformed points, v2 round trip and malformed range refusal |
| `module7.persistence` | Idempotence, mate conflicts/noncommit preview, state/BOM/cache distinctions, nested cycle refusal, portable reopen without original file, restart recovery, exact-face invalidation after real part-depth edit, explicit repair/replacement and undo |
| `module7.b3` | Frozen B3: 100 occurrences, 10 B1-derived definitions including final fillet, 99 distance mates, 495 free dimensions, bounded visible triangles, shared geometry, coarse unload, undo, save/reopen and recovery |
| `module7.contracts` | Real worker IPC: Assembly UI insertion/rotation, mates/conflict/cancel, exact-reference choices, overlap highlight, state/undo, portable copy and worker restart |
| `nativeAssembly` | Same flows on Cocoa with real OpenGL; transformed occurrence picking and synthetic mouse drag/cancel, five effective scale settings and screenshots |

All persistence/native tests create marked disposable temp roots and refuse release
recovery directories. Test root allowlisting adds only `cad-module7-*`. A cold
unloaded occurrence intentionally shows its state without resolving geometry; an
explicit load rebuilds from authoritative inputs. Coarse display cache is process
local, not authoritative portable data. See the adapter contract for cache and solver
limits. Full physical/GPU/FPS/platform acceptance remains Blocked.

## Verification results

Available-Mac final verification passed. Development failures/logs are retained under
`artifacts/module-7/dev`. Early builds exposed typed CBOR/Qt/OCCT integration errors;
focused persistence tests caught an extra compatibility-manifest SQL bind. Both
were corrected before final verification. The native drag test initially projected
a point on a silhouette and missed; it now finds a visible transformed face, asserts
the picked occurrence, then verifies the mouse drag signal and cancellable preview.
The upstream-edit fixture also updates the hole support origin when changing the
part depth, keeping the intended source edit geometrically valid.

Existing regression expectations change only where the product now extends:
Module 4 asserts four command tabs and contextual Assembly enablement; Module 6
uses a Drawing for its future-closure refusal, because Assembly closure is now
implemented. Its Part/source/export checks remain intact.

## Final available-Mac evidence

Run `20261021T183127.304947Z`: macOS 26.6.2 arm64 / Apple M2 / **8,589,934,592 physical RAM
bytes (8 GiB)**. Topic branch `feature/module-7-assemblies`, commit
`d1fce435b1e9e4f05b7f36fab6392f870fedd8d7` plus the recorded dirty input hashes. Aggregate source
SHA-256: `bdcb3bafcf4425e7e92242b2ac85117d52d63b88bf71ce5f7572c524de7fdf3a`. [Source identity](artifacts/module-7/runs/20261021T183127.304947Z/source-identity.json)
and [consistency](artifacts/module-7/runs/20261021T183127.304947Z/evidence-consistency.json) record unchanged tested
inputs and the pinned dependency/tool/Qt-patch/B3 recipe hashes.

| Verification | Result | Evidence |
|---|---|---|
| Configure/build | Pass | [commands/results](artifacts/module-7/runs/20261021T183127.304947Z/verification.json) |
| Full enabled CTest | **44 passed**, zero failures, 174.33 seconds including runner overhead; intentional failure probe Disabled | [CTest](artifacts/module-7/runs/20261021T183127.304947Z/ctest.txt) |
| B3 | 100 occurrences, 10 B1-derived exact definitions, 99 mates, 495 free dimensions, 6,600 unique / 66,000 visible triangles; correctness/recovery Pass | [B3](artifacts/module-7/runs/20261021T183127.304947Z/b3-correctness.json) |
| Formatting/static analysis | Pass, no analyzer warnings | [format](artifacts/module-7/runs/20261021T183127.304947Z/format.txt), [analysis](artifacts/module-7/runs/20261021T183127.304947Z/static-analysis.txt) |
| Ownership/research | Pass; desktop has no OCCT/SQLite/Qt Test dependency; research unchanged | [ownership](artifacts/module-7/runs/20261021T183127.304947Z/architecture.json), [isolation](artifacts/module-7/runs/20261021T183127.304947Z/research-isolation.txt) |
| Native assembly | Pass at 100/125/150/175/200%; no failed/skipped cases | [results](artifacts/module-7/runs/20261021T183127.304947Z/verification.json) |
| Prior native workflows | Modules 1–6 Pass | [results](artifacts/module-7/runs/20261021T183127.304947Z/verification.json) |
| Fresh source reproduction | Pass; fresh application build plus all 44 enabled tests | [clean run](artifacts/module-7/runs/20261021T183127.304947Z/clean-reproduction-details.txt) |
| Visual subset | Reviewed 100% and 200% screenshots; no observed viewport/layout corruption | [review](artifacts/module-7/runs/20261021T183127.304947Z/visual-review.json), [100%](artifacts/module-7/runs/20261021T183127.304947Z/native-100/assembly.png), [200%](artifacts/module-7/runs/20261021T183127.304947Z/native-200/assembly.png) |
| Physical footprint sampling | Completed; full resource acceptance Blocked | [summary](artifacts/module-7/runs/20261021T183127.304947Z/memory-summary.json) |

B3's measured test-process sampled maximum footprint was **34,423,384 bytes**.
Native test/worker-family sampled maxima ranged from **128,943,592 to 301,663,816
bytes** across scales. The [raw-sample summary](artifacts/module-7/runs/20261021T183127.304947Z/memory-summary.json) explains
ownership, interval and limitations; these values do not establish full SRS
steady/peak/shared/GPU/FPS acceptance. Native screenshots report DPR 1 through 2,
OpenGL 4.1 and a single viewport initialization without graphics failures.

Clean reproduction uses a disposable source export with hash-verified cached
archives and existing pinned dependency binaries; it does not rebuild every
dependency or prove installed-release behavior. Documentation validation is
recorded separately in [spec validation](artifacts/module-7/runs/20261021T183127.304947Z/spec-validation.json).

Tasks 7.1a–7.4b are checked for the available-Mac scope. **7.4c remains Blocked**
for the full resource/platform gate, with its portable/cache/recovery subset Pass.
Module 6.1b/6.4c remain deferred. Stop before **Module 8 / 8.1a**.

## Remaining gates

- Module 6.1b/6.4c parser/peak/platform gates stay deferred and Blocked.
- Full NFR-002 physical/shared/GPU/steady/peak/FPS loading budgets, all 100-cycle
  assembly resource-retention checks, Windows/Linux/other GPU/minimum macOS,
  physical-device/accessibility and installed builds remain Blocked.
- Arbitrary nonlinear mate convergence, minimum conflict-set proofs, external-file
  linking, selected-subgraph portable copies and drawing/BOM layout are not claimed.
- Coarse array/entry accounting is not a proof of OCCT live-object or process/GPU
  memory bounds. Sampled process trees can double count shared pages and miss peaks.
