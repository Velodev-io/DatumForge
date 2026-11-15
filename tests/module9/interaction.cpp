#include "DrawingPanel.hpp"
#include "MainWindow.hpp"
#include "render/Viewport.hpp"
#include <QAccessible>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QKeySequenceEdit>
#include <QListWidget>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
using namespace cad;
namespace {
bool wait(std::function<bool()> f) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 30000)
        QTest::qWait(10);
    return f();
}
} // namespace
class Interaction : public QObject {
    Q_OBJECT
  private slots:
    void lifecycle() {
        exercise(false);
    }
    void nativeInteraction() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Native session required");
        exercise(true);
    }

  private:
    void exercise(bool native) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module9-native-XXXXXX"),
            files(QDir::tempPath() + "/cad-module9-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            QVERIFY(r->isValid());
            QFile f(r->filePath(".cad-test-root"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("disposable\n");
        }
        auto prefs = files.filePath("prefs.ini");
        {
            QSettings s(prefs, QSettings::IniFormat);
            s.setValue("startup/offerRecovery", false);
        }
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
        auto settle = [&] { return wait([&] { return !c->outstanding(); }); };
        // Native keyboard creates a Drawing using the actual new-project dialog.
        QTimer::singleShot(100, &w, [&] {
            auto *dialog = w.findChild<QDialog *>("newDocumentDialog");
            QVERIFY(dialog);
            auto *type = dialog->findChild<QComboBox *>("documentTypeInput");
            type->setFocus();
            QTest::keyClick(type, Qt::Key_End);
            auto *name = dialog->findChild<QLineEdit *>("documentNameInput");
            name->setFocus();
            QTest::keySequence(name, QKeySequence::SelectAll);
            QTest::keyClicks(name, "Keyboard drawing");
            QTest::keyClick(name, Qt::Key_Return);
        });
        QTest::keySequence(&w, QKeySequence::New);
        QVERIFY(wait([&] { return c->state().projectOpen; }));
        QVERIFY(settle());
        QCOMPARE(c->state().documentRecords[0].toMap().value("type").toString(),
                 QString("Drawing"));
        w.activateWindow();
        w.setFocus();
        if (native)
            QVERIFY(QTest::qWaitForWindowActive(&w));
        else
            QApplication::setActiveWindow(&w);
        QTest::qWait(30);
        QTest::keySequence(&w, QKeySequence::Find);
        QVERIFY(wait([&] { return w.findChild<QDialog *>("commandPalette"); }));
        auto *palette = w.findChild<QDialog *>("commandPalette");
        auto *query = palette->findChild<QLineEdit *>("paletteQuery");
        QTest::keyClicks(query, "drawing.edit");
        QTest::keyClick(query, Qt::Key_Return);
        QVERIFY(wait([&] { return w.findChild<DrawingPanel *>()->active(); }));
        w.activateWindow();
        if (native)
            QVERIFY(QTest::qWaitForWindowActive(&w));
        else
            QApplication::setActiveWindow(&w);
        QTest::qWait(30);
        auto *name = w.findChild<QLineEdit *>("drawingSheetName");
        name->setFocus();
        QTest::keySequence(name, QKeySequence::SelectAll);
        QTest::keyClicks(name, "Keyboard sheet");
        auto *apply = w.findChild<QPushButton *>("drawingSaveSheet");
        apply->setFocus();
        QTest::keyClick(apply, Qt::Key_Space);
        auto rev = c->state().revision;
        auto *accept = w.findChild<QPushButton *>("drawingAccept");
        accept->setFocus();
        QTest::qWait(30);
        QVERIFY(QApplication::focusWidget() == accept);
        QTest::keyClick(accept, Qt::Key_Return);
        QVERIFY(wait(
            [&] { return c->state().revision == rev + 1 || c->state().phase == Phase::Failed; }));
        QVERIFY2(settle(), qPrintable(c->state().message));
        QCOMPARE(c->state().revision, rev + 1);
        auto *panel = w.findChild<DrawingPanel *>();
        QVERIFY(!panel->active());
        w.command("drawing.edit")->trigger();
        QVERIFY(panel->active());
        name->setFocus();
        QTest::keyClick(name, Qt::Key_Escape);
        QVERIFY(settle());
        QVERIFY(!panel->active());
        QCOMPARE(c->state().revision, rev + 1);
        auto *accessible = QAccessible::queryAccessibleInterface(name);
        QVERIFY(accessible);
        QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
        QCOMPARE(accessible->role(), QAccessible::EditableText);
        w.command("drawing.edit")->trigger();
        QVERIFY(panel->active());
        name->setFocus();
        QWidget *before = QApplication::focusWidget();
        QVERIFY(before == name);
        QTest::keyClick(before, Qt::Key_Tab);
        QVERIFY(QApplication::focusWidget() != before);
        panel->discard();
        QVERIFY(settle());
        auto saved = files.filePath("keyboard.cadproj");
        c->saveProject(saved);
        QVERIFY(settle());
        QTest::keySequence(&w, QKeySequence::Save);
        QVERIFY(settle());
        QVERIFY(QFileInfo(saved).size() > 0);
        auto semantic = c->state().revision;
        w.command("layout.simplified")->trigger();
        QVERIFY(!w.findChild<QDockWidget *>("resourcesDock")->isVisible());
        QCOMPARE(c->state().revision, semantic);
        w.command("layout.simplified")->trigger();
        QCOMPARE(c->state().revision, semantic);
        // Native shortcut preferences retain the same QAction, with conflict refusal.
        QTimer::singleShot(100, &w, [&] {
            auto *dialog = w.findChild<QDialog *>("shortcutPreferences");
            QVERIFY(dialog);
            auto editors = dialog->findChildren<QKeySequenceEdit *>();
            QVERIFY(editors.size() > 2);
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            editors[0]->setKeySequence(QKeySequence("Alt+8"));
            editors[1]->setKeySequence(QKeySequence("Alt+8"));
            buttons->button(QDialogButtonBox::Ok)->click();
            QVERIFY(dialog->isVisible());
            editors[1]->setKeySequence(QKeySequence("Alt+9"));
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        w.command("commands.shortcuts")->trigger();
        if (native) {
            auto out = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            QVERIFY(QDir().mkpath(out));
            QVERIFY(w.grab().save(out + "/interaction.png"));
            w.command("drawing.edit")->trigger();
            QVERIFY(panel->active());
            QVERIFY(accept->isVisible());
            QVERIFY(w.rect().contains(accept->mapTo(&w, accept->rect().center())));
            panel->discard();
            QVERIFY(settle());
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
    f.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    Interaction t;
    return QTest::qExec(&t, argc, argv);
}
#include "interaction.moc"
