#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/SketchView.hpp"
#include "render/Viewport.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif
using namespace cad;
namespace {
bool wait(std::function<bool()> f, int ms = 10000) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < ms)
        QTest::qWait(5);
    return f();
}
bool ready(Client &c) {
    return c.ready() && c.state().projectOpen && c.outstanding() == 0;
}
void mark(QString root) {
    QFile f(root + "/.cad-test-root");
    if (!f.open(QIODevice::WriteOnly))
        qFatal("Cannot mark disposable root");
    f.write("disposable\n");
}
QCborMap part(Client &c) {
    return c.state().documentRecords[0].toMap();
}
void click(QWidget &w, QString name) {
    auto *b = w.findChild<QPushButton *>(name);
    if (!b)
        qFatal("Missing button");
    for (auto *s : w.findChildren<QScrollArea *>())
        if (s->isAncestorOf(b))
            s->ensureWidgetVisible(b);
    QTest::qWait(10);
    QTest::mouseClick(b, Qt::LeftButton);
}
void coordinate(QWidget &w, double x, double y) {
    w.findChild<QLineEdit *>("sketchX")->setText(QString::number(x));
    w.findChild<QLineEdit *>("sketchY")->setText(QString::number(y));
    click(w, "addSketchCoordinate");
}
} // namespace
class Contracts : public QObject {
    Q_OBJECT
  private slots:
    void drafts() {
        exercise(false);
    }
    void pointerGestures() {
        gestures(false, "mm");
        gestures(false, "in");
    }
    void nativeSketches() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Blocked: native session required");
        exercise(true);
        gestures(true, "mm");
        gestures(true, "in");
    }

  private:
    void gestures(bool native, QString units) {
        qInfo() << "Pointer gesture workflow in" << units;
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-gesture-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        auto prefs = files.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow window(prefs, nullptr, root.path());
        QSignalSpy graphics(window.sceneView(), &Viewport::graphicsFailed);
        window.show();
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.raise();
            window.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&window));
        }
        auto *c = window.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        if (native)
            QVERIFY2(graphics.empty(), "Native renderer initialization failed");
        c->newProject("Part", "Pointer gestures", units);
        QVERIFY(wait([&] { return ready(*c); }));
        QVERIFY(window.findChild<SketchView *>()->accessibleDescription().startsWith("No sketch"));
        auto rev = c->state().revision;
        window.command("sketch")->trigger();
        auto *tool = window.findChild<QComboBox *>("sketchTool");
        auto *view = window.findChild<SketchView *>();
        auto *entities = window.findChild<QListWidget *>("sketchEntities");
        QVERIFY(window.findChild<QLineEdit *>("sketchRadius")->isHidden());
        QVERIFY(window.findChild<QLineEdit *>("sketchStart")->isHidden());
        tool->setCurrentText("Circle");
        QVERIFY(!window.findChild<QLineEdit *>("sketchRadius")->isHidden());
        QVERIFY(window.findChild<QLineEdit *>("sketchStart")->isHidden());
        QTest::qWait(30); // Let the contextual form finish its layout before locating model points.
        QPoint center = view->screen({.02, .02}).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
        QCOMPARE(entities->count(), 0); // Press must not add a fixed-radius circle.
        auto move = [&](QPoint at, Qt::MouseButtons held = Qt::LeftButton) {
            QMouseEvent event(QEvent::MouseMove, at, view->mapToGlobal(at), Qt::NoButton, held,
                              Qt::NoModifier);
            QApplication::sendEvent(view, &event);
            QTest::qWait(15);
        };
        auto ink = [&](QPoint at) {
            auto image = view->grab().toImage();
            auto ratio = image.devicePixelRatio();
            for (int dy = -3; dy <= 3; ++dy)
                for (int dx = -3; dx <= 3; ++dx) {
                    auto color = image.pixelColor(qRound((at.x() + dx) * ratio),
                                                  qRound((at.y() + dy) * ratio));
                    if (color.green() > 150 && (color.blue() > 150 || color.red() > 200))
                        return true;
                }
            return false;
        };
        move(center + QPoint(30, 0));
        QVERIFY2(ink(center + QPoint(30, 0)), "5 mm radius must be visible while held");
        move(center + QPoint(60, 0));
        QVERIFY2(ink(center + QPoint(60, 0)), "10 mm radius must be visible while held");
        QVERIFY2(!ink(center + QPoint(30, 0)), "Previous radius must be replaced");
        if (native) {
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!folder.isEmpty());
            QVERIFY(window.grab().save(folder + "/circle-drag-" + units + ".png"));
        }
        QCOMPARE(entities->count(), 0);
        QCOMPARE(c->state().revision, rev);
        QVERIFY(part(*c).value("sketches").toArray().empty());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center + QPoint(60, 0));
        QCOMPARE(entities->count(), 2);
        // Zero-radius release, Escape, and a tool switch must create no orphan points.
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, center);
        QCOMPARE(entities->count(), 2);
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
        move(center + QPoint(90, 0));
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(window.findChild<SketchPanel *>()->active());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center + QPoint(90, 0));
        QCOMPARE(entities->count(), 2);
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
        move(center + QPoint(120, 0));
        tool->setCurrentText("Point");
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center + QPoint(120, 0));
        QCOMPARE(entities->count(), 2);
        QSignalSpy solved(c, &Client::sketchPreview);
        click(window, "previewSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && solved.size() == 1; }),
                 qPrintable(c->state().message));
        auto result = sketch::parse(solved.last()[0].value<QCborArray>());
        QCOMPARE(result[0].entities[1].kind, QString("Circle"));
        QVERIFY(std::abs(result[0].entities[1].radius - .01) < 1e-12);
        QCOMPARE(c->state().revision, rev);
        click(window, "acceptSketchButton");
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == rev + 1; }));
        auto accepted = part(*c).value("sketches").toArray();
        window.command("undo")->trigger();
        QVERIFY(wait([&] { return ready(*c) && part(*c).value("sketches").toArray().empty(); }));
        window.command("redo")->trigger();
        QVERIFY(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == accepted; }));
        auto path = files.filePath("circles.cadproj");
        c->saveProject(path);
        QVERIFY(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }));
        c->closeProject();
        QVERIFY(wait([&] { return !c->state().projectOpen && !c->outstanding(); }));
        c->openProject(path);
        QVERIFY(wait([&] { return ready(*c); }));
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        click(window, "editSketchButton");
        // Two-click primitives show a rubber-band preview and Escape only cancels that input.
        for (auto type : {QString("Line"), QString("Polyline"), QString("Rectangle")}) {
            tool->setCurrentText(type);
            QTest::qWait(30);
            QPoint a = view->screen({.04, .03}).toPoint(), b = a + QPoint(60, -30);
            QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, a);
            move(b, Qt::NoButton);
            QVERIFY2(ink(b), qPrintable(type + " must preview its pending endpoint/corner"));
            QCOMPARE(entities->count(), 2);
            QTest::keyClick(view, Qt::Key_Escape);
            QVERIFY(window.findChild<SketchPanel *>()->active());
            QCOMPARE(entities->count(), 2);
        }
        // A complete pointer line/rectangle remains local until sketch acceptance.
        tool->setCurrentText("Line");
        QTest::qWait(30);
        auto first = view->screen({.04, .03}).toPoint();
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, first);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, first + QPoint(60, 0));
        QCOMPARE(entities->count(), 5);
        tool->setCurrentText("Rectangle");
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, first + QPoint(0, -60));
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, first + QPoint(60, -120));
        QCOMPARE(entities->count(), 13);
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        click(window, "cancelSketchButton");
        click(window, "editSketchButton");
        tool->setCurrentText("Circle");
        window.findChild<QLineEdit *>("sketchRadius")->setText("3.25");
        coordinate(window, .5, .75);
        QCOMPARE(entities->count(), 4);
        tool->setCurrentText("Arc");
        QVERIFY(!window.findChild<QLineEdit *>("sketchStart")->isHidden());
        QVERIFY(!window.findChild<QLineEdit *>("sketchSweep")->isHidden());
        coordinate(window, 1.5, .75);
        QCOMPARE(entities->count(), 6);
        solved.clear();
        click(window, "previewSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && solved.size() == 1; }),
                 qPrintable(c->state().message));
        result = sketch::parse(solved.last()[0].value<QCborArray>());
        double expectedRadius = domain::toSI(3.25, "length", units);
        QCOMPARE(result[0].entities[3].radius, expectedRadius);
        QCOMPARE(result[0].entities[5].radius, expectedRadius);
        QCOMPARE(result[0].entities[5].kind, QString("Arc"));
        // Escape without a pending primitive cancels the complete edit.
        view->setFocus();
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(!window.findChild<SketchPanel *>()->active());
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        window.close();
    }
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-ipc-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        auto prefs = files.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow window(prefs, nullptr, root.path());
        QSignalSpy graphics(window.sceneView(), &Viewport::graphicsFailed);
        window.show();
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.raise();
            window.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&window));
        }
        auto *c = window.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        if (native)
            QVERIFY2(graphics.empty(), "Native renderer initialization failed");
        c->newProject("Part", "B1 sketches");
        QVERIFY(wait([&] { return ready(*c); }));
        QVERIFY(window.command("sketch")->isEnabled());
        window.command("sketch")->trigger();
        window.findChild<QComboBox *>("sketchTool")->setCurrentText("Point");
        QSignalSpy precise(c, &Client::sketchPreview);
        coordinate(window, 12.7, .25);
        click(window, "previewSketchButton");
        QVERIFY2(wait([&] { return !c->outstanding() && precise.size() == 1; }),
                 qPrintable(c->state().message));
        auto points = sketch::parse(precise[0][0].value<QCborArray>());
        QCOMPARE(points[0].entities[0].point.x, .0127);
        QCOMPARE(points[0].entities[0].point.y, .00025);
        click(window, "cancelSketchButton");
        window.command("sketch")->trigger();
        window.findChild<QComboBox *>("sketchTool")->setCurrentText("Rectangle");
        coordinate(window, 0, 0);
        coordinate(window, 80, 50);
        auto rev = c->state().revision;
        click(window, "previewSketchButton");
        QVERIFY2(
            wait([&] { return c->state().phase == Phase::PreviewReady && c->outstanding() == 0; }),
            qPrintable(c->state().message));
        QCOMPARE(c->state().revision, rev);
        QVERIFY(part(*c).value("sketches").toArray().empty());
        auto *view = window.findChild<SketchView *>();
        QVERIFY(view);
        click(window, "previewSketchButton");
        view->setFocus();
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(!window.findChild<SketchPanel *>()->active());
        QVERIFY(wait([&] { return !c->outstanding(); }));
        QVERIFY(c->state().phase != Phase::PreviewReady);
        QVERIFY(part(*c).value("sketches").toArray().empty());
        for (auto box :
             {std::array<double, 4>{0, 0, 80, 50}, std::array<double, 4>{20, 20, 40, 40}}) {
            window.command("sketch")->trigger();
            coordinate(window, box[0], box[1]);
            coordinate(window, box[2], box[3]);
            click(window, "acceptSketchButton");
            ++rev;
            QVERIFY2(wait([&] { return ready(*c) && c->state().revision == rev; }),
                     qPrintable(c->state().message));
        }
        auto accepted = part(*c).value("sketches").toArray();
        QCOMPARE(accepted.size(), 2);
        QCOMPARE(accepted[0].toMap().value("entities").toArray().size(), 8);
        window.findChild<QComboBox *>("sketchChoice")->setCurrentIndex(0);
        click(window, "editSketchButton");
        window.findChild<QComboBox *>("sketchTool")->setCurrentText("Select / drag");
        auto point = view->screen({0, 0}).toPoint();
        auto draggedId = view->pick(point);
        QVERIFY(!draggedId.isEmpty());
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, point);
        QMouseEvent move(QEvent::MouseMove, point + QPoint(30, -10),
                         view->mapToGlobal(point + QPoint(30, -10)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(view, &move);
        QCOMPARE(view->pick(point + QPoint(30, -10)), draggedId);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, point + QPoint(30, -10));
        QTest::keyClick(view, Qt::Key_Escape);
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        window.command("undo")->trigger();
        QVERIFY(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray().size() == 1; }));
        window.command("redo")->trigger();
        QVERIFY(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray().size() == 2; }));
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        auto *choice = window.findChild<QComboBox *>("sketchChoice");
        for (int index = 0; index < 2; ++index) {
            choice->setCurrentIndex(index);
            click(window, "editSketchButton");
            auto addRelation = [&](QString kind, int entity, QString expr = {}) {
                auto *list = window.findChild<QListWidget *>("sketchEntities");
                list->clearSelection();
                list->item(entity)->setSelected(true);
                window.findChild<QComboBox *>("sketchRelation")->setCurrentText(kind);
                if (!expr.isEmpty())
                    window.findChild<QLineEdit *>("sketchDimension")->setText(expr);
                click(window, "addSketchConstraint");
            };
            addRelation("Fixed", 0);
            for (int j = 0; j < 4; ++j)
                addRelation(j % 2 ? "Vertical" : "Horizontal", 4 + j);
            window.findChild<QLineEdit *>("sketchNameExpression")
                ->setText(index ? "Width = 20 mm" : "Width = 80 mm");
            click(window, "setSketchName");
            addRelation("Length", 4, "Width");
            addRelation("Length", 5, index ? "20 mm" : "50 mm");
            auto revision = c->state().revision;
            click(window, "previewSketchButton");
            QVERIFY2(wait([&] {
                         return c->state().phase == Phase::PreviewReady && c->outstanding() == 0;
                     }),
                     qPrintable(c->state().message));
            QVERIFY(c->state().message.contains("FullyDefined"));
            auto *dimension = window.findChild<QLineEdit *>("sketchDimension");
            if (native) {
                window.raise();
                window.activateWindow();
                QVERIFY(QTest::qWaitForWindowActive(&window));
                for (auto *scroll : window.findChildren<QScrollArea *>())
                    if (scroll->isAncestorOf(dimension))
                        scroll->ensureWidgetVisible(dimension);
                QTest::mouseClick(dimension, Qt::LeftButton);
                QVERIFY(wait([&] { return dimension->hasFocus(); }));
            } else
                dimension->setFocus();
            QTest::keyClick(dimension, Qt::Key_Return);
            QCOMPARE(c->state().revision, revision);
            QVERIFY(window.findChild<SketchPanel *>()->active());
            if (native && index == 1) {
                window.raise();
                window.activateWindow();
                QVERIFY(QTest::qWaitForWindowActive(&window));
                QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, QPoint(20, 80));
                QVERIFY(wait([&] { return view->hasFocus(); }));
                QTest::keyClick(view, Qt::Key_Return);
            } else
                click(window, "acceptSketchButton");
            QVERIFY2(wait([&] { return ready(*c) && c->state().revision == revision + 1; }),
                     qPrintable(c->state().message));
        }
        choice->setCurrentIndex(0);
        accepted = part(*c).value("sketches").toArray();
        QCOMPARE(
            part(*c).value("sketch_diagnostics").toArray()[0].toMap().value("freedom").toInteger(),
            0);
        click(window, "editSketchButton");
        window.findChild<QLineEdit *>("sketchNameExpression")->setText("Width = 90 mm");
        click(window, "setSketchName");
        click(window, "previewSketchButton");
        QVERIFY(
            wait([&] { return c->state().phase == Phase::PreviewReady && c->outstanding() == 0; }));
        auto dimensionRevision = c->state().revision;
        click(window, "acceptSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && c->state().revision == dimensionRevision + 1; }),
                 qPrintable(c->state().message));
        auto changed = part(*c).value("sketches").toArray();
        auto changedSketch = sketch::parse(changed);
        QVERIFY(std::abs(changedSketch[0].entities[2].point.x - .09) < 1e-10);
        window.command("undo")->trigger();
        QVERIFY(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == accepted; }));
        window.command("redo")->trigger();
        QVERIFY(wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == changed; }));
        accepted = changed;
        click(window, "editSketchButton");
        auto *constraints = window.findChild<QListWidget *>("sketchConstraints");
        constraints->setCurrentRow(5);
        window.findChild<QLineEdit *>("sketchDimension")->setText("100 mm");
        click(window, "editSketchDimension");
        click(window, "previewSketchButton");
        QVERIFY(wait([&] { return !c->outstanding(); }));
        QTest::keyClick(window.findChild<QLineEdit *>("sketchDimension"), Qt::Key_Escape);
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        click(window, "editSketchButton");
        constraints->setCurrentRow(6);
        click(window, "deleteSketchConstraint");
        auto deleteRevision = c->state().revision;
        click(window, "acceptSketchButton");
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == deleteRevision + 1; }));
        QCOMPARE(
            part(*c).value("sketch_diagnostics").toArray()[0].toMap().value("freedom").toInteger(),
            1);
        window.command("undo")->trigger();
        QVERIFY(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == accepted; }));
        auto path = files.filePath("B1.cadproj");
        c->saveProject(path);
        QVERIFY2(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }),
                 qPrintable(c->state().message));
        auto savedRevision = c->state().savedRevision;
        click(window, "editSketchButton");
        window.findChild<QLineEdit *>("sketchNameExpression")->setText("Width = 91 mm");
        click(window, "setSketchName");
        click(window, "acceptSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && c->state().revision == savedRevision + 1; }),
                 qPrintable(c->state().message));
        auto recoverable = part(*c).value("sketches").toArray();
        QVERIFY(recoverable != accepted);
