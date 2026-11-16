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
#include <QSignalSpy>
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
bool waitFor(std::function<bool()> ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 15000)
        QTest::qWait(5);
    return ready();
}
struct NativeSession {
    QTemporaryDir root{QDir::tempPath() + "/cad-module6-review-native-XXXXXX"};
    QTemporaryDir files{QDir::tempPath() + "/cad-module6-review-native-files-XXXXXX"};
    std::unique_ptr<MainWindow> window;
    Client *client = nullptr;
    bool start(QString units) {
        for (auto *dir : {&root, &files}) {
            if (!dir->isValid())
                return false;
            QFile marker(dir->filePath(".cad-test-root"));
            if (!marker.open(QIODevice::WriteOnly))
                return false;
            marker.write("disposable\n");
            marker.close();
        }
        QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        window = std::make_unique<MainWindow>(root.filePath("settings.ini"), nullptr, root.path());
        window->resize(1400, 900);
        window->show();
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            window->raise();
            window->activateWindow();
            if (!QTest::qWaitForWindowExposed(window.get()) ||
                !waitFor([&] { return window->isActiveWindow(); }))
                return false;
        }
        client = window->workerClient();
        if (!waitFor([&] { return client->ready(); }))
            return false;
        client->newProject("Part", "CM6 native review", units);
        return idle();
    }
    bool idle() {
        QTest::qWait(20);
        return waitFor([&] { return !client->outstanding(); });
    }
    QCborMap document() const {
        for (auto value : client->state().documentRecords)
            if (value.toMap().value("id") == client->state().activeDocument)
                return value.toMap();
        return {};
    }
};

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

