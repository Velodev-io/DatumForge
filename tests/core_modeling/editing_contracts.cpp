#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/Viewport.hpp"
#include "sketch/LineEditing.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
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
void click(MainWindow &w, QString id) {
    auto *b = w.findChild<QPushButton *>(id);
    if (!b)
        qFatal("Missing button");
    for (auto *scroll : w.findChildren<QScrollArea *>())
        if (scroll->isAncestorOf(b))
            scroll->ensureWidgetVisible(b);
    QTest::qWait(10);
    QTest::mouseClick(b, Qt::LeftButton);
}
} // namespace
class EditingContracts : public QObject {
    Q_OBJECT
  private slots:
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
        for (QString units : {"mm", "in"})
            for (auto mode : sketch::lineEditModes()) {
                qInfo() << "Line edit workflow" << mode << units;
                QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm2-ui-XXXXXX"),
                    files(QDir::tempPath() + "/cad-module3-cm2-ui-files-XXXXXX");
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
                    QVERIFY(std::abs(w.devicePixelRatioF() -
                                     qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
                }
                auto *c = w.workerClient();
                QVERIFY2(wait([&] { return c->ready(); }), qPrintable(c->state().message));
                if (native) {
                    // Sketch projects hide the GL view; verify its visible renderer first.
                    c->loadReference();
                    QVERIFY2(wait([&] {
                                 return !c->outstanding() && c->mesh() &&
                                        w.sceneView()->graphicsDescription().contains(
                                            "initialized 1");
                             }),
                             qPrintable(w.sceneView()->graphicsDescription()));
                    QVERIFY2(graphics.empty(), "Native graphics initialization failed");
                    c->closeReference();
                    QVERIFY(wait([&] { return !c->outstanding() && !c->mesh(); }));
                }
                c->newProject("Part", "Line edits", units);
                QVERIFY(wait([&] { return c->state().projectOpen && !c->outstanding(); }));
                sketch::Sketch s;
                bool extend = mode.contains("extend", Qt::CaseInsensitive);
                auto source = s.line(s.point({0, 0}), s.point({extend ? .02 : .04, 0}));
                auto left = s.line(s.point({.01, -.01}), s.point({.01, .01}), true);
                auto right = s.line(s.point({.03, mode.startsWith("Extend") ? .01 : -.01}),
                                    s.point({.03, .02}), true);
                sketch::Constraint dimension;
                dimension.kind = "Length";
                dimension.a = source;
                dimension.expression = extend ? "20 mm" : "40 mm";
                s.constraints.push_back(dimension);
                c->projectCommand("SetSketches",
                                  sketch::fields({{"sketches", sketch::encode({s})}}));
                QVERIFY2(wait([&] { return !c->outstanding() && sketches(c).size() == 1; }),
                         qPrintable(c->state().message));
                auto original = sketches(c);
                auto revision = c->state().revision;
                click(w, "editSketchButton");
                auto *panel = w.findChild<SketchPanel *>();
                auto *view = panel->canvas();
                auto *action = w.command("sketch.tool.Trim / extend");
                QVERIFY(action && action->isEnabled());
                action->trigger();
                auto *method = w.findChild<QComboBox *>("lineEditMode");
                method->setCurrentText(mode);
                auto *lines = w.findChild<QComboBox *>("lineEditSource");
                lines->setCurrentIndex(lines->findData(source));
                auto *boundaries = w.findChild<QListWidget *>("lineEditBoundaries");
                for (int i = 0; i < boundaries->count(); ++i) {
                    auto *item = boundaries->item(i);
                    auto id = item->data(Qt::UserRole).toString();
                    item->setSelected(id == right || (!extend && id == left));
                }
                double pick = extend ? .019 : mode == "Trim to closest" ? .035 : .02;
                double unit = domain::toSI(1, "length", units);
                w.findChild<QLineEdit *>("lineEditPickX")
                    ->setText(QString::number(pick / unit, 'g', 17));
                w.findChild<QLineEdit *>("lineEditPickY")->setText("0");
                auto *apply = w.findChild<QPushButton *>("applyLineEdit");
                auto *disclosure = w.findChild<QListWidget *>("lineEditDisclosure");
                auto *consent = w.findChild<QCheckBox *>("lineEditConsent");
                click(w, "previewLineEdit");
                QVERIFY2(wait([&] { return !c->outstanding() && disclosure->count() == 1; }),
                         qPrintable(c->state().message));
                QVERIFY(!apply->isEnabled());
                QCOMPARE(c->state().revision, revision);
                QCOMPARE(sketches(c), original);
                QVERIFY(disclosure->item(0)->toolTip().contains(dimension.id));
                QVERIFY(disclosure->item(0)->text().contains(dimension.expression));
                // Finish cannot bypass the constraint decision or implicitly install a candidate.
                panel->finish();
                QCOMPARE(c->state().revision, revision);
                QVERIFY(panel->active());
                click(w, "cancelLineEdit");
                QVERIFY(!apply->isEnabled());
                QCOMPARE(sketches(c), original);
                // Pointer pick makes the same exact preview. Pick is intentionally away from hits.
                w.command("fit")->trigger();
                QTest::qWait(20);
                auto point = view->screen({pick, 0}).toPoint();
                QVERIFY(view->rect().contains(point));
                QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, point);
                QVERIFY2(wait([&] { return !c->outstanding() && disclosure->count() == 1; }),
                         qPrintable(c->state().message));
                // Cancel in flight invalidates the generation and leaves the draft untouched.
                click(w, "cancelLineEdit");
                QSignalSpy delivered(c, &Client::sketchEditPreview);
                w.findChild<QPushButton *>("previewLineEdit")->click();
                w.findChild<QPushButton *>("cancelLineEdit")->click();
                QVERIFY(wait([&] { return !c->outstanding(); }));
                QCOMPARE(delivered.count(), 0);
                QCOMPARE(sketches(c), original);
                // Repeated previews retain one running request and one latest draft.
                for (int n = 0; n < 20; ++n) {
                    w.findChild<QPushButton *>("previewLineEdit")->click();
                    QVERIFY(c->outstanding() <= 2);
                }
                QVERIFY(wait([&] { return !c->outstanding(); }));
                QCOMPARE(delivered.count(), 1);
                click(w, "previewLineEdit");
                QVERIFY(wait([&] { return !c->outstanding() && disclosure->count() == 1; }));
                consent->setChecked(true);
                QVERIFY(apply->isEnabled());
                if (native && units == "mm" && mode == "Trim away inside") {
                    auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
                    QVERIFY(QDir().mkpath(out));
                    QVERIFY(w.grab().save(out + "/cm2-inside-disclosure.png"));
                }
                click(w, "applyLineEdit");
                QCOMPARE(c->state().revision, revision);
                QCOMPARE(sketches(c), original);
                auto *entities = w.findChild<QListWidget *>("sketchEntities");
                int editedCount = entities->count();
                w.command("undo")->trigger();
                QCOMPARE(entities->count(), int(s.entities.size()));
                w.command("redo")->trigger();
                QCOMPARE(entities->count(), editedCount);
                panel->finish();
                QVERIFY2(wait([&] { return !c->outstanding() && c->state().revision > revision; }),
                         qPrintable(c->state().message));
                auto accepted = sketches(c);
                auto edited = sketch::parse(accepted)[0];
                QCOMPARE(edited.edits.size(), 1);
                QVERIFY(edited.constraints.empty());
                QCOMPARE(edited.edits[0].toMap().value("removed_constraints").toArray(),
                         QCborArray{dimension.id});
                auto descendants = edited.edits[0].toMap().value("descendants").toArray();
                QCOMPARE(descendants.size(), mode == "Trim away inside" ? 2 : 1);
                w.command("undo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == original; }));
                w.command("redo")->trigger();
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
                auto path = files.filePath("editing.cadproj");
                c->saveProject(path);
                QVERIFY2(wait([&] {
                             return !c->outstanding() && QFile::exists(path) &&
                                    c->state().savedPath == QFileInfo(path).canonicalFilePath();
                         }),
                         qPrintable("Save " + path + " observed " + c->state().savedPath + " · " +
                                    c->state().message));
                c->openProject(path, "Saved", true);
                QVERIFY(wait([&] { return !c->outstanding() && sketches(c) == accepted; }));
                c->restart();
                QVERIFY(wait([&] { return c->ready(); }));
                c->openProject(path, "Saved");
                QVERIFY2(wait([&] { return !c->outstanding() && sketches(c) == accepted; }),
                         qPrintable(c->state().message));
            }
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
    app.setApplicationName("CoreModelingEditing");
    EditingContracts tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "editing_contracts.moc"
