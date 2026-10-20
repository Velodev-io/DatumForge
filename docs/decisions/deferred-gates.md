# Unavailable/deferred verification gates — 0.9c

| Gate | State / missing evidence | Owner / blocking effect |
|---|---|---|
| Windows 11 x86_64 | Blocked: no accessible native runner/compiler supplied | OD-02, Modules 0/1/9/12; cannot claim Windows support |
| Ubuntu x86_64 X11 and Wayland | Blocked: no accessible native runner/session supplied | OD-02, Modules 0/1/9/12; both sessions needed |
| Mac x86_64 and macOS 13 minimum | Blocked: only macOS 26.6.2 arm64 available | OD-02, Modules 0/12; deployment-target flag is not runtime evidence |
| Real 8 GB Windows/Linux systems | Blocked: unavailable; this Mac is physically 8 GB | OD-07, Module 11; cannot infer cross-platform performance |
| VoiceOver/NVDA/Orca and all scales | Blocked until native assistive-technology/scaling procedures executed | OD-08, Module 9; AX presence alone insufficient |
| Renderer longevity / driver failure | Open feasibility decision; no implemented Metal backend | OD-04, Modules 0/1; must resolve before renderer support claims |
| Full B2/B3 semantic evaluation | Not run; recipes/hashes exist, history/assembly engine absent | Modules 5/7; complexity counts and memory budgets remain pending |
| Curated colored/damaged/mixed-unit B4 | Not run; analytic box translator corpus only in Module 0 | Module 6; full import robustness and measured counts pending |
| B5 associative drawings/PDF | Not run; recipe exists, no drawing implementation | Module 8; paper scale/BOM/associativity acceptance pending |
| Physical power-loss filesystem behavior | Blocked: process-exit matrix is not power-loss hardware evidence | Modules 10/12; document actual filesystem guarantees |
| Product identity/license/signing | Blocked: product owner choices/credentials unavailable | OD-01, Module 12; no distribution/publication implied |
| Installed clean builds/upgrades | Blocked until actual product packages exist | Module 12; source archive is not an installer |
| Other-platform formatter/toolchains | Blocked: only Mac arm64 clang-format 21.1.8 wheel and native compiler verified | Build maintainer; select native tool pins on each host |

Module 1 remains unstarted and requires the user's approval. Release gates are
retained even if available-target experiments pass. No budget or R1 scope revision
is proposed merely to close an unavailable check.
