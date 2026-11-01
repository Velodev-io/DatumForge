#include "../support/modal.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/Viewport.hpp"
#include "sketch/Dimensions.hpp"
#include <QDialog>
#include <QFile>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>
using namespace cad;
namespace {
bool wait(std::function<bool()> f) {
    QElapsedTimer timer;
    timer.start();
    while (!f() && timer.elapsed() < 12000)
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
void select(MainWindow &w, QStringList ids) {
    auto *list = w.findChild<QListWidget *>("sketchEntities");
    if (!list)
        qFatal("Missing sketch entities");
    list->clearSelection();
    for (auto id : ids)
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toString() == id)
                list->item(i)->setSelected(true);
}
} // namespace
class DimensionContracts : public QObject {
    Q_OBJECT
  private slots:
    void identity() {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm3-identity-XXXXXX");
        QTemporaryDir files(QDir::tempPath() + "/cad-module3-cm3-identity-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        auto prefs = root.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow w(prefs, nullptr, root.path());
        w.resize(1280, 800);
        w.show();
        auto *c = w.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        c->newProject("Part", "Dimension identity", "mm");
        QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
        auto *panel = w.findChild<SketchPanel *>();
        auto cleanup = qScopeGuard([&] {
            if (panel->active())
                panel->discard();
            c->closeProject();
            wait([&] { return !c->state().projectOpen && !c->outstanding(); });
        });
        sketch::Sketch s;
        auto line = s.line(s.point({0, 0}), s.point({.03, .04}));
        sketch::Constraint driver;
        driver.kind = "Length";
        driver.a = line;
        driver.value = .05;
        driver.expression = "50 mm";
        driver.placement = sketch::Vec2{.02, .06};
        auto readout = driver;
        readout.id = newId();
        readout.driving = false;
        readout.expression.clear();
        readout.placement = sketch::Vec2{.04, .08};
        s.constraints = {driver, readout};
        c->projectCommand("SetSketches", sketch::fields({{"sketches", sketch::encode({s})}}));
        QVERIFY(wait([&] { return !c->outstanding() && sketches(c).size() == 1; }));
        w.findChild<QPushButton *>("editSketchButton")->click();
        QVERIFY(panel->active());
        auto *canvas = panel->canvas();
        QVERIFY(wait([&] { return !canvas->dimensionLabelRect(readout.id).isEmpty(); }));
        bool applied = false;
        auto modalAction = test::whenModal(&w, [&](QDialog *dialog) {
            auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            QVERIFY(dialog->findChild<QCheckBox *>("dimensionReference")->isChecked());
            dialog->findChild<QLineEdit *>("dimensionPlacementX")->setText("70");
            dialog->findChild<QPushButton *>("dimensionPreview")->click();
            auto *apply = dialog->findChild<QPushButton *>("dimensionApply");
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            applied = true;
            apply->click();
        });
        QTest::mouseDClick(canvas, Qt::LeftButton, Qt::NoModifier,
                           canvas->dimensionLabelRect(readout.id).center().toPoint());
        QVERIFY(applied);
        QVERIFY(wait([&] { return !c->outstanding(); }));
        panel->finish();
        QVERIFY(wait([&] { return !c->outstanding() && !panel->active(); }));
        auto accepted = sketch::parse(sketches(c))[0];
        QCOMPARE(accepted.constraints.size(), size_t(2));
        for (auto &dimension : accepted.constraints) {
            if (dimension.id == driver.id) {
                QVERIFY2(dimension.driving, "Editing a readout must retain the original driver");
                QCOMPARE(dimension.expression, driver.expression);
                QCOMPARE(dimension.placement->x, .02);
            } else {
                QCOMPARE(dimension.id, readout.id);
                QVERIFY(!dimension.driving);
                QCOMPARE(dimension.placement->x, .07);
            }
        }
        auto confirmed = sketches(c);
        c->projectCommand("Undo");
        QVERIFY(wait([&] { return !c->outstanding(); }));
        auto undone = sketch::parse(sketches(c))[0];
        QCOMPARE(undone.constraints[0].id, driver.id);
        QVERIFY(undone.constraints[0].driving);
        QCOMPARE(undone.constraints[1].id, readout.id);
        QCOMPARE(undone.constraints[1].placement->x, .04);
        c->projectCommand("Redo");
        QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == confirmed; }));
        auto file = files.filePath("identity.cadproj");
        c->saveProject(file);
        QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(file); }));
        c->closeProject();
        QVERIFY(wait([&] { return !c->outstanding() && !c->state().projectOpen; }));
        c->openProject(file);
        QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == confirmed; }));
    }
    void widgets() {
        workflow(false);
    }
    void native() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native desktop required");
        workflow(true);
    }

  private:
    void workflow(bool native) {
        for (QString units : {"mm", "in"}) {
            QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm3-ui-XXXXXX"),
                files(QDir::tempPath() + "/cad-module3-cm3-ui-files-XXXXXX");
            QVERIFY(root.isValid() && files.isValid());
            mark(root.path());
            mark(files.path());
            auto prefs = files.filePath("settings.ini");
            QSettings settings(prefs, QSettings::IniFormat);
            settings.setValue("startup/offerRecovery", false);
            settings.sync();
            MainWindow w(prefs, nullptr, root.path());
            w.resize(1280, 800);
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
            c->newProject("Part", "Dimensions", units);
            QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
            auto *panel = w.findChild<SketchPanel *>();
            for (QString mode :
                 {"Length", "Distance", "HorizontalDistance", "VerticalDistance", "Angle", "Radius",
                  "Diameter", "Sweep", "CenterlineDiameter", "ArcLength", "HorizontalOrdinate",
                  "VerticalOrdinate", "CenterlineDiameterPoint", "AxisAngle"}) {
                bool axisAngle = mode == "AxisAngle";
                if (axisAngle)
                    mode = "Angle";
                bool pointDiameter = mode == "CenterlineDiameterPoint";
                if (pointDiameter)
                    mode = "CenterlineDiameter";
                qInfo() << "Dimension workflow" << mode << units << "point diameter"
                        << pointDiameter;
                sketch::Sketch s;
                auto a = s.point({0, 0}), b = s.point({.03, .04});
                auto line = s.line(a, b);
                auto other = s.line(a, s.point({.04, -.01}));
                auto circle = s.circle(s.point({.06, 0}), .005);
                auto arc = s.arc(s.point({.06, .02}), .005, 0, 1.2);
                auto axis = s.line(s.point({0, -.01}), s.point({0, .1}), true);
                auto profile = s.line(s.point({.03, 0}), s.point({.03, .04}));
                auto pointProfile = s.entity(profile).a;
                if (mode == "CenterlineDiameter") {
                    for (auto point : {s.entity(axis).a, s.entity(axis).b}) {
                        sketch::Constraint fixed;
                        fixed.kind = "Fixed";
                        fixed.a = point;
                        fixed.value = s.entity(point).point.x;
                        fixed.second = s.entity(point).point.y;
                        s.constraints.push_back(fixed);
                    }
                }
                c->projectCommand("SetSketches",
                                  sketch::fields({{"sketches", sketch::encode({s})}}));
                QVERIFY2(wait([&] { return !c->outstanding() && sketches(c).size() == 1; }),
                         qPrintable(c->state().message));
                auto original = sketches(c);
                auto revision = c->state().revision;
                auto *edit = w.findChild<QPushButton *>("editSketchButton");
                if (!panel->active())
                    edit->click();
                QVERIFY(panel->active());
                QStringList ids =
                    mode == "CenterlineDiameter" ? (pointDiameter ? QStringList{pointProfile, axis}
                                                                  : QStringList{axis, profile})
                    : mode == "Angle"
                        ? (axisAngle ? QStringList{axis, line} : QStringList{line, other})
                    : mode == "Distance" || sketch::componentDimension(mode)
                        ? QStringList{a, b}
                        : QStringList{mode == "Radius" || mode == "Diameter"   ? circle
                                      : mode == "Sweep" || mode == "ArcLength" ? arc
                                                                               : line};
                select(w, ids);
                QVERIFY(panel->dimensionAvailable());
                auto open = [&](std::function<void(QDialog *)> action, QString dimensionId = {}) {
                    bool opened = false;
                    auto modalAction = test::whenModal(&w, [&](QDialog *d) {
                        opened = true;
                        auto cleanup = qScopeGuard([&] {
                            if (d->isVisible())
                                d->reject();
                        });
                        if (axisAngle)
                            QCOMPARE(d->findChild<QComboBox *>("dimensionMode")
                                         ->currentData()
                                         .toString(),
                                     QString("Angle"));
                        action(d);
                    });
                    if (dimensionId.isEmpty()) {
                        QVERIFY(w.command("sketch.dimension")->isEnabled());
                        w.command("sketch.dimension")->trigger();
                    } else {
                        // Hit rectangles are published by paintEvent, not showSketch/fit.
                        // Native repaint scheduling can outlast a fixed qWait(20).
                        QVERIFY2(
                            wait([&] {
                                panel->canvas()->repaint();
                                return !panel->canvas()->dimensionLabelRect(dimensionId).isEmpty();
                            }),
                            qPrintable(QString("Dimension label not painted; canvas visible=%1 "
                                               "enabled=%2, dimension=%3")
                                           .arg(panel->canvas()->isVisible())
                                           .arg(panel->canvas()->isEnabled())
                                           .arg(dimensionId)));
                        auto rect = panel->canvas()->dimensionLabelRect(dimensionId);
                        QVERIFY(!rect.isEmpty());
                        QTest::mouseDClick(panel->canvas(), Qt::LeftButton, Qt::NoModifier,
                                           rect.center().toPoint());
                    }
                    QVERIFY2(opened, qPrintable(w.findChild<QLabel *>("sketchContext")->text()));
                };
                // Invalid units cannot be applied; cancel an in-flight valid preview.
                open([&](QDialog *d) {
                    auto *modes = d->findChild<QComboBox *>("dimensionMode");
                    modes->setCurrentIndex(modes->findData(mode));
                    auto *expression = d->findChild<QLineEdit *>("dimensionExpression");
                    auto *preview = d->findChild<QPushButton *>("dimensionPreview");
                    auto *apply = d->findChild<QPushButton *>("dimensionApply");
                    expression->setText(mode == "Angle" || mode == "Sweep" ? "10 mm" : "5 deg");
                    preview->click();
                    QVERIFY(wait([&] { return !c->outstanding(); }));
                    QVERIFY(!apply->isEnabled());
                    expression->setText(mode == "Angle" || mode == "Sweep" ? "60 deg" : "50.8 mm");
                    preview->click();
                    d->reject();
                });
                QVERIFY(wait([&] { return !c->outstanding(); }));
                QCOMPARE(sketches(c), original);
                QCOMPARE(c->state().revision, revision);
                // Valid preview is temporary. Restore discards it and disables Apply.
                open([&](QDialog *d) {
                    auto *modes = d->findChild<QComboBox *>("dimensionMode");
                    modes->setCurrentIndex(modes->findData(mode));
                    auto *expression = d->findChild<QLineEdit *>("dimensionExpression");
                    auto *preview = d->findChild<QPushButton *>("dimensionPreview");
                    auto *apply = d->findChild<QPushButton *>("dimensionApply");
                    expression->setText(mode == "Angle" || mode == "Sweep" ? "60 deg" : "50.8 mm");
                    preview->click();
                    QVERIFY2(wait([&] { return apply->isEnabled(); }),
                             qPrintable(d->findChild<QLabel *>("dimensionStatus")->text()));
                    QCOMPARE(c->state().revision, revision);
                    QCOMPARE(sketches(c), original);
                    d->findChild<QPushButton *>("dimensionRestore")->click();
                    QVERIFY(!apply->isEnabled());
                    expression->setText(mode == "Angle" || mode == "Sweep" ? "60 deg"
                                                                           : "2 * 25.4 mm");
                    preview->click();
                    QVERIFY(wait([&] { return apply->isEnabled(); }));
                    apply->click();
                });
                QVERIFY(wait([&] { return !c->outstanding(); }));
                QVERIFY2(panel->canUndoDraft(), qPrintable(mode + " " + units));
                panel->finish();
                QVERIFY2(wait([&] { return !c->outstanding() && !panel->active(); }),
                         qPrintable(c->state().message));
                auto accepted = sketch::parse(sketches(c))[0];
                QCOMPARE(accepted.constraints.size(), size_t(mode == "CenterlineDiameter" ? 3 : 1));
                auto dimension = accepted.constraints.back();
                QCOMPARE(dimension.kind, mode);
                if (sketch::associatedDimension(mode))
                    QCOMPARE(accepted.map().value("version").toInteger(), 7);
                if (mode == "CenterlineDiameter") {
                    QCOMPARE(dimension.a, pointDiameter ? pointProfile : profile);
                    QCOMPARE(dimension.b, axis);
                    QVERIFY(dimension.reversed);
                    auto &edge = accepted.entity(profile);
                    QVERIFY(std::abs(accepted.entity(edge.a).point.x - .0254) < 1e-8);
                    if (!pointDiameter)
                        QVERIFY(std::abs(accepted.entity(edge.b).point.x - .0254) < 1e-8);
                }
                if (mode == "HorizontalOrdinate" || mode == "VerticalOrdinate") {
                    QCOMPARE(dimension.a, a);
                    QCOMPARE(dimension.b, b);
                }
                QVERIFY(dimension.driving && dimension.placement.has_value());
                QVERIFY(std::abs(dimension.value - (mode == "Angle" || mode == "Sweep"
                                                        ? 3.141592653589793 / 3
                                                        : .0508)) < 1e-9);
                // Convert the same dimension to a reference, preserving its identity.
                edit->click();
                select(w, ids);
                w.command("fit")->trigger();
                QTest::qWait(20);
                open(
                    [&](QDialog *d) {
                        auto *modes = d->findChild<QComboBox *>("dimensionMode");
                        modes->setCurrentIndex(modes->findData(mode));
                        auto *reference = d->findChild<QCheckBox *>("dimensionReference");
                        reference->setChecked(true);
                        QVERIFY(!d->findChild<QLineEdit *>("dimensionExpression")->isEnabled());
                        d->findChild<QPushButton *>("dimensionPreview")->click();
                        auto *apply = d->findChild<QPushButton *>("dimensionApply");
                        QVERIFY(wait([&] { return apply->isEnabled(); }));
                        apply->click();
                    },
                    dimension.id);
                QVERIFY(wait([&] { return !c->outstanding(); }));
                panel->finish();
                QVERIFY2(wait([&] { return !c->outstanding() && !panel->active(); }),
                         qPrintable(c->state().message));
                auto reference = sketch::parse(sketches(c))[0].constraints.back();
                QCOMPARE(reference.id, dimension.id);
                QVERIFY(!reference.driving && reference.expression.isEmpty());
                if (native && sketch::associatedDimension(mode) &&
                    qEnvironmentVariableIsSet("CAD_ARTIFACT_DIR"))
                    QVERIFY(w.grab().save(qEnvironmentVariable("CAD_ARTIFACT_DIR") + "/cm3-" +
                                          mode + (pointDiameter ? "-point" : "") + "-" + units +
                                          ".png"));
                QVERIFY(std::abs(reference.value - sketch::measureDimension(accepted, dimension)) <
                        1e-9);
                if (pointDiameter) {
                    // A readout can cross its former axis side without moving the point.
                    auto moved = sketch::parse(sketches(c));
                    for (auto &constraint : moved[0].constraints)
                        if (constraint.kind == "Fixed")
                            constraint.value += .1;
                    c->projectCommand("SetSketches",
                                      sketch::fields({{"sketches", sketch::encode(moved)}}));
                    QVERIFY(wait([&] { return !c->outstanding(); }));
                    edit->click();
                    select(w, ids);
                    open([&](QDialog *d) {
                        auto *readout = d->findChild<QCheckBox *>("dimensionReference");
                        QVERIFY(readout->isChecked());
                        readout->setChecked(false);
                        d->findChild<QPushButton *>("dimensionPreview")->click();
                        auto *apply = d->findChild<QPushButton *>("dimensionApply");
                        QVERIFY(wait([&] { return apply->isEnabled(); }));
                        apply->click();
                    });
                    QVERIFY(wait([&] { return !c->outstanding(); }));
                    panel->finish();
                    QVERIFY(wait([&] { return !c->outstanding() && !panel->active(); }));
                    auto confirmed = sketch::parse(sketches(c))[0];
                    QVERIFY(confirmed.constraints.back().driving &&
                            !confirmed.constraints.back().reversed);
                    QVERIFY(std::abs(confirmed.entity(pointProfile).point.x - .0254) < 1e-8);
                }
                if (mode == "CenterlineDiameter") {
                    auto saved = sketches(c);
                    auto path = files.filePath(pointDiameter ? "point-diameter.cadproj"
                                                             : "line-diameter.cadproj");
                    c->saveProject(path);
                    QVERIFY(wait([&] { return !c->outstanding() && QFile::exists(path); }));
                    c->restart();
                    QVERIFY2(wait([&] {
                                 return c->ready() && !c->outstanding() && sketches(c) == saved;
                             }),
                             qPrintable(c->state().message));
                }
            }
            if (native && qEnvironmentVariableIsSet("CAD_ARTIFACT_DIR"))
                QVERIFY(w.grab().save(qEnvironmentVariable("CAD_ARTIFACT_DIR") +
                                      "/cm3-dimensions-" + units + ".png"));
            c->closeProject();
            QVERIFY(wait([&] { return !c->outstanding() && !c->state().projectOpen; }));
        }
    }
};
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    DimensionContracts test;
    return QTest::qExec(&test, argc, argv);
}
#include "dimension_contracts.moc"
