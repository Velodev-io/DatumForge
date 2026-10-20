# First production vertical slice — frozen scope (0.9b)

Production work begins only after Module 0 prerequisites and explicit user approval.
Scope: native shell → planar constrained rectangle → extrusion → width edit →
durable save/reopen → worker crash/recovery. No assembly/drawing controls are
presented as functioning during this milestone. R1 breadth remains required later.

| Step | Owner | Requirement / expected evidence |
|---|---|---|
| Native frame and revisioned mesh | Module 1 | FR-001/003/004/035, NFR-013; T-01/03/04/35/53 native placement, picking, worker restart and stale replies |
| Semantic document and working DB | Module 2 | FR-006–010/034/039; stable IDs/SI units, deduplication and coherent committed/recoverable/saved status |
| Planar rectangle sketch | Module 3 | FR-012–016; T-12–16 residuals, DOF/conflicts, units/expressions and drag/command cancel |
| Exact extrusion and edit | Module 4 | FR-017/020/021/022/025; T-17/20/21/22/25 analytic volume, dependency rebuild and explicit missing/ambiguous reference |
| Save/reopen and recovery | Modules 2/4 | T-07/08/34/39/51, AC-02/06 subset; actual native files/process death, prior saved snapshot retained, acknowledged revision recovered once |

Use frozen B1's base rectangle initially: 80×50 mm, 10 mm depth (40000 mm³).
Change width to 90 mm (45000 mm³), cancel a different edit without semantic change,
save revision N, commit N+1 without explicit save, terminate the worker and offer
saved N versus recoverable N+1. Query uncertain operation IDs after restart.
UI never owns exact BRep or writes the project DB. Native interaction, numerical
geometry and process/file integrity are separate evidence classes.

Passing this slice does not satisfy all AC-01/AC-02 or R1. Fillet, exchange,
assembly, drawing, topology repair breadth, installed native builds and B2–B5
performance retain their owning modules and existing SRS gates.
