#include "Client.hpp"
#include "domain/Documents.hpp"
#include "drawing/Paths.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
namespace cad {
Client::Client(QObject *parent, QString workerExecutable, QString recoveryRoot)
    : QObject(parent), privateRoot(QDir::tempPath() + "/cad-worker-XXXXXX"),
      executable(workerExecutable), recoveryDirectory(recoveryRoot) {
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
        connect(peer, &QLocalSocket::readyRead, this, [this, source = peer] {
            if (peer != source)
                return;
            const auto receivingEpoch = projection.epoch;
            try {
                while (peer == source && projection.epoch == receivingEpoch &&
                       source->bytesAvailable()) {
                    auto batch = frames.feed(source->read(16384));
                    for (const auto &map : batch) {
                        if (peer != source || projection.epoch != receivingEpoch)
                            return;
                        receive(map);
                    }
                }
            } catch (const std::exception &e) {
                if (peer == source && projection.epoch == receivingEpoch)
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
        process.write(endpoint + ' ' + secret.toHex() + ' ' + projection.epoch.toUtf8() +
                      (recoveryDirectory.isEmpty()
                           ? QByteArray()
                           : QByteArray(" ") + recoveryDirectory.toUtf8().toBase64()) +
                      '\n');
        process.closeWriteChannel();
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!stopping)
            fail("WorkerUnavailable",
                 "Cannot start the packaged CAD worker. Check the local installation.");
    });
    connect(&process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (!stopping)
            fail("WorkerUnavailable", "Worker exited. Restart can recover the last durable project "
                                      "revision; pending operation outcomes can be queried.");
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
    if (!projection.projectOpen)
        projection.revision = 0;
    projection.generation = 0;
    projection.selection = {};
    projection.stale = bool(display) || projection.projectOpen;
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
    projection.stale = bool(display) || projection.projectOpen;
    projection.selection = {};
    emit changed();
}
void Client::restart() {
    auto project = projection.projectOpen ? projection.context.project : QString();
    bool reopen = bool(display) || (!projection.projectOpen && projection.phase != Phase::Empty);
    start();
    if (!project.isEmpty())
        recoverProject(project);
    else if (reopen)
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
    if (e.command == "PreviewReference" || e.command == "PreviewSketchEdit" ||
        e.command == "PreviewSketches" ||
        (e.command == "PreviewFeatures" || e.command == "PreviewHistory" ||
         (e.command == "ReferenceCandidates" || e.command == "PreviewAssembly" ||
          e.command == "DisplayAssembly")))
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
    pending.insert(e.request,
                   {e,
                    clock.elapsed() + ((e.command == "SaveProject" || e.command == "SaveAs" ||
                                        e.command == "OpenProject")
                                           ? 120000
                                           : 10000),
                    0});
    if (e.command == "ImportGeometry" || e.command == "ExportGeometry" ||
        e.command == "PortableCopy" || e.command == "ExportDrawing" ||
        QStringList{"SetFeatures", "CommitHistory", "SetAssembly", "SetDrawing", "UpdateDrawing",
                    "DisplayDrawing", "PreviewDrawing", "DisplayAssembly", "PreviewAssembly"}
            .contains(e.command))
        pending[e.request].deadline = clock.elapsed() + 120000;
    if (!QStringList{"QueryOperation", "GrantFile", "RecoverProject", "ListRecoveries",
                     "InspectProject"}
             .contains(e.command))
        lastOperation = e.operation;
    pending[e.request].started = clock.elapsed();
    deadlines.start();
    peer->write(wire::frame(e.map()));
}
void Client::loadReference(int count) {
    if (projection.projectOpen) {
        projection.message = "Close the project before loading a temporary reference scene";
        emit changed();
        return;
    }
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
    if ((!projection.current() && !(projection.projectOpen && projection.stale &&
                                    projection.phase == Phase::PreviewReady)) ||
        !display)
        return;
    bool instance = false;
    for (const auto &i : display->instances)
        if (i.id == s.occurrence)
            instance = true;
    if (!s.empty() && (!instance || !((s.kind == "Face" && display->faceIds.contains(s.entity)) ||
                                      (s.kind == "Edge" && display->edgeIds.contains(s.entity)) ||
                                      ((s.kind == "Body" || s.kind == "Component") &&
                                       (s.entity == "body/reference-box" ||
                                        (projection.projectOpen && wire::uuid(s.entity)))))))
        return;
    projection.selection = s;
    emit changed();
}
void Client::measure() {
    if (!projection.current() || (projection.selection.empty() && !projection.projectOpen))
        return;
    if (projection.projectOpen && projection.selection.empty()) {
        projectCommand("MeasurePart", QCborMap{{QStringLiteral("body"), QString()},
                                               {QStringLiteral("token"), QString()},
                                               {QStringLiteral("second_token"), QString()},
                                               {QStringLiteral("density_kg_m3"), 0.}});
        return;
    }
    auto s = projection.selection;
    QCborMap p;
    p.insert(QStringLiteral("kind"), s.kind);
    p.insert(QStringLiteral("entity"), s.entity);
    p.insert(QStringLiteral("occurrence"), s.occurrence);
    projection.job = "Measuring exact worker geometry";
    if (projection.projectOpen) {
        auto entity = s.entity;
        auto slash = entity.indexOf('/');
        auto body = slash < 0 ? entity : entity.left(slash),
             token = slash < 0 ? QString() : entity.mid(slash + 1).section("/segment/", 0, 0);
        QCborMap query{{QStringLiteral("body"), body},
                       {QStringLiteral("token"), token},
                       {QStringLiteral("second_token"), QString()},
                       {QStringLiteral("density_kg_m3"), 0.}};
        enqueue(request("MeasurePart", query));
    } else
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
    bool reference = QStringList{"LoadReference",     "PreviewReference",    "Measure",
                                 "PreviewFeatures",   "DisplayPart",         "MeasurePart",
                                 "PreviewHistory",    "ReferenceCandidates", "PreviewAssembly",
                                 "DisplayAssembly",   "DisplayDrawing",      "PreviewDrawing",
                                 "PreviewSketchEdit", "PreviewSketches"}
                         .contains(e.command);
    if ((reference && !visibleResult(projection, e, revision)) || e.epoch != projection.epoch) {
        pending.erase(found);
        transfers.clear();
        // Retiring a canceled/stale request changes the idle state even though
        // its model result must not be installed.
        emit changed();
        pump();
        return;
    }
    auto status = wire::text(m, "status");
    if (!found->timed && (status == "Failed" || status == "Canceled" || status == "Succeeded")) {
        found->timed = true;
        emit jobFinished(e.command, status, clock.elapsed() - found->started,
                         transfers.bytesReserved());
    }
    auto data = m.value("data").toMap();
    auto diagnostics = m.value("diagnostics").toMap();
    if (status == "Progress") {
        wire::keys(data, {"stage", "percent"});
        auto percent = wire::integer(data, "percent", 100);
        projection.job = wire::text(data, "stage") + QString(" · %1%").arg(percent);
        emit exchangeProgress(wire::text(data, "stage"), int(percent));
        emit changed();
        return;
    }
    if (data.value("cancel_requested").toBool()) {
        pending.erase(found);
        projection.job = "Cancellation requested; waiting for the worker outcome";
        emit changed();
        return;
    }
    if (e.command == "CancelJob" && status == "Failed" &&
        diagnostics.value("code") == "StaleSession") {
        pending.erase(found);
        projection.message = "The computation already ended; its terminal outcome is retained";
        emit changed();
        pump();
        return;
    }
    if (status == "Failed" || status == "Canceled") {
        projection.message =
            wire::text(diagnostics, "code") + ": " + wire::text(diagnostics, "message");
        projection.job = status;
        projection.phase = Phase::Failed;
        projection.stale = bool(display);
        pending.erase(found);
        transfers.clear();
        if (e.command == "GrantFile")
            ordinary.clear();
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
        projection.phase = (e.command == "PreviewReference" ||
                            (e.command == "PreviewFeatures" || e.command == "PreviewHistory" ||
                             (e.command == "ReferenceCandidates" ||
                              e.command == "PreviewAssembly" || e.command == "DisplayAssembly")))
                               ? Phase::PreviewReady
                               : Phase::Ready;
        projection.job = e.command == "PreviewReference" ? "Preview — no document was committed"
                                                         : "Reference scene ready";
        projection.message.clear();
        pending.erase(found);
        emit meshChanged(display);
        if (e.command == "DisplayPart" ||
            (e.command == "PreviewFeatures" || e.command == "PreviewHistory" ||
             (e.command == "ReferenceCandidates" || e.command == "PreviewAssembly" ||
              e.command == "DisplayAssembly"))) {
            bool assembly = e.command == "PreviewAssembly" || e.command == "DisplayAssembly";
            projection.job =
                assembly ? (e.command == "PreviewAssembly" ? "Assembly preview · not committed"
                                                           : "Assembly ready")
                : e.command == "PreviewFeatures" ? "Feature preview · not committed"
                                                 : "Exact part ready";
            projection.stale = partReply.value("part_stale").toBool();
            if (projection.stale) {
                projection.job = assembly ? "Assembly unresolved / stale"
                                 : e.command == "ReferenceCandidates"
                                     ? "Reference source preview · final model STALE"
                                     : "STALE · last valid result";
                projection.message =
                    assembly
                        ? "Resolve components or repair mate references before exact evaluation."
                    : e.command == "ReferenceCandidates"
                        ? "Repair preview of upstream source geometry; final-model measurement is "
                          "unavailable."
                        : "Rebuild failed. Repair or edit the affected feature; current exact "
                          "measurement is unavailable.";
            }
            if (partReply.contains(QStringLiteral("reference_resolution")))
                emit referenceResolution(partReply.value("reference_resolution").toMap());
            emit partResult(partReply);
        }
        emit changed();
        pump();
        return;
    }
    wire::require(status == "Succeeded", "unknown response status");
    if (data.contains(QStringLiteral("display_manifest"))) {
        wire::require((projection.projectOpen || revision == 1) &&
                          data.value("display_manifest").isMap(),
                      "display result");
        if (e.command == "DisplayPart" ||
            (e.command == "PreviewFeatures" || e.command == "PreviewHistory" ||
             (e.command == "ReferenceCandidates" || e.command == "PreviewAssembly" ||
              e.command == "DisplayAssembly")))
            partReply = data;
        found->result = revision;
        transfers.begin(e, data.value("display_manifest").toMap());
        return;
    }
    if (data.contains(QStringLiteral("assembly_references")) ||
        data.contains(QStringLiteral("interference"))) {
        pending.erase(found);
        emit assemblyInformation(data);
        emit changed();
        pump();
        return;
    }
    if (data.contains(QStringLiteral("reference_resolution"))) {
        pending.erase(found);
        emit referenceResolution(data.value("reference_resolution").toMap());
        emit changed();
        pump();
        return;
    }
    if (data.contains(QStringLiteral("cache_statistics")) &&
        !data.contains(QStringLiteral("display_manifest")) &&
        !data.contains(QStringLiteral("part_measurement"))) {
        pending.erase(found);
        emit partResult(data);
        emit changed();
        pump();
        return;
    }
    if (data.value("part_empty").toBool()) {
        pending.erase(found);
        display.reset();
        projection.stale = data.value("part_stale").toBool();
        projection.phase = e.command == "PreviewHistory" ? Phase::PreviewReady : Phase::Ready;
        partReply = data;
        emit meshChanged({});
        emit partResult(data);
        emit changed();
        pump();
        return;
    }
    if (e.command == "MeasurePart") {
        wire::require(data.value("part_measurement").isMap(), "Exact part measurement required");
        pending.erase(found);
        projection.phase = Phase::Ready;
        projection.job = "Exact measurement ready";
        emit partResult(data);
        emit changed();
        pump();
        return;
    }
    if (e.command == "PreviewSketchEdit") {
        wire::keys(data, {"sketch_edit_preview", "affected_constraints"});
        wire::require(data.value("sketch_edit_preview").isArray() &&
                          data.value("affected_constraints").isArray(),
                      "Sketch edit preview collections required");
        bool current = visibleResult(projection, e, revision);
        pending.erase(found);
        if (current) {
            projection.phase = Phase::PreviewReady;
            projection.job = "Edit preview · review affected constraints";
            projection.message.clear();
            emit sketchEditPreview(data.value("sketch_edit_preview").toArray(),
                                   data.value("affected_constraints").toArray());
        }
        emit changed();
        pump();
        return;
    }
    if (e.command == "PreviewSketches") {
        wire::keys(data, {"sketch_preview", "sketch_diagnostics"});
        wire::require(data.value("sketch_preview").isArray(), "sketch preview array");
        bool current = visibleResult(projection, e, revision);
        pending.erase(found);
        if (current) {
            projection.phase = Phase::PreviewReady;
            projection.job = "Preview · not committed";
            projection.message.clear();
            emit sketchPreview(data.value("sketch_preview").toArray());
            QStringList labels;
            bool valid = true;
            for (auto d : data.value("sketch_diagnostics").toArray()) {
                auto m = d.toMap();
                valid = valid && m.value("valid").toBool();
                labels.append(m.value("status").toString() + " · DOF " +
                              QString::number(m.value("freedom").toInteger()) + " " +
                              m.value("message").toString());
            }
            projection.message = labels.join("; ");
            if (!valid) {
                projection.phase = Phase::Failed;
                projection.job = "Failed solve · accepted sketch retained";
            }
            emit sketchDiagnosed(data.value("sketch_diagnostics").toArray());
        }
        emit changed();
        pump();
        return;
    }
    if (data.contains(QStringLiteral("migration_report"))) {
        pending.erase(found);
        projection.job = "Validated upgraded project copy published; original preserved";
        emit exchangeReport(data.value("migration_report").toMap());
        emit changed();
        pump();
        return;
    }
    if (data.contains(QStringLiteral("exchange_report"))) {
        emit exchangeReport(data.value("exchange_report").toMap());
        if (!data.contains(QStringLiteral("project_state"))) {
            pending.erase(found);
            projection.job = "Export published";
            emit changed();
            pump();
            return;
        }
    }
    if (data.contains(QStringLiteral("closure_manifest"))) {
        pending.erase(found);
        emit exchangeReport(data.value("closure_manifest").toMap());
        emit changed();
        pump();
        return;
    }
    if (data.contains(QStringLiteral("drawing_result")) ||
        data.contains(QStringLiteral("drawing_anchors"))) {
        pending.erase(found);
        if (data.contains(QStringLiteral("drawing_result"))) {
            drawing::validateDisplay(data.value("drawing_result").toMap());
            projection.stale = !data.value("drawing_result").toMap().value("current").toBool();
            projection.job = projection.stale ? "Drawing stale or dangling - update/repair required"
                                              : "Drawing current";
            emit drawingInformation(data.value("drawing_result").toMap());
        } else
            emit drawingAnchors(data.value("drawing_anchors").toArray());
        emit changed();
        pump();
        return;
    }
    if (data.contains(QStringLiteral("project_state"))) {
        wire::require(data.value("project_state").isMap(), "project state map");
        installProject(data.value("project_state").toMap());
        pending.erase(found);
        for (auto value : projection.documentRecords) {
            auto d = value.toMap();
            if (d.value("id").toString() == projection.activeDocument &&
                d.value("type") == "Drawing") {
                enqueue(request("DisplayDrawing"));
                break;
            }
            if (d.value("id").toString() == projection.activeDocument &&
                d.value("type").toString() == "Assembly") {
                displayAssembly();
                break;
            }
            if (d.value("id").toString() == projection.activeDocument &&
                !d.value("features").toArray().empty()) {
                displayPart();
                break;
            }
        }
        emit projectChanged();
        emit changed();
        pump();
        return;
    }
    if (data.value("project_closed").toBool()) {
        projection.projectOpen = false;
        projection.documentRecords = {};
        projection.openDocuments.clear();
        projection.context = Context();
        projection.activeDocument.clear();
        projection.revision = 0;
        projection.savedRevision = -1;
        projection.savedPath.clear();
        projection.phase = Phase::Empty;
        projection.job = "Project closed; durable recovery retained";
        projection.save = "No project open";
        projection.stale = false;
        pending.erase(found);
        emit projectChanged();
        emit changed();
        pump();
        return;
    }
    if (e.command == "GrantFile") {
        pending.erase(found);
        pump();
        return;
    }
    if (e.command == "ListRecoveries") {
        wire::require(data.value("recoveries").isArray(), "recovery list");
        pending.erase(found);
        emit recoveryOffers(data.value("recoveries").toArray());
        pump();
        return;
    }
    if (e.command == "InspectProject") {
        wire::require(data.value("inspection").isMap(), "project inspection");
        pending.erase(found);
        emit projectInspection(data.value("inspection").toMap());
        pump();
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
        wire::require(data.value("durable").isBool(), "operation durability type");
        if (projection.projectOpen)
            wire::require(outcome == "DurablyRecorded" || outcome == "Unknown",
                          "project operation outcome");
        else
            wire::require(!data.value("durable").toBool(), "unexpected reference durability claim");
        projection.message =
            outcome + (data.value("durable").toBool()
                           ? " — recorded in durable project storage"
                           : " — no durable outcome is recorded; inspect before retrying");
        emit operationOutcome(id, outcome);
    } else
        wire::require(false, "missing display manifest");
    pending.erase(found);
    emit changed();
    pump();
}

void Client::installProject(const QCborMap &m) {
    wire::keys(m, {"project_id", "revision", "saved_revision", "save_generation", "active_document",
                   "documents", "open_documents", "undo_available", "redo_available", "saved_path",
                   "recoverable"});
    auto project = domain::id(wire::text(m, "project_id")),
         active = wire::text(m, "active_document");
    if (!active.isEmpty())
        domain::id(active);
    auto rev = wire::integer(m, "revision");
    wire::integer(m, "save_generation");
    auto saved = m.value("saved_revision");
    wire::require(saved.isInteger() && saved.toInteger() >= -1 && saved.toInteger() <= rev &&
                      m.value("recoverable").isBool() && m.value("recoverable").toBool(),
                  "project revision/durability");
    auto docs = m.value("documents");
    wire::require(docs.isArray() && docs.toArray().size() <= 128, "document records");
    QSet<QString> seen;
    for (const auto &item : docs.toArray()) {
        wire::require(item.isMap(), "document record map");
        auto d = item.toMap();
        auto documentKeys = QSet<QString>{"id",         "type",          "name",
                                          "units",      "revision",      "properties_entity",
                                          "parameters", "sketches",      "sketch_diagnostics",
                                          "features",   "feature_status"};
        if (d.contains(QStringLiteral("drawing")))
            documentKeys.insert("drawing");
        if (d.contains(QStringLiteral("assembly")))
            documentKeys.insert("assembly");
        wire::keys(d, documentKeys);
        auto id = domain::id(wire::text(d, "id"));
        wire::require(!seen.contains(id), "duplicate document identity");
        seen.insert(id);
        domain::kind(wire::text(d, "type"));
        domain::label(wire::text(d, "name"));
        domain::units(wire::text(d, "units"));
        domain::id(wire::text(d, "properties_entity"));
        wire::require(wire::integer(d, "revision") <= rev && d.value("parameters").isArray(),
                      "document revision/parameters");
        for (auto p : d.value("parameters").toArray())
            domain::Parameter::parse(p.toMap());
    }
    wire::require(active.isEmpty() || seen.contains(active), "active document identity");
    QStringList opened;
    wire::require(m.value("open_documents").isArray(), "open document list");
    for (auto id : m.value("open_documents").toArray()) {
        wire::require(id.isString() && seen.contains(id.toString()) &&
                          !opened.contains(id.toString()),
                      "open document identity");
        opened.append(id.toString());
    }
    wire::require(active.isEmpty() || opened.contains(active), "active document must be open");
    wire::require(m.value("undo_available").isBool() && m.value("redo_available").isBool(),
                  "history availability");
    bool changedContext =
        projection.context.project != project || projection.activeDocument != active;
    projection.projectOpen = true;
    projection.context.project = project;
    projection.context.document = active.isEmpty() ? project : active;
    if (changedContext) {
        projection.context.session = newId();
        ++projection.generation;
    }
    projection.activeDocument = active;
    projection.revision = rev;
    projection.savedRevision = saved.toInteger();
    projection.documentRecords = docs.toArray();
    projection.openDocuments = opened;
    projection.savedPath = wire::text(m, "saved_path");
    projection.canUndo = m.value("undo_available").toBool();
    projection.canRedo = m.value("redo_available").toBool();
    projection.phase = Phase::Ready;
    projection.stale = false;
    projection.selection = {};
    projection.editMode = "Document metadata";
    projection.job = QString("Recoverable revision %1").arg(rev);
    projection.save =
        projection.savedRevision == rev
            ? QString("Saved revision %1").arg(rev)
            : QString("Modified · last saved %1")
                  .arg(projection.savedRevision < 0 ? "never"
                                                    : QString::number(projection.savedRevision));
    projection.message.clear();
    if (display) {
        display.reset();
        emit meshChanged({});
    }
}
bool Client::projectReady() {
    if (!alive || !projection.projectOpen || outstanding()) {
        projection.message = "Wait for the active request, or open/recover a project first";
        emit changed();
        return false;
    }
    return true;
}
void Client::discardSketchPreview() {
    ++projection.generation;
    if (preview &&
        (preview->command == "PreviewSketchEdit" || preview->command == "PreviewSketches"))
        preview.reset();
    projection.phase = Phase::Ready;
    projection.job = "Preview canceled · accepted sketch retained";
    emit changed();
}
void Client::displayPart() {
    if (!projection.projectOpen || projection.activeDocument.isEmpty())
        return;
    enqueue(request("DisplayPart", QCborMap{{QStringLiteral("deflection_mm"), .2}}));
}
void Client::cancelPartPreview() {
    ++projection.generation;
    preview.reset();
    projection.phase = Phase::Ready;
    projection.message.clear();
    bool hasFeatures = false;
    for (auto value : projection.documentRecords) {
        auto d = value.toMap();
        if (d.value("id").toString() == projection.activeDocument)
            hasFeatures = !d.value("features").toArray().empty();
    }
    if (hasFeatures)
        displayPart();
    else {
        display.reset();
        emit meshChanged({});
    }
    emit changed();
}
void Client::projectCommand(QString command, QCborMap payload) {
    bool sketchPreview = command == "PreviewSketchEdit" || command == "PreviewSketches";
    if (sketchPreview && alive && projection.projectOpen) {
        // Permit one running and one replaceable pending sketch preview, while
        // preserving serialization with commits, file operations and other jobs.
        bool busy = !ordinary.empty();
        for (const auto &entry : pending)
            busy = busy || (entry.request.command != "PreviewSketchEdit" &&
                            entry.request.command != "PreviewSketches");
        if (busy) {
            projection.message = "Wait for the active operation before previewing a sketch";
            emit changed();
            return;
        }
    } else if (!projectReady())
        return;
    projection.job = "Running " + command;
    if (command == "PreviewFeatures" || command == "PreviewHistory" ||
        command == "PreviewAssembly" || command == "PreviewDrawing" ||
        command == "PreviewSketchEdit" || command == "PreviewSketches") {
        ++projection.generation;
        projection.phase = Phase::PreviewPending;
    }
    enqueue(request(command, payload));
    emit changed();
}
void Client::newProject(QString type, QString name, QString units, bool replace) {
    if (!alive || outstanding())
        return;
    auto e = request("NewProject");
    e.context = Context();
    e.revision = 0;
    e.payload.insert(QStringLiteral("id"), e.context.document);
    e.payload.insert(QStringLiteral("type"), type);
    e.payload.insert(QStringLiteral("name"), name);
    e.payload.insert(QStringLiteral("units"), units);
    e.payload.insert(QStringLiteral("properties_entity"), newId());
    e.payload.insert(QStringLiteral("replace"), replace);
    projection.job = "Creating recoverable project";
    enqueue(e);
    emit changed();
}
void Client::createDocument(QString type, QString name, QString units) {
    QCborMap p;
    p.insert(QStringLiteral("id"), newId());
    p.insert(QStringLiteral("type"), type);
    p.insert(QStringLiteral("name"), name);
    p.insert(QStringLiteral("units"), units);
    p.insert(QStringLiteral("properties_entity"), newId());
    projectCommand("CreateDocument", p);
}
void Client::switchDocument(QString id) {
    if (!projectReady())
        return;
    auto e = request("SwitchDocument");
    e.context.document = id;
    projection.job = "Switching document";
    enqueue(e);
    emit changed();
}
void Client::closeDocument() {
    projectCommand("CloseDocument");
}
void Client::closeProject() {
    QCborMap p;
    p.insert(QStringLiteral("choice"), "KeepRecoverable");
    projectCommand("CloseProject", p);
}
void Client::fileOperation(QString command, QString path, QCborMap payload) {
    if (!alive || outstanding())
        return;
    auto token = newId();
    QCborMap grant;
    grant.insert(QStringLiteral("token"), token);
    grant.insert(QStringLiteral("path"), path);
    grant.insert(QStringLiteral("access"),
                 (command == "SaveProject" || command == "SaveAs" || command == "ExportGeometry" ||
                  command == "PortableCopy" || command == "ExportDrawing")
                     ? "save"
                     : "read");
    enqueue(request("GrantFile", grant));
    payload.insert(QStringLiteral("capability"), token);
    enqueue(request(command, payload));
    projection.job = (command == "SaveProject" || command == "SaveAs")
                         ? QString("Saving revision %1").arg(projection.revision)
                         : "Reading chosen project";
    emit changed();
}
void Client::openProject(QString path, QString choice, bool replace) {
    QCborMap p;
    p.insert(QStringLiteral("choice"), choice);
    p.insert(QStringLiteral("replace"), replace);
    fileOperation("OpenProject", path, p);
}
void Client::inspectProject(QString path) {
    fileOperation("InspectProject", path, {});
}
void Client::saveProject(QString path, bool saveAs) {
    if (!projectReady())
        return;
    if (path.isEmpty())
        path = projection.savedPath;
    if (path.isEmpty()) {
        projection.message = "Choose a destination before saving";
        emit changed();
        return;
    }
    QCborMap p;
    if (saveAs)
        p.insert(QStringLiteral("new_project_id"), newId());
    fileOperation(saveAs ? "SaveAs" : "SaveProject", path, p);
}
void Client::listRecoveries() {
    if (alive && !outstanding())
        enqueue(request("ListRecoveries"));
}
void Client::recoverProject(QString id) {
    if (!alive || outstanding())
        return;
    QCborMap p;
    p.insert(QStringLiteral("project_id"), id);
    enqueue(request("RecoverProject", p));
}

void Client::importGeometry(QString path, QString format, QString units, QString healing,
                            QString sha) {
    if (!projectReady())
        return;
    QCborMap p{{QStringLiteral("format"), format},
               {QStringLiteral("units"), units},
               {QStringLiteral("healing"), healing},
               {QStringLiteral("feature_id"), newId()},
               {QStringLiteral("name"), QString("Imported body")},
               {QStringLiteral("expected_sha256"), sha}};
    fileOperation("ImportGeometry", path, p);
}
void Client::exportGeometry(QString path, QString format, QString units, double detail,
                            QString body, bool overwrite) {
    if (!projectReady())
        return;
    QCborMap p{{QStringLiteral("format"), format},
               {QStringLiteral("units"), units},
               {QStringLiteral("deflection_mm"), detail},
               {QStringLiteral("body"), body},
               {QStringLiteral("overwrite"), overwrite}};
    fileOperation("ExportGeometry", path, p);
}
void Client::portableCopy(QString path) {
    if (!projectReady())
        return;
    fileOperation("PortableCopy", path,
                  QCborMap{{QStringLiteral("new_project_id"), newId()},
                           {QStringLiteral("policy"), QString("Copied")}});
}
void Client::cancelExchange() {
    if (!peer || !authenticated)
        return;
    for (const auto &value : pending)
        if (value.request.command == "CancelJob")
            return;
    for (auto value : pending)
        if (value.request.command == "ImportGeometry" ||
            value.request.command == "ExportGeometry" ||
            QStringList{"SetSketches", "SetFeatures", "CommitHistory", "ResolveReference",
                        "SetAssembly", "PreviewAssembly", "DisplayAssembly", "SetDrawing",
                        "UpdateDrawing", "PreviewDrawing", "DisplayDrawing", "ExportDrawing",
                        "Undo", "Redo"}
                .contains(value.request.command)) {
            auto e = request("CancelJob",
                             QCborMap{{QStringLiteral("operation"), value.request.operation}});
            pending.insert(e.request, {e, clock.elapsed() + 10000, 0});
            peer->write(wire::frame(e.map()));
            return;
        }
}
void Client::displayAssembly() {
    if (projection.projectOpen && !projection.activeDocument.isEmpty())
        enqueue(request("DisplayAssembly"));
}
void Client::migrateProject(QString source, QString destination) {
    if (!alive || outstanding())
        return;
    auto read = newId(), save = newId();
    enqueue(request("GrantFile", QCborMap{{QStringLiteral("token"), read},
                                          {QStringLiteral("path"), source},
                                          {QStringLiteral("access"), QStringLiteral("read")}}));
    enqueue(request("GrantFile", QCborMap{{QStringLiteral("token"), save},
                                          {QStringLiteral("path"), destination},
                                          {QStringLiteral("access"), QStringLiteral("save")}}));
    enqueue(request("MigrateProject", QCborMap{{QStringLiteral("source_capability"), read},
                                               {QStringLiteral("destination_capability"), save}}));
}
void Client::exportDrawing(QString path, bool overwrite) {
    fileOperation("ExportDrawing", path, QCborMap{{QStringLiteral("overwrite"), overwrite}});
}
} // namespace cad
