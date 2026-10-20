# Observed setup failures and fixes

1. Qt 6.10.0 / Apple Clang 21 arm64 failed in `qyieldcpu.h:37`: implicitly
   declaring library function `__yield`; compiler requested `arm_acle.h`.
   A minimal source including that public header passed syntax checking. The
   original log is retained in `qt-initial-build-failure.txt`; the tracked patch
   adds the header only for ARM/Clang. The complete patched Qt build succeeded.
2. First PlaneGCS test expected x=10 but observed x=-10. The pinned solver's
   `ConstraintDifference::error()` is `param2 - param1 - difference`. Our spike
   passed the arguments in the reverse order. Corrected the adapter call, retained
   the analytic expected coordinates, and reran the real solver successfully.
   `initial-ctest.txt` retains the first failed test result.
3. Two OCCT build orchestrators accidentally overlapped. One failed creating
   `libTKGeomBase.7.9.dylib` with `File exists`. Both old build jobs were stopped
   and a single build restarted. A native nonblocking setup lock now prevents
   concurrent dependency builds. This was an orchestration defect, not kernel
   geometry evidence. Final source options also explicitly enable exceptions
   (`BUILD_RELEASE_DISABLE_EXCEPTIONS=OFF`) and use `-O1 -DNDEBUG`; the initial
   default `-O3` build is not the measured/tested final dependency configuration.

No failed attempt is a passed platform or product acceptance check.

4. Adding writer admission initially locked the SQLite file itself. On this Mac,
   the resulting `database is locked` failure exposed interference with SQLite's
   own locking. The final prototype locks a separate private file keyed by the
   working database's device/inode (Windows file-identity implementation is unrun).
   The expanded real-file matrix passes 34 operations, including refusing a real
   mutation while an alias holds the writer lock. Working-file identity is stable
   in this spike; saved-file replacement identity remains a production hardening gate.
5. Sandboxed native Cocoa launch aborted on macOS UI-service access; offscreen
   transport could not bind its private local socket. Host-authorized reruns passed.
   These are environment restrictions, not proof that native graphics/IPC is absent.
6. The separate computer-use inspection tool timed out. Qt's native screenshot and
   process/GPU tests provide evidence; no accessibility/VoiceOver result is inferred
   from that failed inspection. The first profiling run returned no swap callbacks;
   its memory samples are retained, and navigation metrics are explicitly unavailable
   unless a later native run collects the required samples.
7. A redundant dependency recheck encountered stale Ninja dependency metadata from
   the earlier overlap and began recompiling. It was stopped; installed libraries
   from the completed source build were retained and hashed. Clean application
   source reproduction using that verified cache passes. No second clean dependency
   rebuild is claimed, and no recheck result is presented as Pass.
8. Final linking itself succeeds when its exact generated compiler/linker argument
   list is run directly; the final native test and all 11 direct checks pass.
   Final build/CTest wrapper retries still time out after process cleanup, even
   with closed inherited descriptors and separate sessions. The root cause is
   unresolved; `wrapper-retry.json` preserves both Blocked outcomes. This prevents
   final clean-build closure and is not relabeled as a platform or CAD failure.
