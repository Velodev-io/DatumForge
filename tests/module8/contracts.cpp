#include "../module4/Fixture.hpp"
#include "DrawingPanel.hpp"
#include "MainWindow.hpp"
#include "render/Viewport.hpp"
#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
using namespace cad;
namespace {
bool wait(std::function<bool()> f) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 60000)
        QTest::qWait(10);
    return f();
}
} // namespace
class DrawingContracts : public QObject {
    Q_OBJECT
  private slots:
    void lifecycle() {
        exercise(false);
    }
    void nativeDrawing() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        exercise(true);
    }

  private:
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module8-native-XXXXXX"),
            files(QDir::tempPath() + "/cad-module8-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            QVERIFY(r->isValid());
            QFile f(r->filePath(".cad-test-root"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("disposable\n");
        }
        auto prefs = files.filePath("prefs.ini");
        {
            QSettings s(prefs, QSettings::IniFormat);
            s.setValue("startup/offerRecovery", false);
        }
        MainWindow w(prefs, nullptr, root.path());
        w.show();
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            w.raise();
            w.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&w));
        }
        auto *c = w.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        auto settle = [&] { return wait([&] { return c->outstanding() == 0; }); };
        c->newProject("Part", "Bracket");
        QVERIFY(settle());
        fixture4::B1 b;
        c->projectCommand("SetSketches", sketch::fields({{"sketches", b.sketches()}}));
        QVERIFY(settle());
        c->projectCommand("SetFeatures", sketch::fields({{"features", b.features()}}));
        QVERIFY(settle());
        auto part = c->state().activeDocument;
        c->createDocument("Drawing", "Native drawing");
        QVERIFY(settle());
        auto doc = c->state().activeDocument;
        auto *panel = w.findChild<DrawingPanel *>("drawingProperties");
        QVERIFY(panel && panel->isVisible());
        for (auto id : {"drawingAddSheet", "drawingAddView", "drawingAddAnnotation",
                        "drawingRepairAnnotation", "drawingSaveSheet"})
            QVERIFY(!w.findChild<QPushButton *>(id)->isEnabled());
        QVERIFY(!w.findChild<QLineEdit *>("drawingSheetName")->isEnabled());
        QVERIFY(w.command("drawing.edit")->isEnabled());
        w.command("drawing.edit")->trigger();
        QVERIFY(panel->active());
        QVERIFY(w.findChild<QPushButton *>("drawingAddSheet")->isEnabled());
        QVERIFY(w.findChild<QLineEdit *>("drawingSheetName")->isEnabled());
        QVERIFY(!w.command("save")->isEnabled());
        auto click = [&](QString id) {
            auto *b = w.findChild<QPushButton *>(id);
            if (!b || !b->isEnabled())
                return false;
            b->click();
            return settle();
        };
        QVERIFY(click("drawingAddView"));
        auto rev = c->state().revision;
        QSignalSpy results(c, &Client::drawingInformation);
        QVERIFY(click("drawingPreview"));
        QVERIFY(!results.empty());
        QCOMPARE(c->state().revision, rev);
        QVERIFY(click("drawingAccept"));
        QVERIFY(!panel->active());
        QVERIFY(!w.findChild<QPushButton *>("drawingAddView")->isEnabled());
        QVERIFY2(c->state().phase != Phase::Failed, qPrintable(c->state().message));
        QVERIFY(!c->state().stale);
        QCOMPARE(c->state().revision, rev + 1);
        QVERIFY(click("drawingBegin"));
        w.findChild<QComboBox *>("drawingOrientation")->setCurrentText("Top");
        QVERIFY(click("drawingAddView"));
        QVERIFY(click("drawingCancel"));
        QCOMPARE(c->state().revision, rev + 1);
        QVERIFY(click("drawingBegin"));
        QVERIFY(click("drawingAnchors"));
        auto *anchors = w.findChild<QComboBox *>("drawingAnchor");
        QVERIFY(anchors->count() > 0);
        w.findChild<QComboBox *>("drawingAnnotationKind")->setCurrentText("Text");
        w.findChild<QLineEdit *>("drawingText")->setText("Bracket drawing");
        w.findChild<QDoubleSpinBox *>("drawingY")->setValue(130);
        QVERIFY(click("drawingAddAnnotation"));
        QVERIFY(click("drawingAccept"));
        QVERIFY(!c->state().stale);
        auto pdf = files.filePath("native.pdf");
        QSignalSpy exports(c, &Client::exchangeReport);
        c->exportDrawing(pdf);
        QVERIFY(settle());
        QVERIFY(QFileInfo(pdf).size() > 1000);
        QVERIFY(!exports.empty());
        auto saved = files.filePath("native.cadproj");
        c->saveProject(saved);
        QVERIFY(settle());
        auto project = c->state().context.project;
        c->restart();
        QVERIFY(wait([&] { return c->ready(); }));
        c->recoverProject(project);
        QVERIFY(settle());
        c->switchDocument(doc);
        QVERIFY(settle());
        QVERIFY(!c->state().stale);
        c->switchDocument(part);
        QVERIFY(settle());
        c->projectCommand("RenameDocument", sketch::fields({{"name", "Updated bracket"}}));
        QVERIFY(settle());
        c->switchDocument(doc);
        QVERIFY(settle());
        QVERIFY(c->state().stale);
        w.command("drawing.update")->trigger();
        QVERIFY(settle());
        QVERIFY(!c->state().stale);
        if (native) {
            auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!out.isEmpty());
            QVERIFY(QDir().mkpath(out));
            QVERIFY(w.grab().save(out + "/drawing.png"));
            QVERIFY(QFile::copy(pdf, out + "/native-drawing.pdf"));
            auto *canvas = w.findChild<DrawingCanvas *>();
            QVERIFY(canvas->isVisible());
            QCOMPARE(w.size(), QSize(1280, 800));
        }
        c->closeProject();
        QVERIFY(settle());
        w.close();
    }
};
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QSurfaceFormat f;
    f.setVersion(3, 3);
    f.setProfile(QSurfaceFormat::CoreProfile);
    f.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    DrawingContracts test;
    return QTest::qExec(&test, argc, argv);
}
#include "contracts.moc"
