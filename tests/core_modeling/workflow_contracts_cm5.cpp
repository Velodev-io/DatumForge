#include "../support/modal.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "ModelingFixture.hpp"
#include "SketchPanel.hpp"
#include "SupportEditor.hpp"
#include "features/ModelingIntent.hpp"
#include "model/SupportEditing.hpp"
#include "render/Viewport.hpp"
#include "sketch/Curves.hpp"
#include <QDialogButtonBox>
#include <QFile>
#include <QMouseEvent>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTimer>
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

inline void checkNear(double actual, double expected, double tolerance) {
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
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            bool ok = false;
            double expected = qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble(&ok);
            if (!ok || std::abs(window->devicePixelRatioF() - expected) > .01)
                return false;
        }
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
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST"))
            QVERIFY(QGuiApplication::platformName() == "cocoa");
    }

    void savedParametersAndPointerRegression() {
        Client client;
        sketch::Sketch sketch;
        sketch.rectangle({0, 0}, {.03, .02});
        auto intent = modeling_fixture::extrude(sketch.id);
        auto direction = modeling_fixture::blind();
        direction.remove(QStringLiteral("value"));
        direction.insert(QStringLiteral("mode"), "OffsetFromSurface");
        direction.insert(QStringLiteral("reference"),
                         support_fixture::ref(sketch.id, "Plane").map());
        direction.insert(QStringLiteral("offset"), .003);
        direction.insert(QStringLiteral("offset_reverse"), true);
        intent.insert(QStringLiteral("direction1"), direction);
        auto doc =
            sketch::fields({{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                            {"units", "mm"},
                            {"sketches", sketch::encode({sketch})},
                            {"features", QCborArray{}},
                            {"model_supports", modeling_fixture::base({sketch}).map()},
                            {"modeling", sketch::fields({{"intents", QCborArray{intent}}})}});
        FeaturePanel panel(&client);
        panel.install(doc, true);
        panel.begin({}, intent.value("id").toString());
        QCOMPARE(panel.findChild<QDoubleSpinBox *>("featureSurfaceOffset")->value(), 3.0);
        QVERIFY(panel.findChild<QCheckBox *>("featureOffsetReverse")->isChecked());
        panel.discard();
        auto largeOffset = intent;
        auto largeDirection = direction;
        largeDirection.insert(QStringLiteral("offset"), 3.0);
        largeOffset.insert(QStringLiteral("direction1"), largeDirection);
        doc.insert(QStringLiteral("modeling"),
                   sketch::fields({{"intents", QCborArray{largeOffset}}}));
        panel.install(doc, true);
        panel.begin({}, largeOffset.value("id").toString());
        QCOMPARE(panel.findChild<QDoubleSpinBox *>("featureSurfaceOffset")->value(), 3000.0);
        panel.discard();
        auto second = direction;
        second.insert(QStringLiteral("offset"), .005);
        second.insert(QStringLiteral("offset_reverse"), false);
        intent.insert(QStringLiteral("direction2"), second);
        doc.insert(QStringLiteral("modeling"), sketch::fields({{"intents", QCborArray{intent}}}));
        panel.install(doc, true);
        panel.begin({}, intent.value("id").toString());
        QCOMPARE(panel.findChild<QDoubleSpinBox *>("featureSurfaceOffset2")->value(), 5.0);
        QVERIFY(!panel.findChild<QCheckBox *>("featureOffsetReverse2")->isChecked());
        panel.discard();
        auto revolved = intent;
        revolved.insert(QStringLiteral("kind"), "Revolve");
        revolved.insert(QStringLiteral("direction1"),
                        sketch::fields({{"mode", "Midplane"}, {"value", std::numbers::pi}}));
        revolved.insert(QStringLiteral("direction2"), QCborValue(nullptr));
        auto axis = support_fixture::ref(sketch.id, "Curve", sketch.entities[0].id);
        revolved.insert(QStringLiteral("axis"), axis.map());
        doc.insert(QStringLiteral("modeling"), sketch::fields({{"intents", QCborArray{revolved}}}));
        panel.install(doc, true);
        panel.begin({}, revolved.value("id").toString());
        QCOMPARE(panel.findChild<QComboBox *>("featureRevolveExtent")->currentText(),
                 QString("Midplane"));
        panel.discard();
        auto owner = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto wanted = support_fixture::ref(owner, "Face");
        wanted.topology = "face/" + QString(64, 'b');
        auto first = wanted;
        first.topology = "face/" + QString(64, 'a');
        intent.insert(QStringLiteral("start"),
                      sketch::fields({{"mode", "Surface"}, {"reference", wanted.map()}}));
        doc.insert(QStringLiteral("modeling"), sketch::fields({{"intents", QCborArray{intent}}}));
        panel.install(doc, true);
        panel.begin({}, intent.value("id").toString());
        client.modelingInputs(sketch::fields({{"bodies", QCborArray{}},
                                              {"references", QCborArray{first.map(), wanted.map()}},
                                              {"sketches", sketch::encode({sketch})}}));
        QCOMPARE(
            panel.findChild<QComboBox *>("featureStartReference")->currentData().value<QCborMap>(),
            wanted.map());
        panel.discard();
        Viewport viewport;
        viewport.resize(640, 480);
        viewport.setDraftManipulator({}, {0, 0, 1}, 10);
        QSignalSpy moved(&viewport, &Viewport::draftManipulatorMoved);
        auto start = viewport.project({});
        QMouseEvent press(QEvent::MouseButtonPress, start, start, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(&viewport, &press);
        auto end = start + QPointF(10, 0);
        for (int i = 0; i < 2; ++i) {
            QMouseEvent move(QEvent::MouseMove, end, end, Qt::NoButton, Qt::LeftButton,
                             Qt::NoModifier);
            QApplication::sendEvent(&viewport, &move);
        }
        QCOMPARE(moved.size(), 2);
        QCOMPARE(moved[0][0].toDouble(), moved[1][0].toDouble());
        viewport.setDraftManipulator({}, {0, 1, 0}, 180, 360);
        viewport.dragDraftAngleHandle(10);
        QCOMPARE(moved.last()[0].toDouble(), 190.0);
    }

    void typedInputsAndParameterHandles() {
        SessionCM5 s;
        if (!s.start() || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
        sketch::Sketch profile;
        profile.rectangle({0, 0}, {.02, .02});
        s.client->projectCommand("SetSketches",
                                 sketch::fields({{"sketches", sketch::encode({profile})}}));
        QVERIFY(s.idle());
        auto *panel = s.window->findChild<FeaturePanel *>();
        s.window->command("extrude")->trigger();
        QVERIFY(s.idle());
        QVERIFY(panel->isModelingActive());
        auto ready = [&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        };
        panel->preview();
        QVERIFY2(wait(ready),
                 qPrintable(s.client->state().message + " / " +
                            s.window->findChild<QLabel *>("featureDiagnostic")->text()));
        panel->finish();
        QVERIFY(s.idle());
        QVERIFY(!s.window->command("fillet")->isEnabled());
        panel->begin("Fillet");
        QVERIFY(!panel->active());
        auto revision = s.client->state().revision;
        panel->begin("Extrude");
        QVERIFY(s.idle());
        s.window->findChild<QCheckBox *>("featureDirection2Enable")->setChecked(true);
        s.window->findChild<QDoubleSpinBox *>("featureDepth2")->setValue(20);
        auto *handles = s.window->findChild<QComboBox *>("featureHandleParameter");
        handles->setCurrentText("Direction 2 depth");
        s.window->sceneView()->dragExtrudeDepthHandle(5);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth2")->value(), 25.0);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth")->value(), 10.0);
        QVERIFY(s.idle());
        panel->discard();
        QVERIFY(s.idle());
        panel->begin("Extrude");
        QVERIFY(s.idle());
        s.window->findChild<QComboBox *>("featureThinMode")->setCurrentText("TwoDirection");
        s.window->findChild<QDoubleSpinBox *>("featureThickness2")->setValue(2);
        handles->setCurrentText("Thickness 2");
        s.window->sceneView()->dragExtrudeDepthHandle(1);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureThickness2")->value(), 3.0);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureThickness1")->value(), 1.0);
        QVERIFY(s.idle());
        panel->discard();
        QVERIFY(s.idle());
        panel->begin("Extrude");
        QVERIFY(s.idle());
        s.window->findChild<QComboBox *>("featureStartMode")->setCurrentText("Vertex");
        auto *start = s.window->findChild<QComboBox *>("featureStartReference");
        for (int i = 0; i < start->count(); ++i)
            if (start->itemData(i).value<QCborMap>().value("role") == "Vertex") {
                start->setCurrentIndex(i);
                break;
            }
        QCOMPARE(start->currentData().value<QCborMap>().value("role").toString(),
                 QString("Vertex"));
        panel->preview();
        QVERIFY2(wait(ready),
                 qPrintable(s.client->state().message + " / " +
                            s.window->findChild<QLabel *>("featureDiagnostic")->text()));
        panel->discard();
        QVERIFY(s.idle());
        panel->begin("Extrude");
        QVERIFY(s.idle());
        s.window->findChild<QComboBox *>("featureStartMode")->setCurrentText("Offset");
        s.window->findChild<QDoubleSpinBox *>("featureStartOffset")->setValue(-5);
        s.window->findChild<QComboBox *>("featureExtent")->setCurrentText("UpToBody");
        auto *end = s.window->findChild<QComboBox *>("featureDir1Reference");
        QCOMPARE(end->currentData().value<QCborMap>().value("role").toString(), QString("Object"));
        panel->preview();
        QVERIFY2(wait(ready),
                 qPrintable(s.client->state().message + " / " +
                            s.window->findChild<QLabel *>("featureDiagnostic")->text()));
        panel->discard();
        QVERIFY(s.idle());
        QCOMPARE(s.client->state().revision, revision);
    }

    void asymmetricDraftRectangleFixture() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
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
        QVERIFY(s.idle());
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

        s.window->findChild<QComboBox *>("featureHandleParameter")
            ->setCurrentText("Direction 1 draft");
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
        checkNear(f1.value("direction1").toMap().value("draft").toMap().value("angle").toDouble(),
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
        checkNear(s.volume(), expectedVol, 1e-7);

        // Tree integration: double-click intent to edit
        auto *tree = s.window->findChild<QTreeWidget *>("modelTree");
        QVERIFY(tree);
        auto intentId = f1.value("id").toString();
        panel->begin({}, intentId);
        QVERIFY(s.idle());
        QVERIFY(panel->active());
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth")->value(), 10.0);
        QCOMPARE(s.window->findChild<QDoubleSpinBox *>("featureDepth2")->value(), 15.0);
        panel->discard();
        QVERIFY(!panel->active());
    }

    void thinExtrusionRectangleFixture() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
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
        QVERIFY(s.idle());
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
        checkNear(s.volume(), expectedVol, 1e-8);
    }

    void t6SelectedContoursAndReuse() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
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
        QVERIFY(s.idle());
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
        QVERIFY(s.idle());
        profile->setCurrentIndex(profile->findData(multi.id));
        s.window->findChild<QComboBox *>("featureOperation")->setCurrentText("Add");
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(20);

        s.window->findChild<QComboBox *>("featureProfileMode")->setCurrentText("SelectedRegions");
        bool selectedThroughDialog = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *curves = dialog->findChild<QComboBox *>("featureContourCurve");
            dialog->findChild<QPushButton *>("featureContourAddRegion")->click();
            for (int i = 0; i < curves->count(); ++i)
                if (curves->itemText(i).startsWith("Circle"))
                    curves->setCurrentIndex(i);
            dialog->findChild<QPushButton *>("featureContourAddCurve")->click();
            selectedThroughDialog = true;
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        s.window->findChild<QPushButton *>("featureSelectRegions")->click();
        QVERIFY(selectedThroughDialog);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());

        auto doc = s.document();
        auto intents = doc.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(intents.size(), 2);
        QCOMPARE(intents[1].toMap().value("profile").toMap().value("mode").toString(),
                 QString("SelectedRegions"));
        QCOMPARE(intents[1].toMap().value("profile").toMap().value("regions").toArray().size(), 1);
        checkNear(s.volume(),
                  (.04 * .03 - std::numbers::pi * .005 * .005) * .01 +
                      std::numbers::pi * .005 * .005 * .02,
                  1e-10);
        // A third Boolean must name the stage after the second operation.
        panel->begin("Extrude");
        QVERIFY(s.idle());
        profile->setCurrentIndex(profile->findData(multi.id));
        s.window->findChild<QComboBox *>("featureOperation")->setCurrentText("Add");
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(25);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());
        auto third = s.document().value("modeling").toMap().value("intents").toArray()[2].toMap();
        QCOMPARE(third.value("target").toMap().value("source").toMap().value("object"),
                 intents[1].toMap().value("id"));
        // Sketch remains intact in document
        QCOMPARE(doc.value("sketches").toArray().size(), 1);
    }

    void t6OpenProfileCut() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }

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
        QVERIFY(s.idle());
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
        checkNear(s.volume(), totalBlockVol, 1e-8);

        // 2. Open-profile cut with flip_side = false cutting into block (-Z)
        panel->begin("Extrude");
        QVERIFY(s.idle());
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
        checkNear(volAfterCut1, totalBlockVol - 0.04 * 0.02 * 0.01, 1e-7);

        // Edit cut feature to flip cut side
        auto cutIntents = s.document().value("modeling").toMap().value("intents").toArray();
        QCOMPARE(cutIntents.size(), 2);
        panel->begin({}, cutIntents[1].toMap().value("id").toString());
        QVERIFY(s.idle());
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
        checkNear(s.volume(), totalBlockVol - 0.04 * 0.02 * 0.01, 1e-7);
    }

    void revolveModesAndAngles() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
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
        QVERIFY(s.idle());
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
        checkNear(s.volume(), expectedVol, 1e-8);
    }

    void t8StandaloneDraftAndRepair() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
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
        QVERIFY(s.idle());
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
        QVERIFY(s.idle());
        QVERIFY(panel->active());
        s.window->findChild<QDoubleSpinBox *>("draftStandaloneAngle")->setValue(5);
        s.window->findChild<QCheckBox *>("draftStandaloneOutward")->setChecked(true);
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

        // A missing face must be repaired through the actual pending native draft.
        auto acceptedRevision = s.client->state().revision;
        auto originalFace = intents[1].toMap().value("faces").toArray()[0].toMap();
        panel->begin({}, intents[1].toMap().value("id").toString());
        QVERIFY(s.idle());
        auto missing = originalFace;
        missing.insert(QStringLiteral("topology"), "face/" + QString(64, '0'));
        facesCombo->addItem("Missing reference", QVariant::fromValue(missing));
        facesCombo->setCurrentIndex(facesCombo->count() - 1);
        panel->preview();
        QVERIFY(s.idle());
        QCOMPARE(s.client->state().phase, Phase::Failed);
        QCOMPARE(s.client->state().revision, acceptedRevision);
        s.window->findChild<QComboBox *>("featureRepairPath")->setCurrentText("faces/0");
        s.window->findChild<QPushButton *>("featureRepairSupports")->click();
        QVERIFY(s.idle());
        auto *repairCombo = s.window->findChild<QComboBox *>("featureRepairCandidate");
        int replacement = -1;
        for (int i = 0; i < repairCombo->count(); ++i)
            if (repairCombo->itemData(i).value<QCborMap>().value("topology") ==
                originalFace.value("topology"))
                replacement = i;
        QVERIFY(replacement >= 0);
        repairCombo->setCurrentIndex(replacement);
        s.window->findChild<QPushButton *>("featureApplyRepair")->click();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        // The original face is restored exactly; change angle to create a real revision.
        s.window->findChild<QDoubleSpinBox *>("draftStandaloneAngle")->setValue(6);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());
        QVERIFY(!panel->active());
        QCOMPARE(s.client->state().revision, acceptedRevision + 1);
        auto repaired =
            s.document().value("modeling").toMap().value("intents").toArray()[1].toMap();
        QCOMPARE(repaired.value("faces").toArray()[0].toMap(), originalFace);
        auto saved = s.files.filePath("repaired.cadproj");
        s.client->saveProject(saved, true);
        QVERIFY(s.idle());
        s.client->closeProject();
        QVERIFY(s.idle());
        s.client->openProject(saved);
        QVERIFY(s.idle());
        QCOMPARE(s.document().value("modeling").toMap().value("intents").toArray()[1].toMap(),
                 repaired);
    }

    void draftLifecycleAndCancel() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
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
        QVERIFY(s.idle());
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
        double actual = qEnvironmentVariableIsSet("CAD_NATIVE_TEST")
                            ? qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()
                            : 1.0;
        QTest::newRow("mm") << actual << QString("mm") << false;
        QTest::newRow("inch-inclined") << actual << QString("in") << true;
    }

    void nativeEffectiveScales() {
        QFETCH(double, dpr);
        QFETCH(QString, units);
        QFETCH(bool, inclinedPlane);
        SessionCM5 s;
        if (!s.start(units, dpr) || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST"))
            QCOMPARE(s.window->devicePixelRatioF(), dpr);
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
        QVERIFY(s.idle());
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

        checkNear(s.volume(), 0.02 * 0.02 * 0.01, 1e-8);
        auto artifacts = qEnvironmentVariable("CAD_ARTIFACT_DIR");
        if (!artifacts.isEmpty()) {
            QVERIFY(wait([&] {
                return s.window->sceneView()->graphicsDescription().contains("initialized 1");
            }));
            QVERIFY(s.window->grab().save(artifacts + "/" + units + ".png"));
        }
    }

    void upstreamMutationAndReopen() {
        SessionCM5 s;
        if (!s.start("mm") || !s.client) {
            QFAIL("Disposable worker session failed to start");
            return;
        }
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
        QVERIFY(s.idle());
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

        checkNear(s.volume(), 0.03 * 0.02 * 0.01, 1e-8);

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
        checkNear(s.volume(), 0.04 * 0.02 * 0.01, 1e-8);

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
        checkNear(s.volume(), 0.04 * 0.02 * 0.01, 1e-8);
    }
};

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    WorkflowContractsCM5 tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "workflow_contracts_cm5.moc"
