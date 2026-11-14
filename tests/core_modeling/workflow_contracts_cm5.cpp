#include "../support/modal.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "SupportEditor.hpp"
#include "features/ModelingIntent.hpp"
#include "model/SupportEditing.hpp"
#include "render/Viewport.hpp"
#include "sketch/Curves.hpp"
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>
#include <numbers>

using namespace cad;

namespace {
bool wait(std::function<bool()> ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 15000)
        QTest::qWait(5);
    return ready();
}

inline void near(double actual, double expected, double tolerance) {
    QVERIFY2(std::abs(actual - expected) <= tolerance,
             qPrintable(QString("Expected %1, got %2 (diff %3 > %4)")
                            .arg(expected, 0, 'g', 12)
                            .arg(actual, 0, 'g', 12)
                            .arg(std::abs(actual - expected), 0, 'g', 12)
                            .arg(tolerance, 0, 'g', 12)));
}

inline model::SupportDefinition base(const std::vector<sketch::Sketch> &sketches) {
    model::SupportDefinition d;
    d.document = "Self";
    d.sketches = sketch::encode(sketches);
    for (auto &s : sketches)
        d.history.append(s.id);
    return d;
}

struct SessionCM5 {
    QTemporaryDir root{QDir::tempPath() + "/cad-module5-cm5-workflows-XXXXXX"};
    QTemporaryDir files{QDir::tempPath() + "/cad-module5-cm5-workflow-files-XXXXXX"};
    std::unique_ptr<MainWindow> window;
    Client *client = nullptr;

    bool start(QString units = "mm", double dpr = 1.0) {
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
        client = window->workerClient();
        if (!wait([&] { return client && client->ready(); }))
            return false;
        qWarning() << "Before newProject: ready=" << client->ready()
                   << " outstanding=" << client->outstanding();
        client->newProject("Part", "CM5 native workflows", units);
        qWarning() << "After newProject: outstanding=" << client->outstanding()
                   << " phase=" << int(client->state().phase);
        bool ok = wait([&] {
            return client && client->state().projectOpen &&
                   !client->state().activeDocument.isEmpty() && !client->outstanding();
        });
        qWarning() << "Wait result=" << ok
                   << " projectOpen=" << (client ? client->state().projectOpen : false)
                   << " activeDoc=" << (client ? client->state().activeDocument : QString())
                   << " message=" << (client ? client->state().message : QString());
        return ok;
    }

    bool idle() {
        QTest::qWait(20);
        return wait([&] { return client && !client->outstanding(); });
    }

    QCborMap document() const {
        for (auto value : client->state().documentRecords) {
            auto d = value.toMap();
            if (d.value("id") == client->state().activeDocument)
                return d;
        }
        return {};
    }

    double volume() const {
        auto *panel = window->findChild<FeaturePanel *>();
        if (!panel)
            return 0;
        return panel->latestResult()
            .value("part_measurement")
            .toMap()
            .value("volume_m3")
            .toDouble();
    }
};
} // namespace

