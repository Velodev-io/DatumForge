#include "MainWindow.hpp"
#include "protocol/Client.hpp"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
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
void mark(QString root) {
    QFile f(root + "/.cad-test-root");
    if (!f.open(QIODevice::WriteOnly))
        qFatal("Cannot mark disposable root");
    f.write("disposable\n");
}
QCborMap fields(std::initializer_list<std::pair<QString, QCborValue>> list) {
    QCborMap m;
    for (auto f : list)
        m.insert(f.first, f.second);
    return m;
}
QString worker() {
    return QCoreApplication::applicationDirPath() + "/cad-worker";
}
bool ready(Client &c) {
    return c.state().projectOpen && c.state().phase == Phase::Ready && c.outstanding() == 0;
}
void acceptDocument(QString name, QString kind = "Part") {
    QTimer::singleShot(20, [name, kind] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        dialog->findChild<QLineEdit *>("documentNameInput")->setText(name);
        dialog->findChild<QComboBox *>("documentTypeInput")->setCurrentText(kind);
        QTest::mouseClick(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),
                          Qt::LeftButton);
    });
}
void keepRecoverable() {
    QTimer::singleShot(20, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!box)
            return;
        for (auto *button : box->buttons())
            if (button->text() == "Keep recoverable") {
                QTest::mouseClick(button, Qt::LeftButton);
                return;
            }
    });
}
} // namespace
class Contracts : public QObject {
    Q_OBJECT
  private slots:
    void lifecycle() {
        QTemporaryDir root(QDir::tempPath() + "/cad-module2-ipc-XXXXXX"),
            files(QDir::tempPath() + "/cad-module2-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        QString project, operation, part, assembly, drawing;
        auto path = files.filePath("project.cadproj");
        {
            Client c(nullptr, worker(), root.path());
            c.start();
            QVERIFY(wait([&] { return c.ready(); }));
            c.newProject("Part", "Fixture part");
            QVERIFY2(wait([&] { return ready(c); }), qPrintable(c.state().message));
            project = c.state().context.project;
            part = c.state().activeDocument;
            QCOMPARE(c.state().revision, 1);
            QVERIFY(c.state().savedRevision < 0);
            c.createDocument("Assembly", "Fixture assembly");
            QVERIFY(wait([&] { return ready(c) && c.state().documentRecords.size() == 2; }));
            assembly = c.state().activeDocument;
            c.createDocument("Drawing", "Fixture drawing");
            QVERIFY(wait([&] { return ready(c) && c.state().documentRecords.size() == 3; }));
            drawing = c.state().activeDocument;
            c.switchDocument(part);
            QVERIFY(wait([&] { return ready(c) && c.state().activeDocument == part; }));
            c.projectCommand(
                "SetParameter",
                fields({{"name", "Thickness"}, {"dimension", "length"}, {"value_si", .005}}));
            QVERIFY(wait([&] { return ready(c) && c.state().revision == 4; }));
            operation = c.latestOperation();
            c.saveProject(path);
            QVERIFY2(wait([&] { return ready(c) && c.state().savedRevision == 4; }),
                     qPrintable(c.state().message));
            c.projectCommand("RenameDocument", fields({{"name", "Changed part"}}));
            QVERIFY(wait([&] { return ready(c) && c.state().revision == 5; }));
            c.projectCommand("Undo");
            QVERIFY(wait([&] { return ready(c) && c.state().revision == 6; }));
            c.projectCommand("Redo");
            QVERIFY(wait([&] { return ready(c) && c.state().revision == 7; }));
            c.projectCommand(
                "SetParameter",
                fields({{"name", "bad();"}, {"dimension", "length"}, {"value_si", 1.}}));
            QVERIFY(wait([&] { return c.state().phase == Phase::Failed && c.outstanding() == 0; }));
            QCOMPARE(c.state().revision, 7);
            QVERIFY(c.state().message.contains("InvalidInput"));
#ifdef Q_OS_UNIX
            QVERIFY(::kill(pid_t(c.workerPid()), SIGKILL) == 0);
            QVERIFY(wait([&] { return c.state().phase == Phase::WorkerUnavailable; }));
            c.restart();
            QVERIFY2(wait([&] { return ready(c) && c.state().revision == 7; }),
                     qPrintable(c.state().message));
            QCOMPARE(c.state().savedRevision, 4);
#endif
            QSignalSpy outcomes(&c, &Client::operationOutcome);
            c.queryOperation(operation);
            QVERIFY(wait([&] { return outcomes.size() == 1; }));
            QCOMPARE(outcomes[0][1].toString(), "DurablyRecorded");
            c.closeDocument();
            QVERIFY(wait([&] { return ready(c) && c.state().activeDocument != part; }));
            QCOMPARE(c.state().documentRecords.size(), 3);
            c.switchDocument(part);
            QVERIFY(wait([&] { return ready(c) && c.state().activeDocument == part; }));
        }
        {
            Client restarted(nullptr, worker(), root.path());
            restarted.start();
            QVERIFY(wait([&] { return restarted.ready(); }));
            QSignalSpy offers(&restarted, &Client::recoveryOffers);
            restarted.listRecoveries();
            QVERIFY(wait([&] { return offers.size() == 1; }));
            QCOMPARE(offers[0][0].value<QCborArray>()[0].toMap().value("revision").toInteger(), 7);
            restarted.openProject(path, "Saved");
            QVERIFY2(wait([&] { return ready(restarted); }), qPrintable(restarted.state().message));
            QCOMPARE(restarted.state().revision, 4);
            QCOMPARE(restarted.state().documentRecords[0].toMap().value("id").toString(), part);
            auto copy = files.filePath("copy.cadproj");
            restarted.saveProject(copy, true);
            QVERIFY2(wait([&] {
                         return ready(restarted) && restarted.state().context.project != project;
                     }),
                     qPrintable(restarted.state().message));
            auto copyId = restarted.state().context.project;
            Client original(nullptr, worker(), root.path());
            original.start();
            QVERIFY(wait([&] { return original.ready(); }));
            original.openProject(path, "Saved");
            QVERIFY2(wait([&] { return ready(original); }), qPrintable(original.state().message));
            QCOMPARE(original.state().context.project, project);
            QCOMPARE(restarted.state().context.project, copyId);
        }
    }
    void nativeDocuments() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Blocked: native desktop session required");
        QTemporaryDir root(QDir::tempPath() + "/cad-module2-native-XXXXXX"),
            files(QDir::tempPath() + "/cad-module2-files-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        auto preferences = files.filePath("preferences.ini");
        QSettings settings(preferences, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow window(preferences, nullptr, root.path());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.raise();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *c = window.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        acceptDocument("Native part");
        window.command("new")->trigger();
        QVERIFY2(wait([&] { return ready(*c); }), qPrintable(c->state().message));
        auto part = c->state().activeDocument;
        window.findChild<QTabWidget *>("managerTabs")->setCurrentIndex(2);
        auto *name = window.findChild<QLineEdit *>("parameterNameInput");
        QVERIFY(name);
        name->setText("Depth");
        window.findChild<QDoubleSpinBox *>("parameterValueInput")->setValue(25);
        window.findChild<QComboBox *>("parameterUnitInput")->setCurrentText("mm");
        QTest::mouseClick(window.findChild<QPushButton *>("setParameterButton"), Qt::LeftButton);
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == 2; }));
        QCOMPARE(c->state()
                     .documentRecords[0]
                     .toMap()
                     .value("parameters")
                     .toArray()[0]
                     .toMap()
                     .value("value_si")
                     .toDouble(),
                 .025);
        name->setText("bad();");
        QTest::mouseClick(window.findChild<QPushButton *>("setParameterButton"), Qt::LeftButton);
        QVERIFY(wait([&] { return c->state().phase == Phase::Failed && c->outstanding() == 0; }));
        QCOMPARE(c->state().revision, 2);
        QCOMPARE(name->text(), QString("bad();"));
        name->setText("Depth");
        window.command("undo")->trigger();
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == 3; }));
        window.command("redo")->trigger();
        QVERIFY(wait([&] { return ready(*c) && c->state().revision == 4; }));
        acceptDocument("Native assembly", "Assembly");
        window.command("document.new")->trigger();
        QVERIFY(wait([&] { return ready(*c) && c->state().documentRecords.size() == 2; }));
        auto assembly = c->state().activeDocument;
        acceptDocument("Native drawing", "Drawing");
        window.command("document.new")->trigger();
        QVERIFY(wait([&] { return ready(*c) && c->state().documentRecords.size() == 3; }));
        auto *tabs = window.findChild<QTabBar *>("documentTabs");
        QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(0).center());
        QVERIFY(wait([&] { return ready(*c) && c->state().activeDocument == part; }));
        keepRecoverable();
        window.command("close")->trigger();
        QVERIFY(wait([&] { return ready(*c) && !c->state().openDocuments.contains(part); }));
        c->switchDocument(assembly);
        QVERIFY(wait([&] { return ready(*c) && c->state().activeDocument == assembly; }));
        auto path = files.filePath("native.cadproj");
        c->saveProject(path);
        QVERIFY2(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }),
                 qPrintable(c->state().message));
        window.findChild<QLineEdit *>("documentNameEditor")->setText("Renamed assembly");
        QTest::mouseClick(window.findChild<QPushButton *>("renameDocumentButton"), Qt::LeftButton);
        QVERIFY(wait([&] { return ready(*c) && c->state().revision > c->state().savedRevision; }));
        window.command("save")->trigger();
        QVERIFY(wait([&] { return ready(*c) && c->state().savedRevision == c->state().revision; }));
        QVERIFY(!window.command("sketch")->isEnabled());
        QVERIFY(!window.command("extrude")->isEnabled());
        auto artifact = qEnvironmentVariable("CAD_ARTIFACT_DIR");
        QVERIFY(!artifact.isEmpty());
        QDir().mkpath(artifact);
        QVERIFY(window.grab().save(artifact + "/module2-native.png"));
        QFile result(artifact + "/native.json");
        QVERIFY(result.open(QIODevice::WriteOnly));
        result.write(QJsonDocument(QJsonObject{{"platform", QGuiApplication::platformName()},
                                               {"window_width", window.width()},
                                               {"window_height", window.height()},
                                               {"DPR", window.devicePixelRatioF()},
                                               {"project_id", c->state().context.project},
                                               {"revision", c->state().revision},
                                               {"saved_revision", c->state().savedRevision},
                                               {"documents", c->state().documentRecords.size()}})
                         .toJson());
        c->projectCommand("RenameDocument", fields({{"name", "Recoverable newer assembly"}}));
        QVERIFY(wait([&] { return ready(*c) && c->state().revision > c->state().savedRevision; }));
        auto recoverable = c->state().revision, saved = c->state().savedRevision;
        keepRecoverable();
        window.command("project.close")->trigger();
        QVERIFY(wait([&] { return !c->state().projectOpen && c->outstanding() == 0; }));
        window.close();
        settings.setValue("startup/offerRecovery", true);
        settings.sync();
        bool offered = false;
        QTimer chooser;
        chooser.setInterval(10);
        connect(&chooser, &QTimer::timeout, [&] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            offered = true;
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
            chooser.stop();
        });
        chooser.start();
        MainWindow recovered(preferences, nullptr, root.path());
        recovered.show();
        QVERIFY(QTest::qWaitForWindowExposed(&recovered));
        QVERIFY2(wait([&] { return ready(*recovered.workerClient()); }),
                 qPrintable(recovered.state().message));
        QVERIFY(offered);
        QCOMPARE(recovered.state().revision, recoverable);
        QCOMPARE(recovered.state().savedRevision, saved);
        QVERIFY(recovered.grab().save(artifact + "/module2-recovered-native.png"));
        keepRecoverable();
        recovered.close();
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
    app.setApplicationName("Module2");
    Contracts tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "contracts.moc"
