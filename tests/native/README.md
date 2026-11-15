# Native verification matrix (0.4e)

Automated CTest `native.process_transport` uses real processes and local sockets
but an offscreen Qt platform. It cannot establish native graphics or accessibility.
`slice_desktop --native-test OUTPUT.png` uses the current native Qt platform,
exercises box rendering, candidate face picking, resize/docking, 30 box/100-instance
camera samples and worker restart. A screenshot must be inspected. Rendering errors
fail rather than silently using an offscreen or software fallback.

Manual/native matrix, each result recorded independently:

| Target | Required checks | Owner if unavailable |
|---|---|---|
| Mac arm64 and x86_64 | GPU/OS, 100–200% scale, dock/resize, face/edge IDs, overlays, camera, exact worker measure, terminate/restart | Module 0, renderer owner |
| Windows 11 x86_64 | Native driver/HiDPI, focus/shortcuts, process/pipe permissions and restart | Module 0 + Modules 1/9 |
| Ubuntu 22.04/24.04 x86_64 X11 | Native GL, keyboard, font/scaling, local-socket access | Module 0 + Modules 1/9 |
| Ubuntu Wayland | Same plus compositor/docking behavior; X11 results do not cover Wayland | Module 0 + Modules 1/9 |
| Accessibility | VoiceOver / NVDA / Orca, names/roles, tab order, keyboard-only forms and non-color states | OD-08, Module 9 |
| Installed packages | Clean OS, no dev libs/services, offline launch/save/recover/export, upgrade/uninstall preservation | Module 12 |
| Physical RAM | One real 8 GB system for each published platform, B0–B5 with raw resource samples | OD-07, Module 11 |

Never alter the user's OS display/accessibility settings just to turn a pending
check into Pass. Missing hosts/hardware, untested scales and assistive technologies
are Blocked. Record native observations separately from numerical tests.

Module 5 adds `module5_contracts nativeHistory` behind `CAD_NATIVE_TESTS=1`.
Use `tools/verify_module5.py --native --clean` to retain each scale's interaction
log, screenshots and graphics facts. The test launches a disposable project,
previews/commits history changes, preserves visibly stale results, explicitly
repairs a missing edge, exercises undo/redo/save/reopen and kills only its own
worker before recovery. Existing Module 1–4 native workflows are regressions.
The five scale factors map to DPR 1/1.25/1.5/1.75/2 on the available Retina display;
they do not replace physical-device, assistive-technology or other-platform checks.


Module 6 adds `module6_contracts nativeExchange` behind `CAD_NATIVE_TESTS=1`.
`tools/verify_module6.py --native --clean` records import/export/portable-copy Qt
file-picker workflows, source-independent reopen, responsive IPC cancellation,
recovery and screenshots at five effective scales. It samples the explicitly
owned test/worker family. Qt file pickers run on the native Cocoa session; this
does not automate the OS-native file picker or certify physical-device behavior.

Module 7 adds `module7_contracts nativeAssembly` behind `CAD_NATIVE_TESTS=1`.
`tools/verify_module7.py --native --clean` tests native assembly insertion/placement,
shared geometry, exact-reference choices, mate conflict/cancel, transformed
component picking and synthetic mouse drag/cancel, interference highlight,
state controls/undo, portable copy and worker recovery at five effective scales.
Prior Module 1–6 native cases are regressions. Physical input/accessibility and
other-platform acceptance remain Blocked.

The all-arc Module 3 follow-up uses `module3_contracts nativeArcs` behind
`CAD_NATIVE_TESTS=1`. `tools/verify_all_arcs.py --native` records Centerpoint,
3 Point and Tangent/Normal native mm/in workflows and the EDP Line transition at
five effective scales, plus prior sketch regressions at the endpoint scales.
QTest events and visual review do not certify physical input or assistive technology.
