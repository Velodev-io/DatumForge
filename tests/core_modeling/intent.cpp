#include "sketch/ArcGeometry.hpp"
#include "sketch/Curves.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
using namespace cad;
namespace {
sketch::Constraint &add(sketch::Sketch &s, QString kind, QString a, QString b = {},
                        QString third = {}) {
    sketch::Constraint c;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.third = third;
    if (kind == "Fixed") {
        auto p = sketch::location(s, s.entity(a));
        c.value = p.x;
        c.second = p.y;
    }
    s.constraints.push_back(c);
    return s.constraints.back();
}
void require(sketch::Result r) {
    if (!r.valid)
        throw std::runtime_error(r.message.toStdString());
    CHECK(r.maximumResidual <= 1e-7);
}
std::pair<QString, double> tangentCurve(sketch::Sketch &s, QString kind, bool upper, double scale) {
    auto point = [&](double x, double y) { return s.point({x * scale, y * scale}); };
    double center = upper ? .025 : .005;
    if (kind == "Line")
        return {s.line(point(-.03, .015), point(.03, .015)), .5};
    if (kind == "Circle")
        return {s.circle(point(0, center), .01 * scale),
                upper ? 3 * sketch::arcPi / 2 : sketch::arcPi / 2};
    if (kind == "Arc")
        return {s.arc(point(0, center), .01 * scale, upper ? sketch::arcPi : 0, sketch::arcPi),
                upper ? 3 * sketch::arcPi / 2 : sketch::arcPi / 2};
    if (kind == "Ellipse" || kind == "EllipticalArc")
        return {sketch::createEllipse(s, {0, center * scale},
                                      {(upper ? .02 : .03) * scale, center * scale},
                                      {0, (center + .01) * scale}, kind == "EllipticalArc",
                                      upper ? sketch::arcPi : 0, sketch::arcPi),
                upper ? 3 * sketch::arcPi / 2 : sketch::arcPi / 2};
    if (kind == "Parabola")
        return {sketch::createParabola(s, {0, (upper ? .025 : .005) * scale}, {0, .015 * scale},
                                       {-.02 * scale, 0}, {.02 * scale, 0}),
                0};
    return {sketch::createSpline(s, {{-.03 * scale, (upper ? .03 : 0) * scale},
                                     {-.01 * scale, (upper ? .01 : .02) * scale},
                                     {.01 * scale, (upper ? .01 : .02) * scale},
                                     {.03 * scale, (upper ? .03 : 0) * scale}}),
            .5};
}
void contacts() {
    QStringList types{"Line", "Circle", "Arc", "Ellipse", "EllipticalArc", "Parabola", "Spline"};
    for (double scale : {.001, 1., 1000.})
        for (auto first : types)
            for (auto second : types) {
                if (!sketch::advancedCurve(first) && !sketch::advancedCurve(second))
                    continue;
                std::cout << "Tangent " << first.toStdString() << " / " << second.toStdString()
                          << " " << scale << std::endl;
                sketch::Sketch s;
                auto [a, u] = tangentCurve(s, first, false, scale);
                auto [b, v] = tangentCurve(s, second, true, scale);
                int free = sketch::solve(s).freedom;
                auto x = sketch::curveDerivative(s, s.entity(a), u),
                     y = sketch::curveDerivative(s, s.entity(b), v);
                auto &c = add(s, "CurveTangent", a, b);
                c.value = u;
                c.second = v;
                c.reversed = x.x * y.x + x.y * y.y < 0;
                auto result = sketch::solve(s);
                require(result);
                CHECK(result.freedom == free - 1);
                auto &contact = result.sketch.constraints[0];
                near(
                    sketch::length(
                        sketch::curveValue(result.sketch, result.sketch.entity(a), contact.value),
                        sketch::curveValue(result.sketch, result.sketch.entity(b), contact.second)),
                    0, 1e-8 * scale);
                auto stored = sketch::Sketch::parse(result.sketch.map());
                CHECK(stored.map() == result.sketch.map());
            }
}
void symmetryAndTangency() {
    for (double scale : {.001, 1., 1000.}) {
        for (QString kind : {"Line", "Arc", "Ellipse"}) {
            sketch::Sketch s;
            auto p = [&](double x, double y) { return s.point({x * scale, y * scale}); };
            QString first, second;
            if (kind == "Line") {
                first = s.line(p(-.03, .01), p(-.02, .02));
                second = s.line(p(.031, .011), p(.021, .022));
            }
            if (kind == "Arc") {
                first = s.arc(p(-.03, 0), .01 * scale, 0, sketch::arcPi);
                second = s.arc(p(.031, .001), .011 * scale, 0, sketch::arcPi);
            }
            if (kind == "Ellipse") {
                first = sketch::createEllipse(s, {-.03 * scale, 0}, {-.01 * scale, 0},
                                              {-.03 * scale, .01 * scale});
                second = sketch::createEllipse(s, {.031 * scale, .001 * scale},
                                               {.011 * scale, .001 * scale},
                                               {.031 * scale, .012 * scale});
            }
            auto axis = s.line(p(0, -.05), p(0, .05), true);
            for (auto id : sketch::controlPoints(s.entity(first)))
                add(s, "Fixed", id);
            for (auto id : sketch::controlPoints(s.entity(axis)))
                add(s, "Fixed", id);
            add(s, "Symmetric", first, second, axis);
            auto result = sketch::solve(s);
            require(result);
            auto &out = result.sketch;
            auto a = sketch::location(out, out.entity(first)),
                 b = sketch::location(out, out.entity(second));
            near(a.x + b.x, 0, 1e-8 * scale);
            near(a.y - b.y, 0, 1e-8 * scale);
            if (kind == "Arc")
                for (int endpoint : {0, 1}) {
                    a = sketch::curveEnd(out, out.entity(first), endpoint);
                    b = sketch::curveEnd(out, out.entity(second), 1 - endpoint);
                    near(a.x + b.x, 0, 1e-8 * scale);
                    near(a.y - b.y, 0, 1e-8 * scale);
                }
            sketch::validateAccepted(sketch::encode({out}));
        }
        for (QString first : {"Circle", "Arc"})
            for (QString second : {"Circle", "Arc"})
                for (bool internal : {false, true}) {
                    sketch::Sketch s;
                    auto a = s.point({0, 0}), b = s.point({(internal ? .01 : .03) * scale, 0});
                    auto x = first == "Circle" ? s.circle(a, .02 * scale)
                                               : s.arc(a, .02 * scale, -.5, 1);
                    auto y = second == "Circle"
                                 ? s.circle(b, .01 * scale)
                                 : s.arc(b, .01 * scale, internal ? -.5 : sketch::arcPi - .5, 1);
                    add(s, "Fixed", a);
                    add(s, "Fixed", b);
                    add(s, "Radius", x).value = .02 * scale;
                    add(s, "Radius", y).value = .01 * scale;
                    add(s, "Tangent", x, y).value = internal ? 1 : 0;
                    require(sketch::solve(s));
                    if (second == "Arc") {
                        s.entity(y).start = internal ? sketch::arcPi - .5 : -.5;
                        auto invalid = sketch::solve(s);
                        CHECK(!invalid.valid &&
                              invalid.message.contains("outside the selected curve trim"));
                    }
                }
        sketch::Sketch s;
        auto a = s.point({-.03 * scale, .01 * scale}), b = s.point({.03 * scale, .01 * scale}),
             center = s.point({0, 0});
        auto line = s.line(a, b), arc = s.arc(center, .01 * scale, 0, sketch::arcPi);
        for (auto id : {a, b, center})
            add(s, "Fixed", id);
        add(s, "Radius", arc).value = .01 * scale;
        add(s, "Tangent", line, arc);
        require(sketch::solve(s));
        s.entity(arc).start = sketch::arcPi;
        CHECK(!sketch::solve(s).valid);
    }
}
void relations() {
    for (double scale : {.001, 1., 1000.}) {
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.04 * scale, 0}),
             c = s.point({.02 * scale, -.02 * scale}), d = s.point({.02 * scale, .02 * scale});
        auto horizontal = s.line(a, b), vertical = s.line(c, d),
             point = s.point({.01 * scale, .01 * scale});
        for (auto id : {a, b, c, d})
            add(s, "Fixed", id);
        add(s, "Intersection", point, horizontal, vertical);
        auto intersection = sketch::solve(s);
        require(intersection);
        near(intersection.sketch.entity(point).point.x, .02 * scale, 1e-8 * scale);
        near(intersection.sketch.entity(point).point.y, 0, 1e-8 * scale);
        CHECK(intersection.freedom == 0);
        auto bad = s;
        bad.entity(c).point = {0, .01 * scale};
        bad.entity(d).point = {.04 * scale, .01 * scale};
        auto invalid = sketch::solve(bad);
        CHECK(!invalid.valid && invalid.message.contains("unique intersection"));
        sketch::Sketch mirror;
        auto p = mirror.point({-.02 * scale, .01 * scale}),
             q = mirror.point({.018 * scale, .012 * scale}),
             axisA = mirror.point({0, -.03 * scale}, true),
             axisB = mirror.point({0, .03 * scale}, true), axis = mirror.line(axisA, axisB, true);
        for (auto id : {p, axisA, axisB})
            add(mirror, "Fixed", id);
        add(mirror, "Symmetric", p, q, axis);
        auto reflected = sketch::solve(mirror);
        require(reflected);
        near(reflected.sketch.entity(q).point.x, .02 * scale, 1e-8 * scale);
        near(reflected.sketch.entity(q).point.y, .01 * scale, 1e-8 * scale);
        sketch::Sketch merge;
        auto first = merge.point({.01 * scale, .02 * scale}),
             second = merge.point({.012 * scale, .018 * scale});
        add(merge, "Fixed", first);
        add(merge, "Merge", first, second);
        auto merged = sketch::solve(merge);
        require(merged);
        near(sketch::length(merged.sketch.entity(first).point, merged.sketch.entity(second).point),
             0, 1e-8 * scale);
        CHECK(merged.sketch.entities.size() == 2);
        sketch::Sketch fixed;
        auto p1 = fixed.point({-.01 * scale, 0}), p2 = fixed.point({.01 * scale, 0}),
             line = fixed.line(p1, p2);
        add(fixed, "Fixed", line);
        auto anchored = sketch::solve(fixed);
        require(anchored);
        CHECK(anchored.freedom == 2);
        add(fixed, "Horizontal", line);
        add(fixed, "Length", line).value = .04 * scale;
        auto resized = sketch::solve(fixed);
        require(resized);
        CHECK(resized.freedom == 0);
        near(sketch::location(resized.sketch, resized.sketch.entity(line)).x, 0, 1e-8 * scale);
        near(sketch::length(resized.sketch.entity(p1).point, resized.sketch.entity(p2).point),
             .04 * scale, 1e-8 * scale);
        for (auto kind : {QString("Circle"), QString("Arc")})
            for (bool reversed : {false, true}) {
                sketch::Sketch concentric;
                auto center = concentric.point({.01 * scale, .01 * scale}),
                     point = concentric.point({.02 * scale, .02 * scale}),
                     curve = kind == "Circle"
                                 ? concentric.circle(center, .01 * scale)
                                 : concentric.arc(center, .01 * scale, 0, sketch::arcPi);
                add(concentric, "Fixed", center);
                add(concentric, "Concentric", reversed ? point : curve, reversed ? curve : point);
                require(sketch::solve(concentric));
            }
    }
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        relations();
        contacts();
        symmetryAndTangency();
    });
}
