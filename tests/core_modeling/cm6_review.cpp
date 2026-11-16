#include "ModelingFixture.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "persistence/ProjectStore.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <numbers>
using namespace cad;
using namespace cad::geometry;
using namespace modeling_fixture;
namespace {
double volume(const TopoDS_Shape &shape) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(shape, p);
    return p.Mass() * 1e-9;
}
domain::GeometryReference topFace(const TopoDS_Shape &shape, QString owner) {
    for (TopExp_Explorer face(shape, TopAbs_FACE); face.More(); face.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(face.Current()));
        if (surface.GetType() == GeomAbs_Plane &&
            std::abs(surface.Plane().Axis().Direction().Z()) > .99 &&
            std::abs(surface.Plane().Location().Z() - 50) < 1e-6) {
            auto r = support_fixture::ref(owner, "Face");
            r.topology = supportToken(face.Current());
            return r;
        }
    }
    throw std::runtime_error("Expected top face at 50 mm");
}
struct Plate {
    sketch::Sketch sketch, positions, stop;
    QStringList lines;
    QString point;
    QCborMap ext;
    QString id;
    model::SupportDefinition definition;
    Plate() {
        lines = sketch.rectangle({0, 0}, {.1, .1});
        positions.frame.origin = {0, 0, .05};
        point = positions.point({.03, .03});
        stop.frame.origin = {0, 0, .03};
        ext = extrude(sketch.id);
        ext.insert(QStringLiteral("direction1"), blind(.05));
        id = ext.value("id").toString();
        definition = base({sketch, positions, stop});
    }
    QCborMap hole(QString mode = "Blind") const {
        auto result = evaluateModeling(definition, {ext});
        auto termination = fields({{"mode", mode}, {"depth_reference", "Shoulder"}});
        if (mode == "UpToSurface" || mode == "OffsetFromSurface")
            termination.insert(QStringLiteral("reference"),
                               support_fixture::ref(stop.id, "Plane").map());
        if (mode == "OffsetFromSurface") {
            termination.insert(QStringLiteral("offset"), .005);
            termination.insert(QStringLiteral("offset_reverse"), false);
        }
        return fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(id)},
             {"family", "Simple"},
             {"placement", fields({{"face", topFace(result.part.bodies[0].shape, id).map()},
                                   {"sketch", positions.id},
                                   {"points", QCborArray{point}}})},
             {"sizing", fields({{"mode", "Custom"}, {"diameter", .01}, {"depth", .01}})},
             {"termination", termination}});
    }
    QCborMap pattern(QString seed = {}) const {
        return fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "LinearPattern"},
             {"operation", "NewBody"},
             {"target", nullptr},
             {"seed_kind", "Feature"},
             {"seed", seed.isEmpty() ? id : seed},
             {"direction1",
              fields({{"reference", support_fixture::ref(sketch.id, "Curve", lines[0]).map()},
                      {"spacing", .15},
                      {"count", 2},
                      {"reverse", false}})},
             {"direction2", nullptr}});
    }
};
void repetition() {
    Plate p;
    auto pattern = p.pattern();
    auto first = evaluateModeling(p.definition, {p.ext, pattern});
    auto second = evaluateModeling(p.definition, {p.ext, pattern});
    CHECK(first.part.bodies.size() == 2);
    CHECK(first.part.bodies[1].id == second.part.bodies[1].id);
    CHECK(first.modelingProvenance == second.modelingProvenance);
    CHECK(!first.part.stages.value(pattern.value("id").toString()).IsNull());
    CHECK(first.modelingProvenance[1].toMap().value("outputs").toArray().size() == 1);
    auto grid = pattern;
    grid.insert(QStringLiteral("direction2"),
                fields({{"reference", support_fixture::ref(p.sketch.id, "Curve", p.lines[1]).map()},
                        {"spacing", .15},
                        {"count", 2},
                        {"reverse", false}}));
    auto gridFirst = evaluateModeling(p.definition, {p.ext, grid});
    auto d2 = grid.value("direction2").toMap();
    d2.insert(QStringLiteral("count"), 3);
    grid.insert(QStringLiteral("direction2"), d2);
    auto gridSecond = evaluateModeling(p.definition, {p.ext, grid});
    auto instance = [](const kernel::Part &part) {
        for (auto &body : part.bodies) {
            GProp_GProps mass;
            BRepGProp::VolumeProperties(body.shape, mass);
            auto center = mass.CentreOfMass();
            if (center.Distance(gp_Pnt(200, 50, 25)) < 1e-6)
                return body.id;
        }
        return QString();
    };
    CHECK(!instance(gridFirst.part).isEmpty());
    CHECK(instance(gridFirst.part) == instance(gridSecond.part));
    auto mirror = fields({{"version", 1},
                          {"id", newId()},
                          {"kind", "Mirror"},
                          {"operation", "NewBody"},
                          {"target", nullptr},
                          {"seed_kind", "Body"},
                          {"seed", body(p.id)},
                          {"plane", support_fixture::ref(p.stop.id, "Plane").map()}});
    auto mirrored = evaluateModeling(p.definition, {p.ext, mirror});
    CHECK(mirrored.part.bodies[1].id ==
          evaluateModeling(p.definition, {p.ext, mirror}).part.bodies[1].id);
    auto circular = fields({{"version", 1},
                            {"id", newId()},
                            {"kind", "CircularPattern"},
                            {"operation", "NewBody"},
                            {"target", nullptr},
                            {"seed_kind", "Body"},
                            {"seed", body(p.id)},
                            {"axis", support_fixture::ref(p.sketch.id, "Curve", p.lines[1]).map()},
                            {"count", 3},
                            {"equal_spacing", true},
                            {"angle", 2 * std::numbers::pi},
                            {"reverse", false}});
    auto circ = evaluateModeling(p.definition, {p.ext, circular});
    CHECK(circ.part.bodies.size() == 3);
    near(volume(circ.part.bodies[2].shape), .1 * .1 * .05, 1e-12);
    auto bad = pattern;
    auto d = bad.value("direction1").toMap();
    auto ref = support_fixture::ref(p.id, "Edge");
    ref.topology = "edge/" + QString(64, '0');
    d.insert(QStringLiteral("reference"), ref.map());
    bad.insert(QStringLiteral("direction1"), d);
    support_fixture::error("ReferenceMissing",
                           [&] { evaluateModeling(p.definition, {p.ext, bad}); });

    sketch::Sketch boss;
    boss.frame.origin = {0, 0, .05};
    boss.rectangle({.01, .01}, {.02, .02});
    auto model = base({p.sketch, boss});
    auto add = extrude(boss.id, "Add", body(p.id));
    auto bossPattern = p.pattern(add.value("id").toString());
    auto result = evaluateModeling(model, {p.ext, add, bossPattern});
    near(volume(result.part.bodies[1].shape), .01 * .01 * .01, 1e-12);
}
void holeExtents() {
    Plate p;
    double area = std::numbers::pi * .005 * .005;
    for (auto item : {std::pair{"Blind", .01},
                      {"ThroughAll", .05},
                      {"UpToNext", .05},
                      {"UpToSurface", .02},
                      {"OffsetFromSurface", .015}}) {
        auto h = p.hole(item.first);
        auto r = evaluateModeling(p.definition, {p.ext, h});
        near(.1 * .1 * .05 - volume(r.part.bodies[0].shape), area * item.second, 1e-11);
    }
    auto h = p.hole();
    auto placement = h.value("placement").toMap();
    auto face = placement.value("face").toMap();
    face.insert(QStringLiteral("topology"), "face/" + QString(64, '0'));
    placement.insert(QStringLiteral("face"), face);
    h.insert(QStringLiteral("placement"), placement);
    support_fixture::error("ReferenceMissing", [&] { evaluateModeling(p.definition, {p.ext, h}); });
}
void closedFaceSeeds() {
    Plate p;
    auto baseResult = evaluateModeling(p.definition, {p.ext});
    QCborArray faces;
    for (TopExp_Explorer ex(baseResult.part.bodies[0].shape, TopAbs_FACE); ex.More(); ex.Next()) {
        auto ref = support_fixture::ref(p.id, "Face");
        ref.topology = supportToken(ex.Current());
        faces.append(ref.map());
    }
    auto pattern = p.pattern();
    pattern.insert(QStringLiteral("seed_kind"), "Face");
    pattern.insert(QStringLiteral("seed"), faces);
    auto result = evaluateModeling(p.definition, {p.ext, pattern});
    CHECK(result.part.bodies.size() == 2);
    near(volume(result.part.bodies[1].shape), .1 * .1 * .05, 1e-12);
    CHECK(result.part.bodies[1].id ==
          evaluateModeling(p.definition, {p.ext, pattern}).part.bodies[1].id);
    auto mirror = fields({{"version", 1},
                          {"id", newId()},
                          {"kind", "Mirror"},
                          {"operation", "NewBody"},
                          {"target", nullptr},
                          {"seed_kind", "Face"},
                          {"seed", faces},
                          {"plane", support_fixture::ref(p.stop.id, "Plane").map()}});
    auto mirrored = evaluateModeling(p.definition, {p.ext, mirror});
    near(volume(mirrored.part.bodies[1].shape), .1 * .1 * .05, 1e-12);
    GProp_GProps mass;
    BRepGProp::VolumeProperties(mirrored.part.bodies[1].shape, mass);
    near(mass.CentreOfMass().Z(), 35, 1e-8);
    faces.removeLast();
    pattern.insert(QStringLiteral("seed"), faces);
    support_fixture::error("InvalidGeometry",
                           [&] { evaluateModeling(p.definition, {p.ext, pattern}); });
}
void ribsAndTreatments() {
    Plate p;
    sketch::Sketch line;
    line.frame.origin = {0, 0, .08};
    auto a = line.point({.02, .04}), b = line.point({.08, .04});
    auto id = line.line(a, b);
    auto model = base({p.sketch, line});
    auto rib = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Rib"},
         {"operation", "Add"},
         {"target", body(p.id)},
         {"thickness", .013},
         {"side", "Both"},
         {"direction", "Normal"},
         {"flip_material", true},
         {"profile",
          fields({{"sketch", line.id},
                  {"mode", "OpenChains"},
                  {"regions",
                   QCborArray{QCborArray{QCborArray{fields(
                       {{"entity", id}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}})}}}}})}});
    for (auto side : {"Both", "First", "Second"}) {
        rib.insert(QStringLiteral("side"), side);
        auto result = evaluateModeling(model, {p.ext, rib});
        near(volume(result.part.bodies[0].shape) - .1 * .1 * .05, .06 * .013 * .03, 1e-11);
    }
    rib.insert(QStringLiteral("flip_material"), false);
    rejects([&] { evaluateModeling(model, {p.ext, rib}); });
    auto baseResult = evaluateModeling(p.definition, {p.ext});
    domain::GeometryReference edge = support_fixture::ref(p.id, "Edge");
    for (TopExp_Explorer ex(baseResult.part.bodies[0].shape, TopAbs_EDGE); ex.More(); ex.Next()) {
        edge.topology = supportToken(ex.Current());
        break;
    }
    auto fillet = fields({{"version", 1},
                          {"id", newId()},
                          {"kind", "Fillet"},
                          {"target", body(p.id)},
                          {"method", "Constant"},
                          {"radius", .002},
                          {"propagate_tangent", false},
                          {"edges", QCborArray{edge.map()}}});
    support_fixture::error("UnsupportedOperation",
                           [&] { evaluateModeling(p.definition, {p.ext, fillet}); });
}
void durablePattern() {
    Plate p;
    QTemporaryDir root(QDir::tempPath() + "/cad-module6-review-store-XXXXXX"),
        files(QDir::tempPath() + "/cad-module6-review-files-XXXXXX");
    for (auto *dir : {&root, &files}) {
        CHECK(dir->isValid());
        QFile marker(dir->filePath(".cad-test-root"));
        CHECK(marker.open(QIODevice::WriteOnly));
        marker.write("disposable\n");
    }
    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);
    auto command = [&](QString type, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = type;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };
    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "CM6 review"},
                                                    {"units", "mm"}})));
    auto model = p.definition;
    model.document = document;
    auto pattern = p.pattern();
    store.execute(command("SetModelSupports", fields({{"model", model.map()},
                                                      {"intents", QCborArray{p.ext, pattern}}})));
    auto id = store.evaluatePart(document).bodies[1].id;
    auto path = files.filePath("pattern.cadproj");
    store.save(command("SaveProject"), path);
    store.close();
    store.open(path, "Saved");
    CHECK(store.evaluatePart(document).bodies[1].id == id);
    store.execute(command("Undo"));
    store.execute(command("Redo"));
    CHECK(store.evaluatePart(document).bodies[1].id == id);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        repetition();
        holeExtents();
        closedFaceSeeds();
        ribsAndTreatments();
        durablePattern();
    });
}
