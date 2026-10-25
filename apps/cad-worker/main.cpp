#include "Projects.hpp"
#include "geometry/occt/ReferenceScene.hpp"
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QLocalSocket>
#include <QQueue>
#include <QTimer>
#include <Standard_Failure.hxx>
#include <atomic>
#include <iostream>
#include <optional>
#include <thread>
namespace {
constexpr QEvent::Type progressEvent = QEvent::Type(QEvent::User + 31),
                       finishedEvent = QEvent::Type(QEvent::User + 32);
struct JobEvent : QEvent {
    cad::wire::Envelope request;
    QCborMap data;
    QString code, message;
    JobEvent(QEvent::Type type, cad::wire::Envelope e, QCborMap result = {}, QString error = {},
             QString detail = {})
        : QEvent(type), request(std::move(e)), data(std::move(result)), code(std::move(error)),
          message(std::move(detail)) {}
};
class Worker : public QObject {
    cad::worker::Projects projects;
    QLocalSocket socket;
    cad::wire::Frames input;
    QString epoch;
    QByteArray capability;
    QQueue<cad::wire::Envelope> queue;
    QMap<QString, QPair<QByteArray, QCborMap>> outcomes;
    QMap<QString, cad::PackedMesh> cachedDisplays;
    void chunks(const cad::wire::Envelope &e, const cad::PackedMesh &packed, qint64 revision = 1) {
        auto id = cad::wire::text(packed.manifest, "transfer_id");
        for (qsizetype offset = 0; offset < packed.bytes.size(); offset += 4096) {
            QCborMap chunk;
            chunk.insert(QStringLiteral("transfer_id"), id);
            chunk.insert(QStringLiteral("offset"), offset);
            chunk.insert(QStringLiteral("bytes"), packed.bytes.mid(offset, 4096));
            auto frame = cad::wire::reply(e, "Chunk", chunk);
            frame.insert(QStringLiteral("result_revision"), revision);
            send(frame);
        }
    }
    cad::kernel::ReferenceScene scene;
    std::optional<cad::Context> context;
    qint64 generation = -1;
    int instanceCount = 1;
    bool scheduled = false;
    std::unique_ptr<std::jthread> job;
    std::optional<cad::wire::Envelope> activeJob;
    std::atomic_bool cancelJob{false};
    void finishProject(const cad::wire::Envelope &e, QCborMap data) {
        auto bytes = data.take(QStringLiteral("_display_bytes")).toByteArray();
        auto answer = cad::wire::reply(e, "Succeeded", data);
        answer.insert(QStringLiteral("result_revision"), projects.revision());
        send(answer);
        if (!bytes.isEmpty())
            chunks(e, {data.value("display_manifest").toMap(), bytes}, projects.revision());
    }
    void startExchange(cad::wire::Envelope e) {
        activeJob = e;
        cancelJob = false;
        job = std::make_unique<std::jthread>([this, e] {
            QCborMap data;
            QString code, message;
            int last = -1;
            cad::exchange::Control control;
            control.canceled = [this] { return cancelJob.load(); };
            control.progress = [this, e, &last](QString stage, int percent) {
                if (percent <= last)
                    return;
                last = percent;
                QCoreApplication::postEvent(
                    this,
                    new JobEvent(progressEvent, e,
                                 cad::sketch::fields({{"stage", stage}, {"percent", percent}})));
            };
            try {
                data = projects.run(e, control);
            } catch (const cad::domain::Error &error) {
                code = error.code;
                message = QString::fromUtf8(error.what());
            } catch (const Standard_Failure &error) {
                code = "InvalidGeometry";
                message = QString::fromUtf8(error.GetMessageString());
            } catch (const std::exception &error) {
                code = "InvalidInput";
                message = QString::fromUtf8(error.what());
            }
            QCoreApplication::postEvent(this, new JobEvent(finishedEvent, e, data, code, message));
        });
    }
    bool event(QEvent *event) override {
        if (event->type() != progressEvent && event->type() != finishedEvent)
            return QObject::event(event);
        auto &result = *static_cast<JobEvent *>(event);
        if (event->type() == progressEvent)
            send(cad::wire::reply(result.request, "Progress", result.data));
        else {
            job.reset();
            activeJob.reset();
            if (result.code.isEmpty())
                finishProject(result.request, result.data);
            else
                failure(result.request, result.code, result.message);
            if (!queue.empty()) {
                scheduled = true;
                QTimer::singleShot(0, this, [this] { next(); });
            }
        }
        return true;
    }
    void send(const QCborMap &map) {
        cad::wire::require(socket.bytesToWrite() <
                               qint64(cad::limits::segment + cad::limits::control),
                           "outgoing transport cap");
        socket.write(cad::wire::frame(map));
    }
    void failure(const cad::wire::Envelope &e, QString code, QString message) {
        send(cad::wire::reply(e, code == "Canceled" ? "Canceled" : "Failed", {}, code, message));
    }
    void enqueue(cad::wire::Envelope e) {
        if (e.epoch != epoch) {
            failure(e, "StaleSession", "Worker session has changed");
            return;
        }
        if (e.command == "CancelJob") {
            try {
                cad::wire::keys(e.payload, {"operation"});
                auto target = cad::wire::text(e.payload, "operation");
                for (int i = 0; i < queue.size(); ++i)
                    if (queue[i].operation == target && queue[i].context == e.context) {
                        auto old = queue.takeAt(i);
                        send(cad::wire::reply(old, "Canceled", {}, "Canceled",
                                              "Queued operation canceled before compute"));
                        send(cad::wire::reply(e, "Succeeded",
                                              cad::sketch::fields({{"cancel_requested", true}})));
                        return;
                    }
                cad::domain::check(
                    activeJob && activeJob->operation == cad::wire::text(e.payload, "operation") &&
                        activeJob->context == e.context,
                    "StaleSession", "No matching job in this session");
                cancelJob = true;
                send(cad::wire::reply(e, "Succeeded",
                                      cad::sketch::fields({{"cancel_requested", true}})));
            } catch (const cad::domain::Error &error) {
                failure(e, error.code, QString::fromUtf8(error.what()));
            }
            return;
        }
        if (job) {
            failure(e, "ResourceLimit", "One computation owns the worker; wait or cancel it");
            return;
        }
        if (e.command == "PreviewReference")
            for (int i = queue.size() - 1; i >= 0; --i)
                if (queue[i].command == e.command && queue[i].context == e.context) {
                    auto old = queue.takeAt(i);
                    send(cad::wire::reply(old, "Canceled", {}, "Canceled", "Superseded preview"));
                }
        if (queue.size() >= cad::limits::outstanding) {
            failure(e, "ResourceLimit", "Request queue is full");
            return;
        }
        queue.enqueue(e);
        if (!scheduled) {
            scheduled = true;
            QTimer::singleShot(0, this, [this] { next(); });
        }
    }
    void next() {
        scheduled = false;
        if (queue.empty())
            return;
        auto e = queue.dequeue();
        try {
            execute(e);
        } catch (const cad::domain::Error &error) {
            failure(e, error.code, QString::fromUtf8(error.what()));
        } catch (const Standard_Failure &error) {
            failure(e, "InvalidGeometry", QString::fromUtf8(error.GetMessageString()));
        } catch (const std::exception &error) {
            failure(e, "InvalidInput", QString::fromUtf8(error.what()));
        }
        if (!job && !queue.empty()) {
            scheduled = true;
            QTimer::singleShot(0, this, [this] { next(); });
        }
    }
    void execute(const cad::wire::Envelope &e) {
        using namespace cad;
        using wire::require;
        if (projects.handles(e.command)) {
            if (e.command == "ImportGeometry" || e.command == "ExportGeometry" ||
                QStringList{"SetSketches", "SetFeatures", "CommitHistory", "ResolveReference",
                            "SetAssembly", "PreviewAssembly", "DisplayAssembly", "SetDrawing",
                            "UpdateDrawing", "PreviewDrawing", "DisplayDrawing", "ExportDrawing",
                            "Undo", "Redo"}
                    .contains(e.command))
                startExchange(e);
            else
                finishProject(e, projects.run(e));
            return;
        }
        cad::domain::check(!projects.open(), "InvalidInput",
                           "Close the project before opening a temporary reference scene");
        if (e.command == "QueryOperation") {
            wire::keys(e.payload, {"operation"});
            auto id = wire::text(e.payload, "operation");
            require(wire::uuid(id), "operation identity");
            QCborMap data;
            auto found = outcomes.constFind(id);
            data.insert(QStringLiteral("operation"), id);
            data.insert(QStringLiteral("outcome"), found == outcomes.cend()
                                                       ? "UnknownNotDurable"
                                                       : "CompletedInWorkerSession");
            data.insert(QStringLiteral("durable"), false);
            if (found != outcomes.cend())
                data.insert(QStringLiteral("original_status"),
                            found.value().second.value("status"));
            send(wire::reply(e, "Succeeded", data));
            return;
        }
        if (e.command != "LoadReference" && e.command != "PreviewReference" &&
            e.command != "Measure") {
            failure(e, "UnsupportedOperation", "Command is not implemented in Module 1");
            return;
        }
        auto hash = e.intentHash();
        auto previous = outcomes.constFind(e.operation);
        if (previous != outcomes.cend()) {
            if (previous.value().first != hash) {
                failure(e, "InvalidInput", "Operation ID reused with different content");
                return;
            }
            if (!context || e.context != *context || e.generation != generation) {
                failure(e, "StaleRevision", "Original display generation is no longer current");
                return;
            }
            auto answer = previous.value().second;
            answer.insert(QStringLiteral("request_id"), e.request);
            send(answer);
            if (cachedDisplays.contains(e.operation))
                chunks(e, cachedDisplays.value(e.operation));
            return;
        }
        if (e.command != "LoadReference" &&
            (!context || e.context != *context || e.revision != 1)) {
            failure(e, "StaleRevision", "Reference scene context/revision is no longer current");
            return;
        }
        if (context && e.context == *context && e.generation < generation) {
            failure(e, "StaleRevision", "Preview generation was superseded");
            return;
        }
        QCborMap data;
        std::optional<PackedMesh> packed;
        if (e.command == "Measure") {
            if (e.generation != generation) {
                failure(e, "StaleRevision", "Measurement requires the displayed generation");
                return;
            }
            wire::keys(e.payload, {"kind", "entity", "occurrence"});
            data = scene.measure({wire::text(e.payload, "kind"), wire::text(e.payload, "entity"),
                                  wire::text(e.payload, "occurrence")});
        } else {
            double x = 10, y = 20, z = 30;
            int count = instanceCount;
            if (e.command == "LoadReference") {
                wire::keys(e.payload, {"instances"});
                count = int(wire::integer(e.payload, "instances", 100));
                require(count == 1 || count == 100, "reference count");
            } else {
                wire::keys(e.payload, {"x_mm", "y_mm", "z_mm"});
                auto dimension = [&](QString key) {
                    auto value = e.payload.value(key);
                    require(value.isDouble() || value.isInteger(), "numeric length");
                    double n = value.toDouble();
                    require(std::isfinite(n) && n >= 1 && n <= 1000, "size 1–1000 mm");
                    return n;
                };
                x = dimension("x_mm");
                y = dimension("y_mm");
                z = dimension("z_mm");
            }
            kernel::ReferenceScene candidate;
            candidate.build(x, y, z, count);
            packed = pack(candidate.mesh());
            scene = std::move(candidate);
            context = e.context;
            generation = e.generation;
            instanceCount = count;
            data.insert(QStringLiteral("display_manifest"), packed->manifest);
            data.insert(QStringLiteral("preview"), e.command == "PreviewReference");
        }
        auto result = wire::reply(e, "Succeeded", data);
        result.insert(QStringLiteral("result_revision"), 1);
        if (outcomes.size() >= 32) {
            auto id = outcomes.firstKey();
            outcomes.remove(id);
            cachedDisplays.remove(id);
        }
        outcomes.insert(e.operation, {hash, result});
        if (packed)
            cachedDisplays.insert(e.operation, *packed);
        send(result);
        if (packed)
            chunks(e, *packed);
    }

