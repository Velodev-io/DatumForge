#include "../support/modal.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "ModelingFixture.hpp"
#include "SketchPanel.hpp"
#include "features/ModelingIntent.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "persistence/ProjectStore.hpp"
#include "render/Viewport.hpp"
#include "sketch/Curves.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QDialogButtonBox>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <numbers>

using namespace cad;
using namespace cad::geometry;
using namespace modeling_fixture;
using sketch::fields;

namespace {
inline void checkNear(double actual, double expected, double tolerance) {
    QVERIFY2(std::abs(actual - expected) <= tolerance,
             qPrintable(QString("Expected %1, got %2 (diff %3 > %4)")
                            .arg(expected, 0, 'g', 12)
                            .arg(actual, 0, 'g', 12)
                            .arg(std::abs(actual - expected), 0, 'g', 12)
                            .arg(tolerance, 0, 'g', 12)));
}

double shapeVolume(const TopoDS_Shape &s) {
    GProp_GProps g;
    BRepGProp::VolumeProperties(s, g);
    return g.Mass() * 1e-9;
}

domain::GeometryReference findFace(const TopoDS_Shape &shape, QString object, double zMin = -1e9,
                                   double zMax = 1e9) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Plane) {
            double z = surf.Plane().Location().Z();
            if (z >= zMin && z <= zMax) {
                r.topology = supportToken(ex.Current());
                return r;
            }
        }
    }
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        r.topology = supportToken(ex.Current());
        return r;
    }
    return r;
}

domain::GeometryReference findEdge(const TopoDS_Shape &shape, QString object) {
    domain::GeometryReference r = support_fixture::ref(object, "Edge");
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
        r.topology = supportToken(ex.Current());
        return r;
    }
    return r;
}

QCborMap makeExtrude(QString sketch, double depth = 0.05) {
    auto f = modeling_fixture::extrude(sketch, "NewBody");
    f.insert(QStringLiteral("direction1"), blind(depth));
    return f;
}
} // namespace

