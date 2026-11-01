# CM4.a author review

Base: `c11c3da`; local branch: `review/cm4-a-20261101`.
Review type: separate author self-review of the final source diff, not an
independent reviewer. Automatic delegation is prohibited by workspace guidance.

## Scope checked

- Versioned reference parsing/encoding: strict fields and version, canonical
  identities, geometry roles, member-versus-topology selectors, finite parameters
  and endpoint/parameter exclusion. Exact topology resolution is not claimed.
- Graph admission and traversal: bounded nodes/references/members, unique node
  IDs across kinds, compatible source/member roles, document scope, all-node
  cycle detection, deterministic order and complete descendant closure.
- History validation: direct and transitive violations, including paths through
  legacy unordered nodes; existing feature history remains explicitly ordered.
- Production integration: the same graph validates both Pierce ordering and
  feature plans; legacy feature-only results and semantic collection encodings
  remain intact. Hash propagation includes upstream sketch definitions and
  leaves unrelated branches reusable.
- Storage integration: cross-Part shared profiles retain their source closure,
  including speculative replacements. Existing profile/project byte and count
  admission still applies. Real-store tests use marked disposable roots and
  cover rejected edits, revision retention, undo/redo and saved reopen.
- Architecture: graph/reference code contains no OCCT/SQL/solver objects;
  unchanged worker ownership and packaged-worker identity are checked.

## Assessment

No unresolved critical or important issue identified in this structural scope.
Full-suite/static/structural results are recorded in the companion verification
record. This author review does not authorize a merge.

Future datum/support callers must supply their complete history ordering and
perform worker-owned geometry resolution. General document closure, new support
schemas/manifests, atomic ModelEdit and exact/native workflows remain open in
CM4.b–e/shared work. The 22 CM4 course entries remain Not run. Other-platform,
native device, installed-build and physical resource gates are not judged passed.
