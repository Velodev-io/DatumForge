# CM5.c — durable solid intents, provenance and explicit repair

3 November 2026. Authorized by “continue” after verified CM5.b. Base `2f20c2d`,
branch `review/cm5-c-20261103`. Preserve deferred 6.1b/6.4c/7.4c, native worker
ownership and physical 8 GB baseline. Stop before CM5.d and Module 11. Commit
and push verified scope on this review branch; no merge or force-push.

## Chosen storage and execution

Extend the existing support feature collection with optional `modeling` metadata:
`{version:1, intents:[CM5.a maps], provenance:[...]}`. Preserve original supports
and base features separately; replay them and then CM5.b intents exactly. Retain
the existing atomic `set_model` delta for transaction, undo/redo, recovery and
save/reopen. No authoritative BRep or per-edit shape snapshot is serialized.

The compatibility manifest appends `;modeling-intents=1` to model-supports=1.
Readers require rank 10 whenever active or retained history contains modeling
metadata. Older projects open unchanged; new semantics require Save As to a new
copy before overwriting a prior-format saved source. Never lower the requirement
after undo. Unknown versions, forged lower manifests, extra keys, identity
collisions and replay/provenance mismatches fail explicitly.

`SetModelSupports` accepts optional `intents`. On existing modeling projects,
omission preserves existing intents; an explicit array replaces them, including
an explicit empty array to delete. This protects earlier support/sketch editors
from silently deleting CM5 definitions. New native authoring remains CM5.d.
Accepted state exposes typed modeling metadata to the client; existing original
support/feature projections remain unchanged. All exact evaluation and writes
stay in the sole worker; failed or canceled edits preserve the accepted revision.

## Provenance

Persist per-intent identity/kind/operation, original target selection, output body
identities and actual kernel Boolean/draft Generated/Modified/Deleted maps.
NewBody profile provenance is its original semantic definition; no synthetic
Boolean is invented. Query-only cap checks do not masquerade as construction
history. Rebuild and compare the entire bounded provenance deterministically on
load/undo/redo; semantic definitions remain authoritative. Use existing byte,
feature/body/graph/history admission rather than raising capacity limits.

## Repair

Topology references keep their original definitions. Exact unique lineage may
resolve them, with explicit resolution evidence; missing, split, merged or
ambiguous references require a replacement. Add a read-only modeling-reference
candidate query that stops before the selected intent and inspects its actual
producing stage. Repair takes the complete pending base model/intents, owner UUID,
a whitelisted reference path, a current typed candidate and a fingerprint of the
entire pending definition. Check revision, fingerprint and candidate membership
before replacement, then rebuild every dependent intent and commit atomically.

Paths cover direct start/end/axis/target/neutral/pull references and selected
draft faces. The exact candidate endpoint supports Face/Edge/Vertex topology;
changing a missing source/member or body selection is an explicit complete-model
edit and must validate/rebuild before admission. No nearest-face or display-index
repair. Ordinary support repair retains saved CM5 intents as well.

## Verification

Test production SQLite/OCCT replay with retained identities and independent volume
equations: commit/dedup, edit/upstream rebuild, failed/canceled edits, undo/redo,
saved reopen, worker recovery, original-source byte preservation and new-copy
upgrade. Exercise forged metadata/provenance/lower history manifest refusal and
explicit missing/split topology repair with stale fingerprint/candidate rejection.
Check real client/worker transport against accepted modeling state and stale
revisions. Marked disposable roots only; refuse release/private recovery paths.
Run full enabled CTest, pinned format, changed-source Clang analysis, ownership,
coverage/spec checks and frozen source identity. Do not claim native control,
other-platform, installed, whole-course or physical-resource acceptance. Existing
CM5.b geometry combination restrictions remain unchanged.
