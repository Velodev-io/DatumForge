# Resource and native measurement procedure (0.5c–0.5d)

SRS §6 budgets are unchanged. Record fixture recipe **and evaluated** hash/counts,
build/source/lock hashes, exact OS/compiler/CPU/RAM/GPU/driver, viewport, scale,
background apps/load, thermal state, battery/AC and power mode. Missing telemetry
is Blocked; a missing metric is never a zero. Preserve errors and failed samples.

Use a 1280×800 logical window, 100% scale, fixed orthographic/perspective camera,
0.2 mm mesh deviation for the box experiment; freeze production detail for each
fixture before measuring. Repeat separately at 200% and native device scale.
At least five genuinely cold launches (reboot/validated cold cache; do not relabel
a fresh process as cold), ten warm process launches and thirty scripted actions
after warm-up. Use nearest-rank p95 and median, record all samples; sample size
is provisional and must be revisited if variance makes it statistically weak.

Launch timing starts before process creation, stops at responsive editor readiness
with the required data displayed. B0 waits thirty seconds with no document. B1–B4
settle after evaluation; record thirty seconds steady state and the sampled peak
including compute/mesh/import/save. Capture sub-250 ms peaks via OS peak counters
or an allocation profiler where available; polling alone cannot prove peak budget.

`python3 tools/sample_memory.py UI_PID WORKER_PID --seconds 30 --output OUTPUT.json`
records raw Mac physical footprint and RSS via `proc_pid_rusage`. Keep per-process
values and disclose shared-memory double counting in any family sum. GPU/unified
memory requires a separate measured accounting policy; never add a guessed GPU
value. Linux must use smaps/PSS plus shared GPU accounting, Windows working set,
private commit and GPU counters. Those native implementations are Blocked until
the corresponding host is supplied. Capture swap/pressure before/during/after,
plus temp/database/WAL/snapshot sizes and remaining SSD space.

For inputs record event-dispatch → visible acknowledgment; for navigation record
actual frame presentation intervals (median FPS and p95 frame time), not CPU draw
submission alone. For edits record preview and accepted evaluation separately;
for save/open record durable publication/readiness. Store process-failure outcome,
semantic revision and exact invariants with each timing. The Module 0 GL experiment
reports CPU submission as supplementary; it cannot close NFR-004.

Use one heavy job, no accepted-operation coalescing, bounded previews and cache
eviction. Preserve original fixtures if slow. Do not drop failures or relax budgets.
Installed release and physical-power-loss tests remain separate from a debug
process-kill experiment. Module 11 owns complete workload performance acceptance.
