#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "protocol/Client.hpp"
#include "render/SketchView.hpp"
#include "render/Viewport.hpp"
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNativeGestureEvent>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QWheelEvent>
using namespace cad;
namespace {
bool wait(std::function<bool()> f) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 12000)
        QTest::qWait(5);
    return f();
}
void mark(QString path) {
    QFile f(path + "/.cad-test-root");
    if (!f.open(QIODevice::WriteOnly))
        qFatal("No disposable marker");
    f.write("disposable\n");
}
QToolButton *button(MainWindow &w, QString id) {
    for (auto *b : w.findChildren<QToolButton *>())
        if (b->defaultAction() == w.command(id) && b->isVisible())
            return b;
    return nullptr;
}
QCborMap part(Client &c) {
    for (auto item : c.state().documentRecords) {
        auto d = item.toMap();
        if (d.value("type").toString() == "Part")
            return d;
    }
    return {};
}
} // namespace
class Workspace : public QObject {
    Q_OBJECT
  private slots:
    void layout() {
        exercise(false);
    }
    void nativeWorkspace() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Blocked: native session required");
        exercise(true);
    }
    void toolSelection() {
        QTemporaryDir root(QDir::tempPath() + "/cad-module4-tools-XXXXXX");
        QVERIFY(root.isValid());
        mark(root.path());
        auto prefs = root.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow w(prefs, nullptr, root.path());
        w.show();
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TESTS")) {
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            w.raise();
            w.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&w));
        }
        auto *c = w.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        c->newProject("Part", "Tool selection regression");
        QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
        w.command("sketch")->trigger();
        auto *panel = w.findChild<SketchPanel *>();
        auto *view = panel->canvas();
        auto *line = w.command("sketch.tool.Line");
        auto *select = w.command("sketch.tool.Select / drag");
        auto *arc = w.command("sketch.tool.Tangent Arc");
        auto *entities = w.findChild<QListWidget *>("sketchEntities");
        auto *tabs = w.findChild<QTabWidget *>("commandTabs");
        tabs->setCurrentIndex(1);
        auto expectTool = [&](QString name) {
            auto *action = w.command("sketch.tool." + name);
            return panel->toolName() == name && action->isChecked() &&
                   action->actionGroup()->checkedAction() == action;
        };
        auto capture = [&](QString name) {
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            if (!folder.isEmpty())
                QVERIFY(w.grab().save(folder + "/" + name + ".png"));
        };
        QVERIFY(expectTool("Select / drag"));
        auto *lineButton = button(w, "sketch.tool.Line");
        QVERIFY(lineButton);
        QTest::mouseClick(lineButton, Qt::LeftButton);
        QVERIFY(expectTool("Line"));
        capture("line-selected");
        // Clicking the active drawing button must leave drawing mode.
        QTest::mouseClick(lineButton, Qt::LeftButton);
        QVERIFY(expectTool("Select / drag"));
        auto click = [&](sketch::Vec2 p) {
            QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen(p).toPoint());
        };
        click({-.02, 0});
        click({0, 0});
        QCOMPARE(entities->count(), 0);
        line->trigger();
        click({-.02, 0}); // Cancel a pending line using the same button.
        line->trigger();
        QVERIFY(expectTool("Select / drag"));
        click({.02, .02});
        QCOMPARE(entities->count(), 0);
        for (QString name : {"Circle", "Rectangle", "Select / drag", "Line"}) {
            panel->setTool(name);
            QVERIFY2(expectTool(name), qPrintable(name));
        }
        click({-.02, 0});
        click({0, 0});
        QCOMPARE(entities->count(), 3);
        auto move = [&](sketch::Vec2 p) {
            auto at = view->screen(p);
            QMouseEvent e(QEvent::MouseMove, at, view->mapToGlobal(at), Qt::NoButton, Qt::NoButton,
                          Qt::NoModifier);
            QApplication::sendEvent(view, &e);
        };
        move({.005, 0});
        move({0, 0});
        QVERIFY(expectTool("Tangent Arc"));
        QVERIFY(!line->isChecked());
        QCOMPARE(w.findChild<QToolButton *>("sketchArcFlyout")->defaultAction(), arc);
        capture("inferred-tangent-arc");
        arc->trigger(); // Turning off the inferred arc also stops drawing.
        QVERIFY(expectTool("Select / drag"));
        QVERIFY(!view->sizingArc());
        capture("drawing-stopped");
        click({.02, .02});
        QCOMPARE(entities->count(), 3);
        select->trigger();
        QVERIFY(expectTool("Select / drag"));
        panel->discard();
        panel->setTool("Circle");
        w.command("sketch")->trigger();
        QVERIFY(expectTool("Select / drag"));
        panel->discard();
        c->closeProject();
        QVERIFY(wait([&] { return !c->state().projectOpen && !c->outstanding(); }));
        w.close();
    }

  private:
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-workspace-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-files-XXXXXX");
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
        QVERIFY(wait([&] { return c->ready(); }));
        c->newProject("Part", "Workspace bracket");
        QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
        if (native)
            QVERIFY2(graphics.empty(), "Native renderer initialization failed");
        auto *tabs = w.findChild<QTabWidget *>("commandTabs");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 5);
        QCOMPARE(tabs->tabText(0), QString("Features"));
        QCOMPARE(tabs->tabText(1), QString("Sketch"));
        QCOMPARE(tabs->tabText(2), QString("Evaluate"));
        QCOMPARE(tabs->tabText(3), QString("Assembly"));
        QVERIFY(!tabs->isTabEnabled(3));
        for (auto *bar : {tabs->tabBar(), w.findChild<QTabWidget *>("managerTabs")->tabBar()})
            for (int i = 0; i < bar->count(); ++i) {
                QVERIFY(bar->tabRect(i).width() >=
                        bar->fontMetrics().horizontalAdvance(bar->tabText(i)) + 8);
                QVERIFY(bar->rect().contains(bar->tabRect(i)));
            }
        for (auto id : {"extrude", "revolve", "fillet", "hole", "pattern", "mirror"}) {
            QVERIFY(!w.command(id)->isEnabled());
            QVERIFY(!w.command(id)->property("unavailableReason").toString().isEmpty());
        }
        auto *dock = w.findChild<QDockWidget *>("modelDock");
        QVERIFY(dock);
        QCOMPARE(w.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
        auto *tree = w.findChild<QTreeWidget *>("modelTree");
        QVERIFY(tree->topLevelItem(0)->childCount() >= 4);
        auto partId = part(*c).value("id").toString();
        auto revision = c->state().revision;
        auto *newSketch = button(w, "sketch");
        QVERIFY(newSketch);
        QTest::mouseClick(newSketch, Qt::LeftButton);
        auto *panel = w.findChild<SketchPanel *>();
        auto *view = panel->canvas();
        auto *properties = w.findChild<QWidget *>("sketchPropertyManager");
        QTest::qWait(30);
        QVERIFY(panel->active());
        QCOMPARE(tabs->currentIndex(), 1);
        QCOMPARE(w.findChild<QTabWidget *>("managerTabs")->currentIndex(), 1);
        QVERIFY(properties->mapToGlobal(QPoint(properties->width(), 0)).x() <=
                view->mapToGlobal(QPoint(0, 0)).x());
        auto *circle = button(w, "sketch.tool.Circle");
        QVERIFY(circle);
        QTest::mouseClick(circle, Qt::LeftButton);
        QTest::qWait(20);
        auto firstPixel = view->screen({0, 0}).toPoint();
        auto firstWorld = view->local(firstPixel);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, firstPixel);
        auto beforePan = view->screen({0, 0});
        QPointF at(view->rect().center());
        QWheelEvent pan(at, view->mapToGlobal(at), {30, -20}, {}, Qt::NoButton, Qt::NoModifier,
                        Qt::ScrollUpdate, false);
        QApplication::sendEvent(view, &pan);
        QVERIFY(QLineF(view->screen({0, 0}), beforePan + QPointF(30, -20)).length() < 1e-10);
        auto zoomAnchor = view->local(at);
        QNativeGestureEvent pinch(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(),
                                  2, at, at, view->mapToGlobal(at), .2, {});
        QApplication::sendEvent(view, &pinch);
        auto zoomAfter = view->local(at);
        QVERIFY(std::abs(zoomAfter.x - zoomAnchor.x) < 1e-12 &&
                std::abs(zoomAfter.y - zoomAnchor.y) < 1e-12);
        auto *resources = w.findChild<QDockWidget *>("resourcesDock");
        resources->toggleViewAction()->trigger();
        QTest::qWait(30);
        QVERIFY(resources->isVisible());
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen({.006, 0}).toPoint());
        resources->toggleViewAction()->trigger();
        QTest::qWait(30);
        auto *rectangle = button(w, "sketch.tool.Rectangle");
        QVERIFY(rectangle);
        QTest::mouseClick(rectangle, Qt::LeftButton);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier,
                          view->screen({-.02, -.01}).toPoint());
        w.resize(1100, 720);
        QTest::qWait(30);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen({.02, .01}).toPoint());
        QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), 10);
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(panel->active());
        QCOMPARE(panel->toolName(), QString("Select / drag"));
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen({.02, .01}).toPoint());
        auto *width = w.findChild<QLineEdit *>("selectedWidth");
        auto *height = w.findChild<QLineEdit *>("selectedHeight");
        QVERIFY(width->isVisible() && height->isVisible());
        width->setText("60");
        height->setText("30");
        QTest::mouseClick(w.findChild<QPushButton *>("applySelectionProperties"), Qt::LeftButton);
        QVERIFY2(wait([&] { return !c->outstanding() && c->state().phase == Phase::PreviewReady; }),
                 qPrintable(c->state().message));
        QCOMPARE(c->state().revision, revision);
        QVERIFY(part(*c).value("sketches").toArray().empty());
        w.resize(1280, 800);
        QTest::qWait(30);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen({.006, 0}).toPoint());
        auto *dimension = button(w, "sketch.dimension");
        QVERIFY(dimension && dimension->isEnabled());
        QTimer::singleShot(30, [] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (dialog) {
                dialog->setTextValue("8 mm");
                dialog->accept();
            }
        });
        QTest::mouseClick(dimension, Qt::LeftButton);
        QVERIFY2(wait([&] { return !c->outstanding() && c->state().phase == Phase::PreviewReady; }),
                 qPrintable(c->state().message));
        QCOMPARE(c->state().revision, revision);
        // A driving radius blocks free dragging and leaves a visible explanation after release.
        auto radiusPixel = view->screen({firstWorld.x + .008, firstWorld.y}).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, radiusPixel);
        QMouseEvent resize(QEvent::MouseMove, radiusPixel + QPoint(20, 0),
                           view->mapToGlobal(radiusPixel + QPoint(20, 0)), Qt::NoButton,
                           Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view, &resize);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, radiusPixel + QPoint(20, 0));
        auto *context = w.findChild<QLabel *>("sketchContext");
        QVERIFY(context->text().contains("Controlled by Radius"));
        QTest::qWait(30);
        QVERIFY(context->visibleRegion().boundingRect().height() >=
                context->heightForWidth(context->width()));
        QVERIFY(!w.findChild<QWidget *>("numericSketchInput")->isVisible());
        QVERIFY(!w.findChild<QWidget *>("advancedSketchInput")->isVisible());
        if (native) {
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!folder.isEmpty());
            QVERIFY(w.grab().save(folder + "/workspace-editing.png"));
        }
        auto *finish = w.findChild<QPushButton *>("acceptSketchButton");
        QVERIFY(finish->visibleRegion().boundingRect().height() > 0);
        QTest::mouseClick(finish, Qt::LeftButton);
        QVERIFY(wait([&] { return !c->outstanding() && c->state().revision == revision + 1; }));
        QVERIFY(!panel->active());
        auto sketches = sketch::parse(part(*c).value("sketches").toArray());
        QCOMPARE(sketches.size(), size_t(1));
        QCOMPARE(sketches[0].entities.size(), size_t(10));
        QVERIFY(std::abs(sketches[0].entities[0].point.x - firstWorld.x) < 1e-12 &&
                std::abs(sketches[0].entities[0].point.y - firstWorld.y) < 1e-12);
        auto circleId = sketches[0].entities[1].id;
        QVERIFY(std::abs(sketches[0].entity(circleId).radius - .008) < 1e-10);
        auto *rootItem = tree->topLevelItem(0);
        rootItem->setExpanded(false);
        c->projectCommand("RenameDocument", sketch::fields({{"name", "Bracket"}}));
        QVERIFY(wait([&] { return !c->outstanding() && c->state().revision == revision + 2; }));
        QVERIFY(!tree->topLevelItem(0)->isExpanded());
        tree->topLevelItem(0)->setExpanded(true);
        c->createDocument("Assembly", "Assembly");
        QVERIFY(wait([&] { return !c->outstanding() && c->state().documentRecords.size() == 2; }));
        QVERIFY(!w.command("sketch.tool.Circle")->isEnabled());
        QVERIFY(!w.command("sketch")->isEnabled());
        QVERIFY(tabs->isTabEnabled(3));
        QVERIFY(w.command("assembly")->isEnabled());
        c->switchDocument(partId);
        QVERIFY(wait([&] { return !c->outstanding() && c->state().activeDocument == partId; }));
        if (native) {
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(w.grab().save(folder + "/workspace-model.png"));
        }
        auto saved = files.filePath("workspace.cadproj");
        c->saveProject(saved);
        QVERIFY(wait(
            [&] { return !c->outstanding() && c->state().revision == c->state().savedRevision; }));
        w.saveLayout();
        if (native) {
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QFile f(folder + "/workspace.json");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(
                        QJsonObject{{"platform", QGuiApplication::platformName()},
                                    {"DPR", w.devicePixelRatioF()},
                                    {"left_property_manager", true},
                                    {"command_tabs", 3},
                                    {"resize_while_drawing", true},
                                    {"dimensions_via_shared_action", true},
                                    {"physical_trackpad", "Blocked: application events only"}})
                        .toJson());
        }
        w.close();
        QTemporaryDir other(QDir::tempPath() + "/cad-module3-layout-XXXXXX");
        QVERIFY(other.isValid());
        mark(other.path());
        MainWindow restored(prefs, nullptr, other.path());
        restored.show();
        QVERIFY(wait([&] { return restored.workerClient()->ready(); }));
        QCOMPARE(restored.dockWidgetArea(restored.findChild<QDockWidget *>("modelDock")),
                 Qt::LeftDockWidgetArea);
        restored.close();
    }
};
int main(int argc, char **argv) {
    QSurfaceFormat f;
    f.setVersion(3, 3);
    f.setProfile(QSurfaceFormat::CoreProfile);
    f.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    Workspace test;
    return QTest::qExec(&test, argc, argv);
}
#include "workspace.moc"