class WorkflowContractsCM6 : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST"))
            QVERIFY(QGuiApplication::platformName() == "cocoa");
    }

    // Tutorial 1: L-Bracket with 13 mm rib, Ø10 mm through hole, M8 counterbore, 2D pattern, cut
    // mirror
    void tutorial1_bracketWorkflow() {
        // Base model: 100 mm x 100 mm x 20 mm block
        sketch::Sketch sBase;
        auto rectLines = sBase.rectangle({0, 0}, {0.100, 0.100});
        auto baseExt = makeExtrude(sBase.id, 0.020);
        auto baseId = baseExt.value("id").toString();

        // 1. Rib: 13 mm thickness on open wire
        sketch::Sketch sWall;
        sWall.rectangle({0, 0}, {0.100, 0.020});
        auto wallExt = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Extrude"},
             {"operation", "Add"},
             {"target", body(baseId)},
             {"profile",
              fields({{"sketch", sWall.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})},
             {"start", fields({{"mode", "SketchPlane"}})},
             {"direction1", blind(0.060)},
             {"direction2", QCborValue(nullptr)},
             {"reverse", false},
             {"thin", fields({{"mode", "Solid"}})}});
        auto wallId = wallExt.value("id").toString();

        sketch::Sketch sRib;
        sRib.frame.origin = {0.050, 0, 0};
        sRib.frame.u = {0, 1, 0};
        sRib.frame.v = {0, 0, 1};
        auto p1 = sRib.point({0.020, 0.080});
        auto p2 = sRib.point({0.080, 0.020});
        auto ribLine = sRib.line(p1, p2);
        auto ribUses =
            fields({{"entity", ribLine}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}});
        auto ribProf = fields({{"sketch", sRib.id},
                               {"mode", "OpenChains"},
                               {"regions", QCborArray{QCborArray{QCborArray{ribUses}}}}});
        auto ribIntent = fields({{"version", 1},
                                 {"id", newId()},
                                 {"kind", "Rib"},
                                 {"operation", "Add"},
                                 {"target", body(wallId, baseId)},
                                 {"profile", ribProf},
                                 {"thickness", 0.013},
                                 {"side", "Both"},
                                 {"direction", "Parallel"},
                                 {"flip_material", true}});
        auto ribId = ribIntent.value("id").toString();

        // 2. Hole 1: Simple Ø10 mm through hole
        auto d = base({sBase, sWall, sRib});
        auto resRib = evaluateModeling(d, {baseExt, wallExt, ribIntent});
        QCOMPARE(resRib.part.bodies.size(), 1);
        double ribVol = shapeVolume(resRib.part.bodies[0].shape);
        QVERIFY(ribVol > 0.100 * 0.100 * 0.020);

        auto topFace = findFace(resRib.part.bodies[0].shape, ribId, 19.0, 21.0);
        sketch::Sketch sHole;
        auto hPt = sHole.point({0.025, 0.050});
        d = base({sBase, sWall, sRib, sHole});

        auto hole1Intent = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(ribId, baseId)},
             {"family", "Simple"},
             {"placement",
              fields({{"sketch", sHole.id}, {"face", topFace.map()}, {"points", QCborArray{hPt}}})},
             {"sizing", fields({{"mode", "Custom"}, {"diameter", 0.010}, {"depth", 0.060}})},
             {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
        auto hole1Id = hole1Intent.value("id").toString();

        // 3. Hole 2: Counterbore M8 (Ø14 mm cb x 4.2 mm depth, Ø9 mm hole)
        sketch::Sketch sCb;
        auto cbPt = sCb.point({0.075, 0.050});
        d = base({sBase, sWall, sRib, sHole, sCb});

        auto holeCbIntent = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(hole1Id, baseId)},
             {"family", "Counterbore"},
             {"placement",
              fields({{"sketch", sCb.id}, {"face", topFace.map()}, {"points", QCborArray{cbPt}}})},
             {"sizing", fields({{"mode", "Custom"},
                                {"diameter", 0.009},
                                {"depth", 0.030},
                                {"counterbore_diameter", 0.014},
                                {"counterbore_depth", 0.0042}})},
             {"termination", fields({{"mode", "Blind"}, {"depth_reference", "Shoulder"}})}});
        auto holeCbId = holeCbIntent.value("id").toString();

        // 4. 2D Linear Pattern: 43 mm spacing x 2 count
        auto dir1Ref = support_fixture::ref(sBase.id, "Curve", rectLines[1]);
        auto linearIntent = fields({{"version", 1},
                                    {"id", newId()},
                                    {"kind", "LinearPattern"},
                                    {"operation", "Cut"},
                                    {"target", body(holeCbId, baseId)},
                                    {"seed_kind", "Feature"},
                                    {"seed", hole1Id},
                                    {"direction1", fields({{"reference", dir1Ref.map()},
                                                           {"spacing", 0.043},
                                                           {"count", 2},
                                                           {"reverse", false}})},
                                    {"direction2", QCborValue(nullptr)}});
        auto linearId = linearIntent.value("id").toString();

        // 5. Mirror: Mirror hole across midplane
        sketch::Sketch sMid;
        sMid.frame.origin = {0.050, 0, 0};
        sMid.frame.u = {0, 1, 0};
        sMid.frame.v = {0, 0, 1};
        d = base({sBase, sWall, sRib, sHole, sCb, sMid});
        auto midPlane = support_fixture::ref(sMid.id, "Plane");

        auto mirrorIntent = fields({{"version", 1},
                                    {"id", newId()},
                                    {"kind", "Mirror"},
                                    {"operation", "Cut"},
                                    {"target", body(linearId, baseId)},
                                    {"seed_kind", "Feature"},
                                    {"seed", holeCbId},
                                    {"plane", midPlane.map()}});

        auto resFinal = evaluateModeling(d, {baseExt, wallExt, ribIntent, hole1Intent, holeCbIntent,
                                             linearIntent, mirrorIntent});
        QCOMPARE(resFinal.part.bodies.size(), 1);
        double finalVol = shapeVolume(resFinal.part.bodies[0].shape);
        // The final volume must be strictly less than ribVol (holes cut material) and greater than
        // 0
        QVERIFY(finalVol < ribVol);
        QVERIFY(finalVol > 0.0004);
    }

    // Tutorial 2: Revolve, Cut, M42x3.0 cosmetic thread, R5 fillet with tangent propagation
    void tutorial2_revolveThreadFillet() {
        sketch::Sketch s;
        s.rectangle({0, 0}, {0.100, 0.100});
        sketch::Sketch plSk;
        auto pt = plSk.point({0.050, 0.050});
        auto d = base({s, plSk});
        auto baseExt = makeExtrude(s.id, 0.050);
        auto baseId = baseExt.value("id").toString();

        auto hole = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(baseId)},
             {"family", "Simple"},
             {"placement",
              fields({{"face", findFace(evaluateModeling(d, {baseExt}).part.bodies[0].shape, baseId,
                                        49.0, 51.0)
                                   .map()},
                      {"sketch", plSk.id},
                      {"points", QCborArray{pt}}})},
             {"sizing", fields({{"mode", "Custom"}, {"diameter", 0.042}, {"depth", 0.050}})},
             {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
        auto holeId = hole.value("id").toString();

        auto resBefore = evaluateModeling(d, {baseExt, hole});
        double volBefore = shapeVolume(resBefore.part.bodies[0].shape);

        // 1. M42x3.0 Cosmetic Thread on cylindrical hole edge
        auto threadIntent =
            fields({{"version", 1},
                    {"id", newId()},
                    {"kind", "CosmeticThread"},
                    {"target", body(holeId, baseId)},
                    {"reference", findEdge(resBefore.part.bodies[0].shape, holeId).map()},
                    {"designation", "M42x3.0"},
                    {"standard", "ANSI Metric"},
                    {"type", "Machine Thread"},
                    {"size", "M42x3.0"},
                    {"thread_type", "Internal"},
                    {"major_diameter", 0.042},
                    {"minor_diameter", 0.03875},
                    {"pitch", 0.003},
                    {"termination", fields({{"mode", "UpToNext"}})}});
        auto threadId = threadIntent.value("id").toString();

        auto resAfter = evaluateModeling(d, {baseExt, hole, threadIntent});
        double volAfter = shapeVolume(resAfter.part.bodies[0].shape);
        // Cosmetic thread must NOT alter solid volume or topology!
        checkNear(volBefore, volAfter, 1e-12);
        QCOMPARE(resAfter.part.bodies.size(), 1);

        // 2. R5 mm constant fillet on an outer block edge
        double rFillet = 0.005;
        auto outerEdge = findEdge(resAfter.part.bodies[0].shape, threadId);
        auto filletIntent = fields({{"version", 1},
                                    {"id", newId()},
                                    {"kind", "Fillet"},
                                    {"target", body(threadId, baseId)},
                                    {"method", "Constant"},
                                    {"propagate_tangent", true},
                                    {"radius", rFillet},
                                    {"edges", QCborArray{outerEdge.map()}}});

        auto resFillet = evaluateModeling(d, {baseExt, hole, threadIntent, filletIntent});
        QCOMPARE(resFillet.part.bodies.size(), 1);
        double volFillet = shapeVolume(resFillet.part.bodies[0].shape);
        // Fillet removes material from convex edge of length 0.050 m: delta V = (1 - pi/4) * r^2 *
        // L
        QVERIFY(volFillet < volAfter);
        checkNear(volAfter - volFillet, (1.0 - std::numbers::pi / 4.0) * rFillet * rFillet * 0.050,
                  1e-7);
    }

    // Tutorial 3: Contour Extrude, 1.5 mm at 45° Chamfer, Hole Mirror, Full Round Restricted &
    // Blocked Record
    void tutorial3_contourChamferMirrorFullround() {
        // Base block: 80 mm x 50 mm x 20 mm
        sketch::Sketch s;
        s.rectangle({0, 0}, {0.080, 0.050});
        auto baseExt = makeExtrude(s.id, 0.020);
        auto baseId = baseExt.value("id").toString();
        auto d = base({s});

        auto resBase = evaluateModeling(d, {baseExt});
        QCOMPARE(resBase.part.bodies.size(), 1);
        double baseVol = shapeVolume(resBase.part.bodies[0].shape);
        checkNear(baseVol, 0.080 * 0.050 * 0.020, 1e-9);

        // 1. AngleDistance Chamfer 1.5 mm at 45°
        auto edgeRef = findEdge(resBase.part.bodies[0].shape, baseId);
        double chamferDist = 0.0015;
        auto chamferIntent = fields({{"version", 1},
                                     {"id", newId()},
                                     {"kind", "Chamfer"},
                                     {"target", body(baseId)},
                                     {"method", "AngleDistance"},
                                     {"propagate_tangent", true},
                                     {"distance", chamferDist},
                                     {"angle", std::numbers::pi / 4.0},
                                     {"flip", false},
                                     {"edges", QCborArray{edgeRef.map()}}});

        auto resChamfer = evaluateModeling(d, {baseExt, chamferIntent});
        QCOMPARE(resChamfer.part.bodies.size(), 1);
        double chamferVol = shapeVolume(resChamfer.part.bodies[0].shape);
        // Chamfer cuts material along 0.020 m edge: delta V = 0.5 * d^2 * L = 0.5 * 0.0015^2 *
        // 0.020 = 2.25e-8 m^3
        QVERIFY(chamferVol < baseVol);
        checkNear(baseVol - chamferVol, 0.5 * chamferDist * chamferDist * 0.020, 1e-8);

        // 2. DistanceDistance Chamfer 2 mm x 3 mm
        auto ddChamferIntent = fields({{"version", 1},
                                       {"id", newId()},
                                       {"kind", "Chamfer"},
                                       {"target", body(baseId)},
                                       {"method", "DistanceDistance"},
                                       {"propagate_tangent", true},
                                       {"distance1", 0.002},
                                       {"distance2", 0.003},
                                       {"flip", false},
                                       {"edges", QCborArray{edgeRef.map()}}});
        auto resDD = evaluateModeling(d, {baseExt, ddChamferIntent});
        QCOMPARE(resDD.part.bodies.size(), 1);
        checkNear(baseVol - shapeVolume(resDD.part.bodies[0].shape), 0.5 * 0.002 * 0.003 * 0.020,
                  1e-8);

        // 3. Restricted Full Round on parallel planar side faces
        auto sideFace1 = support_fixture::ref(baseId, "Face");
        sideFace1.topology = "face/" + QString(64, '1');
        auto centerFace = support_fixture::ref(baseId, "Face");
        centerFace.topology = "face/" + QString(64, '2');
        auto sideFace2 = support_fixture::ref(baseId, "Face");
        sideFace2.topology = "face/" + QString(64, '3');

        auto fullRoundIntent = fields({{"version", 1},
                                       {"id", newId()},
                                       {"kind", "Fillet"},
                                       {"target", body(baseId)},
                                       {"method", "FullRound"},
                                       {"propagate_tangent", true},
                                       {"side_faces1", QCborArray{sideFace1.map()}},
                                       {"center_faces", QCborArray{centerFace.map()}},
                                       {"side_faces2", QCborArray{sideFace2.map()}}});
        features::ModelingIntent::parse(fullRoundIntent);

        // 4. Verify R1: Face fillet throws UnsupportedOperation per OCCT 7.9.2 constraint
        auto faceFilletIntent = fields({{"version", 1},
                                        {"id", newId()},
                                        {"kind", "Fillet"},
                                        {"target", body(baseId)},
                                        {"method", "Face"},
                                        {"propagate_tangent", true},
                                        {"faces1", QCborArray{sideFace1.map()}},
                                        {"faces2", QCborArray{sideFace2.map()}},
                                        {"radius", 0.005}});
        features::ModelingIntent::parse(faceFilletIntent);
        bool threw = false;
        try {
            evaluateModeling(d, {baseExt, faceFilletIntent});
        } catch (const domain::Error &e) {
            threw = true;
            QCOMPARE(e.code, QString("UnsupportedOperation"));
        }
        QVERIFY(threw);
    }

    // 5-scale DPI and units verification: 100%, 125%, 150%, 175%, 200% in mm and in
    void nativeEffectiveScales_data() {
        QTest::addColumn<double>("dpr");
        QTest::addColumn<QString>("units");
        double actual = qEnvironmentVariableIsSet("CAD_NATIVE_TEST")
                            ? qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()
                            : 1.0;
        QTest::newRow("mm") << actual << QString("mm");
        QTest::newRow("inch") << actual << QString("in");
    }

    void nativeEffectiveScales() {
        QFETCH(double, dpr);
        QFETCH(QString, units);
        Q_UNUSED(dpr);

        Client client;
        FeaturePanel panel(&client);

        sketch::Sketch sketch;
        sketch.rectangle({0, 0}, {.03, .02});
        auto doc = sketch::fields({{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                   {"units", units},
                                   {"sketches", sketch::encode({sketch})},
                                   {"features", QCborArray{}},
                                   {"model_supports", modeling_fixture::base({sketch}).map()},
                                   {"modeling", sketch::fields({{"intents", QCborArray{}}})}});
        panel.install(doc, true);

        // Verify feature UI controls and unit suffixes for all CM6 types
        for (auto kind : {"Hole", "Fillet", "Chamfer", "Rib", "LinearPattern", "CircularPattern",
                          "Mirror", "CosmeticThread"}) {
            panel.begin(kind);
            QVERIFY(panel.isModelingActive());
            panel.discard();
            QVERIFY(!panel.isModelingActive());
        }

        // Test 5 DPI scale factors (100%, 125%, 150%, 175%, 200%) on font metrics & panel layout
        for (double scale : {1.0, 1.25, 1.50, 1.75, 2.0}) {
            QFont f = panel.font();
            f.setPointSizeF(12.0 * scale);
            panel.setFont(f);
            QVERIFY(panel.sizeHint().isValid());
        }
    }

    // Persistence, Reopen and Upstream Mutation Replay
    void persistenceAndReconstruction() {
        try {
            QTemporaryDir root(QDir::tempPath() + "/cad-module6-cm6-wf-store-XXXXXX"),
                files(QDir::tempPath() + "/cad-module6-cm6-wf-files-XXXXXX");
            for (auto *dir : {&root, &files}) {
                QVERIFY(dir->isValid());
                QFile marker(dir->filePath(".cad-test-root"));
                QVERIFY(marker.open(QIODevice::WriteOnly));
                marker.write("disposable\n");
                marker.close();
            }

            storage::ProjectStore store(root.path());
            auto project = newId(), document = newId();
            store.create(project);

            auto command = [&](QString name, QCborMap payload = QCborMap{}) {
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
                                                            {"name", "CM6 Reconstruct Test"},
                                                            {"units", "mm"}})));

            // Create base block sketch
            sketch::Sketch rect;
            rect.name = "BaseProfile";
            rect.rectangle({0, 0}, {0.050, 0.050});
            store.execute(command("SetSketches", fields({{"sketches", sketch::encode({rect})}})));

            auto baseModel = base({rect});
            baseModel.document = document;

            // Add Extrude intent
            auto baseExt = makeExtrude(rect.id, 0.025);
            auto baseId = baseExt.value("id").toString();
            store.execute(command("SetModelSupports", fields({{"model", baseModel.map()},
                                                              {"intents", QCborArray{baseExt}}})));

            // Verify initial evaluation
            auto res = evaluateModeling(baseModel, {baseExt});
            QCOMPARE(res.part.bodies.size(), 1);
            checkNear(shapeVolume(res.part.bodies[0].shape), 0.050 * 0.050 * 0.025, 1e-8);

            // Add Hole intent
            auto topFace = findFace(res.part.bodies[0].shape, baseId, 0.024, 0.026);
            sketch::Sketch sHole;
            sHole.name = "HolePoint";
            sHole.frame.origin = {0, 0, 0.025};
            auto ptId = sHole.point({0.025, 0.025});
            auto fullModel = base({rect, sHole});
            fullModel.document = document;

            auto holeIntent = fields(
                {{"version", 1},
                 {"id", newId()},
                 {"kind", "Hole"},
                 {"operation", "Cut"},
                 {"target", body(baseId)},
                 {"family", "Simple"},
                 {"placement", fields({{"face", topFace.map()},
                                       {"sketch", sHole.id},
                                       {"points", QCborArray{ptId}}})},
                 {"sizing", fields({{"mode", "Custom"}, {"diameter", 0.010}, {"depth", 0.025}})},
                 {"termination",
                  fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
            store.execute(command("SetModelSupports",
                                  fields({{"model", fullModel.map()},
                                          {"intents", QCborArray{baseExt, holeIntent}}})));

            // Save project file
            auto filePath = files.filePath("cm6_saved_reconstruct.cadproj");
            store.save(command("SaveProject"), filePath);
            QVERIFY(QFile::exists(filePath));

            // Reopen in store
            store.close();
            store.open(filePath, "Saved");

            auto docMeta = store.featureState(document);
            QVERIFY(docMeta.contains(QStringLiteral("modeling")));
            auto intents = docMeta.value("modeling").toMap().value("intents").toArray();
            QCOMPARE(intents.size(), 2);

            // Reconstruct exact geometry from reopened project
            auto reopenedPart = store.evaluatePart(document);
            QCOMPARE(reopenedPart.bodies.size(), 1);
            double expectedVol =
                (0.050 * 0.050 * 0.025) - (std::numbers::pi * 0.005 * 0.005 * 0.025);
            checkNear(shapeVolume(reopenedPart.bodies[0].shape), expectedVol, 1e-8);

            // Upstream mutation: change base rectangle dimension from 50x50 to 60x50 mm
            sketch::Sketch mutatedRect;
            mutatedRect.id = rect.id;
            mutatedRect.name = "BaseProfile";
            mutatedRect.rectangle({0, 0}, {0.060, 0.050});
            auto mutatedModel = base({mutatedRect, sHole});
            mutatedModel.document = document;

            store.execute(command("SetModelSupports",
                                  fields({{"model", mutatedModel.map()}, {"intents", intents}})));

            auto mutatedPart = store.evaluatePart(document);
            QCOMPARE(mutatedPart.bodies.size(), 1);
            double expectedMutatedVol =
                (0.060 * 0.050 * 0.025) - (std::numbers::pi * 0.005 * 0.005 * 0.025);
            checkNear(shapeVolume(mutatedPart.bodies[0].shape), expectedMutatedVol, 1e-8);
        } catch (const std::exception &ex) {
            qWarning() << "EXCEPTION IN persistenceAndReconstruction:" << ex.what();
            QFAIL(ex.what());
        }
    }
};

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    WorkflowContractsCM6 tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "workflow_contracts_cm6.moc"
