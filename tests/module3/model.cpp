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
        std::cout << "3.1a: frames, primitives, connected points, construction, snap, persistent "
                     "IDs, bounded invalid input\n";
    });
}
