#include "../module4/Fixture.hpp"
#include "protocol/Client.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
using namespace cad;
namespace {
bool wait(std::function<bool()> f) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 30000)
        QTest::qWait(5);
    return f();
}
} // namespace
class FaultContracts : public QObject {
    Q_OBJECT
  private slots:
    void cancellationAndRestart() {
        QTemporaryDir root(QDir::tempPath() + "/cad-module10-worker-XXXXXX"),
            files(QDir::tempPath() + "/cad-module10-files-XXXXXX");
        QVERIFY(root.isValid());
        QFile marker(root.filePath(".cad-test-root"));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("disposable\n");
        marker.close();
        QVERIFY(files.isValid());
        QFile outputMarker(files.filePath(".cad-test-root"));
        QVERIFY(outputMarker.open(QIODevice::WriteOnly));
        outputMarker.write("disposable\n");
        outputMarker.close();
        Client c(nullptr, {}, root.path());
        c.start();
        QVERIFY(wait([&] { return c.ready(); }));
        auto settle = [&] { return wait([&] { return !c.outstanding(); }); };
        c.newProject("Part", "Cancellation fixture");
        QVERIFY(settle());
        fixture4::B1 b;
        c.projectCommand("SetSketches", sketch::fields({{"sketches", b.sketches()}}));
        QVERIFY(settle());
        QSignalSpy terminal(&c, &Client::jobFinished);
        bool requested = false;
        auto connection = connect(&c, &Client::exchangeProgress, this, [&](QString stage, int) {
            if (!requested && stage == "Before compute") {
                requested = true;
                c.cancelExchange();
            }
        });
        auto base = c.state().revision;
        c.projectCommand("SetFeatures", sketch::fields({{"features", b.features()}}));
        auto operation = c.latestOperation();
        QVERIFY(settle());
        QVERIFY(requested);
        QVERIFY(c.state().revision == base || c.state().revision == base + 1);
        bool committed = c.state().revision == base + 1;
        QSignalSpy queried(&c, &Client::operationOutcome);
        c.queryOperation(operation);
        QVERIFY(settle());
        QVERIFY(!queried.empty());
        QCOMPARE(queried.last()[1].toString(),
                 committed ? QString("DurablyRecorded") : QString("Unknown"));
        QCOMPARE(c.transferBytes(), quint64(0));
        disconnect(connection);
        // Force termination after a real compute start; recovery determines the actual winner.
        bool restarted = false;
        connection = connect(&c, &Client::exchangeProgress, this, [&](QString stage, int) {
            if (!restarted && stage == "Before compute") {
                restarted = true;
                c.restart();
            }
        });
        base = c.state().revision;
        c.projectCommand("SetFeatures", sketch::fields({{"features", b.features()}}));
        operation = c.latestOperation();
        QVERIFY2(wait([&] { return restarted && c.ready(); }), qPrintable(c.state().message));
        disconnect(connection);
        QVERIFY(settle());
        QVERIFY(c.state().revision == base || c.state().revision == base + 1);
        queried.clear();
        c.queryOperation(operation);
        QVERIFY(settle());
        QVERIFY(!queried.empty());
        QCOMPARE(queried.last()[1].toString(),
                 c.state().revision == base + 1 ? QString("DurablyRecorded") : QString("Unknown"));
        QCOMPARE(c.transferBytes(), quint64(0));
        WorkspaceState state = c.state();
        wire::Envelope old;
        old.command = "PreviewDrawing";
        old.epoch = newId();
        old.context = state.context;
        old.generation = state.generation;
        QVERIFY(!visibleResult(state, old, state.revision));
        old.epoch = state.epoch;
        old.generation = state.generation - 1;
        QVERIFY(!visibleResult(state, old, state.revision));
        old.generation = state.generation;
        old.context.session = newId();
        QVERIFY(!visibleResult(state, old, state.revision));
        auto saved = files.filePath("older.cadproj"), upgraded = files.filePath("upgraded.cadproj");
        c.saveProject(saved);
        QVERIFY(settle());
        QFile source(saved);
        QVERIFY2(source.open(QIODevice::ReadOnly), qPrintable(c.state().message));
        auto bytes = source.readAll();
        source.close();
        QSignalSpy migrated(&c, &Client::exchangeReport);
        c.migrateProject(saved, upgraded);
        QVERIFY(settle());
        QVERIFY2(!migrated.empty(), qPrintable(c.state().message));
        QVERIFY(QFileInfo(upgraded).size() > 0);
        QVERIFY2(source.open(QIODevice::ReadOnly), qPrintable(c.state().message));
        QCOMPARE(source.readAll(), bytes);
        source.close();
        QVERIFY(!terminal.empty());
        c.closeProject();
        QVERIFY(settle());
    }
};
QTEST_GUILESS_MAIN(FaultContracts)
#include "contracts.moc"
