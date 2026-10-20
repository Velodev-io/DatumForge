# DatumForge publication and branch workflow

The repository was initially empty. Feature commits were reconstructed from saved local file trees, then the current working files were captured as a separate WIP snapshot. No previous commit history was replaced.

Branches are stacked in this order; review each against the preceding branch:

| Branch | Scope | Base |
| --- | --- | --- |
| `main` | Specifications, research and Module 0 build/feasibility foundation | Initial commit |
| `feature/native-workbench` | Native desktop, bounded IPC and exact-geometry worker, Module 1 | `main` |
| `feature/document-persistence` | Documents, history, save/recovery and worker dispatch, Module 2 | `feature/native-workbench` |
| `feature/parametric-sketches` | Planar sketches, constraints and typed expressions, Module 3 | `feature/document-persistence` |
| `feature/sketch-input` | Sketch interaction repairs and their evidence | `feature/parametric-sketches` |
| `feature/modeling-workspace` | Command tabs, left PropertyManager and direct editing, Module 4.0 | `feature/sketch-input` |
| `feature/solid-modeling` | Exact solid features, measurement and Module 4 verification | `feature/modeling-workspace` |
| `feature/model-evaluation-wip` | Incomplete Module 5 planning/cache/history work | `feature/solid-modeling` |

Module 5 remains WIP and is not merged into `main` or the solid-modeling branch. No Module 6 implementation is authorized. The branches preserve the sequence of the available saved snapshots; they are not independent implementations that can be merged in arbitrary order.

Historical branch identifiers and reviewer labels were anonymized for publication. Measured values and timestamps were retained. Historical source hashes describe the files before publication metadata changes; they are not hashes of the newly branded checkout. The research baseline is unchanged.

`.deps`, generated builds, local user data and credentials are excluded. The research folder is retained, including its documented installer inspection evidence. These research binaries are evidence, not application dependencies.

## Validation

Publication-time results are recorded in `docs/testing/artifacts/publication/checks.json` on the WIP branch. Historical module evidence is retained in its original module folders. Unavailable Windows/Linux, physical-device and full workload checks remain Blocked or Not run.

Publication checks passed for all 31 enabled tests on the available Mac solid-modeling source (one intentional failure probe disabled). Configure/build, formatting, Clang static analysis (48 project-owned translation units), ownership and research-integrity checks passed. Every published branch passes the specification structure check.

The frozen Module 5 snapshot is **not merge-ready**: `HistoryPanel.cpp` fails to compile because a CBOR value is passed directly as combo-box user data, and formatting fails. No Module 5 test pass is claimed. Newer live source edits were left untouched outside the frozen snapshot.
