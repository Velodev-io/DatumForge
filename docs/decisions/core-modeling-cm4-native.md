# CM4.d — native datum selection, visibility and Normal To

3 November 2026. Authorized by the user's “okk go ahead with cm$.d”, referring
to CM4.d in the preceding review. Branch `review/cm4-d-20261103`; scoped
verification before commit/push, no merge. CM4.e and deferred 6.1b/6.4c/7.4c
remain open. Stop before Module 11.

Implement the CM4.d task as written: select accepted datum planes/axes, toggle
their display, and navigate Normal To an accepted plane or sketch. This task
does not add support authoring, Convert Entities authoring or the complete
upstream-edit/repair course workflows required by CM4.e. No control claims those
operations. Existing supported-Part loose edit restrictions remain enforced.

The worker's read-only, revision-checked `SupportGeometry` command evaluates
original accepted definitions and publishes bounded typed datum frames/directions.
The client validates finite orthonormal frames, unit axes, identity and count.
No exact/kernel/database object enters the UI. The response neither previews nor
commits a model. Epoch/session/document/revision changes invalidate display data;
obsolete replies cannot install geometry. Failed inspection never substitutes a
saved frame. Principal datums use explicit semantic constants; accepted sketch
frames come from the already validated project projection.

The model tree and a native datum browser expose every output of multiple-plane
definitions with stable IDs, named construction mode, selection and visibility.
Viewport datum picking uses projected display geometry, a separate Datum filter,
and only visible current primitives. Hidden rows remain selectable for inspection
and Normal To. Axis Normal To is disabled because an axis alone does not define
an in-plane orientation. Display toggles are session-local per project/document;
they do not mutate definitions, revisions, history, saved state or geometry.

Normal To uses the selected plane's origin and full orthonormal u/v frame in an
orthographic camera, including tilted/reversed planes. It preserves the current
zoom, centers the selected origin, and clears pan. Standard views and orbit reset
or update that orientation consistently. Datum-only Parts remain navigable without
a display mesh. Inspection can be refreshed explicitly after failure.

Verify real worker transport, selection through tree/browser/viewport, independent
screen projection of tilted/reversed frames, axis refusal, hidden picking refusal,
revision retention, document switch/restart, and offscreen/native UI at five
effective scales. Physical devices, other OSes, installed builds, physical 8 GB
and full CM4.e acceptance remain separate unrun gates.
