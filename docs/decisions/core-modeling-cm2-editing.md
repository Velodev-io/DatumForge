# CM2 editing contracts — first production slice

25 October 2026. CM2 is authorized; this slice starts with straight-line Trim to
closest, Trim away inside/outside two boundaries, Dynamic extend, and Extend to
apparent intersection. Power trim strokes, Corner, treatments, offsets and
transforms remain open. No whole CM2 acceptance is implied.

- Exact editing runs in the sole worker, on SI semantic geometry. The desktop
  supplies semantic entity IDs and a pick coordinate. Display meshes never
  determine intersections. Finite line and circle/arc boundaries are supported;
  conic/spline boundaries are explicitly refused in this first slice.
- Closest removes the interval containing the projected pick, bounded by actual
  intersections or source endpoints. Inside/outside require exactly two distinct
  boundary entities with one unambiguous intersection each. Dynamic extend uses
  the nearest intersection beyond the picked half's endpoint, within each finite
  boundary. Apparent intersection uses one explicitly selected line's infinite
  support. Overlap, an intersection pick, a missing hit or a degenerate fragment
  is an error; accepted geometry remains unchanged.
- A unique remaining line keeps its semantic ID. An interior split retires its
  source ID and allocates independent descendant IDs. No descendant is selected
  by display order. Existing references to a retired ID require explicit repair;
  no automatic substitution is performed. Unchanged endpoint IDs are retained;
  cut endpoints get new point IDs so neighboring geometry cannot move silently.
- Changed line relations and detached endpoint relations are enumerated with
  IDs, kinds and expressions before applying a draft. Horizontal/Vertical on a
  uniquely retained line are invariant and kept. All other affected constraints,
  including driving dimensions, require an explicit removal decision. Cancel is
  always available. Construction-group members are refused until an explicit
  group-edit/dissolution contract exists; rectangle/slot intent is never erased.
- Sketch schema 5 stores bounded edit records: operation/mode, source, ordered
  descendant IDs and removed constraint IDs. They are historical provenance,
  not a replay engine or live associative offset/pattern definition. Descendants
  may later be edited/deleted. A sticky `sketch-edits=1` manifest protects retained
  undo/redo/checkpoint content; old versions remain readable without rewriting
  source files. Existing 256 entities/512 constraints/16 sketches/256 KiB caps
  and 64 edit-record cap apply; reaching a cap refuses the edit atomically.
- Preview returns a candidate and disclosure without changing draft or durable
  revision. Apply checks the complete source draft still matches, makes one draft
  undo step and requires disclosure acknowledgement. The existing SetSketches
  transaction owns durable commit, deduplication, dependency evaluation, recovery
  and undo. Generation/context/revision checks reject stale preview replies; only
  one exact preview runs with one replaceable pending preview.

Native keyboard/property selection and pointer pick workflows, mm/in units,
preview/cancel, disclosure, draft/durable undo, save/reopen and worker restart are
required for this slice. Other OS/device/resource/installed gates and
6.1b/6.4c/7.4c remain unchanged. Stop before Module 11; no commit or push.
