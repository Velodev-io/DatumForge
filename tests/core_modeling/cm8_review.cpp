#include "ModelingFixture.hpp"
#include "SupportFixture.hpp"
#include "geometry/occt/Loft.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/Part.hpp"
#include "geometry/occt/Shell.hpp"
#include "geometry/occt/Sweep.hpp"
#include "model/Supports.hpp"
#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
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
using support_fixture::error;
using support_fixture::ref;

namespace {

void markDisposable(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}

double shapeVolume(const TopoDS_Shape &s) {
    GProp_GProps g;
    BRepGProp::VolumeProperties(s, g);
    return g.Mass() * 1e-9;
}

domain::GeometryReference findPlanarFace(const TopoDS_Shape &shape, QString object, gp_Dir normal,
                                         double zTarget, double tol = 1e-3) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Plane) {
            auto dir = surf.Plane().Axis().Direction();
            if (dir.IsParallel(normal, 0.1)) {
                double dist = surf.Plane().Distance(gp_Pnt(0, 0, zTarget * 1000.0)) / 1000.0;
                if (dist <= tol) {
                    r.topology = supportToken(ex.Current());
                    return r;
                }
            }
        }
    }
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        r.topology = supportToken(ex.Current());
        return r;
    }
    return r;
}

// 1. T8 Shell Tutorial Model: 100x100x50 mm box with top face removed, 5 mm walls,
// 10 mm bottom wall override, verified against analytical volume and upstream rebuild.
void testT8ShellModel() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module8-review-shell-XXXXXX");
    markDisposable(root);
    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "T8 Shell Tutorial"},
                                                    {"units", "mm"}})));

    sketch::Sketch sBase;
    sBase.name = "BaseRectangle";
    sBase.rectangle({0, 0}, {0.100, 0.100});
    auto baseExt =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "Extrude"},
                {"operation", "NewBody"},
                {"target", QCborValue(nullptr)},
                {"profile",
                 fields({{"sketch", sBase.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})},
                {"start", fields({{"mode", "SketchPlane"}})},
                {"direction1", blind(0.050)},
                {"direction2", QCborValue(nullptr)},
                {"reverse", false},
                {"thin", fields({{"mode", "Solid"}})}});
    auto baseId = baseExt.value("id").toString();

    auto m1 = base({sBase});
    m1.document = document;
    store.execute(command("SetModelSupports",
                          fields({{"model", m1.map()}, {"intents", QCborArray{baseExt}}})));

    auto res1 = evaluateModeling(m1, {baseExt});
    CHECK(res1.part.bodies.size() == 1);
    near(shapeVolume(res1.part.bodies[0].shape), 0.100 * 0.100 * 0.050, 1e-7);

    // Find top face (z=50 mm, normal +Z) and bottom face (z=0 mm, normal -Z)
    auto topFace = findPlanarFace(res1.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.050);
    auto bottomFace = findPlanarFace(res1.part.bodies[0].shape, baseId, gp_Dir(0, 0, -1), 0.000);

    // Add Shell intent: 5 mm thickness inward, top face removed, bottom face overridden to 10 mm
    auto shellIntent = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Shell"},
         {"target", body(baseId)},
         {"thickness", 0.005},
         {"inward", true},
         {"remove_faces", QCborArray{topFace.map()}},
         {"overrides", QCborArray{fields({{"face", bottomFace.map()}, {"thickness", 0.010}})}}});

    store.execute(
        command("SetModelSupports",
                fields({{"model", m1.map()}, {"intents", QCborArray{baseExt, shellIntent}}})));

    auto res2 = evaluateModeling(m1, {baseExt, shellIntent});
    CHECK(res2.part.bodies.size() == 1);
    // Analytical: outer (100x100x50) = 0.000500, inner cavity (90x90x40) = 0.000324, diff =
    // 0.000176 m3
    double expectedVol = (0.100 * 0.100 * 0.050) - (0.090 * 0.090 * 0.040);
    near(shapeVolume(res2.part.bodies[0].shape), expectedVol, 1e-6);

    // Upstream mutation: increase height from 50 mm to 60 mm
    baseExt.insert(QStringLiteral("direction1"), blind(0.060));
    error("ReferenceMissing", [&] {
        store.execute(
            command("SetModelSupports",
                    fields({{"model", m1.map()}, {"intents", QCborArray{baseExt, shellIntent}}})));
    });

    auto inspection =
        store.modelingReferenceCandidates(document, m1.map(), QCborArray{baseExt, shellIntent},
                                          shellIntent.value("id").toString(), "remove_faces/0");
    auto mutatedBase = evaluateModeling(m1, {baseExt});
    auto newTopFace =
        findPlanarFace(mutatedBase.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.060);

    store.execute(command("ResolveModelingReference",
                          fields({{"model", m1.map()},
                                  {"intents", QCborArray{baseExt, shellIntent}},
                                  {"owner", shellIntent.value("id")},
                                  {"path", "remove_faces/0"},
                                  {"replacement", newTopFace.map()},
                                  {"fingerprint", inspection.value("fingerprint")}})));

    auto repairedDoc = store.featureState(document);
    auto repairedIntents = repairedDoc.value("modeling").toMap().value("intents").toArray();
    auto resMut = evaluateModeling(m1, {repairedIntents[0].toMap(), repairedIntents[1].toMap()});
    CHECK(resMut.part.bodies.size() == 1);
    // New analytical: outer (100x100x60) = 0.000600, inner (90x90x50) = 0.000405, diff = 0.000195
    // m3
    double expectedMutVol = (0.100 * 0.100 * 0.060) - (0.090 * 0.090 * 0.050);
    near(shapeVolume(resMut.part.bodies[0].shape), expectedMutVol, 1e-6);
}

