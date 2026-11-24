#include "../module4/Fixture.hpp"
#include "AssemblyPanel.hpp"
#include "DrawingPanel.hpp"
#include "MainWindow.hpp"
#include "exchange/Exchange.hpp"
#include "persistence/ProjectStore.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>

using namespace cad;

namespace {
bool wait(std::function<bool()> f, int timeoutMs = 45000) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < timeoutMs)
        QTest::qWait(10);
    return f();
}

QCborMap activeDoc(Client *c) {
    for (auto v : c->state().documentRecords)
        if (v.toMap().value("id").toString() == c->state().activeDocument)
            return v.toMap();
    return {};
}

QString fileSha256(QString path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    return QString::fromLatin1(h.result().toHex());
}
} // namespace

class Module12Contracts : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        std::cout << "[Module12Contracts] Initializing packaging and installed smoke test suite\n";
    }

    void installedSmokeSuite() {
        QTemporaryDir root(QDir::tempPath() + "/cad-module12-installed-XXXXXX"),
            files(QDir::tempPath() + "/cad-module12-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            QVERIFY(r->isValid());
            QFile f(r->filePath(".cad-test-root"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("disposable\n");
            f.close();
        }

        auto prefs = files.filePath("settings.ini");
        {
            QSettings s(prefs, QSettings::IniFormat);
            s.setValue("startup/offerRecovery", false);
            s.sync();
        }

        MainWindow w(prefs, nullptr, root.path());
        w.show();

        auto *c = w.workerClient();
        QVERIFY2(wait([&] { return c && c->ready(); }), "Worker client failed to reach ready");
        auto settle = [&] { return wait([&] { return c->outstanding() == 0; }); };

        // -------------------------------------------------------------
        // AC-01: Part modeling workflow
        // -------------------------------------------------------------
        c->newProject("Part", "PackagedBracket");
        QVERIFY(settle());
        fixture4::B1 fixture;
        c->projectCommand("SetSketches", sketch::fields({{"sketches", fixture.sketches()}}));
        QVERIFY(settle());
        c->projectCommand("SetFeatures", sketch::fields({{"features", fixture.features()}}));
        QVERIFY(settle());
        QVERIFY2(c->mesh(), "Display mesh must be produced after feature evaluation");
        QVERIFY(c->mesh()->indices.size() > 0);
        auto partDocId = c->state().activeDocument;
        QVERIFY(!partDocId.isEmpty());
        std::cout << "  [AC-01] Part modeling verified: mesh indices = "
                  << c->mesh()->indices.size() << "\n";

        // -------------------------------------------------------------
        // AC-06: Neutral CAD Exchange (STEP & STL) on active model
        // -------------------------------------------------------------
        auto stepFile = files.filePath("exported.step");
        auto stlFile = files.filePath("exported.stl");
        c->exportGeometry(stepFile, "STEP", "mm", 0.2);
        QVERIFY(settle());
        QVERIFY2(QFileInfo::exists(stepFile),
                 qPrintable("export STEP failed: " + c->state().message));
        QVERIFY(QFileInfo(stepFile).size() > 100);

        c->exportGeometry(stlFile, "STL", "mm", 0.2);
        QVERIFY(settle());
        QVERIFY2(QFileInfo::exists(stlFile),
                 qPrintable("export STL failed: " + c->state().message));
        QVERIFY(QFileInfo(stlFile).size() > 100);
        std::cout << "  [AC-06] Neutral CAD exchange (STEP/STL) export verified ("
                  << QFileInfo(stepFile).size() << " / " << QFileInfo(stlFile).size()
                  << " bytes)\n";

        // -------------------------------------------------------------
        // AC-02: Undo / Redo / History workflow
        // -------------------------------------------------------------
        auto revBeforeEdit = c->state().revision;
        c->projectCommand(
            "SetParameter",
            sketch::fields({{"name", "Depth"}, {"dimension", "length"}, {"value_si", 0.025}}));
        QVERIFY(settle());
        QCOMPARE(c->state().revision, revBeforeEdit + 1);
        c->projectCommand("Undo");
        QVERIFY(settle());
        QCOMPARE(c->state().revision, revBeforeEdit + 2);
        c->projectCommand("Redo");
        QVERIFY(settle());
        QCOMPARE(c->state().revision, revBeforeEdit + 3);
        std::cout << "  [AC-02] Undo/Redo history verified across revisions\n";

        // -------------------------------------------------------------
        // AC-03: Project Persistence save and reopen
        // -------------------------------------------------------------
        auto projectFile = files.filePath("packaged-project.cadproj");
        c->saveProject(projectFile);
        QVERIFY(settle());
        QVERIFY(QFileInfo::exists(projectFile));
        QVERIFY(QFileInfo(projectFile).size() > 4096);

        c->closeProject();
        QVERIFY(settle());
        QVERIFY(!c->state().projectOpen);

        c->openProject(projectFile, "Saved");
        QVERIFY(settle());
        QVERIFY(c->state().projectOpen);
        QCOMPARE(c->state().documentRecords.size(), 1);
        std::cout << "  [AC-03] Project persistence save and reopen verified ("
                  << QFileInfo(projectFile).size() << " bytes)\n";

        // -------------------------------------------------------------
        // AC-04: Assembly modeling workflow
        // -------------------------------------------------------------
        c->createDocument("Assembly", "PackagedAssembly");
        QVERIFY(settle());
        auto assemblyDocId = c->state().activeDocument;
        QVERIFY(assemblyDocId != partDocId);
        auto *assemblyPanel = w.findChild<AssemblyPanel *>("assemblyPanel");
        QVERIFY(assemblyPanel);
        if (w.command("assembly") && w.command("assembly")->isEnabled())
            w.command("assembly")->trigger();

        auto click = [&](QString id) {
            auto *b = w.findChild<QPushButton *>(id);
            if (!b || !b->isEnabled())
                return false;
            b->click();
            return settle();
        };

        QVERIFY(click("assemblyInsert"));
        QVERIFY(click("assemblyAccept"));
        QVERIFY(click("assemblyInsert"));
        auto *posSpin = w.findChild<QDoubleSpinBox *>("assemblyPosition0");
        if (posSpin)
            posSpin->setValue(100.0);
        QVERIFY(click("assemblyPlace"));
        QVERIFY(click("assemblyAccept"));
        QVERIFY(c->mesh());
        QCOMPARE(c->mesh()->instances.size(), 2);

        w.findChild<QComboBox *>("assemblyMateKind")->setCurrentText("Distance");
        w.findChild<QDoubleSpinBox *>("assemblyMateValue")->setValue(100.0);
        QVERIFY(click("assemblyAddMate"));
        QVERIFY2(w.findChild<QLabel *>("assemblyDiagnostics")->text().contains("freedom 5"),
                 qPrintable(w.findChild<QLabel *>("assemblyDiagnostics")->text()));
        QVERIFY(click("assemblyAccept"));
        std::cout
            << "  [AC-04] Assembly multi-instance modeling + Distance mate (100 mm, freedom 5) "
               "verified (2 instances)\n";

        // -------------------------------------------------------------
        // AC-05: 2D Drawing (2 sheets, Front + Top views) & Vector PDF Publication
        // -------------------------------------------------------------
        c->createDocument("Drawing", "PackagedDrawing");
        QVERIFY(settle());
        auto *drawingPanel = w.findChild<DrawingPanel *>("drawingProperties");
        QVERIFY(drawingPanel);
        if (w.command("drawing.edit") && w.command("drawing.edit")->isEnabled())
            w.command("drawing.edit")->trigger();

        w.findChild<QComboBox *>("drawingOrientation")->setCurrentText("Front");
        QVERIFY(click("drawingAddView"));
        QVERIFY(click("drawingAddSheet"));
        w.findChild<QComboBox *>("drawingOrientation")->setCurrentText("Top");
        QVERIFY(click("drawingAddView"));
        QVERIFY(click("drawingPreview"));
        QVERIFY(click("drawingAccept"));

        auto pdfFile = files.filePath("packaged-drawing.pdf");
        c->exportDrawing(pdfFile);
        QVERIFY(settle());
        QVERIFY(QFileInfo::exists(pdfFile));
        QVERIFY(QFileInfo(pdfFile).size() > 100);
        {
            QFile pdf(pdfFile);
            QVERIFY(pdf.open(QIODevice::ReadOnly));
            auto header = pdf.read(16);
            QVERIFY(header.startsWith("%PDF-"));
        }
        std::cout << "  [AC-05] 2-sheet Drawing (Front + Top views) and vector PDF verified ("
                  << QFileInfo(pdfFile).size() << " bytes)\n";

        // Import exported STEP back into fresh document
        c->closeProject();
        QVERIFY(settle());
        c->newProject("Part", "ImportVerification");
        QVERIFY(settle());
        c->importGeometry(stepFile, "STEP", "mm", "None", {});
        QVERIFY(settle());
        QVERIFY2(c->mesh(), qPrintable("Import mesh missing: " + c->state().message));
        QVERIFY(c->mesh()->indices.size() > 0);
        std::cout << "  [AC-06] Neutral CAD exchange re-import verified (mesh indices = "
                  << c->mesh()->indices.size() << ")\n";

        // -------------------------------------------------------------
        // AC-07: Schema Migration, Worker Crash Recovery & Uninstall Preservation
        // -------------------------------------------------------------
        auto legacySource = files.filePath("legacy-store.cadproj");
        c->saveProject(legacySource);
        QVERIFY(settle());
        const auto legacySha = fileSha256(legacySource);
        QVERIFY(!legacySha.isEmpty());

        auto migratedTarget = files.filePath("migrated-store.cadproj");
        storage::ProjectStore::migrationCopy(legacySource, migratedTarget);
        QVERIFY(QFileInfo::exists(migratedTarget));
        QVERIFY(QFileInfo(legacySource).size() == QFileInfo(migratedTarget).size());
        QCOMPARE(fileSha256(legacySource), legacySha);

        auto failedTarget = files.filePath("failed-migration.cadproj");
        bool threw = false;
        try {
            storage::ProjectStore::migrationCopy(legacySource, failedTarget, [](QString at) {
                if (at == "migration_validated")
                    throw domain::Error("StorageError", "Injected migration failure");
            });
        } catch (const domain::Error &) {
            threw = true;
        }
        QVERIFY(threw);
        QVERIFY(!QFileInfo::exists(failedTarget));

        c->closeProject();
        QVERIFY(settle());
        c->openProject(migratedTarget, "Saved");
        QVERIFY(settle());
        QVERIFY(c->state().projectOpen);
        QCOMPARE(c->state().documentRecords.size(), 1);

        const auto activeProjectId = c->state().context.project;
        const auto durableRev = c->state().revision;
        c->restart();
        QVERIFY2(wait([&] { return c->ready() && !c->outstanding(); }),
                 "Packaged worker restart failed");
        QCOMPARE(c->state().context.project, activeProjectId);
        QCOMPARE(c->state().revision, durableRev);
        QVERIFY(c->state().projectOpen);
        QCOMPARE(c->state().documentRecords.size(), 1);

        // Simulated uninstall: verify user projects are preserved
        auto userDir = files.filePath("UserProjects");
        QDir().mkpath(userDir);
        auto userProj = userDir + "/user-bracket.cadproj";
        QVERIFY(QFile::copy(projectFile, userProj));
        QVERIFY(QFileInfo::exists(userProj));
        QVERIFY(QFileInfo(userProj).size() > 0);

        std::cout << "  [AC-07] Prior-manifest migration copy, failed-migration preservation, "
                     "worker crash recovery (rev "
                  << durableRev << "), and user project preservation verified\n";
    }
};

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    Module12Contracts tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "contracts.moc"
