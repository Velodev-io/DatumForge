# CM4.e — native associative model editing and workflow verification

3 November 2026. Authorized by “go ahead” after the completed CM4.d task.
Branch `review/cm4-e-20261103`, based on `8ce8f2e`. Preserve CM4.d evidence,
deferred 6.1b/6.4c/7.4c and the stop before Module 11. Commit/push verified
scope for review; no merge or force-push.

The native support editor authors/edits all existing datum modes, original sketch
attachments and Convert Entities definitions using typed source selections.
Lengths use document units; angles use degrees, converted to SI/radians at the
boundary. All exact topology choices and repair evidence come from the worker.
No display index, nearest shape or saved evaluated frame becomes a source.
Exact rebuild preview is required before applying the unchanged draft. The
accepted display remains visible until commit; the preview presents exact frame,
member and volume results. Cancel restores accepted display and changes no revision.

The editor owns a bounded complete semantic draft. Adding/changing a dependency
sequences all nodes topologically while preserving existing priority where possible;
cycles and missing roles fail. Multiple changes can be staged before one atomic
commit. Missing/ambiguous topology is inspected at the owner's exact history stage;
current unique candidates have worker-derived descriptions. A user-chosen repair
is previewed, then `ResolveSupportReference` checks the original inspection
fingerprint and revision and atomically commits the complete repaired draft.
Non-topological missing members/objects require explicit source reselection.

Native feature/sketch editing of supported Parts submits `SetModelSupports` with
the original support collections. Unchanged evaluated descendants retain their
original base definitions; attached sketch frame placement stays worker-owned.
Converted members are read-only in the ordinary sketch editor: edits go to the
source or conversion definition. No generated curve is copied into base geometry.
Existing pure-planar drag/dimension interaction previews retain their planar scope;
the associated model's strict rebuild is the final admission/commit boundary.

All geometry evaluation and persistence remain in the sole worker. The UI contains
only semantic drafts/display data. Project/document/revision/epoch changes retire
the editor's draft; changing controls invalidates prior previews and repair choices.
Failure/cancellation preserves accepted state. New commands or raised resource
limits are not required. Saved/recovered files rebuild the original definitions.

Verify analytical datum families and downstream volumes/frames, real worker
save/reopen/undo/redo, missing/split repair, rejected stale/forged commands,
conversions/external references and native pointer/keyboard workflows in mm/in at
five effective scales. Full source-course, platform, installed, device/assistive
and physical-8-GB resource acceptance stay distinct from development verification.
