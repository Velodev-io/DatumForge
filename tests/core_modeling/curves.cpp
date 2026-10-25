#include "sketch/Curves.hpp"
#include "geometry/occt/Part.hpp"
#include "sketch/ArcGeometry.hpp"
#include "sketch/Editing.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
using namespace cad;
namespace {
sketch::Constraint fixed(QString id, sketch::Vec2 p) {
    sketch::Constraint c;
    c.kind = "Fixed";
    c.a = id;
    c.value = p.x;
    c.second = p.y;
    return c;
}
void values() {
    for (double scale : {.001, 1., 1000.}) {
        sketch::Sketch s;
        auto id = sketch::createEllipse(s, {0, 0}, {.03 * scale, 0}, {0, .01 * scale});
        auto e = s.entity(id);
        auto p = sketch::curveValue(s, e, sketch::arcPi / 2);
        near(p.x, 0, 1e-10 * scale);
        near(p.y, .01 * scale, 1e-10 * scale);
        auto result = sketch::solve(s);
        CHECK(result.valid && result.freedom == 5);
        CHECK(sketch::Sketch::parse(s.map()).map() == s.map());
        auto before = s.map();
        rejects([&] { sketch::createEllipse(s, {}, {.01, 0}, {0, .02}); });
        CHECK(s.map() == before);
        auto point = s.point({.018 * scale, .009 * scale});
        sketch::Constraint c;
        c.kind = "Coincident";
        c.a = point;
        c.b = id;
        s.constraints.push_back(c);
        for (auto control : sketch::controlPoints(e))
            s.constraints.push_back(fixed(control, s.entity(control).point));
        auto solved = sketch::solve(s);
        CHECK(solved.valid && solved.maximumResidual <= 1e-7);
        auto q = solved.sketch.entity(point).point, center = solved.sketch.entity(e.a).point,
             focus = solved.sketch.entity(e.b).point;
        sketch::Vec2 other{2 * center.x - focus.x, 2 * center.y - focus.y};
        near(sketch::length(q, focus) + sketch::length(q, other),
             2 * std::hypot(sketch::length(center, focus), solved.sketch.entity(id).radius),
             1e-8 * scale);
        sketch::Sketch parabola;
        auto par = sketch::createParabola(parabola, {.01 * scale, 0}, {0, 0}, {0, -.02 * scale},
                                          {0, .02 * scale});
        auto endpoint = sketch::curveValue(parabola, parabola.entity(par), .02 * scale);
        near(endpoint.x, .01 * scale, 1e-10 * scale);
        near(endpoint.y, .02 * scale, 1e-10 * scale);
        CHECK(sketch::solve(parabola).freedom == 6);
        sketch::Sketch spline;
        auto curve = sketch::createSpline(
            spline,
            {{0, 0}, {.01 * scale, .02 * scale}, {.02 * scale, .02 * scale}, {.03 * scale, 0}});
        auto poles = spline.entity(curve).poles;
        auto mid = sketch::curveValue(spline, spline.entity(curve), .5);
        near(mid.x, .015 * scale, 1e-10 * scale);
        near(mid.y, .015 * scale, 1e-10 * scale);
        CHECK(sketch::solve(spline).freedom == 8);
        for (int i : {0, 2, 3})
            spline.constraints.push_back(fixed(poles[i], spline.entity(poles[i]).point));
        auto anchor = spline.point({.01 * scale, 0}, true);
        spline.constraints.push_back(fixed(anchor, spline.entity(anchor).point));
        sketch::Constraint vertical;
        vertical.kind = "Vertical";
        vertical.a = poles[1];
        vertical.b = anchor;
        spline.constraints.push_back(vertical);
        auto target = spline.point({.015 * scale, .012 * scale});
        spline.constraints.push_back(fixed(target, spline.entity(target).point));
        sketch::Constraint contact;
        contact.kind = "Coincident";
        contact.a = target;
        contact.b = curve;
        contact.second = .5;
        spline.constraints.push_back(contact);
        auto constrained = sketch::solve(spline);
        if (!constrained.valid)
            throw std::runtime_error(constrained.message.toStdString());
        near(constrained.sketch.entity(poles[1]).point.y, .012 * scale, 1e-8 * scale);
        CHECK(constrained.freedom == 0);
        near(constrained.sketch.constraints.back().second, .5, 1e-8);
        CHECK(sketch::Sketch::parse(constrained.sketch.map()).map() == constrained.sketch.map());
        auto bad = spline;
        bad.entity(curve).weights[0] = 0;
        rejects([&] { bad.validate(); });
        bad = spline;
        bad.entity(curve).multiplicities[0] = 1;
        rejects([&] { bad.validate(); });
        auto lower = spline.map();
        lower.insert(QStringLiteral("version"), 3);
        rejects([&] { sketch::Sketch::parse(lower); });
    }
}
double volume(sketch::Sketch s, QString curve, bool close) {
    auto &e = s.entity(curve);
    if (close) {
        auto range = sketch::curveRange(e);
        auto p = sketch::curveValue(s, e, range.first), q = sketch::curveValue(s, e, range.second);
        s.line(s.point(q), s.point(p));
    }
    features::Feature feature;
    feature.kind = "Extrude";
    feature.parameters = sketch::fields({{"sketch", s.id},
                                         {"operation", "NewBody"},
                                         {"target", ""},
                                         {"depth", .01},
                                         {"extent", "Forward"}});
    return kernel::Part::build(sketch::encode({s}), features::encode({feature}))
        .measure(1)
        .value("volume_m3")
        .toDouble();
}
void exact() {
    sketch::Sketch ellipse;
    auto id = sketch::createEllipse(ellipse, {}, {.03, 0}, {0, .01});
    near(volume(ellipse, id, false), sketch::arcPi * .03 * .01 * .01, 1e-12);
    sketch::Sketch arc;
    id = sketch::createEllipse(arc, {}, {.03, 0}, {0, .01}, true, 0, sketch::arcPi);
    near(volume(arc, id, true), sketch::arcPi * .03 * .01 * .01 / 2, 1e-12);
    sketch::Sketch parabola;
    id = sketch::createParabola(parabola, {.01, 0}, {}, {0, -.02}, {0, .02});
    near(volume(parabola, id, true), .04 * .01 * 2 / 3 * .01, 1e-12);
    sketch::Sketch spline;
    id = sketch::createSpline(spline, {{0, 0}, {.03, 0}, {.03, .03}, {0, .03}});
    near(volume(spline, id, true), .00054 * .01, 1e-12);
    sketch::Sketch rational;
    id = sketch::createSpline(rational, {{.02, 0}, {.02, .02}, {0, .02}}, 2);
    rational.entity(id).weights[1] = std::sqrt(.5);
    auto p = sketch::curveValue(rational, rational.entity(id), .5);
    near(p.x, .02 / std::sqrt(2), 1e-10);
    near(p.y, p.x, 1e-10);
    near(volume(rational, id, true), (.0004 * sketch::arcPi / 4 - .0002) * .01, 1e-12);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        values();
        exact();
    });
}
