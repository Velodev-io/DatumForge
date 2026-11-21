#include "../support/modal.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "ModelingFixture.hpp"
#include "SketchPanel.hpp"
#include "features/ModelingIntent.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/Loft.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/Shell.hpp"
#include "geometry/occt/Sweep.hpp"
#include "model/SupportEditing.hpp"
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
    while (!ready() && timer.elapsed() < 20000)
        QTest::qWait(5);
    return ready();
}

struct NativeSession {
    QTemporaryDir root{QDir::tempPath() + "/cad-module8-review-native-XXXXXX"};
    QTemporaryDir files{QDir::tempPath() + "/cad-module8-review-native-files-XXXXXX"};
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
            if (!QTest::qWaitForWindowExposed(window.get()))
                return false;
        }
        client = window->workerClient();
        if (!waitFor([&] { return client->ready(); }))
            return false;
        client->newProject("Part", "CM8 native review", units);
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

QCborMap makeExtrude(QString sketch, double depth = 0.05) {
    auto f = modeling_fixture::extrude(sketch, "NewBody");
    f.insert(QStringLiteral("direction1"), blind(depth));
    return f;
}
} // namespace

class WorkflowContractsCM8 : public QObject {
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
        auto *featurePanel = window.findChild<FeaturePanel *>();
        QVERIFY(featurePanel != nullptr);

        // 1. Initial Solid Setup: 100x100x50 mm box
        sketch::Sketch sBase;
        sBase.name = "BaseBoxSketch";
        sBase.rectangle({0, 0}, {0.100, 0.100});
        auto baseExt = makeExtrude(sBase.id, 0.050);
        auto baseId = baseExt.value("id").toString();

        auto modelWithBase = model::editSupportedSketches(
            session.document(), sketch::encode(std::vector<sketch::Sketch>{sBase}));
        client->projectCommand("SetModelSupports", fields({{"model", modelWithBase.map()},
                                                           {"intents", QCborArray{baseExt}}}));
        QVERIFY(session.idle());

        auto docAfterBase = session.document();
        QVERIFY(docAfterBase.contains(QStringLiteral("modeling")));
        auto baseIntents = docAfterBase.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(baseIntents.size(), 1);

