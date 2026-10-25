#include "../module4/Fixture.hpp"
#include "AssemblyPanel.hpp"
#include "MainWindow.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
using namespace cad;
namespace {
bool wait(std::function<bool()> predicate) {
    QElapsedTimer t;
    t.start();
    while (!predicate() && t.elapsed() < 30000)
        QTest::qWait(5);
    return predicate();
}
QCborMap document(Client *c) {
    for (auto v : c->state().documentRecords)
        if (v.toMap().value("id").toString() == c->state().activeDocument)
            return v.toMap();
    return {};
}
} // namespace
class AssemblyContracts : public QObject {
    Q_OBJECT
  private slots:
    void lifecycle() {
        exercise(false);
    }
    void nativeAssembly() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        exercise(true);
    }

  private:
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module7-native-XXXXXX"),
            files(QDir::tempPath() + "/cad-module7-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            QVERIFY(r->isValid());
            QFile mark(r->filePath(".cad-test-root"));
            QVERIFY(mark.open(QIODevice::WriteOnly));
            mark.write("disposable\n");
        }
        auto prefs = files.filePath("settings.ini");
        {
            QSettings s(prefs, QSettings::IniFormat);
            s.setValue("startup/offerRecovery", false);
            s.sync();
        }
        MainWindow w(prefs, nullptr, root.path());
        w.show();
        QSignalSpy graphics(w.sceneView(), &Viewport::graphicsFailed);
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            w.raise();
            w.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&w));
        }
        auto *c = w.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        auto settle = [&] { return wait([&] { return !c->outstanding(); }); };
        c->newProject("Part", "Shared bracket");
        QVERIFY(settle());
        fixture4::B1 b1;
        c->projectCommand("SetSketches", sketch::fields({{"sketches", b1.sketches()}}));
        QVERIFY(settle());
        c->projectCommand("SetFeatures", sketch::fields({{"features", b1.features()}}));
        QVERIFY(settle());
        QVERIFY(c->mesh());
        auto partId = c->state().activeDocument;
        c->createDocument("Assembly", "Native assembly");
        QVERIFY(settle());
        auto assemblyId = c->state().activeDocument;
        QVERIFY(w.command("assembly")->isEnabled());
        w.command("assembly")->trigger();
        auto *panel = w.findChild<AssemblyPanel *>("assemblyPanel");
        QVERIFY(panel && panel->isVisible());
        auto click = [&](QString id) {
            auto *b = w.findChild<QPushButton *>(id);
            if (!b || !b->isEnabled())
                return false;
            b->click();
            return settle();
        };
        QVERIFY(click("assemblyInsert"));
        QVERIFY2(w.findChild<QPushButton *>("assemblyAccept")->isEnabled(),
                 qPrintable(c->state().message));
        QVERIFY(panel->active());
        QVERIFY(!w.command("save")->isEnabled());
        QVERIFY(click("assemblyAccept"));
        auto model = assembly::Model::parse(document(c).value("assembly").toMap());
        QCOMPARE(model.occurrences.size(), size_t(1));
        QVERIFY(model.occurrences[0].grounded);
        QVERIFY(!panel->active());
        auto first = model.occurrences[0].id;
        QVERIFY(click("assemblyInsert"));
        w.findChild<QDoubleSpinBox *>("assemblyPosition0")->setValue(100);
        w.findChild<QDoubleSpinBox *>("assemblyAngle2")->setValue(30);
        QVERIFY(click("assemblyPlace"));
        QVERIFY(click("assemblyAccept"));
        model = assembly::Model::parse(document(c).value("assembly").toMap());
        QCOMPARE(model.occurrences.size(), size_t(2));
        auto second = model.occurrences[1].id;
        QVERIFY(c->mesh());
        QCOMPARE(c->mesh()->instances.size(), 2);
        QCOMPARE(c->mesh()->instances[0].firstTriangle, c->mesh()->instances[1].firstTriangle);
        QVERIFY(std::abs(c->mesh()->instances[1].rotation.z()) > .1);
        if (native) {
            w.sceneView()->setView("Isometric");
            w.sceneView()->fit();
            QTest::qWait(100);
            QPoint p;
            bool found = false;
            auto display = c->mesh();
            auto instance = display->instances[1];
            for (int t = instance.firstTriangle;
                 t < instance.firstTriangle + instance.triangleCount; ++t) {
                QVector3D center;
                for (int k = 0; k < 3; ++k)
                    center += instance.map(display->vertices[display->indices[t * 3 + k]]) / 3;
                auto candidate = w.sceneView()->project(center).toPoint();
                if (w.sceneView()->rect().contains(candidate) &&
                    w.sceneView()->pick(candidate).occurrence == second) {
                    p = candidate;
                    found = true;
                    break;
                }
            }
            QVERIFY2(found, "A visible transformed component must be pickable");
            QSignalSpy drag(w.sceneView(), &Viewport::componentDragged);
            QTest::mousePress(w.sceneView(), Qt::LeftButton, Qt::NoModifier, p);
            QTest::mouseMove(w.sceneView(), p + QPoint(20, 10));
            QTest::mouseRelease(w.sceneView(), Qt::LeftButton, Qt::NoModifier, p + QPoint(20, 10));
            QVERIFY(!drag.empty());
            QVERIFY(settle());
            QVERIFY(panel->active());
            QVERIFY(click("assemblyCancel"));
        }
        // UI datum mate, remaining freedom and conflicting draft all travel through the real
        // worker.
        w.findChild<QComboBox *>("assemblyMateKind")->setCurrentText("Distance");
        w.findChild<QDoubleSpinBox *>("assemblyMateValue")->setValue(100);
        QVERIFY(click("assemblyAddMate"));
        QVERIFY(w.findChild<QLabel *>("assemblyDiagnostics")->text().contains("freedom 5"));
        QVERIFY(click("assemblyAccept"));
        auto revision = c->state().revision;
        w.findChild<QDoubleSpinBox *>("assemblyMateValue")->setValue(200);
        QVERIFY(click("assemblyAddMate"));
        QVERIFY(!w.findChild<QPushButton *>("assemblyAccept")->isEnabled());
        QCOMPARE(c->state().revision, revision);
        QVERIFY(click("assemblyCancel"));
        QCOMPARE(c->state().revision, revision);
        QVERIFY(!panel->active());
        // Drag preview/cancel never changes the acknowledged transform or revision.
        panel->drag(second, {0, 10, 0});
        QVERIFY(settle());
        QVERIFY(panel->active());
        QCOMPARE(c->state().revision, revision);
        QVERIFY(click("assemblyCancel"));
        QVERIFY(click("assemblyReferences"));
        QVERIFY(wait([&] {
            return !c->outstanding() &&
                   w.findChild<QComboBox *>("assemblySecondReference")->count() > 7;
        }));
        QVERIFY(w.findChild<QComboBox *>("assemblyFirstReference")->count() > 7);
        // Remove mate, overlap both instances, then highlight an exact interference result.
        auto *mate = w.findChild<QComboBox *>("assemblyMate");
        mate->setCurrentIndex(1);
        QVERIFY(click("assemblyRemoveMate"));
        QVERIFY(click("assemblyAccept"));
        auto *occ = w.findChild<QComboBox *>("assemblyOccurrence");
        occ->setCurrentIndex(occ->findData(second));
        w.findChild<QDoubleSpinBox *>("assemblyPosition0")->setValue(0);
        w.findChild<QDoubleSpinBox *>("assemblyPosition1")->setValue(0);
        w.findChild<QDoubleSpinBox *>("assemblyAngle2")->setValue(0);
        QVERIFY(click("assemblyPlace"));
        QVERIFY(click("assemblyAccept"));
        QVERIFY(click("assemblyInterference"));
        QVERIFY(w.findChild<QLabel *>("assemblyDiagnostics")->text().contains("Overlap"));
        QCOMPARE(c->state().selection.occurrence, first);
        // Independent state controls preserve authoritative occurrences; undo restores visibility.
        occ->setCurrentIndex(occ->findData(second));
        w.findChild<QCheckBox *>("assemblyHidden")->setChecked(true);
        QVERIFY(click("assemblyPlace"));
        QVERIFY(click("assemblyAccept"));
        QCOMPARE(c->mesh()->instances.size(), 1);
        c->projectCommand("Undo");
        QVERIFY(settle());
        QCOMPARE(c->mesh()->instances.size(), 2);
        auto original = c->state().context.project;
        auto copy = files.filePath("assembly.cadproj");
        c->portableCopy(copy);
        QVERIFY(settle());
        QVERIFY(QFileInfo::exists(copy));
        QCOMPARE(c->state().context.project, original);
        c->closeProject();
        QVERIFY(settle());
        c->openProject(copy);
        QVERIFY(settle());
        c->switchDocument(assemblyId);
        QVERIFY(settle());
        QCOMPARE(c->mesh()->instances.size(), 2);
        QVERIFY(c->state().context.project != original);
        auto project = c->state().context.project;
        revision = c->state().revision;
        c->restart();
        QVERIFY(wait([&] { return c->ready() && !c->outstanding(); }));
        QCOMPARE(c->state().context.project, project);
        QCOMPARE(c->state().revision, revision);
        c->switchDocument(assemblyId);
        QVERIFY(settle());
        QVERIFY(c->mesh());
        QCOMPARE(c->mesh()->instances.size(), 2);
        QCOMPARE(assembly::Model::parse(document(c).value("assembly").toMap()).occurrences[1].id,
                 second);
        c->switchDocument(partId);
        QVERIFY(settle());
        QVERIFY(!w.command("assembly")->isEnabled());
        c->switchDocument(assemblyId);
        QVERIFY(settle());
        if (native) {
            auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!directory.isEmpty());
            QVERIFY(w.grab().save(directory + "/assembly.png"));
            QVERIFY(graphics.empty());
            QVERIFY(w.sceneView()->graphicsDescription().contains("initialized 1"));
            QFile out(directory + "/assembly-native.json");
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write(
                QJsonDocument(
                    QJsonObject{
                        {"status", "Pass"},
                        {"dpr", w.devicePixelRatioF()},
                        {"renderer", w.sceneView()->graphicsDescription()},
                        {"scope",
                         "Native Cocoa assembly panel, transformed shared geometry, datum mates, "
                         "conflict/cancel, viewport drag signal, exact-reference choices, overlap "
                         "highlight, states/undo, portable copy and worker recovery"},
                        {"physical_device", "Blocked: automated input"}})
                    .toJson());
        }
        c->closeProject();
        QVERIFY(settle());
        w.close();
    }
};
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    AssemblyContracts test;
    return QTest::qExec(&test, argc, argv);
}
#include "contracts.moc"
