# Sketch tool selection repair — 26 October 2026

User report: drawing continued with Line apparently deselected; curved geometry
could be created after choosing only Line. Work is on `fix/sketch-tool-selection`.
The existing interactive draft and pre-existing `docs/TECH_SPEC.md` edits are
preserved. No commit, merge or push is part of this repair.

## Reproduction and diagnosis

The new `module4.tool_selection` regression uses a marked disposable recovery
root and a real local worker. Its [pre-fix failure](artifacts/module-4/tool-selection-20261026/red-test.txt)
records that a new sketch did not start in Select. The selector's first item was
Point, so new sketches could create geometry without an explicit drawing choice.
The canvas likewise defaulted to Point, and the previous tool survived creating
another sketch.

Toolbar synchronization additionally blocked every tool action's signals while
changing its checked state. This prevented `QActionGroup` from tracking those
changes. Clicking an action could uncheck its indicator while its handler still
set the same drawing tool; no selector change was emitted to repair that state.
The pinned Qt 6.10 implementation confirms that `QActionGroup` maintains its
checked action from `QAction::changed`; blocking that signal bypasses this update.

Curves from Line have a separate, intentional source: moving more than 20 screen
pixels away from a chained line endpoint and returning within 8 pixels selects
Tangent Arc. Tangent and normal intent support major circular arcs; the operation
returns to Line after completion. This is the existing
[course arc interaction](module-3-all-arcs.md), retained by this repair.

## Changed behavior

- New sketch drafts start in Select, including after a previous drawing session.
- Clicking an active drawing action again selects Select and cancels pending input.
- Toolbar and flyout actions track the actual tool, including inferred Tangent Arc.
- The canvas header names the active tool. Line's hint and tooltip explain the
  endpoint-return gesture and how to stop drawing.

The regression checks the selected action and group, pointer clicks after leaving
Line, cancellation of a pending line, programmatic tool changes, inferred arc
selection and cancellation, and the Arc flyout's current action. Tests close their
own disposable projects explicitly; an initial post-fix run blocked on the test's
unsaved-project close prompt and was interrupted. Its log is retained as
`green-close-prompt.txt`; it is not counted as a completed run.

## Verification

The [focused post-fix run](artifacts/module-4/tool-selection-20261026/green-test.txt)
passed. The first complete suite run passed 69 of 70 enabled tests; its one
failure was the old `module3.interaction` fixture assuming Rectangle would remain
active in a new sketch. The fixture now explicitly chooses Rectangle; production
defaults are not weakened to retain that assumption. Its first result is preserved
in `ctest.txt`.

Final available-Mac results:

| Check | Result |
| --- | --- |
| Development build | Pass; application and sibling worker rebuilt |
| [Complete CTest suite](artifacts/module-4/tool-selection-20261026/ctest-final.txt) | 70 enabled tests passed in 319.25 s; deliberate failure probe disabled |
| [Native scale ledger](artifacts/module-4/tool-selection-20261026/native.json) | Tool regression at 100/125/150/175/200%; existing mm/in arc workflow at 100/200%; all pass |
| [Formatting](artifacts/module-4/tool-selection-20261026/format.txt) | Pinned formatter and whitespace checks pass |
| [Scoped static analysis](artifacts/module-4/tool-selection-20261026/static-analysis.txt) | All five changed C++ translation units pass with real compile flags |

[Inferred arc at 100%](artifacts/module-4/tool-selection-20261026/native-100/inferred-tangent-arc.png)
shows the highlighted Tangent Arc action and matching canvas header.
[Drawing stopped at 200%](artifacts/module-4/tool-selection-20261026/native-200/drawing-stopped.png)
shows Select highlighted, no pending primitive and a matching header. These images
were visually inspected. The tests use their own native windows and marked roots;
the existing user app was not restarted and its draft was not modified by this run.
Relaunch the rebuilt development bundle after preserving that draft to load the fix.

Historical 6.1b/6.4c/7.4c gates and all unrun physical/platform acceptance remain
open. Module 11 is not started. Source hashes and the final check ledger are in
[verification](artifacts/module-4/tool-selection-20261026/verification.json).
