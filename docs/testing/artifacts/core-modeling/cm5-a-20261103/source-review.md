# CM5.a author source review

3 November 2026. Compared the new parser, tests and CMake changes to CM5.a and
all 29 scheduled CM5 coverage variants. No automatic delegation; this is author
self-review. Independent review remains required before merge.

- Ownership: library uses Qt Core semantic types and the existing model graph;
  no OCCT/SQLite/UI types or new dependencies. No exact geometry or product
  command is added. The legacy feature parser/manifest remains unchanged.
- Admission: strict version/type/key checks; positive bounded SI length/angle,
  draft below pi/2, maximum one-turn revolve, explicit null target/direction2,
  region/curve-use/byte/reference/graph limits; no inactive parameter retention.
- Identity: sketch/curve UUIDs and typed reference roles; no display indices.
  Exact interval connectivity, topology/body membership and solid validity
  are deliberately not claimed by structural validation.
- Dependencies: source sketch and every contour member, start/end support,
  producing body stage, revolve/pull axis, neutral plane and draft face appear
  in the graph. `graph()` adds Sketch/Feature kind checks and complete history;
  `graphNode()` alone is not a complete admission API.
- Direction rules: signed start offset is independent of reverse; Midplane
  value is total; a second direction cannot coexist with Midplane or use its
  own Midplane mode. Draft parameters are independent per extrude direction.
- Thickness/profile rules: no open solid boss or non-thin open revolve; open
  extrude cut requires explicit side. Side and thickness are distinct from
  extrusion/revolve direction, with material-side conventions written down.
- Failure: immutable parser output, no writer/preview/accepted state touched.
  Validation uses the existing explicit domain errors and bounded graph.
- Found/corrected: draft face lexical document equality rejected equivalent
  Self/explicit-current-document aliases. A behavioral red was retained, then
  graph scope validation was used to admit the alias and still refuse foreign
  documents. Other malformed input and incompatible combinations remain refused.
- Verification boundary: contract CBOR round trips are not project persistence
  or native rendering evidence. No existing deferred gate or release acceptance
  is closed; CM5.b–e and Module 11 are not started.

The first full verification is pending at review time; the task evidence records
its actual result separately. No independent review or merge approval is implied.
