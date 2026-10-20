#include "MainWindow.hpp"
#include "geometry/occt/ReferenceScene.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>
#include <QWindow>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif
#ifdef Q_OS_MACOS
#include <libproc.h>
#include <sys/resource.h>
#endif
using namespace cad;
namespace {
bool nativeEnvironmentBlocked = false;
QString worker() {
    return QCoreApplication::applicationDirPath() + "/cad-worker";
}
bool waitUntil(std::function<bool()> test, int ms = 5000) {
    QElapsedTimer clock;
    clock.start();
    while (!test() && clock.elapsed() < ms)
        QTest::qWait(5);
    return test();
}
struct RawWorker {
    QTemporaryDir dir;
    QLocalServer server;
    QProcess process;
    QLocalSocket *socket = nullptr;
    wire::Frames frames;
    QString epoch = newId();
    QByteArray capability = QByteArray(32, 'x');
    QList<QCborMap> replies;
    ~RawWorker() {
        process.kill();
        process.waitForFinished(2000);
    }
    bool start() {
        server.setSocketOptions(QLocalServer::UserAccessOption);
        if (!server.listen(dir.path() + "/worker"))
            return false;
        process.start(worker(), {});
        if (!process.waitForStarted(3000))
            return false;
        process.write(server.fullServerName().toUtf8().toBase64() + ' ' + capability.toHex() + ' ' +
                      epoch.toUtf8() + '\n');
        process.closeWriteChannel();
        if (!waitUntil([&] { return server.hasPendingConnections(); }))
            return false;
        socket = server.nextPendingConnection();
        if (!waitUntil([&] { return socket->bytesAvailable() > 0; }))
            return false;
        auto maps = frames.feed(socket->readAll());
        return maps.size() == 1 && maps[0].value("capability").toByteArray() == capability &&
               maps[0].value("epoch").toString() == epoch;
    }
    void send(const wire::Envelope &e) {
        socket->write(wire::frame(e.map()));
        socket->flush();
    }
    bool receive(int minimum = 1) {
        return waitUntil([&] {
            if (socket->bytesAvailable())
                replies.append(frames.feed(socket->readAll()));
            return replies.size() >= minimum;
        });
    }
};
} // namespace
class Contracts : public QObject {
    Q_OBJECT
  private slots:
    void frameCorpus() {
        wire::Envelope e;
        e.epoch = newId();
        e.command = "LoadReference";
        e.payload.insert(QStringLiteral("instances"), 1);
        auto bytes = wire::frame(e.map());
        wire::Frames parser;
        QList<QCborMap> results;
        for (char byte : bytes)
            results.append(parser.feed(QByteArray(1, byte)));
        QCOMPARE(results.size(), 1);
        QVERIFY(sameRequest(e, wire::Envelope::parse(results.first())));
        for (const auto &field : {"protocol_version", "worker_epoch", "request_id", "operation_id",
                                  "project_id", "document_id", "command_session_id",
                                  "base_revision", "preview_generation", "payload"}) {
            auto bad = e.map();
            bad.insert(QString::fromLatin1(field), -1);
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, wire::Envelope::parse(bad));
        }
        auto bad = e.map();
        bad.insert(QStringLiteral("unknown"), 1);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, wire::Envelope::parse(bad));
        // Duplicate keys in top-level and nested maps, indefinite data, tagged data,
        // illegal UTF-8, oversized string/collection declarations, and depth overflow.
        for (const QByteArray &raw :
             {QByteArray::fromHex("a2616101616102"), QByteArray::fromHex("a16161a2616201616202"),
              QByteArray::fromHex("bf"), QByteArray::fromHex("c001"),
              QByteArray::fromHex("a1616161ff"), QByteArray::fromHex("a161617a00004001"),
              QByteArray::fromHex("a161619a000186a1"),
              QByteArray(40, char(0x81)) + QByteArray(1, char(0x00))})
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, wire::decode(raw));
        wire::Frames oversized;
        QByteArray header(4, 0);
        qToBigEndian(quint32(limits::control + 1), header.data());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, oversized.feed(header));
        auto response = wire::reply(e, "Failed", {}, "InvalidInput", "example");
        QVERIFY(sameRequest(wire::replyEnvelope(response), e));
        auto d = response.value("diagnostics").toMap();
        d.insert(QStringLiteral("operation_id"), newId());
        response.insert(QStringLiteral("diagnostics"), d);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, wire::replyEnvelope(response));
    }
    void transferCorpus() {
        kernel::ReferenceScene scene;
        scene.build(10, 20, 30, 100);
        auto packed = pack(scene.mesh());
        auto restored = unpack(packed.manifest, packed.bytes);
        QCOMPARE(restored->instances.size(), 100);
        QCOMPARE(restored->faceIds.size(), 6);
        QCOMPARE(restored->edgeIds.size(), 12);
        auto damaged = packed.bytes;
        damaged[0] ^= 1;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, unpack(packed.manifest, damaged));
        damaged = packed.bytes;
        auto offset = scene.mesh().vertices.size() * 12;
        qToLittleEndian(quint32(0xffffffff), damaged.data() + offset);
        auto manifest = packed.manifest;
        manifest.insert(QStringLiteral("checksum"),
                        QCryptographicHash::hash(damaged, QCryptographicHash::Sha256));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, unpack(manifest, damaged));
        manifest = packed.manifest;
        manifest.insert(QStringLiteral("bytes"), qint64(limits::segment + 1));
        Transfers transfers;
        wire::Envelope e;
        e.epoch = newId();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, transfers.begin(e, manifest));
        QCOMPARE(transfers.bytesReserved(), 0u);
        auto chunk = [&](int offset, QByteArray bytes) {
            QCborMap p;
            p.insert(QStringLiteral("transfer_id"), packed.manifest.value("transfer_id"));
            p.insert(QStringLiteral("offset"), offset);
            p.insert(QStringLiteral("bytes"), bytes);
            return p;
        };
        transfers.begin(e, packed.manifest);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, transfers.begin(e, packed.manifest));
        for (int identity = 0; identity < 6; ++identity) {
            auto old = e;
            if (identity == 0)
                old.epoch = newId();
            if (identity == 1)
                old.context.document = newId();
            if (identity == 2)
                old.generation++;
            if (identity == 3)
                old.revision++;
            if (identity == 4)
                old.operation = newId();
            if (identity == 5)
                old.request = newId();
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     transfers.append(old, chunk(0, packed.bytes)));
        }
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, transfers.append(e, chunk(1, packed.bytes)));
        auto complete = transfers.append(e, chunk(0, packed.bytes));
        QVERIFY(complete);
        QCOMPARE(transfers.bytesReserved(), 0u);
        // Aggregate budget: eight legal 32 MiB segments, ninth refused before allocation.
        manifest = packed.manifest;
        constexpr qint64 n = 2097152;
        manifest.insert(QStringLiteral("bytes"), qint64(limits::segment));
        manifest.insert(QStringLiteral("vertices"), n);
        manifest.insert(QStringLiteral("indices"), qint64(1572864));
        manifest.insert(QStringLiteral("edges"), qint64(0));
        manifest.insert(QStringLiteral("edge_ids"), QCborArray{});
        for (int i = 0; i < 8; ++i) {
            manifest.insert(QStringLiteral("transfer_id"), newId());
            transfers.begin(e, manifest);
        }
        QCOMPARE(transfers.bytesReserved(), limits::transfer);
        manifest.insert(QStringLiteral("transfer_id"), newId());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, transfers.begin(e, manifest));
        transfers.clear();
        QCOMPARE(transfers.bytesReserved(), 0u);
        WorkspaceState s;
        s.epoch = e.epoch;
        s.context = e.context;
        s.revision = 1;
        s.generation = 5;
        e.generation = 5;
        QVERIFY(visibleResult(s, e, 1));
        e.generation = 4;
        QVERIFY(!visibleResult(s, e, 1));
        e.generation = 6;
        QVERIFY(!visibleResult(s, e, 1));
        e.generation = 5;
        e.epoch = newId();
        QVERIFY(!visibleResult(s, e, 1));
        e.epoch = s.epoch;
        e.context.session = newId();
        QVERIFY(!visibleResult(s, e, 1));
        e.context = s.context;
        QVERIFY(!visibleResult(s, e, 0));
    }
    void workerCorpus() {
        RawWorker raw;
        QVERIFY(raw.start());
        wire::Envelope e;
        e.epoch = newId();
        e.command = "LoadReference";
        e.generation = 1;
        e.payload.insert(QStringLiteral("instances"), 1);
        raw.send(e);
        QVERIFY(raw.receive());
        QCOMPARE(raw.replies.takeFirst().value("diagnostics").toMap().value("code").toString(),
                 "StaleSession");
        e.epoch = raw.epoch;
        raw.send(e);
        auto received = raw.receive(2);
        if (!received) {
            for (auto r : raw.replies)
                qWarning() << QCborValue(r).toDiagnosticNotation();
            qWarning() << raw.process.state() << raw.process.exitCode() << raw.process.exitStatus()
                       << raw.process.readAllStandardError();
        }
        QVERIFY(received);
        auto first = raw.replies.takeFirst();
        QCOMPARE(first.value("status").toString(), "Succeeded");
        auto original = e;
        // Simulate losing the display acknowledgment: resend same operation with a new request.
        raw.replies.clear();
        e.request = newId();
        raw.send(e);
        QVERIFY(raw.receive(2));
        QCOMPARE(raw.replies.first().value("request_id").toString(), e.request);
        QCOMPARE(raw.replies.last().value("status").toString(), "Chunk");
        raw.replies.clear();
        e.payload.insert(QStringLiteral("instances"), 100);
        raw.send(e);
        QVERIFY(raw.receive());
        QCOMPARE(raw.replies.takeFirst().value("diagnostics").toMap().value("code").toString(),
                 "InvalidInput");
        e = original;
        e.request = newId();
        e.operation = newId();
        e.command = "QueryOperation";
        e.payload.clear();
        e.payload.insert(QStringLiteral("operation"), original.operation);
        raw.send(e);
        QVERIFY(raw.receive());
        QCOMPARE(raw.replies.takeFirst().value("data").toMap().value("outcome").toString(),
                 "CompletedInWorkerSession");
        e.operation = newId();
        e.request = newId();
        e.command = "PreviewReference";
        e.revision = 1;
        e.generation = 2;
        e.payload.clear();
        for (auto key : {"x_mm", "y_mm", "z_mm"})
            e.payload.insert(QString::fromLatin1(key), 40.);
        raw.send(e);
        QVERIFY(raw.receive(2));
        raw.replies.clear();
        e.request = newId();
        e.operation = newId();
        e.generation = 1;
        raw.send(e);
        QVERIFY(raw.receive());
        QCOMPARE(raw.replies.takeFirst().value("diagnostics").toMap().value("code").toString(),
                 "StaleRevision");
        // Malformed CBOR is a connection-level violation, not a geometry command.
        raw.socket->write(QByteArray::fromHex("00000001bf"));
        raw.socket->flush();
        QVERIFY(waitUntil([&] { return raw.process.state() == QProcess::NotRunning; }));
        QCOMPARE(raw.process.exitCode(), 2);
    }
    void clientLifecycle() {
        Client client(nullptr, worker());
        client.start();
        QVERIFY(waitUntil([&] { return client.ready(); }));
        client.loadReference();
        QVERIFY2(waitUntil([&] { return client.state().current(); }),
                 qPrintable(client.state().message));
        QCOMPARE(client.mesh()->instances.size(), 1);
        auto epoch = client.state().epoch;
        QSignalSpy measured(&client, &Client::measured);
        client.select({"Body", "body/reference-box", client.mesh()->instances[0].id});
        client.measure();
        QVERIFY(waitUntil([&] { return measured.size() == 1; }));
        QVERIFY(measured[0][0].toString().contains("6000"));
        for (int i = 0; i < 200; ++i) {
            client.previewReference(11 + i % 100, 21, 31);
            QVERIFY(client.outstanding() <= 2);
        }
        client.previewReference(44, 22, 33);
        QVERIFY(waitUntil([&] { return client.state().current() && client.outstanding() == 0; }));
        QCOMPARE(client.state().phase, Phase::PreviewReady);
        QCOMPARE(client.mesh()->maximum, QVector3D(44, 22, 33));
        client.previewReference(55, 66, 77);
        client.cancelPreview();
        QVERIFY(waitUntil([&] { return client.state().current() && client.outstanding() == 0; }));
        QCOMPARE(client.mesh()->maximum, QVector3D(10, 20, 30));
        QCOMPARE(client.state().phase, Phase::Ready);
        QCOMPARE(client.transferBytes(), 0u);
        auto op = client.latestOperation();
        QSignalSpy outcomes(&client, &Client::operationOutcome);
        client.queryOperation(op);
        QVERIFY(waitUntil([&] { return outcomes.size() == 1; }));
        QCOMPARE(outcomes[0][1].toString(), "CompletedInWorkerSession");
#ifdef Q_OS_UNIX
        QVERIFY(::kill(pid_t(client.workerPid()), SIGKILL) == 0);
        QVERIFY(waitUntil([&] { return client.state().phase == Phase::WorkerUnavailable; }));
        QVERIFY(client.state().stale);
        QVERIFY(client.mesh());
        QCOMPARE(client.transferBytes(), 0u);
        QCOMPARE(client.outstanding(), 0);
        client.restart();
        QVERIFY(waitUntil([&] { return client.state().current(); }));
        QVERIFY(client.state().epoch != epoch);
        outcomes.clear();
        client.queryOperation(op);
        QVERIFY(waitUntil([&] { return outcomes.size() == 1; }));
        QCOMPARE(outcomes[0][1].toString(), "UnknownNotDurable");
#endif
        client.loadReference(100);
        QVERIFY(waitUntil(
            [&] { return client.state().current() && client.mesh()->instances.size() == 100; }));
        client.closeReference();
        QVERIFY(!client.mesh());
        QCOMPARE(client.state().phase, Phase::Empty);
        QCOMPARE(client.transferBytes(), 0u);
    }
    void clientFailureCorpus() {
        for (auto mode : {"capability", "version", "epoch", "malformed", "oversized", "index",
                          "allocation", "termination"}) {
            qputenv("CAD_TEST_FAULT", mode);
            Client client(nullptr, QCoreApplication::applicationDirPath() + "/module1_adversary");
            client.start();
            client.loadReference();
            QVERIFY2(waitUntil([&] { return client.state().phase == Phase::WorkerUnavailable; }),
                     mode);
            QVERIFY(!client.mesh());
            QCOMPARE(client.outstanding(), 0);
            QCOMPARE(client.transferBytes(), 0u);
            QVERIFY(!client.ready());
            QVERIFY(!client.state().message.isEmpty());
        }
        qunsetenv("CAD_TEST_FAULT");
    }
    void nativeInteraction() {
        if (!qEnvironmentVariableIsSet("CAD_NATIVE_TESTS"))
            QSKIP("Real native graphics check requires CAD_NATIVE_TESTS=1 and a desktop session");
        QTemporaryDir directory;
        MainWindow window(directory.filePath("layout.ini"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.raise();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(waitUntil([&] { return window.workerClient()->ready(); }));
        QCOMPARE(window.size(), QSize(1280, 800));
        QVERIFY(!window.command("save")->isEnabled());
        QVERIFY(!window.command("save")->property("unavailableReason").toString().isEmpty());
        window.command("reference")->trigger();
        QVERIFY(waitUntil([&] { return window.state().current(); }));
        auto *v = window.sceneView();
        QVERIFY(window.findChild<QTabBar *>("documentTabs")->y() > v->y());
        QVERIFY(waitUntil([&] { return v->isValid(); }));
        QTest::qWait(100);
        QVERIFY(v->graphicsDescription().contains("initialized 1"));
        auto *tree = window.findChild<QTreeWidget *>("modelTree");
        auto *body = tree->topLevelItem(0)->child(0);
        auto *face = body->child(0);
        tree->scrollToItem(face);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tree->visualItemRect(face).center());
        QCOMPARE(window.state().selection, face->data(0, Qt::UserRole).value<Selection>());
        auto point = v->project({5, 10, 30});
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QVERIFY(!window.state().selection.empty());
        QVERIFY(tree->selectedItems().size() == 1);
        QCOMPARE(tree->selectedItems().first()->data(0, Qt::UserRole).value<Selection>(),
                 window.state().selection);
        for (auto kind : {"Body", "Component"}) {
            window.findChild<QComboBox *>("selectionFilter")->setCurrentText(kind);
            QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, point.toPoint());
            QCOMPARE(window.state().selection.kind, QString(kind));
        }
        window.command("measure")->trigger();
        QVERIFY(waitUntil([&] { return window.state().message.contains("6000"); }));
        window.command("view.Front")->trigger();
        window.findChild<QComboBox *>("selectionFilter")->setCurrentText("Edge");
        auto edgePoint = v->project({5, 0, 30});
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, edgePoint.toPoint());
        QCOMPARE(window.state().selection.kind, QString("Edge"));
        QSignalSpy measured(window.workerClient(), &Client::measured);
        window.command("measure")->trigger();
        QVERIFY(waitUntil([&] { return measured.size() == 1; }));
        QVERIFY(measured[0][0].toString().contains("10 mm"));
        window.findChild<QComboBox *>("selectionFilter")->setCurrentText("Face");
        point = v->project({5, 10, 0});
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QCOMPARE(window.state().selection.kind, QString("Face"));
        measured.clear();
        window.command("measure")->trigger();
        QVERIFY(waitUntil([&] { return measured.size() == 1; }));
        QVERIFY(measured[0][0].toString().contains("200 mm"));
        auto before = v->project({0, 0, 0});
        QTest::mousePress(v, Qt::RightButton, Qt::NoModifier, {200, 200});
        QTest::mouseMove(v, {250, 220});
        QTest::mouseRelease(v, Qt::RightButton, Qt::NoModifier, {250, 220});
        QVERIFY(v->project({0, 0, 0}) != before);
        auto previousPan = v->cameraPan();
        QTest::mousePress(v, Qt::MiddleButton, Qt::NoModifier, {200, 200});
        QTest::mouseMove(v, {220, 230});
        QTest::mouseRelease(v, Qt::MiddleButton, Qt::NoModifier, {220, 230});
        QVERIFY(v->cameraPan() != previousPan);
        auto distance = v->cameraDistance();
        QWheelEvent wheel(QPointF(200, 200), v->mapToGlobal(QPoint(200, 200)), {}, QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(v, &wheel);
        QVERIFY(v->cameraDistance() < distance);
        QTest::keyClick(v, Qt::Key_F);
        QCOMPARE(v->cameraPan(), QVector2D());
        for (auto mode : {"Shaded", "Wireframe", "Hidden edges", "Shaded with edges"}) {
            window.findChild<QComboBox *>("viewMode")->setCurrentText(mode);
            v->update();
            QTest::qWait(20);
            QVERIFY(v->findChild<QLabel *>("viewportOverlay")->text().contains(mode));
        }
        auto *dock = window.findChild<QDockWidget *>("modelDock");
        QVERIFY(window.dockWidgetArea(dock) == Qt::LeftDockWidgetArea);
        window.addDockWidget(Qt::RightDockWidgetArea, dock);
        window.resize(1440, 760);
        QTest::qWait(50);
        QVERIFY(v->width() > 0 && v->height() > 0);
        window.saveLayout();
        auto *properties = window.findChild<QTabWidget *>("managerTabs");
        properties->setCurrentIndex(1);
        window.findChild<QDoubleSpinBox *>("widthInput")->setValue(42);
        QTest::mouseClick(window.findChild<QPushButton *>("previewButton"), Qt::LeftButton);
        QVERIFY(window.state().phase == Phase::PreviewPending);
        QVERIFY(window.state().stale);
        QVERIFY(waitUntil([&] { return window.state().phase == Phase::PreviewReady; }));
        QCOMPARE(window.workerClient()->mesh()->maximum.x(), 42.f);
        QTest::mouseClick(window.findChild<QPushButton *>("cancelPreviewButton"), Qt::LeftButton);
        QVERIFY(waitUntil([&] { return window.state().phase == Phase::Ready; }));
        QCOMPARE(window.workerClient()->mesh()->maximum.x(), 10.f);
        window.command("instances")->trigger();
        QVERIFY(waitUntil([&] {
            return window.state().current() &&
                   window.workerClient()->mesh()->instances.size() == 100;
        }));
        v->fit();
        window.command("view.Isometric")->trigger();
        QJsonArray swapSamples;
        for (int i = 0; i < 60; ++i) {
            window.raise();
            window.activateWindow();
            if (!QTest::qWaitForWindowActive(&window) || !QTest::qWaitForWindowExposed(&window)) {
                nativeEnvironmentBlocked = true;
                QSKIP("Blocked: native presentation requires an active, exposed window");
            }
            QSignalSpy swap(v, &QOpenGLWidget::frameSwapped);
            QElapsedTimer timing;
            timing.start();
            v->setView(i % 2 ? "Isometric" : "Front");
            bool presented = !swap.isEmpty() || swap.wait(2000);
            if (!presented)
                qWarning() << "presentation-timeout" << "iteration" << i << "active"
                           << window.isActiveWindow() << "exposed"
                           << window.windowHandle()->isExposed() << "visible" << v->isVisible()
                           << "updates" << v->updatesEnabled() << "valid" << v->isValid()
                           << "paint_samples" << v->samples().size() << "graphics"
                           << v->graphicsDescription();
            if (!presented && (!window.isActiveWindow() || !window.windowHandle()->isExposed())) {
                nativeEnvironmentBlocked = true;
                QSKIP("Blocked: native window lost exposure during presentation measurement");
            }
            QVERIFY(presented);
            swapSamples.append(timing.nsecsElapsed() / 1e6);
        }
        auto path = qEnvironmentVariable("CAD_ARTIFACT_DIR");
        QVERIFY(!path.isEmpty());
        QDir().mkpath(path);
        v->setView("Isometric");
        QTest::qWait(50);
        QVERIFY(window.grab().save(path + "/native-" + QString::number(v->devicePixelRatioF()) +
                                   ".png"));
        QJsonObject result{{"graphics", v->graphicsDescription()},
                           {"viewport_logical", QJsonArray{v->width(), v->height()}},
                           {"instances", 100},
                           {"input_to_swap_ms", swapSamples}};
        auto actualMesh = window.workerClient()->mesh();
        auto packed = pack(*actualMesh);
        result["fixture"] = QJsonObject{
            {"id", "M1-reference-box-100-v1"},
            {"units", "mm"},
            {"vertices", actualMesh->vertices.size()},
            {"triangles", actualMesh->indices.size() / 3},
            {"faces", actualMesh->faceIds.size()},
            {"edges", actualMesh->edgeIds.size()},
            {"mesh_bytes_sha256",
             QString::fromLatin1(
                 QCryptographicHash::hash(packed.bytes, QCryptographicHash::Sha256).toHex())},
            {"instance_transforms_sha256",
             QString::fromLatin1(
                 QCryptographicHash::hash(packed.manifest.value("instances").toCbor(),
                                          QCryptographicHash::Sha256)
                     .toHex())}};
        QJsonArray samples;
        for (double ms : v->samples())
            samples.append(ms);
        result["cpu_submission_ms"] = samples;
#ifdef Q_OS_MACOS
        QJsonArray memory;
        for (auto pid : {QCoreApplication::applicationPid(), window.workerClient()->workerPid()}) {
            rusage_info_v2 usage{};
            QVERIFY(proc_pid_rusage(int(pid), RUSAGE_INFO_V2,
                                    reinterpret_cast<rusage_info_t *>(&usage)) == 0);
            memory.append(QJsonObject{{"pid", pid},
                                      {"physical_footprint_bytes", qint64(usage.ri_phys_footprint)},
                                      {"resident_bytes", qint64(usage.ri_resident_size)}});
        }
        result["process_memory"] = memory;
#endif
        QFile report(path + "/native-" + QString::number(v->devicePixelRatioF()) + ".json");
        QVERIFY(report.open(QIODevice::WriteOnly));
        report.write(QJsonDocument(result).toJson());
        window.close();
        MainWindow restored(directory.filePath("layout.ini"));
        QCOMPARE(restored.dockWidgetArea(restored.findChild<QDockWidget *>("modelDock")),
                 Qt::RightDockWidgetArea);
        QCOMPARE(restored.size(), QSize(1440, 760));
        restored.close();
    }
};
int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setOrganizationName("NativeCadTests");
    app.setApplicationName("Module1");
    Contracts tests;
    int result = QTest::qExec(&tests, argc, argv);
    return nativeEnvironmentBlocked ? 77 : result;
}
#include "contracts.moc"
