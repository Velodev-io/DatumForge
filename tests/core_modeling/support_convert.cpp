#include "SupportFixture.hpp"
#include "geometry/occt/Convert.hpp"
#include "sketch/Curves.hpp"
#include <QCoreApplication>
#include <iomanip>
#include <numbers>
using namespace cad;
using namespace support_fixture;
void lineConversion() {
    geometry::SupportReferences scene;
    sketch::Sketch source, target;
    source.frame.origin[2] = .04;
    auto line = source.line(source.point({0, 0}), source.point({.02, 0}));
    scene.sketches[source.id] = source;
    model::Conversion c;
    c.sketch = target.id;
    c.sources = {ref(source.id, "Curve", line)};
    c.angle = std::numbers::pi / 2;
    c.scale = .75;
    c.translation = {.01, .02};
    auto converted = geometry::convert(c, target.frame, scene);
    CHECK(converted.entities.size() == 3);
    const sketch::Entity *edge = nullptr;
    for (auto &e : converted.entities)
        if (e.kind == "Line")
            edge = &e;
    CHECK(edge);
    near(converted.entity(edge->a).point.x, .01);
    near(converted.entity(edge->a).point.y, .02);
    near(converted.entity(edge->b).point.x, .01);
    near(converted.entity(edge->b).point.y, .035);
    auto repeated = geometry::convert(c, target.frame, scene);
    CHECK(repeated.map() == converted.map());
    source.entity(line).b = source.point({.04, 0});
    scene.sketches[source.id] = source;
    auto updated = geometry::convert(c, target.frame, scene);
    CHECK(updated.entity(edge->id).b == edge->b);
    near(updated.entity(edge->b).point.y, .05);
}
void exactCurveConversion() {
    geometry::SupportReferences scene;
    sketch::Sketch source, target;
    source.frame.v = {0, .5, std::sqrt(.75)};
    auto circle = source.circle(source.point({.005, .006}), .01);
    scene.sketches[source.id] = source;
    model::Conversion c;
    c.sketch = target.id;
    c.sources = {ref(source.id, "Curve", circle)};
    auto projected = geometry::convert(c, target.frame, scene);
    auto spline = projected.entities.back();
    CHECK(spline.kind == "Spline");
    for (int i = 0; i <= 32; ++i) {
        auto q = sketch::curveValue(projected, spline, spline.start + spline.sweep * i / 32);
        near(std::pow((q.x - .005) / .01, 2) + std::pow((q.y - .003) / .005, 2), 1, 1e-9);
    }
    features::Feature extrude;
    extrude.parameters = sketch::fields({{"sketch", projected.id},
                                         {"operation", "NewBody"},
                                         {"target", ""},
                                         {"depth", .02},
                                         {"extent", "Forward"}});
    auto part = kernel::Part::build(sketch::encode({projected}), features::encode({extrude}));
    std::cout << std::setprecision(17)
              << "Ellipse volume: " << part.measure(0).value("volume_m3").toDouble()
              << ", analytic: " << std::numbers::pi * .01 * .005 * .02 << '\n';
    near(part.measure(0).value("volume_m3").toDouble(), std::numbers::pi * .01 * .005 * .02, 1e-12);
    auto collapsed = source;
    collapsed.frame.v = {0, 0, 1};
    scene.sketches[source.id] = collapsed;
    error("InvalidGeometry", [&] { geometry::convert(c, target.frame, scene); });
    scene.sketches[source.id] = source;
    auto parabola = sketch::createParabola(source, {.01, 0}, {0, 0}, {0, -.02}, {0, .02});
    source.frame.v = {0, 1, 0};
    scene.sketches[source.id] = source;
    c.sources = {ref(source.id, "Curve", parabola)};
    auto p = geometry::convert(c, target.frame, scene);
    auto e = p.entities.back();
    for (int i = 0; i <= 12; ++i) {
        auto q = sketch::curveValue(p, e, e.start + e.sweep * i / 12);
        near(q.x, q.y * q.y / .04, 1e-10);
    }
    auto rational = sketch::createSpline(source, {{0, 0}, {.01, .02}, {.02, -.01}, {.03, .01}});
    source.entity(rational).weights = {1, 2, .7, 1};
    source.frame.v = {0, .5, std::sqrt(.75)};
    scene.sketches[source.id] = source;
    c.sources = {ref(source.id, "Curve", rational)};
    auto r = geometry::convert(c, target.frame, scene);
    auto curve = r.entities.back();
    for (int i = 0; i <= 12; ++i) {
        auto expected = sketch::curveValue(source, source.entity(rational), i / 12.);
        auto actual = sketch::curveValue(r, curve, i / 12.);
        near(actual.x, expected.x, 1e-10);
        near(actual.y, expected.y * .5, 1e-10);
    }
}
void trimmedConicsAndPoints() {
    geometry::SupportReferences scene;
    sketch::Sketch source, target;
    source.frame.v = {0, .5, std::sqrt(.75)};
    auto center = source.point({0, 0});
    auto arc = source.arc(center, .01, 0, std::numbers::pi / 2);
    auto ellipse = sketch::createEllipse(source, {0, 0}, {.02, 0}, {0, .01});
    auto ellipticalArc =
        sketch::createEllipse(source, {0, 0}, {.02, 0}, {0, .01}, true, 0, std::numbers::pi / 2);
    scene.sketches[source.id] = source;
    model::Conversion conversion;
    conversion.sketch = target.id;
    for (auto id : {arc, ellipse, ellipticalArc}) {
        conversion.sources = {ref(source.id, "Curve", id)};
        auto output = geometry::convert(conversion, target.frame, scene);
        auto e = output.entities.back();
        CHECK(e.kind == "Spline");
        double major = id == arc ? .01 : .02;
        double minor = .005;
        for (int i = 0; i <= 16; ++i) {
            auto q = sketch::curveValue(output, e, e.start + e.sweep * i / 16);
            near(q.x * q.x / (major * major) + q.y * q.y / (minor * minor), 1, 1e-9);
        }
    }
    conversion.sources = {ref(source.id, "Point", center)};
    auto output = geometry::convert(conversion, target.frame, scene);
    CHECK(output.entities.size() == 1 && output.entities[0].kind == "Point");
    near(output.entities[0].point.x, 0);
    near(output.entities[0].point.y, 0);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        lineConversion();
        exactCurveConversion();
        trimmedConicsAndPoints();
    });
}
