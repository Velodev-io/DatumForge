# Author source review: CM5 native repairs

Reviewed scope: typed prefix inspection, exact reference identity, display frames,
native semantic controls, contour boundary editor, scoped repair, pointer input
and compatibility with the legacy feature editor. This is author self-review,
not independent merge approval.

- Exact geometry, stage/body membership and topological identities are obtained
  in the existing worker. No UI kernel/SQL ownership or saved schema change.
- The input-inspection contract validates revision, version, fields, typed
  references, finite vectors and existing control/body limits. Cancellation and
  stale replies retain serialized worker ownership until terminal completion.
- Body-stage inspection handles bare solids and compound results. A three-feature
  Boolean regression verifies that source stage and body identity remain distinct.
- Original references remain intact when a display candidate is absent. Repairs
  check the current pending snapshot, revision, selected path and current candidate
  membership before whole-model exact rebuilding. Preview waits for prefix
  inspection, and the accepted revision is unchanged on a failed draft.
- No-op Direction 2 reverse control was removed: the versioned contract defines
  Direction 2 as opposite Direction 1. Independent extents/draft/offset/thickness
  values retain their actual semantic fields and unit conversion.
- Reference-terminated features retain references. Their handles never substitute
  blind depth. Nonplanar start supports without a unique display station keep
  numeric exact editing rather than a fabricated handle plane.
- The contour editor builds explicit curve/interval/traversal selections, with
  holes and regions; the exact worker validates closure, nesting and intersections.
  Sketches are retained and reused by multiple intents.
- Existing legacy models keep their feature editor. CM5-only bodies do not enable
  downstream legacy operations whose evaluator cannot consume them.
- Native tests now request the production OpenGL profile and assert both actual
  DPR and rendered initialization. Source-course profiles remain separate from
  the seeded analytical rectangles. No platform/device/resource gate is closed.

The initial lifecycle failure and compound-stage diagnosis are retained beside
this review. Final verification and frozen source identity are recorded in the
timestamped verification directory; only its final status establishes a pass.

Trailing horizontal whitespace from three Qt diagnostic logs was trimmed for
repository whitespace checks; diagnostic content was retained.
