#include "SupportFixture.hpp"
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

    sketch::Sketch openArc;
    auto center = openArc.point({0, 0});
    openArc.arc(center, .02, 0, std::numbers::pi / 2);
    auto arcSelected = selection(openArc, "OpenChains");
    auto arcMid = fields({{"mode", "Midplane"}, {"thickness", .002}});
    auto arcProfile = geometry::modelingProfile(openArc, arcSelected, arcMid);
    CHECK(!arcProfile.open);
    near(area(arcProfile.shape), 20 * std::numbers::pi, 1e-7);
    Bnd_Box arcBox;
    BRepBndLib::AddOptimal(arcProfile.shape, arcBox, false, false);
    arcBox.Get(x0, y0, z0, x1, y1, z1);
    near(x0, 0, 1e-7);
    near(x1, 21, 1e-7);
    near(y0, 0, 1e-7);
    near(y1, 21, 1e-7);
    auto arcTwo = fields({{"mode", "TwoDirection"}, {"thickness1", .001}, {"thickness2", .002}});
    near(area(geometry::modelingProfile(openArc, arcSelected, arcTwo).shape),
         29.25 * std::numbers::pi, 1e-7);
    CHECK(geometry::modelingProfile(openArc, arcSelected, fields({{"mode", "Solid"}})).open);
    rejects([&] {
        geometry::modelingProfile(
            openArc, arcSelected,
            fields({{"mode", "OneDirection"}, {"thickness", .025}, {"reverse", true}}));
    });

    sketch::Sketch trimmed;
    trimmed.polyline({{0, 0}, {.04, 0}}, false);
    QString lineId;
    for (auto &e : trimmed.entities)
        if (e.kind == "Line")
            lineId = e.id;
    CHECK(!lineId.isEmpty());
    auto trimmedUse = fields({{"entity", lineId}, {"from", .25}, {"to", .75}, {"reverse", false}});
    auto trimmedSelection = fields({{"sketch", trimmed.id},
                                    {"mode", "OpenChains"},
                                    {"regions", QCborArray{QCborArray{QCborArray{trimmedUse}}}}});
    auto trimmedSection = geometry::modelingProfile(trimmed, trimmedSelection, mid);
    near(area(trimmedSection.shape), 60, 1e-7);
    Bnd_Box trimmedBox;
    BRepBndLib::AddOptimal(trimmedSection.shape, trimmedBox, false, false);
    trimmedBox.Get(x0, y0, z0, x1, y1, z1);
    near(x0, 10, 1e-7);
    near(x1, 30, 1e-7);
    auto revTrimmedUse =
        fields({{"entity", lineId}, {"from", .75}, {"to", .25}, {"reverse", true}});
    auto revTrimmedSelection =
        fields({{"sketch", trimmed.id},
                {"mode", "OpenChains"},
                {"regions", QCborArray{QCborArray{QCborArray{revTrimmedUse}}}}});
    near(area(geometry::modelingProfile(trimmed, revTrimmedSelection, mid).shape), 60, 1e-7);

    sketch::Sketch disconnected;
    disconnected.polyline({{0, 0}, {.01, 0}}, false);
    disconnected.polyline({{.02, 0}, {.03, 0}}, false);
    auto disSelected = selection(disconnected, "OpenChains");
    support_fixture::error("ReferenceMissing",
                           [&] { geometry::modelingProfile(disconnected, disSelected, mid); });

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
    support_fixture::error("ResourceLimit", [&] {
        geometry::modelingProfile(many, excessive, fields({{"mode", "Solid"}}));
    });
    QCborArray seventeenRegions;
    for (int i = 0; i < 17; ++i) {
        QCborArray loop;
        for (int j = 0; j < 4; ++j)
            loop.append(members[i * 4 + j]);
        seventeenRegions.append(QCborArray{loop});
    }
    excessive.insert(QStringLiteral("regions"), seventeenRegions);
    support_fixture::error("ResourceLimit", [&] {
        geometry::modelingProfile(many, excessive, fields({{"mode", "Solid"}}));
    });
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
        // Collapsing inward offset on rectangle: thickness .006 m > half-height .005 m
        auto collapsing =
            fields({{"mode", "OneDirection"}, {"thickness", .006}, {"reverse", false}});
        rejects([&] { geometry::modelingProfile(s, all(s.id), collapsing); });
        selectedRegions();
        thinSections();
    });
}
