#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/SketchView.hpp"
#include "render/Viewport.hpp"
#include "sketch/Editing.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
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
void reveal(QWidget &w, QWidget *widget) {
    std::vector<QWidget *> sections;
    for (auto *p = widget->parentWidget(); p; p = p->parentWidget())
        if (p->property("disclosureToggle").isValid())
            sections.push_back(p);
    for (auto it = sections.rbegin(); it != sections.rend(); ++it) {
        auto *toggle = w.findChild<QToolButton *>((*it)->property("disclosureToggle").toString());
        if (toggle && !toggle->isChecked())
            QTest::mouseClick(toggle, Qt::LeftButton);
    }
}
void click(QWidget &w, QString name) {
    auto *b = w.findChild<QPushButton *>(name);
    if (!b)
        qFatal("Missing button");
    reveal(w, b);
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
    void arcGestures() {
        arcs(false, "mm");
        arcs(false, "in");
    }
    void allArcTypes() {
        moreArcs(false, "mm");
        moreArcs(false, "in");
    }
    void nativeArcs() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Blocked: native session required");
        arcs(true, "mm");
        arcs(true, "in");
        moreArcs(true, "mm");
        moreArcs(true, "in");
    }
    void shapeEditing() {
        editingWorkflow(false);
    }
    void nativeSketches() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Blocked: native session required");
        exercise(true);
        gestures(true, "mm");
        gestures(true, "in");
        editingWorkflow(true);
    }

  private:
    void moreArcs(bool native, QString units) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-all-arcs-XXXXXX"),
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
        c->newProject("Part", "All arc methods", units);
        QVERIFY(wait([&] { return ready(*c); }));
        auto revision = c->state().revision;
        window.command("sketch")->trigger();
        auto *panel = window.findChild<SketchPanel *>();
        auto *view = panel->canvas();
        auto *method = window.findChild<QComboBox *>("sketchArcMethod");
        auto *flyout = window.findChild<QToolButton *>("sketchArcFlyout");
        QVERIFY(method && flyout && flyout->menu());
        QCOMPARE(flyout->menu()->actions().size(), 3);
        auto clickAt = [&](sketch::Vec2 p) {
            QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen(p).toPoint());
        };
        auto move = [&](sketch::Vec2 p) {
            auto at = view->screen(p).toPoint();
            QMouseEvent e(QEvent::MouseMove, at, view->mapToGlobal(at), Qt::NoButton, Qt::NoButton,
                          Qt::NoModifier);
            QApplication::sendEvent(view, &e);
        };
        auto capture = [&](QString name) {
            if (native) {
                QTest::qWait(20);
                QVERIFY(window.grab().save(qEnvironmentVariable("CAD_ARTIFACT_DIR") + "/" + name +
                                           "-" + units + ".png"));
            }
        };
        method->setCurrentIndex(1);
        QCOMPARE(panel->toolName(), QString("3 Point Arc"));
        QCOMPARE(flyout->defaultAction(), window.command("sketch.tool.3 Point Arc"));
        QTest::qWait(30);
        clickAt({.03, .02});
        clickAt({.05, .02});
        move({.04, .03});
        capture("three-point-live");
        clickAt({.04, .03});
        window.command("sketch.tool.Line")->trigger();
        QTest::qWait(20);
        clickAt({-.02, 0});
        clickAt({0, 0});
        QTest::keyClick(view, Qt::Key_Escape);
        window.command("sketch.tool.Tangent Arc")->trigger();
        QTest::qWait(20);
        clickAt({0, 0});
        move({.003, 0});
        move({.01, .01});
        capture("tangent-live");
        clickAt({.01, .01});
        clickAt({-.02, 0});
        move({-.02, -.003});
        move({-.03, -.01});
        capture("normal-live");
        clickAt({-.03, -.01});
        clickAt({.05, .02});
        move({.05, .017});
        move({.06, .01});
        clickAt({.06, .01});
        QSignalSpy solved(c, &Client::sketchPreview);
        click(window, "previewSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && solved.size() == 1; }),
                 qPrintable(c->state().message));
        auto result = sketch::parse(solved.last()[0].value<QCborArray>());
        int tangents = 0, normals = 0;
        for (auto &join : result[0].constraints)
            if (join.kind == "ArcTangent" || join.kind == "ArcNormal") {
                join.kind == "ArcTangent" ? ++tangents : ++normals;
                QVERIFY(sketch::length(
                            sketch::curveEnd(result[0], result[0].entity(join.a), int(join.value)),
                            sketch::curveEnd(result[0], result[0].entity(join.b),
                                             int(join.second))) < 1e-8);
            }
        QCOMPARE(tangents, 2);
        QCOMPARE(normals, 1);
        QCOMPARE(c->state().revision, revision);
        click(window, "acceptSketchButton");
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == revision + 1; }));
        auto accepted = part(*c).value("sketches").toArray();
        auto path = files.filePath("all-arcs.cadproj");
        c->saveProject(path);
        QVERIFY(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }));
        c->closeProject();
        QVERIFY(wait([&] { return !c->state().projectOpen && !c->outstanding(); }));
        c->openProject(path);
        QVERIFY(wait([&] { return ready(*c); }));
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        c->newProject("Part", "Course normal-arc exercise", units, true);
        QVERIFY(wait([&] { return ready(*c); }));
        window.command("sketch")->trigger();
        window.command("sketch.tool.Line")->trigger();
        QTest::qWait(30);
        for (auto p :
             {sketch::Vec2{0, 0}, {.06, 0}, {.06, .035}, {.05, .035}, {.05, .025}, {.04, .025}})
            clickAt(p);
        move({.035, .025});
        move({.04, .025});
        QCOMPARE(panel->toolName(), QString("Tangent Arc"));
        QVERIFY(view->sizingArc());
        move({.04, .02});
        move({.02, .025});
        capture("course-normal-transition");
        clickAt({.02, .025});
        QCOMPARE(panel->toolName(), QString("Line"));
        for (auto p : {sketch::Vec2{.01, .025}, {.01, .035}, {0, .035}, {0, 0}})
            clickAt(p);
        QTest::keyClick(view, Qt::Key_Escape);
        solved.clear();
        click(window, "previewSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && solved.size() == 1; }),
                 qPrintable(c->state().message));
        result = sketch::parse(solved.last()[0].value<QCborArray>());
        int arcs = 0, lines = 0, links = 0;
        for (auto &e : result[0].entities) {
            if (e.kind == "Arc") {
                ++arcs;
                QVERIFY(std::abs(e.radius - .01) < 1e-8);
                QVERIFY(std::abs(e.sweep - sketch::arcPi) < 1e-8);
            }
            if (e.kind == "Line")
                ++lines;
        }
        for (auto &join : result[0].constraints)
            if (join.kind == "ArcNormal" || join.kind == "ArcCoincident")
                ++links;
        QCOMPARE(arcs, 1);
        QCOMPARE(lines, 9);
        QCOMPARE(links, 2);
        capture("course-solved");
        click(window, "acceptSketchButton");
        QVERIFY(wait([&] { return ready(*c) && !panel->active(); }));
        accepted = part(*c).value("sketches").toArray();
        c->saveProject(files.filePath("course.cadproj"));
        QVERIFY(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }));
        if (native)
            QVERIFY2(graphics.empty(), "Native graphics failure");
        window.close();
    }
    void arcs(bool native, QString units) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-arcs-XXXXXX"),
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
        c->newProject("Part", "Centerpoint arc", units);
        QVERIFY(wait([&] { return ready(*c); }));
        auto rev = c->state().revision;
        window.command("sketch")->trigger();
        window.command("sketch.tool.Arc")->trigger();
        auto *panel = window.findChild<SketchPanel *>();
        auto *view = panel->canvas();
        auto *entities = window.findChild<QListWidget *>("sketchEntities");
        auto clickAt = [&](sketch::Vec2 p) {
            QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, view->screen(p).toPoint());
        };
        auto move = [&](sketch::Vec2 p) {
            auto at = view->screen(p).toPoint();
            QMouseEvent event(QEvent::MouseMove, at, view->mapToGlobal(at), Qt::NoButton,
                              Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(view, &event);
        };
        QTest::qWait(30);
        clickAt({0, 0});
        QCOMPARE(entities->count(), 0);
        click(window, "previewSketchButton");
        click(window, "acceptSketchButton");
        QVERIFY(view->sizingArc());
        QVERIFY(panel->active());
        QCOMPARE(c->outstanding(), 0);
        QCOMPARE(c->state().revision, rev);
        move({.005, 0});
        move({.01, 0});
        if (native) {
            QTest::qWait(30);
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!folder.isEmpty());
            QVERIFY(window.grab().save(folder + "/centerpoint-arc-radius-" + units + ".png"));
        }
        clickAt({.01, 0});
        QSignalSpy rows(entities->model(), &QAbstractItemModel::rowsInserted);
        for (int i = 1; i <= 60; ++i) {
            double theta = i * 3.141592653589793 / 120;
            move({.01 * std::cos(theta), .01 * std::sin(theta)});
        }
        QCOMPARE(rows.size(), 0);
        QCOMPARE(c->outstanding(), 0);
        QCOMPARE(entities->count(), 0);
        if (native) {
            QTest::qWait(30);
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!folder.isEmpty());
            QVERIFY(window.grab().save(folder + "/centerpoint-arc-live-" + units + ".png"));
            QVERIFY2(graphics.empty(), "Native renderer failed");
        }
        clickAt({0, .01});
        QCOMPARE(entities->count(), 2);
        QCOMPARE(c->state().revision, rev);
        panel->undoDraftEdit();
        QCOMPARE(entities->count(), 0);
        panel->redoDraftEdit();
        QCOMPARE(entities->count(), 2);
        clickAt({.03, 0});
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(!view->sizingArc());
        QCOMPARE(entities->count(), 2);
        QCOMPARE(panel->toolName(), QString("Arc"));
        clickAt({.03, 0});
        window.command("sketch.tool.Line")->trigger();
        QVERIFY(!view->sizingArc());
        QCOMPARE(entities->count(), 2);
        QSignalSpy solved(c, &Client::sketchPreview);
        click(window, "previewSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && solved.size() == 1; }),
                 qPrintable(c->state().message));
        auto result = sketch::parse(solved.last()[0].value<QCborArray>());
        QCOMPARE(result[0].entities.size(), size_t(2));
        auto arc = result[0].entities[1];
        QCOMPARE(arc.kind, QString("Arc"));
        QVERIFY(std::abs(arc.radius - .01) < 1e-10);
        QVERIFY(std::abs(arc.start) < 1e-10);
        QVERIFY(std::abs(arc.sweep - 3.141592653589793 / 2) < 1e-10);
        click(window, "acceptSketchButton");
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == rev + 1; }));
        auto accepted = part(*c).value("sketches").toArray();
        window.command("undo")->trigger();
        QVERIFY(wait([&] { return ready(*c) && part(*c).value("sketches").toArray().empty(); }));
        window.command("redo")->trigger();
        QVERIFY(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == accepted; }));
        auto path = files.filePath("arc.cadproj");
        c->saveProject(path);
        QVERIFY(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }));
        c->closeProject();
        QVERIFY(wait([&] { return !c->state().projectOpen && !c->outstanding(); }));
        c->openProject(path);
        QVERIFY(wait([&] { return ready(*c); }));
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        window.close();
    }
    void editingWorkflow(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-edit-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        auto prefs = files.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow window(prefs, nullptr, root.path());
        window.show();
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.raise();
            window.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&window));
        }
        auto *c = window.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        c->newProject("Part", "Editable shapes");
        QVERIFY(wait([&] { return ready(*c); }));
        window.command("sketch")->trigger();
        auto *panel = window.findChild<SketchPanel *>();
        auto *view = window.findChild<SketchView *>();
        auto *tool = window.findChild<QComboBox *>("sketchTool");
        tool->setCurrentText("Circle");
        window.findChild<QLineEdit *>("sketchRadius")->setText("5");
        coordinate(window, 0, 0);
        tool->setCurrentText("Rectangle");
        coordinate(window, -20, -10);
        coordinate(window, 20, 10);
        auto revision = c->state().revision;
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(panel->active());
        QCOMPARE(tool->currentText(), QString("Select / drag"));
        window.command("undo")->trigger();
        QCOMPARE(window.findChild<QListWidget *>("sketchEntities")->count(), 2);
        window.command("redo")->trigger();
        QCOMPARE(window.findChild<QListWidget *>("sketchEntities")->count(), 10);
        QCOMPARE(c->state().revision, revision);
        click(window, "acceptSketchButton");
        QVERIFY2(wait([&] { return ready(*c) && c->state().revision == revision + 1; }),
                 qPrintable(c->state().message));
        auto accepted = part(*c).value("sketches").toArray();
        click(window, "editSketchButton");
        window.command("fit")->trigger();
        QTest::qWait(30);
        auto circle = sketch::parse(accepted)[0].entities[1];
        auto center = sketch::parse(accepted)[0].entities[0].point;
        QPoint start = view->screen({center.x + circle.radius, center.y}).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, start);
        auto *list = window.findChild<QListWidget *>("sketchEntities");
        QSignalSpy removed(list->model(), &QAbstractItemModel::rowsRemoved),
            inserted(list->model(), &QAbstractItemModel::rowsInserted);
        auto operation = c->latestOperation();
        QSignalSpy dragPreviews(c, &Client::sketchPreview);
        int maximumPending = 0;
        std::vector<double> timings;
        for (int i = 1; i <= 60; ++i) {
            QPoint at = start + QPoint(i, 0);
            QElapsedTimer timer;
            timer.start();
            QMouseEvent move(QEvent::MouseMove, at, view->mapToGlobal(at), Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view, &move);
            QApplication::processEvents();
            maximumPending = std::max(maximumPending, c->outstanding());
            QVERIFY2(c->outstanding() <= 2,
                     "Drag previews must coalesce to one running and one pending request");
            timings.push_back(timer.nsecsElapsed() / 1e6);
        }
        QCOMPARE(removed.size(), 0);
        QCOMPARE(inserted.size(), 0);
        QVERIFY2(c->latestOperation() != operation, "Constrained dragging is solved by the worker");
        QCOMPARE(c->state().revision, revision + 1);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, start + QPoint(60, 0));
        QVERIFY(wait([&] { return !c->outstanding(); }));
        QVERIFY(!dragPreviews.empty());
        int workerDragResults = dragPreviews.size();
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        QVERIFY(panel->active());
        auto *radius = window.findChild<QLineEdit *>("selectedRadius");
        QVERIFY(!radius->isHidden());
        radius->setText("7.25");
        click(window, "applySelectionProperties");
        QVERIFY2(wait([&] { return !c->outstanding() && c->state().phase == Phase::PreviewReady; }),
                 qPrintable(c->state().message));
        QCOMPARE(part(*c).value("sketches").toArray(), accepted);
        click(window, "acceptSketchButton");
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == revision + 2; }));
        auto sized = part(*c).value("sketches").toArray();
        auto semantics = sketch::parse(sized);
        QCOMPARE(semantics[0].entity(circle.id).radius, .00725);
        window.command("undo")->trigger();
        QVERIFY(
            wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == accepted; }));
        window.command("redo")->trigger();
        QVERIFY(wait([&] { return ready(*c) && part(*c).value("sketches").toArray() == sized; }));
        click(window, "editSketchButton");
        auto corner = semantics[0].entities[4].id;
        auto *entities = window.findChild<QListWidget *>("sketchEntities");
        entities->clearSelection();
        for (int i = 0; i < entities->count(); ++i)
            if (entities->item(i)->data(Qt::UserRole).toString() == corner)
                entities->item(i)->setSelected(true);
        window.findChild<QLineEdit *>("selectedWidth")->setText("60");
        window.findChild<QLineEdit *>("selectedHeight")->setText("30");
        click(window, "applySelectionProperties");
        QVERIFY2(wait([&] { return !c->outstanding() && c->state().phase == Phase::PreviewReady; }),
                 qPrintable(c->state().message));
        click(window, "acceptSketchButton");
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == revision + 5; }));
        auto resized = part(*c).value("sketches").toArray();
        semantics = sketch::parse(resized);
        auto rect = sketch::rectangleOf(semantics[0], corner);
        QVERIFY(rect.has_value());
        double left = 1e100, right = -1e100, bottom = 1e100, top = -1e100;
        for (auto &id : rect->points) {
            auto p = semantics[0].entity(id).point;
            left = std::min(left, p.x);
            right = std::max(right, p.x);
            bottom = std::min(bottom, p.y);
            top = std::max(top, p.y);
        }
        QVERIFY(std::abs(right - left - .06) < 1e-10 && std::abs(top - bottom - .03) < 1e-10);
        click(window, "editSketchButton");
        entities->clearSelection();
        for (int i = 0; i < entities->count(); ++i)
            if (entities->item(i)->data(Qt::UserRole).toString() == circle.id)
                entities->item(i)->setSelected(true);
        click(window, "deleteSelectedEntity");
        QCOMPARE(entities->count(), 8);
        window.command("undo")->trigger();
        QCOMPARE(entities->count(), 10);
        window.command("redo")->trigger();
        QCOMPARE(entities->count(), 8);
        click(window, "cancelSketchButton");
        QCOMPARE(part(*c).value("sketches").toArray(), resized);
        auto path = files.filePath("edits.cadproj");
        c->saveProject(path);
        QVERIFY(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }));
        c->closeProject();
        QVERIFY(wait([&] { return !c->state().projectOpen && !c->outstanding(); }));
        c->openProject(path);
        QVERIFY(wait([&] { return ready(*c); }));
        QCOMPARE(part(*c).value("sketches").toArray(), resized);
        if (native) {
            auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!folder.isEmpty());
            std::sort(timings.begin(), timings.end());
            QFile f(folder + "/input-latency.json");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(
                        QJsonObject{
                            {"samples", int(timings.size())},
                            {"p95_event_to_paint_ms", timings[size_t(timings.size() * .95)]},
                            {"max_ms", timings.back()},
                            {"list_rebuilds_during_drag", 0},
                            {"maximum_pending_drag_requests", maximumPending},
                            {"worker_preview_results_during_drag", workerDragResults},
                            {"source", "Application-dispatched mouse events plus event processing; "
                                       "not physical trackpad latency or SRS acceptance"}})
                        .toJson());
            QVERIFY(window.grab().save(folder + "/editable-shapes.png"));
        }
        window.close();
    }

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
                    if ((color.blue() > 140 && color.blue() > color.red() * 1.3 &&
                         color.blue() > color.green() * 1.1) ||
                        (color.red() > 160 && color.green() > 90 &&
                         color.red() > color.blue() * 1.5))
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
        // Escape leaves the tool and preserves the complete edit. Explicit Cancel discards it.
        view->setFocus();
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(window.findChild<SketchPanel *>()->active());
        click(window, "cancelSketchButton");
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
        click(window, "cancelSketchButton");
        QVERIFY(!window.findChild<SketchPanel *>()->active());
        QVERIFY(wait([&] { return !c->outstanding(); }));
        QVERIFY(c->state().phase != Phase::PreviewReady);
        QVERIFY2(window.command("sketch")->isEnabled(),
                 "Canceled preview completed but idle sketch actions did not refresh");
        QVERIFY(part(*c).value("sketches").toArray().empty());
        for (auto box :
             {std::array<double, 4>{0, 0, 80, 50}, std::array<double, 4>{20, 20, 40, 40}}) {
            window.command("sketch")->trigger();
            window.findChild<QComboBox *>("sketchTool")->setCurrentText("Rectangle");
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
        QTest::qWait(20);
        point = view->screen({0, 0}).toPoint();
        auto target = view->local(point + QPoint(30, -10));
        QSignalSpy previews(c, &Client::sketchPreview);
        QMouseEvent move(QEvent::MouseMove, point + QPoint(30, -10),
                         view->mapToGlobal(point + QPoint(30, -10)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(view, &move);
        QVERIFY(wait([&] { return !c->outstanding() && !previews.empty(); }));
        QCOMPARE(view->pick(view->screen(target)), draggedId);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, point + QPoint(30, -10));
        QVERIFY(wait([&] { return !c->outstanding(); }));
        click(window, "cancelSketchButton");
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
            // Rectangle creation already adds the four orientation relations.
            QCOMPARE(window.findChild<QListWidget *>("sketchConstraints")->count(), 5);
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
        click(window, "cancelSketchButton");
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
            click(window, "editSketchButton");
            for (auto id : {"previewSketchButton", "acceptSketchButton", "cancelSketchButton"}) {
                auto *button = window.findChild<QPushButton *>(id);
                QVERIFY(button->visibleRegion().boundingRect().height() > 0);
            }
            click(window, "cancelSketchButton");
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
