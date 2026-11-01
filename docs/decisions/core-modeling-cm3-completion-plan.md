# CM3 completion — proposed TDD plan

Date: 31 October 2026. Status: Interface/test priorities explicitly approved
by the user on 31 October 2026. This document does not
claim implementation or passing evidence. Starting point: `c30de06`.

## Scope and interfaces

Complete CM3.a–e and coverage CM-076–097 within the existing native planar-sketch
architecture. Preserve Modules 0–10 evidence, CM1/CM2 work and deferred
6.1b/6.4c/7.4c. Do not start Module 11 or add assembly/drawing functionality.
Unrun platform, physical-device, installed-build and full resource acceptance
remain explicitly Blocked; they cannot be established by a development build.

Keep the existing Dimension action and label double-click editor. Add controls
for reversing supported linear dimensions, unit-aware spin increments and the
stored drawing-import mark. The mark is model metadata only; actual drawing
import belongs outside this scope. Keep exact Preview/Apply/Restore/Cancel and
stable dimension identities. Changing a readout must not weaken a driver.

Add Path length and datum-based multi-target Ordinate actions. Path membership
uses stable curve identities in an ordered, connected, nonbranching chain, with
explicit rejection of missing/ambiguous/disconnected members. Exact worker
geometry owns measurement and driving equations; sampled display polylines are
never authoritative lengths. Support the current planar curve families where
the pinned exact kernel/solver supports the required equations; unresolved
prerequisites must be recorded rather than hidden behind an approximation.

Add an Automatic relations toggle, visible inference guides and a proposal
review showing the inferred relations. Proximity alone never silently writes a
relation. Confirmation adds the listed relations to the draft; Cancel adds none.
Use the course's Horizontal, Vertical, Coincident, Midpoint, Intersection,
Tangent and Perpendicular relation families and current supported operands.

Add Fully Define with All/Selected scope, a deterministic relation/dimension
proposal, remaining-freedom feedback and explicit Apply/Cancel. Existing
constraints and shape are preserved; blanket replacement by Fixed constraints
does not implement this behavior. A selected subset must not freeze unrelated
geometry. Unsupported or inconsistent inputs retain the prior accepted model.

Extend Select/drag to worker-solved temporary targets. Preview requests are
bounded/coalesced and generation fenced. The UI retains the last valid result
when a target is impossible; release accepts that last valid draft once, and
Escape restores the pregesture draft. Temporary targets are never persisted as
ordinary sketch constraints. The single worker retains numerical/exact ownership.

Reuse SetSketches as the durable publication boundary. Extend versioned sketch
data/manifests only when needed for path membership and new dimension metadata.
New worker preview commands must return typed semantic candidates/diagnostics
through the existing client lifecycle; no second solver or database enters UI.
The exact command shapes will be documented and tested at each vertical slice.

## Prioritized behavior tests and vertical slices

For each item, write **one** failing public-interface behavior test, retain its
red result, implement only that behavior, obtain green, then choose the next
test. The rows below are a backlog, not permission to write all tests first.

1. **Dimension identity defect:** edit the second of same-kind driving/reference
   dimensions through its actual label/editor. Only that ID changes; the driver,
   expression and geometry survive Apply, Finish, undo/redo and saved reopen.
2. **Remaining dimension controls:** reverse valid signed linear/centerline
   senses; increment/decrement in document units; preserve expressions until
   an explicit numeric edit; retain drawing marks and label placement. Reject
   invalid units/ranges without changing the draft. Test each behavior separately.
3. **Ordinate workflow:** choose a datum or sketch origin, place multiple X/Y
   dimensions with stable identities and orderly labels, edit a target value,
   and verify upstream datum changes, cancellation and saved reopen.
4. **Driving/reference path length:** measure and drive an ordered connected
   chain against independent lengths; change individual segment shape while
   retaining total length. Cover line/arc and the remaining current curve
   families incrementally, as well as closed/reversed chains, identity changes,
   conflicts and explicit ambiguity. No chord-length substitute for curves.
5. **Constrained dragging:** begin with one dimensioned movable endpoint, then
   incrementally cover lines, circles/arcs, groups and supported exact curves.
   Preserve driving relations and path length, last-valid preview, cancellation,
   branch guards, one undo step and stale/out-of-order response rejection.
6. **Inference:** propose and display each supported relation family without
   writing it; confirm/reject through the native UI, test toggle behavior and
   competing/ambiguous guides, then persist only confirmed results.
7. **Fully Define:** deterministic, nonredundant proposals for All and Selected
   scopes, preserving shape/IDs/drivers. Add geometry-family cases one at a time;
   verify remaining degrees of freedom and failure/cancel behavior independently.
8. **Integrated course/reliability corpus:** two-point Horizontal, revolved-profile
   diameter, constant path length under drag, mixed curve sketches, unit and
   keyboard workflows. Extend public-store/worker tests for duplicate/failed/
   canceled commands, undo/reopen/portable copy, restart and interrupted commit.
   Run these as additional red/green behavior slices where coverage is missing.

## Final verification and evidence

- Build and run focused tests after each slice. Refactor only while green, and
  rerun the affected behavior tests after each refactor.
- Run the complete enabled CTest suite, pinned formatting, changed-source Clang
  analysis, architecture, isolation, coverage and specification checks on the
  final source identity. Use only marked disposable project/recovery roots.
- Run actual native mm/in and keyboard/pointer workflows at 100/125/150/175/200%
  effective scales on the available Mac. Retain failed attempts and classify
  environmental blockers honestly; offscreen runs are not native verification.
- Update contracts, the page-indexed coverage register, task statuses and
  `docs/testing/core-modeling.md` with per-behavior evidence. Distinguish
  production implementation, available-Mac verification and full acceptance.
- Keep work on a separate review branch. No merge or push. Local feature
  commits remain governed by the existing authorization and passing evidence.

## Source basis

- `docs/CORE_MODELING.md`, CM3.a–e and the completion contract.
- `docs/research/core-modeling-coverage.json`, CM-076–097.
- Preserved T4 text/pages 11–12, 20–24 and 32–33; preserved T5 pages 12–15.
- Existing schema-6/schema-7 dimension contracts and retained failed/passing
  CM3 verification records.
- `/private/tmp/datumforge-cm3-review/CM3_REVIEW.md`, reproduced identity defect.