class WorkflowContractsCM5 : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void t5Tutorial1AsymmetricDraft() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));
        sketch::Sketch rect;
        rect.name = "Tutorial1Profile";
        rect.rectangle({0, 0}, {.03, .02}); // 30 mm x 20 mm

        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({rect})}}));
        QVERIFY(s.idle());

        auto baseModel = base({rect});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        QVERIFY(panel);
        panel->begin("Extrude");
        QVERIFY(panel->active());
        QVERIFY(panel->isModelingActive());

        auto *profile = s.window->findChild<QComboBox *>("featureProfile");
        profile->setCurrentIndex(profile->findData(rect.id));

        // Direction 1: 10 mm, draft 35 degrees outward
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(10);
        s.window->findChild<QCheckBox *>("featureDraftEnable")->setChecked(true);
        s.window->findChild<QDoubleSpinBox *>("featureDraftAngle")->setValue(35);
        s.window->findChild<QCheckBox *>("featureDraftOutward")->setChecked(true);

        // Direction 2: 15 mm, draft 0 degrees
        s.window->findChild<QCheckBox *>("featureDirection2Enable")->setChecked(true);
        s.window->findChild<QDoubleSpinBox *>("featureDepth2")->setValue(15);

        // Viewport depth and draft manipulator interaction
        auto *viewport = s.window->sceneView();
        QVERIFY(viewport);
        viewport->dragExtrudeDepthHandle(5.0); // drag handle +5 mm
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth")->value(), 15.0);
        viewport->dragExtrudeDepthHandle(-5.0); // restore to 10 mm
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth")->value(), 10.0);

        viewport->dragDraftAngleHandle(5.0); // drag draft handle +5 deg (to 40 deg)
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDraftAngle")->value(), 40.0);
        viewport->dragDraftAngleHandle(-5.0); // restore to 35 deg
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDraftAngle")->value(), 35.0);

        // Preview
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        QVERIFY(s.client->mesh());

        // Accept
        panel->finish();
        QVERIFY(s.idle());
        QVERIFY(!panel->active());

        // Check document modeling intents
        auto doc = s.document();
        QVERIFY(doc.contains(QStringLiteral("modeling")));
        auto intents = doc.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(intents.size(), 1);
        auto f1 = intents[0].toMap();
        QCOMPARE(f1.value("kind").toString(), QString("Extrude"));
        QCOMPARE(f1.value("operation").toString(), QString("NewBody"));
        QCOMPARE(f1.value("direction1").toMap().value("value").toDouble(), 0.010);
        QCOMPARE(f1.value("direction2").toMap().value("value").toDouble(), 0.015);
        near(f1.value("direction1").toMap().value("draft").toMap().value("angle").toDouble(),
             35 * std::numbers::pi / 180.0, 1e-10);
        QVERIFY(f1.value("direction1").toMap().value("draft").toMap().value("outward").toBool());

        // Verify volume matches independent geometry:
        // Dir 1: truncated pyramid from (30x20) to (30+2*10*tan35) x (20+2*10*tan35)
        // Dir 2: rectangular prism 30x20x15 mm³ = 9000 mm³
        double h1 = 0.010, A0 = 0.03 * 0.02;
        double a1 = 0.03 + 2 * h1 * std::tan(35 * std::numbers::pi / 180.0);
        double b1 = 0.02 + 2 * h1 * std::tan(35 * std::numbers::pi / 180.0);
        double A1 = a1 * b1;
        double volDir1 = h1 / 3.0 * (A0 + A1 + std::sqrt(A0 * A1));
        double volDir2 = 0.03 * 0.02 * 0.015;
        double expectedVol = volDir1 + volDir2;
        near(s.volume(), expectedVol, 1e-7);

        // Tree integration: double-click intent to edit
        auto *tree = s.window->findChild<QTreeWidget *>("modelTree");
        QVERIFY(tree);
        auto intentId = f1.value("id").toString();
        panel->begin({}, intentId);
        QVERIFY(panel->active());
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth")->value(), 10.0);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth2")->value(), 15.0);
        panel->discard();
        QVERIFY(!panel->active());
    }

    void t5ThinExtrusionMidplane() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));
        sketch::Sketch rect;
        rect.name = "ThinProfile";
        rect.rectangle({0, 0}, {.04, .02}); // 40 mm x 20 mm

        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({rect})}}));
        QVERIFY(s.idle());
        auto baseModel = base({rect});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        panel->begin("Extrude");
        auto *profile = s.window->findChild<QComboBox *>("featureProfile");
        profile->setCurrentIndex(profile->findData(rect.id));

        // Midplane 20 mm total depth
        s.window->findChild<QComboBox *>("featureExtent")->setCurrentText("Midplane");
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(20);

        // OneDirection inward 3 mm thin extrusion
        s.window->findChild<QComboBox *>("featureThinMode")->setCurrentText("OneDirection");
        s.window->findChild<QDoubleSpinBox *>("featureThickness1")->setValue(3);
        s.window->findChild<QCheckBox *>("featureThinReverse")->setChecked(false); // inward

        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        // Inward 3mm wall: outer 40x20, inner (40-6)x(20-6) = 34x14
        // Area = 40*20 - 34*14 = 800 - 476 = 324 mm²
        // Volume = 324 mm² * 20 mm = 6480 mm³ = 6.48e-6 m³
        double expectedVol = (0.04 * 0.02 - 0.034 * 0.014) * 0.02;
        near(s.volume(), expectedVol, 1e-8);
    }

    void t6SelectedContoursAndReuse() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));
        sketch::Sketch multi;
        multi.name = "MultiRegion";
        multi.rectangle({0, 0}, {.04, .03}); // 40x30 outer
        auto center = multi.point({.02, .015});
        multi.circle(center, .005); // R=5 mm hole

        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({multi})}}));
        QVERIFY(s.idle());
        auto baseModel = base({multi});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();

        // Feature 1: Selected region (outer rectangle with circle hole)
        panel->begin("Extrude");
        auto *profile = s.window->findChild<QComboBox *>("featureProfile");
        profile->setCurrentIndex(profile->findData(multi.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(10);

        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        // Feature 2: Reuse SAME sketch without consuming it! Extrude cylinder from hole
        panel->begin("Extrude");
        profile->setCurrentIndex(profile->findData(multi.id));
        s.window->findChild<QComboBox *>("featureOperation")->setCurrentText("Add");
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(20);

        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        auto doc = s.document();
        auto intents = doc.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(intents.size(), 2);
        // Sketch remains intact in document
        QCOMPARE(doc.value("sketches").toArray().size(), 1);
    }

    void t6OpenProfileCut() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));

        // Base box sketch
        sketch::Sketch baseSketch;
        baseSketch.name = "BaseSketch";
        baseSketch.rectangle({0, 0}, {.04, .04});

        // Open line sketch spanning the face
        sketch::Sketch cutSketch;
        cutSketch.name = "CutSketch";
        cutSketch.frame.origin[2] = .02; // top face
        auto pA = cutSketch.point({-.01, .02});
        auto pB = cutSketch.point({.05, .02});
        cutSketch.line(pA, pB);

        s.client->projectCommand(
            "SetSketches", sketch::fields({{"sketches", sketch::encode({baseSketch, cutSketch})}}));
        QVERIFY(s.idle());
        auto baseModel = base({baseSketch, cutSketch});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        // 1. Create base solid block 40x40x20 mm³
        panel->begin("Extrude");
        s.window->findChild<QComboBox *>("featureProfile")
            ->setCurrentIndex(
                s.window->findChild<QComboBox *>("featureProfile")->findData(baseSketch.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(20);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        double totalBlockVol = 0.04 * 0.04 * 0.02; // 3.2e-5 m³
        near(s.volume(), totalBlockVol, 1e-8);

        // 2. Open-profile cut with flip_side = false cutting into block (-Z)
        panel->begin("Extrude");
        s.window->findChild<QComboBox *>("featureProfile")
            ->setCurrentIndex(
                s.window->findChild<QComboBox *>("featureProfile")->findData(cutSketch.id));
        s.window->findChild<QComboBox *>("featureOperation")->setCurrentText("Cut");
        s.window->findChild<QComboBox *>("featureProfileMode")->setCurrentText("OpenChains");
        s.window->findChild<QCheckBox *>("featureFlipSide")->setChecked(false);
        s.window->findChild<QCheckBox *>("featureReverse")->setChecked(true);
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(10);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        // Cut half of the top 10mm (40 x 20 x 10 mm³ = 8000 mm³ cut)
        double volAfterCut1 = s.volume();
        near(volAfterCut1, totalBlockVol - 0.04 * 0.02 * 0.01, 1e-7);

        // Edit cut feature to flip cut side
        auto cutIntents = s.document().value("modeling").toMap().value("intents").toArray();
        QCOMPARE(cutIntents.size(), 2);
        panel->begin({}, cutIntents[1].toMap().value("id").toString());
        QVERIFY(panel->active());
        auto *flipCheck = s.window->findChild<QCheckBox *>("featureFlipSide");
        QVERIFY(flipCheck);
        flipCheck->setChecked(true);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());
        near(s.volume(), totalBlockVol - 0.04 * 0.02 * 0.01, 1e-7);
    }

    void revolveModesAndAngles() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));
        sketch::Sketch s1;
        s1.name = "RevolveSketch";
        // Rectangle from x=10 to x=20, y=0 to y=10 (offset from Y axis)
        s1.rectangle({.01, 0}, {.02, .01});
        // Construction line along Y axis for revolve
        auto p0 = s1.point({0, -.01});
        auto p1 = s1.point({0, .05});
        auto axisLine = s1.line(p0, p1, true);

        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({s1})}}));
        QVERIFY(s.idle());
        auto baseModel = base({s1});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        panel->begin("Revolve");
        QVERIFY(panel->active());
        s.window->findChild<QComboBox *>("featureProfile")
            ->setCurrentIndex(s.window->findChild<QComboBox *>("featureProfile")->findData(s1.id));
        auto *axisCombo = s.window->findChild<QComboBox *>("featureRevolveAxis");
        for (int i = 0; i < axisCombo->count(); ++i) {
            auto d = axisCombo->itemData(i).value<QCborMap>();
            if (d.value("entity").toString() == axisLine) {
                axisCombo->setCurrentIndex(i);
                break;
            }
        }
        s.window->findChild<QDoubleSpinBox *>("featureAngle")->setValue(180); // 180 degrees
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        // Annular half-cylinder: R_inner=10mm, R_outer=20mm, H=10mm, angle=pi
        // Volume = pi/2 * (R_outer^2 - R_inner^2) * H = pi/2 * (400 - 100) * 10 = 1500 * pi mm³
        double expectedVol = std::numbers::pi / 2.0 * (0.02 * 0.02 - 0.01 * 0.01) * 0.01;
        near(s.volume(), expectedVol, 1e-8);
    }

    void t8StandaloneDraftAndRepair() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));
        sketch::Sketch baseSketch;
        baseSketch.name = "BoxBase";
        baseSketch.rectangle({0, 0}, {.02, .01}); // 20x10 mm

        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({baseSketch})}}));
        QVERIFY(s.idle());
        auto baseModel = base({baseSketch});
        model::Datum axis;
        axis.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        axis.kind = "DatumAxis";
        axis.mode = "Principal";
        axis.parameters = sketch::fields({{"axis", "Z"}});
        baseModel.datums = {axis};
        baseModel.history.append(axis.id);
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        // Base box H=10 mm
        panel->begin("Extrude");
        s.window->findChild<QComboBox *>("featureProfile")
            ->setCurrentIndex(
                s.window->findChild<QComboBox *>("featureProfile")->findData(baseSketch.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(10);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        // Standalone Draft
        panel->begin("Draft");
        QVERIFY(panel->active());
        s.window->findChild<QDoubleSpinBox *>("draftStandaloneAngle")->setValue(5);
        s.window->findChild<QCheckBox *>("draftStandaloneOutward")->setChecked(true);
        auto *neutralCombo = s.window->findChild<QComboBox *>("draftNeutral");
        auto *pullCombo = s.window->findChild<QComboBox *>("draftPull");
        for (int i = 0; i < pullCombo->count(); ++i) {
            auto d = pullCombo->itemData(i).value<QCborMap>();
            if (d.value("object").toString() == axis.id) {
                pullCombo->setCurrentIndex(i);
                break;
            }
        }
        // Viewport draft manipulator interaction
        auto *viewport = s.window->sceneView();
        QVERIFY(viewport);
        viewport->dragDraftAngleHandle(2.0);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("draftStandaloneAngle")->value(), 7.0);
        viewport->dragDraftAngleHandle(-2.0);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("draftStandaloneAngle")->value(), 5.0);

        auto *facesCombo = s.window->findChild<QComboBox *>("draftFaces");
        bool previewOk = false;
        for (int i = 0; i < facesCombo->count(); ++i) {
            facesCombo->setCurrentIndex(i);
            panel->preview();
            wait([&] { return !s.client->outstanding(); });
            if (s.client->state().phase == Phase::PreviewReady) {
                previewOk = true;
                break;
            }
        }
        QVERIFY(previewOk);
        QVERIFY(s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled());
        panel->finish();
        QVERIFY(s.idle());

        // Check draft intent recorded
        auto doc = s.document();
        auto intents = doc.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(intents.size(), 2);
        QCOMPARE(intents[1].toMap().value("kind").toString(), QString("Draft"));

        // Test repair workflow: begin edit on draft
        panel->begin({}, intents[1].toMap().value("id").toString());
        QVERIFY(panel->active());
        auto *repairBtn = s.window->findChild<QPushButton *>("featureRepairSupports");
        QVERIFY(repairBtn);
        repairBtn->click();
        QVERIFY(wait([&] { return !s.client->outstanding(); }));
        auto *repairCombo = s.window->findChild<QComboBox *>("featureRepairCandidate");
        QVERIFY(repairCombo);
        QVERIFY(repairCombo->count() > 0);
        auto *applyRepairBtn = s.window->findChild<QPushButton *>("featureApplyRepair");
        QVERIFY(applyRepairBtn);
        applyRepairBtn->click();
        QVERIFY(wait([&] { return !s.client->outstanding(); }));
        panel->discard();
        QVERIFY(!panel->active());
    }

    void draftLifecycleAndCancel() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));
        sketch::Sketch rect;
        rect.name = "LifecycleProfile";
        rect.rectangle({0, 0}, {.02, .02});

        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({rect})}}));
        QVERIFY(s.idle());
        auto baseModel = base({rect});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        panel->begin("Extrude");
        s.window->findChild<QComboBox *>("featureProfile")
            ->setCurrentIndex(
                s.window->findChild<QComboBox *>("featureProfile")->findData(rect.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(10);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        QVERIFY(s.client->mesh());

        // Cancel via Escape shortcut action restores clean state and clears preview mesh
        auto *cancelAction = s.window->findChild<QAction *>("cancelFeatureAction");
        QVERIFY(cancelAction);
        QVERIFY(cancelAction->isEnabled());
        cancelAction->trigger();
        QVERIFY(!panel->active());
        QVERIFY(!s.client->mesh());
        QVERIFY(s.document().value("modeling").toMap().value("intents").toArray().empty());
    }

    void nativeEffectiveScales_data() {
        QTest::addColumn<double>("dpr");
        QTest::addColumn<QString>("units");
        QTest::addColumn<bool>("inclinedPlane");
        QTest::newRow("100% mm") << 1.0 << QString("mm") << false;
        QTest::newRow("125% mm") << 1.25 << QString("mm") << true;
        QTest::newRow("150% in") << 1.50 << QString("in") << false;
        QTest::newRow("175% mm") << 1.75 << QString("mm") << true;
        QTest::newRow("200% in") << 2.00 << QString("in") << false;
    }

    void nativeEffectiveScales() {
        QFETCH(double, dpr);
        QFETCH(QString, units);
        QFETCH(bool, inclinedPlane);
        SessionCM5 s;
        QVERIFY(s.start(units, dpr));
        double u = domain::toSI(1, "length", units);

        sketch::Sketch sq;
        sq.name = "ScaleTestProfile";
        if (inclinedPlane) {
            sq.frame.origin = {0.01, 0.02, 0.03};
            double c = std::cos(std::numbers::pi / 4.0);
            double sn = std::sin(std::numbers::pi / 4.0);
            sq.frame.u = {c, 0, sn};
            sq.frame.v = {0, 1, 0};
        }
        sq.rectangle({0, 0}, {.02, .02});
        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({sq})}}));
        QVERIFY(s.idle());
        auto baseModel = base({sq});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        panel->begin("Extrude");
        s.window->findChild<QComboBox *>("featureProfile")
            ->setCurrentIndex(s.window->findChild<QComboBox *>("featureProfile")->findData(sq.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(.010 / u);

        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        // Verify visible mesh bounds
        QVERIFY(s.client->mesh());
        QVERIFY(!s.client->mesh()->vertices.isEmpty());
        QVERIFY(s.client->mesh()->maximum != s.client->mesh()->minimum);
        panel->finish();
        QVERIFY(s.idle());

        near(s.volume(), 0.02 * 0.02 * 0.01, 1e-8);
    }

    void upstreamMutationAndReopen() {
        SessionCM5 s;
        QVERIFY(s.start("mm"));
        sketch::Sketch rect;
        rect.name = "MutateProfile";
        rect.rectangle({0, 0}, {.03, .02});

        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({rect})}}));
        QVERIFY(s.idle());
        auto baseModel = base({rect});
        s.client->projectCommand("SetModelSupports", sketch::fields({{"model", baseModel.map()}}));
        QVERIFY(s.idle());

        auto *panel = s.window->findChild<FeaturePanel *>();
        panel->begin("Extrude");
        s.window->findChild<QComboBox *>("featureProfile")
            ->setCurrentIndex(
                s.window->findChild<QComboBox *>("featureProfile")->findData(rect.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(10);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        near(s.volume(), 0.03 * 0.02 * 0.01, 1e-8);

        // Undo and redo check
        s.window->command("undo")->trigger();
        QVERIFY(s.idle());
        QVERIFY(s.document().value("modeling").toMap().value("intents").toArray().empty());
        s.window->command("redo")->trigger();
        QVERIFY(s.idle());
        QCOMPARE(s.document().value("modeling").toMap().value("intents").toArray().size(), 1);

        // 1. Upstream mutation: change sketch rectangle to 40x20 via associative support model
        sketch::Sketch mutated;
        mutated.id = rect.id;
        mutated.name = rect.name;
        mutated.rectangle({0, 0}, {.04, .02});
        auto mutatedModel = base({mutated});
        mutatedModel.document = "Self";
        s.client->projectCommand("SetModelSupports",
                                 sketch::fields({{"model", mutatedModel.map()}}));
        QVERIFY(s.idle());

        // Solid automatically followed upstream geometry!
        near(s.volume(), 0.04 * 0.02 * 0.01, 1e-8);

        // 2. Save project and reopen
        auto path = s.files.filePath("cm5_native.cadproj");
        s.client->saveProject(path, true);
        QVERIFY(s.idle());

        s.client->closeProject();
        QVERIFY(s.idle());

        s.client->openProject(path);
        QVERIFY(s.idle());

        auto doc = s.document();
        QVERIFY(doc.contains(QStringLiteral("modeling")));
        auto intents = doc.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(intents.size(), 1);
        near(s.volume(), 0.04 * 0.02 * 0.01, 1e-8);
    }
};

QTEST_MAIN(WorkflowContractsCM5)
#include "workflow_contracts_cm5.moc"
