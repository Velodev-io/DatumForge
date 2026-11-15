# CM4.e source review

Review base: `8ce8f2e`; review target: CM4.e working tree on
`review/cm4-e-20261103`. This is an author self-review, not an independent review.
The workspace's explicit instruction not to delegate automatically takes precedence
for this run. No merge decision is made here.

Reviewed the native editor's input/reference/preview/inspection/commit/cancel state
transitions, supported feature/sketch adapters, original-definition preservation,
complete dependency sequencing and worker-side candidate descriptions. Reviewed
real worker/native workflow tests and their retained failed attempts.

- Definitions and current typed candidates retain semantic identity. A replacement
  does not become accepted by being listed or selected; exact rebuild and the
  existing revision/fingerprint/candidate commit checks remain required.
- Conversion declarations come from successful exact previews. Generated output is
  excluded from unchanged base sketches; ordinary converted-sketch editing is
  refused, while original source edits rebuild the chain.
- Preview/cancel retain accepted state. Changing fields invalidates the prior
  preview and repair choice. Project/document/revision/epoch changes retire the
  modal draft. All geometry and writes remain worker-owned.
- History sequencing keeps valid existing priority and places new nodes after
  their declared inputs; missing roles and cycles still fail through the graph.
- Native feature/sketch draft repair retains the pending upstream edit and applies
  the selected repair in the same complete semantic transaction.
- No schema/resource limit increase, new network requirement, Module 11 work or
  deferred-gate change was introduced.

No additional Critical/Important issue was identified by this source review.
The earlier identified conversion endpoint default and descendant-draft issues
have retained focused regression results. Final regression/static/native results
are recorded separately by the timestamped verifier; this review does not imply
unrun verification passed.

Scope rulings retained from the task contract:

1. Exact support preview reports evaluated frames/members/volume while retaining
   the accepted mesh until Apply. A draft display mesh/manipulator was not added;
   the dialog explicitly describes this behavior. Cost: no live draft-solid visual
   comparison in this editor; full course/UI acceptance remains open.
2. Existing interactive drag/dimension previews retain planar scope; complete
   associated-model rebuild is required at the final supported-model admission
   boundary. Cost: an intermediate planar preview may not predict every downstream
   rebuild failure; no downstream success is claimed before the strict rebuild.
3. Native workflows cover representative exact constructions and transaction
   chains, not every page-indexed course/device/platform/resource case. All CM4
   coverage rows and full acceptance remain open until their broader evidence is
   available.

No additional deferred minor finding was recorded. Final approval before merge
still belongs to a reviewer; no branch is merged by this task.
