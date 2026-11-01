# CM4.b — exact datum and associative support engine

1 November 2026. User instruction: “go ahead with cm4.b”, followed by “continue”.
Branch `review/cm4-b-20261101`, base `997e1db`. [Contracts](../decisions/core-modeling-cm4-supports.md)
and [plan](../superpowers/plans/2026-11-01-cm4-b.md).

## Implementation scope

The production worker engine implements principal datums, nine scheduled plane
variants (the through-points/lines family has ThreePoints/LinePoint/TwoLines
methods), five scheduled axis variants, planar-face/datum sketch attachment,
exact Convert Entities and same-Part external sketch references. Rotation,
positive uniform scale, target-local translation, multiple offset planes and
normal reversal remain semantic inputs. Mixed history is validated before exact
evaluation; feature prefixes use the updated sketches and worker-owned cache.

`PreviewModelSupports` is a versioned, read-only project-worker command. It
returns bounded semantic frames/sketches/member mappings and exact measurements;
the desktop receives no kernel objects. Success, failure, canceled work and
restart do not write support definitions or advance durable project revision.
One running / one replaceable pending model preview uses revision/context/epoch/
generation fences. Later generated-member references are declared structurally
and verified against exact recomputation before use.

Converted conics/rational splines use exact affine projection of OCCT rational
control geometry, not sampled fitted curves. Straight projections retain lines.
Generated identities are derived from conversion and source semantic identities;
generated controls are fixed before solving. Degenerate projections, missing or
ambiguous topology, conflicting downstream inputs and budget excesses fail.

CM4.c durable definitions/manifests/reference evolution/repair, CM4.d native
controls/navigation and CM4.e full acceptance workflows remain open. General
external-document geometry edges remain explicitly unsupported; historical
cross-Part profile use remains supported. No installed/native/device/platform/
physical-resource acceptance is inferred. Preserve 6.1b/6.4c/7.4c and stop before
Module 11. This is engine scope, not an interactive completed CM4 product.

## Executable checks

| CTest | Scope |
| --- | --- |
| `cm4.support_model` | Strict versioned transient contracts, operand/parameter admission, multiple plane IDs, graph order/cycles and duplicate attachment rejection |
| `cm4.support_geometry` | Principal datums; all plane and axis families; oriented finite-edge parameters/normal; cylinder/cone axes and tangent surface; singularity, skew, coincident and ambiguous/missing topology refusal |
| `cm4.support_convert` | Orthographic line/point/arc/circle/ellipse/elliptical arc/parabola/rational-spline projection; 0.75 scale/rotation/translation; identity retention; analytic ellipse equation and downstream solid volume |
| `cm4.support_engine` | Exact feature-face → datum → supported sketch → solid history; direct face attachment; transformed face-boundary conversion; offset edits; generated external member chaining and forged declaration refusal; cycles/missing references and cancellation |
| `cm4.support_worker` | Real project-store preview, repeated requests, exact measurement revision, unchanged accepted state, stale revision and canceled request |
| `cm4.support_transport` | Real isolated process/socket/client; accepted-solid preservation; cancellation/queue drain; 20-draft coalescing, latest-result publication and worker restart |

Artifacts are under [CM4.b evidence](artifacts/core-modeling/cm4-b-20261101/).
The `*-api-red.txt` files are expected missing-header/link failures establishing
new APIs; behavior reds then use compiled real implementations. Method/selector,
orientation, generated-member and cancellation regressions retain their own
red/green logs. Some expanded regression families began green and are labeled
as regressions rather than new behavioral red cycles.

## Retained diagnosis and failures

- The default volume integration gave 3.1455317614497903e-6 m³ for a projected
  ellipse prism whose analytic volume is 3.1415926535897938e-6. Ordinary adaptive
  Gauss also failed this independent check despite a small internal estimate.
  Knot-aware Gauss–Kronrod gave 3.1415926535406057e-6. Spline-bounded solids now
  use that path with a checked error estimate; primitive solids retain their
  existing analytic integration. The unchanged analytic assertion passes.
  `projected-volume-*.txt` preserves the probes and failed attempts.
- Reversed edges initially retained the underlying forward tangent in NormalCurve;
  the oriented-edge red/green verifies the corrected tangent direction.
- An immediate canceled preview with an accepted solid left DisplayPart queued
  after the terminal result and CancelJob acknowledgement. The red timed out;
  pumping after the acknowledgement now drains the queue, without admitting
  another computation while stale Progress still owns the worker.
- The first generated-member external reference failed structural role lookup.
  Bounded declared members now participate in planning and must match exact
  output before use; a forged declaration is rejected.
- `midplane-ambiguity-red.txt` and `coalescing-restart.txt` were inadvertently
  run on the previous binary after a fixture compile error; they are not evidence
  for the added cases. Their `*-compiled-*` records and final suite supersede them.
  The QVariant fixture conversion was corrected without dropping assertions.
- Existing duplicate static-library linker warnings are retained in build logs.

## Final verification

Full development build and pinned formatting passed. Clang analysis passed all
15 changed translation units. The full suite passed **85 enabled CTests, zero
failures, in 398.70 seconds**; the deliberate harness failure probe was disabled.
All six new CM4.b entries and prior module/CM1–CM3 regressions passed. Ownership,
coverage, research isolation, specifications and frozen-source integrity passed.
All 232 source/configuration hashes stayed unchanged through verification.
The desktop has no exact-kernel/database/test dependency and the packaged worker
matches. [Machine-readable result](artifacts/core-modeling/cm4-b-20261101/verification.json).

CM4.b engine implementation and available-Mac development verification are **Pass**.
Native verification is **Not run** for these new tools; full CM4 acceptance stays
**Blocked**. Twenty engine coverage entries remain **In progress** for downstream
durable/native/full-workflow gates; navigation/visibility remain **Not run**.

Review is an author self-review under the workspace's no-automatic-delegation
rule; a separate review remains required before merging. Commit/push follows the
standing user instruction only after scoped verification passes.
