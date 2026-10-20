#include "support/check.hpp"
#include <GCS.h>
#include <array>
namespace {
void rectangle(bool redundant, bool conflict) {
    // Shared corners form a rectangle; coordinates are owned for the whole solve.
    std::array<double, 8> v{.1, .2, 9.8, .1, 10.1, 5.2, -.1, 4.9};
    GCS::Point a{&v[0], &v[1]}, b{&v[2], &v[3]}, c{&v[4], &v[5]}, d{&v[6], &v[7]};
    GCS::System system;
    GCS::VEC_pD unknowns;
    for (auto &x : v)
        unknowns.push_back(&x);
    double zero = 0, width = 10, height = 5, wrong = 11;
    system.addConstraintCoordinateX(a, &zero, 1);
    system.addConstraintCoordinateY(a, &zero, 2);
    system.addConstraintHorizontal(a, b, 3);
    system.addConstraintVertical(b, c, 4);
    system.addConstraintHorizontal(c, d, 5);
    system.addConstraintVertical(d, a, 6);
    // PlaneGCS uses param2 - param1 = difference (verified in pinned source).
    system.addConstraintDifference(a.x, b.x, &width, 7);
    system.addConstraintDifference(b.y, c.y, &height, 8);
    if (redundant)
        system.addConstraintDifference(a.x, b.x, &width, 9);
    if (conflict)
        system.addConstraintDifference(a.x, b.x, &wrong, 10);
    system.declareUnknowns(unknowns);
    system.initSolution();
    system.diagnose();
    GCS::VEC_I duplicates, conflicts;
    system.getRedundant(duplicates);
    system.getConflicting(conflicts);
    if (conflict) {
        CHECK(!conflicts.empty());
        std::cout << "contradictory tags=" << conflicts.size() << '\n';
        return;
    }
    if (redundant) {
        CHECK(!duplicates.empty());
        std::cout << "redundant tags=" << duplicates.size() << '\n';
        return;
    }
    CHECK(system.solve() == GCS::Success);
    system.applySolution();
    near(v[0], 0);
    near(v[1], 0);
    near(v[2], 10);
    near(v[3], 0);
    near(v[4], 10);
    near(v[5], 5);
    near(v[6], 0);
    near(v[7], 5);
    CHECK(system.dofsNumber() == 0);
    for (int tag = 1; tag <= 8; ++tag)
        near(system.calculateConstraintErrorByTag(tag), 0);
}
} // namespace
int main() {
    return run([] {
        rectangle(false, false);
        rectangle(true, false);
        rectangle(false, true);
        double x = 1, y = 2, r = 2, radius = 5, zero = 0;
        GCS::Circle circle;
        circle.center = {&x, &y};
        circle.rad = &r;
        GCS::System system;
        GCS::VEC_pD variables{&x, &y, &r};
        system.addConstraintCoordinateX(circle.center, &zero, 1);
        system.addConstraintCoordinateY(circle.center, &zero, 2);
        system.addConstraintEqual(&r, &radius, 3);
        system.declareUnknowns(variables);
        system.initSolution();
        CHECK(system.solve() == GCS::Success);
        system.applySolution();
        near(x, 0);
        near(y, 0);
        near(r, 5);
        // Underdefined line endpoint remains at the drag seed while horizontal is solved.
        double ax = 0, ay = 0, bx = 7, by = 3;
        GCS::Point a{&ax, &ay}, b{&bx, &by};
        GCS::System drag;
        GCS::VEC_pD moving{&bx, &by};
        drag.addConstraintHorizontal(a, b, 1);
        drag.declareUnknowns(moving);
        drag.initSolution();
        drag.diagnose();
        CHECK(drag.dofsNumber() == 1);
        CHECK(drag.solve() == GCS::Success);
        drag.applySolution();
        near(by, 0);
        near(bx, 7);
        auto accepted = std::array{bx, by};
        bx = 9;
        by = 4;
        drag.initSolution();
        CHECK(drag.solve() == GCS::Success);
        drag.applySolution();
        near(by, 0);
        near(bx, 9);
        bx = accepted[0];
        by = accepted[1];
        near(bx, 7);
        near(by, 0); // cancel restores semantic values
        auto validate = [](double value) { CHECK(std::isfinite(value) && value > 1e-9); };
        rejects([&] { validate(-1); });
        rejects([&] { validate(0); });
        rejects([&] { validate(INFINITY); });
        std::cout << "rectangle/circle residuals, redundant/conflict diagnostics, 1-DOF "
                     "drag/cancel, finite/degenerate admission\n";
    });
}
