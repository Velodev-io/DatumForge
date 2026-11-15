#include "sketch/Editing.hpp"
#include "sketch/Sketch.hpp"
#include "support/check.hpp"
using namespace cad::sketch;
int main() {
    return run([] {
        Frame f{{.1, .2, .3}, {0, 1, 0}, {0, 0, 1}};
        auto w = f.world({.004, .005});
        near(w[0], .1, 1e-14);
        near(w[1], .204, 1e-14);
        near(w[2], .305, 1e-14);
        auto p = f.local(w);
        near(p.x, .004, 1e-14);
        near(p.y, .005, 1e-14);
        rejects([&] { f.local({.11, .2, .3}); });
        f.v = f.u;
        rejects([&] { f.validate(); });
        Sketch s;
        s.rectangle({0, 0}, {.08, .05});
        s.polyline({{.1, 0}, {.12, .01}, {.13, 0}}, false, true);
        auto center = s.point({.04, .025});
        s.circle(center, .002);
        s.arc(center, .01, 0, 1.5, true);
        s.validate();
        auto restored = Sketch::parse(s.map());
        CHECK(restored.map() == s.map());
        QString snap;
        auto q = s.snap({.00001, .00002}, .0001, .001, &snap);
        CHECK(!snap.isEmpty());
        near(q.x, 0);
        near(q.y, 0);
        q = s.snap({.0524, .0413}, .0001, .001);
        near(q.x, .052);
        near(q.y, .041);
        auto invalid = s;
        invalid.entities.back().radius = -1;
        rejects([&] { invalid.validate(); });
        invalid = s;
        invalid.entities[4].b = invalid.entities[4].a;
        rejects([&] { invalid.validate(); });
        invalid = s;
        invalid.entities[0].point.x = INFINITY;
        rejects([&] { invalid.validate(); });
        invalid = s;
        invalid.entities[1].id = invalid.entities[0].id;
        rejects([&] { invalid.validate(); });
        rejects([&] { parse({s.map(), s.map()}); });
        Sketch edit;
        auto edges = edit.rectangle({0, 0}, {.08, .05});
        auto corner = edit.entities[2].id;
        auto changed = dragEntity(edit, corner, {.08, .05}, {.09, .06});
        CHECK(edit.entities[2].point.x == .08);
        CHECK(changed.entities[2].id == corner);
        near(changed.entities[1].point.x, .09);
        near(changed.entities[3].point.y, .06);
        CHECK(rectangleOf(changed, edges[0]).has_value());
        rejects([&] { dragEntity(edit, corner, {.08, .05}, {-.01, .06}); });
        auto editCenter = edit.point({.2, 0});
        auto circle = edit.circle(editCenter, .01);
        auto resized = dragEntity(edit, circle, {.21, 0}, {.215, 0});
        near(resized.entity(circle).radius, .015);
        Constraint radius;
        radius.kind = "Radius";
        radius.a = circle;
        radius.value = .01;
        edit.constraints.push_back(radius);
        rejects([&] { dragEntity(edit, circle, {.21, 0}, {.215, 0}); });
        auto erased = eraseEntity(edit, edges[0]);
        CHECK(erased.entities.size() == 2 && erased.constraints.size() == 1);
        CHECK(erased.entity(circle).id == circle);
        erased = eraseEntity(erased, circle);
        CHECK(erased.entities.empty() && erased.constraints.empty());
        std::cout << "3.1a: frames, primitives, connected points, construction, snap, persistent "
                     "IDs, bounded invalid input\n";
    });
}