        // 2. Shell Workflow
        featurePanel->begin("Shell");
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());

        auto *shellThick = window.findChild<QDoubleSpinBox *>("shellThickness");
        auto *shellIn = window.findChild<QCheckBox *>("shellInward");
        auto *shellFaces = window.findChild<QListWidget *>("shellRemoveFaces");
        auto *shellOver = window.findChild<QTableWidget *>("shellOverrides");
        auto *addOverrideBtn = window.findChild<QPushButton *>("addShellOverride");
        QVERIFY(shellThick != nullptr);
        QVERIFY(shellIn != nullptr);
        QVERIFY(shellFaces != nullptr);
        QVERIFY(shellOver != nullptr);
        QVERIFY(addOverrideBtn != nullptr);

        shellThick->setValue(0.005 / unit);
        shellIn->setChecked(true);

        // Select the first face in shellFaces to remove
        if (shellFaces->count() > 0) {
            shellFaces->item(0)->setCheckState(Qt::Checked);
        }

        auto *acceptFeatureBtn = window.findChild<QPushButton *>("acceptFeatureButton");
        auto *diagLabel = window.findChild<QLabel *>("featureDiagnostic");
        QVERIFY(acceptFeatureBtn != nullptr);
        featurePanel->preview();
        QVERIFY(session.idle());
        if (!acceptFeatureBtn->isEnabled()) {
            qWarning() << "Shell Preview failed! Diag:" << (diagLabel ? diagLabel->text() : "")
                       << "Client error:" << client->state().message;
        }
        QVERIFY(acceptFeatureBtn->isEnabled());
        featurePanel->finish();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        auto docAfterShell = session.document();
        auto shellIntents = docAfterShell.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(shellIntents.size(), 2);
        auto shellObj = shellIntents[1].toMap();
        QCOMPARE(shellObj.value("kind").toString(), QString("Shell"));
        checkNear(shellObj.value("thickness").toDouble(), 0.005, 1e-6);
        QCOMPARE(shellObj.value("inward").toBool(), true);
        auto shellId = shellObj.value("id").toString();

        // Re-edit Shell intent and verify round-trip
        featurePanel->begin({}, shellId);
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());
        checkNear(shellThick->value() * unit, 0.005, 1e-4);
        QCOMPARE(shellIn->isChecked(), true);
        featurePanel->discard();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        // 3. Circular Sweep Workflow
        // Add a path sketch (line from (0.200, 0, 0) to (0.200, 0, 0.100))
        sketch::Sketch sPath;
        sPath.name = "SweepPath";
        sPath.frame.origin = {0.200, 0, 0};
        sPath.frame.u = {0, 0, 1};
        sPath.frame.v = {0, 1, 0};
        auto pt1 = sPath.point({0, 0});
        auto pt2 = sPath.point({0.100, 0});
        sPath.line(pt1, pt2);

        auto docBeforeSweep = session.document();
        auto sketchesSweep = sketch::parse(docBeforeSweep.value("sketches").toArray());
        sketchesSweep.push_back(sPath);
        auto modelWithSweepPath =
            model::editSupportedSketches(docBeforeSweep, sketch::encode(sketchesSweep));
        client->projectCommand("SetModelSupports", fields({{"model", modelWithSweepPath.map()},
                                                           {"intents", shellIntents}}));
        QVERIFY(session.idle());

        featurePanel->begin("Sweep");
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());

        auto *sweepType = window.findChild<QComboBox *>("sweepProfileType");
        auto *sweepDia = window.findChild<QDoubleSpinBox *>("sweepDiameter");
        auto *sweepPathCombo = window.findChild<QComboBox *>("sweepPath");
        auto *sweepOp = window.findChild<QComboBox *>("featureOperation");
        QVERIFY(sweepType != nullptr);
        QVERIFY(sweepDia != nullptr);
        QVERIFY(sweepPathCombo != nullptr);
        QVERIFY(sweepOp != nullptr);

        sweepType->setCurrentText("Circular");
        sweepDia->setValue(0.010 / unit);
        sweepOp->setCurrentText("NewBody");

        // Set path to sPath
        for (int i = 0; i < sweepPathCombo->count(); ++i) {
            if (sweepPathCombo->itemText(i).contains("SweepPath")) {
                sweepPathCombo->setCurrentIndex(i);
                break;
            }
        }

        featurePanel->preview();
        QVERIFY(session.idle());
        QVERIFY(acceptFeatureBtn->isEnabled());
        featurePanel->finish();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        auto docAfterSweep = session.document();
        auto sweepIntents = docAfterSweep.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(sweepIntents.size(), 3);
        auto sweepObj = sweepIntents[2].toMap();
        QCOMPARE(sweepObj.value("kind").toString(), QString("Sweep"));
        auto sweepId = sweepObj.value("id").toString();

        // Re-edit Sweep intent
        featurePanel->begin({}, sweepId);
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());
        QCOMPARE(sweepType->currentText(), QString("Circular"));
        checkNear(sweepDia->value() * unit, 0.010, 1e-4);
        featurePanel->discard();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        // 4. Multisection Loft Workflow
        sketch::Sketch sLoft1;
        sLoft1.name = "LoftSec1";
        sLoft1.frame.origin = {0.350, 0, 0};
        sLoft1.rectangle({0, 0}, {0.040, 0.040});

        sketch::Sketch sLoft2;
        sLoft2.name = "LoftSec2";
        sLoft2.frame.origin = {0.350, 0, 0.060};
        sLoft2.rectangle({0, 0}, {0.020, 0.020});

        auto docBeforeLoft = session.document();
        auto sketchesLoft = sketch::parse(docBeforeLoft.value("sketches").toArray());
        sketchesLoft.push_back(sLoft1);
        sketchesLoft.push_back(sLoft2);
        auto modelWithLoft =
            model::editSupportedSketches(docBeforeLoft, sketch::encode(sketchesLoft));
        client->projectCommand("SetModelSupports",
                               fields({{"model", modelWithLoft.map()}, {"intents", sweepIntents}}));
        QVERIFY(session.idle());

        featurePanel->begin("Loft");
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());

        auto *loftOut = window.findChild<QComboBox *>("loftOutput");
        auto *loftSecs = window.findChild<QListWidget *>("loftSections");
        auto *addSecBtn = window.findChild<QPushButton *>("addLoftSection");
        auto *profCombo = window.findChild<QComboBox *>("featureProfile");
        auto *loftOp = window.findChild<QComboBox *>("featureOperation");
        QVERIFY(loftOut != nullptr);
        QVERIFY(loftSecs != nullptr);
        QVERIFY(addSecBtn != nullptr);
        QVERIFY(profCombo != nullptr);

        loftOut->setCurrentText("Solid");
        loftOp->setCurrentText("NewBody");

        // Add sLoft1
        for (int i = 0; i < profCombo->count(); ++i) {
            if (profCombo->itemText(i).contains("LoftSec1")) {
                profCombo->setCurrentIndex(i);
                addSecBtn->click();
                break;
            }
        }
        // Add sLoft2
        for (int i = 0; i < profCombo->count(); ++i) {
            if (profCombo->itemText(i).contains("LoftSec2")) {
                profCombo->setCurrentIndex(i);
                addSecBtn->click();
                break;
            }
        }
        QCOMPARE(loftSecs->count(), 2);

        featurePanel->preview();
        QVERIFY(session.idle());
        QVERIFY(acceptFeatureBtn->isEnabled());
        featurePanel->finish();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        auto docAfterLoft = session.document();
        auto loftIntents = docAfterLoft.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(loftIntents.size(), 4);
        auto loftObj = loftIntents[3].toMap();
        QCOMPARE(loftObj.value("kind").toString(), QString("Loft"));
        auto loftId = loftObj.value("id").toString();

        // Re-edit Loft intent and add a connector correspondence handle
        featurePanel->begin({}, loftId);
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());
        QCOMPARE(loftOut->currentText(), QString("Solid"));
        QCOMPARE(loftSecs->count(), 2);

        auto *addConnBtn = window.findChild<QPushButton *>("addLoftConnector");
        auto *connTable = window.findChild<QTableWidget *>("loftConnectors");
        QVERIFY(addConnBtn != nullptr);
        QVERIFY(connTable != nullptr);
        QCOMPARE(connTable->rowCount(), 0);

        addConnBtn->click();
        QCOMPARE(connTable->rowCount(), 1);
        connTable->item(0, 1)->setText("0.1, 0.2");

        featurePanel->preview();
        QVERIFY(session.idle());
        QVERIFY(acceptFeatureBtn->isEnabled());
        featurePanel->finish();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        // Verify persisted loft has connector with points maps
        auto docWithConn = session.document();
        auto loftWithConn =
            docWithConn.value("modeling").toMap().value("intents").toArray()[3].toMap();
        auto connsArr = loftWithConn.value("connectors").toArray();
        QCOMPARE(connsArr.size(), 1);
        auto ptsArr = connsArr[0].toMap().value("points").toArray();
        QCOMPARE(ptsArr.size(), 2);
        QCOMPARE(ptsArr[0].toMap().value("section").toInteger(), 0);
        checkNear(ptsArr[0].toMap().value("parameter").toDouble(), 0.1, 1e-4);
        QCOMPARE(ptsArr[1].toMap().value("section").toInteger(), 1);
        checkNear(ptsArr[1].toMap().value("parameter").toDouble(), 0.2, 1e-4);

        // Re-open again to check UI loading of connector parameters
        featurePanel->begin({}, loftId);
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());
        QCOMPARE(connTable->rowCount(), 1);
        QVERIFY(connTable->item(0, 1)->text().contains("0.1"));
        featurePanel->discard();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        // 5. Project Save & Reopen Round-Trip
        auto savePath = session.files.filePath("cm8_native_review.cadproj");
        client->saveProject(savePath, true);
        QVERIFY(session.idle());
        QVERIFY2(QFile::exists(savePath), qPrintable(client->state().message));

        client->closeProject();
        QVERIFY(session.idle());
        QVERIFY(!client->state().projectOpen);

        client->openProject(savePath, "Saved");
        QVERIFY(session.idle());
        QVERIFY(client->state().projectOpen);

        auto reopenedDoc = session.document();
        QVERIFY(reopenedDoc.contains(QStringLiteral("modeling")));
        auto reopenedIntents = reopenedDoc.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(reopenedIntents.size(), 4);

        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            if (!directory.isEmpty())
                QVERIFY(window.grab().save(directory + "/cm8-" + units + ".png"));
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
    WorkflowContractsCM8 tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "workflow_contracts_cm8.moc"