#ifdef Q_OS_UNIX
        QVERIFY(::kill(pid_t(c->workerPid()), SIGKILL) == 0);
        QVERIFY(wait([&] { return c->state().phase == Phase::WorkerUnavailable; }));
#endif
        c->restart();
        QVERIFY2(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == recoverable; }),
            qPrintable(c->state().message));
        QCOMPARE(c->state().savedRevision, savedRevision);
        c->closeProject();
        QVERIFY(wait([&] { return !c->state().projectOpen && c->outstanding() == 0; }));
        c->openProject(path);
        QVERIFY2(wait([&] { return ready(*c); }), qPrintable(c->state().message));
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        if (native) {
            QString artifacts = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!artifacts.isEmpty());
            for (auto *scroll : window.findChildren<QScrollArea *>())
                scroll->verticalScrollBar()->setValue(0);
            QTest::qWait(30);
            for (auto id : {"previewSketchButton", "acceptSketchButton", "cancelSketchButton"}) {
                auto *button = window.findChild<QPushButton *>(id);
                QVERIFY(button->visibleRegion().boundingRect().height() > 0);
            }
            QVERIFY(window.grab().save(artifacts + "/sketches-native.png"));
            QFile report(artifacts + "/native.json");
            QVERIFY(report.open(QIODevice::WriteOnly));
            report.write(
                QJsonDocument(QJsonObject{{"platform", QGuiApplication::platformName()},
                                          {"DPR", window.devicePixelRatioF()},
                                          {"graphics", window.sceneView()->graphicsDescription()},
                                          {"revision", c->state().revision},
                                          {"sketches", accepted.size()},
                                          {"keyboard_cancel", true}})
                    .toJson());
        }
        window.close();
    }
};
int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("NativeCadTests");
    app.setApplicationName("Module3");
    Contracts tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "contracts.moc"