domain::GeometryReference threadFace(const TopoDS_Shape &shape, QString object) {
    auto r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next())
        if (BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType() == GeomAbs_Cylinder) {
            r.topology = supportToken(ex.Current());
            return r;
        }
    throw std::runtime_error("Thread cylinder missing");
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
    void bracketFixture() {
        // Base model: 100 mm x 100 mm x 20 mm block
        sketch::Sketch sBase;
        auto rectLines = sBase.rectangle({0, 0}, {0.100, 0.100});
        auto baseExt = makeExtrude(sBase.id, 0.020);
        auto baseId = baseExt.value("id").toString();

        // 1. Rib: 13 mm thickness on open wire
        sketch::Sketch sWall;
        sWall.rectangle({0, 0}, {0.100, 0.020});
        sWall.frame.origin = {0, 0, .020};
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
                                {"diameter", 0.0085},
                                {"depth", 0.030},
                                {"counterbore_diameter", 0.014},
                                {"counterbore_depth", 0.0042}})},
             {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
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
        double expectedRib = .100 * .100 * .020 + .100 * .020 * .060 + .5 * .060 * .060 * .013;
        double simpleHoles = 2 * std::numbers::pi * .005 * .005 * .020;
        double counterbore = std::numbers::pi * (.007 * .007 * .0042 + .00425 * .00425 * .0158);
        double mirroredExtra = std::numbers::pi * (.007 * .007 - .005 * .005) * .0042;
        checkNear(finalVol, expectedRib - simpleHoles - counterbore - mirroredExtra, 1e-10);
    }

    // Tutorial 2: Revolve, Cut, M42x3.0 cosmetic thread, R5 fillet with tangent propagation
    void threadAndFilletFixture() {
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
                    {"reference", threadFace(resBefore.part.bodies[0].shape, holeId).map()},
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
    void chamferAndBlockedBlendFixture() {
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
        support_fixture::error("UnsupportedOperation",
                               [&] { evaluateModeling(d, {baseExt, fullRoundIntent}); });

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

    void nativeEffectiveScales_data() {
        QTest::addColumn<QString>("units");
        QTest::newRow("mm") << QString("mm");
        QTest::newRow("inch") << QString("in");
    }
    int threadPixels(Viewport *view) {
        auto image = view->grabFramebuffer().convertToFormat(QImage::Format_RGB32);
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                auto c = image.pixelColor(x, y);
                if (c.red() > 180 && c.green() > 110 && c.green() < 230 && c.blue() < 140 &&
                    c.red() > c.green() && c.green() > c.blue())
                    ++count;
            }
        return count;
    }
    void nativeEffectiveScales() {
        QFETCH(QString, units);
        NativeSession session;
        QVERIFY(session.start(units));
        auto &window = *session.window;
        auto *client = session.client;
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            QCOMPARE(QGuiApplication::platformName(), QString("cocoa"));
            QVERIFY(std::abs(window.devicePixelRatioF() -
                             qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
        }
        auto *panel = window.findChild<FeaturePanel *>();
        auto combo = [&](const char *id) { return window.findChild<QComboBox *>(id); };
        auto spin = [&](const char *id) { return window.findChild<QDoubleSpinBox *>(id); };
        auto check = [&](const char *id) { return window.findChild<QCheckBox *>(id); };
        auto selectReference = [&](const char *id, QCborMap ref) {
            auto *box = combo(id);
            for (int i = 0; i < box->count(); ++i)
                if (box->itemData(i).value<QCborMap>() == ref) {
                    box->setCurrentIndex(i);
                    return true;
                }
            return false;
        };
        auto accept = [&] {
            panel->preview();
            if (!waitFor([&] { return !client->outstanding(); }) ||
                client->state().phase != Phase::PreviewReady) {
                qWarning() << client->state().message
                           << window.findChild<QLabel *>("featureDiagnostic")->text();
                return false;
            }
            panel->finish();
            return session.idle() && !panel->active() && client->state().phase != Phase::Failed;
        };
        double unit = units == "in" ? .0254 : .001;
        sketch::Sketch plate, positions;
        plate.name = "Plate";
        positions.name = "Hole positions";
        auto lines = plate.rectangle({0, 0}, {.1, .1});
        positions.frame.origin = {0, 0, .05};
        auto firstPoint = positions.point({.03, .03});
        auto secondPoint = positions.point({.07, .07});
        auto model = base({plate, positions});
        auto ext = makeExtrude(plate.id, .05);
        auto extId = ext.value("id").toString();
        client->projectCommand("SetModelSupports",
                               fields({{"model", model.map()}, {"intents", QCborArray{ext}}}));
        QVERIFY(session.idle());
        QCOMPARE(client->state().phase, Phase::Ready);
        auto evaluated = evaluateModeling(model, {ext});
        auto face = findFace(evaluated.part.bodies[0].shape, extId, 49, 51);
        panel->begin("Hole");
        QVERIFY(session.idle());
        combo("featureProfile")->setCurrentIndex(combo("featureProfile")->findData(positions.id));
        QVERIFY(selectReference("holePlacementFace", face.map()));
        auto *points = window.findChild<QListWidget *>("holePoints");
        QCOMPARE(points->count(), 2);
        for (int i = 0; i < points->count(); ++i)
            points->item(i)->setCheckState(
                points->item(i)->data(Qt::UserRole).toString() == firstPoint ? Qt::Checked
                                                                             : Qt::Unchecked);
        combo("holeFamily")->setCurrentText("Counterbore");
        spin("holeDiameter")->setValue(.0085 / unit);
        spin("holeCounterboreDia")->setValue(.014 / unit);
        spin("holeCounterboreDepth")->setValue(.0042 / unit);
        QVERIFY(accept());
        auto intent =
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap();
        QCOMPARE(intent.value("placement").toMap().value("points").toArray(),
                 QCborArray{firstPoint});
        auto id = intent.value("id").toString();
        panel->begin("Hole", id);
        QVERIFY(session.idle());
        QCOMPARE(combo("featureProfile")->currentData().toString(), positions.id);
        QVERIFY(accept());
        QCOMPARE(
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap(),
            intent);

        // Invalid dimensions must fail without changing either the displayed value or accepted
        // revision.
        panel->begin("Hole", id);
        QVERIFY(session.idle());
        spin("holeCounterboreDia")->setValue(.004 / unit);
        auto revision = client->state().revision;
        panel->preview();
        QVERIFY(session.idle());
        QVERIFY(!window.findChild<QPushButton *>("acceptFeatureButton")->isEnabled());
        QCOMPARE(client->state().revision, revision);
        checkNear(spin("holeCounterboreDia")->value() * unit, .004, unit * .51e-8);
        panel->discard();
        QVERIFY(session.idle());

        // Resolved fixture catalog metadata survives a native no-op edit.
        auto catalog = intent;
        auto reversedPlacement = catalog.value("placement").toMap();
        reversedPlacement.insert(QStringLiteral("points"), QCborArray{secondPoint, firstPoint});
        catalog.insert(QStringLiteral("placement"), reversedPlacement);
        auto sizing = catalog.value("sizing").toMap();
        sizing.insert(QStringLiteral("mode"), "Catalog");
        for (auto pair : {std::pair{"dataset", "test-fixture"},
                          {"version", "2026.1"},
                          {"standard", "Fixture"},
                          {"type", "Counterbore"},
                          {"size", "Test"},
                          {"fit", "Normal"}})
            sizing.insert(QString(pair.first), QString(pair.second));
        catalog.insert(QStringLiteral("sizing"), sizing);
        client->projectCommand("SetModelSupports", fields({{"model", model.map()},
                                                           {"intents", QCborArray{ext, catalog}}}));
        QVERIFY(session.idle());
        panel->begin("Hole", id);
        QVERIFY(session.idle());
        QVERIFY(!check("holeCustomSizing")->isChecked());
        QVERIFY(accept());
        QCOMPARE(
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap(),
            catalog);

        panel->begin("Fillet");
        QVERIFY(session.idle());
        auto *edges = window.findChild<QListWidget *>("filletEdges");
        QSet<QString> verticals;
        for (TopExp_Explorer ex(evaluated.part.bodies[0].shape, TopAbs_EDGE); ex.More();
             ex.Next()) {
            GProp_GProps length;
            BRepGProp::LinearProperties(ex.Current(), length);
            if (std::abs(length.Mass() - 50) < 1e-6)
                verticals.insert(supportToken(ex.Current()));
        }
        int selected = 0;
        for (int i = 0; i < edges->count() && selected < 2; ++i)
            if (verticals.contains(edges->item(i)
                                       ->data(Qt::UserRole)
                                       .value<QCborMap>()
                                       .value("topology")
                                       .toString())) {
                edges->item(i)->setCheckState(Qt::Checked);
                ++selected;
            }
        QCOMPARE(selected, 2);
        spin("filletRadius")->setValue(.002 / unit);
        QVERIFY(accept());
        auto fillet =
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap();
        auto reversedEdges = fillet.value("edges").toArray();
        fillet.insert(QStringLiteral("edges"), QCborArray{reversedEdges[1], reversedEdges[0]});
        client->projectCommand(
            "SetModelSupports",
            fields({{"model", model.map()}, {"intents", QCborArray{ext, catalog, fillet}}}));
        QVERIFY(session.idle());
        auto filletId = fillet.value("id").toString();
        panel->begin("Fillet", filletId);
        QVERIFY(session.idle());
        QVERIFY(accept());
        QCOMPARE(
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap(),
            fillet);
        panel->begin("Fillet", filletId);
        QVERIFY(session.idle());
        combo("filletMethod")->setCurrentText("MultipleRadius");
        auto *radii = window.findChild<QTableWidget *>("filletRadii");
        QCOMPARE(radii->rowCount(), 2);
        radii->item(0, 1)->setText(QString::number(.002 / unit, 'g', 12));
        radii->item(1, 1)->setText(QString::number(.003 / unit, 'g', 12));
        QVERIFY(accept());
        auto multiple =
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap();
        QCOMPARE(multiple.value("radii").toArray().size(), 2);
        checkNear(multiple.value("radii").toArray()[1].toMap().value("radius").toDouble(), .003,
                  1e-11);
        panel->begin("Fillet", filletId);
        QVERIFY(session.idle());
        combo("filletMethod")->setCurrentText("Variable");
        QVERIFY(selectReference("filletVariableEdge", fillet.value("edges").toArray()[0].toMap()));
        auto *law = window.findChild<QTableWidget *>("filletControlPoints");
        law->item(0, 1)->setText(QString::number(.002 / unit, 'g', 12));
        law->item(1, 1)->setText(QString::number(.003 / unit, 'g', 12));
        QVERIFY(accept());
        panel->begin("Fillet", filletId);
        QVERIFY(session.idle());
        law->item(1, 1)->setText(QString::number(.004 / unit, 'g', 12));
        QVERIFY(accept());
        auto variable =
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap();
        checkNear(variable.value("control_points").toArray()[1].toMap().value("radius").toDouble(),
                  .004, 1e-11);

        // A real cosmetic thread reaches the mesh, changes no solid, and has a separate document
        // display setting.
        auto acceptedIntents =
            session.document().value("modeling").toMap().value("intents").toArray();
        auto solid = evaluateModeling(model, acceptedIntents);
        auto cylinder = threadFace(solid.part.bodies[0].shape, filletId);
        panel->begin("CosmeticThread");
        QVERIFY(session.idle());
        QVERIFY(selectReference("threadEdgeRef", cylinder.map()));
        combo("threadTermination")->setCurrentText("Blind");
        spin("threadDepth")->setValue(.010 / unit);
        QVERIFY(accept());
        auto threadedIntents =
            session.document().value("modeling").toMap().value("intents").toArray();
        auto threaded = evaluateModeling(model, threadedIntents);
        checkNear(shapeVolume(threaded.part.bodies[0].shape),
                  shapeVolume(solid.part.bodies[0].shape), 1e-12);
        QVERIFY(std::any_of(client->mesh()->edgeIds.begin(), client->mesh()->edgeIds.end(),
                            [](auto edge) { return edge.startsWith("cosmetic/"); }));
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            QVERIFY(waitFor([&] { return window.sceneView()->isValid(); }));
            auto faceView = positions.frame;
            faceView.origin = {.05, .05, .05};
            QVERIFY(window.sceneView()->normalTo(faceView));
            QTest::qWait(50);
            QVERIFY2(threadPixels(window.sceneView()) > 4,
                     "Cosmetic overlay must be visible in the rendered frame");
            auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            if (!directory.isEmpty())
                QVERIFY(window.grab().save(directory + "/cm6-thread-" + units + ".png"));
        }
        check("threadShadedDisplay")->setChecked(false);
        QVERIFY(session.idle());
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            QTest::qWait(50);
            QCOMPARE(threadPixels(window.sceneView()), 0);
        }

        panel->begin("LinearPattern");
        QVERIFY(session.idle());
        combo("featureOperation")->setCurrentText("NewBody");
        combo("patSeed")->setCurrentIndex(combo("patSeed")->findData(extId));
        QVERIFY(selectReference("linearDir1Ref",
                                support_fixture::ref(plate.id, "Curve", lines[0]).map()));
        spin("linearSpacing1")->setValue(.15 / unit);
        QVERIFY(accept());
        auto pattern =
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap();
        panel->begin("LinearPattern", pattern.value("id").toString());
        QVERIFY(session.idle());
        QCOMPARE(combo("patSeed")->currentData().toString(), extId);
        QVERIFY(accept());
        QCOMPARE(
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap(),
            pattern);
        auto path = session.files.filePath("native.cadproj");
        client->saveProject(path, true);
        QVERIFY(session.idle());
        QVERIFY2(QFile::exists(path), qPrintable(client->state().message));
        QVERIFY2(client->state().current(), qPrintable(client->state().message));
        client->closeProject();
        QVERIFY(session.idle());
        QVERIFY(!client->state().projectOpen);
        client->openProject(path, "Saved");
        QVERIFY(session.idle());
        QVERIFY2(client->state().projectOpen && client->state().current(),
                 qPrintable(client->state().message));
        QCOMPARE(QFileInfo(client->state().savedPath).canonicalFilePath(),
                 QFileInfo(path).canonicalFilePath());
        QCOMPARE(
            session.document().value("modeling").toMap().value("intents").toArray().last().toMap(),
            pattern);
        QCOMPARE(session.document().value("modeling").toMap().value("intents").toArray()[1].toMap(),
                 catalog);
        QVERIFY(!check("threadShadedDisplay")->isChecked());
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            QVERIFY(waitFor([&] { return window.sceneView()->isValid(); }));
            auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            if (!directory.isEmpty())
                QVERIFY(window.grab().save(directory + "/cm6-" + units + ".png"));
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
            auto topFace = findFace(res.part.bodies[0].shape, baseId, 24.0, 26.0);
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

            auto beforeRevision = store.revision();
            support_fixture::error("ReferenceMissing", [&] {
                store.execute(command("SetModelSupports", fields({{"model", mutatedModel.map()},
                                                                  {"intents", intents}})));
            });
            QCOMPARE(store.revision(), beforeRevision);
            auto inspection = store.modelingReferenceCandidates(
                document, mutatedModel.map(), intents, holeIntent.value("id").toString(),
                "placement/face");
            auto mutatedBase = evaluateModeling(mutatedModel, {baseExt});
            auto replacement = findFace(mutatedBase.part.bodies[0].shape, baseId, 24, 26);
            store.execute(command("ResolveModelingReference",
                                  fields({{"model", mutatedModel.map()},
                                          {"intents", intents},
                                          {"owner", holeIntent.value("id")},
                                          {"path", "placement/face"},
                                          {"replacement", replacement.map()},
                                          {"fingerprint", inspection.value("fingerprint")}})));

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
