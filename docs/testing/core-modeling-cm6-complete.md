# CM6 implementation and review corrections

The 15 November branch added versioned intents, OCCT adapters, persistence and
native controls for CM6. Its original completion claim was withdrawn after the
16 November review reproduced wrong geometry, unstable output identities and
editor round-trip failures. The original tests passing did not establish full
course or native acceptance.

[Review repairs and verification](core-modeling-cm6-review.md) are the current
record. Exact face fillet/full round (R1), closed-face repetition (R4) and catalog
picker data (CM9/R2) remain Blocked. No restricted full-round production success
is claimed. The former edge-seeded full-round attempt was removed after it failed
a real parallel-sided box; parser acceptance of fake face tokens was not an exact
geometry test. Course-like fixtures are independent analytical fixtures, not
replays of complete T7 models.

CM6 remains In progress. Physical Windows/Linux, installed builds, assistive
technology/device checks and the physical 8 GB acceptance workload remain
unverified/deferred; gates 6.1b/6.4c/7.4c and the stop before Module 11 remain.