// 2. Ordinary Pipe Sweep: circular profile sweep and thin-wall hollow pipe along straight/curved
// path.
void testOrdinaryPipeSweep() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module8-review-pipe-XXXXXX");
    markDisposable(root);
    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "Pipe Sweep Review"},
                                                    {"units", "mm"}})));

    // Path: 100 mm line along Z axis
    sketch::Sketch sPath;
    sPath.name = "PipePath";
    sPath.frame.origin = {0, 0, 0};
    sPath.frame.u = {0, 0, 1};
    sPath.frame.v = {0, 1, 0};
    auto p1 = sPath.point({0, 0});
    auto p2 = sPath.point({0.100, 0});
    auto pathLine = sPath.line(p1, p2);

    auto m = base({sPath});
    m.document = document;

    // A. Solid Circular Sweep: dia 20 mm, length 100 mm
    auto sweepSolid = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Sweep"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"type", "Circular"}, {"diameter", 0.020}})},
         {"path",
          fields({{"reference", ref(sPath.id, "Curve", pathLine).map()}, {"reverse", false}})},
         {"guides", QCborArray{}},
         {"corner_transition", "Round"},
         {"start_tangency", "None"},
         {"end_tangency", "None"},
         {"thin", fields({{"mode", "Solid"}})},
         {"preview_sections", 0}});

    auto resSolid = evaluateModeling(m, {sweepSolid});
    CHECK(resSolid.part.bodies.size() == 1);
    double expectedSolidVol = std::numbers::pi * 0.010 * 0.010 * 0.100;
    near(shapeVolume(resSolid.part.bodies[0].shape), expectedSolidVol, 1e-6);

    // B. Thin Wall Pipe Sweep: wall thickness 2 mm OneDirection
    auto sweepThin = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Sweep"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"type", "Circular"}, {"diameter", 0.020}})},
         {"path",
          fields({{"reference", ref(sPath.id, "Curve", pathLine).map()}, {"reverse", false}})},
         {"guides", QCborArray{}},
         {"corner_transition", "Round"},
         {"start_tangency", "None"},
         {"end_tangency", "None"},
         {"thin", fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", false}})},
         {"preview_sections", 0}});

    auto resThin = evaluateModeling(m, {sweepThin});
    CHECK(resThin.part.bodies.size() == 1);
    // Outer dia 20 mm, inner dia 16 mm -> Vol = pi * (0.010^2 - 0.008^2) * 0.100
    double expectedThinVol = std::numbers::pi * (0.010 * 0.010 - 0.008 * 0.008) * 0.100;
    near(shapeVolume(resThin.part.bodies[0].shape), expectedThinVol, 1e-6);
}

