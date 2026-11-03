#include "geometry/occt/ModelingProfiles.hpp"
#include "support/check.hpp"
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <numbers>
using namespace cad;
using sketch::fields;
namespace {
double area(const TopoDS_Shape &shape) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(shape, p);
    CHECK(BRepCheck_Analyzer(shape).IsValid());
    return p.Mass();
}
QCborMap all(QString id) {
    return fields({{"sketch", id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}});
}
QCborMap selection(const sketch::Sketch &s, QString mode = "SelectedRegions") {
    QCborArray uses;
    for (auto &e : s.entities)
        if (!e.construction && e.kind != "Point")
            uses.append(fields({{"entity", e.id}, {"from", 0.}, {"to", 1.}, {"reverse", false}}));
    return fields({{"sketch", s.id}, {"mode", mode}, {"regions", QCborArray{QCborArray{uses}}}});
}
void thinSections() {
    sketch::Sketch rectangle;
    rectangle.rectangle({0, 0}, {.02, .01});
    auto one = fields({{"mode", "OneDirection"}, {"thickness", .001}, {"reverse", false}});
    auto left = geometry::modelingProfile(rectangle, all(rectangle.id), one);
    near(area(left.shape), 200 - 18 * 8, 1e-7);
    std::cout << "closed inward rectangle passed\n";
    one.insert(QStringLiteral("reverse"), true);
    near(area(geometry::modelingProfile(rectangle, all(rectangle.id), one).shape), 22 * 12 - 200,
         1e-7);
    std::cout << "closed outward rectangle passed\n";
    sketch::Sketch circle;
    circle.circle(circle.point({0, 0}), .01);
    one.insert(QStringLiteral("reverse"), false);
    one.insert(QStringLiteral("thickness"), .002);
    near(area(geometry::modelingProfile(circle, all(circle.id), one).shape),
         std::numbers::pi * (100 - 64), 1e-7);
    std::cout << "closed circle passed\n";
    one.insert(QStringLiteral("thickness"), .02);
    rejects([&] { geometry::modelingProfile(circle, all(circle.id), one); });
    sketch::Sketch open;
    open.polyline({{0, 0}, {.02, 0}}, false);
    auto selected = selection(open, "OpenChains");
    auto two = fields({{"mode", "TwoDirection"}, {"thickness1", .001}, {"thickness2", .002}});
    auto section = geometry::modelingProfile(open, selected, two);
    CHECK(!section.open);
    near(area(section.shape), 60, 1e-7);
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(section.shape, bounds, false, false);
    double x0, y0, z0, x1, y1, z1;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    near(x0, 0, 1e-7);
    near(x1, 20, 1e-7);
    near(y0, -2, 1e-7);
    near(y1, 1, 1e-7);
    auto mid = fields({{"mode", "Midplane"}, {"thickness", .003}});
    near(area(geometry::modelingProfile(open, selected, mid).shape), 60, 1e-7);
    auto openSection = geometry::modelingProfile(open, selected, fields({{"mode", "Solid"}}));
    CHECK(openSection.open);
    std::cout << "thin sections passed\n";
}
void selectedRegions() {
    sketch::Sketch s;
    s.rectangle({0, 0}, {.02, .01});
    s.rectangle({.004, .003}, {.012, .006});
    auto p = selection(s);
    auto uses = p.value("regions").toArray()[0].toArray()[0].toArray();
    QCborArray outer, inner;
    for (int i = 0; i < 8; ++i)
        (i < 4 ? outer : inner).append(uses[i]);
    p.insert(QStringLiteral("regions"), QCborArray{QCborArray{outer, inner}});
    near(area(geometry::modelingProfile(s, p, fields({{"mode", "Solid"}})).shape), 176, 1e-7);
    auto broken = p;
    auto gap = outer;
    auto interval = gap[0].toMap();
    interval.insert(QStringLiteral("to"), .5);
    gap[0] = interval;
    broken.insert(QStringLiteral("regions"), QCborArray{QCborArray{gap}});
    rejects([&] { geometry::modelingProfile(s, broken, fields({{"mode", "Solid"}})); });
    sketch::Sketch adjacent;
    adjacent.rectangle({0, 0}, {.02, .01});
    adjacent.rectangle({.02, 0}, {.04, .01});
    auto cells = selection(adjacent);
    uses = cells.value("regions").toArray()[0].toArray()[0].toArray();
    outer = {};
    inner = {};
    for (int i = 0; i < 8; ++i)
        (i < 4 ? outer : inner).append(uses[i]);
    cells.insert(QStringLiteral("regions"), QCborArray{QCborArray{outer}, QCborArray{inner}});
    near(area(geometry::modelingProfile(adjacent, cells, fields({{"mode", "Solid"}})).shape), 400,
         1e-7);
    cells.insert(QStringLiteral("regions"), QCborArray{QCborArray{outer}, QCborArray{outer}});
    rejects([&] { geometry::modelingProfile(adjacent, cells, fields({{"mode", "Solid"}})); });
    sketch::Sketch touching;
    touching.rectangle({0, 0}, {.02, .01});
    touching.rectangle({0, .003}, {.004, .006});
    auto contact = selection(touching);
    uses = contact.value("regions").toArray()[0].toArray()[0].toArray();
    outer = {};
    inner = {};
    for (int i = 0; i < 8; ++i)
        (i < 4 ? outer : inner).append(uses[i]);
    contact.insert(QStringLiteral("regions"), QCborArray{QCborArray{outer, inner}});
    rejects([&] { geometry::modelingProfile(touching, contact, fields({{"mode", "Solid"}})); });
    sketch::Sketch many;
    many.rectangle({0, 0}, {.1, .1});
    for (int i = 0; i < 16; ++i)
        many.rectangle({.005 + i * .005, .01}, {.006 + i * .005, .011});
    auto excessive = selection(many);
    auto members = excessive.value("regions").toArray()[0].toArray()[0].toArray();
    QCborArray boundaries;
    for (int i = 0; i < 17; ++i) {
        QCborArray loop;
        for (int j = 0; j < 4; ++j)
            loop.append(members[i * 4 + j]);
        boundaries.append(loop);
    }
    excessive.insert(QStringLiteral("regions"), QCborArray{boundaries});
    rejects([&] { geometry::modelingProfile(many, excessive, fields({{"mode", "Solid"}})); });
    std::cout << "selected regions passed\n";
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        sketch::Sketch s;
        s.rectangle({0, 0}, {.02, .01});
        auto solid = fields({{"mode", "Solid"}});
        auto whole = geometry::modelingProfile(s, all(s.id), solid);
        near(area(whole.shape), 200, 1e-8);
        auto chosen = geometry::modelingProfile(s, selection(s), solid);
        near(area(chosen.shape), 200, 1e-8);
        CHECK(!chosen.open);
        // Reusing a sketch does not consume or rewrite its entities.
        auto before = s.map();
        geometry::modelingProfile(s, selection(s), solid);
        CHECK(s.map() == before);
        selectedRegions();
        thinSections();
    });
}
