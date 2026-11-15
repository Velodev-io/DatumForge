// Test executable only. Never linked into or shipped with cad-worker/desktop.
#include "protocol/Mesh.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QLocalSocket>
#include <QTimer>
#include <cstdio>
using namespace cad;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QFile inherited;
    if (!inherited.open(stdin, QIODevice::ReadOnly))
        return 1;
    auto fields = inherited.readLine(2048).trimmed().split(' ');
    if (fields.size() != 3)
        return 2;
    QString mode = qEnvironmentVariable("CAD_TEST_FAULT"), epoch = QString::fromUtf8(fields[2]);
    auto capability = QByteArray::fromHex(fields[1]);
    QLocalSocket socket;
    wire::Frames frames;
    QObject::connect(&socket, &QLocalSocket::connected, [&] {
        auto hello = wire::bootstrap(epoch, capability);
        if (mode == "capability")
            hello.insert(QStringLiteral("capability"), QByteArray(32, '!'));
        if (mode == "version")
            hello.insert(QStringLiteral("version"), 2);
        socket.write(wire::frame(hello));
    });
    QObject::connect(&socket, &QLocalSocket::readyRead, [&] {
        try {
            for (auto m : frames.feed(socket.readAll())) {
                auto e = wire::Envelope::parse(m);
                if (mode == "malformed") {
                    socket.write(QByteArray::fromHex("00000001bf"));
                    return;
                }
                if (mode == "oversized") {
                    QByteArray header(4, 0);
                    qToBigEndian(quint32(limits::control + 1), header.data());
                    socket.write(header);
                    return;
                }
                if (mode == "epoch") {
                    e.epoch = newId();
                    socket.write(wire::frame(wire::reply(e, "Succeeded")));
                    return;
                }
                DisplayMesh mesh;
                mesh.vertices = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
                mesh.indices = {0, 1, 2};
                mesh.triangleRoles = {0};
                mesh.faceIds = {"face"};
                mesh.instances = {{"instance", {}}};
                mesh.minimum = {0, 0, 0};
                mesh.maximum = {1, 1, 0};
                auto packed = pack(mesh);
                if (mode == "index") {
                    qToLittleEndian(quint32(9), packed.bytes.data() + 36);
                    packed.manifest.insert(
                        QStringLiteral("checksum"),
                        QCryptographicHash::hash(packed.bytes, QCryptographicHash::Sha256));
                }
                if (mode == "allocation")
                    packed.manifest.insert(QStringLiteral("bytes"), qint64(limits::segment + 1));
                QCborMap data;
                data.insert(QStringLiteral("display_manifest"), packed.manifest);
                auto response = wire::reply(e, "Succeeded", data);
                response.insert(QStringLiteral("result_revision"), 1);
                socket.write(wire::frame(response));
                socket.flush();
                if (mode == "termination") {
                    QTimer::singleShot(20, &app, &QCoreApplication::quit);
                    return;
                }
                QCborMap chunk;
                chunk.insert(QStringLiteral("transfer_id"), packed.manifest.value("transfer_id"));
                chunk.insert(QStringLiteral("offset"), 0);
                chunk.insert(QStringLiteral("bytes"), packed.bytes);
                auto part = wire::reply(e, "Chunk", chunk);
                part.insert(QStringLiteral("result_revision"), 1);
                socket.write(wire::frame(part));
            }
        } catch (...) {
            app.exit(3);
        }
    });
    QObject::connect(&socket, &QLocalSocket::disconnected, &app, &QCoreApplication::quit);
    QTimer::singleShot(5000, &app, &QCoreApplication::quit);
    socket.connectToServer(QString::fromUtf8(QByteArray::fromBase64(fields[0])));
    return app.exec();
}
