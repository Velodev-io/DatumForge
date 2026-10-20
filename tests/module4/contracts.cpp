#include "FeaturePanel.hpp"
#include "Fixture.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/Viewport.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <numbers>
using namespace cad;
namespace {
bool wait(std::function<bool()> f) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 20000)
        QTest::qWait(5);
    return f();
}
void mark(QString root) {
    QFile f(root + "/.cad-test-root");
    if (!f.open(QIODevice::WriteOnly))
        qFatal("Marker failed");
    f.write("disposable\n");
}
QCborMap doc(Client *c) {
    for (auto v : c->state().documentRecords)
        if (v.toMap().value("id").toString() == c->state().activeDocument)
            return v.toMap();
    return {};
}
} // namespace
class Contracts : public QObject {
    Q_OBJECT
  private slots:
    void lifecycle() {
        exercise(false);
    }
    void nativeFeatures() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        exercise(true);
    }

  private:
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module4-native-XXXXXX"),
            files(QDir::tempPath() + "/cad-module4-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        auto prefs = files.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow w(prefs, nullptr, root.path());
        QSignalSpy graphics(w.sceneView(), &Viewport::graphicsFailed);
        w.show();
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            w.raise();
            w.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&w));
        }
        auto *c = w.workerClient();
        auto *sketch = w.findChild<SketchPanel *>();
        auto *panel = w.findChild<FeaturePanel *>();
        QVERIFY(panel);
        QVERIFY(wait([&] { return c->ready(); }));
        c->newProject("Part", "B1 features");
        QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
        auto click = [&](QString name) {
            auto *b = w.findChild<QPushButton *>(name);
            QVERIFY2(b, qPrintable(name));
            QVERIFY2(b->isEnabled(), qPrintable(name + " disabled: " + c->state().message));
            b->click();
        };
        auto spin = [&](QString name, double value) {
            auto *b = w.findChild<QDoubleSpinBox *>(name);
            QVERIFY2(b, qPrintable(name));
            b->setValue(value);
        };
        auto combo = [&](QString name, QString value) {
            auto *b = w.findChild<QComboBox *>(name);
            QVERIFY2(b, qPrintable(name));
            b->setCurrentText(value);
        };
        auto settle = [&] { return wait([&] { return !c->outstanding(); }); };
        auto coordinate = [&](double x, double y) {
            w.findChild<QLineEdit *>("sketchX")->setText(QString::number(x));
            w.findChild<QLineEdit *>("sketchY")->setText(QString::number(y));
            click("addSketchCoordinate");
        };
        // Both frozen B1 sketches are created through the actual sketch PropertyManager.
        for (auto r :
             {std::array<double, 4>{0, 0, 80, 50}, std::array<double, 4>{20, 20, 40, 40}}) {
            w.command("sketch")->trigger();
            QVERIFY(sketch->active());
            combo("sketchTool", "Rectangle");
            coordinate(r[0], r[1]);
            coordinate(r[2], r[3]);
            click("previewSketchButton");
            QVERIFY(settle());
            QVERIFY2(c->state().phase == Phase::PreviewReady, qPrintable(c->state().message));
            sketch->finish();
            QVERIFY(settle());
            QVERIFY(!sketch->active());
        }
        auto sketches = doc(c).value("sketches").toArray();
        QCOMPARE(sketches.size(), 2);
        auto preview = [&] {
            auto rev = c->state().revision;
            click("previewFeatureButton");
            QVERIFY(settle());
            QVERIFY2(c->state().phase == Phase::PreviewReady,
                     qPrintable(c->state().message + " / " +
                                w.findChild<QLabel *>("featureDiagnostic")->text()));
            QCOMPARE(c->state().revision, rev);
        };
        auto accept = [&] {
            auto rev = c->state().revision;
            click("acceptFeatureButton");
            QVERIFY(settle());
            QVERIFY2(!panel->active(), qPrintable(c->state().message));
            QCOMPARE(c->state().revision, rev + 1);
            QVERIFY(c->mesh());
        };
        w.command("extrude")->trigger();
        QVERIFY(panel->active());
        preview();
        auto rev = c->state().revision;
        auto previewMesh = c->mesh();
        panel->discard();
        QVERIFY(settle());
        QCOMPARE(c->state().revision, rev);
        QVERIFY(doc(c).value("features").toArray().empty());
        QVERIFY(!c->mesh());
        w.command("extrude")->trigger();
        preview();
        accept();
        QVERIFY(w.sceneView()->isVisible());
        QVERIFY(!sketch->isVisible());
        auto base = doc(c).value("features").toArray()[0].toMap().value("id").toString();
        auto volume = [&] {
            return panel->latestResult()
                .value("part_measurement")
                .toMap()
                .value("volume_m3")
                .toDouble();
        };
        QVERIFY(std::abs(volume() - .00004) < 1e-12);
        w.command("extrude")->trigger();
        w.findChild<QComboBox *>("featureProfile")->setCurrentIndex(1);
        combo("featureOperation", "Cut");
        preview();
        accept();
        QVERIFY(std::abs(volume() - .000036) < 1e-12);
        w.command("hole")->trigger();
        spin("featureOriginX", 10);
        spin("featureOriginY", 10);
        spin("featureOriginZ", 10);
        spin("featureSize", 4);
        preview();
        accept();
        auto hole = doc(c).value("features").toArray().last().toMap().value("id").toString();
        QString x;
        for (int i = 0; i < 3; ++i) {
            w.command("pattern")->trigger();
            combo("featureOperation", "Cut");
            auto *source = w.findChild<QComboBox *>("featureSource");
            source->setCurrentIndex(source->findData(i == 2 ? x : hole));
            spin("featureSpacing", i == 0 ? 50 : 30);
            spin("featureAxisX", i == 0 ? 1 : 0);
            spin("featureAxisY", i == 0 ? 0 : 1);
            preview();
            accept();
            if (i == 0)
                x = doc(c).value("features").toArray().last().toMap().value("id").toString();
        }
        QVERIFY(std::abs(volume() - (36000 - 160 * std::numbers::pi) * 1e-9) < 1e-12);
        auto edge = fixture4::filletEdge(panel->latestResult().value("topology").toArray());
        QVERIFY(!edge.isEmpty());
        auto xy = doc(c).value("features").toArray().last().toMap().value("id").toString();
        Q_UNUSED(xy);
        w.command("fillet")->trigger();
        c->select({"Edge", base + "/" + edge + "/segment/0", "part"});
        QVERIFY(!c->state().selection.empty());
        spin("featureSize", 2);
        preview();
        accept();
        QVERIFY(std::abs(volume() - 35488.76110196153e-9) < 1e-12);
        auto acceptedFeatures = doc(c).value("features").toArray();
        auto fillet = acceptedFeatures.last().toMap().value("id").toString();
        panel->begin({}, fillet);
        spin("featureSize", 1000);
        rev = c->state().revision;
        click("previewFeatureButton");
        QVERIFY(settle());
        QCOMPARE(c->state().phase, Phase::Failed);
        QCOMPARE(c->state().revision, rev);
        QCOMPARE(doc(c).value("features").toArray(), acceptedFeatures);
        QVERIFY(!w.findChild<QPushButton *>("acceptFeatureButton")->isEnabled());
        panel->discard();
        QVERIFY(settle());
        // Editing a real parameter and canceling preserves the accepted semantic revision.
        panel->begin({}, fillet);
        spin("featureSize", 1);
        preview();
        if (native) {
            w.raise();
            w.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&w));
            w.sceneView()->setFocus();
            QVERIFY(wait([&] { return w.sceneView()->hasFocus(); }));
        }
        QTest::keyClick(w.sceneView(), Qt::Key_Escape);
        QVERIFY(settle());
        QVERIFY(!panel->active());
        QCOMPARE(c->state().revision, rev);
        QCOMPARE(doc(c).value("features").toArray(), acceptedFeatures);
        panel->begin({}, fillet);
        spin("featureSize", 1);
        preview();
        accept();
        auto smaller = volume();
        QVERIFY(smaller > 35488.76110196153e-9);
        c->projectCommand("Undo");
        QVERIFY(settle());
        QVERIFY(std::abs(volume() - 35488.76110196153e-9) < 1e-12);
        c->projectCommand("Redo");
        QVERIFY(settle());
        QVERIFY(std::abs(volume() - smaller) < 1e-12);
        c->projectCommand("Undo");
        QVERIFY(settle());
        // Actual upstream rectangle edit: 80 -> 90 mm; ordered exact rebuild preserves stable IDs.
        sketch->chooseSketch(sketches[0].toMap().value("id").toString());
        sketch->editSelected();
        QVERIFY(sketch->active());
        auto *view = sketch->canvas();
        view->fit();
        QTest::qWait(20);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen({.08, .05}).toPoint());
        auto *width = w.findChild<QLineEdit *>("selectedWidth");
        QVERIFY(width->isVisible());
        width->setText("90");
        click("applySelectionProperties");
        QVERIFY(settle());
        sketch->finish();
        QVERIFY(settle());
        QVERIFY(!sketch->active());
        QVERIFY(std::abs(volume() - 40488.76110196153e-9) < 1e-12);
        c->projectCommand("Undo");
        QVERIFY(settle());
        QVERIFY(std::abs(volume() - 35488.76110196153e-9) < 1e-12);
        c->projectCommand("Redo");
        QVERIFY(settle());
        QVERIFY(std::abs(volume() - 40488.76110196153e-9) < 1e-12);
        // Display settings remain projections; density assignment is a durable SI scalar
        // convention.
        auto exact = volume();
        w.sceneView()->setMode("Wireframe");
        w.sceneView()->setMode("Shaded with edges");
        c->projectCommand("DisplayPart", sketch::fields({{"deflection_mm", 1.}}));
        QVERIFY(settle());
        QCOMPARE(volume(), exact);
        spin("materialDensity", 7800);
        click("assignDensityButton");
        QVERIFY(settle());
        c->projectCommand("MeasurePart", sketch::fields({{"body", QString()},
                                                         {"token", QString()},
                                                         {"second_token", QString()},
                                                         {"density_kg_m3", 7800.}}));
        QVERIFY(settle());
        QVERIFY(std::abs(panel->latestResult()
                             .value("part_measurement")
                             .toMap()
                             .value("mass_kg")
                             .toDouble() -
                         exact * 7800) < 1e-10);
        auto path = files.filePath("B1.cadproj");
        c->saveProject(path);
        QVERIFY(settle());
        QCOMPARE(c->state().savedRevision, c->state().revision);
        auto saved = c->state().revision;
        auto project = c->state().context.project;
        auto copy = doc(c).value("features").toArray();
        c->projectCommand(
            "SetParameter",
            sketch::fields({{"name", "RecoveryProbe"}, {"dimension", "scalar"}, {"value_si", 1.}}));
        QVERIFY(settle());
        auto recovery = c->state().revision;
        QProcess killer;