// 3. Thin Chair Frame with Sweep + Existing Mirror
void testChairFrameSweepAndMirror() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module8-review-chair-XXXXXX");
    markDisposable(root);
    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "Chair Frame"},
                                                    {"units", "mm"}})));

    // Half frame path: L-shape from (0.200, 0, 0) up to (0.200, 0, 0.400) and across to (0.200,
    // 0.200, 0.400)
    sketch::Sketch sPath;
    sPath.name = "ChairHalfPath";
    sPath.frame.origin = {0.200, 0, 0};
    sPath.frame.u = {0, 0, 1};
    sPath.frame.v = {0, 1, 0};
    auto ptA = sPath.point({0, 0});
    auto ptB = sPath.point({0.400, 0});
    auto ptC = sPath.point({0.400, 0.200});
    sPath.line(ptA, ptB);
    sPath.line(ptB, ptC);

    sketch::Sketch sMid;
    sMid.name = "ChairMidplane";
    sMid.frame.origin = {0, 0.100, 0};
    sMid.frame.u = {1, 0, 0};
    sMid.frame.v = {0, 0, 1};

    auto m = base({sPath, sMid});
    m.document = document;

    // Swept tube
    auto sweepLeg = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Sweep"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"type", "Circular"}, {"diameter", 0.025}})},
         {"path", fields({{"reference", ref(sPath.id, "Curve").map()}, {"reverse", false}})},
         {"guides", QCborArray{}},
         {"corner_transition", "Round"},
         {"start_tangency", "None"},
         {"end_tangency", "None"},
         {"thin", fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", false}})},
         {"preview_sections", 0}});
    auto sweepId = sweepLeg.value("id").toString();

    // Mirror feature across XZ plane (Y=0.100)
    auto mirrorLeg = fields({{"version", 1},
                             {"id", newId()},
                             {"kind", "Mirror"},
                             {"operation", "NewBody"},
                             {"target", QCborValue(nullptr)},
                             {"seed_kind", "Feature"},
                             {"seed", sweepId},
                             {"plane", ref(sMid.id, "Plane").map()}});

    // Evaluate
    auto res = evaluateModeling(m, {sweepLeg, mirrorLeg});
    CHECK(res.part.bodies.size() >= 2);
    // Both legs should have identical non-zero volumes
    near(shapeVolume(res.part.bodies[0].shape), shapeVolume(res.part.bodies[1].shape), 1e-6);
}

