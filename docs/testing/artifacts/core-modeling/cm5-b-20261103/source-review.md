# CM5.b author source review

3 November 2026. Author self-review of new profile/extent/feature adapters,
shared Part helpers, optional preview transport, tests and verification runner.
No automatic delegation. Independent review is still required before merge.

- Ownership: new kernel types live only under geometry/occt; UI-facing intent
  remains semantic. Existing single worker performs exact evaluation. No new
  dependency, durable schema/manifest, product control or unsupported menu entry.
- References: complete graph/history checks cover original support closure and
  all appended intents. Body lookup checks producing-stage membership, local
  scope and current writable shape. Exact axes include converted derived curves.
- Units and signs: SI semantics convert to kernel mm once; normal, reverse,
  second direction, normal surface offset side, thickness material side and
  draft side have independent equations. Midplane uses total depth/thickness.
- Profiles: directed exact parameter intervals; no fitted projection. Contour
  connectivity, area/topology/self-intersection, hole containment/separation,
  cell overlap, offset branch/collapse and end closure checked. Exact 16-boundary
  admission and combined 100-feature/128-KiB budget preserved.
- Extents: exact support halfspaces/solid cells construct cap geometry; seed rays
  and finite bounds choose workspace and branch only. Entire result boundary and
  full profile footprints are checked against supports and original lateral walls.
  Workspace end caps cannot survive as an accidental fixed-depth substitution.
- Draft/Boolean: analytic draft requires validity without topology shortcuts.
  Finite support domains are rechecked after drafting. Add/Cut preserve one current
  target solid, intended volume change and unchanged accepted input on failure.
- Cancellation/preview: original PreviewModelSupports request/response lifecycle
  and bounded result retained; optional typed intent maps only. Stale revisions
  fail before evaluation. No writes, operation dedup record or durable acceptance
  occurs on preview. Long kernel calls retain existing cooperative granularity.
- Review corrections retained with regressions: wrong open-offset mode, missing
  offset orientation context, null feature-prism result, partial trimmed support
  coverage, crossing supports, touching holes, combined metadata budget, exact
  boundary budget and unavailable converted axes. No failed attempt relabeled.
- Limits: drafted open non-thin cuts/nonplanar neutral draft explicitly refused;
  imported/complex offset/draft corpus remains open. Full CM5 acceptance and
  CM5.c–e are not established by this engine task. Deferred gates unchanged.

Final full verification is recorded separately; no success is inferred from this
review. The core-modeling evidence records actual commands, outcomes and limits.
