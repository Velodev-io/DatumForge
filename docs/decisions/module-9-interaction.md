# Module 9 interaction, offline and diagnostic contracts

Authorized 22 October 2026 after Module 8. Full platform, assistive-technology,
physical-input, resource and installed-build acceptance remains separate.

The existing command tabs and left PropertyManager remain. Find Command uses the
same QActions as menus/toolbars, searchable by label, ID and explicit aliases
(boss/pad, component/mate, PDF/print). Disabled entries show availability reasons.
Native Find opens a keyboard-searchable palette; Return runs the selected enabled
command, Escape dismisses it. Shortcut preferences update the same actions,
retain native key rendering, reject duplicate shortcuts and reserve Enter/Escape/
Tab for command/dialog editing. Preferences are local QSettings, never model data.

Familiar and simplified layouts share commands and semantics. Simplification
hides the optional resource dock/rail, preserving their layout state. Native
QMainWindow versioned dock restoration remains the layout authority. Part,
assembly and drawing accept/cancel controls remain native widgets. Return in an
active PropertyManager accepts a valid enabled draft; Escape uses the existing
central cancellation route. Sketch canvas Return retains its preview gate.
Drawings have fixed Preview/Accept/Cancel controls outside their scrolling form.
Dialogs keep native Return/Escape and platform file picker behavior.

Accessible names are populated from form buddies and meaningful control names;
project structure, document tabs, command tabs, save state and diagnostics have
explicit names. State messages use text, including Stale/Dangling/Unresolved and
Saved/Recovery. Qt accessibility-interface tests are a narrower check than actual
VoiceOver, NVDA or Orca operation. Untested combinations remain Blocked.

All engine/project/import/expression/IPC limits introduced in earlier modules
remain. Drawing vectors additionally use packed float32 sheet coordinates:
60,000 points maximum, under the existing 1 MiB control-envelope admission. Page
coordinates are <=2000 mm, so quantization is below the 0.01 mm path tolerance.
The desktop validates drawing counts, finite coordinates, bounds and labels before
painting. No imported macros, shell expressions or arbitrary scripts are executed.
Kernel translator expansion is still the deferred Module 6.1b/6.4c gate.

Job timing is measured locally from dispatch to the first terminal control reply;
mesh transfer completion and CPU/physical-memory use are distinct measurements.
Per-feature compute/cache timing comes from worker evaluation diagnostics.
The local journal accepts only command/status/time/allocation fields, rotates at
64 KiB across four files (256 KiB cap), and does not accept raw messages, model
text, names or source paths. Diagnostic export is a visible user action using a
chosen JSON destination. Reports project allowed fields even if an extra field
is injected into a local log. No background uploader or telemetry is present.

`tools/offline_workflows.py` executes actual core IPC workflows in a macOS sandbox
that denies IP networking while retaining Unix local sockets. It first requires
OS PermissionError for IPv4 TCP/UDP and IPv6 TCP probes. An inaccessible endpoint
alone does not count as offline proof. Other OS sandbox mechanisms are not claimed.