// 4. Impeller Centerline Loft + Existing Circular Pattern
void testImpellerLoftAndCircularPattern() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module8-review-impeller-XXXXXX");
    markDisposable(root);
    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "Impeller Blade Pattern"},
                                                    {"units", "mm"}})));

    // Two parallel aerofoil/rectangle sections along a curved/offset path
    sketch::Sketch s1;
    s1.name = "BladeRoot";
    s1.frame.origin = {0.020, 0, 0};
    s1.rectangle({0, 0}, {0.030, 0.006});

    sketch::Sketch s2;
    s2.name = "BladeTip";
    s2.frame.origin = {0.060, 0, 0.020};
    s2.rectangle({0, 0}, {0.020, 0.004});

    sketch::Sketch sAxis;
    sAxis.name = "CenterAxis";
    sAxis.frame.origin = {0, 0, 0};
    sAxis.frame.u = {0, 0, 1};
    sAxis.frame.v = {0, 1, 0};
    auto pA = sAxis.point({0, 0});
    auto pB = sAxis.point({0.100, 0});
    auto axisLine = sAxis.line(pA, pB);

    auto m = base({s1, s2, sAxis});
    m.document = document;

    auto loftBlade = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Loft"},
         {"output", "Solid"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"sections",
          QCborArray{
              fields({{"sketch", s1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", s2.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})}},
         {"centerline", QCborValue(nullptr)},
         {"guides", QCborArray{}},
         {"connectors", QCborArray{}},
         {"start_condition", "None"},
         {"end_condition", "None"},
         {"thin", fields({{"mode", "Solid"}})}});
    auto bladeId = loftBlade.value("id").toString();

    // Circular pattern: 4 blades around Z axis
    auto circPat = fields({{"version", 1},
                           {"id", newId()},
                           {"kind", "CircularPattern"},
                           {"operation", "NewBody"},
                           {"target", QCborValue(nullptr)},
                           {"seed_kind", "Feature"},
                           {"seed", bladeId},
                           {"axis", ref(sAxis.id, "Curve", axisLine).map()},
                           {"count", 4},
                           {"equal_spacing", true},
                           {"angle", 2.0 * std::numbers::pi},
                           {"reverse", false}});

    auto res = evaluateModeling(m, {loftBlade, circPat});
    CHECK(res.part.bodies.size() == 4);
    double singleBladeVol = shapeVolume(res.part.bodies[0].shape);
    CHECK(singleBladeVol > 0);
    for (size_t i = 1; i < 4; ++i) {
        near(shapeVolume(res.part.bodies[i].shape), singleBladeVol, 1e-6);
    }
}

// 5. Multisection Loft (3 sections) + Surface Loft volume check
void testMultisectionLoftAndSurface() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module8-review-multiloft-XXXXXX");
    markDisposable(root);
    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "Multisection Loft"},
                                                    {"units", "mm"}})));

    sketch::Sketch s1;
    s1.name = "Sec1";
    s1.frame.origin = {0, 0, 0};
    s1.rectangle({0, 0}, {0.050, 0.050});

    sketch::Sketch s2;
    s2.name = "Sec2";
    s2.frame.origin = {0, 0, 0.050};
    s2.rectangle({0, 0}, {0.030, 0.030});

    sketch::Sketch s3;
    s3.name = "Sec3";
    s3.frame.origin = {0, 0, 0.100};
    s3.rectangle({0, 0}, {0.040, 0.040});

    auto m = base({s1, s2, s3});
    m.document = document;

    // A. 3-section Solid Loft
    auto loftSolid = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Loft"},
         {"output", "Solid"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"sections",
          QCborArray{
              fields({{"sketch", s1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", s2.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", s3.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})}},
         {"centerline", QCborValue(nullptr)},
         {"guides", QCborArray{}},
         {"connectors", QCborArray{}},
         {"start_condition", "None"},
         {"end_condition", "None"},
         {"thin", fields({{"mode", "Solid"}})}});

    auto resSolid = evaluateModeling(m, {loftSolid});
    CHECK(resSolid.part.bodies.size() == 1);
    CHECK(shapeVolume(resSolid.part.bodies[0].shape) > 0);

    // B. 3-section Surface Loft (CM-221: measure volume throws UnsupportedOperation)
    auto loftSurface = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Loft"},
         {"output", "Surface"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"sections",
          QCborArray{
              fields({{"sketch", s1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", s2.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", s3.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})}},
         {"centerline", QCborValue(nullptr)},
         {"guides", QCborArray{}},
         {"connectors", QCborArray{}},
         {"start_condition", "None"},
         {"end_condition", "None"},
         {"thin", fields({{"mode", "Solid"}})}});

    auto resSurface = evaluateModeling(m, {loftSurface});
    CHECK(resSurface.part.bodies.size() == 1);
    auto surfBodyId = resSurface.part.bodies[0].id;
    error("UnsupportedOperation", [&] { resSurface.part.measure(1, surfBodyId); });
}

