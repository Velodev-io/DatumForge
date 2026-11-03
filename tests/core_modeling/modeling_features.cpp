#include "ModelingFixture.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/ModelingProfiles.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <numbers>
using namespace cad;
using namespace modeling_fixture;
namespace {
void startsAndLimits() {
    sketch::Sketch s, limit;
    s.rectangle({0, 0}, {.02, .01});
    const double angle = .2, height = .012;
    limit.frame.origin = {0, 0, height};
    limit.frame.v = {0, std::cos(angle), std::sin(angle)};
    auto point = limit.point({0, 0});
    auto d = base({s, limit});
    auto f = extrude(s.id);
    auto end = blind(.03);
    end.insert(QStringLiteral("mode"), "Midplane");
    f.insert(QStringLiteral("direction1"), end);
    auto m = geometry::evaluateModeling(d, QCborArray{f}).part.measure(0);
    near(m.value("volume_m3").toDouble(), .02 * .01 * .03, 1e-12);
    near(m.value("minimum_m").toArray()[2].toDouble(), -.015, 1e-10);
    std::cout << "midplane passed\n";
    f = extrude(s.id);
    f.insert(QStringLiteral("start"), fields({{"mode", "Offset"}, {"distance", -.003}}));
    m = geometry::evaluateModeling(d, QCborArray{f}).part.measure(0);
    near(m.value("minimum_m").toArray()[2].toDouble(), -.003, 1e-10);
    std::cout << "offset start passed\n";
    auto vertex = support_fixture::ref(limit.id, "Point", point).map();
    f.insert(QStringLiteral("start"), fields({{"mode", "Vertex"}, {"reference", vertex}}));
    m = geometry::evaluateModeling(d, QCborArray{f}).part.measure(0);
    near(m.value("minimum_m").toArray()[2].toDouble(), height, 1e-10);
    std::cout << "vertex start passed\n";
    f = extrude(s.id);
    auto surface = support_fixture::ref(limit.id, "Plane").map();
    end = fields(
        {{"mode", "UpToSurface"}, {"reference", surface}, {"draft", blind().value("draft")}});
    f.insert(QStringLiteral("direction1"), end);
    double expected = .02 * .01 * (height + .005 * std::tan(angle));
    m = geometry::evaluateModeling(d, QCborArray{f}).part.measure(0);
    near(m.value("volume_m3").toDouble(), expected, 1e-12);
    std::cout << "inclined termination passed\n";
    // Updating the same reference changes the actual limiting geometry.
    limit.frame.origin[2] = .018;
    auto changed = base({s, limit});
    near(geometry::evaluateModeling(changed, QCborArray{f})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         .02 * .01 * (.018 + .005 * std::tan(angle)), 1e-12);
    end.insert(QStringLiteral("mode"), "OffsetFromSurface");
    end.insert(QStringLiteral("offset"), .002);
    for (bool reverse : {false, true}) {
        end.insert(QStringLiteral("offset_reverse"), reverse);
        f.insert(QStringLiteral("direction1"), end);
        auto volume = geometry::evaluateModeling(d, QCborArray{f})
                          .part.measure(0)
                          .value("volume_m3")
                          .toDouble();
        near(volume, expected + (reverse ? -1 : 1) * .02 * .01 * .002 / std::cos(angle), 1e-12);
    }
    f = extrude(s.id);
    f.insert(
        QStringLiteral("direction1"),
        fields({{"mode", "UpToVertex"}, {"reference", vertex}, {"draft", blind().value("draft")}}));
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         .02 * .01 * height, 1e-12);
    f = extrude(s.id);
    f.insert(QStringLiteral("start"), fields({{"mode", "Surface"}, {"reference", surface}}));
    m = geometry::evaluateModeling(d, QCborArray{f}).part.measure(0);
    near(m.value("volume_m3").toDouble(), .02 * .01 * .01, 1e-12);
    near(m.value("minimum_m").toArray()[2].toDouble(), height, 1e-10);
    std::cout << "starts and exact plane limits passed\n";
}
void draftedDirections() {
    sketch::Sketch s;
    s.rectangle({0, 0}, {.02, .01});
    auto d = base({s});
    auto f = extrude(s.id);
    auto alpha = 35 * std::numbers::pi / 180;
    f.insert(QStringLiteral("direction1"), blind(.01, alpha, true));
    f.insert(QStringLiteral("direction2"), blind(.015));
    double k = std::tan(alpha), h = .01;
    double expected = .02 * .01 * (h + .015) + (.02 + .01) * k * h * h + 4 * k * k * h * h * h / 3;
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         expected, 1e-12);
    f = extrude(s.id);
    f.insert(QStringLiteral("direction1"), blind(.01, .1, false));
    k = std::tan(.1);
    expected = .02 * .01 * h - (.02 + .01) * k * h * h + 4 * k * k * h * h * h / 3;
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         expected, 1e-12);
    auto mid = blind(.02, .1, true);
    mid.insert(QStringLiteral("mode"), "Midplane");
    f.insert(QStringLiteral("direction1"), mid);
    expected = 2 * (.02 * .01 * h + (.02 + .01) * k * h * h + 4 * k * k * h * h * h / 3);
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         expected, 1e-12);
    sketch::Sketch limit;
    limit.frame.origin[2] = .01;
    auto referenced = base({s, limit});
    f = extrude(s.id);
    auto end = fields({{"mode", "UpToSurface"},
                       {"reference", support_fixture::ref(limit.id, "Plane").map()},
                       {"draft", blind(.01, .1, true).value("draft")}});
    f.insert(QStringLiteral("direction1"), end);
    expected = .02 * .01 * h + (.02 + .01) * k * h * h + 4 * k * k * h * h * h / 3;
    near(geometry::evaluateModeling(referenced, QCborArray{f})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         expected, 1e-12);
    std::cout << "independent drafted directions passed\n";
}
void revolves() {
    sketch::Sketch s;
    s.rectangle({.01, 0}, {.02, .01});
    auto d = base({s});
    model::Datum axis;
    axis.kind = "DatumAxis";
    axis.mode = "Principal";
    axis.parameters = fields({{"axis", "Y"}});
    d.datums = {axis};
    d.history.append(axis.id);
    auto f = extrude(s.id);
    f.insert(QStringLiteral("kind"), "Revolve");
    f.insert(QStringLiteral("axis"), support_fixture::ref(axis.id, "Axis").map());
    f.insert(QStringLiteral("direction1"),
             fields({{"mode", "Blind"}, {"value", 2 * std::numbers::pi}}));
    double expected = std::numbers::pi * (.02 * .02 - .01 * .01) * .01;
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         expected, 1e-12);
    f.insert(QStringLiteral("direction1"),
             fields({{"mode", "Blind"}, {"value", std::numbers::pi / 2}}));
    f.insert(QStringLiteral("direction2"),
             fields({{"mode", "Blind"}, {"value", std::numbers::pi / 2}}));
    f.insert(QStringLiteral("reverse"), true);
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         expected / 2, 1e-12);
    f.insert(QStringLiteral("direction1"),
             fields({{"mode", "Midplane"}, {"value", std::numbers::pi}}));
    f.insert(QStringLiteral("direction2"), QCborValue(nullptr));
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         expected / 2, 1e-12);
    f.insert(QStringLiteral("thin"),
             fields({{"mode", "OneDirection"}, {"thickness", .001}, {"reverse", false}}));
    double wall = expected - std::numbers::pi * (.019 * .019 - .011 * .011) * .008;
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         wall / 2, 1e-12);
    sketch::Sketch open;
    open.polyline({{.01, 0}, {.01, .01}}, false);
    auto openModel = base({open});
    openModel.datums = {axis};
    openModel.history.append(axis.id);
    QCborArray uses;
    for (auto &e : open.entities)
        if (e.kind == "Line")
            uses.append(fields({{"entity", e.id}, {"from", 0.}, {"to", 1.}, {"reverse", false}}));
    f.insert(QStringLiteral("profile"), fields({{"sketch", open.id},
                                                {"mode", "OpenChains"},
                                                {"regions", QCborArray{QCborArray{uses}}}}));
    f.insert(QStringLiteral("direction1"),
             fields({{"mode", "Blind"}, {"value", 2 * std::numbers::pi}}));
    f.insert(QStringLiteral("thin"), fields({{"mode", "Midplane"}, {"thickness", .002}}));
    near(geometry::evaluateModeling(openModel, QCborArray{f})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         std::numbers::pi * (.011 * .011 - .009 * .009) * .01, 1e-12);
    std::cout << "solid and thin revolves passed\n";
}
void derivedAxis() {
    sketch::Sketch source, target, profile;
    auto line = source.line(source.point({0, 0}), source.point({0, .01}));
    profile.rectangle({.01, 0}, {.02, .01});
    model::Conversion conversion;
    conversion.sketch = target.id;
    conversion.sources = {support_fixture::ref(source.id, "Curve", line)};
    conversion.construction = true;
    auto d = base({source, target, profile});
    d.conversions = {conversion};
    d.history = {source.id, conversion.id, target.id, profile.id};
    auto f = extrude(profile.id);
    f.insert(QStringLiteral("kind"), "Revolve");
    f.insert(QStringLiteral("axis"), support_fixture::ref(conversion.id, "Curve").map());
    f.insert(QStringLiteral("direction1"),
             fields({{"mode", "Blind"}, {"value", 2 * std::numbers::pi}}));
    near(geometry::evaluateModeling(d, QCborArray{f}).part.measure(0).value("volume_m3").toDouble(),
         std::numbers::pi * (.02 * .02 - .01 * .01) * .01, 1e-12);
    std::cout << "derived exact axis passed\n";
}
void booleansAndBodies() {
    sketch::Sketch s, cut, addition;
    s.rectangle({0, 0}, {.04, .03});
    cut.circle(cut.point({.01, .01}), .003);
    addition.rectangle({.005, .005}, {.015, .015});
    addition.frame.origin[2] = .02;
    auto d = base({s, cut, addition});
    auto first = extrude(s.id);
    auto id = first.value("id").toString();
    auto tool = extrude(cut.id, "Cut", body(id));
    auto end = fields({{"mode", "ThroughAll"}, {"draft", blind().value("draft")}});
    tool.insert(QStringLiteral("direction1"), end);
    auto result = geometry::evaluateModeling(d, QCborArray{first, tool});
    double expected = .04 * .03 * .01 - std::numbers::pi * .003 * .003 * .01;
    near(result.part.measure(0).value("volume_m3").toDouble(), expected, 1e-12);
    std::cout << "through-all cut passed\n";
    auto before = result.part.measure(0);
    // An earlier snapshot is not the current writable target stage.
    auto stale = extrude(cut.id, "Cut", body(id));
    rejects([&] { geometry::evaluateModeling(d, QCborArray{first, tool, stale}); });
    CHECK(result.part.measure(0) == before);
    auto add = extrude(addition.id, "Add", body(id));
    add.insert(QStringLiteral("reverse"), true);
    for (auto mode : {"UpToNext", "UpToBody"}) {
        auto limit = fields({{"mode", mode}, {"draft", blind().value("draft")}});
        if (QString(mode) == "UpToBody") {
            limit.insert(QStringLiteral("reference"), support_fixture::ref(id, "Object").map());
            limit.insert(QStringLiteral("body"), id);
        }
        add.insert(QStringLiteral("direction1"), limit);
        near(geometry::evaluateModeling(d, QCborArray{first, add})
                 .part.measure(0)
                 .value("volume_m3")
                 .toDouble(),
             .04 * .03 * .01 + .01 * .01 * .01, 1e-12);
        std::cout << mode << " add passed\n";
        limit.insert(QStringLiteral("draft"), blind(.01, .1, true).value("draft"));
        add.insert(QStringLiteral("direction1"), limit);
        double k = std::tan(.1), h = .01;
        // Starts at z=20 mm and expands while approaching the base at z=10.
        near(geometry::evaluateModeling(d, QCborArray{first, add})
                 .part.measure(0)
                 .value("volume_m3")
                 .toDouble(),
             .04 * .03 * .01 + .01 * .01 * h + .02 * k * h * h + 4 * k * k * h * h * h / 3, 1e-12);
    }
    // A thin cut is a real volumetric annular tool.
    tool = extrude(cut.id, "Cut", body(id));
    tool.insert(QStringLiteral("thin"),
                fields({{"mode", "OneDirection"}, {"thickness", .001}, {"reverse", false}}));
    // A through annulus isolates an inner solid and must be refused by the
    // existing one-solid target policy; a half-depth annulus stays connected.
    rejects([&] { geometry::evaluateModeling(d, QCborArray{first, tool}); });
    tool.insert(QStringLiteral("direction1"), blind(.005));
    near(geometry::evaluateModeling(d, QCborArray{first, tool})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         .04 * .03 * .01 - std::numbers::pi * (.003 * .003 - .002 * .002) * .005, 1e-12);
    sketch::Sketch groove;
    groove.frame.origin = {.02, .015, 0};
    groove.frame.v = {0, 0, 1};
    groove.rectangle({.003, 0}, {.005, .005});
    auto a = groove.point({0, 0}), b = groove.point({0, .01});
    auto axisLine = groove.line(a, b);
    groove.entity(axisLine).construction = true;
    auto g = base({s, groove});
    auto cutRevolve = extrude(groove.id, "Cut", body(id));
    cutRevolve.insert(QStringLiteral("kind"), "Revolve");
    cutRevolve.insert(QStringLiteral("axis"),
                      support_fixture::ref(groove.id, "Curve", axisLine).map());
    cutRevolve.insert(QStringLiteral("direction1"),
                      fields({{"mode", "Blind"}, {"value", 2 * std::numbers::pi}}));
    near(geometry::evaluateModeling(g, QCborArray{first, cutRevolve})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         .04 * .03 * .01 - std::numbers::pi * (.005 * .005 - .003 * .003) * .005, 1e-12);
    std::cout << "Boolean and body-terminated features passed\n";
}
void curvedLimits() {
    sketch::Sketch s;
    s.circle(s.point({0, 0}), .005);
    features::Feature imported;
    imported.kind = "Imported";
    imported.parameters = fields({{"source_sha256", QString(64, 'a')},
                                  {"format", "STEP"},
                                  {"units", "m"},
                                  {"healing", "None"},
                                  {"source_name", "analytic sphere"},
                                  {"report", QCborMap{}},
                                  {"operation", "NewBody"}});
    auto sphere = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, -10), 20).Shape();
    auto d = base({s});
    d.features = features::encode({imported});
    d.history.append(imported.id);
    auto f = extrude(s.id);
    TopExp_Explorer face(sphere, TopAbs_FACE);
    auto ref = support_fixture::ref(imported.id, "Face");
    ref.topology = geometry::supportToken(face.Current());
    f.insert(QStringLiteral("direction1"), fields({{"mode", "UpToSurface"},
                                                   {"reference", ref.map()},
                                                   {"draft", blind().value("draft")}}));
    geometry::SupportBuild build = [&](const QCborArray &sketches, const QCborArray &features) {
        return kernel::Part::build(sketches, features, nullptr, false,
                                   [&](const features::Feature &) { return sphere; });
    };
    double r = .005, R = .02;
    double expected = 2 * std::numbers::pi / 3 * (R * R * R - std::pow(R * R - r * r, 1.5)) -
                      std::numbers::pi * r * r * .01;
    near(geometry::evaluateModeling(d, QCborArray{f}, build)
             .part.measure(0, f.value("id").toString())
             .value("volume_m3")
             .toDouble(),
         expected, 1e-12);
    std::cout << "exact curved termination passed\n";
}
void openCuts() {
    sketch::Sketch baseSketch, chain;
    baseSketch.rectangle({0, 0}, {.04, .03});
    chain.polyline({{-.01, .01}, {.05, .01}}, false);
    auto d = base({baseSketch, chain});
    auto first = extrude(baseSketch.id);
    auto cut = extrude(chain.id, "Cut", body(first.value("id").toString()));
    QCborArray uses;
    for (auto &e : chain.entities)
        if (e.kind == "Line")
            uses.append(fields({{"entity", e.id}, {"from", 0.}, {"to", 1.}, {"reverse", false}}));
    cut.insert(QStringLiteral("profile"), fields({{"sketch", chain.id},
                                                  {"mode", "OpenChains"},
                                                  {"regions", QCborArray{QCborArray{uses}}}}));
    cut.insert(QStringLiteral("direction1"),
               fields({{"mode", "ThroughAll"}, {"draft", blind().value("draft")}}));
    for (bool flip : {false, true}) {
        cut.insert(QStringLiteral("flip_side"), flip);
        double volume = geometry::evaluateModeling(d, QCborArray{first, cut})
                            .part.measure(0)
                            .value("volume_m3")
                            .toDouble();
        near(volume, .04 * (flip ? .02 : .01) * .01, 1e-12);
    }
    cut.insert(QStringLiteral("flip_side"), false);
    cut.insert(QStringLiteral("direction1"), blind(.005));
    near(geometry::evaluateModeling(d, QCborArray{first, cut})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         .04 * .03 * .01 - .04 * .02 * .005, 1e-12);
    std::cout << "open cuts and explicit side passed\n";
}
void standaloneDraft() {
    sketch::Sketch s;
    s.rectangle({0, 0}, {.02, .01});
    auto d = base({s});
    model::Datum axis;
    axis.kind = "DatumAxis";
    axis.mode = "Principal";
    axis.parameters = fields({{"axis", "Z"}});
    d.datums = {axis};
    d.history.append(axis.id);
    auto first = extrude(s.id);
    auto accepted = geometry::evaluateModeling(d, QCborArray{first});
    auto stage = first.value("id").toString();
    QCborArray faces;
    for (TopExp_Explorer face(accepted.part.stages.value(stage), TopAbs_FACE); face.More();
         face.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(face.Current()));
        if (surface.GetType() == GeomAbs_Plane &&
            std::abs(surface.Plane().Axis().Direction().Z()) < 1e-10) {
            auto r = support_fixture::ref(stage, "Face");
            r.topology = geometry::supportToken(face.Current());
            faces.append(r.map());
        }
    }
    CHECK(faces.size() == 4);
    auto f = fields({{"version", 1},
                     {"id", newId()},
                     {"kind", "Draft"},
                     {"target", body(stage)},
                     {"neutral", support_fixture::ref(s.id, "Plane").map()},
                     {"pull", support_fixture::ref(axis.id, "Axis").map()},
                     {"faces", faces},
                     {"angle", .1},
                     {"outward", true}});
    double k = std::tan(.1), h = .01;
    double expected = .02 * .01 * h + (.02 + .01) * k * h * h + 4 * k * k * h * h * h / 3;
    near(geometry::evaluateModeling(d, QCborArray{first, f})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         expected, 1e-12);
    near(accepted.part.measure(0).value("volume_m3").toDouble(), .02 * .01 * .01, 1e-12);
    auto broken = f;
    broken.insert(QStringLiteral("neutral"), support_fixture::ref(newId(), "Plane").map());
    rejects([&] { geometry::evaluateModeling(d, QCborArray{first, broken}); });
    sketch::Sketch circle;
    circle.circle(circle.point({0, 0}), .01);
    auto curved = base({circle});
    curved.datums = {axis};
    curved.history.append(axis.id);
    auto cylinder = extrude(circle.id);
    auto cylindrical = geometry::evaluateModeling(curved, QCborArray{cylinder});
    auto owner = cylinder.value("id").toString();
    QCborArray wall;
    for (TopExp_Explorer face(cylindrical.part.stages.value(owner), TopAbs_FACE); face.More();
         face.Next())
        if (BRepAdaptor_Surface(TopoDS::Face(face.Current())).GetType() == GeomAbs_Cylinder) {
            auto r = support_fixture::ref(owner, "Face");
            r.topology = geometry::supportToken(face.Current());
            wall.append(r.map());
        }
    CHECK(wall.size() == 1);
    auto cylindricalDraft = f;
    cylindricalDraft.insert(QStringLiteral("target"), body(owner));
    cylindricalDraft.insert(QStringLiteral("neutral"),
                            support_fixture::ref(circle.id, "Plane").map());
    cylindricalDraft.insert(QStringLiteral("faces"), wall);
    double radius = .01, end = radius + h * k;
    near(geometry::evaluateModeling(curved, QCborArray{cylinder, cylindricalDraft})
             .part.measure(0)
             .value("volume_m3")
             .toDouble(),
         std::numbers::pi * h * (radius * radius + radius * end + end * end) / 3, 1e-12);
    std::cout << "standalone draft passed\n";
}
void failureBoundaries() {
    sketch::Sketch s, limit;
    s.rectangle({0, 0}, {.02, .01});
    limit.rectangle({.008, .003}, {.012, .007});
    limit.frame.origin[2] = .01;
    auto d = base({s, limit});
    auto f = extrude(s.id);
    features::Feature imported;
    imported.kind = "Imported";
    imported.parameters = fields({{"source_sha256", QString(64, 'b')},
                                  {"format", "STEP"},
                                  {"units", "m"},
                                  {"healing", "None"},
                                  {"source_name", "partial surface"},
                                  {"report", QCborMap{}},
                                  {"operation", "NewBody"}});
    auto tool =
        geometry::modelingProfile(
            limit, fields({{"sketch", limit.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
            fields({{"mode", "Solid"}}))
            .shape;
    // A real producing stage, with a small top face that cannot cover the profile.
    auto slab = BRepPrimAPI_MakePrism(tool, gp_Vec(0, 0, 1)).Shape();
    auto badref = support_fixture::ref(imported.id, "Face");
    for (TopExp_Explorer face(slab, TopAbs_FACE); face.More(); face.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(face.Current()));
        if (surface.GetType() == GeomAbs_Plane &&
            std::abs(surface.Plane().Location().Z() - 10) < 1e-7)
            badref.topology = geometry::supportToken(face.Current());
    }
    CHECK(!badref.topology.isEmpty());
    d.features = features::encode({imported});
    d.history.append(imported.id);
    f.insert(QStringLiteral("direction1"), fields({{"mode", "UpToSurface"},
                                                   {"reference", badref.map()},
                                                   {"draft", blind().value("draft")}}));
    geometry::SupportBuild build = [&](const QCborArray &sketches, const QCborArray &features) {
        return kernel::Part::build(sketches, features, nullptr, false,
                                   [&](const features::Feature &) { return slab; });
    };
    rejects([&] { geometry::evaluateModeling(d, QCborArray{f}, build); });
    // Missing/duplicate identities, wrong sketch member, reversed and crossing limits.
    d = base({s, limit});
    f = extrude(s.id);
    f.insert(QStringLiteral("direction1"),
             fields({{"mode", "UpToSurface"},
                     {"reference", support_fixture::ref(limit.id, "Plane").map()},
                     {"draft", blind().value("draft")}}));
    limit.frame.origin[2] = -.01;
    d = base({s, limit});
    rejects([&] { geometry::evaluateModeling(d, QCborArray{f}); });
    limit.frame.origin[2] = -.001;
    limit.frame.v = {0, std::cos(.3), std::sin(.3)};
    d = base({s, limit});
    rejects([&] { geometry::evaluateModeling(d, QCborArray{f}); });
    auto normal = extrude(s.id);
    rejects([&] { geometry::evaluateModeling(base({s}), QCborArray{normal, normal}); });
    auto immutable = d.map();
    rejects([&] { geometry::evaluateModeling(d, QCborArray{f}); });
    CHECK(d.map() == immutable);
    auto expensive = extrude(s.id);
    auto curve = std::find_if(s.entities.begin(), s.entities.end(), [](auto &e) {
                     return e.kind == "Line";
                 })->id;
    auto use = fields({{"entity", curve}, {"from", 0.}, {"to", 1.}, {"reverse", false}});
    QCborArray regions;
    for (int i = 0; i < 32; ++i) {
        QCborArray holes;
        for (int j = 0; j < 8; ++j)
            holes.append(QCborArray{use});
        regions.append(holes);
    }
    expensive.insert(QStringLiteral("profile"),
                     fields({{"sketch", s.id}, {"mode", "SelectedRegions"}, {"regions", regions}}));
    QCborArray heavy;
    while (QCborValue(heavy).toCbor().size() < 140000) {
        expensive.insert(QStringLiteral("id"), newId());
        heavy.append(expensive);
    }
    CHECK(QCborValue(heavy).toCbor().size() < 240000);
    exchange::Control admission;
    admission.canceled = [] { return true; };
    support_fixture::error("ResourceLimit",
                           [&] { geometry::evaluateModeling(base({s}), heavy, {}, admission); });
    std::cout << "geometry and reference failure boundaries passed\n";
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        sketch::Sketch s;
        s.rectangle({0, 0}, {.02, .01});
        auto d = base({s});
        auto f = extrude(s.id);
        auto result = geometry::evaluateModeling(d, QCborArray{f});
        near(result.part.measure(0).value("volume_m3").toDouble(), .02 * .01 * .01, 1e-12);
        CHECK(result.order.back() == f.value("id").toString());
        CHECK(!result.part.history.empty());
        f.insert(QStringLiteral("direction2"), blind(.015));
        result = geometry::evaluateModeling(d, QCborArray{f});
        auto m = result.part.measure(0);
        near(m.value("volume_m3").toDouble(), .02 * .01 * .025, 1e-12);
        near(m.value("minimum_m").toArray()[2].toDouble(), -.015, 1e-10);
        near(m.value("maximum_m").toArray()[2].toDouble(), .01, 1e-10);
        f.insert(QStringLiteral("reverse"), true);
        m = geometry::evaluateModeling(d, QCborArray{f}).part.measure(0);
        near(m.value("minimum_m").toArray()[2].toDouble(), -.01, 1e-10);
        near(m.value("maximum_m").toArray()[2].toDouble(), .015, 1e-10);
        exchange::Control canceled;
        canceled.canceled = [] { return true; };
        support_fixture::error("Canceled",
                               [&] { geometry::evaluateModeling(d, QCborArray{f}, {}, canceled); });
        startsAndLimits();
        draftedDirections();
        revolves();
        derivedAxis();
        booleansAndBodies();
        curvedLimits();
        openCuts();
        standaloneDraft();
        failureBoundaries();
    });
}
