# Development and review conventions

Read docs/README.md and its linked specifications before editing. Follow the documented module and lettered task order; blocked prerequisites permit only independent later work.
Record Pass, Fail, Blocked, Not run or Not applicable with reason. Only check a
task after verifying its whole completion condition. Modules 1–3 were authorized on 2026-10-19; Module 4 was explicitly authorized on 2026-10-20. Stop before Module 5 (5.1a) and ask before proceeding. Keep research unchanged and preserve evidence of failed runs.

Use short topic branches from `main`; review changes with actual test evidence.
Never rewrite shared history. This repository has no remote until supplied.
No initial commit is required to run tests; source tree hashes identify uncommitted
builds. All CAD spikes live under tests; Modules 1–4 production code lives under apps and src.

Build commands and dependency options are recorded in `cmake/README.md`.
Candidate presets are not verified-platform claims. Run the native manual matrix
in addition to CTest; offscreen tests do not establish GPU/accessibility support.

Avoid global package installation or changing unrelated projects. Do not track
downloaded libraries, actual user models, recovery data, logs with user paths,
credentials or signing material. Track dependency pins, notices and safe synthetic
fixture definitions. Keep tests deterministic and verify failure exit status.