  public:
    ~Worker() override {
        cancelJob = true;
        job.reset();
    }
    Worker(QString address, QByteArray secret, QString workerEpoch, QString recovery)
        : projects(recovery), epoch(workerEpoch), capability(secret) {
        socket.setReadBufferSize(cad::limits::control + 4);
        connect(&socket, &QLocalSocket::connected, this, [this] {
            send(cad::wire::bootstrap(epoch, capability));
            capability.fill('\0');
            capability.clear();
        });
        connect(&socket, &QLocalSocket::readyRead, this, [this] {
            try {
                while (socket.bytesAvailable())
                    for (const auto &map : input.feed(socket.read(16384)))
                        enqueue(cad::wire::Envelope::parse(map));
            } catch (const std::exception &e) {
                std::cerr << "ProtocolViolation: " << e.what() << '\n';
                QCoreApplication::exit(2);
            }
        });
        connect(&socket, &QLocalSocket::disconnected, qApp, &QCoreApplication::quit);
        QTimer::singleShot(5000, this, [this] {
            if (socket.state() != QLocalSocket::ConnectedState)
                QCoreApplication::exit(3);
        });
        socket.connectToServer(address);
    }
};
} // namespace
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setOrganizationName("local.solidworks-clone.dev");
    app.setApplicationName("Solidworks Clone");
    try {
        QFile inherited;
        if (!inherited.open(stdin, QIODevice::ReadOnly))
            return 1;
        auto line = inherited.readLine(2048);
        cad::wire::require(line.endsWith('\n'), "bootstrap limit");
        auto parts = line.trimmed().split(' ');
        cad::wire::require((parts.size() == 3 || parts.size() == 4), "bootstrap fields");
        auto address = QString::fromUtf8(QByteArray::fromBase64(parts[0]));
        auto secret = QByteArray::fromHex(parts[1]);
        auto epoch = QString::fromUtf8(parts[2]);
        cad::wire::require(!address.isEmpty() && secret.size() == 32 && cad::wire::uuid(epoch),
                           "bootstrap types");
        Worker worker(address, secret, epoch,
                      parts.size() == 4 ? QString::fromUtf8(QByteArray::fromBase64(parts[3]))
                                        : QString());
        return app.exec();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
