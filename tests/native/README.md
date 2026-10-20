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
