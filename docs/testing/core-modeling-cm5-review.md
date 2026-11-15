# CM5 native review fixes — 15 November 2026

Scope: repair the ten findings in the review of `b1017df` and `150302e`.
Work is isolated on `review/cm5-integration-review-20261115`; the original
worktree and its uncommitted edits are preserved. No merge is authorized.

## Implementation and regression mapping

| Finding | Repair | Regression evidence |
|---|---|---|
| 1: references rebound on edit | Match complete typed reference maps; retain unavailable original definitions for explicit repair | `savedParametersAndPointerRegression`: second-face identity |
| 2: saved parameters lost | Restore revolve Midplane; independent Direction 1/2 surface offsets and sides | `savedParametersAndPointerRegression`: 3/5 mm independent offsets, side and revolve mode |
| 3: obsolete producing stage | Worker inspects the exact prefix before the edited intent and publishes `{source, body}` selections; compound and solid stages are supported | `t6SelectedContoursAndReuse`: third Boolean names second operation's stage |
| 4: invalid body/absent vertex candidates | Bounded read-only `ModelingInputs` returns Object body selections and exact Face/Edge/Vertex identities; mode-aware UI filtering | `cm5.worker`: eight real box vertices, correct source, stale/type/cancel refusal; `typedInputsAndParameterHandles`: Vertex start and UpToBody preview |
| 5: no contour selection UI | Native boundary editor chooses stable sketch curve IDs, normalized intervals and traversal, organized as regions/outer boundaries/holes; exact worker validates the result | `t6SelectedContoursAndReuse`: select circle through actual dialog, saved SelectedRegions and independent volume |
| 6: incorrect routing | New empty Parts use CM5 authoring; existing legacy models retain legacy editing; unsupported downstream legacy commands are disabled on CM5 solids and refused at the panel boundary | `typedInputsAndParameterHandles`; adapted legacy `module4.lifecycle` preserves downstream treatment tests |
| 7: overwritten/missing handle paths | Explicit handle selector maps direction/depth/draft/angle/thickness to the actual field; worker-resolved frames/axes support display; acceptance clears handles | `typedInputsAndParameterHandles`: independent Direction 2 and thickness; native frame/mesh cases |
| 8: event-dependent angle and wrong clamp | Advance pointer baseline per event; separate draft and revolution angular limits | `savedParametersAndPointerRegression`: stationary pointer, 180 to 190 degree revolution |
| 9: hard-coded repair path | Select any whitelisted reference path; verify current pending-definition snapshot, revision and candidate membership; preserve other references and preview after prefix inspection | `t8StandaloneDraftAndRepair`: missing face, failed-preview revision preservation, repair, accept and saved reopen |
| 10: overstated native/course evidence | Separate native Cocoa runs assert observed DPR and OpenGL initialization; rectangle fixtures are named as fixtures; historical completion claims are superseded | `verify_cm5_review.py --native`, source hashes, scale logs and screenshots |

All exact inspection/evaluation and writes stay in the existing serial worker.
No kernel objects or SQL are introduced into desktop code. Existing byte/body/
feature limits, sticky manifests, omitted-versus-empty intent semantics and
marked disposable storage roots are retained. Inspection is read-only and uses
revision/generation correlation; failed or canceled operations do not replace
accepted model state. Original pending reference definitions are retained rather
than replaced by a nearby/display-index reference.

Native reference controls also permit explicit whole-definition replacement of
plane/axis/body selections. The candidate-repair endpoint remains scoped to exact
Face/Edge/Vertex replacements. Unsupported analytic geometry is still refused.
Depth handles for a surface start require a uniquely resolved planar display
station; curved starts retain numeric editing and exact preview without inventing
a plane or a cached blind-depth substitute. Thickness handles edit the named
semantic parameter; their line is a parameter guide, not a measured wall edge.

## Verification

Scoped implementation and available-Mac review verification **Pass**.

- [Full regression log](artifacts/core-modeling/cm5-review-20261115/verification/20261115T045527.463720Z/ctest.txt): 98 enabled tests passed, zero failures; one intentional failure probe Disabled.
- [Final follow-up](artifacts/core-modeling/cm5-review-20261115/final-followup/verification.json): all 12 affected tests, ten changed/affected-source Clang analyses, pinned format, ownership/isolation/coverage/specification checks and five native scale runs passed.
- Each native scale ran the complete scoped workflow executable: 14 QtTest entries passed (including setup/cleanup), zero failures. Actual Cocoa DPR and OpenGL initialization were asserted; mm/in screenshots and an inclined sketch frame were retained.
- [Final source identity](artifacts/core-modeling/cm5-review-20261115/final-followup/source-identity.json) remained unchanged during the follow-up. It records exact differences from the full-suite snapshot: explicit Qt tree ownership, stronger test startup guards/removal of an unused variable, and lexical path filtering in the verification harness. These cleanup changes received the affected-test/analysis/native follow-up; a second full suite is not claimed.
- [Author source review](artifacts/core-modeling/cm5-review-20261115/review/source-review.md) is separate from independent merge approval.

Reproduction command:

```sh
python3 tools/verify_cm5_review.py --native
```

The runner retains a timestamped source identity, full enabled CTest run, pinned
format, changed-source Clang analysis, architecture/isolation/spec/coverage checks,
and native cases at observed 100/125/150/175/200% effective DPR. Native cases use
Cocoa, actual OpenGL rendering, mm/in, an inclined sketch frame and synthetic Qt
pointer events. A passing headless test is not graphics/device acceptance.

Earlier investigative runs are retained separately. The first full run failed
`module4.lifecycle` because its harness assumed the old first-feature route; the
updated legacy-fixture lifecycle passed its affected follow-up. The contour
regression exposed compound-body stage inspection and was fixed at the shared
worker inspection boundary. Earlier native screenshots exposed missing Core
Profile setup in the test executable; it now matches the production application
and asserts successful rendering before capturing native evidence.

## Acceptance boundary

This closes the stated review defects only after the mapped checks pass. It does
not close the complete CM5 course corpus or all 29 workflow rows. The 10 mm/35°
and 15 mm/0° rectangle and 3 mm thin rectangle are analytical UI fixtures, not the
actual T5 tutorial profiles. Full source-course sketch construction, broader exact
combinations and physical/platform/resource acceptance remain open/Blocked.

Preserve deferred gates 6.1b/6.4c/7.4c, the physical 8 GB baseline and the stop
before Module 11. Author source review is not independent merge approval.
