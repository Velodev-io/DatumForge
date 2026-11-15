#include "MainWindow.hpp"
#include "exchange/Exchange.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
using namespace cad;
namespace {
bool wait(std::function<bool()> predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 30000)
        QTest::qWait(5);
    return predicate();
}
QCborMap document(Client *c) {
    for (auto value : c->state().documentRecords)
        if (value.toMap().value("id").toString() == c->state().activeDocument)
            return value.toMap();
    return {};
}
QString sha(QString path) {
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&input))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}
} // namespace
class ExchangeContracts : public QObject {
    Q_OBJECT
  private slots:
    void lifecycle() {
        exercise(false);
    }
    void nativeExchange() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        exercise(true);
    }

  private:
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module6-native-XXXXXX"),
            files(QDir::tempPath() + "/cad-module6-files-XXXXXX");
        for (auto *dir : {&root, &files}) {
            QVERIFY(dir->isValid());
            QFile mark(dir->filePath(".cad-test-root"));
            QVERIFY(mark.open(QIODevice::WriteOnly));
            mark.write("disposable\n");
        }
        auto prefs = files.filePath("settings.ini");
        {
            QSettings settings(prefs, QSettings::IniFormat);
            settings.setValue("startup/offerRecovery", false);
            settings.sync();
        }
        auto source = files.filePath("source.step");
        exchange::write(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), source, "STEP", "mm", .2);
        MainWindow w(prefs, nullptr, root.path());
        w.show();
        QSignalSpy graphics(w.sceneView(), &Viewport::graphicsFailed);
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            w.raise();
            w.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&w));
        }
        auto *c = w.workerClient();
        QVERIFY(wait([&] { return c->ready(); }));
        auto settle = [&] { return wait([&] { return !c->outstanding(); }); };
        c->newProject("Part", "Module 6 exchange");
        QVERIFY(settle());
        auto *page = w.findChild<QWidget *>("exchangeProperties");
        QVERIFY(page);
        QVERIFY(w.command("exchange")->isEnabled());
        w.command("exchange")->trigger();
        QVERIFY(page->isVisible());
        auto *formatChoice = w.findChild<QComboBox *>("exchangeFormat");
        formatChoice->setCurrentText("STL");
        QVERIFY(!w.findChild<QPushButton *>("exchangeImport")->isEnabled());
        formatChoice->setCurrentText("STEP");
        QVERIFY(w.findChild<QPushButton *>("exchangeImport")->isEnabled());
        auto clickFile = [&](QString button, QString path) {
            qInfo() << "File workflow" << button << path;
            auto *control = w.findChild<QPushButton *>(button);
            QVERIFY(control);
            QVERIFY(control->isEnabled());
            bool chosen = false;
            QTimer::singleShot(50, &w, [&] {
                for (auto *top : QApplication::topLevelWidgets())
                    if (auto *dialog = qobject_cast<QFileDialog *>(top)) {
                        dialog->setDirectory(QFileInfo(path).absolutePath());
                        chosen = true;
                        QTimer::singleShot(400, dialog, [dialog, path] {
                            auto *editor = dialog->findChild<QLineEdit *>("fileNameEdit");
                            if (editor) {
                                editor->setFocus();
                                editor->setText(path);
                            }
                            qInfo() << "Picker selection" << dialog->directory().absolutePath()
                                    << dialog->selectedFiles();
                            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                            qInfo() << "Picker result" << dialog->result() << dialog->isVisible();
                        });
                        return;
                    }
            });
            QTimer::singleShot(5000, &w, [] {
                for (auto *top : QApplication::topLevelWidgets())
                    if (auto *dialog = qobject_cast<QFileDialog *>(top))
                        dialog->reject();
            });
            control->click();
            QVERIFY(chosen);
            QVERIFY(settle());
            qInfo() << "Workflow settled" << button << c->state().message;
        };
        clickFile("exchangeImport", source);
        QVERIFY2(document(c).value("features").toArray().size() == 1,
                 qPrintable(c->state().message));
        QVERIFY(c->mesh());
        QCOMPARE(w.findChild<QProgressBar *>("exchangeProgress")->value(), 100);
        QVERIFY(page->isVisible());
        QVERIFY(w.findChild<QLabel *>("exchangeReport")->text().contains("Retained source"));
        auto *units = w.findChild<QComboBox *>("exchangeUnits");
        units->setCurrentText("in");
        auto output = files.filePath("export.step");
        clickFile("exchangeExport", output);
        QVERIFY(QFileInfo::exists(output));
        QVERIFY(w.findChild<QLabel *>("exchangeReport")->text().contains("Published STEP"));
        auto result = exchange::read(output, "STEP");
        QVERIFY(std::abs(exchange::invariants(result.shape).value("volume_mm3").toDouble() - 6000) <
                1e-4);
        auto copy = files.filePath("portable.cadproj");
        auto original = c->state().context.project;
        clickFile("exchangeCopy", copy);
        QVERIFY(QFileInfo::exists(copy));
        QCOMPARE(c->state().context.project, original);
        w.findChild<QPushButton *>("exchangeInspect")->click();
        QVERIFY(settle());
        QVERIFY(w.findChild<QLabel *>("exchangeReport")->text().contains("1 source objects"));
        QVERIFY(QFile::remove(source));
        c->closeProject();
        QVERIFY(settle());
        c->openProject(copy);
        QVERIFY(settle());
        QVERIFY(c->state().context.project != original);
        QVERIFY(c->mesh());
        w.command("exchange")->trigger();
        QVERIFY(page->isVisible());
        if (native) {
            auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(!directory.isEmpty());
            w.grab().save(directory + "/exchange.png");
            QVERIFY(graphics.empty());
            QVERIFY(w.sceneView()->graphicsDescription().contains("initialized 1"));
            QFile evidence(directory + "/exchange-native.json");
            QVERIFY(evidence.open(QIODevice::WriteOnly));
            evidence.write(
                QJsonDocument(
                    QJsonObject{
                        {"status", "Pass"},
                        {"dpr", w.devicePixelRatioF()},
                        {"revision", c->state().revision},
                        {"renderer", w.sceneView()->graphicsDescription()},
                        {"scope",
                         "Qt file pickers on native Cocoa widgets; source import, unit-tagged "
                         "export, closure inspection, portable copy and source-independent reopen"},
                        {"physical_device", "Blocked: automated input"}})
                    .toJson());
        }
        // A sizable source makes cancellation observable through the real responsive IPC loop.
        auto large = files.filePath("cancel.step");
        {
            QFile input(output), out(large);
            QVERIFY(input.open(QIODevice::ReadOnly));
            QVERIFY(out.open(QIODevice::WriteOnly));
            auto data = input.readAll();
            auto pos = data.indexOf("DATA;") + 5;
            QVERIFY(pos >= 5);
            out.write(data.left(pos));
            QByteArray comment = "\n/*" + QByteArray(1990, 'x') + "*/";
            for (int i = 0; i < 25000; ++i)
                out.write(comment);
            out.write(data.mid(pos));
        }
        auto revision = c->state().revision;
        bool requested = false;
        auto connection = connect(c, &Client::exchangeProgress, &w, [&](QString, int) {
            if (!requested) {
                requested = true;
                c->cancelExchange();
            }
        });
        c->importGeometry(large, "STEP", "File", "None", sha(large));
        QVERIFY(settle());
        disconnect(connection);
        QVERIFY(requested);
        QCOMPARE(c->state().revision, revision);
        QVERIFY2(c->state().message.contains("Canceled"), qPrintable(c->state().message));
        auto durableProject = c->state().context.project;
        c->restart();
        QVERIFY(wait([&] { return c->ready() && !c->outstanding(); }));
        QCOMPARE(c->state().context.project, durableProject);
        QVERIFY(c->mesh());
        c->closeProject();
        QVERIFY(settle());
        w.close();
    }
};
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    ExchangeContracts test;
    return QTest::qExec(&test, argc, argv);
}
#include "contracts.moc"
