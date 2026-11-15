#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/Viewport.hpp"
#include "sketch/Constructions.hpp"
#include "sketch/Curves.hpp"
#include "sketch/Slots.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
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
        qFatal("Cannot mark disposable root");
    f.write("disposable\n");
}
QCborArray sketches(Client *c) {
    for (auto v : c->state().documentRecords)
        if (v.toMap().value("type").toString() == "Part")
            return v.toMap().value("sketches").toArray();
    return {};
}
void click(MainWindow &w, QString id) {
    auto *b = w.findChild<QPushButton *>(id);
    if (!b)
        qFatal("Missing button");
    for (auto *p = b->parentWidget(); p; p = p->parentWidget()) {
        auto toggle = p->property("disclosureToggle").toString();
        if (!toggle.isEmpty()) {
            auto *t = w.findChild<QToolButton *>(toggle);
            if (!t->isChecked())
                QTest::mouseClick(t, Qt::LeftButton);
        }
    }
    for (auto *s : w.findChildren<QScrollArea *>())
        if (s->isAncestorOf(b))
            s->ensureWidgetVisible(b);
    QTest::qWait(10);
    QTest::mouseClick(b, Qt::LeftButton);
}
} // namespace
class Contracts : public QObject {
    Q_OBJECT
  private slots:
    void pointerPrimitives() {
        primitivePointer(false);
    }
    void nativePointerPrimitives() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        primitivePointer(true);
    }
    void pierceTools() {
        pierceWorkflow(false);
    }
    void nativePierceTools() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        pierceWorkflow(true);
    }
    void curveTools() {
        curveWorkflows(false);
    }
    void nativeCurveTools() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        curveWorkflows(true);
    }
    void slotTools() {
        slotWorkflows(false);
    }
    void nativeSlotTools() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        slotWorkflows(true);
    }
    void rectangles() {
        rectangleWorkflows(false);
        rectanglePointer(false);
    }
    void nativeRectangles() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        rectangleWorkflows(true);
        rectanglePointer(true);
    }
    void pointRelations() {
        exercise(false);
        additional(false);
    }
    void nativeRelations() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        exercise(true);
        additional(true);
    }

  private:
    void primitivePointer(bool native) {
        for (QString kind : {"Slot", "Ellipse", "EllipticalArc", "Parabola", "Spline"})
            for (QString method : kind == "Slot" ? sketch::slotMethods() : QStringList{QString()})
                for (bool drag : {false, true}) {
                    qInfo() << "Pointer primitive" << kind << method << "drag" << drag;
                    QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-pointer-XXXXXX"),
                        files(QDir::tempPath() + "/cad-module3-cm1-pointer-files-XXXXXX");
                    QVERIFY(root.isValid() && files.isValid());
                    mark(root.path());
                    mark(files.path());
                    auto prefs = files.filePath("settings.ini");
                    QSettings settings(prefs, QSettings::IniFormat);
                    settings.setValue("startup/offerRecovery", false);
                    settings.sync();
                    MainWindow w(prefs, nullptr, root.path());
                    w.show();
                    if (native) {
                        QVERIFY(QTest::qWaitForWindowExposed(&w));
                        w.raise();
                        w.activateWindow();
                        QVERIFY(QTest::qWaitForWindowActive(&w));
                        QVERIFY(std::abs(w.devicePixelRatioF() -
                                         qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) <
                                .01);
                    }
                    auto *c = w.workerClient();
                    QVERIFY2(wait([&] { return c->ready(); }), qPrintable(c->state().message));
                    c->newProject("Part", "Pointer primitives");
                    QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
                    w.command("sketch")->trigger();
                    w.command("sketch.tool." + kind)->trigger();
                    auto *view = w.findChild<SketchView *>();
                    if (kind == "Slot")
                        w.findChild<QComboBox *>("sketchSlotMethod")->setCurrentText(method);
                    QTest::qWait(30);
                    std::vector<sketch::Vec2> points;
                    if (kind == "Slot") {
                        if (method == "Straight" || method == "Centerpoint Straight")
                            points = {{0, 0}, {.03, 0}, {.015, .005}};
                        else if (method == "3 Point Arc")
                            points = {{.02, 0}, {0, .02}, {.0141421356, .0141421356}, {.024, 0}};
                        else
                            points = {{0, 0}, {.02, 0}, {0, .02}, {.024, 0}};
                    } else if (kind == "Ellipse")
                        points = {{0, 0}, {.025, 0}, {0, .008}};
                    else if (kind == "EllipticalArc")
                        points = {{0, 0}, {.025, 0}, {0, .008}, {.025, 0}, {0, .008}};
                    else if (kind == "Parabola")
                        points = {{.006, 0}, {0, 0}, {0, -.015}, {0, .015}};
                    else
                        points = {{-.02, 0}, {-.01, .02}, {.01, .02}, {.02, 0}};
                    auto revision = c->state().revision;
                    size_t at = 0;
                    if (drag) {
                        auto a = view->screen(points[0]).toPoint(),
                             b = view->screen(points[1]).toPoint();
                        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, a);
                        QMouseEvent move(QEvent::MouseMove, b, view->mapToGlobal(b), Qt::NoButton,
                                         Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(view, &move);
                        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, b);
                        at = 2;
                    }
                    for (; at < points.size(); ++at) {
                        auto p = view->screen(points[at]).toPoint();
                        QTest::mouseMove(view, p);
                        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, p);
                    }
                    if (kind == "Spline") {
                        view->setFocus();
                        QTest::keyClick(view, Qt::Key_Return);
                    }
                    auto *list = w.findChild<QListWidget *>("sketchEntities");
                    qInfo() << "Creation status" << w.findChild<QLabel *>("sketchContext")->text()
                            << view->accessibleDescription();
                    QCOMPARE(list->count(), kind == "Slot" ? (sketch::curvedSlot(method) ? 8 : 11)
                                            : kind == "Spline" ? 5
                                                               : 3);
                    w.command("undo")->trigger();
                    QCOMPARE(list->count(), 0);
                    w.command("redo")->trigger();
                    QVERIFY(list->count() > 0);
                    click(w, "previewSketchButton");
                    QVERIFY2(wait([&] {
                                 return !c->outstanding() &&
                                        c->state().phase == Phase::PreviewReady;
                             }),
                             qPrintable(c->state().message));
                    QCOMPARE(c->state().revision, revision);
                    QVERIFY(sketches(c).empty());
                    click(w, "acceptSketchButton");
                    QVERIFY2(
                        wait([&] { return !c->outstanding() && c->state().revision > revision; }),
                        qPrintable(c->state().message));
                    auto s = sketch::parse(sketches(c))[0];
                    if (kind == "Slot")
                        QCOMPARE(s.groups[0].method, method);
                    else
                        QCOMPARE(s.entities.back().kind, kind);
                }
    }
    void pierceWorkflow(bool native) {
        for (QString kind : {"Line", "Circle"}) {
            QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-pierce-ui-XXXXXX"),
                files(QDir::tempPath() + "/cad-module3-cm1-pierce-ui-files-XXXXXX");
            QVERIFY(root.isValid() && files.isValid());
            mark(root.path());
            mark(files.path());
            auto prefs = files.filePath("settings.ini");
            QSettings settings(prefs, QSettings::IniFormat);
            settings.setValue("startup/offerRecovery", false);
            settings.sync();
            MainWindow w(prefs, nullptr, root.path());
            w.show();
            if (native) {
                QVERIFY(QTest::qWaitForWindowExposed(&w));
                w.raise();
                w.activateWindow();
                QVERIFY(QTest::qWaitForWindowActive(&w));
                QVERIFY(std::abs(w.devicePixelRatioF() -
                                 qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
            }
            auto *c = w.workerClient();
            QVERIFY2(wait([&] { return c->ready(); }), qPrintable(c->state().message));
            c->newProject("Part", "Pierce dependency");
            QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
            sketch::Sketch source, target;
            source.name = "Crossing source";
            target.name = "Target";
            target.frame.u = {1, 0, 0};
            target.frame.v = {0, 0, 1};
            auto curve = kind == "Line"
                             ? source.line(source.point({.02, -.02}), source.point({.02, .02}))
                             : source.circle(source.point({0, 0}), .02);
            auto point = target.point({.01, 0});
            target.line(point, target.point({.04, .02}));
            c->projectCommand("SetSketches",
                              sketch::fields({{"sketches", sketch::encode({source, target})}}));
            QVERIFY2(wait([&] { return !c->outstanding() && sketches(c).size() == 2; }),
                     qPrintable(c->state().message));
            auto before = sketches(c);
            w.findChild<QComboBox *>("sketchChoice")->setCurrentIndex(1);
            click(w, "editSketchButton");
            auto *entities = w.findChild<QListWidget *>("sketchEntities");
            entities->clearSelection();
            for (int i = 0; i < entities->count(); ++i)
                if (entities->item(i)->data(Qt::UserRole).toString() == point)
                    entities->item(i)->setSelected(true);
            auto *relation = w.findChild<QComboBox *>("sketchRelation");
            relation->setCurrentText("Pierce");
            auto *sources = w.findChild<QComboBox *>("pierceSource");
            int index = -1;
            for (int i = 0; i < sources->count(); ++i)
                if (sources->itemData(i).toMap().value("entity").toString() == curve)
                    index = i;
            QVERIFY(index >= 0);
            sources->setCurrentIndex(index);
            click(w, "addSketchConstraint");
            click(w, "previewSketchButton");
            QVERIFY(wait([&] { return !c->outstanding(); }));
            QCOMPARE(sketches(c), before);
            if (kind == "Circle") {
                QCOMPARE(c->state().phase, Phase::Failed);
                QVERIFY(c->state().message.contains("ReferenceAmbiguous"));
                auto *branches = w.findChild<QComboBox *>("pierceBranch");
                QCOMPARE(branches->count(), 2);
                branches->setCurrentIndex(0);
                QVERIFY(QMetaObject::invokeMethod(branches, "activated", Q_ARG(int, 0)));
                click(w, "repairPierceReference");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                auto rev = c->state().revision;
                w.findChild<QLineEdit *>("pierceLower")->setText("1");
                w.findChild<QLineEdit *>("pierceUpper")->setText("2");
                click(w, "repairPierceReference");
                QVERIFY(
                    wait([&] { return !c->outstanding() && c->state().phase == Phase::Failed; }));
                QVERIFY(c->state().message.contains("ReferenceMissing"));
                QCOMPARE(c->state().revision, rev);
                QCOMPARE(branches->count(), 2);
                branches->setCurrentIndex(0);
                QVERIFY(QMetaObject::invokeMethod(branches, "activated", Q_ARG(int, 0)));
                click(w, "repairPierceReference");
            }
            QVERIFY2(
                wait([&] { return !c->outstanding() && c->state().phase == Phase::PreviewReady; }),
                qPrintable(c->state().message));
            if (native) {
                auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
                QVERIFY(!out.isEmpty());
                QVERIFY(w.grab().save(out + "/cm1-pierce-" + kind.toLower() + ".png"));
            }
            auto rev = c->state().revision;
            click(w, "acceptSketchButton");
            QVERIFY2(wait([&] { return !c->outstanding() && c->state().revision > rev; }),
                     qPrintable(c->state().message));
            auto accepted = sketches(c);
            auto model = sketch::parse(accepted);
            QCOMPARE(model[1].constraints.size(), size_t(1));
            QCOMPARE(model[1].constraints[0].reference->entity, curve);
            QVERIFY(std::abs(model[1].entity(point).point.x - .02) < 1e-10);
            w.command("undo")->trigger();
            QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == before; }));
            w.command("redo")->trigger();
            QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
            auto path = files.filePath("pierce.cadproj");
            c->saveProject(path);
            QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(path); }));
            c->openProject(path, "Saved", true);
            QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
            c->restart();
            QVERIFY(wait([&] { return c->ready(); }));
            c->openProject(path, "Saved");
            QVERIFY2(wait([&] { return !c->outstanding() && sketches(c) == accepted; }),
                     qPrintable(c->state().message));
        }
    }
    void curveWorkflows(bool native) {
        for (QString units : {"mm", "in"})
            for (QString kind : {"Ellipse", "EllipticalArc", "Parabola", "Spline"}) {
                qInfo() << "Curve workflow" << kind << units;
                QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-curve-ui-XXXXXX"),
                    files(QDir::tempPath() + "/cad-module3-cm1-curve-ui-files-XXXXXX");
                QVERIFY(root.isValid() && files.isValid());
                mark(root.path());
                mark(files.path());
                auto prefs = files.filePath("settings.ini");
                QSettings settings(prefs, QSettings::IniFormat);
                settings.setValue("startup/offerRecovery", false);
                settings.sync();
                MainWindow w(prefs, nullptr, root.path());
                w.show();
                if (native) {
                    QVERIFY(QTest::qWaitForWindowExposed(&w));
                    w.raise();
                    w.activateWindow();
                    QVERIFY(QTest::qWaitForWindowActive(&w));
                    QVERIFY(std::abs(w.devicePixelRatioF() -
                                     qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
                }
                auto *c = w.workerClient();
                QVERIFY(wait([&] { return c->ready(); }));
                c->newProject("Part", "Exact curve workflow");
                QVERIFY(wait([&] { return !c->outstanding() && c->state().projectOpen; }));
                c->projectCommand("SetDocumentUnits", sketch::fields({{"units", units}}));
                QVERIFY(wait([&] { return !c->outstanding(); }));
                w.command("sketch")->trigger();
                auto action = w.command("sketch.tool." + kind);
                QVERIFY(action && action->isEnabled());
                action->trigger();
                auto *view = w.findChild<SketchView *>();
                double unit = domain::toSI(1, "length", units);
                auto coordinate = [&](sketch::Vec2 p) {
                    w.findChild<QLineEdit *>("sketchX")->setText(
                        QString::number(p.x / unit, 'g', 17));
                    w.findChild<QLineEdit *>("sketchY")->setText(
                        QString::number(p.y / unit, 'g', 17));
                    click(w, "addSketchCoordinate");
                };
                std::vector<sketch::Vec2> points;
                if (kind == "Ellipse")
                    points = {{0, 0}, {.03, 0}, {0, .01}};
                else if (kind == "EllipticalArc")
                    points = {{0, 0}, {.03, 0}, {0, .01}, {.03, 0}, {-.03, 0}};
                else if (kind == "Parabola")
                    points = {{.01, 0}, {0, 0}, {0, -.02}, {0, .02}};
                else
                    points = {{0, 0}, {.01, .02}, {.02, .02}, {.03, 0}};
                auto revision = c->state().revision;
                coordinate(points[0]);
                click(w, "previewSketchButton");
                QCOMPARE(c->state().revision, revision);
                QVERIFY(sketches(c).empty());
                view->setFocus();
                QTest::keyClick(view, Qt::Key_Escape);
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), 0);
                for (auto p : points)
                    coordinate(p);
                if (kind == "Spline") {
                    view->setFocus();
                    QTest::keyClick(view, Qt::Key_Return);
                }
                int count = kind == "Spline" ? 5 : 3;
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), count);
                w.command("undo")->trigger();
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), 0);
                w.command("redo")->trigger();
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), count);
                click(w, "previewSketchButton");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                QVERIFY(sketches(c).empty());
                click(w, "acceptSketchButton");
                QVERIFY(wait([&] { return !c->outstanding() && c->state().revision > revision; }));
                auto accepted = sketches(c);
                auto before = sketch::parse(accepted)[0];
                auto curve = before.entities.back();
                QCOMPARE(curve.kind, kind);
                QCOMPARE(before.map().value("version").toInteger(), 4);
                click(w, "editSketchButton");
                auto *list = w.findChild<QListWidget *>("sketchEntities");
                list->clearSelection();
                list->item(list->count() - 1)->setSelected(true);
                if (kind == "Spline") {
                    auto *weights = w.findChild<QLineEdit *>("selectedCurveWeights");
                    QVERIFY(weights->isVisible());
                    weights->setText("1 -1 1 1");
                    auto op = c->latestOperation();
                    click(w, "applySelectionProperties");
                    QCOMPARE(c->latestOperation(), op);
                    QCOMPARE(sketches(c), accepted);
                    weights->setText("1 1.1 1.1 1");
                    auto *controls = w.findChild<QPlainTextEdit *>("selectedCurveControls");
                    auto rows = controls->toPlainText().split('\n');
                    rows[1] = QString::number(.01 / unit, 'g', 17) + " " +
                              QString::number(.024 / unit, 'g', 17);
                    controls->setPlainText(rows.join("\n"));
                } else {
                    auto *width = w.findChild<QLineEdit *>("selectedWidth");
                    QVERIFY(width->isVisible());
                    width->setText(
                        QString::number((kind == "Parabola" ? .012 : .035) / unit, 'g', 17));
                    auto *rotation = w.findChild<QLineEdit *>("selectedRectangleRotation");
                    QVERIFY(rotation->isVisible());
                    rotation->setText("15");
                }
                click(w, "applySelectionProperties");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                if (native) {
                    w.command("fit")->trigger();
                    QTest::qWait(20);
                    QVERIFY(w.grab().save(qEnvironmentVariable("CAD_ARTIFACT_DIR") + "/cm1-curve-" +
                                          kind + "-" + units + ".png"));
                }
                revision = c->state().revision;
                click(w, "acceptSketchButton");
                QVERIFY(wait([&] { return !c->outstanding() && c->state().revision > revision; }));
                auto changed = sketches(c);
                auto after = sketch::parse(changed)[0];
                QCOMPARE(after.entities.back().id, curve.id);
                if (kind == "Spline") {
                    QCOMPARE(after.entities.back().weights[1], 1.1);
                    QVERIFY(std::abs(after.entity(after.entities.back().poles[1]).point.y - .024) <
                            1e-11);
                }
                QCOMPARE(sketch::controlPoints(after.entities.back()),
                         sketch::controlPoints(curve));
                w.command("undo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
                w.command("redo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == changed; }));
                auto path = files.filePath("curves.cadproj");
                c->saveProject(path);
                QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(path); }));
                c->restart();
                QVERIFY(wait(
                    [&] { return c->ready() && !c->outstanding() && sketches(c) == changed; }));
                c->openProject(path, "Saved", true);
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == changed; }));
            }
    }
    void slotWorkflows(bool native) {
        for (QString units : {"mm", "in"})
            for (auto method : sketch::slotMethods()) {
                QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-slot-ui-XXXXXX"),
                    files(QDir::tempPath() + "/cad-module3-cm1-slot-ui-files-XXXXXX");
                QVERIFY(root.isValid() && files.isValid());
                mark(root.path());
                mark(files.path());
                auto prefs = files.filePath("settings.ini");
                QSettings settings(prefs, QSettings::IniFormat);
                settings.setValue("startup/offerRecovery", false);
                settings.sync();
                MainWindow w(prefs, nullptr, root.path());
                w.show();
                if (native) {
                    QVERIFY(QTest::qWaitForWindowExposed(&w));
                    w.raise();
                    w.activateWindow();
                    QVERIFY(QTest::qWaitForWindowActive(&w));
                    QVERIFY(std::abs(w.devicePixelRatioF() -
                                     qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
                }
                auto *c = w.workerClient();
                QVERIFY(wait([&] { return c->ready(); }));
                c->newProject("Part", "Slot workflow");
                QVERIFY(wait([&] { return !c->outstanding() && c->state().projectOpen; }));
                c->projectCommand("SetDocumentUnits", sketch::fields({{"units", units}}));
                QVERIFY(wait([&] { return !c->outstanding(); }));
                w.command("sketch")->trigger();
                auto action = w.command("sketch.tool.Slot");
                QVERIFY(action && action->isEnabled());
                action->trigger();
                w.findChild<QComboBox *>("sketchSlotMethod")->setCurrentText(method);
                w.findChild<QComboBox *>("sketchSlotLengthMode")->setCurrentText("Overall Length");
                w.findChild<QCheckBox *>("sketchSlotAddDimensions")->setChecked(units == "in");
                auto *view = w.findChild<SketchView *>();
                auto *panel = w.findChild<SketchPanel *>();
                auto coordinate = [&](sketch::Vec2 p) {
                    double unit = domain::toSI(1, "length", units);
                    w.findChild<QLineEdit *>("sketchX")->setText(
                        QString::number(p.x / unit, 'g', 15));
                    w.findChild<QLineEdit *>("sketchY")->setText(
                        QString::number(p.y / unit, 'g', 15));
                    click(w, "addSketchCoordinate");
                };
                std::vector<sketch::Vec2> inputs;
                if (method == "Straight" || method == "Centerpoint Straight")
                    inputs = {{0, 0}, {.03, 0}, {.015, .005}};
                else if (method == "3 Point Arc")
                    inputs = {
                        {.03, 0}, {0, .03}, {.0212132034355964, .0212132034355964}, {.035, 0}};
                else
                    inputs = {{0, 0}, {.03, 0}, {0, .03}, {.035, 0}};
                auto revision = c->state().revision;
                coordinate(inputs[0]);
                click(w, "previewSketchButton");
                QCOMPARE(c->state().revision, revision);
                QVERIFY(sketches(c).empty());
                view->setFocus();
                QTest::keyClick(view, Qt::Key_Escape);
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), 0);
                for (auto p : inputs)
                    coordinate(p);
                int count = sketch::curvedSlot(method) ? 8 : 11;
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), count);
                w.command("undo")->trigger();
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), 0);
                w.command("redo")->trigger();
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), count);
                click(w, "previewSketchButton");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                QVERIFY(sketches(c).empty());
                click(w, "acceptSketchButton");
                QVERIFY(wait([&] { return !c->outstanding() && c->state().revision > revision; }));
                auto accepted = sketches(c);
                auto before = sketch::parse(accepted)[0];
                auto group = before.groups[0];
                QCOMPARE(group.kind, QString("Slot"));
                QCOMPARE(group.method, method);
                QCOMPARE(group.lengthMode, QString("Overall Length"));
                QCOMPARE(before.map().value("version").toInteger(), 4);
                click(w, "editSketchButton");
                auto *list = w.findChild<QListWidget *>("sketchEntities");
                list->clearSelection();
                for (int i = 0; i < list->count(); ++i)
                    if (list->item(i)->data(Qt::UserRole).toString() == group.edges[0])
                        list->item(i)->setSelected(true);
                auto parameters = sketch::slotParameters(before, group);
                double unit = domain::toSI(1, "length", units);
                auto targetWidth = parameters.width * .8;
                w.findChild<QLineEdit *>("selectedHeight")
                    ->setText(QString::number(targetWidth / unit, 'g', 15));
                w.findChild<QLineEdit *>("selectedRectangleRotation")
                    ->setText(
                        QString::number((parameters.rotation + .1) * 180 / sketch::arcPi, 'g', 15));
                click(w, "applySelectionProperties");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                if (native) {
                    w.command("fit")->trigger();
                    QTest::qWait(20);
                    auto name = method;
                    name.replace(' ', '-');
                    QVERIFY(w.grab().save(qEnvironmentVariable("CAD_ARTIFACT_DIR") + "/cm1-slot-" +
                                          name + "-" + units + ".png"));
                }
                revision = c->state().revision;
                click(w, "acceptSketchButton");
                QVERIFY(wait([&] { return !c->outstanding() && c->state().revision > revision; }));
                auto changed = sketches(c);
                auto after = sketch::parse(changed)[0];
                QVERIFY(std::abs(sketch::slotParameters(after, after.groups[0]).width -
                                 targetWidth) < 1e-8);
                QCOMPARE(after.groups[0].edges, group.edges);
                QCOMPARE(after.groups[0].supports, group.supports);
                QVERIFY(after.groups[0].dimensions);
                w.command("undo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
                w.command("redo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == changed; }));
                auto path = files.filePath("slot.cadproj");
                c->saveProject(path);
                QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(path); }));
                c->restart();
                QVERIFY(wait(
                    [&] { return c->ready() && !c->outstanding() && sketches(c) == changed; }));
                c->openProject(path, "Saved", true);
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == changed; }));
                Q_UNUSED(panel);
            }
    }
    void rectangleWorkflows(bool native) {
        for (QString units : {"mm", "in"})
            for (auto method : sketch::rectangleMethods()) {
                QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-rect-ui-XXXXXX"),
                    files(QDir::tempPath() + "/cad-module3-cm1-rect-ui-files-XXXXXX");
                QVERIFY(root.isValid() && files.isValid());
                mark(root.path());
                mark(files.path());
                auto prefs = files.filePath("settings.ini");
                QSettings settings(prefs, QSettings::IniFormat);
                settings.setValue("startup/offerRecovery", false);
                settings.sync();
                MainWindow w(prefs, nullptr, root.path());
                w.show();
                if (native) {
                    QVERIFY(QTest::qWaitForWindowExposed(&w));
                    w.raise();
                    w.activateWindow();
                    QVERIFY(QTest::qWaitForWindowActive(&w));
                    QVERIFY(std::abs(w.devicePixelRatioF() -
                                     qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
                }
                auto *c = w.workerClient();
                QVERIFY(wait([&] { return c->ready(); }));
                c->newProject("Part", "Rectangle workflows");
                QVERIFY(wait([&] { return !c->outstanding() && c->state().projectOpen; }));
                c->projectCommand("SetDocumentUnits", sketch::fields({{"units", units}}));
                QVERIFY(wait([&] { return !c->outstanding(); }));
                w.command("sketch")->trigger();
                w.findChild<QComboBox *>("sketchTool")->setCurrentText("Rectangle");
                w.findChild<QComboBox *>("sketchRectangleMethod")->setCurrentText(method);
                QCOMPARE(w.findChild<SketchPanel *>()->toolName(), QString("Rectangle"));
                double unit = domain::toSI(1, "length", units);
                auto coordinate = [&](sketch::Vec2 p) {
                    w.findChild<QLineEdit *>("sketchX")->setText(
                        QString::number(p.x / unit, 'g', 15));
                    w.findChild<QLineEdit *>("sketchY")->setText(
                        QString::number(p.y / unit, 'g', 15));
                    click(w, "addSketchCoordinate");
                };
                std::vector<sketch::Vec2> p =
                    method == "Corner" || method == "Center"
                        ? std::vector<sketch::Vec2>{{.01, .01}, {.03, .025}}
                        : std::vector<sketch::Vec2>{{.01, .01}, {.034, .028}, {.0266, .0412}};
                auto rev = c->state().revision;
                coordinate(p[0]);
                click(w, "previewSketchButton");
                QCOMPARE(c->state().revision, rev);
                QVERIFY(sketches(c).empty());
                auto *view = w.findChild<SketchView *>();
                view->setFocus();
                QTest::keyClick(view, Qt::Key_Escape);
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), 0);
                for (auto v : p)
                    coordinate(v);
                int count = method.contains("Center") ? 11 : 8;
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), count);
                w.command("undo")->trigger();
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), 0);
                w.command("redo")->trigger();
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(), count);
                click(w, "previewSketchButton");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                QVERIFY(sketches(c).empty());
                click(w, "acceptSketchButton");
                QVERIFY2(wait([&] { return !c->outstanding() && c->state().revision > rev; }),
                         qPrintable(c->state().message));
                auto accepted = sketches(c);
                auto original = sketch::parse(accepted)[0];
                QCOMPARE(original.groups.size(), size_t(1));
                QCOMPARE(original.groups[0].method, method);
                QCOMPARE(original.map().value("version").toInteger(), 3);
                auto group = original.groups[0];
                w.findChild<QComboBox *>("sketchChoice")->setCurrentIndex(0);
                click(w, "editSketchButton");
                auto *list = w.findChild<QListWidget *>("sketchEntities");
                list->clearSelection();
                for (int i = 0; i < list->count(); ++i)
                    if (list->item(i)->data(Qt::UserRole).toString() == group.edges[0])
                        list->item(i)->setSelected(true);
                auto old = sketch::rectangleParameters(original, group);
                auto shapeCount = w.findChild<QListWidget *>("sketchConstraints")->count();
                w.findChild<QListWidget *>("sketchConstraints")->setCurrentRow(0);
                click(w, "deleteSketchConstraint");
                QCOMPARE(w.findChild<QListWidget *>("sketchConstraints")->count(), shapeCount);
                double targetRotation = old.rotation, targetSideAngle = old.sideAngle;
                if (method.contains("3 Point") || method == "Parallelogram") {
                    targetRotation += .1;
                    w.findChild<QLineEdit *>("selectedRectangleRotation")
                        ->setText(
                            QString::number(targetRotation * 180 / 3.141592653589793, 'g', 15));
                }
                if (method == "Parallelogram") {
                    targetSideAngle += .05;
                    w.findChild<QLineEdit *>("selectedRectangleSideAngle")
                        ->setText(
                            QString::number(targetSideAngle * 180 / 3.141592653589793, 'g', 15));
                }
                w.findChild<QLineEdit *>("selectedWidth")
                    ->setText(QString::number(old.width * 1.25 / unit, 'g', 15));
                w.findChild<QLineEdit *>("selectedHeight")
                    ->setText(QString::number(old.side * .8 / unit, 'g', 15));
                click(w, "applySelectionProperties");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                if (native || qEnvironmentVariableIsSet("CAD_CAPTURE_RECTANGLES")) {
                    w.command("fit")->trigger();
                    QTest::qWait(20);
                    auto name = method;
                    name.replace(' ', '-');
                    auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
                    QVERIFY(!directory.isEmpty());
                    QVERIFY(w.grab().save(directory + (native ? "/native-" : "/offscreen-") +
                                          "rectangle-" + name + "-" + units + "-preview.png"));
                }
                rev = c->state().revision;
                click(w, "acceptSketchButton");
                QVERIFY2(wait([&] { return !c->outstanding() && c->state().revision > rev; }),
                         qPrintable(c->state().message));
                auto changed = sketches(c);
                auto edited = sketch::parse(changed)[0];
                auto actual = sketch::rectangleParameters(edited, edited.groups[0]);
                QVERIFY(std::abs(actual.width - old.width * 1.25) < 1e-8);
                QVERIFY(std::abs(actual.side - old.side * .8) < 1e-8);
                QVERIFY(std::abs(actual.rotation - targetRotation) < 1e-8);
                QVERIFY(std::abs(actual.sideAngle - targetSideAngle) < 1e-8);
                QCOMPARE(edited.groups[0].map(), group.map());
                w.command("undo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
                w.command("redo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == changed; }));
                auto path = files.filePath("rectangle.cadproj");
                c->saveProject(path);
                QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(path); }));
                c->restart();
                QVERIFY2(
                    wait([&] { return c->ready() && !c->outstanding() && sketches(c) == changed; }),
                    qPrintable(c->state().message));
                c->openProject(path, "Saved", true);
                QVERIFY(wait([&] {
                    return !c->outstanding() &&
                           c->state().savedPath == QFileInfo(path).canonicalFilePath();
                }));
                QCOMPARE(sketches(c), changed);
            }
    }
    void rectanglePointer(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-rect-pointer-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-cm1-rect-pointer-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        auto prefs = files.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
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
        c->newProject("Part", "Pointer rectangles");
        QVERIFY(wait([&] { return !c->outstanding() && c->state().projectOpen; }));
        auto *view = w.findChild<SketchView *>();
        for (auto method : sketch::rectangleMethods())
            for (bool drag : {false, true}) {
                w.command("sketch")->trigger();
                w.findChild<QComboBox *>("sketchTool")->setCurrentText("Rectangle");
                w.findChild<QComboBox *>("sketchRectangleMethod")->setCurrentText(method);
                w.findChild<QCheckBox *>("sketchGrid")->setChecked(false);
                w.findChild<QCheckBox *>("sketchConstruction")->setChecked(drag);
                auto a = view->screen({.01, .01}).toPoint(),
                     b = view->screen({.03, .025}).toPoint(),
                     d = view->screen({.02, .045}).toPoint();
                auto move = [&](QPoint q) {
                    QMouseEvent e(QEvent::MouseMove, q, view->mapToGlobal(q), Qt::NoButton,
                                  Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(view, &e);
                };
                if (drag) {
                    QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, a);
                    move(b);
                    QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, b);
                } else {
                    QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, a);
                    QTest::mouseMove(view, b);
                    QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, b);
                }
                if (sketch::rectangleInputCount(method) == 3) {
                    if (drag) {
                        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, b);
                        move(d);
                        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, d);
                    } else {
                        QTest::mouseMove(view, d);
                        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, d);
                    }
                }
                QCOMPARE(w.findChild<QListWidget *>("sketchEntities")->count(),
                         method.contains("Center") ? 11 : 8);
                auto rev = c->state().revision;
                click(w, "acceptSketchButton");
                QVERIFY2(wait([&] { return !c->outstanding() && c->state().revision > rev; }),
                         qPrintable(c->state().message));
                auto models = sketch::parse(sketches(c));
                auto &last = models.back();
                for (auto id : last.groups[0].edges)
                    QCOMPARE(last.entity(id).construction, drag);
            }
    }
    void additional(bool native) {
        for (QString test : {"CoincidentLine", "CoincidentCircle", "Midpoint", "Collinear",
                             "Concentric", "Coradial", "Equal", "Intersection", "SymmetricPoint",
                             "SymmetricLine", "SymmetricArc", "SymmetricEllipse", "Merge",
                             "CurveTangent", "HorizontalMultiple", "CollinearMultiple"}) {
            QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-extra-XXXXXX"),
                files(QDir::tempPath() + "/cad-module3-cm1-extra-files-XXXXXX");
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
                auto expected = qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble();
                QVERIFY(expected >= 1 && expected <= 2);
                qInfo() << "Native relation DPR" << w.devicePixelRatioF() << "expected" << expected;
                QVERIFY(std::abs(w.devicePixelRatioF() - expected) < .01);
            }
            auto *c = w.workerClient();
            QVERIFY(wait([&] { return c->ready(); }));
            if (native)
                QVERIFY(graphics.empty());
            c->newProject("Part", "Additional relations");
            QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
            sketch::Sketch seed;
            QString first, second, third, kind = test;
            int expectedConstraints = 1;
            if (test.startsWith("Symmetric")) {
                kind = "Symmetric";
                if (test == "SymmetricPoint") {
                    first = seed.point({-.02, .01});
                    second = seed.point({.021, .012});
                }
                if (test == "SymmetricLine") {
                    first = seed.line(seed.point({-.03, .01}), seed.point({-.02, .02}));
                    second = seed.line(seed.point({.031, .011}), seed.point({.021, .022}));
                }
                if (test == "SymmetricArc") {
                    first = seed.arc(seed.point({-.03, 0}), .01, 0, sketch::arcPi);
                    second = seed.arc(seed.point({.031, .001}), .011, 0, sketch::arcPi);
                }
                if (test == "SymmetricEllipse") {
                    first = sketch::createEllipse(seed, {-.03, 0}, {-.01, 0}, {-.03, .01});
                    second = sketch::createEllipse(seed, {.031, .001}, {.011, .001}, {.031, .012});
                }
                third = seed.line(seed.point({0, -.04}, true), seed.point({0, .04}, true), true);
            } else if (test == "Intersection") {
                first = seed.point({.01, .01});
                second = seed.line(seed.point({0, 0}), seed.point({.04, 0}));
                third = seed.line(seed.point({.02, -.02}), seed.point({.02, .02}));
            } else if (test == "Merge") {
                first = seed.point({.01, .01});
                second = seed.point({.012, .013});
            } else if (test == "CurveTangent") {
                first = sketch::createSpline(seed, {{-.03, 0}, {-.01, .02}, {.01, .02}, {.03, 0}});
                second =
                    sketch::createSpline(seed, {{-.03, .03}, {-.01, .01}, {.01, .01}, {.03, .03}});
            } else if (test == "HorizontalMultiple") {
                kind = "Horizontal";
                expectedConstraints = 2;
                first = seed.point({0, 0});
                second = seed.point({.01, .002});
                third = seed.point({.02, .004});
            } else if (test == "CollinearMultiple") {
                kind = "Collinear";
                expectedConstraints = 2;
                first = seed.line(seed.point({0, 0}), seed.point({.01, 0}));
                second = seed.line(seed.point({.02, .001}), seed.point({.03, .001}));
                third = seed.line(seed.point({.04, .002}), seed.point({.05, .002}));
            } else if (test == "CoincidentLine" || test == "Midpoint" || test == "Collinear") {
                auto a = seed.point({.01, .01}), b = seed.point({.03, .01});
                second = seed.line(a, b);
                if (test == "Collinear") {
                    first = second;
                    second = seed.line(seed.point({.01, .03}), seed.point({.03, .04}));
                } else
                    first = seed.point({.02, .025});
                if (test == "CoincidentLine")
                    kind = "Coincident";
            } else if (test == "CoincidentCircle") {
                second = seed.circle(seed.point({.01, .01}), .005);
                first = seed.point({.013, .012});
                kind = "Coincident";
            } else {
                first = seed.circle(seed.point({.01, .01}), .005);
                second = seed.arc(seed.point({.04, .03}), .008, 0, 1.5);
            }
            c->projectCommand("SetSketches",
                              sketch::fields({{"sketches", sketch::encode({seed})}}));
            QVERIFY2(wait([&] { return !c->outstanding() && sketches(c).size() == 1; }),
                     qPrintable(c->state().message));
            auto before = sketches(c);
            w.findChild<QComboBox *>("sketchChoice")->setCurrentIndex(0);
            click(w, "editSketchButton");
            auto *entities = w.findChild<QListWidget *>("sketchEntities");
            entities->clearSelection();
            for (int i = 0; i < entities->count(); ++i) {
                auto id = entities->item(i)->data(Qt::UserRole).toString();
                if (id == first || id == second || id == third)
                    entities->item(i)->setSelected(true);
            }
            QCOMPARE(entities->selectedItems().size(), third.isEmpty() ? 2 : 3);
            auto *choice = w.findChild<QComboBox *>("sketchRelation");
            int relationIndex = choice->findData(kind);
            if (relationIndex >= 0)
                choice->setCurrentIndex(relationIndex);
            else
                choice->setCurrentText(kind);
            QVERIFY(w.findChild<QPushButton *>("addSketchConstraint")->isEnabled());
            click(w, "addSketchConstraint");
            click(w, "previewSketchButton");
            QVERIFY2(
                wait([&] { return !c->outstanding() && c->state().phase == Phase::PreviewReady; }),
                qPrintable(c->state().message));
            QCOMPARE(sketches(c), before);
            if (native) {
                auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
                QVERIFY(!out.isEmpty());
                QVERIFY(w.grab().save(out + "/cm1-" + test.toLower() + "-preview.png"));
            }
            auto revision = c->state().revision;
            click(w, "acceptSketchButton");
            QVERIFY2(wait([&] { return !c->outstanding() && c->state().revision > revision; }),
                     qPrintable(c->state().message));
            auto accepted = sketches(c);
            auto result = sketch::parse(accepted)[0];
            QCOMPARE(result.constraints.size(), size_t(expectedConstraints));
            QCOMPARE(result.constraints[0].kind, kind);
            QCOMPARE(result.id, seed.id);
            for (const auto &e : seed.entities)
                QCOMPARE(result.entity(e.id).kind, e.kind);
            w.command("undo")->trigger();
            QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == before; }));
            w.command("redo")->trigger();
            QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
            auto path = files.filePath("extra.cadproj");
            c->saveProject(path);
            QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(path); }));
            c->openProject(path, "Saved", true);
            QVERIFY2(wait([&] {
                         return !c->outstanding() &&
                                c->state().savedPath == QFileInfo(path).canonicalFilePath();
                     }),
                     qPrintable(c->state().message));
            QCOMPARE(sketches(c), accepted);
            if (native) {
                auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
                QVERIFY(!out.isEmpty());
                QVERIFY(w.grab().save(out + "/cm1-" + test.toLower() + ".png"));
            }
        }
    }
    void exercise(bool native) {
        for (const auto &units : {QString("mm"), QString("in")})
            for (QString kind : {"Horizontal", "Vertical"}) {
                QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-native-XXXXXX"),
                    files(QDir::tempPath() + "/cad-module3-cm1-native-files-XXXXXX");
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
                    auto expected = qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble();
                    QVERIFY(expected >= 1 && expected <= 2);
                    qInfo() << "Native point-pair DPR" << w.devicePixelRatioF() << "expected"
                            << expected;
                    QVERIFY(std::abs(w.devicePixelRatioF() - expected) < .01);
                }
                auto *c = w.workerClient();
                QVERIFY(wait([&] { return c->ready(); }));
                if (native)
                    QVERIFY(graphics.empty());
                c->newProject("Part", "Point relations");
                QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
                c->projectCommand("SetDocumentUnits", sketch::fields({{"units", units}}));
                QVERIFY(wait([&] { return !c->outstanding(); }));
                auto legacy = files.filePath("legacy.cadproj");
                c->saveProject(legacy);
                QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(legacy); }));
                QFile legacyFile(legacy);
                QVERIFY(legacyFile.open(QIODevice::ReadOnly));
                auto legacyBytes = legacyFile.readAll();
                legacyFile.close();
                w.command("sketch")->trigger();
                w.findChild<QComboBox *>("sketchTool")->setCurrentText("Point");
                for (auto p : {QPointF(10, 10), QPointF(30, 20)}) {
                    w.findChild<QLineEdit *>("sketchX")->setText(QString::number(p.x()));
                    w.findChild<QLineEdit *>("sketchY")->setText(QString::number(p.y()));
                    click(w, "addSketchCoordinate");
                }
                auto *list = w.findChild<QListWidget *>("sketchEntities");
                QCOMPARE(list->count(), 2);
                list->clearSelection();
                list->item(0)->setSelected(true);
                list->item(1)->setSelected(true);
                w.findChild<QComboBox *>("sketchRelation")->setCurrentText(kind);
                auto *add = w.findChild<QPushButton *>("addSketchConstraint");
                QVERIFY(add->isEnabled());
                click(w, "addSketchConstraint");
                auto rev = c->state().revision;
                click(w, "previewSketchButton");
                QVERIFY2(wait([&] {
                             return !c->outstanding() && c->state().phase == Phase::PreviewReady;
                         }),
                         qPrintable(c->state().message));
                QCOMPARE(c->state().revision, rev);
                QVERIFY(sketches(c).empty());
                click(w, "acceptSketchButton");
                QVERIFY2(wait([&] { return !c->outstanding() && c->state().revision > rev; }),
                         qPrintable(c->state().message));
                auto accepted = sketches(c);
                QCOMPARE(accepted.size(), 1);
                auto solved = sketch::parse(accepted)[0];
                QCOMPARE(solved.constraints.size(), size_t(1));
                QCOMPARE(solved.map().value("version").toInteger(), 2);
                auto a = solved.entities[0].point, b = solved.entities[1].point;
                QVERIFY(std::abs(kind == "Horizontal" ? a.y - b.y : a.x - b.x) < 1e-9);
                w.command("undo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c).empty(); }));
                w.command("redo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
                c->saveProject(legacy);
                QVERIFY(wait([&] { return !c->outstanding(); }));
                QVERIFY(c->state().message.startsWith("UpgradeCopyRequired:"));
                QVERIFY(c->state().message.contains("Save As"));
                QVERIFY(legacyFile.open(QIODevice::ReadOnly));
                QCOMPARE(legacyFile.readAll(), legacyBytes);
                legacyFile.close();
                auto path = files.filePath("relations.cadproj");
                c->saveProject(path, true);
                QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(path); }));
                c->restart();
                QVERIFY2(wait([&] {
                             return c->ready() && !c->outstanding() && sketches(c) == accepted;
                         }),
                         qPrintable(c->state().message));
                c->openProject(path, "Saved", true);
                QVERIFY2(wait([&] {
                             return !c->outstanding() &&
                                    c->state().savedPath == QFileInfo(path).canonicalFilePath();
                         }),
                         qPrintable(c->state().message));
                QCOMPARE(sketches(c), accepted);
                w.findChild<QComboBox *>("sketchChoice")->setCurrentIndex(0);
                click(w, "editSketchButton");
                rev = c->state().revision;
                click(w, "cancelSketchButton");
                QCOMPARE(c->state().revision, rev);
                QCOMPARE(sketches(c), accepted);
                if (native) {
                    auto folder = qEnvironmentVariable("CAD_ARTIFACT_DIR");
                    QVERIFY(!folder.isEmpty());
                    QVERIFY(
                        w.grab().save(folder + "/cm1-" + kind.toLower() + "-" + units + ".png"));
                }
            }
    }
};
int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("NativeCadTests");
    app.setApplicationName("CoreModeling");
    Contracts t;
    return QTest::qExec(&t, argc, argv);
}
#include "contracts.moc"
