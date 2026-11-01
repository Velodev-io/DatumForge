#include "../support/modal.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "sketch/Constructions.hpp"
#include "sketch/Curves.hpp"
#include "sketch/Dimensions.hpp"
#include "sketch/Slots.hpp"
#include <QDialog>
#include <QFile>
#include <QScopeGuard>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>
using namespace cad;
namespace {
bool wait(std::function<bool()> predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 12000)
        QTest::qWait(5);
    return predicate();
}
struct Session {
    QTemporaryDir root{QDir::tempPath() + "/cad-module3-cm3-completion-ui-XXXXXX"};
    std::unique_ptr<MainWindow> window;
    Client *client = nullptr;
    SketchPanel *panel = nullptr;
    bool start(QString units = "mm") {
        auto failed = [&](QString stage) {
            qWarning() << "CM3 fixture setup failed:" << stage << "platform"
                       << QGuiApplication::platformName() << "DPR"
                       << (window ? window->devicePixelRatioF() : 0) << "expected"
                       << qEnvironmentVariable("CAD_EXPECTED_DPR")
                       << (client ? client->state().message : QString());
            return false;
        };
        if (!root.isValid())
            return failed("disposable root");
        QFile marker(root.filePath(".cad-test-root"));
        if (!marker.open(QIODevice::WriteOnly))
            return failed("test marker");
        marker.write("disposable\n");
        marker.close();
        auto prefs = root.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        window = std::make_unique<MainWindow>(prefs, nullptr, root.path());
        window->resize(1280, 800);
        window->show();
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TESTS")) {
            if (!QTest::qWaitForWindowExposed(window.get()))
                return failed("native window exposure");
            window->raise();
            window->activateWindow();
            if (!QTest::qWaitForWindowActive(window.get()))
                return failed("native window activation");
            if (std::abs(window->devicePixelRatioF() -
                         qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) > .01)
                return failed("native DPR");
        }
        client = window->workerClient();
        panel = window->findChild<SketchPanel *>();
        if (!wait([&] { return client->ready(); }))
            return failed("worker handshake");
        client->newProject("Part", "CM3 completion", units);
        if (!wait([&] { return client->state().projectOpen && !client->outstanding(); }))
            return failed("project creation");
        return true;
    }
    QCborArray sketches() const {
        if (!client)
            return {};
        for (auto v : client->state().documentRecords)
            if (v.toMap().value("type").toString() == "Part")
                return v.toMap().value("sketches").toArray();
        return {};
    }
    bool seed(const sketch::Sketch &s) {
        if (!client || !window || !panel)
            return false;
        client->projectCommand("SetSketches", sketch::fields({{"sketches", sketch::encode({s})}}));
        if (!wait([&] { return !client->outstanding() && sketches().size() == 1; }))
            return false;
        window->findChild<QPushButton *>("editSketchButton")->click();
        return panel->active();
    }
    bool dimension(QString id, std::function<void(QDialog *)> action) {
        if (!client || !window || !panel)
            return false;
        auto *canvas = panel->canvas();
        if (!wait([&] { return !canvas->dimensionLabelRect(id).isEmpty(); }))
            return false;
        bool opened = false;
        auto modalAction = test::whenModal(window.get(), [&](QDialog *dialog) {
            opened = true;
            auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            action(dialog);
            QVERIFY(snapshot(QString::fromLatin1(QTest::currentTestFunction()) + "-preview"));
        });
        QTest::mouseDClick(canvas, Qt::LeftButton, Qt::NoModifier,
                           canvas->dimensionLabelRect(id).center().toPoint());
        return opened;
    }
    bool finish() {
        if (!client || !panel)
            return false;
        if (!wait([&] { return !client->outstanding(); }))
            return false;
        panel->finish();
        return wait([&] { return !client->outstanding() && !panel->active(); });
    }
    bool snapshot(QString name, QWidget *widget = nullptr) const {
        if (!window)
            return false;
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS") ||
            !qEnvironmentVariableIsSet("CAD_ARTIFACT_DIR"))
            return true;
        auto tag = QString::fromLatin1(QTest::currentDataTag());
        return (widget ? widget : window.get())
            ->grab()
            .save(qEnvironmentVariable("CAD_ARTIFACT_DIR") + "/" + name +
                  (tag.isEmpty() ? "" : "-" + tag) + ".png");
    }
    ~Session() {
        if (!client)
            return;
        if (panel && panel->active())
            panel->discard();
        client->closeProject();
        wait([&] { return !client->outstanding() && !client->state().projectOpen; });
    }
};
} // namespace
class CompletionContracts : public QObject {
    Q_OBJECT
  private slots:
    void delayedModalReadiness_data() {
        QTest::addColumn<bool>("nestedWait");
        QTest::newRow("ready") << false;
        QTest::newRow("nested") << true;
    }
    void delayedModalReadiness() {
        QFETCH(bool, nestedWait);
        QWidget parent;
        parent.resize(640, 480);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
        QDialog dialog(&parent);
        bool called = false;
        auto timer = test::whenModal(&dialog, [&](QDialog *active) {
            QCOMPARE(active, &dialog);
            called = true;
            active->accept();
            if (nestedWait)
                QTest::qWait(25);
        });
        dialog.setModal(true);
        dialog.show();
        QTest::qWait(25); // Native show processing can precede exec's event loop.
        QVERIFY(!called);
        QTimer::singleShot(2000, &dialog, &QDialog::reject);
        QCOMPARE(dialog.exec(), int(QDialog::Accepted));
        QVERIFY(called);
    }
    void proposalRestart_data() {
        QTest::addColumn<QString>("editor");
        for (QString editor : {"fullyDefine", "infer", "dimension", "ordinates"})
            QTest::newRow(qPrintable(editor)) << editor;
    }
    void proposalRestart() {
        QFETCH(QString, editor);
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, .00005}), line = s.line(a, b);
        if (editor == "dimension") {
            sketch::Constraint c;
            c.kind = "Length";
            c.a = line;
            c.value = std::hypot(.03, .00005);
            s.constraints.push_back(c);
        }
        QVERIFY(session.seed(s));
        auto *entities = session.window->findChild<QListWidget *>("sketchEntities");
        for (int i = 0; i < entities->count(); ++i) {
            auto id = entities->item(i)->data(Qt::UserRole).toString();
            entities->item(i)->setSelected(editor == "ordinates" ? (id == a || id == b)
                                                                 : id == line);
        }
        auto original = session.sketches();
        bool opened = false;
        auto modalAction = test::whenModal(session.window.get(), [&](QDialog *dialog) {
            opened = true;
            auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            auto prefix = editor == "dimension"   ? QString("dimension")
                          : editor == "ordinates" ? QString("ordinate")
                                                  : QString("proposal");
            auto *calculate = dialog->findChild<QPushButton *>(
                prefix == "proposal" ? "proposalCalculate" : prefix + "Preview");
            auto *apply = dialog->findChild<QPushButton *>(prefix + "Apply");
            QVERIFY(calculate && apply);
            calculate->click();
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            session.client->restart();
            QVERIFY(wait([&] {
                return session.client->ready() && !session.client->outstanding() &&
                       session.sketches() == original;
            }));
            QVERIFY2(!dialog->isVisible(), "An old-epoch proposal editor must close before it can "
                                           "recalculate a discarded draft");
        });
        session.window->command("sketch." + editor)->trigger();
        QVERIFY(opened);
        QVERIFY(!session.panel->active());
        QVERIFY(!session.panel->canUndoDraft());
        QCOMPARE(session.sketches(), original);
    }
    void dragRestart() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, 0}), c = s.point({.03, .04});
        auto first = s.line(a, b), second = s.line(b, c);
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = first;
        path.members = {first, second};
        path.directions = {false, false};
        path.value = .07;
        path.expression = "70 mm";
        path.increment = .001;
        path.drawing = true;
        s.constraints.push_back(path);
        QVERIFY(session.seed(s));
        auto original = session.sketches();
        auto revision = session.client->state().revision;
        auto *canvas = session.panel->canvas();
        QSignalSpy previews(session.client, &Client::sketchPreview);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                          canvas->screen({.03, 0}).toPoint());
        auto at = canvas->screen({.04, .01});
        QMouseEvent move(QEvent::MouseMove, at, canvas->mapToGlobal(at), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        QVERIFY(wait([&] { return !previews.empty() && !session.client->outstanding(); }));
        auto proposed = previews.last()[0].value<QCborArray>();
        QVERIFY(proposed != original);
        session.client->restart();
        QVERIFY2(wait([&] {
                     return session.client->ready() && !session.client->outstanding() &&
                            session.sketches() == original;
                 }),
                 qPrintable(session.client->state().message));
        QCOMPARE(session.client->state().revision, revision);
        QVERIFY(!session.panel->active());
        QVERIFY(!session.panel->canUndoDraft());
    }
    void inferenceRecheckTangent() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto curve = sketch::createSpline(s, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}});
        auto p = sketch::curveValue(s, s.entity(curve), .37),
             t = sketch::curveDerivative(s, s.entity(curve), .37);
        double n = std::hypot(t.x, t.y);
        t = {t.x / n, t.y / n};
        p = {p.x - .00001 * t.y, p.y + .00001 * t.x};
        auto line = s.line(s.point({p.x - .02 * t.x, p.y - .02 * t.y}),
                           s.point({p.x + .02 * t.x, p.y + .02 * t.y}));
        QVERIFY(session.seed(s));
        auto *entities = session.window->findChild<QListWidget *>("sketchEntities");
        for (int i = 0; i < entities->count(); ++i)
            entities->item(i)->setSelected(entities->item(i)->data(Qt::UserRole).toString() ==
                                           line);
        bool opened = false;
        auto modalAction = test::whenModal(session.window.get(), [&](QDialog *dialog) {
            opened = true;
            auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            auto *apply = dialog->findChild<QPushButton *>("proposalApply");
            dialog->findChild<QPushButton *>("proposalCalculate")->click();
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            auto *list = dialog->findChild<QListWidget *>("proposalRelations");
            QCOMPARE(list->count(), 1);
            QVERIFY(list->item(0)->text().contains("Tangent"));
            list->item(0)->setCheckState(Qt::Unchecked);
            QVERIFY(!apply->isEnabled());
            list->item(0)->setCheckState(Qt::Checked);
            dialog->findChild<QPushButton *>("proposalPreview")->click();
            QVERIFY2(wait([&] { return apply->isEnabled(); }),
                     qPrintable(dialog->findChild<QLabel *>("proposalStatus")->text()));
            apply->click();
        });
        session.window->command("sketch.infer")->trigger();
        QVERIFY(opened);
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QCOMPARE(result.constraints.size(), size_t(1));
        QCOMPARE(result.constraints[0].kind, QString("CurveTangent"));
        auto &c = result.constraints[0];
        QVERIFY(sketch::length(sketch::curveValue(result, result.entity(c.a), c.value),
                               sketch::curveValue(result, result.entity(c.b), c.second)) < 1e-8);
    }
    void fullyDefineReview_data() {
        QTest::addColumn<QString>("units");
        QTest::newRow("mm") << QString("mm");
        QTest::newRow("in") << QString("in");
    }
    void fullyDefineReview() {
        QFETCH(QString, units);
        Session session;
        QVERIFY(session.start(units));
        sketch::Sketch s;
        auto line = s.line(s.point({.01, .02}), s.point({.05, .03}));
        s.circle(s.point({.1, .02}), .01);
        QVERIFY(session.seed(s));
        auto original = session.sketches();
        auto revision = session.client->state().revision;
        auto *entities = session.window->findChild<QListWidget *>("sketchEntities");
        for (int i = 0; i < entities->count(); ++i)
            entities->item(i)->setSelected(entities->item(i)->data(Qt::UserRole).toString() ==
                                           line);
        auto *action = session.window->command("sketch.fullyDefine");
        QVERIFY2(action && action->isEnabled(),
                 "Fully Define needs an All/Selected proposal review");
        for (bool accept : {false, true}) {
            bool opened = false;
            auto modalAction = test::whenModal(session.window.get(), [&](QDialog *dialog) {
                opened = true;
                auto close = qScopeGuard([&] {
                    if (dialog->isVisible())
                        dialog->reject();
                });
                auto *scope = dialog->findChild<QComboBox *>("proposalScope");
                QVERIFY(scope && scope->findText("All") >= 0 && scope->findText("Selected") >= 0);
                scope->setFocus();
                QTest::keyClick(scope, Qt::Key_End);
                QCOMPARE(scope->currentText(), QString("Selected"));
                dialog->findChild<QPushButton *>("proposalCalculate")->click();
                auto *apply = dialog->findChild<QPushButton *>("proposalApply");
                QVERIFY(wait([&] { return apply->isEnabled(); }));
                QVERIFY(dialog->findChild<QLabel *>("proposalStatus")
                            ->text()
                            .contains("3 degrees of freedom"));
                QCOMPARE(session.sketches(), original);
                QCOMPARE(session.client->state().revision, revision);
                if (accept) {
                    QVERIFY(session.snapshot("fully-define", dialog));
                    QVERIFY(session.snapshot("fully-define-canvas"));
                    apply->setFocus();
                    QTest::keyClick(apply, Qt::Key_Space);
                } else
                    QTest::keyClick(dialog, Qt::Key_Escape);
            });
            action->trigger();
            QVERIFY(opened);
            if (!accept)
                QVERIFY(!session.panel->canUndoDraft());
        }
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        bool diagnosed = false;
        for (auto value : session.client->state().documentRecords)
            for (auto item : value.toMap().value("sketch_diagnostics").toArray()) {
                auto diagnostic = item.toMap();
                if (diagnostic.value("sketch_id").toString() == s.id) {
                    diagnosed = true;
                    QVERIFY(diagnostic.value("valid").toBool());
                    QCOMPARE(diagnostic.value("freedom").toInteger(), qint64(3));
                }
            }
        QVERIFY(diagnosed);
        for (auto &e : s.entities)
            if (e.kind == "Point")
                QVERIFY(sketch::length(result.entity(e.id).point, e.point) < 1e-9);
    }
    void automaticInference() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, .00005});
        QVERIFY(session.seed(s));
        auto revision = session.client->state().revision;
        auto *automatic = session.window->findChild<QCheckBox *>("automaticRelations");
        QVERIFY2(automatic, "Automatic relations needs an explicit toggle");
        QVERIFY(!automatic->isChecked());
        automatic->setChecked(true);
        session.panel->setTool("Line");
        auto *add = session.window->findChild<QPushButton *>("addSketchCoordinate");
        QVERIFY(add);
        auto *x = session.window->findChild<QLineEdit *>("sketchX");
        auto *y = session.window->findChild<QLineEdit *>("sketchY");
        QVERIFY(x && y);
        x->setText("0");
        y->setText("0");
        add->click();
        x->setText("30");
        y->setText("0.05");
        add->click();
        QVERIFY(wait([&] {
            return session.panel->canvas()->accessibleDescription().contains("Unconfirmed") &&
                   !session.client->outstanding();
        }));
        QVERIFY(session.panel->canvas()->accessibleDescription().contains("Horizontal"));
        QVERIFY(session.snapshot("automatic-guides"));
        QCOMPARE(session.client->state().revision, revision);
        QCOMPARE(sketch::parse(session.sketches())[0].constraints.size(), size_t(0));
        automatic->setChecked(false);
        QVERIFY(!session.panel->canvas()->accessibleDescription().contains("Unconfirmed"));
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QVERIFY(result.constraints.empty());
        QCOMPARE(result.entities.size(), size_t(3));
    }
    void confirmInference() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, .00005}), line = s.line(a, b);
        QVERIFY(session.seed(s));
        auto original = session.sketches();
        auto revision = session.client->state().revision;
        auto *entities = session.window->findChild<QListWidget *>("sketchEntities");
        for (int i = 0; i < entities->count(); ++i)
            entities->item(i)->setSelected(entities->item(i)->data(Qt::UserRole).toString() ==
                                           line);
        auto *action = session.window->command("sketch.infer");
        QVERIFY2(action && action->isEnabled(),
                 "Inference proposals must be explicitly reviewable");
        QString confirmed;
        for (bool accept : {false, true}) {
            bool opened = false;
            auto modalAction = test::whenModal(session.window.get(), [&](QDialog *dialog) {
                opened = true;
                auto close = qScopeGuard([&] {
                    if (dialog->isVisible())
                        dialog->reject();
                });
                auto *calculate = dialog->findChild<QPushButton *>("proposalCalculate");
                auto *apply = dialog->findChild<QPushButton *>("proposalApply");
                QVERIFY(calculate && apply);
                calculate->click();
                QVERIFY(wait([&] { return apply->isEnabled(); }));
                auto *list = dialog->findChild<QListWidget *>("proposalRelations");
                QVERIFY(list && list->count() == 1);
                QVERIFY(list->item(0)->text().contains("Horizontal"));
                confirmed = list->item(0)->data(Qt::UserRole).toString();
                QCOMPARE(session.sketches(), original);
                QCOMPARE(session.client->state().revision, revision);
                if (accept)
                    apply->click();
                else
                    dialog->reject();
            });
            action->trigger();
            QVERIFY(opened);
            if (!accept) {
                QVERIFY(!session.panel->canUndoDraft());
                QCOMPARE(session.sketches(), original);
            }
        }
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QCOMPARE(result.constraints.size(), size_t(1));
        QCOMPARE(result.constraints[0].id, confirmed);
        QCOMPARE(result.constraints[0].kind, QString("Horizontal"));
        QVERIFY(std::abs(result.entity(a).point.y - result.entity(b).point.y) < 1e-10);
    }
    void dragCancel_data() {
        QTest::addColumn<bool>("released");
        QTest::newRow("button-held") << false;
        QTest::newRow("released-request-pending") << true;
    }
    void dragCancel() {
        QFETCH(bool, released);
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, .04}), line = s.line(a, b);
        sketch::Constraint fixed, length;
        fixed.kind = "Fixed";
        fixed.a = a;
        length.kind = "Length";
        length.a = line;
        length.value = .05;
        s.constraints = {fixed, length};
        QVERIFY(session.seed(s));
        auto original = session.sketches();
        session.window->command("fit")->trigger();
        auto *canvas = session.panel->canvas();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                          canvas->screen({.03, .04}).toPoint());
        auto at = canvas->screen({.04, .04});
        QMouseEvent move(QEvent::MouseMove, at, canvas->mapToGlobal(at), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        if (released) {
            QVERIFY(session.client->outstanding());
            QMouseEvent release(QEvent::MouseButtonRelease, at, canvas->mapToGlobal(at),
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(canvas, &release);
        }
        // Cancel while a worker request may still be running. Its eventual
        // response must not create a draft edit or change accepted geometry.
        QTest::keyClick(canvas, Qt::Key_Escape, Qt::NoModifier, 0);
        QVERIFY(wait([&] { return !session.client->outstanding(); }));
        QVERIFY(session.panel->active());
        QVERIFY(!session.panel->canUndoDraft());
        QVERIFY(session.finish());
        QCOMPARE(session.sketches(), original);
    }
    void dragLastValid() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, .04}), line = s.line(a, b);
        sketch::Constraint fixed, length;
        fixed.kind = "Fixed";
        fixed.a = a;
        length.kind = "Length";
        length.a = line;
        length.value = .05;
        s.constraints = {fixed, length};
        QVERIFY(session.seed(s));
        session.window->command("fit")->trigger();
        auto *canvas = session.panel->canvas();
        QSignalSpy previews(session.client, &Client::sketchPreview);
        QSignalSpy diagnostics(session.client, &Client::sketchDiagnosed);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                          canvas->screen({.03, .04}).toPoint());
        auto moveTo = [&](sketch::Vec2 point) {
            auto at = canvas->screen(point), global = canvas->mapToGlobal(at);
            QMouseEvent move(QEvent::MouseMove, at, global, Qt::NoButton, Qt::LeftButton,
                             Qt::NoModifier);
            QApplication::sendEvent(canvas, &move);
            return global;
        };
        moveTo({.04, .04});
        QVERIFY(wait([&] { return !previews.empty() && !session.client->outstanding(); }));
        auto lastValid = sketch::parse(previews.last()[0].value<QCborArray>())[0].entity(b).point;
        int count = diagnostics.size();
        auto global = moveTo({-.05, 0});
        QVERIFY(wait([&] { return diagnostics.size() > count && !session.client->outstanding(); }));
        QVERIFY(!diagnostics.last()[0].value<QCborArray>()[0].toMap().value("valid").toBool());
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                            canvas->mapFromGlobal(global.toPoint()));
        QVERIFY(
            wait([&] { return !session.client->outstanding() && session.panel->canUndoDraft(); }));
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QVERIFY(sketch::length(result.entity(b).point, lastValid) < 1e-9);
        QCOMPARE(result.constraints.size(), s.constraints.size());
    }
    void constrainedCurvePointer() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, 0}), line = s.line(a, b);
        sketch::Constraint length;
        length.kind = "Length";
        length.a = line;
        length.value = .03;
        s.constraints.push_back(length);
        QVERIFY(session.seed(s));
        session.window->command("fit")->trigger();
        auto *canvas = session.panel->canvas();
        QSignalSpy preview(session.client, &Client::sketchPreview);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                          canvas->screen({.006, 0}).toPoint());
        auto at = canvas->screen({.012, .004});
        auto global = canvas->mapToGlobal(at);
        QMouseEvent move(QEvent::MouseMove, at, global, Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        QVERIFY2(wait([&] { return !preview.empty() && !session.client->outstanding(); }),
                 "Dimensioned curve bodies must use worker-solved pointer targets");
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                            canvas->mapFromGlobal(global.toPoint()));
        QVERIFY(
            wait([&] { return !session.client->outstanding() && session.panel->canUndoDraft(); }));
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QVERIFY(result.entity(a).point.x > .003);
        QVERIFY(std::abs(sketch::length(result.entity(a).point, result.entity(b).point) - .03) <
                1e-9);
        QCOMPARE(result.constraints[0].id, length.id);
    }
    void dragFamilies_data() {
        QTest::addColumn<QString>("kind");
        for (QString kind : {"Line", "Circle", "Arc", "Ellipse", "EllipticalArc", "Parabola",
                             "Spline", "Rectangle", "Slot", "Path"})
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void dragFamilies() {
        QFETCH(QString, kind);
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        QString entity;
        if (kind == "Line" || kind == "Path") {
            auto a = s.point({0, 0}), b = s.point({.03, 0});
            entity = s.line(a, b);
            sketch::Constraint dimension;
            dimension.kind = kind == "Path" ? "PathLength" : "Length";
            dimension.a = entity;
            dimension.value = .03;
            if (kind == "Path") {
                auto other = s.line(b, s.point({.03, .04}));
                dimension.members = {entity, other};
                dimension.directions = {false, false};
                dimension.value = .07;
                sketch::Constraint fixed;
                fixed.kind = "Fixed";
                fixed.a = a;
                s.constraints.push_back(fixed);
            }
            s.constraints.push_back(dimension);
        } else if (kind == "Circle" || kind == "Arc") {
            auto center = s.point({0, 0});
            entity = kind == "Circle" ? s.circle(center, .01) : s.arc(center, .01, 0, 1.2);
            sketch::Constraint radius;
            radius.kind = "Radius";
            radius.a = entity;
            radius.value = .01;
            s.constraints.push_back(radius);
        } else if (kind == "Ellipse" || kind == "EllipticalArc")
            entity = sketch::createEllipse(s, {0, 0}, {.03, 0}, {0, .01}, kind == "EllipticalArc",
                                           .2, 1.2);
        else if (kind == "Parabola")
            entity = sketch::createParabola(s, {.01, 0}, {0, 0}, {.01, -.02}, {.01, .02});
        else if (kind == "Spline")
            entity = sketch::createSpline(s, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}});
        else {
            if (kind == "Slot")
                sketch::createSlot(s, "Straight", {{0, 0}, {.03, 0}, {.015, .005}},
                                   "Center to Center", true);
            else
                sketch::createRectangle(s, "Corner", {{0, 0}, {.03, .02}});
            entity = s.groups.back().edges[0];
        }
        QVERIFY(session.seed(s));
        auto accepted = session.sketches();
        auto original = sketch::parse(accepted)[0];
        auto from = kind == "Circle" || kind == "Arc"
                        ? sketch::curveValue(original, original.entity(entity), .6)
                        : sketch::location(original, original.entity(entity));
        auto to = sketch::Vec2{from.x + .006, from.y + .004};
        QSignalSpy preview(session.client, &Client::sketchPreview);
        QSignalSpy diagnosed(session.client, &Client::sketchDiagnosed);
        auto interaction = sketch::fields({{"kind", "Drag"},
                                           {"sketch", s.id},
                                           {"entity", entity},
                                           {"from", QCborArray{from.x, from.y}},
                                           {"to", QCborArray{to.x, to.y}}});
        session.client->projectCommand(
            "PreviewSketches",
            sketch::fields({{"sketches", accepted}, {"interaction", interaction}}));
        QVERIFY(wait([&] { return !session.client->outstanding(); }));
        QVERIFY2(!preview.empty() && !diagnosed.empty(),
                 qPrintable(session.client->state().message));
        auto diagnostic = diagnosed.last()[0].value<QCborArray>()[0].toMap();
        QVERIFY2(diagnostic.value("valid").toBool(),
                 qPrintable(diagnostic.value("message").toString()));
        auto result = sketch::parse(preview.last()[0].value<QCborArray>())[0];
        QVERIFY2(result.map() != original.map(),
                 qPrintable(kind + " must respond to a feasible drag target"));
        QCOMPARE(result.constraints.size(), original.constraints.size());
        for (size_t i = 0; i < original.constraints.size(); ++i) {
            QCOMPARE(result.constraints[i].id, original.constraints[i].id);
            QCOMPARE(result.constraints[i].expression, original.constraints[i].expression);
            QVERIFY(std::abs(result.constraints[i].value - original.constraints[i].value) < 1e-10);
        }
        QCOMPARE(session.sketches(), accepted);
    }
    void constrainedPointerDrag() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, .04}), line = s.line(a, b);
        sketch::Constraint fixed, length;
        fixed.kind = "Fixed";
        fixed.a = a;
        length.kind = "Length";
        length.a = line;
        length.value = .05;
        s.constraints = {fixed, length};
        QVERIFY(session.seed(s));
        session.window->command("fit")->trigger();
        auto *canvas = session.panel->canvas();
        auto revision = session.client->state().revision;
        QSignalSpy preview(session.client, &Client::sketchPreview);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                          canvas->screen({.03, .04}).toPoint());
        auto at = canvas->screen({.04, .04});
        auto globalAt = canvas->mapToGlobal(at);
        QMouseEvent move(QEvent::MouseMove, at, globalAt, Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        QVERIFY2(wait([&] { return !preview.empty() && !session.client->outstanding(); }),
                 "Constrained pointer movement must request a worker-solved preview");
        // Selection/property layout can move the canvas while the worker runs.
        // A stationary physical pointer must release at its same global point.
        auto releaseAt = canvas->mapFromGlobal(globalAt.toPoint());
        qInfo() << "Drag release layout delta" << releaseAt - at.toPoint();
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, releaseAt);
        QVERIFY(
            wait([&] { return !session.client->outstanding() && session.panel->canUndoDraft(); }));
        QCOMPARE(session.client->state().revision, revision);
        session.panel->undoDraftEdit();
        QVERIFY(!session.panel->canUndoDraft());
        QVERIFY(session.panel->canRedoDraft());
        session.panel->redoDraftEdit();
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        for (auto response : preview) {
            auto point = sketch::parse(response[0].value<QCborArray>())[0].entity(b).point;
            qInfo() << "Drag preview endpoint" << point.x << point.y;
        }
        QVERIFY2(result.entity(b).point.x > .03, qPrintable(QString("Unexpected endpoint (%1,%2)")
                                                                .arg(result.entity(b).point.x)
                                                                .arg(result.entity(b).point.y)));
        QVERIFY(std::abs(sketch::length(result.entity(a).point, result.entity(b).point) - .05) <
                1e-9);
        QCOMPARE(result.constraints.size(), s.constraints.size());
    }
    void dragProtocol() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, .04}), line = s.line(a, b);
        sketch::Constraint fixed, length;
        fixed.kind = "Fixed";
        fixed.a = a;
        length.kind = "Length";
        length.a = line;
        length.value = .05;
        s.constraints = {fixed, length};
        QVERIFY(session.seed(s));
        auto accepted = session.sketches();
        auto revision = session.client->state().revision;
        QSignalSpy preview(session.client, &Client::sketchPreview);
        QSignalSpy diagnosed(session.client, &Client::sketchDiagnosed);
        auto interaction = sketch::fields({{"kind", "Drag"},
                                           {"sketch", s.id},
                                           {"entity", b},
                                           {"from", QCborArray{.03, .04}},
                                           {"to", QCborArray{.04, .04}}});
        session.client->projectCommand(
            "PreviewSketches",
            sketch::fields({{"sketches", accepted}, {"interaction", interaction}}));
        QVERIFY(wait([&] { return !session.client->outstanding(); }));
        QVERIFY2(!preview.empty() && !diagnosed.empty(),
                 qPrintable(session.client->state().message));
        QVERIFY(diagnosed.last()[0].value<QCborArray>()[0].toMap().value("valid").toBool());
        auto output = sketch::parse(preview.last()[0].value<QCborArray>())[0];
        auto point = output.entity(b).point;
        QVERIFY(point.x > .03 && point.y < .04);
        QVERIFY(std::abs(sketch::length(output.entity(a).point, point) - .05) < 1e-9);
        QCOMPARE(output.constraints.size(), s.constraints.size());
        QCOMPARE(output.constraints[0].id, fixed.id);
        QCOMPARE(output.constraints[1].id, length.id);
        QCOMPARE(session.client->state().revision, revision);
        QCOMPARE(session.sketches(), accepted);
    }
    void pathEditor_data() {
        QTest::addColumn<QString>("units");
        QTest::newRow("mm") << QString("mm");
        QTest::newRow("in") << QString("in");
    }
    void pathEditor() {
        QFETCH(QString, units);
        Session session;
        QVERIFY(session.start(units));
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, 0}), c = s.point({.03, .04});
        auto first = s.line(a, b), second = s.line(b, c);
        QVERIFY(session.seed(s));
        auto *list = session.window->findChild<QListWidget *>("sketchEntities");
        list->clearSelection();
        for (auto id : {second, first})
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toString() == id)
                    list->item(i)->setSelected(true);
        auto *action = session.window->command("sketch.pathLength");
        QVERIFY2(action && action->isEnabled(),
                 "A selected connected curve chain must expose Path length");
        bool applied = false;
        auto modalAction = test::whenModal(session.window.get(), [&](QDialog *dialog) {
            auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            auto *expression = dialog->findChild<QLineEdit *>("dimensionExpression");
            QVERIFY(wait(
                [&] { return !session.client->outstanding() && !expression->text().isEmpty(); }));
            expression->setText(QString::number(.1 / domain::toSI(1, "length", units), 'g', 17));
            dialog->findChild<QPushButton *>("dimensionPreview")->click();
            auto *apply = dialog->findChild<QPushButton *>("dimensionApply");
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            QVERIFY(session.snapshot("path-editor", dialog));
            QVERIFY(session.snapshot("path-canvas"));
            applied = true;
            apply->click();
        });
        action->trigger();
        QVERIFY(applied);
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QCOMPARE(result.constraints.size(), size_t(1));
        auto &path = result.constraints[0];
        QCOMPARE(path.kind, QString("PathLength"));
        QVERIFY(path.driving);
        QCOMPARE(QSet<QString>(path.members.begin(), path.members.end()),
                 QSet<QString>({first, second}));
        double total = sketch::length(result.entity(a).point, result.entity(b).point) +
                       sketch::length(result.entity(b).point, result.entity(c).point);
        QVERIFY(std::abs(total - .1) < 1e-9);
    }
    void ordinateSeries_data() {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("units");
        for (QString mode : {"shared", "origin", "reference"})
            for (QString units : {"mm", "in"})
                QTest::newRow(qPrintable(mode + "-" + units)) << mode << units;
    }
    void ordinateSeries() {
        QFETCH(QString, mode);
        QFETCH(QString, units);
        Session session;
        QVERIFY(session.start(units));
        sketch::Sketch s;
        auto datum = s.point({.01, .02});
        QStringList targets;
        for (int i = 1; i <= 3; ++i)
            targets << s.point({.01 + .02 * i, .02 + .01 * i});
        QVERIFY(session.seed(s));
        auto *list = session.window->findChild<QListWidget *>("sketchEntities");
        list->clearSelection();
        for (int i = 0; i < list->count(); ++i)
            if (targets.contains(list->item(i)->data(Qt::UserRole).toString()))
                list->item(i)->setSelected(true);
        auto *action = session.window->command("sketch.ordinates");
        QVERIFY2(action && action->isEnabled(),
                 "Selected points must support a shared-datum ordinate series");
        bool applied = false;
        auto modalAction = test::whenModal(session.window.get(), [&](QDialog *dialog) {
            auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            auto *origin = dialog->findChild<QComboBox *>("ordinateDatum");
            QVERIFY(origin);
            origin->setCurrentIndex(mode == "shared" ? origin->findData(datum) : 0);
            if (mode != "shared")
                dialog->findChild<QComboBox *>("ordinateAxis")->setCurrentText("Vertical");
            dialog->findChild<QCheckBox *>("ordinateReference")->setChecked(mode == "reference");
            dialog->findChild<QPushButton *>("ordinatePreview")->click();
            auto *apply = dialog->findChild<QPushButton *>("ordinateApply");
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            QVERIFY(session.snapshot("ordinates", dialog));
            QVERIFY(session.snapshot("ordinates-canvas"));
            applied = true;
            apply->click();
        });
        action->trigger();
        QVERIFY(applied);
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QCOMPARE(result.constraints.size(), mode == "shared" ? size_t(3) : size_t(4));
        QSet<double> rows;
        for (auto &dimension : result.constraints) {
            if (dimension.kind == "Fixed")
                continue;
            QCOMPARE(dimension.kind, mode == "shared" ? QString("HorizontalOrdinate")
                                                      : QString("VerticalOrdinate"));
            if (mode == "shared")
                QCOMPARE(dimension.a, datum);
            else {
                QVERIFY(result.entity(dimension.a).construction);
                QVERIFY(sketch::length(result.entity(dimension.a).point, {0, 0}) < 1e-12);
            }
            QCOMPARE(dimension.driving, mode != "reference");
            QVERIFY(targets.removeOne(dimension.b));
            QVERIFY(dimension.placement.has_value());
            rows.insert(dimension.placement->y);
            QVERIFY(std::abs(dimension.value - (mode == "shared"
                                                    ? result.entity(dimension.b).point.x - .01
                                                    : result.entity(dimension.b).point.y)) < 1e-10);
        }
        QCOMPARE(rows.size(), qsizetype(3));
        QVERIFY(targets.empty());
    }
    void reverseCenterline() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({0, .1});
        auto axis = s.line(a, b, true), profile = s.point({.03, .04});
        sketch::Constraint first, last, diameter;
        first.kind = last.kind = "Fixed";
        first.a = a;
        last.a = b;
        last.second = .1;
        diameter.kind = "CenterlineDiameter";
        diameter.a = profile;
        diameter.b = axis;
        diameter.value = .06;
        diameter.reversed = true;
        diameter.placement = sketch::Vec2{.04, .06};
        s.constraints = {first, last, diameter};
        QVERIFY(session.seed(s));
        QVERIFY(session.dimension(diameter.id, [&](QDialog *dialog) {
            auto *reverse = dialog->findChild<QPushButton *>("dimensionReverse");
            QVERIFY(reverse && reverse->isEnabled());
            reverse->click();
            dialog->findChild<QPushButton *>("dimensionPreview")->click();
            auto *apply = dialog->findChild<QPushButton *>("dimensionApply");
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            apply->click();
        }));
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QVERIFY(std::abs(result.entity(profile).point.x + .03) < 1e-10);
        QVERIFY(!result.constraints.back().reversed);
        QCOMPARE(result.constraints.back().id, diameter.id);
    }
    void reverseBlocked() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto p = s.point({0, 0}), q = s.point({.03, .04}), line = s.line(p, q);
        sketch::Constraint a, b, dimension;
        a.kind = b.kind = "Fixed";
        a.a = p;
        b.a = q;
        b.value = .03;
        b.second = .04;
        dimension.kind = "Length";
        dimension.a = line;
        dimension.value = .05;
        dimension.placement = sketch::Vec2{.01, .06};
        s.constraints = {a, b, dimension};
        QVERIFY(session.seed(s));
        auto original = session.sketches();
        QVERIFY(session.dimension(dimension.id, [&](QDialog *dialog) {
            dialog->findChild<QPushButton *>("dimensionReverse")->click();
            dialog->findChild<QPushButton *>("dimensionPreview")->click();
            QVERIFY(wait([&] { return !session.client->outstanding(); }));
            QVERIFY2(!dialog->findChild<QPushButton *>("dimensionApply")->isEnabled(),
                     "Fixed endpoints must not report a successful reversal of unchanged geometry");
            dialog->reject();
        }));
        QCOMPARE(session.sketches(), original);
    }
    void reverseAligned() {
        for (QString kind : {"Length", "Distance"}) {
            Session session;
            QVERIFY(session.start());
            sketch::Sketch s;
            auto p = s.point({0, 0}), q = s.point({.03, .04}), line = s.line(p, q);
            sketch::Constraint fixed;
            fixed.kind = "Fixed";
            fixed.a = p;
            sketch::Constraint dimension;
            dimension.kind = kind;
            dimension.a = kind == "Length" ? line : p;
            dimension.b = kind == "Length" ? QString() : q;
            dimension.value = .05;
            dimension.expression = "50 mm";
            dimension.placement = sketch::Vec2{.01, .06};
            s.constraints = {fixed, dimension};
            QVERIFY(session.seed(s));
            QVERIFY(session.dimension(dimension.id, [&](QDialog *dialog) {
                auto *reverse = dialog->findChild<QPushButton *>("dimensionReverse");
                QVERIFY(reverse && reverse->isEnabled());
                reverse->click();
                dialog->findChild<QPushButton *>("dimensionPreview")->click();
                auto *apply = dialog->findChild<QPushButton *>("dimensionApply");
                QVERIFY(wait([&] { return apply->isEnabled(); }));
                apply->click();
            }));
            QVERIFY(session.finish());
            auto result = sketch::parse(session.sketches())[0];
            auto a = result.entity(p).point, b = result.entity(q).point;
            QVERIFY((b.x - a.x) * .03 + (b.y - a.y) * .04 < 0);
            QVERIFY(std::abs(sketch::length(a, b) - .05) < 1e-10);
            QCOMPARE(result.constraints.back().id, dimension.id);
            QVERIFY(result.constraints.back().reversed);
        }
    }
    void reverseComponent() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto p = s.point({0, 0}), q = s.point({.03, .04});
        s.line(p, q);
        sketch::Constraint dimension;
        dimension.kind = "HorizontalDistance";
        dimension.a = p;
        dimension.b = q;
        dimension.value = .03;
        dimension.expression = "W";
        dimension.placement = sketch::Vec2{.01, .06};
        s.names.insert("W", "30 mm");
        s.constraints.push_back(dimension);
        QVERIFY(session.seed(s));
        QVERIFY(session.dimension(dimension.id, [&](QDialog *dialog) {
            auto *reverse = dialog->findChild<QPushButton *>("dimensionReverse");
            QVERIFY2(reverse, "A linear dimension must expose Reverse sense");
            reverse->click();
            dialog->findChild<QPushButton *>("dimensionPreview")->click();
            auto *apply = dialog->findChild<QPushButton *>("dimensionApply");
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            apply->click();
        }));
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QCOMPARE(result.constraints[0].id, dimension.id);
        QVERIFY(result.constraints[0].driving);
        QVERIFY(std::abs(result.entity(q).point.x - result.entity(p).point.x + .03) < 1e-10);
        QVERIFY(result.constraints[0].expression.contains("W"));
    }
    void drawingMark() {
        Session session;
        QVERIFY(session.start());
        sketch::Sketch s;
        auto circle = s.circle(s.point({0, 0}), .005);
        sketch::Constraint dimension;
        dimension.kind = "Radius";
        dimension.a = circle;
        dimension.value = .005;
        dimension.placement = sketch::Vec2{.01, .01};
        s.constraints.push_back(dimension);
        QVERIFY(session.seed(s));
        QVERIFY(session.dimension(dimension.id, [&](QDialog *dialog) {
            auto *mark = dialog->findChild<QCheckBox *>("dimensionDrawingMark");
            QVERIFY2(mark, "Dimension drawing-import intent must be explicitly editable");
            mark->setChecked(true);
            dialog->findChild<QPushButton *>("dimensionPreview")->click();
            auto *apply = dialog->findChild<QPushButton *>("dimensionApply");
            QVERIFY(wait([&] { return apply->isEnabled(); }));
            apply->click();
        }));
        QVERIFY(session.finish());
        auto result = sketch::parse(session.sketches())[0];
        QVERIFY(result.constraints[0].drawing);
        QCOMPARE(result.constraints[0].id, dimension.id);
        QVERIFY(std::abs(result.entity(circle).radius - .005) < 1e-12);
    }
    void increments() {
        for (QString units : {"mm", "in"}) {
            Session session;
            QVERIFY(session.start(units));
            sketch::Sketch s;
            auto circle = s.circle(s.point({0, 0}), .005);
            sketch::Constraint radius;
            radius.kind = "Radius";
            radius.a = circle;
            radius.value = .005;
            radius.expression = "R";
            radius.placement = sketch::Vec2{.01, .01};
            s.names.insert("R", "5 mm");
            s.constraints.push_back(radius);
            QVERIFY(session.seed(s));
            QVERIFY(session.dimension(radius.id, [&](QDialog *dialog) {
                auto *increment = dialog->findChild<QLineEdit *>("dimensionIncrement");
                QVERIFY2(increment, "Dimension editor must expose its unit-aware spin increment");
                auto *value = dialog->findChild<QLineEdit *>("dimensionExpression");
                QCOMPARE(value->text(), QString("R"));
                increment->setText(units == "in" ? "0.02" : "0.5");
                QCOMPARE(value->text(), QString("R"));
                auto *increase = dialog->findChild<QPushButton *>("dimensionIncrease");
                QVERIFY(increase);
                increase->click();
                dialog->findChild<QPushButton *>("dimensionPreview")->click();
                auto *apply = dialog->findChild<QPushButton *>("dimensionApply");
                QVERIFY(wait([&] { return apply->isEnabled(); }));
                apply->click();
            }));
            QVERIFY(session.finish());
            auto result = sketch::parse(session.sketches())[0];
            double step = units == "in" ? .02 * .0254 : .0005;
            QVERIFY(std::abs(result.entity(circle).radius - (.005 + step)) < 1e-10);
            QVERIFY(std::abs(result.constraints[0].increment.value_or(0) - step) < 1e-12);
            QCOMPARE(result.constraints[0].id, radius.id);
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
    CompletionContracts test;
    return QTest::qExec(&test, argc, argv);
}
#include "completion_contracts.moc"