// 6. Edge cases and diagnostics
void testEdgeCasesAndDiagnostics() {
    sketch::Sketch s1;
    s1.rectangle({0, 0}, {0.050, 0.050});
    auto baseExt = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Extrude"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"sketch", s1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})},
         {"start", fields({{"mode", "SketchPlane"}})},
         {"direction1", blind(0.050)},
         {"direction2", QCborValue(nullptr)},
         {"reverse", false},
         {"thin", fields({{"mode", "Solid"}})}});
    auto baseId = baseExt.value("id").toString();
    auto m = base({s1});

    // Shell with invalid thickness collapsing the solid
    auto invalidShell = fields({{"version", 1},
                                {"id", newId()},
                                {"kind", "Shell"},
                                {"target", body(baseId)},
                                {"thickness", 0.060}, // 60 mm > 50 mm dimension
                                {"inward", true},
                                {"remove_faces", QCborArray{}},
                                {"overrides", QCborArray{}}});

    error("InvalidGeometry", [&] { evaluateModeling(m, {baseExt, invalidShell}); });

    // Sweep with crossing or invalid guide curves
    sketch::Sketch sPath;
    auto p1 = sPath.point({0, 0});
    auto p2 = sPath.point({0.050, 0});
    auto pLine = sPath.line(p1, p2);
    auto mPath = base({sPath});

    auto dupGuideSweep = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Sweep"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"type", "Circular"}, {"diameter", 0.010}})},
         {"path", fields({{"reference", ref(sPath.id, "Curve", pLine).map()}, {"reverse", false}})},
         {"guides", QCborArray{ref(sPath.id, "Curve", pLine).map()}}, // Duplicate path as guide
         {"corner_transition", "Round"},
         {"start_tangency", "None"},
         {"end_tangency", "None"},
         {"thin", fields({{"mode", "Solid"}})},
         {"preview_sections", 0}});

    error("InvalidInput", [&] { features::ModelingIntent::parse(dupGuideSweep); });

    // Loft with crossed connectors
    auto crossedLoft = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Loft"},
         {"output", "Solid"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"sections",
          QCborArray{
              fields({{"sketch", s1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", s1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})}},
         {"centerline", QCborValue(nullptr)},
         {"guides", QCborArray{}},
         {"connectors", QCborArray{fields({{"id", "c1"}, {"points", QCborArray{0.2, 0.8}}}),
                                   fields({{"id", "c2"}, {"points", QCborArray{0.7, 0.3}}})}},
         {"start_condition", "None"},
         {"end_condition", "None"},
         {"thin", fields({{"mode", "Solid"}})}});

    error("InvalidInput", [&] { features::ModelingIntent::parse(crossedLoft); });
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto runTest = [](const char *name, auto fn) {
        std::cout << "Starting " << name << "...\n" << std::flush;
        try {
            fn();
            std::cout << name << " passed.\n" << std::flush;
        } catch (const std::exception &e) {
            std::cerr << name << " FAILED: " << e.what() << "\n" << std::flush;
            throw;
        }
    };
    runTest("testT8ShellModel", testT8ShellModel);
    runTest("testOrdinaryPipeSweep", testOrdinaryPipeSweep);
    runTest("testChairFrameSweepAndMirror", testChairFrameSweepAndMirror);
    runTest("testImpellerLoftAndCircularPattern", testImpellerLoftAndCircularPattern);
    runTest("testMultisectionLoftAndSurface", testMultisectionLoftAndSurface);
    runTest("testEdgeCasesAndDiagnostics", testEdgeCasesAndDiagnostics);
    return 0;
}
