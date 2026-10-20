#include "support/protocol.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QDataStream>
#include <QLocalSocket>
#include <QTimer>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        // Private capability arrives on inherited stdin, never in argv or logs.
        std::string setup;
        std::getline(std::cin, setup);
        auto parts = QByteArray::fromStdString(setup).split(' ');
        wire::require(parts.size() == 3, "private bootstrap");
        auto endpoint = QString::fromUtf8(QByteArray::fromBase64(parts[0]));
        auto capability = parts[1];
        auto epoch = QString::fromUtf8(parts[2]);
        QLocalSocket socket;
        wire::Frames input;
        auto shape = BRepPrimAPI_MakeBox(10, 20, 30).Shape();
        GProp_GProps properties;
        BRepGProp::VolumeProperties(shape, properties);
        BRepMesh_IncrementalMesh mesher(shape, .2);
        QObject::connect(&socket, &QLocalSocket::connected, [&] {
            QCborMap hello;
            hello.insert(QStringLiteral("version"), 1);
            hello.insert(QStringLiteral("capability"), capability);
            hello.insert(QStringLiteral("epoch"), epoch);
            socket.write(wire::frame(hello));
        });
        QObject::connect(&socket, &QLocalSocket::readyRead, [&] {
            try {
                // Bounded reads keep the transport buffer independent of peer claims.
                while (socket.bytesAvailable())
                    for (const auto &request : input.feed(socket.read(16384))) {
                        wire::validate(request, epoch);
                        auto reply = request;
                        reply.insert(QStringLiteral("volume_mm3"), properties.Mass());
                        reply.insert(QStringLiteral("revision"), 1);
                        if (request.value("command_type").toString() == "Box") {
                            QByteArray vertices, indices, edges;
                            QCborArray faceIds;
                            int face = 0;
                            quint32 vertex = 0;
                            for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
                                auto f = TopoDS::Face(it.Current());
                                TopLoc_Location location;
                                auto mesh = BRep_Tool::Triangulation(f, location);
                                wire::require(!mesh.IsNull(), "triangulation");
                                ++face;
                                for (int t = 1; t <= mesh->NbTriangles(); ++t) {
                                    int a, b, c;
                                    mesh->Triangle(t).Get(a, b, c);
                                    if (f.Orientation() == TopAbs_REVERSED)
                                        std::swap(b, c);
                                    for (int node : {a, b, c}) {
                                        auto p =
                                            mesh->Node(node).Transformed(location.Transformation());
                                        float xyz[]{float(p.X()), float(p.Y()), float(p.Z())};
                                        vertices.append(reinterpret_cast<char *>(xyz), sizeof xyz);
                                        indices.append(reinterpret_cast<char *>(&vertex),
                                                       sizeof vertex);
                                        ++vertex;
                                    }
                                    faceIds.append(face);
                                }
                            }
                            // TopExp edge occurrences repeat per face; assign unique local edge
                            // IDs.
                            TopTools_IndexedMapOfShape uniqueEdges;
                            TopExp::MapShapes(shape, TopAbs_EDGE, uniqueEdges);
                            for (int i = 1; i <= uniqueEdges.Extent(); ++i) {
                                BRepAdaptor_Curve curve(TopoDS::Edge(uniqueEdges(i)));
                                for (auto p : {curve.Value(curve.FirstParameter()),
                                               curve.Value(curve.LastParameter())}) {
                                    float xyz[]{float(p.X()), float(p.Y()), float(p.Z())};
                                    edges.append(reinterpret_cast<char *>(xyz), sizeof xyz);
                                }
                            }
                            reply.insert(QStringLiteral("edges"), edges);
                            auto bulk = vertices + indices;
                            reply.insert(QStringLiteral("vertices_bytes"), vertices.size());
                            reply.insert(QStringLiteral("bulk_bytes"), bulk.size());
                            reply.insert(
                                QStringLiteral("transfer_checksum"),
                                QCryptographicHash::hash(bulk, QCryptographicHash::Sha256));
                            reply.insert(QStringLiteral("face_ids"), faceIds);
                            reply.insert(QStringLiteral("faces"), face);
                            reply.insert(QStringLiteral("checksum"),
                                         QCryptographicHash::hash(vertices + indices,
                                                                  QCryptographicHash::Sha256));
                            socket.write(wire::frame(reply));
                            // Bounded chunk fallback; opaque stream, no shared-memory path
                            // capability.
                            for (qsizetype offset = 0; offset < bulk.size(); offset += 128) {
                                QCborMap chunk;
                                chunk.insert(QStringLiteral("worker_epoch"), epoch);
                                chunk.insert(QStringLiteral("chunk_offset"), offset);
                                chunk.insert(QStringLiteral("chunk"), bulk.mid(offset, 128));
                                socket.write(wire::frame(chunk));
                            }
                            continue;
                        }
                        socket.write(wire::frame(reply));
                    }
            } catch (const std::exception &e) {
                std::cerr << e.what() << '\n';
                app.exit(2);
            }
        });
        QObject::connect(&socket, &QLocalSocket::disconnected, &app, &QCoreApplication::quit);
        QTimer::singleShot(10000, [&] {
            if (socket.state() != QLocalSocket::ConnectedState)
                app.exit(3);
        });
        socket.setReadBufferSize(limits::control + 4);
        socket.connectToServer(endpoint);
        return app.exec();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
