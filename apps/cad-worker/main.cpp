#include "geometry/occt/ReferenceScene.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QLocalSocket>
#include <QQueue>
#include <QTimer>
#include <Standard_Failure.hxx>
#include <iostream>
#include <optional>
namespace {
class Worker : public QObject {
    QLocalSocket socket;
    cad::wire::Frames input;
    QString epoch;
    QByteArray capability;
    QQueue<cad::wire::Envelope> queue;
    QMap<QString, QPair<QByteArray, QCborMap>> outcomes;
    QMap<QString, cad::PackedMesh> cachedDisplays;
    void chunks(const cad::wire::Envelope &e, const cad::PackedMesh &packed) {
        auto id = cad::wire::text(packed.manifest, "transfer_id");
        for (qsizetype offset = 0; offset < packed.bytes.size(); offset += 4096) {
            QCborMap chunk;
            chunk.insert(QStringLiteral("transfer_id"), id);
            chunk.insert(QStringLiteral("offset"), offset);
            chunk.insert(QStringLiteral("bytes"), packed.bytes.mid(offset, 4096));
            auto frame = cad::wire::reply(e, "Chunk", chunk);
            frame.insert(QStringLiteral("result_revision"), 1);
            send(frame);
        }
    }
    cad::kernel::ReferenceScene scene;
    std::optional<cad::Context> context;
    qint64 generation = -1;
    int instanceCount = 1;
    bool scheduled = false;
    void send(const QCborMap &map) {
        cad::wire::require(socket.bytesToWrite() <
                               qint64(cad::limits::segment + cad::limits::control),
                           "outgoing transport cap");
        socket.write(cad::wire::frame(map));
    }
    void failure(const cad::wire::Envelope &e, QString code, QString message) {
        send(cad::wire::reply(e, "Failed", {}, code, message));
    }
    void enqueue(cad::wire::Envelope e) {
        if (e.epoch != epoch) {
            failure(e, "StaleSession", "Worker session has changed");
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
        } catch (const Standard_Failure &error) {
            failure(e, "InvalidGeometry", QString::fromUtf8(error.GetMessageString()));
        } catch (const std::exception &error) {
            failure(e, "InvalidInput", QString::fromUtf8(error.what()));
        }
        if (!queue.empty()) {
            scheduled = true;
            QTimer::singleShot(0, this, [this] { next(); });
        }
    }
    void execute(const cad::wire::Envelope &e) {
        using namespace cad;
        using wire::require;
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
    Worker(QString address, QByteArray secret, QString workerEpoch)
        : epoch(workerEpoch), capability(secret) {
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
    QCoreApplication app(argc, argv);
    try {
        QFile inherited;
        if (!inherited.open(stdin, QIODevice::ReadOnly))
            return 1;
        auto line = inherited.readLine(2048);
        cad::wire::require(line.endsWith('\n'), "bootstrap limit");
        auto parts = line.trimmed().split(' ');
        cad::wire::require(parts.size() == 3, "bootstrap fields");
        auto address = QString::fromUtf8(QByteArray::fromBase64(parts[0]));
        auto secret = QByteArray::fromHex(parts[1]);
        auto epoch = QString::fromUtf8(parts[2]);
        cad::wire::require(!address.isEmpty() && secret.size() == 32 && cad::wire::uuid(epoch),
                           "bootstrap types");
        Worker worker(address, secret, epoch);
        return app.exec();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