#ifdef Q_OS_WIN
        killer.start("taskkill", {"/PID", QString::number(c->workerPid()), "/F"});
#else
        killer.start("/bin/kill", {"-KILL", QString::number(c->workerPid())});
#endif
        QVERIFY(killer.waitForFinished(5000));
        QCOMPARE(killer.exitCode(), 0);
        QVERIFY(wait([&] { return c->state().phase == Phase::WorkerUnavailable; }));
        c->restart();
        QVERIFY(wait(
            [&] { return c->ready() && !c->outstanding() && c->state().phase == Phase::Ready; }));
        QCOMPARE(c->state().revision, recovery);
        QCOMPARE(c->state().savedRevision, saved);
        QCOMPARE(c->state().context.project, project);
        QCOMPARE(doc(c).value("features").toArray(), copy);
        QVERIFY(std::abs(volume() - exact) < 1e-12);
        c->closeProject();
        QVERIFY(settle());
        c->openProject(path, "Saved");
        QVERIFY(settle());
        QCOMPARE(c->state().revision, saved);
        QCOMPARE(doc(c).value("features").toArray(), copy);
        QVERIFY(std::abs(volume() - exact) < 1e-12);
        if (native) {
            QVERIFY2(graphics.empty(), "Native graphics initialization failed");
            w.sceneView()->setFilter("Face");
            QTest::mouseClick(w.sceneView(), Qt::LeftButton, Qt::NoModifier,
                              w.sceneView()->project({45, 10, 10}).toPoint());
            QVERIFY(c->state().selection.kind == "Face" &&
                    c->state().selection.entity.startsWith(base));

            auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QDir().mkpath(out);
            w.resize(1280, 800);
            QTest::qWait(100);
            QVERIFY(w.grab().save(out + "/features-B1.png"));
            auto b = doc(c).value("features").toArray()[0].toMap().value("id").toString();
            panel->begin({}, b);
            QTest::qWait(50);
            auto *acceptButton = w.findChild<QPushButton *>("acceptFeatureButton");
            QVERIFY(acceptButton->isVisible());
            QVERIFY(w.rect().contains(acceptButton->mapTo(&w, QPoint(0, 0))));
            QVERIFY(w.grab().save(out + "/features-editing.png"));
            panel->discard();
            QVERIFY(settle());
            QFile info(out + "/features-native.json");
            QVERIFY(info.open(QIODevice::WriteOnly));
            info.write(
                QJsonDocument(QJsonObject{{"status", "Pass"},
                                          {"dpr", w.devicePixelRatioF()},
                                          {"width", w.width()},
                                          {"height", w.height()},
                                          {"renderer", w.sceneView()->graphicsDescription()},
                                          {"saved_revision", saved},
                                          {"recovered_revision", recovery},
                                          {"volume_m3", exact},
                                          {"physical_device", "Blocked: synthetic input only"}})
                    .toJson());
        }
        c->closeProject();
        QVERIFY(settle());
        w.close();
    }
};
int main(int argc, char **argv) {
    QSurfaceFormat f;
    f.setVersion(3, 3);
    f.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc, argv);
    Contracts t;
    return QTest::qExec(&t, argc, argv);
}
#include "contracts.moc"
