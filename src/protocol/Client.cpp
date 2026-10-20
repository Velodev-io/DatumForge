#include "Client.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
namespace cad {
Client::Client(QObject *parent, QString workerExecutable)
    : QObject(parent), privateRoot(QDir::tempPath() + "/cad-worker-XXXXXX"),
      executable(workerExecutable) {
    if (executable.isEmpty())
        executable = QCoreApplication::applicationDirPath() + "/cad-worker";
#ifdef Q_OS_WIN
    if (!executable.endsWith(".exe"))
        executable += ".exe";
#endif
    clock.start();
    deadlines.setInterval(250);
    connect(&deadlines, &QTimer::timeout, this, [this] {
        for (const auto &p : pending)
            if (clock.elapsed() > p.deadline) {
                fail("WorkerUnavailable", "Worker request timed out. Outcome is uncertain; restart "
                                          "and inspect before retrying.");
                return;
            }
    });
    connect(&server, &QLocalServer::newConnection, this, [this] {
        if (peer) {
            auto *extra = server.nextPendingConnection();
            extra->abort();
            extra->deleteLater();
            return;
        }
        peer = server.nextPendingConnection();
        peer->setReadBufferSize(limits::control + 4);
        connect(peer, &QLocalSocket::readyRead, this, [this] {
            try {
                while (peer && peer->bytesAvailable())
                    for (const auto &map : frames.feed(peer->read(16384)))
                        receive(map);
            } catch (const std::exception &e) {
                fail("ProtocolViolation", QString::fromUtf8(e.what()));
            }
        });
        connect(peer, &QLocalSocket::disconnected, this, [this] {
            if (!stopping)
                fail("WorkerUnavailable",
                     "Worker disconnected. The displayed scene is stale; restart the worker.");
        });
    });
    connect(&process, &QProcess::started, this, [this] {
        auto endpoint = server.fullServerName().toUtf8().toBase64();
        process.write(endpoint + ' ' + secret.toHex() + ' ' + projection.epoch.toUtf8() + '\n');
        process.closeWriteChannel();
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!stopping)
            fail("WorkerUnavailable",
                 "Cannot start the packaged CAD worker. Check the local installation.");
    });
    connect(&process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (!stopping)
            fail("WorkerUnavailable",
                 "Worker exited. Restart is available; document recovery arrives with Module 2.");
    });
    connect(&process, &QProcess::readyReadStandardError, this,
            [this] { process.readAllStandardError(); });
    connect(&process, &QProcess::readyReadStandardOutput, this,
            [this] { process.readAllStandardOutput(); });
}
Client::~Client() {
    stop();
}
void Client::stop() {
    stopping = true;
    deadlines.stop();
    authenticated = false;
    alive = false;
    if (peer) {
        peer->disconnect(this);
        peer->abort();
        peer->deleteLater();
        peer = nullptr;
    }
    server.close();
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(2000);
    }
    frames = wire::Frames();
    transfers.clear();
    pending.clear();
    ordinary.clear();
    preview.reset();
    secret.fill('\0');
    secret.clear();
    stopping = false;
}
void Client::start() {
    stop();
    projection.epoch = newId();
    projection.revision = 0;
    projection.generation = 0;
    projection.selection = {};
    projection.stale = bool(display);
    projection.phase = Phase::Empty;
    projection.job = "Starting local worker";
    projection.message.clear();
    emit changed();
    if (!privateRoot.isValid() ||
        !QFile::setPermissions(privateRoot.path(), QFileDevice::ReadOwner |
                                                       QFileDevice::WriteOwner |
                                                       QFileDevice::ExeOwner)) {
        fail("PermissionDenied", "Cannot create a private worker directory");
        return;
    }
    QString endpoint = privateRoot.path() + "/engine";
#ifdef Q_OS_WIN
    endpoint = "cad-" + newId();
#endif
    QLocalServer::removeServer(endpoint);
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(endpoint)) {
        fail("WorkerUnavailable", "Private local IPC is unavailable: " + server.errorString());
        return;
    }
    quint32 words[8];
    QRandomGenerator::system()->fillRange(words);
    secret = QByteArray(reinterpret_cast<char *>(words), sizeof words);
    alive = true;
    process.start(executable, {});
    auto epoch = projection.epoch;
    QTimer::singleShot(5000, this, [this, epoch] {
        if (alive && projection.epoch == epoch && !authenticated)
            fail("WorkerUnavailable", "Worker handshake timed out");
    });
}
void Client::fail(QString code, QString message) {
    if (stopping)
        return;
    stop();
    projection.phase = Phase::WorkerUnavailable;
    projection.job = "Failed";
    projection.message = code + ": " + message;
    projection.stale = bool(display);
    projection.selection = {};
    emit changed();
}
void Client::restart() {
    bool reopen = bool(display) || projection.phase != Phase::Empty;
    start();
    if (reopen)
        loadReference(referenceInstances);
}
wire::Envelope Client::request(QString command, QCborMap payload) {
    wire::Envelope e;
    e.epoch = projection.epoch;
    e.context = projection.context;
    e.revision = projection.revision;
    e.generation = projection.generation;
    e.command = command;
    e.payload = payload;
    return e;
}
void Client::enqueue(wire::Envelope e) {
    if (!alive) {
        projection.message = "Worker unavailable. Restart before issuing commands.";
        emit changed();
        return;
    }
    if (e.command == "PreviewReference")
        preview = e;
    else {
        if (outstanding() >= limits::outstanding) {
            projection.message = "ResourceLimit: at most 32 outstanding requests";
            emit changed();
            return;
        }
        ordinary.enqueue(e);
    }
    pump();
}
void Client::pump() {
    if (!authenticated || !peer || !pending.isEmpty())
        return;
    wire::Envelope e;
    if (!ordinary.empty())
        e = ordinary.dequeue();
    else if (preview) {
        e = *preview;
        preview.reset();
    } else
        return;
    pending.insert(e.request, {e, clock.elapsed() + 10000, 0});
    lastOperation = e.operation;
    deadlines.start();
    peer->write(wire::frame(e.map()));
}
void Client::loadReference(int count) {
    if (count != 1 && count != 100)
        return;
    referenceInstances = count;
    preview.reset();
    ordinary.clear();
    pending.clear();
    transfers.clear();
    projection.context = Context();
    projection.revision = 0;
    projection.generation = 1;
    projection.phase = Phase::Loading;
    projection.job = "Loading reference scene";
    projection.selection = {};
    projection.stale = bool(display);
    projection.message.clear();
    projection.editMode = "Inspect";
    QCborMap p;
    p.insert(QStringLiteral("instances"), count);
    enqueue(request("LoadReference", p));
    emit changed();
}
void Client::closeReference() {
    ordinary.clear();
    preview.reset();
    pending.clear();
    transfers.clear();
    projection.context = Context();
    projection.revision = 0;
    projection.generation = 0;
    projection.phase = Phase::Empty;
    projection.stale = false;
    projection.selection = {};
    projection.job = "Idle";
    display.reset();
    emit meshChanged({});
    emit changed();
}
void Client::previewReference(double x, double y, double z) {
    if (projection.revision != 1 || !alive)
        return;
    for (double n : {x, y, z})
        if (!std::isfinite(n) || n < 1 || n > 1000) {
            projection.message = "InvalidInput: reference dimensions must be 1–1000 mm";
            emit changed();
            return;
        }
    ++projection.generation;
    projection.phase = Phase::PreviewPending;
    projection.editMode = "Reference preview";
    projection.job = "Computing preview";
    projection.stale = true;
    projection.selection = {};
    QCborMap p;
    p.insert(QStringLiteral("x_mm"), x);
    p.insert(QStringLiteral("y_mm"), y);
    p.insert(QStringLiteral("z_mm"), z);
    enqueue(request("PreviewReference", p));
    emit changed();
}
void Client::cancelPreview() {
    if (projection.revision != 1)
        return;
    ++projection.generation;
    preview.reset();
    ordinary.clear();
    projection.phase = Phase::Loading;
    projection.editMode = "Inspect";
    projection.stale = true;
    projection.job = "Restoring reference";
    projection.selection = {};
    QCborMap p;
    p.insert(QStringLiteral("instances"), referenceInstances);
    enqueue(request("LoadReference", p));
    emit changed();
}
void Client::select(Selection s) {
    if (!projection.current() || !display)
        return;
    bool instance = false;
    for (const auto &i : display->instances)
        if (i.id == s.occurrence)
            instance = true;
    if (!s.empty() && (!instance || !((s.kind == "Face" && display->faceIds.contains(s.entity)) ||
                                      (s.kind == "Edge" && display->edgeIds.contains(s.entity)) ||
                                      ((s.kind == "Body" || s.kind == "Component") &&
                                       s.entity == "body/reference-box"))))
        return;
    projection.selection = s;
    emit changed();
}
void Client::measure() {
    if (!projection.current() || projection.selection.empty())
        return;
    auto s = projection.selection;
    QCborMap p;
    p.insert(QStringLiteral("kind"), s.kind);
    p.insert(QStringLiteral("entity"), s.entity);
    p.insert(QStringLiteral("occurrence"), s.occurrence);
    projection.job = "Measuring exact worker geometry";
    enqueue(request("Measure", p));
    emit changed();
}
void Client::queryOperation(QString operation) {
    if (!wire::uuid(operation))
        return;
    QCborMap p;
    p.insert(QStringLiteral("operation"), operation);
    enqueue(request("QueryOperation", p));
}
void Client::receive(const QCborMap &m) {
    if (!authenticated) {
        wire::keys(m, {"version", "epoch", "capability"});
        wire::require(wire::integer(m, "version") == 1 &&
                          wire::text(m, "epoch") == projection.epoch &&
                          m.value("capability").isByteArray() &&
                          m.value("capability").toByteArray() == secret,
                      "invalid handshake");
        authenticated = true;
        secret.fill('\0');
        secret.clear();
        projection.job = "Worker ready";
        emit changed();
        pump();
        return;
    }
    auto e = wire::replyEnvelope(m);
    auto found = pending.find(e.request);
    if (found == pending.end())
        return;
    wire::require(sameRequest(e, found->request), "reply request correlation");
    auto revision = wire::integer(m, "result_revision");
    if (!visibleResult(projection, e, revision)) {
        pending.erase(found);
        transfers.clear();
        pump();
        return;
    }
    auto status = wire::text(m, "status");
    auto data = m.value("data").toMap();
    auto diagnostics = m.value("diagnostics").toMap();
    if (status == "Failed" || status == "Canceled") {
        projection.message =
            wire::text(diagnostics, "code") + ": " + wire::text(diagnostics, "message");
        projection.job = status;
        projection.phase = Phase::Failed;
        projection.stale = bool(display);
        pending.erase(found);
        transfers.clear();
        emit changed();
        pump();
        return;
    }
    if (status == "Chunk") {
        wire::require(found->result == revision, "chunk result revision");
        auto mesh = transfers.append(e, data);
        if (!mesh)
            return;
        display = std::move(mesh);
        projection.revision = revision;
        projection.stale = false;
        projection.phase = e.command == "PreviewReference" ? Phase::PreviewReady : Phase::Ready;
        projection.job = e.command == "PreviewReference" ? "Preview — no document was committed"
                                                         : "Reference scene ready";
        projection.message.clear();
        pending.erase(found);
        emit meshChanged(display);
        emit changed();
        pump();
        return;
    }
    wire::require(status == "Succeeded", "unknown response status");
    if (data.contains(QStringLiteral("display_manifest"))) {
        wire::require(revision == 1 && data.value("display_manifest").isMap(), "display result");
        found->result = revision;
        transfers.begin(e, data.value("display_manifest").toMap());
        return;
    }
    if (e.command == "Measure") {
        wire::keys(data, {"value", "unit", "label"});
        auto n = data.value("value");
        wire::require(n.isDouble() && std::isfinite(n.toDouble()) && n.toDouble() > 0,
                      "measurement value");
        auto result = wire::text(data, "label") + ": " + QString::number(n.toDouble(), 'g', 12) +
                      " " + wire::text(data, "unit");
        projection.message = result;
        projection.job = "Exact measurement ready";
        emit measured(result);
    } else if (e.command == "QueryOperation") {
        auto id = wire::text(data, "operation"), outcome = wire::text(data, "outcome");
        wire::require(data.value("durable").isBool() && !data.value("durable").toBool(),
                      "unexpected durability claim");
        projection.message = outcome + " — reference operations are not durable project commands";
        emit operationOutcome(id, outcome);
    } else
        wire::require(false, "missing display manifest");
    pending.erase(found);
    emit changed();
    pump();
}
} // namespace cad
