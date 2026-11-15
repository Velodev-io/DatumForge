# CM4.c author review

This is an author self-review, not an independent approval. No automatic
sub-agent delegation was used; separate review is required before merge.

Reviewed the complete change across semantic definitions, exact source-stage
lookup, writer transactions, saved admission, retained history, worker jobs,
client state and coverage/specification boundaries.

- Original support inputs remain separate from evaluated sketches. Reopen and
  recovery reconstruct original inputs and independently compare stored results.
- An atomic model delta includes both feature and sketch collections and the
  exact former collection. Undo before support adoption and redo are exercised.
- The new manifest is sticky across undo and other document types; unknown or
  downgraded retained semantics are refused. The slot construction-group field
  collision was reproduced by the original regression and corrected.
- Reference lookup never chooses a nearest/display-ordinal candidate. Unique
  signatures and one-to-one kernel Modified lineage are the only automatic paths.
- Explicit repair checks the complete model fingerprint, current project revision,
  typed operand slot and current exact candidate membership before commit.
- Canceled/stale/invalid drafts preserve the accepted revision. The worker remains
  the sole exact-geometry owner/writer; the client receives only semantic data.
- Existing loose feature/sketch edits refuse supported Parts rather than silently
  erase their associations. Complete typed replacement remains the explicit path
  for missing source objects or changed generated sketch members.
- No module/acceptance boundaries are expanded. New native controls remain CM4.d;
  complete course/native workflow acceptance stays CM4.e. Preserve deferred
  6.1b/6.4c/7.4c, physical 8 GB and all unrun platforms/installed/device gates.

Review reproduced two additional cases in an isolated exact-kernel probe:
source-stage isolation from later splits and attached-conversion operand
inspection before consuming its frame. Both cases now have compiled
production-API red/green regressions. Evolution
filters kernel history to the producing stage, and attachment inspection precedes
conversion-frame consumption. Final unchanged-source verification passed:
87 enabled CTests, 12 Clang analyses,
format/ownership/specification checks and 246 source/configuration hashes. No
remaining blocker was found in the scoped CM4.c review. The unchanged CM3 exact-
response repeatability test failed once; six isolated repeats and the final full
run passed, and its unresolved cause remains documented. Native/full acceptance
boundaries remain open. This author review is not an independent merge approval.
