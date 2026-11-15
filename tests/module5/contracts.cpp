#include "../module4/Fixture.hpp"
#include "HistoryPanel.hpp"
#include "MainWindow.hpp"
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
using namespace cad;
using sketch::fields;
namespace {
bool wait(std::function<bool()> f) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 30000)
        QTest::qWait(5);
    return f();
}
QCborMap document(Client *c) {
    for (auto v : c->state().documentRecords)
        if (v.toMap().value("id").toString() == c->state().activeDocument)
            return v.toMap();
    return {};
}
} // namespace
class HistoryContracts : public QObject {
    Q_OBJECT
  private slots:
    void lifecycle() {
        exercise(false);
    }
    void nativeHistory() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        exercise(true);
    }

  private:
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-native-XXXXXX"),
            files(QDir::tempPath() + "/cad-module5-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            QVERIFY(r->isValid());
            QFile f(r->filePath(".cad-test-root"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("disposable\n");
        }
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
        auto *panel = w.findChild<HistoryPanel *>();
        QVERIFY(panel);
        QVERIFY(wait([&] { return c->ready(); }));
        auto settle = [&] { return wait([&] { return !c->outstanding(); }); };
        auto click = [&](QString name) {
            auto *b = w.findChild<QPushButton *>(name);
            QVERIFY2(b, qPrintable(name));
            QVERIFY2(b->isEnabled(), qPrintable(name + " disabled: " + c->state().message));
            b->click();
        };
        auto combo = [&](QString name, QString value) {
            w.findChild<QComboBox *>(name)->setCurrentText(value);
        };
        auto select = [&](QString id) {
            auto *f = w.findChild<QComboBox *>("historyFeature");
            f->setCurrentIndex(f->findData(id));
        };
        c->newProject("Part", "Module 5 history");
        QVERIFY(settle());
        fixture4::B1 b;
        c->projectCommand("SetSketches", fields({{"sketches", b.sketches()}}));
        QVERIFY(settle());
        QCborMap info;
        connect(c, &Client::partResult, &w, [&](QCborMap v) { info = v; });
        c->projectCommand("SetFeatures", fields({{"features", features::encode({b.base})}}));
        QVERIFY(settle());
        QString edge;
        for (auto value : info.value("topology").toArray()) {
            auto m = value.toMap();
            auto p = m.value("center_si").toArray();
            if (m.value("token").toString().startsWith("edge/") &&
                std::abs(p[0].toDouble() - .04) < 1e-10 && std::abs(p[1].toDouble()) < 1e-10 &&
                std::abs(p[2].toDouble() - .01) < 1e-10)
                edge = m.value("token").toString();
        }
        QVERIFY(!edge.isEmpty());
        auto fillet = b.fillet(edge);
        fillet.parameters.insert(QStringLiteral("source"), b.base.id);
        fillet.parameters.insert(QStringLiteral("size"), .001);
        c->projectCommand("SetFeatures",
                          fields({{"features", features::encode({b.base, fillet})}}));
        QVERIFY(settle());
        auto original = c->state().revision;
        w.command("history")->trigger();
        QVERIFY(panel->active());
        select(fillet.id);
        combo("historyAction", "Rename");
        w.findChild<QLineEdit *>("historyName")->setText("Rounded edge");
        click("previewHistoryButton");
        QVERIFY(settle());
        QCOMPARE(c->state().revision, original);
        QVERIFY(!c->state().stale);
        click("acceptHistoryButton");
        QVERIFY(settle());
        QVERIFY(!panel->active());
        QCOMPARE(document(c).value("features").toArray().last().toMap().value("name").toString(),
                 QString("Rounded edge"));
        // Suppression impact remains a preview until explicit stale-result acknowledgment and
        // commit.
        w.command("history")->trigger();
        select(b.base.id);
        combo("historyAction", "Suppress");
        w.findChild<QCheckBox *>("historySuppressed")->setChecked(true);
        auto revision = c->state().revision;
        click("previewHistoryButton");
        QVERIFY(settle());
        QVERIFY(c->state().stale);
        QVERIFY(c->mesh());
        QVERIFY(!w.findChild<QPushButton *>("acceptHistoryButton")->isEnabled());
        if (native) {
            auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QDir().mkpath(out);
            QVERIFY(w.grab().save(out + "/history-impact.png"));
        }
        w.findChild<QCheckBox *>("acknowledgeStale")->setChecked(true);
        click("acceptHistoryButton");
        QVERIFY(settle());
        QCOMPARE(c->state().revision, revision + 1);
        QVERIFY(c->state().stale);
        QCOMPARE(document(c).value("feature_status").toString(), QString("Stale"));
        c->projectCommand("MeasurePart", fields({{"body", QString()},
                                                 {"token", QString()},
                                                 {"second_token", QString()},
                                                 {"density_kg_m3", 0.}}));
        QVERIFY(settle());
        QCOMPARE(c->state().phase, Phase::Failed);
        QVERIFY(c->state().message.contains("StaleResult"));
        c->projectCommand("Undo");
        QVERIFY(settle());
        QVERIFY(!c->state().stale);
        w.command("history")->trigger();
        select(fillet.id);
        combo("historyAction", "Reorder");
        w.findChild<QSpinBox *>("historyPosition")->setValue(1);
        revision = c->state().revision;
        click("previewHistoryButton");
        QVERIFY(settle());
        QCOMPARE(c->state().revision, revision);
        QVERIFY(!w.findChild<QPushButton *>("acceptHistoryButton")->isEnabled());
        click("cancelHistoryButton");
        QVERIFY(settle());
        w.command("history")->trigger();
        select(b.base.id);
        combo("historyAction", "Delete");
        w.findChild<QCheckBox *>("historyCascade")->setChecked(true);
        click("previewHistoryButton");
        QVERIFY(settle());
        QVERIFY(!c->mesh());
        click("acceptHistoryButton");
        QVERIFY(settle());
        QVERIFY(document(c).value("features").toArray().empty());
        c->projectCommand("Undo");
        QVERIFY(settle());
        QCOMPARE(document(c).value("features").toArray().size(), 2);
        // A unit-aware feature expression moves the top edge; preserve stale display and repair
        // explicitly.
        w.command("history")->trigger();
        select(b.base.id);
        combo("historyAction", "Expression");
        combo("historyParameter", "depth");
        w.findChild<QLineEdit *>("historyExpression")->setText("12 mm");
        click("previewHistoryButton");
        QVERIFY(settle());
        QVERIFY(c->state().stale);
        w.findChild<QCheckBox *>("acknowledgeStale")->setChecked(true);
        click("acceptHistoryButton");
        QVERIFY(settle());
        QVERIFY(c->state().stale);
        auto staleFile = files.filePath("stale.cadproj");
        c->saveProject(staleFile);
        QVERIFY(settle());
        c->closeProject();
        QVERIFY(settle());
        c->openProject(staleFile);
        QVERIFY(settle());
        QVERIFY2(c->state().stale, qPrintable(c->state().message));
        w.command("history")->trigger();
        select(fillet.id);
        click("inspectReferenceButton");
        QVERIFY(settle());
        QCOMPARE(w.findChild<QComboBox *>("referenceCandidates")->count(), 1);
        QVERIFY(w.findChild<QLabel *>("historyDiagnostic")->text().contains("Candidate"));
        QVERIFY(c->state().stale);
        QVERIFY(c->mesh());
        if (native) {
            auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QTest::qWait(50);
            QVERIFY(w.grab().save(out + "/history-repair.png"));
        }
        click("replaceReferenceButton");
        QVERIFY(settle());
        QVERIFY2(!c->state().stale, qPrintable(c->state().message));
        c->projectCommand("Undo");
        QVERIFY(settle());
        QVERIFY(c->state().stale);
        c->projectCommand("Redo");
        QVERIFY(settle());
        QVERIFY(!c->state().stale);
        auto path = files.filePath("repaired.cadproj");
        c->saveProject(path);
        QVERIFY(settle());
        auto saved = c->state().revision;
        c->projectCommand(
            "SetParameter",
            fields({{"name", "RecoveryProbe"}, {"dimension", "scalar"}, {"value_si", 1.}}));
        QVERIFY(settle());
        auto recovered = c->state().revision;
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
        QCOMPARE(c->state().revision, recovered);
        QCOMPARE(c->state().savedRevision, saved);
        QVERIFY(!c->state().stale);
        if (native) {
            QVERIFY(graphics.empty());
            QVERIFY(w.sceneView()->graphicsDescription().contains("initialized 1"));
            auto *accept = w.findChild<QPushButton *>("acceptHistoryButton");
            panel->begin(document(c), fillet.id);
            QVERIFY(accept->isVisible());
            QVERIFY(w.rect().contains(accept->mapTo(&w, QPoint(0, 0))));
            panel->discard();
            QVERIFY(settle());
            auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QFile evidence(out + "/history-native.json");
            QVERIFY(evidence.open(QIODevice::WriteOnly));
            evidence.write(
                QJsonDocument(QJsonObject{{"status", "Pass"},
                                          {"dpr", w.devicePixelRatioF()},
                                          {"saved_revision", saved},
                                          {"recovered_revision", recovered},
                                          {"renderer", w.sceneView()->graphicsDescription()},
                                          {"physical_device", "Blocked: automated input"}})
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
    HistoryContracts t;
    return QTest::qExec(&t, argc, argv);
}
#include "contracts.moc"
