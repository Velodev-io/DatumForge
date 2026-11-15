# Module 9 — unified interaction, accessibility, offline operation and input safety

Date: 22 October 2026. Authorized after Module 8; stop before Module 11 after
Module 10. Preserve prior 6.1b/6.4c/7.4c deferrals. [Interaction/privacy contracts](../decisions/module-9-interaction.md)
define shared actions and aliases, native command palette, shortcut preferences,
familiar/simplified layouts, PropertyManager Return/Escape, accessible naming,
non-color states, diagnostic bounds and offline verification.

## Reproduction and evidence boundaries

Run `python3 tools/verify_modules8_10.py --native --clean`, then
`python3 docs/scripts/check_specs.py --write-report`.

`module9.privacy` checks 256 KiB rotation, safe command allowlisting and default
export projection (including poisoned extra log fields), oversized drawing text,
nonfinite and malformed packed vectors. Prior malformed IPC/project/expression/
import corpora remain enabled. Arbitrary kernel parser expansion is still the
deferred Module 6 gate, not a guarantee provided by these checks.

`module9.interaction` tests keyboard New, command search/aliases, Drawing edit,
Return accept, Escape cancel, Tab traversal, save, layout semantics, Qt accessible
editable roles/names and shortcut conflict refusal. Native Cocoa variants run at
100/125/150/175/200% effective scaling; tests verify visible fixed acceptance
controls at the 1280×800 logical baseline. Seeded CAD and earlier native workflows
are tested separately; this is not a claim that every complete AC workflow has
been run exclusively by keyboard or with an assistive technology.

`tools/offline_workflows.py` runs real sketch/Part/history/exchange/assembly/drawing
IPC workflows under an OS sandbox denying IP networking. IPv4 TCP, IPv4 UDP and
IPv6 TCP must first return OS PermissionError; local Unix sockets remain usable.
No account, external asset, service, telemetry or cloud backend is needed. Offline
passes do not certify installed releases or an unavailable operating system.

Focused privacy, headless interaction and Cocoa interaction checks passed, as did
the OS-denied network workflow run. Final available-Mac verification is recorded below.
Development logs remain in `artifacts/module-9/dev`.

## Native review and remaining gates

The qualitative reference is the preserved atlas's `UI_DESIGN_NOTES.md`: top
commands/search, contextual tabs, left Model/Properties, central model/sheet,
optional right resources and bottom document/status context. This is an original
native design, not exact SolidWorks pixel parity. Native screenshot review is
recorded with the final run, separately from numerical correctness.

Actual VoiceOver/NVDA/Orca combinations, physical keyboard/pointing-device matrix,
OS-native picker automation, Windows/Linux/minimum macOS/other GPUs, independent
physical displays and installed builds remain Blocked/Not run. Qt accessibility
interface queries are narrower evidence. Full 9.2b/9.4a acceptance remains Blocked.
Physical 8 GB/performance/resource acceptance stays with Module 11, not started.


## Final available-Mac verification

Run `20261022T031119.905623Z` on macOS-26.6.2-arm64-arm-64bit-Mach-O / Apple M2 /
**8,589,934,592 physical RAM bytes (8 GiB)**. Branch
`feature/modules-8-10-drawings-hardening`, commit `d1fce435b1e9e4f05b7f36fab6392f870fedd8d7` plus all preserved dirty inputs.
Aggregate tested source SHA-256: `eb6225b47536ec9bcc9a864d10dc132b55de935857c2a9ea831cb26e8d13b0de`.

[Verification](artifacts/module-10/runs/20261022T031119.905623Z/verification.json),
[source identity](artifacts/module-10/runs/20261022T031119.905623Z/source-identity.json),
[complete CTest](artifacts/module-10/runs/20261022T031119.905623Z/ctest.txt),
[architecture](artifacts/module-10/runs/20261022T031119.905623Z/architecture.json),
[offline](artifacts/module-10/runs/20261022T031119.905623Z/offline/offline.json),
[visual review](artifacts/module-10/runs/20261022T031119.905623Z/visual-review.json), and
[clean reproduction](artifacts/module-10/runs/20261022T031119.905623Z/clean-reproduction-details.txt)
record exact commands and outcomes. **All 52 enabled CTest cases passed**; the
intentional failure probe remains Disabled. Configure/build, formatting/static
analysis, ownership/research isolation, verified network-denial workflows and
frozen corpora, five-scale Cocoa drawing/keyboard checks, prior native Modules 1–7
and fresh source application reproduction passed. Dependency archives/pins and
existing dependency binaries were reused; dependencies were not rebuilt and no
installed release is certified.

[Actual B5 PDF](artifacts/module-10/runs/20261022T031119.905623Z/module8-B5.pdf) and its
[PDFKit inspection](artifacts/module-10/runs/20261022T031119.905623Z/pdf/pdf-inspection.json)
are separate from screenshots. [Exact CAD crash matrix](artifacts/module-10/runs/20261022T031119.905623Z/module10-fault-matrix.json)
has 29 SIGKILL cases (27 exact CAD and two PDF publication cuts); [prior storage matrix](artifacts/module-10/runs/20261022T031119.905623Z/module2-fault-matrix.json)
has 35 cases. Earlier development failures and superseded verification runs are retained. No failed/unrun check is promoted to full acceptance.

Deferred **6.1b/6.4c/7.4c remain Blocked**, unchanged. Full native/platform/device/
assistive-technology/physical-resource/filesystem/installed acceptance remains
Blocked or Not run as stated above. **Stop before Module 11. No commit or push.**

[Final evidence consistency](artifacts/module-10/runs/20261022T031119.905623Z/evidence-consistency.json) and
[documentation structure check](artifacts/module-10/runs/20261022T031119.905623Z/spec-validation.json) passed. The latter
checks documentation structure and links only, not product acceptance.
