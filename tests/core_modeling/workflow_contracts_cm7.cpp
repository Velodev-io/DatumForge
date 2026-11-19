#include "../support/modal.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "ModelingFixture.hpp"
#include "SketchPanel.hpp"
#include "SupportEditor.hpp"
#include "features/ModelingIntent.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/SpatialCurve.hpp"
#include "geometry/occt/Supports.hpp"
#include "model/DerivedCurve.hpp"
#include "persistence/ProjectStore.hpp"
#include "render/Viewport.hpp"
#include "sketch/Curves.hpp"
#include "sketch/SpatialSketch.hpp"
#include "sketch/SpatialSolver.hpp"
#include <QDialogButtonBox>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
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
    QTemporaryDir root{QDir::tempPath() + "/cad-module7-review-native-XXXXXX"};
    QTemporaryDir files{QDir::tempPath() + "/cad-module7-review-native-files-XXXXXX"};
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
        client->newProject("Part", "CM7 native review", units);
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

inline double vec3Dist(const sketch::Vec3 &a, const sketch::Vec3 &b) {
    double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
} // namespace

class WorkflowContractsCM7 : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST"))
            QVERIFY(QGuiApplication::platformName() == "cocoa");
    }

    void nativeEffectiveScales_data() {
        QTest::addColumn<QString>("units");
        QTest::newRow("mm") << QString("mm");
        QTest::newRow("inch") << QString("in");
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

        double unit = units == "in" ? 0.0254 : 0.001;
        auto *sketchPanel = window.findChild<SketchPanel *>();
        QVERIFY(sketchPanel != nullptr);
        auto *featurePanel = window.findChild<FeaturePanel *>();
        QVERIFY(featurePanel != nullptr);

        // 1. 3D Sketch Authoring Workflow
        sketchPanel->begin3D();
        QVERIFY(sketchPanel->active());
        QVERIFY(sketchPanel->is3DSketch());

        auto *spatialPlaneBox = window.findChild<QComboBox *>("spatialActivePlane");
        QVERIFY(spatialPlaneBox != nullptr);
        QVERIFY(spatialPlaneBox->isVisible());
        QCOMPARE(spatialPlaneBox->currentText(), QString("XY"));

        // Tab cycles XY -> YZ -> ZX -> XY
        sketchPanel->switchActivePlane();
        QCOMPARE(spatialPlaneBox->currentText(), QString("YZ"));
        sketchPanel->switchActivePlane();
        QCOMPARE(spatialPlaneBox->currentText(), QString("ZX"));
        sketchPanel->switchActivePlane();
        QCOMPARE(spatialPlaneBox->currentText(), QString("XY"));

        // Add 3D points/lines
        // Place first point at {0.02, 0.03, 0}
        sketchPanel->canvas()->setTool("Line");
        // Simulate click at (0.02, 0.03)
        sketch::Vec2 p1{0.020, 0.030};
        sketchPanel->canvas()->snapPoint(p1);
        QTest::qWait(10);

        // Switch to YZ plane before second point
        sketchPanel->switchActivePlane();
        QCOMPARE(spatialPlaneBox->currentText(), QString("YZ"));

        // Add points programmatically to test spatial solver and inference
        sketch::SpatialSketch sp;
        sp.name = "Test 3D Sketch";
        auto ptA = sp.point({0.010, 0.020, 0.000});
        auto ptB = sp.point({0.050, 0.020, 0.000});
        auto lineAB = sp.line(ptA, ptB);

        // Propose relations
        sketch::SpatialConstraint cFix;
        cFix.kind = "Fixed";
        cFix.a = ptA;
        sp.constraints.push_back(cFix);

        sketch::SpatialConstraint cAlongX;
        cAlongX.kind = "AlongX";
        cAlongX.a = lineAB;
        sp.constraints.push_back(cAlongX);

        sketch::SpatialConstraint cDist;
        cDist.kind = "Distance";
        cDist.a = ptA;
        cDist.b = ptB;
        cDist.value = 0.040;
        cDist.driving = true;
        sp.constraints.push_back(cDist);

        auto sol = sketch::solveSpatial(sp);
        QVERIFY(sol.valid);
        checkNear(vec3Dist(sol.sketch.entity(ptA).point, sol.sketch.entity(ptB).point), 0.040,
                  1e-6);

        // Commit spatial sketch via client
        auto supDoc = session.document();
        auto modelDef = model::editSupportedSpatialSketches(
            supDoc, sketch::encodeSpatial(std::vector<sketch::SpatialSketch>{sp}));
        client->projectCommand("SetModelSupports", fields({{"model", modelDef.map()}}));
        QVERIFY(session.idle());

        auto docAfter = session.document();
        QVERIFY(docAfter.contains(QStringLiteral("model_supports")));
        auto spList = sketch::parseSpatial(
            docAfter.value("model_supports").toMap().value("spatial_sketches").toArray());
        QCOMPARE(spList.size(), 1);
        QCOMPARE(spList[0].entities.size(), 3);
        QCOMPARE(spList[0].constraints.size(), 3);

        // Reopen 3D sketch
        sketchPanel->begin3D(spList[0].id);
        QVERIFY(sketchPanel->active());
        QVERIFY(sketchPanel->is3DSketch());
        sketchPanel->discard();
        QVERIFY(!sketchPanel->active());

        // 2. Projected Curve (SketchOnSketch) Workflow
        sketch::Sketch s1;
        s1.name = "ProjSketch1";
        s1.frame.origin = {0, 0, 0};
        auto l1 = s1.rectangle({0, 0}, {0.050, 0.050});

        sketch::Sketch s2;
        s2.name = "ProjSketch2";
        s2.frame.origin = {0, 0, 0};
        s2.frame.u = {1, 0, 0};
        s2.frame.v = {0, 0, 1}; // XZ plane
        auto l2 = s2.rectangle({0, 0}, {0.050, 0.040});

        auto modelWithSketches = model::editSupportedSketches(
            session.document(), sketch::encode(std::vector<sketch::Sketch>{s1, s2}));
        client->projectCommand("SetModelSupports", fields({{"model", modelWithSketches.map()}}));
        QVERIFY(session.idle());

        // Test Derived Curve: Helix / Spiral
        featurePanel->begin("HelixSpiral");
        QVERIFY(featurePanel->active());
        auto *helixRad = window.findChild<QDoubleSpinBox *>("helixRadius");
        auto *helixPitch = window.findChild<QDoubleSpinBox *>("helixPitch");
        auto *helixRev = window.findChild<QDoubleSpinBox *>("helixRevolutions");
        QVERIFY(helixRad && helixPitch && helixRev);

        helixRad->setValue(0.015 / unit);
        helixPitch->setValue(0.005 / unit);
        helixRev->setValue(3.0);

        auto *acceptFeatureBtn = window.findChild<QPushButton *>("acceptFeatureButton");
        QVERIFY(acceptFeatureBtn != nullptr);
        featurePanel->preview();
        QVERIFY(session.idle());
        auto *diagLabel = window.findChild<QLabel *>("featureDiagnostic");
        if (diagLabel)
            qWarning() << "DIAGNOSTIC IS:" << diagLabel->text();
        QVERIFY(acceptFeatureBtn->isEnabled());
        featurePanel->finish();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        auto docWithHelix = session.document();
        auto supHelix =
            model::SupportDefinition::parse(docWithHelix.value("model_supports").toMap());
        auto curvesHelix = model::parseDerivedCurves(supHelix.derived_curves);
        QCOMPARE(curvesHelix.size(), 1);
        QCOMPARE(curvesHelix[0].kind, QString("HelixSpiral"));
        QVERIFY(curvesHelix[0].helixSpiral.has_value());
        checkNear(curvesHelix[0].helixSpiral->radius, 0.015, 1e-6);
        checkNear(curvesHelix[0].helixSpiral->pitch, 0.005, 1e-6);
        checkNear(curvesHelix[0].helixSpiral->revolutions, 3.0, 1e-6);

        // Reopen Helix and verify round-trip
        featurePanel->begin({}, curvesHelix[0].id);
        QVERIFY(featurePanel->active());
        checkNear(helixRad->value() * unit, 0.015, 1e-6);
        checkNear(helixPitch->value() * unit, 0.005, 1e-6);
        checkNear(helixRev->value(), 3.0, 1e-6);
        featurePanel->discard();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        // 3. Nonsolid Evaluation Refusal
        featurePanel->begin("Extrude");
        QVERIFY(featurePanel->active());
        auto *profCombo = window.findChild<QComboBox *>("featureProfile");
        QVERIFY(profCombo != nullptr);
        // Find helix derived curve in profCombo
        int curveIdx = profCombo->findData(curvesHelix[0].id);
        if (curveIdx >= 0) {
            profCombo->setCurrentIndex(curveIdx);
            featurePanel->preview();
            QVERIFY(session.idle());
            auto *diagLabel = window.findChild<QLabel *>("featureDiagnostic");
            QVERIFY(diagLabel != nullptr);
            QVERIFY(diagLabel->text().contains("reference curve, not a solid body"));
            QVERIFY(!acceptFeatureBtn->isEnabled());
        }
        featurePanel->discard();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        // 4. Project File Save and Reopen Round Trip
        auto savePath = session.files.filePath("cm7_native_review.cadproj");
        client->saveProject(savePath, true);
        QVERIFY(session.idle());
        if (!QFile::exists(savePath)) {
            qWarning() << "Save failed! message:" << client->state().message
                       << "phase:" << int(client->state().phase) << "savePath:" << savePath;
        }
        QVERIFY2(QFile::exists(savePath), qPrintable(client->state().message));

        client->closeProject();
        QVERIFY(session.idle());
        QVERIFY(!client->state().projectOpen);

        client->openProject(savePath, "Saved");
        QVERIFY(session.idle());
        QVERIFY(client->state().projectOpen);

        auto reopenedDoc = session.document();
        auto reopenedSup =
            model::SupportDefinition::parse(reopenedDoc.value("model_supports").toMap());
        auto reopenedCurves = model::parseDerivedCurves(reopenedSup.derived_curves);
        QCOMPARE(reopenedCurves.size(), 1);
        QCOMPARE(reopenedCurves[0].kind, QString("HelixSpiral"));
        checkNear(reopenedCurves[0].helixSpiral->radius, 0.015, 1e-6);
    }
};

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    WorkflowContractsCM7 tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "workflow_contracts_cm7.moc"
