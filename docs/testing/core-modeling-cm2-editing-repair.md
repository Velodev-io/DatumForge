# CM2 editing review repair — 14 November 2026

Scope: repair the reported spline endpoint, point selection, offset-chain and
Arc/Spline dispatch defects and the additional geometry/constraint/resource bugs
found in the complete LineEditing review. This is a scoped production repair on
`review/cm2-editing-repair-20261114`, not full CM2 acceptance.

## Implementation

- Worker-only OCCT planar intersections replace sampled sign-change roots for
  advanced boundaries. Power trim uses the same finite exact intersections.
  Dynamic extend retains hits beyond the source line. Overlaps and single-hit
  circle trims are rejected explicitly.
- Advanced fillet contacts come from the existing OCCT curve adapter and tangent
  circle solver. Endpoint matching evaluates the actual trimmed range. Contacts
  trim parameters rather than poles or conic center/focus controls. Existing
  line/arc solvers retain their behavior, filtering finite arc contacts before
  selecting a center. Consumed line/arc operands are rejected.
- Offset traversal tracks oriented free endpoints, ignores unrelated unsupported
  geometry, handles circles alone and rejects branches/disconnected selections.
  Signed line/arc offsets join on their exact supports. Line and arc caps use the
  actual oriented ends and tangents, including arc-ended chains.
- Chamfer setbacks use the actual included angle and reject consumed segments.
  Pattern counts are checked before integer conversion/multiplication, explicit
  circular spacing is accepted and unsupported operands refuse the whole request.
- Copies preserve shared point identity per selection/instance. Copies remain
  geometry copies; existing source constraints are not cloned. In-place transforms
  disclose changed-geometry relations. In-place mirror preserves entity IDs.
  Construction groups require their existing group editor.
- The original native trim/extend help now correctly describes advanced boundary
  support. No additional native edit controls or modules were started.

## Verification

Evidence: [artifacts](artifacts/core-modeling/cm2-editing-repair-20261114/).

- Added semantic regression assertions to `cm2.editing`: both spline endpoints,
  curve-locus preservation and tangency, Arc/Spline and conic point selection,
  unrelated splines/circle auto-chain, reversed chains, mixed joins, arc caps,
  dynamic extend, duplicated roots, tangency, actual chamfer angle, oversize
  refusal, constrained moves/mirror, shared copied points, circular spacing,
  invalid count refusal, unsupported pattern refusal and single-hit circle trim.
- Focused `cm2.editing` passed after the finite-contact candidate filtering repair.
- Full development build passed. Complete host suite result is recorded below.
- All four changed translation units (engine, regression test and UI help)
  passed Clang analysis.
- Pinned formatting, architecture/ownership, source isolation and core coverage
  checks passed. Documentation links/specification checks passed.

The first full-suite attempt ran inside the sandbox, failed local worker IPC
contracts, and was interrupted. Its `ctest.txt` is preserved. The host run uses the
existing tests' marked disposable roots; no user recovery/project roots are used.

Full host suite: **Pass — all 97 enabled tests**, including `cm2.editing` and
`cm2.editing_contracts` (448.04 seconds). The intentional
`harness.deliberate_failure` remains disabled (98 tests registered). See
`ctest-host.txt` for individual results. All five changed source/header hashes in
`sources.json` still match the verified build snapshot. This is development-build
regression evidence, not full product acceptance.

## Acceptance limits

Native interaction/device/scaling checks, installed builds, Windows/Linux and
physical 8 GB resource acceptance were not run. The broader native controls and
course corpus remain open. Deferred 6.1b/6.4c/7.4c and the stop before Module 11
remain unchanged. No merge is authorized by this repair.
